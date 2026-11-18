#!/bin/bash
# A Windows VST2 plugin's editor, through the Wine bridge, embedded in the
# tracker's editor window: opens the bridge's test plugin (which paints a
# colour gradient) under Xvfb, through the screenshot pass, and checks that
# its window shows up inside the tracker's. The picture of the editor window
# then has the plugin's thousands of colours; the dialog alone has a few
# hundred at most.
#   app_wine_editor_test.sh <openmpt> <wine.mptm> <BridgeTestGain.dll> <out dir>
# Exits with 77 (skipped) without Xvfb or Wine, or for a 32-bit plugin
# (BridgeTestGain32.dll) without Wine's 32-bit part.
set -u
here=$(cd "$(dirname "$0")" && pwd)
openmpt=$1
module=$2
dll=$(realpath "$3")
out=$4
if ! command -v xvfb-run >/dev/null || ! command -v wine >/dev/null; then
	echo "needs xvfb-run and wine, skipped"
	exit 77
fi
case "$dll" in
*32.dll)
	# The 32-bit bridge, run with no arguments, prints its usage and exits
	# with 2, if Wine runs 32-bit programs.
	WINEDEBUG=-all wine "$(dirname "$(realpath "$openmpt")")/../vstbridge/linopenmpt-vstbridge32.exe" >/dev/null 2>&1
	if [ $? != 2 ]; then
		echo "Wine's 32-bit part is not installed, skipped"
		exit 77
	fi
	;;
esac
# Wine's server for the test's Wine folder, and its desktop and services,
# may still run from an earlier test or the check above, on an X display
# now gone (or none): end them, so that this test's Wine starts on its own
# Xvfb.
stop_wine() { wineserver -k 2>/dev/null; wineserver -w 2>/dev/null; }
stop_wine
# Without QT_QPA_PLATFORM the pass runs under Xvfb, and the tracker on X11.
env -u QT_QPA_PLATFORM LINOPENMPT_VSTBRIDGE_TEST_DLL="$dll" "$here/../screenshots/pass.sh" "$openmpt" "$out" \
	"wine-editor:$module:ID_VIEW_GLOBALS,CViewGlobals:IDC_BUTTON2" || exit 1
colours=$(python3 -I "$here/png_colours.py" "$out/wine-editor-1.png") || exit 1
echo "editor window: $colours colours"
if [ "$colours" -lt 1000 ]; then
	echo "the plugin's editor is not in the window"
	exit 1
fi

# A click in the editor: the plugin reports it as a parameter change, which
# must reach the tracker, and at the right place (Wine translates mouse
# positions with where it believes its window is). Needs xdotool.
if ! command -v xdotool >/dev/null; then
	echo "no xdotool, click not checked"
	exit 0
fi
stop_wine
env -u QT_QPA_PLATFORM xvfb-run -a -s "-screen 0 1600x1000x24" "$here/app_wine_editor_click.sh" "$openmpt" "$module" "$dll" "$out"
if ! grep -q "bridge notify opcode 0 index 0 opt 0.75" "$out/wine-click.log"; then
	echo "the click did not reach the tracker as gain 0.75:"
	grep "bridge notify" "$out/wine-click.log"
	exit 1
fi
echo "a click in the editor reached the tracker"
