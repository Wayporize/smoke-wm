#ifndef BINDING_H
#define BINDING_H

#include <xkbcommon/xkbcommon.h>

/**
 * Bindings are actions associated to physical keys.
 * They either trigger when pressing a key down or releasing the key.
 */

#include "action.h"

/* Clear all bindings. */
void clear_bindings(void);

/* Set a key binding. */
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
