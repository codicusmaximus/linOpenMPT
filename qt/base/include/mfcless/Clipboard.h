/*
 * Clipboard.h
 * -----------
 * Purpose: The system clipboard by Windows' format numbers, which upstream's
 *          code passes around: text, Unicode text, wave data and formats
 *          registered by name (as MIME types of their own).
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */

#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QString>

#include <cstdint>

namespace mfcless
{
namespace clip
{

inline constexpr uint32_t Text = 1, Wave = 12, UnicodeText = 13;

// A format of its own for name; the same name gives the same number.
uint32_t RegisterFormat(const QString &name);
bool IsFormatAvailable(uint32_t format);
// The clipboard's data in this format. Text is 8-bit (UTF-8) and ends with
// a null; Unicode text is wchar_t and ends with a null.
QByteArray GetData(uint32_t format);
void SetData(uint32_t format, const QByteArray &data);

} // namespace clip
} // namespace mfcless
