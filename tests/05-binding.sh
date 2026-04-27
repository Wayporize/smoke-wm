#!/bin/bash

set -e

# Create a temporary directory and clean it up at exit
temp="$(mktemp -d /tmp/bindings.XXXXXX)"

at_exit() {
    rm -rf "$temp"
}

trap at_exit INT EXIT

mkdir "$temp/smoke-wm"

# Bindings to test
bindings=(
    "KP Shift+Control a"
    "KR Control b"
    "KP Mod4 XF86Search"
    "KP Mod1+Mod5 XF86Search"
    "KP Shift+Mod4 XF86Search"
    "KP Control+Shift+Mod4 XF86Search"
    "KP Shift+Mod4+Control XF86AudioMicMute"
    "KP Mod4 a"
    "KP Mod4 b"
    "KP Mod4 c"
    "KP Mod4 d"
    "KP Mod4 e"
    "KR Mod4 F"
    "KP Mod4 g"
    "KP Mod4 dead_caron"
    "KP Mod4 oacute"
    "KP Mod4 ae"
    "KP Control AE"
    "KP Shift+Mod4 b"
    "KP Mod3 Shift_L"
    "KR Mod2 asciitilde"
    "KP Control questiondown"
    "KP None questiondown"
    "KP Lock+Shift+Mod4 KP_Multiply"
    "BP None LeftButton"
    "BR Alt RightButton"
    "BP Shift+Super ScrollUp"
)

# Translate a modifier constant to an integer mask
modifier_to_integer() {
    case "$1" in
    [Ss]hift) integer=1 ;;
    [Ll]ock) integer=0 ;;
    [Cc]ontrol) integer=4 ;;
    [Mm]od1) integer=8 ;;
    [Mm]od2) integer=16 ;;
    [Mm]od3) integer=32 ;;
    [Mm]od4) integer=64 ;;
    [Mm]od5) integer=128 ;;
    *)
        # Use xmodmap, it might have the right key like Alt_L associated to a
        # modifier
        if modifier="$(xmodmap | grep "$1")" ; then
            # The output is for example "mod4      ...", so trim the after
            # "mod4"
            modifier_to_integer "${modifier%% *}"
            return
        else
            echo "invalid modifier: $1"
            exit 1
        fi
        ;;
    esac
    echo -n "$integer"
}

# Compute the modifiers to ignore
result="$(xmodmap -pm | grep -E 'Num_Lock|Scroll_Lock')"
ignore_modifiers_mask="$(modifier_to_integer "${result%% *}")"

# Entries with space separated entries themselves
# [i][0] := Release flag
# [i][1] := Modifiers as integer
# [i][2...] := List of matching keycodes
hard_bindings=()

# Write a configuration file and create the hard bindings
for b in "${bindings[@]}" ; do
    fields=($b)
    release="${fields[0]}"
    modifiers="${fields[1]}"
    key_symbol="${fields[2]}"
    button="${fields[2]}"

    echo '[[wm.binding]]' >> "$temp/smoke-wm/config.toml"
    if [ "${release:1}" = "R" ] ; then
        echo "release = true" >> "$temp/smoke-wm/config.toml"
    fi
    echo "modifiers = \"$modifiers\"" >> "$temp/smoke-wm/config.toml"
    if [ "${release:0:1}" = "K" ] ; then
        echo "key = \"$key_symbol\"" >> "$temp/smoke-wm/config.toml"
    else
        echo "button = \"$button\"" >> "$temp/smoke-wm/config.toml"
    fi

    old_IFS="$IFS"
    IFS='+'
    modifiers=0
    for m in ${fields[1]} ; do
        case "$m" in
        None) break ;;
        *) modifiers=$((modifiers | $(modifier_to_integer "$m"))) ;;
        esac
    done
    IFS="$old_IFS"

    modifiers=$((modifiers & ~ignore_modifiers_mask))

    if [ "${release:0:1}" = "K" ] ; then
        # This is how we interpret the xmodmap output:
        # [0] = "keycode"
        # [1] = keycode value
        # [2] = "="
        # [3...] = key symbols
        while read -r line ; do
            modmap_fields=($line)
            hard_bindings+=("$release $modifiers ${modmap_fields[1]}")
        done < <(xmodmap -pke | grep -E '\<'"$key_symbol"'\>')
    else
        case "$button" in
        L*) button=0 ;;
        M*) button=1 ;;
        R*) button=2 ;;
        [WS]*U*) button=3 ;;
        [WS]*D*) button=4 ;;
        [WS]*L*) button=5 ;;
        [WS]*R*) button=6 ;;
        esac
        hard_bindings+=("$release $modifiers $button")
    fi
done

# Capture input of the smoke-wm program
{
# Get to the first line of the binding dump
while read -r line ; do
    line="${line#\[*\] }"
    if [ "$line" = "start of dumping bindings" ] ; then
        break
    fi
done

# Go through all lines of the bindings dump
while read -r line ; do
    line="${line#\[*\] }"
    # Check for end signal
    if [ "$line" = "end of dumping bindings" ] ; then
        break
    fi

    fields=($line)

    # Try to find a match for the line in hard_bindings
    match=""
    for b in "${hard_bindings[@]}" ; do
        keycodes="${b#* * }"
        # Compare modifiers and release flag
        if [ "${fields[0]} ${fields[1]} " != "${b%$keycodes}" ] ; then
            continue
        fi

        # Compare keycodes/buttons
        is_k_match=false
        for k in $keycodes ; do
            if [ ${fields[2]} -eq $k ] ; then
                is_k_match=true
                break
            fi
        done

        if $is_k_match ; then
            match="$b"
            break
        fi
    done

    # If the match was left empty, we found none
    if [ -z "$match" ] ; then
        echo "mismatch for $line"
        exit 1
    fi

    # Remove the match from hard_bindings
    new_hard_bindings=()
    for b in "${hard_bindings[@]}" ; do
        if [ "$b" = "$match" ] ; then
            continue
        fi
        new_hard_bindings+=("$b")
    done
    hard_bindings=("${new_hard_bindings[@]}")
done
} < <(XDG_CONFIG_HOME="$temp" "$SMOKE_WM" 2>/dev/null)

# If there is a bug in the test or not enough bindings printed
if ! [ "${#hard_bindings[@]}" -eq 0 ] ; then
    echo "got left over hard bindings:"
    for b in "${hard_bindings[@]}" ; do
        echo "$b"
    done
    exit 1
fi
