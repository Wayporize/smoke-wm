#!/bin/bash

set -e

# Kill all child processes and remove all fifos at exit
at_exit() {
    pkill -P $$
    rm "$fifo1"
    rm "$fifo2"
}
trap at_exit INT EXIT

# Create two temporary fifos
fifo1="/tmp/$$.fifo1"
mkfifo "$fifo1"
exec 3<>"$fifo1"

fifo2="/tmp/$$.fifo2"
mkfifo "$fifo2"
exec 4<>"$fifo2"

wait_for_line() {
    while read -t 4 -r line ; do
        line="${line#\[*\] }"
        if [ "$line" = "$2" ] ; then
            return 0
        fi
    done < "$1"

    echo did not get "$2" for "$1"
    return 1
}

XDG_CONFIG_HOME=/tmp XDG_CONFIG_DIRS= "$SMOKE_WM" >"$fifo1" &
smoke_wm_pid="$!"

# Wait for the first to take over
wait_for_line "$fifo1" "taking over"

# Replace the window manager with the same window manager
XDG_CONFIG_HOME=/tmp XDG_CONFIG_DIRS= "$SMOKE_WM" >"$fifo2" &
other_smoke_wm_pid="$!"

# Wait for the second to take over
wait_for_line "$fifo2" "taking over"

# Wait for the first to go dormant
wait_for_line "$fifo1" "going dormant"

# Kill the second one
kill "$other_smoke_wm_pid"

# Wait for the first one to take over again
wait_for_line "$fifo1" "taking over"
