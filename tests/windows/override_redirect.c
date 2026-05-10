#include <X11/Xlib.h>

/**
 * This is a simple `override_redirect` window that is mapped and then listens
 * for events without actually doing anything.
 *
 * cc *.c -o * -lX11
 */

int main(void)
{
    Display *display;
    Window window;
    XSetWindowAttributes attributes;

    display = XOpenDisplay(NULL);

    attributes.override_redirect = True;
    window = XCreateWindow(display, DefaultRootWindow(display), 0, 0, 25, 25, 0,
            CopyFromParent, CopyFromParent, CopyFromParent, CWOverrideRedirect, &attributes);

    XMapWindow(display, window);

    XEvent event;
    while (XNextEvent(display, &event), true) {
    }

    return 0;
}
