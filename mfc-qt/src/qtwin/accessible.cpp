// Accessibility (screen readers) for windows: every HWND is a Win32Widget,
// which Qt alone would describe as a nameless client area. As Windows' own
// accessibility proxies do for its windows, this one names and describes
// them from what they are: the role from the window class and style, the
// name from the window text (or, for an edit or list, the label before it),
// the value and the check state. A window that says more about itself (MFC's
// get_accName and friends, which upstream overrides for its pattern, sample
// and instrument editors) answers through a hook (SetAccessibleTextHook).
// NotifyWinEvent tells the screen reader that a window's name or value
// changed, as upstream does when the cursor moves.
#include "QtWin.h"

#include <QtGui/QAccessible>
#include <QtWidgets/QAccessibleWidget>

#include <cwctype>

namespace mfcqt::qtwin
{

namespace
{
AccessibleTextHook g_textHook = nullptr;

std::wstring ClassName(HWND hwnd)
{
	WCHAR name[64] = {};
	::GetClassName(hwnd, name, static_cast<int>(std::size(name)));
	return name;
}

bool IsClass(const std::wstring &cls, const wchar_t *name)
{
	if(cls.size() != std::wcslen(name))
		return false;
	for(std::size_t i = 0; i < cls.size(); i++)
	{
		if(std::towlower(cls[i]) != std::towlower(name[i]))
			return false;
	}
	return true;
}

QString WindowText(HWND hwnd)
{
	const int length = ::GetWindowTextLength(hwnd);
	std::wstring text(static_cast<std::size_t>(length) + 1, L'\0');
	text.resize(static_cast<std::size_t>(::GetWindowText(hwnd, text.data(), length + 1)));
	return QString::fromStdWString(text);
}

// Mnemonics ("&Open") read without the ampersand ("&&" is one).
QString WithoutMnemonic(QString text)
{
	QString out;
	for(int i = 0; i < text.size(); i++)
	{
		if(text[i] == u'&' && i + 1 < text.size())
			i++;
		out += text[i];
	}
	return out;
}

QAccessible::Role RoleOf(HWND hwnd)
{
	const std::wstring cls = ClassName(hwnd);
	const DWORD style = static_cast<DWORD>(::GetWindowLongPtr(hwnd, GWL_STYLE));
	if(IsClass(cls, L"Button"))
	{
		switch(style & BS_TYPEMASK)
		{
		case BS_CHECKBOX:
		case BS_AUTOCHECKBOX:
		case BS_3STATE:
		case BS_AUTO3STATE:
			return (style & BS_PUSHLIKE) ? QAccessible::Button : QAccessible::CheckBox;
		case BS_RADIOBUTTON:
		case BS_AUTORADIOBUTTON:
			return (style & BS_PUSHLIKE) ? QAccessible::Button : QAccessible::RadioButton;
		case BS_GROUPBOX:
			return QAccessible::Grouping;
		default:
			return QAccessible::Button;
		}
	}
	if(IsClass(cls, L"Static"))
		return QAccessible::StaticText;
	if(IsClass(cls, L"Edit"))
		return QAccessible::EditableText;
	if(IsClass(cls, L"ListBox") || IsClass(cls, WC_LISTVIEW))
		return QAccessible::List;
	if(IsClass(cls, L"ComboBox") || IsClass(cls, WC_COMBOBOXEX))
		return QAccessible::ComboBox;
	if(IsClass(cls, WC_TREEVIEW))
		return QAccessible::Tree;
	if(IsClass(cls, TRACKBAR_CLASS))
		return QAccessible::Slider;
	if(IsClass(cls, UPDOWN_CLASS))
		return QAccessible::SpinBox;
	if(IsClass(cls, PROGRESS_CLASS))
		return QAccessible::ProgressBar;
	if(IsClass(cls, WC_TABCONTROL))
		return QAccessible::PageTabList;
	if(IsClass(cls, TOOLBARCLASSNAME))
		return QAccessible::ToolBar;
	if(IsClass(cls, STATUSCLASSNAME))
		return QAccessible::StatusBar;
	if(IsClass(cls, WC_HEADER))
		return QAccessible::ColumnHeader;
	if(IsClass(cls, WC_LINK))
		return QAccessible::Link;
	if(IsClass(cls, L"ScrollBar"))
		return QAccessible::ScrollBar;
	if(IsClass(cls, L"#32770"))
		return QAccessible::Dialog;
	if(!(style & WS_CHILD))
		return QAccessible::Window;
	return QAccessible::Client;
}

// Controls named by the label before them, as Windows does: the nearest
// earlier sibling that is a static text.
bool NamedByLabel(QAccessible::Role role)
{
	switch(role)
	{
	case QAccessible::EditableText:
	case QAccessible::List:
	case QAccessible::ComboBox:
	case QAccessible::Tree:
	case QAccessible::Slider:
	case QAccessible::SpinBox:
	case QAccessible::ProgressBar:
		return true;
	default:
		return false;
	}
}

QString LabelBefore(HWND hwnd)
{
	for(HWND sibling = ::GetWindow(hwnd, GW_HWNDPREV); sibling != nullptr; sibling = ::GetWindow(sibling, GW_HWNDPREV))
	{
		if(!::IsWindowVisible(sibling))
			continue;
		if(IsClass(ClassName(sibling), L"Static"))
			return WithoutMnemonic(WindowText(sibling)).trimmed();
		return QString();
	}
	return QString();
}

class Win32Accessible : public QAccessibleWidget
{
public:
	explicit Win32Accessible(Win32Widget *widget)
		: QAccessibleWidget(widget, QAccessible::Client)
	{
	}

	HWND Handle() const { return static_cast<Win32Widget *>(widget())->hwnd(); }

	QAccessible::Role role() const override
	{
		return RoleOf(Handle());
	}

	QString text(QAccessible::Text t) const override
	{
		const HWND hwnd = Handle();
		int kind = -1;
		switch(t)
		{
		case QAccessible::Name: kind = 0; break;
		case QAccessible::Value: kind = 1; break;
		case QAccessible::Description: kind = 2; break;
		default: break;
		}
		std::wstring answer;
		if(kind >= 0 && g_textHook != nullptr && g_textHook(hwnd, kind, answer))
			return QString::fromStdWString(answer);
		const QAccessible::Role r = role();
		switch(t)
		{
		case QAccessible::Name:
			if(NamedByLabel(r))
				return LabelBefore(hwnd);
			if(r == QAccessible::Client || r == QAccessible::ScrollBar)
				return QAccessibleWidget::text(t);
			return WithoutMnemonic(WindowText(hwnd));
		case QAccessible::Value:
			if(r == QAccessible::EditableText || r == QAccessible::ComboBox)
				return WindowText(hwnd);
			if(r == QAccessible::Slider)
				return QString::number(::SendMessage(hwnd, TBM_GETPOS, 0, 0));
			return QString();
		default:
			return QAccessibleWidget::text(t);
		}
	}

	QAccessible::State state() const override
	{
		QAccessible::State s = QAccessibleWidget::state();
		const QAccessible::Role r = role();
		if(r == QAccessible::CheckBox || r == QAccessible::RadioButton)
		{
			s.checkable = true;
			const LRESULT check = ::SendMessage(Handle(), BM_GETCHECK, 0, 0);
			s.checked = check == BST_CHECKED;
			s.checkStateMixed = check == BST_INDETERMINATE;
		}
		if(r == QAccessible::EditableText)
		{
			s.editable = !(::GetWindowLongPtr(Handle(), GWL_STYLE) & ES_READONLY);
			s.readOnly = !s.editable;
			s.multiLine = (::GetWindowLongPtr(Handle(), GWL_STYLE) & ES_MULTILINE) != 0;
		}
		return s;
	}
};

QAccessibleInterface *Factory(const QString &, QObject *object)
{
	if(auto *w = dynamic_cast<Win32Widget *>(object))
		return new Win32Accessible(w);
	return nullptr;
}
} // namespace

void InstallAccessibility()
{
	static const bool installed = [] {
		QAccessible::installFactory(Factory);
		return true;
	}();
	(void)installed;
}

void DescribeFocusProxy(Win32Widget *w)
{
	QWidget *proxy = w->focusProxy();
	if(proxy == nullptr || !QAccessible::isActive())
		return;
	const Win32Accessible window(w);
	proxy->setAccessibleName(window.text(QAccessible::Name));
	proxy->setAccessibleDescription(window.text(QAccessible::Description));
}

void SetAccessibleTextHook(AccessibleTextHook hook)
{
	g_textHook = hook;
}

} // namespace mfcqt::qtwin

using namespace mfcqt::qtwin;

void MfcQtSetAccessibleTextHook(BOOL (*hook)(HWND hwnd, int kind, std::wstring &text))
{
	SetAccessibleTextHook(hook);
}

void NotifyWinEvent(DWORD event, HWND hwnd, LONG, LONG)
{
	Win32Widget *w = FromHandle(hwnd);
	if(w == nullptr || !QAccessible::isActive())
		return;
	switch(event)
	{
	case EVENT_OBJECT_FOCUS:
		{
			QAccessibleEvent e(w, QAccessible::Focus);
			QAccessible::updateAccessibility(&e);
		}
		break;
	case EVENT_OBJECT_SELECTION:
		{
			QAccessibleEvent e(w, QAccessible::Selection);
			QAccessible::updateAccessibility(&e);
		}
		break;
	case EVENT_OBJECT_NAMECHANGE:
		{
			QAccessibleEvent e(w, QAccessible::NameChanged);
			QAccessible::updateAccessibility(&e);
		}
		break;
	case EVENT_OBJECT_VALUECHANGE:
		if(QAccessibleInterface *iface = QAccessible::queryAccessibleInterface(w))
		{
			QAccessibleValueChangeEvent e(w, iface->text(QAccessible::Value));
			QAccessible::updateAccessibility(&e);
		}
		break;
	default:
		break;
	}
}
