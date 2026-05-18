#!/bin/bash

set -e

# Kill all child processes and remove all fifos at exit
at_exit() {
    pkill -P $$
    rm "$fifo"
}
trap at_exit INT EXIT

RUN=./build/tests/windows/override_redirect

mkdir -p build/tests/windows
cc tests/windows/override_redirect.c -o "$RUN" -lX11

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
wait_for_line "taking over"

"$RUN" &
run_pid="$!"

wait_for_line "window (0x[0-9a-f]+) creation registered"
window_id="${BASH_REMATCH[1]}"

kill "$run_pid"

wait_for_line "window $window_id destruction registered"
