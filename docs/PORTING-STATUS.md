# Porting status

Upstream: OpenMPT-1.32.12.00

## Step 1: build infrastructure (done)

- `mpt_common`, `mpt_sounddevice`, `mpt_libopenmpt` build on Ubuntu 24.04
  (GCC 13, CMake 3.28, Qt 6.4) with no upstream changes.
- `render_module` loads and renders `test.mod`, `test.s3m`, `test.xm`,
  `test.mptm` from upstream's test folder. (`test.xm` and `test.mptm` are
  silent by design; upstream's `openmpt123` renders them as silence too.)
- `sounddev_list` enumerates devices through OpenMPT's sound device layer.

## Step 2: tracker engine on Linux (done)

The engine half of the tracker now builds from upstream sources in its real
tracker configuration (`MODPLUG_TRACKER`, `MPT_WITH_MFC`, `UNICODE`), the same
way `OpenMPT.exe` is built. Upstream code is used unchanged apart from two
one-line patches (see `patches/README.md`).

What builds:

- `mpt_soundlib`: all of `soundlib/`, `sounddsp/`, `soundlib/plugins/` (built-in
  plugins and OpenMPT's own DMO effect emulations), `unarchiver/`.
- `mpt_tracker_logic`: the non-UI parts of `mptrack/` (`Settings.cpp`,
  `TrackerSettings.cpp`, `MIDIMapping.cpp`, `Undo.cpp`,
  `StreamEncoderSettings.cpp`, `VstPresets.cpp`) and `tracklib/` (sample
  editing, time stretch and pitch shift).
- `linopenmpt_platform`: a Linux `CTrackApp` that loads and saves
  `mptrack.ini` in `$XDG_CONFIG_HOME/linOpenMPT` (or `$LINOPENMPT_CONFIG_DIR`),
  creates the settings, plugin cache and sound device manager.

What is proven by tests (`tracker_engine_mod`, `tracker_engine_s3m`): start the
app object, load a module, render it (output matches libopenmpt), convert it
to IT, edit a pattern cell, save it with OpenMPT's own IT writer, load it back
and check the cell. The saved file opens in openmpt123 and identifies as
written by OpenMPT 1.32.

Turned off on Linux for now:

- `MPT_WITH_VST`: hosting of VST plugins (step 4, native Linux VSTs and a
  bridge for Windows ones).
- `MPT_WITH_DMO`: hosting of Windows DirectX Media Object plugins. OpenMPT's
  built-in emulations of the DMO effects still work.
- MIDI input reports no devices until RtMidi is wired in.

The headless UI boundary (`platform/UiBoundary.cpp`) is the list of UI symbols
the engine reaches. Each one becomes a Qt implementation in step 3:
`CMainFrame::GetMainFrame`, `SetHelpText`, `Reporting::*` (messages to
stderr), `FileDialog::Show`, `HighDPISupport::GetWindowPlacement`,
`PatternClipboard`, the plugin editor classes (`CAbstractVstEditor`,
`CDefaultVstEditor`, `LFOPluginEditor`), `DialogBase`, `ResizableDialog`,
`MidiInOut::Create`, `GetAsyncKeyState`, `PostMessage`.

`platform/ModDocHooks.cpp` still supplies the handful of `CModDoc` members the
engine test calls, because the real `Moddoc.cpp` (now compiling, see below)
cannot be linked until the MFC layer has an implementation.
`ExceptionHandler.cpp` (needs Windows dbghelp) gets a Linux replacement later.

### Step 2b and 2c: the rest of mptrack/ against the MFC layer (done)

The goal is that every upstream UI file compiles unchanged, so step 3 only
has to implement MFC and Win32 on top of Qt, not rewrite OpenMPT's UI.

- MFC message maps are real (`compat/include/linopenmpt/mfc_msgmap.h`):
  `BEGIN_MESSAGE_MAP`, `ON_COMMAND`, `ON_UPDATE_COMMAND_UI`, control
  notifications, `ON_NOTIFY`, `ON_MESSAGE` and the `ON_WM_*` handlers build
  the same tables as MFC, and `CCmdTarget::OnCmdMsg` routes commands through
  them (tested by `msgmap_test`).
- MFC run-time classes are real too (`RUNTIME_CLASS`, `DECLARE_DYNCREATE`,
  `IMPLEMENT_DYNAMIC`, `CreateObject`, `IsKindOf`), since document templates
  create frames and views through them and the tracker compares view class
  names.
- The Win32 and MFC UI surface is declared in
  `compat/include/linopenmpt/`: `win32_user.h` (windows, messages, menus,
  keyboard, mouse, cursors, hooks), `win32_gdi.h` (GDI), `win32_commctrl.h`
  (common controls), `win32_shell.h` (COM, shell, time, file search) and
  `mfc_ui.h` (the MFC classes). Smaller headers stand in for `<afxole.h>`
  (drag and drop), `<afxdatarecovery.h>`, `<gdiplus.h>` (the GDI+ subset the
  tracker loads PNG/SVG images with), `<atlbase.h>`, `<uxtheme.h>`,
  `<HtmlHelp.h>`, `<mmsystem.h>` (MIDI input) and others.
- Header-only parts are implemented (geometry classes, collections, dynamic
  layout resources, CTime). Everything that needs a window system is
  declared only, so step 3 gets a complete list of what to implement from
  the linker.
- `mpt_tracker_ui` compiles 92 upstream UI files: all of them except the
  Windows-only files below. It is not linked into anything yet.
- One upstream patch,
  [`0003-tracker-ui-portability.patch`](../patches/0003-tracker-ui-portability.patch),
  touches 21 files. Almost all of it replaces `AsNative()`/`FromNative()`
  with the TCHAR accessors (`ToCString()`, `FromCString()`, `ToWide()`)
  where upstream assumes native paths are wide. Those are the same thing on
  Windows. See [patches/README.md](../patches/README.md).

Left out because they only make sense on Windows, and what replaces each:

| File | What it does | Replacement |
|------|--------------|-------------|
| `ExceptionHandler.cpp` | Crash dumps with dbghelp | Signal handler that saves open documents and writes a backtrace (step 3) |
| `FileDialog.cpp` | Win32 common file dialogs | `FileDialog`/`BrowseForFolder` (same header) on `QFileDialog`, with sample preview (step 3) |
| `HTTP.cpp` | WinInet | HTTP client for the update check (step 4) |
| `UpdateCheck.cpp` | Update check, verified with Windows CNG | Same checks with a portable crypto library, or distribution packages (step 4) |
| `MPTrackWine.cpp`, `MPTrackUtilWine.cpp` | Integration for running under Wine | Not needed natively; `GetWine()` returns nothing |
| `MPTrackLink.cpp`, `WineSoundDeviceStub.cpp` | MSVC linker directives, Wine sound bridge | Not needed |

Known gaps to handle in later steps:

- The tree view's file browser lists drive letters. Linux has none, so it
  needs mount points and the home folder instead (step 4).
- Windows shell links (`.lnk`) are not resolved; symbolic links work as
  normal paths.
- Registry lookups (the DirectMusic GM bank) find nothing.

## Step 3: Qt6 shell

### Step 3a: Win32 and MFC window core on Qt (done)

- `platform/qtwin`: windows and window classes, sent and posted messages,
  timers, the message loop with idle time and modal loops, painting with
  invalid regions and a persistent backing image, the whole GDI used by
  upstream (DCs, pens, brushes, fonts, bitmaps, DIB sections, blits and
  raster ops, text, regions, clipping, `DrawEdge`/`DrawFrameControl`),
  keyboard with Windows virtual keys and scan codes, mouse and capture,
  window scroll bars, cursors and system icons, system metrics, CBT hooks.
- `platform/mfc`: `CWnd` (handle maps, creation through the CBT hook,
  subclassing, message cracking, command and notification routing,
  reflection, every Win32 wrapper), `CDC` and the GDI object classes,
  `CPaintDC`/`CClientDC`/`CWindowDC`, `CWinApp`/`CWinThread` with `Run`,
  `PreTranslateMessage`, `OnIdle` and wait cursors.
- Tests: `qtwin_test` (44 checks) and `mfc_test` (20 checks) run offscreen and
  pass at display scales 1, 1.25, 1.5 and 2.
- Linking upstream's UI files now leaves 430 undefined symbols, down from 853:
  common controls (tree, list, combo, edit, toolbar, tab, spin, slider and
  so on), menus, frame and document/view classes, the clipboard, resources
  from `mptrack.rc`, MIDI input, and a few shell and COM calls. These are
  the next parts of step 3.

### Next in step 3

1. Resources: dialogs, menus, accelerators, bitmaps, icons and strings from
   `mptrack.rc`, compiled into the binary.
2. Menus and common controls on Qt widgets, dialogs (`DoModal`,
   `IsDialogMessage`).
3. Frames and document/view (`CMDIFrameWnd`, `CView`, `CDocTemplate`), then
   upstream's `CMainFrame` and `CTrackApp::InitInstance` running for real.
4. Playback through JACK from the main window.

## Step 4: views and dialogs

Not started. The checklist of every view and dialog will live here.
