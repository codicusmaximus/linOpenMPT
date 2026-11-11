// linOpenMPT: the program's entry point. On Windows MFC's WinMain runs
// AfxWinMain, which starts the one CWinApp object (upstream's theApp, in
// Mptrack.cpp): InitInstance, then the message loop, or ExitInstance at once
// if starting failed. This does the same on a QApplication, after making
// the tracker's resources (dialogs, menus, bitmaps, strings) available.
#include "stdafx.h"
#include "Moddoc.h"
#include "Mptrack.h"
#include "MptrackResources.h"

#include "QtWin.h"

#include <QtCore/QTimer>
#include <QtCore/QVariant>
#include <QtGui/QGuiApplication>
#include <QtGui/QPixmap>
#include <QtGui/QScreen>
#include <QtWidgets/QApplication>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QDialog>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QWidget>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <typeinfo>
#include <vector>

namespace
{

// For debugging layouts: the window tree under hwnd, with each window's
// class, id, visibility, text and rectangle relative to the main window.
void DumpWindows(HWND hwnd, HWND origin, int depth)
{
	TCHAR cls[64] = {}, text[64] = {};
	::GetClassName(hwnd, cls, 64);
	::GetWindowText(hwnd, text, 64);
	RECT rc{};
	::GetWindowRect(hwnd, &rc);
	POINT topLeft{rc.left, rc.top};
	::ScreenToClient(origin, &topLeft);
	std::fprintf(stderr, "%*s%ls id=%d %s \"%ls\" (%d,%d %dx%d)\n", depth * 2, "", cls, ::GetDlgCtrlID(hwnd),
		::IsWindowVisible(hwnd) ? "visible" : "hidden", text, (int)topLeft.x, (int)topLeft.y, (int)(rc.right - rc.left), (int)(rc.bottom - rc.top));
	for(HWND child = ::GetWindow(hwnd, GW_CHILD); child != nullptr; child = ::GetWindow(child, GW_HWNDNEXT))
		DumpWindows(child, origin, depth + 1);
}

// Visible top-level windows besides the main window: dialogs (the tracker's
// own, and Qt's file, colour and font dialogs), floating bars, plugin
// editors.
std::vector<QWidget *> OtherWindows(QWidget *mainWidget)
{
	std::vector<QWidget *> windows;
	for(QWidget *w : QApplication::topLevelWidgets())
	{
		const Qt::WindowType type = w->windowType();
		if(w != mainWidget && w->isVisible() && type != Qt::ToolTip && type != Qt::Popup && type != Qt::Desktop)
			windows.push_back(w);
	}
	return windows;
}

// The first visible window under hwnd whose C++ class is called name
// (typeid names are mangled, with the name's length in front).
HWND FindByClass(HWND hwnd, const std::string &name)
{
	if(CWnd *wnd = CWnd::FromHandlePermanent(hwnd); wnd != nullptr && ::IsWindowVisible(hwnd))
	{
		const std::string mangled = typeid(*wnd).name();
		// "12CCtrlSamples", or "N7OpenMPT12CCtrlSamplesE" in a namespace.
		const std::string plain = std::to_string(name.size()) + name;
		const auto endsWith = [&mangled](const std::string &suffix) {
			return mangled.size() >= suffix.size() && mangled.compare(mangled.size() - suffix.size(), suffix.size(), suffix) == 0;
		};
		if(endsWith(plain) || endsWith(plain + "E"))
			return hwnd;
	}
	for(HWND child = ::GetWindow(hwnd, GW_CHILD); child != nullptr; child = ::GetWindow(child, GW_HWNDNEXT))
	{
		if(HWND found = FindByClass(child, name))
			return found;
	}
	return nullptr;
}

void CloseAll(QWidget *mainWidget, int attempt);

// A window's picture. A window with another toolkit's windows inside (a
// plugin editor) is taken from the screen, as Qt does not draw those.
QPixmap Grab(QWidget *w)
{
	if(w->property("linopenmptForeignWindows").toBool() && w->isVisible() && w->screen() != nullptr)
		return w->screen()->grabWindow(w->winId());
	return w->grab();
}

// For tests and screenshots, all driven by the environment:
// - LINOPENMPT_COMMANDS=id,id,... sends these menu commands (decimal ids,
//   as in resource.h) to the main window, half a second apart, once the
//   program has started; a dialog they open stays open. Class:id sends one
//   to the first visible window of that C++ class instead, and
//   Class:pageN shows page N of a property sheet of that class, and
//   Class:id=N selects item N of that window's combo box id (=last for the
//   last item) and notifies the window (CBN_SELCHANGE), as a user's pick.
// - LINOPENMPT_SCREENSHOT=file.png saves the main window after
//   LINOPENMPT_SCREENSHOT_DELAY milliseconds (3000 by default), and any
//   other window open then (a dialog) as file-1.png, file-2.png, ...
// - LINOPENMPT_DUMP_WINDOWS prints the window trees then.
// Then the other windows are cancelled and the main window is closed, as
// the user would.
void ScheduleTestHooks()
{
	const char *file = std::getenv("LINOPENMPT_SCREENSHOT");
	if(file == nullptr || *file == '\0')
		return;
	if(const char *commands = std::getenv("LINOPENMPT_COMMANDS"))
	{
		int when = 500;
		for(const QString &id : QString::fromLocal8Bit(commands).split(u',', Qt::SkipEmptyParts))
		{
			// "Class:id" sends the command to a window of that C++ class
			// instead (a view's control panel, whose toolbar buttons send
			// their commands to it).
			// Class:pageN shows page N of a property sheet (Setup).
			// Class:id=N picks item N of a combo box (=last: the last).
			const QStringList parts = id.trimmed().split(u':');
			QString last = parts.back();
			const bool page = last.startsWith(QLatin1String("page"));
			QString select;
			if(const int eq = last.indexOf(u'='); eq > 0)
			{
				select = last.mid(eq + 1);
				last = last.left(eq);
			}
			const UINT command = (page ? last.mid(4) : last).toUInt();
			const std::string target = parts.size() > 1 ? parts.front().toStdString() : std::string();
			QTimer::singleShot(when, [command, target, page, select]() {
				CWnd *mainWnd = AfxGetMainWnd();
				if(mainWnd == nullptr)
					return;
				if(target.empty())
				{
					mainWnd->PostMessage(WM_COMMAND, command);
					return;
				}
				// In the main window, or in a dialog it opened.
				HWND hwnd = FindByClass(mainWnd->GetSafeHwnd(), target);
				for(QWidget *w : QApplication::topLevelWidgets())
				{
					auto *win32 = dynamic_cast<linopenmpt::qtwin::Win32Widget *>(w);
					if(hwnd == nullptr && win32 != nullptr && w->isVisible())
						hwnd = FindByClass(linopenmpt::qtwin::WindowOf(win32), target);
				}
				if(hwnd == nullptr)
					std::fprintf(stderr, "linOpenMPT: no visible %s for command %u\n", target.c_str(), command);
				else if(page)
					::PostMessage(hwnd, PSM_SETCURSEL, command, 0);
				else if(!select.isEmpty())
				{
					HWND combo = ::GetDlgItem(hwnd, command);
					const int count = combo != nullptr ? static_cast<int>(::SendMessage(combo, CB_GETCOUNT, 0, 0)) : 0;
					const int item = select == QLatin1String("last") ? count - 1 : select.toInt();
					if(combo == nullptr || item < 0 || item >= count)
					{
						std::fprintf(stderr, "linOpenMPT: no item %s in combo box %u\n", select.toLocal8Bit().constData(), command);
						return;
					}
					::SendMessage(combo, CB_SETCURSEL, item, 0);
					::PostMessage(hwnd, WM_COMMAND, MAKEWPARAM(command, CBN_SELCHANGE), reinterpret_cast<LPARAM>(combo));
				}
				else
					::PostMessage(hwnd, WM_COMMAND, command, 0);
			});
			when += 500;
		}
	}
	const char *delayText = std::getenv("LINOPENMPT_SCREENSHOT_DELAY");
	const int delay = delayText != nullptr ? std::atoi(delayText) : 3000;
	const QString path = QString::fromLocal8Bit(file);
	QTimer::singleShot(delay, [path]() {
		CWnd *mainWnd = AfxGetMainWnd();
		QWidget *mainWidget = mainWnd != nullptr && mainWnd->GetSafeHwnd() != nullptr ? linopenmpt::qtwin::FromHandle(mainWnd->GetSafeHwnd()) : nullptr;
		const bool dump = std::getenv("LINOPENMPT_DUMP_WINDOWS") != nullptr;
		if(mainWidget != nullptr)
		{
			Grab(mainWidget).save(path);
			if(dump)
				DumpWindows(mainWnd->GetSafeHwnd(), mainWnd->GetSafeHwnd(), 0);
		}
		// The pictures of the other windows take the main one's format.
		const int dot = path.lastIndexOf(QLatin1Char('.'));
		const QString base = dot > 0 ? path.left(dot) : path;
		const QString suffix = dot > 0 ? path.mid(dot) : QStringLiteral(".png");
		int index = 0;
		for(QWidget *w : OtherWindows(mainWidget))
		{
			Grab(w).save(QStringLiteral("%1-%2%3").arg(base).arg(++index).arg(suffix));
			auto *win32 = dynamic_cast<linopenmpt::qtwin::Win32Widget *>(w);
			const HWND hwnd = win32 != nullptr ? linopenmpt::qtwin::WindowOf(win32) : nullptr;
			if(dump && hwnd != nullptr)
				DumpWindows(hwnd, hwnd, 0);
		}
		CloseAll(mainWidget, 0);
	});
}

// Cancels the other windows first (a modal dialog's loop would otherwise
// run the main window's close underneath it), then closes the main window.
void CloseAll(QWidget *mainWidget, int attempt)
{
	const std::vector<QWidget *> others = OtherWindows(mainWidget);
	if(!others.empty() && attempt < 20)
	{
		for(QWidget *w : others)
		{
			// The tracker's windows are Win32 windows; the rest are Qt's own
			// dialogs (message boxes, file dialogs).
			auto *win32 = dynamic_cast<linopenmpt::qtwin::Win32Widget *>(w);
			const HWND hwnd = win32 != nullptr ? linopenmpt::qtwin::WindowOf(win32) : nullptr;
			if(hwnd == nullptr)
			{
				// A message box answers with its Escape button (No,
				// Cancel), as the user would.
				auto *box = qobject_cast<QMessageBox *>(w);
				if(box != nullptr && box->escapeButton() != nullptr)
					box->escapeButton()->click();
				else if(auto *dialog = qobject_cast<QDialog *>(w))
					dialog->reject();
				else
					w->close();
				continue;
			}
			// A dialog takes IDCANCEL; a message box or another window, Escape.
			::PostMessage(hwnd, WM_COMMAND, IDCANCEL, 0);
			::PostMessage(hwnd, WM_KEYDOWN, VK_ESCAPE, 0);
		}
		QTimer::singleShot(250, [mainWidget, attempt]() { CloseAll(mainWidget, attempt + 1); });
		return;
	}
	// What the commands changed is not saved: no "save changes?" on close.
	if(CWinApp *app = AfxGetApp(); app != nullptr && app->m_pDocManager != nullptr)
	{
		POSITION templatePos = app->m_pDocManager->GetFirstDocTemplatePosition();
		while(templatePos != nullptr)
		{
			CDocTemplate *docTemplate = app->m_pDocManager->GetNextDocTemplate(templatePos);
			POSITION docPos = docTemplate->GetFirstDocPosition();
			while(docPos != nullptr)
			{
				if(auto *modDoc = dynamic_cast<CModDoc *>(docTemplate->GetNextDoc(docPos)))
					modDoc->SetModified(false);
			}
		}
	}
	if(CWnd *mainWnd = AfxGetMainWnd())
		mainWnd->PostMessage(WM_CLOSE);
}

} // namespace

int main(int argc, char *argv[])
{
	// Plugin editors (Linux VST2) are X11 windows that need an X11 window to
	// live in, so on a Wayland desktop the tracker runs through XWayland, as
	// other Linux plugin hosts do. A platform the user picks still wins.
	if(std::getenv("QT_QPA_PLATFORM") == nullptr && std::getenv("DISPLAY") != nullptr)
		::setenv("QT_QPA_PLATFORM", "xcb", 0);
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

	// Before InitInstance, so that a dialog shown while starting (say, for
	// samples a module refers to that are missing) is captured too.
	ScheduleTestHooks();
	if(!app->InitInstance())
	{
		const int code = app->ExitInstance();
		return code != 0 ? code : 1;
	}
	return app->Run();
}
