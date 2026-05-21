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

wait_for_line "focus changed to (0x[0-9a-f]+)"
window_id="${BASH_REMATCH[1]}"
if [ "$window_id" = "$root_id" ] ; then
    echo "root should not have been focused in any way"
    exit 1
fi

xterm &
xterm_pid2="$!"

wait_for_line "focus changed to (0x[0-9a-f]+)"
other_window_id="${BASH_REMATCH[1]}"
if [ "$other_window_id" = "$root_id" ] ; then
    echo "root should not have been focused in any way (2nd)"
    exit 1
fi

if [ "$other_window_id" = "$window_id" ] ; then
    echo "the other window should have been focused"
    exit 1
fi

kill "$xterm_pid2"

# the focus returns to the root
# TODO: it should return to the other window
wait_for_line "focus changed to $root_id"

xdotool windowfocus "$window_id"

wait_for_line "focus changed to $window_id"

# focus is not allowed to change away
if wait_for_line "focus changed to (0x[0-9a-f]+)" >/dev/null ; then
    echo "focus changed away"
    exit 1
fi

kill "$xterm_pid"

wait_for_line "focus changed to $root_id"
