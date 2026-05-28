#include <X11/Xlib.h>
#include <utility/utility.h>

#include <string.h>

int main(int argc, char **argv)
{
    Display *display;

    display = XOpenDisplay(NULL);

    if (strcmp(argv[1], "windowmove") == 0) {
        XEvent message;

        ZERO(&message, 1);
        message.type = ClientMessage;
        message.xclient.window = strtol(argv[2], NULL, 0);
        message.xclient.message_type = XInternAtom(display, "_NET_MOVERESIZE_WINDOW", False);
        message.xclient.format = 32;
        message.xclient.data.l[0] = (1 << 8) | (1 << 9);
        message.xclient.data.l[1] = strtol(argv[3], NULL, 0);
        message.xclient.data.l[2] = strtol(argv[4], NULL, 0);
        XSendEvent(display, DefaultRootWindow(display), False, SubstructureRedirectMask, &message);
    }

    XCloseDisplay(display);

    return 0;
}
