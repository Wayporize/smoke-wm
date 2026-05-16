#ifndef WORKSPACE_H
#define WORKSPACE_H

/* Workspace/desktop management. */

#include <utility/list.h>

#include <xcb/randr.h>
#include <xcb/xproto.h>

/* a unique workspace ID */
typedef uint32_t workspace_t;

struct workspace {
    /* unique identifier of the workspace */
    workspace_t id;
    /* the crtc this workspace is on */
    xcb_randr_crtc_t crtc;
};

/* Focus the workspace identified by @id. */
void focus_workspace(workspace_t id);

#endif
