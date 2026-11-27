/*
 * LinkResolver.cpp
 * ----------------
 * Purpose: Resolve Windows shell link targets
 * Notes  : mfcless twin of upstream's mptrack/LinkResolver.cpp. On Linux,
 *          links are symbolic links.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */

#include "LinkResolver.h"

#include "mfcless/QtString.h"

#include <QtCore/QFileInfo>


OPENMPT_NAMESPACE_BEGIN

LinkResolver::LinkResolver()
{
}


LinkResolver::~LinkResolver()
{
}


mpt::PathString LinkResolver::Resolve(const mpt::uchar *inPath)
{
	const QFileInfo info(mfcless::ToQString(inPath));
	if(info.isSymLink())
	{
		const QString target = info.canonicalFilePath();
		if(!target.isEmpty())
			return mpt::PathString::FromUnicode(mfcless::FromQString(target));
	}
	return mpt::PathString::FromUnicode(inPath);
}

OPENMPT_NAMESPACE_END
