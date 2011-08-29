#define _GNU_SOURCE

#include <errno.h>
#include <linux/nbd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <string.h>
#include <limits.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <netdb.h>
#include <fcntl.h>
#include <poll.h>
#include <math.h>

typedef unsigned long long u64;
typedef unsigned int u32;
typedef unsigned short u16;

/* Return value from the main proxy functions. */
enum proxy_state {
    /* Either side is properly disconnecting. */
    P_DISCONNECT,

    /* Communication protocol error.  Similar effect as P_MELTDOWN. */
    P_PROTOCOL_ERROR,

    /* Something is very wrong and the proxy must exit ASAP. Usually
       means we are out of memory. */
    P_MELTDOWN,

    /* All is well. Communication can continue. */
    P_OK
};

enum handshake_state {
    HS_INIT,

    HS_HELLO,

    HS_NS_HELLO_SERVER,

    HS_NS_HELLO_CLIENT,

    HS_OLD,

    HS_OPTION,

    HS_OPTION_DATA,
    
    HS_DATA,
};

enum server_state {
    /* Initial state. */
    S_INIT,

    /* Polling for connections. */
    S_NOPOLL,

    /* Asynchronous connection. */
    S_CONNECTING,

    /* Delay before next connection attempt. */
    S_CONNECT_DELAY,

    /* NBD handshake */
    S_HANDSHAKING,

    /* Ready to read protocol data. */
    S_READING,

    /* Disconnection was initiated by the client. */
    S_DISCONNECTING,

    /* Panic situation, exit immediately. */
    S_FUKUSHIMA
};

enum client_state {
    /* Initial state. */
    C_INIT,

    /* Listening for client connection. */
    C_LISTENING,

    /* NBD handshake, old style. */
    C_HANDSHAKING,

    /* The client is sending requests. */
    C_READING,

    /* The client is disconnecting. */
    C_DISCONNECTING
};

/* Linked list of proxy requests. */
struct proxy_nbd_request {
    
    /* Set to 1 when the header for this request does not need to be
       resent to the client. */
    int reply_header_sent;

    struct nbd_request *nr;
    struct proxy_nbd_request *next;
};

struct buf {
    char *data;

    /* Data tag. */
    int id;

    /* Size of the buffer pointed by data. */
    size_t sz;

    /* Quantity of data presently in the buffer. */
    size_t len;

    /* Read index in the buffer. */
    size_t idx;

    /* Next buffer in the linked list. */
    struct buf *nextbuf;
};

struct buf_queue {
    char name[12];

    int fd;

    struct buf *first;
    struct buf *last;
};

/* This structure is common between the 2 types of handshake. */
struct nbd_hello {
    char init_passwd[8];
    u64 magic;
} __attribute__ ((packed));

/* New style handshake: initialization data. */
struct nbd_ns_server_init_data {
    u16 zeroes;
} __attribute__ ((packed));

/* New style handshake: initialization data from the client. */
struct nbd_ns_client_init_data {
    u32 zeroes;
} __attribute__ ((packed));

/* New style handshake: handshake option data. */
struct nbd_ns_opt {
    u64 magic;
    u32 option;
    u32 size;
} __attribute__ ((packed));

/* New style handshake: block device data. */
struct nbd_ns_nbd_data {
    u64 size;
    u16 flags;
    char zeros[124];
} __attribute__ ((packed));

/* Old style handshake initialization data. */
struct nbd_init_data {
    u64 size;
    u32 flags;
    char zeros[124];
} __attribute__ ((packed));

/* Commande line options. */
struct buf_queue server_out_queue;
struct buf_queue server_in_queue;
struct buf_queue client_out_queue;
struct buf_queue client_in_queue;

/* Handshake data: option sent by the client. */
struct nbd_ns_opt hs_opt;

/* Handshake data: data attached to the option. */
char *hs_opt_data;

/* Initialization data from the client: currently all zeroes. */
struct nbd_ns_client_init_data hs_client_init_data;

/* Boolean for the debug mode. */
int debug_mode;

/* Number of times the server should try to reconnect before reaching
   S_INIT. */
int conn_strikes;

/* True if the proxy should stay attached to the console. */
int stay_attached;

/* Minimum and maximum value for exponential backoff. */
int backoff_min;
int backoff_max;

#define NBD_PROXY_VERSION "2.0.2"

#define PRINT_DEBUG_FUNC(FMT, ...)                              \
    do {                                                        \
        if (debug_mode) {                                       \
            char buf[255];                                      \
                                                                \
            snprintf(buf, sizeof(buf) - 1, FMT, ##__VA_ARGS__); \
            fprintf(stderr, "[%s] %s", __FUNCTION__, buf);      \
        }                                                       \
    } while (0);                                                \

#define PRINT_DEBUG(...) if (debug_mode) { fprintf(stderr, __VA_ARGS__); }

const char init_passwd[] = "NBDMAGIC";

u64 cliserv_magic = 0x00420281861253LL;
u64 cliserv_new_magic = 0x49484156454F5054LL;

/* New style handshake needs to be done only when listening on this
   port. */
u16 NBD_IANA_PORT = 10809;

/* Server-side state. */
enum server_state s_state;

/* Client-side state. */
enum client_state c_state;

/* Handshake state. */
enum handshake_state h_state;

char *handle_to_string(char *handle) {
    static char res[128];

    sprintf(res, "%X, %X, %X, %X, %X, %X, %X, %X", 
            (unsigned char)handle[0],
            (unsigned char)handle[1],
            (unsigned char)handle[2],
            (unsigned char)handle[3],
            (unsigned char)handle[4],
            (unsigned char)handle[5],
            (unsigned char)handle[6],
            (unsigned char)handle[7]);
    return res;
}

/* add_nbd_request
 *  Adding nbd_request to the chained list (proxy_nbd_request)
 *      r -- nbd_request to add into the chained list
 *      first -- first element of the list
 */
void add_nbd_request(struct nbd_request* nr, struct proxy_nbd_request **first) {
    struct proxy_nbd_request *new_pnr = (struct proxy_nbd_request*) malloc(sizeof(struct proxy_nbd_request));
    struct proxy_nbd_request *current_pnr = *first;

    if(*first == NULL)
        *first = new_pnr;

    else {
        while(current_pnr->next != NULL) 
            current_pnr = current_pnr->next;
        current_pnr->next = new_pnr;
    }
    new_pnr->reply_header_sent = 0;
    new_pnr->nr = nr;
    new_pnr->next = NULL;

    PRINT_DEBUG_FUNC("nbd_request handle(%s) added to linked list\n", handle_to_string(new_pnr->nr->handle));
}

/* get_nbd_request_by_handle
 *  Return nbd_request for a specific handle (nbd_request->handle)
 *      handle -- search param
 *      first -- first proxy_nbd_request of the chained list
 */
struct proxy_nbd_request *get_nbd_request_by_handle(char *handle, struct proxy_nbd_request **first) {
    struct proxy_nbd_request *current_pnr = *first;
    if(current_pnr != NULL) {
        do {
            if(!strncmp(handle, current_pnr->nr->handle, sizeof((*first)->nr->handle))) {
                return current_pnr;
            }
        } while((current_pnr = current_pnr->next) != NULL);
    }
    return NULL;
}

/* rm_nbd_request
 *  Remove nbd_request from proxy chained list
 *      nr -- nbd_request pointer to remove from the list and free
 *      first -- first proxy_nbd_request of the chained list
 */
void rm_nbd_request(struct nbd_request *nr, struct proxy_nbd_request **first) {
    struct proxy_nbd_request *current_pnr = *first;
    struct proxy_nbd_request *previous_pnr = *first;

    if(current_pnr != NULL) {
        do {
            if(current_pnr->nr == nr) {
                PRINT_DEBUG_FUNC("Removing nbd_request: handle(%s)\n", handle_to_string(current_pnr->nr->handle));
                    
                if(current_pnr == *first)
                    *first = current_pnr->next;
                else
                    previous_pnr->next = current_pnr->next;

                /* Free the request structure and the enclosing list struct. */
                free(nr);
                free(current_pnr);
                break;
            }
            previous_pnr = current_pnr;
        } while((current_pnr = current_pnr->next) != NULL);
    } 
    else {
        PRINT_DEBUG_FUNC("proxy_nbd_request empty... \n");
    }
}

void reset_nbd_requests(struct proxy_nbd_request **first) {
    int i = 0;
    struct proxy_nbd_request *current_pnr = *first;
    struct proxy_nbd_request *next_pnr;

    while (current_pnr != NULL) {
        next_pnr = current_pnr->next;
        current_pnr->next = NULL;
        free(current_pnr);
        current_pnr = next_pnr;
        i++;
    }

    PRINT_DEBUG_FUNC("Removed %d orphaned proxy requests from the queue.\n", i);
}

/* count_nbd_request
 *  Count number of nbd_request in queue
 *      first -- first proxy_nbd_request of the chained list
 */
int count_nbd_request(struct proxy_nbd_request **first) {
    int count = 0;
    struct proxy_nbd_request *current_pnr = *first;
    if(current_pnr == NULL)
        return count;
    do {
        count++;
    } while((current_pnr = current_pnr->next) != NULL); 
    return count; 
}

/* ntohll
 *  Network to host format for long long (64bits) data
 *      host_longlong -- 64 bit to transform
 */
uint64_t ntohll(uint64_t host_longlong) {
    int x = 1;

    /* little endian */
    if(*(char *)&x == 1)
        return ((((uint64_t)ntohl(host_longlong)) << 32) + ntohl(host_longlong >> 32));

    /* big endian */
    else
        return host_longlong;
}

#define htonll ntohll

/* sighandler
 *     Handles the different signal during the execution
 *         sig -- the type of signal emitted
 *             SIGINT is handled, it terminates the program
 */
void sighandler(int sig) {
    switch(sig) {
    case SIGINT:
        printf("Ctrl+C pressed, exiting\n");
        exit(EXIT_FAILURE);
        break;
            
    case SIGUSR1:
        break;
    case SIGUSR2:
        break;
    }
}

struct buf *alloc_buf(size_t s, int id) {
    struct buf *rb;

    rb = malloc(sizeof(struct buf));
    
    if (rb == NULL)
        return NULL;

    rb->data = malloc(s);
    if (rb->data == NULL) {
        free(rb);
        return NULL;
    }

    rb->sz = s;
    rb->len = 0;
    rb->id = id;
    rb->idx = 0;
    rb->nextbuf = NULL;    

    return rb;
}

/* Free a sendbuf structure. */
void free_buf(struct buf *sb) {
    free(sb->data);

    sb->data = NULL;
    sb->nextbuf = NULL;

    free(sb);
}

/** Put the sendbuf on the send queue. */
void queue_buf(struct buf_queue *queue, struct buf *sb) {
    PRINT_DEBUG("[%s] Enqueing %zu bytes.\n", queue->name, sb->sz);

    /* Enqueue as the first item. */
    if (queue->first == NULL) {
        queue->first = sb;
        queue->last = sb;
    }
    /* Enqueue as the last item. */
    else {
        queue->last->nextbuf = sb;
        queue->last = sb;
    }
}

/* Flush a input queue. */
void reset_buf_queue(struct buf_queue *queue) {
    int i = 0;
    struct buf *rb, *rbnext;

    /* Nothing to flush. */
    if (queue->first == NULL)
        return;

    rb = queue->first;

    /* Loop around the queue to free each elements. */
    do {
        rbnext = rb->nextbuf;
        free(rb->data);
        free(rb);
        rb = rbnext;
        i++;
    } while (rb != NULL);    

    PRINT_DEBUG("[%s] Freed %d queue elements.\n", queue->name, i);

    queue->first = NULL;
    queue->last = NULL;
}

/*
 * Reset the buf indexes.
 */
void reset_buf(struct buf *rb) { 
    rb->len = rb->sz;
    rb->idx = 0;
}

/* client_to_server
 *    This thread acts as a proxy from client to server
 *        data -- struct containing shared data between threads (thread_data)
 */
enum proxy_state client_proxy_server(struct buf *rb, struct proxy_nbd_request **reqs) {
    struct nbd_request *new_req;
        
    // Copy the request.
    new_req = (struct nbd_request*) malloc(sizeof(struct nbd_request));
    if (new_req == NULL) {
        free_buf(rb);
        return P_MELTDOWN;
    }
        
    memcpy(new_req, rb->data, sizeof(struct nbd_request));
    
    /* Checking if data is a valid nbd_request */
    if(ntohl(new_req->magic) == NBD_REQUEST_MAGIC) {
        
        /* NBD_CMD_READ from client */
        if(ntohl(new_req->type) == NBD_CMD_READ || ntohl(new_req->type) == NBD_CMD_WRITE) {
            PRINT_DEBUG_FUNC("Got NBD request: type(%u) handle(%s) of len(%u) and from(%llu)\n", 
                             ntohl(new_req->type), handle_to_string(new_req->handle), 
                             ntohl(new_req->len), (unsigned long long int)ntohll(new_req->from));
            
            // Add to the list of tracked requests.
            add_nbd_request(new_req, reqs);
        }
        
        /* NBD_CMD_DISC from client */
        else if (ntohl(new_req->type) == NBD_CMD_DISC) {
            free_buf(rb);
            free(new_req);
            return P_DISCONNECT;
        }
    }
    /* Invalid magic?!? */
    else {
        PRINT_DEBUG_FUNC("Invalid magic number.\n");
        free_buf(rb);
        free(new_req);
        return P_PROTOCOL_ERROR;
    }

    /* All data is forwarded to the server if the server is ready to
       accept it. Requests received when the server isn't ready will
       be handled immediately following the successful handshake. */
    if (s_state == S_READING) {      
        reset_buf(rb);
        queue_buf(&server_out_queue, rb);
    }

    return P_OK;
}

/*
 * Manages communication between the NBD server and the NBD client.
 */
enum proxy_state server_proxy_client(struct buf *rb, struct proxy_nbd_request **reqs) {
    static struct proxy_nbd_request *current_pnr = NULL;
    
    /* This is a header. */
    if (rb->id == 0) {
        struct nbd_reply *reply_hdr;
        
        /* Check if the handle is valid. */
        reply_hdr = (struct nbd_reply *)rb->data;                 
        if (reply_hdr->magic != htonl(NBD_REPLY_MAGIC)) {
            PRINT_DEBUG_FUNC("Invalid header magic received.\n");
            free_buf(rb);
            return P_PROTOCOL_ERROR;
        }
        else {
            PRINT_DEBUG_FUNC("Valid header received.\n");
            
            /* Getting corresponding nbd_request in queue (Thread safe) */
            current_pnr = get_nbd_request_by_handle(reply_hdr->handle, reqs);
            
            if (current_pnr == NULL) {
                PRINT_DEBUG_FUNC("Reply received for unknown request.\n");
                free_buf(rb);
                return P_PROTOCOL_ERROR;
            }
            else {
                PRINT_DEBUG_FUNC("Got nbd_reply : handle(%s)\n", handle_to_string(reply_hdr->handle));
            }
            
            reset_buf(rb);
            
            /* Don't resend the header if it was already sent. */
            if (!current_pnr->reply_header_sent) {
                queue_buf(&client_out_queue, rb);
                current_pnr->reply_header_sent = 1;
            }
            else {
                PRINT_DEBUG("Not resending header for handle(%s)\n", handle_to_string(reply_hdr->handle));
            }
                
            /* Queue a recv. */
            struct buf *data_rb = alloc_buf(ntohl(current_pnr->nr->len), 1);

            if (data_rb == NULL)
                return P_MELTDOWN;

            queue_buf(&server_in_queue, data_rb);
        } 
    }
    /* This is some data. */
    else {
        /* Received some data without matching header? BAD!. */
        if (current_pnr == NULL) {
            free_buf(rb);
            return P_MELTDOWN;
        }

        /* Updating current nbd_request.len of received bytes (r_bytes) */
        current_pnr->nr->len = htonl(ntohl(current_pnr->nr->len) - rb->sz);
        current_pnr->nr->from = htonll(ntohll(current_pnr->nr->from) + rb->sz);
        
        /* Removing nbd_request from queue. Not useful anymore. */
        if(current_pnr->nr->len == 0)
            rm_nbd_request(current_pnr->nr, reqs);
        
        /* Send the data to the client. */
        reset_buf(rb);
        queue_buf(&client_out_queue, rb);
    }

    return P_OK;
}

/* create_connect_sock
 *    Create a socket connected to a specific and point.
 *        port -- which port to connect to
 *        addr -- remote IP address (string format)
 *
 *    Return socket file descriptor
 */
int create_connect_sock(int *newfd) {
    int sock;
    int opt = 1;

    sock = socket(PF_INET, SOCK_STREAM, 0);

    if(sock == -1) {
        PRINT_DEBUG_FUNC("Failed to create socket: %m\n");
        return -1;
    }

    if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt)) < 0) {
        PRINT_DEBUG_FUNC("setsockopt(TCP_NODELAY) failed: %m\n");
        close(sock);
        *newfd = -1;
        return -1;
    }
    
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        PRINT_DEBUG_FUNC("setsockopt(SO_REUSEADDR) failed: %m\n");
        close(sock);
        *newfd = -1;
        return -1;
    }

    if ((fcntl(sock, F_SETFL, O_NONBLOCK)) < 0) {
        PRINT_DEBUG_FUNC("fcntl(O_NONBLOCK) failed: %m\n");
        close(sock);
        *newfd = -1;
        return -1;
    }

    *newfd = sock;  

    return 0;
}

/*
 * Return the error code for a asynchronous connect.
 */
int socket_errno(int sock) {
    int opt = 0;
    socklen_t s = sizeof(opt);

    if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &opt, &s) < 0) {
        PRINT_DEBUG_FUNC("Unable to return the socket error code.");
        return -1;
    }

    if (opt != 0)
        PRINT_DEBUG_FUNC("Socket error code: %d.\n", opt);

    return opt;
}

/*
 * Start an asynchronous connect on the server socket, putting the
 * server side in S_CONNECTING mode.
 *
 * Returns: -1 on error
 *          0  if the connection is in progress
 *          1  if the connection was completed
 */
int server_connect(int sock, char *addr, int port) {
    int err = -1;
    struct sockaddr_in struct_addr;

    struct_addr.sin_family = AF_INET;
    struct_addr.sin_port = htons(port);
    struct_addr.sin_addr.s_addr = inet_addr(addr);
    memset(struct_addr.sin_zero, 0, sizeof(struct_addr.sin_zero));

    err = connect(sock, (struct sockaddr *) &struct_addr, sizeof(struct_addr));
    if (err < 0 && errno == EINPROGRESS) {
        PRINT_DEBUG_FUNC("Connecting to %s.\n", addr);
        return 0;
    } 
    else if (err < 0) {
        PRINT_DEBUG_FUNC("Connection to %s failed: %m\n", addr);
        return -1;
    }
    /* The connection might succeed immediately. */
    else if (err == 0) 
        return 1;

    return 0;
}

/* create_listen_sock
 *    Create a socket listening on a port and bind
 *        port -- which port to listen on
 *        addr -- IP addr to bind on
 *
 *    Return socket file descriptor
 */
int create_listen_sock(int port, int addr, int *newfd) {
    int sock, opt = 1;
    struct sockaddr_in struct_addr;

    /* New socket */
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        PRINT_DEBUG_FUNC("Failed to create socket: %m\n");
        *newfd = -1;
        return -1;
    }
    
    struct_addr.sin_family = AF_INET;
    struct_addr.sin_port = htons(port);
    struct_addr.sin_addr.s_addr = ntohl(addr);
    memset(struct_addr.sin_zero, 0, sizeof(struct_addr.sin_zero));

    if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt)) < 0) {
        PRINT_DEBUG_FUNC("setsockopt(TCP_NODELAY) failed: %m\n");
        close(sock);
        *newfd = -1;
        return -1;
    }
    
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        PRINT_DEBUG_FUNC("setsockopt(SO_REUSEADDR) failed: %m\n");
        close(sock);
        *newfd = -1;
        return -1;
    }

    if ((fcntl(sock, F_SETFL, O_NONBLOCK)) < 0) {
        PRINT_DEBUG_FUNC("fcntl(O_NONBLOCK) failed: %m\n");
        close(sock);
        *newfd = -1;
        return -1;
    }

    PRINT_DEBUG_FUNC("Binding client socket.\n");

    if (bind(sock, (struct sockaddr *) &struct_addr, sizeof(struct_addr)) == -1) {
        PRINT_DEBUG_FUNC("bind() failed: %m\n");
        close(sock);
        *newfd = -1;
        return -1;
    }

    PRINT_DEBUG_FUNC("Listening.\n");

    if (listen(sock, 1) == -1) {
        PRINT_DEBUG_FUNC("listen() failed: %m\n");
        close(sock);
        *newfd = -1;
        return -1;
    }

    PRINT_DEBUG_FUNC("Socket bound and ready. Returning\n");

    c_state = C_LISTENING;
    *newfd = sock;

    return 0;
}

/* resend_all_nbd_requests */
int resend_all_nbd_requests(struct proxy_nbd_request *pnr) {
    struct proxy_nbd_request *current_pnr = pnr;

    if(current_pnr == NULL) {
        PRINT_DEBUG_FUNC("No nbd_request in queue\n");
        return 0;
    }

    do {
        if (debug_mode) {
            char *handle;
            handle = handle_to_string(current_pnr->nr->handle);
            PRINT_DEBUG_FUNC("nbd_request : handle(%s) of len(%u) and from(%llu)\n",
                             handle, 
                             ntohl(current_pnr->nr->len), 
                             (unsigned long long int) ntohll(current_pnr->nr->from));
        }

        /* Place a request to resend the partial request. */
        struct buf *sb = alloc_buf(sizeof(struct nbd_request), 0);

        if (sb == NULL)
            return -1;

        memcpy(sb->data, current_pnr->nr, sizeof(struct nbd_request));
        sb->len = sizeof(struct nbd_request);
        queue_buf(&server_out_queue, sb);

    } while ((current_pnr = current_pnr->next) != NULL);

    return 0;
}

int client_accept(int listen_socket, int *client_socket) {
    int newfd, opt;

    if ((newfd = accept(listen_socket, NULL, 0)) < 0) {
        PRINT_DEBUG_FUNC("Accept failed: %m\n");
        return -1;
    } 

    opt = 1;
    if ((setsockopt(newfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) < 0) {
        PRINT_DEBUG_FUNC("setsockopt(SO_REUSEADDR) failed: %m\n");
        return -1;
    }

    if ((fcntl(newfd, F_SETFL, O_NONBLOCK)) == -1) {
        PRINT_DEBUG_FUNC("fcntl(O_NONBLOCK) failed: %m\n");
        return -1;
    }   

    *client_socket = newfd;

    return 0;
}

/*
 * Write data to a queue.
 */
int send_queue(struct buf_queue *queue) {
    struct buf *sb = queue->first;

    /* TODO: It might improve speed a bit if we looped the send call
       until it returns EWOULDBLOCK or use writev.  */

    /* Nothing to send. */
    if (sb == NULL) 
        return 0;

    char *ptr = sb->data + sb->idx;
    size_t len = sb->len - sb->idx;
    ssize_t s;

    s = send(queue->fd, ptr, len, 0);

    /* Disconnect. */
    if (s < 0) {
        /* This seems to happen sometimes. */
        if (errno == EWOULDBLOCK) {
            PRINT_DEBUG("[%s] Spurious readyness signal received. Nevermind.\n", queue->name);
            return 0;
        }
        else if (errno == EINTR) {        
            PRINT_DEBUG("[%s] Write interrupted by signal.\n", queue->name);
            return 0;
        }            
        else {
            PRINT_DEBUG("[%s] Error in send(): %m\n", queue->name);
            return -1;
        }
    }
    else if (s == 0) {
        PRINT_DEBUG("[%s] Disconnected on send(): %m\n", queue->name);
        return -1;
    }
    /* Correct. */
    else {
        sb->idx += s;
        PRINT_DEBUG("[%s] %zu of %zu bytes written.\n", queue->name, sb->idx, sb->len);
    }

    if (sb->idx == sb->len) {
        queue->first = sb->nextbuf;

        /* Clear the last element if needed. */
        if (queue->first == NULL)
            queue->last = NULL;

        free_buf(sb);
    }

    return 0;    
}

/*
 * Read data from readqueue.
 */
int read_queue(struct buf_queue *queue, struct buf **complete_rb) {
    struct buf *rb = queue->first;
    ssize_t s;

    /* TODO: Same TODO as above for send_queue applies here, it's
       just a bit harder to use readv here. */

    /* Nothing to read. */
    if (rb == NULL)
        return 0;

    s = recv(queue->fd, rb->data + rb->len, rb->sz - rb->len, 0);

    if (s < 0) {
        /* This seems to happen sometimes. */
        if (errno == EWOULDBLOCK) {
            PRINT_DEBUG("[%s] Spurious readyness signal received. Nevermind.\n", queue->name);
            return 0;
        }
        /* Interruption by signal means we may be able to continue. */
        else if (errno == EINTR) {
            PRINT_DEBUG("[%s] Read interrupted by signal.\n", queue->name);
            return 0;
        }
        else {
            PRINT_DEBUG("[%s] Error on recv(): %m.\n", queue->name);
            return -1;
        }
    }
    /* Disconnection. */
    if (s == 0) {
        PRINT_DEBUG("[%s] Disconnected on recv().\n", queue->name);
        return -1;
    } 
    /* Proper, complete read. */
    else {
        rb->len += s;
        PRINT_DEBUG("[%s] %zu of %zu bytes read.\n", queue->name, rb->len, rb->sz);
    }

    /* Check if the recv buffer is complete. */
    if (rb->len == rb->sz) {
        *complete_rb = rb;
        
        /* Dequeue the buf */
        queue->first = rb->nextbuf;
        
        /* Clear the last element if the queue is empty. */
        if (queue->last == rb)
            queue->last = NULL;

        /* The readbuf is no longer in a queue. */
        rb->nextbuf = NULL;      
    }
    /* Partial read, nothing for you. */
    else 
        *complete_rb = NULL;

    return 0;
}

/* Initialize the queues we use. */
void init_queues() {
    PRINT_DEBUG_FUNC("Initializing queues.\n");

    strcpy(client_in_queue.name, "client_in");
    client_in_queue.first = NULL;
    client_in_queue.last = NULL;

    strcpy(client_out_queue.name, "client_out");
    client_out_queue.first = NULL;
    client_out_queue.last = NULL;

    strcpy(server_in_queue.name, "server_in");
    server_in_queue.first = NULL;
    server_in_queue.last = NULL;

    strcpy(server_out_queue.name, "server_out");
    server_out_queue.first = NULL;;
    server_out_queue.last = NULL;
}

/*
 * Check if the proxy is in idle state and place the required requests
 * on the queues if not.
 */
int check_idle_state() {
    if (c_state == C_READING && client_in_queue.first == NULL) {
        struct buf *rb = alloc_buf(sizeof(struct nbd_request), 0);

        PRINT_DEBUG("[%s] Idle.\n", client_in_queue.name);

        if (rb == NULL)
            return -1;
        
        queue_buf(&client_in_queue, rb);
    }

    if (s_state == S_READING && server_in_queue.first == NULL) {
        struct buf *rb = alloc_buf(sizeof(struct nbd_reply), 0);
        
        PRINT_DEBUG("[%s] Idle.\n", server_in_queue.name);

        if (rb == NULL)
            return -1;
        
        queue_buf(&server_in_queue, rb);
    }

    return 0;
}

void handshake_set_state(enum handshake_state newstate) {
    if (debug_mode) {
        if (h_state == newstate) return;

        if (newstate == HS_INIT) {
            PRINT_DEBUG_FUNC("Moving to HS_INIT state.\n");
        }
        else if (newstate == HS_HELLO) {
            PRINT_DEBUG_FUNC("Moving to HS_HELLO state.\n");
        }
        else if (newstate == HS_NS_HELLO_SERVER) {
            PRINT_DEBUG_FUNC("Moving to HS_NS_HELLO_SERVER state.\n");
        }
        else if (newstate == HS_NS_HELLO_CLIENT) {
            PRINT_DEBUG_FUNC("Moving to HS_NS_HELLO_CLIENT state.\n");
        }
        else if (newstate == HS_OPTION) {
            PRINT_DEBUG_FUNC("Moving to HS_OPTION state.\n");
        }
        else if (newstate == HS_OPTION_DATA) {
            PRINT_DEBUG_FUNC("Moving to HS_OPTION_DATA state.\n");
        }
        else if (newstate == HS_DATA) {
            PRINT_DEBUG_FUNC("Moving to HS_DATA state.\n");
        }
    }

    h_state = newstate;
}

/* 
 * Verbose change of the server state.
 */
void server_set_state(enum server_state newstate) {
    if (debug_mode) {
        if (s_state == newstate) return;

        if (newstate == S_INIT) {
            PRINT_DEBUG_FUNC("Moving to S_INIT state.\n");
        }
        else if (newstate == S_CONNECTING) {
            PRINT_DEBUG_FUNC("Moving to S_CONNECTING state.\n");
        }
        else if (newstate == S_CONNECT_DELAY) {
            PRINT_DEBUG_FUNC("Moving to S_CONNECT_DELAY state.\n");
        }
        else if (newstate == S_HANDSHAKING) {
            PRINT_DEBUG_FUNC("Moving to S_HANDSHAKING state.\n");
        }
        else if (newstate == S_READING) {
            PRINT_DEBUG_FUNC("Moving to S_READING state.\n");
        }
        else if (newstate == S_FUKUSHIMA) {
            PRINT_DEBUG_FUNC("Server meltdown.\n");
        }
    }

    s_state = newstate;
}

/*
 * Verbose change of the client state.
 */
void client_set_state(enum client_state newstate) {
    if (debug_mode) {
        if (c_state == newstate) return;

        if (newstate == C_LISTENING) {
            PRINT_DEBUG_FUNC("Moving to C_LISTENING state.\n");
        }
        else if (newstate == C_HANDSHAKING) {
            PRINT_DEBUG_FUNC("Moving to C_HANDSHAKING state.\n");
        }
        else if (newstate == C_READING) {
            PRINT_DEBUG_FUNC("Moving to C_READING state.\n");
        }
        else if (newstate == C_DISCONNECTING) {
            PRINT_DEBUG_FUNC("Moving to C_DISCONNECTING state.\n");
        }            
    }

    c_state = newstate;
}

/*
 * State machine for the old style of handshake.
 */
int server_old_handshake(struct buf *input_buf) {
    struct buf *out_buf = NULL;

    if (h_state == HS_OLD && input_buf == NULL) {
        /* Read the server data. */
        out_buf = alloc_buf(sizeof(struct nbd_init_data), 0);
        if (out_buf == NULL)
            return -1;

        queue_buf(&server_in_queue, out_buf);
    } 
    
    if (s_state == HS_OLD && input_buf != NULL) {
        /* If the client is handshaking too, send it the server data. */
        if (c_state == C_HANDSHAKING)
            queue_buf(&client_out_queue, input_buf);

        server_set_state(S_READING);
        
        if (c_state == C_HANDSHAKING)
            client_set_state(C_READING);
    }

    return 0;
}

/*
 * This contraption is a state machine able to asynchronously do the
 * handshake of the new style of protocol.
 */
int server_handshake(struct buf *input_buf) {
    struct buf *out_buf = NULL;
    size_t sz;

    /* Read the server data. */
    if (h_state == HS_INIT) {
        out_buf = alloc_buf(sizeof(struct nbd_hello), 0);
        if (out_buf == NULL)
            return -1;
        
        queue_buf(&server_in_queue, out_buf);

        handshake_set_state(HS_HELLO);
    }

    /* Complete the old handshake. */
    if (h_state == HS_OLD)
        return server_old_handshake(input_buf);

    if (h_state == HS_HELLO && input_buf != NULL) {
        struct nbd_hello *hello_data = ((struct nbd_hello *)input_buf->data);

        queue_buf(&client_out_queue, input_buf);
        input_buf = NULL;

        /* Old style handshake. */
        if (ntohll(hello_data->magic) == cliserv_magic) {
            PRINT_DEBUG_FUNC("Using old style handshake.\n");
            
            handshake_set_state(HS_OLD);
            server_old_handshake(NULL);
        }
        else {
            PRINT_DEBUG_FUNC("Using new style handshake.\n");

            /* Queue all the static size data that can be read. */           
            out_buf = alloc_buf(sizeof(struct nbd_ns_server_init_data), 0);
            if (out_buf == NULL)
                return -1;
            queue_buf(&server_in_queue, out_buf);

            /* Queue the handshake data to be received from the
               client. */
            out_buf = alloc_buf(sizeof(hs_client_init_data), 0);
            if (out_buf == NULL)
                return -1;
            queue_buf(&client_in_queue, out_buf);

            out_buf = alloc_buf(sizeof(hs_opt), 0);
            if (out_buf == NULL)
                return -1;
            queue_buf(&client_in_queue, out_buf);
            
            handshake_set_state(HS_NS_HELLO_SERVER);
        }
    }

    if (h_state == HS_NS_HELLO_SERVER && input_buf != NULL) {
        struct nbd_ns_server_init_data *init_data = ((struct nbd_ns_server_init_data *)input_buf->data);

        /* If the client is in handshake, he wants to received the
           data. */
        if (c_state == C_HANDSHAKING) {
            queue_buf(&client_out_queue, input_buf);
            input_buf = NULL;
        }
        /* Otherwise, simply discard the data. */
        else {
            /* Discard the data. */
            free_buf(input_buf);

            /* Copy the cached response for the next step. */
            input_buf = alloc_buf(sizeof(hs_client_init_data), 0);
            if (input_buf == NULL)
                return -1;
            memcpy(input_buf->data, &hs_client_init_data, sizeof(struct nbd_ns_client_init_data));
        }

        handshake_set_state(HS_NS_HELLO_CLIENT);
    }

    /* Get the reply data back from the client, send them to the
       server. */
    if (h_state == HS_NS_HELLO_CLIENT && input_buf != NULL) {
        /* Immediately forward the data to the server. */
        queue_buf(&server_out_queue, input_buf);
        input_buf = NULL;
        
        /* Use the option we cached if the client isn't in handshake
           mode. */
        if (c_state != C_HANDSHAKING) {
            /* Cache the client zeroes. */
            memcpy(&hs_client_init_data, input_buf->data, sizeof(hs_client_init_data));

            /* Discard the read data. */
            free_buf(input_buf);

            /* Copy the cached response for the next step. */
            input_buf = alloc_buf(sizeof(hs_client_init_data), 0);
            if (input_buf == NULL)
                return -1;
            memcpy(input_buf->data, &hs_opt, sizeof(struct nbd_ns_opt));
        }

        handshake_set_state(HS_OPTION);
    }

    if (h_state == HS_OPTION && input_buf != NULL) {
        /* Pick the option length from the packet. */
        sz = ntohl(((struct nbd_ns_opt *)input_buf->data)->size);

        /* Immediately forward the data to the server. */
        queue_buf(&server_out_queue, input_buf);
        input_buf = NULL;
        
        /* If the client is handshaking, he will send its option data. */
        if (c_state == C_HANDSHAKING) {
            /* The size of the option is set above. */
            out_buf = alloc_buf(sz, 0);
            if (out_buf == NULL)
                return -1;
            queue_buf(&client_in_queue, out_buf);
        }
        /* Othersize, just use the cached option. */
        else {
            free_buf(input_buf);

            input_buf = alloc_buf(sz, 0);
            if (input_buf == NULL)
                return -1;
            memcpy(input_buf->data, hs_opt_data, sz);
        }

        /* Queue the server block device data. */
        out_buf = alloc_buf(sizeof(struct nbd_ns_nbd_data), 0);
        if (out_buf == NULL)
            return -1;
        queue_buf(&server_in_queue, out_buf);

        /* Move to HS_OPTION_DATA to read the option data from the
           client. */
        handshake_set_state(HS_OPTION_DATA);
    }
    
    /* Prepare to receive block device data from the server. */
    if (h_state == HS_OPTION_DATA && input_buf != NULL) {
        /* Immediately forward the option data to the server. */
        queue_buf(&server_out_queue, input_buf);
        input_buf = NULL;

        /* Move to HS_DATA to read the block device data from the
           server. */
        handshake_set_state(HS_DATA);
    }

    /* Forward the block device data. */
    if (h_state == HS_DATA && input_buf != NULL) {
        if (c_state == C_HANDSHAKING) {
            /* Immediately forward the data to the client. */
            queue_buf(&client_out_queue, input_buf);
            
            /* After this has been sent, we expect that both the client
               and the server will be ready. */
            client_set_state(C_READING);
        } 
        server_set_state(S_READING);
    }

    return 0;
}

int proxy_poll(struct pollfd *fds, int nbconn, int listen_socket, int server_socket, int client_socket) {
    int poll_timeout = -1;
    int nbpoll;
    const int server = 0, client = 1;

    server_in_queue.fd = server_socket;
    server_out_queue.fd = server_socket;
    fds[server].fd = server_socket;
    fds[server].events = 0;
    fds[client].events = 0;
    fds[server].revents = 0;
    fds[client].revents = 0;

    /* Check if we need to trap new connections. */
    if (c_state == C_LISTENING) {
        fds[client].fd = listen_socket;
        fds[client].events |= (POLLIN | POLLOUT);
    }
    /* Once we have one client we no longer care about the
       listening socket. */
    else {
        fds[client].fd = client_socket;

        if (client_out_queue.first)
            fds[client].events |= POLLOUT;
        if (client_in_queue.first)
            fds[client].events |= POLLIN;
    }

    /* Check if we have to wait for the server connection to
       complete. */
    if (s_state == S_CONNECTING)
        fds[server].events |= (POLLIN | POLLOUT);
    else {
        /* Don't poll on the server socket if we are in a delay
           state. */
        if (s_state == S_CONNECT_DELAY)
            poll_timeout = (-backoff_max / pow(nbconn, 0.4)) + (backoff_max + backoff_min);
        else {
            if (s_state != S_NOPOLL) {
                if (server_out_queue.first)
                    fds[server].events |= POLLOUT;
                if (server_in_queue.first)
                    fds[server].events |= POLLIN;
            }
        }
    }

    if (s_state == S_NOPOLL)
        nbpoll = poll(&fds[1], 1, -1);
    else
        nbpoll = poll(fds, 2, poll_timeout);

    /* Timeout. */
    if (nbpoll == 0) return 0;

    /* Check for poll error. */
    if (nbpoll < 0) {
            
        /* Interruption by signal: nevermind */
        if (errno == EINTR) {
            PRINT_DEBUG_FUNC("Loop interrupted by signal.\n");
            return 0;
        }
            
        /* Any other kind of error with poll makes us exit the
           loop. */
        PRINT_DEBUG_FUNC("poll() error: %m\n");            
        client_set_state(C_DISCONNECTING);

        return -1;
    }

    return 0;
}

/* main
 *    entry point
 */
int main(int argc, char *argv[]) {
    int opt;
    int server_port, listen_port;
    int client_socket = -1, server_socket = -1, listen_socket = -1;
    int nbconn = 0;    
    char server_address[INET_ADDRSTRLEN];    
    char *smax, *smin;

    stay_attached = 0;
    debug_mode = 0;
    conn_strikes = 0;
    backoff_min = 0;
    backoff_max = 30;

    while ((opt = getopt(argc, argv, "adc:e:v")) != -1) {
        switch (opt) {
        case 'a':
            stay_attached = 1;
            break;
        case 'd':
            debug_mode = 1;
            stay_attached = 1;
            break;
        case 'c':
            conn_strikes = atoi(optarg);
            break;
        case 'v':
            fprintf(stdout, NBD_PROXY_VERSION"\n");
            exit(EXIT_SUCCESS);
            break;
        case 'e':
            smin = strtok(optarg, ":");
            smax = strtok(NULL, ":");
            if (smin == NULL || smax == NULL) {
                fprintf(stderr, "Invalid exponential backoff option.\n");
                exit(EXIT_FAILURE);
            }
            else {
                backoff_min = atoi(smin);
                backoff_max = atoi(smax);
            }
            break;
        default:
            fprintf(stderr, "Unknown option: %c.\n", opt);
            exit(EXIT_FAILURE);
        }
    }

    if (argc - optind < 3) {
        printf("Usage : nbd-proxy -v -a [-c nb conn] [-e backoff-min:backoff-max] -d server_address server_port listening_port\n");
        exit(1);
    }

    /* Server addr verification */
    if(inet_pton(AF_INET, argv[optind], &server_address) == 0) {
        struct hostent *host = gethostbyname(argv[optind]);
        if(host == NULL) { 
            printf("Invalid hostname or IP address: %s\n", server_address);
            exit(EXIT_FAILURE);
        }
        memcpy(server_address, inet_ntoa(*((struct in_addr **)host->h_addr_list)[0]), INET_ADDRSTRLEN);
    } else
        memcpy(server_address, argv[optind], INET_ADDRSTRLEN);

    /* Port verification */
    server_port = atoi(argv[optind + 1]);
    listen_port = atoi(argv[optind + 2]);
    if((server_port < 1 || server_port > 65535) ||
       (listen_port < 1 || listen_port > 65535)) {
        printf("Bad port range\n");
        exit(EXIT_FAILURE);
    }

    struct proxy_nbd_request *pnr = NULL;

    signal(SIGINT, sighandler);
    signal(SIGUSR1, SIG_IGN);
    signal(SIGUSR2, SIG_IGN);

    /* Our process ID and Session ID */
    pid_t pid, sid;

    if (!stay_attached) {
        sigset_t set;

        sigemptyset(&set);
        sigaddset(&set, SIGUSR2);
        sigaddset(&set, SIGCHLD);

        /* Block SIGUSR2 and SIGCLD from happening until we are
           ready. */
        sigprocmask(SIG_BLOCK, &set, NULL);

        /* Fork off the parent process */
        pid = fork();
        if (pid < 0) {
            fprintf(stderr, "Failed to fork nbd-proxy: %m\n");
            exit(EXIT_FAILURE);
        }

        /* If we got a good PID, then we can exit the parent
           process. */
        if (pid > 0) {
            int signo;
        
            signo = sigwaitinfo(&set, NULL);

            if (signo == SIGUSR2)
                exit(EXIT_SUCCESS);
            else
                exit(EXIT_FAILURE);
        }
        else {
            /* Change the file mode mask */
            umask(0);
    
            /* Create a new SID for the child process */
            sid = setsid();
            if (sid < 0) {
                fprintf(stderr, "setsid() failed: %m\n");
                exit(EXIT_FAILURE);
            }

            /* Change the current working directory */
            if (chdir("/") < 0) {
                fprintf(stderr, "chdir(/) failed: %m\n");
                exit(EXIT_FAILURE);
            }
        }
    }

    /* New client socket */
    if (create_listen_sock(listen_port, INADDR_LOOPBACK, &listen_socket) < 0) {
        close(server_socket);
        return -1;
    }

    /* Initialize the queues. */
    init_queues(server_socket);

    /* Tell the parent PID that we started successfully. */
    if (!stay_attached)
        kill(getppid(), SIGUSR2);

    /* Start the read/write loop, not leaving until the client
       disconnects. */
    while (c_state != C_DISCONNECTING && s_state != S_FUKUSHIMA) {
        struct pollfd fds[2];
        const int server = 0, client = 1;

        /* Start the asynchronous connection to the servcer. */
        if (s_state == S_INIT) {
            /* Flush the server queues. If this is a reconnection, we
               want to start with empty queues. */
            reset_buf_queue(&server_out_queue);
            reset_buf_queue(&server_in_queue);

            /* Don't retry to connect if we are out of connection
               attempts. */
            if (conn_strikes > 0 && nbconn == conn_strikes) {
                PRINT_DEBUG_FUNC("No more reconnect left (%d of %d).\n", nbconn, conn_strikes);
                server_set_state(S_FUKUSHIMA);
                continue;
            }

            /* Close then recreate the server socket if it's not new. */
            if (server_socket > 0) {
                shutdown(server_socket, SHUT_RDWR);
                close(server_socket);
            }

            /* New server socket */
            if (create_connect_sock(&server_socket) < 0) {
                server_set_state(S_INIT);
                continue;
            }

            /* The do-nothing, state. */
            server_set_state(S_NOPOLL);
        }

        /* Put the default read requests on the queues if needed. */
        if (check_idle_state() < 0) break;

        /* Poll for events on the sockets. */
        if (proxy_poll(fds, nbconn, listen_socket, server_socket, client_socket) < 0) break;

        /* The client wants to hangup or poll has problems waiting for
           the client. This means that we should disconnect since its
           the responsibility of the client to restart the proxy. */
        if (fds[client].revents & POLLHUP || fds[client].revents & POLLERR) {
            if (debug_mode && fds[client].revents & POLLHUP) {
                PRINT_DEBUG_FUNC("Client side hangup.\n");
            } else if (debug_mode && fds[client].revents & POLLERR) {
                PRINT_DEBUG_FUNC("Client side poll error.\n");
            }

            client_set_state(C_DISCONNECTING);
        }

        /* The server wants to hangup or poll has problems waiting for
           the server. */
        if (fds[server].revents & POLLHUP || fds[server].revents & POLLERR) {

            if (s_state == S_CONNECTING) {
                /* This will certainly fail but we will get more
                   information from the socket. */
                int e = socket_errno(fds[server].fd);                
                PRINT_DEBUG_FUNC("Connect failed: %s.\n", strerror(e));
            }
            if (debug_mode && fds[server].revents & POLLHUP) {
                PRINT_DEBUG_FUNC("Server side hangup.\n");
            } else if (debug_mode && fds[server].revents & POLLERR) {
                PRINT_DEBUG_FUNC("Server side poll error.\n");
            }

            server_set_state(S_INIT);
            continue;
        }                

        /* Complete connection on the server side. */
        if (s_state == S_CONNECTING && fds[server].revents & POLLOUT) {
            /* Check if the socket has an error. */
            if (socket_errno(fds[server].fd) != 0)
                server_set_state(S_INIT);

            server_set_state(S_HANDSHAKING);
            server_handshake(NULL);
            
            /* Reset the connection count when connected. */
            nbconn = 0;

            /* Must re-poll after handshake is started. */
            continue;
        }

        /* Complete handshake on the server side. */
        if (s_state == S_HANDSHAKING && (fds[server].revents & POLLIN | fds[client].revents & POLLIN)) {
            struct buf *rb = NULL;

            if (fds[server].revents & POLLIN && read_queue(&server_in_queue, &rb) < 0) {
                server_set_state(S_INIT);
                continue;
            }
            
            if (fds[client].revents & POLLIN && read_queue(&client_in_queue, &rb) < 0) {
                break;
            }

            /* Received some data. */
            else if (s_state == S_HANDSHAKING && rb != NULL) {
                if (server_handshake(rb) < 0) 
                    server_set_state(S_INIT);
                
                if (s_state == S_READING && resend_all_nbd_requests(pnr) < 0)
                    server_set_state(S_FUKUSHIMA);
            }
            
            /* This makes sure we don't try to re-read again without a
               poll. */
            continue;
        }

        /* Accept connection on the client side. */
        if (c_state == C_LISTENING && fds[client].revents & POLLIN) {
            int c;

            /* Start connecting to the server. */
            if (s_state == S_NOPOLL) {
                c = server_connect(server_socket, server_address, server_port);

                /* Immediate connection. */
                if (c == 1)
                    server_set_state(S_HANDSHAKING);
                else if (c == 0)
                    server_set_state(S_CONNECTING);
                else if (c < 0)
                    server_set_state(S_INIT);

                nbconn++;
            }

            /* Accept the connection from the client. */
            if (client_accept(listen_socket, &client_socket) < 0)
                client_set_state(C_DISCONNECTING);
            
            client_set_state(C_HANDSHAKING);

            /* Configure the client queue. */            
            client_in_queue.fd = client_socket;
            client_out_queue.fd = client_socket;

            continue;
        }

        /* Data ready from the server. */
        if (fds[server].revents & POLLIN) {
            struct buf *rb = NULL;

            if (read_queue(&server_in_queue, &rb) < 0) {
                server_set_state(S_INIT);
                continue;
            }

            /* Received some data. */
            else if (rb != NULL) {
                enum proxy_state ps = server_proxy_client(rb, &pnr);

                if (ps == P_MELTDOWN)
                    server_set_state(S_FUKUSHIMA);
                else if (ps == P_DISCONNECT || ps == P_PROTOCOL_ERROR)
                    server_set_state(S_INIT);
            }
        }

        /* Ready to write data to the server. */
        if (fds[server].revents & POLLOUT) {
            if (send_queue(&server_out_queue) < 0) {
                server_set_state(S_INIT);
                continue;
            }
        }

        /* Data ready from the client. */
        if (fds[client].revents & POLLIN) {
            struct buf *rb = NULL;

            if (read_queue(&client_in_queue, &rb) < 0) {
                client_set_state(C_DISCONNECTING);
                continue;
            }
            
            /* Received some data. */
            else if (rb != NULL) {
                enum proxy_state ps = client_proxy_server(rb, &pnr);

                if (ps == P_MELTDOWN || ps == P_PROTOCOL_ERROR || ps == P_DISCONNECT)
                    client_set_state(C_DISCONNECTING);
            }
        }

        if (fds[client].revents & POLLOUT)
            if (send_queue(&client_out_queue) < 0)
                client_set_state(C_DISCONNECTING);
    }

    PRINT_DEBUG_FUNC("Proxy quitting.\n");

    reset_nbd_requests(&pnr);   
    reset_buf_queue(&server_in_queue);
    reset_buf_queue(&client_in_queue);
    reset_buf_queue(&server_out_queue);
    reset_buf_queue(&client_out_queue);

    /* Shutdown all the sockets. */
    if (listen_socket != -1) {
        shutdown(listen_socket, SHUT_RDWR);
        close(listen_socket);
    }
    
    if (server_socket != -1) {
        shutdown(server_socket, SHUT_RDWR);
        close(server_socket);
    }

    if (client_socket != -1) {
        shutdown(client_socket, SHUT_RDWR);
        close(client_socket);
    }

    return 0;
}
