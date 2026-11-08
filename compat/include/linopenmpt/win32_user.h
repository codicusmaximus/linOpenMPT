// linOpenMPT compatibility layer: Win32 USER (windows, menus, keyboard,
// mouse, scroll bars, message loop, resources), with Windows' values.
//
// Functions here are declared only. The UI layer implements them (on Qt in
// plan step 3), so every one upstream reaches is visible as a link error
// until it exists.

#pragma once

#include "win32_api.h"
#include "win32_messages.h"

#include <cstdarg>
#include <cwchar>

// ---- notification header and tooltips (shared by many controls) --------
struct NMHDR { HWND hwndFrom; UINT_PTR idFrom; UINT code; };
using LPNMHDR = NMHDR *;
struct TOOLINFO { UINT cbSize; UINT uFlags; HWND hwnd; UINT_PTR uId; RECT rect; HINSTANCE hinst; LPTSTR lpszText; LPARAM lParam; void *lpReserved; };
using TTTOOLINFO = TOOLINFO;

// ---- virtual keys --------------------------------------------------------
#define VK_LBUTTON 0x01
#define VK_RBUTTON 0x02
#define VK_CANCEL 0x03
#define VK_MBUTTON 0x04
#define VK_XBUTTON1 0x05
#define VK_XBUTTON2 0x06
#define VK_BACK 0x08
#define VK_TAB 0x09
#define VK_CLEAR 0x0C
#define VK_RETURN 0x0D
#define VK_SHIFT 0x10
#define VK_CONTROL 0x11
#define VK_MENU 0x12
#define VK_PAUSE 0x13
#define VK_CAPITAL 0x14
#define VK_KANA 0x15
#define VK_KANJI 0x19
#define VK_ESCAPE 0x1B
#define VK_CONVERT 0x1C
#define VK_NONCONVERT 0x1D
#define VK_SPACE 0x20
#define VK_PRIOR 0x21
#define VK_NEXT 0x22
#define VK_END 0x23
#define VK_HOME 0x24
#define VK_LEFT 0x25
#define VK_UP 0x26
#define VK_RIGHT 0x27
#define VK_DOWN 0x28
#define VK_SELECT 0x29
#define VK_PRINT 0x2A
#define VK_EXECUTE 0x2B
#define VK_SNAPSHOT 0x2C
#define VK_INSERT 0x2D
#define VK_DELETE 0x2E
#define VK_HELP 0x2F
#define VK_LWIN 0x5B
#define VK_RWIN 0x5C
#define VK_APPS 0x5D
#define VK_SLEEP 0x5F
#define VK_NUMPAD0 0x60
#define VK_NUMPAD1 0x61
#define VK_NUMPAD2 0x62
#define VK_NUMPAD3 0x63
#define VK_NUMPAD4 0x64
#define VK_NUMPAD5 0x65
#define VK_NUMPAD6 0x66
#define VK_NUMPAD7 0x67
#define VK_NUMPAD8 0x68
#define VK_NUMPAD9 0x69
#define VK_MULTIPLY 0x6A
#define VK_ADD 0x6B
#define VK_SEPARATOR 0x6C
#define VK_SUBTRACT 0x6D
#define VK_DECIMAL 0x6E
#define VK_DIVIDE 0x6F
#define VK_F1 0x70
#define VK_F2 0x71
#define VK_F3 0x72
#define VK_F4 0x73
#define VK_F5 0x74
#define VK_F6 0x75
#define VK_F7 0x76
#define VK_F8 0x77
#define VK_F9 0x78
#define VK_F10 0x79
#define VK_F11 0x7A
#define VK_F12 0x7B
#define VK_F13 0x7C
#define VK_F14 0x7D
#define VK_F15 0x7E
#define VK_F16 0x7F
#define VK_F17 0x80
#define VK_F18 0x81
#define VK_F19 0x82
#define VK_F20 0x83
#define VK_F21 0x84
#define VK_F22 0x85
#define VK_F23 0x86
#define VK_F24 0x87
#define VK_NUMLOCK 0x90
#define VK_SCROLL 0x91
#define VK_LSHIFT 0xA0
#define VK_RSHIFT 0xA1
#define VK_LCONTROL 0xA2
#define VK_RCONTROL 0xA3
#define VK_LMENU 0xA4
#define VK_RMENU 0xA5
#define VK_BROWSER_BACK 0xA6
#define VK_BROWSER_FORWARD 0xA7
#define VK_BROWSER_REFRESH 0xA8
#define VK_BROWSER_STOP 0xA9
#define VK_BROWSER_SEARCH 0xAA
#define VK_BROWSER_FAVORITES 0xAB
#define VK_BROWSER_HOME 0xAC
#define VK_VOLUME_MUTE 0xAD
#define VK_VOLUME_DOWN 0xAE
#define VK_VOLUME_UP 0xAF
#define VK_MEDIA_NEXT_TRACK 0xB0
#define VK_MEDIA_PREV_TRACK 0xB1
#define VK_MEDIA_STOP 0xB2
#define VK_MEDIA_PLAY_PAUSE 0xB3
#define VK_LAUNCH_MAIL 0xB4
#define VK_LAUNCH_MEDIA_SELECT 0xB5
#define VK_LAUNCH_APP1 0xB6
#define VK_LAUNCH_APP2 0xB7
#define VK_OEM_1 0xBA
#define VK_OEM_PLUS 0xBB
#define VK_OEM_COMMA 0xBC
#define VK_OEM_MINUS 0xBD
#define VK_OEM_PERIOD 0xBE
#define VK_OEM_2 0xBF
#define VK_OEM_3 0xC0
#define VK_OEM_4 0xDB
#define VK_OEM_5 0xDC
#define VK_OEM_6 0xDD
#define VK_OEM_7 0xDE
#define VK_OEM_8 0xDF
#define VK_OEM_102 0xE2
#define VK_PROCESSKEY 0xE5
#define VK_PACKET 0xE7
#define VK_ATTN 0xF6
#define VK_PLAY 0xFA
#define VK_ZOOM 0xFB
#define VK_OEM_CLEAR 0xFE

#define MAPVK_VK_TO_VSC 0
#define MAPVK_VSC_TO_VK 1
#define MAPVK_VK_TO_CHAR 2
#define MAPVK_VSC_TO_VK_EX 3
#define KEYEVENTF_EXTENDEDKEY 0x0001
#define KEYEVENTF_KEYUP 0x0002
#define KEYEVENTF_UNICODE 0x0004
#define KEYEVENTF_SCANCODE 0x0008
#define INPUT_MOUSE 0
#define INPUT_KEYBOARD 1
#define INPUT_HARDWARE 2

LINOPENMPT_DECLARE_HANDLE(HKL);
struct KEYBDINPUT { WORD wVk; WORD wScan; DWORD dwFlags; DWORD time; ULONG_PTR dwExtraInfo; };
struct MOUSEINPUT { LONG dx; LONG dy; DWORD mouseData; DWORD dwFlags; DWORD time; ULONG_PTR dwExtraInfo; };
struct HARDWAREINPUT { DWORD uMsg; WORD wParamL; WORD wParamH; };
struct INPUT
{
	DWORD type;
	union { MOUSEINPUT mi; KEYBDINPUT ki; HARDWAREINPUT hi; };
};

UINT MapVirtualKey(UINT code, UINT mapType);
UINT MapVirtualKeyEx(UINT code, UINT mapType, HKL layout);
SHORT VkKeyScanW(WCHAR ch);
SHORT VkKeyScanEx(TCHAR ch, HKL layout);
#define VkKeyScan VkKeyScanW
HKL GetKeyboardLayout(DWORD threadID);
int GetKeyboardLayoutList(int count, HKL *list);
BOOL GetKeyboardState(PBYTE keyState);
int GetKeyNameText(LONG lParam, LPTSTR buffer, int size);
int ToUnicodeEx(UINT vk, UINT scanCode, const BYTE *keyState, LPWSTR buf, int bufSize, UINT flags, HKL layout);
UINT SendInput(UINT count, INPUT *inputs, int size);
LPARAM GetMessageExtraInfo();
LPARAM SetMessageExtraInfo(LPARAM info);

// ---- mouse ---------------------------------------------------------------
#define MK_LBUTTON 0x0001
#define MK_RBUTTON 0x0002
#define MK_SHIFT 0x0004
#define MK_CONTROL 0x0008
#define MK_MBUTTON 0x0010
#define MK_XBUTTON1 0x0020
#define MK_XBUTTON2 0x0040
#define XBUTTON1 0x0001
#define XBUTTON2 0x0002
#define WHEEL_DELTA 120
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#define GET_WHEEL_DELTA_WPARAM(wp) ((short)HIWORD(wp))
#define GET_KEYSTATE_WPARAM(wp) (LOWORD(wp))
#define GET_XBUTTON_WPARAM(wp) (HIWORD(wp))

HWND SetCapture(HWND hwnd);
HWND GetCapture();
BOOL ReleaseCapture();
BOOL GetCursorPos(LPPOINT point);
BOOL SetCursorPos(int x, int y);
UINT GetDoubleClickTime();

// ---- cursors, icons, resources --------------------------------------------
LINOPENMPT_DECLARE_HANDLE(HRSRC);
#define MAKEINTRESOURCEW(i) ((LPWSTR)((ULONG_PTR)((WORD)(i))))
#define MAKEINTRESOURCE MAKEINTRESOURCEW
#define MAKEINTATOM(i) ((LPTSTR)((ULONG_PTR)((WORD)(i))))
#define IS_INTRESOURCE(r) ((((ULONG_PTR)(r)) >> 16) == 0)
#define IDC_ARROW MAKEINTRESOURCE(32512)
#define IDC_IBEAM MAKEINTRESOURCE(32513)
#define IDC_WAIT MAKEINTRESOURCE(32514)
#define IDC_CROSS MAKEINTRESOURCE(32515)
#define IDC_UPARROW MAKEINTRESOURCE(32516)
#define IDC_SIZENWSE MAKEINTRESOURCE(32642)
#define IDC_SIZENESW MAKEINTRESOURCE(32643)
#define IDC_SIZEWE MAKEINTRESOURCE(32644)
#define IDC_SIZENS MAKEINTRESOURCE(32645)
#define IDC_SIZEALL MAKEINTRESOURCE(32646)
#define IDC_NO MAKEINTRESOURCE(32648)
#define IDC_HAND MAKEINTRESOURCE(32649)
#define IDI_APPLICATION MAKEINTRESOURCE(32512)
#define IDI_HAND MAKEINTRESOURCE(32513)
#define IDI_ERROR IDI_HAND
#define IDI_QUESTION MAKEINTRESOURCE(32514)
#define IDI_EXCLAMATION MAKEINTRESOURCE(32515)
#define IDI_WARNING IDI_EXCLAMATION
#define IDI_ASTERISK MAKEINTRESOURCE(32516)
#define IDI_INFORMATION IDI_ASTERISK
#define IMAGE_BITMAP 0
#define IMAGE_ICON 1
#define IMAGE_CURSOR 2
#define LR_DEFAULTCOLOR 0x0000
#define LR_LOADFROMFILE 0x0010
#define LR_LOADTRANSPARENT 0x0020
#define LR_DEFAULTSIZE 0x0040
#define LR_LOADMAP3DCOLORS 0x1000
#define LR_CREATEDIBSECTION 0x2000
#define LR_SHARED 0x8000
#define RT_BITMAP MAKEINTRESOURCE(2)
#define RT_ICON MAKEINTRESOURCE(3)
#define RT_MENU MAKEINTRESOURCE(4)
#define RT_DIALOG MAKEINTRESOURCE(5)
#define RT_STRING MAKEINTRESOURCE(6)
#define RT_ACCELERATOR MAKEINTRESOURCE(9)
#define RT_RCDATA MAKEINTRESOURCE(10)
#define RT_GROUP_ICON MAKEINTRESOURCE(14)
#define RT_HTML MAKEINTRESOURCE(23)
#define RT_CURSOR MAKEINTRESOURCE(1)
#define RT_GROUP_CURSOR MAKEINTRESOURCE(12)
#define RT_VERSION MAKEINTRESOURCE(16)
#define RT_DLGINIT MAKEINTRESOURCE(240)
// MFC's dynamic layout data (afxres.h): a named resource type.
#define RT_DIALOG_LAYOUT _T("AFX_DIALOG_LAYOUT")

HCURSOR LoadCursor(HINSTANCE instance, LPCTSTR name);
HCURSOR SetCursor(HCURSOR cursor);
int ShowCursor(BOOL show);
// Mouse leave and hover notifications.
#define TME_HOVER 0x00000001
#define TME_LEAVE 0x00000002
#define TME_NONCLIENT 0x00000010
#define TME_QUERY 0x40000000
#define TME_CANCEL 0x80000000
#define HOVER_DEFAULT 0xFFFFFFFF
struct TRACKMOUSEEVENT { DWORD cbSize; DWORD dwFlags; HWND hwndTrack; DWORD dwHoverTime; };
using LPTRACKMOUSEEVENT = TRACKMOUSEEVENT *;
BOOL TrackMouseEvent(LPTRACKMOUSEEVENT event);
HWND GetNextDlgTabItem(HWND dialog, HWND control, BOOL previous);
HWND GetNextDlgGroupItem(HWND dialog, HWND control, BOOL previous);
HCURSOR GetCursor();
HICON LoadIcon(HINSTANCE instance, LPCTSTR name);
BOOL DestroyIcon(HICON icon);
BOOL DestroyCursor(HCURSOR cursor);
HANDLE LoadImage(HINSTANCE instance, LPCTSTR name, UINT type, int cx, int cy, UINT flags);
HRSRC FindResource(HMODULE module, LPCTSTR name, LPCTSTR type);
HGLOBAL LoadResource(HMODULE module, HRSRC resource);
LPVOID LockResource(HGLOBAL data);
DWORD SizeofResource(HMODULE module, HRSRC resource);
HMODULE GetModuleHandle(LPCTSTR name);
int LoadString(HINSTANCE instance, UINT id, LPTSTR buffer, int max);
HRSRC FindResourceEx(HMODULE module, LPCTSTR type, LPCTSTR name, WORD language);
BOOL FreeResource(HGLOBAL data);
HBITMAP LoadBitmap(HINSTANCE instance, LPCTSTR name);
// linOpenMPT: makes a compiled .res file (as tools/rc/rc.py writes it) the
// resources of a module, as they are linked into a Windows executable.
// GetModuleHandle(nullptr) is the executable.
void LinOpenMPTRegisterResources(HMODULE module, const void *res, size_t size);

// ---- menus ---------------------------------------------------------------
#define MF_INSERT 0x00000000L
#define MF_CHANGE 0x00000080L
#define MF_APPEND 0x00000100L
#define MF_DELETE 0x00000200L
#define MF_REMOVE 0x00001000L
#define MF_BYCOMMAND 0x00000000L
#define MF_BYPOSITION 0x00000400L
#define MF_SEPARATOR 0x00000800L
#define MF_ENABLED 0x00000000L
#define MF_GRAYED 0x00000001L
#define MF_DISABLED 0x00000002L
#define MF_UNCHECKED 0x00000000L
#define MF_CHECKED 0x00000008L
#define MF_USECHECKBITMAPS 0x00000200L
#define MF_STRING 0x00000000L
#define MF_BITMAP 0x00000004L
#define MF_OWNERDRAW 0x00000100L
#define MF_POPUP 0x00000010L
#define MF_MENUBARBREAK 0x00000020L
#define MF_MENUBREAK 0x00000040L
#define MF_UNHILITE 0x00000000L
#define MF_HILITE 0x00000080L
#define MF_DEFAULT 0x00001000L
#define MF_SYSMENU 0x00002000L
#define MF_HELP 0x00004000L
#define MF_RIGHTJUSTIFY 0x00004000L
#define MF_MOUSESELECT 0x00008000L
#define MFT_STRING MF_STRING
#define MFT_SEPARATOR MF_SEPARATOR
#define MFT_RADIOCHECK 0x00000200L
#define MFS_GRAYED 0x00000003L
#define MFS_DISABLED MFS_GRAYED
#define MFS_CHECKED MF_CHECKED
#define MFS_ENABLED MF_ENABLED
#define MFS_DEFAULT MF_DEFAULT
#define MIIM_STATE 0x00000001
#define MIIM_ID 0x00000002
#define MIIM_SUBMENU 0x00000004
#define MIIM_CHECKMARKS 0x00000008
#define MIIM_TYPE 0x00000010
#define MIIM_DATA 0x00000020
#define MIIM_STRING 0x00000040
#define MIIM_BITMAP 0x00000080
#define MIIM_FTYPE 0x00000100
#define TPM_LEFTBUTTON 0x0000L
#define TPM_RIGHTBUTTON 0x0002L
#define TPM_LEFTALIGN 0x0000L
#define TPM_CENTERALIGN 0x0004L
#define TPM_RIGHTALIGN 0x0008L
#define TPM_TOPALIGN 0x0000L
#define TPM_VCENTERALIGN 0x0010L
#define TPM_BOTTOMALIGN 0x0020L
#define TPM_HORIZONTAL 0x0000L
#define TPM_VERTICAL 0x0040L
#define TPM_NONOTIFY 0x0080L
#define TPM_RETURNCMD 0x0100L
#define TPM_NOANIMATION 0x4000L
struct TPMPARAMS { UINT cbSize; RECT rcExclude; };
using LPTPMPARAMS = TPMPARAMS *;
struct MENUITEMINFO
{
	UINT cbSize; UINT fMask; UINT fType; UINT fState; UINT wID; HMENU hSubMenu;
	HBITMAP hbmpChecked; HBITMAP hbmpUnchecked; ULONG_PTR dwItemData; LPTSTR dwTypeData; UINT cch; HBITMAP hbmpItem;
};
using LPMENUITEMINFO = MENUITEMINFO *;
using LPCMENUITEMINFO = const MENUITEMINFO *;
#define OBJID_WINDOW ((LONG)0x00000000)
#define OBJID_SYSMENU ((LONG)0xFFFFFFFF)
#define OBJID_CLIENT ((LONG)0xFFFFFFFC)
#define CHILDID_SELF 0
// Accessibility events (screen readers). The platform layer maps them to
// QAccessible events.
#define EVENT_OBJECT_FOCUS 0x8005
#define EVENT_OBJECT_SELECTION 0x8006
#define EVENT_OBJECT_NAMECHANGE 0x800C
#define EVENT_OBJECT_VALUECHANGE 0x800E
void NotifyWinEvent(DWORD event, HWND hwnd, LONG idObject, LONG idChild);
// Window hooks. The tracker uses a CBT hook to see focus changes.
#define WH_KEYBOARD 2
#define WH_CBT 5
#define WH_MOUSE 7
#define WH_KEYBOARD_LL 13
#define HCBT_MOVESIZE 0
#define HCBT_MINMAX 1
#define HCBT_CREATEWND 3
#define HCBT_DESTROYWND 4
#define HCBT_ACTIVATE 5
#define HCBT_SETFOCUS 9
struct CBT_CREATEWND { CREATESTRUCT *lpcs; HWND hwndInsertAfter; };
#define HC_ACTION 0
using HOOKPROC = LRESULT (*)(int code, WPARAM wParam, LPARAM lParam);
HHOOK SetWindowsHookEx(int idHook, HOOKPROC proc, HINSTANCE module, DWORD threadId);
BOOL UnhookWindowsHookEx(HHOOK hook);
LRESULT CallNextHookEx(HHOOK hook, int code, WPARAM wParam, LPARAM lParam);
#define OBJID_MENU ((LONG)0xFFFFFFFD)
struct MENUBARINFO { DWORD cbSize; RECT rcBar; HMENU hMenu; HWND hwndMenu; BOOL fBarFocused : 1; BOOL fFocused : 1; };

HMENU CreateMenu();
HMENU CreatePopupMenu();
BOOL DestroyMenu(HMENU menu);
HMENU LoadMenu(HINSTANCE instance, LPCTSTR name);
BOOL AppendMenu(HMENU menu, UINT flags, UINT_PTR id, LPCTSTR text);
BOOL InsertMenu(HMENU menu, UINT position, UINT flags, UINT_PTR id, LPCTSTR text);
BOOL InsertMenuItem(HMENU menu, UINT item, BOOL byPosition, LPCMENUITEMINFO info);
BOOL ModifyMenu(HMENU menu, UINT position, UINT flags, UINT_PTR id, LPCTSTR text);
BOOL RemoveMenu(HMENU menu, UINT position, UINT flags);
BOOL DeleteMenu(HMENU menu, UINT position, UINT flags);
DWORD CheckMenuItem(HMENU menu, UINT id, UINT check);
BOOL CheckMenuRadioItem(HMENU menu, UINT first, UINT last, UINT check, UINT flags);
BOOL EnableMenuItem(HMENU menu, UINT id, UINT enable);
UINT GetMenuState(HMENU menu, UINT id, UINT flags);
int GetMenuItemCount(HMENU menu);
UINT GetMenuItemID(HMENU menu, int pos);
HMENU GetSubMenu(HMENU menu, int pos);
int GetMenuString(HMENU menu, UINT id, LPTSTR text, int max, UINT flags);
BOOL GetMenuItemInfo(HMENU menu, UINT item, BOOL byPosition, LPMENUITEMINFO info);
BOOL SetMenuItemInfo(HMENU menu, UINT item, BOOL byPosition, LPCMENUITEMINFO info);
BOOL SetMenuDefaultItem(HMENU menu, UINT item, UINT byPos);
BOOL TrackPopupMenu(HMENU menu, UINT flags, int x, int y, int reserved, HWND hwnd, const RECT *rect);
BOOL TrackPopupMenuEx(HMENU menu, UINT flags, int x, int y, HWND hwnd, LPTPMPARAMS params);
HMENU GetMenu(HWND hwnd);
BOOL SetMenu(HWND hwnd, HMENU menu);
BOOL DrawMenuBar(HWND hwnd);
HMENU GetSystemMenu(HWND hwnd, BOOL revert);
BOOL GetMenuBarInfo(HWND hwnd, LONG object, LONG item, MENUBARINFO *info);

// ---- scroll bars ---------------------------------------------------------
#define SB_HORZ 0
#define SB_VERT 1
#define SB_CTL 2
#define SB_BOTH 3
#define SB_LINEUP 0
#define SB_LINELEFT 0
#define SB_LINEDOWN 1
#define SB_LINERIGHT 1
#define SB_PAGEUP 2
#define SB_PAGELEFT 2
#define SB_PAGEDOWN 3
#define SB_PAGERIGHT 3
#define SB_THUMBPOSITION 4
#define SB_THUMBTRACK 5
#define SB_TOP 6
#define SB_LEFT 6
#define SB_BOTTOM 7
#define SB_RIGHT 7
#define SB_ENDSCROLL 8
#define SIF_RANGE 0x0001
#define SIF_PAGE 0x0002
#define SIF_POS 0x0004
#define SIF_DISABLENOSCROLL 0x0008
#define SIF_TRACKPOS 0x0010
#define SIF_ALL (SIF_RANGE | SIF_PAGE | SIF_POS | SIF_TRACKPOS)
#define ESB_ENABLE_BOTH 0x0000
#define ESB_DISABLE_BOTH 0x0003
struct SCROLLINFO { UINT cbSize; UINT fMask; int nMin; int nMax; UINT nPage; int nPos; int nTrackPos; };
using LPSCROLLINFO = SCROLLINFO *;
using LPCSCROLLINFO = const SCROLLINFO *;
int SetScrollInfo(HWND hwnd, int bar, LPCSCROLLINFO info, BOOL redraw);
BOOL GetScrollInfo(HWND hwnd, int bar, LPSCROLLINFO info);
int GetScrollPos(HWND hwnd, int bar);
int SetScrollPos(HWND hwnd, int bar, int pos, BOOL redraw);
BOOL SetScrollRange(HWND hwnd, int bar, int minPos, int maxPos, BOOL redraw);
BOOL GetScrollRange(HWND hwnd, int bar, LPINT minPos, LPINT maxPos);
BOOL ShowScrollBar(HWND hwnd, int bar, BOOL show);
BOOL EnableScrollBar(HWND hwnd, UINT bar, UINT arrows);

// ---- windows ---------------------------------------------------------------
#define HWND_TOP ((HWND)0)
#define HWND_BOTTOM ((HWND)1)
#define HWND_TOPMOST ((HWND)-1)
#define HWND_NOTOPMOST ((HWND)-2)
#define HWND_DESKTOP ((HWND)0)
#define HWND_BROADCAST ((HWND)0xffff)
#define HWND_MESSAGE ((HWND)-3)
#define SWP_DRAWFRAME 0x0020
#define SWP_NOCOPYBITS 0x0100
#define SWP_NOREPOSITION 0x0200
#define SWP_NOSENDCHANGING 0x0400
#define SWP_DEFERERASE 0x2000
#define SWP_ASYNCWINDOWPOS 0x4000
#define SW_SCROLLCHILDREN 0x0001
#define SW_INVALIDATE 0x0002
#define SW_ERASE 0x0004
#define SW_SMOOTHSCROLL 0x0010
#define GW_HWNDFIRST 0
#define GW_HWNDLAST 1
#define GW_HWNDNEXT 2
#define GW_HWNDPREV 3
#define GW_OWNER 4
#define GW_CHILD 5
#define GW_ENABLEDPOPUP 6
#define GA_PARENT 1
#define GA_ROOT 2
#define GA_ROOTOWNER 3
#define WA_INACTIVE 0
#define WA_ACTIVE 1
#define WA_CLICKACTIVE 2
#define SIZE_RESTORED 0
#define SIZE_MINIMIZED 1
#define SIZE_MAXIMIZED 2
#define SIZE_MAXSHOW 3
#define SIZE_MAXHIDE 4
#define SC_SIZE 0xF000
#define SC_MOVE 0xF010
#define SC_MINIMIZE 0xF020
#define SC_MAXIMIZE 0xF030
#define SC_NEXTWINDOW 0xF040
#define SC_PREVWINDOW 0xF050
#define SC_CLOSE 0xF060
#define SC_KEYMENU 0xF100
#define SC_CONTEXTHELP 0xF180
#define SC_RESTORE 0xF120
#define HTERROR (-2)
#define HTTRANSPARENT (-1)
#define HTNOWHERE 0
#define HTCLIENT 1
#define HTCAPTION 2
#define HTSYSMENU 3
#define HTMENU 5
#define HTHSCROLL 6
#define HTVSCROLL 7
#define HTMINBUTTON 8
#define HTMAXBUTTON 9
#define HTLEFT 10
#define HTRIGHT 11
#define HTTOP 12
#define HTTOPLEFT 13
#define HTTOPRIGHT 14
#define HTBOTTOM 15
#define HTBOTTOMLEFT 16
#define HTBOTTOMRIGHT 17
#define HTBORDER 18
#define HTCLOSE 20
#define CW_USEDEFAULT ((int)0x80000000)
#define WS_EX_DLGMODALFRAME 0x00000001L
#define WS_EX_TOPMOST 0x00000008L
#define WS_EX_ACCEPTFILES 0x00000010L
#define WS_EX_TRANSPARENT 0x00000020L
#define WS_EX_MDICHILD 0x00000040L
#define WS_EX_WINDOWEDGE 0x00000100L
#define WS_EX_CONTEXTHELP 0x00000400L
#define WS_EX_CONTROLPARENT 0x00010000L
#define WS_EX_APPWINDOW 0x00040000L
#define WS_EX_LAYERED 0x00080000
#define WS_EX_NOACTIVATE 0x08000000L
#define SS_LEFT 0x00000000L
#define SS_CENTER 0x00000001L
#define SS_RIGHT 0x00000002L
#define SS_ICON 0x00000003L
#define SS_OWNERDRAW 0x0000000DL
#define SS_BITMAP 0x0000000EL
#define SS_ETCHEDHORZ 0x00000010L
#define SS_NOPREFIX 0x00000080L
#define SS_NOTIFY 0x00000100L
#define SS_CENTERIMAGE 0x00000200L
#define SS_SUNKEN 0x00001000L
#define BS_PUSHBUTTON 0x00000000L
#define BS_DEFPUSHBUTTON 0x00000001L
#define BS_CHECKBOX 0x00000002L
#define BS_AUTOCHECKBOX 0x00000003L
#define BS_RADIOBUTTON 0x00000004L
#define BS_3STATE 0x00000005L
#define BS_AUTO3STATE 0x00000006L
#define BS_GROUPBOX 0x00000007L
#define BS_AUTORADIOBUTTON 0x00000009L
#define BS_OWNERDRAW 0x0000000BL
#define BS_TYPEMASK 0x0000000FL
#define BS_LEFTTEXT 0x00000020L
#define BS_ICON 0x00000040L
#define BS_BITMAP 0x00000080L
#define BS_PUSHLIKE 0x00001000L
#define BS_MULTILINE 0x00002000L
#define BS_FLAT 0x00008000L
#define BS_SPLITBUTTON 0x0000000CL
#define CBS_SIMPLE 0x0001L
#define CBS_DROPDOWN 0x0002L
#define CBS_DROPDOWNLIST 0x0003L
#define CBS_OWNERDRAWFIXED 0x0010L
#define CBS_AUTOHSCROLL 0x0040L
#define CBS_SORT 0x0100L
#define CBS_HASSTRINGS 0x0200L
#define LBS_NOTIFY 0x0001L
#define LBS_SORT 0x0002L
#define LBS_OWNERDRAWFIXED 0x0010L
#define LBS_HASSTRINGS 0x0040L
#define LBS_MULTIPLESEL 0x0008L
#define LBS_EXTENDEDSEL 0x0800L
#define DS_MODALFRAME 0x80L
#define DS_SETFONT 0x40L
#define DS_CONTROL 0x0400L
#define DLGC_WANTARROWS 0x0001
#define DLGC_WANTTAB 0x0002
#define DLGC_WANTALLKEYS 0x0004
#define DLGC_WANTMESSAGE 0x0004
#define DLGC_HASSETSEL 0x0008
#define DLGC_DEFPUSHBUTTON 0x0010
#define DLGC_UNDEFPUSHBUTTON 0x0020
#define DLGC_RADIOBUTTON 0x0040
#define DLGC_WANTCHARS 0x0080
#define DLGC_STATIC 0x0100
#define DLGC_BUTTON 0x2000

// Control messages upstream sends directly.
#define EM_GETSEL 0x00B0
#define EM_SETSEL 0x00B1
#define EM_SCROLLCARET 0x00B7
#define EM_LINESCROLL 0x00B6
#define EM_GETMODIFY 0x00B8
#define EM_SETMODIFY 0x00B9
#define EM_GETLINECOUNT 0x00BA
#define EM_REPLACESEL 0x00C2
#define EM_LIMITTEXT 0x00C5
#define EM_SETLIMITTEXT EM_LIMITTEXT
#define EM_SETREADONLY 0x00CF
#define EM_GETFIRSTVISIBLELINE 0x00CE
#define BM_GETCHECK 0x00F0
#define BM_SETCHECK 0x00F1
#define BM_GETSTATE 0x00F2
#define BM_SETSTATE 0x00F3
#define BM_SETSTYLE 0x00F4
#define BM_CLICK 0x00F5
#define BM_SETIMAGE 0x00F7
#define CB_GETEDITSEL 0x0140
#define CB_LIMITTEXT 0x0141
#define CB_SETEDITSEL 0x0142
#define CB_ADDSTRING 0x0143
#define CB_DELETESTRING 0x0144
#define CB_GETCOUNT 0x0146
#define CB_GETCURSEL 0x0147
#define CB_GETLBTEXT 0x0148
#define CB_GETLBTEXTLEN 0x0149
#define CB_INSERTSTRING 0x014A
#define CB_RESETCONTENT 0x014B
#define CB_FINDSTRING 0x014C
#define CB_SELECTSTRING 0x014D
#define CB_SETCURSEL 0x014E
#define CB_SHOWDROPDOWN 0x014F
#define CB_GETITEMDATA 0x0150
#define CB_SETITEMDATA 0x0151
#define CB_GETDROPPEDSTATE 0x0157
#define CB_FINDSTRINGEXACT 0x0158
#define LB_ADDSTRING 0x0180
#define LB_INSERTSTRING 0x0181
#define LB_DELETESTRING 0x0182
#define LB_RESETCONTENT 0x0184
#define LB_SETSEL 0x0185
#define LB_SETCURSEL 0x0186
#define LB_GETSEL 0x0187
#define LB_GETCURSEL 0x0188
#define LB_GETTEXT 0x0189
#define LB_GETCOUNT 0x018B
#define LB_GETITEMDATA 0x0199
#define LB_SETITEMDATA 0x019A
#define LB_ITEMFROMPOINT 0x01A9
#define STM_SETICON 0x0170
#define STM_SETIMAGE 0x0172
#define WM_COPYDATA 0x004A
struct COPYDATASTRUCT { ULONG_PTR dwData; DWORD cbData; PVOID lpData; };
using PCOPYDATASTRUCT = COPYDATASTRUCT *;

using WNDPROC = LRESULT (*)(HWND, UINT, WPARAM, LPARAM);
using WNDENUMPROC = BOOL (*)(HWND, LPARAM);
struct WNDCLASS
{
	UINT style; WNDPROC lpfnWndProc; int cbClsExtra; int cbWndExtra; HINSTANCE hInstance;
	HICON hIcon; HCURSOR hCursor; HBRUSH hbrBackground; LPCTSTR lpszMenuName; LPCTSTR lpszClassName;
};
#define CS_VREDRAW 0x0001
#define CS_HREDRAW 0x0002
#define CS_DBLCLKS 0x0008
#define CS_OWNDC 0x0020
#define CS_NOCLOSE 0x0200
ATOM RegisterClass(const WNDCLASS *wc);
BOOL UnregisterClass(LPCTSTR className, HINSTANCE instance);
HWND CreateWindowEx(DWORD exStyle, LPCTSTR className, LPCTSTR windowName, DWORD style, int x, int y, int w, int h, HWND parent, HMENU menu, HINSTANCE instance, LPVOID param);
#define CreateWindow(c, n, s, x, y, w, h, p, m, i, l) CreateWindowEx(0, c, n, s, x, y, w, h, p, m, i, l)
BOOL DestroyWindow(HWND hwnd);
LRESULT DefWindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
LRESULT CallWindowProc(WNDPROC proc, HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
HWND GetDlgItem(HWND dlg, int id);
int GetDlgCtrlID(HWND hwnd);
UINT GetDlgItemText(HWND dlg, int id, LPTSTR text, int max);
BOOL SetDlgItemText(HWND dlg, int id, LPCTSTR text);
UINT IsDlgButtonChecked(HWND dlg, int id);
BOOL CheckDlgButton(HWND dlg, int id, UINT check);
LRESULT SendDlgItemMessage(HWND dlg, int id, UINT msg, WPARAM wp, LPARAM lp);
BOOL EnableWindow(HWND hwnd, BOOL enable);
BOOL IsWindowEnabled(HWND hwnd);
BOOL ShowWindow(HWND hwnd, int cmd);
BOOL IsWindowVisible(HWND hwnd);
BOOL IsIconic(HWND hwnd);
BOOL IsZoomed(HWND hwnd);
BOOL IsChild(HWND parent, HWND hwnd);
HWND GetParent(HWND hwnd);
HWND SetParent(HWND child, HWND parent);
HWND GetAncestor(HWND hwnd, UINT flags);
HWND GetWindow(HWND hwnd, UINT cmd);
HWND GetTopWindow(HWND hwnd);
HWND GetDesktopWindow();
HWND GetForegroundWindow();
BOOL SetForegroundWindow(HWND hwnd);
HWND SetActiveWindow(HWND hwnd);
HWND WindowFromPoint(POINT point);
HWND ChildWindowFromPoint(HWND parent, POINT point);
HWND FindWindow(LPCTSTR className, LPCTSTR windowName);
BOOL EnumChildWindows(HWND parent, WNDENUMPROC proc, LPARAM lParam);
BOOL EnumWindows(WNDENUMPROC proc, LPARAM lParam);
DWORD GetWindowThreadProcessId(HWND hwnd, LPDWORD processID);
int GetClassName(HWND hwnd, LPTSTR className, int max);
int GetWindowText(HWND hwnd, LPTSTR text, int max);
int GetWindowTextA(HWND hwnd, LPSTR text, int max);
int GetWindowTextLength(HWND hwnd);
int GetWindowTextLengthA(HWND hwnd);
BOOL SetWindowText(HWND hwnd, LPCTSTR text);
BOOL MoveWindow(HWND hwnd, int x, int y, int w, int h, BOOL repaint);
BOOL SetWindowPos(HWND hwnd, HWND after, int x, int y, int cx, int cy, UINT flags);
BOOL GetWindowPlacement(HWND hwnd, WINDOWPLACEMENT *placement);
BOOL SetWindowPlacement(HWND hwnd, const WINDOWPLACEMENT *placement);
BOOL ScreenToClient(HWND hwnd, LPPOINT point);
BOOL ClientToScreen(HWND hwnd, LPPOINT point);
int MapWindowPoints(HWND from, HWND to, LPPOINT points, UINT count);
BOOL InvalidateRect(HWND hwnd, const RECT *rect, BOOL erase);
BOOL ValidateRect(HWND hwnd, const RECT *rect);
BOOL GetUpdateRect(HWND hwnd, LPRECT rect, BOOL erase);
BOOL UpdateWindow(HWND hwnd);
BOOL RedrawWindow(HWND hwnd, const RECT *update, HRGN region, UINT flags);
int ScrollWindowEx(HWND hwnd, int dx, int dy, const RECT *scroll, const RECT *clip, HRGN update, LPRECT updateRect, UINT flags);
LRESULT SendMessageW(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
BOOL PostThreadMessage(DWORD threadID, UINT msg, WPARAM wp, LPARAM lp);
void PostQuitMessage(int exitCode);
UINT RegisterWindowMessage(LPCTSTR name);
typedef void (*TIMERPROC)(HWND, UINT, UINT_PTR, DWORD);
UINT_PTR SetTimer(HWND hwnd, UINT_PTR id, UINT elapse, void *proc);
BOOL KillTimer(HWND hwnd, UINT_PTR id);
BOOL FlashWindow(HWND hwnd, BOOL invert);
#define RDW_INVALIDATE 0x0001
#define RDW_INTERNALPAINT 0x0002
#define RDW_ERASE 0x0004
#define RDW_VALIDATE 0x0008
#define RDW_NOERASE 0x0020
#define RDW_ALLCHILDREN 0x0080
#define RDW_UPDATENOW 0x0100
#define RDW_ERASENOW 0x0200
#define RDW_FRAME 0x0400
#define RDW_NOFRAME 0x0800

LINOPENMPT_DECLARE_HANDLE(HDWP);
HDWP BeginDeferWindowPos(int count);
HDWP DeferWindowPos(HDWP info, HWND hwnd, HWND after, int x, int y, int cx, int cy, UINT flags);
BOOL EndDeferWindowPos(HDWP info);

// ---- message loop --------------------------------------------------------
#define PM_NOREMOVE 0x0000
#define PM_REMOVE 0x0001
#define PM_NOYIELD 0x0002
#define PM_QS_INPUT 0x00070000
#define PM_QS_POSTMESSAGE 0x00980000
#define PM_QS_PAINT 0x00200000
#define PM_QS_SENDMESSAGE 0x00400000
BOOL PeekMessage(LPMSG msg, HWND hwnd, UINT filterMin, UINT filterMax, UINT remove);
BOOL GetMessage(LPMSG msg, HWND hwnd, UINT filterMin, UINT filterMax);
BOOL TranslateMessage(const MSG *msg);
LRESULT DispatchMessage(const MSG *msg);
BOOL IsDialogMessage(HWND dlg, LPMSG msg);
// linOpenMPT extensions, for MFC's message loop on top of Qt's. Input
// reaches windows from Qt's event loop rather than from GetMessage(), so a
// message loop hands over its pre-translation step and idle processing:
//  - hook sees every input and posted message before it is dispatched, as
//    PreTranslateMessage() does in a Win32 message loop (TRUE: handled);
//  - LinOpenMPTRunMessageLoop() runs until WM_QUIT and returns its exit
//    code, calling idle(count) with count 0, 1, ... whenever no messages
//    are waiting, until it returns FALSE.
void LinOpenMPTSetPreTranslateHook(BOOL (*hook)(MSG *msg));
int LinOpenMPTRunMessageLoop(BOOL (*idle)(LONG count));
// Runs a nested loop until LinOpenMPTEndModalLoop() (modal dialogs and
// menus), with the same pre-translation and idle processing.
int LinOpenMPTRunModalLoop(BOOL (*idle)(LONG count));
void LinOpenMPTEndModalLoop(int result);
int TranslateAccelerator(HWND hwnd, HACCEL accel, LPMSG msg);

// ---- system parameters -------------------------------------------------------
#define SPI_GETWORKAREA 0x0030
#define SPI_GETNONCLIENTMETRICS 0x0029
#define SPI_GETKEYBOARDDELAY 0x0016
#define SPI_GETKEYBOARDSPEED 0x000A
#define SPI_GETWHEELSCROLLLINES 0x0068
#define SPI_GETWHEELSCROLLCHARS 0x006C
#define SPI_GETFLATMENU 0x1022
#define SPI_GETHIGHCONTRAST 0x0042
struct NONCLIENTMETRICS
{
	UINT cbSize; int iBorderWidth; int iScrollWidth; int iScrollHeight; int iCaptionWidth; int iCaptionHeight;
	LOGFONT lfCaptionFont; int iSmCaptionWidth; int iSmCaptionHeight; LOGFONT lfSmCaptionFont;
	int iMenuWidth; int iMenuHeight; LOGFONT lfMenuFont; LOGFONT lfStatusFont; LOGFONT lfMessageFont; int iPaddedBorderWidth;
};
BOOL SystemParametersInfo(UINT action, UINT param, PVOID data, UINT winIni);
HBRUSH GetSysColorBrush(int index);
#define COLOR_SCROLLBAR 0
#define COLOR_BACKGROUND 1
#define COLOR_ACTIVECAPTION 2
#define COLOR_INACTIVECAPTION 3
#define COLOR_MENU 4
#define COLOR_WINDOWFRAME 6
#define COLOR_MENUTEXT 7
#define COLOR_CAPTIONTEXT 9
#define COLOR_ACTIVEBORDER 10
#define COLOR_INACTIVEBORDER 11
#define COLOR_APPWORKSPACE 12
#define COLOR_INACTIVECAPTIONTEXT 19
#define COLOR_INFOTEXT 23
#define COLOR_INFOBK 24
#define COLOR_HOTLIGHT 26
#define COLOR_GRADIENTACTIVECAPTION 27
#define COLOR_GRADIENTINACTIVECAPTION 28
#define COLOR_MENUHILIGHT 29
#define COLOR_MENUBAR 30
#define COLOR_DESKTOP COLOR_BACKGROUND
#define COLOR_3DFACE COLOR_BTNFACE
#define COLOR_3DSHADOW COLOR_BTNSHADOW
#define COLOR_3DHIGHLIGHT COLOR_BTNHIGHLIGHT
#define COLOR_3DHILIGHT COLOR_BTNHIGHLIGHT
#define COLOR_BTNHILIGHT COLOR_BTNHIGHLIGHT

// ---- strings -------------------------------------------------------------
// wsprintf: printf-style formatting into a buffer, with MSVC format
// semantics (%s is the TCHAR string), via the CString formatter.
int wvsprintfW(LPWSTR buf, LPCWSTR format, va_list args);
int wsprintfW(LPWSTR buf, LPCWSTR format, ...);
#define wsprintf wsprintfW
#define wvsprintf wvsprintfW
int lstrcmpi(LPCWSTR a, LPCWSTR b);
int lstrcmpiA(LPCSTR a, LPCSTR b);
int lstrcmp(LPCWSTR a, LPCWSTR b);
int lstrlen(LPCWSTR s);
#define NORM_IGNORECASE 0x00000001
#define NORM_IGNORENONSPACE 0x00000002
#define NORM_IGNORESYMBOLS 0x00000004
#define NORM_IGNOREKANATYPE 0x00010000
#define NORM_IGNOREWIDTH 0x00020000
#define CSTR_LESS_THAN 1
#define CSTR_EQUAL 2
#define CSTR_GREATER_THAN 3
int CompareString(LCID locale, DWORD flags, LPCWSTR a, int lenA, LPCWSTR b, int lenB);
inline int _tstoi(const wchar_t *s) { return static_cast<int>(std::wcstol(s, nullptr, 10)); }
inline double _tstof(const wchar_t *s) { return std::wcstod(s, nullptr); }
#define _ttof _tstof
// MSVC's swprintf without a size. Upstream passes arrays, whose size is known.
template <std::size_t size, typename... Args>
inline int _stprintf(wchar_t (&buffer)[size], const wchar_t *format, Args... args)
{
	return std::swprintf(buffer, size, format, args...);
}
#define _ttoi _tstoi

// ---- clipboard formats -----------------------------------------------------
#define CF_BITMAP 2
#define CF_DIB 8
#define CF_WAVE 12
#define CF_HDROP 15
UINT RegisterClipboardFormat(LPCTSTR name);
BOOL IsClipboardFormatAvailable(UINT format);

// ---- drag and drop of files ------------------------------------------------
UINT DragQueryFile(HDROP drop, UINT index, LPTSTR file, UINT size);
BOOL DragQueryPoint(HDROP drop, LPPOINT point);
void DragFinish(HDROP drop);
void DragAcceptFiles(HWND hwnd, BOOL accept);
