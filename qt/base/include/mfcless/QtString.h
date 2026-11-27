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

#include <cstdarg>
#include <cstdio>
#include <vector>

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

// printf-style formatting into a UTF-8 string, as upstream's
// CString::Format does (%s takes UTF-8 strings).
inline ::mpt::ustring PrintF(const char8_t *format, ...)
{
	va_list args, copy;
	va_start(args, format);
	va_copy(copy, args);
	const int size = std::vsnprintf(nullptr, 0, reinterpret_cast<const char *>(format), copy);
	va_end(copy);
	::mpt::ustring result;
	if(size > 0)
	{
		std::vector<char> buffer(static_cast<std::size_t>(size) + 1);
		std::vsnprintf(buffer.data(), buffer.size(), reinterpret_cast<const char *>(format), args);
		result.assign(reinterpret_cast<const char8_t *>(buffer.data()), static_cast<std::size_t>(size));
	}
	va_end(args);
	return result;
}

} // namespace mfcless
