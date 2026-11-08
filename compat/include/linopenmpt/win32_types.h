// linOpenMPT compatibility layer: Win32 base types, constants and macros.
//
// Upstream OpenMPT's tracker code uses Win32 integer and handle types even in
// its platform-neutral parts. This header supplies them with the *Windows*
// sizes (LONG/DWORD are 32-bit here, unlike Linux `long`), so data layouts
// and arithmetic behave exactly as on Windows.
//
// Only types and pure helpers live here. Anything that would need a real
// window system is deliberately absent; UI code is rewritten in Qt instead.

#pragma once

#include <cstddef>
#include <cstdint>
#include <cfloat>
#include <climits>
#include <cstring>
#include <cwchar>

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

// ---- integer types (Windows LLP64 sizes) ---------------------------------
using BYTE = std::uint8_t;
using UCHAR = std::uint8_t;
using CHAR = char;
using WCHAR = wchar_t;
using SHORT = std::int16_t;
using USHORT = std::uint16_t;
using WORD = std::uint16_t;
using INT = int;
using UINT = unsigned int;
using LONG = std::int32_t;
using ULONG = std::uint32_t;
using DWORD = std::uint32_t;
using LONGLONG = std::int64_t;
using ULONGLONG = std::uint64_t;
using DWORD64 = std::uint64_t;
using QWORD = std::uint64_t;
using INT_PTR = std::intptr_t;
using UINT_PTR = std::uintptr_t;
using LONG_PTR = std::intptr_t;
using ULONG_PTR = std::uintptr_t;
using DWORD_PTR = std::uintptr_t;
using SIZE_T = std::size_t;
using SSIZE_T = std::ptrdiff_t;
using BOOL = int;
using BOOLEAN = BYTE;
using FLOAT = float;
using HRESULT = LONG;
using LPARAM = LONG_PTR;
using WPARAM = UINT_PTR;
using LRESULT = LONG_PTR;
using ATOM = WORD;
using COLORREF = DWORD;
using LCID = DWORD;
using LANGID = WORD;
using FOURCC = DWORD;
using MMRESULT = UINT;

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

// ---- characters and strings ----------------------------------------------
using TCHAR = wchar_t;   // OpenMPT is built with UNICODE on Windows
using LPSTR = char *;
using LPCSTR = const char *;
using LPWSTR = wchar_t *;
using LPCWSTR = const wchar_t *;
using LPTSTR = TCHAR *;
using LPCTSTR = const TCHAR *;
using PCSTR = LPCSTR;
using PCWSTR = LPCWSTR;
using LPBYTE = BYTE *;
using LPWORD = WORD *;
using LPDWORD = DWORD *;
using LPLONG = LONG *;
using LPINT = int *;
using LPUINT = UINT *;
using LPVOID = void *;
using LPCVOID = const void *;
using PVOID = void *;

#ifndef _T
#define _T(x) L##x
#endif
#ifndef TEXT
#define TEXT(x) L##x
#endif

#ifndef MAX_PATH
#define MAX_PATH 260
#endif

// ---- opaque handles ------------------------------------------------------
// Distinct types (as with Windows STRICT) so overloads keep working.
#define LINOPENMPT_DECLARE_HANDLE(name) struct name##__ { int unused; }; using name = name##__ *
LINOPENMPT_DECLARE_HANDLE(HWND);
LINOPENMPT_DECLARE_HANDLE(HDC);
LINOPENMPT_DECLARE_HANDLE(HINSTANCE);
LINOPENMPT_DECLARE_HANDLE(HFONT);
LINOPENMPT_DECLARE_HANDLE(HBITMAP);
LINOPENMPT_DECLARE_HANDLE(HBRUSH);
LINOPENMPT_DECLARE_HANDLE(HPEN);
LINOPENMPT_DECLARE_HANDLE(HRGN);
LINOPENMPT_DECLARE_HANDLE(HMENU);
LINOPENMPT_DECLARE_HANDLE(HICON);
LINOPENMPT_DECLARE_HANDLE(HKEY);
LINOPENMPT_DECLARE_HANDLE(HMONITOR);
LINOPENMPT_DECLARE_HANDLE(HIMAGELIST);
LINOPENMPT_DECLARE_HANDLE(HGLOBAL__);
LINOPENMPT_DECLARE_HANDLE(HDROP);
LINOPENMPT_DECLARE_HANDLE(HMIDIIN);
LINOPENMPT_DECLARE_HANDLE(HMIDIOUT);
LINOPENMPT_DECLARE_HANDLE(HACCEL);
LINOPENMPT_DECLARE_HANDLE(HTREEITEM);
#undef LINOPENMPT_DECLARE_HANDLE
using HMODULE = HINSTANCE;
using HCURSOR = HICON;
using HGDIOBJ = void *;
using HANDLE = void *;
using HGLOBAL = HANDLE;
using HLOCAL = HANDLE;

#ifndef INVALID_HANDLE_VALUE
#define INVALID_HANDLE_VALUE (reinterpret_cast<HANDLE>(static_cast<LONG_PTR>(-1)))
#endif

// ---- geometry and colour -------------------------------------------------
struct POINT { LONG x; LONG y; };
struct SIZE { LONG cx; LONG cy; };
struct RECT { LONG left; LONG top; LONG right; LONG bottom; };
using LPPOINT = POINT *;
using LPSIZE = SIZE *;
using LPRECT = RECT *;
using LPCRECT = const RECT *;
using POINTS = POINT;

constexpr COLORREF RGB(BYTE r, BYTE g, BYTE b) noexcept
{
	return static_cast<COLORREF>(r) | (static_cast<COLORREF>(g) << 8) | (static_cast<COLORREF>(b) << 16);
}
constexpr BYTE GetRValue(COLORREF c) noexcept { return static_cast<BYTE>(c & 0xFF); }
constexpr BYTE GetGValue(COLORREF c) noexcept { return static_cast<BYTE>((c >> 8) & 0xFF); }
constexpr BYTE GetBValue(COLORREF c) noexcept { return static_cast<BYTE>((c >> 16) & 0xFF); }

struct RGBQUAD { BYTE rgbBlue; BYTE rgbGreen; BYTE rgbRed; BYTE rgbReserved; };

struct BITMAPINFOHEADER
{
	DWORD biSize;
	LONG biWidth;
	LONG biHeight;
	WORD biPlanes;
	WORD biBitCount;
	DWORD biCompression;
	DWORD biSizeImage;
	LONG biXPelsPerMeter;
	LONG biYPelsPerMeter;
	DWORD biClrUsed;
	DWORD biClrImportant;
};
struct BITMAPINFO { BITMAPINFOHEADER bmiHeader; RGBQUAD bmiColors[1]; };

#ifndef LF_FACESIZE
#define LF_FACESIZE 32
#endif
struct LOGFONTW
{
	LONG lfHeight;
	LONG lfWidth;
	LONG lfEscapement;
	LONG lfOrientation;
	LONG lfWeight;
	BYTE lfItalic;
	BYTE lfUnderline;
	BYTE lfStrikeOut;
	BYTE lfCharSet;
	BYTE lfOutPrecision;
	BYTE lfClipPrecision;
	BYTE lfQuality;
	BYTE lfPitchAndFamily;
	WCHAR lfFaceName[LF_FACESIZE];
};
using LOGFONT = LOGFONTW;

struct MSG
{
	HWND hwnd;
	UINT message;
	WPARAM wParam;
	LPARAM lParam;
	DWORD time;
	POINT pt;
};
using LPMSG = MSG *;

struct GUID
{
	std::uint32_t Data1;
	std::uint16_t Data2;
	std::uint16_t Data3;
	std::uint8_t Data4[8];
};
using IID = GUID;
using CLSID = GUID;
using REFGUID = const GUID &;
using REFIID = const IID &;
using REFCLSID = const CLSID &;
inline bool operator==(const GUID &a, const GUID &b) noexcept { return std::memcmp(&a, &b, sizeof(GUID)) == 0; }
inline bool operator!=(const GUID &a, const GUID &b) noexcept { return !(a == b); }

// ---- word/byte helpers ---------------------------------------------------
#define MAKEWORD(a, b) (static_cast<WORD>((static_cast<BYTE>(a)) | (static_cast<WORD>(static_cast<BYTE>(b)) << 8)))
#define MAKELONG(a, b) (static_cast<LONG>((static_cast<WORD>(a)) | (static_cast<DWORD>(static_cast<WORD>(b)) << 16)))
#define LOWORD(l) (static_cast<WORD>(static_cast<DWORD_PTR>(l) & 0xFFFF))
#define HIWORD(l) (static_cast<WORD>((static_cast<DWORD_PTR>(l) >> 16) & 0xFFFF))
#define LOBYTE(w) (static_cast<BYTE>(static_cast<DWORD_PTR>(w) & 0xFF))
#define HIBYTE(w) (static_cast<BYTE>((static_cast<DWORD_PTR>(w) >> 8) & 0xFF))
#define MAKELPARAM(l, h) (static_cast<LPARAM>(static_cast<DWORD>(MAKELONG(l, h))))
#define MAKEWPARAM(l, h) (static_cast<WPARAM>(static_cast<DWORD>(MAKELONG(l, h))))

#define S_OK ((HRESULT)0L)
#define S_FALSE ((HRESULT)1L)
#define E_FAIL ((HRESULT)0x80004005L)
#define SUCCEEDED(hr) (((HRESULT)(hr)) >= 0)
#define FAILED(hr) (((HRESULT)(hr)) < 0)

// ---- process priority classes (stored in OpenMPT's settings) -------------
#define IDLE_PRIORITY_CLASS 0x00000040
#define BELOW_NORMAL_PRIORITY_CLASS 0x00004000
#define NORMAL_PRIORITY_CLASS 0x00000020
#define ABOVE_NORMAL_PRIORITY_CLASS 0x00008000
#define HIGH_PRIORITY_CLASS 0x00000080
#define REALTIME_PRIORITY_CLASS 0x00000100

// ---- calling-convention and annotation macros: no-ops --------------------
#ifndef WINAPI
#define WINAPI
#endif
#ifndef CALLBACK
#define CALLBACK
#endif
#ifndef APIENTRY
#define APIENTRY
#endif
#ifndef STDMETHODCALLTYPE
#define STDMETHODCALLTYPE
#endif
#ifndef __stdcall
#define __stdcall
#endif
#ifndef __cdecl
#define __cdecl
#endif
#ifndef UNREFERENCED_PARAMETER
#define UNREFERENCED_PARAMETER(p) ((void)(p))
#endif

DWORD GetCurrentThreadId();
DWORD GetTickCount();
