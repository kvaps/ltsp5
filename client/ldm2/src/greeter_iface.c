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
#include <pty.h>

char *localpasswd;

#include "ldm.h"

int
popen2(char **args, int *fpin, int *fpout)
{
    int pin[2], pout[2];
    pid_t pid;

    if(pipe(pin) == -1)
        die("pipe\n");
    if(pipe(pout) == -1)
        die("pipe\n");

    pid = fork();

    if(pid == -1)
        die("fork\n");

    if (pid == 0) {
        /* we are "in" the child */

        /* this process' stdin should be the read end of the pin pipe.
         * dup2 closes original stdin */
        dup2(pin[0], STDIN_FILENO);
        close(pin[0]);
        close(pin[1]);
        dup2(pout[1], STDOUT_FILENO);
        /* this process' stdout should be the write end of the pout
         * pipe. dup2 closes original stdout */
        close(pout[1]);
        close(pout[0]);

        execv(args[0], args);
        /* on sucess, execv never returns */
        return(-1);
    }

    close(pin[0]);
    close(pout[1]);
    *fpin = pin[1];
    *fpout = pout[0];
    return(pid);
}

void
spawn_greeter()
{
    char *greet[] = {
        ldminfo.greeter_prog,
        NULL};

    
    ldminfo.greeterpid = popen2(greet, &ldminfo.greeterwfd,
                                       &ldminfo.greeterrfd);
}
    
int
get_greeter_string(char *str)
{
    char b[BUFSIZ];
    char *p = b;
    int i = 0;
    int ret;

    while(TRUE) {
        if (i == (BUFSIZ - 1))
            break;
        ret = read(ldminfo.greeterrfd, p, 1);
        if (ret < 0)
            return 1;
        if (*p == '\n')
            break;
        p++;
        i++;
    }

    *p = '\0';

    mystrncpy(str, b, LDMSTRSZ);

    return 0;
}


int
get_userid()
{
    char *prompt = "prompt <b>Username</b>\n";
    char *p;

    fprintf(ldmlog, "In get_userid\n");
    if (p = getenv("LDM_USERNAME")) {
        mystrncpy(ldminfo.username, p, LDMSTRSZ);
        return 0;
    } else {
        write(ldminfo.greeterwfd, prompt, strlen(prompt));
        return get_greeter_string(ldminfo.username);
    }
}
    
int
get_passwd()
{
    char *prompt = "prompt <b>Password</b>\n";
    char *pw = "passwd\n";
    char *p;

    fprintf(ldmlog, "In get_passwd\n");
    if (p = getenv("LDM_PASSWORD")) {
        mystrncpy(ldminfo.password, p, LDMSTRSZ);
        return 0;
    } else {
        write(ldminfo.greeterwfd, prompt, strlen(prompt));
        write(ldminfo.greeterwfd, pw, strlen(pw));
        return get_greeter_string(ldminfo.password);
    }
}

void
set_message(char *message)
{
    char password[255];
    char *prompt = "msg ";
    char *pw = "\n";
    char *p;

    write(ldminfo.greeterwfd, prompt, strlen(prompt));
    write(ldminfo.greeterwfd, message, strlen(message));
    write(ldminfo.greeterwfd, pw, strlen(pw));
}

void
close_greeter()
{
    kill(ldminfo.greeterpid, SIGTERM);
    ldm_wait(ldminfo.greeterpid);
    close(ldminfo.greeterrfd);
    close(ldminfo.greeterwfd);
    ldminfo.greeterrfd = 0;
    ldminfo.greeterwfd = 0;
    ldminfo.greeterpid = 0;
}
