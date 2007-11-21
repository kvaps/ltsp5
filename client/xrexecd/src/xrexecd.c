/*
 * xrexec.c: - rexec via atoms.
 *
 * (c) 2006,2007 Scott Balneaves
 *
 * This software is distributed under the terms and conditions of the
 * GNU General Public License. See file COPYING for the full text of
 * the license.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pwd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xproto.h>
#include <X11/Xatom.h>
#include <X11/Xresource.h>
#include <X11/Xlib.h>


#define ATOM_USERNAME "LTSP_USERNAME"
#define ATOM_PROGRAM  "LTSP_COMMAND"
#define LOCKFILE      "/var/run/xrexecd.pid"
#define MIN_UID 500

/*
 * Globals
 */

Atom user_atom;                 /* Our communication atom */
Atom prog_atom;                 /* Our communication atom */
Display *dpy;                   /* Display */
Window root;                    /* root window */

void
signal_handler(sig)
{
    XCloseDisplay(dpy);
    exit(0);
}

void
run_command(struct passwd *pw, char *command)
{
    int i;
    i = open("/dev/null", O_RDWR);               /* stdin */
    dup(i);                                      /* stdout */
    dup(i);                                      /* stderr */

    /* set supplementary groups */
    if (initgroups(pw->pw_name, pw->pw_gid) == -1) {
        exit(1);
    }

    /* set gid */
    if (setgid(pw->pw_gid) == -1) {
        exit(1);
    }

    /* set uid */
    if (setuid(pw->pw_uid) == -1) {
        exit(1);
    }

    /* change to home dir */
    if (chdir(pw->pw_dir) == -1) {
        exit(1);
    }

    umask(027);

    /* set $USER */
    if (setenv("USER", pw->pw_name, 1) == -1) {
        exit(1);
    }

    /* set $HOME */
    if (setenv("HOME", pw->pw_dir, 1) == -1) {
        exit(1);
    }

    /* set $SHELL */
    if (setenv("SHELL", pw->pw_shell, 1) == -1) {
        exit(1);
    }

    system(command);
    exit(0);
}

/*
 * domount: actually bindmounts path1 onto path2.
 */

void
atom_exec(struct passwd *pw, const char *command)
{
    int status;
    pid_t child;

    child = fork();

    if (child == 0) {
        int i;
        pid_t child1;

        setsid();                                /* be process group leader */
        for (i = getdtablesize(); i >= 0; i--)   /* close all open fd's */
            close(i);

        child1 = fork();			/* fork again */

        if (child1 == 0)
            run_command(pw, (char *)command);
        else
            exit(0);
    } else if (child > 0) {
        return;
    } else if (child < 0) {
        perror("Error: fork() failed");
        exit(1);
    }

}

void
handle_data()
{
    char *command;
    char *username;
    Atom type;
    int format, res;
    unsigned long nitems, nleft;
    struct passwd *pw;

    res = XGetWindowProperty(dpy, root, user_atom, 0, 512, 0, XA_STRING,
                             &type, &format, &nitems, &nleft,
                             (unsigned char **) &username);

    if (res != Success)
        return;
    if (type != XA_STRING) {
        XFree(username);
        return;
    }

    /*
     * check password entry here
     */

    pw = getpwnam(username);
    XFree(username);

    if (!pw)
        return;

    if (pw->pw_uid < MIN_UID)                    /* only allow exec as unpriv user */
        return;

    res = XGetWindowProperty(dpy, root, prog_atom, 0, 512, 0, XA_STRING,
                             &type, &format, &nitems, &nleft,
                             (unsigned char **) &command);

    if (res != Success)
        return;
    if (type != XA_STRING) {
        XFree(command);
        return;
    }

    /*
     * We now have a string (username) and a string (command).
     */

    atom_exec(pw, command);
    XFree(command);                              /* free up returned data */
}

int
main(int argc, char *argv[])
{
    XEvent ev;
    char *d = getenv("DISPLAY");
    char str[10];
    int lockfile;

    daemon(0,0);

    lockfile = open(LOCKFILE, O_RDWR|O_CREAT, 0640);
    if (lockfile < 0)
        exit(1);
    if (lockf(lockfile, F_TLOCK, 0) < 0)
	exit(1);
    sprintf(str, "%d\n", getpid());
    write(lockfile, str, strlen(str));


    /*
     * Open our display
     */

    if (!(dpy = XOpenDisplay(d))) {
        fprintf(stderr, "Cannot open X display.\n");
        exit(1);
    }

    signal(SIGCHLD, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGHUP, signal_handler);
    signal(SIGTERM, signal_handler);

    root = RootWindow(dpy, DefaultScreen(dpy));

    /*
     * We've got the root window now.  First, register our needed atoms.
     */

    user_atom = XInternAtom(dpy, ATOM_USERNAME, False);
    prog_atom = XInternAtom(dpy, ATOM_PROGRAM, False);

    if (user_atom == None) {
        printf("Couldn't allocate atom for %s\n", ATOM_USERNAME);
        exit(1);
    }

    if (prog_atom == None) {
        printf("Couldn't allocate atom for %s\n", ATOM_PROGRAM);
        exit(1);
    }

    /*
     * declare our interest in obtaining PropertyChange events.
     */

    XSelectInput(dpy, root, PropertyChangeMask);

    /*
     * Finally, sit in our event loop, and search for the property changes.
     */

    for (;;) {
        XNextEvent(dpy, &ev);
        switch (ev.type) {
        case PropertyNotify:
            if (ev.xproperty.atom == prog_atom)
                handle_data();
        }
    }

    XCloseDisplay(dpy);
    return 0;
}
