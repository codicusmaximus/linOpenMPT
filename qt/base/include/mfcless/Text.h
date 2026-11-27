/*
 * Text.h
 * ------
 * Purpose: A text argument that takes any of the string types upstream's
 *          code hands to the UI: QString, OpenMPT's mpt::ustring (UTF-8 in
 *          this build), string literals of every width.
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */

#pragma once

#include <QtCore/QString>

#include <string>
#include <string_view>

namespace mfcless
{

struct Text
{
	QString value;

	Text() = default;
	Text(const QString &s) : value(s) {}
	Text(const char *s) : value(QString::fromUtf8(s)) {}
	Text(const std::string &s) : value(QString::fromStdString(s)) {}
	Text(const char8_t *s) : value(QString::fromUtf8(reinterpret_cast<const char *>(s))) {}
	Text(const std::u8string &s) : value(QString::fromUtf8(reinterpret_cast<const char *>(s.data()), static_cast<qsizetype>(s.size()))) {}
	Text(const char16_t *s) : value(QString::fromUtf16(s)) {}
	Text(const std::u16string &s) : value(QString::fromStdU16String(s)) {}
	Text(const wchar_t *s) : value(QString::fromWCharArray(s)) {}
	Text(const std::wstring &s) : value(QString::fromStdWString(s)) {}

	operator const QString &() const { return value; }
};

} // namespace mfcless
