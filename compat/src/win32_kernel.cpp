// linOpenMPT compatibility layer: kernel32 pieces on POSIX. Events and
// waiting, directory change notifications, file search, the environment,
// errors, processes and threads, the registry (there is none), time, and
// string functions.
//
// Waitable handles are file descriptors plus one, like the file handles of
// win32.cpp: an event is an eventfd, a change notification an inotify
// descriptor, so WaitForMultipleObjects() is poll() and CloseHandle() works
// on all of them.
#include "linopenmpt/win32_api.h"
#include "linopenmpt/win32_shell.h"
#include "winioctl.h"
#include "linopenmpt/mfc_cstring.h"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <climits>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <cwctype>
#include <mutex>
#include <set>
#include <string>
#include <vector>

#include <dirent.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <poll.h>
#include <strings.h>
#include <sys/eventfd.h>
#include <sys/inotify.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace
{

thread_local DWORD t_lastError = ERROR_SUCCESS;

int ToFd(HANDLE handle)
{
	if(handle == nullptr || handle == INVALID_HANDLE_VALUE)
		return -1;
	return static_cast<int>(reinterpret_cast<intptr_t>(handle) - 1);
}

HANDLE FromFd(int fd)
{
	return reinterpret_cast<HANDLE>(static_cast<intptr_t>(fd) + 1);
}

// Events that stay set until ResetEvent(); the others reset when a wait
// takes them.
std::mutex g_eventMutex;
std::set<int> g_manualResetEvents;
std::set<int> g_autoResetEvents;

enum class Kind { Other, ManualEvent, AutoEvent };

Kind KindOf(int fd)
{
	std::lock_guard<std::mutex> lock(g_eventMutex);
	if(g_manualResetEvents.count(fd))
		return Kind::ManualEvent;
	if(g_autoResetEvents.count(fd))
		return Kind::AutoEvent;
	return Kind::Other;
}

// An auto-reset event is taken by the wait that sees it; another waiter may
// have taken it first.
bool Take(int fd)
{
	if(KindOf(fd) != Kind::AutoEvent)
		return true;
	eventfd_t value = 0;
	return ::eventfd_read(fd, &value) == 0;
}

std::string Narrow(const wchar_t *s)
{
	if(s == nullptr)
		return std::string();
	std::mbstate_t state{};
	const wchar_t *src = s;
	const std::size_t len = std::wcsrtombs(nullptr, &src, 0, &state);
	if(len == static_cast<std::size_t>(-1))
		return std::string();
	std::string out(len, '\0');
	src = s;
	state = {};
	std::wcsrtombs(out.data(), &src, len, &state);
	return out;
}

std::wstring Wide(const char *s)
{
	if(s == nullptr)
		return std::wstring();
	std::mbstate_t state{};
	const char *src = s;
	const std::size_t len = std::mbsrtowcs(nullptr, &src, 0, &state);
	if(len == static_cast<std::size_t>(-1))
	{
		// Not valid in the locale: byte by byte.
		std::wstring out;
		for(const char *p = s; *p; p++)
			out += static_cast<wchar_t>(static_cast<unsigned char>(*p));
		return out;
	}
	std::wstring out(len, L'\0');
	src = s;
	state = {};
	std::mbsrtowcs(out.data(), &src, len, &state);
	return out;
}

// Copies a result into a caller's buffer the way the Get*/Expand* functions
// do: the length without the terminator when it fits, else the size needed.
DWORD CopyOut(const std::wstring &text, LPWSTR buffer, DWORD size, bool countTerminatorWhenFits)
{
	const DWORD needed = static_cast<DWORD>(text.size() + 1);
	if(buffer == nullptr || size < needed)
		return needed;
	std::wmemcpy(buffer, text.c_str(), text.size() + 1);
	return countTerminatorWhenFits ? needed : needed - 1;
}

void FillFileTime(FILETIME &ft, const struct timespec &ts)
{
	// 100-nanosecond intervals since 1601-01-01.
	const unsigned long long t = (static_cast<unsigned long long>(ts.tv_sec) + 11644473600ULL) * 10000000ULL + static_cast<unsigned long long>(ts.tv_nsec) / 100;
	ft.dwLowDateTime = static_cast<DWORD>(t & 0xFFFFFFFF);
	ft.dwHighDateTime = static_cast<DWORD>(t >> 32);
}

// A file search: the directory, the name pattern, and where it got to.
struct FindState
{
	DIR *dir = nullptr;
	std::string path;
	std::string pattern;
};

bool NextMatch(FindState &state, LPWIN32_FIND_DATA data)
{
	while(struct dirent *entry = ::readdir(state.dir))
	{
		if(::fnmatch(state.pattern.c_str(), entry->d_name, FNM_PERIOD & 0) != 0)
			continue;
		*data = WIN32_FIND_DATA{};
		const std::string full = state.path + "/" + entry->d_name;
		struct stat st{};
		if(::stat(full.c_str(), &st) == 0)
		{
			data->dwFileAttributes = S_ISDIR(st.st_mode) ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
			if(entry->d_name[0] == '.' && std::strcmp(entry->d_name, ".") && std::strcmp(entry->d_name, ".."))
				data->dwFileAttributes |= FILE_ATTRIBUTE_HIDDEN;
			if(::access(full.c_str(), W_OK) != 0)
				data->dwFileAttributes |= FILE_ATTRIBUTE_READONLY;
			FillFileTime(data->ftCreationTime, st.st_ctim);
			FillFileTime(data->ftLastAccessTime, st.st_atim);
			FillFileTime(data->ftLastWriteTime, st.st_mtim);
			data->nFileSizeHigh = static_cast<DWORD>(static_cast<unsigned long long>(st.st_size) >> 32);
			data->nFileSizeLow = static_cast<DWORD>(static_cast<unsigned long long>(st.st_size) & 0xFFFFFFFF);
		}
		const std::wstring name = Wide(entry->d_name);
		std::wcsncpy(data->cFileName, name.c_str(), MAX_PATH - 1);
		return true;
	}
	return false;
}

// Case-insensitive wildcard match (* and ?), as PathMatchSpec does it.
bool WildcardMatch(const wchar_t *s, const wchar_t *p)
{
	const wchar_t *star = nullptr, *afterStar = nullptr;
	while(*s)
	{
		if(*p == L'*')
		{
			star = p++;
			afterStar = s;
		} else if(*p == L'?' || (*p && std::towlower(*p) == std::towlower(*s)))
		{
			p++;
			s++;
		} else if(star)
		{
			p = star + 1;
			s = ++afterStar;
		} else
		{
			return false;
		}
	}
	while(*p == L'*')
		p++;
	return *p == 0;
}

} // namespace

// The handle registry win32.cpp's CloseHandle() clears.
void LinOpenMPTForgetHandle(int fd)
{
	std::lock_guard<std::mutex> lock(g_eventMutex);
	g_manualResetEvents.erase(fd);
	g_autoResetEvents.erase(fd);
}

// ---- errors ------------------------------------------------------------------

DWORD GetLastError()
{
	return t_lastError;
}

void SetLastError(DWORD error)
{
	t_lastError = error;
}

// ---- events and waiting ----------------------------------------------------

HANDLE CreateEvent(void *, BOOL manualReset, BOOL initialState, LPCWSTR)
{
	const int fd = ::eventfd(initialState ? 1 : 0, EFD_NONBLOCK | EFD_CLOEXEC);
	if(fd < 0)
	{
		SetLastError(ERROR_ACCESS_DENIED);
		return nullptr;
	}
	std::lock_guard<std::mutex> lock(g_eventMutex);
	(manualReset ? g_manualResetEvents : g_autoResetEvents).insert(fd);
	return FromFd(fd);
}

BOOL SetEvent(HANDLE event)
{
	const int fd = ToFd(event);
	return fd >= 0 && ::eventfd_write(fd, 1) == 0;
}

BOOL ResetEvent(HANDLE event)
{
	const int fd = ToFd(event);
	if(fd < 0)
		return FALSE;
	eventfd_t value = 0;
	::eventfd_read(fd, &value);
	return TRUE;
}

DWORD WaitForMultipleObjects(DWORD count, const HANDLE *handles, BOOL waitAll, DWORD milliseconds)
{
	if(count == 0 || handles == nullptr)
	{
		SetLastError(ERROR_INVALID_HANDLE);
		return WAIT_FAILED;
	}
	std::vector<struct pollfd> fds(count);
	for(DWORD i = 0; i < count; i++)
	{
		fds[i].fd = ToFd(handles[i]);
		fds[i].events = POLLIN;
		if(fds[i].fd < 0)
		{
			SetLastError(ERROR_INVALID_HANDLE);
			return WAIT_FAILED;
		}
	}
	using clock = std::chrono::steady_clock;
	const auto deadline = clock::now() + std::chrono::milliseconds(milliseconds == INFINITE ? 0 : milliseconds);
	for(;;)
	{
		int timeout = -1;
		if(milliseconds != INFINITE)
		{
			const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - clock::now()).count();
			timeout = static_cast<int>(std::clamp<long long>(left, 0, INT_MAX));
		}
		for(auto &p : fds)
			p.revents = 0;
		const int ready = ::poll(fds.data(), count, timeout);
		if(ready < 0)
		{
			if(errno == EINTR)
				continue;
			SetLastError(ERROR_INVALID_HANDLE);
			return WAIT_FAILED;
		}
		if(ready == 0)
		{
			if(timeout == 0 || (milliseconds != INFINITE && clock::now() >= deadline))
				return WAIT_TIMEOUT;
			continue;
		}
		if(waitAll)
		{
			const bool all = std::all_of(fds.begin(), fds.end(), [](const struct pollfd &p) { return (p.revents & POLLIN) != 0; });
			if(all)
			{
				// Not atomic as on Windows; good enough for the callers here.
				for(auto &p : fds)
					Take(p.fd);
				return WAIT_OBJECT_0;
			}
			// Wait for the rest without spinning.
			::usleep(1000);
			continue;
		}
		for(DWORD i = 0; i < count; i++)
		{
			if((fds[i].revents & POLLIN) && Take(fds[i].fd))
				return WAIT_OBJECT_0 + i;
		}
	}
}

DWORD WaitForSingleObject(HANDLE handle, DWORD milliseconds)
{
	return WaitForMultipleObjects(1, &handle, FALSE, milliseconds);
}

// ---- directory change notifications -----------------------------------------

HANDLE FindFirstChangeNotification(const char *path, BOOL, DWORD filter)
{
	const int fd = ::inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
	if(fd < 0)
		return INVALID_HANDLE_VALUE;
	uint32_t mask = 0;
	if(filter & (FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME))
		mask |= IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO | IN_DELETE_SELF | IN_MOVE_SELF;
	if(filter & FILE_NOTIFY_CHANGE_ATTRIBUTES)
		mask |= IN_ATTRIB;
	if(filter & (FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE))
		mask |= IN_MODIFY | IN_CLOSE_WRITE;
	// Subtrees are not watched: upstream only watches the folder it shows.
	if(::inotify_add_watch(fd, path, mask) < 0)
	{
		::close(fd);
		SetLastError(ERROR_PATH_NOT_FOUND);
		return INVALID_HANDLE_VALUE;
	}
	return FromFd(fd);
}

BOOL FindNextChangeNotification(HANDLE handle)
{
	// Signalled until the changes are read.
	const int fd = ToFd(handle);
	if(fd < 0)
		return FALSE;
	alignas(struct inotify_event) char buffer[4096];
	while(::read(fd, buffer, sizeof(buffer)) > 0) { }
	return TRUE;
}

BOOL FindCloseChangeNotification(HANDLE handle)
{
	const int fd = ToFd(handle);
	return fd >= 0 && ::close(fd) == 0;
}

// ---- files and folders ------------------------------------------------------

HANDLE FindFirstFile(const char *pattern, LPWIN32_FIND_DATA data)
{
	if(pattern == nullptr || data == nullptr)
		return INVALID_HANDLE_VALUE;
	std::string spec = pattern;
	// Windows separators, if any.
	std::replace(spec.begin(), spec.end(), '\\', '/');
	auto state = new FindState;
	const std::size_t slash = spec.find_last_of('/');
	state->path = slash == std::string::npos ? "." : (slash == 0 ? "/" : spec.substr(0, slash));
	state->pattern = slash == std::string::npos ? spec : spec.substr(slash + 1);
	// "*.*" matches every name on Windows, also those without a dot.
	if(state->pattern == "*.*")
		state->pattern = "*";
	state->dir = ::opendir(state->path.c_str());
	if(state->dir == nullptr || !NextMatch(*state, data))
	{
		if(state->dir)
			::closedir(state->dir);
		delete state;
		SetLastError(ERROR_FILE_NOT_FOUND);
		return INVALID_HANDLE_VALUE;
	}
	return state;
}

BOOL FindNextFile(HANDLE find, LPWIN32_FIND_DATA data)
{
	if(find == nullptr || find == INVALID_HANDLE_VALUE)
		return FALSE;
	if(!NextMatch(*static_cast<FindState *>(find), data))
	{
		SetLastError(ERROR_NO_MORE_FILES);
		return FALSE;
	}
	return TRUE;
}

BOOL FindClose(HANDLE find)
{
	if(find == nullptr || find == INVALID_HANDLE_VALUE)
		return FALSE;
	auto state = static_cast<FindState *>(find);
	::closedir(state->dir);
	delete state;
	return TRUE;
}

BOOL RemoveDirectory(const char *path)
{
	return path != nullptr && ::rmdir(path) == 0;
}

DWORD GetCurrentDirectory(DWORD size, LPWSTR buffer)
{
	char cwd[PATH_MAX];
	if(::getcwd(cwd, sizeof(cwd)) == nullptr)
		return 0;
	return CopyOut(Wide(cwd), buffer, size, false);
}

BOOL SetCurrentDirectory(const char *path)
{
	if(path == nullptr || ::chdir(path) != 0)
	{
		SetLastError(ERROR_PATH_NOT_FOUND);
		return FALSE;
	}
	return TRUE;
}

BOOL IsDebuggerPresent()
{
	// A tracer (gdb, strace) shows in the process status.
	FILE *status = std::fopen("/proc/self/status", "r");
	if(status == nullptr)
		return FALSE;
	char line[256];
	int tracer = 0;
	while(std::fgets(line, sizeof(line), status) != nullptr)
	{
		if(std::sscanf(line, "TracerPid: %d", &tracer) == 1)
			break;
	}
	std::fclose(status);
	return tracer != 0 ? TRUE : FALSE;
}

UINT GetDriveType(LPCWSTR rootPath)
{
	// No drive letters here; the root of the file system is a fixed disk.
	if(rootPath != nullptr && rootPath[0] == L'/')
		return DRIVE_FIXED;
	return DRIVE_NO_ROOT_DIR;
}

namespace
{

// /proc/self/mounts writes spaces, tabs, newlines and backslashes in octal.
std::string UnescapeMountPath(const std::string &path)
{
	std::string out;
	for(std::size_t i = 0; i < path.size(); i++)
	{
		if(path[i] == '\\' && i + 3 < path.size()
			&& path[i + 1] >= '0' && path[i + 1] <= '7' && path[i + 2] >= '0' && path[i + 2] <= '7' && path[i + 3] >= '0' && path[i + 3] <= '7')
		{
			out += static_cast<char>(((path[i + 1] - '0') << 6) | ((path[i + 2] - '0') << 3) | (path[i + 3] - '0'));
			i += 3;
		} else
		{
			out += path[i];
		}
	}
	return out;
}

bool IsDirectory(const std::string &path)
{
	struct stat st;
	return ::stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

// Where drives get mounted: /mnt/name, /media/name, /media/user/name and
// /run/media/user/name (what udisks and fstab entries use). Mounts nested
// deeper are part of something else.
bool IsDriveMountPoint(const std::string &path)
{
	const auto depthBelow = [&path](const std::string &prefix) {
		if(path.size() <= prefix.size() || path.compare(0, prefix.size(), prefix) != 0)
			return -1;
		std::string rest = path.substr(prefix.size());
		while(!rest.empty() && rest.back() == '/')
			rest.pop_back();
		return rest.empty() ? -1 : static_cast<int>(std::count(rest.begin(), rest.end(), '/')) + 1;
	};
	const int mnt = depthBelow("/mnt/"), media = depthBelow("/media/"), runMedia = depthBelow("/run/media/");
	return mnt == 1 || media == 1 || media == 2 || runMedia == 2;
}

} // namespace

std::vector<std::wstring> LinOpenMPTFileBrowserPlaces()
{
	std::vector<std::string> places{"/"};
	if(const char *home = std::getenv("HOME"); home != nullptr && home[0] == '/' && IsDirectory(home))
		places.push_back(home);
	if(FILE *mounts = std::fopen("/proc/self/mounts", "r"))
	{
		char line[4096];
		while(std::fgets(line, sizeof(line), mounts) != nullptr)
		{
			char device[1024], mountPoint[1024];
			if(std::sscanf(line, "%1023s %1023s", device, mountPoint) != 2)
				continue;
			const std::string path = UnescapeMountPath(mountPoint);
			if(IsDriveMountPoint(path) && IsDirectory(path))
				places.push_back(path);
		}
		std::fclose(mounts);
	}
	std::vector<std::wstring> result;
	for(std::string &place : places)
	{
		if(place.back() != '/')
			place += '/';
		std::wstring wide = Wide(place.c_str());
		if(std::find(result.begin(), result.end(), wide) == result.end())
			result.push_back(std::move(wide));
	}
	return result;
}

BOOL DeviceIoControl(HANDLE, DWORD, LPVOID, DWORD, LPVOID, DWORD, LPDWORD returned, LPOVERLAPPED)
{
	// Only asked of drive letters, which do not exist here.
	if(returned)
		*returned = 0;
	SetLastError(ERROR_INVALID_FUNCTION);
	return FALSE;
}

DWORD ExpandEnvironmentStrings(LPCWSTR src, LPWSTR dst, DWORD size)
{
	if(src == nullptr)
		return 0;
	std::wstring out;
	for(const wchar_t *p = src; *p; p++)
	{
		if(*p == L'%')
		{
			const wchar_t *end = std::wcschr(p + 1, L'%');
			if(end != nullptr && end > p + 1)
			{
				const std::string name = Narrow(std::wstring(p + 1, end).c_str());
				if(const char *value = std::getenv(name.c_str()))
				{
					out += Wide(value);
					p = end;
					continue;
				}
			}
		}
		out += *p;
	}
	// The size includes the terminator, whether or not it fits.
	return CopyOut(out, dst, size, true);
}

// ---- processes and threads --------------------------------------------------

void ExitProcess(UINT exitCode)
{
	// As on Windows, no stack unwinding; buffered output still goes out.
	std::fflush(nullptr);
	std::_Exit(static_cast<int>(exitCode));
}

HANDLE GetCurrentProcess()
{
	return reinterpret_cast<HANDLE>(static_cast<LONG_PTR>(-1));
}

HANDLE GetCurrentThread()
{
	return reinterpret_cast<HANDLE>(static_cast<LONG_PTR>(-2));
}

BOOL SetPriorityClass(HANDLE process, DWORD priorityClass)
{
	if(process != GetCurrentProcess())
		return FALSE;
	int nice = 0;
	switch(priorityClass)
	{
	case IDLE_PRIORITY_CLASS: nice = 19; break;
	case BELOW_NORMAL_PRIORITY_CLASS: nice = 5; break;
	case ABOVE_NORMAL_PRIORITY_CLASS: nice = -5; break;
	case HIGH_PRIORITY_CLASS: nice = -10; break;
	case REALTIME_PRIORITY_CLASS: nice = -20; break;
	default: nice = 0; break;
	}
	// Raising the priority needs privileges; Windows would also succeed
	// with less than asked for, so the call does not fail.
	::setpriority(PRIO_PROCESS, 0, nice);
	return TRUE;
}

DWORD GetPriorityClass(HANDLE process)
{
	if(process != GetCurrentProcess())
		return 0;
	errno = 0;
	const int nice = ::getpriority(PRIO_PROCESS, 0);
	if(nice >= 15) return IDLE_PRIORITY_CLASS;
	if(nice > 0) return BELOW_NORMAL_PRIORITY_CLASS;
	if(nice <= -20) return REALTIME_PRIORITY_CLASS;
	if(nice <= -10) return HIGH_PRIORITY_CLASS;
	if(nice < 0) return ABOVE_NORMAL_PRIORITY_CLASS;
	return NORMAL_PRIORITY_CLASS;
}

BOOL SetThreadPriority(HANDLE thread, int priority)
{
	if(thread != GetCurrentThread())
		return FALSE;
	// Linux threads have their own nice value. Only lowering is allowed
	// without privileges, which is what upstream asks for of its threads.
	int nice = 0;
	if(priority <= THREAD_PRIORITY_IDLE) nice = 19;
	else if(priority <= THREAD_PRIORITY_LOWEST) nice = 10;
	else if(priority < THREAD_PRIORITY_NORMAL) nice = 5;
	else if(priority > THREAD_PRIORITY_NORMAL) nice = -5;
	::setpriority(PRIO_PROCESS, static_cast<id_t>(::syscall(SYS_gettid)), nice);
	return TRUE;
}

// ---- registry ----------------------------------------------------------------

LSTATUS RegOpenKeyEx(HKEY, LPCWSTR, DWORD, REGSAM, PHKEY result)
{
	if(result)
		*result = nullptr;
	return ERROR_FILE_NOT_FOUND;
}

LSTATUS RegQueryValueEx(HKEY, LPCWSTR, LPDWORD, LPDWORD, LPBYTE, LPDWORD)
{
	return ERROR_FILE_NOT_FOUND;
}

LSTATUS RegCloseKey(HKEY)
{
	return ERROR_SUCCESS;
}

// ---- time --------------------------------------------------------------------

DWORD timeGetTime()
{
	using namespace std::chrono;
	return static_cast<DWORD>(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

void GetSystemTimeAsFileTime(LPFILETIME time)
{
	if(time == nullptr)
		return;
	struct timespec ts{};
	::clock_gettime(CLOCK_REALTIME, &ts);
	FillFileTime(*time, ts);
}

// ---- strings -----------------------------------------------------------------

int lstrcmpi(LPCWSTR a, LPCWSTR b)
{
	return ::wcscasecmp(a ? a : L"", b ? b : L"");
}

int lstrcmpiA(LPCSTR a, LPCSTR b)
{
	return ::strcasecmp(a ? a : "", b ? b : "");
}

int wvsprintfW(LPWSTR buf, LPCWSTR format, va_list args)
{
	// MSVC's format rules (%s is the wide string here), as CString::Format.
	CStringW text;
	text.FormatV(format, args);
	// wsprintf writes at most 1024 characters.
	const std::size_t length = std::min<std::size_t>(static_cast<std::size_t>(text.GetLength()), 1023);
	std::wmemcpy(buf, text.GetString(), length);
	buf[length] = L'\0';
	return static_cast<int>(length);
}

int wsprintfW(LPWSTR buf, LPCWSTR format, ...)
{
	va_list args;
	va_start(args, format);
	const int written = wvsprintfW(buf, format, args);
	va_end(args);
	return written;
}

BOOL PathMatchSpec(LPCWSTR file, LPCWSTR spec)
{
	if(file == nullptr || spec == nullptr)
		return FALSE;
	// Patterns separated by semicolons, with optional spaces.
	std::wstring list = spec;
	std::size_t start = 0;
	while(start <= list.size())
	{
		std::size_t end = list.find(L';', start);
		if(end == std::wstring::npos)
			end = list.size();
		std::wstring one = list.substr(start, end - start);
		while(!one.empty() && one.front() == L' ')
			one.erase(one.begin());
		if(!one.empty() && WildcardMatch(file, one.c_str()))
			return TRUE;
		start = end + 1;
	}
	return FALSE;
}

// ---- COM strings ---------------------------------------------------------------

BSTR SysAllocString(const wchar_t *s)
{
	return s != nullptr ? ::wcsdup(s) : nullptr;
}

void SysFreeString(BSTR s)
{
	std::free(s);
}
