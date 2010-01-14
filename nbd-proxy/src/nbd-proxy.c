#include <stdio.h>
#include <stdlib.h>
#include <linux/nbd.h>
#include <sys/socket.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/types.h>
#include <limits.h>
#include <unistd.h>

#include "nbd-proxy.h"

void print_nrs(struct thread_data *infos) {
    int count = 0;
    if(*(infos->reqs) == NULL) {
        printf("[COUNT] %d\n", count);
        return;
    }
    struct proxy_nbd_request *current_pnr = *(infos->reqs);
    while((current_pnr = current_pnr->next) != NULL) {count++;}
    printf("[COUNT] %d\n", count);
}

/* client_to_server
 *    This thread acts as a proxy from client to server
 *        data -- struct containing shared data between threads (thread_data)
 */
void *client_to_server(void *data) {
    struct thread_data *infos = (struct thread_data*) data;
    // nbd_request size : 28 bytes
    char recv_buf[sizeof(struct nbd_request)];
    size_t bytes_read;

    printf("[th_client] Init mainloop\n");
    while(1) {
        bytes_read = recv(infos->client_socket, recv_buf, sizeof(recv_buf), 0);
        if(bytes_read == 0) {
            printf("[th_client] Client disconnected on recv(). Reconnecting\n");
            reconnect_client(infos);
            continue;
        }

        struct nbd_request *new_req = (struct nbd_request*) malloc(sizeof(struct nbd_request));
        memcpy(new_req, recv_buf, sizeof(recv_buf));
        // Checking if data is a valid nbd_request
        if((new_req->magic == ntohl(NBD_REQUEST_MAGIC)) && (new_req->type != ntohl(NBD_CMD_WRITE))) {
            //printf("[th_client] Got nbd_request\n");
            pthread_mutex_lock(&pnr_lock);
            add_nbd_request(new_req, infos->reqs);
            pthread_mutex_unlock(&pnr_lock);
        }

        // Sending received data to server
        if(send_to_server(infos, recv_buf, bytes_read) == -1) 
            resend_all_nbd_requests(infos);
    }
}

/* server_to_client
 *    This thread acts as a proxy from server to client
 *        data -- struct containing shared data between threads (thread_data)
 */
void *server_to_client(void *data) {
    struct thread_data *infos = (struct thread_data*) data;
    struct nbd_reply nr;
    char recv_buf[SRV_RECV_BUF];
    size_t bytes_read;
    size_t r_bytes;
    struct nbd_request *current_nr = NULL;
    int discard_reply_flag = 0;
    int size_recv_buf = SRV_RECV_BUF;
    struct nbd_request *flag_disc;

    // Main loop
    printf("[th_server] Init mainloop\n");
    while(1) {
        bytes_read = recv(infos->server_socket, recv_buf, size_recv_buf, 0);
        // Keep track of bytes read for specific use
        r_bytes = bytes_read;
        discard_reply_flag = 0;
        flag_disc = NULL;
        if(bytes_read == 0) {
            printf("[th_server] Server disconnected on recv(). Reconnecting\n");
            pthread_mutex_lock(&pnr_lock);
            reconnect_server(infos);
            // Sending last nbd_request modified
            if(current_nr != NULL) {
                send_to_server(infos, (char *) current_nr, sizeof(struct nbd_request));
                flag_disc = current_nr;
                discard_reply_flag = sizeof(struct nbd_reply);
                printf("[th_server] Last known nbd_request sent\n");
                printf("   |-- len : %u\n", ntohl(current_nr->len));
                printf("   |-- from : %lu\n", ntohll(current_nr->from));
                char *handle = current_nr->handle;
                printf("   |-- handle : %X, %X, %X, %X, %X, %X, %X, %X\n", 
                    handle[0], handle[1], handle[2], handle[3],
                    handle[4], handle[5], handle[6], handle[7]);
            }
            //resend_all_nbd_requests(infos);
            pthread_mutex_unlock(&pnr_lock);
            continue;
        }

        // Getting nbd_reply
        memcpy(&nr, recv_buf, sizeof(struct nbd_reply));
        if(nr.magic == ntohl(NBD_REPLY_MAGIC)) {
            //printf("[th_server] Got nbd_reply\n");
            // The packet received contain a valid nbd_reply
            pthread_mutex_lock(&pnr_lock);
            if((current_nr = get_nbd_request_by_handle(nr.handle, infos->reqs)) == NULL) {
                printf("[th_server] nbd_reply handle unknown\n");
            }
            pthread_mutex_unlock(&pnr_lock);

            // Ignoring nbd_reply size for len in nbd_request
            r_bytes -= 16;
        } else if(current_nr == NULL) {
            printf("[th_server] Fatal error: No nbd_reply received and no nbd_request to serve\n");
        }

        // Sending data to client (P -> C)
        if(send_to_client(infos, recv_buf + discard_reply_flag, 
                bytes_read - discard_reply_flag) == -1) {
            continue;
        }

        // Updating current nbd_request.len of received bytes (r_bytes)
        if(current_nr != NULL) {
            current_nr->len = htonl(ntohl(current_nr->len) - r_bytes);
            current_nr->from = htonll(ntohll(current_nr->from) + r_bytes);

            if((current_nr->len) == 0) {
                pthread_mutex_lock(&pnr_lock);
                rm_nbd_request(current_nr, infos->reqs);
                pthread_mutex_unlock(&pnr_lock);
                if(current_nr == flag_disc)
                    resend_all_nbd_requests(infos);
                current_nr = NULL;
            }
            if(current_nr != NULL && ntohl(current_nr->len) < SRV_RECV_BUF) {
                size_recv_buf = ntohl(current_nr->len);
            } else {
                size_recv_buf = SRV_RECV_BUF;
            }
        }
    }
}

/* nbd_connect
 *    Connect to server with the specific negotiation protocol of nbd
 *        sock -- socket to nbd server
 *
 *    Return nbd_init_data* containing all the information from server. This
 *    needs to be resend to the client
 */
struct nbd_init_data *nbd_connect(int sock) {
    struct nbd_init_data *nid = (struct nbd_init_data *) malloc(sizeof(struct nbd_init_data));

    printf("Negotiation: ");
    /* Read INIT_PASSWD */
    if (read(sock, &(nid->init_passwd) , sizeof(nid->init_passwd)) < 0)
        printf("Failed/1: %m\n");
    if (strlen(nid->init_passwd)==0)
        printf("Server closed connection\n");
    if (strcmp(nid->init_passwd, INIT_PASSWD))
        printf("INIT_PASSWD bad\n");
    printf(".");

    /* Read cliserv_magic */
    if (read(sock, &(nid->magic), sizeof(nid->magic)) < 0)
        printf("Failed/2: %m\n");
    nid->magic = ntohll(nid->magic);
    if (nid->magic != cliserv_magic)
        printf("Not enough cliserv_magic\n");
    nid->magic = ntohll(nid->magic);
    printf(".");

    /* Read size */
    if (read(sock, &(nid->size), sizeof(nid->size)) < 0)
        printf("Failed/3: %m\n");
    //nid->size = ntohll(nid->size);
    printf(".");

    /* Read flags */
    if (read(sock, &(nid->flags), sizeof(nid->flags)) < 0)
        printf("Failed/4: %m\n");
    //nid->flags = ntohl(nid->flags);
    printf(".");

    /* Read zeros */
    if (read(sock, &(nid->zeros), sizeof(nid->zeros)) < 0)
        printf("Failed/5: %m\n");
    printf("\n");

    return nid;
}

/* create_connect_sock
 *    Create a socket connected to a specific and point.
 *        port -- which port to connect to
 *        addr -- remote IP address 
 *
 *    Return socket file descriptor
 */
int create_connect_sock(int port, char *addr) {
    int sock;
    int err = -1;
    struct sockaddr_in struct_addr;

    sock = socket(PF_INET, SOCK_STREAM, 0);
    if(sock == -1) {
        printf("Unable to create Socket\n");
        exit(1);
    }

    struct_addr.sin_family = AF_INET;
    struct_addr.sin_port = htons(port);
    struct_addr.sin_addr.s_addr = inet_addr(addr);
    memset(struct_addr.sin_zero, 0, sizeof(struct_addr.sin_zero));

    printf("[create_connect_sock] Connect socket to %s\n", addr);
    err = connect(sock, (struct sockaddr *) &struct_addr, sizeof(struct_addr));
    if(err == -1) {
        printf("Server unable to connect\n");
    }

    printf("[create_connect_sock] Connected and ready.\n");
    return sock;
}

/* create_listen_sock
 *    Create a socket listening on a port and bind
 *        port -- which port to listen on
 *        addr -- IP addr to bind on
 *
 *    Return socket file descriptor
 */
int create_listen_sock(int port, int addr) {

    int sock, newfd, s_size;
    int opt = 1;
    struct sockaddr_in struct_addr;

    // New socket
    sock = socket(PF_INET, SOCK_STREAM, 0);
    if(sock == -1) {
        printf("Unable to create Socket\n");
        exit(1);
    }

    struct_addr.sin_family = AF_INET;
    struct_addr.sin_port = htons(port);
    struct_addr.sin_addr.s_addr = ntohl(addr);
    memset(struct_addr.sin_zero, 0, sizeof(struct_addr.sin_zero));
    
    printf("[create_listen_sock] Binding socket to localhost\n");
    if(!bind(sock, (struct sockaddr *) &struct_addr, sizeof(struct_addr)) == -1) {
        printf("Unable to bind socket\n");
        exit(1);
    }

    printf("[create_listen_sock] Listining to localhost\n");
    if(listen(sock,1) == -1) {
        printf("Unable to listen\n");
        exit(1);
    }

    printf("[create_listen_sock] Accepting socket\n");
    s_size = sizeof(struct_addr);
    if((newfd = accept(sock, (struct sockaddr *) &struct_addr, &s_size)) == -1) {
        printf("Accept() failed\n");
        exit(1);
    } 

    setsockopt(newfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    printf("[create_listen_sock] Socket bound and ready. Returning\n");

    return newfd;
}

/* resend_all_nbd_requests
 *     Resend all nbd_requests in memory to nbd server
 *         infos -- struct thread_data
 */
void resend_all_nbd_requests(struct thread_data *infos) {
    struct proxy_nbd_request *current_pnr = *(infos->reqs);
    if(current_pnr == NULL) {
        printf("[resend] No nbd_request in queue\n");
        return;
    }
    do {
        send_to_server(infos, (char*) current_pnr->nr, sizeof(struct nbd_request));
        printf("[resend] nbd_request\n");
    } while((current_pnr = current_pnr->next) != NULL);
}

/* reconnect_server
 *     Clean data structure and reconnect to server
 *         infos -- struct thread_data
 */
void reconnect_server(struct thread_data *infos) {
    // Close current server socket
    close(infos->server_socket);
    // New server socket
    infos->server_socket = create_connect_sock(infos->server_port, infos->server_addr);
    server_connect(infos);
}

/* reconnect_client
 *     Clean data structure and reconnect to client
 *         infos -- struct thread_data
 */
void reconnect_client(struct thread_data *infos) {
    // Close old client socket
    close(infos->client_socket);
    // New client socket
    infos->client_socket = create_listen_sock(infos->listen_port, INADDR_LOOPBACK);
    // Connect client (server negotiation sent to client)
    client_connect(infos);
    // Update proxy_nbd_request first element to NULL (no more elems)
    pthread_mutex_lock(&pnr_lock);
    *(infos->reqs) = NULL; 
    pthread_mutex_unlock(&pnr_lock);
}

/* server_connect
 *     Establish connection to nbd server
 *         infos -- struct thread_data
 */
void server_connect(struct thread_data *infos) {
    // Saving nid information to thread_data
    infos->nid = nbd_connect(infos->server_socket);
    printf("Server Connected\n");
}

/* client_connect
 *    Connect client to server (nbd point of view)
 *        td -- struct thread_data containing thread informations
 */
void client_connect(struct thread_data *td) {
    struct nbd_init_data *nid = td->nid;
    send(td->client_socket, &(nid->init_passwd), sizeof(nid->init_passwd), 0);
    send(td->client_socket, &(nid->magic), sizeof(nid->magic), 0);
    send(td->client_socket, &(nid->size), sizeof(nid->size) + sizeof(nid->flags) + sizeof(nid->zeros) - 4, 0);
    printf("Client Connected\n");
}

/* send_to_client
 *     Send data to client with safe control
 *         infos -- thread_data
 *         buf -- data to send
 *        size -- size of data to send
 *    Return -1 if error detected
 *    Else 0
 */         
int send_to_client(struct thread_data *infos, char *buf, size_t size) {
    int flag = 0;
    int count = RESEND_MAX;
    // Sending to server
    while((send(infos->client_socket, buf, size, 0) == -1)) {
        flag = -1;
        printf("Client disconnected on send(). Reconnecting\n");
        if(--count == 0) {
            sleep(30);
            count = RESEND_MAX;
        }
        // Reconnect to nbd server
        reconnect_client(infos);
    }
    return flag;
}

/* send_to_server
 *     Send data to server with safe control
 *         infos -- thread_data
 *         buf -- data to send
 *        size -- size of data to send
 *    Return -1 if error detected
 *    Else 0
 */
int send_to_server(struct thread_data *infos, char *buf, size_t size) {
    int count = RESEND_MAX;
    int flag = 0;
    // Sending to server
    while((send(infos->server_socket, buf, size, 0) == -1)) {
        flag = -1;
        printf("Server disconnected on send(). Reconnecting\n");
        if(--count == 0) {
            sleep(30);
            count = RESEND_MAX;
        }
        // Reconnect to nbd server
        reconnect_server(infos);
    }
    return flag;
}

/* main
 *    entry point
 */
int main(int argc, char *argv[]) {

    int server_port, listen_port, client_socket, server_socket;
    int client_th, server_th, rc;
    char *server_address;
    void *status;
    pthread_t threads[NUM_THREADS];
    struct nbd_init_data *nid;

    if(argc < 2) {
        printf("Usage : nbd-proxy server_address server_port listening_port\n");
        exit(1);
    }

    server_address = argv[1];
    server_port = atoi(argv[2]);
    listen_port = atoi(argv[3]);

    struct thread_data *th_d1 = 
        (struct thread_data *) malloc(sizeof(struct thread_data));
    struct proxy_nbd_request *pnr = NULL;

    printf("[main] Creating sockets\n");
    // New server socket
    server_socket = create_connect_sock(server_port, server_address);

    printf("[main] nbd_connect\n");
    // Negotiate with nbd server
    nid = nbd_connect(server_socket);

    // New client socket
    client_socket = create_listen_sock(listen_port, INADDR_LOOPBACK);

    th_d1->client_socket = client_socket;
    th_d1->server_socket = server_socket;
    th_d1->server_port = server_port;
    th_d1->server_addr = server_address;
    th_d1->listen_port = listen_port;
    th_d1->nid = nid;
    th_d1->reqs = &pnr;

    // Connect client with server negotiation data
    client_connect(th_d1);

    printf("[main] Spawning thread server\n");
    server_th = pthread_create(&threads[1], NULL, server_to_client, (void *) th_d1);
    printf("[main] Spawning thread client\n");
    client_th = pthread_create(&threads[0], NULL, client_to_server, (void *) th_d1);

    rc = pthread_join(threads[0], &status);
    if (rc) {
        printf("ERROR, return code from pthread_join is %d\n", rc);
    }
    rc = pthread_join(threads[1], &status);
    if (rc) {
        printf("ERROR, return code from pthread_join is %d\n", rc);
    }

    printf("[main] main is dying\n");
    pthread_exit(NULL);
}
