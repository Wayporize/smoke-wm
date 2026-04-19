#!/bin/bash

# TODO: make this test actually usable.
# It seems like xdotool itself triggers a 'mapping changed' event.  And when the
# sleep duration is too small, the events change.  Very confusing and makes this
# test not usable in an automated fashion.

set -e

shopt -s extglob

run="./build/smoke-wm"

make "$run"

#./tests/check-xkb-events "$run" "mapping changed" "mapping changed" "layout changed" &
"$run" &

{
    read -r old_rules
    read -r old_model
    read -r old_layout
    old_layout="${old_layout##*:+( )}"
    read -r old_variant
    old_variant="${old_variant##*:+( )}"
    read -r old_options
    old_options="${old_options##*:+( )}"
} < <(setxkbmap -query)

at_exit() {
    setxkbmap -layout "$old_layout" -variant "$old_variant" -option "$old_options"
    kill %1
}

trap at_exit INT EXIT

setxkbmap -layout de,us,gr

sleep 1.5

setxkbmap -layout de,us -option "grp:alt_shift_toggle"

sleep 1.5

xdotool key Shift+Alt_L

kill %1

echo fre
