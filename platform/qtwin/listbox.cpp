// The "ListBox" window class, and "ComboLBox", the list of a combo box.
//
// Drawn with GDI as Windows draws it (so owner-drawn list boxes, which
// upstream has several of, get WM_MEASUREITEM and WM_DRAWITEM as there),
// in the colours the parent picks with WM_CTLCOLORLISTBOX. Scrolling uses
// the window's own scroll bar (WS_VSCROLL).
#include "QtWin.h"

#include <QtWidgets/QApplication>

#include <algorithm>
#include <cwctype>

namespace linopenmpt::qtwin
{

struct ListBoxState;
// The combo box that owns a "ComboLBox" (combobox.cpp): list box
// notifications, plus ComboChosen and ComboCancel; and the keys the list gets
// while it is dropped (true if the combo box handled the key).
void ComboListNotify(HWND combo, HWND list, UINT code);
bool ComboListKey(HWND combo, UINT msg, WPARAM wp, LPARAM lp);

struct ListItem
{
	std::wstring text;
	ULONG_PTR data = 0;
	bool selected = false;
	int height = 0; // LBS_OWNERDRAWVARIABLE
};

struct ListBoxState : ControlState
{
	std::vector<ListItem> items;
	int top = 0;
	int caret = -1;   // the item with the focus rectangle
	int anchor = -1;  // where extended selections start
	int selected = -1; // single-selection list boxes
	int itemHeight = 16;
	int horizontalExtent = 0;
	int columnWidth = 0;
	int scrollX = 0;
	bool focused = false;
	bool capturing = false;
	bool comboList = false; // "ComboLBox"
	HWND combo = nullptr;   // its combo box
	bool quiet = false;     // mouse tracking in a combo box list: no LBN_SELCHANGE
	std::vector<int> tabStops; // in pixels
	LCID locale = 0x0409;
};

ListBoxState *ListState(HWND hwnd)
{
	return Control<ListBoxState>(hwnd);
}

namespace
{

LONG_PTR Style(HWND hwnd)
{
	return ::GetWindowLongPtr(hwnd, GWL_STYLE);
}

bool HasStrings(HWND hwnd)
{
	const LONG_PTR style = Style(hwnd);
	return !(style & (LBS_OWNERDRAWFIXED | LBS_OWNERDRAWVARIABLE)) || (style & LBS_HASSTRINGS);
}

bool OwnerDraw(HWND hwnd)
{
	return (Style(hwnd) & (LBS_OWNERDRAWFIXED | LBS_OWNERDRAWVARIABLE)) != 0;
}

bool MultiSel(HWND hwnd)
{
	return (Style(hwnd) & (LBS_MULTIPLESEL | LBS_EXTENDEDSEL)) != 0;
}

// The dropped-down list of a CBS_DROPDOWN(LIST) combo box (not CBS_SIMPLE's).
bool PopupList(HWND hwnd, const ListBoxState &s)
{
	return s.comboList && !(Style(hwnd) & WS_CHILD);
}

HWND NotifyTarget(HWND hwnd, const ListBoxState &s)
{
	return s.comboList ? s.combo : ::GetParent(hwnd);
}

UINT CtlType(const ListBoxState &s)
{
	return s.comboList ? ODT_COMBOBOX : ODT_LISTBOX;
}

UINT CtlId(HWND hwnd, const ListBoxState &s)
{
	return static_cast<UINT>(::GetDlgCtrlID(s.comboList ? s.combo : hwnd));
}

void Notify(HWND hwnd, ListBoxState &s, UINT code)
{
	if(s.comboList)
	{
		if(s.quiet && code == LBN_SELCHANGE)
			return;
		ComboListNotify(s.combo, hwnd, code);
		return;
	}
	if(!(Style(hwnd) & LBS_NOTIFY))
		return;
	NotifyParent(hwnd, code);
}

HFONT ListFont(HWND hwnd)
{
	const WindowData *d = Data(hwnd);
	return d != nullptr && d->font != nullptr ? d->font : static_cast<HFONT>(::GetStockObject(DEFAULT_GUI_FONT));
}

int FontHeight(HWND hwnd)
{
	HDC dc = ::CreateCompatibleDC(nullptr);
	HGDIOBJ old = ::SelectObject(dc, ListFont(hwnd));
	TEXTMETRIC tm{};
	::GetTextMetrics(dc, &tm);
	::SelectObject(dc, old);
	::DeleteDC(dc);
	return std::max(1, static_cast<int>(tm.tmHeight));
}

int ClientHeight(HWND hwnd)
{
	RECT rc{};
	::GetClientRect(hwnd, &rc);
	return rc.bottom;
}

int ItemHeight(HWND hwnd, const ListBoxState &s, int index)
{
	if((Style(hwnd) & LBS_OWNERDRAWVARIABLE) && index >= 0 && index < static_cast<int>(s.items.size()))
		return std::max(1, s.items[static_cast<std::size_t>(index)].height);
	return s.itemHeight;
}

// How many items fit from top down (at least one).
int VisibleCount(HWND hwnd, const ListBoxState &s, int from)
{
	const int height = ClientHeight(hwnd);
	if(!(Style(hwnd) & LBS_OWNERDRAWVARIABLE))
		return std::max(1, height / std::max(1, s.itemHeight));
	int y = 0, n = 0;
	for(int i = from; i < static_cast<int>(s.items.size()); i++)
	{
		y += ItemHeight(hwnd, s, i);
		if(y > height && n > 0)
			break;
		n++;
	}
	return std::max(1, n);
}

int MaxTop(HWND hwnd, const ListBoxState &s)
{
	const int count = static_cast<int>(s.items.size());
	if(!(Style(hwnd) & LBS_OWNERDRAWVARIABLE))
		return std::max(0, count - VisibleCount(hwnd, s, 0));
	// The first item from which the rest fits.
	int y = 0;
	const int height = ClientHeight(hwnd);
	for(int i = count - 1; i >= 0; i--)
	{
		y += ItemHeight(hwnd, s, i);
		if(y > height)
			return std::min(count - 1, i + 1);
	}
	return 0;
}

RECT ItemRect(HWND hwnd, const ListBoxState &s, int index)
{
	RECT rc{};
	::GetClientRect(hwnd, &rc);
	int y = 0;
	if(index >= s.top)
	{
		for(int i = s.top; i < index; i++)
			y += ItemHeight(hwnd, s, i);
	} else
	{
		for(int i = index; i < s.top; i++)
			y -= ItemHeight(hwnd, s, i);
	}
	rc.top = y;
	rc.bottom = y + ItemHeight(hwnd, s, index);
	if(s.horizontalExtent > rc.right)
		rc.right = s.horizontalExtent;
	::OffsetRect(&rc, -s.scrollX, 0);
	return rc;
}

int ItemFromPoint(HWND hwnd, const ListBoxState &s, int y, bool &outside)
{
	const int count = static_cast<int>(s.items.size());
	outside = y < 0 || y >= ClientHeight(hwnd);
	if(count == 0)
	{
		outside = true;
		return 0;
	}
	if(y < 0)
		return std::max(0, s.top - 1);
	int top = 0;
	for(int i = s.top; i < count; i++)
	{
		const int h = ItemHeight(hwnd, s, i);
		if(y < top + h)
			return i;
		top += h;
	}
	outside = true;
	return count - 1;
}

void UpdateScroll(HWND hwnd, ListBoxState &s)
{
	const LONG_PTR style = Style(hwnd);
	if(style & WS_VSCROLL)
	{
		SCROLLINFO si{sizeof(si), SIF_RANGE | SIF_PAGE | SIF_POS | ((style & LBS_DISABLENOSCROLL) ? SIF_DISABLENOSCROLL : 0u), 0, 0, 0, 0, 0};
		si.nMax = std::max(0, static_cast<int>(s.items.size()) - 1);
		si.nPage = static_cast<UINT>(VisibleCount(hwnd, s, s.top));
		si.nPos = s.top;
		::SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
	}
	if((style & WS_HSCROLL) && s.horizontalExtent > 0)
	{
		RECT rc{};
		::GetClientRect(hwnd, &rc);
		SCROLLINFO si{sizeof(si), SIF_RANGE | SIF_PAGE | SIF_POS | ((style & LBS_DISABLENOSCROLL) ? SIF_DISABLENOSCROLL : 0u), 0, s.horizontalExtent - 1, static_cast<UINT>(rc.right), s.scrollX, 0};
		::SetScrollInfo(hwnd, SB_HORZ, &si, TRUE);
	}
}

void Redraw(HWND hwnd)
{
	::InvalidateRect(hwnd, nullptr, TRUE);
}

void RedrawItem(HWND hwnd, const ListBoxState &s, int index)
{
	if(index < 0 || index >= static_cast<int>(s.items.size()))
		return;
	const RECT rc = ItemRect(hwnd, s, index);
	::InvalidateRect(hwnd, &rc, TRUE);
}

void SetTop(HWND hwnd, ListBoxState &s, int top)
{
	top = std::clamp(top, 0, MaxTop(hwnd, s));
	if(top == s.top)
		return;
	s.top = top;
	UpdateScroll(hwnd, s);
	Redraw(hwnd);
}

void EnsureVisible(HWND hwnd, ListBoxState &s, int index)
{
	if(index < 0 || index >= static_cast<int>(s.items.size()))
		return;
	if(index < s.top)
		SetTop(hwnd, s, index);
	else
	{
		const int visible = VisibleCount(hwnd, s, s.top);
		// Fully visible items only, as Windows scrolls.
		int fully = visible;
		if(!(Style(hwnd) & LBS_OWNERDRAWVARIABLE) && fully * s.itemHeight > ClientHeight(hwnd))
			fully = std::max(1, fully - 1);
		if(index >= s.top + fully)
		{
			if(Style(hwnd) & LBS_OWNERDRAWVARIABLE)
			{
				int top = index;
				int y = ItemHeight(hwnd, s, index);
				while(top > 0 && y + ItemHeight(hwnd, s, top - 1) <= ClientHeight(hwnd))
					y += ItemHeight(hwnd, s, --top);
				SetTop(hwnd, s, top);
			} else
			{
				SetTop(hwnd, s, index - fully + 1);
			}
		}
	}
}

void SetCaret(HWND hwnd, ListBoxState &s, int index)
{
	if(index == s.caret)
		return;
	const int old = s.caret;
	s.caret = index;
	if(s.focused)
	{
		RedrawItem(hwnd, s, old);
		RedrawItem(hwnd, s, index);
	}
}

// Single selection: index becomes the selection (-1: none).
bool SelectSingle(HWND hwnd, ListBoxState &s, int index)
{
	if(Style(hwnd) & LBS_NOSEL)
		return false;
	if(index == s.selected)
		return false;
	RedrawItem(hwnd, s, s.selected);
	if(s.selected >= 0 && s.selected < static_cast<int>(s.items.size()))
		s.items[static_cast<std::size_t>(s.selected)].selected = false;
	s.selected = index;
	if(index >= 0 && index < static_cast<int>(s.items.size()))
		s.items[static_cast<std::size_t>(index)].selected = true;
	RedrawItem(hwnd, s, index);
	return true;
}

bool SetItemSelected(HWND hwnd, ListBoxState &s, int index, bool selected)
{
	if(index < 0 || index >= static_cast<int>(s.items.size()) || (Style(hwnd) & LBS_NOSEL))
		return false;
	ListItem &item = s.items[static_cast<std::size_t>(index)];
	if(item.selected == selected)
		return false;
	item.selected = selected;
	RedrawItem(hwnd, s, index);
	return true;
}

bool SelectRange(HWND hwnd, ListBoxState &s, int first, int last, bool selected, bool clearOthers)
{
	if(first > last)
		std::swap(first, last);
	bool changed = false;
	for(int i = 0; i < static_cast<int>(s.items.size()); i++)
	{
		if(i >= first && i <= last)
			changed |= SetItemSelected(hwnd, s, i, selected);
		else if(clearOthers)
			changed |= SetItemSelected(hwnd, s, i, false);
	}
	return changed;
}

int CompareText(const ListBoxState &s, const std::wstring &a, const std::wstring &b)
{
	(void)s;
	const QString qa = QString::fromStdWString(a), qb = QString::fromStdWString(b);
	return QString::localeAwareCompare(qa.toCaseFolded(), qb.toCaseFolded());
}

int InsertPosition(HWND hwnd, ListBoxState &s, const ListItem &item)
{
	const int count = static_cast<int>(s.items.size());
	if(!(Style(hwnd) & LBS_SORT))
		return count;
	const bool strings = HasStrings(hwnd);
	int lo = 0, hi = count;
	while(lo < hi)
	{
		const int mid = (lo + hi) / 2;
		int cmp;
		if(strings)
		{
			cmp = CompareText(s, item.text, s.items[static_cast<std::size_t>(mid)].text);
		} else
		{
			COMPAREITEMSTRUCT cis{};
			cis.CtlType = CtlType(s);
			cis.CtlID = CtlId(hwnd, s);
			cis.hwndItem = hwnd;
			cis.itemID1 = static_cast<UINT>(-1);
			cis.itemData1 = item.data;
			cis.itemID2 = static_cast<UINT>(mid);
			cis.itemData2 = s.items[static_cast<std::size_t>(mid)].data;
			cis.dwLocaleId = s.locale;
			cmp = static_cast<int>(::SendMessage(NotifyTarget(hwnd, s), WM_COMPAREITEM, cis.CtlID, reinterpret_cast<LPARAM>(&cis)));
		}
		if(cmp < 0)
			hi = mid;
		else
			lo = mid + 1;
	}
	return lo;
}

void MeasureItem(HWND hwnd, ListBoxState &s, int index)
{
	MEASUREITEMSTRUCT mis{};
	mis.CtlType = CtlType(s);
	mis.CtlID = CtlId(hwnd, s);
	mis.itemID = static_cast<UINT>(std::max(index, 0));
	mis.itemHeight = static_cast<UINT>(FontHeight(hwnd));
	if(index >= 0 && index < static_cast<int>(s.items.size()))
		mis.itemData = s.items[static_cast<std::size_t>(index)].data;
	RECT rc{};
	::GetClientRect(hwnd, &rc);
	mis.itemWidth = static_cast<UINT>(rc.right);
	::SendMessage(NotifyTarget(hwnd, s), WM_MEASUREITEM, mis.CtlID, reinterpret_cast<LPARAM>(&mis));
	if(index >= 0 && index < static_cast<int>(s.items.size()))
		s.items[static_cast<std::size_t>(index)].height = static_cast<int>(mis.itemHeight);
	else
		s.itemHeight = std::max(1, static_cast<int>(mis.itemHeight));
}

int Insert(HWND hwnd, ListBoxState &s, int index, LPARAM lp)
{
	ListItem item;
	if(HasStrings(hwnd))
		item.text = lp ? reinterpret_cast<LPCWSTR>(lp) : L"";
	else
		item.data = static_cast<ULONG_PTR>(lp);
	const int count = static_cast<int>(s.items.size());
	if(index == -2)
		index = InsertPosition(hwnd, s, item); // LBS_SORT
	else if(index < 0 || index > count)
		index = count;
	s.items.insert(s.items.begin() + index, item);
	if(s.selected >= index)
		s.selected++;
	if(s.caret >= index)
		s.caret++;
	if(s.anchor >= index)
		s.anchor++;
	if(Style(hwnd) & LBS_OWNERDRAWVARIABLE)
		MeasureItem(hwnd, s, index);
	UpdateScroll(hwnd, s);
	Redraw(hwnd);
	return index;
}

void SendDeleteItem(HWND hwnd, ListBoxState &s, int index)
{
	const ListItem &item = s.items[static_cast<std::size_t>(index)];
	if(!OwnerDraw(hwnd) && item.data == 0)
		return;
	DELETEITEMSTRUCT dis{};
	dis.CtlType = CtlType(s);
	dis.CtlID = CtlId(hwnd, s);
	dis.itemID = static_cast<UINT>(index);
	dis.hwndItem = hwnd;
	dis.itemData = item.data;
	::SendMessage(NotifyTarget(hwnd, s), WM_DELETEITEM, dis.CtlID, reinterpret_cast<LPARAM>(&dis));
}

int Delete(HWND hwnd, ListBoxState &s, int index)
{
	if(index < 0 || index >= static_cast<int>(s.items.size()))
		return LB_ERR;
	SendDeleteItem(hwnd, s, index);
	if(ListState(hwnd) == nullptr)
		return LB_ERR;
	s.items.erase(s.items.begin() + index);
	if(s.selected == index)
		s.selected = -1;
	else if(s.selected > index)
		s.selected--;
	if(s.caret >= static_cast<int>(s.items.size()))
		s.caret = static_cast<int>(s.items.size()) - 1;
	else if(s.caret > index)
		s.caret--;
	if(s.anchor > index)
		s.anchor--;
	s.top = std::clamp(s.top, 0, MaxTop(hwnd, s));
	UpdateScroll(hwnd, s);
	Redraw(hwnd);
	return static_cast<int>(s.items.size());
}

void Reset(HWND hwnd, ListBoxState &s)
{
	for(int i = 0; i < static_cast<int>(s.items.size()); i++)
		SendDeleteItem(hwnd, s, i);
	s.items.clear();
	s.top = 0;
	s.caret = -1;
	s.anchor = -1;
	s.selected = -1;
	s.scrollX = 0;
	UpdateScroll(hwnd, s);
	Redraw(hwnd);
}

int Find(HWND hwnd, const ListBoxState &s, int start, LPARAM lp, bool exact)
{
	const int count = static_cast<int>(s.items.size());
	if(count == 0)
		return LB_ERR;
	const bool strings = HasStrings(hwnd);
	const std::wstring wanted = strings && lp ? reinterpret_cast<LPCWSTR>(lp) : L"";
	const QString w = QString::fromStdWString(wanted).toCaseFolded();
	if(start < -1 || start >= count)
		start = -1;
	for(int n = 1; n <= count; n++)
	{
		const int i = (start + n) % count;
		const ListItem &item = s.items[static_cast<std::size_t>(i)];
		if(!strings)
		{
			if(item.data == static_cast<ULONG_PTR>(lp))
				return i;
			continue;
		}
		const QString t = QString::fromStdWString(item.text).toCaseFolded();
		if(exact ? t == w : t.startsWith(w))
			return i;
	}
	return LB_ERR;
}

// ---- painting ---------------------------------------------------------------------------

void DrawItem(HWND hwnd, ListBoxState &s, HDC dc, int index, const RECT &rc, UINT action)
{
	const ListItem &item = s.items[static_cast<std::size_t>(index)];
	const bool selected = item.selected;
	const bool focus = s.focused && index == s.caret;
	if(OwnerDraw(hwnd))
	{
		DRAWITEMSTRUCT dis{};
		dis.CtlType = CtlType(s);
		dis.CtlID = CtlId(hwnd, s);
		dis.itemID = static_cast<UINT>(index);
		dis.itemAction = action;
		dis.itemState = (selected ? ODS_SELECTED : 0) | (focus ? ODS_FOCUS : 0) | (::IsWindowEnabled(hwnd) ? 0 : ODS_DISABLED);
		dis.hwndItem = hwnd;
		dis.hDC = dc;
		dis.rcItem = rc;
		dis.itemData = item.data;
		::SendMessage(NotifyTarget(hwnd, s), WM_DRAWITEM, dis.CtlID, reinterpret_cast<LPARAM>(&dis));
		return;
	}
	const COLORREF oldText = ::GetTextColor(dc), oldBk = ::GetBkColor(dc);
	if(selected)
	{
		::SetBkColor(dc, ::GetSysColor(COLOR_HIGHLIGHT));
		::SetTextColor(dc, ::GetSysColor(COLOR_HIGHLIGHTTEXT));
	} else if(!::IsWindowEnabled(hwnd))
	{
		::SetTextColor(dc, ::GetSysColor(COLOR_GRAYTEXT));
	}
	RECT text = rc;
	::ExtTextOut(dc, 0, 0, ETO_OPAQUE, &rc, nullptr, 0, nullptr);
	text.left += 2;
	UINT flags = DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX;
	if(Style(hwnd) & LBS_USETABSTOPS)
		flags |= DT_EXPANDTABS;
	::DrawText(dc, item.text.c_str(), static_cast<int>(item.text.size()), &text, flags);
	::SetTextColor(dc, oldText);
	::SetBkColor(dc, oldBk);
	if(focus)
		::DrawFocusRect(dc, &rc);
}

void Paint(HWND hwnd, HDC dc)
{
	ListBoxState *s = ListState(hwnd);
	if(s == nullptr)
		return;
	RECT client{};
	::GetClientRect(hwnd, &client);
	HBRUSH brush = s->comboList ? ControlBrush(s->combo, dc, WM_CTLCOLORLISTBOX) : ControlBrush(hwnd, dc, WM_CTLCOLORLISTBOX);
	HGDIOBJ oldFont = ::SelectObject(dc, ListFont(hwnd));
	const int count = static_cast<int>(s->items.size());
	int y = 0;
	for(int i = s->top; i < count && y < client.bottom; i++)
	{
		const RECT rc = ItemRect(hwnd, *s, i);
		if(OwnerDraw(hwnd))
			::FillRect(dc, &rc, brush);
		DrawItem(hwnd, *s, dc, i, rc, ODA_DRAWENTIRE);
		if((s = ListState(hwnd)) == nullptr)
			break;
		y = rc.bottom;
	}
	if(s != nullptr && y < client.bottom)
	{
		RECT rest = client;
		rest.top = y;
		::FillRect(dc, &rest, brush);
		// An empty list box with the focus shows it on its first line.
		if(count == 0 && s->focused)
		{
			RECT first = client;
			first.bottom = s->itemHeight;
			::DrawFocusRect(dc, &first);
		}
	}
	::SelectObject(dc, oldFont);
}

// ---- input --------------------------------------------------------------------------------

void ExtendedClick(HWND hwnd, ListBoxState &s, int index, bool shift, bool ctrl)
{
	bool changed = false;
	if(shift)
	{
		if(s.anchor < 0)
			s.anchor = index;
		changed = SelectRange(hwnd, s, s.anchor, index, true, !ctrl);
	} else if(ctrl)
	{
		changed = SetItemSelected(hwnd, s, index, !s.items[static_cast<std::size_t>(index)].selected);
		s.anchor = index;
	} else
	{
		changed = SelectRange(hwnd, s, index, index, true, true);
		s.anchor = index;
	}
	SetCaret(hwnd, s, index);
	EnsureVisible(hwnd, s, index);
	if(changed)
		Notify(hwnd, s, LBN_SELCHANGE);
}

void ClickItem(HWND hwnd, ListBoxState &s, int index, bool shift, bool ctrl, bool down)
{
	const LONG_PTR style = Style(hwnd);
	if(index < 0 || index >= static_cast<int>(s.items.size()))
		return;
	if(style & LBS_EXTENDEDSEL)
	{
		ExtendedClick(hwnd, s, index, shift, ctrl);
	} else if(style & LBS_MULTIPLESEL)
	{
		if(down)
		{
			const bool changed = SetItemSelected(hwnd, s, index, !s.items[static_cast<std::size_t>(index)].selected);
			SetCaret(hwnd, s, index);
			if(changed)
				Notify(hwnd, s, LBN_SELCHANGE);
		} else
		{
			SetCaret(hwnd, s, index);
		}
	} else
	{
		const bool changed = SelectSingle(hwnd, s, index);
		SetCaret(hwnd, s, index);
		EnsureVisible(hwnd, s, index);
		if(changed)
			Notify(hwnd, s, LBN_SELCHANGE);
	}
}

void MoveCaret(HWND hwnd, ListBoxState &s, int index)
{
	const int count = static_cast<int>(s.items.size());
	if(count == 0)
		return;
	index = std::clamp(index, 0, count - 1);
	const LONG_PTR style = Style(hwnd);
	const bool shift = (::GetKeyState(VK_SHIFT) & 0x8000) != 0;
	const bool ctrl = (::GetKeyState(VK_CONTROL) & 0x8000) != 0;
	if(style & LBS_EXTENDEDSEL)
	{
		if(ctrl && !shift)
		{
			SetCaret(hwnd, s, index);
			EnsureVisible(hwnd, s, index);
		} else
		{
			ExtendedClick(hwnd, s, index, shift, false);
		}
	} else if(style & LBS_MULTIPLESEL)
	{
		SetCaret(hwnd, s, index);
		EnsureVisible(hwnd, s, index);
	} else
	{
		ClickItem(hwnd, s, index, false, false, true);
	}
}

void KeyDown(HWND hwnd, ListBoxState &s, WPARAM vk)
{
	if((Style(hwnd) & LBS_WANTKEYBOARDINPUT) && !s.comboList)
	{
		const LRESULT r = ::SendMessage(::GetParent(hwnd), WM_VKEYTOITEM, MAKEWPARAM(vk, std::max(s.caret, 0)), reinterpret_cast<LPARAM>(hwnd));
		if(r == -2 || ListState(hwnd) == nullptr)
			return;
		if(r >= 0)
		{
			MoveCaret(hwnd, s, static_cast<int>(r));
			return;
		}
	}
	const int caret = std::max(s.caret, 0);
	const int page = std::max(1, VisibleCount(hwnd, s, s.top) - 1);
	switch(vk)
	{
	case VK_UP: MoveCaret(hwnd, s, s.caret < 0 ? 0 : caret - 1); break;
	case VK_DOWN: MoveCaret(hwnd, s, s.caret < 0 ? 0 : caret + 1); break;
	case VK_PRIOR: MoveCaret(hwnd, s, caret - page); break;
	case VK_NEXT: MoveCaret(hwnd, s, caret + page); break;
	case VK_HOME: MoveCaret(hwnd, s, 0); break;
	case VK_END: MoveCaret(hwnd, s, static_cast<int>(s.items.size()) - 1); break;
	case VK_LEFT:
		if(s.horizontalExtent > 0)
			::SendMessage(hwnd, WM_HSCROLL, SB_LINEUP, 0);
		else
			MoveCaret(hwnd, s, caret - 1);
		break;
	case VK_RIGHT:
		if(s.horizontalExtent > 0)
			::SendMessage(hwnd, WM_HSCROLL, SB_LINEDOWN, 0);
		else
			MoveCaret(hwnd, s, s.caret < 0 ? 0 : caret + 1);
		break;
	case VK_SPACE:
		if(s.caret >= 0 && (Style(hwnd) & (LBS_MULTIPLESEL | LBS_EXTENDEDSEL)))
		{
			const bool extended = (Style(hwnd) & LBS_EXTENDEDSEL) != 0;
			if(!extended || (::GetKeyState(VK_CONTROL) & 0x8000))
			{
				if(SetItemSelected(hwnd, s, s.caret, !s.items[static_cast<std::size_t>(s.caret)].selected))
					Notify(hwnd, s, LBN_SELCHANGE);
				s.anchor = s.caret;
			}
		}
		break;
	default:
		break;
	}
}

void CharKey(HWND hwnd, ListBoxState &s, WCHAR ch)
{
	if((Style(hwnd) & LBS_WANTKEYBOARDINPUT) && !s.comboList)
	{
		const LRESULT r = ::SendMessage(::GetParent(hwnd), WM_CHARTOITEM, MAKEWPARAM(ch, std::max(s.caret, 0)), reinterpret_cast<LPARAM>(hwnd));
		if(r == -2 || ListState(hwnd) == nullptr)
			return;
		if(r >= 0)
		{
			MoveCaret(hwnd, s, static_cast<int>(r));
			return;
		}
	}
	if(!HasStrings(hwnd) || ch < 0x20)
		return;
	const wchar_t prefix[2] = {ch, 0};
	const int found = Find(hwnd, s, s.caret, reinterpret_cast<LPARAM>(prefix), false);
	if(found >= 0)
		MoveCaret(hwnd, s, found);
}

void Scroll(HWND hwnd, ListBoxState &s, UINT code, int thumb)
{
	const int page = std::max(1, VisibleCount(hwnd, s, s.top) - 1);
	switch(code)
	{
	case SB_LINEUP: SetTop(hwnd, s, s.top - 1); break;
	case SB_LINEDOWN: SetTop(hwnd, s, s.top + 1); break;
	case SB_PAGEUP: SetTop(hwnd, s, s.top - page); break;
	case SB_PAGEDOWN: SetTop(hwnd, s, s.top + page); break;
	case SB_TOP: SetTop(hwnd, s, 0); break;
	case SB_BOTTOM: SetTop(hwnd, s, MaxTop(hwnd, s)); break;
	case SB_THUMBTRACK:
	case SB_THUMBPOSITION:
		SetTop(hwnd, s, thumb);
		break;
	default: break;
	}
}

void HScroll(HWND hwnd, ListBoxState &s, UINT code, int thumb)
{
	RECT rc{};
	::GetClientRect(hwnd, &rc);
	const int maxX = std::max(0, s.horizontalExtent - rc.right);
	int x = s.scrollX;
	switch(code)
	{
	case SB_LINEUP: x -= 8; break;
	case SB_LINEDOWN: x += 8; break;
	case SB_PAGEUP: x -= rc.right; break;
	case SB_PAGEDOWN: x += rc.right; break;
	case SB_TOP: x = 0; break;
	case SB_BOTTOM: x = maxX; break;
	case SB_THUMBTRACK:
	case SB_THUMBPOSITION: x = thumb; break;
	default: break;
	}
	x = std::clamp(x, 0, maxX);
	if(x != s.scrollX)
	{
		s.scrollX = x;
		UpdateScroll(hwnd, s);
		Redraw(hwnd);
	}
}

void FitIntegralHeight(HWND hwnd, ListBoxState &s)
{
	const LONG_PTR style = Style(hwnd);
	if((style & (LBS_NOINTEGRALHEIGHT | LBS_OWNERDRAWVARIABLE)) || s.comboList)
		return;
	const int height = ClientHeight(hwnd);
	const int remainder = height % std::max(1, s.itemHeight);
	if(remainder == 0 || height <= s.itemHeight)
		return;
	RECT wr{};
	::GetWindowRect(hwnd, &wr);
	::SetWindowPos(hwnd, nullptr, 0, 0, wr.right - wr.left, wr.bottom - wr.top - remainder, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

LRESULT ItemText(const ListBoxState &s, HWND hwnd, int index, LPWSTR buf)
{
	if(index < 0 || index >= static_cast<int>(s.items.size()))
		return LB_ERR;
	const ListItem &item = s.items[static_cast<std::size_t>(index)];
	if(!HasStrings(hwnd))
	{
		if(buf)
			*reinterpret_cast<ULONG_PTR *>(buf) = item.data;
		return sizeof(ULONG_PTR);
	}
	if(buf)
	{
		std::copy(item.text.begin(), item.text.end(), buf);
		buf[item.text.size()] = 0;
	}
	return static_cast<LRESULT>(item.text.size());
}

LRESULT CALLBACK ListBoxProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	ListBoxState *s = ListState(hwnd);
	if(s == nullptr && msg != WM_NCCREATE)
		return ::DefWindowProc(hwnd, msg, wp, lp);
	switch(msg)
	{
	case WM_NCCREATE:
		if(WindowData *d = Data(hwnd))
		{
			auto state = std::make_unique<ListBoxState>();
			const auto *cs = reinterpret_cast<const CREATESTRUCT *>(lp);
			if(d->windowClass != nullptr && d->windowClass->name == L"ComboLBox")
			{
				state->comboList = true;
				state->combo = cs != nullptr ? static_cast<HWND>(cs->lpCreateParams) : nullptr;
			}
			d->control = std::move(state);
		}
		return ::DefWindowProc(hwnd, msg, wp, lp);
	case WM_CREATE:
		s->itemHeight = FontHeight(hwnd);
		if(Style(hwnd) & LBS_OWNERDRAWFIXED)
			MeasureItem(hwnd, *s, -1);
		if((s = ListState(hwnd)) != nullptr)
		{
			FitIntegralHeight(hwnd, *s);
			UpdateScroll(hwnd, *s);
		}
		return 0;
	case WM_SIZE:
		FitIntegralHeight(hwnd, *s);
		if((s = ListState(hwnd)) != nullptr)
		{
			s->top = std::clamp(s->top, 0, MaxTop(hwnd, *s));
			UpdateScroll(hwnd, *s);
		}
		return 0;
	case WM_SETFONT:
		::DefWindowProc(hwnd, msg, wp, 0);
		if(!OwnerDraw(hwnd))
			s->itemHeight = FontHeight(hwnd);
		UpdateScroll(hwnd, *s);
		if(lp)
			Redraw(hwnd);
		return 0;
	case WM_GETDLGCODE:
		return DLGC_WANTARROWS | DLGC_WANTCHARS;
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
	case WM_ENABLE:
		Redraw(hwnd);
		return 0;
	case WM_SETFOCUS:
		s->focused = true;
		if(s->caret < 0 && !s->items.empty() && !MultiSel(hwnd))
			s->caret = std::max(s->selected, 0);
		RedrawItem(hwnd, *s, s->caret);
		if(s->items.empty())
			Redraw(hwnd);
		Notify(hwnd, *s, LBN_SETFOCUS);
		return 0;
	case WM_KILLFOCUS:
		s->focused = false;
		RedrawItem(hwnd, *s, s->caret);
		if(s->items.empty())
			Redraw(hwnd);
		Notify(hwnd, *s, LBN_KILLFOCUS);
		return 0;
	case WM_LBUTTONDOWN:
	case WM_LBUTTONDBLCLK:
		{
			if(!s->comboList && ::GetFocus() != hwnd)
				::SetFocus(hwnd);
			if((s = ListState(hwnd)) == nullptr)
				return 0;
			bool outside = false;
			const int index = ItemFromPoint(hwnd, *s, GET_Y_LPARAM(lp), outside);
			if(msg == WM_LBUTTONDBLCLK && !outside)
			{
				Notify(hwnd, *s, LBN_DBLCLK);
				return 0;
			}
			if(outside && s->items.empty())
				return 0;
			s->capturing = true;
			::SetCapture(hwnd);
			if((s = ListState(hwnd)) != nullptr && !outside)
			{
				s->quiet = PopupList(hwnd, *s);
				ClickItem(hwnd, *s, index, (wp & MK_SHIFT) != 0, (wp & MK_CONTROL) != 0, true);
				if((s = ListState(hwnd)) != nullptr)
					s->quiet = false;
			}
		}
		return 0;
	case WM_MOUSEMOVE:
		if(s->capturing || PopupList(hwnd, *s))
		{
			bool outside = false;
			const int y = GET_Y_LPARAM(lp);
			int index = ItemFromPoint(hwnd, *s, y, outside);
			if(s->capturing && (y < 0 || y >= ClientHeight(hwnd)))
			{
				// Dragging past the edge scrolls.
				index = y < 0 ? s->top - 1 : s->top + VisibleCount(hwnd, *s, s->top);
				index = std::clamp(index, 0, static_cast<int>(s->items.size()) - 1);
				outside = false;
			}
			if(!outside && index != s->caret && !s->items.empty())
			{
				s->quiet = PopupList(hwnd, *s);
				if(Style(hwnd) & LBS_EXTENDEDSEL)
					ExtendedClick(hwnd, *s, index, true, (wp & MK_CONTROL) != 0);
				else if(!(Style(hwnd) & LBS_MULTIPLESEL))
					ClickItem(hwnd, *s, index, false, false, true);
				if((s = ListState(hwnd)) != nullptr)
					s->quiet = false;
			}
		}
		return 0;
	case WM_LBUTTONUP:
		if(s->capturing)
		{
			s->capturing = false;
			::ReleaseCapture();
			if(s->comboList)
				ComboListNotify(s->combo, hwnd, ComboChosen);
		} else if(PopupList(hwnd, *s))
		{
			// Pressed on the combo box, released on an item of its list.
			bool outside = false;
			ItemFromPoint(hwnd, *s, GET_Y_LPARAM(lp), outside);
			RECT rc{};
			::GetClientRect(hwnd, &rc);
			const POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
			if(!outside && ::PtInRect(&rc, pt))
				ComboListNotify(s->combo, hwnd, ComboChosen);
		}
		return 0;
	case WM_CLOSE:
		if(s->comboList)
		{
			// Qt closes a popup that is clicked outside of.
			ComboListNotify(s->combo, hwnd, ComboCancel);
			return 0;
		}
		return ::DefWindowProc(hwnd, msg, wp, lp);
	case WM_SYSCOMMAND:
		if(s->comboList && (wp & 0xFFF0) == SC_CLOSE)
		{
			ComboListNotify(s->combo, hwnd, ComboCancel);
			return 0;
		}
		return ::DefWindowProc(hwnd, msg, wp, lp);
	case WM_CAPTURECHANGED:
		s->capturing = false;
		return 0;
	case WM_MOUSEWHEEL:
		{
			const int lines = -static_cast<short>(HIWORD(wp)) * 3 / WHEEL_DELTA;
			SetTop(hwnd, *s, s->top + (lines == 0 ? (static_cast<short>(HIWORD(wp)) > 0 ? -1 : 1) : lines));
		}
		return 0;
	case WM_VSCROLL:
		{
			SCROLLINFO si{sizeof(si), SIF_TRACKPOS, 0, 0, 0, 0, 0};
			::GetScrollInfo(hwnd, SB_VERT, &si);
			Scroll(hwnd, *s, LOWORD(wp), si.nTrackPos);
		}
		return 0;
	case WM_HSCROLL:
		{
			SCROLLINFO si{sizeof(si), SIF_TRACKPOS, 0, 0, 0, 0, 0};
			::GetScrollInfo(hwnd, SB_HORZ, &si);
			HScroll(hwnd, *s, LOWORD(wp), si.nTrackPos);
		}
		return 0;
	case WM_KEYDOWN:
		if(s->comboList && ComboListKey(s->combo, msg, wp, lp))
			return 0;
		if((s = ListState(hwnd)) != nullptr)
			KeyDown(hwnd, *s, wp);
		return 0;
	case WM_CHAR:
		if(s->comboList && ComboListKey(s->combo, msg, wp, lp))
			return 0;
		if((s = ListState(hwnd)) != nullptr)
			CharKey(hwnd, *s, static_cast<WCHAR>(wp));
		return 0;
	case WM_SYSKEYDOWN:
		if(s->comboList && ComboListKey(s->combo, msg, wp, lp))
			return 0;
		return ::DefWindowProc(hwnd, msg, wp, lp);

	case LB_ADDSTRING:
		return Insert(hwnd, *s, (Style(hwnd) & LBS_SORT) ? -2 : static_cast<int>(s->items.size()), lp);
	case LB_INSERTSTRING:
		return Insert(hwnd, *s, static_cast<int>(wp) < 0 ? static_cast<int>(s->items.size()) : static_cast<int>(wp), lp);
	case LB_DELETESTRING:
		return Delete(hwnd, *s, static_cast<int>(wp));
	case LB_RESETCONTENT:
		Reset(hwnd, *s);
		return 0;
	case LB_GETCOUNT:
		return static_cast<LRESULT>(s->items.size());
	case LB_GETTEXT:
		return ItemText(*s, hwnd, static_cast<int>(wp), reinterpret_cast<LPWSTR>(lp));
	case LB_GETTEXTLEN:
		return ItemText(*s, hwnd, static_cast<int>(wp), nullptr);
	case LB_GETITEMDATA:
		if(static_cast<int>(wp) < 0 || wp >= s->items.size())
			return LB_ERR;
		return static_cast<LRESULT>(s->items[wp].data);
	case LB_SETITEMDATA:
		if(static_cast<int>(wp) < 0 || wp >= s->items.size())
			return LB_ERR;
		s->items[wp].data = static_cast<ULONG_PTR>(lp);
		return TRUE;
	case LB_GETCURSEL:
		if(MultiSel(hwnd))
			return s->caret;
		return s->selected;
	case LB_SETCURSEL:
		if(MultiSel(hwnd))
			return LB_ERR;
		{
			const int index = static_cast<int>(wp);
			if(index >= static_cast<int>(s->items.size()))
				return LB_ERR;
			SelectSingle(hwnd, *s, index < 0 ? -1 : index);
			SetCaret(hwnd, *s, index < 0 ? -1 : index);
			if(index >= 0)
			{
				EnsureVisible(hwnd, *s, index);
				return index;
			}
		}
		return LB_ERR;
	case LB_GETSEL:
		if(static_cast<int>(wp) < 0 || wp >= s->items.size())
			return LB_ERR;
		return s->items[wp].selected ? 1 : 0;
	case LB_SETSEL:
		if(!MultiSel(hwnd))
			return LB_ERR;
		if(static_cast<int>(lp) == -1)
			SelectRange(hwnd, *s, 0, static_cast<int>(s->items.size()) - 1, wp != 0, false);
		else if(static_cast<int>(lp) < 0 || static_cast<std::size_t>(lp) >= s->items.size())
			return LB_ERR;
		else
		{
			SetItemSelected(hwnd, *s, static_cast<int>(lp), wp != 0);
			if(wp)
			{
				SetCaret(hwnd, *s, static_cast<int>(lp));
				s->anchor = static_cast<int>(lp);
			}
		}
		return 0;
	case LB_SELITEMRANGE:
		if(!MultiSel(hwnd))
			return LB_ERR;
		SelectRange(hwnd, *s, LOWORD(lp), HIWORD(lp), wp != 0, false);
		return 0;
	case LB_SELITEMRANGEEX:
		if(!MultiSel(hwnd))
			return LB_ERR;
		if(static_cast<int>(lp) >= static_cast<int>(wp))
			SelectRange(hwnd, *s, static_cast<int>(wp), static_cast<int>(lp), true, false);
		else
			SelectRange(hwnd, *s, static_cast<int>(lp), static_cast<int>(wp), false, false);
		return 0;
	case LB_GETSELCOUNT:
		if(!MultiSel(hwnd))
			return LB_ERR;
		return std::count_if(s->items.begin(), s->items.end(), [](const ListItem &i) { return i.selected; });
	case LB_GETSELITEMS:
		if(!MultiSel(hwnd))
			return LB_ERR;
		{
			int n = 0;
			auto *out = reinterpret_cast<int *>(lp);
			for(int i = 0; i < static_cast<int>(s->items.size()) && n < static_cast<int>(wp); i++)
			{
				if(s->items[static_cast<std::size_t>(i)].selected)
					out[n++] = i;
			}
			return n;
		}
	case LB_GETTOPINDEX:
		return s->top;
	case LB_SETTOPINDEX:
		if(static_cast<int>(wp) < 0 || (wp >= s->items.size() && !s->items.empty()))
			return LB_ERR;
		SetTop(hwnd, *s, static_cast<int>(wp));
		return 0;
	case LB_FINDSTRING:
		return Find(hwnd, *s, static_cast<int>(wp), lp, false);
	case LB_FINDSTRINGEXACT:
		return Find(hwnd, *s, static_cast<int>(wp), lp, true);
	case LB_SELECTSTRING:
		if(MultiSel(hwnd))
			return LB_ERR;
		{
			const int found = Find(hwnd, *s, static_cast<int>(wp), lp, false);
			if(found < 0)
				return LB_ERR;
			SelectSingle(hwnd, *s, found);
			SetCaret(hwnd, *s, found);
			EnsureVisible(hwnd, *s, found);
			return found;
		}
	case LB_GETITEMRECT:
		if(static_cast<int>(wp) < 0 || (wp >= s->items.size() && !(s->items.empty() && wp == 0)))
			return LB_ERR;
		*reinterpret_cast<RECT *>(lp) = ItemRect(hwnd, *s, static_cast<int>(wp));
		return 1;
	case LB_GETITEMHEIGHT:
		return ItemHeight(hwnd, *s, static_cast<int>(wp));
	case LB_SETITEMHEIGHT:
		if(LOWORD(lp) == 0 || LOWORD(lp) > 255)
			return LB_ERR;
		if(Style(hwnd) & LBS_OWNERDRAWVARIABLE)
		{
			if(wp >= s->items.size())
				return LB_ERR;
			s->items[wp].height = LOWORD(lp);
		} else
		{
			s->itemHeight = LOWORD(lp);
		}
		s->top = std::clamp(s->top, 0, MaxTop(hwnd, *s));
		UpdateScroll(hwnd, *s);
		Redraw(hwnd);
		return 0;
	case LB_ITEMFROMPOINT:
		{
			bool outside = false;
			const int index = ItemFromPoint(hwnd, *s, GET_Y_LPARAM(lp), outside);
			RECT rc{};
			::GetClientRect(hwnd, &rc);
			if(GET_X_LPARAM(lp) < 0 || GET_X_LPARAM(lp) >= rc.right)
				outside = true;
			return MAKELONG(index, outside ? 1 : 0);
		}
	case LB_SETCARETINDEX:
		if(static_cast<int>(wp) < 0 || wp >= s->items.size())
			return LB_ERR;
		SetCaret(hwnd, *s, static_cast<int>(wp));
		if(!lp)
			EnsureVisible(hwnd, *s, static_cast<int>(wp));
		return 0;
	case LB_GETCARETINDEX:
		return s->caret;
	case LB_SETANCHORINDEX:
		if(static_cast<int>(wp) < -1 || static_cast<int>(wp) >= static_cast<int>(s->items.size()))
			return LB_ERR;
		s->anchor = static_cast<int>(wp);
		return 0;
	case LB_GETANCHORINDEX:
		return s->anchor;
	case LB_SETHORIZONTALEXTENT:
		s->horizontalExtent = static_cast<int>(wp);
		if(Style(hwnd) & WS_HSCROLL)
		{
			RECT rc{};
			::GetClientRect(hwnd, &rc);
			::ShowScrollBar(hwnd, SB_HORZ, s->horizontalExtent > rc.right || (Style(hwnd) & LBS_DISABLENOSCROLL));
		}
		if((s = ListState(hwnd)) != nullptr)
			UpdateScroll(hwnd, *s);
		return 0;
	case LB_GETHORIZONTALEXTENT:
		return s->horizontalExtent;
	case LB_SETCOLUMNWIDTH:
		s->columnWidth = static_cast<int>(wp);
		return 0;
	case LB_SETTABSTOPS:
		s->tabStops.assign(reinterpret_cast<const int *>(lp), reinterpret_cast<const int *>(lp) + wp);
		return TRUE;
	case LB_INITSTORAGE:
		s->items.reserve(s->items.size() + wp);
		return static_cast<LRESULT>(s->items.capacity());
	case LB_SETCOUNT:
		if(!(Style(hwnd) & LBS_NODATA))
			return LB_ERR;
		s->items.resize(wp);
		UpdateScroll(hwnd, *s);
		Redraw(hwnd);
		return 0;
	case LB_SETLOCALE:
		{
			const LCID old = s->locale;
			s->locale = static_cast<LCID>(wp);
			return static_cast<LRESULT>(old);
		}
	case LB_GETLOCALE:
		return static_cast<LRESULT>(s->locale);
	case LB_DIR:
	case LB_ADDFILE:
		return LB_ERR;
	case WM_DESTROY:
		Reset(hwnd, *s);
		return 0;
	case WM_NCDESTROY:
		if(WindowData *d = Data(hwnd))
			d->control.reset();
		return ::DefWindowProc(hwnd, msg, wp, lp);
	default:
		return ::DefWindowProc(hwnd, msg, wp, lp);
	}
}

} // namespace

LRESULT ListBoxDefault(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	return ListBoxProc(hwnd, msg, wp, lp);
}

void RegisterListBoxClasses()
{
	WNDCLASS wc{};
	wc.style = CS_DBLCLKS | CS_PARENTDC;
	wc.lpfnWndProc = ListBoxProc;
	wc.hCursor = ::LoadCursor(nullptr, IDC_ARROW);
	wc.lpszClassName = L"ListBox";
	::RegisterClass(&wc);
	wc.style = CS_DBLCLKS | CS_SAVEBITS;
	wc.lpszClassName = L"ComboLBox";
	::RegisterClass(&wc);
}

} // namespace linopenmpt::qtwin
