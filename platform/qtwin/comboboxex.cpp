// The ComboBoxEx of comctl32 (ComboBoxEx32): a combo box whose items carry
// an image, a selected image, an overlay and an indent.
//
// Built as comctl32 builds it: the control hosts an owner-drawn "ComboBox"
// child (CBEM_GETCOMBOCONTROL) that fills it, draws the child's items itself
// from its own item list, and passes the child's CBN_* notifications on to
// its parent as its own. CB_* messages go to the child, except those about
// item data, which is the items' lParam here. Text and images may come from
// the parent through CBEN_GETDISPINFO.
#include "QtWin.h"

#include <algorithm>
#include <string>
#include <vector>

namespace linopenmpt::qtwin
{

namespace
{

constexpr int ComboId = 1001; // comctl32 does not document it; any id works
constexpr int IndentWidth = 10; // pixels per iIndent, as in comctl32
constexpr int ImageGap = 4;   // between image and text

struct Item
{
	bool callbackText = false;
	std::wstring text;
	int image = I_IMAGECALLBACK - 1; // none
	int selectedImage = I_IMAGECALLBACK - 1;
	int overlay = 0;
	int indent = 0;
	LPARAM lParam = 0;
	UINT mask = 0;
};

struct ComboExState : ControlState
{
	HWND combo = nullptr;
	HIMAGELIST images = nullptr;
	DWORD exStyle = 0;
	int dropTotal = 0; // the combo box's height including its dropped-down list
	std::vector<Item> items;
};

ComboExState *State(HWND hwnd)
{
	return Control<ComboExState>(hwnd);
}

LRESULT Notify(HWND hwnd, NMHDR &hdr, UINT code)
{
	hdr.hwndFrom = hwnd;
	hdr.idFrom = static_cast<UINT_PTR>(::GetDlgCtrlID(hwnd));
	hdr.code = code;
	return ::SendMessage(::GetParent(hwnd), WM_NOTIFY, hdr.idFrom, reinterpret_cast<LPARAM>(&hdr));
}

void ToItem(Item &item, const COMBOBOXEXITEM &from)
{
	if(from.mask & CBEIF_TEXT)
	{
		item.callbackText = from.pszText == LPSTR_TEXTCALLBACK;
		item.text = (from.pszText != nullptr && !item.callbackText) ? std::wstring(from.pszText) : std::wstring();
	}
	if(from.mask & CBEIF_IMAGE)
		item.image = from.iImage;
	if(from.mask & CBEIF_SELECTEDIMAGE)
		item.selectedImage = from.iSelectedImage;
	if(from.mask & CBEIF_OVERLAY)
		item.overlay = from.iOverlay;
	if(from.mask & CBEIF_INDENT)
		item.indent = from.iIndent;
	if(from.mask & CBEIF_LPARAM)
		item.lParam = from.lParam;
	item.mask |= from.mask & ~CBEIF_DI_SETITEM;
}

// What the item shows, asking the parent for the callback parts.
Item Resolve(HWND hwnd, int index)
{
	ComboExState *s = State(hwnd);
	if(s == nullptr || index < 0 || static_cast<std::size_t>(index) >= s->items.size())
		return {};
	Item item = s->items[index];
	UINT ask = 0;
	if(item.callbackText)
		ask |= CBEIF_TEXT;
	if((item.mask & CBEIF_IMAGE) && item.image == I_IMAGECALLBACK)
		ask |= CBEIF_IMAGE;
	if((item.mask & CBEIF_SELECTEDIMAGE) && item.selectedImage == I_IMAGECALLBACK)
		ask |= CBEIF_SELECTEDIMAGE;
	if((item.mask & CBEIF_OVERLAY) && item.overlay == I_IMAGECALLBACK)
		ask |= CBEIF_OVERLAY;
	if((item.mask & CBEIF_INDENT) && item.indent == I_INDENTCALLBACK)
		ask |= CBEIF_INDENT;
	if(ask == 0)
		return item;
	NMCOMBOBOXEX nm{};
	WCHAR buffer[CBEMAXSTRLEN] = {};
	nm.ceItem.mask = ask;
	nm.ceItem.iItem = index;
	nm.ceItem.pszText = buffer;
	nm.ceItem.cchTextMax = CBEMAXSTRLEN;
	nm.ceItem.lParam = item.lParam;
	Notify(hwnd, nm.hdr, CBEN_GETDISPINFO);
	if(ask & CBEIF_TEXT)
		item.text = nm.ceItem.pszText != nullptr ? nm.ceItem.pszText : L"";
	if(ask & CBEIF_IMAGE)
		item.image = nm.ceItem.iImage;
	if(ask & CBEIF_SELECTEDIMAGE)
		item.selectedImage = nm.ceItem.iSelectedImage;
	if(ask & CBEIF_OVERLAY)
		item.overlay = nm.ceItem.iOverlay;
	if(ask & CBEIF_INDENT)
		item.indent = nm.ceItem.iIndent;
	if((nm.ceItem.mask & CBEIF_DI_SETITEM) && (s = State(hwnd)) != nullptr && static_cast<std::size_t>(index) < s->items.size())
	{
		Item &stored = s->items[index];
		if(ask & CBEIF_TEXT)
		{
			stored.callbackText = false;
			stored.text = item.text;
		}
		if(ask & CBEIF_IMAGE)
			stored.image = item.image;
		if(ask & CBEIF_SELECTEDIMAGE)
			stored.selectedImage = item.selectedImage;
		if(ask & CBEIF_OVERLAY)
			stored.overlay = item.overlay;
		if(ask & CBEIF_INDENT)
			stored.indent = item.indent;
	}
	return item;
}

QSize ImageSize(const ComboExState &s)
{
	int cx = 0, cy = 0;
	if(s.images != nullptr && ImageList_GetIconSize(s.images, &cx, &cy))
		return QSize(cx, cy);
	return QSize(0, 0);
}

int FontHeight(HWND hwnd)
{
	HFONT font = reinterpret_cast<HFONT>(::SendMessage(hwnd, WM_GETFONT, 0, 0));
	HDC dc = ::CreateCompatibleDC(nullptr);
	HGDIOBJ old = ::SelectObject(dc, font != nullptr ? font : static_cast<HFONT>(::GetStockObject(DEFAULT_GUI_FONT)));
	TEXTMETRIC tm{};
	::GetTextMetrics(dc, &tm);
	::SelectObject(dc, old);
	::DeleteDC(dc);
	return std::max(1, static_cast<int>(tm.tmHeight));
}

int ItemHeight(HWND hwnd, const ComboExState &s)
{
	return std::max(FontHeight(hwnd) + 2, ImageSize(s).height() + 2);
}

void UpdateItemHeight(HWND hwnd, ComboExState &s)
{
	if(s.combo == nullptr)
		return;
	const int height = ItemHeight(hwnd, s);
	::SendMessage(s.combo, CB_SETITEMHEIGHT, static_cast<WPARAM>(-1), height);
	::SendMessage(s.combo, CB_SETITEMHEIGHT, 0, height);
}

// Our own height follows the combo box's closed height, as in comctl32.
void FitHeight(HWND hwnd, ComboExState &s)
{
	if(s.combo == nullptr)
		return;
	RECT combo{}, own{};
	::GetWindowRect(s.combo, &combo);
	::GetWindowRect(hwnd, &own);
	if(combo.bottom - combo.top != own.bottom - own.top)
		::SetWindowPos(hwnd, nullptr, 0, 0, own.right - own.left, combo.bottom - combo.top, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void DrawItem(HWND hwnd, const DRAWITEMSTRUCT &dis)
{
	ComboExState *s = State(hwnd);
	if(s == nullptr)
		return;
	HDC dc = dis.hDC;
	RECT rc = dis.rcItem;
	const bool selected = (dis.itemState & ODS_SELECTED) != 0;
	const bool field = (dis.itemState & ODS_COMBOBOXEDIT) != 0;
	const bool disabled = (dis.itemState & ODS_DISABLED) != 0;
	const int index = static_cast<int>(dis.itemID);
	const DWORD exStyle = s->exStyle;
	const HIMAGELIST images = s->images;
	const QSize imageSize = ImageSize(*s);
	// In the selection field, an item with no index is drawn as empty.
	const Item item = Resolve(hwnd, index);
	if((s = State(hwnd)) == nullptr)
		return;

	int x = rc.left + 1;
	if(!field || !(exStyle & CBES_EX_NOEDITIMAGEINDENT))
	{
		if(!field)
			x += item.indent * IndentWidth;
	}
	const bool drawImage = images != nullptr && !(field && (exStyle & (CBES_EX_NOEDITIMAGE | CBES_EX_NOEDITIMAGEINDENT)));
	if(drawImage)
	{
		const int image = (selected && (item.mask & CBEIF_SELECTEDIMAGE)) ? item.selectedImage : item.image;
		if(index >= 0 && image >= 0)
		{
			UINT style = ILD_TRANSPARENT;
			if(item.overlay > 0)
				style |= INDEXTOOVERLAYMASK(item.overlay);
			ImageList_Draw(images, image, dc, x, rc.top + (rc.bottom - rc.top - imageSize.height()) / 2, style);
		}
		x += imageSize.width() + ImageGap;
	} else if(images != nullptr && field && (exStyle & CBES_EX_NOEDITIMAGE) && !(exStyle & CBES_EX_NOEDITIMAGEINDENT))
	{
		x += imageSize.width() + ImageGap; // space kept, no image
	}
	if(index < 0)
		return;

	HGDIOBJ oldFont = ::SelectObject(dc, reinterpret_cast<HFONT>(::SendMessage(hwnd, WM_GETFONT, 0, 0)));
	SIZE extent{};
	::GetTextExtentPoint32(dc, item.text.c_str(), static_cast<int>(item.text.size()), &extent);
	RECT textRect{x, rc.top, std::min(static_cast<int>(rc.right), x + static_cast<int>(extent.cx) + 4), rc.bottom};
	if(selected)
	{
		::FillRect(dc, &textRect, ::GetSysColorBrush(COLOR_HIGHLIGHT));
		::SetTextColor(dc, ::GetSysColor(COLOR_HIGHLIGHTTEXT));
	} else
	{
		::SetTextColor(dc, ::GetSysColor(disabled ? COLOR_GRAYTEXT : COLOR_WINDOWTEXT));
	}
	::SetBkMode(dc, TRANSPARENT);
	RECT inner = textRect;
	inner.left += 2;
	::DrawText(dc, item.text.c_str(), static_cast<int>(item.text.size()), &inner, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | ((exStyle & CBES_EX_TEXTENDELLIPSIS) ? DT_END_ELLIPSIS : 0));
	if(dis.itemState & ODS_FOCUS)
		::DrawFocusRect(dc, &textRect);
	::SelectObject(dc, oldFont);
}

void DeleteNotify(HWND hwnd, int index)
{
	NMCOMBOBOXEX nm{};
	nm.ceItem.mask = CBEIF_LPARAM;
	nm.ceItem.iItem = index;
	if(ComboExState *s = State(hwnd); s != nullptr && static_cast<std::size_t>(index) < s->items.size())
		nm.ceItem.lParam = s->items[index].lParam;
	Notify(hwnd, nm.hdr, CBEN_DELETEITEM);
}

LRESULT InsertItem(HWND hwnd, const COMBOBOXEXITEM *from)
{
	ComboExState *s = State(hwnd);
	if(s == nullptr || from == nullptr || s->combo == nullptr)
		return -1;
	int index = static_cast<int>(from->iItem);
	if(index < 0 || static_cast<std::size_t>(index) > s->items.size())
		index = static_cast<int>(s->items.size());
	Item item;
	ToItem(item, *from);
	s->items.insert(s->items.begin() + index, item);
	// The child holds the text too, so CB_GETLBTEXT, CB_FINDSTRING and the
	// edit field work as in comctl32.
	const wchar_t *text = item.callbackText ? L"" : item.text.c_str();
	const LRESULT result = ::SendMessage(s->combo, CB_INSERTSTRING, static_cast<WPARAM>(index), reinterpret_cast<LPARAM>(text));
	if(result < 0)
	{
		if((s = State(hwnd)) != nullptr)
			s->items.erase(s->items.begin() + index);
		return -1;
	}
	NMCOMBOBOXEX nm{};
	nm.ceItem = *from;
	nm.ceItem.iItem = index;
	Notify(hwnd, nm.hdr, CBEN_INSERTITEM);
	return index;
}

LRESULT DeleteItem(HWND hwnd, int index)
{
	ComboExState *s = State(hwnd);
	if(s == nullptr || index < 0 || static_cast<std::size_t>(index) >= s->items.size())
		return CB_ERR;
	DeleteNotify(hwnd, index);
	if((s = State(hwnd)) == nullptr || static_cast<std::size_t>(index) >= s->items.size())
		return CB_ERR;
	s->items.erase(s->items.begin() + index);
	return ::SendMessage(s->combo, CB_DELETESTRING, static_cast<WPARAM>(index), 0);
}

void ResetContent(HWND hwnd)
{
	ComboExState *s = State(hwnd);
	if(s == nullptr)
		return;
	for(int i = static_cast<int>(s->items.size()) - 1; i >= 0; i--)
	{
		DeleteNotify(hwnd, i);
		if((s = State(hwnd)) == nullptr)
			return;
	}
	s->items.clear();
	::SendMessage(s->combo, CB_RESETCONTENT, 0, 0);
}

BOOL GetItem(HWND hwnd, COMBOBOXEXITEM *to)
{
	ComboExState *s = State(hwnd);
	if(s == nullptr || to == nullptr)
		return FALSE;
	int index = static_cast<int>(to->iItem);
	if(index == -1)
	{
		// The selection field: the current selection, as in comctl32.
		index = static_cast<int>(::SendMessage(s->combo, CB_GETCURSEL, 0, 0));
		if(index < 0)
			return FALSE;
	}
	if(index < 0 || static_cast<std::size_t>(index) >= s->items.size())
		return FALSE;
	const Item &item = s->items[index];
	if(to->mask & CBEIF_TEXT)
	{
		if(item.callbackText)
			to->pszText = LPSTR_TEXTCALLBACK;
		else if(to->pszText != nullptr && to->cchTextMax > 0)
		{
			const std::size_t n = std::min(item.text.size(), static_cast<std::size_t>(to->cchTextMax) - 1);
			std::copy_n(item.text.c_str(), n, to->pszText);
			to->pszText[n] = 0;
		}
	}
	if(to->mask & CBEIF_IMAGE)
		to->iImage = item.image;
	if(to->mask & CBEIF_SELECTEDIMAGE)
		to->iSelectedImage = item.selectedImage;
	if(to->mask & CBEIF_OVERLAY)
		to->iOverlay = item.overlay;
	if(to->mask & CBEIF_INDENT)
		to->iIndent = item.indent;
	if(to->mask & CBEIF_LPARAM)
		to->lParam = item.lParam;
	return TRUE;
}

BOOL SetItem(HWND hwnd, const COMBOBOXEXITEM *from)
{
	ComboExState *s = State(hwnd);
	if(s == nullptr || from == nullptr)
		return FALSE;
	int index = static_cast<int>(from->iItem);
	if(index == -1)
		index = static_cast<int>(::SendMessage(s->combo, CB_GETCURSEL, 0, 0));
	if(index < 0 || static_cast<std::size_t>(index) >= s->items.size())
		return FALSE;
	Item &item = s->items[index];
	ToItem(item, *from);
	if(from->mask & CBEIF_TEXT)
	{
		// Keep the child's copy of the text, and its selection.
		const std::wstring text = item.callbackText ? std::wstring() : item.text;
		const HWND combo = s->combo;
		const LRESULT selection = ::SendMessage(combo, CB_GETCURSEL, 0, 0);
		const LRESULT data = ::SendMessage(combo, CB_GETITEMDATA, static_cast<WPARAM>(index), 0);
		::SendMessage(combo, CB_DELETESTRING, static_cast<WPARAM>(index), 0);
		::SendMessage(combo, CB_INSERTSTRING, static_cast<WPARAM>(index), reinterpret_cast<LPARAM>(text.c_str()));
		::SendMessage(combo, CB_SETITEMDATA, static_cast<WPARAM>(index), data);
		if(selection == index)
			::SendMessage(combo, CB_SETCURSEL, static_cast<WPARAM>(index), 0);
	}
	if((s = State(hwnd)) != nullptr)
		::InvalidateRect(s->combo, nullptr, TRUE);
	return TRUE;
}

LRESULT CALLBACK ComboBoxExProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	ComboExState *s = State(hwnd);
	switch(msg)
	{
	case WM_NCCREATE:
		if(WindowData *d = Data(hwnd))
		{
			d->control = std::make_unique<ComboExState>();
			// WS_VSCROLL and WS_HSCROLL are for the list (the child combo
			// gets them in WM_CREATE), not scroll bars beside the control.
			d->style &= ~(WS_VSCROLL | WS_HSCROLL);
			d->scroll[SB_HORZ].shown = d->scroll[SB_VERT].shown = false;
		}
		return ::DefWindowProc(hwnd, msg, wp, lp);
	case WM_CREATE:
		if(s != nullptr)
		{
			const auto *cs = reinterpret_cast<const CREATESTRUCT *>(lp);
			// The child takes the type, the scroll bars and the list's
			// behaviour; it is always owner drawn with strings. CBS_SORT is
			// ignored, as in comctl32.
			const DWORD keep = CBS_SIMPLE | CBS_DROPDOWN | CBS_DROPDOWNLIST | WS_VSCROLL | WS_HSCROLL | CBS_AUTOHSCROLL | CBS_DISABLENOSCROLL | CBS_NOINTEGRALHEIGHT | CBS_UPPERCASE | CBS_LOWERCASE;
			const DWORD style = WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS | (static_cast<DWORD>(cs->style) & keep);
			s->dropTotal = cs->cy;
			const HWND combo = ::CreateWindowEx(0, L"ComboBox", L"", style, 0, 0, cs->cx, cs->cy, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ComboId)), cs->hInstance, nullptr);
			if((s = State(hwnd)) == nullptr)
				return -1;
			s->combo = combo;
			if(combo == nullptr)
				return -1;
			::SendMessage(combo, WM_SETFONT, reinterpret_cast<WPARAM>(::GetStockObject(DEFAULT_GUI_FONT)), FALSE);
			UpdateItemHeight(hwnd, *s);
			FitHeight(hwnd, *s);
		}
		return 0;
	case WM_DESTROY:
		if(s != nullptr)
		{
			ResetContent(hwnd);
			if((s = State(hwnd)) != nullptr && s->combo != nullptr)
			{
				const HWND combo = s->combo;
				s->combo = nullptr;
				::DestroyWindow(combo);
			}
		}
		return 0;
	case WM_NCDESTROY:
		if(WindowData *d = Data(hwnd))
			d->control.reset();
		return ::DefWindowProc(hwnd, msg, wp, lp);
	case WM_WINDOWPOSCHANGING:
		if(s != nullptr && s->combo != nullptr)
		{
			// As a combo box: a taller size is the dropped-down height.
			auto *pos = reinterpret_cast<WINDOWPOS *>(lp);
			if(!(pos->flags & SWP_NOSIZE))
			{
				RECT combo{};
				::GetWindowRect(s->combo, &combo);
				const int closed = combo.bottom - combo.top;
				if(pos->cy > closed)
				{
					s->dropTotal = pos->cy;
					pos->cy = closed;
				}
			}
		}
		return 0;
	case WM_SIZE:
		if(s != nullptr && s->combo != nullptr)
		{
			RECT rc{};
			::GetClientRect(hwnd, &rc);
			::SetWindowPos(s->combo, nullptr, 0, 0, rc.right, std::max(static_cast<int>(rc.bottom), s->dropTotal), SWP_NOZORDER | SWP_NOACTIVATE);
		}
		return 0;
	case WM_SETFONT:
		::DefWindowProc(hwnd, msg, wp, lp);
		if(s != nullptr && s->combo != nullptr)
		{
			::SendMessage(s->combo, WM_SETFONT, wp, lp);
			if((s = State(hwnd)) != nullptr)
			{
				UpdateItemHeight(hwnd, *s);
				FitHeight(hwnd, *s);
			}
		}
		return 0;
	case WM_ENABLE:
		if(s != nullptr && s->combo != nullptr)
			::EnableWindow(s->combo, wp != 0);
		return 0;
	case WM_SETFOCUS:
		if(s != nullptr && s->combo != nullptr)
			::SetFocus(s->combo);
		return 0;
	case WM_GETDLGCODE:
		return s != nullptr && s->combo != nullptr ? ::SendMessage(s->combo, WM_GETDLGCODE, wp, lp) : 0;
	case WM_ERASEBKGND:
		return TRUE;
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		::BeginPaint(hwnd, &ps);
		::EndPaint(hwnd, &ps);
		return 0;
	}
	case WM_SETTEXT:
	case WM_GETTEXT:
	case WM_GETTEXTLENGTH:
		return s != nullptr && s->combo != nullptr ? ::SendMessage(s->combo, msg, wp, lp) : ::DefWindowProc(hwnd, msg, wp, lp);
	case WM_MEASUREITEM:
		if(s != nullptr)
		{
			auto *mis = reinterpret_cast<MEASUREITEMSTRUCT *>(lp);
			mis->itemHeight = static_cast<UINT>(ItemHeight(hwnd, *s));
			return TRUE;
		}
		return FALSE;
	case WM_DRAWITEM:
		DrawItem(hwnd, *reinterpret_cast<const DRAWITEMSTRUCT *>(lp));
		return TRUE;
	case WM_DELETEITEM:
		return TRUE;
	case WM_CTLCOLOREDIT:
	case WM_CTLCOLORLISTBOX:
	case WM_CTLCOLORSTATIC:
		// The parent picks the colours, with us as the control.
		return ::SendMessage(::GetParent(hwnd), msg, wp, reinterpret_cast<LPARAM>(hwnd));
	case WM_COMMAND:
		if(s != nullptr && reinterpret_cast<HWND>(lp) == s->combo)
		{
			// The child's CBN_* notifications are ours.
			const int id = ::GetDlgCtrlID(hwnd);
			return ::SendMessage(::GetParent(hwnd), WM_COMMAND, MAKEWPARAM(id, HIWORD(wp)), reinterpret_cast<LPARAM>(hwnd));
		}
		return 0;
	case WM_NOTIFY:
		return ::SendMessage(::GetParent(hwnd), msg, wp, lp);

	case CBEM_INSERTITEM:
		return InsertItem(hwnd, reinterpret_cast<const COMBOBOXEXITEM *>(lp));
	case CBEM_DELETEITEM: // CB_DELETESTRING
		return DeleteItem(hwnd, static_cast<int>(wp));
	case CBEM_GETITEM:
		return GetItem(hwnd, reinterpret_cast<COMBOBOXEXITEM *>(lp));
	case CBEM_SETITEM:
		return SetItem(hwnd, reinterpret_cast<const COMBOBOXEXITEM *>(lp));
	case CBEM_SETIMAGELIST:
		if(s != nullptr)
		{
			const HIMAGELIST old = s->images;
			s->images = reinterpret_cast<HIMAGELIST>(lp);
			UpdateItemHeight(hwnd, *s);
			if((s = State(hwnd)) != nullptr)
			{
				FitHeight(hwnd, *s);
				if(s->combo != nullptr)
					::InvalidateRect(s->combo, nullptr, TRUE);
			}
			return reinterpret_cast<LRESULT>(old);
		}
		return 0;
	case CBEM_GETIMAGELIST:
		return s != nullptr ? reinterpret_cast<LRESULT>(s->images) : 0;
	case CBEM_GETCOMBOCONTROL:
		return s != nullptr ? reinterpret_cast<LRESULT>(s->combo) : 0;
	case CBEM_GETEDITCONTROL:
		if(s != nullptr && s->combo != nullptr)
		{
			COMBOBOXINFO info{};
			info.cbSize = sizeof(info);
			// For CBS_DROPDOWNLIST, hwndItem is the combo box itself: no edit.
			if(::SendMessage(s->combo, CB_GETCOMBOBOXINFO, 0, reinterpret_cast<LPARAM>(&info)) && info.hwndItem != s->combo)
				return reinterpret_cast<LRESULT>(info.hwndItem);
		}
		return 0;
	case CBEM_SETEXSTYLE:
	case CBEM_SETEXTENDEDSTYLE:
		if(s != nullptr)
		{
			const DWORD old = s->exStyle;
			const DWORD mask = (msg == CBEM_SETEXSTYLE || wp == 0) ? ~DWORD(0) : static_cast<DWORD>(wp);
			s->exStyle = (old & ~mask) | (static_cast<DWORD>(lp) & mask);
			if(s->combo != nullptr)
				::InvalidateRect(s->combo, nullptr, TRUE);
			return old;
		}
		return 0;
	case CBEM_GETEXTENDEDSTYLE:
		return s != nullptr ? s->exStyle : 0;
	case CBEM_HASEDITCHANGED:
		return FALSE;
	case CB_RESETCONTENT:
		ResetContent(hwnd);
		return 0;
	case CB_GETITEMDATA:
		if(s != nullptr && wp < s->items.size())
			return s->items[wp].lParam;
		return CB_ERR;
	case CB_SETITEMDATA:
		if(s != nullptr && wp < s->items.size())
		{
			s->items[wp].lParam = lp;
			s->items[wp].mask |= CBEIF_LPARAM;
			return TRUE;
		}
		return CB_ERR;
	case CB_ADDSTRING:
	case CB_INSERTSTRING:
		{
			// Not documented for ComboBoxEx, but they work: items with text only.
			COMBOBOXEXITEM item{};
			item.mask = CBEIF_TEXT;
			item.iItem = msg == CB_ADDSTRING ? -1 : static_cast<INT_PTR>(static_cast<int>(wp));
			item.pszText = reinterpret_cast<LPWSTR>(lp);
			return InsertItem(hwnd, &item);
		}
	default:
		if(msg >= CB_GETEDITSEL && msg < CB_MSGMAX && s != nullptr && s->combo != nullptr)
			return ::SendMessage(s->combo, msg, wp, lp);
		return ::DefWindowProc(hwnd, msg, wp, lp);
	}
}

} // namespace

void RegisterComboBoxExClass()
{
	WNDCLASS wc{};
	wc.lpfnWndProc = ComboBoxExProc;
	wc.hCursor = ::LoadCursor(nullptr, IDC_ARROW);
	wc.lpszClassName = WC_COMBOBOXEX;
	::RegisterClass(&wc);
}

} // namespace linopenmpt::qtwin
