#!/bin/sh

set -e

original_display="$DISPLAY"
test_display="8"

# Let called scripts pick up the new DISPLAY value
export DISPLAY=":$test_display"
export SMOKE_WM="./build/smoke-wm"

make "$SMOKE_WM"

# Install exit handler
xephyr_pid=""
at_exit() {
    set +e
    if [ -n "$xephyr_pid" ] ; then
        kill -s INT "$xephyr_pid" 2>/dev/null
    fi
}
trap at_exit INT EXIT

# Run all tests
for f in ./tests/[0-9][0-9]*.sh ; do
    # Run test X server for each test
    DISPLAY="$original_display" Xephyr ":$test_display" 2>/dev/null &
    xephyr_pid="$!"

    # Wait until the X server has started
    while [ ! -S "/tmp/.X11-unix/X$test_display" ] ; do
        sleep 0.05
    done

    name="$(basename "$f" .sh)"
    if "$f" ; then
        echo "$name succeeded"
    else
        echo "$name failed"
        exit 1
    fi

    kill -s INT "$xephyr_pid"

    # Wait until the X server has stopped
    while [ -S "/tmp/.X11-unix/X$test_display" ] ; do
        sleep 0.05
    done
done
