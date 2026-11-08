# Porting status

Upstream: OpenMPT-1.32.12.00

## Step 1: build infrastructure (done)

- `mpt_common`, `mpt_sounddevice`, `mpt_libopenmpt` build on Ubuntu 24.04
  (GCC 13, CMake 3.28, Qt 6.4) with no upstream changes.
- `render_module` loads and renders `test.mod`, `test.s3m`, `test.xm`,
  `test.mptm` from upstream's test folder. (`test.xm` and `test.mptm` are
  silent by design; upstream's `openmpt123` renders them as silence too.)
- `sounddev_list` enumerates devices through OpenMPT's sound device layer.

## Step 2: soundlib in tracker configuration (starting point)

Building `mpt_soundlib` with `-DLINOPENMPT_TRACKER_SOUNDLIB=ON`:
**116 of 142** translation units fail. Nearly all of it comes from a few
headers, so a small compatibility layer should clear most of it.

Missing identifiers, by number of errors:

| Identifier | Errors | Kind |
|------------|-------:|------|
| `CString` | 664 | MFC string |
| `DWORD` | 533 | Win32 type |
| `UINT` | 455 | Win32 type |
| `LONG` | 54 | Win32 type |
| `ASSERT` | 32 | MFC macro |
| `COLORREF` | 13 | Win32 type |
| `afx_msg`, `HDC`, `HMODULE`, `CWnd`, … | under 15 each | MFC/Win32 UI, via `mptrack/` headers |

Headers the errors come from:

| Header | Errors | Note |
|--------|-------:|------|
| `sounddsp/AGC.h`, `sounddsp/DSP.h` | ~800 | Win32 integer types only |
| `soundlib/MIDIMacros.h` | ~300 | `CString` in tracker-only helpers |
| `mptrack/TrackerSettings.h`, `Settings.h` | ~270 | tracker settings, used by soundlib |
| `mptrack/Mptrack.h` | ~160 | app object |
| `soundlib/plugins/PlugInterface.h` | ~180 | plugin base class, Win32 types and editor hooks |
| `mptrack/MIDIMapping.h`, `Vstplug.h` | ~120 | MIDI mapping, VST host |

`mptrack/` headers that soundlib includes in tracker configuration (these
are the coupling points between engine and app that step 2 has to handle):
`TrackerSettings.h` (11), `Moddoc.h` (8), `Mptrack.h` (4), `Vstplug.h` (3),
`Reporting.h` (3), `Mainfrm.h` (2), `AbstractVstEditor.h` (2), and one each of
`MidiInOut.h`, `LFOPluginEditor.h`, `WindowMessages.h`, `VstPresets.h`,
`MIDIMapping.h`, `InputHandler.h`, `FileDialog.h`, `ExceptionHandler.h`,
`DefaultVstEditor.h`.

Also needed: `dmoreg.h` (Windows DirectX Media Objects) and `Msctf.h` (Windows
text input), which only matter for Windows-specific features and will be
stubbed or guarded.

## Step 3: Qt6 shell

Not started. Qt6 is found by the build already.

## Step 4: views and dialogs

Not started. The checklist of every view and dialog will live here.
