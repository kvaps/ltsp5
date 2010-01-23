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
#include <signal.h>
#include <unistd.h>
#include <stdarg.h>

#include "nbd-proxy.h"

/* sighandler
 *     Handles the different signal during the execution
 *         sig -- the type of signal emitted
 *             SIGINT is handled, it terminates the program
 */
void sighandler(int sig) {
    switch(sig) {
        case SIGINT:
            printf("Ctrl+c pressed, exiting\n");
            exit(EXIT_FAILURE);
            break;
    }
}

/* client_to_server
 *    This thread acts as a proxy from client to server
 *        data -- struct containing shared data between threads (thread_data)
 */
void *client_to_server(void *data) {
    struct thread_data *infos = (struct thread_data*) data;
    // nbd_request size : 28 bytes
    char recv_buf[sizeof(struct nbd_request)];
    ssize_t bytes_read;

    PRINT_DEBUG("[t_client] Init mainloop\n");
    while(1) {
        bytes_read = recv(infos->client_socket, recv_buf, sizeof(recv_buf), 0);
        if(bytes_read <= 0) {
            PRINT_DEBUG("[t_client] Client disconnected on recv(). Reconnecting\n");
            pthread_mutex_lock(&proxy_lock);
            reconnect_client(infos);
            pthread_mutex_unlock(&proxy_lock);
            continue;
        }

        struct nbd_request *new_req = (struct nbd_request*) malloc(sizeof(struct nbd_request));
        memcpy(new_req, recv_buf, sizeof(recv_buf));
        // Grab lock

        // Checking if data is a valid nbd_request
        if(new_req->magic == ntohl(NBD_REQUEST_MAGIC)) {
            // NBD_CMD_READ from client    
            if(new_req->type == ntohl(NBD_CMD_READ)) {
                PRINT_DEBUG("[t_client] Got nbd_request : handle(%s) of len(%u) and from(%lu)\n", 
                        handle_to_string(new_req->handle), ntohl(new_req->len), ntohll(new_req->from));
                // Adding nbd_request received to queue (atomic action)
                pthread_mutex_lock(&proxy_lock);
                add_nbd_request(new_req, infos->reqs);
                pthread_mutex_unlock(&proxy_lock);
            // NBD_CMD_DISC from client
            } else if(new_req->type == ntohl(NBD_CMD_DISC)) {
                PRINT_DEBUG("[t_client] NBD_DISCONNECT from client. Cleaning\n");
                // On thin client infrastructure, this should not happen
                pthread_mutex_lock(&proxy_lock);
                reconnect_client(infos);
                pthread_mutex_unlock(&proxy_lock);
                continue;
            }
        }

        // Sending received data to server
        if(send_to_server(infos, recv_buf, bytes_read) == -1) {
            PRINT_DEBUG("[t_client] Client detect server disconnect. Resendig all nbd_request\n");
            pthread_mutex_lock(&proxy_lock);
            resend_all_nbd_requests(infos, new_req);
            pthread_mutex_unlock(&proxy_lock);
        }
        // Release lock
    }
    PRINT_DEBUG("[t_client] WTF client_to_server outside while\n");
}

/* server_to_client
 *    This thread acts as a proxy from server to client
 *        data -- struct containing shared data between threads (thread_data)
 */
void *server_to_client(void *data) {
    struct thread_data *infos = (struct thread_data*) data;
    struct nbd_reply nr;
    struct nbd_request *current_nr = NULL;
    struct nbd_request *flag_disc = NULL;
    char recv_buf[SRV_RECV_BUF];
    ssize_t bytes_read;
    ssize_t r_bytes;
    int discard_reply_flag = 0;
    int size_recv_buf = SRV_RECV_BUF;
    char send_buf[SRV_RECV_BUF * SEND_BUF_FACTOR];
    ssize_t total_size = 0;

    // Main loop
    PRINT_DEBUG("[t_server] Init mainloop\n");
    while(1) {
        bytes_read = recv(infos->server_socket, recv_buf, size_recv_buf, MSG_WAITALL);
        // Keep track of bytes read for specific use
        r_bytes = bytes_read;
        if(bytes_read <= 0) {
            PRINT_DEBUG("[t_server] Server disconnected on recv() (bytes_read = %d). Reconnecting\n", 
                    (int) bytes_read);
            pthread_mutex_lock(&proxy_lock);
            reconnect_server(infos);
            pthread_mutex_unlock(&proxy_lock);
            // Sending last nbd_request modified
            if(current_nr != NULL) {
                PRINT_DEBUG("[t_server] nbd_request count : %d\n", count_nbd_request(infos->reqs));
                pthread_mutex_lock(&proxy_lock);
                send_to_server(infos, (char *) current_nr, sizeof(struct nbd_request));
                pthread_mutex_unlock(&proxy_lock);

                flag_disc = current_nr;
                discard_reply_flag = sizeof(struct nbd_reply);
                PRINT_DEBUG("[t_server] Last known nbd_request sent\n");
                PRINT_DEBUG("|-- nbd_request : handle(%s) of len(%u) and from(%lu)\n", 
                        handle_to_string(current_nr->handle), ntohl(current_nr->len), 
                        ntohll(current_nr->from));
            } else {
                PRINT_DEBUG("[t_server] Resending all nbd_request after recv error from server\n");
                pthread_mutex_lock(&proxy_lock);
                resend_all_nbd_requests(infos, NULL);
                pthread_mutex_unlock(&proxy_lock);
            }
            total_size = 0;
            continue;
        }

        PRINT_DEBUG("[t_server] Bytes read : %d\n", (int)bytes_read);

        // Getting nbd_reply
        memcpy(&nr, recv_buf, sizeof(struct nbd_reply));
        // If the packet received contain a valid nbd_reply
        if(nr.magic == ntohl(NBD_REPLY_MAGIC)) {
            PRINT_DEBUG("[t_server] Got nbd_reply : handle(%s)\n", handle_to_string(nr.handle));
            // Locking nbd_request queue
            pthread_mutex_lock(&proxy_lock);
            if((current_nr = get_nbd_request_by_handle(nr.handle, infos->reqs)) == NULL) {
                PRINT_DEBUG("[t_server] nbd_reply handle unknown\n");
            }
            pthread_mutex_unlock(&proxy_lock);

            // Ignoring nbd_reply size for len in nbd_request
            r_bytes -= sizeof(struct nbd_reply);
        } else if(current_nr == NULL) {
            PRINT_DEBUG("[t_server] Fatal error: No nbd_reply received and no nbd_request to serve\n");
        }

        // Filling send_buf
        PRINT_DEBUG("Copy %d bytes to send_buf at %d pos of send_buf\n", (int)bytes_read, (int)total_size);
        memcpy(send_buf + total_size, recv_buf, bytes_read);
        total_size += bytes_read;

        PRINT_DEBUG("[t_server] nbd_request in queue : %d\n", count_nbd_request(infos->reqs));
        // Sending to client when size of send_buf is reach or the end of transmission
        if(total_size >= sizeof(send_buf) || bytes_read < SRV_RECV_BUF || ntohl(current_nr->len) <= SRV_RECV_BUF) {
            PRINT_DEBUG("[t_server] Sending %d bytes to client\n",(int) total_size - discard_reply_flag);
            // Sending data to client (P -> C) (thread safe)
            send_to_client(infos, send_buf + discard_reply_flag, total_size - discard_reply_flag);
            total_size = 0;
            discard_reply_flag = 0;
        }

        // Updating current nbd_request.len of received bytes (r_bytes)
        if(current_nr != NULL) {
            current_nr->len = htonl(ntohl(current_nr->len) - r_bytes);
            current_nr->from = htonll(ntohll(current_nr->from) + r_bytes);

            if((current_nr->len) == 0) {
                pthread_mutex_lock(&proxy_lock);
                // Removing nbd_request from queue. Not useful anymore (atomic action)
                rm_nbd_request(current_nr, infos->reqs);
                if(current_nr == flag_disc) {
                    PRINT_DEBUG("[t_server] Last known nbd_request done. Resending queue (count : %d)\n",count_nbd_request(infos->reqs));
                    resend_all_nbd_requests(infos, NULL);
                    flag_disc = NULL;
                }
                pthread_mutex_unlock(&proxy_lock);
                current_nr = NULL;
            }
            if(current_nr != NULL && ntohl(current_nr->len) < SRV_RECV_BUF) {
                size_recv_buf = ntohl(current_nr->len);
                PRINT_DEBUG("[t_server] Adjusting recv buffer to : %d\n", size_recv_buf);
            } else {
                size_recv_buf = SRV_RECV_BUF;
            }
        }
    }
    PRINT_DEBUG("[t_server] WTF server_to_client outside while\n");
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

    PRINT_DEBUG("[nbd_connect] Negotiation: ");
    /* Read INIT_PASSWD */
    if (read(sock, &(nid->init_passwd) , sizeof(nid->init_passwd)) < 0)
        PRINT_DEBUG("[nbd_connect] Failed/1: %m\n");
    if (strlen(nid->init_passwd)==0)
        PRINT_DEBUG("[nbd_connect] Server closed connection\n");
    if (strcmp(nid->init_passwd, INIT_PASSWD))
        PRINT_DEBUG("[nbd_connect] INIT_PASSWD bad\n");
    PRINT_DEBUG(".");

    /* Read cliserv_magic */
    if (read(sock, &(nid->magic), sizeof(nid->magic)) < 0)
        PRINT_DEBUG("[nbd_connect] Failed/2: %m\n");
    nid->magic = ntohll(nid->magic);
    if (nid->magic != cliserv_magic)
        PRINT_DEBUG("[nbd_connect] Not enough cliserv_magic\n");
    nid->magic = ntohll(nid->magic);
    PRINT_DEBUG(".");

    /* Read size */
    if (read(sock, &(nid->size), sizeof(nid->size)) < 0)
        PRINT_DEBUG("[nbd_connect] Failed/3: %m\n");
    PRINT_DEBUG(".");

    /* Read flags */
    if (read(sock, &(nid->flags), sizeof(nid->flags)) < 0)
        PRINT_DEBUG("[nbd_connect] Failed/4: %m\n");
    PRINT_DEBUG(".");

    /* Read zeros */
    if (read(sock, &(nid->zeros), sizeof(nid->zeros)) < 0)
        PRINT_DEBUG("[nbd_connect] Failed/5: %m\n");
    PRINT_DEBUG("\n");

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
    int opt = 1;
    struct sockaddr_in struct_addr;

    sock = socket(PF_INET, SOCK_STREAM, 0);
    if(sock == -1) {
        PRINT_DEBUG("Unable to create connect socket\n");
        exit(1);
    }
    
    if((setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) == -1)
        perror("setsockopt");

    struct_addr.sin_family = AF_INET;
    struct_addr.sin_port = htons(port);
    struct_addr.sin_addr.s_addr = inet_addr(addr);
    memset(struct_addr.sin_zero, 0, sizeof(struct_addr.sin_zero));

    PRINT_DEBUG("[create_connect_sock] Connect socket to %s\n", addr);
    err = connect(sock, (struct sockaddr *) &struct_addr, sizeof(struct_addr));
    if(err == -1) {
        PRINT_DEBUG("Server unable to connect\n");
    }

    PRINT_DEBUG("[create_connect_sock] Connected and ready.\n");
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
    int sock, newfd;
    socklen_t s_size;
    int opt = 1;
    struct sockaddr_in struct_addr;

    // New socket
    if((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        PRINT_DEBUG("Unable to create listen socket\n");
        exit(EXIT_FAILURE);
    }
    
    struct_addr.sin_family = AF_INET;
    struct_addr.sin_port = htons(port);
    struct_addr.sin_addr.s_addr = ntohl(addr);
    memset(struct_addr.sin_zero, 0, sizeof(struct_addr.sin_zero));

    if((setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) == -1)
        perror("setsockopt");

    PRINT_DEBUG("[create_listen_sock] Binding socket to localhost\n");
    if(bind(sock, (struct sockaddr *) &struct_addr, sizeof(struct_addr)) == -1) {
        perror("bind");
        PRINT_DEBUG("Unable to bind socket\n");
        exit(EXIT_FAILURE);
    }

    PRINT_DEBUG("[create_listen_sock] Listining to localhost\n");
    if(listen(sock,1) == -1) {
        perror("listen");
        PRINT_DEBUG("Unable to listen\n");
        exit(EXIT_FAILURE);
    }
#ifndef DEBUG
    /* Send SIGHUP to detach the parent process */
    kill(getppid(), SIGHUP);
#endif


    PRINT_DEBUG("[create_listen_sock] Accepting socket\n");
    s_size = sizeof(struct_addr);
    if((newfd = accept(sock, (struct sockaddr *) &struct_addr, &s_size)) == -1) {
        perror("accept");
        PRINT_DEBUG("Accept() failed\n");
        exit(EXIT_FAILURE);
    } 

    if((setsockopt(newfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) == -1)
        perror("setsockopt");
    PRINT_DEBUG("[create_listen_sock] Socket bound and ready. Returning\n");

    return newfd;
}

/* resend_all_nbd_requests
 *     Resend all nbd_requests in memory to nbd server
 *         infos -- struct thread_data
 *         except_nr -- nbd_request NOT to send to server
 */
void resend_all_nbd_requests(struct thread_data *infos, struct nbd_request *except_nr) {
    struct proxy_nbd_request *current_pnr = *(infos->reqs);
    if(current_pnr == NULL) {
        PRINT_DEBUG("[resend] No nbd_request in queue\n");
        return;
    }
    do {
        // Don't send specific nbd_request
        if(current_pnr->nr == except_nr)
            continue;
        send_to_server(infos, (char*) current_pnr->nr, sizeof(struct nbd_request));
        PRINT_DEBUG("[resend] nbd_request : handle(%s) of len(%u) and from(%lu)\n",
                handle_to_string(current_pnr->nr->handle), ntohl(current_pnr->nr->len),
                ntohll(current_pnr->nr->from));
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
    PRINT_DEBUG("[reconnect_server] Reconnected to server\n");
}

/* reconnect_client
 *     Clean data structure and reconnect to client
 *         infos -- struct thread_data
 */
void reconnect_client(struct thread_data *infos) {
    // Close old client socket
    shutdown(infos->client_socket, SHUT_RDWR);
    close(infos->client_socket);
    // New client socket
    infos->client_socket = create_listen_sock(infos->listen_port, INADDR_LOOPBACK);
    // Connect client (server negotiation sent to client)
    client_connect(infos);
    // Update proxy_nbd_request first element to NULL (no more elems)
    pthread_mutex_lock(&proxy_lock);
    PRINT_DEBUG("[reconnect_client] Cleaning nbd_request queue\n");
    *(infos->reqs) = NULL; 
    pthread_mutex_unlock(&proxy_lock);
    PRINT_DEBUG("[reconnect_client] Reconnected to client\n");
}

/* server_connect
 *     Establish connection to nbd server
 *         infos -- struct thread_data
 */
void server_connect(struct thread_data *infos) {
    // Saving nid information to thread_data
    infos->nid = nbd_connect(infos->server_socket);
    PRINT_DEBUG("Server Connected\n");
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
    PRINT_DEBUG("Client Connected\n");
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
        PRINT_DEBUG("Client disconnected on send(). Reconnecting\n");
        if(--count == 0) {
            PRINT_DEBUG("After 3 times... Waiting 30 seconds\n");
            sleep(30);
            count = RESEND_MAX;
        }
        // Reconnect to nbd server
        pthread_mutex_lock(&proxy_lock);
        reconnect_client(infos);
        pthread_mutex_unlock(&proxy_lock);
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
        PRINT_DEBUG("Server disconnected on send(). Reconnecting\n");
        if(--count == 0) {
            PRINT_DEBUG("After 3 times... Waiting 30 seconds\n");
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

    if(argc < 4) {
        printf("Usage : nbd-proxy server_address server_port listening_port\n");
        exit(1);
    }

    server_address = argv[1];
    server_port = atoi(argv[2]);
    listen_port = atoi(argv[3]);

    struct thread_data *th_d1 = (struct thread_data *) malloc(sizeof(struct thread_data));
    struct proxy_nbd_request *pnr = NULL;

    signal(SIGINT, sighandler);

#ifndef DEBUG
    /* Our process ID and Session ID */
    pid_t pid, sid;

    /* Fork off the parent process */
    pid = fork();
    if (pid < 0) {
        exit(EXIT_FAILURE);
    }
    /* If we got a good PID, then
       we can exit the parent process. */
    if (pid > 0) {
        /* Wait until the child process is ready to process the requests and exit */
        pause();
        exit(EXIT_SUCCESS);
    }

    /* Change the file mode mask */
    umask(0);

    /* Create a new SID for the child process */
    sid = setsid();
    if (sid < 0) {
        /* Log the failure */
        exit(EXIT_FAILURE);
    }

    /* Change the current working directory */
    if ((chdir("/")) < 0) {
        /* Log the failure */
        exit(EXIT_FAILURE);
    }

    /* Close out the standard file descriptors */
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
#endif

    PRINT_DEBUG("[main] Creating sockets\n");
    // New server socket
    server_socket = create_connect_sock(server_port, server_address);

    PRINT_DEBUG("[main] nbd_connect\n");
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

    PRINT_DEBUG("[main] Spawning thread server\n");
    server_th = pthread_create(&threads[1], NULL, server_to_client, (void *) th_d1);
    PRINT_DEBUG("[main] Spawning thread client\n");
    client_th = pthread_create(&threads[0], NULL, client_to_server, (void *) th_d1);

    rc = pthread_join(threads[0], &status);
    if (rc) {
        PRINT_DEBUG("ERROR, return code from pthread_join is %d\n", rc);
    }
    rc = pthread_join(threads[1], &status);
    if (rc) {
        PRINT_DEBUG("ERROR, return code from pthread_join is %d\n", rc);
    }

    PRINT_DEBUG("[main] main is dying\n");
    pthread_exit(NULL);
}
