/*
 * sshutils.c
 * LTSP display manager.
 * Manages spawning a session to a server.
 *
 * (c) Scott Balneaves, sbalneav@ltsp.org
 *
 * This software is licensed under the GPL v2 or later.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <syslog.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <string.h>
#include <signal.h>
#include <pty.h>
#include <utmp.h>

#include "ldm.h"

void
spawn_ssh(int fd)
{
    char *sshcmd[MAXARGS];
    char dpy[ENVSIZE];
    char *env[3];
    int i = 0;
    
    snprintf(dpy, sizeof dpy, "DISPLAY=%s", ldminfo.display);
    env[0] = "LANG=C";
    env[1] = dpy;
    env[2] = NULL;

    sshcmd[i++] = "/usr/bin/ssh";

    if (!ldminfo.directx)
        sshcmd[i++] = "-X";

    sshcmd[i++] = "-t";
    sshcmd[i++] = "-M";
    sshcmd[i++] = "-S";
    sshcmd[i++] = ldminfo.control_socket;

    if (ldminfo.override_port) {
        sshcmd[i++] = "-p";
        sshcmd[i++] = ldminfo.override_port;
    }

    sshcmd[i++] = "-l";
    sshcmd[i++] = ldminfo.username;
    sshcmd[i++] = ldminfo.server;
    sshcmd[i++] = "echo";
    sshcmd[i++] = SENTINEL;
    sshcmd[i++] = ";";
    sshcmd[i++] = "LANG=C";
    sshcmd[i++] = "/bin/sh";
    sshcmd[i++] = "-";
    sshcmd[i++] = NULL;

    (void) setsid();
    login_tty(fd);
    execve (*sshcmd, sshcmd, env);
    perror ("Error: execv() returned");
}

int
expect(int fd, float seconds, ...)
{
    fd_set set;
    struct timeval timeout;
    int i, st;
    ssize_t size = 0;
    size_t total = 0;
    va_list ap;
    char buffer[BUFSIZ];
    char *expects[16];
    char p[MAXEXP];

    bzero(p, sizeof p);

    va_start(ap, seconds);

    for (i = 0; i < 16; i++) {
        expects[i] = va_arg(ap, char *);
        if (expects[i] == NULL)
            break;
    }

    va_end(ap);

    /*
     * Set our file descriptor to be watched.
     */

    FD_ZERO(&set);
    FD_SET(fd, &set);

    /*
     * Turn (float) seconds into (int) sec and (int) usec.
     */

    timeout.tv_sec = seconds;             /* float to int, grabs seconds */
    timeout.tv_usec = (int)((seconds - (float)timeout.tv_sec) * 1000000);

    /*
     * Main loop.
     */

    while(1) {
        st = select(FD_SETSIZE, &set, NULL, NULL, &timeout);
        if (st <= 0)                /* timeout or bad thing */
            break;
        size = read(fd, buffer, sizeof buffer);
        if (size < 0)
            break;

        if ((total + size) < MAXEXP) {
            strncpy(p + total, buffer, size);
            total += size;
        }

        for (i = 0; expects[i] != NULL && i < 16; i++)
            if (strstr(p, expects[i]))
                goto loopend;
    }

loopend:

    fprintf(ldmlog, "expect saw: %s\n", p);

    if (size < 0 || st < 0)
        return -1;                  /* error occured */
    if (st == 0)
        return 1;                   /* timed out */
    else
        return i + 1;               /* which expect did we see? */
}
            
int
ssh_chat(int fd)
{
    int seen;
    char *password;

    fprintf(ldmlog, "ssh_chat: getting password from greeter\n");
get_pass:
    password = get_passwd();
retrypass:
    fprintf(ldmlog, "ssh_chat: looking for ssword: from ssh fd\n");
    seen = expect(fd, 120.0, "ssword:", "continue connecting", NULL);
    if (seen == 1) {
        fprintf(ldmlog, "ssh_chat: got it!  Sending pw\n");
        write(fd, password, strlen(password));
        write(fd, "\r\n", 2);
    } else if (seen == 2) {
        fprintf(ldmlog, "ssh_chat: whoops!  Got the ssh are you sure you want to connect message. Sending yes\n");
        write(fd, "yes\n", 4);
        goto retrypass;
    } else  {
        fprintf(ldmlog, "ssh_chat: Got something else.  Blargh! I'm ded!\n");
        exit(1);
    }

    seen = expect(fd, 120.0, SENTINEL, "please try again.", NULL);
    if (seen == 1) {
        fprintf(ldmlog, "ssh_chat: Saw sentinel. Logged in successfully\n");
    } else if (seen == 2) {
        fprintf(ldmlog, "ssh_chat: Type your passord right, you dork!\n");
        free(password);
        goto get_pass;
    } else {
        fprintf(ldmlog, "ssh_chat: Wierdness!  Dying!\n");
        return 1;
    }

    return 0;
}
        
int
ssh_session()
{
    int fd_master, fd_slave;
    pid_t pid;

    openpty(&fd_master, &fd_slave, NULL, NULL, NULL);
    if ( (pid = fork()) < 0)
        perror("fork error");
    else if (pid > 0) {           /* parent */
        close(fd_slave);
        ssh_chat(fd_master);
        ldminfo.sshfd = fd_master;
        ldminfo.sshpid = pid;
    } else {
        close(fd_master);           /* child */
        spawn_ssh(fd_slave);
    }

    return 0;
}

int
ssh_endsession()
{
    int seen;
    int status;

    write(ldminfo.sshfd, "exit\n", 5);
    seen = expect(ldminfo.sshfd, 30.0, "closed.", NULL);
    if (seen == 1)
        status = ldm_wait(ldminfo.sshpid);
    else {
        /* hmm, didn't close.  Lets kill it (less nice) */
        kill(ldminfo.sshpid, SIGTERM);
        status = ldm_wait(ldminfo.sshpid);
    }

    close (ldminfo.sshfd);
    return status;
}
