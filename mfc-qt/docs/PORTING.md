# Porting an MFC application with mfc-qt

mfc-qt lets an MFC application's own source build on Linux. The work of a
port is mostly in the build and at the edges: the entry point, the
resource script, and the few Windows facilities Linux has no counterpart
for. Window code, message maps, dialogs and drawing should not need to
change.

## 1. Build

Add mfc-qt to the project (a git submodule works well) and link the
application against `mfcqt::mfcqt`:

```cmake
add_subdirectory(mfc-qt)
target_link_libraries(myapp PRIVATE mfcqt::mfcqt)
```

This brings the include path for `<afxwin.h>` and the other Windows and MFC
headers, `UNICODE` and `_UNICODE`, C++20, and Qt 6 Widgets.

Options, set before `add_subdirectory`:

| Option | Default | What |
|--------|---------|------|
| `MFCQT_BUILD_TESTS` | on when mfc-qt is the top-level project | mfc-qt's own tests |
| `MFCQT_RTMIDI_TARGET` | empty | A CMake target for RtMidi, if the application builds its own; otherwise mfc-qt looks for `rtmidi` with pkg-config, and without it `midiInGetNumDevs()` reports no devices |

Applications usually compile with `-fpermissive` at first, as code written
for MSVC often relies on things GCC rejects (a temporary bound to a
non-const reference, missing `typename`). Fix those as they come up, or
keep the flag.

## 2. Resources

Compile the application's `.rc` script into it:

```cmake
mfcqt_add_resources(myapp res/myapp.rc FUNCTION RegisterMyAppResources WINDOWS_PATHS)
```

- `FUNCTION` names a generated `void RegisterMyAppResources()`, which makes
  the resources the executable's, as if linked in by the Windows linker:
  `GetModuleHandle(nullptr)`, `AfxGetResourceHandle()` and a null
  `HINSTANCE` all find them.
- `WINDOWS_PATHS` is for a script written for Windows, as Visual Studio
  writes them: it resolves backslashed, case-insensitive file names
  (`res\Toolbar.bmp`) to the real files.
- `INCLUDE_DIRECTORIES` and `DEFINITIONS` are passed to the preprocessor.
  The script's own directory and mfc-qt's `afxres.h` and `winver.h` are
  already searched.

The script is preprocessed by the C++ compiler with `RC_INVOKED` defined,
then compiled by `tools/rc.py` into the same binary format `rc.exe`
writes. It reads what Visual Studio writes: `DIALOG`/`DIALOGEX` with every
control statement, `MENU`/`MENUEX`, `ACCELERATORS`, `STRINGTABLE`, `ICON`,
`CURSOR`, `BITMAP`, `VERSIONINFO`, file resources of any type (`PNG`,
`RCDATA`) and inline data (`DLGINIT`, `AFX_DIALOG_LAYOUT`).

## 3. Entry point

There is no `WinMain`. Write a `main()` that does what MFC's `AfxWinMain`
does, after creating a `QApplication`:

```cpp
#include <afxwin.h>
#include <QtWidgets/QApplication>

void RegisterMyAppResources();

int main(int argc, char **argv)
{
	QApplication qt(argc, argv);
	RegisterMyAppResources();

	CWinApp *app = AfxGetApp();  // the application's global CWinApp object
	if(app == nullptr)
		return 1;
	if(app->m_pszAppName == nullptr)
		app->m_pszAppName = _T("MyApp");
	if(app->m_pszExeName == nullptr)
		app->m_pszExeName = _T("myapp");
	if(!app->InitInstance())
	{
		const int code = app->ExitInstance();
		return code != 0 ? code : 1;
	}
	return app->Run();
}
```

`CWinApp::Run()` runs Qt's event loop with MFC's `PreTranslateMessage` and
`OnIdle` plugged in, and returns `ExitInstance()`'s result after `WM_QUIT`.
Modal dialogs, `TrackPopupMenu` and `DoModal` run nested loops the same way.

The application's `.cpp` files are compiled unchanged. Keep the Linux
`main()`, and anything else written only for Linux, in files of its own.

## 4. What is different from Windows

Most of these keep the application's source the same; a few may need a
change, usually behind `#ifdef _WIN32`.

**Types and strings.**
- `CString`, `TCHAR` and `LPTSTR` are wide (`wchar_t`), as in a `UNICODE`
  build. On Linux `wchar_t` is 32-bit (UTF-32), so code that assumes
  `sizeof(wchar_t) == 2`, or writes `wchar_t` strings to files and
  expects UTF-16, needs attention.
- `LONG`, `DWORD`, `ULONG` and `HRESULT` keep their Windows sizes
  (32-bit). Pointers and `LPARAM`, `WPARAM`, `LRESULT` and the `*_PTR`
  types are 64-bit.
- `long` is 64-bit on Linux, so `long` and `LONG` are different types:
  overloads and `printf` formats that mix them need care.

**Files.**
- File functions that open or change a path (`CreateFile`,
  `CreateDirectory`, `FindFirstFile`, `DeleteFile`, `LoadLibrary`) take
  native narrow paths: UTF-8, with `/`. Functions that fill or take text
  (`GetWindowText`, `LoadString`, `GetModuleFileName`) take `TCHAR`.
- Paths are case-sensitive and have no drive letters.
- `GetPrivateProfileString` and the other INI functions work as on
  Windows. Files are written as UTF-8; UTF-16 files copied from Windows are
  read too.
- `SHGetFolderPath` maps `CSIDL_APPDATA` to `$XDG_CONFIG_HOME` (or
  `~/.config`), `CSIDL_LOCAL_APPDATA` to `$XDG_DATA_HOME`, and
  `CSIDL_PERSONAL` to the documents folder.

**Windows facilities without a Linux counterpart.** These are declared with
their Windows values and fail the way Windows does when something is not
there, so the application takes its existing fallback paths:
- the registry (keys are never found);
- COM servers, shell links, DDE and OLE automation;
- structured exceptions (`__try`/`__except`);
- MFC's data recovery (`CDataRecoveryHandler`);
- compiled HTML help: `HtmlHelp` shows nothing unless the application sets
  a handler with `MfcQtSetHtmlHelpHandler()`, for example to open online
  help in the browser.

**Look and feel.**
- Controls are drawn by the Qt style, and `GetSysColor` follows the Qt
  palette, so a dark desktop theme gives dark windows. Custom drawing that
  uses system colours follows along; hard-coded colours do not.
- Dialog units are computed from the dialog's font as on Windows, with the
  fonts available on Linux. A dialog laid out tightly for Segoe UI or MS
  Sans Serif may need a few pixels more.
- The display scale (`QT_SCALE_FACTOR`, or the desktop's) is applied to
  whole windows, so GDI code keeps drawing in the pixels it expects.

**Windows outside the application.** There is no shared desktop window
tree. `GetDesktopWindow()` returns a handle whose rectangle is the primary
screen; `FindWindow` and `EnumWindows` see only the application's own
windows. Embedding another program's window (`SetParent` of a foreign
`HWND`) is possible only for X11 windows, with `MfcQtEmbedX11Window()`.

## 5. When something is missing

mfc-qt covers what its first application, OpenMPT, uses: a large part of
MFC and the Win32 user interface API, not all of it. A port shows what is
missing in three ways:

1. **Compile errors** for undeclared functions, constants and classes. Add
   the declaration to the matching header: Win32 to
   `include/mfcqt/win32_*.h` (`win32_user.h` for windows and messages,
   `win32_gdi.h` for drawing, ...), MFC to `include/mfcqt/mfc_*.h`. Take
   the values from the Windows SDK documentation.
2. **Link errors** for functions that are declared but not written. Link
   once with all of the application's objects to get the full list, then
   implement them: Win32 functions in `src/qtwin` (or `src/win32` for
   kernel32 and friends), MFC classes in `src/mfc`, using only Win32 calls,
   as MFC does.
3. **Behaviour** that differs at run time. Compare with Windows, write a
   test in `tests/` that shows the difference, then fix it.

Some rules keep the layers working like Windows:

- MFC code (`src/mfc`) uses the Win32 API only, never Qt directly. Qt is
  the business of `src/qtwin`.
- Windows behave the way Windows documents them: the order of messages,
  what `DefWindowProc` does, what a control sends its parent. MFC and the
  application depend on those details.
- An application-specific behaviour belongs in the application, behind a
  hook mfc-qt calls (`MfcQtSetHtmlHelpHandler`,
  `MfcQtSetSysColorProvider`, `MfcQtSetPreTranslateHook`), not in mfc-qt.

## 6. Testing

The tests run offscreen with `QT_QPA_PLATFORM=offscreen`, which needs no
display, and use Qt Test to send real input:

```sh
cmake -S mfc-qt -B build -G Ninja
cmake --build build
ctest --test-dir build
```

`tests/resources.rc` is a small resource script with each kind of
resource; `tests/resource_test.cpp` loads it through the Win32 and MFC
functions. An application can test its own windows the same way: create
them offscreen, send input with `QTest`, and check the messages
it receives, its state, and what it paints (`QWidget::grab()` on the
window's widget).
