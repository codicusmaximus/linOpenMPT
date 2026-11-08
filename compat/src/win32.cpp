// linOpenMPT compatibility layer: the few Win32 functions shared code calls.
#include "linopenmpt/win32_api.h"

#include <chrono>
#include <cstdlib>
#include <string>
#include <sys/sysinfo.h>
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

BOOL GlobalMemoryStatusEx(MEMORYSTATUSEX *status)
{
	struct sysinfo info{};
	if(!status || ::sysinfo(&info) != 0)
		return FALSE;
	const DWORDLONG unit = info.mem_unit ? info.mem_unit : 1;
	status->dwMemoryLoad = info.totalram ? static_cast<DWORD>(100 - (100 * info.freeram) / info.totalram) : 0;
	status->ullTotalPhys = info.totalram * unit;
	status->ullAvailPhys = info.freeram * unit;
	status->ullTotalPageFile = (info.totalram + info.totalswap) * unit;
	status->ullAvailPageFile = (info.freeram + info.freeswap) * unit;
	status->ullTotalVirtual = status->ullTotalPhys;
	status->ullAvailVirtual = status->ullAvailPhys;
	status->ullAvailExtendedVirtual = 0;
	return TRUE;
}

DWORD GetSysColor(int index)
{
	switch(index)
	{
	case COLOR_WINDOW: return RGB(255, 255, 255);
	case COLOR_WINDOWTEXT: return RGB(0, 0, 0);
	case COLOR_HIGHLIGHT: return RGB(0, 120, 215);
	case COLOR_HIGHLIGHTTEXT: return RGB(255, 255, 255);
	case COLOR_BTNFACE: return RGB(240, 240, 240);
	case COLOR_BTNSHADOW: return RGB(160, 160, 160);
	case COLOR_GRAYTEXT: return RGB(109, 109, 109);
	case COLOR_BTNTEXT: return RGB(0, 0, 0);
	case COLOR_BTNHIGHLIGHT: return RGB(255, 255, 255);
	case COLOR_3DDKSHADOW: return RGB(105, 105, 105);
	case COLOR_3DLIGHT: return RGB(227, 227, 227);
	default: return RGB(0, 0, 0);
	}
}

static std::string XdgDir(const char *var, const char *fallbackBelowHome)
{
	if(const char *v = std::getenv(var); v && *v)
		return v;
	const char *home = std::getenv("HOME");
	return std::string(home ? home : "") + fallbackBelowHome;
}

HRESULT SHGetFolderPath(HWND, int folder, HANDLE, DWORD, LPWSTR path)
{
	std::string dir;
	switch(folder)
	{
	case CSIDL_LOCAL_APPDATA: dir = XdgDir("XDG_DATA_HOME", "/.local/share"); break;
	case CSIDL_APPDATA: dir = XdgDir("XDG_CONFIG_HOME", "/.config"); break;
	case CSIDL_PERSONAL: dir = XdgDir("XDG_DOCUMENTS_DIR", "/Documents"); break;
	default: return E_FAIL;
	}
	if(!path || dir.empty() || dir.size() >= MAX_PATH)
		return E_FAIL;
	// Paths are UTF-8 on Linux; decode to wide.
	std::mbstate_t state{};
	const char *src = dir.c_str();
	const std::size_t n = std::mbsrtowcs(path, &src, MAX_PATH - 1, &state);
	if(n == static_cast<std::size_t>(-1))
		return E_FAIL;
	path[n] = 0;
	return S_OK;
}

int MulDiv(int number, int numerator, int denominator)
{
	if(denominator == 0)
		return -1;
	const long long r = static_cast<long long>(number) * numerator;
	// Windows rounds half away from zero.
	const long long half = (denominator > 0 ? denominator : -denominator) / 2;
	const long long q = ((r >= 0) == (denominator > 0)) ? (r + (r >= 0 ? half : -half)) / denominator : (r - (r >= 0 ? half : -half)) / denominator;
	return static_cast<int>(q);
}

UINT midiInGetNumDevs()
{
	return 0;
}

MMRESULT midiInGetDevCaps(UINT_PTR, MIDIINCAPS *, UINT)
{
	return MMSYSERR_BADDEVICEID;
}
