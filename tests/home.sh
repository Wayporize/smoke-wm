#!/bin/bash

run=./build/smoke-wm

make "$run" || exit

check_home() {
    while read -r line ; do
        case "$line" in
        'user home: '*)
            if [ "${line#user home: }" != "$2" ] ; then
                echo "home is not $2"
                exit 1
            fi
            break
        esac
    done < <(HOME="$1" "$run")
}

# Check a few names, they must match
for h in a b c LOL /home/what "$HOME" ; do
    check_home "$h" "$h"
done

# Check for fallback through pw entry
check_home "" "$HOME"
