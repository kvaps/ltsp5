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
#include <glib.h>

#include "ldm.h"

struct ldm_info ldminfo;
FILE *ldmlog;

void
usage()
{
    fprintf(stderr, "Usage: ldm <vt[1-N]> <:[0-N]>\n");
    exit(1);
}

/*
 * die()
 *
 * Close display manager down with an error message.
 */

void
die(char *msg)
{
    fprintf(ldmlog, "%s", msg);
    fclose(ldmlog);

    /*
     * Shut things down gracefully if we can
     */ 

    if (ldminfo.greeterpid)
        close_greeter();
    if (ldminfo.sshpid)
        ssh_endsession();
    if (ldminfo.xserverpid) {
        kill(ldminfo.xserverpid, SIGTERM);      /* Kill Xorg server off */
        ldm_wait(ldminfo.xserverpid);
    }

    exit(1);
}

/*
 * scopy()
 *
 * Copy a string.  Used to move data in and out of our ldminfo structure.
 * Note: if the source string is null, or points to a valid string of '\0',
 * both result in a dest string length of 0.
 */

char *
scopy(char *dest, char *source)
{
    if (!source)
        *dest = '\0';
    else {
        strncpy(dest, source, LDMSTRSZ - 1);
        *(dest + LDMSTRSZ - 1) = '\0';     /* ensure null termination */
    }

    return dest;
}

/*
 * ldm_getenv_bool()
 *
 * Handle boolean environment types.
 */

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
ldm_spawn (char **argv)
{
    pid_t pid;
  
    g_spawn_async(NULL, argv, NULL,
                  G_SPAWN_DO_NOT_REAP_CHILD |
                  G_SPAWN_STDOUT_TO_DEV_NULL |
                  G_SPAWN_STDERR_TO_DEV_NULL,
                  NULL, NULL, &pid, NULL);
    return pid;
}

int
ldm_wait(pid_t pid)
{
    int status;

    if (waitpid (pid, &status, 0) < 0)
        die("Error: wait() call failed");
    if (!WIFEXITED (status)) {
        fprintf(ldmlog, "Process returned no status\n");
        return 1;
    } else
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
    char *argv[MAXARGS];
    int i = 0;
    int fd;

    /*
     * Create an empty .Xauthority file.
     */
    fd = creat(ldminfo.authfile, S_IRUSR | S_IWUSR);
    close(fd);

    /* Spawn the X server */
    argv[i++] = "/usr/bin/X";
    argv[i++] = "-auth";
    argv[i++] = ldminfo.authfile;
    argv[i++] = "-br";
    argv[i++] = "-ac";
    argv[i++] = "-noreset";
    if (*ldminfo.fontpath != '\0') {
        argv[i++] = "-fp";
        argv[i++] = ldminfo.fontpath;
    }
    argv[i++] = ldminfo.vty;
    argv[i++] = ldminfo.display;
    argv[i++] = NULL;
        
    ldminfo.xserverpid = ldm_spawn(argv);
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
            
    cmd[i++] = ldminfo.session;

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

    g_type_init();

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

    scopy(ldminfo.vty, argv[1]);
    scopy(ldminfo.display, argv[2]);

    /*
     * Open our log.  Since we're on a terminal, logging to syslog is preferred,
     * as then we'll be able to collect the logging on the server.
     * Since we're handling login info, log to AUTHPRIV
     */

    if (!(ldmlog = fopen("/var/log/ldm.log", "a")))
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

    if (ldm_getenv_bool("USE_XFS")) {
        char *xfs_server;
        xfs_server = getenv("XFS_SERVER");
        snprintf(ldminfo.fontpath, sizeof ldminfo.fontpath, "tcp/%s:7100", 
                 xfs_server ? xfs_server : ldminfo.server);
    } 

    ldminfo.sound = ldm_getenv_bool("SOUND");
    scopy(ldminfo.sound_daemon, getenv("SOUND_DAEMON"));
    ldminfo.localdev = ldm_getenv_bool("LOCALDEV");
    scopy(ldminfo.override_port, getenv("SSH_OVERRIDE_PORT"));
    ldminfo.directx = ldm_getenv_bool("LDM_DIRECTX");
    if (getenv("LDM_USERNAME") || getenv("LDM_PASSWORD"))
        ldminfo.autologin = TRUE;
    scopy(ldminfo.lang, getenv("LDM_LANGUAGE"));
    scopy(ldminfo.session, getenv("LDM_SESSION"));
    if (*ldminfo.session == '\0')
        scopy(ldminfo.session, "/etc/X11/Xsession");
    scopy(ldminfo.greeter_prog, getenv("LDM_GREETER"));
    if (*ldminfo.greeter_prog == '\0')
        scopy(ldminfo.greeter_prog, "/usr/bin/ldmgtkgreet");
    scopy(ldminfo.authfile, "/root/.Xauthority");
    scopy(ldminfo.control_socket, "/var/run/ldm_socket");

    snprintf(display_env, sizeof display_env,  "DISPLAY=%s", ldminfo.display);
    snprintf(xauth_env, sizeof xauth_env, "XAUTHORITY=%s", ldminfo.authfile);
    snprintf(socket_env, sizeof socket_env, "LDM_SOCKET=%s", 
             ldminfo.control_socket);

    /* 
     * Update our environment with a few extra variables.
     */

    putenv(display_env);
    putenv(xauth_env);
    putenv(socket_env);

    /*
     * Begin running display manager
     */

    fprintf(ldmlog, "Launching Xorg\n");
    launch_x();
    create_xauth();                         /* recreate .Xauthority */
    
    if (!ldminfo.autologin) {
        fprintf(ldmlog, "Spawning greeter: %s\n", ldminfo.greeter_prog);
        spawn_greeter();
    }

    if (get_userid() ||
        *(ldminfo.username) == '\0')
        die("Greeter returned null userid");

    if (get_host() ||
        *(ldminfo.server) == '\0')
        die("Couldn't get a valid hostname");

    fprintf(ldmlog, "Establishing a session with %s\n", ldminfo.server);

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

    /*
     * Clear out the password so it's not sitting in memory anywhere
     */

    bzero(ldminfo.password, sizeof ldminfo.password);

    if (get_language())
        die("Couldn't get a valid language setting");
    if (get_session())
        die("Couldn't get a valid session setting");

    snprintf(server_env, sizeof server_env,  "LDM_SERVER=%s", ldminfo.server);
    putenv(server_env);

    fprintf(ldmlog, "Established ssh session.\n");
    if (ldminfo.greeterpid)
        close_greeter();

    fprintf(ldmlog, "Executing rc files.\n");
    rc_files("start");                      /* Execute any rc files */
    fprintf(ldmlog, "Beginning X session.\n");
    x_session();                            /* Start X session up */
    fprintf(ldmlog, "X session ended.\n");

    /* x_session's exited.  So, clean up. */

    fprintf(ldmlog, "Executing rc files.\n");
    rc_files("stop");                       /* Execute any rc files */

    fprintf(ldmlog, "Killing X server.\n");
    kill(ldminfo.xserverpid, SIGTERM);      /* Kill Xorg server off */
    ldm_wait(ldminfo.xserverpid);

    fprintf(ldmlog, "Ending ssh session.\n");
    ssh_endsession();                       /* Log out of server */

    fclose(ldmlog);
    exit(0);
}
