/*
 * MPTrackUtil.cpp
 * ---------------
 * Purpose: Various useful utility functions.
 * Notes  : mfcless twin of upstream's mptrack/MPTrackUtil.cpp. Shell links
 *          are symbolic links; resources come from the embedded resource
 *          file.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "MPTrackUtil.h"

#include "mfcless/QtString.h"
#include "mfcless/Resources.h"

#include <QtCore/QFile>


OPENMPT_NAMESPACE_BEGIN


static bool CreateShellLink(const mpt::PathString &path, const mpt::PathString &target, const mpt::ustring &/*description*/)
{
	// The description has no place in a symbolic link.
	const QString link = mfcless::ToQString(path.ToUnicode());
	QFile::remove(link);
	return QFile::link(mfcless::ToQString(target.ToUnicode()), link);
}


bool CreateShellFolderLink(const mpt::PathString &path, const mpt::PathString &target, const mpt::ustring &description)
{
	return CreateShellLink(path, target, description);
}


bool CreateShellFileLink(const mpt::PathString &path, const mpt::PathString &target, const mpt::ustring &description)
{
	return CreateShellLink(path, target, description);
}


mpt::const_byte_span GetResource(uint32 id, const mpt::ustring &type)
{
	const QString typeName = mfcless::ToQString(type);
	const auto res = mfcless::res::FindData(mfcless::res::ResourceName(typeName.toStdU16String()), static_cast<uint16>(id));
	if(!res)
	{
		return mpt::const_byte_span();
	}
	return mpt::const_byte_span(reinterpret_cast<const std::byte *>(res.data), res.size);
}


mpt::ustring LoadResourceString(uint32 nID)
{
	const std::u16string str = mfcless::res::String(nID);
	MPT_ASSERT(!str.empty());
	return mfcless::FromQString(QString::fromStdU16String(str));
}


OPENMPT_NAMESPACE_END
