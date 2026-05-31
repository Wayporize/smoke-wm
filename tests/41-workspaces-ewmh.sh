#!/bin/bash

set -e

# Kill all child processes and remove all fifos at exit
at_exit() {
    pkill -P $$
    rm "$fifo"
}
trap at_exit INT EXIT

# Create temporary fifo
fifo="/tmp/$$.fifo"
mkfifo "$fifo"
exec 3<>"$fifo"

wait_for_line() {
    while read -t 4 -r line ; do
        line="${line#\[*\] }"
        if [[ "$line" =~ $1 ]] ; then
            return 0
        fi
    done < "$fifo"

    echo did not get "$1"
    return 1
}

_net_number_of_desktops=0
_net_desktop_geometry=0
_net_desktop_viewport_zero_count=0
_net_current_desktop=0
_net_desktop_names=()

update_properties() {
    while read -r line ; do
        if [[ "$line" =~ (_NET_[A-Z_]+)(:|\ =)\ (.*) ]] ; then
            case "${BASH_REMATCH[1]}" in
            _NET_NUMBER_OF_DESKTOPS) _net_number_of_desktops="${BASH_REMATCH[3]}" ;;
            _NET_DESKTOP_GEOMETRY) _net_desktop_geometry="${BASH_REMATCH[3]}" ;;
            _NET_DESKTOP_VIEWPORT)
                numbers="${BASH_REMATCH[3]}"
                _net_desktop_viewport_zero_count=0
                while [[ "$numbers" =~ 0(,\ )? ]] ; do
                    numbers="${numbers:${#BASH_REMATCH[0]}}"
                    _net_desktop_viewport_zero_count=$((_net_desktop_viewport_zero_count + 1))
                done
                ;;
            _NET_CURRENT_DESKTOP) _net_current_desktop="${BASH_REMATCH[3]}" ;;
            _NET_DESKTOP_NAMES)
                _net_desktop_names=()
                names="${BASH_REMATCH[3]}"
                while [[ "$names" =~ \"([^\"]|\\\")*\"(,\ )? ]] ; do
                    names="${names:${#BASH_REMATCH[0]}}"
                    _net_desktop_names+=("${BASH_REMATCH[1]//\\\"/\"}")
                done
                ;;
            esac
        fi
    done < <(xprop -notype -root)
}

XDG_CONFIG_HOME=/tmp XDG_CONFIG_DIRS= "$SMOKE_WM_FAKE_RANDR" >"$fifo" &

# Wait for smoke-wm to take over
wait_for_line "taking over"

# Get all relevant properties set by smoke-wm
update_properties

# Same as `echo` but fail
false_echo() {
    echo "$@"
    return 1
}

[ $_net_number_of_desktops -eq 2 ] || false_echo "_NET_NUMBER_OF_DESKTOPS is wrong"
[ $_net_current_desktop -eq 0 ] || false_echo "_NET_CURRENT_DESKTOP is wrong"
[ $_net_desktop_viewport_zero_count -eq $((_net_number_of_desktops * 2)) ] || false_echo "_NET_DESKTOP_VIEWPORT is wrong"
[ ${#_net_desktop_names[@]} -eq 2 ] || false_echo "_NET_DESKTOP_NAMES is wrong (miscount)"
[ "${_net_desktop_names[0]}" = "1" ] && [ "${_net_desktop_names[1]}" = "2" ] || false_echo "_NET_DESKTOP_NAMES is wrong"
