/*
 * ldm.c
 * LTSP display manager.
 * Manages spawning a session to a server.
 *
 * (c) Scott Balneaves, sbalneav@ltsp.org
 *
 * This software is licensed under the GPL v2 or later.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>

#include "ldm.h"

struct ldm_info ldminfo;

void
usage()
{
    fprintf(stderr, "Usage: ldm <vt[1-N]> <:[0-N]>\n");
    exit(1);
}

void
die(char *msg)
{
    syslog(LOG_ERR, "%s", msg);
    exit(1);
}

char *
ldm_getenv(const char *name)
{
    char *env = NULL;
    char *copy;

    env = getenv(name);

    if (!env)
        return NULL;
    else {
        copy = strdup(env);
        if (!copy)
            die("out of memory");
    }

    return copy;
}

int
ldm_getenv_bool(const char *name)
{
    char *env;
    int result = 0;

    env = ldm_getenv(name);

    if (env) {
        int c = tolower(*env);
        if (c == 'y' || c == 't')
            result = 1;
        free(env);
    }

    return result;
}


int
ldm_spawn (char *const argv[])
{
    pid_t pid;
  
    pid = fork();
  
    if (pid == 0) {
        execv(argv[0], argv);                   /* execve our arglist */
        die ("Error: execv() returned");
    } else if (pid > 0) {
        return pid;
    } else if (pid < 0) {
        die ("Error: fork() failed");
    }
}

int
ldm_wait(pid_t pid)
{
    int status;

    if (waitpid (pid, &status, 0) < 0)
        die("Error: wait() call failed");
    if (!WIFEXITED (status))
        die ("Error: execv() returned no status");
    else
        return WEXITSTATUS (status);
}

void
create_xauth()
{
    int fd;
    pid_t xauthpid;
    FILE *mcookiepipe;
    char mcookiebuf[34];
    char *xauth_command[] = {
       "/usr/bin/xauth", 
       "-i",
       "-n",
       "-f",
       ldminfo.authfile,
       "add",
       ldminfo.display,
       ".",
       mcookiebuf,
       NULL };

    /*
     * Create an empty .Xauthority file.
     */

    fd = creat(ldminfo.authfile, S_IRUSR | S_IWUSR);
    close(fd);

	/* get an mcookie */
    mcookiepipe = popen("mcookie", "r");
    fgets(mcookiebuf, sizeof mcookiebuf, mcookiepipe);
    pclose(mcookiepipe);
    /* fix the newline */
    mcookiebuf[strlen(mcookiebuf) - 1] = '\0';
    /* We've spawned the X server, create an Xauth key */
    xauthpid = ldm_spawn(xauth_command);
    ldm_wait(xauthpid);
}

void
launch_x()
{
    char *server_command[MAXARGS];
    int i = 0;

    /* Spawn the X server */
    server_command[i++] = "/usr/bin/X";
    server_command[i++] = "-auth";
    server_command[i++] = ldminfo.authfile;
    server_command[i++] = "-br";
    server_command[i++] = "-ac";
    server_command[i++] = "-noreset";
    if (ldminfo.fontpath) {
        server_command[i++] = "-fp";
        server_command[i++] = ldminfo.fontpath;
    }
    server_command[i++] = ldminfo.vty;
    server_command[i++] = ldminfo.display;
    server_command[i++] = NULL;
        
    ldminfo.xserverpid = ldm_spawn(server_command);
}

void
x_session()
{
    char *cmd[MAXARGS];
    char displayenv[BUFSIZ];
    char ltspclienv[BUFSIZ];
    char soundcmd1[BUFSIZ];
    char soundcmd2[BUFSIZ];
    char *esdcmd[] = {
        "/usr/bin/esd",
        "-nobeeps",
        "-public", 
        "-tcp",
        NULL };
    pid_t xsessionpid;
    pid_t esdpid = 0;
    int i = 0;

    if (ldminfo.directx)
        snprintf(displayenv, sizeof displayenv, 
                "DISPLAY=%s%s", ldminfo.ipaddr, ldminfo.display);
    snprintf(ltspclienv, sizeof ltspclienv,
             "LTSP_CLIENT=", ldminfo.ipaddr);

    cmd[i++] = "/usr/bin/ssh";

    if (!ldminfo.directx)
        cmd[i++] = "-X";

    cmd[i++] = "-S";
    cmd[i++] = ldminfo.control_socket;
    cmd[i++] = "-l";
    cmd[i++] = ldminfo.username;
    cmd[i++] = ldminfo.server;
    cmd[i++] = ltspclienv;

    /*
     * Set the DISPLAY env, of not running over encrypted ssh
     */

    if (ldminfo.directx)
        cmd[i++] = displayenv;

    /*
     * Handle sound
     */

    if (ldminfo.sound) {
        char *daemon = ldminfo.sound_daemon;

        if (!daemon || !strncmp(daemon, "pulse", 5)) {
            snprintf(soundcmd1, sizeof soundcmd1, "PULSE_SERVER=tcp:%s:4713",
                     ldminfo.ipaddr);
            snprintf(soundcmd1, sizeof soundcmd1, "ESPEAKER=%s:16001",
                     ldminfo.ipaddr);
            cmd[i++] = soundcmd1;
            cmd[i++] = soundcmd2;
        } else if (!strncmp(daemon, "esd", 3)) {
            snprintf(soundcmd1, sizeof soundcmd1, "ESPEAKER=%s:16001",
                     ldminfo.ipaddr);
            cmd[i++] = soundcmd1;
            esdpid = ldm_spawn(esdcmd);         /* launch ESD */
        } else if (!strncmp(daemon, "nasd", 4)) {
            snprintf(soundcmd1, sizeof soundcmd1, "AUDIOSERVER=%s:0",
                     ldminfo.ipaddr);
            cmd[i++] = soundcmd1;
        }
    }
            
    cmd[i++] = "/etc/X11/Xsession";

    if (ldminfo.localdev) {
        cmd[i++] = ";";
        cmd[i++] = "&&";
        cmd[i++] = "/usr/sbin/ltspfsmounter";
        cmd[i++] = "all";
        cmd[i++] = "cleanup";
    }

    cmd[i++] = NULL;

    xsessionpid = ldm_spawn(cmd);
    ldm_wait(xsessionpid);
    if (esdpid) {
        kill(esdpid, SIGTERM);
        ldm_wait(esdpid);
    }
}

/*
 * mainline
 */
        
int
main(int argc, char *argv[])
{
    /* decls */
    char display_env[BUFSIZ], xauth_env[BUFSIZ];
    char fontpath[BUFSIZ];

    /*
     * Process command line args.  Need to get our vt, and our display number
     * Since we don't have any fancy args, we won't bother to do this with
     * getopt, rather, we'll just do it manually.
     */

    if (argc < 3)
        usage();

    if (strncmp(argv[1], "vt", 2))      /* sanity */
        usage();
    if (*argv[2] != ':')            /* more sanity */
        usage();

    ldminfo.vty = strdup(argv[1]);
    ldminfo.display = strdup(argv[2]);

    /*
     * Open our log.  Since we're on a terminal, logging to syslog is preferred,
     * as then we'll be able to collect the logging on the server.
     * Since we're handling login info, log to AUTHPRIV
     */

    openlog("ldm", LOG_PID | LOG_PERROR , LOG_AUTHPRIV);
    
    /*
     * Get our IP address.
     */

    get_ipaddr();

    /*
     * Get some of the environment variables we'll need.
     */

    ldminfo.server = ldm_getenv("LDM_SERVER");
    if (!ldminfo.server)
        ldminfo.server = ldm_getenv("SERVER");
    if (!ldminfo.server)
        die("no server specified");

    if (ldm_getenv_bool("USE_XFS")) {
        char *xfs_server;

        xfs_server = ldm_getenv("XFS_SERVER");
        snprintf(fontpath, sizeof fontpath, "tcp/%s:7100", 
                 xfs_server ? xfs_server : ldminfo.server);
        ldminfo.fontpath = fontpath;
        if (xfs_server)
            free(xfs_server);
    } else
        ldminfo.fontpath = NULL;

    ldminfo.sound = ldm_getenv_bool("SOUND");
    ldminfo.sound_daemon = ldm_getenv_bool("SOUND_DAEMON");
    ldminfo.localdev = ldm_getenv_bool("LOCALDEV");
    ldminfo.override_port = ldm_getenv("SSH_OVERRIDE_PORT");
    ldminfo.directx = ldm_getenv_bool("LDM_DIRECTX");
    ldminfo.username = ldm_getenv("LDM_USERNAME");
    ldminfo.password = ldm_getenv("LDM_PASSWORD");
    ldminfo.autologin =  (ldminfo.username && ldminfo.password) ? TRUE : FALSE;
    ldminfo.lang = ldm_getenv("LDM_LANGUAGE");
    ldminfo.session = ldm_getenv("LDM_SESSION");
    ldminfo.greeter_prog = ldm_getenv("LDM_GREETER");
    ldminfo.authfile = "/tmp/.Xauthority";
    ldminfo.control_socket = "/var/run/ldm_socket";

    snprintf(display_env, sizeof display_env,  "DISPLAY=%s", ldminfo.display);
    snprintf(xauth_env, sizeof xauth_env, "XAUTHORITY=%s", ldminfo.authfile);

    /*
     * Main loop
     */

    while (1) {
        create_xauth();
        launch_x();

        /* Update our environment with a few extra variables. */
        putenv(display_env);
        putenv(xauth_env);
        
        if (!ldminfo.autologin)
            ldminfo.username = get_userid();

        ssh_session();
        x_session();

        /* x_session's exited.  So, clean up. */
        if (!ldminfo.autologin)
            free(ldminfo.username);

        kill(ldminfo.xserverpid, SIGTERM);
        ldm_wait(ldminfo.xserverpid);

        unsetenv("DISPLAY");
        unsetenv("XAUTHORITY");

        ssh_endsession();
    }
}
