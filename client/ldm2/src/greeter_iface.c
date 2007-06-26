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

int remap_pipe(int r, int w)
{
    if (r == 0 && w == 1)
        return TRUE;
    if (r >= 1 && w > 1)
        return dup2(r, 0) == 0 && close(r) == 0 &&
               dup2(w, 1) == 0 && close(w) == 0;
    if (r == 0 && w >= 1)
        return dup2(w, 1) == 0 && close(w) == 0;
    if (r >= 1 && w == 1)
        return dup2(r, 0) == 0 && close(r) == 0;
    if (r >= 1 && w == 0)
        return dup2(w, 1) == 0 && close(w) == 0 &&
               dup2(r, 0) == 0 && close(r) == 0;
    if (r == 1 && w == 0) {
        const int tmp = dup(w);
        return tmp > 1 && close(w) == 0 &&
               dup2(r, 0) == 0 && close(r) == 0 &&
               dup2(tmp, 1) == 0 && close(tmp) == 0;
    }
    return  FALSE;
}

void
spawn_greeter()
{
    char *greet[] = {
        ldminfo.greeter_prog,
        NULL};
    pid_t pid;
    int writepipe[2];                           /* parent -> child */
    int readpipe[2];                            /* child -> parent */

    fprintf(ldmlog, "spawning greeter\n");
    if (pipe(readpipe) < 0 || pipe(writepipe) < 0)
        die("Couldn't open greeter pipes\n");

    pid = fork();
    if (pid == 0) {                             /* child */
        if (!remap_pipe(writepipe[0], readpipe[1]))
            die("Couldn't remap greeter pipes");
        execv(greet[0], greet);                   /* execve our arglist */
        die ("Error: execv() returned\n");
    } else if (pid > 0) {
        ldminfo.greeterpid = pid;
        ldminfo.greeterrfd = readpipe[0];
        ldminfo.greeterwfd = writepipe[1];
    } else if (pid < 0)
        die ("Error: fork() failed\n");
}
    
char *
get_userid()
{
    char username[255];
    char *prompt = "prompt <b>Username</b>\n";
    char *p;

    write(ldminfo.greeterwfd, prompt, strlen(prompt));

    p = username;
    while(1) {
        read(ldminfo.greeterrfd, p, 1);
        if (*p == '\n')
            break;
        p++;
    }

    *p = '\0';

    return strdup(username);
}
    
char *
get_passwd()
{
    char password[255];
    char *prompt = "prompt <b>Password</b>\n";
    char *pw = "passwd\n";
    char *p;

    write(ldminfo.greeterwfd, prompt, strlen(prompt));
    write(ldminfo.greeterwfd, pw, strlen(pw));

    p = password;
    while(1) {
        read(ldminfo.greeterrfd, p, 1);
        if (*p == '\n')
            break;
        p++;
    }

    *p = '\0';

    return strdup(password);
}

void
close_greeter()
{
    char *quit = "quit\n";
    write(ldminfo.greeterwfd, quit, strlen(quit));
    ldm_wait(ldminfo.greeterpid);
}
