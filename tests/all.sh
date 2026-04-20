#!/bin/sh

set -e

test_display="8"

# Run test X server and kill it at script exit
Xephyr ":$test_display" 2>/dev/null &
xephyr_pid="$!"

# Wait until the X server has started
while [ ! -S "/tmp/.X11-unix/X$test_display" ] ; do
    sleep 0.1
done

# Install exit handler
at_exit() {
    kill -9 "$xephyr_pid"
}
trap at_exit INT EXIT

# Let called scripts pick up the new DISPLAY value
export DISPLAY=":$test_display"

# Run all tests
for f in usage home toml configuration configuration-path binding replace-manager ; do
    "./tests/$f.sh" && echo "$f tests succeeded"
done
