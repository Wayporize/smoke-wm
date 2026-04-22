#!/bin/bash

set -e

# Create a temporary directory and clean it up at exit
temp="$(mktemp -d /tmp/configuration.XXXXXX)"

at_exit() {
    rm -rf "$temp"
}

trap at_exit INT EXIT

mkdir "$temp/smoke-wm"

# Go through all valid toml files
for f in tests/toml/valid/*.toml ; do
    # Add it as configuration file
    cp "$f" "$temp/smoke-wm/config.toml"
    # Get the corresponding expected output
    output="$(dirname "$f")/$(basename "$f" .toml).output"

    # Capture input of the smoke-wm program
    {
    # Get to the first line of the configuration dump
    while read -r line ; do
        if [ "$line" = "start of dumping configuration" ] ; then
            break
        fi
    done

    # Get the output we actually receive
    result="$(mktemp "$temp/output.XXXXXX")"

    # Go through all lines of the configuration dump
    while read -r line ; do
        # Check for end signal
        if [ "$line" = "end of dumping configuration" ] ; then
            break
        fi

        echo "$line" >> "$result"
    done

    } < <(XDG_CONFIG_HOME="$temp" "$SMOKE_WM" 2>/dev/null)

    # Compare the actually received with the expected output
    if ! cmp -s "$result" "$output" ; then
        echo "compare failed on $f"
        diff "$result" "$output"
        exit 1
    fi
done
