# mfcless file map

Each upstream `mptrack/` source file and what the mfcless build does with it.
**reuse**: compiled from upstream unchanged (no windows or messages in it).
**twin**: rewritten in Qt as `qt/<same name>`, keeping upstream's class and
function names and order, so upstream changes can be carried over by eye.

Upstream: 8bc4cd9

| Upstream file | Lines | Plan | Status |
|---|---:|---|---|
| `AboutDialog.cpp` | 592 | twin | todo |
| `AbstractVstEditor.cpp` | 1031 | twin | todo |
| `AdvancedConfigDlg.cpp` | 327 | twin | todo |
| `AppendModule.cpp` | 325 | reuse | todo |
| `AutoSaver.cpp` | 231 | reuse | todo |
| `Autotune.cpp` | 441 | twin | todo |
| `CDecimalSupport.cpp` | 55 | twin | todo |
| `CImageListEx.cpp` | 173 | twin | todo |
| `ChannelManagerDlg.cpp` | 1048 | twin | todo |
| `Childfrm.cpp` | 571 | twin | todo |
| `CleanupSong.cpp` | 1125 | twin | todo |
| `CloseMainDialog.cpp` | 142 | twin | todo |
| `ColorConfigDlg.cpp` | 654 | twin | todo |
| `ColorPickerButton.cpp` | 99 | twin | done |
| `ColourEdit.cpp` | 68 | twin | todo |
| `CommandSet.cpp` | 2330 | reuse | todo |
| `Ctrl_com.cpp` | 370 | twin | todo |
| `Ctrl_gen.cpp` | 838 | twin | todo |
| `Ctrl_ins.cpp` | 3163 | twin | todo |
| `Ctrl_pat.cpp` | 1442 | twin | todo |
| `Ctrl_seq.cpp` | 1690 | twin | todo |
| `Ctrl_smp.cpp` | 2982 | twin | todo |
| `DefaultVstEditor.cpp` | 501 | twin | todo |
| `DialogBase.cpp` | 158 | twin | todo |
| `Draw_pat.cpp` | 1980 | twin | todo |
| `EffectInfo.cpp` | 1157 | reuse | todo |
| `EffectVis.cpp` | 952 | twin | todo |
| `ExceptionHandler.cpp` | 861 | reuse | todo |
| `ExternalSamples.cpp` | 373 | twin | todo |
| `FileDialog.cpp` | 274 | twin | todo |
| `FolderScanner.cpp` | 89 | twin | done |
| `GeneralConfigDlg.cpp` | 246 | twin | todo |
| `Globals.cpp` | 896 | twin | todo |
| `HTTP.cpp` | 522 | reuse | todo |
| `HighDPISupport.cpp` | 226 | twin | todo |
| `IPCWindow.cpp` | 253 | twin | todo |
| `Image.cpp` | 340 | twin | todo |
| `InputHandler.cpp` | 617 | twin | todo |
| `KeyConfigDlg.cpp` | 1175 | twin | todo |
| `LinkResolver.cpp` | 62 | reuse | todo |
| `MIDIMacroDialog.cpp` | 452 | twin | todo |
| `MIDIMapping.cpp` | 182 | reuse | todo |
| `MIDIMappingDialog.cpp` | 503 | twin | todo |
| `MPTHacks.cpp` | 515 | reuse | todo |
| `MPTrackLink.cpp` | 69 | reuse | todo |
| `MPTrackUtil.cpp` | 85 | reuse | todo |
| `MPTrackUtilWine.cpp` | 313 | twin | todo |
| `MPTrackWine.cpp` | 896 | reuse | todo |
| `MainFrm.cpp` | 3691 | twin | todo |
| `Mainbar.cpp` | 1815 | twin | todo |
| `Mod2wave.cpp` | 1436 | twin | todo |
| `ModConvert.cpp` | 686 | reuse | todo |
| `ModDocTemplate.cpp` | 245 | twin | todo |
| `Moddoc.cpp` | 3462 | twin | todo |
| `Modedit.cpp` | 1411 | reuse | todo |
| `MoveFXSlotDialog.cpp` | 92 | twin | todo |
| `Mpdlgs.cpp` | 2048 | twin | todo |
| `Mpt_midi.cpp` | 216 | twin | todo |
| `Mptrack.cpp` | 2761 | twin | todo |
| `OPLExport.cpp` | 629 | twin | todo |
| `OPLInstrDlg.cpp` | 290 | twin | todo |
| `PSRatioCalc.cpp` | 177 | twin | todo |
| `PathConfigDlg.cpp` | 232 | twin | todo |
| `PatternClipboard.cpp` | 1224 | twin | todo |
| `PatternEditorDialogs.cpp` | 2106 | twin | todo |
| `PatternFindReplace.cpp` | 572 | twin | todo |
| `PatternFindReplaceDlg.cpp` | 1017 | twin | todo |
| `PatternFont.cpp` | 483 | twin | todo |
| `PatternGotoDialog.cpp` | 204 | twin | todo |
| `PlugNotFoundDlg.cpp` | 79 | twin | todo |
| `PluginComboBox.cpp` | 205 | reuse | todo |
| `ProgressDialog.cpp` | 120 | twin | todo |
| `QuickStartDialog.cpp` | 393 | twin | todo |
| `Reporting.cpp` | 186 | twin | todo |
| `ResizableDialog.cpp` | 182 | twin | todo |
| `SampleConfigDlg.cpp` | 149 | twin | todo |
| `SampleEditorDialogs.cpp` | 993 | twin | todo |
| `SampleGenerator.cpp` | 737 | twin | todo |
| `SampleTrimmer.cpp` | 179 | twin | todo |
| `ScaleEnvPointsDlg.cpp` | 86 | twin | todo |
| `SelectPluginDialog.cpp` | 886 | twin | todo |
| `Settings.cpp` | 526 | reuse | todo |
| `StreamEncoderSettings.cpp` | 105 | reuse | todo |
| `TempoSwingDialog.cpp` | 482 | twin | todo |
| `TrackerSettings.cpp` | 1671 | reuse | todo |
| `TuningDialog.cpp` | 1743 | twin | todo |
| `Undo.cpp` | 1018 | reuse | todo |
| `UpdateCheck.cpp` | 1781 | twin | todo |
| `UpdateToolTip.cpp` | 94 | twin | todo |
| `VSTEditor.cpp` | 149 | twin | todo |
| `View_gen.cpp` | 1858 | twin | todo |
| `View_ins.cpp` | 2978 | twin | todo |
| `View_pat.cpp` | 7719 | twin | todo |
| `View_smp.cpp` | 4263 | twin | todo |
| `View_tre.cpp` | 4732 | twin | todo |
| `VstPresets.cpp` | 316 | reuse | todo |
| `Vstplug.cpp` | 1886 | reuse | todo |
| `WelcomeDialog.cpp` | 207 | twin | todo |
| `WineSoundDeviceStub.cpp` | 392 | reuse | todo |
| `dlg_misc.cpp` | 1653 | twin | todo |
| `mod2midi.cpp` | 1069 | twin | todo |
| `tuningRatioMapWnd.cpp` | 194 | twin | todo |
| `view_com.cpp` | 829 | twin | todo |
