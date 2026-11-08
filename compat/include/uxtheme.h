// linOpenMPT compatibility layer: <uxtheme.h> (visual styles). Qt draws
// native-looking controls itself, so these only exist to compile.
#pragma once
#include "linopenmpt/win32_api.h"
HRESULT SetWindowTheme(HWND hwnd, LPCWSTR subAppName, LPCWSTR subIdList);
BOOL IsAppThemed();
BOOL IsThemeActive();
#define ETDT_DISABLE 0x00000001
#define ETDT_ENABLE 0x00000002
#define ETDT_USETABTEXTURE 0x00000004
#define ETDT_ENABLETAB (ETDT_ENABLE | ETDT_USETABTEXTURE)
HRESULT EnableThemeDialogTexture(HWND hwnd, DWORD flags);
