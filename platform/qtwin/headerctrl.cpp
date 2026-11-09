// The header control of comctl32 (SysHeader32): the column headings of list
// views. Items with text, images and formats in a display order, HDM_LAYOUT,
// dividers dragged with HDN_BEGINTRACK/TRACK/ENDTRACK and the
// HDN_ITEMCHANGING/CHANGED pair, clicks as HDN_ITEMCLICK. Drawn with the Qt
// style's header sections.
#include "QtWin.h"

#include <QtGui/QIcon>
#include <QtWidgets/QApplication>
#include <QtWidgets/QStyle>
#include <QtWidgets/QStyleOption>

#include <algorithm>
#include <string>
#include <vector>

namespace linopenmpt::qtwin
{

namespace
{

constexpr int DividerZone = 5; // pixels on each side of a divider that resize
constexpr int LabelMargin = 6;  // comctl32: 3 * SM_CXEDGE on each side of the text

struct HeaderItem
{
	std::wstring text;
	int width = 50;
	int fmt = HDF_LEFT | HDF_STRING;
	LPARAM lParam = 0;
	int image = -1;
	HBITMAP bitmap = nullptr;
	UINT state = 0;
};

struct HeaderState : ControlState
{
	std::vector<HeaderItem> items;
	std::vector<int> order; // display position -> index
	HIMAGELIST imageList = nullptr;
	int pressed = -1;       // HDS_BUTTONS item held down
	bool pressedInside = false;
	int tracking = -1;      // divider dragged (index of the item left of it)
	int trackOffset = 0;    // mouse x minus the divider's x
	int trackStartWidth = 0;
	int hotDivider = -1;
};

LONG_PTR StyleOf(HWND hwnd)
{
	return ::GetWindowLongPtr(hwnd, GWL_STYLE);
}

HFONT FontOfControl(HWND hwnd)
{
	const WindowData *d = Data(hwnd);
	return d != nullptr && d->font != nullptr ? d->font : static_cast<HFONT>(::GetStockObject(DEFAULT_GUI_FONT));
}

int Count(const HeaderState &s)
{
	return static_cast<int>(s.items.size());
}

// Left edge of each item, in display order.
QRect ItemRect(HWND hwnd, const HeaderState &s, int index)
{
	RECT rc{};
	::GetClientRect(hwnd, &rc);
	int x = 0;
	for(int i : s.order)
	{
		const int w = std::max(0, s.items[static_cast<std::size_t>(i)].width);
		if(i == index)
			return QRect(x, 0, w, rc.bottom);
		x += w;
	}
	return QRect();
}

LRESULT Notify(HWND hwnd, int item, UINT code, HDITEM *hdi = nullptr, int button = 0)
{
	NMHEADER nm{};
	nm.hdr.hwndFrom = hwnd;
	nm.hdr.idFrom = static_cast<UINT_PTR>(::GetDlgCtrlID(hwnd));
	nm.hdr.code = code;
	nm.iItem = item;
	nm.iButton = button;
	nm.pitem = hdi;
	const HWND parent = ::GetParent(hwnd);
	return parent != nullptr ? ::SendMessage(parent, WM_NOTIFY, nm.hdr.idFrom, reinterpret_cast<LPARAM>(&nm)) : 0;
}

int HitTest(HWND hwnd, const HeaderState &s, POINT pt, UINT *flags)
{
	RECT rc{};
	::GetClientRect(hwnd, &rc);
	UINT f = 0;
	int item = -1;
	if(pt.y < 0)
		f |= HHT_ABOVE;
	else if(pt.y >= rc.bottom)
		f |= HHT_BELOW;
	if(pt.x < 0)
		f |= HHT_TOLEFT;
	else if(pt.x >= rc.right)
		f |= HHT_TORIGHT;
	if(f == 0)
	{
		// Dividers first: the right edge of each item, the grip belonging to it.
		const bool sizing = !(StyleOf(hwnd) & HDS_NOSIZING);
		int x = 0;
		for(std::size_t pos = 0; pos < s.order.size() && sizing; pos++)
		{
			const int i = s.order[pos];
			const HeaderItem &it = s.items[static_cast<std::size_t>(i)];
			x += std::max(0, it.width);
			if(it.fmt & HDF_FIXEDWIDTH)
				continue;
			if(pt.x >= x - DividerZone && pt.x < x + DividerZone)
			{
				// A zero-width item to the right opens from its own divider.
				item = i;
				f = HHT_ONDIVIDER;
				if(pos + 1 < s.order.size() && s.items[static_cast<std::size_t>(s.order[pos + 1])].width == 0 && pt.x >= x)
				{
					item = s.order[pos + 1];
					f = HHT_ONDIVOPEN;
				}
			}
		}
		if(f == 0)
		{
			for(int i = 0; i < Count(s); i++)
			{
				if(ItemRect(hwnd, s, i).contains(pt.x, pt.y))
				{
					item = i;
					f = HHT_ONHEADER;
					break;
				}
			}
			if(f == 0)
				f = HHT_NOWHERE;
		}
	}
	if(flags != nullptr)
		*flags = f;
	return item;
}

int SectionHeight(HWND hwnd)
{
	HDC dc = ::GetDC(hwnd);
	HGDIOBJ old = ::SelectObject(dc, FontOfControl(hwnd));
	TEXTMETRIC tm{};
	::GetTextMetrics(dc, &tm);
	::SelectObject(dc, old);
	::ReleaseDC(hwnd, dc);
	QStyleOptionHeader opt;
	opt.text = QStringLiteral("Xg");
	opt.fontMetrics = QFontMetrics(FontOf(FontOfControl(hwnd)));
	const QSize styled = QApplication::style()->sizeFromContents(QStyle::CT_HeaderSection, &opt, QSize(), nullptr);
	return std::max(static_cast<int>(tm.tmHeight) + 6, styled.height());
}

QIcon ItemIcon(const HeaderState &s, const HeaderItem &item)
{
	if((item.fmt & HDF_IMAGE) && s.imageList != nullptr && item.image >= 0)
	{
		const QImage image = ImageListImage(s.imageList, item.image);
		if(!image.isNull())
			return QIcon(QPixmap::fromImage(image));
	}
	if((item.fmt & HDF_BITMAP) && item.bitmap != nullptr)
		return QIcon(QPixmap::fromImage(BitmapImage(item.bitmap)));
	return QIcon();
}

void PaintHeader(HWND hwnd, HDC dc)
{
	HeaderState *s = Control<HeaderState>(hwnd);
	Win32Widget *w = FromHandle(hwnd);
	if(s == nullptr || w == nullptr)
		return;
	RECT rc{};
	::GetClientRect(hwnd, &rc);
	::FillRect(dc, &rc, ::GetSysColorBrush(COLOR_BTNFACE));
	QStyle *qs = QApplication::style();
	const bool enabled = ::IsWindowEnabled(hwnd) != FALSE;
	const QFont font = FontOf(FontOfControl(hwnd));
	int right = 0;
	for(std::size_t pos = 0; pos < s->order.size(); pos++)
	{
		const int i = s->order[pos];
		const HeaderItem &item = s->items[static_cast<std::size_t>(i)];
		const QRect r = ItemRect(hwnd, *s, i);
		right = r.right() + 1;
		if(r.width() <= 0)
			continue;
		if(item.fmt & HDF_OWNERDRAW)
		{
			DRAWITEMSTRUCT dis{};
			dis.CtlType = ODT_HEADER;
			dis.CtlID = static_cast<UINT>(::GetDlgCtrlID(hwnd));
			dis.itemID = static_cast<UINT>(i);
			dis.itemAction = ODA_DRAWENTIRE;
			dis.itemState = (s->pressed == i && s->pressedInside) ? ODS_SELECTED : 0;
			dis.hwndItem = hwnd;
			dis.hDC = dc;
			dis.rcItem = RECT{r.left(), r.top(), r.right() + 1, r.bottom() + 1};
			dis.itemData = static_cast<ULONG_PTR>(item.lParam);
			::SendMessage(::GetParent(hwnd), WM_DRAWITEM, dis.CtlID, reinterpret_cast<LPARAM>(&dis));
			continue;
		}
		QPainter *p = DCPainter(dc);
		if(p == nullptr)
			return;
		QStyleOptionHeader opt;
		opt.initFrom(w);
		opt.rect = r;
		opt.section = static_cast<int>(pos);
		opt.orientation = Qt::Horizontal;
		opt.text = (item.fmt & HDF_STRING) || !(item.fmt & (HDF_BITMAP | HDF_IMAGE)) ? ToQString(item.text.c_str()) : QString();
		opt.icon = ItemIcon(*s, item);
		opt.fontMetrics = QFontMetrics(font);
		switch(item.fmt & HDF_JUSTIFYMASK)
		{
		case HDF_RIGHT: opt.textAlignment = Qt::AlignRight | Qt::AlignVCenter; break;
		case HDF_CENTER: opt.textAlignment = Qt::AlignHCenter | Qt::AlignVCenter; break;
		default: opt.textAlignment = Qt::AlignLeft | Qt::AlignVCenter; break;
		}
		opt.iconAlignment = Qt::AlignVCenter;
		// The arrow is drawn below with PE_IndicatorHeaderArrow, where SortUp
		// points up, as HDF_SORTUP does.
		if(item.fmt & HDF_SORTUP)
			opt.sortIndicator = QStyleOptionHeader::SortUp;
		else if(item.fmt & HDF_SORTDOWN)
			opt.sortIndicator = QStyleOptionHeader::SortDown;
		if(s->order.size() == 1)
			opt.position = QStyleOptionHeader::OnlyOneSection;
		else if(pos == 0)
			opt.position = QStyleOptionHeader::Beginning;
		else if(pos + 1 == s->order.size())
			opt.position = QStyleOptionHeader::End;
		else
			opt.position = QStyleOptionHeader::Middle;
		opt.state &= ~(QStyle::State_HasFocus | QStyle::State_MouseOver);
		if(!enabled)
			opt.state &= ~QStyle::State_Enabled;
		opt.state |= (s->pressed == i && s->pressedInside) ? QStyle::State_Sunken : QStyle::State_Raised;
		p->save();
		p->setFont(font);
		p->setClipRect(r, Qt::IntersectClip);
		// The section, then the label inset by comctl32's margin (3 edges each
		// side), with the sort arrow at the right. CE_Header alone puts the
		// text against the divider.
		qs->drawControl(QStyle::CE_HeaderSection, &opt, p, w);
		QStyleOptionHeader label = opt;
		label.rect = r.adjusted(LabelMargin, 0, -LabelMargin, 0);
		if(opt.sortIndicator != QStyleOptionHeader::None)
		{
			const int mark = qs->pixelMetric(QStyle::PM_HeaderMarkSize, &opt, w);
			QStyleOptionHeader arrow = opt;
			arrow.rect = QRect(label.rect.right() - mark + 1, r.top() + (r.height() - mark) / 2, mark, mark);
			qs->drawPrimitive(QStyle::PE_IndicatorHeaderArrow, &arrow, p, w);
			label.rect.setRight(arrow.rect.left() - LabelMargin);
		}
		qs->drawControl(QStyle::CE_HeaderLabel, &label, p, w);
		p->restore();
	}
	// The space after the last item.
	if(right < rc.right)
	{
		QPainter *p = DCPainter(dc);
		if(p == nullptr)
			return;
		QStyleOptionHeader opt;
		opt.initFrom(w);
		opt.rect = QRect(right, 0, rc.right - right, rc.bottom);
		opt.orientation = Qt::Horizontal;
		opt.position = QStyleOptionHeader::End;
		opt.state &= ~(QStyle::State_HasFocus | QStyle::State_MouseOver);
		opt.state |= QStyle::State_Raised;
		p->save();
		// comctl32 draws the rest as a blank section; Fusion's empty area is
		// the window colour, which reads as a hole in the header.
		qs->drawControl(QStyle::CE_HeaderSection, &opt, p, w);
		p->restore();
	}
}

void CopyIn(HeaderItem &item, const HDITEM *in)
{
	if(in->mask & HDI_WIDTH)
		item.width = in->cxy;
	if(in->mask & HDI_TEXT)
		item.text = (in->pszText != nullptr && in->pszText != LPSTR_TEXTCALLBACKW) ? std::wstring(in->pszText) : std::wstring();
	if(in->mask & HDI_FORMAT)
		item.fmt = in->fmt;
	if(in->mask & HDI_LPARAM)
		item.lParam = in->lParam;
	if(in->mask & HDI_IMAGE)
		item.image = in->iImage;
	if(in->mask & HDI_BITMAP)
		item.bitmap = in->hbm;
	if(in->mask & HDI_STATE)
		item.state = in->state;
}

void CopyOut(const HeaderState &s, int index, HDITEM *out)
{
	const HeaderItem &item = s.items[static_cast<std::size_t>(index)];
	if(out->mask & HDI_WIDTH)
		out->cxy = item.width;
	if((out->mask & HDI_TEXT) && out->pszText != nullptr && out->cchTextMax > 0)
	{
		const std::size_t n = std::min(item.text.size(), static_cast<std::size_t>(out->cchTextMax - 1));
		std::copy_n(item.text.c_str(), n, out->pszText);
		out->pszText[n] = 0;
	}
	if(out->mask & HDI_FORMAT)
		out->fmt = item.fmt;
	if(out->mask & HDI_LPARAM)
		out->lParam = item.lParam;
	if(out->mask & HDI_IMAGE)
		out->iImage = item.image;
	if(out->mask & HDI_BITMAP)
		out->hbm = item.bitmap;
	if(out->mask & HDI_STATE)
		out->state = item.state;
	if(out->mask & HDI_ORDER)
		out->iOrder = static_cast<int>(std::find(s.order.begin(), s.order.end(), index) - s.order.begin());
}

// A width change from the user, with HDN_ITEMCHANGING (which may refuse it).
bool ChangeWidth(HWND hwnd, int index, int width)
{
	HDITEM hdi{};
	hdi.mask = HDI_WIDTH;
	hdi.cxy = std::max(0, width);
	if(Notify(hwnd, index, HDN_ITEMCHANGING, &hdi) != 0)
		return false;
	HeaderState *s = Control<HeaderState>(hwnd);
	if(s == nullptr || index >= Count(*s))
		return false;
	s->items[static_cast<std::size_t>(index)].width = hdi.cxy;
	::InvalidateRect(hwnd, nullptr, FALSE);
	Notify(hwnd, index, HDN_ITEMCHANGED, &hdi);
	return true;
}

LRESULT CALLBACK HeaderProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	HeaderState *s = Control<HeaderState>(hwnd);
	switch(msg)
	{
	case WM_NCCREATE:
		if(WindowData *d = Data(hwnd))
			d->control = std::make_unique<HeaderState>();
		return ::DefWindowProc(hwnd, msg, wp, lp);
	case WM_ERASEBKGND:
		return 1;
	case WM_PAINT:
	case WM_PRINTCLIENT:
		if(msg == WM_PRINTCLIENT)
		{
			PaintHeader(hwnd, reinterpret_cast<HDC>(wp));
		} else
		{
			PAINTSTRUCT ps;
			HDC dc = ::BeginPaint(hwnd, &ps);
			if(dc != nullptr)
				PaintHeader(hwnd, dc);
			::EndPaint(hwnd, &ps);
		}
		return 0;
	case WM_SIZE:
	case WM_ENABLE:
		::InvalidateRect(hwnd, nullptr, FALSE);
		return 0;
	case WM_SETFONT:
		::DefWindowProc(hwnd, msg, wp, 0);
		if(lp)
			::InvalidateRect(hwnd, nullptr, FALSE);
		return 0;
	case WM_SETCURSOR:
		if(s != nullptr && reinterpret_cast<HWND>(wp) == hwnd && LOWORD(lp) == HTCLIENT)
		{
			POINT pt{};
			::GetCursorPos(&pt);
			::ScreenToClient(hwnd, &pt);
			UINT flags = 0;
			HitTest(hwnd, *s, pt, &flags);
			if(s->tracking >= 0 || (flags & (HHT_ONDIVIDER | HHT_ONDIVOPEN)))
			{
				::SetCursor(::LoadCursor(nullptr, IDC_SIZEWE));
				return TRUE;
			}
		}
		return ::DefWindowProc(hwnd, msg, wp, lp);
	case WM_LBUTTONDOWN:
		if(s != nullptr)
		{
			POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
			UINT flags = 0;
			const int item = HitTest(hwnd, *s, pt, &flags);
			if(item >= 0 && (flags & (HHT_ONDIVIDER | HHT_ONDIVOPEN)))
			{
				HDITEM hdi{};
				hdi.mask = HDI_WIDTH;
				hdi.cxy = s->items[static_cast<std::size_t>(item)].width;
				if(Notify(hwnd, item, HDN_BEGINTRACK, &hdi) != 0)
					return 0;
				if((s = Control<HeaderState>(hwnd)) == nullptr)
					return 0;
				s->tracking = item;
				s->trackStartWidth = s->items[static_cast<std::size_t>(item)].width;
				s->trackOffset = pt.x - ItemRect(hwnd, *s, item).right() - 1;
				::SetCapture(hwnd);
			} else if(item >= 0 && (StyleOf(hwnd) & HDS_BUTTONS))
			{
				s->pressed = item;
				s->pressedInside = true;
				::SetCapture(hwnd);
				::InvalidateRect(hwnd, nullptr, FALSE);
			}
		}
		return 0;
	case WM_LBUTTONDBLCLK:
		if(s != nullptr)
		{
			POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
			UINT flags = 0;
			const int item = HitTest(hwnd, *s, pt, &flags);
			if(item >= 0 && (flags & (HHT_ONDIVIDER | HHT_ONDIVOPEN)))
				Notify(hwnd, item, HDN_DIVIDERDBLCLICK);
			else if(item >= 0)
				Notify(hwnd, item, HDN_ITEMDBLCLICK);
		}
		return 0;
	case WM_MOUSEMOVE:
		if(s != nullptr && s->tracking >= 0)
		{
			const int item = s->tracking;
			const QRect r = ItemRect(hwnd, *s, item);
			const int width = std::max(0, GET_X_LPARAM(lp) - s->trackOffset - r.left());
			HDITEM hdi{};
			hdi.mask = HDI_WIDTH;
			hdi.cxy = width;
			if(Notify(hwnd, item, HDN_TRACK, &hdi) != 0)
				return 0;
			if(StyleOf(hwnd) & HDS_FULLDRAG)
				ChangeWidth(hwnd, item, width);
			else if((s = Control<HeaderState>(hwnd)) != nullptr)
				s->items[static_cast<std::size_t>(item)].width = width, ::InvalidateRect(hwnd, nullptr, FALSE);
		} else if(s != nullptr && s->pressed >= 0)
		{
			const bool inside = ItemRect(hwnd, *s, s->pressed).contains(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
			if(inside != s->pressedInside)
			{
				s->pressedInside = inside;
				::InvalidateRect(hwnd, nullptr, FALSE);
			}
		}
		return 0;
	case WM_LBUTTONUP:
	case WM_CAPTURECHANGED:
		if(s != nullptr && s->tracking >= 0)
		{
			const int item = s->tracking;
			const int width = s->items[static_cast<std::size_t>(item)].width;
			s->tracking = -1;
			if(msg == WM_LBUTTONUP)
				::ReleaseCapture();
			HDITEM hdi{};
			hdi.mask = HDI_WIDTH;
			hdi.cxy = width;
			Notify(hwnd, item, HDN_ENDTRACK, &hdi);
			if(!(StyleOf(hwnd) & HDS_FULLDRAG))
			{
				// The change is made, or refused, now.
				if((s = Control<HeaderState>(hwnd)) != nullptr)
				{
					s->items[static_cast<std::size_t>(item)].width = s->trackStartWidth;
					ChangeWidth(hwnd, item, width);
				}
			}
		} else if(s != nullptr && s->pressed >= 0)
		{
			const int item = s->pressed;
			const bool inside = s->pressedInside;
			s->pressed = -1;
			s->pressedInside = false;
			if(msg == WM_LBUTTONUP)
				::ReleaseCapture();
			::InvalidateRect(hwnd, nullptr, FALSE);
			if(inside && msg == WM_LBUTTONUP)
				Notify(hwnd, item, HDN_ITEMCLICK);
		}
		return 0;
	case WM_RBUTTONUP:
		{
			NMHDR hdr{hwnd, static_cast<UINT_PTR>(::GetDlgCtrlID(hwnd)), NM_RCLICK};
			::SendMessage(::GetParent(hwnd), WM_NOTIFY, hdr.idFrom, reinterpret_cast<LPARAM>(&hdr));
		}
		return 0;

	case HDM_GETITEMCOUNT:
		return s ? Count(*s) : 0;
	case HDM_INSERTITEMW:
		if(s != nullptr && lp != 0)
		{
			const auto *in = reinterpret_cast<const HDITEM *>(lp);
			const int index = std::clamp(static_cast<int>(wp), 0, Count(*s));
			HeaderItem item;
			CopyIn(item, in);
			s->items.insert(s->items.begin() + index, std::move(item));
			for(int &o : s->order)
			{
				if(o >= index)
					o++;
			}
			int pos = (in->mask & HDI_ORDER) ? std::clamp(in->iOrder, 0, static_cast<int>(s->order.size())) : index;
			pos = std::min(pos, static_cast<int>(s->order.size()));
			s->order.insert(s->order.begin() + pos, index);
			::InvalidateRect(hwnd, nullptr, FALSE);
			return index;
		}
		return -1;
	case HDM_DELETEITEM:
		if(s != nullptr && static_cast<int>(wp) >= 0 && static_cast<int>(wp) < Count(*s))
		{
			const int index = static_cast<int>(wp);
			s->items.erase(s->items.begin() + index);
			s->order.erase(std::find(s->order.begin(), s->order.end(), index));
			for(int &o : s->order)
			{
				if(o > index)
					o--;
			}
			::InvalidateRect(hwnd, nullptr, FALSE);
			return TRUE;
		}
		return FALSE;
	case HDM_GETITEMW:
		if(s != nullptr && lp != 0 && static_cast<int>(wp) >= 0 && static_cast<int>(wp) < Count(*s))
		{
			CopyOut(*s, static_cast<int>(wp), reinterpret_cast<HDITEM *>(lp));
			return TRUE;
		}
		return FALSE;
	case HDM_SETITEMW:
		if(s != nullptr && lp != 0 && static_cast<int>(wp) >= 0 && static_cast<int>(wp) < Count(*s))
		{
			const int index = static_cast<int>(wp);
			HDITEM copy = *reinterpret_cast<const HDITEM *>(lp);
			if(Notify(hwnd, index, HDN_ITEMCHANGING, &copy) != 0)
				return FALSE;
			if((s = Control<HeaderState>(hwnd)) == nullptr || index >= Count(*s))
				return FALSE;
			CopyIn(s->items[static_cast<std::size_t>(index)], reinterpret_cast<const HDITEM *>(lp));
			if(copy.mask & HDI_ORDER)
			{
				auto it = std::find(s->order.begin(), s->order.end(), index);
				s->order.erase(it);
				s->order.insert(s->order.begin() + std::clamp(copy.iOrder, 0, static_cast<int>(s->order.size())), index);
			}
			::InvalidateRect(hwnd, nullptr, FALSE);
			Notify(hwnd, index, HDN_ITEMCHANGED, &copy);
			return TRUE;
		}
		return FALSE;
	case HDM_LAYOUT:
		if(lp != 0)
		{
			auto *layout = reinterpret_cast<HDLAYOUT *>(lp);
			if(layout->prc == nullptr || layout->pwpos == nullptr)
				return FALSE;
			const int height = (StyleOf(hwnd) & HDS_HIDDEN) ? 0 : SectionHeight(hwnd);
			layout->pwpos->hwnd = hwnd;
			layout->pwpos->hwndInsertAfter = nullptr;
			layout->pwpos->x = layout->prc->left;
			layout->pwpos->y = layout->prc->top;
			layout->pwpos->cx = layout->prc->right - layout->prc->left;
			layout->pwpos->cy = height;
			layout->pwpos->flags = SWP_NOZORDER | SWP_NOACTIVATE;
			layout->prc->top += height;
			return TRUE;
		}
		return FALSE;
	case HDM_HITTEST:
		if(s != nullptr && lp != 0)
		{
			auto *info = reinterpret_cast<HDHITTESTINFO *>(lp);
			info->iItem = HitTest(hwnd, *s, info->pt, &info->flags);
			return info->iItem;
		}
		return -1;
	case HDM_GETITEMRECT:
		if(s != nullptr && lp != 0 && static_cast<int>(wp) >= 0 && static_cast<int>(wp) < Count(*s))
		{
			const QRect r = ItemRect(hwnd, *s, static_cast<int>(wp));
			*reinterpret_cast<RECT *>(lp) = RECT{r.left(), r.top(), r.left() + r.width(), r.top() + r.height()};
			return TRUE;
		}
		return FALSE;
	case HDM_SETIMAGELIST:
		if(s != nullptr)
		{
			const HIMAGELIST old = s->imageList;
			s->imageList = reinterpret_cast<HIMAGELIST>(lp);
			::InvalidateRect(hwnd, nullptr, FALSE);
			return reinterpret_cast<LRESULT>(old);
		}
		return 0;
	case HDM_GETIMAGELIST:
		return s ? reinterpret_cast<LRESULT>(s->imageList) : 0;
	case HDM_ORDERTOINDEX:
		return (s != nullptr && static_cast<std::size_t>(wp) < s->order.size()) ? s->order[wp] : static_cast<LRESULT>(wp);
	case HDM_GETORDERARRAY:
		if(s != nullptr && lp != 0 && static_cast<std::size_t>(wp) == s->order.size())
		{
			std::copy(s->order.begin(), s->order.end(), reinterpret_cast<int *>(lp));
			return TRUE;
		}
		return FALSE;
	case HDM_SETORDERARRAY:
		if(s != nullptr && lp != 0 && static_cast<std::size_t>(wp) == s->order.size())
		{
			const auto *a = reinterpret_cast<const int *>(lp);
			s->order.assign(a, a + wp);
			::InvalidateRect(hwnd, nullptr, FALSE);
			return TRUE;
		}
		return FALSE;
	case HDM_SETHOTDIVIDER:
		if(s != nullptr)
			s->hotDivider = static_cast<int>(lp);
		return s ? s->hotDivider : -1;
	default:
		return ::DefWindowProc(hwnd, msg, wp, lp);
	}
}

} // namespace

void RegisterHeaderClass()
{
	WNDCLASS wc{};
	wc.style = CS_DBLCLKS;
	wc.lpfnWndProc = HeaderProc;
	wc.hCursor = ::LoadCursor(nullptr, IDC_ARROW);
	wc.lpszClassName = WC_HEADER;
	::RegisterClass(&wc);
}

} // namespace linopenmpt::qtwin
