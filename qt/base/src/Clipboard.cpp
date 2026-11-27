/*
 * Clipboard.cpp
 * -------------
 * Purpose: The system clipboard by Windows' format numbers.
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */

#include "mfcless/Clipboard.h"

#include <QtCore/QMimeData>
#include <QtGui/QClipboard>
#include <QtGui/QGuiApplication>

#include <map>
#include <string>

namespace mfcless
{
namespace clip
{

namespace
{

std::map<uint32_t, QString> &Registered()
{
	static std::map<uint32_t, QString> formats;
	return formats;
}

QString MimeType(uint32_t format)
{
	if(format == Wave)
		return QStringLiteral("audio/x-wav");
	auto it = Registered().find(format);
	if(it != Registered().end())
		return QStringLiteral("application/x-openmpt-") + it->second.toLower().replace(QLatin1Char(' '), QLatin1Char('-'));
	return QString();
}

} // namespace


uint32_t RegisterFormat(const QString &name)
{
	for(const auto &[id, n] : Registered())
	{
		if(n == name)
			return id;
	}
	const uint32_t id = 0xC000 + static_cast<uint32_t>(Registered().size());
	Registered().emplace(id, name);
	return id;
}


bool IsFormatAvailable(uint32_t format)
{
	const QMimeData *data = QGuiApplication::clipboard()->mimeData();
	if(!data)
		return false;
	if(format == Text || format == UnicodeText)
		return data->hasText();
	const QString mime = MimeType(format);
	return !mime.isEmpty() && data->hasFormat(mime);
}


QByteArray GetData(uint32_t format)
{
	const QMimeData *data = QGuiApplication::clipboard()->mimeData();
	if(!data)
		return {};
	if(format == Text)
	{
		if(!data->hasText())
			return {};
		QByteArray text = data->text().toUtf8();
		text.append('\0');
		return text;
	}
	if(format == UnicodeText)
	{
		if(!data->hasText())
			return {};
		const std::wstring text = data->text().toStdWString();
		return QByteArray(reinterpret_cast<const char *>(text.c_str()), static_cast<qsizetype>((text.size() + 1) * sizeof(wchar_t)));
	}
	const QString mime = MimeType(format);
	return mime.isEmpty() ? QByteArray() : data->data(mime);
}


void SetData(uint32_t format, const QByteArray &bytes)
{
	auto *data = new QMimeData;
	if(format == Text)
	{
		data->setText(QString::fromUtf8(bytes.constData(), qstrnlen(bytes.constData(), bytes.size())));
	} else if(format == UnicodeText)
	{
		const auto *chars = reinterpret_cast<const wchar_t *>(bytes.constData());
		qsizetype length = bytes.size() / static_cast<qsizetype>(sizeof(wchar_t));
		for(qsizetype i = 0; i < length; i++)
		{
			if(chars[i] == 0)
			{
				length = i;
				break;
			}
		}
		data->setText(QString::fromWCharArray(chars, length));
	} else
	{
		data->setData(MimeType(format), bytes);
	}
	QGuiApplication::clipboard()->setMimeData(data);
}

} // namespace clip
} // namespace mfcless
