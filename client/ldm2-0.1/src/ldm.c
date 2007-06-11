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

FILE *tty2;
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
ldm_spawn (char *const argv[], pid_t *pid, int wait)
{
    int status;
    pid_t child;
  
    child = fork();
  
    if (child == 0) {
        /* execve our arglist */
        execv(argv[0], argv);
        die ("Error: execv() returned");
    } else if (child > 0) {
        if (pid)                /* save pid if we've been passed a pointer */
            *pid = child;
        if (wait) {
            if (waitpid (child, &status, 0) < 0)
                die("Error: wait() call failed");
        }
    } else if (child < 0) {
        die ("Error: fork() failed");
    }
  
    if (wait) {
        if (!WIFEXITED (status)) {
            die ("Error: execv() returned no status");
        }
  
        return WEXITSTATUS (status);
    } else
        return 0;
}

void
create_xauth()
{
    int fd;
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
    ldm_spawn(xauth_command, NULL, WAIT);
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
        
    ldm_spawn(server_command, &ldminfo.xserverpid, NOWAIT);
}

void
x_session()
{
    char *cmd[MAXARGS];
    char displayenv[BUFSIZ];
    int i = 0;

    if (ldminfo.directx)
        snprintf(displayenv, sizeof displayenv, 
                "DISPLAY=${SSH_CLIENT%%%% *}%s", 
                ldminfo.display);

    cmd[i++] = "/usr/bin/ssh";
    if (!ldminfo.directx)
        cmd[i++] = "-X";
    cmd[i++] = "-S";
    cmd[i++] = ldminfo.control_socket;
    cmd[i++] = "-l";
    cmd[i++] = ldminfo.username;
    cmd[i++] = ldminfo.server;
    if (ldminfo.directx)
        cmd[i++] = displayenv;
    cmd[i++] = "/etc/X11/Xsession";
    cmd[i++] = NULL;

    ldm_spawn(cmd, NULL, WAIT);
}

/*
 * mainline
 */
        
int
main(int argc, char *argv[])
{
    /* decls */
    char display_env[BUFSIZ], xauth_env[BUFSIZ];

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
     * Get some of the environment variables we'll need.
     */

    ldminfo.server = ldm_getenv("LDM_SERVER");
    if (!ldminfo.server)
        ldminfo.server = ldm_getenv("SERVER");
    if (!ldminfo.server)
        die("no server specified");

    if (ldm_getenv_bool("USE_XFS")) {
        int slen;
        char *xfs_server;

        xfs_server = ldm_getenv("XFS_SERVER");
        if (!xfs_server)
            xfs_server = ldminfo.server;
        slen = strlen(xfs_server);
        ldminfo.fontpath = malloc(slen + 10);
        if (!ldminfo.fontpath)
            die("out of memory");
        sprintf(ldminfo.fontpath, "tcp/%s:7100", xfs_server);
    } else
        ldminfo.fontpath = NULL;

    ldminfo.sound = ldm_getenv_bool("SOUND");
    ldminfo.localdev = ldm_getenv_bool("LOCALDEV");
    ldminfo.override_port = ldm_getenv("SSH_OVERRIDE_PORT");
    ldminfo.directx = ldm_getenv_bool("LDM_DIRECTX");
    ldminfo.username = ldm_getenv("LDM_USERNAME");
    ldminfo.password = ldm_getenv("LDM_PASSWORD");
    if (ldminfo.username && ldminfo.password)
        ldminfo.autologin = 1;
    else
        ldminfo.autologin = 0;
    ldminfo.lang = ldm_getenv("LDM_LANGUAGE");
    ldminfo.session = ldm_getenv("LDM_SESSION");
    ldminfo.greeter_prog = ldm_getenv("LDM_GREETER");
    ldminfo.authfile = "/tmp/.Xauthority";
    ldminfo.control_socket = "/tmp/.ldm_socket";

    sprintf(display_env, "DISPLAY=%s", ldminfo.display);
    sprintf(xauth_env, "XAUTHORITY=%s", ldminfo.authfile);

    /*
     * Main loop
     */

    while (1) {
        int status;
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
        waitpid (ldminfo.xserverpid, &status, 0);

        unsetenv("DISPLAY");
        unsetenv("XAUTHORITY");

        ssh_endsession();
    }
}

