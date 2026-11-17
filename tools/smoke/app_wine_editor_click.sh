#!/bin/bash
# For app_wine_editor_test.sh, inside Xvfb: opens the Windows test plugin's
# editor and clicks it with xdotool, at 192 of its 256 pixels (gain 0.75).
# The tracker's log (wine-click.log) says what reached it.
#   app_wine_editor_click.sh <openmpt> <wine.mptm> <BridgeTestGain.dll> <out dir>
set -u
openmpt=$1
module=$2
dll=$3
out=$4
home=$(mktemp -d)
trap 'rm -rf "$home"' EXIT
mkdir -p "$home/.config/OpenMPT"
printf '[Version]\nVersion=1.32.12.00\n[VST Plugins]\nNumPlugins=1\nPlugin0=%s\n' "$dll" >"$home/.config/OpenMPT/mptrack.ini"
# The general view (ID_VIEW_GLOBALS), then its editor button (IDC_BUTTON2).
HOME=$home XDG_CONFIG_HOME= WINEPREFIX=${WINEPREFIX:-$HOME/.wine} LINOPENMPT_BRIDGE_TRACE=1 \
LINOPENMPT_COMMANDS=32781,CViewGlobals:1002 \
LINOPENMPT_SCREENSHOT=$out/wine-click.png LINOPENMPT_SCREENSHOT_DELAY=12000 \
	timeout 60 "$openmpt" "$module" >"$out/wine-click.log" 2>&1 &
editor=""
for _ in $(seq 60); do
	editor=$(xdotool search --name "Plugin editor" 2>/dev/null | head -1)
	[ -n "$editor" ] && break
	sleep 0.25
done
if [ -n "$editor" ]; then
	sleep 1
	xdotool mousemove --window "$editor" 192 60 click 1
fi
wait
