#!/bin/bash

set -e

run="./build/smoke-wm"

display=:8

make "$run"

Xephyr "$display" 2>/dev/null &

# wait for x server to start
sleep 1

DISPLAY="$display" i3 >/dev/null 2>/dev/null &

# wait for i3 to start
sleep 1

# start two terminals
DISPLAY="$display" "$TERMINAL" 2>/dev/null &
DISPLAY="$display" "$TERMINAL" 2>/dev/null &

# wait for terminals to start
sleep 1

# try to take over
{
    while read -r line ; do
        if [[ "$line" =~  got\ map\ request:\ 0x[0-9a-f]+ ]] ; then
            pkill Xephyr || true
            break
        fi
    done < <(DISPLAY="$display" "$run")
} &

# Give it some time
sleep 1

set +e
# This should issue a map request
DISPLAY="$display" timeout 5s "$TERMINAL" 2>/dev/null
if [ $? -eq 124 ] ; then
    # timed out
    # kill all sub processes
    pkill -P $$
    exit 1
fi

# Xephyr was destroyed which destroyed the terminal above, meaning we got the
# a request
exit 0
