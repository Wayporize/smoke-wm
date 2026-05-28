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

wait_for_line "window $xterm_id added to workspace ([0-9]+)"
xterm_workspace_id="${BASH_REMATCH[1]}"

"$TOOL" windowmove "$xterm_id" 820 0 &

wait_for_line "window $xterm_id removed from workspace 1"
wait_for_line "window $xterm_id added to workspace 2"
