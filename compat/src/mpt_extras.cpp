// linOpenMPT compatibility layer: implementations for mpt_extras.h.
// Compiled as part of mpt_common (tracker configuration).
#include "stdafx.h"

#include "mptRandom.h"
#include "mpt/uuid/uuid.hpp"

#include <time.h>
#include <unistd.h>

namespace mpt
{

mpt::PathString NewTemporaryPathname(const mpt::PathString &fileNameExtension)
{
	// Same naming as upstream: OpenMPT-<uuid>.<ext> in the temp directory.
	const mpt::UUID uuid = mpt::UUID::GenerateLocalUseOnly(mpt::global_prng());
	mpt::PathString name = mpt::PathString::FromNative(mpt::common_directories::get_temp_directory().AsNative())
		+ MPT_PATHSTRING("OpenMPT-") + mpt::PathString::FromUnicode(uuid.ToUString());
	if(!fileNameExtension.empty())
		name += MPT_PATHSTRING(".") + fileNameExtension;
	return name;
}

static bool IsBelow(const mpt::RawPathString &path, const mpt::RawPathString &base)
{
	return path.size() >= base.size() && path.compare(0, base.size(), base) == 0;
}

mpt::PathString AbsolutePathToRelative(const mpt::PathString &path, const mpt::PathString &relativeTo)
{
	if(path.empty() || relativeTo.empty())
		return path;
	mpt::RawPathString base = relativeTo.AsNative();
	if(!base.empty() && base.back() != '/')
		base.push_back('/');
	const mpt::RawPathString &p = path.AsNative();
	if(IsBelow(p, base))
		return mpt::PathString::FromNative("./" + p.substr(base.size()));
	return path;
}

mpt::PathString RelativePathToAbsolute(const mpt::PathString &path, const mpt::PathString &relativeTo)
{
	// As upstream: "./x" and any other path that is not absolute are below
	// relativeTo; absolute paths stay as they are.
	if(path.empty() || relativeTo.empty())
		return path;
	const mpt::RawPathString &p = path.AsNative();
	if(p[0] == '/')
		return path;
	mpt::RawPathString base = relativeTo.AsNative();
	if(!base.empty() && base.back() != '/')
		base.push_back('/');
	if(p.size() >= 2 && p[0] == '.' && p[1] == '/')
		return mpt::PathString::FromNative(base + p.substr(2));
	return mpt::PathString::FromNative(base + p);
}

namespace OS
{
namespace Windows
{

int Bitness(Architecture arch) noexcept
{
	return (static_cast<int>(arch) & 0x0800) ? 64 : 32;
}

mpt::ustring Name(Architecture arch)
{
	// Same names as upstream (misc/mptOS.cpp).
	switch(arch)
	{
	case Architecture::x86: return U_("x86");
	case Architecture::amd64: return U_("amd64");
	case Architecture::arm: return U_("arm");
	case Architecture::arm64: return U_("arm64");
	case Architecture::mips: return U_("mips");
	case Architecture::ppc: return U_("ppc");
	case Architecture::shx: return U_("shx");
	case Architecture::alpha: return U_("alpha");
	case Architecture::alpha64: return U_("alpha64");
	case Architecture::ia64: return U_("ia64");
	default: return U_("unknown");
	}
}

Architecture GetProcessArchitecture() noexcept
{
#if defined(__x86_64__)
	return Architecture::amd64;
#elif defined(__i386__)
	return Architecture::x86;
#elif defined(__aarch64__)
	return Architecture::arm64;
#elif defined(__arm__)
	return Architecture::arm;
#elif defined(__mips__)
	return Architecture::mips;
#elif defined(__powerpc__)
	return Architecture::ppc;
#else
	return Architecture::unknown;
#endif
}

Architecture GetHostArchitecture() noexcept
{
	return GetProcessArchitecture();
}

std::vector<Architecture> GetSupportedProcessArchitectures(Architecture host)
{
	return {host};
}

uint64 GetSystemMemorySize()
{
	const long pages = ::sysconf(_SC_PHYS_PAGES);
	const long pageSize = ::sysconf(_SC_PAGE_SIZE);
	if(pages <= 0 || pageSize <= 0)
		return 0;
	return static_cast<uint64>(pages) * static_cast<uint64>(pageSize);
}

} // namespace Windows
} // namespace OS

} // namespace mpt

namespace Util
{

uint64 MultimediaClock::NowNanoseconds() const
{
	timespec ts{};
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return static_cast<uint64>(ts.tv_sec) * 1000000000u + static_cast<uint64>(ts.tv_nsec);
}

uint32 MultimediaClock::Now() const
{
	return static_cast<uint32>(NowNanoseconds() / 1000000u);
}

} // namespace Util
