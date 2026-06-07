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
[[wm.workspace]]
name = "Chat"
output = "secondary"

[[wm.workspace]]
name = "Primary"
output = "primary"

[[wm.workspace]]
name = "Exist"
output = "primary"

[[wm.window]]
class = "XTerm"
workspace = "Chat"

[[wm.window]]
class = "st-256color"
workspace = "Primary"
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

# Before the configuration kicks in
wait_for_line "workspace 1 is now associated to monitor 11"
wait_for_line "workspace 2 is now associated to monitor 12"

# After the configuration kicked in
wait_for_line "workspace Chat is now associated to monitor 12"
wait_for_line "workspace Primary is now associated to monitor 11"

# Try some combinations of starting st/xterm and make sure that all open on the
# correct workspace
for i in xterm xterm st st xterm st xterm st st xterm ; do
    "$i" &
    if [ "$i" = "xterm" ] ; then
        workspace="Chat"
    else
        workspace="Primary"
    fi
    wait_for_line "window (0x[a-f0-9]+) added to workspace $workspace"
done
