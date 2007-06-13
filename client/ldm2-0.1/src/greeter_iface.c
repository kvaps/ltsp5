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
char *
get_userid()
{
    FILE *xdg;
    char username[BUFSIZ];
    char password[BUFSIZ];
    char dummy[BUFSIZ];
    char *p;

    xdg = popen("ldmgtkgreet", "r");

    fgets(username, sizeof username, xdg);
    fgets(password, sizeof password, xdg);
    fgets(dummy, sizeof dummy, xdg);
    fgets(dummy, sizeof dummy, xdg);
    pclose(xdg);

    username[strlen(username) - 1] = '\0';
    password[strlen(password) - 1] = '\0';

    p = strdup(username);
    localpasswd = strdup(password);
    return p;
}
    
char *
get_passwd()
{
    return localpasswd;
}
