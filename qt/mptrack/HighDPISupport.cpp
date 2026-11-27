/*
 * HighDPISupport.cpp
 * ------------------
 * Purpose: Various helpers for making the GUI scale properly on high-DPI displays.
 * Notes  : mfcless twin of upstream's mptrack/HighDPISupport.cpp. Qt scales
 *          windows to the screen itself (HighDPISupport.h), so what is left
 *          is the dialog font and the sizes of window parts.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */

#include "openmpt/all/BuildSettings.hpp"
#include "HighDPISupport.h"

#include <QtGui/QFont>
#include <QtWidgets/QApplication>
#include <QtWidgets/QStyle>
#include <QtWidgets/QWidget>

OPENMPT_NAMESPACE_BEGIN

// Upstream: the message box font of the non-client metrics. Here: the
// desktop's (Qt's) font for dialogs, or the window's own.
void HighDPISupport::CreateGUIFont(QFont &font, const QWidget *hwnd)
{
	font = hwnd ? hwnd->font() : QApplication::font("QMessageBox");
}


int HighDPISupport::GetSystemMetrics(SystemMetric index, uint32 /*dpi*/)
{
	QStyle *style = QApplication::style();
	switch(index)
	{
	case SystemMetric::CxBorder:
	case SystemMetric::CyBorder:
		return 1;
	case SystemMetric::CxVScroll:
	case SystemMetric::CyHScroll:
		return style ? style->pixelMetric(QStyle::PM_ScrollBarExtent) : 16;
	case SystemMetric::CxEdge:
	case SystemMetric::CyEdge:
		return style ? style->pixelMetric(QStyle::PM_DefaultFrameWidth) : 2;
	default:
		return 0;
	}
}

OPENMPT_NAMESPACE_END
