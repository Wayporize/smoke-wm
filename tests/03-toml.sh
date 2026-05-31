#!/bin/bash

set -e

# Create a temporary directory and clean it up at exit
temp="$(mktemp -d /tmp/bindings.XXXXXX)"

success="parsing configuration succeeded"
failure="parsing configuration failed"

at_exit() {
    rm -rf "$temp"
}

trap at_exit INT EXIT

mkdir "$temp/smoke-wm"

run_config_test() {
    # Try to find a parsing configuration status line
    while read -r line ; do
        line="${line#\[*\] }"
        if [ "$line" = "$1" ] ; then
            echo "test failed on '$f'"
            return 1
        elif [ "$line" = "$2" ] ; then
            return 0
        fi
    done < <(XDG_CONFIG_HOME="$temp" "$SMOKE_WM")
    return 1
}

for f in tests/toml/valid/*.toml ; do
    # Add it as configuration file
    cp "$f" "$temp/smoke-wm/config.toml"
    run_config_test "$failure" "$success"
done

for f in tests/toml/invalid/*.toml ; do
    # Add it as configuration file
    cp "$f" "$temp/smoke-wm/config.toml"

    run_config_test "$success" "$failure"
done

