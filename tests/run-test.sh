#!/bin/sh

set -e

original_display="$DISPLAY"
test_display="8"

# Let called scripts pick up the new DISPLAY value
export DISPLAY=":$test_display"
export SMOKE_WM="./build/smoke-wm"

make "$SMOKE_WM"

DISPLAY="$original_display" Xephyr ":$test_display" 2>/dev/null &
xephyr_pid="$!"

# Install exit handler
at_exit() {
    set +e
    kill -s INT "$xephyr_pid" 2>/dev/null
}
trap at_exit INT EXIT

# Wait until the X server has started
while [ ! -S "/tmp/.X11-unix/X$test_display" ] ; do
    sleep 0.05
done

name="$(basename "$1" .sh)"
if "$1" ; then
    echo "$name succeeded"
else
    echo "$name failed"
fi
