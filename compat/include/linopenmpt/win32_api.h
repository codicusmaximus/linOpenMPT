// linOpenMPT compatibility layer: Win32 API functions and structures that
// OpenMPT's shared (non-UI) code calls directly.
//
// Functions here are declared only. The ones a Linux build actually reaches
// are implemented in compat/src (pure ones) or by the Qt application (ones
// that talk to windows, e.g. PostMessage to the main frame). An unresolved
// symbol at link time therefore marks exactly what still needs porting.

#pragma once

#include "linopenmpt/win32_types.h"

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

// ---- functions -----------------------------------------------------------
BSTR SysAllocString(const wchar_t *s);
void SysFreeString(BSTR s);
BOOL IsClipboardFormatAvailable(UINT format);
BOOL OpenClipboard(HWND owner);
BOOL CloseClipboard();
BOOL EmptyClipboard();
HANDLE GetClipboardData(UINT format);
HANDLE SetClipboardData(UINT format, HANDLE mem);
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
