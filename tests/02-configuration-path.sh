#!/bin/bash

set -e

check_configuration_path() {
    while read -r line ; do
        case "$line" in
        \[*\]\ 'configuration path: '*)
            if [ "${line#\[*\] configuration path: }" != "$3" ] ; then
                echo "configuration path is not $3"
                echo XDG_CONFIG_HOME="$1" XDG_CONFIG_DIRS="$2"
                return 1
            fi
            break
        esac
    done < <(XDG_CONFIG_HOME="$1" XDG_CONFIG_DIRS="$2" "$SMOKE_WM" 2>/dev/null)
}

# Collect all files/directories we create and delete them at exit
created_directories=()
created_files=()

remove_creations() {
    for f in "${created_files[@]}" ; do
        rm "$f"
    done

    for d in "${created_directories[@]}" ; do
        rmdir "$d"
    done
}

trap remove_creations EXIT

# Create a normal configuration file
if ! [ -d ~/.config ] ; then
    mkdir ~/.config
    created_directories=(~/.config "${created_directories[@]}")
fi

if ! [ -d ~/.config/smoke-wm ] ; then
    mkdir ~/.config/smoke-wm
    created_directories=(~/.config/smoke-wm "${created_directories[@]}")
fi

if ! [ -f ~/.config/smoke-wm/config/toml ] ; then
    touch ~/.config/smoke-wm/config.toml
    created_files+=(~/.config/smoke-wm/config.toml)
fi

# Check XDG_CONFIG_HOME
check_configuration_path "$HOME/.config" "" "$HOME/.config/smoke-wm/config.toml"
# Check the fallback $HOME/.config
check_configuration_path "" "" "$HOME/.config/smoke-wm/config.toml"
# Check XDG_CONFIG_DIRS
check_configuration_path "" "$HOME:$HOME/.config" "$HOME/.config/smoke-wm/config.toml"

# Create /tmp/test.XXXXXX/smoke-wm/config.toml
temp="$(mktemp -d /tmp/test.XXXXXX)"
created_directories=("$temp" "${created_directories[@]}")
mkdir "$temp/smoke-wm"
created_directories=("$temp/smoke-wm" "${created_directories[@]}")
touch "$temp/smoke-wm/config.toml"
created_files+=("$temp/smoke-wm/config.toml")

# Check if XDG_CONFIG_HOME is used and that it takes priority over XDG_CONFIG_DIRS
check_configuration_path "$temp" "$HOME/.config" "$temp/smoke-wm/config.toml"
# Check if invalid directories are ignored and XDG_CONFIG_DIRS is correctly interpreted
check_configuration_path "/tmp" "$HOME:$temp:$HOME/.config" "$temp/smoke-wm/config.toml"
