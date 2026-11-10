#!/bin/sh
# Starts the real application with a module, offscreen and with a fresh
# configuration, saves a picture of the main window and closes it as a user
# would. Passes if the picture was saved and the program exited cleanly.
#   app_start_test.sh <openmpt> <module> [picture]
set -e
openmpt=$1
module=$2
home=$(mktemp -d)
trap 'rm -rf "$home"' EXIT
picture=${3:-$home/main.png}
HOME=$home XDG_CONFIG_HOME= QT_QPA_PLATFORM=offscreen \
LINOPENMPT_SCREENSHOT=$picture LINOPENMPT_SCREENSHOT_DELAY=4000 \
	timeout 60 "$openmpt" "$module"
test -s "$picture"
test -s "$home/.config/OpenMPT/mptrack.ini"
echo "main window saved to $picture; settings written on close"
