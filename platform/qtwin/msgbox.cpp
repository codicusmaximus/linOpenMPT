// MessageBox() on QMessageBox: the desktop's own message box, with the
// buttons, icon, default button and return values of the Win32 one.
#include "QtWin.h"

#include <QtCore/QPointer>

#include <QtWidgets/QApplication>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>

#include <vector>

using namespace linopenmpt::qtwin;

int MessageBox(HWND owner, LPCTSTR text, LPCTSTR caption, UINT type)
{
	QWidget *parent = nullptr;
	if(Win32Widget *w = FromHandle(owner))
		parent = w->window();
	// On the heap: if the owner is destroyed while the box is up (as
	// DestroyWindow destroys owned windows on Windows), Qt deletes the box
	// with it and exec() returns.
	QPointer<QMessageBox> boxPtr = new QMessageBox(parent);
	QMessageBox &box = *boxPtr;
	box.setTextFormat(Qt::PlainText);
	box.setText(ToQString(text != nullptr ? text : L""));
	box.setWindowTitle(caption != nullptr ? ToQString(caption) : QStringLiteral("Error"));
	switch(type & MB_ICONMASK)
	{
	case MB_ICONERROR: box.setIcon(QMessageBox::Critical); break;
	case MB_ICONQUESTION: box.setIcon(QMessageBox::Question); break;
	case MB_ICONWARNING: box.setIcon(QMessageBox::Warning); break;
	case MB_ICONINFORMATION: box.setIcon(QMessageBox::Information); break;
	default: break;
	}

	struct Button
	{
		QMessageBox::StandardButton standard;
		const char *text; // custom buttons (Try Again, Continue)
		int id;
	};
	std::vector<Button> buttons;
	int escape = -1;
	switch(type & MB_TYPEMASK)
	{
	case MB_OKCANCEL:
		buttons = {{QMessageBox::Ok, nullptr, IDOK}, {QMessageBox::Cancel, nullptr, IDCANCEL}};
		escape = IDCANCEL;
		break;
	case MB_ABORTRETRYIGNORE:
		buttons = {{QMessageBox::Abort, nullptr, IDABORT}, {QMessageBox::Retry, nullptr, IDRETRY}, {QMessageBox::Ignore, nullptr, IDIGNORE}};
		break;
	case MB_YESNOCANCEL:
		buttons = {{QMessageBox::Yes, nullptr, IDYES}, {QMessageBox::No, nullptr, IDNO}, {QMessageBox::Cancel, nullptr, IDCANCEL}};
		escape = IDCANCEL;
		break;
	case MB_YESNO:
		buttons = {{QMessageBox::Yes, nullptr, IDYES}, {QMessageBox::No, nullptr, IDNO}};
		break;
	case MB_RETRYCANCEL:
		buttons = {{QMessageBox::Retry, nullptr, IDRETRY}, {QMessageBox::Cancel, nullptr, IDCANCEL}};
		escape = IDCANCEL;
		break;
	case MB_CANCELTRYCONTINUE:
		buttons = {{QMessageBox::Cancel, nullptr, IDCANCEL}, {QMessageBox::NoButton, "&Try Again", IDTRYAGAIN}, {QMessageBox::NoButton, "&Continue", IDCONTINUE}};
		escape = IDCANCEL;
		break;
	default:
		buttons = {{QMessageBox::Ok, nullptr, IDOK}};
		escape = IDOK;
		break;
	}
	std::vector<std::pair<QAbstractButton *, int>> made;
	for(const Button &b : buttons)
	{
		QAbstractButton *button = b.text != nullptr
			? static_cast<QAbstractButton *>(box.addButton(QString::fromUtf8(b.text), QMessageBox::AcceptRole))
			: static_cast<QAbstractButton *>(box.addButton(b.standard));
		made.emplace_back(button, b.id);
		if(b.id == escape)
			box.setEscapeButton(button);
	}
	const std::size_t defIndex = (type & MB_DEFMASK) >> 8;
	if(defIndex < made.size())
		box.setDefaultButton(qobject_cast<QPushButton *>(made[defIndex].first));
	if(type & (MB_TOPMOST | MB_SYSTEMMODAL))
		box.setWindowFlag(Qt::WindowStaysOnTopHint);

	// Keys and mouse go to the box; the owner is disabled, as on Windows.
	const bool ownerEnabled = owner != nullptr && ::IsWindowEnabled(owner);
	if(ownerEnabled)
		::EnableWindow(owner, FALSE);
	int result = escape >= 0 ? escape : made.front().second;
	{
		ForeignModalLoop loop;
		boxPtr->exec();
	}
	if(boxPtr.isNull())
		return result; // the owner went, and the box with it
	if(ownerEnabled && ::IsWindow(owner))
		::EnableWindow(owner, TRUE);
	// The owner is active again, as on Windows.
	if(parent != nullptr && ::IsWindow(owner) && parent->isVisible())
		parent->activateWindow();
	for(const auto &m : made)
	{
		if(box.clickedButton() == m.first)
			result = m.second;
	}
	delete boxPtr;
	return result;
}
