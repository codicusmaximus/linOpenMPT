#!/bin/bash
# The step 4 screenshot pass: starts the real application once per entry,
# with a module open, runs one or more menu commands (by their resource.h
# names) and saves the main window and any dialog they opened.
#   tools/screenshots/pass.sh <openmpt> <out dir> [entry ...]
# An entry is "name:module:COMMAND[,COMMAND...]"; without entries, the list
# below. Runs under Xvfb at 1600x1000 when it is installed, else offscreen,
# unless QT_QPA_PLATFORM says otherwise.
# Exits with 1 when an entry crashed, hung or saved no picture (the
# app_views test).
set -u
openmpt=$1
out=$2
shift 2
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)
test=$root/upstream/test
mkdir -p "$out"
failed=0

entries=("$@")
if [ ${#entries[@]} -eq 0 ]; then
	entries=(
		"start:$test/test.mod:"
		"welcome:$test/test.mod:"
		"start-missing-samples:$test/test.mptm:"
		"general:$test/test.xm:ID_VIEW_GLOBALS"
		"patterns:$test/test.xm:ID_VIEW_PATTERNS"
		"samples:$test/test.xm:ID_VIEW_SAMPLES"
		"instruments:$test/test.xm:ID_VIEW_INSTRUMENTS"
		"comments:$test/test.xm:ID_VIEW_COMMENTS"
		"about:$test/test.mod:ID_APP_ABOUT"
		"open:$test/test.mod:ID_FILE_OPEN"
		"save-as:$test/test.mod:ID_FILE_SAVE_AS"
		"stream-export:$test/test.mod:ID_FILE_SAVEASWAVE"
		"midi-export:$test/test.mod:ID_FILE_SAVEMIDI"
		"cleanup:$test/test.xm:ID_EDIT_CLEANUP"
		"sample-trimmer:$test/test.xm:ID_EDIT_SAMPLETRIMMER"
		"find:$test/test.xm:ID_VIEW_PATTERNS,ID_EDIT_FIND"
		"goto:$test/test.xm:ID_VIEW_PATTERNS,ID_EDIT_GOTO_MENU"
		"split-keyboard:$test/test.xm:ID_VIEW_PATTERNS,ID_EDIT_SPLITKEYBOARDSETTINGS"
		"setup:$test/test.mod:ID_VIEW_OPTIONS"
		"plugin-manager:$test/test.mod:ID_PLUGIN_SETUP"
		"channel-manager:$test/test.xm:ID_CHANNEL_MANAGER"
		"clipboard-manager:$test/test.xm:ID_CLIPBOARD_MANAGER"
		"song-properties:$test/test.xm:ID_VIEW_SONGPROPERTIES"
		"zxx-macros:$test/test.xm:ID_PATTERN_MIDIMACRO"
		"midi-mapping:$test/test.xm:ID_VIEW_MIDIMAPPING"
		"edit-history:$test/test.xm:ID_VIEW_EDITHISTORY"
		"mpt-hacks:$test/test.mod:ID_VIEW_MPTHACKS"
		"song-length:$test/test.mod:ID_ESTIMATESONGLENGTH"
		"quick-start::"
		"opl-export:$test/test.xm:ID_FILE_SAVEOPL"
		"setup-soundcard:$test/test.mod:ID_VIEW_OPTIONS,CPropertySheetMPT:page1"
		"setup-mixer:$test/test.mod:ID_VIEW_OPTIONS,CPropertySheetMPT:page2"
		"setup-player:$test/test.mod:ID_VIEW_OPTIONS,CPropertySheetMPT:page3"
		"setup-sample-editor:$test/test.mod:ID_VIEW_OPTIONS,CPropertySheetMPT:page4"
		"setup-keyboard:$test/test.mod:ID_VIEW_OPTIONS,CPropertySheetMPT:page5"
		"setup-colors:$test/test.mod:ID_VIEW_OPTIONS,CPropertySheetMPT:page6"
		"setup-midi:$test/test.mod:ID_VIEW_OPTIONS,CPropertySheetMPT:page7"
		"setup-paths:$test/test.mod:ID_VIEW_OPTIONS,CPropertySheetMPT:page8"
		"setup-advanced:$test/test.mod:ID_VIEW_OPTIONS,CPropertySheetMPT:page9"
		"sample-amplify:$test/test.xm:ID_VIEW_SAMPLES,CCtrlSamples:IDC_SAMPLE_AMPLIFY"
		"sample-resample:$test/test.xm:ID_VIEW_SAMPLES,CCtrlSamples:IDC_SAMPLE_RESAMPLE"
		"sample-silence:$test/test.xm:ID_VIEW_SAMPLES,CCtrlSamples:IDC_SAMPLE_SILENCE"
		"sample-xfade:$test/test.xm:ID_VIEW_SAMPLES,CCtrlSamples:IDC_SAMPLE_XFADE"
		"sample-autotune:$test/test.xm:ID_VIEW_SAMPLES,CCtrlSamples:IDC_SAMPLE_AUTOTUNE"
		"sample-grid:$test/test.xm:ID_VIEW_SAMPLES,CViewSample:ID_SAMPLE_GRID"
		"sample-raw-import:$test/test.xm:ID_VIEW_SAMPLES,CCtrlSamples:IDC_SAMPLE_OPENRAW"
		"sample-map:$test/test.xm:ID_VIEW_INSTRUMENTS,CViewInstrument:ID_INSTRUMENT_SAMPLEMAP"
		"envelope-scale:$test/test.xm:ID_VIEW_INSTRUMENTS,CViewInstrument:ID_ENVELOPE_SCALEPOINTS"
		"pattern-properties:$test/test.xm:ID_VIEW_PATTERNS,CViewPattern:ID_PATTERN_PROPERTIES"
		"channel-settings:$test/test.xm:ID_VIEW_PATTERNS,CViewPattern:ID_CHANNEL_SETTINGS"
		"remove-channels:$test/test.xm:ID_VIEW_PATTERNS,CViewPattern:ID_PATTERN_REMOVECHANNELDIALOG"
		"pattern-amplify:$test/test.xm:ID_VIEW_PATTERNS,CViewPattern:ID_PATTERN_AMPLIFY"
		"effect-visualizer:$test/test.xm:ID_VIEW_PATTERNS,CViewPattern:ID_PATTERN_VISUALIZE_EFFECT"
		"chord-editor:$test/test.xm:ID_VIEW_PATTERNS,CCtrlPatterns:ID_PATTERN_CHORDEDIT"
		"metronome:$test/test.xm:ID_VIEW_PATTERNS,CCtrlPatterns:ID_METRONOME_SETTINGS"
		"plugin-editor:$test/test.xm:ID_VIEW_GLOBALS,CViewGlobals:IDC_BUTTON2"
		"plugin-select:$test/test.xm:ID_VIEW_GLOBALS,CViewGlobals:IDC_BUTTON1"
		"move-fx-slot:$test/test.xm:ID_VIEW_GLOBALS,CViewGlobals:IDC_MOVEFXSLOT"
		"insert-fx-slot:$test/test.xm:ID_VIEW_GLOBALS,CViewGlobals:IDC_INSERTFXSLOT"
		"plugin-save-preset:$test/test.xm:ID_VIEW_GLOBALS,CViewGlobals:IDC_BUTTON8"
	)
	# Modules with the built-in plugins, written by make_plugin_module
	# (tools/smoke); the app_views test passes their folder.
	plugins=${LINOPENMPT_PLUGIN_MODULES:-}
	if [ -n "$plugins" ] && [ -f "$plugins/plugins.mptm" ]; then
		entries+=(
			"lfo-editor:$plugins/plugins.mptm:ID_VIEW_GLOBALS,CViewGlobals:IDC_BUTTON2"
			"echo-editor:$plugins/plugins.mptm:ID_VIEW_GLOBALS,CViewGlobals:IDC_BUTTON4,CViewGlobals:IDC_BUTTON2"
			"missing-plugin:$plugins/missing-plugin.mptm:"
		)
	fi
fi

id_of()
{
	# The tracker's own ids, or MFC's standard ones.
	local id
	id=$(grep -hE "^#define $1 " "$root/upstream/mptrack/resource.h" "$root/compat/include/afxres.h" | head -1 | awk '{print $3}')
	[ -n "$id" ] && echo $((id))
}

run()
{
	# A platform chosen by the caller (the tests run offscreen) wins.
	if [ -n "${QT_QPA_PLATFORM:-}" ]; then
		"$@"
	elif command -v xvfb-run >/dev/null; then
		xvfb-run -a -s "-screen 0 1600x1000x24" "$@"
	else
		QT_QPA_PLATFORM=offscreen "$@"
	fi
}

for entry in "${entries[@]}"; do
	IFS=: read -r name module commands <<<"$entry"
	ids=""
	for c in ${commands//,/ }; do
		# Class:NAME goes to a window of that class (see app/main.cpp).
		target=""
		if [[ $c == *:* ]]; then
			target="${c%%:*}:"
			c=${c#*:}
		fi
		if [[ $c == page[0-9]* ]]; then
			ids="$ids${ids:+,}$target$c"
			continue
		fi
		id=$(id_of "$c")
		[ -z "$id" ] && { echo "$name: unknown command $c"; continue 2; }
		ids="$ids${ids:+,}$target$id"
	done
	home=$(mktemp -d)
	# Not the first run, so the welcome dialog stays away (entry "welcome"
	# shows it on purpose).
	if [ "$name" != welcome ]; then
		mkdir -p "$home/.config/OpenMPT"
		printf '[Version]\nVersion=1.32.12.00\n' >"$home/.config/OpenMPT/mptrack.ini"
	fi
	rm -f "$out/$name".png "$out/$name"-*.png
	HOME=$home XDG_CONFIG_HOME= LINOPENMPT_COMMANDS=$ids \
	LINOPENMPT_SCREENSHOT=$out/$name.png LINOPENMPT_SCREENSHOT_DELAY=$((2500 + 500 * $(echo "$ids" | tr ',' '\n' | grep -c .))) \
		run timeout 60 "$openmpt" ${module:+"$module"} >"$out/$name.log" 2>&1
	status=$?
	rm -rf "$home"
	shots=$(ls "$out/$name".png "$out/$name"-*.png 2>/dev/null | wc -l)
	echo "$name: exit $status, $shots picture(s)"
	if [ $status -ne 0 ] || [ ! -f "$out/$name.png" ]; then
		echo "$name: FAILED, see $out/$name.log"
		failed=1
	fi
done
exit $failed
