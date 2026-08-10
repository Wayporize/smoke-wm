#!/bin/sh

userresources="$HOME/.Xresources"
usermodmap="$HOME/.Xmodmap"
sysresources=/etc/X11/xinit/.Xresources
sysmodmap=/etc/X11/xinit/.Xmodmap

# merge in defaults and keymaps

if [ -f "$sysresources" ]
then
	xrdb -merge "$sysresources"
fi

if [ -f "$sysmodmap" ]
then
	xmodmap "$sysmodmap"
fi

if [ -f "$userresources" ]
then
	xrdb -merge "$userresources"
fi

if [ -f "$usermodmap" ]
then
	xmodmap "$usermodmap"
fi

if [ -d /etc/X11/xinit/xinitrc.d ]
then
	for f in /etc/X11/xinit/xinitrc.d/?*.sh
	do
		[ -x "$f" ] && . "$f"
	done
fi

set -e

temp="$(mktemp -d /tmp/configuration.XXXXXX)"
mkdir "$temp/smoke-wm"
cat >"$temp/smoke-wm/config.toml" <<EOF
[[wm.workspace]]
name = "Chat"
output = "HDMI-A-0"

[[wm.workspace]]
name = "Primary"
output = "DisplayPort-1"

[[wm.window]]
class = "XTerm"
workspace = "Chat"
border.decoration = "none"

[[wm.window]]
class = "st-256color"
workspace = "Primary"

[wm.bindings]
"Super+1" = "focus-workspace:Primary"
"Super+2" = "focus-workspace:Chat"
"Super+3" = "focus-workspace:1"
"Super+4" = "focus-workspace:2"
"Super+Control+1" = "move-workspace:Primary"
"Super+Control+2" = "move-workspace:Chat"
"Super+Control+3" = "move-workspace:1"
"Super+Control+4" = "move-workspace:2"
"Super+Shift+1" = "move:Primary"
"Super+Shift+2" = "move:Chat"
"Super+Shift+3" = "move:1"
"Super+Shift+4" = "move:2"
"Super+Return" = "run:st"
"Super+Shift+Return" = "run:xterm"
"RightButton" = "run:xterm"

# Add a NULL binding that does nothing but also blocks no normal user input
[[wm.binding]]
transparent = true
button = "LeftButton"

EOF

XDG_CONFIG_HOME="$temp" XDG_CONFIG_DIRS= ~/Coding/smoke-wm/build/smoke-wm >~/Coding/smoke-wm/log.txt
