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
    char    *greeter_prog;
    char    *control_socket;
    int     autologin;
    int     sound;
    int     localdev;
    int     directx;
    int     sshfd;
    pid_t   sshpid;
};

/* forward decls */

void usage();
void die(char *msg);
char *ldm_getenv(const char *name);
int ldm_getenv_bool(const char *name);
int ldm_spawn (char *const argv[], int wait);
void create_xauth();
void launch_x();
char *get_userid();
char *get_passwd();
void spawn_ssh(int fd);
int ssh_session();
int ssh_endsession();

extern struct ldm_info ldminfo;
extern FILE *tty2;
#define NOWAIT 0
#define WAIT 1
#define MAXARGS 30
#define SENTINEL "LTSPROCKS"
#define MAXEXP 4096
