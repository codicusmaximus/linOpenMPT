# mfcless file map

Each upstream `mptrack/` source file and what the mfcless (pure Qt 6, no MFC)
build does with it.

- **twin**: rewritten in Qt as `qt/mptrack/<same name>`, keeping upstream's
  class and function names and their order, so that an upstream change can be
  carried over by reading the two side by side.
- **reuse**: compiled from upstream's file in the `build-qt/upstream-mfcless`
  worktree. "patched" means a small patch in `patches/mfcless/` swaps MFC
  types (CString, CArchive and so on) for standard or mpt ones.
- **dropped**: Windows-only code with no counterpart on Linux.

Upstream: 8bc4cd9 (OpenMPT 1.32.12.00)

## How the build compares with the MFC bridge build

Both builds are run headless on the same steps (`LINOPENMPT_COMMANDS`,
`LINOPENMPT_SCREENSHOT`) and their windows compared screenshot by screenshot.
Checked so far, all matching in layout and content:

- Start-up: main window, menus, toolbar, tree, status bar, Quick Start dialog.
- A loaded module: the General, Patterns, Samples, Instruments and Comments
  pages and their views.
- Setup: all eleven pages of the options dialog.
- Dialogs: About, song properties, channel manager, plugin manager, MIDI
  mapping, pattern properties, go to, keyboard split, MPT hacks, edit history.

Linux and Qt idioms used on purpose: Qt's tree and list look (group headers,
expand arrows), button box icons, a Help button in place of the title bar's
"?" button, and a toolbar overflow arrow in place of a second toolbar row.

## Files

| Upstream file | Lines | Plan | Status |
|---|---:|---|---|
| `AboutDialog.cpp` | 608 | twin | done |
| `AbstractVstEditor.cpp` | 1031 | twin | done |
| `AdvancedConfigDlg.cpp` | 327 | twin | done |
| `AppendModule.cpp` | 325 | reuse | done |
| `AutoSaver.cpp` | 231 | twin | done |
| `Autotune.cpp` | 441 | twin | done |
| `CDecimalSupport.cpp` | 56 | twin | done |
| `CImageListEx.cpp` | 173 | twin | done |
| `ChannelManagerDlg.cpp` | 1048 | twin | done |
| `Childfrm.cpp` | 571 | twin | done |
| `CleanupSong.cpp` | 1125 | twin | done |
| `CloseMainDialog.cpp` | 142 | twin | done |
| `ColorConfigDlg.cpp` | 654 | twin | done |
| `ColorPickerButton.cpp` | 99 | twin | done |
| `ColourEdit.cpp` | 68 | twin | done |
| `CommandSet.cpp` | 2330 | reuse (patched) | done |
| `Ctrl_com.cpp` | 370 | twin | done |
| `Ctrl_gen.cpp` | 838 | twin | done |
| `Ctrl_ins.cpp` | 3163 | twin | done |
| `Ctrl_pat.cpp` | 1442 | twin | done |
| `Ctrl_seq.cpp` | 1690 | twin | done |
| `Ctrl_smp.cpp` | 2982 | twin | done |
| `DefaultVstEditor.cpp` | 501 | twin | done |
| `DialogBase.cpp` | 158 | twin | done |
| `Draw_pat.cpp` | 1980 | twin | done |
| `EffectInfo.cpp` | 1157 | twin | done |
| `EffectVis.cpp` | 952 | twin | done |
| `ExceptionHandler.cpp` | 861 | reuse | done |
| `ExternalSamples.cpp` | 373 | twin | done |
| `FileDialog.cpp` | 274 | twin | done |
| `FolderScanner.cpp` | 89 | twin | done |
| `GeneralConfigDlg.cpp` | 246 | twin | done |
| `Globals.cpp` | 896 | twin | done |
| `HTTP.cpp` | 522 | reuse | done |
| `HighDPISupport.cpp` | 226 | twin | done |
| `IPCWindow.cpp` | 253 | twin | done |
| `Image.cpp` | 340 | twin | done |
| `InputHandler.cpp` | 617 | twin | done |
| `KeyConfigDlg.cpp` | 1175 | twin | done |
| `LinkResolver.cpp` | 62 | twin | done |
| `MIDIMacroDialog.cpp` | 452 | twin | done |
| `MIDIMapping.cpp` | 182 | reuse (patched) | done |
| `MIDIMappingDialog.cpp` | 503 | twin | done |
| `MPTHacks.cpp` | 515 | reuse | done |
| `MPTrackLink.cpp` | 69 | reuse | done |
| `MPTrackUtil.cpp` | 85 | twin | done |
| `MPTrackUtilWine.cpp` | 313 | dropped | no Wine integration in the native build |
| `MPTrackWine.cpp` | 896 | dropped | no Wine integration in the native build |
| `MainFrm.cpp` | 3691 | twin | done |
| `Mainbar.cpp` | 1815 | twin | done |
| `Mod2wave.cpp` | 1436 | twin | done |
| `ModConvert.cpp` | 686 | reuse | done |
| `ModDocTemplate.cpp` | 249 | twin | done |
| `Moddoc.cpp` | 3462 | reuse (patched) | done |
| `Modedit.cpp` | 1411 | reuse (patched) | done |
| `MoveFXSlotDialog.cpp` | 92 | twin | done |
| `Mpdlgs.cpp` | 2048 | twin | done |
| `Mpt_midi.cpp` | 216 | twin | done |
| `Mptrack.cpp` | 2763 | twin | done |
| `OPLExport.cpp` | 629 | twin | done |
| `OPLInstrDlg.cpp` | 290 | twin | done |
| `PSRatioCalc.cpp` | 177 | twin | done |
| `PathConfigDlg.cpp` | 232 | twin | done |
| `PatternClipboard.cpp` | 1224 | twin | done |
| `PatternEditorDialogs.cpp` | 2106 | twin | done |
| `PatternFindReplace.cpp` | 572 | twin | done |
| `PatternFindReplaceDlg.cpp` | 1017 | twin | done |
| `PatternFont.cpp` | 483 | twin | done |
| `PatternGotoDialog.cpp` | 204 | twin | done |
| `PlugNotFoundDlg.cpp` | 79 | twin | done |
| `PluginComboBox.cpp` | 205 | twin | done |
| `ProgressDialog.cpp` | 120 | twin | done |
| `QuickStartDialog.cpp` | 393 | twin | done |
| `Reporting.cpp` | 186 | twin | done |
| `ResizableDialog.cpp` | 182 | twin | done |
| `SampleConfigDlg.cpp` | 149 | twin | done |
| `SampleEditorDialogs.cpp` | 993 | twin | done |
| `SampleGenerator.cpp` | 737 | twin | done |
| `SampleTrimmer.cpp` | 179 | twin | done |
| `ScaleEnvPointsDlg.cpp` | 86 | twin | done |
| `SelectPluginDialog.cpp` | 895 | twin | done |
| `Settings.cpp` | 528 | reuse (patched) | done |
| `StreamEncoderSettings.cpp` | 105 | reuse (patched) | done |
| `TempoSwingDialog.cpp` | 482 | twin | done |
| `TrackerSettings.cpp` | 1671 | reuse (patched) | done |
| `TuningDialog.cpp` | 1743 | twin | done |
| `Undo.cpp` | 1018 | reuse (patched) | done |
| `UpdateCheck.cpp` | 1781 | twin | done |
| `UpdateToolTip.cpp` | 94 | twin | done |
| `VSTEditor.cpp` | 153 | twin | done |
| `View_gen.cpp` | 1858 | twin | done |
| `View_ins.cpp` | 2978 | twin | done |
| `View_pat.cpp` | 7719 | twin | done |
| `View_smp.cpp` | 4263 | twin | done |
| `View_tre.cpp` | 4745 | twin | done |
| `VstPresets.cpp` | 316 | reuse (patched) | done |
| `Vstplug.cpp` | 1891 | twin | done |
| `WelcomeDialog.cpp` | 255 | twin | done |
| `WineSoundDeviceStub.cpp` | 392 | dropped | no Wine integration in the native build |
| `dlg_misc.cpp` | 1653 | twin | done |
| `mod2midi.cpp` | 1069 | twin | done |
| `tuningRatioMapWnd.cpp` | 194 | twin | done |
| `view_com.cpp` | 829 | twin | done |
| `plugins/LFOPluginEditor.cpp` | 371 | twin | done |
| `plugins/MidiInOut.cpp` | 749 | reuse (patched) | done |
| `plugins/MidiInOutEditor.cpp` | 347 | twin | done |
