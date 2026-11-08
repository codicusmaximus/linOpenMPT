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

// ---- TCHAR string routines -------------------------------------------------
inline std::size_t _tcslen(const wchar_t *s) { return std::wcslen(s); }
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
BOOL CopyFile(const char *from, const char *to, BOOL failIfExists);
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
#define FILE_ATTRIBUTE_DIRECTORY 0x00000010
#define FILE_ATTRIBUTE_NORMAL 0x00000080
#define FILE_ATTRIBUTE_TEMPORARY 0x00000100
#ifndef INVALID_HANDLE_VALUE
#define INVALID_HANDLE_VALUE (reinterpret_cast<HANDLE>(static_cast<LONG_PTR>(-1)))
#endif
HANDLE CreateFile(const char *path, DWORD access, DWORD share, void *security, DWORD disposition, DWORD flags, HANDLE templateFile);
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
