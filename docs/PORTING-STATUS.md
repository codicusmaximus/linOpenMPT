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
| `ExceptionHandler.cpp` | Crash dumps with dbghelp | `platform/ExceptionHandler.cpp`: fatal signals and unhandled exceptions are reported with a backtrace and what the tracker was doing (step 3g). Saving open documents on a crash is still to do. |
| `FileDialog.cpp` | Win32 common file dialogs | `app/FileDialog.cpp`: `FileDialog`/`BrowseForFolder` (same header) on `QFileDialog`, with sample preview (step 3g) |
| `HTTP.cpp` | WinInet | HTTP client for the update check (step 4) |
| `UpdateCheck.cpp` | Update check, verified with Windows CNG | Switched off with upstream's own `MPT_ENABLE_UPDATE` for now. Same checks with a portable crypto library, or left to distribution packages (step 4) |
| `MPTrackWine.cpp`, `MPTrackUtilWine.cpp` | Integration for running under Wine | `app/WineIntegration.cpp`: the Wine wrapper component exists but is never available, as on Windows proper |
| `MPTrackLink.cpp` | MSVC linker directives | Not needed |

`WineSoundDeviceStub.cpp` is compiled unchanged: with the Wine wrapper never
available it lists no devices.

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

### Step 3b: resources, the non-client area and menus (done)

- `tools/rc`: upstream's `mptrack.rc` is compiled unchanged into a Win32
  `.res` file (byte for byte what llvm-rc writes, where llvm-rc can read the
  script) and linked into the executable. `FindResource`, `LoadResource`,
  `LoadString`, `LoadBitmap`, `LoadImage`, `LoadIcon` and `LoadCursor` read
  it, and so do MFC's loaders (`CString::LoadString`, `CBitmap::LoadBitmap`,
  `CWinApp::LoadIcon`/`LoadCursor`). Icons pick the image for the display
  scale.
- The non-client area works as on Windows: `WM_NCCALCSIZE` decides the
  client area (so MFC's control bars can reserve borders), `WM_NCPAINT`
  draws borders, client edges, child captions and the scroll bar corner,
  and `WM_NCHITTEST` reports what the mouse is over, with `WM_NC*` mouse
  messages outside the client area. Client coordinates, client DCs and
  child positions start at the client origin; `GetWindowDC` covers the
  whole window. Top-level frames and captions stay the window manager's.
- Menus: `HMENU` with the whole Win32 menu API (loaded from `MENU` and
  `MENUEX` resources), menu bars on `QMenuBar` in the non-client area,
  popup menus on `QMenu` with `TrackPopupMenu(Ex)` and `TPM_RETURNCMD`,
  `WM_INITMENU`/`WM_INITMENUPOPUP` before a menu shows (so MFC's command UI
  updates it), `WM_MENUSELECT`, `WM_COMMAND`, the menu loop messages, and
  Alt+letter, Alt and F10 through `SC_KEYMENU`. `CMenu` and the menu
  functions of `CWnd` wrap it.
- Keys reach windows before Qt shortcuts, as accelerators do on Windows.
- Tests: `resource_test` (24 checks) and `nonclient_test` (41 checks) pass at
  display scales 1, 1.5 and 2, with the earlier tests.
- Linking upstream's UI files now leaves 399 undefined symbols, down from
  430. Menus and resources are no longer among them. What is left: common
  controls (tree, list, list box, toolbar, edit, combo, spin, tab, slider),
  frames, control bars and document/view, GDI+ (used for PNGs), the
  clipboard, dialogs and a few shell and COM calls.

### Step 3c: dialogs and the standard controls (done)

- Win32 dialogs from upstream's templates: `CreateDialog(Indirect)Param`,
  `DialogBox(Indirect)Param`, dialog units and fonts, the `DS_*` styles,
  `IsDialogMessage` (Tab and arrow navigation, mnemonics, default and cancel
  buttons, `DM_GETDEFID`/`DM_SETDEFID`, `WM_GETDLGCODE`) and `EndDialog`.
  All 79 templates in `mptrack.rc` create, with all 863 of their standard
  controls.
- The standard controls as Win32 window procedures, drawn with the Qt style:
  buttons (push, check, radio, group box, owner draw), statics, edits
  (single and multi-line, on Qt's text widgets), list boxes (sorted, multiple
  selection, owner draw) and combo boxes (simple, drop-down and drop-down
  list, with the list in a popup). Messages, notifications, `WM_CTLCOLOR*`
  and the owner-draw messages follow Windows.
- System colours come from the Qt palette.
- MFC's dialog classes: `CDialog` (modal and modeless, `RunModalLoop`,
  `WM_KICKIDLE`, `PreTranslateMessage`), `CDataExchange` with the `DDX_*` and
  `DDV_*` functions, the control wrappers (`CButton`, `CEdit`, `CListBox`,
  `CComboBox`, `CStatic`, `CScrollBar`), `CCheckListBox`, `RT_DLGINIT` data,
  and `MessageBox`/`AfxMessageBox` on the desktop's message box.
- Tests: `dialog_test` passes at display scales 1, 1.5 and 2, with the
  earlier tests.
- Linking upstream's UI files now leaves 318 undefined symbols, down from
  399. Dialogs and the standard controls are no longer among them.

### Step 3d: the common controls, part 1 (done)

- Win32 window procedures drawn with the Qt style, following comctl32 (and
  Wine's comctl32 where the documentation is quiet):
  - trackbars (`msctls_trackbar32`): ranges, ticks, page and line sizes,
    keys, wheel, dragging, buddies, `TBS_TOOLTIPS` and custom draw;
  - up-down controls (`msctls_updown32`): buddies (`UDS_AUTOBUDDY`,
    `UDS_SETBUDDYINT`, alignment), accelerators and `UDN_DELTAPOS`;
  - progress bars (`msctls_progress32`), with the marquee;
  - image lists (`ImageList_*`), with 32-bit alpha, masks and overlays;
  - tab controls (`SysTabControl32`): single and multiple rows, the scroll
    arrows, owner draw, `TCM_ADJUSTRECT`;
  - header controls (`SysHeader32`): ordering, divider dragging, sort arrows,
    owner draw;
  - list views (`SysListView32`) in report view, the only view upstream
    uses: groups, check boxes, owner data (`LVS_OWNERDATA`), sorting, label
    editing, incremental search and custom draw. The icon, small icon and
    list views fall back to one column;
  - tree views (`SysTreeView32`): lines and buttons, images and state
    images, check boxes, bold and cut items, callbacks, single expand,
    sorting, label editing, drag notifications, the insert mark and custom
    draw.
- MFC: `CSliderCtrl`, `CSpinButtonCtrl`, `CProgressCtrl`, `CImageList`,
  `CTabCtrl`, `CHeaderCtrl`, `CListCtrl`, `CMFCListCtrl` (cell colours and
  sorting by a header click) and `CTreeCtrl`.
- Tests: `commctrl_test` covers the controls' messages, notifications and
  mouse and key input; it and the earlier tests pass at display scales 1,
  1.5 and 2. `dialog_test` now creates every control in `mptrack.rc` except
  the toolbars, `ComboBoxEx32` and `SysLink`.
- Linking upstream's UI files now leaves 208 undefined symbols, down from
  318.

### Step 3d: the common controls, part 2 (done)

- Win32, following comctl32 (and Wine's where the documentation is quiet):
  - tooltips (`tooltips_class32`): tools by window or rectangle,
    `TTM_RELAYEVENT` and `TTF_SUBCLASS`, the show, pop and leave delays,
    `TTN_GETDISPINFO`, tracking, titles, balloons, close buttons and links;
  - toolbars (`ToolbarWindow32`): image lists (normal, disabled, hot),
    check and group buttons, drop-down buttons (`TBN_DROPDOWN`), flat hot
    tracking, text buttons, `TB_AUTOSIZE` with the `CCS_*` placement styles
    and tooltips by command;
  - `ComboBoxEx32`: an owner-drawn combo box child with images, selected
    images, overlays, indents and `CBEN_GETDISPINFO` callbacks;
  - `SysLink`: `<a href id>` links in wrapped text, `NM_CLICK` and
    `NM_RETURN` with `NMLINK`, Tab between links, `LM_*` messages;
  - property sheets: `PropertySheet()` and `CreatePropertySheetPage()`,
    pages made when first shown, the `PSN_*` notifications and `PSM_*`
    messages, Apply, Ctrl+Tab between pages, modal and modeless sheets.
    Wizards are not done (upstream has none).
- MFC: `EnableToolTips` and `CToolTipCtrl`, `CToolBarCtrl`, `CComboBoxEx`,
  and `CPropertySheet` and `CPropertyPage` as in MFC's dlgprop.cpp
  (`DoModal()` runs a modeless sheet in its own modal loop). The frame
  toolbar, `CToolBar`, comes with the frames.
- Dialog fixes found on the way: `FALSE` from `WM_INITDIALOG` keeps the
  focus the procedure set, and a modal dialog keeps that focus when its
  owner is disabled.
- Tests: `commctrl_test` covers the new controls; `dialog_test` runs
  property sheets with upstream's Options pages and now creates every
  control of every dialog in `mptrack.rc`. All tests pass at display scales
  1, 1.5 and 2.
- Linking upstream's UI files now leaves 181 undefined symbols, down from
  208.

### Step 3e: accelerators and resizable dialogs (done)

- Dynamic layouts: `CMFCDynamicLayout` as MFC's afxlayout.cpp has it, with
  `AFX_DIALOG_LAYOUT` resources loaded in `CDialog::OnInitDialog` and
  controls moved or grown by their percentages on `WM_SIZE`. Upstream's
  `ResizableDialog` (13 dialogs) reads the same resources itself; the other
  dialogs with layouts use this.
- `WM_GETMINMAXINFO`: top-level windows ask when they show, and the
  minimum and maximum track sizes become limits the window manager keeps
  while the user resizes.
- Accelerator tables: `CreateAcceleratorTable`, `LoadAccelerators`
  (`RT_ACCELERATOR`), `CopyAcceleratorTable` and `TranslateAccelerator`.
  Upstream has no accelerator resources (its own input handler maps keys),
  but MFC's frames translate through them, so the path works.
- Fix found on the way: moving or resizing a hidden window now sends
  `WM_MOVE` and `WM_SIZE` at once, as Windows does, where Qt held them back
  until the window showed.
- Tests: `dialog_test` resizes upstream's edit history dialog and checks
  every control against its layout, checks the minimum size holds, and
  runs accelerators with and without Ctrl. All tests pass at display
  scales 1, 1.5 and 2.
- Linking upstream's UI files now leaves 180 undefined symbols, down from
  181.

### Step 3f part 1: MDI, frames and document/view (done)

- user32's multiple-document interface (`platform/qtwin/mdi.cpp`): the
  `MDIClient` class, `DefFrameProc`, `DefMDIChildProc`, `CreateMDIWindow`
  and every `WM_MDI*` message. Children activate on click (now with
  `WM_MOUSEACTIVATE`), maximize into the client with minimize, restore and
  close buttons on the frame's menu bar and "Frame - [Child]" as the title,
  minimize to the bottom, tile and cascade, and get listed in the Window
  menu. The client gets scroll bars while a child reaches outside, and
  Ctrl+F4 and Ctrl+F6 work (`TranslateMDISysAccel`).
- Child windows with a caption can now be moved and sized with the mouse,
  and have working caption buttons (they were drawn only on top-level
  windows before); `WS_MAXIMIZE` and `WS_MINIMIZE` in the style say the
  window's state, as on Windows.
- MFC's frames: `CFrameWnd` (LoadFrame from a resource id, the view of a
  `CCreateContext`, control bar layout through `RepositionBars` and
  `WM_SIZEPARENT`, the active view, titles, and command routing from the
  view to its document, the frame and the application), `CMDIFrameWnd`
  and `CMDIChildWnd`, and `CCmdUI` for menu items (`WM_INITMENUPOPUP`,
  items without a handler disabled).
- Document/view: `CView`, `CScrollView`, `CFormView`, `CDocument`,
  `CDocTemplate`, `CMultiDocTemplate`, `CDocManager` and CWinApp's document
  functions, with MFC's object lifetimes (views delete themselves with
  their window, documents with their last view). `CSplitterWnd` for static
  splitters, as upstream's `CChildFrame` uses it; the panes follow the
  mouse while the bar is dragged, where MFC drags an outline.
- Not yet: `CDocManager::DoPromptFileName` (MFC's own file dialog; upstream
  uses its own `FileDialog`, which comes with the dialogs in step 4), and
  the MDI client losing its sunken edge while a child is maximized.
- Tests: `frame_test` drives the MDI messages, then builds an MFC
  application with upstream's `IDR_MAINFRAME` and `IDR_MODULETYPE`
  resources: documents with a two-pane splitter, a scroll view and a form
  view (upstream's general tab), titles, command routing, menu updates,
  and closing frames and documents. All tests pass at display scales 1,
  1.5 and 2.
- Linking upstream's UI files now leaves 159 undefined symbols, down from
  180.

### Step 3f part 2: control bars, docking and startup (done)

- MFC's control bars, written after barcore, bartool, barstat and bardlg:
  `CControlBar` (layout through `WM_SIZEPARENT`, delayed showing and
  hiding, borders and grippers, flyby help in the status bar and tooltips,
  idle command updates), `CToolBar` on the toolbar control (buttons from an
  id array, `LoadToolBar`, `LoadBitmap`, button text, wrapping into rows
  and columns, dynamic sizing), `CStatusBar` on a new status bar control
  (`msctls_statusbar32`, with indicators named from their strings and the
  key indicators) and `CDialogBar`.
- Docking, after bardock, dockcont, winmini and dockstat: `CDockBar` rows
  and placeholders, `CDockContext` dragging and resizing with MFC's outline
  and Ctrl to stop docking, `CMiniDockFrameWnd` floating frames with their
  own caption and close box (so docking by dragging works under any window
  manager), `EnableDocking`, `DockControlBar`, `FloatControlBar`,
  `ShowControlBar`, and `SaveBarState`/`LoadBarState` with MFC's section
  and key names.
- What `CWinApp::InitInstance` uses: `ParseCommandLine` (flags start with
  `-`; `/` starts a path here), `ProcessShellCommand`, the profile
  functions (an ini file, under `~/.config/<registry key>/` once
  `SetRegistryKey` is called), the recent file list, `EnableShellOpen`
  (the desktop opens files through the command line) and files dropped on
  the main window (`DragAcceptFiles`, `WM_DROPFILES`, `DragQueryFile`).
- Fixes found on the way: `FWS_PREFIXTITLE` and `FWS_SNAPTOBARS` had the
  values of window styles (`WS_THICKFRAME`, `WS_SYSMENU`); a re-parented
  child kept a clip from its old parent and could vanish; a status bar's
  `WM_GETFONT` now answers with the font it draws with.
- Tests: `bars_test` lays out a frame with a toolbar, status bar and dialog
  bar, checks idle updates and flyby text, docks the toolbar top and left,
  floats it, drags it back and out again, saves and loads the bar state,
  and runs the command line, profile and file drop functions. All tests
  pass at display scales 1, 1.5 and 2.
- Linking upstream's UI files now leaves 129 undefined symbols, down from
  159.

### Step 3g: the real application, playing through JACK (done)

- `openmpt` is built from upstream's tracker files unchanged
  (`mpt_tracker_ui`) and three small replacements in `app/`: `main.cpp`
  (what `AfxWinMain` does: `InitInstance`, then the message loop),
  `FileDialog.cpp` and `WineIntegration.cpp`. Upstream's `CTrackApp` and
  `CMainFrame` start for real: settings and key bindings in
  `~/.config/OpenMPT/`, the toolbars, the tree view, the quick start dialog,
  modules from the command line (`openmpt -play song.it` plays at once),
  and closing saves the settings and exits cleanly.
- The Win32 and system pieces it needed: events, waits and change
  notifications on file descriptors (`eventfd`, `inotify`, `poll`), file
  finding, the clipboard (on `QClipboard`, registered formats as their own
  MIME types), the shell functions (trash, copy and move, file icons,
  opening files and links), GDI+ for the PNG toolbar and icon images (on
  `QImage`), winmm MIDI input on RtMidi (ALSA sequencer or JACK), the
  colour and font dialogs, OLE file drops on the tree view, window class
  atoms, and the default size and position of top-level windows.
- Sound: upstream's own sound device layer through the bundled RtAudio.
  When a JACK server is running (JACK lists devices only then, and PipeWire
  always provides one), its devices now rank first, ahead of PulseAudio
  (`patches/0004`). A device chosen in the settings still wins.
- Help opens the OpenMPT wiki manual page for the topic in the browser,
  since the Windows help file does not exist here.
- Fixes found on the way: `CWinApp::InitInstance` returned `FALSE`;
  `CFrameWnd::GetActiveView` was virtual, which MFC's is not, so
  `CMainFrame`'s own version took over the frame's and closing the main
  window used a deleted document; GCC dropped MFC's `this == NULL` checks
  (`GetSafeHwnd`), so everything now builds with
  `-fno-delete-null-pointer-checks`; clipboard formats registered from
  static initialisers came before the registry existed; relative paths on
  the command line were not resolved against the starting folder.
- Tests: `app_start` starts the application offscreen with a module and a
  fresh configuration, saves a picture of the main window and closes it;
  `app_jack_playback` (when `jackd` is installed) starts a private JACK
  server with the dummy driver, plays the module with `-play` and measures
  the sound on the tracker's JACK ports.
- Known gap, fixed in step 4: the General tab's upper pane was empty. (The
  VU meter was fine: it only looked black in a still picture taken while
  nothing played.)

## Step 4: views and dialogs

### Step 4a: the screenshot pass (done)

`tools/screenshots/pass.sh <openmpt> <out dir>` starts the real application
once per entry, opens one of upstream's test modules, runs menu commands by
their `resource.h` names and saves the main window and every dialog they
open. `app/main.cpp` has the hooks it uses (`LINOPENMPT_COMMANDS`,
`LINOPENMPT_SCREENSHOT`, `LINOPENMPT_DUMP_WINDOWS`); when the picture is
taken, the dialogs are cancelled and the main window is closed, as a user
would.

Found and fixed with it:

- The General tab's upper pane was hidden behind the tab control. A tab
  control now leaves its display area to the windows behind it, as on
  Windows, unless it holds its pages as its own children.
- The comments view crashed: list and tree views answered `WM_GETFONT`
  with no font where comctl32 answers with the control's default one.
- Closing the main window while a dialog was open (a message box, a file
  or colour dialog) freed the dialog twice. Qt's dialogs are now on the
  heap and go with their owner.
- Sample paths saved on Windows (`.\test.flac`) were not found, because
  the backslashes were taken as part of the file name.
- The About dialog named the system "Generic Windows NT (Version 0.0)"; it
  now shows `uname` (patch 0005).
- The Open dialog started in the program's own folder; with no folder of
  its own it now starts in the songs folder or home, as Windows' dialog
  does.

Checked and the same as upstream on Windows: the pattern view's empty band
above row 0 ("always center active row" is on by default), the channel
manager's highlight-coloured background, the MIDI mapping dialog's tall
up/down control.

Dialogs and views (78 in `mptrack.rc`), checked so far: the five module
views (General, Patterns, Samples, Instruments, Comments), About, Welcome,
splash screen, missing external samples, Open, Save As, stream export,
MIDI export, cleanup, sample trimmer, find, go to, keyboard split, Setup
(General page), plugin manager, channel manager, clipboard manager, song
properties, Zxx macros, MIDI mapping, edit history, MPT hacks, song length.

Still to check: the other Setup pages, the sample editor's dialogs
(amplify, resample, pitch shift, silence, generator, crossfade, grid,
raw import, autotune), the instrument editor's (sample map, envelope
scaling, tuning), the pattern editor's (effect visualizer, edit command,
pattern properties, find range, chords, tempo swing, channel settings),
plugin editors and plugin-related dialogs, OPL, metronome, close/modified
documents, remove channels, progress and update dialogs.

Known gaps:

- The update check is compiled out, so the Welcome dialog shows its
  update options with a placeholder line, as upstream does in such a build.

### Step 4b: upstream's optional libraries (done)

Upstream's Windows build (`build/premake/mpt-OpenMPT.lua`) has a set of
optional libraries for sample formats, stream export and archives. This
build now has all of them that make sense on Linux
(`cmake/OpenMPTExternal.cmake`):

| Library | Gives | From |
|---------|-------|------|
| FLAC | FLAC samples, FLAC export | system (`libflac-dev`) |
| Vorbis encoder | Ogg Vorbis export | system (`libvorbis-dev`) |
| Opus, opusfile, libopusenc | Opus samples, Opus export | system |
| LAME | MP3 export | system (`libmp3lame-dev`) |
| lhasa | modules in LHA archives | system (`liblhasa-dev`) |
| ancient | PowerPacker, XPK and MMCMP packed modules | `upstream/include/ancient` |
| UnRAR | modules in RAR archives | `upstream/include/unrar` (upstream's trimmed copy) |
| minizip | modules in ZIP archives | `upstream/include/zlib/contrib/minizip`, on the system zlib |

r8brain (resampling) and Signalsmith Stretch (time stretching) were
already built from `upstream/include`. Left out, because they are only
used on Windows: PortAudio (Windows sound APIs; JACK, PulseAudio and ALSA
go through RtAudio here), ASIO, and Crypto++ (signatures for the update
check, which is not built yet).

Test: `codecs_test` renders a module, writes it as FLAC, MP3, Opus and
Vorbis with each encoder's default settings, reads every file back as a
sample, and loads a module from a ZIP archive. The test module with an
external FLAC sample (`test.mptm`) now opens without the missing samples
dialog.

### Step 4c: the editors' dialogs and the Setup pages (done)

The screenshot pass can now press buttons inside a view's control panel
and switch Setup pages: a command written `Class:NAME` goes to the first
visible window of that C++ class (`CCtrlSamples:IDC_SAMPLE_AMPLIFY`), and
`Class:pageN` shows page N of a property sheet. 27 entries were added, so
55 in all, every one opened and closed by the `app_views` test.

Checked: every Setup page (General, Sound Card, Mixer, DSP, Samples,
Keyboard, Display, MIDI, Paths / Auto Save, Advanced; Update needs the
update check and Wine only applies on Wine), Quick Start, OPL export, the
sample editor's amplify, resample, silence, crossfade, autotune, grid
size and raw import file dialog, the instrument editor's sample map and
envelope scaling, and the pattern editor's pattern properties, channel
settings, remove channels, amplify, effect visualizer, chord editor and
metronome settings.

Found and fixed:

- A combo box with an icon column (ComboBoxEx: Sound Card device, fade
  curves) showed a scroll bar beside it. Its `WS_VSCROLL` is for the
  list, as for a plain combo box.
- A check box on a group box's top line (DSP's "Enable Graphic
  Equalizer") had the frame line drawn through it. Windows paints
  overlapping siblings in Z order, so the control below the group box
  wins; the group box now leaves those controls out when it draws.
- Message box buttons showed the Qt style's icons (a tick on OK).

Still to check, because they need a particular state to open (a plugin,
an OPL instrument, a tuning, a raw file chosen, several open documents,
a long operation, a search): plugin editors and the plugin dialogs
(missing plugins, move slot, plugin scan, LFO and MIDI I/O editors), OPL
instrument parameters, the tuning editor, raw sample import options,
the sample generator, mix samples, tempo swing, close documents, modified
samples, legacy playback, find range, pattern edit command, EQ preset
save, progress, info box and input dialogs.

### Step 4d: the plugin dialogs (done)

The built-in plugins' dialogs are in the screenshot pass: the generic
plugin editor (on the DirectX Echo emulation), the LFO editor, plugin
selection, move and insert slot, preset saving, and the message for a
plugin that is not installed. `make_plugin_module` (`tools/smoke`) writes
the modules they need from `test.mptm`: one with LFO and Echo, one with a
plugin that is not installed. 64 entries in all.

Found and fixed:

- The plugin editors' first row was under their menu bar. On Windows,
  `SetMenu` moves the client area down and the child windows with it,
  since they are placed in client coordinates; when the non-client area
  changes, the children now move with the client area.
- A Yes/No message box closed without an answer (its owner destroyed, or
  rejected) answered Yes, the first button. It now answers with the
  cancelling button, or the last one (No). The test harness found this by
  answering "search for the missing plugin online?" with Yes.

Not in the pass: the MIDI Input Output plugin needs the ALSA sequencer
(`/dev/snd/seq`), which containers and CI lack; there, upstream reports
the plugin as not found, as it does on Windows when its MIDI layer fails.

Next: hosting VST plugins (`MPT_WITH_VST`, off so far), with native Linux
VST plugins and the plugin editors embedded in Qt windows.

## Step 5: the MFC to Qt6 bridge as its own project

Once OpenMPT runs at parity, `compat/`, `platform/qtwin/` and
`platform/mfc/` move into a standalone, reusable project for porting other
MFC applications to Linux:

- Move the OpenMPT-specific parts out of the headers (`afxwin.h`,
  `mfc_ui.h` and friends) into this repository, behind hooks the bridge
  calls.
- A reusable CMake function that compiles an application's `.rc` file into
  its resources (what `tools/rc` does for `mptrack.rc` today).
- Its own tests and test resources, so it no longer depends on upstream's
  `mptrack.rc`; the smoke tests here become the start of that suite.
- Packaged as a CMake library (`find_package` and `add_subdirectory`), with
  a license, a README and a short porting guide. This repository then uses
  it as a dependency.

It covers what OpenMPT uses of MFC and Win32, which is a large part of both
but not all: another application may need pieces added.
