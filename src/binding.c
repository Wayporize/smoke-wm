#include <xcb/xproto.h>

#include <utility/utility.h>

/**
 * Bindings are stored in an array that associates a key code/modifiers with a
 * list of press and release actions.
 */

#include "binding.h"
#include "x11.h"

/* a binding consists of a list of actions for pressing and releasing */
struct binding {
    /* number of actions in `press_actions` */
    unsigned press_actions_length;
    /* number of actions in `release_actions` */
    unsigned release_actions_length;
    /* the actions to execute when the button/key is pressed */
    struct action *press_actions;
    /* the actions to execute when the button/key is released */
    struct action *release_actions;
};

/* key bindings, the first index is the key code, the other is the adjusted
 * modifiers (modifiers without lock mask)
 */
static struct binding key_bindings[256 - 8][256 >> 1];

#ifdef DEBUG

/* Dump all bindings created to stdout. */
void debug_dump_bindings(void)
{
    struct binding *binding;

    for (unsigned kc = 0; kc < SIZE(key_bindings); kc++) {
        for (unsigned m = 0; m < SIZE(key_bindings[0]); m++) {
            binding = &key_bindings[kc][m];
            if (binding->press_actions != NULL) {
                printf("P %u %u\n",
                        ((m << 1) | (m & 1)) & ~XCB_MOD_MASK_LOCK,
                        kc + 8);
            }
            if (binding->release_actions != NULL) {
                printf("R %u %u\n",
                        ((m << 1) | (m & 1)) & ~XCB_MOD_MASK_LOCK,
                        kc + 8);
            }
        }
    }
}

#endif

/* Clear all bindings. */
void clear_bindings(void)
{
    struct binding *binding;

    for (unsigned i = 0; i < SIZE(key_bindings); i++) {
        for (unsigned j = 0; j < SIZE(key_bindings[0]); j++) {
            binding = &key_bindings[i][j];
            if (binding->press_actions != NULL) {
                binding->press_actions_length = 0;
                free(binding->press_actions);
                binding->press_actions = NULL;
            }
            if (binding->release_actions != NULL) {
                binding->release_actions_length = 0;
                free(binding->release_actions);
                binding->release_actions = NULL;
            }
        }
    }
}

/* Get a pointer to the binding corresponding to given modifiers and key code.
 */
static struct binding *get_key_binding_pointer(xkb_mod_mask_t modifiers,
        xkb_keycode_t key_code)
{
    xkb_mod_mask_t saved_bits;
    unsigned ignore_modifiers = 0;

    /* ignore NumLock and ScrollLock modifiers */
    ignore_modifiers |= xkb_keymap_mod_get_mask(display.keymap,
            XKB_VMOD_NAME_NUM);
    ignore_modifiers |= xkb_keymap_mod_get_mask(display.keymap,
            XKB_VMOD_NAME_SCROLL);
    modifiers &= ~ignore_modifiers;

    /* get rid of the LOCK mask by shifting above bits into it */
    saved_bits = (modifiers & (XCB_MOD_MASK_LOCK - 1));
    modifiers >>= 1;
    modifiers |= saved_bits;
    modifiers &= 127;

    if (key_code < 8 || key_code >= 256) {
        return NULL;
    }

    key_code -= 8;

    return &key_bindings[key_code][modifiers];
}

/* Associate a key (with modifiers) on the keyboard with an action. */
void set_key_binding(bool is_release, xkb_mod_mask_t modifiers,
        xkb_keycode_t key_code, struct action action)
{
    struct binding *binding;

    binding = get_key_binding_pointer(modifiers, key_code);
    if (binding != NULL) {
        if (is_release) {
            REALLOCATE(binding->release_actions,
                    binding->release_actions_length + 1);
            binding->release_actions[binding->release_actions_length] = action;
            binding->release_actions_length++;
        } else {
            REALLOCATE(binding->press_actions,
                    binding->press_actions_length + 1);
            binding->press_actions[binding->press_actions_length] = action;
            binding->press_actions_length++;
        }
    }
}

/* struct to pass into `xkb_keymap_key_for_each()` for `set_bind_iterator()` */
struct key_iterator_context {
    /* current keyboard state to use */
    struct xkb_state *state;
    /* the key symbol to resolve */
    xkb_keysym_t key_symbol;
    /* information to pass to `set_key_binding()` */
    bool is_release;
    xkb_mod_mask_t modifiers;
    struct action action;
};

/* Iterator for key codes of a keymap. */
static void set_bind_iterator(struct xkb_keymap *keymap, xkb_keycode_t key_code,
        void *data)
{
    struct key_iterator_context *context;
    xkb_layout_index_t layout;
    xkb_level_index_t num_levels;
    const xkb_keysym_t *key_symbols;
    int count;

    context = data;

    /* for the layout the key code is on, try to find a match for a key symbol
     * on any level (modifier shift level)
     */
    layout = xkb_state_key_get_layout(context->state, key_code);
    num_levels = xkb_keymap_num_levels_for_key(keymap, key_code, layout);
    for (xkb_level_index_t level = 0; level < num_levels; level++) {
        count = xkb_keymap_key_get_syms_by_level(keymap, key_code, layout,
                level, &key_symbols);
        for (int i = 0; i < count; i++) {
            if (key_symbols[i] == context->key_symbol) {
                set_key_binding(context->is_release, context->modifiers,
                        key_code, context->action);
            }
        }
    }
}

/* Set a key binding using a key symbol. */
void set_key_symbol_binding(bool is_release, xkb_mod_mask_t modifiers,
        xkb_keysym_t key_symbol, struct action action)
{
    struct key_iterator_context key_iterator_context;

    key_iterator_context.state = display.keyboard_state;
    key_iterator_context.key_symbol = key_symbol;
    key_iterator_context.is_release = is_release;
    key_iterator_context.modifiers = modifiers;
    key_iterator_context.action = action;
    xkb_keymap_key_for_each(display.keymap, set_bind_iterator,
            &key_iterator_context);
}

/* Get action associated to a key binding. */
const struct action *get_key_binding(bool is_release, xkb_mod_mask_t modifiers,
        xkb_keycode_t key_code)
{
    struct binding *binding;
    struct action *actions = NULL;

    binding = get_key_binding_pointer(modifiers, key_code);
    if (binding != NULL) {
        if (is_release) {
            actions = binding->release_actions;
        } else {
            actions = binding->press_actions;
        }
    }
    return actions;
}
