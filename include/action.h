#ifndef ACTION_H
#define ACTION_H

/* Actions are an interface for users to interact with internal functionality of
 * the window manager.
 *
 * TODO: Add the actual actions
 */

#include <utility/types.h>

#define DECLARE_ALL_ACTIONS \
    X(NONE, VOID) \
    X(FOCUS, STRING) \
    X(CLOSE, VOID) \
    X(RUN, STRING) \
    X(WORKSPACE, STRING)

/* type of the action, this implies the action value type */
enum action_type {
    ACTION_NULL,
#define X(name, type) ACTION_##name,
    DECLARE_ALL_ACTIONS
#undef X
};

/* value of the action if it has a data type */
union action_value {
    int integer;
    utf8_t *string;
};

enum action_data_type {
    ACTION_DATA_NULL,
    ACTION_DATA_VOID,
    ACTION_DATA_INTEGER,
    ACTION_DATA_STRING
};

/* wrapper around an action type/value */
struct action {
    /* action type */
    enum action_type type;
    /* action data (data type implied through `type`) */
    union action_value value;
};

/* Get the action type corresponding to given string. */
enum action_type convert_string_to_action_type(const char *string);

/* Get a string representation of an action type. */
const char *get_string_of_action_type(enum action_type type);

/* Get the data type a specific action requires. */
enum action_data_type get_data_type_of_action_type(enum action_type type);

/* Print the action value to stdout.
 *
 * @prefix is printed before a non-void value.
 */
void print_action_value(enum action_type type, union action_value value, const char *prefix);

/* Execute a user action. */
void execute_action(const struct action *action);

#endif
