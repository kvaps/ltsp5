#define LDMSTRSZ 255

struct ldm_info {
    char    server[LDMSTRSZ + 1];
    char    vty[LDMSTRSZ + 1];
    char    display[LDMSTRSZ + 1];
    char    fontpath[LDMSTRSZ + 1];
    char    override_port[LDMSTRSZ + 1];
    char    authfile[LDMSTRSZ + 1];
    char    username[LDMSTRSZ + 1];
    char    password[LDMSTRSZ + 1];
    char    lang[LDMSTRSZ + 1];
    char    session[LDMSTRSZ + 1];
    char    sound_daemon[LDMSTRSZ + 1];
    char    greeter_prog[LDMSTRSZ + 1];
    char    control_socket[LDMSTRSZ + 1];
    char    ipaddr[LDMSTRSZ + 1];
    int     autologin;
    int     sound;
    int     localdev;
    int     directx;
    int     sshfd;
    int     greeterrfd;
    int     greeterwfd;
    pid_t   sshpid;
    pid_t   xserverpid;
    pid_t   greeterpid;
};

/* forward decls */

void usage();
void die(char *msg);
int ldm_getenv_bool(const char *name);
pid_t ldm_spawn (char *const argv[]);
char *mystrncpy(char *d, char *s, int n);
int ldm_wait (pid_t pid);
void create_xauth();
void launch_x();
int get_userid();
int get_passwd();
void spawn_ssh(int fd);
int ssh_session();
int ssh_endsession();
void get_ipaddr();
void clean_username();
void clean_password();

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
