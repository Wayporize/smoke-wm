#!/bin/bash

set -e

temp=
# Kill all child processes and remove all fifos at exit
at_exit() {
    pkill -P $$
    rm "$fifo"
    rm -rf "$temp"
}
trap at_exit INT EXIT

# Create temporary fifo
fifo="/tmp/$$.fifo"
mkfifo "$fifo"
exec 3<>"$fifo"

temp="$(mktemp -d /tmp/configuration.XXXXXX)"
mkdir "$temp/smoke-wm"
cat >"$temp/smoke-wm/config.toml" <<EOF
[wm.bindings]
"Mod4+r" = "run:xterm"
EOF

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

XDG_CONFIG_HOME="$temp" XDG_CONFIG_DIRS= "$SMOKE_WM_FAKE_RANDR" >"$fifo" &

wait_for_line "taking over"

xdotool key Super_L+r

wait_for_line "running action RUN: xterm"
