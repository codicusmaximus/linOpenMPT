/*
 * MPTrackUtil.h
 * -------------
 * Purpose: Various useful utility functions.
 * Notes  : mfcless twin of upstream's mptrack/MPTrackUtil.h. Resources come
 *          from the embedded resource file (mfcless::res).
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"

#include <chrono>
#include <string>


OPENMPT_NAMESPACE_BEGIN


bool CreateShellFolderLink(const mpt::PathString &path, const mpt::PathString &target, const mpt::ustring &description = mpt::ustring());

bool CreateShellFileLink(const mpt::PathString &path, const mpt::PathString &target, const mpt::ustring &description = mpt::ustring());


/*
 * Gets resource as raw byte data.
 * [in] id and type: the resource's ID and type name (such as U_("PNG")).
 * Return: span representing the resource data, valid for the program's lifetime.
 */
mpt::const_byte_span GetResource(uint32 id, const mpt::ustring &type);


mpt::ustring LoadResourceString(uint32 nID);


namespace Util
{
	inline uint64 GetTickCount64()
	{
		return static_cast<uint64>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
	}
}


OPENMPT_NAMESPACE_END
