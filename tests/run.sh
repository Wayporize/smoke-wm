#!/bin/sh

set -e

# This script can be called with a single argument, indicating which tests to
# run (space separated), otherwise all tests run.

original_display="$DISPLAY"
test_display="8"

# Let called scripts pick up the new DISPLAY value
export DISPLAY=":$test_display"
# Give called scripts some programs to run
export SMOKE_WM="./build/smoke-wm"
export OVERRIDE_REDIRECT="./build/tests/windows/override_redirect"
export WM_TAKE_FOCUS="./build/tests/windows/wm_take_focus"

make "$SMOKE_WM"
make -f tests/windows/GNUmakefile

# Install exit handler
xephyr_pid=""
at_exit() {
    set +e
    if [ -n "$xephyr_pid" ] ; then
        kill -s INT "$xephyr_pid" 2>/dev/null
    fi
}
trap at_exit INT EXIT

if [ "$#" -gt 0 ] ; then
    tests="$*"
else
    tests=tests/[0-9][0-9]*.sh
fi

# Run all tests
for f in $tests ; do
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
