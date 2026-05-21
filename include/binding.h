#ifndef BINDING_H
#define BINDING_H

#include <xcb/xproto.h>
#include <xkbcommon/xkbcommon.h>

#include <utility/attributes.h>

/**
 * Bindings are actions associated to physical keys.
 * They either trigger when pressing a key down or releasing the key.
 */

#include "action.h"

/* Dump all bindings created to stdout. */
void dump_bindings(void);

/* Clear all bindings. */
void clear_bindings(void);

/* Associate a key (with modifiers) on the keyboard with an action.
 *
 * If the key binding already exists, the action is appended to the already
 * existing actions.
 *
 * @is_release controls whether to trigger the binding on press or release.
 */
void append_key_binding(bool is_release, xkb_mod_mask_t modifiers,
        xkb_keycode_t key_code, struct action action);

/* Set a key binding using a key symbol.
 *
 * This is a series of calls to `append_key_binding()` for each key that has the
 * given key symbol.
 */
void append_key_symbol_binding(bool is_release, xkb_mod_mask_t modifiers,
        xkb_keysym_t key_symbol, struct action action);

/* Get action associated to a key binding. */
const struct action *get_key_binding(bool is_release, xkb_mod_mask_t modifiers,
        xkb_keycode_t key_code);

/* Associate a button on the mouse or other device with an action.
 *
 * If the button binding already exists, the action is appended to the already
 * existing actions.
 *
 * @is_release controls whether to trigger the binding on press or release.
 * @is_transparent controls whether to, in addition to triggering the action,
 *                 also send the event to the underlying window.
 */
void append_button_binding(bool is_release, bool is_transparent,
        xkb_mod_mask_t modifiers, xcb_button_t button, struct action action);

/* Get a list of actions associated to a button.
 *
 * @is_transparent[out] stores the `is_transparent` flag.
 *
 * @return a list of actions terminated by `.type = ACTION_NONE` or `NULL` if
 *         the binding does not exist.
 */
const struct action *get_button_binding(bool is_release,
        xkb_mod_mask_t modifiers, xcb_button_t button,
        _Out bool *is_transparent);

#endif
