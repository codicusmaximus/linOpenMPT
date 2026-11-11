// linOpenMPT compatibility layer: Win32 API functions and structures that
// OpenMPT's shared (non-UI) code calls directly.
//
// Functions here are declared only. The ones a Linux build actually reaches
// are implemented in compat/src (pure ones) or by the Qt application (ones
// that talk to windows, e.g. PostMessage to the main frame). An unresolved
// symbol at link time therefore marks exactly what still needs porting.

#pragma once

#include "linopenmpt/win32_types.h"

#include <cerrno>
#include <cstdlib>

// ---- structures ----------------------------------------------------------
struct WINDOWPLACEMENT
{
	UINT length;
	UINT flags;
	UINT showCmd;
	POINT ptMinPosition;
	POINT ptMaxPosition;
	RECT rcNormalPosition;
};
struct CREATESTRUCT
{
	LPVOID lpCreateParams;
	HINSTANCE hInstance;
	HMENU hMenu;
	HWND hwndParent;
	int cy;
	int cx;
	int y;
	int x;
	LONG style;
	LPCTSTR lpszName;
	LPCTSTR lpszClass;
	DWORD dwExStyle;
};
using LPCREATESTRUCT = CREATESTRUCT *;
struct WINDOWPOS { HWND hwnd; HWND hwndInsertAfter; int x; int y; int cx; int cy; UINT flags; };
struct NCCALCSIZE_PARAMS { RECT rgrc[3]; WINDOWPOS *lppos; };
struct MIDIHDR
{
	LPSTR lpData;
	DWORD dwBufferLength;
	DWORD dwBytesRecorded;
	DWORD_PTR dwUser;
	DWORD dwFlags;
	MIDIHDR *lpNext;
	DWORD_PTR reserved;
	DWORD dwOffset;
	DWORD_PTR dwReserved[8];
};
struct HHOOK__ { int unused; };
using HHOOK = HHOOK__ *;

// ---- constants -----------------------------------------------------------
#define VK_BACK 0x08
#define VK_TAB 0x09
#define VK_RETURN 0x0D
#define VK_SHIFT 0x10
#define VK_CONTROL 0x11
#define VK_MENU 0x12
#define VK_ESCAPE 0x1B
#define VK_SPACE 0x20
#define VK_LSHIFT 0xA0
#define VK_RSHIFT 0xA1
#define VK_LCONTROL 0xA2
#define VK_RCONTROL 0xA3
#define VK_LMENU 0xA4
#define VK_RMENU 0xA5
#define LOCALE_USER_DEFAULT 0x0400
#define LOCALE_SDECIMAL 0x0000000E
#define LOCALE_SNEGATIVESIGN 0x00000051
#define CBRS_ALIGN_LEFT 0x1000L
#define CBRS_ALIGN_TOP 0x2000L
#define CBRS_ALIGN_RIGHT 0x4000L
#define CBRS_ALIGN_BOTTOM 0x8000L
#define CBRS_ALIGN_ANY 0xF000L

// ---- COM automation / accessibility types --------------------------------
using BSTR = wchar_t *;
struct VARIANT { WORD vt; LONG lVal; };
#define CHILDID_SELF 0
#define CF_TEXT 1
#define CF_UNICODETEXT 13
#define _CVTBUFSIZE (309 + 40)
// MSVC's secure ecvt, on glibc's ecvt_r. Returns 0 on success.
inline int _ecvt_s(char *buffer, size_t size, double value, int count, int *dec, int *sign)
{
	return ::ecvt_r(value, count, dec, sign, buffer, size) == 0 ? 0 : EINVAL;
}
template <size_t size>
inline int _ecvt_s(char (&buffer)[size], double value, int count, int *dec, int *sign)
{
	return _ecvt_s(buffer, size, value, count, dec, sign);
}

// ---- system information ------------------------------------------------------
using DWORDLONG = std::uint64_t;
#ifndef SIZE_T_MAX
#define SIZE_T_MAX SIZE_MAX
#endif
struct MEMORYSTATUSEX
{
	DWORD dwLength;
	DWORD dwMemoryLoad;
	DWORDLONG ullTotalPhys;
	DWORDLONG ullAvailPhys;
	DWORDLONG ullTotalPageFile;
	DWORDLONG ullAvailPageFile;
	DWORDLONG ullTotalVirtual;
	DWORDLONG ullAvailVirtual;
	DWORDLONG ullAvailExtendedVirtual;
};
BOOL GlobalMemoryStatusEx(MEMORYSTATUSEX *status);

// System colours (default Windows 10 values; the Qt UI uses its palette).
#define COLOR_WINDOW 5
#define COLOR_WINDOWTEXT 8
#define COLOR_HIGHLIGHT 13
#define COLOR_HIGHLIGHTTEXT 14
#define COLOR_BTNFACE 15
#define COLOR_BTNSHADOW 16
#define COLOR_GRAYTEXT 17
#define COLOR_BTNTEXT 18
#define COLOR_BTNHIGHLIGHT 20
#define COLOR_3DDKSHADOW 21
#define COLOR_3DLIGHT 22
DWORD GetSysColor(int index);
// Where GetSysColor() gets its colours from (the Qt palette, in the UI).
void LinOpenMPTSetSysColorProvider(DWORD (*provider)(int index));

// Known folders. Maps to XDG directories on Linux.
#define CSIDL_PERSONAL 0x0005
#define CSIDL_MYDOCUMENTS CSIDL_PERSONAL
#define CSIDL_APPDATA 0x001a
#define CSIDL_LOCAL_APPDATA 0x001c
#define SHGFP_TYPE_CURRENT 0
HRESULT SHGetFolderPath(HWND owner, int folder, HANDLE token, DWORD flags, LPWSTR path);

int MulDiv(int number, int numerator, int denominator);

// ---- MIDI input (winmm). No devices until the MIDI milestone. --------------
#define MAXPNAMELEN 32
#define MMSYSERR_NOERROR 0
#define MMSYSERR_BADDEVICEID 2
#define MMSYSERR_ALLOCATED 4
#define MMSYSERR_INVALHANDLE 5
#define MMSYSERR_INVALFLAG 10
#define MMSYSERR_INVALPARAM 11
#define MIDIERR_UNPREPARED 64
#define MIDIERR_STILLPLAYING 65
struct MIDIINCAPS
{
	WORD wMid;
	WORD wPid;
	UINT vDriverVersion;
	WCHAR szPname[MAXPNAMELEN];
	DWORD dwSupport;
};
UINT midiInGetNumDevs();
MMRESULT midiInGetDevCaps(UINT_PTR id, MIDIINCAPS *caps, UINT size);
// MIDI input. The platform layer implements these on the ALSA sequencer.
#define CALLBACK_NULL 0x00000000L
#define CALLBACK_WINDOW 0x00010000L
#define CALLBACK_THREAD 0x00020000L
#define CALLBACK_FUNCTION 0x00030000L
#define MIM_OPEN 0x3C1
#define MIM_CLOSE 0x3C2
#define MIM_DATA 0x3C3
#define MIM_LONGDATA 0x3C4
#define MIM_ERROR 0x3C5
#define MIM_LONGERROR 0x3C6
#define MIM_MOREDATA 0x3CC
#define MHDR_DONE 0x00000001
#define MHDR_PREPARED 0x00000002
#define MHDR_INQUEUE 0x00000004
using LPMIDIHDR = MIDIHDR *;
using LPHMIDIIN = HMIDIIN *;
MMRESULT midiInOpen(LPHMIDIIN handle, UINT deviceID, DWORD_PTR callback, DWORD_PTR instance, DWORD flags);
MMRESULT midiInClose(HMIDIIN handle);
MMRESULT midiInStart(HMIDIIN handle);
MMRESULT midiInStop(HMIDIIN handle);
MMRESULT midiInReset(HMIDIIN handle);
MMRESULT midiInPrepareHeader(HMIDIIN handle, LPMIDIHDR header, UINT size);
MMRESULT midiInUnprepareHeader(HMIDIIN handle, LPMIDIHDR header, UINT size);
MMRESULT midiInAddBuffer(HMIDIIN handle, LPMIDIHDR header, UINT size);

// ---- TCHAR string routines -------------------------------------------------
inline std::size_t _tcslen(const wchar_t *s) { return std::wcslen(s); }
inline wchar_t *_tcsdup(const wchar_t *s) { return ::wcsdup(s); }
inline wchar_t *_tcscat(wchar_t *d, const wchar_t *s) { return std::wcscat(d, s); }
inline int _tcsncmp(const wchar_t *a, const wchar_t *b, std::size_t n) { return std::wcsncmp(a, b, n); }
template <typename... Args>
inline int _sntprintf(wchar_t *buffer, std::size_t count, const wchar_t *format, Args... args)
{
	return std::swprintf(buffer, count, format, args...);
}
inline int _tcscmp(const wchar_t *a, const wchar_t *b) { return std::wcscmp(a, b); }
inline int _tcsicmp(const wchar_t *a, const wchar_t *b) { return ::wcscasecmp(a, b); }
inline int _tcsncicmp(const wchar_t *a, const wchar_t *b, std::size_t n) { return ::wcsncasecmp(a, b, n); }
inline wchar_t *_tcscpy(wchar_t *d, const wchar_t *s) { return std::wcscpy(d, s); }
inline wchar_t *_tcsncpy(wchar_t *d, const wchar_t *s, std::size_t n) { return std::wcsncpy(d, s, n); }
inline const wchar_t *_tcschr(const wchar_t *s, wchar_t c) { return std::wcschr(s, c); }
inline const wchar_t *_tcsrchr(const wchar_t *s, wchar_t c) { return std::wcsrchr(s, c); }
inline long _tcstol(const wchar_t *s, wchar_t **end, int base) { return std::wcstol(s, end, base); }
inline double _tcstod(const wchar_t *s, wchar_t **end) { return std::wcstod(s, end); }
inline int _ttoi(const wchar_t *s) { return static_cast<int>(std::wcstol(s, nullptr, 10)); }
inline wchar_t *_wcsdup(const wchar_t *s) { return ::wcsdup(s); }

// ---- functions -----------------------------------------------------------
BSTR SysAllocString(const wchar_t *s);
void SysFreeString(BSTR s);
BOOL IsClipboardFormatAvailable(UINT format);
BOOL OpenClipboard(HWND owner);
BOOL CloseClipboard();
BOOL EmptyClipboard();
HANDLE GetClipboardData(UINT format);
HANDLE SetClipboardData(UINT format, HANDLE mem);
// Global memory blocks, as used for clipboard data (compat/src/win32.cpp).
#define GMEM_FIXED 0x0000
#define GMEM_MOVEABLE 0x0002
#define GMEM_ZEROINIT 0x0040
#define GMEM_DDESHARE 0x2000
#define GHND (GMEM_MOVEABLE | GMEM_ZEROINIT)
#define GPTR (GMEM_FIXED | GMEM_ZEROINIT)
LPVOID GlobalLock(HGLOBAL mem);
BOOL GlobalUnlock(HGLOBAL mem);
HGLOBAL GlobalAlloc(UINT flags, SIZE_T bytes);
HGLOBAL GlobalFree(HGLOBAL mem);
SIZE_T GlobalSize(HGLOBAL mem);
SHORT GetAsyncKeyState(int vKey);
SHORT GetKeyState(int vKey);
BOOL PostMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
LRESULT SendMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
DWORD timeGetTime();
int GetLocaleInfo(LCID locale, DWORD type, LPTSTR data, int size);
BOOL IsWindow(HWND hwnd);
// Native paths are narrow (UTF-8/locale) on Linux.
BOOL CreateDirectory(const char *path, void *securityAttributes);
// Creates every missing folder of the path (mkdir -p). Returns ERROR_SUCCESS,
// or ERROR_ALREADY_EXISTS when the folder was there already.
int SHCreateDirectoryEx(HWND owner, const char *path, const void *securityAttributes);
// Shared libraries (dlopen). Plugins are native .so files on Linux.
using FARPROC = intptr_t (*)();
HMODULE LoadLibrary(const char *path);
FARPROC GetProcAddress(HMODULE module, const char *name);
BOOL FreeLibrary(HMODULE module);
// Why the last LoadLibrary failed, as dlerror() put it.
const char *LinOpenMPTLastLoadLibraryError();
#ifndef _MAX_PATH
#define _MAX_PATH 260
#endif
BOOL CopyFile(const char *from, const char *to, BOOL failIfExists);
BOOL RemoveDirectory(const char *path);
BOOL SetCurrentDirectory(const char *path);
// Buffer-returning functions write TCHAR (wide) text, as with UNICODE.
DWORD GetCurrentDirectory(DWORD size, LPWSTR buffer);
DWORD ExpandEnvironmentStrings(LPCWSTR src, LPWSTR dst, DWORD size);
DWORD GetLastError();
void SetLastError(DWORD error);
#define ERROR_SUCCESS 0L
#define ERROR_FILE_NOT_FOUND 2L
#define ERROR_PATH_NOT_FOUND 3L
#define ERROR_MOD_NOT_FOUND 126L
#define ERROR_DLL_INIT_FAILED 1114L
#define ERROR_BAD_EXE_FORMAT 193L
#define ERROR_INVALID_FUNCTION 1L
#define ERROR_ACCESS_DENIED 5L
#define ERROR_INVALID_HANDLE 6L
#define ERROR_NO_MORE_FILES 18L
#define ERROR_NOT_READY 21L
#define ERROR_INVALID_PARAMETER 87L
#define ERROR_INVALID_FLAGS 1004L
#define ERROR_ALREADY_EXISTS 183L
#define ERROR_INSUFFICIENT_BUFFER 122L
#define ERROR_MORE_DATA 234L
#define ERROR_CANCELLED 1223L

// ---- events and waiting ----------------------------------------------------
#define INFINITE 0xFFFFFFFF
#define WAIT_OBJECT_0 0x00000000L
#define WAIT_ABANDONED 0x00000080L
#define WAIT_TIMEOUT 0x00000102L
#define WAIT_FAILED ((DWORD)0xFFFFFFFF)
HANDLE CreateEvent(void *securityAttributes, BOOL manualReset, BOOL initialState, LPCWSTR name);
BOOL SetEvent(HANDLE event);
BOOL ResetEvent(HANDLE event);
DWORD WaitForSingleObject(HANDLE handle, DWORD milliseconds);
DWORD WaitForMultipleObjects(DWORD count, const HANDLE *handles, BOOL waitAll, DWORD milliseconds);

// Directory change notifications (inotify in the platform layer).
#define FILE_NOTIFY_CHANGE_FILE_NAME 0x00000001
#define FILE_NOTIFY_CHANGE_DIR_NAME 0x00000002
#define FILE_NOTIFY_CHANGE_ATTRIBUTES 0x00000004
#define FILE_NOTIFY_CHANGE_SIZE 0x00000008
#define FILE_NOTIFY_CHANGE_LAST_WRITE 0x00000010
HANDLE FindFirstChangeNotification(const char *path, BOOL watchSubtree, DWORD filter);
BOOL FindNextChangeNotification(HANDLE handle);
BOOL FindCloseChangeNotification(HANDLE handle);

// Drives. Linux has no drive letters: every letter is DRIVE_NO_ROOT_DIR.
#define DRIVE_UNKNOWN 0
#define DRIVE_NO_ROOT_DIR 1
#define DRIVE_REMOVABLE 2
#define DRIVE_FIXED 3
#define DRIVE_REMOTE 4
#define DRIVE_CDROM 5
#define DRIVE_RAMDISK 6
UINT GetDriveType(LPCWSTR rootPath);

// ---- processes -------------------------------------------------------------
[[noreturn]] void ExitProcess(UINT exitCode);
BOOL IsDebuggerPresent();
HANDLE GetCurrentProcess();
BOOL SetPriorityClass(HANDLE process, DWORD priorityClass);
DWORD GetPriorityClass(HANDLE process);

// Structured exception handling. GCC has no SEH: __try is libstdc++'s own
// macro for `try`, and __except catches C++ exceptions only. Crashes are
// handled by the platform layer's signal handler instead.
#ifndef __try
#define __try try
#endif
#define __except(filter) catch(...)
struct _EXCEPTION_POINTERS;
_EXCEPTION_POINTERS *GetExceptionInformation();
#define EXCEPTION_EXECUTE_HANDLER 1
#define EXCEPTION_CONTINUE_SEARCH 0
#define EXCEPTION_CONTINUE_EXECUTION (-1)

// ---- registry --------------------------------------------------------------
// Linux has no registry: the platform layer opens no keys, so callers take
// their fallback paths.
using PHKEY = HKEY *;
using REGSAM = DWORD;
using LSTATUS = LONG;
#define HKEY_CLASSES_ROOT ((HKEY)(ULONG_PTR)((LONG)0x80000000))
#define HKEY_CURRENT_USER ((HKEY)(ULONG_PTR)((LONG)0x80000001))
#define HKEY_LOCAL_MACHINE ((HKEY)(ULONG_PTR)((LONG)0x80000002))
#define KEY_QUERY_VALUE 0x0001
#define KEY_SET_VALUE 0x0002
#define KEY_READ 0x20019
#define KEY_WRITE 0x20006
#define KEY_WOW64_64KEY 0x0100
#define KEY_WOW64_32KEY 0x0200
#define REG_NONE 0
#define REG_SZ 1
#define REG_EXPAND_SZ 2
#define REG_BINARY 3
#define REG_DWORD 4
LSTATUS RegOpenKeyEx(HKEY key, LPCWSTR subKey, DWORD options, REGSAM desired, PHKEY result);
LSTATUS RegQueryValueEx(HKEY key, LPCWSTR valueName, LPDWORD reserved, LPDWORD type, LPBYTE data, LPDWORD size);
LSTATUS RegCloseKey(HKEY key);
BOOL IsTextUnicode(const void *buf, int size, LPINT result);

// INI files (compat/src/win32_profile.cpp). Text is wide as on Windows with
// UNICODE; file names are native (narrow) paths.
DWORD GetPrivateProfileString(LPCWSTR section, LPCWSTR key, LPCWSTR def, LPWSTR out, DWORD size, const char *file);
UINT GetPrivateProfileInt(LPCWSTR section, LPCWSTR key, INT def, const char *file);
BOOL GetPrivateProfileStruct(LPCWSTR section, LPCWSTR key, LPVOID data, UINT size, const char *file);
BOOL WritePrivateProfileString(LPCWSTR section, LPCWSTR key, LPCWSTR value, const char *file);
BOOL WritePrivateProfileStruct(LPCWSTR section, LPCWSTR key, LPVOID data, UINT size, const char *file);
BOOL WritePrivateProfileSection(LPCWSTR section, LPCWSTR data, const char *file);

// ---- files (compat/src/win32.cpp) ----------------------------------------
// Only what upstream uses: create/open a file to set attributes, and the
// rename/delete helpers. Paths are native (narrow).
#define GENERIC_READ 0x80000000u
#define GENERIC_WRITE 0x40000000u
#define FILE_SHARE_READ 0x00000001
#define FILE_SHARE_WRITE 0x00000002
#define FILE_SHARE_DELETE 0x00000004
#define CREATE_NEW 1
#define CREATE_ALWAYS 2
#define OPEN_EXISTING 3
#define OPEN_ALWAYS 4
#define TRUNCATE_EXISTING 5
#define FILE_ATTRIBUTE_READONLY 0x00000001
#define FILE_ATTRIBUTE_HIDDEN 0x00000002
#define FILE_ATTRIBUTE_SYSTEM 0x00000004
#define FILE_ATTRIBUTE_ARCHIVE 0x00000020
#define FILE_ATTRIBUTE_OFFLINE 0x00001000
#define FILE_READ_ATTRIBUTES 0x0080
#define FILE_ATTRIBUTE_DIRECTORY 0x00000010
#define FILE_ATTRIBUTE_NORMAL 0x00000080
#define FILE_ATTRIBUTE_TEMPORARY 0x00000100
#ifndef INVALID_HANDLE_VALUE
#define INVALID_HANDLE_VALUE (reinterpret_cast<HANDLE>(static_cast<LONG_PTR>(-1)))
#endif
HANDLE CreateFile(const char *path, DWORD access, DWORD share, void *security, DWORD disposition, DWORD flags, HANDLE templateFile);
// Wide (TCHAR) paths are encoded as the locale (UTF-8) expects.
HANDLE CreateFile(LPCWSTR path, DWORD access, DWORD share, void *security, DWORD disposition, DWORD flags, HANDLE templateFile);
BOOL CloseHandle(HANDLE handle);
BOOL DeleteFile(const char *path);
BOOL MoveFile(const char *from, const char *to);
void Sleep(DWORD milliseconds);

// ---- atomics -------------------------------------------------------------
inline long InterlockedExchange(volatile long *target, long value) { return __atomic_exchange_n(target, value, __ATOMIC_SEQ_CST); }
inline LONG InterlockedExchange(volatile LONG *target, LONG value) { return __atomic_exchange_n(target, value, __ATOMIC_SEQ_CST); }
inline long InterlockedIncrement(volatile long *target) { return __atomic_add_fetch(target, 1, __ATOMIC_SEQ_CST); }
inline long InterlockedDecrement(volatile long *target) { return __atomic_sub_fetch(target, 1, __ATOMIC_SEQ_CST); }

// ---- windows (UI layer) --------------------------------------------------
// Declared here, implemented by the UI layer (headless for now in
// platform/UiBoundary.cpp, Qt later).
HWND GetActiveWindow();
HWND GetFocus();
HWND SetFocus(HWND hwnd);
BOOL GetClientRect(HWND hwnd, LPRECT rect);
BOOL GetWindowRect(HWND hwnd, LPRECT rect);
BOOL SendNotifyMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
int GetSystemMetrics(int index);

// Windows taskbar progress (COM). Not available on Linux; upstream only
// holds a pointer, which stays null.
struct ITaskbarList3;
