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

XDG_CONFIG_HOME=/tmp XDG_CONFIG_DIRS= "$SMOKE_WM" >"$fifo" &

# Wait for take over
wait_for_line "taking over"

"$TERMINAL" &
xterm_pid="$!"

# Now we expect Create -> Map -> Destroy
wait_for_line "window (0x[a-f0-9]+) creation registered"
window_id="${BASH_REMATCH[1]}"

wait_for_line "got map request for $window_id"

kill "$xterm_pid"

wait_for_line "window $window_id destruction registered"
