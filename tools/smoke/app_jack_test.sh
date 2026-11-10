#!/bin/sh
# Plays a module from the real application through JACK and checks that
# sound arrives: a private JACK server with the dummy driver, the tracker
# started with -play (it picks JACK because the server runs), and a meter
# client listening to the tracker's output ports.
#   app_jack_test.sh <openmpt> <module> <jack_meter>
set -e
openmpt=$1
module=$2
meter=$3
home=$(mktemp -d)
export JACK_DEFAULT_SERVER="linopenmpt-test-$$"
cleanup()
{
	[ -n "$app" ] && kill "$app" 2>/dev/null || true
	[ -n "$server" ] && kill "$server" 2>/dev/null || true
	wait 2>/dev/null || true
	rm -rf "$home"
}
trap cleanup EXIT
jackd --no-realtime -d dummy -r 48000 -p 256 >"$home/jackd.log" 2>&1 &
server=$!
sleep 1
HOME=$home XDG_CONFIG_HOME= QT_QPA_PLATFORM=offscreen \
LINOPENMPT_SCREENSHOT=$home/main.png LINOPENMPT_SCREENSHOT_DELAY=8000 \
	timeout 60 "$openmpt" -play "$module" >"$home/openmpt.log" 2>&1 &
app=$!
"$meter" "OpenMPT" 3
wait "$app"
app=
echo "played through JACK and closed cleanly"
