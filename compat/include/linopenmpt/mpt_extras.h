// linOpenMPT compatibility layer: pieces of upstream's mpt/ and misc/
// libraries that upstream only provides on Windows, but which the shared
// tracker code (Settings.h, Mptrack.h, FileDialog.h, MidiInOut.h, ...) uses.
//
// Force-included right after upstream's own precompiled header (PCH.h), so
// everything upstream declares is already visible here.

#pragma once

#if defined(MODPLUG_TRACKER) && !MPT_OS_WINDOWS

// DirectX Media Object hosting is Windows-only (COM). OpenMPT's own built-in
// emulations of the DMO effects (soundlib/plugins/dmo/*) do not depend on it
// and stay enabled.
#undef MPT_WITH_DMO

// Hosting VST plugins is Windows-specific upstream (DLL loading and the
// 32/64-bit plugin bridge built on Win32 events and windows). It is off until
// the plugin milestone, which adds native Linux plugins and a bridge for
// Windows ones. Built-in plugins (DMO emulations, LFO, echo, MIDI I/O) work.
#undef MPT_WITH_VST

#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>

#include <strings.h>

#include "mpt/fs/fs.hpp"

namespace mpt
{
// On Windows these are the TCHAR (wide, UNICODE) string types.
using tstring = std::wstring;
using tstring_view = std::wstring_view;
using winstring = mpt::tstring;
using winstring_view = mpt::tstring_view;

// Result of ToWin(). On Windows a native path string *is* a winstring, and
// upstream code (e.g. FileDialog.h) assigns one to the other. On Linux native
// paths are narrow, so this wide string also converts to a native path.
class winstring_result : public mpt::winstring
{
public:
	using mpt::winstring::winstring;
	winstring_result(mpt::winstring s) : mpt::winstring(std::move(s)) { }
	operator mpt::RawPathString() const { return mpt::PathString::FromWide(*this).AsNative(); }
};

template <typename Tsrc>
inline winstring_result ToWin(Tsrc &&str)
{
	return mpt::transcode<mpt::winstring>(std::forward<Tsrc>(str));
}
template <typename Tsrc, typename Tencoding>
inline winstring_result ToWin(Tencoding &&from, Tsrc &&str)
{
	return mpt::transcode<mpt::winstring>(std::forward<Tencoding>(from), std::forward<Tsrc>(str));
}

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
	bool delete_tree(mpt::native_path path)
	{
		std::error_code ec;
		std::filesystem::remove_all(p(path), ec);
		return !ec;
	}
};
using native_fs = fs<mpt::native_path>;

// Upstream compares paths case-insensitively on Windows (lstrcmpi).
inline int PathCompareNoCase(const PathString &a, const PathString &b)
{
	return ::strcasecmp(a.AsNative().c_str(), b.AsNative().c_str());
}

// POSIX versions of upstream's install-relative path helpers.
// Paths below relativeTo become "./sub/path"; others are returned unchanged.
mpt::PathString AbsolutePathToRelative(const mpt::PathString &path, const mpt::PathString &relativeTo);
mpt::PathString RelativePathToAbsolute(const mpt::PathString &path, const mpt::PathString &relativeTo);
} // namespace mpt

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

#endif // MODPLUG_TRACKER && !MPT_OS_WINDOWS
