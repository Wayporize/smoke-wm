#include <utility/list.h>
#include <utility/utility.h>

#include "display.h"
#include "monitor.h"

STATIC_LIST(struct output, outputs);
STATIC_LIST(struct monitor, monitors);

/* primary output device */
xcb_randr_output_t primary;

/* Initialize the output and monitor list with the current RandR configuration. */
void initialize_monitor_setup(xcb_randr_get_screen_resources_cookie_t cookie)
{
    xcb_randr_get_screen_resources_reply_t *reply;
    xcb_randr_get_output_primary_cookie_t primary_cookie;
    xcb_randr_get_output_primary_reply_t *primary_reply;
    xcb_randr_output_t *output_ids;
    xcb_randr_crtc_t *monitor_ids;

    reply = xcb_randr_get_screen_resources_reply(display.xcb, cookie, NULL);
    ASSERT(reply != NULL, "could not get screen resources");

    output_ids = xcb_randr_get_screen_resources_outputs(reply);
    outputs_length = xcb_randr_get_screen_resources_outputs_length(reply);
    monitor_ids = xcb_randr_get_screen_resources_crtcs(reply);
    monitors_length = xcb_randr_get_screen_resources_crtcs_length(reply);

    /* send out a bunch of requests at once */
    primary_cookie = xcb_randr_get_output_primary(display.xcb, display.root);

    xcb_randr_get_output_info_cookie_t output_info_cookies[outputs_length];
    for (size_t i = 0; i < outputs_length; i++) {
        output_info_cookies[i] = xcb_randr_get_output_info(display.xcb,
                output_ids[i], reply->config_timestamp);
    }

    xcb_randr_get_crtc_info_cookie_t crtc_info_cookies[monitors_length];
    for (size_t i = 0; i < monitors_length; i++) {
        crtc_info_cookies[i] = xcb_randr_get_crtc_info(display.xcb,
                monitor_ids[i], reply->config_timestamp);
    }

    /* start filling our local output/monitor configuration */
    ALLOCATE_ZERO(outputs, outputs_length);
    ALLOCATE_ZERO(monitors, monitors_length);

    /* get information about all outputs */
    for (size_t i = 0; i < outputs_length; i++) {
        xcb_randr_get_output_info_reply_t *info_reply;
        uint8_t *name;
        int name_length;

        info_reply = xcb_randr_get_output_info_reply(display.xcb, output_info_cookies[i], NULL);
        ASSERT(info_reply != NULL, "could not get output info");

        outputs[i].id = output_ids[i];
        name = xcb_randr_get_output_info_name(info_reply);
        name_length = xcb_randr_get_output_info_name_length(info_reply);
        outputs[i].name = xstrndup((char*) name, name_length);
        outputs[i].crtc = info_reply->crtc;
        outputs[i].connection = info_reply->connection;

        free(info_reply);
    }

    /* get information about all monitors */
    for (size_t i = 0; i < monitors_length; i++) {
        xcb_randr_get_crtc_info_reply_t *info_reply;

        info_reply = xcb_randr_get_crtc_info_reply(display.xcb, crtc_info_cookies[i], NULL);
        ASSERT(info_reply != NULL, "could not get crtc info");

        monitors[i].id = monitor_ids[i];
        monitors[i].x = info_reply->x;
        monitors[i].y = info_reply->y;
        monitors[i].width = info_reply->width;
        monitors[i].height = info_reply->height;
        monitors[i].rotation = info_reply->rotation;

        free(info_reply);
    }

    free(reply);

    primary_reply = xcb_randr_get_output_primary_reply(display.xcb, primary_cookie, NULL);
    ASSERT(primary_reply != NULL, "failed to get primary output");
    primary = primary_reply->output;
    free(primary_reply);

    /* TODO: dump configuration for testing */
}

/* Cache output properties. */
void change_output(xcb_randr_output_t output, xcb_randr_crtc_t crtc, xcb_randr_mode_t mode,
        xcb_randr_rotation_t rotation, xcb_randr_connection_t connection);

/* Cache crtc properties. */
void change_crtc(xcb_randr_crtc_t crtc, xcb_randr_mode_t mode, xcb_randr_rotation_t rotation,
        int32_t x, int32_t y, int32_t width, int32_t height);
