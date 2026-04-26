#!/bin/bash

set -e

# Create temporary fifo
fifo="/tmp/$$.fifo"
mkfifo "$fifo"
exec 3<>"$fifo"
at_exit() {
    rm "$fifo"
}
trap at_exit INT EXIT

XDG_CONFIG_HOME=/tmp XDG_CONFIG_DIRS= "$SMOKE_WM" >"$fifo" &
smoke_wm_pid="$!"
at_exit() {
    rm "$fifo"
    kill -s INT "$smoke_pid"
}

wait_for_line() {
    while read -t 4 -r line ; do
        if [ "$line" = "$1" ] ; then
            return 0
        fi
    done < "$fifo"

    echo did not get "$1"
    return 1
}

# Wait for smoke-wm to start
wait_for_line "taking over"

# Replace the window manager with i3
i3 --replace >/dev/null 2>/dev/null &
i3_pid="$!"
at_exit() {
    rm "$fifo"
    kill "$i3_pid" 2>/dev/null || true
    kill "$smoke_wm_pid"
}

# Wait until we become dormant
wait_for_line "going dormant"

# Kill i3 and then see if we take over again
kill "$i3_pid"

wait_for_line "taking over"
