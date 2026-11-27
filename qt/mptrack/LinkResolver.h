/*
 * LinkResolver.h
 * --------------
 * Purpose: Resolve Windows shell link targets
 * Notes  : mfcless twin of upstream's mptrack/LinkResolver.h. Resolves
 *          symbolic links (Windows: shell links).
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "../common/mptPathString.h"

OPENMPT_NAMESPACE_BEGIN

class LinkResolver
{
public:
	LinkResolver();
	~LinkResolver();


	mpt::PathString Resolve(const mpt::uchar *inPath);
	mpt::PathString Resolve(const mpt::ustring &inPath) { return Resolve(inPath.c_str()); }
};

OPENMPT_NAMESPACE_END
