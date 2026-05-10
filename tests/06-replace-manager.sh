#!/bin/bash

set -e

# Kill all child processes at exit
at_exit() {
    pkill -P $$
}
trap at_exit INT EXIT

# Start i3 with two terminals
i3 >/dev/null 2>/dev/null &

# start two terminals
"$TERMINAL" 2>/dev/null &
"$TERMINAL" 2>/dev/null &

# wait for i3 and terminals to start
sleep 1

# try to take over and try to read a map request
read_map_request() {
    while read -t 2 -r line ; do
        line="${line#\[*\] }"
        if [ "$line" = "taking over" ] ; then
            "$TERMINAL" 2>/dev/null &
        fi

        if [[ "$line" =~ got\ map\ request\ for\ 0x[0-9a-f]+ ]] ; then
            return 0
        fi
    done < <(XDG_CONFIG_HOME=/tmp XDG_CONFIG_DIRS= "$SMOKE_WM" 2>/dev/null)

    # got timeout
    return 1
}

read_map_request
