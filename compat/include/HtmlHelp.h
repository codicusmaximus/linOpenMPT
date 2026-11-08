// linOpenMPT compatibility layer: <HtmlHelp.h>. OpenMPT's help is online;
// the platform layer opens it in the browser.
#pragma once
#include "linopenmpt/win32_api.h"
#define HH_DISPLAY_TOPIC 0x0000
#define HH_DISPLAY_TOC 0x0001
#define HH_DISPLAY_INDEX 0x0002
#define HH_DISPLAY_SEARCH 0x0003
#define HH_HELP_CONTEXT 0x000F
#define HH_CLOSE_ALL 0x0012
#define HH_INITIALIZE 0x001C
#define HH_UNINITIALIZE 0x001D
HWND HtmlHelp(HWND caller, LPCTSTR file, UINT command, DWORD_PTR data);
