#ifndef TESTS__FAKE_RANDR_H
#define TESTS__FAKE_RANDR_H

#include <xcb/randr.h>

/* Return an empty cookie. */
xcb_randr_get_screen_resources_cookie_t fake_get_screen_resources(void);

/* Create a fake screen resources reply. */
xcb_randr_get_screen_resources_reply_t *fake_get_screen_resources_reply(void);

/* Return a cookie with internal sequence number for the _reply version. */
xcb_randr_get_output_info_cookie_t fake_get_output_info(xcb_randr_output_t output);

/* Create a fake reply for getting output information. */
xcb_randr_get_output_info_reply_t *fake_get_output_info_reply(xcb_randr_get_output_info_cookie_t cookie);

/* Return a cookie with internal sequence number for the _reply version. */
xcb_randr_get_crtc_info_cookie_t fake_get_crtc_info(xcb_randr_crtc_t crtc);

/* Create a fake reply for getting crtc information. */
xcb_randr_get_crtc_info_reply_t *fake_get_crtc_info_reply(xcb_randr_get_crtc_info_cookie_t cookie);

/* Return an empty cookie. */
xcb_randr_get_output_primary_cookie_t fake_get_output_primary(void);

/* Simply return the current primary output. */
xcb_randr_get_output_primary_reply_t *fake_get_output_primary_reply(void);

/* Overwrite the used RandR function calls. */

#define xcb_randr_select_input(xcb, root, flags) ((void) 0)

#define xcb_randr_get_screen_resources(xcb, root) \
    fake_get_screen_resources()
#define xcb_randr_get_screen_resources_reply(xcb, cookie, error) \
    fake_get_screen_resources_reply()

#define xcb_randr_get_output_info(xcb, output, timestamp) \
    fake_get_output_info(output)
#define xcb_randr_get_output_info_reply(xcb, cookie, error) \
    fake_get_output_info_reply(cookie)

#define xcb_randr_get_crtc_info(xcb, crtc, timestamp) \
    fake_get_crtc_info(crtc)
#define xcb_randr_get_crtc_info_reply(xcb, cookie, error) \
    fake_get_crtc_info_reply(cookie)

#define xcb_randr_get_output_primary(xcb, root) \
    fake_get_output_primary()
#define xcb_randr_get_output_primary_reply(xcb, cookie, error) \
    fake_get_output_primary_reply()

#endif
