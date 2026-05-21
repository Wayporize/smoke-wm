#include <inttypes.h>
#include <utility/list.h>
#include <utility/log.h>
#include <utility/utility.h>

#include "display.h"
#include "monitor.h"

/* list of output devices the user has */
STATIC_LIST(struct output, outputs);

/* sub regions of the screen */
STATIC_LIST(struct monitor, monitors);

/* primary output device */
xcb_randr_output_t primary;

/* Get information about an output. */
static struct output *get_output(xcb_randr_output_t id)
{
    for (size_t i = 0; i < outputs_length; i++) {
        if (outputs[i].id == id) {
            return &outputs[i];
        }
    }
    return NULL;
}

/* Get information about a monitor. */
static struct monitor *get_monitor(xcb_randr_crtc_t id)
{
    for (size_t i = 0; i < monitors_length; i++) {
        if (monitors[i].id == id) {
            return &monitors[i];
        }
    }
    return NULL;
}

/* Dump the monitor setup to stdout. */
static void dump_monitor_setup(void)
{
    printf("primary %" PRIu32 "\n", primary);
    for (size_t i = 0; i < monitors_length; i++) {
        printf("monitor %u: %" PRId32 "x%" PRId32 "+%" PRId32 "+%" PRId32 " %u\n",
                monitors[i].id, monitors[i].width, monitors[i].height,
                monitors[i].x, monitors[i].y, monitors[i].rotation);
    }
    for (size_t i = 0; i < outputs_length; i++) {
        printf("output %u: %s %" PRIu32 " %s\n", outputs[i].id,
                outputs[i].name, outputs[i].crtc,
                outputs[i].connection == XCB_RANDR_CONNECTION_CONNECTED ? "connected" :
                outputs[i].connection == XCB_RANDR_CONNECTION_DISCONNECTED ? "disconnected" :
                "unknown");
    }
}

/* Initialize the output and monitor list with the current RandR configuration. */
void initialize_monitor_setup(xcb_randr_get_screen_resources_cookie_t cookie)
{
    xcb_randr_get_screen_resources_reply_t *reply;
    xcb_randr_output_t *output_ids;
    xcb_randr_crtc_t *monitor_ids;
    xcb_randr_get_output_primary_cookie_t primary_cookie;
    xcb_randr_get_output_primary_reply_t *primary_reply;

    reply = xcb_randr_get_screen_resources_reply(display.xcb, cookie, NULL);
    ASSERT(reply != NULL, "could not get screen resources");

    output_ids = xcb_randr_get_screen_resources_outputs(reply);
    outputs_length = xcb_randr_get_screen_resources_outputs_length(reply);
    monitor_ids = xcb_randr_get_screen_resources_crtcs(reply);
    monitors_length = xcb_randr_get_screen_resources_crtcs_length(reply);

    /* send out a bunch of requests at once */
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

    primary_cookie = xcb_randr_get_output_primary(display.xcb, display.root);

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
        monitors[i].mode = info_reply->mode;
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

    /* associated to test "randr-setup" */
    notef("start of dumping monitor setup\n");
    dump_monitor_setup();
    notef("end of dumping monitor setup\n");
}

/* Cache output properties. */
void change_output(xcb_randr_output_t output, xcb_randr_crtc_t crtc, xcb_randr_connection_t connection,
        xcb_timestamp_t config_timestamp)
{
    struct output *info;

    notef("randr: output %" PRIu32 " changed: %" PRIu32 " %u" "\n", output,
            crtc, connection);
    info = get_output(output);
    if (info == NULL) {
        xcb_randr_get_output_info_cookie_t cookie;
        xcb_randr_get_output_info_reply_t *reply;
        uint8_t *name;
        int name_length;

        notef("this output is new\n");

        /* send out a request for the name, this is so rare that it does not
         * need to be efficient, in fact it might never happen once in a user's
         * lifetime
         */
        cookie = xcb_randr_get_output_info(display.xcb, output, config_timestamp);
        reply = xcb_randr_get_output_info_reply(display.xcb, cookie, NULL);
        ASSERT(reply != NULL, "could not get output info");

        LIST_APPEND(outputs, NULL, 1);
        info = &outputs[outputs_length - 1];
        info->id = output;
        name = xcb_randr_get_output_info_name(reply);
        name_length = xcb_randr_get_output_info_name_length(reply);
        info->name = xstrndup((char*) name, name_length);
        info->crtc = reply->crtc;
        info->connection = reply->connection;
        free(reply);
        /* TODO: now maybe a workspaces need to be added or windows configured
         * to be on this output should be moved to it if not explicitly moved
         * away some time in the past in case a crtc is present
         */
    } else {
        /* TODO: if the crtc changed, we need to inform the user as windows
         * might be hidden now
         */
        info->crtc = crtc;
        /* TODO: if the connection is disconnected, should it be treated the same
         * as no crtc?
         */
        info->connection = connection;
    }
}

/* Cache crtc properties. */
void change_crtc(xcb_randr_crtc_t crtc, xcb_randr_mode_t mode, xcb_randr_rotation_t rotation,
        int32_t x, int32_t y, int32_t width, int32_t height)
{
    struct monitor *info;

    notef("randr: crtc %" PRIu32 " changed: %" PRIu32 " %u %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32 "\n",
            crtc, mode, rotation, x, y, width, height);
    info = get_monitor(crtc);
    if (info == NULL) {
        notef("this crtc is new\n");

        LIST_APPEND(monitors, NULL, 1);
        info = &monitors[monitors_length - 1];
        info->id = crtc;
        info->mode = mode;
        info->rotation = rotation;
        info->x = x;
        info->y = y;
        info->width = width;
        info->height = height;
        /* TODO: new content might be visible now */
    } else {
        /* TODO: the size might have changed, need to adjust tiling windows and
         * put windows in bounds
         */
        if (mode == XCB_NONE) {
            /* TODO: delete this crtc? */
            info->mode = mode;
            info->rotation = rotation;
            /* do not set the position and size here! */
        } else {
            /* TODO: if the previous mode was `XCB_NONE`, there might be some
             * new visible content now
             */
            info->mode = mode;
            info->rotation = rotation;
            info->x = x;
            info->y = y;
            info->width = width;
            info->height = height;
        }
    }
}
