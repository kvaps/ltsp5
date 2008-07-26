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
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <glib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xproto.h>
#include <X11/Xatom.h>
#include <X11/Xresource.h>
#include <X11/Xlib.h>


#define ATOM_PROGRAM  "LTSP_COMMAND"
#define MIN_UID 500

/*
 * Globals
 */

Atom prog_atom;                 /* Our communication atom */
Display *dpy;                   /* Display */
Window root;                    /* root window */
struct passwd *pw;

void
signal_handler(sig)
{
    XCloseDisplay(dpy);
    exit(0);
}

void
setup_env()
{
    /* Become a process group leader */
    (void) setsid();
}

int
atom_exec(char *command)
{
    gchar **argv = NULL;        /* To parse up command line */
    GPid  pid;
    GSpawnFlags flags = G_SPAWN_SEARCH_PATH;

    if (!g_shell_parse_argv (command, NULL, &argv, NULL)) {
        return FALSE;
    }

    if (!g_spawn_async(pw->pw_dir, argv, NULL, flags, setup_env, NULL, &pid,
        NULL)) {
        return FALSE;
    }
    return TRUE;
}

void
handle_data()
{
    char *command;
    Atom type;
    int format, res;
    unsigned long nitems, nleft;


    res = XGetWindowProperty(dpy, root, prog_atom, 0, 512, 0, XA_STRING,
                             &type, &format, &nitems, &nleft,
                             (unsigned char **) &command);

    if (res != Success) {
        return;
    } else if (type != XA_STRING) {
        XFree(command);
        return;
    }

    /*
     * We now have a string (command).
     */

    if (atom_exec(command) == FALSE) {
        fprintf(stderr, "Couldn't execute.\n");
        /* syslog something here */
    }
    XFree(command);                              /* free up returned data */
}

int
main(int argc, char *argv[])
{
    XEvent ev;
    gchar *d = getenv("DISPLAY");
    gchar *username = getenv("LDM_USERNAME");	/* passed by the ldm rc.d env */

    /*
     * check password entry here
     */

    pw = getpwnam(username);

    if (!pw) {
        exit(1);
    }

    if (pw->pw_uid < MIN_UID) {         /* only allow exec as unpriv user */
        exit(1);
    }

    /*
     * We have a valid LDM_USERNAME that we have a valid passwd entry for.
     */

    daemon(0,0);

    /* set $USER */
    if (setenv("USER", pw->pw_name, 1) == -1) {
        return;
    }

    /* set $USERNAME */
    if (setenv("USERNAME", pw->pw_name, 1) == -1) {
        return;
    }

    /* set $HOME */
    if (setenv("HOME", pw->pw_dir, 1) == -1) {
        return;
    }

    /* set $SHELL */
    if (setenv("SHELL", pw->pw_shell, 1) == -1) {
        return;
    }

    /* set supplementary groups */
    if (initgroups(pw->pw_name, pw->pw_gid) == -1) {
        return FALSE;
    }

    /* set gid */
    if (setgid(pw->pw_gid) == -1) {
        return FALSE;
    }

    /* set uid */
    if (setuid(pw->pw_uid) == -1) {
        return FALSE;
    }

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
     * We've got the root window now.  First, register our needed atom.
     */

    prog_atom = XInternAtom(dpy, ATOM_PROGRAM, False);

    if (prog_atom == None) {
        printf("Couldn't allocate atom for %s\n", ATOM_PROGRAM);
        exit(1);
    }

    /*
     * Declare our interest in obtaining PropertyChange events.
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
