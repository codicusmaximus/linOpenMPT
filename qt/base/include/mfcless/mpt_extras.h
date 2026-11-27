// mfcless base: pieces of upstream's mpt/ and misc/ libraries that upstream
// only provides on Windows, but which the tracker configuration of the engine
// and the tracker logic use. The MFC build has its own copy with the Win32
// string types added (compat/include/linopenmpt/mpt_extras.h); this one has
// no MFC or Win32 names.
//
// Force-included right after upstream's own precompiled header (PCH.h), so
// everything upstream declares is already visible here.

#pragma once

#if defined(MODPLUG_TRACKER) && !MPT_OS_WINDOWS

// DirectX Media Object hosting is Windows-only (COM). OpenMPT's own built-in
// emulations of the DMO effects (soundlib/plugins/dmo/*) do not depend on it
// and stay enabled.
#undef MPT_WITH_DMO

// VST plugins: native Linux VST2 plugins (.so) are loaded in-process, with
// LoadLibrary on dlopen. Upstream's 32/64-bit plugin bridge (Win32 events,
// shared memory and windows) is not built; BridgeWrapper's stand-in reports
// it as missing.

// OpenMPT's update check downloads and runs Windows installers (WinInet,
// Windows signature checks). Upstream's own switch turns it off, as Linux
// distributions ship updates themselves; the menu item and options page go
// with it.
#undef MPT_ENABLE_UPDATE

#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>

#include <strings.h>

#include "mpt/fs/fs.hpp"
#include "mpt/string/buffer.hpp"

#include <array>
#include <type_traits>
#include <vector>

namespace mpt
{
// Filesystem queries with the interface of upstream's Windows-only
// mpt::fs<mpt::native_path> (src/mpt/fs/fs.hpp), on std::filesystem.
template <>
class fs<mpt::native_path>
{
	static std::filesystem::path p(const mpt::native_path &path) { return std::filesystem::path(path.AsNative().c_str()); }

public:
	bool is_directory(const mpt::native_path &path)
	{
		std::error_code ec;
		return std::filesystem::is_directory(p(path), ec);
	}
	bool is_file(const mpt::native_path &path)
	{
		std::error_code ec;
		return std::filesystem::is_regular_file(p(path), ec);
	}
	bool exists(const mpt::native_path &path)
	{
		std::error_code ec;
		return std::filesystem::exists(p(path), ec);
	}
	mpt::native_path absolute(const mpt::native_path &path)
	{
		std::error_code ec;
		auto abs = std::filesystem::absolute(p(path), ec);
		return ec ? path : mpt::native_path::FromNative(mpt::RawPathString(abs.string().c_str()));
	}
	bool create_directory(const mpt::native_path &path)
	{
		std::error_code ec;
		return std::filesystem::create_directory(p(path), ec) || std::filesystem::is_directory(p(path), ec);
	}
	bool delete_tree(mpt::native_path path)
	{
		std::error_code ec;
		std::filesystem::remove_all(p(path), ec);
		return !ec;
	}
};
using native_fs = fs<mpt::native_path>;

// Upstream's mpt::common_directories (src/mpt/fs/common_directories.hpp)
// is Windows-only.
class common_directories
{
public:
	// The directory of the running executable, with a trailing slash.
	static mpt::native_path get_application_directory()
	{
		std::error_code ec;
		auto exe = std::filesystem::read_symlink("/proc/self/exe", ec);
		if(ec)
			return mpt::native_path::FromNative(mpt::RawPathString("./"));
		return mpt::native_path::FromNative(mpt::RawPathString(exe.parent_path().string().c_str())).WithTrailingSlash();
	}
	static mpt::native_path get_system_directory() { return mpt::native_path::FromNative(mpt::RawPathString("/usr/lib/")); }
	static mpt::native_path get_temp_directory()
	{
		std::error_code ec;
		auto dir = std::filesystem::temp_directory_path(ec);
		return mpt::native_path::FromNative(mpt::RawPathString((ec ? std::string("/tmp") : dir.string()).c_str())).WithTrailingSlash();
	}
};

// Upstream's Trace facility (common/Logging.h) is Windows-only; settings
// still toggle it, so provide the switch as a no-op.
namespace log
{
namespace Trace
{
inline void Enable(std::size_t) { }
inline void Disable() { }
enum ThreadKind
{
	ThreadKindGUI,
	ThreadKindAudio,
	ThreadKindNotify,
	ThreadKindWatchdir,
};
inline void SetThreadId(ThreadKind, uint32) { }
inline uint32 GetThreadId(ThreadKind) { return 0; }
inline void Seal() { }
inline bool Dump(const mpt::PathString &) { return false; }
} // namespace Trace
} // namespace log

// Upstream compares paths case-insensitively on Windows (lstrcmpi).
inline int PathCompareNoCase(const PathString &a, const PathString &b)
{
	return ::strcasecmp(a.AsNative().c_str(), b.AsNative().c_str());
}

// POSIX versions of upstream's install-relative path helpers.
// Paths below relativeTo become "./sub/path"; others are returned unchanged.
mpt::PathString AbsolutePathToRelative(const mpt::PathString &path, const mpt::PathString &relativeTo);
mpt::PathString RelativePathToAbsolute(const mpt::PathString &path, const mpt::PathString &relativeTo);

// Upstream's temporary file helpers (common/mptFileTemporary.h) are
// Windows-only. Same interface and behaviour on POSIX.
mpt::PathString NewTemporaryPathname(const mpt::PathString &fileNameExtension);

// Returns a new unique absolute path in the temp directory.
class TemporaryPathname
{
	mpt::PathString m_Path;
public:
	TemporaryPathname(const mpt::PathString &fileNameExtension = MPT_PATHSTRING("tmp"))
		: m_Path(NewTemporaryPathname(fileNameExtension)) { }
	mpt::PathString GetPathname() const { return m_Path; }
};

// Deletes the file when going out of scope. The file is not created.
class TempFileGuard
{
	const mpt::PathString filename;
public:
	TempFileGuard(const mpt::TemporaryPathname &pathname = mpt::TemporaryPathname{})
		: filename(pathname.GetPathname()) { }
	mpt::PathString GetFilename() const { return filename; }
	~TempFileGuard()
	{
		if(!filename.empty())
			std::remove(filename.AsNative().c_str());
	}
};

// Creates the directory, and deletes it with its contents when going out of scope.
class TempDirGuard
{
	mpt::PathString dirname;
public:
	TempDirGuard(const mpt::TemporaryPathname &pathname = mpt::TemporaryPathname{})
		: dirname(pathname.GetPathname().WithTrailingSlash())
	{
		std::error_code ec;
		if(!dirname.empty() && !std::filesystem::create_directory(std::filesystem::path(dirname.AsNative().c_str()), ec))
			dirname = mpt::PathString();
	}
	mpt::PathString GetDirname() const { return dirname; }
	~TempDirGuard()
	{
		if(!dirname.empty())
			mpt::native_fs{}.delete_tree(dirname);
	}
};

} // namespace mpt

namespace mpt
{
namespace OS
{
namespace Windows
{
// Upstream's misc/mptOS.h declares these only on Windows. The tracker shows
// the architecture in the About dialog and compares it between instances
// (IPCWindow). Same enum values as upstream; implemented in mpt_extras.cpp.
enum class Architecture
{
	unknown = -1,
	x86 = 0x0401,
	amd64 = 0x0801,
	arm = 0x0402,
	arm64 = 0x0802,
	mips = 0x0403,
	ppc = 0x0404,
	shx = 0x0405,
	alpha = 0x0406,
	alpha64 = 0x0806,
	ia64 = 0x0807,
};
int Bitness(Architecture arch) noexcept;
mpt::ustring Name(Architecture arch);
// The host and the process architecture are the same on Linux.
Architecture GetHostArchitecture() noexcept;
Architecture GetProcessArchitecture() noexcept;
std::vector<Architecture> GetSupportedProcessArchitectures(Architecture host);
uint64 GetSystemMemorySize();
} // namespace Windows
} // namespace OS
} // namespace mpt

// Upstream's misc/mptOSException.h: structured exception handling, with which
// OpenMPT survives a crashing plugin on Windows. Linux has no equivalent that
// leaves the process in a usable state, so a plugin's crash is the tracker's,
// as with other Linux hosts. TryOrError runs the function and reports no
// exception.
namespace Windows
{
namespace SEH
{
struct Code
{
private:
	uint32 m_Code;
public:
	constexpr Code(uint32 code) noexcept : m_Code(code) { }
	constexpr uint32 code() const noexcept { return m_Code; }
};
template <typename Tfn>
uint32 TryOrError(const Tfn &fn)
{
	fn();
	return 0;
}
} // namespace SEH
} // namespace Windows

namespace Util
{
// Monotonic millisecond clock with the same interface as upstream's Windows
// MultimediaClock (misc/mptClock.h). Resolution is always 1 ms on Linux.
class MultimediaClock
{
public:
	MultimediaClock() = default;
	explicit MultimediaClock(uint32 ms) { SetResolution(ms); }
	uint32 SetResolution(uint32 ms) { return ms ? 1 : 0; }
	uint32 GetResolution() const { return 1; }
	uint32 Now() const;
	uint64 NowNanoseconds() const;
};
} // namespace Util

namespace linopenmpt
{
// Upstream writes Windows separators in path literals, e.g.
// GetConfigPath() + P_("tunings\\"). On Linux every backslash in such a
// literal becomes '/', so the same code builds the right paths.
inline mpt::PathString PortablePathLiteral(const mpt::PathString &path)
{
	mpt::RawPathString raw = path.AsNative();
	for(auto &c : raw)
	{
		if(c == '\\')
			c = '/';
	}
	return mpt::PathString::FromNative(raw);
}
} // namespace linopenmpt

#undef P_
#define P_(x) ::linopenmpt::PortablePathLiteral(MPT_PATHSTRING(x))

#endif // MODPLUG_TRACKER && !MPT_OS_WINDOWS
