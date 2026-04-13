#!/bin/bash

set -e

run=./build/tests/bindings/run

make -f tests/GNUmakefile "$run"

# Create a temporary directory and clean it up at exit
temp="$(mktemp -d /tmp/bindings.XXXXXX)"

at_exit() {
    rm -rf "$temp"
}

trap at_exit EXIT

mkdir "$temp/smoke-wm"

# Bindings to test
bindings=(
    "P Shift+Control a"
    "R Control b"
    "P Mod4 XF86Search"
    "P Mod1+Mod5 XF86Search"
    "P Shift+Mod4 XF86Search"
    "P Control+Shift+Mod4 XF86Search"
    "P Shift+Mod4+Control XF86AudioMicMute"
    "P Mod4 a"
    "P Mod4 b"
    "P Mod4 c"
    "P Mod4 d"
    "P Mod4 e"
    "R Mod4 F"
    "P Mod4 g"
    "P Mod4 dead_caron"
    "P Mod4 oacute"
    "P Mod4 ae"
    "P Control AE"
    "P Shift+Mod4 b"
    "P Mod3 Shift_L"
    "R Mod2 asciitilde"
    "P Control questiondown"
    "P None questiondown"
    "P Lock+Shift+Mod4 KP_Multiply"
)

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

    echo '[[wm.binding]]' >> "$temp/smoke-wm/config.toml"
    if [ "$release" = "R" ] ; then
        echo "release = true" >> "$temp/smoke-wm/config.toml"
    fi
    echo "modifiers = \"$modifiers\"" >> "$temp/smoke-wm/config.toml"
    echo "key = \"$key_symbol\"" >> "$temp/smoke-wm/config.toml"

    old_IFS="$IFS"
    IFS='+'
    modifiers=0
    for m in ${fields[1]} ; do
        case $m in
        None) break ;;
        Shift) integer=0 ;;
        Lock) continue ;;
        Control) integer=2 ;;
        Mod1) integer=3 ;;
        Mod2) integer=4 ;;
        Mod3) integer=5 ;;
        Mod4) integer=6 ;;
        Mod5) integer=7 ;;
        esac
        modifiers=$((modifiers | (1 << integer)))
    done
    IFS="$old_IFS"

    # This is how we interpret the xmodmap output:
    # [0] = "keycode"
    # [1] = keycode value
    # [2] = "="
    # [3...] = key symbols
    while read -r line ; do
        modmap_fields=($line)
        hard_bindings+=("$release $modifiers ${modmap_fields[1]}")
    done < <(xmodmap -pke | grep -E '\<'"$key_symbol"'\>')
done

# Go through all lines of the bindings dump
while read -r line ; do
    fields=($line)

    # Try to find a match for the line in hard_bindings
    match=""
    for b in "${hard_bindings[@]}" ; do
        keycodes="${b#* * }"
        # Compare modifiers and release flag
        if [ "${fields[0]} ${fields[1]} " != "${b%$keycodes}" ] ; then
            continue
        fi

        # Compare keycodes
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

    hard_bindings=("${hard_bindings[@]}")
done < <(XDG_CONFIG_HOME="$temp" "$run")

# If there is a bug in the test or not enough bindings printed
if [ "${#hard_bindings[@]}" -eq 0 ] ; then
    echo "got left over hard bindings"
    exit 1
fi
