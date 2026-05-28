#include <utility/list.h>

#include "monitor.h"

LIST(uint32_t, cookie_ids);

xcb_randr_output_t primary_output = 1;

/* fake outputs */
struct output fake_outputs[] = {
    { 1, "primary", 11, 0 },
    { 2, "secondary", 12, 0 }
};

/* two side by side fake monitors */
struct monitor fake_monitors[] = {
    { 11, 21, 0, 0, 800, 600, 1 },
    { 12, 22, 800, 0, 800, 600, 1 }
};

/* Return an empty cookie. */
xcb_randr_get_screen_resources_cookie_t fake_get_screen_resources(void)
{
    xcb_randr_get_screen_resources_cookie_t cookie;
    ZERO(&cookie, 1);
    return cookie;
}

/* Create a fake screen resources reply. */
xcb_randr_get_screen_resources_reply_t *fake_get_screen_resources_reply(void)
{
    xcb_randr_get_screen_resources_reply_t *reply;
    uint32_t *base;

    reply = xmalloc(sizeof(*reply) + (SIZE(fake_monitors) + SIZE(fake_outputs)) * sizeof(uint32_t));

    reply->num_crtcs = SIZE(fake_monitors);
    base = (uint32_t*) ((char*) reply + sizeof(*reply));
    for (size_t i = 0; i < SIZE(fake_monitors); i++) {
        base[i] = fake_monitors[i].id;
    }

    reply->num_outputs = SIZE(fake_outputs);
    base = (uint32_t*) ((char*) reply + sizeof(*reply) + SIZE(fake_monitors) * sizeof(uint32_t));
    for (size_t i = 0; i < SIZE(fake_outputs); i++) {
        base[i] = fake_outputs[i].id;
    }

    reply->num_modes = 0;
    reply->names_len = 0;

    return reply;
}

/* Store the ID associated to a cookie. */
static unsigned store_new_cookie_id(uint32_t data)
{
    size_t index;

    for (index = 0; index < cookie_ids_length; index++) {
        if (cookie_ids[index] == XCB_NONE) {
            break;
        }
    }
    if (index == cookie_ids_length) {
        LIST_APPEND_VALUE(cookie_ids, data);
    }

    return index;
}

/* Return a cookie with internal sequence number for the _reply version. */
xcb_randr_get_output_info_cookie_t fake_get_output_info(xcb_randr_output_t output)
{
    xcb_randr_get_output_info_cookie_t cookie;
    cookie.sequence = store_new_cookie_id(output);
    return cookie;
}

/* Create a fake reply for getting output information. */
xcb_randr_get_output_info_reply_t *fake_get_output_info_reply(xcb_randr_get_output_info_cookie_t cookie)
{
    const xcb_randr_output_t output = cookie_ids[cookie.sequence];
    cookie_ids[cookie.sequence] = XCB_NONE;
    for (size_t i = 0; i < SIZE(fake_outputs); i++) {
        if (fake_outputs[i].id == output) {
            xcb_randr_get_output_info_reply_t *reply;

            /* +1 for the null terminator */
            const size_t name_len = strlen(fake_outputs[i].name) + 1;

            /* put the name at the end of the structure */
            reply = xcalloc(1, sizeof(*reply) + name_len);
            reply->name_len = name_len;
            reply->crtc = fake_outputs[i].crtc;
            reply->connection = fake_outputs[i].connection;
            memcpy((char*) reply + sizeof(*reply), fake_outputs[i].name, name_len);
            return reply;
        }
    }
    return NULL;
}

/* Return a cookie with internal sequence number for the _reply version. */
xcb_randr_get_crtc_info_cookie_t fake_get_crtc_info(xcb_randr_crtc_t crtc)
{
    xcb_randr_get_crtc_info_cookie_t cookie;
    cookie.sequence = store_new_cookie_id(crtc);
    return cookie;
}

/* Create a fake reply for getting crtc information. */
xcb_randr_get_crtc_info_reply_t *fake_get_crtc_info_reply(xcb_randr_get_crtc_info_cookie_t cookie)
{
    const xcb_randr_crtc_t crtc = cookie_ids[cookie.sequence];
    cookie_ids[cookie.sequence] = XCB_NONE;
    for (size_t i = 0; i < SIZE(fake_monitors); i++) {
        if (fake_monitors[i].id == crtc) {
            xcb_randr_get_crtc_info_reply_t *reply;

            ALLOCATE_ZERO(reply, 1);
            reply->x = fake_monitors[i].x;
            reply->y = fake_monitors[i].y;
            reply->width = fake_monitors[i].width;
            reply->height = fake_monitors[i].height;
            reply->mode = fake_monitors[i].mode;
            reply->rotation = fake_monitors[i].rotation;
            return reply;
        }
    }
    return NULL;
}

/* Return an empty cookie, the value does not matter. */
xcb_randr_get_output_primary_cookie_t fake_get_output_primary(void)
{
    xcb_randr_get_output_primary_cookie_t cookie;
    ZERO(&cookie, 1);
    return cookie;
}

/* Simply return the current primary output. */
xcb_randr_get_output_primary_reply_t *fake_get_output_primary_reply(void)
{
    xcb_randr_get_output_primary_reply_t *reply;

    ALLOCATE(reply, 1);
    reply->output = primary_output;
    return reply;
}
