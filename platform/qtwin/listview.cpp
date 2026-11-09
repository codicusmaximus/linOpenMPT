// The list view of comctl32 (SysListView32) in report view, the view
// upstream uses: columns in a header control, items with subitems, images,
// state and check boxes, single and multiple selection, groups, virtual
// lists (LVS_OWNERDATA), sorting, label editing, custom draw and comctl32's
// notifications. The other views show the items in one column.
#include "QtWin.h"

#include <QtCore/QElapsedTimer>
#include <QtWidgets/QApplication>
#include <QtWidgets/QStyle>
#include <QtWidgets/QStyleOption>

#include <algorithm>
#include <cstring>
#include <numeric>
#include <string>
#include <vector>

namespace linopenmpt::qtwin
{

namespace
{

constexpr int I_GroupIdCallback = -1;
constexpr int I_GroupIdNone = -2;
constexpr int LabelPadding = 2;    // around the item's own label
constexpr int SubItemPadding = 6;  // around subitem texts
constexpr int StateIconSize = 16;  // check boxes
constexpr UINT_PTR EditTimer = 42;

struct LvItem
{
	std::vector<std::wstring> text; // by subitem
	std::vector<bool> callback;     // LPSTR_TEXTCALLBACK, by subitem
	int image = 0;
	LPARAM param = 0;
	UINT state = 0;
	int indent = 0;
	int groupId = I_GroupIdNone;
};

struct LvColumn
{
	int fmt = LVCFMT_LEFT;
	int width = 0;
	std::wstring text;
	int subItem = 0;
	int image = -1;
};

struct LvGroup
{
	int id = 0;
	std::wstring header;
	UINT state = 0;
	UINT align = LVGA_HEADER_LEFT;
};

// A line of the report: an item, or a group's heading.
struct Row
{
	bool group = false;
	int index = 0; // item, or index into groups
	int top = 0;   // in content coordinates
	int height = 0;
};

struct ListState : ControlState
{
	std::vector<LvItem> items;   // not with LVS_OWNERDATA
	int ownerCount = 0;           // with LVS_OWNERDATA
	std::vector<UINT> ownerState; // with LVS_OWNERDATA: selection and focus
	std::vector<LvColumn> columns;
	std::vector<LvGroup> groups;
	bool groupView = false;
	DWORD exStyle = 0;
	HIMAGELIST images[3]{};
	HWND header = nullptr;
	int headerHeight = 0;
	int rowHeight = 16;
	int groupHeight = 24;
	std::vector<Row> rows;
	int contentHeight = 0;
	int scrollX = 0;
	int scrollY = 0;
	int focus = -1;
	int mark = -1;
	int hot = -1;
	bool focused = false;
	COLORREF bk = CLR_DEFAULT;
	COLORREF text = CLR_DEFAULT;
	COLORREF textBk = CLR_DEFAULT;
	UINT callbackMask = 0;
	HWND edit = nullptr;
	int editItem = -1;
	int pendingEdit = -1;
	std::wstring search;
	QElapsedTimer searchTime;
	bool updatingScroll = false;
	bool dragArmed = false;
	POINT dragFrom{};
	int dragItem = -1;
	bool layoutPending = false;
};

LONG_PTR StyleOf(HWND hwnd)
{
	return ::GetWindowLongPtr(hwnd, GWL_STYLE);
}

bool OwnerData(HWND hwnd)
{
	return (StyleOf(hwnd) & LVS_OWNERDATA) != 0;
}

bool Report(HWND hwnd)
{
	return (StyleOf(hwnd) & LVS_TYPEMASK) == LVS_REPORT;
}

int Count(HWND hwnd, const ListState &s)
{
	return OwnerData(hwnd) ? s.ownerCount : static_cast<int>(s.items.size());
}

bool Valid(HWND hwnd, const ListState &s, int item)
{
	return item >= 0 && item < Count(hwnd, s);
}

HFONT FontOfControl(HWND hwnd)
{
	const WindowData *d = Data(hwnd);
	return d != nullptr && d->font != nullptr ? d->font : static_cast<HFONT>(::GetStockObject(DEFAULT_GUI_FONT));
}

QRect ToRect(const RECT &r)
{
	return QRect(QPoint(r.left, r.top), QPoint(r.right - 1, r.bottom - 1));
}

RECT ToRECT(const QRect &r)
{
	return RECT{r.left(), r.top(), r.left() + r.width(), r.top() + r.height()};
}

RECT Client(HWND hwnd)
{
	RECT rc{};
	::GetClientRect(hwnd, &rc);
	return rc;
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

UINT ItemState(HWND hwnd, const ListState &s, int item)
{
	if(!Valid(hwnd, s, item))
		return 0;
	return OwnerData(hwnd) ? s.ownerState[static_cast<std::size_t>(item)] : s.items[static_cast<std::size_t>(item)].state;
}

UINT &ItemStateRef(HWND hwnd, ListState &s, int item)
{
	return OwnerData(hwnd) ? s.ownerState[static_cast<std::size_t>(item)] : s.items[static_cast<std::size_t>(item)].state;
}

LPARAM ItemParam(HWND hwnd, const ListState &s, int item)
{
	if(OwnerData(hwnd) || !Valid(hwnd, s, item))
		return 0;
	return s.items[static_cast<std::size_t>(item)].param;
}

// The text of a subitem, asking the parent for callback texts and virtual items.
std::wstring ItemText(HWND hwnd, int item, int sub)
{
	ListState *s = Control<ListState>(hwnd);
	if(s == nullptr || !Valid(hwnd, *s, item) || sub < 0)
		return std::wstring();
	if(!OwnerData(hwnd))
	{
		const LvItem &it = s->items[static_cast<std::size_t>(item)];
		const bool callback = static_cast<std::size_t>(sub) < it.callback.size() && it.callback[static_cast<std::size_t>(sub)];
		if(!callback)
			return static_cast<std::size_t>(sub) < it.text.size() ? it.text[static_cast<std::size_t>(sub)] : std::wstring();
	}
	WCHAR buffer[1024] = {};
	NMLVDISPINFO di{};
	di.item.mask = LVIF_TEXT;
	di.item.iItem = item;
	di.item.iSubItem = sub;
	di.item.pszText = buffer;
	di.item.cchTextMax = 1024;
	di.item.lParam = ItemParam(hwnd, *s, item);
	Notify(hwnd, di.hdr, LVN_GETDISPINFO);
	if(di.item.pszText == nullptr || di.item.pszText == LPSTR_TEXTCALLBACKW)
		return std::wstring();
	return std::wstring(di.item.pszText);
}

int ItemImage(HWND hwnd, int item)
{
	ListState *s = Control<ListState>(hwnd);
	if(s == nullptr || !Valid(hwnd, *s, item))
		return -1;
	if(!OwnerData(hwnd) && s->items[static_cast<std::size_t>(item)].image != I_IMAGECALLBACK)
		return s->items[static_cast<std::size_t>(item)].image;
	NMLVDISPINFO di{};
	di.item.mask = LVIF_IMAGE;
	di.item.iItem = item;
	di.item.lParam = ItemParam(hwnd, *s, item);
	Notify(hwnd, di.hdr, LVN_GETDISPINFO);
	return di.item.iImage;
}

// ---- layout ---------------------------------------------------------------------------

int ColumnLeft(const ListState &s, int col)
{
	int x = 0;
	for(int i = 0; i < col && i < static_cast<int>(s.columns.size()); i++)
		x += s.columns[static_cast<std::size_t>(i)].width;
	return x;
}

int TotalWidth(HWND hwnd, const ListState &s)
{
	if(!Report(hwnd) || s.columns.empty())
		return std::max(0, static_cast<int>(Client(hwnd).right));
	int w = 0;
	for(const LvColumn &c : s.columns)
		w += c.width;
	return w;
}

int ContentTop(const ListState &s)
{
	return s.headerHeight;
}

void Measure(HWND hwnd, ListState &s)
{
	HDC dc = ::GetDC(hwnd);
	HGDIOBJ old = ::SelectObject(dc, FontOfControl(hwnd));
	TEXTMETRIC tm{};
	::GetTextMetrics(dc, &tm);
	::SelectObject(dc, old);
	::ReleaseDC(hwnd, dc);
	int iconHeight = 0;
	int cx = 0, cy = 0;
	if(s.images[LVSIL_SMALL] != nullptr && ImageList_GetIconSize(s.images[LVSIL_SMALL], &cx, &cy))
		iconHeight = cy;
	if(s.exStyle & LVS_EX_CHECKBOXES)
		iconHeight = std::max(iconHeight, StateIconSize);
	s.rowHeight = std::max(static_cast<int>(tm.tmHeight), iconHeight) + 1 + ((s.exStyle & LVS_EX_GRIDLINES) ? 1 : 0);
	s.groupHeight = static_cast<int>(tm.tmHeight) + 12;
	if(StyleOf(hwnd) & LVS_OWNERDRAWFIXED)
	{
		MEASUREITEMSTRUCT mis{};
		mis.CtlType = ODT_LISTVIEW;
		mis.CtlID = static_cast<UINT>(::GetDlgCtrlID(hwnd));
		mis.itemHeight = static_cast<UINT>(s.rowHeight);
		::SendMessage(::GetParent(hwnd), WM_MEASUREITEM, mis.CtlID, reinterpret_cast<LPARAM>(&mis));
		s.rowHeight = std::max(1, static_cast<int>(mis.itemHeight));
	}
}

int GroupIndex(const ListState &s, int id)
{
	for(std::size_t i = 0; i < s.groups.size(); i++)
	{
		if(s.groups[i].id == id)
			return static_cast<int>(i);
	}
	return -1;
}

void BuildRows(HWND hwnd, ListState &s)
{
	s.rows.clear();
	int y = 0;
	const int count = Count(hwnd, s);
	if(s.groupView && !OwnerData(hwnd))
	{
		for(std::size_t g = 0; g < s.groups.size(); g++)
		{
			const LvGroup &group = s.groups[g];
			if(group.state & LVGS_HIDDEN)
				continue;
			std::vector<int> members;
			for(int i = 0; i < count; i++)
			{
				if(s.items[static_cast<std::size_t>(i)].groupId == group.id)
					members.push_back(i);
			}
			if(members.empty())
				continue;
			if(!(group.state & LVGS_NOHEADER))
			{
				s.rows.push_back(Row{true, static_cast<int>(g), y, s.groupHeight});
				y += s.groupHeight;
			}
			if(group.state & LVGS_COLLAPSED)
				continue;
			for(int i : members)
			{
				s.rows.push_back(Row{false, i, y, s.rowHeight});
				y += s.rowHeight;
			}
		}
	} else
	{
		s.rows.reserve(static_cast<std::size_t>(count));
		for(int i = 0; i < count; i++)
		{
			s.rows.push_back(Row{false, i, y, s.rowHeight});
			y += s.rowHeight;
		}
	}
	s.contentHeight = y;
}

int RowOfItem(const ListState &s, int item)
{
	for(std::size_t r = 0; r < s.rows.size(); r++)
	{
		if(!s.rows[r].group && s.rows[r].index == item)
			return static_cast<int>(r);
	}
	return -1;
}

int ViewHeight(HWND hwnd, const ListState &s)
{
	return std::max(0, static_cast<int>(Client(hwnd).bottom) - ContentTop(s));
}

void PlaceHeader(HWND hwnd, ListState &s)
{
	if(s.header == nullptr)
	{
		s.headerHeight = 0;
		return;
	}
	RECT rc = Client(hwnd);
	WINDOWPOS wp{};
	HDLAYOUT layout{&rc, &wp};
	::SendMessage(s.header, HDM_LAYOUT, 0, reinterpret_cast<LPARAM>(&layout));
	const bool shown = Report(hwnd) && !(StyleOf(hwnd) & LVS_NOCOLUMNHEADER);
	s.headerHeight = shown ? wp.cy : 0;
	const int width = std::max(static_cast<int>(Client(hwnd).right) + s.scrollX, TotalWidth(hwnd, s));
	::SetWindowPos(s.header, nullptr, -s.scrollX, 0, width, wp.cy, SWP_NOZORDER | SWP_NOACTIVATE | (shown ? SWP_SHOWWINDOW : SWP_HIDEWINDOW));
}

void UpdateScrollBars(HWND hwnd, ListState &s)
{
	if(s.updatingScroll || (StyleOf(hwnd) & LVS_NOSCROLL))
		return;
	s.updatingScroll = true;
	for(int pass = 0; pass < 2; pass++)
	{
		const int view = ViewHeight(hwnd, s);
		const int maxY = std::max(0, s.contentHeight - view);
		s.scrollY = std::clamp(s.scrollY, 0, maxY);
		SCROLLINFO si{};
		si.cbSize = sizeof(si);
		si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
		si.nMin = 0;
		si.nMax = std::max(0, s.contentHeight - 1);
		si.nPage = static_cast<UINT>(std::max(0, view));
		si.nPos = s.scrollY;
		::SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
		const int width = static_cast<int>(Client(hwnd).right);
		const int total = Report(hwnd) ? TotalWidth(hwnd, s) : 0;
		s.scrollX = std::clamp(s.scrollX, 0, std::max(0, total - width));
		si.nMax = std::max(0, total - 1);
		si.nPage = static_cast<UINT>(std::max(0, width));
		si.nPos = s.scrollX;
		::SetScrollInfo(hwnd, SB_HORZ, &si, TRUE);
		PlaceHeader(hwnd, s);
	}
	s.updatingScroll = false;
}

void Relayout(HWND hwnd, ListState &s)
{
	BuildRows(hwnd, s);
	PlaceHeader(hwnd, s);
	UpdateScrollBars(hwnd, s);
	::InvalidateRect(hwnd, nullptr, TRUE);
}

// The parts of an item's row, in client coordinates.
struct ItemParts
{
	QRect row;    // the whole row
	QRect bounds; // LVIR_BOUNDS
	QRect state;  // check box
	QRect icon;   // small icon
	QRect label;  // the item's text (column 0)
	QRect select; // LVIR_SELECTBOUNDS
};

int SmallIconWidth(const ListState &s)
{
	int cx = 0, cy = 0;
	if(s.images[LVSIL_SMALL] != nullptr && ImageList_GetIconSize(s.images[LVSIL_SMALL], &cx, &cy))
		return cx;
	return 0;
}

bool HasStateIcons(const ListState &s)
{
	return (s.exStyle & LVS_EX_CHECKBOXES) || s.images[LVSIL_STATE] != nullptr;
}

ItemParts PartsOf(HWND hwnd, const ListState &s, int rowIndex)
{
	ItemParts p;
	if(rowIndex < 0 || rowIndex >= static_cast<int>(s.rows.size()))
		return p;
	const Row &row = s.rows[static_cast<std::size_t>(rowIndex)];
	const int top = ContentTop(s) + row.top - s.scrollY;
	const int x0 = -s.scrollX;
	const int total = TotalWidth(hwnd, s);
	p.row = QRect(x0, top, std::max(total, 1), row.height);
	if(row.group)
	{
		p.bounds = p.label = p.select = QRect(x0, top, std::max(total, static_cast<int>(Client(hwnd).right)), row.height);
		return p;
	}
	const int width0 = Report(hwnd) && !s.columns.empty() ? s.columns[0].width : total;
	int x = x0 + (OwnerData(hwnd) ? 0 : s.items[static_cast<std::size_t>(row.index)].indent * std::max(SmallIconWidth(s), 16));
	if(HasStateIcons(s))
	{
		p.state = QRect(x, top, StateIconSize, row.height);
		x += StateIconSize;
	}
	const int iconWidth = SmallIconWidth(s);
	p.icon = QRect(x, top, iconWidth, row.height);
	x += iconWidth;
	p.label = QRect(x, top, std::max(0, x0 + width0 - x), row.height);
	p.bounds = p.row;
	if(s.exStyle & LVS_EX_FULLROWSELECT)
		p.select = QRect(p.icon.left(), top, std::max(0, x0 + total - p.icon.left()), row.height);
	else
		p.select = p.icon.united(p.label);
	return p;
}

int RowAt(const ListState &s, int y)
{
	const int cy = y - ContentTop(s) + s.scrollY;
	if(y < ContentTop(s) || s.rows.empty())
		return -1;
	auto it = std::upper_bound(s.rows.begin(), s.rows.end(), cy, [](int v, const Row &r) { return v < r.top; });
	if(it == s.rows.begin())
		return -1;
	--it;
	if(cy >= it->top + it->height)
		return -1;
	return static_cast<int>(it - s.rows.begin());
}

int SubItemAt(HWND hwnd, const ListState &s, int x)
{
	if(!Report(hwnd))
		return 0;
	int left = -s.scrollX;
	for(std::size_t c = 0; c < s.columns.size(); c++)
	{
		if(x >= left && x < left + s.columns[c].width)
			return static_cast<int>(c);
		left += s.columns[c].width;
	}
	return -1;
}

int HitTest(HWND hwnd, ListState &s, POINT pt, UINT *flags, int *subItem = nullptr, int *group = nullptr)
{
	RECT rc = Client(hwnd);
	UINT f = 0;
	if(pt.y < ContentTop(s))
		f |= LVHT_ABOVE;
	if(pt.y >= rc.bottom)
		f |= LVHT_BELOW;
	if(pt.x < 0)
		f |= LVHT_TOLEFT;
	if(pt.x >= rc.right)
		f |= LVHT_TORIGHT;
	int item = -1;
	if(subItem != nullptr)
		*subItem = -1;
	if(group != nullptr)
		*group = -1;
	if(f == 0)
	{
		const int r = RowAt(s, pt.y);
		if(r < 0)
		{
			f = LVHT_NOWHERE;
		} else if(s.rows[static_cast<std::size_t>(r)].group)
		{
			f = LVHT_EX_GROUP_HEADER | LVHT_NOWHERE;
			if(group != nullptr)
				*group = s.groups[static_cast<std::size_t>(s.rows[static_cast<std::size_t>(r)].index)].id;
		} else
		{
			const ItemParts parts = PartsOf(hwnd, s, r);
			const int candidate = s.rows[static_cast<std::size_t>(r)].index;
			const QPoint p(pt.x, pt.y);
			if(subItem != nullptr)
				*subItem = SubItemAt(hwnd, s, pt.x);
			if(parts.state.contains(p))
				f = LVHT_ONITEMSTATEICON;
			else if(parts.icon.contains(p) && parts.icon.width() > 0)
				f = LVHT_ONITEMICON;
			else if(parts.label.contains(p) || ((s.exStyle & LVS_EX_FULLROWSELECT) && parts.select.contains(p)))
				f = LVHT_ONITEMLABEL;
			else
				f = LVHT_NOWHERE;
			if(f != LVHT_NOWHERE || (subItem != nullptr && *subItem > 0))
				item = candidate;
		}
	}
	if(flags != nullptr)
		*flags = f;
	return (f & LVHT_ONITEM) ? item : (subItem != nullptr ? item : -1);
}

// ---- state changes ------------------------------------------------------------------------

// Changes an item's state as comctl32 does: LVN_ITEMCHANGING (which may
// refuse it), then LVN_ITEMCHANGED.
bool ChangeState(HWND hwnd, int item, UINT state, UINT mask)
{
	ListState *s = Control<ListState>(hwnd);
	if(s == nullptr || !Valid(hwnd, *s, item))
		return false;
	if((s->exStyle & LVS_EX_CHECKBOXES) == 0 && (mask & LVIS_STATEIMAGEMASK) && s->images[LVSIL_STATE] == nullptr)
		mask &= ~LVIS_STATEIMAGEMASK;
	const UINT old = ItemState(hwnd, *s, item);
	UINT next = (old & ~mask) | (state & mask);
	if((StyleOf(hwnd) & LVS_SINGLESEL) && (next & LVIS_SELECTED) && !(old & LVIS_SELECTED))
	{
		// One selected item only: the others go first.
		for(int i = 0; i < Count(hwnd, *s); i++)
		{
			if(i != item && (ItemState(hwnd, *s, i) & LVIS_SELECTED))
			{
				ChangeState(hwnd, i, 0, LVIS_SELECTED);
				if((s = Control<ListState>(hwnd)) == nullptr)
					return false;
			}
		}
	}
	if(next == old)
		return true;
	NMLISTVIEW nm{};
	nm.iItem = item;
	nm.uChanged = LVIF_STATE;
	nm.uNewState = next;
	nm.uOldState = old;
	nm.lParam = ItemParam(hwnd, *s, item);
	if(!OwnerData(hwnd) && Notify(hwnd, nm.hdr, LVN_ITEMCHANGING) != 0)
		return false;
	if((s = Control<ListState>(hwnd)) == nullptr || !Valid(hwnd, *s, item))
		return false;
	if((next & LVIS_FOCUSED) && !(old & LVIS_FOCUSED))
	{
		if(Valid(hwnd, *s, s->focus) && s->focus != item)
		{
			const int previous = s->focus;
			s->focus = item;
			ChangeState(hwnd, previous, 0, LVIS_FOCUSED);
			if((s = Control<ListState>(hwnd)) == nullptr || !Valid(hwnd, *s, item))
				return false;
		}
		s->focus = item;
	} else if(!(next & LVIS_FOCUSED) && (old & LVIS_FOCUSED) && s->focus == item)
	{
		s->focus = -1;
	}
	ItemStateRef(hwnd, *s, item) = next;
	const int r = RowOfItem(*s, item);
	if(r >= 0)
	{
		const RECT rr = ToRECT(PartsOf(hwnd, *s, r).row.adjusted(0, 0, static_cast<int>(Client(hwnd).right), 0));
		::InvalidateRect(hwnd, &rr, TRUE);
	}
	Notify(hwnd, nm.hdr, LVN_ITEMCHANGED);
	return true;
}

void SelectOnly(HWND hwnd, int item)
{
	ListState *s = Control<ListState>(hwnd);
	if(s == nullptr)
		return;
	for(int i = 0; i < Count(hwnd, *s); i++)
	{
		if(i != item && (ItemState(hwnd, *s, i) & LVIS_SELECTED))
		{
			ChangeState(hwnd, i, 0, LVIS_SELECTED);
			if((s = Control<ListState>(hwnd)) == nullptr)
				return;
		}
	}
	if(item >= 0)
		ChangeState(hwnd, item, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
}

void SelectRange(HWND hwnd, int from, int to, bool keepOthers)
{
	ListState *s = Control<ListState>(hwnd);
	if(s == nullptr)
		return;
	// The range follows the display order (groups).
	const int ra = RowOfItem(*s, from), rb = RowOfItem(*s, to);
	std::vector<int> range;
	if(ra >= 0 && rb >= 0)
	{
		for(int r = std::min(ra, rb); r <= std::max(ra, rb); r++)
		{
			if(!s->rows[static_cast<std::size_t>(r)].group)
				range.push_back(s->rows[static_cast<std::size_t>(r)].index);
		}
	}
	for(int i = 0; i < Count(hwnd, *s); i++)
	{
		const bool in = std::find(range.begin(), range.end(), i) != range.end();
		if(in)
			ChangeState(hwnd, i, LVIS_SELECTED, LVIS_SELECTED);
		else if(!keepOthers)
			ChangeState(hwnd, i, 0, LVIS_SELECTED);
		if((s = Control<ListState>(hwnd)) == nullptr)
			return;
	}
	ChangeState(hwnd, to, LVIS_FOCUSED, LVIS_FOCUSED);
}

void EnsureVisible(HWND hwnd, ListState &s, int item, bool partialOK)
{
	const int r = RowOfItem(s, item);
	if(r < 0)
		return;
	const Row &row = s.rows[static_cast<std::size_t>(r)];
	const int view = ViewHeight(hwnd, s);
	int y = s.scrollY;
	if(row.top < y)
		y = row.top;
	else if(row.top + row.height > y + view && !(partialOK && row.top < y + view))
		y = row.top + row.height - view;
	// Whole rows, as comctl32 scrolls a report.
	if(!s.groupView && s.rowHeight > 0)
		y = (y + s.rowHeight - 1) / s.rowHeight * s.rowHeight;
	y = std::clamp(y, 0, std::max(0, s.contentHeight - view));
	if(y != s.scrollY)
	{
		s.scrollY = y;
		UpdateScrollBars(hwnd, s);
		::InvalidateRect(hwnd, nullptr, TRUE);
	}
}

// ---- drawing --------------------------------------------------------------------------------

COLORREF Resolve(COLORREF c, int sys)
{
	return (c == CLR_DEFAULT || c == CLR_NONE) ? ::GetSysColor(sys) : c;
}

void DrawCheckBox(HWND hwnd, HDC dc, const QRect &r, bool checked)
{
	Win32Widget *w = FromHandle(hwnd);
	QPainter *p = DCPainter(dc);
	if(w == nullptr || p == nullptr)
		return;
	QStyle *qs = QApplication::style();
	QStyleOptionButton opt;
	opt.initFrom(w);
	const int size = std::min({qs->pixelMetric(QStyle::PM_IndicatorWidth, nullptr, w), r.width(), r.height()});
	opt.rect = QRect(r.left() + (r.width() - size) / 2, r.top() + (r.height() - size) / 2, size, size);
	opt.state &= ~(QStyle::State_HasFocus | QStyle::State_MouseOver);
	opt.state |= checked ? QStyle::State_On : QStyle::State_Off;
	if(!::IsWindowEnabled(hwnd))
		opt.state &= ~QStyle::State_Enabled;
	p->save();
	qs->drawPrimitive(QStyle::PE_IndicatorCheckBox, &opt, p, w);
	p->restore();
}

void DrawText(HDC dc, const std::wstring &text, QRect r, int fmt, COLORREF color)
{
	if(text.empty() || r.width() <= 0)
		return;
	RECT rc = ToRECT(r);
	UINT flags = DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS;
	switch(fmt & LVCFMT_JUSTIFYMASK)
	{
	case LVCFMT_RIGHT: flags |= DT_RIGHT; break;
	case LVCFMT_CENTER: flags |= DT_CENTER; break;
	default: flags |= DT_LEFT; break;
	}
	::SetTextColor(dc, color);
	::SetBkMode(dc, TRANSPARENT);
	::DrawText(dc, text.c_str(), -1, &rc, flags);
}

void FillColor(HDC dc, const QRect &r, COLORREF color)
{
	if(r.width() <= 0 || r.height() <= 0)
		return;
	HBRUSH b = ::CreateSolidBrush(color);
	const RECT rc = ToRECT(r);
	::FillRect(dc, &rc, b);
	::DeleteObject(b);
}

void PaintGroup(HWND hwnd, HDC dc, const ListState &s, int rowIndex)
{
	const Row &row = s.rows[static_cast<std::size_t>(rowIndex)];
	const LvGroup &group = s.groups[static_cast<std::size_t>(row.index)];
	const ItemParts parts = PartsOf(hwnd, s, rowIndex);
	const QRect r = parts.row.adjusted(0, 0, std::max(0, static_cast<int>(Client(hwnd).right) - parts.row.right() - 1), 0);
	QRect text = r.adjusted(10, 4, -10, -2);
	if(group.state & LVGS_COLLAPSIBLE)
	{
		// The arrow that collapses the group, on the right.
		Win32Widget *w = FromHandle(hwnd);
		QPainter *p = DCPainter(dc);
		if(w != nullptr && p != nullptr)
		{
			QStyleOption opt;
			opt.initFrom(w);
			const int size = std::min(12, r.height() - 6);
			opt.rect = QRect(std::min(r.right(), static_cast<int>(Client(hwnd).right) - 1) - size - 6, r.top() + (r.height() - size) / 2, size, size);
			p->save();
			QApplication::style()->drawPrimitive((group.state & LVGS_COLLAPSED) ? QStyle::PE_IndicatorArrowDown : QStyle::PE_IndicatorArrowUp, &opt, p, w);
			p->restore();
		}
	}
	HFONT base = FontOfControl(hwnd);
	LOGFONT lf{};
	::GetObject(base, sizeof(lf), &lf);
	lf.lfWeight = FW_BOLD;
	HFONT bold = ::CreateFontIndirect(&lf);
	HGDIOBJ oldFont = ::SelectObject(dc, bold);
	RECT measure{0, 0, 0, 0};
	::DrawText(dc, group.header.c_str(), -1, &measure, DT_CALCRECT | DT_SINGLELINE | DT_NOPREFIX);
	const COLORREF color = ::GetSysColor(COLOR_HOTLIGHT);
	int fmt = LVCFMT_LEFT;
	if(group.align & LVGA_HEADER_CENTER)
		fmt = LVCFMT_CENTER;
	else if(group.align & LVGA_HEADER_RIGHT)
		fmt = LVCFMT_RIGHT;
	DrawText(dc, group.header, text, fmt, color);
	::SelectObject(dc, oldFont);
	::DeleteObject(bold);
	// A line after the heading.
	const int lineLeft = (fmt == LVCFMT_LEFT) ? text.left() + measure.right + 8 : r.left() + 10;
	const int lineY = text.center().y() + 1;
	HPEN pen = ::CreatePen(PS_SOLID, 1, ::GetSysColor(COLOR_3DLIGHT));
	HGDIOBJ oldPen = ::SelectObject(dc, pen);
	::MoveToEx(dc, lineLeft, lineY, nullptr);
	::LineTo(dc, r.right() - ((group.state & LVGS_COLLAPSIBLE) ? 26 : 10), lineY);
	::SelectObject(dc, oldPen);
	::DeleteObject(pen);
}

void PaintItem(HWND hwnd, HDC dc, int rowIndex, LRESULT customDraw)
{
	ListState *s = Control<ListState>(hwnd);
	if(s == nullptr)
		return;
	const Row &row = s->rows[static_cast<std::size_t>(rowIndex)];
	const int item = row.index;
	const ItemParts parts = PartsOf(hwnd, *s, rowIndex);
	const LONG_PTR style = StyleOf(hwnd);
	const UINT state = ItemState(hwnd, *s, item);
	const bool selected = (state & LVIS_SELECTED) != 0;
	const bool showSelection = selected && (s->focused || (style & LVS_SHOWSELALWAYS));
	const bool fullRow = (s->exStyle & LVS_EX_FULLROWSELECT) != 0;
	const COLORREF bk = Resolve(s->bk, COLOR_WINDOW);
	COLORREF textColor = Resolve(s->text, COLOR_WINDOWTEXT);
	COLORREF textBk = s->textBk == CLR_DEFAULT || s->textBk == CLR_NONE ? bk : s->textBk;
	const COLORREF selBk = s->focused ? ::GetSysColor(COLOR_HIGHLIGHT) : ::GetSysColor(COLOR_3DFACE);
	const COLORREF selText = s->focused ? ::GetSysColor(COLOR_HIGHLIGHTTEXT) : ::GetSysColor(COLOR_WINDOWTEXT);

	if(style & LVS_OWNERDRAWFIXED)
	{
		DRAWITEMSTRUCT dis{};
		dis.CtlType = ODT_LISTVIEW;
		dis.CtlID = static_cast<UINT>(::GetDlgCtrlID(hwnd));
		dis.itemID = static_cast<UINT>(item);
		dis.itemAction = ODA_DRAWENTIRE;
		dis.itemState = (selected ? ODS_SELECTED : 0) | ((s->focused && item == s->focus) ? ODS_FOCUS : 0);
		dis.hwndItem = hwnd;
		dis.hDC = dc;
		dis.rcItem = ToRECT(parts.row);
		dis.itemData = static_cast<ULONG_PTR>(ItemParam(hwnd, *s, item));
		::SendMessage(::GetParent(hwnd), WM_DRAWITEM, dis.CtlID, reinterpret_cast<LPARAM>(&dis));
		return;
	}

	NMLVCUSTOMDRAW cd{};
	cd.nmcd.hdc = dc;
	cd.nmcd.dwItemSpec = static_cast<DWORD_PTR>(item);
	cd.nmcd.rc = ToRECT(parts.row);
	cd.nmcd.uItemState = (selected ? CDIS_SELECTED : 0) | ((s->focused && item == s->focus) ? CDIS_FOCUS : 0) | (::IsWindowEnabled(hwnd) ? 0 : CDIS_DISABLED);
	cd.nmcd.lItemlParam = ItemParam(hwnd, *s, item);
	cd.clrText = textColor;
	cd.clrTextBk = textBk;
	cd.dwItemType = LVCDI_ITEM;
	LRESULT itemResult = CDRF_DODEFAULT;
	const int savedDC = ::SaveDC(dc);
	if(customDraw & CDRF_NOTIFYITEMDRAW)
	{
		cd.nmcd.dwDrawStage = CDDS_ITEMPREPAINT;
		itemResult = Notify(hwnd, cd.nmcd.hdr, NM_CUSTOMDRAW);
		if((s = Control<ListState>(hwnd)) == nullptr)
			return;
		if(itemResult & CDRF_SKIPDEFAULT)
		{
			::RestoreDC(dc, savedDC);
			return;
		}
		textColor = cd.clrText;
		textBk = cd.clrTextBk == CLR_DEFAULT ? textBk : cd.clrTextBk;
	}

	// Columns, in order: column 0 is the item, the rest subitems.
	const int columns = Report(hwnd) ? std::max(1, static_cast<int>(s->columns.size())) : 1;
	for(int c = 0; c < columns; c++)
	{
		COLORREF subText = textColor, subBk = textBk;
		if(itemResult & CDRF_NOTIFYSUBITEMDRAW)
		{
			cd.nmcd.dwDrawStage = CDDS_ITEMPREPAINT | CDDS_SUBITEM;
			cd.iSubItem = c;
			cd.clrText = textColor;
			cd.clrTextBk = textBk;
			const LRESULT r = Notify(hwnd, cd.nmcd.hdr, NM_CUSTOMDRAW);
			if((s = Control<ListState>(hwnd)) == nullptr)
				return;
			if(r & CDRF_SKIPDEFAULT)
				continue;
			subText = cd.clrText == CLR_DEFAULT ? textColor : cd.clrText;
			subBk = cd.clrTextBk == CLR_DEFAULT ? textBk : cd.clrTextBk;
		}
		const LvColumn *col = Report(hwnd) && !s->columns.empty() ? &s->columns[static_cast<std::size_t>(c)] : nullptr;
		const int left = -s->scrollX + (Report(hwnd) ? ColumnLeft(*s, c) : 0);
		const int width = col != nullptr ? col->width : parts.row.width();
		const QRect cell(left, parts.row.top(), width, parts.row.height());
		if(width <= 0)
			continue;
		const bool cellSelected = showSelection && (fullRow || c == 0);
		const std::wstring text = ItemText(hwnd, item, col != nullptr ? col->subItem : 0);
		if((s = Control<ListState>(hwnd)) == nullptr)
			return;
		if(c == 0)
		{
			if(subBk != bk)
				FillColor(dc, cell, subBk);
			if(HasStateIcons(*s))
			{
				const UINT stateImage = (state & LVIS_STATEIMAGEMASK) >> 12;
				if(s->images[LVSIL_STATE] != nullptr && stateImage > 0)
					ImageList_Draw(s->images[LVSIL_STATE], static_cast<int>(stateImage) - 1, dc, parts.state.left(), parts.state.top() + (parts.state.height() - 16) / 2, ILD_TRANSPARENT);
				else if((s->exStyle & LVS_EX_CHECKBOXES) && stateImage > 0)
					DrawCheckBox(hwnd, dc, parts.state, stateImage == 2);
			}
			if(s->images[LVSIL_SMALL] != nullptr)
			{
				const int image = ItemImage(hwnd, item);
				if((s = Control<ListState>(hwnd)) == nullptr)
					return;
				int cx = 0, cy = 0;
				ImageList_GetIconSize(s->images[LVSIL_SMALL], &cx, &cy);
				if(image >= 0)
					ImageList_Draw(s->images[LVSIL_SMALL], image, dc, parts.icon.left(), parts.icon.top() + (parts.icon.height() - cy) / 2, showSelection ? ILD_SELECTED : ILD_TRANSPARENT);
			}
			QRect label = parts.label;
			if(cellSelected)
			{
				// Without full row selection, only the text is highlighted.
				QRect hl = label;
				if(!fullRow)
				{
					HDC measureDC = dc;
					RECT m{0, 0, 0, 0};
					::DrawText(measureDC, text.c_str(), -1, &m, DT_CALCRECT | DT_SINGLELINE | DT_NOPREFIX);
					hl.setWidth(std::min(label.width(), static_cast<int>(m.right) + 2 * LabelPadding));
				} else
				{
					hl = cell.adjusted(parts.icon.left() - cell.left(), 0, 0, 0);
				}
				FillColor(dc, hl, selBk);
			}
			DrawText(dc, text, label.adjusted(LabelPadding, 0, -LabelPadding, 0), LVCFMT_LEFT, cellSelected ? selText : subText);
		} else
		{
			if(cellSelected)
				FillColor(dc, cell, selBk);
			else if(subBk != bk)
				FillColor(dc, cell, subBk);
			DrawText(dc, text, cell.adjusted(SubItemPadding, 0, -SubItemPadding, 0), col != nullptr ? col->fmt : LVCFMT_LEFT, cellSelected ? selText : subText);
		}
	}
	if(s->focused && item == s->focus && !(::SendMessage(hwnd, WM_QUERYUISTATE, 0, 0) & UISF_HIDEFOCUS))
	{
		QRect fr = fullRow ? parts.select : parts.label;
		if(!fullRow)
		{
			RECT m{0, 0, 0, 0};
			const std::wstring text = ItemText(hwnd, item, 0);
			::DrawText(dc, text.c_str(), -1, &m, DT_CALCRECT | DT_SINGLELINE | DT_NOPREFIX);
			fr.setWidth(std::min(fr.width(), static_cast<int>(m.right) + 2 * LabelPadding));
		}
		const RECT focus = ToRECT(fr);
		::SetTextColor(dc, RGB(0, 0, 0));
		::SetBkColor(dc, RGB(255, 255, 255));
		::DrawFocusRect(dc, &focus);
	}
	::RestoreDC(dc, savedDC);
	if(itemResult & CDRF_NOTIFYPOSTPAINT)
	{
		cd.nmcd.dwDrawStage = CDDS_ITEMPOSTPAINT;
		Notify(hwnd, cd.nmcd.hdr, NM_CUSTOMDRAW);
	}
}

void PaintList(HWND hwnd, HDC dc)
{
	ListState *s = Control<ListState>(hwnd);
	if(s == nullptr)
		return;
	if(s->layoutPending)
	{
		s->layoutPending = false;
		BuildRows(hwnd, *s);
		UpdateScrollBars(hwnd, *s);
	}
	const RECT rc = Client(hwnd);
	const COLORREF bk = Resolve(s->bk, COLOR_WINDOW);
	FillColor(dc, ToRect(rc), bk);
	HGDIOBJ oldFont = ::SelectObject(dc, FontOfControl(hwnd));

	NMLVCUSTOMDRAW cd{};
	cd.nmcd.hdc = dc;
	cd.nmcd.rc = rc;
	cd.nmcd.dwDrawStage = CDDS_PREPAINT;
	const LRESULT pre = Notify(hwnd, cd.nmcd.hdr, NM_CUSTOMDRAW);
	if((s = Control<ListState>(hwnd)) == nullptr)
		return;
	if(!(pre & CDRF_SKIPDEFAULT))
	{
		const int top = ContentTop(*s);
		::IntersectClipRect(dc, 0, top, rc.right, rc.bottom);
		const int first = std::max(0, RowAt(*s, top));
		for(int r = first; r < static_cast<int>(s->rows.size()); r++)
		{
			const Row &row = s->rows[static_cast<std::size_t>(r)];
			if(top + row.top - s->scrollY >= rc.bottom)
				break;
			if(row.group)
				PaintGroup(hwnd, dc, *s, r);
			else
				PaintItem(hwnd, dc, r, pre);
			if((s = Control<ListState>(hwnd)) == nullptr)
				return;
		}
		if((s->exStyle & LVS_EX_GRIDLINES) && Report(hwnd))
		{
			HPEN pen = ::CreatePen(PS_SOLID, 1, ::GetSysColor(COLOR_3DLIGHT));
			HGDIOBJ oldPen = ::SelectObject(dc, pen);
			if(s->groupView && !s->groups.empty())
			{
				// In groups, lines only around the items, so that they stay
				// clear of the group headers.
				for(int r = first; r < static_cast<int>(s->rows.size()); r++)
				{
					const Row &row = s->rows[static_cast<std::size_t>(r)];
					const int y0 = top + row.top - s->scrollY;
					if(y0 >= rc.bottom)
						break;
					if(row.group)
						continue;
					const int y1 = y0 + row.height - 1;
					::MoveToEx(dc, 0, y1, nullptr);
					::LineTo(dc, rc.right, y1);
					int x = -s->scrollX;
					for(const LvColumn &c : s->columns)
					{
						x += c.width;
						::MoveToEx(dc, x - 1, y0, nullptr);
						::LineTo(dc, x - 1, y1);
					}
				}
			}
			else
			{
				int x = -s->scrollX;
				for(const LvColumn &c : s->columns)
				{
					x += c.width;
					::MoveToEx(dc, x - 1, top, nullptr);
					::LineTo(dc, x - 1, rc.bottom);
				}
				for(int y = top + s->rowHeight - 1 - (s->scrollY % std::max(1, s->rowHeight)); y < rc.bottom; y += s->rowHeight)
				{
					::MoveToEx(dc, 0, y, nullptr);
					::LineTo(dc, rc.right, y);
				}
			}
			::SelectObject(dc, oldPen);
			::DeleteObject(pen);
		}
	}
	if(pre & CDRF_NOTIFYPOSTPAINT)
	{
		cd.nmcd.dwDrawStage = CDDS_POSTPAINT;
		Notify(hwnd, cd.nmcd.hdr, NM_CUSTOMDRAW);
	}
	::SelectObject(dc, oldFont);
}

// ---- items ------------------------------------------------------------------------------

int CompareText(const std::wstring &a, const std::wstring &b)
{
	return QString::compare(ToQString(a.c_str()), ToQString(b.c_str()), Qt::CaseInsensitive);
}

void CopyIn(LvItem &item, const LVITEM *in, int sub)
{
	if(in->mask & LVIF_TEXT)
	{
		if(item.text.size() <= static_cast<std::size_t>(sub))
		{
			item.text.resize(static_cast<std::size_t>(sub) + 1);
			item.callback.resize(static_cast<std::size_t>(sub) + 1);
		}
		const bool callback = in->pszText == LPSTR_TEXTCALLBACKW;
		item.callback[static_cast<std::size_t>(sub)] = callback;
		item.text[static_cast<std::size_t>(sub)] = (callback || in->pszText == nullptr) ? std::wstring() : std::wstring(in->pszText);
	}
	if(sub != 0)
		return;
	if(in->mask & LVIF_IMAGE)
		item.image = in->iImage;
	if(in->mask & LVIF_PARAM)
		item.param = in->lParam;
	if(in->mask & LVIF_INDENT)
		item.indent = in->iIndent;
	if(in->mask & LVIF_GROUPID)
		item.groupId = in->iGroupId;
}

void CopyText(const std::wstring &text, LVITEM *out)
{
	if(out->pszText == nullptr || out->cchTextMax <= 0)
		return;
	const std::size_t n = std::min(text.size(), static_cast<std::size_t>(out->cchTextMax - 1));
	std::copy_n(text.c_str(), n, out->pszText);
	out->pszText[n] = 0;
}

int InsertItem(HWND hwnd, ListState &s, const LVITEM *in)
{
	if(OwnerData(hwnd) || in == nullptr || in->iSubItem != 0)
		return -1;
	LvItem item;
	if(s.exStyle & LVS_EX_CHECKBOXES)
		item.state = INDEXTOSTATEIMAGEMASK(1);
	CopyIn(item, in, 0);
	if(in->mask & LVIF_STATE)
		item.state = (item.state & ~in->stateMask) | (in->state & in->stateMask);
	int index = std::clamp(in->iItem, 0, static_cast<int>(s.items.size()));
	const LONG_PTR style = StyleOf(hwnd);
	if(style & (LVS_SORTASCENDING | LVS_SORTDESCENDING))
	{
		const std::wstring text = item.text.empty() ? std::wstring() : item.text[0];
		const bool descending = (style & LVS_SORTDESCENDING) != 0;
		index = 0;
		while(index < static_cast<int>(s.items.size()))
		{
			const LvItem &other = s.items[static_cast<std::size_t>(index)];
			const int c = CompareText(text, other.text.empty() ? std::wstring() : other.text[0]);
			if(descending ? c > 0 : c < 0)
				break;
			index++;
		}
	}
	const UINT state = item.state;
	item.state &= ~(LVIS_FOCUSED | LVIS_SELECTED);
	s.items.insert(s.items.begin() + index, std::move(item));
	if(s.focus >= index)
		s.focus++;
	if(s.mark >= index)
		s.mark++;
	if(s.editItem >= index)
		s.editItem++;
	s.layoutPending = true;
	::InvalidateRect(hwnd, nullptr, TRUE);
	NMLISTVIEW nm{};
	nm.iItem = index;
	Notify(hwnd, nm.hdr, LVN_INSERTITEM);
	if(state & (LVIS_FOCUSED | LVIS_SELECTED))
		ChangeState(hwnd, index, state, LVIS_FOCUSED | LVIS_SELECTED);
	return index;
}

void EndEdit(HWND hwnd, bool save);

bool DeleteItem(HWND hwnd, int index)
{
	ListState *s = Control<ListState>(hwnd);
	if(s == nullptr || !Valid(hwnd, *s, index))
		return false;
	if(s->editItem == index)
		EndEdit(hwnd, false);
	if((s = Control<ListState>(hwnd)) == nullptr || !Valid(hwnd, *s, index))
		return false;
	NMLISTVIEW nm{};
	nm.iItem = index;
	nm.lParam = ItemParam(hwnd, *s, index);
	Notify(hwnd, nm.hdr, LVN_DELETEITEM);
	if((s = Control<ListState>(hwnd)) == nullptr || !Valid(hwnd, *s, index))
		return false;
	if(OwnerData(hwnd))
	{
		s->ownerState.erase(s->ownerState.begin() + index);
		s->ownerCount--;
	} else
	{
		s->items.erase(s->items.begin() + index);
	}
	auto shift = [index](int &v)
	{
		if(v == index)
			v = -1;
		else if(v > index)
			v--;
	};
	shift(s->focus);
	shift(s->mark);
	shift(s->hot);
	s->layoutPending = true;
	::InvalidateRect(hwnd, nullptr, TRUE);
	return true;
}

void DeleteAll(HWND hwnd)
{
	ListState *s = Control<ListState>(hwnd);
	if(s == nullptr)
		return;
	EndEdit(hwnd, false);
	if((s = Control<ListState>(hwnd)) == nullptr)
		return;
	if(Count(hwnd, *s) > 0)
	{
		NMLISTVIEW nm{};
		nm.iItem = -1;
		const bool suppress = Notify(hwnd, nm.hdr, LVN_DELETEALLITEMS) != 0;
		if((s = Control<ListState>(hwnd)) == nullptr)
			return;
		if(!suppress)
		{
			for(int i = 0; i < Count(hwnd, *s); i++)
			{
				NMLISTVIEW one{};
				one.iItem = i;
				one.lParam = ItemParam(hwnd, *s, i);
				Notify(hwnd, one.hdr, LVN_DELETEITEM);
				if((s = Control<ListState>(hwnd)) == nullptr)
					return;
			}
		}
	}
	s->items.clear();
	s->ownerCount = 0;
	s->ownerState.clear();
	s->focus = s->mark = s->hot = -1;
	s->scrollY = 0;
	s->layoutPending = true;
	::InvalidateRect(hwnd, nullptr, TRUE);
}

// Sorts items with a comparison of lParams (LVM_SORTITEMS) or indexes (LVM_SORTITEMSEX).
bool SortItems(HWND hwnd, PFNLVCOMPARE compare, LPARAM data, bool byIndex)
{
	ListState *s = Control<ListState>(hwnd);
	if(s == nullptr || compare == nullptr || OwnerData(hwnd))
		return false;
	std::vector<int> order(s->items.size());
	std::iota(order.begin(), order.end(), 0);
	std::stable_sort(order.begin(), order.end(), [&](int a, int b)
	{
		const LPARAM la = byIndex ? static_cast<LPARAM>(a) : s->items[static_cast<std::size_t>(a)].param;
		const LPARAM lb = byIndex ? static_cast<LPARAM>(b) : s->items[static_cast<std::size_t>(b)].param;
		return compare(la, lb, data) < 0;
	});
	if((s = Control<ListState>(hwnd)) == nullptr || order.size() != s->items.size())
		return false;
	std::vector<LvItem> sorted;
	sorted.reserve(order.size());
	int focus = -1, mark = -1;
	for(std::size_t i = 0; i < order.size(); i++)
	{
		if(order[i] == s->focus)
			focus = static_cast<int>(i);
		if(order[i] == s->mark)
			mark = static_cast<int>(i);
		sorted.push_back(std::move(s->items[static_cast<std::size_t>(order[i])]));
	}
	s->items = std::move(sorted);
	s->focus = focus;
	s->mark = mark;
	Relayout(hwnd, *s);
	return true;
}

int AutoWidth(HWND hwnd, ListState &s, int col, bool useHeader)
{
	HDC dc = ::GetDC(hwnd);
	HGDIOBJ old = ::SelectObject(dc, FontOfControl(hwnd));
	int width = 0;
	const int sub = static_cast<std::size_t>(col) < s.columns.size() ? s.columns[static_cast<std::size_t>(col)].subItem : col;
	for(int i = 0; i < Count(hwnd, s); i++)
	{
		const std::wstring text = ItemText(hwnd, i, sub);
		RECT m{0, 0, 0, 0};
		::DrawText(dc, text.c_str(), -1, &m, DT_CALCRECT | DT_SINGLELINE | DT_NOPREFIX);
		width = std::max(width, static_cast<int>(m.right));
	}
	if(col == 0)
	{
		width += 2 * LabelPadding + SmallIconWidth(s) + (HasStateIcons(s) ? StateIconSize : 0);
		width = std::max(width, 1);
	} else
	{
		width += 2 * SubItemPadding;
	}
	if(useHeader && static_cast<std::size_t>(col) < s.columns.size())
	{
		RECT m{0, 0, 0, 0};
		::DrawText(dc, s.columns[static_cast<std::size_t>(col)].text.c_str(), -1, &m, DT_CALCRECT | DT_SINGLELINE | DT_NOPREFIX);
		width = std::max(width, static_cast<int>(m.right) + 2 * SubItemPadding + 6);
		// The last column fills the rest of the control.
		if(static_cast<std::size_t>(col) + 1 == s.columns.size())
			width = std::max(width, static_cast<int>(Client(hwnd).right) - ColumnLeft(s, col));
	}
	::SelectObject(dc, old);
	::ReleaseDC(hwnd, dc);
	return width;
}

void SetColumnWidth(HWND hwnd, ListState &s, int col, int width)
{
	if(static_cast<std::size_t>(col) >= s.columns.size())
		return;
	s.columns[static_cast<std::size_t>(col)].width = std::max(0, width);
	if(s.header != nullptr)
	{
		HDITEM hdi{};
		hdi.mask = HDI_WIDTH;
		hdi.cxy = s.columns[static_cast<std::size_t>(col)].width;
		// The header tells us back with HDN_ITEMCHANGED, which we ignore for this.
		::SendMessage(s.header, HDM_SETITEM, static_cast<WPARAM>(col), reinterpret_cast<LPARAM>(&hdi));
	}
	UpdateScrollBars(hwnd, s);
	::InvalidateRect(hwnd, nullptr, TRUE);
}

// ---- label editing -------------------------------------------------------------------------

const wchar_t EditOwnerProp[] = L"";

LRESULT CALLBACK EditSubclass(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

struct EditLink
{
	HWND list = nullptr;
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
	ListState *s = Control<ListState>(hwnd);
	if(s == nullptr || s->edit == nullptr)
		return;
	const HWND edit = s->edit;
	const int item = s->editItem;
	s->edit = nullptr;
	s->editItem = -1;
	WCHAR buffer[1024] = {};
	::GetWindowText(edit, buffer, 1024);
	NMLVDISPINFO di{};
	di.item.mask = LVIF_TEXT;
	di.item.iItem = item;
	di.item.pszText = save ? buffer : nullptr;
	di.item.cchTextMax = 1024;
	di.item.lParam = ItemParam(hwnd, *s, item);
	const LRESULT accept = Notify(hwnd, di.hdr, LVN_ENDLABELEDIT);
	if(save && accept && (s = Control<ListState>(hwnd)) != nullptr && Valid(hwnd, *s, item) && !OwnerData(hwnd))
	{
		LVITEM set{};
		set.mask = LVIF_TEXT;
		set.iItem = item;
		set.pszText = buffer;
		::SendMessage(hwnd, LVM_SETITEMTEXT, static_cast<WPARAM>(item), reinterpret_cast<LPARAM>(&set));
	}
	::DestroyWindow(edit);
	auto &links = EditLinks();
	links.erase(std::remove_if(links.begin(), links.end(), [edit](const auto &l) { return l.first == edit; }), links.end());
	if(::GetFocus() == nullptr || ::GetFocus() == edit)
		::SetFocus(hwnd);
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
			EndEdit(link.list, wp == VK_RETURN);
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
			if(ListState *s = Control<ListState>(link.list); s != nullptr && s->edit == hwnd)
				EndEdit(link.list, true);
			return r;
		}
	default:
		break;
	}
	return ::CallWindowProc(link.old, hwnd, msg, wp, lp);
}

HWND BeginEdit(HWND hwnd, int item)
{
	ListState *s = Control<ListState>(hwnd);
	if(s == nullptr)
		return nullptr;
	if(s->edit != nullptr)
		EndEdit(hwnd, true);
	if((s = Control<ListState>(hwnd)) == nullptr || !Valid(hwnd, *s, item))
		return nullptr;
	EnsureVisible(hwnd, *s, item, false);
	const std::wstring text = ItemText(hwnd, item, 0);
	NMLVDISPINFO di{};
	std::wstring copy = text;
	di.item.mask = LVIF_TEXT;
	di.item.iItem = item;
	di.item.pszText = copy.data();
	di.item.cchTextMax = static_cast<int>(copy.size()) + 1;
	di.item.lParam = ItemParam(hwnd, *s, item);
	if(Notify(hwnd, di.hdr, LVN_BEGINLABELEDIT) != 0)
		return nullptr;
	if((s = Control<ListState>(hwnd)) == nullptr || !Valid(hwnd, *s, item))
		return nullptr;
	const int r = RowOfItem(*s, item);
	if(r < 0)
		return nullptr;
	QRect label = PartsOf(hwnd, *s, r).label;
	const HWND edit = ::CreateWindowEx(0, L"Edit", text.c_str(), WS_CHILD | WS_BORDER | ES_AUTOHSCROLL | WS_VISIBLE, label.left(), label.top() - 1, std::max(label.width(), 40), label.height() + 2, hwnd, nullptr, nullptr, nullptr);
	if(edit == nullptr)
		return nullptr;
	::SendMessage(edit, WM_SETFONT, reinterpret_cast<WPARAM>(FontOfControl(hwnd)), FALSE);
	EditLinks().emplace_back(edit, EditLink{hwnd, reinterpret_cast<WNDPROC>(::GetWindowLongPtr(edit, GWLP_WNDPROC))});
	::SetWindowLongPtr(edit, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&EditSubclass));
	if((s = Control<ListState>(hwnd)) == nullptr)
		return nullptr;
	s->edit = edit;
	s->editItem = item;
	::SendMessage(edit, EM_SETSEL, 0, -1);
	::SetFocus(edit);
	return edit;
}

// ---- input -------------------------------------------------------------------------------

UINT KeyFlags()
{
	UINT f = 0;
	if(::GetKeyState(VK_SHIFT) < 0)
		f |= LVKF_SHIFT;
	if(::GetKeyState(VK_CONTROL) < 0)
		f |= LVKF_CONTROL;
	if(::GetKeyState(VK_MENU) < 0)
		f |= LVKF_ALT;
	return f;
}

LRESULT NotifyActivate(HWND hwnd, UINT code, int item, int sub, POINT pt)
{
	ListState *s = Control<ListState>(hwnd);
	NMITEMACTIVATE nm{};
	nm.iItem = item;
	nm.iSubItem = sub;
	nm.ptAction = pt;
	nm.uKeyFlags = KeyFlags();
	if(s != nullptr && Valid(hwnd, *s, item))
	{
		nm.lParam = ItemParam(hwnd, *s, item);
		nm.uNewState = nm.uOldState = ItemState(hwnd, *s, item);
	}
	return Notify(hwnd, nm.hdr, code);
}

void ToggleCheck(HWND hwnd, int item)
{
	ListState *s = Control<ListState>(hwnd);
	if(s == nullptr || !Valid(hwnd, *s, item))
		return;
	const UINT stateImage = (ItemState(hwnd, *s, item) & LVIS_STATEIMAGEMASK) >> 12;
	const UINT next = stateImage == 2 ? 1 : 2;
	ChangeState(hwnd, item, INDEXTOSTATEIMAGEMASK(next), LVIS_STATEIMAGEMASK);
}

void ClickItem(HWND hwnd, int item, bool right)
{
	ListState *s = Control<ListState>(hwnd);
	if(s == nullptr)
		return;
	const bool single = (StyleOf(hwnd) & LVS_SINGLESEL) != 0;
	const bool shift = ::GetKeyState(VK_SHIFT) < 0 && !single;
	const bool ctrl = ::GetKeyState(VK_CONTROL) < 0 && !single;
	if(item < 0)
	{
		if(!ctrl && !shift && !right)
		{
			for(int i = 0; i < Count(hwnd, *s); i++)
			{
				if(ItemState(hwnd, *s, i) & LVIS_SELECTED)
					ChangeState(hwnd, i, 0, LVIS_SELECTED);
				if((s = Control<ListState>(hwnd)) == nullptr)
					return;
			}
		}
		return;
	}
	if(right)
	{
		if(!(ItemState(hwnd, *s, item) & LVIS_SELECTED))
		{
			SelectOnly(hwnd, item);
			if((s = Control<ListState>(hwnd)) != nullptr)
				s->mark = item;
		} else
		{
			ChangeState(hwnd, item, LVIS_FOCUSED, LVIS_FOCUSED);
		}
		return;
	}
	if(shift)
	{
		const int anchor = Valid(hwnd, *s, s->mark) ? s->mark : item;
		SelectRange(hwnd, anchor, item, ctrl);
	} else if(ctrl)
	{
		const bool selected = (ItemState(hwnd, *s, item) & LVIS_SELECTED) != 0;
		ChangeState(hwnd, item, (selected ? 0 : LVIS_SELECTED) | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
		if((s = Control<ListState>(hwnd)) != nullptr)
			s->mark = item;
	} else
	{
		SelectOnly(hwnd, item);
		if((s = Control<ListState>(hwnd)) != nullptr)
			s->mark = item;
	}
}

// Keyboard navigation: the item a key moves the focus to.
int NavigateTarget(HWND hwnd, ListState &s, UINT vk)
{
	if(s.rows.empty())
		return -1;
	std::vector<int> visible;
	for(const Row &r : s.rows)
	{
		if(!r.group)
			visible.push_back(r.index);
	}
	if(visible.empty())
		return -1;
	auto it = std::find(visible.begin(), visible.end(), s.focus);
	int pos = it == visible.end() ? -1 : static_cast<int>(it - visible.begin());
	const int last = static_cast<int>(visible.size()) - 1;
	const int perPage = std::max(1, ViewHeight(hwnd, s) / std::max(1, s.rowHeight));
	switch(vk)
	{
	case VK_UP: pos = pos < 0 ? 0 : std::max(0, pos - 1); break;
	case VK_DOWN: pos = pos < 0 ? 0 : std::min(last, pos + 1); break;
	case VK_HOME: pos = 0; break;
	case VK_END: pos = last; break;
	case VK_PRIOR: pos = std::max(0, (pos < 0 ? 0 : pos) - (perPage - 1)); break;
	case VK_NEXT: pos = std::min(last, (pos < 0 ? 0 : pos) + (perPage - 1)); break;
	default: return -1;
	}
	return visible[static_cast<std::size_t>(pos)];
}

void IncrementalSearch(HWND hwnd, WCHAR ch)
{
	ListState *s = Control<ListState>(hwnd);
	if(s == nullptr || Count(hwnd, *s) == 0)
		return;
	if(!s->searchTime.isValid() || s->searchTime.elapsed() > 1000)
		s->search.clear();
	s->searchTime.restart();
	const bool repeat = !s->search.empty() && s->search.size() == 1 && s->search[0] == ch;
	if(!repeat)
		s->search.push_back(ch);
	const QString needle = ToQString(s->search.c_str());
	const int count = Count(hwnd, *s);
	const int start = s->focus < 0 ? 0 : (repeat || s->search.size() == 1 ? s->focus + 1 : s->focus);
	for(int k = 0; k < count; k++)
	{
		const int i = (start + k) % count;
		if(ToQString(ItemText(hwnd, i, 0).c_str()).startsWith(needle, Qt::CaseInsensitive))
		{
			SelectOnly(hwnd, i);
			if((s = Control<ListState>(hwnd)) != nullptr)
			{
				s->mark = i;
				EnsureVisible(hwnd, *s, i, false);
			}
			return;
		}
		if((s = Control<ListState>(hwnd)) == nullptr)
			return;
	}
}

void Scroll(HWND hwnd, ListState &s, int dx, int dy)
{
	const int maxY = std::max(0, s.contentHeight - ViewHeight(hwnd, s));
	const int maxX = std::max(0, TotalWidth(hwnd, s) - static_cast<int>(Client(hwnd).right));
	const int y = std::clamp(s.scrollY + dy, 0, maxY);
	const int x = std::clamp(s.scrollX + dx, 0, maxX);
	if(x == s.scrollX && y == s.scrollY)
		return;
	s.scrollX = x;
	s.scrollY = y;
	if(s.edit != nullptr)
		EndEdit(hwnd, true);
	if(ListState *live = Control<ListState>(hwnd))
	{
		UpdateScrollBars(hwnd, *live);
		::InvalidateRect(hwnd, nullptr, TRUE);
	}
}

void ScrollMessage(HWND hwnd, ListState &s, int bar, UINT code)
{
	SCROLLINFO si{};
	si.cbSize = sizeof(si);
	si.fMask = SIF_ALL;
	::GetScrollInfo(hwnd, bar, &si);
	const int line = bar == SB_VERT ? s.rowHeight : 16;
	const int page = std::max(line, static_cast<int>(si.nPage) - (bar == SB_VERT ? 0 : 0));
	int pos = bar == SB_VERT ? s.scrollY : s.scrollX;
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
	if(bar == SB_VERT && !s.groupView && s.rowHeight > 0)
		pos = pos / s.rowHeight * s.rowHeight;
	if(bar == SB_VERT)
		Scroll(hwnd, s, 0, pos - s.scrollY);
	else
		Scroll(hwnd, s, pos - s.scrollX, 0);
}

void CreateHeader(HWND hwnd, ListState &s)
{
	if(s.header != nullptr)
		return;
	DWORD style = WS_CHILD | HDS_HORZ | HDS_FULLDRAG;
	if(!(StyleOf(hwnd) & LVS_NOSORTHEADER))
		style |= HDS_BUTTONS;
	if(s.exStyle & LVS_EX_HEADERDRAGDROP)
		style |= HDS_DRAGDROP;
	s.header = ::CreateWindowEx(0, WC_HEADER, nullptr, style, 0, 0, 0, 0, hwnd, nullptr, nullptr, nullptr);
	if(ListState *live = Control<ListState>(hwnd))
		::SendMessage(live->header, WM_SETFONT, reinterpret_cast<WPARAM>(FontOfControl(hwnd)), FALSE);
}

// What the header tells its list view: column widths and clicks.
LRESULT HeaderNotify(HWND hwnd, ListState &s, NMHEADER *nm)
{
	switch(nm->hdr.code)
	{
	case HDN_ITEMCHANGED:
		if(nm->pitem != nullptr && (nm->pitem->mask & HDI_WIDTH) && static_cast<std::size_t>(nm->iItem) < s.columns.size())
		{
			s.columns[static_cast<std::size_t>(nm->iItem)].width = nm->pitem->cxy;
			if(s.edit != nullptr)
				EndEdit(hwnd, true);
			if(ListState *live = Control<ListState>(hwnd))
			{
				UpdateScrollBars(hwnd, *live);
				::InvalidateRect(hwnd, nullptr, TRUE);
			}
		}
		break;
	case HDN_ITEMCLICK:
		{
			NMLISTVIEW lv{};
			lv.iItem = -1;
			lv.iSubItem = nm->iItem;
			Notify(hwnd, lv.hdr, LVN_COLUMNCLICK);
		}
		break;
	case HDN_DIVIDERDBLCLICK:
		if(static_cast<std::size_t>(nm->iItem) < s.columns.size())
			SetColumnWidth(hwnd, s, nm->iItem, AutoWidth(hwnd, s, nm->iItem, false));
		break;
	default:
		break;
	}
	// The list view's parent sees the header's notifications too.
	const HWND parent = ::GetParent(hwnd);
	return parent != nullptr ? ::SendMessage(parent, WM_NOTIFY, nm->hdr.idFrom, reinterpret_cast<LPARAM>(nm)) : 0;
}

bool SetItem(HWND hwnd, const LVITEM *in)
{
	ListState *s = Control<ListState>(hwnd);
	if(s == nullptr || in == nullptr || !Valid(hwnd, *s, in->iItem))
		return false;
	const int item = in->iItem;
	if(OwnerData(hwnd))
	{
		if(in->mask & LVIF_STATE)
			ChangeState(hwnd, item, in->state, in->stateMask);
		return true;
	}
	const UINT changed = in->mask & (LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM | LVIF_INDENT | LVIF_GROUPID);
	if(changed != 0)
	{
		NMLISTVIEW nm{};
		nm.iItem = item;
		nm.iSubItem = in->iSubItem;
		nm.uChanged = changed;
		nm.lParam = s->items[static_cast<std::size_t>(item)].param;
		const bool textCallback = (in->mask & LVIF_TEXT) && in->pszText == LPSTR_TEXTCALLBACKW;
		if(!textCallback && Notify(hwnd, nm.hdr, LVN_ITEMCHANGING) != 0)
			return false;
		if((s = Control<ListState>(hwnd)) == nullptr || !Valid(hwnd, *s, item))
			return false;
		CopyIn(s->items[static_cast<std::size_t>(item)], in, in->iSubItem);
		if(in->mask & LVIF_GROUPID)
			s->layoutPending = true;
		const int r = RowOfItem(*s, item);
		if(r >= 0)
		{
			const RECT rr = ToRECT(PartsOf(hwnd, *s, r).row.adjusted(0, 0, static_cast<int>(Client(hwnd).right), 0));
			::InvalidateRect(hwnd, &rr, TRUE);
		} else
		{
			::InvalidateRect(hwnd, nullptr, TRUE);
		}
		if(!textCallback)
			Notify(hwnd, nm.hdr, LVN_ITEMCHANGED);
	}
	if((in->mask & LVIF_STATE) && in->iSubItem == 0)
		ChangeState(hwnd, item, in->state, in->stateMask);
	return true;
}

bool GetItem(HWND hwnd, LVITEM *out)
{
	ListState *s = Control<ListState>(hwnd);
	if(s == nullptr || out == nullptr || !Valid(hwnd, *s, out->iItem))
		return false;
	const int item = out->iItem;
	if(out->mask & LVIF_TEXT)
		CopyText(ItemText(hwnd, item, out->iSubItem), out);
	if((s = Control<ListState>(hwnd)) == nullptr || !Valid(hwnd, *s, item))
		return false;
	if(out->mask & LVIF_STATE)
		out->state = ItemState(hwnd, *s, item) & out->stateMask;
	if(OwnerData(hwnd))
	{
		if(out->mask & (LVIF_IMAGE | LVIF_PARAM | LVIF_INDENT))
		{
			NMLVDISPINFO di{};
			di.item = *out;
			di.item.mask &= LVIF_IMAGE | LVIF_PARAM | LVIF_INDENT;
			Notify(hwnd, di.hdr, LVN_GETDISPINFO);
			if(out->mask & LVIF_IMAGE)
				out->iImage = di.item.iImage;
			if(out->mask & LVIF_PARAM)
				out->lParam = di.item.lParam;
			if(out->mask & LVIF_INDENT)
				out->iIndent = di.item.iIndent;
		}
		return true;
	}
	const LvItem &it = s->items[static_cast<std::size_t>(item)];
	if(out->mask & LVIF_IMAGE)
		out->iImage = it.image == I_IMAGECALLBACK ? ItemImage(hwnd, item) : it.image;
	if(out->mask & LVIF_PARAM)
		out->lParam = it.param;
	if(out->mask & LVIF_INDENT)
		out->iIndent = it.indent;
	if(out->mask & LVIF_GROUPID)
		out->iGroupId = it.groupId;
	return true;
}

bool GetItemRect(HWND hwnd, ListState &s, int item, int sub, int code, RECT *out)
{
	if(out == nullptr || !Valid(hwnd, s, item))
		return false;
	if(s.layoutPending)
	{
		s.layoutPending = false;
		BuildRows(hwnd, s);
		UpdateScrollBars(hwnd, s);
	}
	const int r = RowOfItem(s, item);
	if(r < 0)
		return false;
	const ItemParts parts = PartsOf(hwnd, s, r);
	QRect q;
	if(sub > 0)
	{
		if(static_cast<std::size_t>(sub) >= s.columns.size())
			return false;
		q = QRect(-s.scrollX + ColumnLeft(s, sub), parts.row.top(), s.columns[static_cast<std::size_t>(sub)].width, parts.row.height());
	} else
	{
		switch(code)
		{
		case LVIR_ICON: q = parts.icon; break;
		case LVIR_LABEL: q = parts.label; break;
		case LVIR_SELECTBOUNDS: q = parts.select; break;
		default: q = sub == 0 && code == LVIR_BOUNDS && Report(hwnd) ? parts.bounds : parts.bounds; break;
		}
	}
	*out = ToRECT(q);
	return true;
}

LRESULT CALLBACK ListProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	ListState *s = Control<ListState>(hwnd);
	switch(msg)
	{
	case WM_NCCREATE:
		if(WindowData *d = Data(hwnd))
			d->control = std::make_unique<ListState>();
		return ::DefWindowProc(hwnd, msg, wp, lp);
	case WM_CREATE:
		if(s != nullptr)
		{
			CreateHeader(hwnd, *s);
			if((s = Control<ListState>(hwnd)) != nullptr)
			{
				Measure(hwnd, *s);
				Relayout(hwnd, *s);
			}
		}
		return 0;
	case WM_DESTROY:
		if(s != nullptr)
		{
			EndEdit(hwnd, false);
			DeleteAll(hwnd);
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
			PaintList(hwnd, reinterpret_cast<HDC>(wp));
		} else
		{
			PAINTSTRUCT ps;
			HDC dc = ::BeginPaint(hwnd, &ps);
			if(dc != nullptr)
				PaintList(hwnd, dc);
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
			if(s->header != nullptr)
				::SendMessage(s->header, WM_SETFONT, wp, lp);
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
	case WM_NOTIFY:
		if(s != nullptr && lp != 0 && reinterpret_cast<NMHDR *>(lp)->hwndFrom == s->header && s->header != nullptr)
			return HeaderNotify(hwnd, *s, reinterpret_cast<NMHEADER *>(lp));
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
			Scroll(hwnd, *s, 0, -delta * static_cast<int>(lines) * s->rowHeight / WHEEL_DELTA);
		}
		return 0;
	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
		if(s != nullptr)
		{
			if(s->edit != nullptr)
				EndEdit(hwnd, true);
			if((s = Control<ListState>(hwnd)) == nullptr)
				return 0;
			if(::GetFocus() != hwnd)
				::SetFocus(hwnd);
			if((s = Control<ListState>(hwnd)) == nullptr)
				return 0;
			POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
			UINT flags = 0;
			int group = -1;
			int sub = -1;
			int item = HitTest(hwnd, *s, pt, &flags, &sub, &group);
			if(!(flags & LVHT_ONITEM))
				item = -1;
			if(flags & LVHT_EX_GROUP_HEADER)
			{
				const int g = GroupIndex(*s, group);
				if(g >= 0 && (s->groups[static_cast<std::size_t>(g)].state & LVGS_COLLAPSIBLE) && msg == WM_LBUTTONDOWN)
				{
					s->groups[static_cast<std::size_t>(g)].state ^= LVGS_COLLAPSED;
					Relayout(hwnd, *s);
				}
				return 0;
			}
			const bool onCheck = (flags & LVHT_ONITEMSTATEICON) && (s->exStyle & LVS_EX_CHECKBOXES) && msg == WM_LBUTTONDOWN;
			const bool wasSelectedFocus = item >= 0 && item == s->focus && (ItemState(hwnd, *s, item) & LVIS_SELECTED) && s->focused;
			if(onCheck)
			{
				ToggleCheck(hwnd, item);
				ChangeState(hwnd, item, LVIS_FOCUSED, LVIS_FOCUSED);
			} else
			{
				ClickItem(hwnd, item, msg == WM_RBUTTONDOWN);
			}
			if((s = Control<ListState>(hwnd)) == nullptr)
				return 0;
			if(msg == WM_LBUTTONDOWN)
			{
				s->dragArmed = item >= 0 && !onCheck;
				s->dragFrom = pt;
				s->dragItem = item;
				// A second click on the focused selected label edits it.
				s->pendingEdit = (wasSelectedFocus && (StyleOf(hwnd) & LVS_EDITLABELS) && (flags & LVHT_ONITEMLABEL) && KeyFlags() == 0) ? item : -1;
				::SetCapture(hwnd);
			} else
			{
				EnsureVisible(hwnd, *s, item, false);
			}
			if(item >= 0)
				EnsureVisible(hwnd, *s, item, false);
		}
		return 0;
	case WM_MOUSEMOVE:
		if(s != nullptr && s->dragArmed && (wp & MK_LBUTTON))
		{
			const int dx = std::abs(GET_X_LPARAM(lp) - s->dragFrom.x), dy = std::abs(GET_Y_LPARAM(lp) - s->dragFrom.y);
			if(dx > ::GetSystemMetrics(SM_CXDRAG) || dy > ::GetSystemMetrics(SM_CYDRAG))
			{
				s->dragArmed = false;
				s->pendingEdit = -1;
				if(::GetCapture() == hwnd)
					::ReleaseCapture();
				NMLISTVIEW nm{};
				nm.iItem = s->dragItem;
				nm.ptAction = s->dragFrom;
				nm.lParam = ItemParam(hwnd, *s, s->dragItem);
				Notify(hwnd, nm.hdr, LVN_BEGINDRAG);
			}
		}
		return 0;
	case WM_LBUTTONUP:
		if(s != nullptr)
		{
			const bool captured = ::GetCapture() == hwnd;
			s->dragArmed = false;
			if(captured)
				::ReleaseCapture();
			if((s = Control<ListState>(hwnd)) == nullptr)
				return 0;
			if(!captured)
				return 0;
			POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
			UINT flags = 0;
			int sub = -1;
			int item = HitTest(hwnd, *s, pt, &flags, &sub);
			if(!(flags & LVHT_ONITEM))
				item = -1;
			const int edit = s->pendingEdit;
			s->pendingEdit = -1;
			NotifyActivate(hwnd, NM_CLICK, item, sub, pt);
			if((s = Control<ListState>(hwnd)) == nullptr)
				return 0;
			if(item >= 0 && (s->exStyle & LVS_EX_ONECLICKACTIVATE))
				NotifyActivate(hwnd, LVN_ITEMACTIVATE, item, sub, pt);
			if((s = Control<ListState>(hwnd)) != nullptr && edit >= 0 && edit == item)
			{
				s->pendingEdit = edit;
				::SetTimer(hwnd, EditTimer, ::GetDoubleClickTime(), nullptr);
			}
		}
		return 0;
	case WM_TIMER:
		if(s != nullptr && wp == EditTimer)
		{
			::KillTimer(hwnd, EditTimer);
			const int item = s->pendingEdit;
			s->pendingEdit = -1;
			if(Valid(hwnd, *s, item) && item == s->focus)
				BeginEdit(hwnd, item);
			return 0;
		}
		return ::DefWindowProc(hwnd, msg, wp, lp);
	case WM_LBUTTONDBLCLK:
	case WM_RBUTTONDBLCLK:
		if(s != nullptr)
		{
			::KillTimer(hwnd, EditTimer);
			s->pendingEdit = -1;
			POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
			UINT flags = 0;
			int sub = -1;
			int item = HitTest(hwnd, *s, pt, &flags, &sub);
			if(!(flags & LVHT_ONITEM))
				item = -1;
			if(msg == WM_LBUTTONDBLCLK && (flags & LVHT_ONITEMSTATEICON) && (s->exStyle & LVS_EX_CHECKBOXES))
			{
				ToggleCheck(hwnd, item);
				return 0;
			}
			NotifyActivate(hwnd, msg == WM_LBUTTONDBLCLK ? NM_DBLCLK : NM_RDBLCLK, item, sub, pt);
			if(msg == WM_LBUTTONDBLCLK && item >= 0 && Control<ListState>(hwnd) != nullptr)
				NotifyActivate(hwnd, LVN_ITEMACTIVATE, item, sub, pt);
		}
		return 0;
	case WM_RBUTTONUP:
		if(s != nullptr)
		{
			POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
			UINT flags = 0;
			int sub = -1;
			int item = HitTest(hwnd, *s, pt, &flags, &sub);
			if(!(flags & LVHT_ONITEM))
				item = -1;
			if(NotifyActivate(hwnd, NM_RCLICK, item, sub, pt) == 0)
			{
				POINT screen = pt;
				::ClientToScreen(hwnd, &screen);
				::SendMessage(hwnd, WM_CONTEXTMENU, reinterpret_cast<WPARAM>(hwnd), MAKELPARAM(screen.x, screen.y));
			}
		}
		return 0;
	case WM_CONTEXTMENU:
		return ::DefWindowProc(hwnd, msg, wp, lp);
	case WM_KEYDOWN:
		if(s != nullptr)
		{
			NMLVKEYDOWN kd{};
			kd.wVKey = static_cast<WORD>(wp);
			kd.flags = static_cast<UINT>(lp);
			Notify(hwnd, kd.hdr, LVN_KEYDOWN);
			if((s = Control<ListState>(hwnd)) == nullptr)
				return 0;
			const bool single = (StyleOf(hwnd) & LVS_SINGLESEL) != 0;
			const bool shift = ::GetKeyState(VK_SHIFT) < 0 && !single;
			const bool ctrl = ::GetKeyState(VK_CONTROL) < 0;
			switch(wp)
			{
			case VK_UP:
			case VK_DOWN:
			case VK_HOME:
			case VK_END:
			case VK_PRIOR:
			case VK_NEXT:
				{
					const int target = NavigateTarget(hwnd, *s, static_cast<UINT>(wp));
					if(target < 0)
						break;
					if(shift)
						SelectRange(hwnd, Valid(hwnd, *s, s->mark) ? s->mark : target, target, ctrl);
					else if(ctrl && !single)
						ChangeState(hwnd, target, LVIS_FOCUSED, LVIS_FOCUSED);
					else
					{
						SelectOnly(hwnd, target);
						if((s = Control<ListState>(hwnd)) != nullptr)
							s->mark = target;
					}
					if((s = Control<ListState>(hwnd)) != nullptr)
						EnsureVisible(hwnd, *s, target, false);
				}
				break;
			case VK_SPACE:
				if(Valid(hwnd, *s, s->focus))
				{
					const int item = s->focus;
					if(s->exStyle & LVS_EX_CHECKBOXES)
					{
						ToggleCheck(hwnd, item);
						if((s = Control<ListState>(hwnd)) != nullptr && !single)
						{
							// The other selected items follow the focused one.
							const UINT checked = ItemState(hwnd, *s, item) & LVIS_STATEIMAGEMASK;
							for(int i = 0; i < Count(hwnd, *s); i++)
							{
								if(i != item && (ItemState(hwnd, *s, i) & LVIS_SELECTED))
									ChangeState(hwnd, i, checked, LVIS_STATEIMAGEMASK);
								if((s = Control<ListState>(hwnd)) == nullptr)
									break;
							}
						}
					} else if(ctrl && !single)
					{
						const bool selected = (ItemState(hwnd, *s, item) & LVIS_SELECTED) != 0;
						ChangeState(hwnd, item, selected ? 0 : LVIS_SELECTED, LVIS_SELECTED);
					} else
					{
						ChangeState(hwnd, item, LVIS_SELECTED, LVIS_SELECTED);
					}
				}
				break;
			case VK_RETURN:
				NotifySimple(hwnd, NM_RETURN);
				if((s = Control<ListState>(hwnd)) != nullptr && Valid(hwnd, *s, s->focus))
					NotifyActivate(hwnd, LVN_ITEMACTIVATE, s->focus, 0, POINT{});
				break;
			case VK_F2:
				if((StyleOf(hwnd) & LVS_EDITLABELS) && Valid(hwnd, *s, s->focus))
					BeginEdit(hwnd, s->focus);
				break;
			default:
				return ::DefWindowProc(hwnd, msg, wp, lp);
			}
		}
		return 0;
	case WM_CHAR:
		if(s != nullptr && wp >= 0x20 && wp != 0x7F && ::GetKeyState(VK_CONTROL) >= 0)
			IncrementalSearch(hwnd, static_cast<WCHAR>(wp));
		return 0;

	case LVM_GETITEMCOUNT:
		return s ? Count(hwnd, *s) : 0;
	case LVM_SETITEMCOUNT:
		if(s != nullptr && OwnerData(hwnd))
		{
			const int count = std::max(0, static_cast<int>(wp));
			s->ownerCount = count;
			s->ownerState.resize(static_cast<std::size_t>(count), 0);
			if(s->focus >= count)
				s->focus = -1;
			if(s->mark >= count)
				s->mark = -1;
			if(!(lp & LVSICF_NOSCROLL))
				s->scrollY = std::min(s->scrollY, std::max(0, count * s->rowHeight - ViewHeight(hwnd, *s)));
			Relayout(hwnd, *s);
		}
		return TRUE;
	case LVM_INSERTCOLUMNW:
		if(s != nullptr && lp != 0)
		{
			const auto *in = reinterpret_cast<const LVCOLUMN *>(lp);
			const int index = std::clamp(static_cast<int>(wp), 0, static_cast<int>(s->columns.size()));
			LvColumn col;
			if(in->mask & LVCF_FMT)
				col.fmt = in->fmt;
			if(in->mask & LVCF_TEXT)
				col.text = in->pszText != nullptr ? std::wstring(in->pszText) : std::wstring();
			col.subItem = (in->mask & LVCF_SUBITEM) ? in->iSubItem : index;
			if(in->mask & LVCF_IMAGE)
				col.image = in->iImage;
			col.width = (in->mask & LVCF_WIDTH) ? std::max(0, in->cx) : 0;
			s->columns.insert(s->columns.begin() + index, col);
			if(s->header != nullptr)
			{
				HDITEM hdi{};
				hdi.mask = HDI_TEXT | HDI_FORMAT | HDI_WIDTH;
				hdi.pszText = s->columns[static_cast<std::size_t>(index)].text.data();
				hdi.fmt = HDF_STRING | ((col.fmt & LVCFMT_JUSTIFYMASK) == LVCFMT_RIGHT ? HDF_RIGHT : (col.fmt & LVCFMT_JUSTIFYMASK) == LVCFMT_CENTER ? HDF_CENTER : HDF_LEFT);
				hdi.cxy = col.width;
				::SendMessage(s->header, HDM_INSERTITEM, static_cast<WPARAM>(index), reinterpret_cast<LPARAM>(&hdi));
			}
			if((s = Control<ListState>(hwnd)) != nullptr)
			{
				UpdateScrollBars(hwnd, *s);
				::InvalidateRect(hwnd, nullptr, TRUE);
			}
			return index;
		}
		return -1;
	case LVM_DELETECOLUMN:
		if(s != nullptr && static_cast<std::size_t>(wp) < s->columns.size())
		{
			s->columns.erase(s->columns.begin() + static_cast<std::ptrdiff_t>(wp));
			if(s->header != nullptr)
				::SendMessage(s->header, HDM_DELETEITEM, wp, 0);
			if((s = Control<ListState>(hwnd)) != nullptr)
			{
				UpdateScrollBars(hwnd, *s);
				::InvalidateRect(hwnd, nullptr, TRUE);
			}
			return TRUE;
		}
		return FALSE;
	case LVM_GETCOLUMNW:
		if(s != nullptr && lp != 0 && static_cast<std::size_t>(wp) < s->columns.size())
		{
			auto *out = reinterpret_cast<LVCOLUMN *>(lp);
			const LvColumn &col = s->columns[wp];
			if(out->mask & LVCF_FMT)
				out->fmt = col.fmt;
			if(out->mask & LVCF_WIDTH)
				out->cx = col.width;
			if((out->mask & LVCF_TEXT) && out->pszText != nullptr && out->cchTextMax > 0)
			{
				const std::size_t n = std::min(col.text.size(), static_cast<std::size_t>(out->cchTextMax - 1));
				std::copy_n(col.text.c_str(), n, out->pszText);
				out->pszText[n] = 0;
			}
			if(out->mask & LVCF_SUBITEM)
				out->iSubItem = col.subItem;
			if(out->mask & LVCF_IMAGE)
				out->iImage = col.image;
			if(out->mask & LVCF_ORDER)
				out->iOrder = static_cast<int>(wp);
			return TRUE;
		}
		return FALSE;
	case LVM_SETCOLUMNW:
		if(s != nullptr && lp != 0 && static_cast<std::size_t>(wp) < s->columns.size())
		{
			const auto *in = reinterpret_cast<const LVCOLUMN *>(lp);
			LvColumn &col = s->columns[wp];
			HDITEM hdi{};
			if(in->mask & LVCF_FMT)
			{
				col.fmt = in->fmt;
				hdi.mask |= HDI_FORMAT;
				hdi.fmt = HDF_STRING | ((col.fmt & LVCFMT_JUSTIFYMASK) == LVCFMT_RIGHT ? HDF_RIGHT : (col.fmt & LVCFMT_JUSTIFYMASK) == LVCFMT_CENTER ? HDF_CENTER : HDF_LEFT);
			}
			if(in->mask & LVCF_TEXT)
			{
				col.text = in->pszText != nullptr ? std::wstring(in->pszText) : std::wstring();
				hdi.mask |= HDI_TEXT;
				hdi.pszText = col.text.data();
			}
			if(in->mask & LVCF_SUBITEM)
				col.subItem = in->iSubItem;
			if(in->mask & LVCF_IMAGE)
				col.image = in->iImage;
			if(in->mask & LVCF_WIDTH)
			{
				col.width = std::max(0, in->cx);
				hdi.mask |= HDI_WIDTH;
				hdi.cxy = col.width;
			}
			if(s->header != nullptr && hdi.mask != 0)
				::SendMessage(s->header, HDM_SETITEM, wp, reinterpret_cast<LPARAM>(&hdi));
			if((s = Control<ListState>(hwnd)) != nullptr)
			{
				UpdateScrollBars(hwnd, *s);
				::InvalidateRect(hwnd, nullptr, TRUE);
			}
			return TRUE;
		}
		return FALSE;
	case LVM_GETCOLUMNWIDTH:
		if(s != nullptr)
		{
			if(!Report(hwnd))
				return static_cast<LRESULT>(Client(hwnd).right);
			return static_cast<std::size_t>(wp) < s->columns.size() ? s->columns[wp].width : 0;
		}
		return 0;
	case LVM_SETCOLUMNWIDTH:
		if(s != nullptr && static_cast<std::size_t>(wp) < s->columns.size())
		{
			int width = static_cast<short>(LOWORD(lp));
			if(width == LVSCW_AUTOSIZE)
				width = AutoWidth(hwnd, *s, static_cast<int>(wp), false);
			else if(width == LVSCW_AUTOSIZE_USEHEADER)
				width = AutoWidth(hwnd, *s, static_cast<int>(wp), true);
			if((s = Control<ListState>(hwnd)) != nullptr)
				SetColumnWidth(hwnd, *s, static_cast<int>(wp), width);
			return TRUE;
		}
		return FALSE;
	case LVM_GETHEADER:
		return s ? reinterpret_cast<LRESULT>(s->header) : 0;
	case LVM_GETCOLUMNORDERARRAY:
	case LVM_SETCOLUMNORDERARRAY:
		if(s != nullptr && s->header != nullptr)
			return ::SendMessage(s->header, msg == LVM_GETCOLUMNORDERARRAY ? HDM_GETORDERARRAY : HDM_SETORDERARRAY, wp, lp);
		return FALSE;
	case LVM_INSERTITEMW:
		return s ? InsertItem(hwnd, *s, reinterpret_cast<const LVITEM *>(lp)) : -1;
	case LVM_SETITEMW:
		return SetItem(hwnd, reinterpret_cast<const LVITEM *>(lp)) ? TRUE : FALSE;
	case LVM_GETITEMW:
		return GetItem(hwnd, reinterpret_cast<LVITEM *>(lp)) ? TRUE : FALSE;
	case LVM_SETITEMTEXTW:
		if(lp != 0)
		{
			LVITEM copy = *reinterpret_cast<const LVITEM *>(lp);
			copy.iItem = static_cast<int>(wp);
			copy.mask = LVIF_TEXT;
			return SetItem(hwnd, &copy) ? TRUE : FALSE;
		}
		return FALSE;
	case LVM_GETITEMTEXTW:
		if(s != nullptr && lp != 0 && Valid(hwnd, *s, static_cast<int>(wp)))
		{
			auto *out = reinterpret_cast<LVITEM *>(lp);
			const std::wstring text = ItemText(hwnd, static_cast<int>(wp), out->iSubItem);
			CopyText(text, out);
			return static_cast<LRESULT>(std::min(text.size(), static_cast<std::size_t>(std::max(0, out->cchTextMax - 1))));
		}
		return 0;
	case LVM_DELETEITEM:
		return DeleteItem(hwnd, static_cast<int>(wp)) ? TRUE : FALSE;
	case LVM_DELETEALLITEMS:
		DeleteAll(hwnd);
		return TRUE;
	case LVM_SETITEMSTATE:
		if(s != nullptr && lp != 0)
		{
			const auto *in = reinterpret_cast<const LVITEM *>(lp);
			const int item = static_cast<int>(wp);
			if(item == -1)
			{
				if(OwnerData(hwnd))
				{
					// One notification for the whole list.
					const int count = Count(hwnd, *s);
					UINT oldState = 0;
					for(int i = 0; i < count; i++)
					{
						oldState |= s->ownerState[static_cast<std::size_t>(i)] & in->stateMask;
						UINT &st = s->ownerState[static_cast<std::size_t>(i)];
						st = (st & ~in->stateMask) | (in->state & in->stateMask);
					}
					if(in->stateMask & LVIS_FOCUSED)
						s->focus = (in->state & LVIS_FOCUSED) ? -1 : -1;
					NMLISTVIEW nm{};
					nm.iItem = -1;
					nm.uChanged = LVIF_STATE;
					nm.uNewState = in->state & in->stateMask;
					nm.uOldState = oldState;
					::InvalidateRect(hwnd, nullptr, TRUE);
					Notify(hwnd, nm.hdr, LVN_ITEMCHANGED);
				} else
				{
					for(int i = 0; i < Count(hwnd, *s); i++)
					{
						ChangeState(hwnd, i, in->state, in->stateMask);
						if((s = Control<ListState>(hwnd)) == nullptr)
							break;
					}
				}
				return TRUE;
			}
			return ChangeState(hwnd, item, in->state, in->stateMask) ? TRUE : FALSE;
		}
		return FALSE;
	case LVM_GETITEMSTATE:
		return s ? (ItemState(hwnd, *s, static_cast<int>(wp)) & static_cast<UINT>(lp)) : 0;
	case LVM_GETNEXTITEM:
		if(s != nullptr)
		{
			const UINT flags = LOWORD(lp);
			const int count = Count(hwnd, *s);
			const int start = static_cast<int>(wp);
			auto matches = [&](int i)
			{
				const UINT st = ItemState(hwnd, *s, i);
				if((flags & LVNI_SELECTED) && !(st & LVIS_SELECTED))
					return false;
				if((flags & LVNI_FOCUSED) && !(st & LVIS_FOCUSED))
					return false;
				if((flags & LVNI_CUT) && !(st & LVIS_CUT))
					return false;
				if((flags & LVNI_DROPHILITED) && !(st & LVIS_DROPHILITED))
					return false;
				return true;
			};
			if(flags & (LVNI_ABOVE | LVNI_BELOW))
			{
				const int r = RowOfItem(*s, start);
				const int step = (flags & LVNI_ABOVE) ? -1 : 1;
				for(int k = r + step; r >= 0 && k >= 0 && k < static_cast<int>(s->rows.size()); k += step)
				{
					const Row &row = s->rows[static_cast<std::size_t>(k)];
					if(!row.group && matches(row.index))
						return row.index;
				}
				return -1;
			}
			for(int i = start + 1; i < count; i++)
			{
				if(matches(i))
					return i;
			}
			return -1;
		}
		return -1;
	case LVM_GETSELECTEDCOUNT:
		if(s != nullptr)
		{
			int n = 0;
			for(int i = 0; i < Count(hwnd, *s); i++)
			{
				if(ItemState(hwnd, *s, i) & LVIS_SELECTED)
					n++;
			}
			return n;
		}
		return 0;
	case LVM_GETSELECTIONMARK:
		return s ? s->mark : -1;
	case LVM_SETSELECTIONMARK:
		if(s != nullptr)
		{
			const int old = s->mark;
			s->mark = static_cast<int>(lp);
			return old;
		}
		return -1;
	case LVM_ENSUREVISIBLE:
		if(s != nullptr && Valid(hwnd, *s, static_cast<int>(wp)))
		{
			if(s->layoutPending)
			{
				s->layoutPending = false;
				BuildRows(hwnd, *s);
				UpdateScrollBars(hwnd, *s);
			}
			EnsureVisible(hwnd, *s, static_cast<int>(wp), LOWORD(lp) != 0);
			return TRUE;
		}
		return FALSE;
	case LVM_ISITEMVISIBLE:
		if(s != nullptr)
		{
			const int r = RowOfItem(*s, static_cast<int>(wp));
			if(r < 0)
				return FALSE;
			const QRect row = PartsOf(hwnd, *s, r).row;
			return row.bottom() >= ContentTop(*s) && row.top() < Client(hwnd).bottom;
		}
		return FALSE;
	case LVM_GETEXTENDEDLISTVIEWSTYLE:
		return s ? s->exStyle : 0;
	case LVM_SETEXTENDEDLISTVIEWSTYLE:
		if(s != nullptr)
		{
			const DWORD old = s->exStyle;
			const DWORD mask = wp != 0 ? static_cast<DWORD>(wp) : 0xFFFFFFFFu;
			s->exStyle = (s->exStyle & ~mask) | (static_cast<DWORD>(lp) & mask);
			if((s->exStyle & LVS_EX_CHECKBOXES) && !(old & LVS_EX_CHECKBOXES))
			{
				// Every item gets an unchecked box.
				for(LvItem &item : s->items)
					item.state = (item.state & ~LVIS_STATEIMAGEMASK) | INDEXTOSTATEIMAGEMASK(1);
			} else if(!(s->exStyle & LVS_EX_CHECKBOXES) && (old & LVS_EX_CHECKBOXES))
			{
				for(LvItem &item : s->items)
					item.state &= ~LVIS_STATEIMAGEMASK;
			}
			Measure(hwnd, *s);
			Relayout(hwnd, *s);
			return old;
		}
		return 0;
	case LVM_SETIMAGELIST:
		if(s != nullptr && wp <= LVSIL_STATE)
		{
			const HIMAGELIST old = s->images[wp];
			s->images[wp] = reinterpret_cast<HIMAGELIST>(lp);
			Measure(hwnd, *s);
			Relayout(hwnd, *s);
			return reinterpret_cast<LRESULT>(old);
		}
		return 0;
	case LVM_GETIMAGELIST:
		return (s != nullptr && wp <= LVSIL_STATE) ? reinterpret_cast<LRESULT>(s->images[wp]) : 0;
	case LVM_GETTOPINDEX:
		if(s != nullptr)
		{
			const int r = RowAt(*s, ContentTop(*s));
			for(int k = std::max(0, r); k < static_cast<int>(s->rows.size()); k++)
			{
				if(!s->rows[static_cast<std::size_t>(k)].group)
					return s->rows[static_cast<std::size_t>(k)].index;
			}
		}
		return 0;
	case LVM_GETCOUNTPERPAGE:
		return s ? std::max(0, ViewHeight(hwnd, *s) / std::max(1, s->rowHeight)) : 0;
	case LVM_GETITEMRECT:
		if(s != nullptr && lp != 0)
		{
			auto *r = reinterpret_cast<RECT *>(lp);
			return GetItemRect(hwnd, *s, static_cast<int>(wp), 0, r->left, r) ? TRUE : FALSE;
		}
		return FALSE;
	case LVM_GETSUBITEMRECT:
		if(s != nullptr && lp != 0)
		{
			auto *r = reinterpret_cast<RECT *>(lp);
			return GetItemRect(hwnd, *s, static_cast<int>(wp), r->top, r->left, r) ? TRUE : FALSE;
		}
		return FALSE;
	case LVM_HITTEST:
	case LVM_SUBITEMHITTEST:
		if(s != nullptr && lp != 0)
		{
			auto *info = reinterpret_cast<LVHITTESTINFO *>(lp);
			int sub = -1, group = -1;
			int item = HitTest(hwnd, *s, info->pt, &info->flags, &sub, &group);
			if(msg == LVM_HITTEST && !(info->flags & LVHT_ONITEM))
				item = -1;
			info->iItem = item;
			info->iSubItem = msg == LVM_SUBITEMHITTEST ? std::max(0, sub) : 0;
			info->iGroup = group;
			return item;
		}
		return -1;
	case LVM_ENABLEGROUPVIEW:
		if(s != nullptr)
		{
			if(OwnerData(hwnd))
				return -1;
			const bool enable = wp != 0;
			if(enable == s->groupView)
				return 0;
			s->groupView = enable;
			Relayout(hwnd, *s);
			return 1;
		}
		return -1;
	case LVM_ISGROUPVIEWENABLED:
		return s ? s->groupView : FALSE;
	case LVM_INSERTGROUP:
		if(s != nullptr && lp != 0)
		{
			const auto *in = reinterpret_cast<const LVGROUP *>(lp);
			if(!(in->mask & LVGF_GROUPID) || GroupIndex(*s, in->iGroupId) >= 0)
				return -1;
			LvGroup g;
			g.id = in->iGroupId;
			if((in->mask & LVGF_HEADER) && in->pszHeader != nullptr)
				g.header = in->pszHeader;
			if(in->mask & LVGF_STATE)
				g.state = in->state & in->stateMask;
			if(in->mask & LVGF_ALIGN)
				g.align = in->uAlign;
			const int index = (static_cast<int>(wp) < 0 || static_cast<std::size_t>(wp) > s->groups.size()) ? static_cast<int>(s->groups.size()) : static_cast<int>(wp);
			s->groups.insert(s->groups.begin() + index, g);
			Relayout(hwnd, *s);
			return index;
		}
		return -1;
	case LVM_REMOVEGROUP:
		if(s != nullptr)
		{
			const int g = GroupIndex(*s, static_cast<int>(wp));
			if(g < 0)
				return -1;
			s->groups.erase(s->groups.begin() + g);
			for(LvItem &item : s->items)
			{
				if(item.groupId == static_cast<int>(wp))
					item.groupId = I_GroupIdNone;
			}
			Relayout(hwnd, *s);
			return g;
		}
		return -1;
	case LVM_REMOVEALLGROUPS:
		if(s != nullptr)
		{
			s->groups.clear();
			for(LvItem &item : s->items)
				item.groupId = I_GroupIdNone;
			Relayout(hwnd, *s);
		}
		return 0;
	case LVM_HASGROUP:
		return s ? (GroupIndex(*s, static_cast<int>(wp)) >= 0) : FALSE;
	case LVM_GETGROUPCOUNT:
		return s ? static_cast<LRESULT>(s->groups.size()) : 0;
	case LVM_SETGROUPINFO:
	case LVM_GETGROUPINFO:
	case LVM_GETGROUPINFOBYINDEX:
		if(s != nullptr && lp != 0)
		{
			const int g = msg == LVM_GETGROUPINFOBYINDEX ? (static_cast<std::size_t>(wp) < s->groups.size() ? static_cast<int>(wp) : -1) : GroupIndex(*s, static_cast<int>(wp));
			if(g < 0)
				return msg == LVM_GETGROUPINFOBYINDEX ? FALSE : -1;
			LvGroup &group = s->groups[static_cast<std::size_t>(g)];
			auto *info = reinterpret_cast<LVGROUP *>(lp);
			if(msg == LVM_SETGROUPINFO)
			{
				if((info->mask & LVGF_HEADER) && info->pszHeader != nullptr)
					group.header = info->pszHeader;
				if(info->mask & LVGF_STATE)
					group.state = (group.state & ~info->stateMask) | (info->state & info->stateMask);
				if(info->mask & LVGF_ALIGN)
					group.align = info->uAlign;
				if(info->mask & LVGF_GROUPID)
					group.id = info->iGroupId;
				Relayout(hwnd, *s);
				return group.id;
			}
			if(info->mask & LVGF_GROUPID)
				info->iGroupId = group.id;
			if(info->mask & LVGF_STATE)
				info->state = group.state & info->stateMask;
			if(info->mask & LVGF_ALIGN)
				info->uAlign = group.align;
			if((info->mask & LVGF_HEADER) && info->pszHeader != nullptr && info->cchHeader > 0)
			{
				const std::size_t n = std::min(group.header.size(), static_cast<std::size_t>(info->cchHeader - 1));
				std::copy_n(group.header.c_str(), n, info->pszHeader);
				info->pszHeader[n] = 0;
			}
			return msg == LVM_GETGROUPINFOBYINDEX ? TRUE : group.id;
		}
		return -1;
	case LVM_GETGROUPSTATE:
		if(s != nullptr)
		{
			const int g = GroupIndex(*s, static_cast<int>(wp));
			return g >= 0 ? (s->groups[static_cast<std::size_t>(g)].state & static_cast<UINT>(lp)) : 0;
		}
		return 0;
	case LVM_SORTITEMS:
	case LVM_SORTITEMSEX:
		return SortItems(hwnd, reinterpret_cast<PFNLVCOMPARE>(lp), static_cast<LPARAM>(wp), msg == LVM_SORTITEMSEX) ? TRUE : FALSE;
	case LVM_EDITLABELW:
		if(static_cast<int>(wp) < 0)
		{
			EndEdit(hwnd, false);
			return 0;
		}
		return reinterpret_cast<LRESULT>(BeginEdit(hwnd, static_cast<int>(wp)));
	case LVM_GETEDITCONTROL:
		return s ? reinterpret_cast<LRESULT>(s->edit) : 0;
	case LVM_REDRAWITEMS:
	case LVM_UPDATE:
		::InvalidateRect(hwnd, nullptr, TRUE);
		return TRUE;
	case LVM_ARRANGE:
		return TRUE;
	case LVM_GETBKCOLOR:
		return s ? Resolve(s->bk, COLOR_WINDOW) : 0;
	case LVM_SETBKCOLOR:
		if(s != nullptr)
		{
			s->bk = static_cast<COLORREF>(lp);
			::InvalidateRect(hwnd, nullptr, TRUE);
		}
		return TRUE;
	case LVM_GETTEXTCOLOR:
		return s ? Resolve(s->text, COLOR_WINDOWTEXT) : 0;
	case LVM_SETTEXTCOLOR:
		if(s != nullptr)
		{
			s->text = static_cast<COLORREF>(lp);
			::InvalidateRect(hwnd, nullptr, TRUE);
		}
		return TRUE;
	case LVM_GETTEXTBKCOLOR:
		return s ? (s->textBk == CLR_DEFAULT ? Resolve(s->bk, COLOR_WINDOW) : s->textBk) : 0;
	case LVM_SETTEXTBKCOLOR:
		if(s != nullptr)
		{
			s->textBk = static_cast<COLORREF>(lp);
			::InvalidateRect(hwnd, nullptr, TRUE);
		}
		return TRUE;
	case LVM_GETVIEWRECT:
		if(s != nullptr && lp != 0)
		{
			*reinterpret_cast<RECT *>(lp) = RECT{-s->scrollX, -s->scrollY, TotalWidth(hwnd, *s) - s->scrollX, s->contentHeight - s->scrollY};
			return TRUE;
		}
		return FALSE;
	case LVM_APPROXIMATEVIEWRECT:
		if(s != nullptr)
		{
			const int count = static_cast<int>(wp) < 0 ? Count(hwnd, *s) : static_cast<int>(wp);
			return MAKELONG(TotalWidth(hwnd, *s), s->headerHeight + count * s->rowHeight);
		}
		return 0;
	case LVM_GETORIGIN:
		if(s != nullptr && lp != 0)
		{
			*reinterpret_cast<POINT *>(lp) = POINT{s->scrollX, s->scrollY};
			return Report(hwnd) ? FALSE : TRUE;
		}
		return FALSE;
	case LVM_SCROLL:
		if(s != nullptr)
		{
			int dy = static_cast<int>(lp);
			if(Report(hwnd) && !s->groupView && s->rowHeight > 0 && dy != 0)
				dy = (dy > 0 ? (dy + s->rowHeight - 1) : (dy - s->rowHeight + 1)) / s->rowHeight * s->rowHeight;
			Scroll(hwnd, *s, static_cast<int>(wp), dy);
			return TRUE;
		}
		return FALSE;
	case LVM_GETSTRINGWIDTHW:
		if(lp != 0)
		{
			HDC dc = ::GetDC(hwnd);
			HGDIOBJ old = ::SelectObject(dc, FontOfControl(hwnd));
			SIZE size{};
			const auto *text = reinterpret_cast<LPCWSTR>(lp);
			::GetTextExtentPoint32(dc, text, static_cast<int>(std::wcslen(text)), &size);
			::SelectObject(dc, old);
			::ReleaseDC(hwnd, dc);
			return size.cx;
		}
		return 0;
	case LVM_FINDITEMW:
		if(s != nullptr && lp != 0)
		{
			const auto *find = reinterpret_cast<const LVFINDINFO *>(lp);
			const int count = Count(hwnd, *s);
			const int start = static_cast<int>(wp) + 1;
			for(int k = 0; k < count; k++)
			{
				const int i = start + k;
				if(i >= count && !(find->flags & LVFI_WRAP))
					break;
				const int index = i % count;
				if(find->flags & LVFI_PARAM)
				{
					if(ItemParam(hwnd, *s, index) == find->lParam)
						return index;
				} else if((find->flags & (LVFI_STRING | LVFI_PARTIAL | LVFI_SUBSTRING)) && find->psz != nullptr)
				{
					const QString text = ToQString(ItemText(hwnd, index, 0).c_str());
					const QString needle = ToQString(find->psz);
					const bool partial = (find->flags & (LVFI_PARTIAL | LVFI_SUBSTRING)) != 0;
					if(partial ? text.startsWith(needle, Qt::CaseInsensitive) : text.compare(needle, Qt::CaseInsensitive) == 0)
						return index;
				}
				if((s = Control<ListState>(hwnd)) == nullptr)
					return -1;
			}
		}
		return -1;
	case LVM_SETVIEW:
		if(wp <= LV_VIEW_LIST)
		{
			::SetWindowLongPtr(hwnd, GWL_STYLE, (StyleOf(hwnd) & ~LVS_TYPEMASK) | static_cast<LONG_PTR>(wp));
			if((s = Control<ListState>(hwnd)) != nullptr)
				Relayout(hwnd, *s);
			return 1;
		}
		return -1;
	case LVM_GETVIEW:
		return StyleOf(hwnd) & LVS_TYPEMASK;
	case LVM_GETCALLBACKMASK:
		return s ? s->callbackMask : 0;
	case LVM_SETCALLBACKMASK:
		if(s != nullptr)
			s->callbackMask = static_cast<UINT>(wp);
		return TRUE;
	case LVM_GETHOTITEM:
		return s ? s->hot : -1;
	case LVM_SETHOTITEM:
		if(s != nullptr)
		{
			const int old = s->hot;
			s->hot = static_cast<int>(wp);
			return old;
		}
		return -1;
	case LVM_GETITEMSPACING:
		return s ? MAKELONG(16, s->rowHeight) : 0;
	case LVM_GETTOOLTIPS:
	case LVM_SETTOOLTIPS:
		return 0;
	default:
		return ::DefWindowProc(hwnd, msg, wp, lp);
	}
}

} // namespace

void RegisterListViewClass()
{
	WNDCLASS wc{};
	wc.style = CS_DBLCLKS;
	wc.lpfnWndProc = ListProc;
	wc.hCursor = ::LoadCursor(nullptr, IDC_ARROW);
	wc.lpszClassName = WC_LISTVIEW;
	::RegisterClass(&wc);
}

} // namespace linopenmpt::qtwin
