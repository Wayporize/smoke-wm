#!/bin/bash

set -e

# Start i3 with two terminals
i3 >/dev/null 2>/dev/null &

# start two terminals
"$TERMINAL" 2>/dev/null &
"$TERMINAL" 2>/dev/null &

# wait for i3 and terminals to start
sleep 1

# try to take over and try to read a map request
while read -t 2 -r line ; do
    line="${line#\[*\] }"
    if [ "$line" = "taking over" ] ; then
        "$TERMINAL" 2>/dev/null &
    fi

    if [[ "$line" =~ got\ map\ request:\ 0x[0-9a-f]+ ]] ; then
        break
    fi
done < <(XDG_CONFIG_HOME=/tmp XDG_CONFIG_DIRS= "$SMOKE_WM" 2>/dev/null)

# kill all terminals we started
pkill -P $$
