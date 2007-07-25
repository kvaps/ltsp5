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
FILE *ldmlog;

void
usage()
{
    fprintf(stderr, "Usage: ldm <vt[1-N]> <:[0-N]>\n");
    exit(1);
}

void
die(char *msg)
{
    fprintf(ldmlog, "%s", msg);
    fclose(ldmlog);
    exit(1);
}

void
dumpargs(char *args[])
{
    int i;

    for (i = 0; args[i]; i++)
        fprintf(ldmlog, "%s\n", args[i]);
}

int
ldm_getenv_bool(const char *name)
{
    char *env = getenv(name);

    if (env)
        if (*env == 'y' || *env == 't' || *env == 'T' || *env == 'Y')
            return TRUE;

    return FALSE;
}


int
ldm_spawn (char *const argv[])
{
    pid_t pid;
  
    pid = fork();
  
    if (pid == 0) {                             /* child */
        int i;
        setsid();                               /* Become group leader */
        for (i = getdtablesize(); i >= 0; --i)
            close(i);                           /* close all descriptors */
        i = open("/dev/null",O_RDWR);           /* open stdin */
        dup(i);                                 /* stdout */
        dup(i);                                 /* stderr */
        execv(argv[0], argv);                   /* execve our arglist */
        die ("Error: execv() returned");
    } else if (pid > 0)
        return pid;
    else if (pid < 0)
        die ("Error: fork() failed");
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
    pid_t xauthpid;
    int status;

    char *xauth_command[] = {
       "/usr/bin/xauth", 
       "-i",
       "-n",
       "-f",
       ldminfo.authfile,
       "generate",
       ldminfo.display,
       NULL };

    /*
     * Since we're talking to the X server, we might have to give it a moment
     * or two to start up.  So do this in a loop.
     */

    do {
        sleep(1);
        xauthpid = ldm_spawn(xauth_command);
        status = ldm_wait(xauthpid);
    } while (status);
}

void
launch_x()
{
    char *server_command[MAXARGS];
    int i = 0;
    int fd;

    /*
     * Create an empty .Xauthority file.
     */
    fd = creat(ldminfo.authfile, S_IRUSR | S_IWUSR);
    close(fd);

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
rc_files(char *action)
{
    pid_t rcpid;

    char *rc_cmd[] = {
        "/bin/sh",
        RC_DIR "/ldm-script",
        action,
        NULL };

    /* start */
        
    rcpid = ldm_spawn(rc_cmd);
    ldm_wait(rcpid);
}

void
x_session()
{
    char *cmd[MAXARGS];
    char displayenv[ENVSIZE];
    char ltspclienv[ENVSIZE];
    char soundcmd1[ENVSIZE];
    char soundcmd2[ENVSIZE];
    char *esdcmd[] = {
        "/usr/bin/esd",
        "-nobeeps",
        "-public", 
        "-tcp",
        NULL };
    pid_t xsessionpid;
    pid_t esdpid = 0;
    int i = 0;

    snprintf(ltspclienv, sizeof ltspclienv, "LTSP_CLIENT=%s", ldminfo.ipaddr);
    if (ldminfo.directx)
        snprintf(displayenv, sizeof displayenv, 
                "DISPLAY=%s%s ", ldminfo.ipaddr, ldminfo.display);

    cmd[i++] = "/usr/bin/ssh";

    if (!ldminfo.directx)
        cmd[i++] = "-X";

    cmd[i++] = "-t";
    cmd[i++] = "-S";
    cmd[i++] = ldminfo.control_socket;
    cmd[i++] = "-l";
    cmd[i++] = ldminfo.username;
    cmd[i++] = ldminfo.server;
    cmd[i++] = ltspclienv;

    /*
     * Set the DISPLAY env, if not running over encrypted ssh
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
            snprintf(soundcmd2, sizeof soundcmd2, "ESPEAKER=%s:16001",
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
        cmd[i++] = "||";        /* closes bug number 121254 */
        cmd[i++] = "/usr/sbin/ltspfsmounter";
        cmd[i++] = "all";
        cmd[i++] = "cleanup";
    }

    cmd[i++] = ";";
    cmd[i++] = "kill";
    cmd[i++] = "-1";
    cmd[i++] = "$PPID";
    cmd[i++] = NULL;

    dumpargs(cmd);
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
    char display_env[ENVSIZE], xauth_env[ENVSIZE];
    char server_env[ENVSIZE], socket_env[ENVSIZE];
    char fontpath[ENVSIZE];

    /*
     * Zero out our info struct.
     */

    bzero(&ldminfo, sizeof ldminfo);

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

    if (!(ldmlog = fopen("/var/log/ldm.log", "w")))
        die("Couldn't open /var/log/ldm.log");

    setbuf(ldmlog, NULL);      /* Unbuffered */

    /*
     * openlog("ldm", LOG_PID | LOG_PERROR , LOG_AUTHPRIV);
     */
    
    fprintf(ldmlog, "LDM2 starting\n");

    /*
     * Get our IP address.
     */

    get_ipaddr();
    fprintf(ldmlog, "LDM2 running on ip address %s\n", ldminfo.ipaddr);

    /*
     * Get some of the environment variables we'll need.
     */

    ldminfo.server = getenv("LDM_SERVER");
    if (!ldminfo.server)
        ldminfo.server = getenv("SERVER");
    if (!ldminfo.server)
        die("no server specified");

    if (ldm_getenv_bool("USE_XFS")) {
        char *xfs_server;

        xfs_server = getenv("XFS_SERVER");
        snprintf(fontpath, sizeof fontpath, "tcp/%s:7100", 
                 xfs_server ? xfs_server : ldminfo.server);
        ldminfo.fontpath = fontpath;
        if (xfs_server)
            free(xfs_server);
    } else
        ldminfo.fontpath = NULL;

    ldminfo.sound = ldm_getenv_bool("SOUND");
    ldminfo.sound_daemon = getenv("SOUND_DAEMON");
    ldminfo.localdev = ldm_getenv_bool("LOCALDEV");
    ldminfo.override_port = getenv("SSH_OVERRIDE_PORT");
    ldminfo.directx = ldm_getenv_bool("LDM_DIRECTX");
    if (getenv("LDM_USERNAME") || getenv("LDM_PASSWORD"))
        ldminfo.autologin = TRUE;
    ldminfo.lang = getenv("LDM_LANGUAGE");
    ldminfo.session = getenv("LDM_SESSION");
    ldminfo.greeter_prog = getenv("LDM_GREETER");
    if (!ldminfo.greeter_prog)
        ldminfo.greeter_prog = "/usr/bin/ldmgtkgreet";
    ldminfo.authfile = "/root/.Xauthority";
    ldminfo.control_socket = "/var/run/ldm_socket";

    snprintf(display_env, sizeof display_env,  "DISPLAY=%s", ldminfo.display);
    snprintf(xauth_env, sizeof xauth_env, "XAUTHORITY=%s", ldminfo.authfile);
    snprintf(server_env, sizeof server_env,  "LDM_SERVER=%s", ldminfo.server);
    snprintf(socket_env, sizeof socket_env, "LDM_SOCKET=%s", 
             ldminfo.control_socket);

    /* 
     * Update our environment with a few extra variables.
     */

    putenv(display_env);
    putenv(xauth_env);
    putenv(server_env);
    putenv(socket_env);

    /*
     * Main loop
     */

    while (TRUE) {
        fprintf(ldmlog, "Launching Xorg\n");
        launch_x();
        create_xauth();                         /* recreate .Xauthority */
        
        if (!ldminfo.autologin) {
            fprintf(ldmlog, "Spawning greeter: %s\n", ldminfo.greeter_prog);
            spawn_greeter();
        }

        ldminfo.username = get_userid();

        while(TRUE) {
            int retval;
            fprintf(ldmlog, "Attempting ssh session as %s\n", ldminfo.username);
            retval = ssh_session();             /* Log in via ssh */
            if (!retval)
                break;
            if (retval == 2) {
                ldm_wait(ldminfo.sshpid);
                close(ldminfo.sshfd);
            }
        }

        clear_username();                       /* Don't keep these in mem */
        clear_password();

        fprintf(ldmlog, "Established ssh session.\n");
        if (ldminfo.greeterpid)
            close_greeter();

        rc_files("start");                      /* Execute any rc files */
        x_session();                            /* Start X session up */

        /* x_session's exited.  So, clean up. */

        rc_files("stop");                       /* Execute any rc files */

        kill(ldminfo.xserverpid, SIGTERM);      /* Kill Xorg server off */
        ldm_wait(ldminfo.xserverpid);

        ssh_endsession();                       /* Log out of server */
    }
}
