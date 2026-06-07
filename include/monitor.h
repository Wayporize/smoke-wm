#ifndef MONITOR_H
#define MONITOR_H

#include <utility/attributes.h>
#include <utility/types.h>

#include <xcb/randr.h>

/* Screen/Monitor management using the XRandr extension (not Xinerama because
 * it is old and dumb).
 */

/* physical output device */
struct output {
    /* identifier of this output */
    xcb_randr_output_t id;
    /* UTF-8 encoded name of this output device */
    utf8_t *name;
    /* crtc projected onto this output */
    xcb_randr_crtc_t crtc;
    /* connection status of this output */
    xcb_randr_connection_t connection;
};

/* each monitor corresponds to a CRTC (rectangular subsection of the screen) */
struct monitor {
    /* the crtc identifier */
    xcb_randr_crtc_t id;
    /* the crtc is disable if this is `XCB_NONE` */
    xcb_randr_mode_t mode;
    /* position and size of the monitor (CRTC) */
    int32_t x, y, width, height;
    /* rotation of the crtc */
    xcb_randr_rotation_t rotation;
};

/* Initialize the output and monitor list with the current RandR configuration. */
void initialize_monitor_setup(xcb_randr_get_screen_resources_cookie_t cookie);

/* Get the monitor that is projected onto the output with given name. */
xcb_randr_crtc_t get_monitor_from_output_name(const utf8_t *name);

/* Get the position and size of a monitor. */
void get_monitor_rectangle(xcb_randr_crtc_t crtc, struct rectangle *rectangle);

/* Get the monitor that intersects given rectangle most.
 *
 * If the mid point of the rectangle is contained in any monitor, this will have
 * priority over the interection area.
 */
xcb_randr_crtc_t get_monitor_from_rectangle(int32_t x, int32_t y, int32_t width, int32_t height);

/* Cache output properties. */
void change_output(xcb_randr_output_t output, xcb_randr_crtc_t crtc,
        xcb_randr_connection_t connection, xcb_timestamp_t config_timestamp);

/* Cache crtc properties. */
void change_crtc(xcb_randr_crtc_t crtc, xcb_randr_mode_t mode, xcb_randr_rotation_t rotation,
        int32_t x, int32_t y, int32_t width, int32_t height);

#endif
