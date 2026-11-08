// linOpenMPT compatibility layer: the few Win32 functions shared code calls.
#include "linopenmpt/win32_api.h"

#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <string>
#include <sys/sysinfo.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cerrno>
#include <ctime>
#include <fcntl.h>
#include <sys/syscall.h>

DWORD GetCurrentThreadId()
{
	return static_cast<DWORD>(::syscall(SYS_gettid));
}

DWORD GetCurrentProcessId()
{
	return static_cast<DWORD>(::getpid());
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

// ---- files ---------------------------------------------------------------
// File handles are POSIX file descriptors plus one, so that a valid handle is
// never null. CloseHandle only knows about these file handles.
HANDLE CreateFile(const char *path, DWORD access, DWORD share, void *security, DWORD disposition, DWORD flags, HANDLE templateFile)
{
	(void)share; (void)security; (void)flags; (void)templateFile;
	int oflags = 0;
	if((access & GENERIC_READ) && (access & GENERIC_WRITE))
		oflags = O_RDWR;
	else if(access & GENERIC_WRITE)
		oflags = O_WRONLY;
	else
		oflags = O_RDONLY;
	switch(disposition)
	{
	case CREATE_NEW: oflags |= O_CREAT | O_EXCL; break;
	case CREATE_ALWAYS: oflags |= O_CREAT | O_TRUNC; break;
	case OPEN_ALWAYS: oflags |= O_CREAT; break;
	case TRUNCATE_EXISTING: oflags |= O_TRUNC; break;
	default: break;
	}
	const int fd = ::open(path, oflags | O_CLOEXEC, 0666);
	if(fd < 0)
		return INVALID_HANDLE_VALUE;
	return reinterpret_cast<HANDLE>(static_cast<intptr_t>(fd) + 1);
}

HANDLE CreateFile(LPCWSTR path, DWORD access, DWORD share, void *security, DWORD disposition, DWORD flags, HANDLE templateFile)
{
	std::mbstate_t state{};
	const wchar_t *src = path;
	const std::size_t len = std::wcsrtombs(nullptr, &src, 0, &state);
	if(len == static_cast<std::size_t>(-1))
		return INVALID_HANDLE_VALUE;
	std::string narrow(len, '\0');
	src = path;
	state = {};
	std::wcsrtombs(narrow.data(), &src, len, &state);
	return CreateFile(narrow.c_str(), access, share, security, disposition, flags, templateFile);
}

BOOL CloseHandle(HANDLE handle)
{
	if(handle == nullptr || handle == INVALID_HANDLE_VALUE)
		return FALSE;
	return ::close(static_cast<int>(reinterpret_cast<intptr_t>(handle) - 1)) == 0 ? TRUE : FALSE;
}

BOOL DeleteFile(const char *path)
{
	return ::unlink(path) == 0 ? TRUE : FALSE;
}

BOOL MoveFile(const char *from, const char *to)
{
	// MoveFile fails if the target exists; rename() would replace it.
	if(::access(to, F_OK) == 0)
		return FALSE;
	return ::rename(from, to) == 0 ? TRUE : FALSE;
}

void Sleep(DWORD milliseconds)
{
	struct timespec ts;
	ts.tv_sec = milliseconds / 1000;
	ts.tv_nsec = static_cast<long>(milliseconds % 1000) * 1000000L;
	while(::nanosleep(&ts, &ts) != 0 && errno == EINTR) { }
}

// ---- global memory -------------------------------------------------------
// A block is a size header followed by the data; the handle points at the
// header. Locking just returns the data pointer.
namespace
{
struct GlobalBlock
{
	SIZE_T size;
	alignas(std::max_align_t) unsigned char data[1];
};
} // namespace

HGLOBAL GlobalAlloc(UINT flags, SIZE_T bytes)
{
	void *mem = (flags & GMEM_ZEROINIT) ? std::calloc(1, offsetof(GlobalBlock, data) + bytes) : std::malloc(offsetof(GlobalBlock, data) + bytes);
	if(mem == nullptr)
		return nullptr;
	static_cast<GlobalBlock *>(mem)->size = bytes;
	return mem;
}

HGLOBAL GlobalFree(HGLOBAL mem)
{
	std::free(mem);
	return nullptr;
}

LPVOID GlobalLock(HGLOBAL mem)
{
	return mem ? static_cast<GlobalBlock *>(mem)->data : nullptr;
}

BOOL GlobalUnlock(HGLOBAL mem)
{
	(void)mem;
	return FALSE;  // like Windows: FALSE once the lock count reaches 0
}

SIZE_T GlobalSize(HGLOBAL mem)
{
	return mem ? static_cast<GlobalBlock *>(mem)->size : 0;
}
