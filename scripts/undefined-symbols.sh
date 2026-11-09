#!/bin/sh
# Links upstream's UI objects (mpt_tracker_ui, built in build-tracker/)
# against the libraries we have, and lists the symbols still missing: the
# measure of how much of the Windows API and MFC is left to write.
#   scripts/undefined-symbols.sh [build-dir]   (default: build-tracker)
set -e
dir=${1:-build-tracker}
out=$(mktemp -d)
trap 'rm -rf "$out"' EXIT
objs=$(find "$dir/CMakeFiles/mpt_tracker_ui.dir" -name '*.o' | sort)
libs="$dir/platform/liblinopenmpt_platform.a $dir/platform/mfc/liblinopenmpt_mfc.a \
$dir/platform/resources/liblinopenmpt_resources.a $dir/platform/qtwin/liblinopenmpt_qtwin.a \
$dir/compat/liblinopenmpt_compat.a $dir/libmpt_tracker_logic.a $dir/libmpt_soundlib.a \
$dir/libmpt_sounddevice.a $dir/libmpt_rtaudio.a $dir/libmpt_common.a"
pkgs="Qt6Widgets Qt6Gui Qt6Core dbus-1"
for p in jack libpulse-simple libpulse alsa vorbisfile vorbis ogg flac libmpg123 zlib portaudio-2.0 sndfile opus opusfile; do
	pkg-config --exists "$p" 2>/dev/null && pkgs="$pkgs $p"
done
c++ -o "$out/a.out" $objs -Wl,--start-group $libs -Wl,--end-group \
	-Wl,--allow-multiple-definition $(pkg-config --libs $pkgs) -lpthread -ldl 2> "$out/log" || true
grep -o "undefined reference to \`[^']*'" "$out/log" | sed "s/undefined reference to \`//; s/'$//" | sort -u
