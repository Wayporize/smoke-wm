#ifndef WORKSPACE_H
#define WORKSPACE_H

/* Workspace/desktop management. */

#include <utility/list.h>

#include <xcb/xproto.h>

struct workspace {
    /* unique identifier of the workspace */
    unsigned id;
    /* list of all windows associated to this workspace */
    LIST(xcb_window_t, windows);
};

/* Focus the workspace identified by @id. */
void focus_workspace(unsigned id);

#undef
