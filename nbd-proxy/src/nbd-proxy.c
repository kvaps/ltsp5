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
#include <netdb.h>

#include "nbd-proxy.h"

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
    }
}

/* client_to_server
 *    This thread acts as a proxy from client to server
 *        data -- struct containing shared data between threads (thread_data)
 */
void *client_to_server(void *data) {
    struct thread_data *infos = (struct thread_data*) data;
    /* nbd_request size : 28 bytes */
    char recv_buf[sizeof(struct nbd_request)];
    ssize_t bytes_read;

    PRINT_DEBUG("[t_client] Init mainloop\n");
    while(1) {
        PRINT_DEBUG("[t_client] recv mode\n");
        bytes_read = recv(infos->client_socket, recv_buf, sizeof(recv_buf), MSG_WAITALL);
        if(bytes_read <= 0) {
            PRINT_DEBUG("[t_client] Client disconnected on recv(). Dying...\n");
            cleaning(infos);
            exit(EXIT_FAILURE);
        }

        struct nbd_request *new_req = (struct nbd_request*) malloc(sizeof(struct nbd_request));
        memcpy(new_req, recv_buf, sizeof(struct nbd_request));

        /* Checking if data is a valid nbd_request */
        if(new_req->magic == ntohl(NBD_REQUEST_MAGIC)) {
            /* NBD_CMD_READ from client */
            if(new_req->type == ntohl(NBD_CMD_READ)) {
                PRINT_DEBUG("[t_client] Got nbd_request : handle(%s) of len(%u) and from(%lu)\n", 
                        handle_to_string(new_req->handle), ntohl(new_req->len), ntohll(new_req->from));

                pthread_mutex_lock(&net_lock); 
                if(send_to_server(infos, recv_buf, bytes_read) == -1)
                    PRINT_DEBUG("[t_client] Failed sending nbd_request to server\n");
                /* Adding nbd_request received to queue (thread safe) */
                pthread_mutex_unlock(&net_lock); 
                add_nbd_request(new_req, infos->reqs);
            /* NBD_CMD_DISC from client */
            } else if(new_req->type == ntohl(NBD_CMD_DISC)) {
                PRINT_DEBUG("[t_client] NBD_DISCONNECT from client. Cleaning\n");
                /* On thin client infrastructure, this should not happen. Quitting properly */
                cleaning(infos);
                exit(EXIT_SUCCESS);
            }
        }
    }
    PRINT_DEBUG("[t_client] WTF... client_to_server outside of while(1)\n");
}

/* server_to_client
 *    This thread acts as a proxy from server to client
 *        data -- struct containing shared data between threads (thread_data)
 */
void *server_to_client(void *data) {
    struct thread_data *infos = (struct thread_data*) data;
    struct nbd_reply nr;
    struct nbd_request *current_nr = NULL;
    struct nbd_request *nr_flag_disconnect = NULL;
    char recv_buf[SRV_RECV_BUF * SEND_BUF_FACTOR];
    char send_buf[SRV_RECV_BUF * SEND_BUF_FACTOR];
    ssize_t bytes_read;
    ssize_t r_bytes;
    ssize_t total_size = 0;
    int discard_reply_flag = 0;
    int out_flag = 0;
    int size_recv_buf = SRV_RECV_BUF;
    int sockopt = MSG_WAITALL;

    /* Main loop */
    PRINT_DEBUG("[t_server] Init mainloop\n");
    while(1) {
        PRINT_DEBUG("\n[t_server] recv mode (%d)\n", size_recv_buf);
        /* Assurance not to bust recv_buf real size */
        if(size_recv_buf > sizeof(recv_buf))
            size_recv_buf = SRV_RECV_BUF * SEND_BUF_FACTOR;
        /* Receiving data from nbd-server */
        bytes_read = recv(infos->server_socket, recv_buf, size_recv_buf, sockopt);
        /* Keep track of bytes read for used to update nbd_request len */
        r_bytes = bytes_read;
        if(bytes_read <= 0) {
            PRINT_DEBUG("[t_server] Server disconnected on recv() (bytes_read = %d). Reconnecting\n", 
                    (int) bytes_read);
            reconnect_server(infos);
            /* Sending last nbd_request modified */
            if(current_nr != NULL) {
                PRINT_DEBUG("[t_server] nbd_request count : %d\n", count_nbd_request(infos->reqs));
                send_to_server(infos, (char *) current_nr, sizeof(struct nbd_request));
                PRINT_DEBUG("[t_server] Last known nbd_request sent\n");
                PRINT_DEBUG("|-- nbd_request : handle(%s) of len(%u) and from(%lu)\n", 
                        handle_to_string(current_nr->handle), ntohl(current_nr->len), 
                        ntohll(current_nr->from));

                nr_flag_disconnect = current_nr;
                discard_reply_flag = sizeof(struct nbd_reply);
                size_recv_buf = ntohl(current_nr->len) + sizeof(struct nbd_reply);
                sockopt = MSG_WAITALL;
            } else {
                PRINT_DEBUG("[t_server] Resending all nbd_request after recv error from server\n");
                resend_all_nbd_requests(infos);
                size_recv_buf = SRV_RECV_BUF;
                sockopt = 0;
                total_size = 0;
            }
            out_flag = 0;
            continue;
        }

        /* Getting nbd_reply */
        memcpy(&nr, recv_buf, sizeof(struct nbd_reply));
        /* If the packet received contain a valid nbd_reply */
        if(nr.magic == ntohl(NBD_REPLY_MAGIC)) {
            PRINT_DEBUG("[t_server] Got nbd_reply : handle(%s)\n", handle_to_string(nr.handle));
            /* Getting corresponding nbd_request in queue (Thread safe) */
            current_nr = get_nbd_request_by_handle(nr.handle, infos->reqs);

            if(current_nr == NULL)
                PRINT_DEBUG("[t_server] nbd_reply handle unknown by proxy\n");
            else {
                /* Adapting recv buffer size to nbd_request len */
                size_recv_buf = (ntohl(current_nr->len) - (bytes_read - sizeof(struct nbd_reply)));
                sockopt = MSG_WAITALL;
                if(size_recv_buf == 0) {
                    size_recv_buf = SRV_RECV_BUF;
                    sockopt = 0;
                    out_flag = 1;
                }
                /* Ignoring nbd_reply size for len of nbd_request */
                r_bytes -= sizeof(struct nbd_reply);
            }
        } else if(current_nr == NULL) {
            PRINT_DEBUG("[t_server] Fatal error: No nbd_reply received and no nbd_request to serve. Bad!\n");
        } else {
            /* Ready to send information to client */
            out_flag = 1;
        }

        /* Filling send_buf */
        PRINT_DEBUG("Copy %ld bytes to send_buf from %ld of send_buf\n", bytes_read, total_size);
        memcpy(send_buf + total_size, recv_buf, bytes_read);
        total_size += bytes_read;

        PRINT_DEBUG("[t_server] nbd_request queue count : %d\n", count_nbd_request(infos->reqs));
        /* Sending to client when all nbd_request's data received */
        if(out_flag) {
            PRINT_DEBUG("[t_server] Sending %d bytes to client\n",(int)total_size - discard_reply_flag);
            /* Sending data to client (P -> C). On client disconnect, nbd proxy STOP! */
            send_to_client(infos, send_buf + discard_reply_flag, total_size - discard_reply_flag);
            total_size = 0;
            discard_reply_flag = 0;
            out_flag = 0;
        }

        /* Updating current nbd_request.len of received bytes (r_bytes) */
        if(current_nr != NULL) {
            current_nr->len = htonl(ntohl(current_nr->len) - r_bytes);
            current_nr->from = htonll(ntohll(current_nr->from) + r_bytes);

            if((current_nr->len) == 0) {
                /* Removing nbd_request from queue. Not useful anymore (Thread safe) */
                rm_nbd_request(current_nr, infos->reqs);
                if(current_nr == nr_flag_disconnect) {
                    PRINT_DEBUG("[t_server] Last known nbd_request done! Resending queue (count : %d)\n",
                            count_nbd_request(infos->reqs));
                    resend_all_nbd_requests(infos);
                    nr_flag_disconnect = NULL;
                }
                current_nr = NULL;
                sockopt = 0;
                size_recv_buf = SRV_RECV_BUF;
            }
        }
    }
    PRINT_DEBUG("[t_server] WTF... server_to_client outside of while(1)\n");
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
 *        addr -- remote IP address (string format)
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

    /* New socket */
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
 */
void resend_all_nbd_requests(struct thread_data *infos) {
    pthread_mutex_lock(&data_lock);
    struct proxy_nbd_request *current_pnr = *(infos->reqs);
    if(current_pnr == NULL) {
        PRINT_DEBUG("[resend] No nbd_request in queue\n");
        pthread_mutex_unlock(&data_lock);
        return;
    }

    do {
        send_to_server(infos, (char*) current_pnr->nr, sizeof(struct nbd_request));
        PRINT_DEBUG("[resend] nbd_request : handle(%s) of len(%u) and from(%lu)\n",
                handle_to_string(current_pnr->nr->handle), ntohl(current_pnr->nr->len),
                ntohll(current_pnr->nr->from));
    } while((current_pnr = current_pnr->next) != NULL);
    pthread_mutex_unlock(&data_lock);
}

/* reconnect_server
 *     Clean data structure and reconnect to server (Thread safe : net_lock)
 *         infos -- struct thread_data
 */
void reconnect_server(struct thread_data *infos) {
    pthread_mutex_lock(&net_lock);
    /* Close current server socket */
    close(infos->server_socket);
    /* New server socket */
    infos->server_socket = create_connect_sock(infos->server_port, infos->server_addr);
    server_connect(infos);
    pthread_mutex_unlock(&net_lock);
    PRINT_DEBUG("[reconnect_server] Reconnected to server\n");
}

/* server_connect
 *     Establish connection to nbd server
 *         infos -- struct thread_data
 */
void server_connect(struct thread_data *infos) {
    /* Saving nid information to thread_data */
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
void send_to_client(struct thread_data *infos, char *buf, size_t size) {
    /* Sending to client */
    if((send(infos->client_socket, buf, size, 0) == -1)) {
        PRINT_DEBUG("Client disconnected on send(). Dying...\n");
        cleaning(infos);
        exit(EXIT_FAILURE);
    }
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
    int flag = 0;
    /* Sending to server */
    if((send(infos->server_socket, buf, size, 0) == -1))
        flag = -1;
    return flag;
}

/* main
 *    entry point
 */
int main(int argc, char *argv[]) {
    int server_port, listen_port, client_socket, server_socket;
    int client_th, server_th, rc;
    char server_address[INET_ADDRSTRLEN];
    void *status;
    struct nbd_init_data *nid;
    struct sched_param param_client;
    struct sched_param param_server;
    pthread_attr_t pattr_client;
    pthread_attr_t pattr_server;
    pthread_t threads[NUM_THREADS];

    if(argc < 4) {
        printf("Usage : nbd-proxy server_address server_port listening_port\n");
        exit(1);
    }

    /* Server addr verification */
    if(inet_pton(AF_INET, argv[1], &server_address) == 0) {
        struct hostent *host = gethostbyname(argv[1]);
        if(host == NULL) { 
            printf("Hostname or ipaddr not understand : %s\n", server_address);
            exit(EXIT_FAILURE);
        }
        memcpy(server_address, inet_ntoa(*((struct in_addr **)host->h_addr_list)[0]), INET_ADDRSTRLEN);
    } else
        memcpy(server_address, argv[1], INET_ADDRSTRLEN);

    /* Port verification */
    server_port = atoi(argv[2]);
    listen_port = atoi(argv[3]);
    if((server_port < 1 || server_port > 65535) || (listen_port < 1 || listen_port > 65535)) {
        printf("Bad port range\n");
        exit(EXIT_FAILURE);
    }

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
    /* New server socket */
    server_socket = create_connect_sock(server_port, server_address);

    PRINT_DEBUG("[main] nbd_connect\n");
    /* Negotiate with nbd server */
    nid = nbd_connect(server_socket);

    /* New client socket */
    client_socket = create_listen_sock(listen_port, INADDR_LOOPBACK);

    th_d1->client_socket = client_socket;
    th_d1->server_socket = server_socket;
    th_d1->server_port = server_port;
    th_d1->server_addr = server_address;
    th_d1->listen_port = listen_port;
    th_d1->nid = nid;
    th_d1->reqs = &pnr;

    /* Connect client with server negotiation data */
    client_connect(th_d1);

    pthread_attr_init(&pattr_client);
    pthread_attr_init(&pattr_server);
   
    /* Allowing sched options to be changed */
    pthread_attr_setinheritsched(&pattr_client, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setinheritsched(&pattr_server, PTHREAD_EXPLICIT_SCHED);

    /* Scheduling to SCHED_FIFO */
    pthread_attr_setschedpolicy(&pattr_client, SCHED_FIFO);
    pthread_attr_setschedpolicy(&pattr_server, SCHED_FIFO);
    
    /* Thread priority */
    param_client.sched_priority = 5;
    param_server.sched_priority = 10;
    
    pthread_attr_setschedparam(&pattr_client, &param_client);
    pthread_attr_setschedparam(&pattr_server, &param_server);
   
    /* Creating threads */
    client_th = pthread_create(&threads[0], &pattr_client, client_to_server, (void *) th_d1);
    server_th = pthread_create(&threads[1], &pattr_server, server_to_client, (void *) th_d1);

    /* Joining threads */
    PRINT_DEBUG("[main] Spawning thread client\n");
    rc = pthread_join(threads[0], &status);
    if (rc) {
        PRINT_DEBUG("ERROR, return code from pthread_join is %d\n", rc);
    }
    PRINT_DEBUG("[main] Spawning thread server\n");
    rc = pthread_join(threads[1], &status);
    if (rc) {
        PRINT_DEBUG("ERROR, return code from pthread_join is %d\n", rc);
    }

    PRINT_DEBUG("[main] main is dying\n");
    pthread_exit(NULL);
}
