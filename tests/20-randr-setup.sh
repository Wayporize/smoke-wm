#!/bin/bash

set -e

# Read the outputs from `xrandr` 
outputs=()
{
# Screen 0: ...
read -r screen_line

while read -r line ; do
    if [[ "$line" =~ ([^ ]+)\ ((dis)?connected)\ ((primary) )?([0-9]+x[0-9]+\+[0-9]+\+[0-9]+)? ]] ; then
        string="${BASH_REMATCH[1]} ${BASH_REMATCH[2]} ${BASH_REMATCH[6]}"
        # TODO: 1 is picked for rotation for now
        string+=" 1"
        if [ "${BASH_REMATCH[5]}" = "primary" ] ; then
            string+=" primary"
        fi
        outputs+=("$string")
    fi
done
} < <(xrandr -q 2>/dev/null)

# Capture input of the smoke-wm program
{
# Wait for start of the dump
while read -t 4 -r line ; do
    line="${line#\[*\] }"
    if [ "$line" = "start of dumping monitor setup" ] ; then
        break
    fi
done

if [ "$line" = "" ] ; then
    echo "timeout occured waiting for start of monitor setup dump"
    exit 1
fi

# Get the primary output
read -r line
if [[ "$line" =~ primary\ ([0-9]+) ]] ; then
    primary="${BASH_REMATCH[1]}"
else
    echo "$line"
    echo "invalid primary line (first line of dump)"
    exit 1
fi

# First all crtcs are printed, we can use this when the outputs are printed
declare -A crtcs

# Go through all lines of the monitor setup dump
while read -t 4 -r line ; do
    line="${line#\[*\] }"
    # Check for end signal
    if [ "$line" = "end of dumping monitor setup" ] ; then
        break
    fi

    if [[ "$line" =~ output\ ([0-9]+):\ ([^ ]+)\ ([0-9]+)\ ([^ ]+) ]] ; then
        line="${BASH_REMATCH[2]} ${BASH_REMATCH[4]}"
        crtc="${crtcs["${BASH_REMATCH[3]}"]}"
        if [ -n "$crtc" ] ; then
            line+=" $crtc"
        fi
        if [ "${BASH_REMATCH[1]}" = "$primary" ] ; then
            line+=" primary"
        fi
        # Check for the output in the output list and exclude it from it
        new_outputs=()
        for o in "${outputs[@]}" ; do
            if [ "$o" != "$line" ] ; then
                new_outputs+=("$o")
            fi
        done
        outputs=("${new_outputs[@]}")
    elif [[ "$line" =~ monitor\ ([0-9]+):\ (.*) ]] ; then
        line="${BASH_REMATCH[2]}"
        crtcs["${BASH_REMATCH[1]}"]="${BASH_REMATCH[2]}"
    fi
done

if [ "$line" = "" ] ; then
    echo "timeout occured waiting for end/more of monitor setup dump"
    exit 1
fi
} < <("$SMOKE_WM")

if [ "${#outputs[@]}" -gt 0 ] ; then
    echo "got left over outputs:"
    for o in "${outputs[@]}" ; do
        echo "$o"
    done
    exit 1
fi

exit 0
