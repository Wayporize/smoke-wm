#!/bin/bash

set -e

# Create two temporary fifos
fifo1="/tmp/$$.fifo1"
mkfifo "$fifo1"
exec 3<>"$fifo1"
at_exit() {
    rm "$fifo1"
}
trap at_exit INT EXIT

fifo2="/tmp/$$.fifo2"
mkfifo "$fifo2"
exec 4<>"$fifo2"
at_exit() {
    rm "$fifo1" "$fifo2"
}

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
at_exit() {
    rm "$fifo1" "$fifo2"
    kill "$smoke_wm_pid"
}

# Wait for the first to take over
wait_for_line "$fifo1" "taking over"

# Replace the window manager with the same window manager
XDG_CONFIG_HOME=/tmp XDG_CONFIG_DIRS= "$SMOKE_WM" >"$fifo2" &
other_smoke_wm_pid="$!"
at_exit() {
    rm "$fifo1" "$fifo2"
    kill "$smoke_wm_pid"
    kill "$other_smoke_wm_pid"
}

# Wait for the first to go dormant
wait_for_line "$fifo1" "going dormant"

# Replace the window manager again but with i3
i3 --replace >/dev/null 2>/dev/null &
i3_pid="$!"
at_exit() {
    rm "$fifo1" "$fifo2"
    kill "$smoke_wm_pid"
    kill "$other_smoke_wm_pid"
    kill "$i3_pid"
}

# Wait for the second to go dormant
wait_for_line "$fifo2" "going dormant"

# Kill i3 again and see who takes over, it should be random who takes over
kill "$i3_pid"
at_exit() {
    rm "$fifo1" "$fifo2"
    kill "$smoke_wm_pid"
    kill "$other_smoke_wm_pid"
}

# Wait for either manager to take over
if wait_for_line "$fifo1" "taking over" ; then
    n=1
elif wait_for_line "$fifo2" "taking over" ; then
    n=2
else
    exit 1
fi

# Kill the manager that took over
if [ $n -eq 1 ] ; then
    kill "$smoke_wm_pid"
    at_exit() {
        rm "$fifo1" "$fifo2"
        kill "$other_smoke_wm_pid"
    }
    fifo="$fifo2"
else
    kill "$other_smoke_wm_pid"
    at_exit() {
        rm "$fifo1" "$fifo2"
        kill "$smoke_wm_pid"
    }
    fifo="$fifo1"
fi

# Wait for the not killed manager to take over
wait_for_line "$fifo" "taking over"
