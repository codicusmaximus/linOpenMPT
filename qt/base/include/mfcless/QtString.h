/*
 * QtString.h
 * ----------
 * Purpose: Conversions between OpenMPT's string types and QString.
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */

#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "mpt/string/types.hpp"
#include "mpt/string_transcode/transcode.hpp"

#include <QtCore/QString>

namespace mfcless
{

inline QString ToQString(const ::mpt::ustring &s)
{
	return QString::fromStdString(::mpt::transcode<std::string>(::mpt::common_encoding::utf8, s));
}

inline ::mpt::ustring FromQString(const QString &s)
{
	return ::mpt::transcode<::mpt::ustring>(::mpt::common_encoding::utf8, s.toStdString());
}

} // namespace mfcless
