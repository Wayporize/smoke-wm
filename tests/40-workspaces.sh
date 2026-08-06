#!/bin/bash

set -e

# Kill all child processes and remove all fifos at exit
at_exit() {
    pkill -P $$
    rm "$fifo"
}
trap at_exit INT EXIT

# Create temporary fifo
fifo="/tmp/$$.fifo"
mkfifo "$fifo"
exec 3<>"$fifo"

wait_for_line() {
    while read -t 4 -r line ; do
        line="${line#\[*\] }"
        if [[ "$line" =~ $1 ]] ; then
            return 0
        fi
    done < "$fifo"

    echo did not get "$1"
    return 1
}

XDG_CONFIG_HOME=/tmp XDG_CONFIG_DIRS= "$SMOKE_WM_FAKE_RANDR" >"$fifo" &

xterm &
xterm_pid="$!"
wait_for_line "window (0x[0-9a-f]+) creation registered"
xterm_id="${BASH_REMATCH[1]}"

# Get the current workspace, make sure the window is added
wait_for_line "window $xterm_id added to workspace ([0-9]+)"
xterm_workspace_id="${BASH_REMATCH[1]}"

# Move the window such that the workspace changes because of the new position
"$TOOL" windowmove "$xterm_id" 820 0 &
wait_for_line "window $xterm_id removed from workspace $xterm_workspace_id"
wait_for_line "window $xterm_id added to workspace ([0-9]+)"
other_workspace_id="${BASH_REMATCH[1]}"
if [ "$other_workspace_id" = "$xterm_workspace_id" ] ; then
    echo "workspace ids are both $xterm_workspace_id"
    exit 1
fi

# Move the window back to witness the reverse change
"$TOOL" windowmove "$xterm_id" 0 0 0 &
wait_for_line "window $xterm_id removed from workspace $other_workspace_id"
wait_for_line "window $xterm_id added to workspace $xterm_workspace_id"
