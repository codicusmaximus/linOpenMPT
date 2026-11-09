// The "ComboBox" window class.
//
// Built as Windows builds it, from an "Edit" child (CBS_DROPDOWN and
// CBS_SIMPLE) and a "ComboLBox" list (listbox.cpp) that holds the items, so
// CB_* messages, the CBN_* notifications, owner drawing (the parent gets
// WM_MEASUREITEM and WM_DRAWITEM with ODT_COMBOBOX) and GetComboBoxInfo()
// behave as there. The dropped-down list is a Qt popup: it closes when the
// user clicks elsewhere, and while it is open the keyboard goes to it, which
// passes the keys that matter back here.
#include "QtWin.h"

#include <QtGui/QGuiApplication>
#include <QtGui/QScreen>
#include <QtWidgets/QApplication>
#include <QtWidgets/QStyle>
#include <QtWidgets/QStyleOption>

#include <algorithm>
#include <cwctype>

namespace linopenmpt::qtwin
{

void ComboListNotify(HWND combo, HWND list, UINT code);
bool ComboListKey(HWND combo, UINT msg, WPARAM wp, LPARAM lp);

namespace
{

constexpr int EditId = 1001; // as on Windows: GetDlgItem(combo, 1001)
constexpr int ListId = 1000;
constexpr int Border = 3;    // around the selection field

struct ComboState : ControlState
{
	HWND list = nullptr;
	HWND edit = nullptr;
	WNDPROC editProc = nullptr;
	int selection = -1;     // what the selection field shows
	int fieldHeight = 15;   // CB_GETITEMHEIGHT(-1)
	int dropHeight = 100;   // the list's height from the creation size
	int droppedWidth = 0;
	int minVisible = 30;
	bool dropped = false;
	bool focused = false;
	bool pressed = false;   // the arrow button, while the mouse holds it
	bool hot = false;
	bool extendedUI = false;
	bool settingText = false; // our own edit updates: no CBN_EDITCHANGE
	DWORD listScroll = 0;   // WS_VSCROLL / WS_HSCROLL, for the list
	std::wstring cue;
	std::wstring typed;     // CBS_DROPDOWNLIST type-ahead
	DWORD typedTime = 0;
};

ComboState *State(HWND hwnd)
{
	return Control<ComboState>(hwnd);
}

LONG_PTR Style(HWND hwnd)
{
	return ::GetWindowLongPtr(hwnd, GWL_STYLE);
}

UINT Type(HWND hwnd)
{
	return static_cast<UINT>(Style(hwnd) & 3);
}

bool HasEdit(HWND hwnd)
{
	return Type(hwnd) != CBS_DROPDOWNLIST;
}

bool OwnerDraw(HWND hwnd)
{
	return (Style(hwnd) & (CBS_OWNERDRAWFIXED | CBS_OWNERDRAWVARIABLE)) != 0;
}

bool HasStrings(HWND hwnd)
{
	return !OwnerDraw(hwnd) || (Style(hwnd) & CBS_HASSTRINGS);
}

HFONT ComboFont(HWND hwnd)
{
	const WindowData *d = Data(hwnd);
	return d != nullptr && d->font != nullptr ? d->font : static_cast<HFONT>(::GetStockObject(DEFAULT_GUI_FONT));
}

int FontHeight(HWND hwnd)
{
	HDC dc = ::CreateCompatibleDC(nullptr);
	HGDIOBJ old = ::SelectObject(dc, ComboFont(hwnd));
	TEXTMETRIC tm{};
	::GetTextMetrics(dc, &tm);
	::SelectObject(dc, old);
	::DeleteDC(dc);
	return std::max(1, static_cast<int>(tm.tmHeight));
}

RECT Client(HWND hwnd)
{
	RECT rc{};
	::GetClientRect(hwnd, &rc);
	return rc;
}

QRect ToQRect(const RECT &r)
{
	return QRect(QPoint(r.left, r.top), QPoint(r.right - 1, r.bottom - 1));
}

RECT ToRECT(const QRect &r)
{
	return RECT{r.left(), r.top(), r.right() + 1, r.bottom() + 1};
}

// The height of the combo box itself (without the dropped-down list).
int ClosedHeight(const ComboState &s)
{
	return s.fieldHeight + 2 * Border;
}

void InitOption(HWND hwnd, const ComboState &s, QStyleOptionComboBox &opt)
{
	Win32Widget *w = FromHandle(hwnd);
	if(w != nullptr)
		opt.initFrom(w);
	const RECT rc = Client(hwnd);
	opt.rect = QRect(0, 0, rc.right, Type(hwnd) == CBS_SIMPLE ? ClosedHeight(s) : rc.bottom);
	opt.editable = HasEdit(hwnd);
	opt.frame = true;
	opt.state &= ~(QStyle::State_HasFocus | QStyle::State_MouseOver | QStyle::State_On | QStyle::State_Sunken);
	const bool enabled = ::IsWindowEnabled(hwnd) != FALSE;
	if(enabled)
		opt.state |= QStyle::State_Enabled;
	else
		opt.state &= ~QStyle::State_Enabled;
	if(s.focused)
		opt.state |= QStyle::State_HasFocus | QStyle::State_KeyboardFocusChange;
	if(s.hot && enabled)
		opt.state |= QStyle::State_MouseOver;
	if(s.dropped)
		opt.state |= QStyle::State_On;
	opt.subControls = QStyle::SC_All;
	opt.activeSubControls = s.pressed ? QStyle::SC_ComboBoxArrow : QStyle::SC_None;
	if(s.pressed)
		opt.state |= QStyle::State_Sunken;
}

RECT FieldRect(HWND hwnd, const ComboState &s)
{
	if(Type(hwnd) == CBS_SIMPLE)
	{
		const RECT rc = Client(hwnd);
		return RECT{0, 0, rc.right, ClosedHeight(s)};
	}
	QStyleOptionComboBox opt;
	InitOption(hwnd, s, opt);
	QRect r = QApplication::style()->subControlRect(QStyle::CC_ComboBox, &opt, QStyle::SC_ComboBoxEditField, FromHandle(hwnd));
	if(r.isEmpty())
		r = opt.rect.adjusted(Border, Border, -Border - ::GetSystemMetrics(SM_CXVSCROLL), -Border);
	return ToRECT(r);
}

RECT ButtonRect(HWND hwnd, const ComboState &s)
{
	if(Type(hwnd) == CBS_SIMPLE)
		return RECT{0, 0, 0, 0};
	QStyleOptionComboBox opt;
	InitOption(hwnd, s, opt);
	QRect r = QApplication::style()->subControlRect(QStyle::CC_ComboBox, &opt, QStyle::SC_ComboBoxArrow, FromHandle(hwnd));
	if(r.isEmpty())
	{
		const RECT rc = Client(hwnd);
		r = QRect(rc.right - ::GetSystemMetrics(SM_CXVSCROLL) - Border, Border, ::GetSystemMetrics(SM_CXVSCROLL), rc.bottom - 2 * Border);
	}
	return ToRECT(r);
}

void Layout(HWND hwnd, ComboState &s)
{
	const RECT rc = Client(hwnd);
	if(Type(hwnd) == CBS_SIMPLE)
	{
		const int top = ClosedHeight(s);
		if(s.edit != nullptr)
			::MoveWindow(s.edit, 0, 0, rc.right, top, TRUE);
		if(s.list != nullptr)
			::MoveWindow(s.list, 0, top, rc.right, std::max(0, static_cast<int>(rc.bottom) - top), TRUE);
		return;
	}
	if(s.edit != nullptr)
	{
		const RECT field = FieldRect(hwnd, s);
		::MoveWindow(s.edit, field.left, field.top, field.right - field.left, field.bottom - field.top, TRUE);
	}
}

// Keeps a CBS_DROPDOWN(LIST) combo box at its closed height.
void FitHeight(HWND hwnd, ComboState &s)
{
	if(Type(hwnd) == CBS_SIMPLE)
		return;
	RECT rc{};
	::GetWindowRect(hwnd, &rc);
	const int height = ClosedHeight(s);
	if(rc.bottom - rc.top != height)
		::SetWindowPos(hwnd, nullptr, 0, 0, rc.right - rc.left, height, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

std::wstring ItemText(const ComboState &s, int index)
{
	if(s.list == nullptr || index < 0)
		return {};
	const LRESULT len = ::SendMessage(s.list, LB_GETTEXTLEN, static_cast<WPARAM>(index), 0);
	if(len == LB_ERR)
		return {};
	std::wstring text(static_cast<std::size_t>(len) + 1, L'\0');
	::SendMessage(s.list, LB_GETTEXT, static_cast<WPARAM>(index), reinterpret_cast<LPARAM>(text.data()));
	text.resize(std::wcslen(text.c_str()));
	return text;
}

int ItemCount(const ComboState &s)
{
	return s.list != nullptr ? static_cast<int>(::SendMessage(s.list, LB_GETCOUNT, 0, 0)) : 0;
}

void Redraw(HWND hwnd)
{
	::InvalidateRect(hwnd, nullptr, TRUE);
}

void SetEditText(HWND hwnd, ComboState &s, const std::wstring &text, bool selectAll)
{
	if(s.edit == nullptr)
		return;
	s.settingText = true;
	::SetWindowText(s.edit, text.c_str());
	if(ComboState *again = State(hwnd))
	{
		again->settingText = false;
		if(selectAll && again->edit != nullptr)
			::SendMessage(again->edit, EM_SETSEL, 0, -1);
	}
}

// Shows the list's selection in the selection field.
void UpdateField(HWND hwnd, ComboState &s)
{
	if(s.edit != nullptr)
	{
		if(HasStrings(hwnd))
			SetEditText(hwnd, s, ItemText(s, s.selection), s.focused);
	} else
	{
		Redraw(hwnd);
	}
}

// The user picked an item: the field shows it and the parent is told.
void Select(HWND hwnd, ComboState &s, int index, bool notify)
{
	if(s.list == nullptr)
		return;
	::SendMessage(s.list, LB_SETCURSEL, static_cast<WPARAM>(index), 0);
	ComboState *state = State(hwnd);
	if(state == nullptr)
		return;
	const bool changed = state->selection != index;
	state->selection = index;
	UpdateField(hwnd, *state);
	if(changed && notify)
		NotifyParent(hwnd, CBN_SELCHANGE);
}

int CurrentListSelection(const ComboState &s)
{
	return s.list != nullptr ? static_cast<int>(::SendMessage(s.list, LB_GETCURSEL, 0, 0)) : -1;
}

RECT DroppedRect(HWND hwnd, const ComboState &s)
{
	RECT rc{};
	::GetWindowRect(hwnd, &rc);
	const int width = std::max(static_cast<int>(rc.right - rc.left), s.droppedWidth);
	int height;
	const int count = ItemCount(s);
	if(Style(hwnd) & CBS_NOINTEGRALHEIGHT)
	{
		height = s.dropHeight;
	} else
	{
		const int visible = std::clamp(count, 1, std::max(1, s.minVisible));
		height = 0;
		if(Style(hwnd) & CBS_OWNERDRAWVARIABLE)
		{
			for(int i = 0; i < visible; i++)
				height += static_cast<int>(::SendMessage(s.list, LB_GETITEMHEIGHT, static_cast<WPARAM>(i), 0));
		} else
		{
			height = visible * static_cast<int>(::SendMessage(s.list, LB_GETITEMHEIGHT, 0, 0));
		}
	}
	height = std::max(height, static_cast<int>(::SendMessage(s.list, LB_GETITEMHEIGHT, 0, 0))) + 2; // WS_BORDER
	RECT dropped{rc.left, rc.bottom, rc.left + width, rc.bottom + height};
	// Above the combo box when it does not fit below, as on Windows.
	QScreen *screen = QGuiApplication::screenAt(QPoint(rc.left, rc.top));
	if(screen == nullptr)
		screen = QGuiApplication::primaryScreen();
	if(screen != nullptr)
	{
		const QRect avail = screen->availableGeometry();
		if(dropped.bottom > avail.bottom() + 1 && rc.top - height >= avail.top())
		{
			dropped.top = rc.top - height;
			dropped.bottom = rc.top;
		}
		if(dropped.right > avail.right() + 1)
		{
			const int shift = std::min(static_cast<int>(dropped.right - avail.right() - 1), static_cast<int>(dropped.left - avail.left()));
			if(shift > 0)
			{
				dropped.left -= shift;
				dropped.right -= shift;
			}
		}
	}
	return dropped;
}

void DropDown(HWND hwnd)
{
	ComboState *s = State(hwnd);
	if(s == nullptr || s->dropped || Type(hwnd) == CBS_SIMPLE || s->list == nullptr)
		return;
	if(::GetFocus() != hwnd && (s->edit == nullptr || ::GetFocus() != s->edit))
		::SetFocus(hwnd);
	NotifyParent(hwnd, CBN_DROPDOWN);
	if((s = State(hwnd)) == nullptr || s->dropped)
		return;
	// A drop-down combo box's list starts at the item its text names.
	int index = s->selection;
	if(s->edit != nullptr && HasStrings(hwnd))
	{
		const int len = ::GetWindowTextLength(s->edit);
		std::wstring text(static_cast<std::size_t>(len) + 1, L'\0');
		::GetWindowText(s->edit, text.data(), len + 1);
		text.resize(static_cast<std::size_t>(len));
		index = text.empty() ? -1 : static_cast<int>(::SendMessage(s->list, LB_FINDSTRINGEXACT, static_cast<WPARAM>(-1), reinterpret_cast<LPARAM>(text.c_str())));
		if(index < 0 && !text.empty())
			index = static_cast<int>(::SendMessage(s->list, LB_FINDSTRING, static_cast<WPARAM>(-1), reinterpret_cast<LPARAM>(text.c_str())));
	}
	::SendMessage(s->list, LB_SETCURSEL, static_cast<WPARAM>(index), 0);
	if(index >= 0)
		::SendMessage(s->list, LB_SETTOPINDEX, static_cast<WPARAM>(index), 0);
	const RECT rc = DroppedRect(hwnd, *s);
	s->dropped = true;
	::SetWindowPos(s->list, HWND_TOPMOST, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, SWP_NOACTIVATE);
	if(index >= 0)
		::SendMessage(s->list, LB_SETTOPINDEX, static_cast<WPARAM>(index), 0);
	::ShowWindow(s->list, SW_SHOWNA);
	Redraw(hwnd);
}

void CloseUp(HWND hwnd, bool ok)
{
	ComboState *s = State(hwnd);
	if(s == nullptr || !s->dropped)
		return;
	s->dropped = false;
	s->pressed = false;
	if(ok)
	{
		const int index = CurrentListSelection(*s);
		if(index != s->selection || (s->edit != nullptr && index >= 0))
			Select(hwnd, *s, index, index != s->selection);
		if((s = State(hwnd)) == nullptr)
			return;
		NotifyParent(hwnd, CBN_SELENDOK);
	} else
	{
		::SendMessage(s->list, LB_SETCURSEL, static_cast<WPARAM>(s->selection), 0);
		NotifyParent(hwnd, CBN_SELENDCANCEL);
	}
	if((s = State(hwnd)) == nullptr)
		return;
	::ShowWindow(s->list, SW_HIDE);
	Redraw(hwnd);
	NotifyParent(hwnd, CBN_CLOSEUP);
}

// Up, Down, Page Up/Down, Home and End on a closed combo box.
bool MoveSelection(HWND hwnd, ComboState &s, WPARAM vk)
{
	const int count = ItemCount(s);
	if(count == 0)
		return vk == VK_UP || vk == VK_DOWN || vk == VK_PRIOR || vk == VK_NEXT || vk == VK_HOME || vk == VK_END;
	const int page = std::max(1, std::min(count, s.minVisible) - 1);
	int index = s.selection;
	switch(vk)
	{
	case VK_UP: index = index < 0 ? 0 : index - 1; break;
	case VK_DOWN: index = index < 0 ? 0 : index + 1; break;
	case VK_PRIOR: index = index < 0 ? 0 : index - page; break;
	case VK_NEXT: index = index < 0 ? 0 : index + page; break;
	case VK_HOME: index = 0; break;
	case VK_END: index = count - 1; break;
	default: return false;
	}
	index = std::clamp(index, 0, count - 1);
	if(index != s.selection)
		Select(hwnd, s, index, true);
	return true;
}

// CBS_DROPDOWNLIST type-ahead: the next item starting with what was typed.
void TypeAhead(HWND hwnd, ComboState &s, WCHAR ch, bool dropped)
{
	if(ch < L' ' || s.list == nullptr)
		return;
	const DWORD now = ::GetTickCount();
	if(now - s.typedTime > 1000)
		s.typed.clear();
	s.typedTime = now;
	s.typed += ch;
	int start = dropped ? CurrentListSelection(s) : s.selection;
	// Repeating one letter cycles through the items starting with it.
	const bool repeat = s.typed.size() > 1 && std::all_of(s.typed.begin(), s.typed.end(), [&](wchar_t c) { return std::towlower(c) == std::towlower(s.typed[0]); });
	const std::wstring key = repeat ? std::wstring(1, ch) : s.typed;
	if(s.typed.size() > 1 && !repeat)
		start--; // the current item may match the longer prefix
	const int index = static_cast<int>(::SendMessage(s.list, LB_FINDSTRING, static_cast<WPARAM>(start), reinterpret_cast<LPARAM>(key.c_str())));
	if(index < 0)
		return;
	if(dropped)
	{
		::SendMessage(s.list, LB_SETCURSEL, static_cast<WPARAM>(index), 0);
		if(ComboState *again = State(hwnd))
			Select(hwnd, *again, index, true);
	} else if(index != s.selection)
	{
		Select(hwnd, s, index, true);
	}
}

// Keys the combo box (or its edit) gets while closed; true if used.
bool KeyDown(HWND hwnd, ComboState &s, UINT msg, WPARAM vk)
{
	const bool alt = msg == WM_SYSKEYDOWN;
	if(Type(hwnd) == CBS_SIMPLE)
	{
		if(alt)
			return false;
		// The list scrolls under the edit.
		if(vk == VK_UP || vk == VK_DOWN || vk == VK_PRIOR || vk == VK_NEXT)
			return MoveSelection(hwnd, s, vk);
		return false;
	}
	if(vk == VK_F4 && !alt)
	{
		DropDown(hwnd);
		return true;
	}
	if(alt && (vk == VK_DOWN || vk == VK_UP))
	{
		DropDown(hwnd);
		return true;
	}
	if(alt)
		return false;
	if(vk == VK_DOWN && s.extendedUI)
	{
		DropDown(hwnd);
		return true;
	}
	if(s.edit != nullptr && (vk == VK_HOME || vk == VK_END))
		return false; // the edit's
	return MoveSelection(hwnd, s, vk);
}

LRESULT CALLBACK EditSubclassProc(HWND edit, UINT msg, WPARAM wp, LPARAM lp)
{
	const HWND combo = ::GetParent(edit);
	ComboState *s = State(combo);
	if(s == nullptr || s->editProc == nullptr)
		return ::DefWindowProc(edit, msg, wp, lp);
	const WNDPROC proc = s->editProc;
	switch(msg)
	{
	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
		if(KeyDown(combo, *s, msg, wp))
			return 0;
		break;
	case WM_CHAR:
		// Enter and Escape are the dialog's, Tab moves the focus.
		break;
	default:
		break;
	}
	return ::CallWindowProc(proc, edit, msg, wp, lp);
}

void PaintField(HWND hwnd, ComboState &s, HDC dc)
{
	const bool enabled = ::IsWindowEnabled(hwnd) != FALSE;
	RECT field = FieldRect(hwnd, s);
	const bool highlight = s.focused && !s.dropped;
	if(OwnerDraw(hwnd))
	{
		DRAWITEMSTRUCT dis{};
		dis.CtlType = ODT_COMBOBOX;
		dis.CtlID = static_cast<UINT>(::GetDlgCtrlID(hwnd));
		dis.itemID = static_cast<UINT>(s.selection);
		dis.itemAction = ODA_DRAWENTIRE;
		dis.itemState = ODS_COMBOBOXEDIT | (highlight ? ODS_SELECTED | ODS_FOCUS : 0) | (enabled ? 0 : ODS_DISABLED);
		dis.hwndItem = hwnd;
		dis.hDC = dc;
		dis.rcItem = field;
		dis.itemData = s.selection >= 0 ? static_cast<ULONG_PTR>(::SendMessage(s.list, LB_GETITEMDATA, static_cast<WPARAM>(s.selection), 0)) : 0;
		HBRUSH brush = ControlBrush(hwnd, dc, enabled ? WM_CTLCOLOREDIT : WM_CTLCOLORSTATIC);
		::FillRect(dc, &field, brush);
		::SendMessage(::GetParent(hwnd), WM_DRAWITEM, dis.CtlID, reinterpret_cast<LPARAM>(&dis));
		return;
	}
	HBRUSH brush = ControlBrush(hwnd, dc, enabled ? WM_CTLCOLOREDIT : WM_CTLCOLORSTATIC);
	(void)brush; // the style draws the field's background; only the colours are used
	const std::wstring text = ItemText(s, s.selection);
	RECT textRect = field;
	if(highlight)
	{
		::FillRect(dc, &field, ::GetSysColorBrush(COLOR_HIGHLIGHT));
		::SetTextColor(dc, ::GetSysColor(COLOR_HIGHLIGHTTEXT));
	} else if(!enabled)
	{
		::SetTextColor(dc, ::GetSysColor(COLOR_GRAYTEXT));
	}
	::SetBkMode(dc, TRANSPARENT);
	textRect.left += 2;
	::DrawText(dc, text.c_str(), static_cast<int>(text.size()), &textRect, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS);
	if(highlight)
	{
		RECT focus = field;
		::DrawFocusRect(dc, &focus);
	}
}

void Paint(HWND hwnd, HDC dc)
{
	ComboState *s = State(hwnd);
	Win32Widget *w = FromHandle(hwnd);
	if(s == nullptr || w == nullptr)
		return;
	const RECT rc = Client(hwnd);
	HBRUSH background = ControlBrush(hwnd, dc, WM_CTLCOLORSTATIC);
	if(Type(hwnd) == CBS_SIMPLE)
	{
		::FillRect(dc, &rc, background);
		return;
	}
	::FillRect(dc, &rc, background);
	if(QPainter *p = DCPainter(dc))
	{
		QStyleOptionComboBox opt;
		InitOption(hwnd, *s, opt);
		p->save();
		QApplication::style()->drawComplexControl(QStyle::CC_ComboBox, &opt, p, w);
		p->restore();
	}
	if(s->edit == nullptr)
	{
		HGDIOBJ oldFont = ::SelectObject(dc, ComboFont(hwnd));
		PaintField(hwnd, *s, dc);
		::SelectObject(dc, oldFont);
	}
}

void Create(HWND hwnd, const CREATESTRUCT *cs)
{
	ComboState *s = State(hwnd);
	if(s == nullptr)
		return;
	const LONG_PTR style = Style(hwnd);
	const UINT type = Type(hwnd);
	s->fieldHeight = FontHeight(hwnd) + 2;
	if(style & CBS_OWNERDRAWFIXED || style & CBS_OWNERDRAWVARIABLE)
	{
		// The selection field's height (itemID -1).
		MEASUREITEMSTRUCT mis{};
		mis.CtlType = ODT_COMBOBOX;
		mis.CtlID = static_cast<UINT>(::GetDlgCtrlID(hwnd));
		mis.itemID = static_cast<UINT>(-1);
		mis.itemWidth = static_cast<UINT>(cs != nullptr ? cs->cx : 0);
		mis.itemHeight = static_cast<UINT>(s->fieldHeight);
		::SendMessage(::GetParent(hwnd), WM_MEASUREITEM, mis.CtlID, reinterpret_cast<LPARAM>(&mis));
		if((s = State(hwnd)) == nullptr)
			return;
		s->fieldHeight = std::max(1, static_cast<int>(mis.itemHeight));
	}
	const int total = cs != nullptr ? cs->cy : ClosedHeight(*s);
	s->dropHeight = std::max(0, total - ClosedHeight(*s));

	DWORD listStyle = s->listScroll | LBS_NOTIFY | LBS_COMBOBOX;
	if(style & CBS_OWNERDRAWFIXED)
		listStyle |= LBS_OWNERDRAWFIXED;
	if(style & CBS_OWNERDRAWVARIABLE)
		listStyle |= LBS_OWNERDRAWVARIABLE;
	if(style & CBS_HASSTRINGS)
		listStyle |= LBS_HASSTRINGS;
	if(style & CBS_SORT)
		listStyle |= LBS_SORT;
	if(style & CBS_DISABLENOSCROLL)
		listStyle |= LBS_DISABLENOSCROLL;
	listStyle |= LBS_NOINTEGRALHEIGHT;
	const RECT rc = Client(hwnd);
	if(type == CBS_SIMPLE)
	{
		listStyle |= WS_CHILD | WS_VISIBLE | WS_BORDER;
		s->list = ::CreateWindowEx(0, L"ComboLBox", nullptr, listStyle, 0, ClosedHeight(*s), rc.right, std::max(0, static_cast<int>(rc.bottom) - ClosedHeight(*s)), hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ListId)), nullptr, hwnd);
	} else
	{
		listStyle |= WS_POPUP | WS_BORDER;
		s->list = ::CreateWindowEx(WS_EX_TOOLWINDOW | WS_EX_TOPMOST, L"ComboLBox", nullptr, listStyle, 0, 0, rc.right, 100, hwnd, nullptr, nullptr, hwnd);
		if(Win32Widget *lw = FromHandle(s->list))
		{
			// A Qt popup: closes when clicked elsewhere, and that click
			// goes nowhere else (on Windows the list has the capture).
			lw->setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
			lw->setAttribute(Qt::WA_NoMouseReplay);
			lw->hide();
		}
	}
	if((s = State(hwnd)) == nullptr)
		return;
	if(s->list != nullptr)
		::SendMessage(s->list, WM_SETFONT, reinterpret_cast<WPARAM>(ComboFont(hwnd)), FALSE);
	if(type != CBS_DROPDOWNLIST)
	{
		DWORD editStyle = WS_CHILD | WS_VISIBLE | ES_LEFT | ES_NOHIDESEL;
		if(style & CBS_AUTOHSCROLL)
			editStyle |= ES_AUTOHSCROLL;
		if(style & CBS_UPPERCASE)
			editStyle |= ES_UPPERCASE;
		if(style & CBS_LOWERCASE)
			editStyle |= ES_LOWERCASE;
		if(style & CBS_OEMCONVERT)
			editStyle |= ES_OEMCONVERT;
		if(type == CBS_SIMPLE)
			editStyle |= WS_BORDER;
		if(!(style & WS_DISABLED))
			editStyle &= ~WS_DISABLED;
		else
			editStyle |= WS_DISABLED;
		HWND edit = ::CreateWindowEx(0, L"Edit", cs != nullptr ? cs->lpszName : nullptr, editStyle, 0, 0, 10, 10, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(EditId)), nullptr, nullptr);
		if((s = State(hwnd)) == nullptr)
			return;
		s->edit = edit;
		if(edit != nullptr)
		{
			::SendMessage(edit, WM_SETFONT, reinterpret_cast<WPARAM>(ComboFont(hwnd)), FALSE);
			s->editProc = reinterpret_cast<WNDPROC>(::SetWindowLongPtr(edit, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(EditSubclassProc)));
		}
	}
	FitHeight(hwnd, *s);
	if((s = State(hwnd)) != nullptr)
		Layout(hwnd, *s);
}

void FocusIn(HWND hwnd, ComboState &s)
{
	if(s.focused)
		return;
	s.focused = true;
	if(s.edit != nullptr)
		::SendMessage(s.edit, EM_SETSEL, 0, -1);
	Redraw(hwnd);
	NotifyParent(hwnd, CBN_SETFOCUS);
}

void FocusOut(HWND hwnd, ComboState &s, HWND next)
{
	if(!s.focused || next == hwnd || (next != nullptr && next == s.edit) || (next != nullptr && next == s.list))
		return;
	if(s.dropped)
		CloseUp(hwnd, false);
	ComboState *state = State(hwnd);
	if(state == nullptr)
		return;
	state->focused = false;
	if(state->edit != nullptr)
		::SendMessage(state->edit, EM_SETSEL, static_cast<WPARAM>(-1), 0);
	Redraw(hwnd);
	NotifyParent(hwnd, CBN_KILLFOCUS);
}

bool ButtonHit(HWND hwnd, const ComboState &s, LPARAM lp)
{
	if(Type(hwnd) == CBS_DROPDOWNLIST)
		return true;
	const RECT button = ButtonRect(hwnd, s);
	const POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
	return ::PtInRect(&button, pt) != FALSE;
}

LRESULT ListMessage(const ComboState &s, UINT msg, WPARAM wp, LPARAM lp)
{
	return s.list != nullptr ? ::SendMessage(s.list, msg, wp, lp) : CB_ERR;
}

LRESULT CALLBACK ComboBoxProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	ComboState *s = State(hwnd);
	if(s == nullptr && msg != WM_NCCREATE)
		return ::DefWindowProc(hwnd, msg, wp, lp);
	switch(msg)
	{
	case WM_NCCREATE:
		if(WindowData *d = Data(hwnd))
		{
			auto state = std::make_unique<ComboState>();
			// The list gets the scroll bars, and the style draws the frame.
			state->listScroll = d->style & (WS_VSCROLL | WS_HSCROLL);
			d->control = std::move(state);
			d->style &= ~(WS_BORDER | WS_VSCROLL | WS_HSCROLL);
			d->scroll[SB_HORZ].shown = d->scroll[SB_VERT].shown = false;
			d->exStyle &= ~WS_EX_CLIENTEDGE;
		}
		return ::DefWindowProc(hwnd, msg, wp, lp);
	case WM_CREATE:
		Create(hwnd, reinterpret_cast<const CREATESTRUCT *>(lp));
		return 0;
	case WM_DESTROY:
		if(s->list != nullptr && !(::GetWindowLongPtr(s->list, GWL_STYLE) & WS_CHILD))
		{
			const HWND list = s->list;
			s->list = nullptr;
			s->dropped = false;
			::DestroyWindow(list);
		}
		return 0;
	case WM_NCDESTROY:
		if(WindowData *d = Data(hwnd))
			d->control.reset();
		return ::DefWindowProc(hwnd, msg, wp, lp);
	case WM_WINDOWPOSCHANGING:
		if(Type(hwnd) != CBS_SIMPLE)
		{
			// Windows takes a combo box's height as its dropped-down height.
			auto *pos = reinterpret_cast<WINDOWPOS *>(lp);
			if(!(pos->flags & SWP_NOSIZE))
			{
				const int closed = ClosedHeight(*s);
				if(pos->cy > closed)
					s->dropHeight = pos->cy - closed;
				pos->cy = closed;
			}
		}
		return 0;
	case WM_SIZE:
		Layout(hwnd, *s);
		return 0;
	case WM_SETFONT:
		::DefWindowProc(hwnd, msg, wp, 0);
		if(s->edit != nullptr)
			::SendMessage(s->edit, WM_SETFONT, wp, FALSE);
		if(s->list != nullptr)
			::SendMessage(s->list, WM_SETFONT, wp, FALSE);
		if((s = State(hwnd)) == nullptr)
			return 0;
		if(!OwnerDraw(hwnd))
		{
			s->fieldHeight = FontHeight(hwnd) + 2;
			if(Type(hwnd) == CBS_SIMPLE)
				Layout(hwnd, *s);
			else
				FitHeight(hwnd, *s);
		}
		if((s = State(hwnd)) != nullptr)
			Layout(hwnd, *s);
		if(lp)
			Redraw(hwnd);
		return 0;
	case WM_GETFONT:
		return reinterpret_cast<LRESULT>(ComboFont(hwnd));
	case WM_ENABLE:
		if(s->edit != nullptr)
			::EnableWindow(s->edit, wp != 0);
		if(s->list != nullptr)
			::EnableWindow(s->list, wp != 0);
		Redraw(hwnd);
		return 0;
	case WM_ERASEBKGND:
		return 1;
	case WM_PAINT:
	case WM_PRINTCLIENT:
		if(msg == WM_PRINTCLIENT)
		{
			Paint(hwnd, reinterpret_cast<HDC>(wp));
		} else
		{
			PAINTSTRUCT ps;
			HDC dc = ::BeginPaint(hwnd, &ps);
			if(dc != nullptr)
				Paint(hwnd, dc);
			::EndPaint(hwnd, &ps);
		}
		return 0;
	case WM_GETDLGCODE:
		{
			LRESULT code = DLGC_WANTARROWS | DLGC_WANTCHARS;
			if(s->dropped && lp != 0)
			{
				const MSG *m = reinterpret_cast<const MSG *>(lp);
				if(m->message == WM_KEYDOWN && (m->wParam == VK_RETURN || m->wParam == VK_ESCAPE))
					code |= DLGC_WANTMESSAGE;
			}
			return code;
		}
	case WM_SETFOCUS:
		if(s->edit != nullptr)
		{
			::SetFocus(s->edit);
			return 0;
		}
		FocusIn(hwnd, *s);
		return 0;
	case WM_KILLFOCUS:
		if(s->edit == nullptr)
			FocusOut(hwnd, *s, reinterpret_cast<HWND>(wp));
		return 0;
	case WM_LBUTTONDOWN:
	case WM_LBUTTONDBLCLK:
		if(!ButtonHit(hwnd, *s, lp))
			return 0;
		if(::GetFocus() != hwnd && (s->edit == nullptr || ::GetFocus() != s->edit))
			::SetFocus(hwnd);
		if((s = State(hwnd)) == nullptr)
			return 0;
		if(s->dropped)
		{
			CloseUp(hwnd, true);
		} else
		{
			s->pressed = true;
			Redraw(hwnd);
			DropDown(hwnd);
		}
		return 0;
	case WM_LBUTTONUP:
		if(s->pressed)
		{
			s->pressed = false;
			Redraw(hwnd);
		}
		return 0;
	case WM_MOUSEMOVE:
		if(!s->hot)
		{
			s->hot = true;
			TRACKMOUSEEVENT tme{sizeof(tme), TME_LEAVE, hwnd, 0};
			::TrackMouseEvent(&tme);
			Redraw(hwnd);
		}
		return 0;
	case WM_MOUSELEAVE:
		s->hot = false;
		Redraw(hwnd);
		return 0;
	case WM_MOUSEWHEEL:
		if(!s->dropped && s->focused && Type(hwnd) != CBS_SIMPLE)
		{
			const int delta = static_cast<short>(HIWORD(wp));
			if(delta != 0)
				MoveSelection(hwnd, *s, delta > 0 ? VK_UP : VK_DOWN);
			return 0;
		}
		return ::DefWindowProc(hwnd, msg, wp, lp);
	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
		if(KeyDown(hwnd, *s, msg, wp))
			return 0;
		return ::DefWindowProc(hwnd, msg, wp, lp);
	case WM_CHAR:
		if(s->edit != nullptr)
			return ::SendMessage(s->edit, msg, wp, lp);
		TypeAhead(hwnd, *s, static_cast<WCHAR>(wp), false);
		return 0;
	case WM_COMMAND:
		if(reinterpret_cast<HWND>(lp) == s->edit && s->edit != nullptr)
		{
			switch(HIWORD(wp))
			{
			case EN_SETFOCUS:
				FocusIn(hwnd, *s);
				break;
			case EN_KILLFOCUS:
				FocusOut(hwnd, *s, ::GetFocus());
				break;
			case EN_CHANGE:
				if(!s->settingText)
				{
					// Typing picks the matching item of an open list.
					if(s->dropped && HasStrings(hwnd))
					{
						const int len = ::GetWindowTextLength(s->edit);
						std::wstring text(static_cast<std::size_t>(len) + 1, L'\0');
						::GetWindowText(s->edit, text.data(), len + 1);
						text.resize(static_cast<std::size_t>(len));
						const int index = text.empty() ? -1 : static_cast<int>(::SendMessage(s->list, LB_FINDSTRING, static_cast<WPARAM>(-1), reinterpret_cast<LPARAM>(text.c_str())));
						::SendMessage(s->list, LB_SETCURSEL, static_cast<WPARAM>(index), 0);
						if(index >= 0)
							::SendMessage(s->list, LB_SETTOPINDEX, static_cast<WPARAM>(index), 0);
					}
					if((s = State(hwnd)) != nullptr)
						s->selection = -1;
					NotifyParent(hwnd, CBN_EDITCHANGE);
				}
				break;
			case EN_UPDATE:
				if(!s->settingText)
					NotifyParent(hwnd, CBN_EDITUPDATE);
				break;
			case EN_ERRSPACE:
				NotifyParent(hwnd, static_cast<UINT>(CBN_ERRSPACE));
				break;
			default:
				break;
			}
		}
		return 0;
	case WM_CTLCOLOREDIT:
	case WM_CTLCOLORLISTBOX:
	case WM_CTLCOLORSTATIC:
	case WM_CTLCOLORBTN:
	case WM_CTLCOLORSCROLLBAR:
	case WM_CTLCOLORMSGBOX:
	case WM_CTLCOLORDLG:
		if(HWND parent = ::GetParent(hwnd))
		{
			const LRESULT r = ::SendMessage(parent, msg, wp, lp);
			if(r != 0)
				return r;
		}
		return DefCtlColor(reinterpret_cast<HDC>(wp), msg);
	case WM_MEASUREITEM:
	case WM_DRAWITEM:
	case WM_DELETEITEM:
	case WM_COMPAREITEM:
		// From the list, already in the combo box's name.
		if(HWND parent = ::GetParent(hwnd))
			return ::SendMessage(parent, msg, wp, lp);
		return 0;
	case WM_SETTEXT:
		if(s->edit != nullptr)
		{
			SetEditText(hwnd, *s, lp ? reinterpret_cast<LPCWSTR>(lp) : L"", false);
			if((s = State(hwnd)) != nullptr)
				s->selection = -1;
			::DefWindowProc(hwnd, msg, wp, lp);
			return TRUE;
		} else
		{
			const LRESULT index = lp != 0 && HasStrings(hwnd) ? ListMessage(*s, LB_FINDSTRINGEXACT, static_cast<WPARAM>(-1), lp) : LB_ERR;
			if(index == LB_ERR)
				return CB_ERR;
			Select(hwnd, *s, static_cast<int>(index), false);
			return TRUE;
		}
	case WM_GETTEXT:
	case WM_GETTEXTLENGTH:
		if(s->edit != nullptr)
			return ::SendMessage(s->edit, msg, wp, lp);
		{
			const std::wstring text = HasStrings(hwnd) ? ItemText(*s, s->selection) : std::wstring();
			if(msg == WM_GETTEXTLENGTH)
				return static_cast<LRESULT>(text.size());
			if(wp == 0 || lp == 0)
				return 0;
			const std::size_t n = std::min(text.size(), static_cast<std::size_t>(wp) - 1);
			std::copy_n(text.c_str(), n, reinterpret_cast<LPWSTR>(lp));
			reinterpret_cast<LPWSTR>(lp)[n] = 0;
			return static_cast<LRESULT>(n);
		}
	case WM_CUT:
	case WM_COPY:
	case WM_PASTE:
	case WM_CLEAR:
	case WM_UNDO:
		return s->edit != nullptr ? ::SendMessage(s->edit, msg, wp, lp) : 0;

	case CB_ADDSTRING:
		return ListMessage(*s, LB_ADDSTRING, wp, lp);
	case CB_INSERTSTRING:
		{
			const LRESULT index = ListMessage(*s, LB_INSERTSTRING, wp, lp);
			if((s = State(hwnd)) != nullptr && index >= 0 && s->selection >= index)
				s->selection = CurrentListSelection(*s);
			return index;
		}
	case CB_DELETESTRING:
		{
			const LRESULT count = ListMessage(*s, LB_DELETESTRING, wp, lp);
			if((s = State(hwnd)) != nullptr && count != LB_ERR)
			{
				const int sel = CurrentListSelection(*s);
				if(sel != s->selection)
				{
					s->selection = sel;
					if(s->edit == nullptr)
						Redraw(hwnd);
				}
			}
			return count;
		}
	case CB_RESETCONTENT:
		ListMessage(*s, LB_RESETCONTENT, 0, 0);
		if((s = State(hwnd)) != nullptr)
		{
			s->selection = -1;
			if(s->edit != nullptr)
				SetEditText(hwnd, *s, std::wstring(), false);
			else
				Redraw(hwnd);
		}
		return CB_OKAY;
	case CB_GETCOUNT:
		return ListMessage(*s, LB_GETCOUNT, 0, 0);
	case CB_GETLBTEXT:
		return ListMessage(*s, LB_GETTEXT, wp, lp);
	case CB_GETLBTEXTLEN:
		return ListMessage(*s, LB_GETTEXTLEN, wp, lp);
	case CB_GETITEMDATA:
		return ListMessage(*s, LB_GETITEMDATA, wp, lp);
	case CB_SETITEMDATA:
		{
			const LRESULT r = ListMessage(*s, LB_SETITEMDATA, wp, lp);
			if((s = State(hwnd)) != nullptr && OwnerDraw(hwnd) && static_cast<int>(wp) == s->selection)
				Redraw(hwnd);
			return r;
		}
	case CB_FINDSTRING:
		return ListMessage(*s, LB_FINDSTRING, wp, lp);
	case CB_FINDSTRINGEXACT:
		return ListMessage(*s, LB_FINDSTRINGEXACT, wp, lp);
	case CB_SELECTSTRING:
		{
			const LRESULT index = ListMessage(*s, LB_FINDSTRING, wp, lp);
			if(index == LB_ERR)
				return CB_ERR;
			if((s = State(hwnd)) != nullptr)
				Select(hwnd, *s, static_cast<int>(index), false);
			return index;
		}
	case CB_GETCURSEL:
		return s->dropped ? CurrentListSelection(*s) : (s->selection >= 0 ? s->selection : CB_ERR);
	case CB_SETCURSEL:
		{
			const int count = ItemCount(*s);
			const int index = static_cast<int>(wp);
			if(index >= count)
				return CB_ERR;
			Select(hwnd, *s, index < 0 ? -1 : index, false);
			return index < 0 ? CB_ERR : index;
		}
	case CB_GETTOPINDEX:
		return ListMessage(*s, LB_GETTOPINDEX, 0, 0);
	case CB_SETTOPINDEX:
		return ListMessage(*s, LB_SETTOPINDEX, wp, 0) == LB_ERR ? CB_ERR : CB_OKAY;
	case CB_GETHORIZONTALEXTENT:
		return ListMessage(*s, LB_GETHORIZONTALEXTENT, 0, 0);
	case CB_SETHORIZONTALEXTENT:
		ListMessage(*s, LB_SETHORIZONTALEXTENT, wp, 0);
		return 0;
	case CB_SETLOCALE:
		return ListMessage(*s, LB_SETLOCALE, wp, 0);
	case CB_GETLOCALE:
		return ListMessage(*s, LB_GETLOCALE, 0, 0);
	case CB_INITSTORAGE:
		return ListMessage(*s, LB_INITSTORAGE, wp, lp);
	case CB_DIR:
		return ListMessage(*s, LB_DIR, wp, lp);
	case CB_SHOWDROPDOWN:
		if(wp)
			DropDown(hwnd);
		else
			CloseUp(hwnd, false);
		return TRUE;
	case CB_GETDROPPEDSTATE:
		return s->dropped ? TRUE : FALSE;
	case CB_GETDROPPEDCONTROLRECT:
		if(lp != 0)
		{
			auto *rc = reinterpret_cast<RECT *>(lp);
			if(Type(hwnd) == CBS_SIMPLE)
			{
				::GetWindowRect(hwnd, rc);
			} else
			{
				RECT combo{};
				::GetWindowRect(hwnd, &combo);
				const RECT list = DroppedRect(hwnd, *s);
				*rc = RECT{combo.left, combo.top, std::max(combo.right, list.right), combo.bottom + (list.bottom - list.top)};
			}
		}
		return CB_OKAY;
	case CB_SETDROPPEDWIDTH:
		if(static_cast<int>(wp) < 0)
			return CB_ERR;
		s->droppedWidth = static_cast<int>(wp);
		[[fallthrough]];
	case CB_GETDROPPEDWIDTH:
		{
			RECT rc{};
			::GetWindowRect(hwnd, &rc);
			return std::max(s->droppedWidth, static_cast<int>(rc.right - rc.left));
		}
	case CB_SETEXTENDEDUI:
		s->extendedUI = wp != 0;
		return CB_OKAY;
	case CB_GETEXTENDEDUI:
		return s->extendedUI ? TRUE : FALSE;
	case CB_GETITEMHEIGHT:
		if(static_cast<int>(wp) == -1)
			return s->fieldHeight;
		return ListMessage(*s, LB_GETITEMHEIGHT, wp, 0);
	case CB_SETITEMHEIGHT:
		if(static_cast<int>(wp) == -1)
		{
			const int height = LOWORD(lp);
			if(height <= 0 || height > 255)
				return CB_ERR;
			s->fieldHeight = height;
			FitHeight(hwnd, *s);
			if((s = State(hwnd)) != nullptr)
				Layout(hwnd, *s);
			Redraw(hwnd);
			return CB_OKAY;
		}
		return ListMessage(*s, LB_SETITEMHEIGHT, wp, lp);
	case CB_LIMITTEXT:
		if(s->edit != nullptr)
			::SendMessage(s->edit, EM_LIMITTEXT, wp, 0);
		return TRUE;
	case CB_GETEDITSEL:
		if(s->edit == nullptr)
			return CB_ERR;
		return ::SendMessage(s->edit, EM_GETSEL, wp, lp);
	case CB_SETEDITSEL:
		if(s->edit == nullptr)
			return CB_ERR;
		::SendMessage(s->edit, EM_SETSEL, static_cast<WPARAM>(static_cast<short>(LOWORD(lp))), static_cast<LPARAM>(static_cast<short>(HIWORD(lp))));
		return TRUE;
	case CB_GETCOMBOBOXINFO:
		if(lp == 0)
			return FALSE;
		{
			auto *info = reinterpret_cast<COMBOBOXINFO *>(lp);
			info->rcItem = FieldRect(hwnd, *s);
			info->rcButton = ButtonRect(hwnd, *s);
			info->stateButton = Type(hwnd) == CBS_SIMPLE ? STATE_SYSTEM_INVISIBLE : (s->pressed ? STATE_SYSTEM_PRESSED : 0);
			info->hwndCombo = hwnd;
			info->hwndItem = s->edit != nullptr ? s->edit : hwnd;
			info->hwndList = s->list;
			return TRUE;
		}
	case CB_SETMINVISIBLE:
		if(static_cast<int>(wp) <= 0)
			return FALSE;
		s->minVisible = static_cast<int>(wp);
		return TRUE;
	case CB_GETMINVISIBLE:
		return s->minVisible;
	case CB_SETCUEBANNER:
		s->cue = lp ? reinterpret_cast<LPCWSTR>(lp) : L"";
		if(s->edit != nullptr)
			::SendMessage(s->edit, EM_SETCUEBANNER, 0, lp);
		Redraw(hwnd);
		return TRUE;
	case CB_GETCUEBANNER:
		if(lp == 0 || wp == 0)
			return FALSE;
		{
			const std::size_t n = std::min(s->cue.size(), static_cast<std::size_t>(wp) - 1);
			std::copy_n(s->cue.c_str(), n, reinterpret_cast<LPWSTR>(lp));
			reinterpret_cast<LPWSTR>(lp)[n] = 0;
			return TRUE;
		}
	default:
		return ::DefWindowProc(hwnd, msg, wp, lp);
	}
}

} // namespace

void ComboListNotify(HWND combo, HWND list, UINT code)
{
	(void)list;
	ComboState *s = State(combo);
	if(s == nullptr)
		return;
	switch(code)
	{
	case LBN_SELCHANGE:
		// The keyboard in the open list, or a click in a CBS_SIMPLE one.
		{
			const int index = CurrentListSelection(*s);
			if(index != s->selection)
			{
				s->selection = index;
				UpdateField(combo, *s);
				NotifyParent(combo, CBN_SELCHANGE);
			}
		}
		break;
	case LBN_DBLCLK:
		if(Type(combo) == CBS_SIMPLE)
			NotifyParent(combo, CBN_DBLCLK);
		break;
	case ComboChosen:
		if(s->dropped)
			CloseUp(combo, true);
		break;
	case ComboCancel:
		if(s->dropped)
		{
			// Clicking the combo box itself closes the list keeping what it shows.
			POINT pt{};
			::GetCursorPos(&pt);
			RECT rc{};
			::GetWindowRect(combo, &rc);
			CloseUp(combo, ::PtInRect(&rc, pt) != FALSE);
		}
		break;
	default:
		break;
	}
}

bool ComboListKey(HWND combo, UINT msg, WPARAM wp, LPARAM lp)
{
	ComboState *s = State(combo);
	if(s == nullptr)
		return false;
	if(!s->dropped)
	{
		// CBS_SIMPLE: its list gets no focus, but just in case.
		return false;
	}
	if(msg == WM_SYSKEYDOWN)
	{
		if(wp == VK_UP || wp == VK_DOWN)
		{
			CloseUp(combo, true);
			return true;
		}
		// Alt+F4 and menu keys: close first.
		CloseUp(combo, false);
		return false;
	}
	if(msg == WM_KEYDOWN)
	{
		switch(wp)
		{
		case VK_RETURN:
		case VK_F4:
			CloseUp(combo, true);
			return true;
		case VK_ESCAPE:
			CloseUp(combo, false);
			return true;
		case VK_TAB:
			CloseUp(combo, true);
			// The focus moves on, as on Windows.
			if(HWND dialog = ::GetParent(combo))
				::PostMessage(dialog, WM_NEXTDLGCTL, (::GetKeyState(VK_SHIFT) & 0x8000) ? 1 : 0, 0);
			return true;
		case VK_UP:
		case VK_DOWN:
		case VK_PRIOR:
		case VK_NEXT:
			return false; // the list moves its selection and tells us
		case VK_HOME:
		case VK_END:
		case VK_LEFT:
		case VK_RIGHT:
		case VK_DELETE:
		case VK_BACK:
			if(s->edit != nullptr)
			{
				::SendMessage(s->edit, msg, wp, lp);
				return true;
			}
			return wp == VK_LEFT || wp == VK_RIGHT || wp == VK_DELETE || wp == VK_BACK;
		default:
			if(s->edit != nullptr)
			{
				::SendMessage(s->edit, msg, wp, lp);
				return true;
			}
			return true;
		}
	}
	if(msg == WM_CHAR)
	{
		if(wp == VK_RETURN || wp == VK_ESCAPE || wp == VK_TAB)
			return true;
		if(s->edit != nullptr)
		{
			::SendMessage(s->edit, msg, wp, lp);
			return true;
		}
		TypeAhead(combo, *s, static_cast<WCHAR>(wp), true);
		return true;
	}
	return false;
}

void RegisterComboBoxClass()
{
	WNDCLASS wc{};
	wc.style = CS_DBLCLKS | CS_PARENTDC | CS_VREDRAW | CS_HREDRAW;
	wc.lpfnWndProc = ComboBoxProc;
	wc.hCursor = ::LoadCursor(nullptr, IDC_ARROW);
	wc.lpszClassName = L"ComboBox";
	::RegisterClass(&wc);
}

} // namespace linopenmpt::qtwin

using namespace linopenmpt::qtwin;

BOOL GetComboBoxInfo(HWND combo, PCOMBOBOXINFO info)
{
	if(info == nullptr || info->cbSize != sizeof(COMBOBOXINFO))
		return FALSE;
	return ::SendMessage(combo, CB_GETCOMBOBOXINFO, 0, reinterpret_cast<LPARAM>(info)) ? TRUE : FALSE;
}
