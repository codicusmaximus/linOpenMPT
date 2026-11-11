#!/bin/bash
# A native Linux VST2 plugin's own editor, embedded in the tracker's editor
# window: opens DISTRHO's MVerb editor under Xvfb (through the screenshot
# pass) and checks that the plugin drew into it. The picture of the editor
# window then has the plugin's thousands of colours; the dialog alone has a
# few hundred at most.
#   app_vst_editor_test.sh <openmpt> <vst.mptm> <out dir>
# Exits with 77 (skipped) without Xvfb or the plugin.
set -u
here=$(cd "$(dirname "$0")" && pwd)
openmpt=$1
module=$2
out=$3
vst=${LINOPENMPT_VST_DIR:-/usr/lib/vst}
if ! command -v xvfb-run >/dev/null || [ ! -f "$vst/MVerb-vst.so" ]; then
	echo "needs xvfb-run and $vst/MVerb-vst.so (dpf-plugins-vst), skipped"
	exit 77
fi
# Without QT_QPA_PLATFORM the pass runs under Xvfb, and the tracker on X11.
env -u QT_QPA_PLATFORM "$here/../screenshots/pass.sh" "$openmpt" "$out" \
	"vst-editor:$module:ID_VIEW_GLOBALS,CViewGlobals:IDC_BUTTON2" || exit 1
colours=$(python3 -I "$here/png_colours.py" "$out/vst-editor-1.png") || exit 1
echo "editor window: $colours colours"
if [ "$colours" -lt 1000 ]; then
	echo "the plugin did not draw its editor"
	exit 1
fi
