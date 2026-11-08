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

### Step 2b: the rest of mptrack/ against the MFC layer

The goal is that every upstream UI file compiles unchanged, so step 3 only
has to implement MFC and Win32 on top of Qt, not rewrite OpenMPT's UI.

- MFC message maps are real (`compat/include/linopenmpt/mfc_msgmap.h`):
  `BEGIN_MESSAGE_MAP`, `ON_COMMAND`, `ON_UPDATE_COMMAND_UI`, control
  notifications, `ON_NOTIFY`, `ON_MESSAGE` and the `ON_WM_*` handlers build
  the same tables as MFC, and `CCmdTarget::OnCmdMsg` routes commands through
  them (tested by `msgmap_test`).
- `mpt_tracker_ui` compiles the UI files that build cleanly. It is not linked
  into anything yet.
- The audio export encoders (`src/openmpt/streamencoder/`) are now part of
  `mpt_tracker_logic`.

Compiling: 38 of 112 files (not counting `MPTrackLink.cpp` and
`WineSoundDeviceStub.cpp`, which only matter on Windows/Wine). Among them are
`Moddoc.cpp`, `Modedit.cpp`, `PatternClipboard.cpp`, `ModConvert.cpp`,
`CleanupSong.cpp`, `AppendModule.cpp`, `SampleTrimmer.cpp`, `Reporting.cpp`
and several dialogs.

Not compiling yet, with the number of compile errors. Most errors are missing
GDI (`CDC` drawing), menu, virtual key, common control (tree, list, toolbar,
tooltip) and window-management declarations, shared by many files.

| File | Errors |
|------|-------:|
| `AboutDialog.cpp` | 1 |
| `CImageListEx.cpp` | 1 |
| `Childfrm.cpp` | 1 |
| `ExceptionHandler.cpp` | 1 |
| `FolderScanner.cpp` | 1 |
| `Globals.cpp` | 1 |
| `HTTP.cpp` | 1 |
| `Image.cpp` | 1 |
| `LinkResolver.cpp` | 1 |
| `MPTrackWine.cpp` | 1 |
| `MainFrm.cpp` | 1 |
| `Mpt_midi.cpp` | 1 |
| `Mptrack.cpp` | 1 |
| `OPLExport.cpp` | 1 |
| `QuickStartDialog.cpp` | 1 |
| `View_gen.cpp` | 1 |
| `View_tre.cpp` | 1 |
| `mod2midi.cpp` | 1 |
| `PathConfigDlg.cpp` | 2 |
| `PlugNotFoundDlg.cpp` | 2 |
| `plugins/LFOPluginEditor.cpp` | 2 |
| `PatternGotoDialog.cpp` | 3 |
| `ColourEdit.cpp` | 5 |
| `Ctrl_gen.cpp` | 5 |
| `ModDocTemplate.cpp` | 5 |
| `OPLInstrDlg.cpp` | 5 |
| `GeneralConfigDlg.cpp` | 7 |
| `MIDIMappingDialog.cpp` | 9 |
| `WelcomeDialog.cpp` | 9 |
| `CDecimalSupport.cpp` | 12 |
| `ExternalSamples.cpp` | 14 |
| `HighDPISupport.cpp` | 14 |
| `ColorPickerButton.cpp` | 16 |
| `Ctrl_com.cpp` | 16 |
| `Mod2wave.cpp` | 16 |
| `SampleEditorDialogs.cpp` | 16 |
| `tuningRatioMapWnd.cpp` | 16 |
| `AdvancedConfigDlg.cpp` | 17 |
| `AutoSaver.cpp` | 17 |
| `MPTrackUtil.cpp` | 17 |
| `PatternFindReplaceDlg.cpp` | 17 |
| `UpdateToolTip.cpp` | 17 |
| `DefaultVstEditor.cpp` | 18 |
| `ProgressDialog.cpp` | 18 |
| `IPCWindow.cpp` | 23 |
| `PatternEditorDialogs.cpp` | 23 |
| `ResizableDialog.cpp` | 27 |
| `DialogBase.cpp` | 29 |
| `FileDialog.cpp` | 36 |
| `TempoSwingDialog.cpp` | 39 |
| `Ctrl_pat.cpp` | 40 |
| `Mpdlgs.cpp` | 40 |
| `Ctrl_smp.cpp` | 41 |
| `MPTrackUtilWine.cpp` | 44 |
| `PatternFont.cpp` | 48 |
| `UpdateCheck.cpp` | 49 |
| `ColorConfigDlg.cpp` | 53 |
| `InputHandler.cpp` | 53 |
| `SelectPluginDialog.cpp` | 59 |
| `EffectVis.cpp` | 67 |
| `view_com.cpp` | 68 |
| `TuningDialog.cpp` | 70 |
| `dlg_misc.cpp` | 71 |
| `AbstractVstEditor.cpp` | 76 |
| `Draw_pat.cpp` | 86 |
| `KeyConfigDlg.cpp` | 92 |
| `ChannelManagerDlg.cpp` | 96 |
| `Ctrl_ins.cpp` | 97 |
| `Ctrl_seq.cpp` | 106 |
| `View_ins.cpp` | 106 |
| `View_pat.cpp` | 138 |
| `Mainbar.cpp` | 188 |
| `View_smp.cpp` | 230 |
| `CommandSet.cpp` | 281 |

## Step 3: Qt6 shell

Not started. Qt6 is found by the build already.

## Step 4: views and dialogs

Not started. The checklist of every view and dialog will live here.
