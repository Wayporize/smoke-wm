#ifndef MONITOR_H
#define MONITOR_H

/* Screen/Monitor management using the XRandr extension (not Xinerama because
 * it is old and dumb).
 */

#include "workspace.h"

struct monitor {
    /* xrandr identifier of the monitor */
    char *name;
    /* identifiers of all workspaces on this monitor */
    LIST(unsigned, workspaces);
};

#endif
