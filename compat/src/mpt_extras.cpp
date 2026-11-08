// linOpenMPT compatibility layer: implementations for mpt_extras.h.
// Compiled as part of mpt_common (tracker configuration).
#include "stdafx.h"

#include "mptRandom.h"
#include "mpt/uuid/uuid.hpp"

#include <time.h>

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
	if(path.empty() || relativeTo.empty())
		return path;
	const mpt::RawPathString &p = path.AsNative();
	if(p.size() >= 2 && p[0] == '.' && p[1] == '/')
	{
		mpt::RawPathString base = relativeTo.AsNative();
		if(!base.empty() && base.back() != '/')
			base.push_back('/');
		return mpt::PathString::FromNative(base + p.substr(2));
	}
	return path;
}

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
