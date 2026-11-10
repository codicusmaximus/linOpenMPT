# Patches to upstream

The goal is to keep this folder empty. Port code lives outside `upstream/`, and
upstream files are only changed when there is no other way.

If a patch is unavoidable:

1. Make the change inside `upstream/`, then save it with
   `git -C upstream diff > patches/NNNN-short-name.patch`.
2. Add a line to the table below explaining why it is needed.
3. Prefer changes that upstream could accept (for example, a missing
   `#if MPT_OS_WINDOWS` guard), and consider sending them upstream so the patch
   can be dropped.

CMake applies every patch here to `upstream/` when it configures (skipping any
that is already applied), and stops if one no longer applies. This leaves the
submodule's working tree modified, which is expected; `git status` shows it.

| Patch | Why |
|-------|-----|
| `0001-settings-wchar-size-assert-windows-only.patch` | `Settings.cpp` asserts `sizeof(wchar_t) == 2` in a function only used to convert old ANSI ini files on Windows. `wchar_t` is 4 bytes on Linux. The assert is now Windows-only. |
| `0002-trackersettings-autosave-path-fromwide.patch` | `TrackerSettings.cpp` builds the default autosave path from a `TCHAR` buffer with `FromNative()`. That only works where native strings are wide (Windows). `FromWide()` is what the code means on every platform. |
| `0003-tracker-ui-portability.patch` | Tracker UI files (21 of them) that assume native paths are wide, which is only true on Windows. Calls such as `SetWindowText(path.AsNative().c_str())` or `FromNative(tcharBuffer)` become `ToCString()`, `FromCString()`, `ToWide()` or `FromWide()`, which are identical on Windows. It also fixes a typo (`m_PulseAudioSimple`) in `Mptrack.cpp` that only shows when PulseAudio is enabled, passes `.c_str()` to `std::filesystem::create_directories` in `AutoSaver.cpp` (native strings use custom traits on Linux), and wraps two Windows-only spots in `#if MPT_OS_WINDOWS`: the About dialog's Wine tab and `SoundDevice::AppInfo::SetHWND`, which upstream's sound device layer only declares on Windows. All of it could go upstream. |
| `0004-sounddevice-prefer-running-jack-on-linux.patch` | Upstream ranks PulseAudio first on Linux and JACK second. The port's audio target is JACK, for the lowest latency, so when a JACK server is running (JACK lists devices only then, and PipeWire always provides one) its devices rank first and the desktop sound servers come next. Without a JACK server nothing changes. A user's chosen device in the settings still wins. |
