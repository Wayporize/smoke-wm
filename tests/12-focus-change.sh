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

# Get the root window
{
    # The first line is empty
    read -r line
    # Read the root window id
    read -r line
    if [[ "$line" =~ xwininfo:\ Window\ id:\ (0x[0-9a-f]+) ]] ; then
        root_id="${BASH_REMATCH[1]}"
    else
        echo "wininfo did not work"
        exit 1
    fi
} < <(xwininfo -root)

XDG_CONFIG_HOME=/tmp XDG_CONFIG_DIRS= "$SMOKE_WM" >"$fifo" &
wait_for_line "taking over"

xterm &
xterm_pid="$!"
wait_for_line "window (0x[0-9a-f]+) creation registered"
window_id="${BASH_REMATCH[1]}"
wait_for_line "focus changed to $window_id"

xterm &
xterm_pid2="$!"
wait_for_line "window (0x[0-9a-f]+) creation registered"
window_id2="${BASH_REMATCH[1]}"
wait_for_line "focus changed to $window_id2"

kill "$xterm_pid2"

# the focus returns to the other window again
wait_for_line "focus changed to $window_id"

# focus is not allowed to change away
if wait_for_line "focus changed to (0x[0-9a-f]+)" >/dev/null ; then
    echo "focus changed away"
    exit 1
fi

kill "$xterm_pid"

wait_for_line "focus changed to $root_id"
