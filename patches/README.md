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
