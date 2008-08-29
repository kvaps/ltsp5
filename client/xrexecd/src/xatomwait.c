/*
 * xatomwait.c: - wait for an atom to change, then exit.
 *
 * (c) 2006,2007,2008 Scott Balneaves
 *
 * This software is distributed under the terms and conditions of the
 * GNU General Public License. See file COPYING for the full text of
 * the license.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xproto.h>
#include <X11/Xatom.h>
#include <X11/Xresource.h>
#include <X11/Xlib.h>

int
handle_xerror(Display *d, XErrorEvent *e)
{
    XCloseDisplay(d);
    exit(1);
}

void
handle_propchange(Display *d, Window w, Atom a)
{
    char *result;
    Atom type;
    int format, res;
    unsigned long nitems, nleft;


    res = XGetWindowProperty(d, w, a, 0, 512, 0, XA_STRING,
                             &type, &format, &nitems, &nleft,
                             (unsigned char **) &result);

    if (res != Success) {
        return;
    } else if (type != XA_STRING) {
        XFree(result);
        return;
    }

    /*
     * We now have a string (command).
     */

    printf("%s\n", result);

    XFree(result);                              /* free up returned data */
}

int
main(int argc, char *argv[])
{
    Display *dpy;                   /* Display */
    Atom prog_atom;                 /* Our atom */
    Window root;                    /* root window */
    char *atomname;                 /* Our atom to watch for */
    XEvent ev;
    char *d = getenv("DISPLAY");

    if (argc != 2) {
        fprintf(stderr, "usage: %s [atomname]\n", argv[0]);
        exit(1);
    }

    /*
     * Open our display
     */

    if (!(dpy = XOpenDisplay(d))) {
        fprintf(stderr, "Cannot open X display.\n");
        exit(1);
    }

    atomname = argv[1];

    XSetErrorHandler(handle_xerror);

    root = RootWindow(dpy, DefaultScreen(dpy));

    /*
     * We've got the root window now.  First, register our needed atom.
     */

    prog_atom = XInternAtom(dpy, atomname, False);

    if (prog_atom == None) {
        printf("Couldn't allocate atom for %s\n", atomname);
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
        if (ev.type == PropertyNotify) {
            if (ev.xproperty.atom == prog_atom) {
                handle_propchange(dpy, root, prog_atom);
                break;
            }
        }
    }

    XCloseDisplay(dpy);
    return 0;
}
