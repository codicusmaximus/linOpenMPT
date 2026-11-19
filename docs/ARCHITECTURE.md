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
3. runs **upstream's views and dialogs too**, by implementing the Win32 API
   they are written against on Qt6 and MFC on top of that Win32 API, the way
   MFC is built on Windows. That layer is [mfc-qt](../mfc-qt/README.md), a
   library of its own that other MFC applications can use. Only what
   cannot work that way (Windows-only facilities) is rewritten in Qt.

Upstream is a git submodule in `upstream/`, pinned to a release tag. Port code
lives outside it. Changes to upstream files are a last resort and are kept as
patches in `patches/`, re-applied by `scripts/update-upstream.sh`.

## Repository layout

| Path | What |
|------|------|
| `upstream/` | OpenMPT (submodule, release tag); `patches/` applied at configure time |
| `cmake/` | Build of upstream sources: source lists, core, sound device |
| `mfc-qt/` | A copy of the [mfc-qt](https://github.com/codicusmaximus/mfc-qt) repository (`scripts/update-mfc-qt.sh`). MFC and the Win32 API on Qt6, a reusable library: Windows and MFC headers, kernel32, windows, GDI and controls on Qt widgets, MFC, the resource compiler |
| `compat/` | What OpenMPT's shared code needs beyond mfc-qt (`mpt_extras.h`, the VST bridge's Windows-side helpers) |
| `platform/` | Linux app object (`CTrackApp`) and the remaining headless UI boundary |
| `app/` | (step 3) the Qt6 application |
| `tools/smoke/` | Small programs and tests that prove each layer works |
| `scripts/` | Upstream and mfc-qt sync scripts |
| `patches/` | Upstream patches, ideally none |

## Build targets

| Target | Configuration | Status |
|--------|---------------|--------|
| `mpt_common` | tracker (`MODPLUG_TRACKER`) | builds |
| `mpt_sounddevice` | tracker; RtAudio with JACK, PulseAudio, ALSA; rtkit | builds, enumerates devices |
| `mpt_libopenmpt` | player (`LIBOPENMPT_BUILD`) | builds, renders modules; stepping stone only |
| `mpt_soundlib` | tracker | builds; loads, renders, edits and saves modules |
| `mpt_tracker_logic` | tracker | non-UI `mptrack/` files, `tracklib/`, audio export encoders; builds |
| `mpt_tracker_ui` | tracker | upstream UI files (documents, dialogs) compiled against the MFC layer; compile-only until step 3 |
| `mfcqt::mfcqt` | - | mfc-qt: `mfcqt_win32` (types, `CString`, kernel32), `mfcqt_qtwin` (Win32 UI on Qt6 Widgets), `mfcqt_mfc` (MFC on the Win32 API) |
| `linopenmpt_platform` | tracker | Linux `CTrackApp`, settings, headless UI boundary; links `mfcqt_mfc` |

The tracker and player configurations use different C++ namespaces upstream,
so both can coexist in one build.

How the compat layer works:

- Upstream's Windows precompiled header `build/pch/PCH.h` is force-included,
  as upstream's own build does, followed by `linopenmpt/mpt_extras.h`.
- `mfc-qt/include` provides `afx.h`, `afxwin.h` and friends. Win32 types keep
  their Windows sizes (`LONG` and `DWORD` are 32-bit). `CString` is wide, so
  `wchar_t` strings are UTF-32 on Linux.
- MFC UI classes are skeletons: virtual functions have empty defaults, other
  members are only declared. Any UI call that reaches a Linux binary is a link
  error, which is how the UI boundary is found and kept small.
- Message maps are real MFC-style tables, so commands and window messages
  can be routed to upstream's handlers unchanged once Qt drives them.
- Run-time class information is real as well, so document templates can
  create upstream's frames and views with `CreateObject()`.
- Win32 functions follow one rule for strings: functions that take a path
  to open or change (`CreateFile`, `CreateDirectory`, `FindFirstFile`) take
  native narrow paths, as upstream passes them `AsNative().c_str()`;
  functions that fill or take TCHAR text take wide strings, as with
  `UNICODE`. Where upstream mixes the two, `patches/0003` switches it to
  the TCHAR accessors, which is a no-op on Windows.
- Windows facilities with no Linux counterpart (registry, COM servers, shell
  links, SEH) are declared with their Windows values and report "not
  found", so upstream takes its existing fallback paths.
- Native paths are UTF-8 (`char`) on Linux. `P_()` path literals have their
  backslashes turned into `/`.
- `GetPrivateProfile*`/`WritePrivateProfile*` are reimplemented, so
  `mptrack.ini` works as on Windows. Files are written as UTF-8, and UTF-16
  files copied from a Windows install are read too.

## UI layering

```
upstream mptrack/ views and dialogs        (unchanged)
        |  MFC calls
mfc-qt/src/mfc     CWnd, message maps, CDC, CWinApp::Run   (Win32 calls only)
        |  Win32 calls
mfc-qt/src/qtwin   HWND = QWidget, WM_* from Qt events, GDI on QPainter
        |
Qt6 Widgets
```

- Every HWND is a `QWidget`. Qt's input, focus, resize and timer events are
  turned into the window messages Windows would send, in the same order, and
  go through `PreTranslateMessage`, `TranslateMessage` and the window
  procedure.
- Each window paints into a persistent backing image, as a Windows window
  keeps its pixels: `WM_PAINT` is sent only for invalidated areas, and
  `ScrollWindowEx` moves pixels. GDI follows Windows' pixel rules (lines
  exclude their end point, raster operations, DIB sections with writable
  bits), so upstream's hand-drawn views look the same at any display scale.
- Keys come with Windows virtual-key codes and scan codes, so OpenMPT's key
  bindings and keyboard note entry work with the same `.mkb` files.
- MFC is implemented as on Windows: a CBT hook attaches the `CWnd` before
  `WM_CREATE` and subclasses the window to `AfxWndProc`; messages are cracked
  through upstream's message maps; notifications are reflected to the child
  that sent them; idle time sends `WM_IDLEUPDATECMDUI` and deletes temporary
  objects. The message loop is Qt's, with MFC's `PreTranslateMessage` and
  `OnIdle` plugged into it.

## Audio

OpenMPT's own sound device layer (`src/openmpt/sounddevice`) is used as-is, so
latency, buffer and device settings behave as on Windows. On Linux it drives
the bundled RtAudio, compiled with JACK, PulseAudio and ALSA backends. JACK is
the low-latency default; on PipeWire systems it runs through `pipewire-jack`.
rtkit (via D-Bus) gives the audio thread realtime priority without root.

## Plan

1. **Repo set up to follow upstream** (done): submodule, CMake build of the
   core and sound device layer, smoke tests, sync script, CI.
2. **Tracker compiling on Linux** (done): `soundlib` and the logic half of
   `mptrack/` build and run in tracker configuration, and every UI file that
   is not Windows-only compiles against the compatibility layer. See
   [PORTING-STATUS.md](PORTING-STATUS.md) for the inventory.
3. **Qt6 app shell with low-latency audio**: the Win32 and MFC layers on Qt,
   then upstream's main window, document tabs and toolbar running on them,
   with playback through JACK. Done when any module opens and plays as in
   the Windows app.
4. **Every view and dialog ported to parity**: pattern, sample, instrument,
   order list and comments views, then every dialog, then plugins (native
   Linux plugins, and Windows VST through a bridge such as yabridge).
