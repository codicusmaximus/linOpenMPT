// The tree view of comctl32 (SysTreeView32): items in a hierarchy with
// images, state images and check boxes, bold and cut items, lines and
// expand buttons, single expand, callbacks (LPSTR_TEXTCALLBACK,
// I_IMAGECALLBACK, I_CHILDRENCALLBACK), sorting, label editing, drag
// notifications, an insert mark, custom draw and comctl32's notifications.
// The behaviour follows Wine's comctl32 where the documentation is quiet:
// NM_CLICK goes out before a click selects, TVM_EXPAND notifies only the
// first time an item expands, and a click selects when the mouse comes up
// without having dragged.
#include "QtWin.h"

#include <QtCore/QElapsedTimer>
#include <QtGui/QPainter>
#include <QtWidgets/QApplication>
#include <QtWidgets/QStyle>
#include <QtWidgets/QStyleOption>

#include <algorithm>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

namespace linopenmpt::qtwin
{

namespace
{

constexpr int MinimumIndent = 19; // comctl32's minimum, also its default
constexpr int LabelPadding = 2;   // between the label's edge and its text
constexpr int ImageGap = 3;       // after the normal image
constexpr UINT_PTR EditTimer = 43;

struct Node
{
	Node *parent = nullptr;
	Node *first = nullptr;
	Node *last = nullptr;
	Node *next = nullptr;
	Node *prev = nullptr;
	int level = -1;
	std::wstring text;
	bool textCallback = false;
	int image = 0;
	int selectedImage = 0;
	int expandedImage = -1;
	int children = I_CHILDRENAUTO; // or 0, 1, I_CHILDRENCALLBACK
	LPARAM param = 0;
	UINT state = 0;
	int integral = 1;
	int textWidth = -1; // cached, -1 when it needs measuring
};

struct TreeState : ControlState
{
	Node root;
	std::unordered_set<const Node *> alive;
	std::vector<Node *> visible; // the expanded items, top to bottom
	bool dirty = true;
	Node *caret = nullptr;
	Node *dropHilite = nullptr;
	Node *insertMark = nullptr;
	bool insertAfter = false;
	Node *hot = nullptr;
	int top = 0;       // first visible row
	int scrollX = 0;   // in pixels
	int indent = MinimumIndent;
	bool indentSet = false;
	int itemHeight = 16;
	bool heightSet = false;
	HIMAGELIST normal = nullptr;
	HIMAGELIST stateList = nullptr;
	COLORREF bk = CLR_NONE;
	COLORREF text = CLR_NONE;
	COLORREF line = CLR_DEFAULT;
	COLORREF insertColor = CLR_DEFAULT;
	DWORD exStyle = 0;
	HFONT bold = nullptr;
	HFONT boldFrom = nullptr;
	bool focused = false;
	HWND edit = nullptr;
	Node *editItem = nullptr;
	Node *pendingEdit = nullptr;
	// A press that may become a click or a drag.
	bool armed = false;
	bool armedRight = false;
	POINT armedAt{};
	Node *armedItem = nullptr;
	UINT armedFlags = 0;
	std::wstring search;
	QElapsedTimer searchTime;
	UINT scrollTime = 200;
	bool updatingScroll = false;
	int contentWidth = 0;
};

HTREEITEM Handle(const Node *n)
{
	return reinterpret_cast<HTREEITEM>(const_cast<Node *>(n));
}

LONG_PTR StyleOf(HWND hwnd)
{
	return ::GetWindowLongPtr(hwnd, GWL_STYLE);
}

HFONT FontOfControl(HWND hwnd)
{
	const WindowData *d = Data(hwnd);
	return d != nullptr && d->font != nullptr ? d->font : static_cast<HFONT>(::GetStockObject(DEFAULT_GUI_FONT));
}

HFONT BoldFont(HWND hwnd, TreeState &s)
{
	HFONT base = FontOfControl(hwnd);
	if(s.bold == nullptr || s.boldFrom != base)
	{
		if(s.bold != nullptr)
			::DeleteObject(s.bold);
		LOGFONT lf{};
		::GetObject(base, sizeof(lf), &lf);
		lf.lfWeight = FW_BOLD;
		s.bold = ::CreateFontIndirect(&lf);
		s.boldFrom = base;
	}
	return s.bold;
}

RECT Client(HWND hwnd)
{
	RECT rc{};
	::GetClientRect(hwnd, &rc);
	return rc;
}

QRect ToRect(const RECT &r)
{
	return QRect(QPoint(r.left, r.top), QPoint(r.right - 1, r.bottom - 1));
}

RECT ToRECT(const QRect &r)
{
	return RECT{r.left(), r.top(), r.left() + r.width(), r.top() + r.height()};
}

COLORREF Resolve(COLORREF c, int sys)
{
	return (c == CLR_DEFAULT || c == CLR_NONE) ? ::GetSysColor(sys) : c;
}

LRESULT Notify(HWND hwnd, NMHDR &hdr, UINT code)
{
	hdr.hwndFrom = hwnd;
	hdr.idFrom = static_cast<UINT_PTR>(::GetDlgCtrlID(hwnd));
	hdr.code = code;
	const HWND parent = ::GetParent(hwnd);
	return parent != nullptr ? ::SendMessage(parent, WM_NOTIFY, hdr.idFrom, reinterpret_cast<LPARAM>(&hdr)) : 0;
}

LRESULT NotifySimple(HWND hwnd, UINT code)
{
	NMHDR hdr{};
	return Notify(hwnd, hdr, code);
}

// The handle as an item of this tree, or nullptr.
Node *Item(const TreeState &s, HTREEITEM h)
{
	const auto *n = reinterpret_cast<const Node *>(h);
	return (n != nullptr && s.alive.count(n)) ? const_cast<Node *>(n) : nullptr;
}

void FillItem(const Node *n, TVITEM &item, UINT mask)
{
	item.mask = mask;
	item.hItem = Handle(n);
	if(n != nullptr)
	{
		item.state = n->state;
		item.lParam = n->param;
	}
}

// The parent's answers to TVN_GETDISPINFO, for callback fields.
void DispInfo(HWND hwnd, const Node *n, UINT mask, NMTVDISPINFO &di, wchar_t *buffer, int size)
{
	di.item.mask = mask;
	di.item.hItem = Handle(n);
	di.item.state = n->state;
	di.item.stateMask = 0xFFFF;
	di.item.lParam = n->param;
	di.item.pszText = buffer;
	di.item.cchTextMax = size;
	di.item.iImage = n->image;
	di.item.iSelectedImage = n->selectedImage;
	di.item.cChildren = n->children;
	buffer[0] = 0;
	Notify(hwnd, di.hdr, TVN_GETDISPINFO);
}

std::wstring TextOf(HWND hwnd, const Node *n)
{
	if(!n->textCallback)
		return n->text;
	wchar_t buffer[1024];
	NMTVDISPINFO di{};
	DispInfo(hwnd, n, TVIF_TEXT, di, buffer, 1024);
	if(di.item.pszText == nullptr)
		return std::wstring();
	return std::wstring(di.item.pszText);
}

int ImageOf(HWND hwnd, const Node *n, bool selected)
{
	const int image = selected ? n->selectedImage : n->image;
	if(image != I_IMAGECALLBACK)
		return image;
	wchar_t buffer[2];
	NMTVDISPINFO di{};
	DispInfo(hwnd, n, selected ? TVIF_SELECTEDIMAGE : TVIF_IMAGE, di, buffer, 2);
	return selected ? di.item.iSelectedImage : di.item.iImage;
}

bool HasChildren(HWND hwnd, const Node *n)
{
	switch(n->children)
	{
	case I_CHILDRENAUTO:
		return n->first != nullptr;
	case I_CHILDRENCALLBACK:
		{
			wchar_t buffer[2];
			NMTVDISPINFO di{};
			DispInfo(hwnd, n, TVIF_CHILDREN, di, buffer, 2);
			return di.item.cChildren != 0;
		}
	default:
		return n->children != 0;
	}
}

bool Expanded(const Node *n)
{
	return (n->state & TVIS_EXPANDED) != 0;
}

void Rebuild(TreeState &s)
{
	if(!s.dirty)
		return;
	s.visible.clear();
	// Depth first over the expanded items.
	std::vector<Node *> stack;
	for(Node *c = s.root.last; c != nullptr; c = c->prev)
		stack.push_back(c);
	while(!stack.empty())
	{
		Node *n = stack.back();
		stack.pop_back();
		s.visible.push_back(n);
		if(Expanded(n))
		{
			for(Node *c = n->last; c != nullptr; c = c->prev)
				stack.push_back(c);
		}
	}
	s.dirty = false;
}

int VisibleIndex(TreeState &s, const Node *n)
{
	Rebuild(s);
	const auto it = std::find(s.visible.begin(), s.visible.end(), n);
	return it == s.visible.end() ? -1 : static_cast<int>(it - s.visible.begin());
}

int ImageWidth(HIMAGELIST list)
{
	int cx = 0, cy = 0;
	if(list != nullptr && ImageList_GetIconSize(list, &cx, &cy))
		return cx;
	return 0;
}

int ImageHeight(HIMAGELIST list)
{
	int cx = 0, cy = 0;
	if(list != nullptr && ImageList_GetIconSize(list, &cx, &cy))
		return cy;
	return 0;
}

void Measure(HWND hwnd, TreeState &s)
{
	if(!s.heightSet)
	{
		HDC dc = ::GetDC(hwnd);
		HGDIOBJ old = ::SelectObject(dc, FontOfControl(hwnd));
		TEXTMETRIC tm{};
		::GetTextMetrics(dc, &tm);
		::SelectObject(dc, old);
		::ReleaseDC(hwnd, dc);
		int h = std::max({static_cast<int>(tm.tmHeight + tm.tmExternalLeading) + 2, ImageHeight(s.normal), ImageHeight(s.stateList)});
		if(!(StyleOf(hwnd) & TVS_NONEVENHEIGHT))
			h = (h + 1) & ~1;
		s.itemHeight = std::max(1, h);
	}
	if(!s.indentSet)
		s.indent = std::max(MinimumIndent, ImageWidth(s.normal) + ImageGap);
	for(const Node *n : s.alive)
		const_cast<Node *>(n)->textWidth = -1;
}

int TextWidth(HWND hwnd, TreeState &s, Node *n)
{
	if(n->textWidth >= 0 && !n->textCallback)
		return n->textWidth;
	const std::wstring text = TextOf(hwnd, n);
	HDC dc = ::GetDC(hwnd);
	HGDIOBJ old = ::SelectObject(dc, (n->state & TVIS_BOLD) ? BoldFont(hwnd, s) : FontOfControl(hwnd));
	SIZE size{};
	::GetTextExtentPoint32(dc, text.c_str(), static_cast<int>(text.size()), &size);
	::SelectObject(dc, old);
	::ReleaseDC(hwnd, dc);
	n->textWidth = static_cast<int>(size.cx);
	return n->textWidth;
}

bool RootLines(HWND hwnd)
{
	const LONG_PTR style = StyleOf(hwnd);
	return (style & TVS_LINESATROOT) && (style & (TVS_HASLINES | TVS_HASBUTTONS));
}

int RowsInView(HWND hwnd, const TreeState &s)
{
	return std::max(0, static_cast<int>(Client(hwnd).bottom) / std::max(1, s.itemHeight));
}

// The parts of a visible item's row, in client coordinates.
struct Parts
{
	QRect row;
	QRect button; // the indent slot before the item
	QRect state;
	QRect icon;
	QRect label;
};

Parts PartsOf(HWND hwnd, TreeState &s, int index)
{
	Node *n = s.visible[static_cast<std::size_t>(index)];
	Parts p;
	const int y = (index - s.top) * s.itemHeight;
	const int width = static_cast<int>(Client(hwnd).right);
	p.row = QRect(0, y, std::max(width, s.contentWidth - s.scrollX), s.itemHeight);
	const int depth = n->level + (RootLines(hwnd) ? 1 : 0);
	int x = depth * s.indent - s.scrollX;
	if(depth > 0)
		p.button = QRect(x - s.indent, y, s.indent, s.itemHeight);
	const int stateWidth = ImageWidth(s.stateList);
	p.state = QRect(x, y, stateWidth, s.itemHeight);
	x += stateWidth;
	const int iconWidth = ImageWidth(s.normal);
	p.icon = QRect(x, y, iconWidth, s.itemHeight);
	if(iconWidth > 0)
		x += iconWidth + ImageGap;
	p.label = QRect(x, y, TextWidth(hwnd, s, n) + 2 * LabelPadding, s.itemHeight);
	return p;
}

void UpdateContentWidth(HWND hwnd, TreeState &s)
{
	Rebuild(s);
	int widest = 0;
	const int depthRoot = RootLines(hwnd) ? 1 : 0;
	const int iconWidth = ImageWidth(s.normal);
	const int stateWidth = ImageWidth(s.stateList);
	for(Node *n : s.visible)
	{
		const int right = (n->level + depthRoot) * s.indent + stateWidth + (iconWidth > 0 ? iconWidth + ImageGap : 0) + TextWidth(hwnd, s, n) + 2 * LabelPadding;
		widest = std::max(widest, right);
	}
	s.contentWidth = widest;
}

void UpdateScrollBars(HWND hwnd, TreeState &s)
{
	const LONG_PTR style = StyleOf(hwnd);
	if(s.updatingScroll || (style & TVS_NOSCROLL))
		return;
	s.updatingScroll = true;
	Rebuild(s);
	UpdateContentWidth(hwnd, s);
	for(int pass = 0; pass < 2; pass++)
	{
		const int count = static_cast<int>(s.visible.size());
		const int page = RowsInView(hwnd, s);
		s.top = std::clamp(s.top, 0, std::max(0, count - page));
		SCROLLINFO si{};
		si.cbSize = sizeof(si);
		si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
		si.nMin = 0;
		si.nMax = std::max(0, count - 1);
		si.nPage = static_cast<UINT>(std::max(0, page));
		si.nPos = s.top;
		::SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
		const int width = static_cast<int>(Client(hwnd).right);
		if(style & TVS_NOHSCROLL)
		{
			s.scrollX = 0;
		} else
		{
			s.scrollX = std::clamp(s.scrollX, 0, std::max(0, s.contentWidth - width));
			si.nMax = std::max(0, s.contentWidth - 1);
			si.nPage = static_cast<UINT>(std::max(0, width));
			si.nPos = s.scrollX;
			::SetScrollInfo(hwnd, SB_HORZ, &si, TRUE);
		}
	}
	s.updatingScroll = false;
}

void Relayout(HWND hwnd, TreeState &s)
{
	s.dirty = true;
	Rebuild(s);
	UpdateScrollBars(hwnd, s);
	::InvalidateRect(hwnd, nullptr, TRUE);
}

void EndEdit(HWND hwnd, bool save);

void ScrollTo(HWND hwnd, TreeState &s, int top, int x)
{
	Rebuild(s);
	const int count = static_cast<int>(s.visible.size());
	top = std::clamp(top, 0, std::max(0, count - RowsInView(hwnd, s)));
	x = (StyleOf(hwnd) & TVS_NOHSCROLL) ? 0 : std::clamp(x, 0, std::max(0, s.contentWidth - static_cast<int>(Client(hwnd).right)));
	if(top == s.top && x == s.scrollX)
		return;
	if(s.edit != nullptr)
	{
		EndEdit(hwnd, true);
		if(Control<TreeState>(hwnd) == nullptr)
			return;
	}
	s.top = top;
	s.scrollX = x;
	UpdateScrollBars(hwnd, s);
	::InvalidateRect(hwnd, nullptr, TRUE);
}

// Expands the parents and scrolls the item into view.
bool EnsureVisible(HWND hwnd, TreeState &s, Node *n);

// ---- painting --------------------------------------------------------------------------

void FillColor(HDC dc, const QRect &r, COLORREF color)
{
	if(r.width() <= 0 || r.height() <= 0)
		return;
	HBRUSH b = ::CreateSolidBrush(color);
	const RECT rc = ToRECT(r);
	::FillRect(dc, &rc, b);
	::DeleteObject(b);
}

void DrawLines(HWND hwnd, HDC dc, TreeState &s, int index, const Parts &parts)
{
	Win32Widget *w = FromHandle(hwnd);
	QPainter *p = DCPainter(dc);
	if(w == nullptr || p == nullptr)
		return;
	Node *n = s.visible[static_cast<std::size_t>(index)];
	const LONG_PTR style = StyleOf(hwnd);
	const bool lines = (style & TVS_HASLINES) != 0;
	const bool rootLines = RootLines(hwnd);
	p->save();
	QPen pen(ToQColor(Resolve(s.line, COLOR_GRAYTEXT)));
	pen.setStyle(Qt::DotLine);
	pen.setWidth(1);
	p->setPen(pen);
	const int top = parts.row.top();
	const int bottom = parts.row.bottom() + 1;
	const int mid = top + s.itemHeight / 2;
	if(lines && (n->level > 0 || rootLines))
	{
		// This item's own branch: down from the parent, across to the item,
		// and on down if siblings follow.
		const int cx = parts.button.left() + s.indent / 2;
		const bool firstAtRoot = n->parent == &s.root && n->prev == nullptr;
		p->drawLine(cx, firstAtRoot ? mid : top, cx, n->next != nullptr ? bottom : mid);
		p->drawLine(cx, mid, parts.button.right() + 1, mid);
		// The ancestors' lines that pass by.
		int slot = n->level + (rootLines ? 1 : 0) - 2;
		for(const Node *a = n->parent; a != nullptr && a != &s.root; a = a->parent, slot--)
		{
			if(slot < 0)
				break;
			if(a->next != nullptr)
			{
				const int ax = slot * s.indent - s.scrollX + s.indent / 2;
				p->drawLine(ax, top, ax, bottom);
			}
		}
	}
	p->restore();
	if((style & TVS_HASBUTTONS) && (n->level > 0 || rootLines) && HasChildren(hwnd, n))
	{
		QStyleOption opt;
		opt.initFrom(w);
		const int size = std::min({s.indent, s.itemHeight, 11});
		opt.rect = QRect(parts.button.left() + (s.indent - size) / 2, mid - size / 2, size, size);
		opt.state = QStyle::State_Children | QStyle::State_Item;
		if(Expanded(n))
			opt.state |= QStyle::State_Open;
		if(::IsWindowEnabled(hwnd))
			opt.state |= QStyle::State_Enabled;
		if(!lines)
		{
			// Without lines Qt's branch arrow alone.
			p->save();
			QApplication::style()->drawPrimitive(QStyle::PE_IndicatorBranch, &opt, p, w);
			p->restore();
		} else
		{
			// comctl32's box with a plus or a minus, over the lines.
			p->save();
			const QRect box = opt.rect;
			p->fillRect(box, ToQColor(Resolve(s.bk, COLOR_WINDOW)));
			p->setPen(ToQColor(::GetSysColor(COLOR_GRAYTEXT)));
			p->drawRect(box.adjusted(0, 0, -1, -1));
			p->setPen(ToQColor(Resolve(s.text, COLOR_WINDOWTEXT)));
			const int c = box.left() + box.width() / 2;
			const int m = box.top() + box.height() / 2;
			p->drawLine(box.left() + 2, m, box.right() - 2, m);
			if(!Expanded(n))
				p->drawLine(c, box.top() + 2, c, box.bottom() - 2);
			p->restore();
		}
	}
}

void PaintItem(HWND hwnd, HDC dc, int index, LRESULT pre)
{
	TreeState *s = Control<TreeState>(hwnd);
	if(s == nullptr)
		return;
	Node *n = s->visible[static_cast<std::size_t>(index)];
	const Parts parts = PartsOf(hwnd, *s, index);
	const LONG_PTR style = StyleOf(hwnd);
	const bool selected = n == s->caret;
	const bool drop = n == s->dropHilite || (n->state & TVIS_DROPHILITED);
	const bool fullRow = (style & TVS_FULLROWSELECT) && !(style & TVS_HASLINES);
	const COLORREF bk = Resolve(s->bk, COLOR_WINDOW);
	COLORREF textColor = Resolve(s->text, COLOR_WINDOWTEXT);
	COLORREF textBk = bk;
	bool highlight = false;
	if(drop || (selected && s->dropHilite == nullptr && s->focused))
	{
		highlight = true;
		textColor = ::GetSysColor(COLOR_HIGHLIGHTTEXT);
		textBk = ::GetSysColor(COLOR_HIGHLIGHT);
	} else if(selected && s->dropHilite == nullptr && (style & TVS_SHOWSELALWAYS))
	{
		// Unfocused, comctl32 6 greys the selection.
		highlight = true;
		textBk = ::GetSysColor(COLOR_BTNFACE);
	}
	if(n == s->hot && (style & TVS_TRACKSELECT) && !highlight)
		textColor = ::GetSysColor(COLOR_HOTLIGHT);

	NMTVCUSTOMDRAW cd{};
	cd.nmcd.hdc = dc;
	cd.nmcd.rc = ToRECT(parts.row);
	cd.nmcd.dwItemSpec = reinterpret_cast<DWORD_PTR>(n);
	cd.nmcd.lItemlParam = n->param;
	cd.nmcd.uItemState = (selected ? CDIS_SELECTED : 0) | (selected && s->focused ? CDIS_FOCUS : 0) | (n == s->hot ? CDIS_HOT : 0) | (::IsWindowEnabled(hwnd) ? 0 : CDIS_DISABLED);
	cd.clrText = textColor;
	cd.clrTextBk = textBk;
	cd.iLevel = n->level;
	LRESULT itemResult = CDRF_DODEFAULT;
	HFONT font = (n->state & TVIS_BOLD) ? BoldFont(hwnd, *s) : FontOfControl(hwnd);
	HGDIOBJ oldFont = ::SelectObject(dc, font);
	if(pre & CDRF_NOTIFYITEMDRAW)
	{
		cd.nmcd.dwDrawStage = CDDS_ITEMPREPAINT;
		itemResult = Notify(hwnd, cd.nmcd.hdr, NM_CUSTOMDRAW);
		if((s = Control<TreeState>(hwnd)) == nullptr || index >= static_cast<int>(s->visible.size()) || s->visible[static_cast<std::size_t>(index)] != n)
			return;
		textColor = cd.clrText;
		textBk = cd.clrTextBk;
		if(itemResult & CDRF_NEWFONT)
			font = static_cast<HFONT>(::GetCurrentObject(dc, OBJ_FONT));
	}
	if(!(itemResult & CDRF_SKIPDEFAULT))
	{
		DrawLines(hwnd, dc, *s, index, parts);
		if(s->stateList != nullptr)
		{
			const UINT stateImage = (n->state & TVIS_STATEIMAGEMASK) >> 12;
			if(stateImage > 0)
				ImageList_Draw(s->stateList, static_cast<int>(stateImage), dc, parts.state.left(), parts.state.top() + (parts.state.height() - ImageHeight(s->stateList)) / 2, ILD_TRANSPARENT);
		} else if(style & TVS_CHECKBOXES)
		{
			// comctl32 makes a state image list of check boxes; draw them directly.
			Win32Widget *w = FromHandle(hwnd);
			QPainter *p = DCPainter(dc);
			const UINT stateImage = (n->state & TVIS_STATEIMAGEMASK) >> 12;
			if(w != nullptr && p != nullptr && stateImage > 0)
			{
				QStyleOptionButton opt;
				opt.initFrom(w);
				const int size = std::min(QApplication::style()->pixelMetric(QStyle::PM_IndicatorWidth, nullptr, w), s->itemHeight);
				opt.rect = QRect(parts.state.left() - size - 2, parts.state.top() + (s->itemHeight - size) / 2, size, size);
				opt.state &= ~(QStyle::State_HasFocus | QStyle::State_MouseOver);
				opt.state |= stateImage == 2 ? QStyle::State_On : QStyle::State_Off;
				p->save();
				QApplication::style()->drawPrimitive(QStyle::PE_IndicatorCheckBox, &opt, p, w);
				p->restore();
			}
		}
		if(s->normal != nullptr)
		{
			int image = ImageOf(hwnd, n, selected);
			if(Expanded(n) && n->expandedImage >= 0)
				image = n->expandedImage;
			if((s = Control<TreeState>(hwnd)) == nullptr)
				return;
			if(image >= 0)
			{
				QPainter *p = DCPainter(dc);
				const bool cut = (n->state & TVIS_CUT) != 0;
				if(cut && p != nullptr)
				{
					p->save();
					p->setOpacity(0.5);
				}
				const UINT overlay = n->state & TVIS_OVERLAYMASK;
				ImageList_Draw(s->normal, image, dc, parts.icon.left(), parts.icon.top() + (parts.icon.height() - ImageHeight(s->normal)) / 2, ILD_TRANSPARENT | overlay);
				if(cut && p != nullptr)
					p->restore();
			}
		}
		QRect label = parts.label;
		if(fullRow)
			label.setRight(std::max(label.right(), parts.row.right()));
		if(highlight || textBk != bk)
			FillColor(dc, label, textBk);
		const std::wstring text = TextOf(hwnd, n);
		if((s = Control<TreeState>(hwnd)) == nullptr)
			return;
		RECT tr = ToRECT(parts.label.adjusted(LabelPadding, 0, -LabelPadding, 0));
		::SetTextColor(dc, textColor);
		::SetBkMode(dc, TRANSPARENT);
		::DrawText(dc, text.c_str(), static_cast<int>(text.size()), &tr, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_LEFT);
		if(selected && s->focused && s->dropHilite == nullptr && !(::SendMessage(hwnd, WM_QUERYUISTATE, 0, 0) & UISF_HIDEFOCUS))
		{
			const RECT fr = ToRECT(label);
			::SetTextColor(dc, RGB(0, 0, 0));
			::SetBkColor(dc, RGB(255, 255, 255));
			::DrawFocusRect(dc, &fr);
		}
		if(n == s->insertMark)
		{
			const int y = s->insertAfter ? parts.row.bottom() : parts.row.top();
			FillColor(dc, QRect(parts.state.left(), y - 1, std::max(parts.label.right() - parts.state.left(), 8), 2), Resolve(s->insertColor, COLOR_WINDOWTEXT));
		}
	}
	if(itemResult & CDRF_NOTIFYPOSTPAINT)
	{
		cd.nmcd.dwDrawStage = CDDS_ITEMPOSTPAINT;
		Notify(hwnd, cd.nmcd.hdr, NM_CUSTOMDRAW);
	}
	::SelectObject(dc, oldFont);
}

void PaintTree(HWND hwnd, HDC dc)
{
	TreeState *s = Control<TreeState>(hwnd);
	if(s == nullptr)
		return;
	Rebuild(*s);
	const RECT rc = Client(hwnd);
	FillColor(dc, ToRect(rc), Resolve(s->bk, COLOR_WINDOW));
	HGDIOBJ oldFont = ::SelectObject(dc, FontOfControl(hwnd));
	NMTVCUSTOMDRAW cd{};
	cd.nmcd.hdc = dc;
	cd.nmcd.rc = rc;
	cd.nmcd.dwDrawStage = CDDS_PREPAINT;
	const LRESULT pre = Notify(hwnd, cd.nmcd.hdr, NM_CUSTOMDRAW);
	if((s = Control<TreeState>(hwnd)) == nullptr)
		return;
	if(!(pre & CDRF_SKIPDEFAULT))
	{
		const int rows = RowsInView(hwnd, *s) + 1;
		for(int i = s->top; i < static_cast<int>(s->visible.size()) && i < s->top + rows; i++)
		{
			PaintItem(hwnd, dc, i, pre);
			if((s = Control<TreeState>(hwnd)) == nullptr)
				return;
			Rebuild(*s);
		}
	}
	if(pre & CDRF_NOTIFYPOSTPAINT)
	{
		cd.nmcd.dwDrawStage = CDDS_POSTPAINT;
		Notify(hwnd, cd.nmcd.hdr, NM_CUSTOMDRAW);
	}
	::SelectObject(dc, oldFont);
}

// ---- items -----------------------------------------------------------------------------

void CopyIn(HWND hwnd, Node &n, const TVITEMEX &in)
{
	if(in.mask & TVIF_TEXT)
	{
		n.textCallback = in.pszText == LPSTR_TEXTCALLBACK;
		n.text = (n.textCallback || in.pszText == nullptr) ? std::wstring() : std::wstring(in.pszText);
		n.textWidth = -1;
	}
	if(in.mask & TVIF_IMAGE)
		n.image = in.iImage;
	if(in.mask & TVIF_SELECTEDIMAGE)
		n.selectedImage = in.iSelectedImage;
	if(in.mask & TVIF_PARAM)
		n.param = in.lParam;
	if(in.mask & TVIF_CHILDREN)
		n.children = in.cChildren == I_CHILDRENCALLBACK ? I_CHILDRENCALLBACK : (in.cChildren == I_CHILDRENAUTO ? I_CHILDRENAUTO : (in.cChildren != 0 ? 1 : 0));
	if(in.mask & TVIF_INTEGRAL)
		n.integral = std::max(1, in.iIntegral);
	if(in.mask & TVIF_STATE)
	{
		// The selection belongs to the tree; TVIS_SELECTED here only sets the bit.
		const UINT old = n.state;
		n.state = (n.state & ~in.stateMask) | (in.state & in.stateMask);
		if((old ^ n.state) & TVIS_BOLD)
			n.textWidth = -1;
	}
	(void)hwnd;
}

int CompareNodes(HWND hwnd, const Node *a, const Node *b)
{
	return QString::compare(ToQString(TextOf(hwnd, a).c_str()), ToQString(TextOf(hwnd, b).c_str()), Qt::CaseInsensitive);
}

void Link(Node *parent, Node *n, Node *after)
{
	n->parent = parent;
	n->prev = after;
	n->next = after != nullptr ? after->next : parent->first;
	if(n->next != nullptr)
		n->next->prev = n;
	else
		parent->last = n;
	if(after != nullptr)
		after->next = n;
	else
		parent->first = n;
}

void Unlink(Node *n)
{
	Node *parent = n->parent;
	if(n->prev != nullptr)
		n->prev->next = n->next;
	else
		parent->first = n->next;
	if(n->next != nullptr)
		n->next->prev = n->prev;
	else
		parent->last = n->prev;
	n->prev = n->next = nullptr;
}

Node *InsertItem(HWND hwnd, TreeState &s, const TVINSERTSTRUCT *is)
{
	if(is == nullptr)
		return nullptr;
	Node *parent = (is->hParent == nullptr || is->hParent == TVI_ROOT) ? &s.root : Item(s, is->hParent);
	if(parent == nullptr)
		return nullptr;
	auto node = std::make_unique<Node>();
	Node *n = node.get();
	n->level = parent->level + 1;
	CopyIn(hwnd, *n, is->itemex);
	n->state &= ~TVIS_SELECTED;
	Node *after = nullptr;
	if(is->hInsertAfter == TVI_FIRST)
	{
		after = nullptr;
	} else if(is->hInsertAfter == TVI_LAST || is->hInsertAfter == nullptr || is->hInsertAfter == TVI_ROOT)
	{
		after = parent->last;
	} else if(is->hInsertAfter == TVI_SORT)
	{
		after = nullptr;
		for(Node *c = parent->first; c != nullptr; c = c->next)
		{
			if(CompareNodes(hwnd, c, n) > 0)
				break;
			after = c;
		}
	} else
	{
		after = Item(s, is->hInsertAfter);
		// An item that is not a child of this parent: comctl32 appends.
		if(after == nullptr || after->parent != parent)
			after = parent->last;
	}
	s.alive.insert(n);
	node.release();
	Link(parent, n, after);
	if(parent == &s.root || Expanded(parent))
		Relayout(hwnd, s);
	else if(parent->first == n && parent->last == n)
		::InvalidateRect(hwnd, nullptr, TRUE); // the parent's button appears
	return n;
}

void SendDeleteNotify(HWND hwnd, Node *n)
{
	NMTREEVIEW nm{};
	nm.action = TVC_UNKNOWN;
	FillItem(n, nm.itemOld, TVIF_HANDLE | TVIF_PARAM);
	Notify(hwnd, nm.hdr, TVN_DELETEITEM);
}

bool SelectItem(HWND hwnd, Node *n, UINT action);

void DeleteSubtree(HWND hwnd, TreeState *&s, Node *n)
{
	// Children first, as comctl32 does.
	while(n->first != nullptr)
	{
		DeleteSubtree(hwnd, s, n->first);
		if((s = Control<TreeState>(hwnd)) == nullptr)
			return;
	}
	SendDeleteNotify(hwnd, n);
	if((s = Control<TreeState>(hwnd)) == nullptr)
		return;
	if(s->dropHilite == n)
		s->dropHilite = nullptr;
	if(s->insertMark == n)
		s->insertMark = nullptr;
	if(s->hot == n)
		s->hot = nullptr;
	if(s->pendingEdit == n)
		s->pendingEdit = nullptr;
	if(s->armedItem == n)
	{
		s->armedItem = nullptr;
		s->armed = false;
	}
	Unlink(n);
	s->alive.erase(n);
	s->dirty = true;
	delete n;
}

bool DeleteItem(HWND hwnd, HTREEITEM h)
{
	TreeState *s = Control<TreeState>(hwnd);
	if(s == nullptr)
		return false;
	if(s->edit != nullptr)
	{
		EndEdit(hwnd, false);
		if((s = Control<TreeState>(hwnd)) == nullptr)
			return false;
	}
	if(h == nullptr || h == TVI_ROOT)
	{
		s->caret = nullptr;
		while(s->root.first != nullptr)
		{
			DeleteSubtree(hwnd, s, s->root.first);
			if(s == nullptr)
				return false;
		}
		s->top = 0;
		s->scrollX = 0;
		Relayout(hwnd, *s);
		return true;
	}
	Node *n = Item(*s, h);
	if(n == nullptr)
		return false;
	Node *parent = n->parent;
	// The selection moves off a deleted item: to the next sibling, else the
	// parent, else the previous sibling (Wine's order).
	bool caretInside = false;
	for(Node *c = s->caret; c != nullptr; c = c->parent)
	{
		if(c == n)
		{
			caretInside = true;
			break;
		}
	}
	Node *newCaret = nullptr;
	if(caretInside)
		newCaret = n->next != nullptr ? n->next : (parent != &s->root ? parent : n->prev);
	DeleteSubtree(hwnd, s, n);
	if(s == nullptr)
		return true;
	if(parent != &s->root && parent->first == nullptr)
		parent->state &= ~TVIS_EXPANDED;
	if(caretInside)
	{
		s->caret = nullptr;
		if(newCaret != nullptr)
			SelectItem(hwnd, newCaret, TVC_UNKNOWN);
		if((s = Control<TreeState>(hwnd)) == nullptr)
			return true;
	}
	Relayout(hwnd, *s);
	return true;
}

bool Expand(HWND hwnd, Node *n, UINT code, bool user);

void SingleExpand(HWND hwnd, Node *oldCaret, Node *newCaret)
{
	// The old branch collapses unless the new item is inside it.
	if(oldCaret != nullptr)
	{
		bool ancestor = false;
		for(Node *c = newCaret; c != nullptr; c = c->parent)
		{
			if(c == oldCaret)
				ancestor = true;
		}
		if(!ancestor)
		{
			for(Node *c = oldCaret; c != nullptr && c->level >= 0; c = c->parent)
			{
				bool shared = false;
				for(Node *d = newCaret; d != nullptr; d = d->parent)
				{
					if(d == c)
						shared = true;
				}
				if(shared)
					break;
				if(Expanded(c))
					Expand(hwnd, c, TVE_COLLAPSE, true);
				TreeState *s = Control<TreeState>(hwnd);
				if(s == nullptr || !s->alive.count(c))
					return;
			}
		}
	}
	if(newCaret != nullptr && !Expanded(newCaret))
		Expand(hwnd, newCaret, TVE_EXPAND, true);
}

bool SelectItem(HWND hwnd, Node *n, UINT action)
{
	TreeState *s = Control<TreeState>(hwnd);
	if(s == nullptr)
		return false;
	Node *old = s->caret;
	if(old == n)
	{
		if(n != nullptr)
			EnsureVisible(hwnd, *s, n);
		return true;
	}
	NMTREEVIEW nm{};
	nm.action = action;
	FillItem(old, nm.itemOld, TVIF_HANDLE | TVIF_STATE | TVIF_PARAM);
	FillItem(n, nm.itemNew, TVIF_HANDLE | TVIF_STATE | TVIF_PARAM);
	if(Notify(hwnd, nm.hdr, TVN_SELCHANGING) != 0)
		return false;
	if((s = Control<TreeState>(hwnd)) == nullptr)
		return false;
	if(n != nullptr && !s->alive.count(n))
		return false;
	if(old != nullptr && s->alive.count(old))
		old->state &= ~TVIS_SELECTED;
	else
		old = nullptr;
	s->caret = n;
	if(n != nullptr)
		n->state |= TVIS_SELECTED;
	if(n != nullptr)
		EnsureVisible(hwnd, *s, n);
	::InvalidateRect(hwnd, nullptr, TRUE);
	FillItem(old, nm.itemOld, TVIF_HANDLE | TVIF_STATE | TVIF_PARAM);
	FillItem(n, nm.itemNew, TVIF_HANDLE | TVIF_STATE | TVIF_PARAM);
	Notify(hwnd, nm.hdr, TVN_SELCHANGED);
	if((s = Control<TreeState>(hwnd)) == nullptr)
		return true;
	if((StyleOf(hwnd) & TVS_SINGLEEXPAND) && action != TVC_UNKNOWN && n != nullptr)
	{
		NMTREEVIEW se{};
		se.action = action;
		FillItem(old, se.itemOld, TVIF_HANDLE | TVIF_PARAM);
		FillItem(n, se.itemNew, TVIF_HANDLE | TVIF_PARAM);
		const LRESULT r = Notify(hwnd, se.hdr, TVN_SINGLEEXPAND);
		if((s = Control<TreeState>(hwnd)) == nullptr)
			return true;
		if(!(r & TVNRET_SKIPOLD))
			SingleExpand(hwnd, (old != nullptr && s->alive.count(old)) ? old : nullptr, (r & TVNRET_SKIPNEW) ? nullptr : n);
	}
	return true;
}

bool Expand(HWND hwnd, Node *n, UINT code, bool user)
{
	TreeState *s = Control<TreeState>(hwnd);
	if(s == nullptr || n == nullptr)
		return false;
	UINT action = code & TVE_ACTIONMASK;
	if(action == TVE_TOGGLE)
		action = Expanded(n) ? TVE_COLLAPSE : TVE_EXPAND;
	if(action == TVE_EXPAND)
	{
		if(Expanded(n))
			return true;
		if(!HasChildren(hwnd, n))
			return false;
		const bool notify = user || !(n->state & TVIS_EXPANDEDONCE);
		NMTREEVIEW nm{};
		nm.action = TVE_EXPAND;
		FillItem(n, nm.itemNew, TVIF_HANDLE | TVIF_STATE | TVIF_PARAM);
		if(notify && Notify(hwnd, nm.hdr, TVN_ITEMEXPANDING) != 0)
			return false;
		if((s = Control<TreeState>(hwnd)) == nullptr || !s->alive.count(n))
			return false;
		n->state |= TVIS_EXPANDED;
		if(!(code & TVE_EXPANDPARTIAL))
			n->state |= TVIS_EXPANDEDONCE;
		Relayout(hwnd, *s);
		if(notify)
		{
			FillItem(n, nm.itemNew, TVIF_HANDLE | TVIF_STATE | TVIF_PARAM);
			Notify(hwnd, nm.hdr, TVN_ITEMEXPANDED);
		}
		if((s = Control<TreeState>(hwnd)) != nullptr && s->alive.count(n))
		{
			// Show as many of the new children as fit.
			const int index = VisibleIndex(*s, n);
			int lastChild = index;
			for(Node *c = n->first; c != nullptr; c = c->next)
				lastChild++;
			const int rows = RowsInView(hwnd, *s);
			if(user && index >= 0 && lastChild >= s->top + rows)
				ScrollTo(hwnd, *s, std::min(index, lastChild - rows + 1), s->scrollX);
		}
		return true;
	}
	if(action == TVE_COLLAPSE)
	{
		if(!Expanded(n) && !(code & TVE_COLLAPSERESET))
			return false;
		NMTREEVIEW nm{};
		nm.action = TVE_COLLAPSE;
		FillItem(n, nm.itemNew, TVIF_HANDLE | TVIF_STATE | TVIF_PARAM);
		if(user && Notify(hwnd, nm.hdr, TVN_ITEMEXPANDING) != 0)
			return false;
		if((s = Control<TreeState>(hwnd)) == nullptr || !s->alive.count(n))
			return false;
		n->state &= ~TVIS_EXPANDED;
		// A selection inside moves up to the collapsed item.
		bool caretInside = false;
		for(Node *c = s->caret != nullptr ? s->caret->parent : nullptr; c != nullptr; c = c->parent)
		{
			if(c == n)
				caretInside = true;
		}
		if(code & TVE_COLLAPSERESET)
		{
			n->state &= ~TVIS_EXPANDEDONCE;
			while(n->first != nullptr)
			{
				DeleteSubtree(hwnd, s, n->first);
				if(s == nullptr)
					return true;
			}
		}
		Relayout(hwnd, *s);
		if(caretInside)
			SelectItem(hwnd, n, TVC_UNKNOWN);
		if(user && (s = Control<TreeState>(hwnd)) != nullptr && s->alive.count(n))
		{
			FillItem(n, nm.itemNew, TVIF_HANDLE | TVIF_STATE | TVIF_PARAM);
			Notify(hwnd, nm.hdr, TVN_ITEMEXPANDED);
		}
		return true;
	}
	return false;
}

bool EnsureVisible(HWND hwnd, TreeState &s, Node *n)
{
	// Expand the parents without notifications, as TVM_EXPAND would.
	std::vector<Node *> parents;
	for(Node *p = n->parent; p != nullptr && p != &s.root; p = p->parent)
		parents.push_back(p);
	for(auto it = parents.rbegin(); it != parents.rend(); ++it)
	{
		if(!Expanded(*it))
		{
			Expand(hwnd, *it, TVE_EXPAND, false);
			if(Control<TreeState>(hwnd) == nullptr)
				return false;
		}
	}
	const int index = VisibleIndex(s, n);
	if(index < 0)
		return false;
	const int rows = std::max(1, RowsInView(hwnd, s));
	int top = s.top;
	if(index < top)
		top = index;
	else if(index >= top + rows)
		top = index - rows + 1;
	int x = s.scrollX;
	const Parts parts = PartsOf(hwnd, s, index);
	const int width = static_cast<int>(Client(hwnd).right);
	if(parts.label.right() >= width && parts.label.width() < width)
		x += parts.label.right() - width + 1;
	if(parts.label.left() < 0 && parts.state.left() < 0)
		x += parts.state.left();
	ScrollTo(hwnd, s, top, x);
	return true;
}

Node *HitTest(HWND hwnd, TreeState &s, POINT pt, UINT *flags)
{
	Rebuild(s);
	const RECT rc = Client(hwnd);
	UINT f = 0;
	if(pt.x < 0)
		f |= TVHT_TOLEFT;
	else if(pt.x >= rc.right)
		f |= TVHT_TORIGHT;
	if(pt.y < 0)
		f |= TVHT_ABOVE;
	else if(pt.y >= rc.bottom)
		f |= TVHT_BELOW;
	if(f != 0)
	{
		if(flags != nullptr)
			*flags = f;
		return nullptr;
	}
	const int index = s.top + pt.y / std::max(1, s.itemHeight);
	if(index < 0 || index >= static_cast<int>(s.visible.size()))
	{
		if(flags != nullptr)
			*flags = TVHT_NOWHERE;
		return nullptr;
	}
	const Parts parts = PartsOf(hwnd, s, index);
	const int x = pt.x;
	const bool checkBoxes = (StyleOf(hwnd) & TVS_CHECKBOXES) && s.stateList == nullptr;
	if(checkBoxes && x >= parts.state.left() - 18 && x < parts.state.left())
		f = TVHT_ONITEMSTATEICON;
	else if(!parts.button.isNull() && x >= parts.button.left() && x <= parts.button.right())
		f = TVHT_ONITEMBUTTON;
	else if(x < parts.state.left())
		f = TVHT_ONITEMINDENT;
	else if(x <= parts.state.right() && parts.state.width() > 0)
		f = TVHT_ONITEMSTATEICON;
	else if(x <= parts.icon.right() && parts.icon.width() > 0)
		f = TVHT_ONITEMICON;
	else if(x <= parts.label.right())
		f = TVHT_ONITEMLABEL;
	else
		f = TVHT_ONITEMRIGHT;
	if(flags != nullptr)
		*flags = f;
	return s.visible[static_cast<std::size_t>(index)];
}

// ---- label editing -------------------------------------------------------------------------

LRESULT CALLBACK EditSubclass(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

struct EditLink
{
	HWND tree = nullptr;
	WNDPROC old = nullptr;
};

std::vector<std::pair<HWND, EditLink>> &EditLinks()
{
	static std::vector<std::pair<HWND, EditLink>> links;
	return links;
}

EditLink FindEditLink(HWND edit)
{
	for(const auto &l : EditLinks())
	{
		if(l.first == edit)
			return l.second;
	}
	return EditLink{};
}

void EndEdit(HWND hwnd, bool save)
{
	TreeState *s = Control<TreeState>(hwnd);
	if(s == nullptr || s->edit == nullptr)
		return;
	const HWND edit = s->edit;
	Node *n = s->editItem;
	s->edit = nullptr;
	s->editItem = nullptr;
	WCHAR buffer[1024] = {};
	::GetWindowText(edit, buffer, 1024);
	if(n != nullptr && s->alive.count(n))
	{
		NMTVDISPINFO di{};
		di.item.mask = TVIF_TEXT | TVIF_HANDLE | TVIF_PARAM;
		di.item.hItem = Handle(n);
		di.item.pszText = save ? buffer : nullptr;
		di.item.cchTextMax = 1024;
		di.item.lParam = n->param;
		const LRESULT accept = Notify(hwnd, di.hdr, TVN_ENDLABELEDIT);
		if(save && accept && (s = Control<TreeState>(hwnd)) != nullptr && s->alive.count(n))
		{
			if(n->textCallback)
			{
				NMTVDISPINFO set{};
				set.item = di.item;
				set.item.pszText = buffer;
				Notify(hwnd, set.hdr, TVN_SETDISPINFO);
			} else
			{
				n->text = buffer;
				n->textWidth = -1;
			}
			if((s = Control<TreeState>(hwnd)) != nullptr)
				UpdateScrollBars(hwnd, *s);
		}
	}
	::DestroyWindow(edit);
	auto &links = EditLinks();
	links.erase(std::remove_if(links.begin(), links.end(), [edit](const auto &l) { return l.first == edit; }), links.end());
	if(::GetFocus() == nullptr || ::GetFocus() == edit)
		::SetFocus(hwnd);
	::InvalidateRect(hwnd, nullptr, TRUE);
}

LRESULT CALLBACK EditSubclass(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	const EditLink link = FindEditLink(hwnd);
	if(link.old == nullptr)
		return ::DefWindowProc(hwnd, msg, wp, lp);
	switch(msg)
	{
	case WM_GETDLGCODE:
		return DLGC_WANTALLKEYS | ::CallWindowProc(link.old, hwnd, msg, wp, lp);
	case WM_KEYDOWN:
		if(wp == VK_RETURN || wp == VK_ESCAPE)
		{
			EndEdit(link.tree, wp == VK_RETURN);
			return 0;
		}
		break;
	case WM_CHAR:
		if(wp == VK_RETURN || wp == VK_ESCAPE)
			return 0;
		break;
	case WM_KILLFOCUS:
		{
			const LRESULT r = ::CallWindowProc(link.old, hwnd, msg, wp, lp);
			if(TreeState *s = Control<TreeState>(link.tree); s != nullptr && s->edit == hwnd)
				EndEdit(link.tree, true);
			return r;
		}
	default:
		break;
	}
	return ::CallWindowProc(link.old, hwnd, msg, wp, lp);
}

HWND BeginEdit(HWND hwnd, Node *n)
{
	TreeState *s = Control<TreeState>(hwnd);
	if(s == nullptr || n == nullptr || !s->alive.count(n))
		return nullptr;
	if(s->edit != nullptr)
	{
		EndEdit(hwnd, true);
		if((s = Control<TreeState>(hwnd)) == nullptr || !s->alive.count(n))
			return nullptr;
	}
	EnsureVisible(hwnd, *s, n);
	if((s = Control<TreeState>(hwnd)) == nullptr || !s->alive.count(n))
		return nullptr;
	std::wstring text = TextOf(hwnd, n);
	NMTVDISPINFO di{};
	std::wstring copy = text;
	di.item.mask = TVIF_HANDLE | TVIF_TEXT | TVIF_PARAM | TVIF_STATE;
	di.item.hItem = Handle(n);
	di.item.pszText = copy.data();
	di.item.cchTextMax = static_cast<int>(copy.size()) + 1;
	di.item.lParam = n->param;
	di.item.state = n->state;
	if(Notify(hwnd, di.hdr, TVN_BEGINLABELEDIT) != 0)
		return nullptr;
	if((s = Control<TreeState>(hwnd)) == nullptr || !s->alive.count(n))
		return nullptr;
	const int index = VisibleIndex(*s, n);
	if(index < 0)
		return nullptr;
	const QRect label = PartsOf(hwnd, *s, index).label;
	const HWND edit = ::CreateWindowEx(0, L"Edit", text.c_str(), WS_CHILD | WS_BORDER | ES_AUTOHSCROLL | WS_VISIBLE, label.left(), label.top(), std::max(label.width() + 16, 60), label.height(), hwnd, nullptr, nullptr, nullptr);
	if(edit == nullptr)
		return nullptr;
	::SendMessage(edit, WM_SETFONT, reinterpret_cast<WPARAM>(FontOfControl(hwnd)), FALSE);
	EditLinks().emplace_back(edit, EditLink{hwnd, reinterpret_cast<WNDPROC>(::GetWindowLongPtr(edit, GWLP_WNDPROC))});
	::SetWindowLongPtr(edit, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&EditSubclass));
	if((s = Control<TreeState>(hwnd)) == nullptr)
		return nullptr;
	s->edit = edit;
	s->editItem = n;
	::SendMessage(edit, EM_SETSEL, 0, -1);
	::SetFocus(edit);
	return edit;
}

// ---- sorting ---------------------------------------------------------------------------------

template <typename Less>
void SortChildren(Node *parent, bool recurse, const Less &less)
{
	std::vector<Node *> children;
	for(Node *c = parent->first; c != nullptr; c = c->next)
		children.push_back(c);
	std::stable_sort(children.begin(), children.end(), less);
	parent->first = parent->last = nullptr;
	Node *after = nullptr;
	for(Node *c : children)
	{
		c->prev = c->next = nullptr;
		Link(parent, c, after);
		after = c;
	}
	if(recurse)
	{
		for(Node *c : children)
			SortChildren(c, true, less);
	}
}

// ---- input -------------------------------------------------------------------------------------

Node *NextVisible(TreeState &s, Node *n, int delta)
{
	Rebuild(s);
	if(s.visible.empty())
		return nullptr;
	const int index = n != nullptr ? VisibleIndex(s, n) : -1;
	const int target = std::clamp(index < 0 ? 0 : index + delta, 0, static_cast<int>(s.visible.size()) - 1);
	return s.visible[static_cast<std::size_t>(target)];
}

void ExpandAll(HWND hwnd, Node *n)
{
	Expand(hwnd, n, TVE_EXPAND, true);
	TreeState *s = Control<TreeState>(hwnd);
	if(s == nullptr || !s->alive.count(n))
		return;
	for(Node *c = n->first; c != nullptr;)
	{
		Node *next = c->next;
		ExpandAll(hwnd, c);
		if((s = Control<TreeState>(hwnd)) == nullptr)
			return;
		c = s->alive.count(next) ? next : nullptr;
	}
}

void IncrementalSearch(HWND hwnd, WCHAR ch)
{
	TreeState *s = Control<TreeState>(hwnd);
	if(s == nullptr)
		return;
	Rebuild(*s);
	if(s->visible.empty())
		return;
	if(!s->searchTime.isValid() || s->searchTime.elapsed() > 1000)
		s->search.clear();
	s->searchTime.restart();
	const bool repeat = s->search.size() == 1 && s->search[0] == ch;
	if(!repeat)
		s->search.push_back(ch);
	const QString needle = ToQString(s->search.c_str());
	const int count = static_cast<int>(s->visible.size());
	const int caret = s->caret != nullptr ? VisibleIndex(*s, s->caret) : -1;
	const int start = caret < 0 ? 0 : (s->search.size() == 1 ? caret + 1 : caret);
	for(int k = 0; k < count; k++)
	{
		Node *n = s->visible[static_cast<std::size_t>((start + k) % count)];
		if(ToQString(TextOf(hwnd, n).c_str()).startsWith(needle, Qt::CaseInsensitive))
		{
			SelectItem(hwnd, n, TVC_BYKEYBOARD);
			return;
		}
		if((s = Control<TreeState>(hwnd)) == nullptr)
			return;
		Rebuild(*s);
	}
}

void ToggleCheck(HWND hwnd, TreeState &s, Node *n)
{
	const UINT image = (n->state & TVIS_STATEIMAGEMASK) >> 12;
	n->state = (n->state & ~TVIS_STATEIMAGEMASK) | INDEXTOSTATEIMAGEMASK(image == 2 ? 1 : 2);
	::InvalidateRect(hwnd, nullptr, TRUE);
	(void)s;
}

void ScrollMessage(HWND hwnd, TreeState &s, int bar, UINT code)
{
	SCROLLINFO si{};
	si.cbSize = sizeof(si);
	si.fMask = SIF_ALL;
	::GetScrollInfo(hwnd, bar, &si);
	const bool vertical = bar == SB_VERT;
	const int line = vertical ? 1 : 16;
	const int page = vertical ? std::max(1, RowsInView(hwnd, s)) : std::max(1, static_cast<int>(si.nPage));
	int pos = vertical ? s.top : s.scrollX;
	switch(code)
	{
	case SB_LINEUP: pos -= line; break;
	case SB_LINEDOWN: pos += line; break;
	case SB_PAGEUP: pos -= page; break;
	case SB_PAGEDOWN: pos += page; break;
	case SB_THUMBTRACK:
	case SB_THUMBPOSITION: pos = si.nTrackPos; break;
	case SB_TOP: pos = 0; break;
	case SB_BOTTOM: pos = si.nMax; break;
	default: return;
	}
	if(vertical)
		ScrollTo(hwnd, s, pos, s.scrollX);
	else
		ScrollTo(hwnd, s, s.top, pos);
}

bool GetItem(HWND hwnd, TreeState &s, TVITEMEX *out)
{
	if(out == nullptr)
		return false;
	Node *n = Item(s, out->hItem);
	if(n == nullptr)
		return false;
	const UINT mask = out->mask;
	if(mask & TVIF_TEXT)
	{
		if(out->pszText != nullptr && out->cchTextMax > 0)
		{
			const std::wstring text = TextOf(hwnd, n);
			const std::size_t count = std::min(text.size(), static_cast<std::size_t>(out->cchTextMax - 1));
			std::copy_n(text.begin(), count, out->pszText);
			out->pszText[count] = 0;
		}
	}
	if(mask & TVIF_IMAGE)
		out->iImage = n->image;
	if(mask & TVIF_SELECTEDIMAGE)
		out->iSelectedImage = n->selectedImage;
	if(mask & TVIF_PARAM)
		out->lParam = n->param;
	if(mask & TVIF_STATE)
		out->state = n->state & (out->stateMask != 0 ? out->stateMask : 0xFFFFu);
	if(mask & TVIF_CHILDREN)
		out->cChildren = n->children == I_CHILDRENAUTO ? (n->first != nullptr ? 1 : 0) : n->children;
	if(mask & TVIF_INTEGRAL)
		out->iIntegral = n->integral;
	return true;
}

bool SetItem(HWND hwnd, TreeState &s, const TVITEMEX *in)
{
	if(in == nullptr)
		return false;
	Node *n = Item(s, in->hItem);
	if(n == nullptr)
		return false;
	TVITEMEX copy = *in;
	// The selection and expansion bits through TVIF_STATE are plain bits.
	const bool wasExpanded = Expanded(n);
	CopyIn(hwnd, *n, copy);
	if(n == s.caret)
		n->state |= TVIS_SELECTED;
	if(Expanded(n) != wasExpanded)
		Relayout(hwnd, s);
	else
		UpdateScrollBars(hwnd, s);
	::InvalidateRect(hwnd, nullptr, TRUE);
	return true;
}

Node *GetNextItem(HWND hwnd, TreeState &s, Node *n, UINT code)
{
	switch(code)
	{
	case TVGN_ROOT: return s.root.first;
	case TVGN_CARET: return s.caret;
	case TVGN_DROPHILITE: return s.dropHilite;
	case TVGN_FIRSTVISIBLE:
		Rebuild(s);
		return s.top < static_cast<int>(s.visible.size()) ? s.visible[static_cast<std::size_t>(s.top)] : nullptr;
	case TVGN_LASTVISIBLE:
		Rebuild(s);
		return s.visible.empty() ? nullptr : s.visible.back();
	default:
		break;
	}
	if(n == nullptr)
		return code == TVGN_CHILD ? s.root.first : nullptr;
	switch(code)
	{
	case TVGN_NEXT: return n->next;
	case TVGN_PREVIOUS: return n->prev;
	case TVGN_PARENT: return n->parent != &s.root ? n->parent : nullptr;
	case TVGN_CHILD: return n->first;
	case TVGN_NEXTVISIBLE:
	case TVGN_PREVIOUSVISIBLE:
		{
			const int index = VisibleIndex(s, n);
			if(index < 0)
				return nullptr;
			const int target = index + (code == TVGN_NEXTVISIBLE ? 1 : -1);
			return (target >= 0 && target < static_cast<int>(s.visible.size())) ? s.visible[static_cast<std::size_t>(target)] : nullptr;
		}
	default:
		return nullptr;
	}
	(void)hwnd;
}

void Disarm(HWND hwnd, TreeState &s)
{
	s.armed = false;
	s.armedItem = nullptr;
	if(::GetCapture() == hwnd)
		::ReleaseCapture();
}

void DeleteAll(HWND hwnd)
{
	if(TreeState *s = Control<TreeState>(hwnd))
	{
		s->caret = nullptr;
		while(s->root.first != nullptr)
		{
			DeleteSubtree(hwnd, s, s->root.first);
			if(s == nullptr)
				return;
		}
	}
}

LRESULT CALLBACK TreeProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	TreeState *s = Control<TreeState>(hwnd);
	switch(msg)
	{
	case WM_NCCREATE:
		if(WindowData *d = Data(hwnd))
		{
			d->control = std::make_unique<TreeState>();
			auto *t = static_cast<TreeState *>(d->control.get());
			t->root.state = TVIS_EXPANDED;
			t->root.level = -1;
		}
		return ::DefWindowProc(hwnd, msg, wp, lp);
	case WM_CREATE:
		if(s != nullptr)
		{
			Measure(hwnd, *s);
			Relayout(hwnd, *s);
		}
		return 0;
	case WM_DESTROY:
		if(s != nullptr)
		{
			EndEdit(hwnd, false);
			DeleteAll(hwnd);
			if((s = Control<TreeState>(hwnd)) != nullptr && s->bold != nullptr)
			{
				::DeleteObject(s->bold);
				s->bold = nullptr;
			}
		}
		return ::DefWindowProc(hwnd, msg, wp, lp);
	case WM_GETDLGCODE:
		return DLGC_WANTARROWS | DLGC_WANTCHARS;
	case WM_ERASEBKGND:
		return 1;
	case WM_PAINT:
	case WM_PRINTCLIENT:
		if(msg == WM_PRINTCLIENT)
		{
			PaintTree(hwnd, reinterpret_cast<HDC>(wp));
		} else
		{
			PAINTSTRUCT ps;
			HDC dc = ::BeginPaint(hwnd, &ps);
			if(dc != nullptr)
				PaintTree(hwnd, dc);
			::EndPaint(hwnd, &ps);
		}
		return 0;
	case WM_SIZE:
		if(s != nullptr)
		{
			UpdateScrollBars(hwnd, *s);
			::InvalidateRect(hwnd, nullptr, TRUE);
		}
		return 0;
	case WM_ENABLE:
		::InvalidateRect(hwnd, nullptr, TRUE);
		return 0;
	case WM_SETFONT:
		::DefWindowProc(hwnd, msg, wp, 0);
		if(s != nullptr)
		{
			Measure(hwnd, *s);
			Relayout(hwnd, *s);
		}
		return 0;
	case WM_SETFOCUS:
	case WM_KILLFOCUS:
		if(s != nullptr)
		{
			s->focused = msg == WM_SETFOCUS;
			::InvalidateRect(hwnd, nullptr, TRUE);
			NotifySimple(hwnd, msg == WM_SETFOCUS ? NM_SETFOCUS : NM_KILLFOCUS);
		}
		return 0;
	case WM_VSCROLL:
	case WM_HSCROLL:
		if(s != nullptr && lp == 0)
			ScrollMessage(hwnd, *s, msg == WM_VSCROLL ? SB_VERT : SB_HORZ, LOWORD(wp));
		return 0;
	case WM_MOUSEWHEEL:
		if(s != nullptr)
		{
			UINT lines = 3;
			::SystemParametersInfo(SPI_GETWHEELSCROLLLINES, 0, &lines, 0);
			const int delta = GET_WHEEL_DELTA_WPARAM(wp);
			ScrollTo(hwnd, *s, s->top - delta * static_cast<int>(lines) / WHEEL_DELTA, s->scrollX);
		}
		return 0;
	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
		if(s != nullptr)
		{
			if(s->edit != nullptr)
			{
				EndEdit(hwnd, true);
				if((s = Control<TreeState>(hwnd)) == nullptr)
					return 0;
			}
			::KillTimer(hwnd, EditTimer);
			const bool wasFocused = s->focused;
			if(::GetFocus() != hwnd)
				::SetFocus(hwnd);
			if((s = Control<TreeState>(hwnd)) == nullptr)
				return 0;
			POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
			UINT flags = 0;
			Node *n = HitTest(hwnd, *s, pt, &flags);
			if(msg == WM_LBUTTONDOWN && n != nullptr && (flags & TVHT_ONITEMBUTTON))
			{
				Expand(hwnd, n, TVE_TOGGLE, true);
				return 0;
			}
			if(msg == WM_LBUTTONDOWN && n != nullptr && (flags & TVHT_ONITEMSTATEICON) && (StyleOf(hwnd) & TVS_CHECKBOXES))
			{
				ToggleCheck(hwnd, *s, n);
				return 0;
			}
			const bool fullRow = (StyleOf(hwnd) & TVS_FULLROWSELECT) && !(StyleOf(hwnd) & TVS_HASLINES);
			const bool onItem = n != nullptr && ((flags & TVHT_ONITEM) || (fullRow && (flags & (TVHT_ONITEMINDENT | TVHT_ONITEMRIGHT))));
			s->armed = true;
			s->armedRight = msg == WM_RBUTTONDOWN;
			s->armedAt = pt;
			s->armedItem = onItem ? n : nullptr;
			s->armedFlags = flags;
			// A press on the selected label of a focused tree may become an edit.
			s->pendingEdit = (msg == WM_LBUTTONDOWN && onItem && n == s->caret && wasFocused && (flags & TVHT_ONITEMLABEL) && (StyleOf(hwnd) & TVS_EDITLABELS)) ? n : nullptr;
			if(msg == WM_RBUTTONDOWN && onItem)
			{
				// comctl32 highlights the item under a right click.
				s->dropHilite = n;
				::InvalidateRect(hwnd, nullptr, TRUE);
			}
			::SetCapture(hwnd);
		}
		return 0;
	case WM_MOUSEMOVE:
		if(s != nullptr)
		{
			if(StyleOf(hwnd) & TVS_TRACKSELECT)
			{
				POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
				UINT flags = 0;
				Node *n = HitTest(hwnd, *s, pt, &flags);
				Node *hot = (n != nullptr && (flags & TVHT_ONITEM)) ? n : nullptr;
				if(hot != s->hot)
				{
					s->hot = hot;
					::InvalidateRect(hwnd, nullptr, TRUE);
				}
			}
			if(s->armed && (wp & (MK_LBUTTON | MK_RBUTTON)))
			{
				const int dx = std::abs(GET_X_LPARAM(lp) - s->armedAt.x), dy = std::abs(GET_Y_LPARAM(lp) - s->armedAt.y);
				if(dx > ::GetSystemMetrics(SM_CXDRAG) || dy > ::GetSystemMetrics(SM_CYDRAG))
				{
					Node *n = s->armedItem;
					const bool right = s->armedRight;
					const POINT at = s->armedAt;
					s->pendingEdit = nullptr;
					if(right && s->dropHilite == n)
					{
						s->dropHilite = nullptr;
						::InvalidateRect(hwnd, nullptr, TRUE);
					}
					Disarm(hwnd, *s);
					if(n != nullptr && !(StyleOf(hwnd) & TVS_DISABLEDRAGDROP))
					{
						NMTREEVIEW nm{};
						nm.action = right ? TVC_BYMOUSE : TVC_BYMOUSE;
						FillItem(n, nm.itemNew, TVIF_HANDLE | TVIF_STATE | TVIF_PARAM);
						nm.ptDrag = at;
						Notify(hwnd, nm.hdr, right ? TVN_BEGINRDRAG : TVN_BEGINDRAG);
					}
				}
			}
		}
		return 0;
	case WM_LBUTTONUP:
	case WM_RBUTTONUP:
		if(s != nullptr)
		{
			const bool right = msg == WM_RBUTTONUP;
			if(!s->armed || s->armedRight != right)
				return 0;
			Node *n = s->armedItem;
			Node *edit = s->pendingEdit;
			s->pendingEdit = nullptr;
			Disarm(hwnd, *s);
			if(right)
			{
				const LRESULT handled = NotifySimple(hwnd, NM_RCLICK);
				if((s = Control<TreeState>(hwnd)) == nullptr)
					return 0;
				if(s->dropHilite == n && n != nullptr)
				{
					s->dropHilite = nullptr;
					::InvalidateRect(hwnd, nullptr, TRUE);
				}
				if(!handled)
				{
					POINT screen{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
					::ClientToScreen(hwnd, &screen);
					::SendMessage(hwnd, WM_CONTEXTMENU, reinterpret_cast<WPARAM>(hwnd), MAKELPARAM(screen.x, screen.y));
				}
				return 0;
			}
			// NM_CLICK goes out before the selection changes.
			if(NotifySimple(hwnd, NM_CLICK) != 0)
				return 0;
			if((s = Control<TreeState>(hwnd)) == nullptr)
				return 0;
			if(n != nullptr && s->alive.count(n))
			{
				const bool wasSelected = n == s->caret;
				SelectItem(hwnd, n, TVC_BYMOUSE);
				if((s = Control<TreeState>(hwnd)) == nullptr)
					return 0;
				// With single expand, a click on the selected item toggles it.
				if(wasSelected && (StyleOf(hwnd) & TVS_SINGLEEXPAND) && s->alive.count(n) && edit == nullptr)
					Expand(hwnd, n, TVE_TOGGLE, true);
				if((s = Control<TreeState>(hwnd)) != nullptr && edit != nullptr && edit == n)
				{
					s->pendingEdit = edit;
					::SetTimer(hwnd, EditTimer, ::GetDoubleClickTime(), nullptr);
				}
			}
		}
		return 0;
	case WM_CAPTURECHANGED:
		if(s != nullptr && reinterpret_cast<HWND>(lp) != hwnd && s->armed)
		{
			s->armed = false;
			if(s->dropHilite == s->armedItem && s->armedRight && s->dropHilite != nullptr)
			{
				s->dropHilite = nullptr;
				::InvalidateRect(hwnd, nullptr, TRUE);
			}
			s->armedItem = nullptr;
		}
		return 0;
	case WM_TIMER:
		if(s != nullptr && wp == EditTimer)
		{
			::KillTimer(hwnd, EditTimer);
			Node *n = s->pendingEdit;
			s->pendingEdit = nullptr;
			if(n != nullptr && s->alive.count(n) && n == s->caret)
				BeginEdit(hwnd, n);
			return 0;
		}
		return ::DefWindowProc(hwnd, msg, wp, lp);
	case WM_LBUTTONDBLCLK:
	case WM_RBUTTONDBLCLK:
		if(s != nullptr)
		{
			::KillTimer(hwnd, EditTimer);
			s->pendingEdit = nullptr;
			POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
			UINT flags = 0;
			Node *n = HitTest(hwnd, *s, pt, &flags);
			if(msg == WM_RBUTTONDBLCLK)
			{
				NotifySimple(hwnd, NM_RDBLCLK);
				return 0;
			}
			if(n != nullptr && (flags & TVHT_ONITEMBUTTON))
			{
				Expand(hwnd, n, TVE_TOGGLE, true);
				return 0;
			}
			const LRESULT handled = NotifySimple(hwnd, NM_DBLCLK);
			if((s = Control<TreeState>(hwnd)) == nullptr)
				return 0;
			if(!handled && n != nullptr && s->alive.count(n) && (flags & (TVHT_ONITEM | TVHT_ONITEMINDENT | TVHT_ONITEMRIGHT)))
				Expand(hwnd, n, TVE_TOGGLE, true);
		}
		return 0;
	case WM_KEYDOWN:
		if(s != nullptr)
		{
			NMTVKEYDOWN kd{};
			kd.wVKey = static_cast<WORD>(wp);
			kd.flags = static_cast<UINT>(lp);
			Notify(hwnd, kd.hdr, TVN_KEYDOWN);
			if((s = Control<TreeState>(hwnd)) == nullptr)
				return 0;
			Rebuild(*s);
			Node *caret = s->caret;
			const int page = std::max(1, RowsInView(hwnd, *s) - 1);
			const bool ctrl = ::GetKeyState(VK_CONTROL) < 0;
			Node *target = nullptr;
			switch(wp)
			{
			case VK_UP:
			case VK_DOWN:
				if(ctrl)
				{
					ScrollTo(hwnd, *s, s->top + (wp == VK_UP ? -1 : 1), s->scrollX);
					return 0;
				}
				target = NextVisible(*s, caret, wp == VK_UP ? -1 : 1);
				break;
			case VK_PRIOR:
			case VK_NEXT:
				if(ctrl)
				{
					ScrollTo(hwnd, *s, s->top + (wp == VK_PRIOR ? -page : page), s->scrollX);
					return 0;
				}
				target = NextVisible(*s, caret, wp == VK_PRIOR ? -page : page);
				break;
			case VK_HOME:
				target = s->visible.empty() ? nullptr : s->visible.front();
				break;
			case VK_END:
				target = s->visible.empty() ? nullptr : s->visible.back();
				break;
			case VK_LEFT:
				if(ctrl)
				{
					ScrollTo(hwnd, *s, s->top, s->scrollX - 16);
					return 0;
				}
				if(caret == nullptr)
					return 0;
				if(Expanded(caret) && HasChildren(hwnd, caret))
				{
					Expand(hwnd, caret, TVE_COLLAPSE, true);
					return 0;
				}
				target = caret->parent != &s->root ? caret->parent : nullptr;
				break;
			case VK_RIGHT:
				if(ctrl)
				{
					ScrollTo(hwnd, *s, s->top, s->scrollX + 16);
					return 0;
				}
				if(caret == nullptr || !HasChildren(hwnd, caret))
					return 0;
				if(!Expanded(caret))
				{
					Expand(hwnd, caret, TVE_EXPAND, true);
					return 0;
				}
				target = caret->first;
				break;
			case VK_ADD:
				if(caret != nullptr)
					Expand(hwnd, caret, TVE_EXPAND, true);
				return 0;
			case VK_SUBTRACT:
				if(caret != nullptr)
					Expand(hwnd, caret, TVE_COLLAPSE, true);
				return 0;
			case VK_MULTIPLY:
				if(caret != nullptr)
					ExpandAll(hwnd, caret);
				return 0;
			case VK_BACK:
				target = (caret != nullptr && caret->parent != &s->root) ? caret->parent : nullptr;
				break;
			case VK_RETURN:
				NotifySimple(hwnd, NM_RETURN);
				return 0;
			case VK_F2:
				if(caret != nullptr && (StyleOf(hwnd) & TVS_EDITLABELS))
					BeginEdit(hwnd, caret);
				return 0;
			case VK_SPACE:
				if(caret != nullptr && (StyleOf(hwnd) & TVS_CHECKBOXES))
					ToggleCheck(hwnd, *s, caret);
				return 0;
			default:
				return 0;
			}
			if(target != nullptr)
				SelectItem(hwnd, target, TVC_BYKEYBOARD);
		}
		return 0;
	case WM_CHAR:
		if(s != nullptr && wp > 0x20 && wp != 0x7F && ::GetKeyState(VK_CONTROL) >= 0)
			IncrementalSearch(hwnd, static_cast<WCHAR>(wp));
		return 0;

	case TVM_INSERTITEM:
		return s ? reinterpret_cast<LRESULT>(Handle(InsertItem(hwnd, *s, reinterpret_cast<const TVINSERTSTRUCT *>(lp)))) : 0;
	case TVM_DELETEITEM:
		return DeleteItem(hwnd, reinterpret_cast<HTREEITEM>(lp));
	case TVM_EXPAND:
		return s ? Expand(hwnd, Item(*s, reinterpret_cast<HTREEITEM>(lp)), static_cast<UINT>(wp), false) : FALSE;
	case TVM_GETITEMRECT:
		if(s != nullptr && lp != 0)
		{
			auto *rc = reinterpret_cast<RECT *>(lp);
			Node *n = Item(*s, *reinterpret_cast<HTREEITEM *>(lp));
			const int index = n != nullptr ? VisibleIndex(*s, n) : -1;
			if(index < 0)
				return FALSE;
			const Parts parts = PartsOf(hwnd, *s, index);
			*rc = wp ? ToRECT(parts.label) : RECT{0, parts.row.top(), static_cast<LONG>(Client(hwnd).right), parts.row.top() + s->itemHeight};
			return TRUE;
		}
		return FALSE;
	case TVM_GETCOUNT:
		return s ? static_cast<LRESULT>(s->alive.size()) : 0;
	case TVM_GETINDENT:
		return s ? s->indent : 0;
	case TVM_SETINDENT:
		if(s != nullptr)
		{
			s->indent = std::max(MinimumIndent, static_cast<int>(wp));
			s->indentSet = true;
			Relayout(hwnd, *s);
		}
		return 0;
	case TVM_GETIMAGELIST:
		if(s == nullptr)
			return 0;
		return reinterpret_cast<LRESULT>(wp == TVSIL_STATE ? s->stateList : (wp == TVSIL_NORMAL ? s->normal : nullptr));
	case TVM_SETIMAGELIST:
		if(s != nullptr && (wp == TVSIL_NORMAL || wp == TVSIL_STATE))
		{
			HIMAGELIST &slot = wp == TVSIL_STATE ? s->stateList : s->normal;
			HIMAGELIST old = slot;
			slot = reinterpret_cast<HIMAGELIST>(lp);
			Measure(hwnd, *s);
			Relayout(hwnd, *s);
			return reinterpret_cast<LRESULT>(old);
		}
		return 0;
	case TVM_GETNEXTITEM:
		if(s == nullptr)
			return 0;
		{
			Node *n = Item(*s, reinterpret_cast<HTREEITEM>(lp));
			if(n == nullptr && lp != 0 && reinterpret_cast<HTREEITEM>(lp) != TVI_ROOT)
			{
				// A stale handle: only the codes that ignore it answer.
				const UINT code = static_cast<UINT>(wp);
				if(code != TVGN_ROOT && code != TVGN_CARET && code != TVGN_DROPHILITE && code != TVGN_FIRSTVISIBLE && code != TVGN_LASTVISIBLE)
					return 0;
			}
			return reinterpret_cast<LRESULT>(Handle(GetNextItem(hwnd, *s, n, static_cast<UINT>(wp))));
		}
	case TVM_SELECTITEM:
		if(s != nullptr)
		{
			Node *n = Item(*s, reinterpret_cast<HTREEITEM>(lp));
			if(n == nullptr && lp != 0)
				return FALSE;
			switch(wp & 0xFF)
			{
			case TVGN_CARET:
				return SelectItem(hwnd, n, TVC_UNKNOWN);
			case TVGN_DROPHILITE:
				s->dropHilite = n;
				::InvalidateRect(hwnd, nullptr, TRUE);
				return TRUE;
			case TVGN_FIRSTVISIBLE:
				if(n != nullptr)
				{
					EnsureVisible(hwnd, *s, n);
					if((s = Control<TreeState>(hwnd)) != nullptr)
						ScrollTo(hwnd, *s, VisibleIndex(*s, n), s->scrollX);
				}
				return TRUE;
			default:
				return FALSE;
			}
		}
		return FALSE;
	case TVM_GETEDITCONTROL:
		return s ? reinterpret_cast<LRESULT>(s->edit) : 0;
	case TVM_GETVISIBLECOUNT:
		return s ? RowsInView(hwnd, *s) : 0;
	case TVM_HITTEST:
		if(s != nullptr && lp != 0)
		{
			auto *info = reinterpret_cast<TVHITTESTINFO *>(lp);
			info->hItem = Handle(HitTest(hwnd, *s, info->pt, &info->flags));
			return reinterpret_cast<LRESULT>(info->hItem);
		}
		return 0;
	case TVM_CREATEDRAGIMAGE:
		if(s != nullptr)
		{
			// The item's image and label in an image list of one.
			Node *n = Item(*s, reinterpret_cast<HTREEITEM>(lp));
			const int index = n != nullptr ? VisibleIndex(*s, n) : -1;
			if(index < 0)
				return 0;
			const Parts parts = PartsOf(hwnd, *s, index);
			const int left = s->normal != nullptr ? parts.icon.left() : parts.label.left();
			const int width = parts.label.right() + 1 - left;
			HIMAGELIST list = ImageList_Create(width, s->itemHeight, ILC_COLOR32, 1, 0);
			HDC screen = ::GetDC(hwnd);
			HDC mem = ::CreateCompatibleDC(screen);
			HBITMAP bmp = ::CreateCompatibleBitmap(screen, width, s->itemHeight);
			HGDIOBJ oldBmp = ::SelectObject(mem, bmp);
			FillColor(mem, QRect(0, 0, width, s->itemHeight), Resolve(s->bk, COLOR_WINDOW));
			if(s->normal != nullptr)
				ImageList_Draw(s->normal, n->image, mem, 0, (s->itemHeight - ImageHeight(s->normal)) / 2, ILD_TRANSPARENT);
			HGDIOBJ oldFont = ::SelectObject(mem, (n->state & TVIS_BOLD) ? BoldFont(hwnd, *s) : FontOfControl(hwnd));
			const std::wstring text = TextOf(hwnd, n);
			RECT tr{parts.label.left() - left + LabelPadding, 0, width, s->itemHeight};
			::SetTextColor(mem, Resolve(s->text, COLOR_WINDOWTEXT));
			::SetBkMode(mem, TRANSPARENT);
			::DrawText(mem, text.c_str(), static_cast<int>(text.size()), &tr, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_LEFT);
			::SelectObject(mem, oldFont);
			::SelectObject(mem, oldBmp);
			ImageList_Add(list, bmp, nullptr);
			::DeleteObject(bmp);
			::DeleteDC(mem);
			::ReleaseDC(hwnd, screen);
			return reinterpret_cast<LRESULT>(list);
		}
		return 0;
	case TVM_SORTCHILDREN:
		if(s != nullptr)
		{
			Node *parent = (lp == 0 || reinterpret_cast<HTREEITEM>(lp) == TVI_ROOT) ? &s->root : Item(*s, reinterpret_cast<HTREEITEM>(lp));
			if(parent == nullptr)
				return FALSE;
			SortChildren(parent, wp != 0, [hwnd](const Node *a, const Node *b) { return CompareNodes(hwnd, a, b) < 0; });
			if((s = Control<TreeState>(hwnd)) != nullptr)
				Relayout(hwnd, *s);
			return TRUE;
		}
		return FALSE;
	case TVM_SORTCHILDRENCB:
		if(s != nullptr && lp != 0)
		{
			const auto *sort = reinterpret_cast<const TVSORTCB *>(lp);
			Node *parent = (sort->hParent == nullptr || sort->hParent == TVI_ROOT) ? &s->root : Item(*s, sort->hParent);
			if(parent == nullptr || sort->lpfnCompare == nullptr)
				return FALSE;
			SortChildren(parent, wp != 0, [sort](const Node *a, const Node *b) { return sort->lpfnCompare(a->param, b->param, sort->lParam) < 0; });
			if((s = Control<TreeState>(hwnd)) != nullptr)
				Relayout(hwnd, *s);
			return TRUE;
		}
		return FALSE;
	case TVM_ENSUREVISIBLE:
		if(s != nullptr)
		{
			Node *n = Item(*s, reinterpret_cast<HTREEITEM>(lp));
			if(n == nullptr)
				return FALSE;
			const int before = s->top;
			EnsureVisible(hwnd, *s, n);
			s = Control<TreeState>(hwnd);
			return s != nullptr && s->top == before;
		}
		return FALSE;
	case TVM_ENDEDITLABELNOW:
		EndEdit(hwnd, wp == FALSE);
		return TRUE;
	case TVM_GETITEM:
		return s ? GetItem(hwnd, *s, reinterpret_cast<TVITEMEX *>(lp)) : FALSE;
	case TVM_SETITEM:
		return s ? SetItem(hwnd, *s, reinterpret_cast<const TVITEMEX *>(lp)) : FALSE;
	case TVM_EDITLABEL:
		return s ? reinterpret_cast<LRESULT>(BeginEdit(hwnd, Item(*s, reinterpret_cast<HTREEITEM>(lp)))) : 0;
	case TVM_SETITEMHEIGHT:
		if(s != nullptr)
		{
			const int old = s->itemHeight;
			if(static_cast<int>(wp) == -1)
			{
				s->heightSet = false;
			} else
			{
				int h = std::max(1, static_cast<int>(wp));
				if(!(StyleOf(hwnd) & TVS_NONEVENHEIGHT))
					h = std::max(2, h & ~1);
				s->itemHeight = h;
				s->heightSet = true;
			}
			Measure(hwnd, *s);
			Relayout(hwnd, *s);
			return old;
		}
		return 0;
	case TVM_GETITEMHEIGHT:
		return s ? s->itemHeight : 0;
	case TVM_SETBKCOLOR:
	case TVM_SETTEXTCOLOR:
	case TVM_SETLINECOLOR:
	case TVM_SETINSERTMARKCOLOR:
		if(s != nullptr)
		{
			COLORREF &slot = msg == TVM_SETBKCOLOR ? s->bk : (msg == TVM_SETTEXTCOLOR ? s->text : (msg == TVM_SETLINECOLOR ? s->line : s->insertColor));
			const COLORREF old = slot;
			slot = static_cast<COLORREF>(lp);
			::InvalidateRect(hwnd, nullptr, TRUE);
			return static_cast<LRESULT>(msg == TVM_SETLINECOLOR && old == CLR_DEFAULT ? CLR_DEFAULT : (old == CLR_NONE ? static_cast<COLORREF>(-1) : old));
		}
		return 0;
	case TVM_GETBKCOLOR:
		return s ? static_cast<LRESULT>(s->bk == CLR_NONE ? static_cast<COLORREF>(-1) : s->bk) : 0;
	case TVM_GETTEXTCOLOR:
		return s ? static_cast<LRESULT>(s->text == CLR_NONE ? static_cast<COLORREF>(-1) : s->text) : 0;
	case TVM_GETLINECOLOR:
		return s ? static_cast<LRESULT>(s->line) : 0;
	case TVM_GETINSERTMARKCOLOR:
		return s ? static_cast<LRESULT>(s->insertColor) : 0;
	case TVM_SETINSERTMARK:
		if(s != nullptr)
		{
			Node *n = Item(*s, reinterpret_cast<HTREEITEM>(lp));
			if(n == nullptr && lp != 0)
				return FALSE;
			s->insertMark = n;
			s->insertAfter = wp != 0;
			::InvalidateRect(hwnd, nullptr, TRUE);
			return TRUE;
		}
		return FALSE;
	case TVM_GETITEMSTATE:
		if(s != nullptr)
		{
			Node *n = Item(*s, reinterpret_cast<HTREEITEM>(wp));
			return n != nullptr ? static_cast<LRESULT>(n->state & static_cast<UINT>(lp)) : 0;
		}
		return 0;
	case TVM_SETEXTENDEDSTYLE:
		if(s != nullptr)
		{
			const DWORD old = s->exStyle;
			const DWORD mask = wp != 0 ? static_cast<DWORD>(wp) : 0xFFFFFFFFu;
			s->exStyle = (s->exStyle & ~mask) | (static_cast<DWORD>(lp) & mask);
			::InvalidateRect(hwnd, nullptr, TRUE);
			return static_cast<LRESULT>(old);
		}
		return 0;
	case TVM_GETEXTENDEDSTYLE:
		return s ? static_cast<LRESULT>(s->exStyle) : 0;
	case TVM_GETTOOLTIPS:
	case TVM_SETTOOLTIPS:
		return 0;
	case TVM_SETSCROLLTIME:
		if(s != nullptr)
		{
			const UINT old = s->scrollTime;
			s->scrollTime = static_cast<UINT>(wp);
			return old;
		}
		return 0;
	case TVM_GETSCROLLTIME:
		return s ? s->scrollTime : 0;
	case TVM_GETISEARCHSTRING:
		if(s != nullptr)
		{
			if(lp != 0)
			{
				auto *out = reinterpret_cast<LPWSTR>(lp);
				std::copy(s->search.begin(), s->search.end(), out);
				out[s->search.size()] = 0;
			}
			return static_cast<LRESULT>(s->search.size());
		}
		return 0;
	default:
		return ::DefWindowProc(hwnd, msg, wp, lp);
	}
}

} // namespace

void RegisterTreeViewClass()
{
	WNDCLASS wc{};
	wc.style = CS_DBLCLKS;
	wc.lpfnWndProc = TreeProc;
	wc.hCursor = ::LoadCursor(nullptr, IDC_ARROW);
	wc.lpszClassName = WC_TREEVIEW;
	::RegisterClass(&wc);
}

} // namespace linopenmpt::qtwin
