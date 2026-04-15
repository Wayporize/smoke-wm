#ifndef ACTION_H
#define ACTION_H

/* Actions are an interface for users to interact with internal functionality of
 * the window manager.
 *
 * TODO: Add the actual actions
 */

/* type of the action, this implies the action value type */
enum action_type {
    ACTION_NULL
};

/* value of the action if it has a data type */
union action_value {
    int value;
};

/* wrapper around an action type/value */
struct action {
    /* action type */
    enum action_type type;
    /* action data (data type implied through `type`) */
    union action_value value;
};

#endif
