// mfcless: the tracker's entry point. Upstream's WinMain runs the one
// application object (theApp, in Mptrack.cpp): InitInstance, then the
// message loop, or ExitInstance at once if starting failed. This does the
// same on a QApplication.
//
// For tests and screenshots, driven by the environment as the MFC build's
// app/main.cpp is, so that both builds can be compared on the same steps:
// - LINOPENMPT_COMMANDS=id,id,... sends these menu commands (decimal ids,
//   as in resource.h) to the main window, half a second apart, once the
//   program has started. Class:id sends one to the first visible window of
//   that C++ class instead, Class:pageN shows page N of a property sheet of
//   that class, Class:id=N selects item N of that window's combo box id
//   (=last for the last item) as a user's pick, and Class:key=Name sends
//   that window the keyboard command of that name, as its shortcut would.
//   Class:click=x/y and Class:rclick=x/y click the left or right mouse
//   button at x/y in that window's client area.
// - LINOPENMPT_SCREENSHOT=file.png saves the main window after
//   LINOPENMPT_SCREENSHOT_DELAY milliseconds (3000 by default), and any
//   other window open then (a dialog) as file-1.png, file-2.png, ...
// - LINOPENMPT_DUMP_WINDOWS prints the widget trees then.
// - LINOPENMPT_CRASH=ms crashes the program (SIGSEGV) that many
//   milliseconds after starting, for the crash handler's test.
// Then the other windows are cancelled and the main window is closed.

#include "stdafx.h"
#include <QtCore/QPointer>
#include "../platform/CrashRescue.h"
#include "CommandSet.h"
#include "DialogBase.h"
#include "InputHandler.h"
#include "Mainfrm.h"
#include "Moddoc.h"
#include "Mptrack.h"
#include "WindowMessages.h"
#include "../soundlib/mod_specifications.h"

#include "mfcless/ModMessage.h"
#include "mfcless/QtString.h"
#include "mfcless/Window.h"

#include <QtCore/QTimer>
#include <QtGui/QPixmap>
#include <QtGui/QScreen>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QWidget>

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <typeinfo>
#include <vector>


namespace
{

QWidget *MainWidget()
{
	return CMainFrame::GetMainFrame();
}

// Visible top-level windows besides the main window.
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

// For debugging layouts: the widget tree, with each widget's class,
// visibility, text and rectangle relative to the top-level window.
void DumpWidgets(QWidget *w, QWidget *origin, int depth)
{
	const QPoint topLeft = w->mapTo(origin, QPoint(0, 0));
	QString text = w->windowTitle();
	if(auto *button = qobject_cast<QAbstractButton *>(w))
		text = button->text();
	else if(auto *edit = qobject_cast<QLineEdit *>(w))
		text = edit->text();
	else if(auto *label = qobject_cast<QLabel *>(w))
		text = label->text();
	std::fprintf(stderr, "%*s%p %s (%s) %s%s%s \"%s\" (%d,%d %dx%d)\n", depth * 2, "", (void *)w, typeid(*w).name(), w->metaObject()->className(),
		w->isVisible() ? "visible" : "hidden", w->isEnabled() ? "" : " disabled", w == QApplication::focusWidget() ? " focus" : "", text.left(40).toLocal8Bit().constData(), topLeft.x(), topLeft.y(), w->width(), w->height());
	for(QObject *child : w->children())
	{
		if(auto *cw = qobject_cast<QWidget *>(child); cw && !cw->isWindow())
			DumpWidgets(cw, origin, depth + 1);
	}
}

// The first visible widget under w whose C++ class is called name
// (typeid names are mangled, with the name's length in front).
QWidget *FindByClass(QWidget *w, const std::string &name)
{
	if(w->isVisible())
	{
		const std::string mangled = typeid(*w).name();
		const std::string plain = std::to_string(name.size()) + name;
		const auto endsWith = [&mangled](const std::string &suffix) {
			return mangled.size() >= suffix.size() && mangled.compare(mangled.size() - suffix.size(), suffix.size(), suffix) == 0;
		};
		if(endsWith(plain) || endsWith(plain + "E"))
			return w;
	}
	for(QObject *child : w->children())
	{
		if(auto *cw = qobject_cast<QWidget *>(child))
		{
			if(QWidget *found = FindByClass(cw, name))
				return found;
		}
	}
	return nullptr;
}

// The first visible window of that C++ class, in the main window or in a
// window it opened.
QWidget *FindTestTarget(const std::string &target)
{
	QWidget *mainWidget = MainWidget();
	if(!mainWidget)
		return nullptr;
	if(QWidget *found = FindByClass(mainWidget, target))
		return found;
	for(QWidget *w : OtherWindows(mainWidget))
	{
		if(QWidget *found = FindByClass(w, target))
			return found;
	}
	return nullptr;
}

// A mouse click at pos in a window's client area (a scroll area's viewport).
void SendClick(const std::string &target, QPoint pos, bool right)
{
	QWidget *w = FindTestTarget(target);
	if(w == nullptr)
	{
		std::fprintf(stderr, "openmpt-qt: no visible %s for click\n", target.c_str());
		return;
	}
	if(auto *area = qobject_cast<QAbstractScrollArea *>(w))
		w = area->viewport();
	const Qt::MouseButton button = right ? Qt::RightButton : Qt::LeftButton;
	const QPointF local(pos), global(w->mapToGlobal(pos));
	QMouseEvent press(QEvent::MouseButtonPress, local, global, button, button, Qt::NoModifier);
	// The press may run a context menu's loop, in which the test closes
	// the program.
	const QPointer<QWidget> guard(w);
	QApplication::sendEvent(w, &press);
	if(!guard)
		return;
	QMouseEvent release(QEvent::MouseButtonRelease, local, global, button, Qt::NoButton, Qt::NoModifier);
	QApplication::sendEvent(w, &release);
}

// What the input handler sends a window when a shortcut is pressed.
void SendKeyCommand(const std::string &target, const QString &name)
{
	CInputHandler *input = CMainFrame::GetInputHandler();
	if(input == nullptr || input->m_activeCommandSet == nullptr)
		return;
	const CCommandSet &commands = *input->m_activeCommandSet;
	const mpt::ustring text = mfcless::FromQString(name);
	for(int cmd = 0; cmd < kcNumCommands; cmd++)
	{
		if(commands.GetCommandText(static_cast<CommandID>(cmd)) != text)
			continue;
		QWidget *w = FindTestTarget(target);
		if(w == nullptr)
		{
			std::fprintf(stderr, "openmpt-qt: no visible %s for key command %s\n", target.c_str(), name.toLocal8Bit().constData());
			return;
		}
		KeyCombination combination;
		if(commands.GetKeyListSize(static_cast<CommandID>(cmd)) > 0)
			combination = commands.GetKey(static_cast<CommandID>(cmd), 0);
		mfcless::SendModMessage(w, WM_MOD_KEYCOMMAND, cmd, combination.AsLPARAM());
		return;
	}
	std::fprintf(stderr, "openmpt-qt: no key command %s\n", name.toLocal8Bit().constData());
}

// When the tracker crashes: the modified modules, saved as upstream's
// crash handler does (numbered, named after their titles).
int RescueModifiedModules(const mpt::PathString &folder)
{
	int saved = 0;
	for(CModDoc *modDoc : theApp.GetOpenDocuments())
	{
		if(!modDoc->IsModified())
			continue;
		const mpt::PathString extension = mpt::PathString::FromUnicode(modDoc->GetSoundFile().GetModSpecifications().GetFileExtension());
		mpt::PathString title = mpt::PathString::FromUnicode(modDoc->GetTitle()).AsSanitizedComponent();
		if(!mpt::PathCompareNoCase(title.GetFilenameExtension(), P_(".") + extension))
			title = title.GetFilenameBase();
		const mpt::PathString file = folder + mpt::PathString::FromUnicode(mpt::ufmt::val(saved + 1)) + P_("_") + title + P_(".") + extension;
		try
		{
			if(modDoc->SaveFile(file, false))
				saved++;
		} catch(...)
		{
		}
	}
	return saved;
}

void CloseAll(QWidget *mainWidget, int attempt);

// A window's picture. A window with another toolkit's windows inside (a
// plugin editor) is taken from the screen, as Qt does not draw those.
QPixmap Grab(QWidget *w)
{
	if(w->property("mfclessForeignWindows").toBool() && w->isVisible() && w->screen() != nullptr)
		return w->screen()->grabWindow(w->winId());
	return w->grab();
}

void ScheduleTestHooks()
{
	const char *file = std::getenv("LINOPENMPT_SCREENSHOT");
	if(file == nullptr || *file == '\0')
		return;
	// A headless X server has no window manager to give the main window the
	// keyboard focus, as a desktop does.
	QTimer::singleShot(200, [] {
		if(QWidget *mainWidget = MainWidget())
			mainWidget->activateWindow();
	});
	if(const char *commands = std::getenv("LINOPENMPT_COMMANDS"))
	{
		int when = 500;
		for(const QString &id : QString::fromLocal8Bit(commands).split(u',', Qt::SkipEmptyParts))
		{
			const QStringList parts = id.trimmed().split(u':');
			QString last = parts.back();
			if((last.startsWith(QLatin1String("click=")) || last.startsWith(QLatin1String("rclick="))) && parts.size() > 1)
			{
				const bool right = last.startsWith(u'r');
				const QStringList xy = last.mid(last.indexOf(u'=') + 1).split(u'/');
				const QPoint pos(xy.value(0).toInt(), xy.value(1).toInt());
				const std::string target = parts.front().toStdString();
				QTimer::singleShot(when, [right, pos, target]() { SendClick(target, pos, right); });
				when += 500;
				continue;
			}
			if(last.startsWith(QLatin1String("key=")) && parts.size() > 1)
			{
				const QString name = last.mid(4);
				const std::string target = parts.front().toStdString();
				QTimer::singleShot(when, [name, target]() { SendKeyCommand(target, name); });
				when += 500;
				continue;
			}
			const bool page = last.startsWith(QLatin1String("page"));
			QString select;
			if(const int eq = last.indexOf(u'='); eq > 0)
			{
				select = last.mid(eq + 1);
				last = last.left(eq);
			}
			const uint32 command = (page ? last.mid(4) : last).toUInt();
			const std::string target = parts.size() > 1 ? parts.front().toStdString() : std::string();
			QTimer::singleShot(when, [command, target, page, select]() {
				QWidget *mainWidget = MainWidget();
				if(mainWidget == nullptr)
					return;
				if(target.empty())
				{
					mfcless::PostCommand(mainWidget, command);
					return;
				}
				QWidget *w = FindTestTarget(target);
				if(w == nullptr)
				{
					std::fprintf(stderr, "openmpt-qt: no visible %s for command %u\n", target.c_str(), command);
				} else if(page)
				{
					if(auto *sheet = dynamic_cast<PropertySheet *>(w))
						sheet->SetActivePage(static_cast<int>(command));
				} else if(!select.isEmpty())
				{
					auto *dlg = dynamic_cast<DialogBase *>(w);
					QComboBox *combo = dlg ? dlg->Item<QComboBox>(command) : nullptr;
					const int count = combo != nullptr ? combo->count() : 0;
					const int item = select == QLatin1String("last") ? count - 1 : select.toInt();
					if(combo == nullptr || item < 0 || item >= count)
					{
						std::fprintf(stderr, "openmpt-qt: no item %s in combo box %u\n", select.toLocal8Bit().constData(), command);
						return;
					}
					combo->setCurrentIndex(item);
					emit combo->activated(item);
				} else
				{
					mfcless::PostCommand(w, command);
				}
			});
			when += 500;
		}
	}
	if(const char *crash = std::getenv("LINOPENMPT_CRASH"))
		QTimer::singleShot(std::atoi(crash), []() { std::raise(SIGSEGV); });
	const char *delayText = std::getenv("LINOPENMPT_SCREENSHOT_DELAY");
	const int delay = delayText != nullptr ? std::atoi(delayText) : 3000;
	const QString path = QString::fromLocal8Bit(file);
	QTimer::singleShot(delay, [path]() {
		QWidget *mainWidget = MainWidget();
		const bool dump = std::getenv("LINOPENMPT_DUMP_WINDOWS") != nullptr;
		if(mainWidget != nullptr)
		{
			Grab(mainWidget).save(path);
			if(dump)
				DumpWidgets(mainWidget, mainWidget, 0);
		}
		const int dot = path.lastIndexOf(QLatin1Char('.'));
		const QString base = dot > 0 ? path.left(dot) : path;
		const QString suffix = dot > 0 ? path.mid(dot) : QStringLiteral(".png");
		int index = 0;
		for(QWidget *w : OtherWindows(mainWidget))
		{
			Grab(w).save(QStringLiteral("%1-%2%3").arg(base).arg(++index).arg(suffix));
			if(dump)
				DumpWidgets(w, w, 0);
		}
		// An open popup menu (a context menu) as file-menu.png; it is closed
		// first, as a click elsewhere would.
		for(QWidget *w : QApplication::topLevelWidgets())
		{
			if(w->isVisible() && w->windowType() == Qt::ToolTip)
				Grab(w).save(QStringLiteral("%1-tip%2").arg(base, suffix));
			if(w->isVisible() && w->windowType() == Qt::Popup)
			{
				Grab(w).save(QStringLiteral("%1-menu%2").arg(base, suffix));
				w->close();
			}
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
			auto *box = qobject_cast<QMessageBox *>(w);
			if(box != nullptr && box->escapeButton() != nullptr)
				box->escapeButton()->click();
			else if(auto *dialog = qobject_cast<QDialog *>(w))
				dialog->reject();
			else
				w->close();
		}
		QTimer::singleShot(250, [mainWidget, attempt]() { CloseAll(mainWidget, attempt + 1); });
		return;
	}
	// What the commands changed is not saved: no "save changes?" on close.
	for(CModDoc *modDoc : theApp.GetOpenDocuments())
		modDoc->SetModified(false);
	if(mainWidget)
		mainWidget->close();
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

	linopenmpt::SetCrashRescue(RescueModifiedModules);

	// The message loop's PreTranslateMessage pass, for every key and mouse
	// event of the application.
	mfcless::InstallMessageFilter();

	// Before InitInstance, so that a dialog shown while starting is
	// captured too.
	ScheduleTestHooks();
	if(!theApp.InitInstance())
	{
		const int code = theApp.ExitInstance();
		return code != 0 ? code : 1;
	}
	const int result = theApp.Run();
	theApp.ExitInstance();
	return result;
}
