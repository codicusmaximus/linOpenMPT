// The tab control of comctl32 (SysTabControl32): items with text, images and
// application data, single or multiple rows, notifications before and after
// the selection changes, and TCM_ADJUSTRECT for the pages it holds. Laid out
// with comctl32's metrics, drawn with the Qt style's tabs and tab frame.
#include "QtWin.h"

#include <QtGui/QIcon>
#include <QtWidgets/QApplication>
#include <QtWidgets/QStyle>
#include <QtWidgets/QStyleOption>

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

namespace linopenmpt::qtwin
{

namespace
{

// comctl32's metrics.
constexpr int SelectedTabOffset = 2;
constexpr int DisplayAreaPadding = 2;
constexpr int ControlBorder = 2;
constexpr int DefaultMinTabWidth = 54;
constexpr int DefaultFixedTabWidth = 96;
constexpr int DefaultPaddingX = 6;
constexpr int DefaultPaddingY = 3;

struct TabItem
{
	std::wstring text;
	int image = -1;
	DWORD state = 0;
	std::vector<BYTE> extra;
	QRect rect; // before scrolling
	int row = 0;
};

struct TabState : ControlState
{
	std::vector<TabItem> items;
	std::size_t extraBytes = sizeof(LPARAM);
	int sel = -1;
	int focus = -1;
	HIMAGELIST imageList = nullptr;
	int fixedWidth = 0;
	int fixedHeight = 0;
	int padX = DefaultPaddingX;
	int padY = DefaultPaddingY;
	int minTabWidth = -1;
	int tabHeight = 0;
	int rows = 1;
	int firstVisible = 0; // single line, scrolled
	HWND scroller = nullptr;
	bool focused = false;
	DWORD exStyle = 0;
};

RECT ClientRectOf(HWND hwnd)
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
	return RECT{r.left(), r.top(), r.right() + 1, r.bottom() + 1};
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

int Count(const TabState &s)
{
	return static_cast<int>(s.items.size());
}

bool Bottom(HWND hwnd)
{
	return (StyleOf(hwnd) & TCS_BOTTOM) != 0;
}

QSize IconSize(const TabState &s)
{
	int cx = 0, cy = 0;
	if(s.imageList != nullptr)
		ImageList_GetIconSize(s.imageList, &cx, &cy);
	return QSize(cx, cy);
}

// The height of the tab rows (what TCM_ADJUSTRECT takes off).
int RowsHeight(HWND hwnd, const TabState &s)
{
	if(s.items.empty() && !(StyleOf(hwnd) & TCS_BUTTONS))
		return s.tabHeight;
	const int extra = (StyleOf(hwnd) & TCS_BUTTONS) ? 3 * (s.rows - 1) : 0;
	return s.tabHeight * s.rows + extra;
}

void UpdateScroller(HWND hwnd, TabState &s, bool overflow, int clientWidth);

// The pages of a tab control are usually its siblings, created after it,
// which Windows draws over the display area; Qt would stack them under the
// tab control. So the display area inside the frame is cut out of the
// control, unless the control holds its pages as its own children there.
void UpdateShape(HWND hwnd, TabState &s)
{
	Win32Widget *w = FromHandle(hwnd);
	if(w == nullptr)
		return;
	const QRect all(QPoint(0, 0), w->size());
	const int rows = RowsHeight(hwnd, s);
	const bool bottom = Bottom(hwnd);
	QRect pane = all;
	if(bottom)
		pane.setBottom(pane.bottom() - rows);
	else
		pane.setTop(pane.top() + rows);
	const QRect inside = pane.adjusted(ControlBorder, ControlBorder, -ControlBorder, -ControlBorder);
	bool ownPages = false;
	for(QObject *child : w->children())
	{
		auto *page = qobject_cast<QWidget *>(child);
		if(page != nullptr && !page->isWindow() && page->geometry().intersects(inside))
			ownPages = true;
	}
	SetShape(w, ownPages || inside.isEmpty() ? QRegion() : QRegion(all) - QRegion(inside));
}

void Layout(HWND hwnd, TabState &s)
{
	const LONG_PTR style = StyleOf(hwnd);
	const RECT client = ClientRectOf(hwnd);
	const int clientWidth = client.right - client.left;
	HDC dc = ::GetDC(hwnd);
	HGDIOBJ oldFont = ::SelectObject(dc, FontOfControl(hwnd));
	TEXTMETRIC tm{};
	::GetTextMetrics(dc, &tm);
	const QSize icon = IconSize(s);
	if(s.fixedHeight > 0)
		s.tabHeight = s.fixedHeight;
	else if(style & TCS_BUTTONS)
		s.tabHeight = std::max(static_cast<int>(tm.tmHeight), icon.height()) + 2 * s.padY + 1;
	else
		s.tabHeight = std::max(static_cast<int>(tm.tmHeight), icon.height()) + SelectedTabOffset + 2 * s.padY;
	const int minWidth = s.minTabWidth >= 0 ? s.minTabWidth : DefaultMinTabWidth - (DefaultPaddingX - s.padX) * 2;
	const bool multi = (style & TCS_MULTILINE) != 0;
	const int gap = (style & TCS_BUTTONS) ? 3 : 0;
	const int rowWidth = clientWidth - 2 * SelectedTabOffset;

	// Widths, then rows.
	std::vector<int> widths;
	for(const TabItem &item : s.items)
	{
		int w;
		if(style & TCS_FIXEDWIDTH)
		{
			w = s.fixedWidth > 0 ? s.fixedWidth : DefaultFixedTabWidth;
		} else
		{
			SIZE size{};
			RECT calc{0, 0, 0, 0};
			::DrawText(dc, item.text.c_str(), -1, &calc, DT_CALCRECT | DT_SINGLELINE);
			size.cx = calc.right;
			const int iconWidth = (item.image >= 0 && icon.width() > 0) ? icon.width() + 3 : 0;
			w = std::max(static_cast<int>(size.cx) + iconWidth + 2 * s.padX, minWidth);
			// Wide enough for the style's margins around the label.
			QStyleOptionTab opt;
			opt.text = ToQString(item.text.c_str());
			opt.fontMetrics = QFontMetrics(FontOf(FontOfControl(hwnd)));
			opt.shape = QTabBar::RoundedNorth;
			if(iconWidth > 0)
				opt.iconSize = icon;
			const int textWidth = opt.fontMetrics.horizontalAdvance(opt.text);
			// As QTabBar's size hint: the label and the style's spacing.
			QStyle *qs = QApplication::style();
			const int hspace = qs->pixelMetric(QStyle::PM_TabBarTabHSpace, &opt, nullptr);
			const QSize styled = qs->sizeFromContents(QStyle::CT_TabBarTab, &opt, QSize(textWidth + (iconWidth > 0 ? icon.width() + 4 : 0) + hspace, s.tabHeight), nullptr);
			w = std::max(w, styled.width());
		}
		widths.push_back(w);
	}
	::SelectObject(dc, oldFont);
	::ReleaseDC(hwnd, dc);

	std::vector<std::vector<int>> rows(1);
	int x = 0;
	for(int i = 0; i < Count(s); i++)
	{
		if(multi && x > 0 && x + widths[i] > rowWidth)
		{
			rows.emplace_back();
			x = 0;
		}
		rows.back().push_back(i);
		x += widths[i] + gap;
	}
	s.rows = multi ? static_cast<int>(rows.size()) : 1;

	// Multiple rows fill the width unless TCS_RAGGEDRIGHT; the row of the
	// selected tab is next to the display area.
	if(multi && rows.size() > 1 && s.sel >= 0)
	{
		const auto selRow = std::find_if(rows.begin(), rows.end(), [&](const std::vector<int> &r) { return std::find(r.begin(), r.end(), s.sel) != r.end(); });
		if(selRow != rows.end())
		{
			const std::size_t n = static_cast<std::size_t>(selRow - rows.begin());
			std::rotate(rows.begin(), rows.begin() + static_cast<std::ptrdiff_t>((n + 1) % rows.size()), rows.end());
		}
	}
	const bool bottom = (style & TCS_BOTTOM) != 0;
	const int clientHeight = client.bottom - client.top;
	for(std::size_t r = 0; r < rows.size(); r++)
	{
		const std::vector<int> &row = rows[r];
		int total = 0;
		for(int i : row)
			total += widths[i] + gap;
		const bool stretch = multi && rows.size() > 1 && !(style & TCS_RAGGEDRIGHT) && !(style & TCS_FIXEDWIDTH) && total < rowWidth && !row.empty();
		const int extra = stretch ? (rowWidth - total) : 0;
		int left = SelectedTabOffset;
		// Row 0 is farthest from the display area.
		const int rowIndex = static_cast<int>(r);
		const int top = bottom ? clientHeight - (static_cast<int>(rows.size()) - rowIndex) * s.tabHeight : rowIndex * s.tabHeight;
		for(std::size_t k = 0; k < row.size(); k++)
		{
			const int i = row[k];
			int w = widths[i];
			if(stretch)
				w += extra / static_cast<int>(row.size()) + (static_cast<int>(k) < extra % static_cast<int>(row.size()) ? 1 : 0);
			s.items[i].rect = QRect(left, top, w, s.tabHeight);
			s.items[i].row = rowIndex;
			left += w + gap;
		}
	}

	// A single row wider than the control scrolls with an up-down control.
	int full = 0;
	for(int w : widths)
		full += w + gap;
	UpdateScroller(hwnd, s, !multi && Count(s) > 0 && full + 2 * SelectedTabOffset > clientWidth, clientWidth);
	UpdateShape(hwnd, s);
}

int ScrollOffset(const TabState &s)
{
	if(s.firstVisible <= 0 || s.firstVisible >= Count(s))
		return 0;
	return s.items[static_cast<std::size_t>(s.firstVisible)].rect.left() - SelectedTabOffset;
}

QRect VisibleRect(const TabState &s, int index)
{
	return s.items[static_cast<std::size_t>(index)].rect.translated(-ScrollOffset(s), 0);
}

void UpdateScroller(HWND hwnd, TabState &s, bool overflow, int clientWidth)
{
	if(!overflow)
	{
		s.firstVisible = 0;
		if(s.scroller != nullptr)
		{
			::DestroyWindow(s.scroller);
			s.scroller = nullptr;
		}
		return;
	}
	const int width = 2 * ::GetSystemMetrics(SM_CXHSCROLL);
	const int height = std::min(s.tabHeight - SelectedTabOffset, ::GetSystemMetrics(SM_CXHSCROLL));
	const int top = Bottom(hwnd) ? ClientRectOf(hwnd).bottom - s.tabHeight + SelectedTabOffset : s.tabHeight - SelectedTabOffset - height;
	if(s.scroller == nullptr)
	{
		s.scroller = ::CreateWindowEx(0, UPDOWN_CLASS, nullptr, WS_CHILD | WS_VISIBLE | UDS_HORZ, clientWidth - width, top, width, height, hwnd, nullptr, nullptr, nullptr);
	} else
	{
		::SetWindowPos(s.scroller, nullptr, clientWidth - width, top, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
	}
	::SendMessage(s.scroller, UDM_SETRANGE32, 0, std::max(0, Count(s) - 1));
	s.firstVisible = std::clamp(s.firstVisible, 0, std::max(0, Count(s) - 1));
	::SendMessage(s.scroller, UDM_SETPOS32, 0, s.firstVisible);
}

int HitTest(HWND hwnd, const TabState &s, QPoint pt, UINT *flags = nullptr)
{
	for(int i = 0; i < Count(s); i++)
	{
		const QRect r = VisibleRect(s, i);
		if(s.scroller != nullptr)
		{
			RECT sr{};
			::GetWindowRect(s.scroller, &sr);
			POINT tl{sr.left, sr.top};
			::ScreenToClient(hwnd, &tl);
			if(pt.x() >= tl.x)
				break;
		}
		if(r.contains(pt))
		{
			if(flags != nullptr)
			{
				const QSize icon = IconSize(s);
				const bool onIcon = s.items[static_cast<std::size_t>(i)].image >= 0 && icon.width() > 0 && pt.x() < r.left() + s.padX + icon.width();
				*flags = onIcon ? TCHT_ONITEMICON : TCHT_ONITEMLABEL;
			}
			return i;
		}
	}
	if(flags != nullptr)
		*flags = TCHT_NOWHERE;
	return -1;
}

// Selects an item as the user does: TCN_SELCHANGING may refuse it.
bool UserSelect(HWND hwnd, int index)
{
	TabState *s = Control<TabState>(hwnd);
	if(s == nullptr || index < 0 || index >= Count(*s) || index == s->sel)
		return false;
	if(NotifySimple(hwnd, TCN_SELCHANGING) != 0)
		return false;
	if((s = Control<TabState>(hwnd)) == nullptr || index >= Count(*s))
		return false;
	s->sel = index;
	s->focus = index;
	Layout(hwnd, *s);
	::InvalidateRect(hwnd, nullptr, TRUE);
	NotifySimple(hwnd, TCN_SELCHANGE);
	return true;
}

// Scrolls a single row so the item is visible.
void EnsureVisible(HWND hwnd, TabState &s, int index)
{
	if(s.scroller == nullptr || index < 0 || index >= Count(s))
		return;
	const int limit = ClientRectOf(hwnd).right - 2 * ::GetSystemMetrics(SM_CXHSCROLL);
	if(index < s.firstVisible)
		s.firstVisible = index;
	while(s.firstVisible < index && VisibleRect(s, index).right() >= limit)
		s.firstVisible++;
	::SendMessage(s.scroller, UDM_SETPOS32, 0, s.firstVisible);
}

QIcon ItemIcon(const TabState &s, const TabItem &item)
{
	if(s.imageList == nullptr || item.image < 0)
		return QIcon();
	const QImage image = ImageListImage(s.imageList, item.image);
	return image.isNull() ? QIcon() : QIcon(QPixmap::fromImage(image));
}

void PaintTabs(HWND hwnd, HDC dc)
{
	TabState *s = Control<TabState>(hwnd);
	Win32Widget *w = FromHandle(hwnd);
	if(s == nullptr || w == nullptr)
		return;
	const LONG_PTR style = StyleOf(hwnd);
	const RECT rc = ClientRectOf(hwnd);
	::FillRect(dc, &rc, ::GetSysColorBrush(COLOR_BTNFACE));
	QPainter *p = DCPainter(dc);
	if(p == nullptr)
		return;
	QStyle *qs = QApplication::style();
	const bool enabled = ::IsWindowEnabled(hwnd) != FALSE;
	const bool bottom = (style & TCS_BOTTOM) != 0;
	const bool buttons = (style & TCS_BUTTONS) != 0;
	const QFont font = FontOf(FontOfControl(hwnd));
	const QSize iconSize = IconSize(*s);
	const int rowsHeight = RowsHeight(hwnd, *s);
	const QRect clip = ToRect(rc).adjusted(0, 0, s->scroller != nullptr ? -2 * ::GetSystemMetrics(SM_CXHSCROLL) : 0, 0);

	// The display area's frame.
	QRect pane = ToRect(rc);
	if(bottom)
		pane.setBottom(pane.bottom() - rowsHeight);
	else
		pane.setTop(pane.top() + rowsHeight);
	if(!buttons)
	{
		QStyleOptionTabWidgetFrame frame;
		frame.initFrom(w);
		frame.rect = pane;
		frame.shape = bottom ? QTabBar::RoundedSouth : QTabBar::RoundedNorth;
		frame.lineWidth = qs->pixelMetric(QStyle::PM_DefaultFrameWidth, nullptr, w);
		frame.midLineWidth = 0;
		if(s->sel >= 0 && s->sel < Count(*s))
			frame.selectedTabRect = VisibleRect(*s, s->sel);
		frame.tabBarRect = QRect(0, bottom ? pane.bottom() : 0, clip.width(), rowsHeight);
		frame.tabBarSize = frame.tabBarRect.size();
		if(!enabled)
			frame.state &= ~QStyle::State_Enabled;
		p->save();
		qs->drawPrimitive(QStyle::PE_FrameTabWidget, &frame, p, w);
		p->restore();
	}

	// Tabs, the selected one last so it is drawn over its neighbours.
	std::vector<int> order;
	for(int i = 0; i < Count(*s); i++)
	{
		if(i != s->sel)
			order.push_back(i);
	}
	if(s->sel >= 0 && s->sel < Count(*s))
		order.push_back(s->sel);
	p->save();
	p->setClipRect(clip, Qt::IntersectClip);
	p->setFont(font);
	for(int i : order)
	{
		const TabItem &item = s->items[static_cast<std::size_t>(i)];
		const QRect r = VisibleRect(*s, i);
		if(!r.intersects(clip))
			continue;
		const bool selected = i == s->sel;
		if(style & TCS_OWNERDRAWFIXED)
		{
			DRAWITEMSTRUCT dis{};
			dis.CtlType = ODT_TAB;
			dis.CtlID = static_cast<UINT>(::GetDlgCtrlID(hwnd));
			dis.itemID = static_cast<UINT>(i);
			dis.itemAction = ODA_DRAWENTIRE;
			dis.itemState = (selected ? ODS_SELECTED : 0) | (s->focused && i == s->focus ? ODS_FOCUS : 0) | (enabled ? 0 : ODS_DISABLED);
			dis.hwndItem = hwnd;
			dis.hDC = dc;
			// The inside of the tab, without its frame.
			dis.rcItem = ToRECT(r.adjusted(3, 3, -3, selected ? 0 : -1));
			LPARAM data = 0;
			std::memcpy(&data, item.extra.data(), std::min(item.extra.size(), sizeof(data)));
			dis.itemData = static_cast<ULONG_PTR>(data);
			if(!buttons)
			{
				QStyleOptionTab opt;
				opt.initFrom(w);
				opt.rect = r;
				opt.shape = bottom ? QTabBar::RoundedSouth : QTabBar::RoundedNorth;
				if(selected)
					opt.state |= QStyle::State_Selected;
				qs->drawControl(QStyle::CE_TabBarTabShape, &opt, p, w);
			}
			p->restore();
			::SendMessage(::GetParent(hwnd), WM_DRAWITEM, dis.CtlID, reinterpret_cast<LPARAM>(&dis));
			p = DCPainter(dc);
			if(p == nullptr)
				return;
			p->save();
			p->setClipRect(clip, Qt::IntersectClip);
			p->setFont(font);
			continue;
		}
		if(buttons)
		{
			QStyleOptionButton opt;
			opt.initFrom(w);
			opt.rect = r;
			opt.text = ToQString(item.text.c_str());
			opt.icon = ItemIcon(*s, item);
			opt.iconSize = iconSize;
			opt.state &= ~(QStyle::State_HasFocus | QStyle::State_MouseOver);
			opt.state |= (selected || (item.state & TCIS_BUTTONPRESSED)) ? QStyle::State_Sunken | QStyle::State_On : QStyle::State_Raised;
			if(s->focused && i == s->focus)
				opt.state |= QStyle::State_HasFocus | QStyle::State_KeyboardFocusChange;
			if(!enabled)
				opt.state &= ~QStyle::State_Enabled;
			if(style & TCS_FLATBUTTONS)
				opt.features |= QStyleOptionButton::Flat;
			qs->drawControl(QStyle::CE_PushButton, &opt, p, w);
			continue;
		}
		QStyleOptionTab opt;
		opt.initFrom(w);
		opt.rect = r;
		opt.text = ToQString(item.text.c_str());
		opt.icon = ItemIcon(*s, item);
		opt.iconSize = iconSize;
		opt.shape = bottom ? QTabBar::RoundedSouth : QTabBar::RoundedNorth;
		opt.state &= ~(QStyle::State_HasFocus | QStyle::State_MouseOver);
		if(!enabled)
			opt.state &= ~QStyle::State_Enabled;
		if(selected)
			opt.state |= QStyle::State_Selected;
		if(item.state & TCIS_HIGHLIGHTED)
			opt.state |= QStyle::State_On;
		const int rowCount = static_cast<int>(std::count_if(s->items.begin(), s->items.end(), [&](const TabItem &t) { return t.row == item.row; }));
		const bool first = std::none_of(s->items.begin(), s->items.end(), [&](const TabItem &t) { return t.row == item.row && t.rect.left() < item.rect.left(); });
		const bool last = std::none_of(s->items.begin(), s->items.end(), [&](const TabItem &t) { return t.row == item.row && t.rect.left() > item.rect.left(); });
		opt.position = rowCount == 1 ? QStyleOptionTab::OnlyOneTab : first ? QStyleOptionTab::Beginning : last ? QStyleOptionTab::End : QStyleOptionTab::Middle;
		if(s->sel >= 0 && s->sel < Count(*s) && s->items[static_cast<std::size_t>(s->sel)].row == item.row)
		{
			const int selLeft = s->items[static_cast<std::size_t>(s->sel)].rect.left();
			if(i + 1 < Count(*s) && s->items[static_cast<std::size_t>(i + 1)].rect.left() == selLeft && i + 1 == s->sel)
				opt.selectedPosition = QStyleOptionTab::NextIsSelected;
			else if(i > 0 && i - 1 == s->sel)
				opt.selectedPosition = QStyleOptionTab::PreviousIsSelected;
		}
		opt.documentMode = false;
		// The style may draw the selected tab into the display area, which
		// a tab bar widget would clip.
		const int overlap = selected ? qs->pixelMetric(QStyle::PM_TabBarBaseOverlap, &opt, w) : 0;
		p->save();
		p->setClipRect(r.adjusted(0, bottom ? -overlap : 0, 0, bottom ? 0 : overlap), Qt::IntersectClip);
		qs->drawControl(QStyle::CE_TabBarTab, &opt, p, w);
		p->restore();
		if(s->focused && i == s->focus && !(style & TCS_FOCUSNEVER))
		{
			QStyleOptionFocusRect focus;
			focus.initFrom(w);
			focus.rect = r.adjusted(3, 3, -3, -2);
			focus.state |= QStyle::State_KeyboardFocusChange;
			qs->drawPrimitive(QStyle::PE_FrameFocusRect, &focus, p, w);
		}
	}
	p->restore();
}

void CopyOut(const TabState &s, const TabItem &item, TCITEM *out)
{
	if(out->mask & TCIF_TEXT)
	{
		if(out->pszText != nullptr && out->cchTextMax > 0)
		{
			const std::size_t n = std::min(item.text.size(), static_cast<std::size_t>(out->cchTextMax - 1));
			std::copy_n(item.text.c_str(), n, out->pszText);
			out->pszText[n] = 0;
		}
	}
	if(out->mask & TCIF_IMAGE)
		out->iImage = item.image;
	if(out->mask & TCIF_PARAM)
		std::memcpy(&out->lParam, item.extra.data(), std::min(item.extra.size(), s.extraBytes));
	if(out->mask & TCIF_STATE)
		out->dwState = item.state & out->dwStateMask;
}

void CopyIn(const TabState &s, TabItem &item, const TCITEM *in)
{
	if(in->mask & TCIF_TEXT)
		item.text = in->pszText != nullptr ? std::wstring(in->pszText) : std::wstring();
	if(in->mask & TCIF_IMAGE)
		item.image = in->iImage;
	if(in->mask & TCIF_PARAM)
	{
		item.extra.assign(s.extraBytes, 0);
		std::memcpy(item.extra.data(), &in->lParam, s.extraBytes);
	}
	if(in->mask & TCIF_STATE)
		item.state = (item.state & ~in->dwStateMask) | (in->dwState & in->dwStateMask);
}

void Refresh(HWND hwnd, TabState &s)
{
	Layout(hwnd, s);
	::InvalidateRect(hwnd, nullptr, TRUE);
}

LRESULT CALLBACK TabProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	TabState *s = Control<TabState>(hwnd);
	switch(msg)
	{
	case WM_NCCREATE:
		if(WindowData *d = Data(hwnd))
			d->control = std::make_unique<TabState>();
		return ::DefWindowProc(hwnd, msg, wp, lp);
	case WM_CREATE:
		if(s != nullptr)
			Layout(hwnd, *s);
		return 0;
	case WM_DESTROY:
		if(s != nullptr && s->scroller != nullptr)
		{
			::DestroyWindow(s->scroller);
			s->scroller = nullptr;
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
			PaintTabs(hwnd, reinterpret_cast<HDC>(wp));
		} else
		{
			PAINTSTRUCT ps;
			HDC dc = ::BeginPaint(hwnd, &ps);
			if(dc != nullptr)
				PaintTabs(hwnd, dc);
			::EndPaint(hwnd, &ps);
		}
		return 0;
	case WM_SIZE:
		if(s != nullptr)
			Refresh(hwnd, *s);
		return 0;
	case WM_ENABLE:
		::InvalidateRect(hwnd, nullptr, FALSE);
		return 0;
	case WM_SETFONT:
		::DefWindowProc(hwnd, msg, wp, 0);
		if(s != nullptr)
		{
			Layout(hwnd, *s);
			if(lp)
				::InvalidateRect(hwnd, nullptr, TRUE);
		}
		return 0;
	case WM_SETFOCUS:
	case WM_KILLFOCUS:
		if(s != nullptr)
		{
			s->focused = msg == WM_SETFOCUS;
			if(s->focused && s->focus < 0)
				s->focus = s->sel;
			::InvalidateRect(hwnd, nullptr, FALSE);
		}
		return 0;
	case WM_HSCROLL:
		if(s != nullptr && reinterpret_cast<HWND>(lp) == s->scroller && s->scroller != nullptr)
		{
			if(LOWORD(wp) == SB_THUMBPOSITION)
			{
				s->firstVisible = std::clamp(static_cast<int>(static_cast<short>(HIWORD(wp))), 0, std::max(0, Count(*s) - 1));
				::InvalidateRect(hwnd, nullptr, TRUE);
			}
			return 0;
		}
		return ::DefWindowProc(hwnd, msg, wp, lp);
	case WM_LBUTTONDOWN:
	case WM_LBUTTONDBLCLK:
		if(s != nullptr)
		{
			const LONG_PTR style = StyleOf(hwnd);
			if((style & TCS_FOCUSONBUTTONDOWN) && !(style & TCS_FOCUSNEVER))
			{
				::SetFocus(hwnd);
				if((s = Control<TabState>(hwnd)) == nullptr)
					return 0;
			}
			const int hit = HitTest(hwnd, *s, QPoint(GET_X_LPARAM(lp), GET_Y_LPARAM(lp)));
			if(hit >= 0)
				UserSelect(hwnd, hit);
		}
		return 0;
	case WM_LBUTTONUP:
		NotifySimple(hwnd, NM_CLICK);
		return 0;
	case WM_RBUTTONUP:
		if(NotifySimple(hwnd, NM_RCLICK) == 0)
		{
			POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
			::ClientToScreen(hwnd, &pt);
			::SendMessage(hwnd, WM_CONTEXTMENU, reinterpret_cast<WPARAM>(hwnd), MAKELPARAM(pt.x, pt.y));
		}
		return 0;
	case WM_KEYDOWN:
		if(s != nullptr)
		{
			NMTCKEYDOWN kd{};
			kd.wVKey = static_cast<WORD>(wp);
			kd.flags = static_cast<UINT>(lp);
			Notify(hwnd, kd.hdr, TCN_KEYDOWN);
			if((s = Control<TabState>(hwnd)) == nullptr)
				return 0;
			int target = -1;
			const int from = s->focus >= 0 ? s->focus : s->sel;
			switch(wp)
			{
			case VK_LEFT:
			case VK_UP:
				target = from - 1;
				break;
			case VK_RIGHT:
			case VK_DOWN:
				target = from + 1;
				break;
			default:
				return ::DefWindowProc(hwnd, msg, wp, lp);
			}
			if(target >= 0 && target < Count(*s))
			{
				if(UserSelect(hwnd, target))
				{
					if((s = Control<TabState>(hwnd)) != nullptr)
					{
						EnsureVisible(hwnd, *s, target);
						::InvalidateRect(hwnd, nullptr, TRUE);
					}
				}
			}
		}
		return 0;

	case TCM_GETITEMCOUNT:
		return s ? Count(*s) : 0;
	case TCM_INSERTITEMW:
		if(s != nullptr && lp != 0)
		{
			int index = static_cast<int>(wp);
			if(index < 0)
				return -1;
			index = std::min(index, Count(*s));
			TabItem item;
			item.extra.assign(s->extraBytes, 0);
			CopyIn(*s, item, reinterpret_cast<const TCITEM *>(lp));
			s->items.insert(s->items.begin() + index, std::move(item));
			if(Count(*s) == 1)
			{
				s->sel = 0;
			} else
			{
				if(s->sel >= index)
					s->sel++;
				if(s->focus >= index)
					s->focus++;
			}
			Refresh(hwnd, *s);
			return index;
		}
		return -1;
	case TCM_GETITEMW:
		if(s != nullptr && lp != 0 && static_cast<int>(wp) >= 0 && static_cast<int>(wp) < Count(*s))
		{
			CopyOut(*s, s->items[wp], reinterpret_cast<TCITEM *>(lp));
			return TRUE;
		}
		return FALSE;
	case TCM_SETITEMW:
		if(s != nullptr && lp != 0 && static_cast<int>(wp) >= 0 && static_cast<int>(wp) < Count(*s))
		{
			CopyIn(*s, s->items[wp], reinterpret_cast<const TCITEM *>(lp));
			Refresh(hwnd, *s);
			return TRUE;
		}
		return FALSE;
	case TCM_DELETEITEM:
		if(s != nullptr && static_cast<int>(wp) >= 0 && static_cast<int>(wp) < Count(*s))
		{
			const int index = static_cast<int>(wp);
			s->items.erase(s->items.begin() + index);
			if(s->sel == index)
				s->sel = -1;
			else if(s->sel > index)
				s->sel--;
			if(s->focus == index)
				s->focus = -1;
			else if(s->focus > index)
				s->focus--;
			Refresh(hwnd, *s);
			return TRUE;
		}
		return FALSE;
	case TCM_DELETEALLITEMS:
		if(s != nullptr)
		{
			s->items.clear();
			s->sel = s->focus = -1;
			s->firstVisible = 0;
			Refresh(hwnd, *s);
		}
		return TRUE;
	case TCM_GETITEMRECT:
		if(s != nullptr && lp != 0 && static_cast<int>(wp) >= 0 && static_cast<int>(wp) < Count(*s))
		{
			*reinterpret_cast<RECT *>(lp) = ToRECT(VisibleRect(*s, static_cast<int>(wp)));
			return TRUE;
		}
		return FALSE;
	case TCM_GETCURSEL:
		return s ? s->sel : -1;
	case TCM_SETCURSEL:
		if(s != nullptr)
		{
			const int index = static_cast<int>(wp);
			if(index < -1 || index >= Count(*s))
				return -1;
			const int old = s->sel;
			s->sel = index;
			s->focus = index;
			EnsureVisible(hwnd, *s, index);
			Refresh(hwnd, *s);
			return old;
		}
		return -1;
	case TCM_GETCURFOCUS:
		return s ? (s->focus >= 0 ? s->focus : s->sel) : -1;
	case TCM_SETCURFOCUS:
		if(s != nullptr)
		{
			const int index = static_cast<int>(wp);
			if(index < -1 || index >= Count(*s))
				return 0;
			if(StyleOf(hwnd) & TCS_BUTTONS)
			{
				s->focus = index;
				::InvalidateRect(hwnd, nullptr, FALSE);
			} else if(index >= 0 && index != s->sel)
			{
				UserSelect(hwnd, index);
			} else
			{
				s->focus = index;
			}
		}
		return 0;
	case TCM_HITTEST:
		if(s != nullptr && lp != 0)
		{
			auto *info = reinterpret_cast<TCHITTESTINFO *>(lp);
			return HitTest(hwnd, *s, QPoint(info->pt.x, info->pt.y), &info->flags);
		}
		return -1;
	case TCM_SETITEMEXTRA:
		if(s != nullptr && s->items.empty() && static_cast<int>(wp) >= 0)
		{
			s->extraBytes = static_cast<std::size_t>(wp);
			return TRUE;
		}
		return FALSE;
	case TCM_ADJUSTRECT:
		if(s != nullptr && lp != 0)
		{
			RECT *r = reinterpret_cast<RECT *>(lp);
			const int tabs = RowsHeight(hwnd, *s);
			const int inset = DisplayAreaPadding + ControlBorder;
			const bool bottom = Bottom(hwnd);
			if(wp)
			{
				(bottom ? r->bottom : r->top) += bottom ? tabs : -tabs;
				::InflateRect(r, inset, inset);
			} else
			{
				::InflateRect(r, -inset, -inset);
				(bottom ? r->bottom : r->top) += bottom ? -tabs : tabs;
			}
		}
		return 0;
	case TCM_SETITEMSIZE:
		if(s != nullptr)
		{
			const LRESULT old = MAKELONG(s->fixedWidth > 0 ? s->fixedWidth : DefaultFixedTabWidth, s->tabHeight);
			if(LOWORD(lp) > 0)
				s->fixedWidth = LOWORD(lp);
			if(HIWORD(lp) > 0)
				s->fixedHeight = HIWORD(lp);
			Refresh(hwnd, *s);
			return old;
		}
		return 0;
	case TCM_SETPADDING:
		if(s != nullptr)
		{
			s->padX = LOWORD(lp);
			s->padY = HIWORD(lp);
			Refresh(hwnd, *s);
		}
		return 0;
	case TCM_SETMINTABWIDTH:
		if(s != nullptr)
		{
			const int old = s->minTabWidth >= 0 ? s->minTabWidth : DefaultMinTabWidth;
			s->minTabWidth = static_cast<int>(lp);
			Refresh(hwnd, *s);
			return old;
		}
		return 0;
	case TCM_GETROWCOUNT:
		return s ? s->rows : 0;
	case TCM_GETIMAGELIST:
		return s ? reinterpret_cast<LRESULT>(s->imageList) : 0;
	case TCM_SETIMAGELIST:
		if(s != nullptr)
		{
			const HIMAGELIST old = s->imageList;
			s->imageList = reinterpret_cast<HIMAGELIST>(lp);
			Refresh(hwnd, *s);
			return reinterpret_cast<LRESULT>(old);
		}
		return 0;
	case TCM_REMOVEIMAGE:
		if(s != nullptr)
		{
			const int image = static_cast<int>(wp);
			for(TabItem &item : s->items)
			{
				if(item.image == image)
					item.image = -1;
				else if(item.image > image)
					item.image--;
			}
			Refresh(hwnd, *s);
		}
		return 0;
	case TCM_DESELECTALL:
		if(s != nullptr)
		{
			for(int i = 0; i < Count(*s); i++)
			{
				if(wp == 0 || i != s->sel)
					s->items[static_cast<std::size_t>(i)].state &= ~TCIS_BUTTONPRESSED;
			}
			::InvalidateRect(hwnd, nullptr, FALSE);
		}
		return 0;
	case TCM_HIGHLIGHTITEM:
		if(s != nullptr && static_cast<int>(wp) >= 0 && static_cast<int>(wp) < Count(*s))
		{
			DWORD &state = s->items[wp].state;
			state = LOWORD(lp) ? (state | TCIS_HIGHLIGHTED) : (state & ~TCIS_HIGHLIGHTED);
			::InvalidateRect(hwnd, nullptr, FALSE);
			return TRUE;
		}
		return FALSE;
	case TCM_SETEXTENDEDSTYLE:
		if(s != nullptr)
		{
			const DWORD old = s->exStyle;
			const DWORD mask = wp != 0 ? static_cast<DWORD>(wp) : 0xFFFFFFFFu;
			s->exStyle = (s->exStyle & ~mask) | (static_cast<DWORD>(lp) & mask);
			return old;
		}
		return 0;
	case TCM_GETEXTENDEDSTYLE:
		return s ? s->exStyle : 0;
	case TCM_GETTOOLTIPS:
	case TCM_SETTOOLTIPS:
		return 0;
	default:
		return ::DefWindowProc(hwnd, msg, wp, lp);
	}
}

} // namespace

void RegisterTabControlClass()
{
	WNDCLASS wc{};
	wc.style = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = TabProc;
	wc.hCursor = ::LoadCursor(nullptr, IDC_ARROW);
	wc.lpszClassName = WC_TABCONTROL;
	::RegisterClass(&wc);
}

} // namespace linopenmpt::qtwin
