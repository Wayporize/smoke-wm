#include <stdlib.h>
#include <xcb/xkb.h>
#include <xkbcommon/xkbcommon-x11.h>

#include "x11.h"

/* the information retrieved from the X server and Xkb context */
struct display display;

/* Open the X11 connection and initialize Xkb. */
void open_display(void)
{
    int connection_error;
    xcb_xkb_use_extension_cookie_t xkb_cookie;
    xcb_xkb_get_device_info_cookie_t device_cookie;
    const xcb_query_extension_reply_t *extension_reply;
    xcb_xkb_use_extension_reply_t *xkb_reply;
    xcb_xkb_get_device_info_reply_t *device_reply;
    xcb_generic_error_t *error;

    /* connect to the X server */
    display.xcb = xcb_connect(NULL, NULL);
    connection_error = xcb_connection_has_error(display.xcb);
    switch (connection_error) {
    case XCB_CONN_ERROR:
        printf("socket, pipe or other error\n");
        exit(1);
    case XCB_CONN_CLOSED_EXT_NOTSUPPORTED:
        printf("extension not supported\n");
        exit(1);
    case XCB_CONN_CLOSED_MEM_INSUFFICIENT:
        printf("memory was insufficient\n");
        exit(1);
    case XCB_CONN_CLOSED_REQ_LEN_EXCEED:
        printf("exceeded request length that server accepts\n");
        exit(1);
    case XCB_CONN_CLOSED_PARSE_ERR:
        printf("error parsing display string\n");
        exit(1);
    case XCB_CONN_CLOSED_INVALID_SCREEN:
        printf("server does not have a screen matching the display\n");
        exit(1);
    }

    xcb_prefetch_extension_data(display.xcb, &xcb_xkb_id);

    xkb_cookie = xcb_xkb_use_extension(display.xcb,
            XCB_XKB_MAJOR_VERSION, XCB_XKB_MINOR_VERSION);

    device_cookie = xcb_xkb_get_device_info(display.xcb,
            XCB_XKB_ID_USE_CORE_KBD, 0, 0, 0, 0, 0, 0);

    extension_reply = xcb_get_extension_data(display.xcb, &xcb_xkb_id);
    if (extension_reply == NULL) {
        printf("failed to query xcb extension data for xkb\n");
        exit(1);
    } else if (!extension_reply->present) {
        printf("xkb is not available on the server\n");
        exit(1);
    }

    display.xkb_base_event = extension_reply->first_event;
    display.xkb_base_error = extension_reply->first_error;

    xkb_reply = xcb_xkb_use_extension_reply(display.xcb, xkb_cookie, &error);
    if (xkb_reply == NULL) {
        printf("using xcb extension xkb failed: error code %d\n",
                error->error_code);
        free(error);
        exit(1);
    } else if (!xkb_reply->supported) {
        printf("server does not support xkb version %d.%d\n",
                XCB_XKB_MAJOR_VERSION, XCB_XKB_MINOR_VERSION);
        exit(1);
    }

    free(xkb_reply);

    device_reply = xcb_xkb_get_device_info_reply(display.xcb, device_cookie,
            &error);
    if (device_reply == NULL) {
        printf("getting xkb device info failed: error code %d\n",
                error->error_code);
        free(error);
        exit(1);
    }

    display.device_id = device_reply->deviceID;

    free(device_reply);

    display.xkb = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (display.xkb == NULL) {
        printf("could not create xkb context\n");
        exit(1);
    }
    display.keymap = xkb_x11_keymap_new_from_device(display.xkb, display.xcb,
            display.device_id, 0);
    if (display.keymap == NULL) {
        printf("could not create keymap from core keyboard device\n");
        exit(1);
    }
}
