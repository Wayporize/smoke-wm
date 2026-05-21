#include <utility/utility.h>

#include <X11/Xlib.h>

/**
 * This window is a simple implementation of handling `WM_TAKE_FOCUS`.
 */

int main(int argc, char **argv)
{
    bool self_test;
    Display *display;
    Window window, child_window = None;
    XSetWindowAttributes attributes;
    Atom wm_protocols_atom;
    Atom wm_take_focus_atom;
    Atom protocols[1];

    /* make stdout line buffered */
    setvbuf(stdout, NULL, _IOLBF, 0);

    if (argc != 2) {
        exit(EXIT_FAILURE);
    }
    self_test = strcmp(argv[1], "self") == 0;

    display = XOpenDisplay(NULL);

    window = XCreateWindow(display, DefaultRootWindow(display), 0, 0, 25, 25, 0,
            CopyFromParent, CopyFromParent, CopyFromParent, 0, NULL);
    if (!self_test) {
        child_window = XCreateWindow(display, window, 0, 0, 12, 25, 0,
                CopyFromParent, CopyFromParent, CopyFromParent, 0, NULL);
        XMapWindow(display, child_window);
    }

    printf("id %#lx\n", window);

    wm_protocols_atom = XInternAtom(display, "WM_PROTOCOLS", False);
    wm_take_focus_atom = XInternAtom(display, "WM_TAKE_FOCUS", False);
    protocols[0] = wm_take_focus_atom;
    XSetWMProtocols(display, window, protocols, SIZE(protocols));

    XMapWindow(display, window);

    XEvent event;
    while (XNextEvent(display, &event), True) {
        switch (event.type) {
        case ClientMessage:
            if (event.xclient.message_type == wm_protocols_atom) {
                if ((unsigned long) event.xclient.data.l[0] == wm_take_focus_atom) {
                    printf("got WM_TAKE_FOCUS\n");
                    if (self_test) {
                        XSetInputFocus(display, window, RevertToParent, event.xclient.data.l[1]);
                    } else {
                        XSetInputFocus(display, child_window, RevertToParent, event.xclient.data.l[1]);
                    }
                }
            }
            break;
        }
    }

    return 0;
}
