struct ldm_info {
    char    *server;
    char    *vty;
    char    *display;
    char    *fontpath;
    char    *override_port;
    char    *authfile;
    char    *username;
    char    *password;
    char    *lang;
    char    *session;
    char    *sound_daemon;
    char    *greeter_prog;
    char    *control_socket;
    char    *ipaddr;
    int     autologin;
    int     sound;
    int     localdev;
    int     directx;
    int     sshfd;
    pid_t   sshpid;
    pid_t   xserverpid;
};

/* forward decls */

void usage();
void die(char *msg);
int ldm_getenv_bool(const char *name);
pid_t ldm_spawn (char *const argv[]);
int ldm_wait (pid_t pid);
void create_xauth();
void launch_x();
char *get_userid();
char *get_passwd();
void spawn_ssh(int fd);
int ssh_session();
int ssh_endsession();
void get_ipaddr();

extern struct ldm_info ldminfo;
extern FILE *ldmlog;

#define NOWAIT 0
#define WAIT 1
#define MAXARGS 32
#define SENTINEL "LTSPROCKS"
#define MAXEXP 4096
#define TRUE 1
#define FALSE 0
#define ENVSIZE 64
