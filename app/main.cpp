// linOpenMPT: the program's entry point. On Windows MFC's WinMain runs
// AfxWinMain, which starts the one CWinApp object (upstream's theApp, in
// Mptrack.cpp): InitInstance, then the message loop, or ExitInstance at once
// if starting failed. This does the same on a QApplication, after making
// the tracker's resources (dialogs, menus, bitmaps, strings) available.
#include "stdafx.h"
#include "Mptrack.h"
#include "MptrackResources.h"

#include "QtWin.h"

#include <QtCore/QTimer>
#include <QtGui/QPixmap>
#include <QtWidgets/QApplication>
#include <QtWidgets/QWidget>

#include <cstdlib>

namespace
{

// For tests and screenshots: LINOPENMPT_SCREENSHOT=file.png saves the main
// window once the program has been running for LINOPENMPT_SCREENSHOT_DELAY
// milliseconds (3000 by default), then closes it as the user would.
void ScheduleScreenshot()
{
	const char *file = std::getenv("LINOPENMPT_SCREENSHOT");
	if(file == nullptr || *file == '\0')
		return;
	const char *delayText = std::getenv("LINOPENMPT_SCREENSHOT_DELAY");
	const int delay = delayText != nullptr ? std::atoi(delayText) : 3000;
	const QString path = QString::fromLocal8Bit(file);
	QTimer::singleShot(delay, [path]() {
		CWnd *mainWnd = AfxGetMainWnd();
		if(mainWnd == nullptr || mainWnd->GetSafeHwnd() == nullptr)
			return;
		if(QWidget *widget = linopenmpt::qtwin::FromHandle(mainWnd->GetSafeHwnd()))
			widget->grab().save(path);
		mainWnd->PostMessage(WM_CLOSE);
	});
}

} // namespace

int main(int argc, char *argv[])
{
	QApplication application(argc, argv);
	QApplication::setApplicationName(QStringLiteral("OpenMPT"));
	QApplication::setOrganizationName(QStringLiteral("OpenMPT"));
	LinOpenMPTRegisterMptrackResources();

	CWinApp *app = AfxGetApp();
	if(app == nullptr)
		return 1;
	// As AfxWinInit: the name of the program, without its folder.
	if(app->m_pszExeName == nullptr)
		app->m_pszExeName = _T("OpenMPT");
	if(app->m_pszAppName == nullptr)
		app->m_pszAppName = _T("OpenMPT");

	if(!app->InitInstance())
	{
		const int code = app->ExitInstance();
		return code != 0 ? code : 1;
	}
	ScheduleScreenshot();
	return app->Run();
}
