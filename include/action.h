#ifndef ACTION_H
#define ACTION_H

enum action_type {
    ACTION_NULL
};

union action_value {
    int value;
};

struct action {
    enum action_type type;
    union action_value value;
};

#endif
