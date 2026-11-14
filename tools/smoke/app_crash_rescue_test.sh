#!/bin/sh
# Crashes the real application (SIGSEGV) after modifying a module, offscreen
# and with a fresh configuration. Passes if the crash handler saved the
# modified module into the crash folder, as a file OpenMPT can open again,
# and said where.
#   app_crash_rescue_test.sh <openmpt> <module.xm>
set -u
openmpt=$1
module=$2
home=$(mktemp -d)
trap 'rm -rf "$home"' EXIT
mkdir -p "$home/.config/OpenMPT"
printf '[Version]\nVersion=1.32.12.00\n' >"$home/.config/OpenMPT/mptrack.ini"
# The pattern view, select all, cut (resource.h and afxres.h ids): the
# module is modified. Then the crash, before the screenshot would be taken.
HOME=$home XDG_CONFIG_HOME= XDG_DATA_HOME= DISPLAY= WAYLAND_DISPLAY= QT_QPA_PLATFORM=offscreen \
LINOPENMPT_COMMANDS=32782,CViewPattern:57642,CViewPattern:57635 \
LINOPENMPT_CRASH=3500 LINOPENMPT_SCREENSHOT=$home/main.png LINOPENMPT_SCREENSHOT_DELAY=10000 \
	timeout 60 "$openmpt" "$module" >"$home/log" 2>&1
status=$?
cat "$home/log"
if [ $status -lt 128 ] || [ $status -eq 124 ]; then
	echo "expected the program to die of a signal, exit status $status"
	exit 1
fi
rescued=$(find "$home/.local/share/OpenMPT/Crash Files" -name '1_test.xm' 2>/dev/null | head -1)
if [ -z "$rescued" ]; then
	echo "no module in the crash folder"
	exit 1
fi
if [ "$(head -c 17 "$rescued")" != "Extended Module: " ]; then
	echo "$rescued is not an XM file"
	exit 1
fi
grep -q "rescued to $home/.local/share/OpenMPT/Crash Files/" "$home/log" || { echo "the crash folder was not reported"; exit 1; }
echo "rescued: $rescued"
