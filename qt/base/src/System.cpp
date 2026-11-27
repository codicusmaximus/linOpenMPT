// mfcless base: see mfcless/System.h.

#include "stdafx.h"
#include "mfcless/System.h"

#include <QtCore/QStandardPaths>
#include <QtGui/QGuiApplication>
#include <QtGui/QPalette>

namespace mfcless
{

uint32 SystemColor(SysColor role)
{
	const QPalette palette = QGuiApplication::palette();
	QColor c;
	switch(role)
	{
	case SysColor::ButtonFace: c = palette.color(QPalette::Button); break;
	case SysColor::ButtonShadow: c = palette.color(QPalette::Dark); break;
	case SysColor::ButtonHighlight: c = palette.color(QPalette::Light); break;
	case SysColor::ButtonText: c = palette.color(QPalette::ButtonText); break;
	case SysColor::Window: c = palette.color(QPalette::Base); break;
	case SysColor::WindowText: c = palette.color(QPalette::Text); break;
	case SysColor::Highlight: c = palette.color(QPalette::Highlight); break;
	case SysColor::HighlightText: c = palette.color(QPalette::HighlightedText); break;
	case SysColor::GrayText: c = palette.color(QPalette::Disabled, QPalette::Text); break;
	}
	return Rgb(static_cast<uint8>(c.red()), static_cast<uint8>(c.green()), static_cast<uint8>(c.blue()));
}

OPENMPT_NAMESPACE::mpt::PathString DataDirectory()
{
	QString dir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
	if(dir.isEmpty())
		dir = QStringLiteral("/tmp");
	return OPENMPT_NAMESPACE::mpt::PathString::FromUTF8((dir + QStringLiteral("/OpenMPT")).toStdString());
}

} // namespace mfcless
