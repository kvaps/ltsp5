#define NUM_THREADS 2
#define INIT_PASSWD "NBDMAGIC"
#define SRV_RECV_BUF 1024
#define htonll ntohll
#define RESEND_MAX 3

#ifdef DEBUG
    #define PRINT_DEBUG(...) fprintf(stderr, __VA_ARGS__)
#else
    #define PRINT_DEBUG(...)
#endif

typedef unsigned long long u64;
typedef unsigned long u32;

u64 cliserv_magic = 0x00420281861253LL;
pthread_mutex_t pnr_lock;

struct proxy_nbd_request {
    struct nbd_request *nr;
    struct proxy_nbd_request *next;
};

struct thread_data {
    int client_socket;
    int server_socket;
    int server_port;
    int listen_port;
    int bsize;
    char *server_addr;
    struct nbd_init_data *nid;
    struct proxy_nbd_request **reqs;
};

struct nbd_init_data {
    char init_passwd[8];
    u64 magic;
    u64 size;
    u32 flags;
    char zeros[124];
};

struct nbd_init_data *nbd_connect(int);
void client_connect(struct thread_data *);
void server_connect(struct thread_data *);
void reconnect_server(struct thread_data *);
void reconnect_client(struct thread_data *);
void resend_all_nbd_requests(struct thread_data *, struct nbd_request*);
int send_to_server(struct thread_data *, char *, size_t);
int send_to_client(struct thread_data *, char *, size_t);

/* add_nbd_request
 *  Adding nbd_request to the chained list (proxy_nbd_request)
 *      r -- nbd_request to add into the chained list
 *      first -- first element of the list
 */
void add_nbd_request(struct nbd_request* nr, struct proxy_nbd_request **first) {
    struct proxy_nbd_request *new_pnr = 
        (struct proxy_nbd_request*) malloc(sizeof(struct proxy_nbd_request));
    struct proxy_nbd_request *current_pnr = *first;

    if(*first == NULL) {
        *first = new_pnr;
    } else {
        while(current_pnr->next != NULL) 
            current_pnr = current_pnr->next;
        current_pnr->next = new_pnr;
    }

    new_pnr->nr = nr;
    new_pnr->next = NULL;
    PRINT_DEBUG("[add_nbd_request] nbd_request added to linked list\n");
}

/* get_nbd_request_by_handle
 *  Return nbd_request for a specific handle (nbd_request->handle)
 *      handle -- search param
 *      first -- first proxy_nbd_request of the chained list
 */
struct nbd_request *get_nbd_request_by_handle(char *handle, struct proxy_nbd_request **first) {
    struct proxy_nbd_request *current_pnr = *first;
    if(current_pnr != NULL) {
        do {
            if(!strncmp(handle, current_pnr->nr->handle, sizeof((*first)->nr->handle))) {
                //PRINT_DEBUG("[get_nbd_request_by_handle] nbd_request found!\n");
                return current_pnr->nr;
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
                PRINT_DEBUG("[rm_nbd_request] removing nbd_request\n");
                if(current_pnr == *first) {
                    *first = current_pnr->next;
                } else {
                    previous_pnr->next = current_pnr->next;
                    free(current_pnr);
                }
                break;
            }
            previous_pnr = current_pnr;
        } while((current_pnr = current_pnr->next) != NULL);
    } else {
        PRINT_DEBUG("[rm_nbd_request] proxy_nbd_request empty... \n");
    }
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

/* handle_to_string
 *  FOR DEBUG PURPOSE ONLY
 */
char *handle_to_string(char *handle) {
    char *res = (char *) malloc(128);

    sprintf(res, "%X, %X, %X, %X, %X, %X, %X, %X\n", 
            handle[0],handle[1],handle[2],handle[3],
            handle[4],handle[5],handle[6],handle[7]);
    return res;
}

