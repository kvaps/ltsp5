#define LDMSTRSZ 256

struct ldm_info {
    char    server[LDMSTRSZ];
    char    vty[LDMSTRSZ];
    char    display[LDMSTRSZ];
    char    fontpath[LDMSTRSZ];
    char    override_port[LDMSTRSZ];
    char    authfile[LDMSTRSZ];
    char    username[LDMSTRSZ];
    char    password[LDMSTRSZ];
    char    lang[LDMSTRSZ];
    char    session[LDMSTRSZ];
    char    sound_daemon[LDMSTRSZ];
    char    greeter_prog[LDMSTRSZ];
    char    control_socket[LDMSTRSZ];
    char    ipaddr[LDMSTRSZ];
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
pid_t ldm_spawn (char **argv);
char *scopy(char *dest, char *source);
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
#define ENVSIZE 64
