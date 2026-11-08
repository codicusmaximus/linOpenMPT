// linOpenMPT compatibility layer: the few Win32 functions shared code calls.
#include "linopenmpt/win32_types.h"

#include <chrono>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/syscall.h>

DWORD GetCurrentThreadId()
{
	return static_cast<DWORD>(::syscall(SYS_gettid));
}

DWORD GetTickCount()
{
	using namespace std::chrono;
	return static_cast<DWORD>(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

BOOL CreateDirectory(const char *path, void *)
{
	return ::mkdir(path, 0777) == 0 ? TRUE : FALSE;
}
