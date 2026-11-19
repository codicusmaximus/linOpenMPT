// mfc-qt: <HtmlHelp.h>. There is no compiled HTML help (.chm) viewer on
// Linux: an application that has its help elsewhere (say, online) sets a
// handler that shows it; without one, HtmlHelp shows nothing.
#pragma once
#include "mfcqt/win32_api.h"
#define HH_DISPLAY_TOPIC 0x0000
#define HH_DISPLAY_TOC 0x0001
#define HH_DISPLAY_INDEX 0x0002
#define HH_DISPLAY_SEARCH 0x0003
#define HH_HELP_CONTEXT 0x000F
#define HH_CLOSE_ALL 0x0012
#define HH_INITIALIZE 0x001C
#define HH_UNINITIALIZE 0x001D
HWND HtmlHelp(HWND caller, LPCTSTR file, UINT command, DWORD_PTR data);

// Shows the help topic HtmlHelp was asked for ("Manual.chm::/Page.html>Window"
// in file, as on Windows); returns whether it did.
void MfcQtSetHtmlHelpHandler(BOOL (*handler)(LPCWSTR file, UINT command, DWORD_PTR data));
