// linOpenMPT compatibility layer: ATL string conversion macros (<atlconv.h>).
// TCHAR and OLECHAR are both wide here, as with UNICODE on Windows.
#pragma once
#include "linopenmpt/win32_types.h"
#define USES_CONVERSION ((void)0)
#define T2COLE(s) (static_cast<LPCWSTR>(s))
#define T2OLE(s) (static_cast<LPWSTR>(s))
#define OLE2CT(s) (static_cast<LPCWSTR>(s))
#define OLE2T(s) (static_cast<LPWSTR>(s))
#define T2CW(s) (static_cast<LPCWSTR>(s))
#define W2CT(s) (static_cast<LPCWSTR>(s))
