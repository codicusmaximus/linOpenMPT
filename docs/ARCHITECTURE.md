# Architecture and plan

## Approach

OpenMPT's Windows build is two layers:

- **The core** (`common/`, `soundlib/`, `sounddsp/`, `misc/`, `src/`): file
  formats, playback engine, built-in effects, sound device layer. Portable C++,
  already used on Linux by libopenmpt and by OpenMPT's own Wine support.
- **The tracker application** (`mptrack/`, about 110 `.cpp` files and 124k
  lines): documents, undo, settings, plugin hosting, and the whole UI, written
  directly against MFC and Win32 (79 dialogs in `mptrack.rc`).

MFC can't be compiled on Linux, so a literal lift-and-shift is impossible. The
port therefore:

1. reuses the core **unmodified**, built in the tracker configuration
   (`MODPLUG_TRACKER`) so editing, saving and plugin features stay in;
2. reuses the **non-UI half of `mptrack/`** (documents, undo, clipboard,
   settings, key bindings, plugin management), compiled against a thin
   compatibility layer for the MFC/Win32 types it touches (`CString`, `DWORD`,
   settings and file APIs);
3. rewrites **only the views and dialogs** in Qt6, one by one, against the
   same document model the Windows UI uses.

Upstream is a git submodule in `upstream/`, pinned to a release tag. Port code
lives outside it. Changes to upstream files are a last resort and are kept as
patches in `patches/`, re-applied by `scripts/update-upstream.sh`.

## Repository layout

| Path | What |
|------|------|
| `upstream/` | OpenMPT (submodule, release tag); `patches/` applied at configure time |
| `cmake/` | Build of upstream sources: source lists, core, sound device |
| `compat/` | MFC/Win32 compatibility layer: types, `CString`, MFC class skeletons, INI files, POSIX paths |
| `platform/` | Linux app object (`CTrackApp`) and the headless UI boundary that Qt replaces in step 3 |
| `app/` | (step 3) the Qt6 application |
| `tools/smoke/` | Small programs and tests that prove each layer works |
| `scripts/` | Upstream sync script |
| `patches/` | Upstream patches, ideally none |

## Build targets

| Target | Configuration | Status |
|--------|---------------|--------|
| `mpt_common` | tracker (`MODPLUG_TRACKER`) | builds |
| `mpt_sounddevice` | tracker; RtAudio with JACK, PulseAudio, ALSA; rtkit | builds, enumerates devices |
| `mpt_libopenmpt` | player (`LIBOPENMPT_BUILD`) | builds, renders modules; stepping stone only |
| `mpt_soundlib` | tracker | builds; loads, renders, edits and saves modules |
| `mpt_tracker_logic` | tracker | non-UI `mptrack/` files and `tracklib/`; builds |
| `linopenmpt_compat` | - | MFC/Win32 compat layer |
| `linopenmpt_platform` | tracker | Linux `CTrackApp`, settings, headless UI boundary |

The tracker and player configurations use different C++ namespaces upstream,
so both can coexist in one build.

How the compat layer works:

- Upstream's Windows precompiled header `build/pch/PCH.h` is force-included,
  as upstream's own build does, followed by `linopenmpt/mpt_extras.h`.
- `compat/include` provides `afx.h`, `afxwin.h` and friends. Win32 types keep
  their Windows sizes (`LONG` and `DWORD` are 32-bit). `CString` is wide, so
  `wchar_t` strings are UTF-32 on Linux.
- MFC UI classes are skeletons: virtual functions have empty defaults, other
  members are only declared. Any UI call that reaches a Linux binary is a link
  error, which is how the UI boundary is found and kept small.
- Native paths are UTF-8 (`char`) on Linux. `P_()` path literals have their
  backslashes turned into `/`.
- `GetPrivateProfile*`/`WritePrivateProfile*` are reimplemented, so
  `mptrack.ini` works as on Windows. Files are written as UTF-8, and UTF-16
  files copied from a Windows install are read too.

## Audio

OpenMPT's own sound device layer (`src/openmpt/sounddevice`) is used as-is, so
latency, buffer and device settings behave as on Windows. On Linux it drives
the bundled RtAudio, compiled with JACK, PulseAudio and ALSA backends. JACK is
the low-latency default; on PipeWire systems it runs through `pipewire-jack`.
rtkit (via D-Bus) gives the audio thread realtime priority without root.

## Plan

1. **Repo set up to follow upstream** (done): submodule, CMake build of the
   core and sound device layer, smoke tests, sync script, CI.
2. **Non-UI half of the tracker compiling on Linux**: build `soundlib` in
   tracker configuration, then the logic half of `mptrack/`, against
   `compat/`. See [PORTING-STATUS.md](PORTING-STATUS.md) for the inventory.
3. **Qt6 app shell with low-latency audio**: main window, document tabs,
   toolbar, playback through JACK. Done when any module opens and plays as in
   the Windows app.
4. **Every view and dialog ported to parity**: pattern, sample, instrument,
   order list and comments views, then every dialog, then plugins (native
   Linux plugins, and Windows VST through a bridge such as yabridge).
