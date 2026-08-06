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
export SMOKE_WM_FAKE_RANDR="./build/fake-randr/smoke-wm"
export OVERRIDE_REDIRECT="./build/tests/windows/override_redirect"
export WM_TAKE_FOCUS="./build/tests/windows/wm_take_focus"
export TOOL="./build/tests/tool"

# Speed up compiling by making all at once
make "$SMOKE_WM" &
make "CFLAGS=-Itests/fake-randr" "SOURCES=tests/fake-randr/randr.c" "BUILD_PREFIX=$(dirname $SMOKE_WM_FAKE_RANDR)" "$SMOKE_WM_FAKE_RANDR" &
make -f tests/GNUmakefile &
for p in $(jobs -p) ; do
    wait "$p"
done

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
    tests="tests/[0-9][0-9]*.sh"
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
