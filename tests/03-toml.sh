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

for f in tests/toml/valid/*.toml ; do
    # Add it as configuration file
    cp "$f" "$temp/smoke-wm/config.toml"

    # Try to find a parsing configuration status line
    while read -r line ; do
        if [ "$line" = "$failure" ] ; then
            echo "test failed on '$f'"
            exit 1
        elif [ "$line" = "$success" ] ; then
            break
        fi
    done < <(XDG_CONFIG_HOME="$temp" "$SMOKE_WM" 2>/dev/null)
done

for f in tests/toml/invalid/*.toml ; do
    # Add it as configuration file
    cp "$f" "$temp/smoke-wm/config.toml"

    # Try to find a parsing configuration status line
    while read -r line ; do
        if [ "$line" = "$failure" ] ; then
            break
        elif [ "$line" = "$success" ] ; then
            echo "test failed on '$f'"
            exit 1
        fi
    done < <(XDG_CONFIG_HOME="$temp" "$SMOKE_WM" 2>/dev/null)
done

