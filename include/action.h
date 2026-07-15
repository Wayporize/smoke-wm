#ifndef ACTION_H
#define ACTION_H

/* Actions are an interface for users to interact with internal functionality of
 * the window manager.
 *
 * TODO: Add the actual actions
 */

#define DECLARE_ALL_ACTIONS \
    X(NONE, VOID) \
    X(RUN, STRING)

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
    char *string;
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

enum action_type convert_string_to_action_type(const char *string);

/* Get the data type a specific action requires. */
enum action_data_type get_data_type_of_action_type(enum action_type type);

/* Execute a user action. */
void execute_action(const struct action *action);

#endif
