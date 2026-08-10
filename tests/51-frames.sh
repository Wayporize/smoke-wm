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
[[wm.window]]
class = "st-256color"
border.decoration = "none"

[wm.bindings]
"Mod4+w" = "run:xterm"
"Mod4+a" = "run:st"
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

hit_key_and_await() {
    local tries=0
    while (( tries < 4 )) ; do
        xdotool key "$1"

        if wait_for_line "$2" >/dev/null ; then
            break
        fi

        sleep 0.1
        let tries++ || true
    done
}

XDG_CONFIG_HOME="$temp" XDG_CONFIG_DIRS= "$SMOKE_WM_FAKE_RANDR" >"$fifo" &

wait_for_line "taking over"

hit_key_and_await Super_L+w "running action RUN: xterm"

wait_for_line "window (0x[0-9a-f]+) creation registered"
window_id="${BASH_REMATCH[1]}"

wait_for_line "reparenting window $window_id into frame (0x[0-9a-f]+)"
frame_id="${BASH_REMATCH[1]}"

wait_for_line "showing window $frame_id"

wait_for_line "focus changed to $window_id"

hit_key_and_await Super_L+a "running action RUN: st"

wait_for_line "window (0x[0-9a-f]+) creation registered"
window_id="${BASH_REMATCH[1]}"

# make sure the window itself is shown and that is was not put into a frame
wait_for_line "showing window $window_id"

wait_for_line "focus changed to $window_id"
