#!/bin/bash

set -e

# Kill all child processes and remove all fifos at exit
at_exit() {
    pkill -P $$
    rm "$fifo1" "$fifo2" "$fifo3"
}
trap at_exit INT EXIT

# Create three temporary fifos
fifo1="/tmp/$$.fifo1"
mkfifo "$fifo1"
exec 3<>"$fifo1"

fifo2="/tmp/$$.fifo2"
mkfifo "$fifo2"
exec 4<>"$fifo2"

fifo3="/tmp/$$.fifo3"
mkfifo "$fifo3"
exec 5<>"$fifo3"

wait_for_line() {
    while read -t 4 -r line ; do
        line="${line#\[*\] }"
        echo "$line"
        if [[ "$line" =~ $2 ]] ; then
            return 0
        fi
    done < "$1"

    echo "did not get '$2' for $1"
    return 1
}

# Start smoke-wm and wait for it to take over
XDG_CONFIG_HOME=/tmp XDG_CONFIG_DIRS= "$SMOKE_WM" >"$fifo1" &
smoke_wm_pid="$!"
wait_for_line "$fifo1" "taking over"

# First the self test: The window will focus itself after getting WM_TAKE_FOCUS

# Start the wm_take_focus window and wait until it gets the client message
"$WM_TAKE_FOCUS" self >"$fifo2" &
# Get the window id
wait_for_line "$fifo2" "id (0x[0-9a-f]+)"
window_id="${BASH_REMATCH[1]}"
wait_for_line "$fifo2" "got WM_TAKE_FOCUS"

# Wait for it to have been registered as focused window
wait_for_line "$fifo1" "focus changed to $window_id"

# Second the child test: The window will focus a child after getting WM_TAKE_FOCUS

# Start the wm_take_focus window and wait until it gets the client message
"$WM_TAKE_FOCUS" child >"$fifo2" &
# Get the window id
wait_for_line "$fifo2" "id (0x[0-9a-f]+)"
window_id="${BASH_REMATCH[1]}"
wait_for_line "$fifo2" "got WM_TAKE_FOCUS"

# Wait for it to have been registered as focused window
wait_for_line "$fifo1" "focus changed to $window_id"
