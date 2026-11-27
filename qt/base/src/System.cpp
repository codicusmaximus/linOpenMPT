// mfcless base: see mfcless/System.h.

#include "stdafx.h"
#include "mfcless/System.h"

#include <QtCore/QStandardPaths>

#include <chrono>
#include <thread>

#include <unistd.h>
#include <QtGui/QGuiApplication>
#include <QtGui/QPalette>
#include <QtGui/QScreen>

namespace mfcless
{

uint32 TickCount()
{
	using namespace std::chrono;
	return static_cast<uint32>(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

uint32 CurrentThreadId()
{
	return static_cast<uint32>(::gettid());
}

void SleepMs(uint32 milliseconds)
{
	std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

static QRect VirtualScreen()
{
	QScreen *screen = QGuiApplication::primaryScreen();
	return screen ? screen->virtualGeometry() : QRect();
}

int VirtualScreenWidth()
{
	return VirtualScreen().width();
}

int VirtualScreenHeight()
{
	return VirtualScreen().height();
}

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
