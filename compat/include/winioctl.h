// linOpenMPT compatibility layer: <winioctl.h>. The tree view's drive list
// checks removable drives for media; Linux has no drive letters, so the
// platform layer lists mount points instead (step 4).
#pragma once
#include "linopenmpt/win32_api.h"
#define IOCTL_STORAGE_CHECK_VERIFY 0x002D4800
#define IOCTL_STORAGE_CHECK_VERIFY2 0x002D0800
struct OVERLAPPED;
using LPOVERLAPPED = OVERLAPPED *;
BOOL DeviceIoControl(HANDLE device, DWORD code, LPVOID in, DWORD inSize, LPVOID out, DWORD outSize, LPDWORD returned, LPOVERLAPPED overlapped);
