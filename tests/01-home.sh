#!/bin/bash

set -e

check_home() {
    while read -r line ; do
        case "$line" in
        \[*\]\ 'user home: '*)
            if [ "${line#\[*\] user home: }" != "$2" ] ; then
                echo "home is not $2"
                return 1
            fi
            return 0
        esac
    done < <(HOME="$1" "$SMOKE_WM" 2>/dev/null)
    return 1
}

# Check a few names, they must match
for h in a b c LOL /home/what "$HOME" ; do
    check_home "$h" "$h"
done

# Check for fallback through pw entry
check_home "" "$HOME"
