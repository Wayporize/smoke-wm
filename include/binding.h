#ifndef BINDING_H
#define BINDING_H

#include <xkbcommon/xkbcommon.h>

/**
 * Bindings are actions associated to physical keys.
 * They either trigger when pressing a key down or releasing the key.
 */

#include "action.h"

#ifdef DEBUG

/* Dump all bindings created to stdout. */
void debug_dump_bindings(void);

#endif

/* Clear all bindings. */
void clear_bindings(void);

/* Associate a key (with modifiers) on the keyboard with an action.
 *
 * If the key binding already exists, the action is appended to the already
 * existing actions.
 *
 * @is_release controls whether to trigger the binding on press or release.
 */
void set_key_binding(bool is_release, xkb_mod_mask_t modifiers,
        xkb_keycode_t key_code, struct action action);

/* Set a key binding using a key symbol.
 *
 * This is a series of calls to `set_key_binding()` for each key that has the
 * given key symbol.
 */
void set_key_symbol_binding(bool is_release, xkb_mod_mask_t modifiers,
        xkb_keysym_t key_symbol, struct action action);

/* Get action associated to a key binding. */
const struct action *get_key_binding(bool is_release, xkb_mod_mask_t modifiers,
        xkb_keycode_t key_code);

#endif
