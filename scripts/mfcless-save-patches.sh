#!/bin/sh
# Save the changes made in the mfcless worktree (<build>/upstream-mfcless)
# back into patches/mfcless/. Run it after editing upstream files there, before
# CMake runs again (CMake re-applies the saved patches).
#
#   scripts/mfcless-save-patches.sh [build-dir]    (default: build-qt)
#
# Each patch owns a set of upstream folders; the Qt twins (links into qt/mptrack)
# are not part of any patch.
set -eu
here=$(cd "$(dirname "$0")/.." && pwd)
tree=${1:-$here/build-qt}/upstream-mfcless
[ -d "$tree" ] || { echo "no worktree at $tree" >&2; exit 1; }
save() {
	out=$here/patches/mfcless/$1; shift
	git -C "$tree" diff --diff-filter=M -- "$@" > "$out.new"
	if [ -s "$out.new" ]; then mv "$out.new" "$out"; else rm -f "$out.new" "$out"; fi
}
save 0001-engine-no-mfc-types.patch soundlib sounddsp common src misc tracklib unarchiver
# The document (CModDoc and its editing functions), patched rather than
# twinned since it is mostly logic.
doc="mptrack/Moddoc.cpp mptrack/Modedit.cpp mptrack/PatternClipboard.cpp mptrack/ModConvert.cpp mptrack/AppendModule.cpp mptrack/ModDocTemplate.cpp"
exclude=$(for f in $doc; do printf ' :!%s' "$f"; done)
save 0002-tracker-logic-no-mfc-types.patch mptrack $exclude
save 0003-document-no-mfc-types.patch $doc
git -C "$here" status --short patches/mfcless
