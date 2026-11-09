// The common controls of comctl32 that hold a value: trackbars
// (msctls_trackbar32), up-down controls (msctls_updown32) and progress bars
// (msctls_progress32).
//
// As the USER controls, they are window procedures with the messages,
// notifications and defaults of comctl32, drawn with the Qt style.
#include "QtWin.h"

#include <QtCore/QElapsedTimer>
#include <QtCore/QLocale>
#include <QtWidgets/QApplication>
#include <QtWidgets/QStyle>
#include <QtWidgets/QStyleOption>
#include <QtWidgets/QToolTip>

#include <algorithm>
#include <map>
#include <vector>

namespace linopenmpt::qtwin
{

namespace
{

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

// WM_NOTIFY to the parent; returns its answer.
LRESULT Notify(HWND hwnd, NMHDR &hdr, UINT code)
{
	hdr.hwndFrom = hwnd;
	hdr.idFrom = static_cast<UINT_PTR>(::GetDlgCtrlID(hwnd));
	hdr.code = code;
	const HWND parent = ::GetParent(hwnd);
	return parent != nullptr ? ::SendMessage(parent, WM_NOTIFY, hdr.idFrom, reinterpret_cast<LPARAM>(&hdr)) : 0;
}

void NotifySimple(HWND hwnd, UINT code)
{
	NMHDR hdr{};
	Notify(hwnd, hdr, code);
}

// Paints with WM_PAINT or WM_PRINTCLIENT.
template <typename PaintFn>
LRESULT PaintMessage(HWND hwnd, UINT msg, WPARAM wp, PaintFn paint)
{
	if(msg == WM_PRINTCLIENT)
	{
		paint(reinterpret_cast<HDC>(wp));
		return 0;
	}
	PAINTSTRUCT ps;
	HDC dc = ::BeginPaint(hwnd, &ps);
	if(dc != nullptr)
		paint(dc);
	::EndPaint(hwnd, &ps);
	return 0;
}

void InitOption(QStyleOption &opt, HWND hwnd, const QRect &rect)
{
	if(Win32Widget *w = FromHandle(hwnd))
		opt.initFrom(w);
	opt.rect = rect;
	opt.state &= ~(QStyle::State_HasFocus | QStyle::State_MouseOver);
	if(::IsWindowEnabled(hwnd))
		opt.state |= QStyle::State_Enabled;
	else
		opt.state &= ~QStyle::State_Enabled;
}

// ---- trackbars -----------------------------------------------------------------------

constexpr UINT_PTR TrackbarPageTimer = 1;

struct TrackbarState : ControlState
{
	int min = 0;
	int max = 100;
	int pos = 0;
	int line = 1;
	int page = 20;
	bool autoPage = true; // the page follows the range until it is set
	int ticFreq = 1;
	std::vector<int> tics; // set with TBM_SETTIC, sorted
	int selStart = 0;
	int selEnd = 0;
	int thumbLength = 0; // 0: the style's
	bool dragging = false;
	int dragOffset = 0;
	int pageDir = 0; // -1 or 1 while a click in the channel pages
	POINT pageTarget{};
	int wheelDelta = 0;
	bool focused = false;
	HWND buddy[2]{}; // left/top and right/bottom
	int tipSide = TBTS_TOP;
};

bool Vertical(HWND hwnd)
{
	return (StyleOf(hwnd) & TBS_VERT) != 0;
}

QStyleOptionSlider TrackbarOption(HWND hwnd, const TrackbarState &s)
{
	const LONG_PTR style = StyleOf(hwnd);
	QStyleOptionSlider opt;
	InitOption(opt, hwnd, ToRect(ClientRectOf(hwnd)));
	const bool vert = (style & TBS_VERT) != 0;
	opt.orientation = vert ? Qt::Vertical : Qt::Horizontal;
	if(!vert)
		opt.state |= QStyle::State_Horizontal;
	opt.minimum = s.min;
	opt.maximum = std::max(s.min, s.max);
	opt.sliderPosition = opt.sliderValue = std::clamp(s.pos, opt.minimum, opt.maximum);
	opt.singleStep = s.line;
	opt.pageStep = s.page;
	// Windows puts the minimum at the top of a vertical trackbar.
	opt.upsideDown = false;
	if(style & TBS_NOTICKS)
		opt.tickPosition = QSlider::NoTicks;
	else if(style & TBS_BOTH)
		opt.tickPosition = QSlider::TicksBothSides;
	else if(style & TBS_TOP) // TBS_LEFT
		opt.tickPosition = QSlider::TicksAbove;
	else
		opt.tickPosition = QSlider::TicksBelow;
	opt.tickInterval = std::max(1, s.ticFreq);
	opt.subControls = QStyle::SC_SliderGroove;
	if(!(style & TBS_NOTHUMB))
		opt.subControls |= QStyle::SC_SliderHandle;
	if(s.dragging)
	{
		opt.activeSubControls = QStyle::SC_SliderHandle;
		opt.state |= QStyle::State_Sunken;
	}
	if(s.focused)
		opt.state |= QStyle::State_HasFocus | QStyle::State_KeyboardFocusChange;
	return opt;
}

QRect ThumbRect(HWND hwnd, const TrackbarState &s, int value)
{
	QStyleOptionSlider opt = TrackbarOption(hwnd, s);
	opt.sliderPosition = opt.sliderValue = std::clamp(value, opt.minimum, opt.maximum);
	return QApplication::style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle, nullptr);
}

QRect ChannelRect(HWND hwnd, const TrackbarState &s)
{
	const QStyleOptionSlider opt = TrackbarOption(hwnd, s);
	return QApplication::style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderGroove, nullptr);
}

// The client coordinate of a value along the channel (the thumb's centre).
int TicCoord(HWND hwnd, const TrackbarState &s, int value)
{
	const QRect thumb = ThumbRect(hwnd, s, value);
	return Vertical(hwnd) ? thumb.center().y() : thumb.center().x();
}

int ValueAt(HWND hwnd, const TrackbarState &s, int coord)
{
	const QStyleOptionSlider opt = TrackbarOption(hwnd, s);
	QStyle *qs = QApplication::style();
	const QRect groove = qs->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderGroove, nullptr);
	const QRect handle = qs->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle, nullptr);
	const bool vert = Vertical(hwnd);
	const int length = vert ? handle.height() : handle.width();
	const int start = vert ? groove.top() : groove.left();
	const int span = (vert ? groove.height() : groove.width()) - length;
	return QStyle::sliderValueFromPosition(opt.minimum, opt.maximum, coord - start, std::max(span, 0), opt.upsideDown);
}

// The tic marks drawn: the ends, then the TBS_AUTOTICKS or TBM_SETTIC ones.
std::vector<int> TicValues(HWND hwnd, const TrackbarState &s)
{
	std::vector<int> values;
	if(StyleOf(hwnd) & TBS_NOTICKS)
		return values;
	values.push_back(s.min);
	if(StyleOf(hwnd) & TBS_AUTOTICKS)
	{
		const int freq = std::max(1, s.ticFreq);
		for(int v = s.min + freq; v < s.max; v += freq)
			values.push_back(v);
	} else
	{
		for(int v : s.tics)
		{
			if(v > s.min && v < s.max)
				values.push_back(v);
		}
	}
	if(s.max != s.min)
		values.push_back(s.max);
	return values;
}

void SendScroll(HWND hwnd, UINT code, int pos = 0)
{
	const HWND parent = ::GetParent(hwnd);
	if(parent == nullptr)
		return;
	const bool withPos = code == TB_THUMBTRACK || code == TB_THUMBPOSITION;
	::SendMessage(parent, Vertical(hwnd) ? WM_VSCROLL : WM_HSCROLL, MAKEWPARAM(code, withPos ? pos : 0), reinterpret_cast<LPARAM>(hwnd));
}

void ShowValueTip(HWND hwnd, const TrackbarState &s)
{
	Win32Widget *w = FromHandle(hwnd);
	if(w == nullptr || !(StyleOf(hwnd) & TBS_TOOLTIPS))
		return;
	const QRect thumb = ThumbRect(hwnd, s, s.pos).translated(ClientOrigin(w));
	QPoint at;
	switch(s.tipSide)
	{
	case TBTS_LEFT: at = QPoint(thumb.left() - 24, thumb.center().y()); break;
	case TBTS_RIGHT: at = QPoint(thumb.right() + 4, thumb.center().y()); break;
	case TBTS_BOTTOM: at = QPoint(thumb.left(), thumb.bottom() + 4); break;
	default: at = QPoint(thumb.left(), thumb.top() - 28); break;
	}
	QToolTip::showText(w->mapToGlobal(at), QString::number(s.pos), w);
}

void SetTrackbarPos(HWND hwnd, TrackbarState &s, int pos)
{
	pos = std::clamp(pos, s.min, std::max(s.min, s.max));
	if(pos != s.pos)
	{
		s.pos = pos;
		::InvalidateRect(hwnd, nullptr, FALSE);
	}
}

// A key or channel click: moves and notifies, unless already at the end.
bool TrackbarStep(HWND hwnd, TrackbarState &s, UINT code)
{
	int pos = s.pos;
	switch(code)
	{
	case TB_LINEUP: pos -= s.line; break;
	case TB_LINEDOWN: pos += s.line; break;
	case TB_PAGEUP: pos -= s.page; break;
	case TB_PAGEDOWN: pos += s.page; break;
	case TB_TOP: pos = s.min; break;
	case TB_BOTTOM: pos = s.max; break;
	default: break;
	}
	pos = std::clamp(pos, s.min, std::max(s.min, s.max));
	if(pos == s.pos)
		return false;
	SetTrackbarPos(hwnd, s, pos);
	SendScroll(hwnd, code, pos);
	return true;
}

void PlaceBuddies(HWND hwnd, const TrackbarState &s)
{
	const HWND parent = ::GetParent(hwnd);
	RECT rc{};
	::GetWindowRect(hwnd, &rc);
	::MapWindowPoints(HWND_DESKTOP, parent, reinterpret_cast<POINT *>(&rc), 2);
	const bool vert = Vertical(hwnd);
	for(int i = 0; i < 2; i++)
	{
		const HWND b = s.buddy[i];
		if(b == nullptr || !::IsWindow(b))
			continue;
		RECT br{};
		::GetWindowRect(b, &br);
		const int bw = br.right - br.left, bh = br.bottom - br.top;
		int x, y;
		if(vert)
		{
			x = (rc.left + rc.right - bw) / 2;
			y = i == 0 ? rc.top - bh : rc.bottom;
		} else
		{
			x = i == 0 ? rc.left - bw : rc.right;
			y = (rc.top + rc.bottom - bh) / 2;
		}
		::SetWindowPos(b, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
	}
}

void PaintTrackbar(HWND hwnd, HDC dc)
{
	TrackbarState *s = Control<TrackbarState>(hwnd);
	if(s == nullptr)
		return;
	const RECT rc = ClientRectOf(hwnd);
	const LONG_PTR style = StyleOf(hwnd);

	NMCUSTOMDRAW cd{};
	cd.hdc = dc;
	cd.rc = rc;
	cd.dwDrawStage = CDDS_PREPAINT;
	if(s->focused)
		cd.uItemState |= CDIS_FOCUS;
	if(!::IsWindowEnabled(hwnd))
		cd.uItemState |= CDIS_DISABLED;
	const LRESULT pre = Notify(hwnd, cd.hdr, NM_CUSTOMDRAW);
	if(pre & CDRF_SKIPDEFAULT)
		return;
	const bool itemDraw = (pre & CDRF_NOTIFYITEMDRAW) != 0;
	// Each part: TBCD_* with its rectangle; false if the parent drew it.
	auto partPre = [&](DWORD_PTR part, const QRect &r) -> LRESULT
	{
		if(!itemDraw)
			return CDRF_DODEFAULT;
		cd.dwDrawStage = CDDS_ITEMPREPAINT;
		cd.dwItemSpec = part;
		cd.rc = ToRECT(r);
		return Notify(hwnd, cd.hdr, NM_CUSTOMDRAW);
	};
	auto partPost = [&](DWORD_PTR part, const QRect &r, LRESULT preResult)
	{
		if(!(preResult & CDRF_NOTIFYPOSTPAINT))
			return;
		cd.dwDrawStage = CDDS_ITEMPOSTPAINT;
		cd.dwItemSpec = part;
		cd.rc = ToRECT(r);
		Notify(hwnd, cd.hdr, NM_CUSTOMDRAW);
	};

	HBRUSH brush = ControlBrush(hwnd, dc, WM_CTLCOLORSTATIC);
	::FillRect(dc, &rc, brush);

	QStyle *qs = QApplication::style();
	QStyleOptionSlider opt = TrackbarOption(hwnd, *s);
	const QRect groove = qs->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderGroove, nullptr);
	const QRect thumb = qs->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle, nullptr);
	const bool vert = (style & TBS_VERT) != 0;

	// Tics, in the text colour of the WM_CTLCOLORSTATIC answer.
	const std::vector<int> tics = TicValues(hwnd, *s);
	if(!tics.empty())
	{
		const LRESULT r = partPre(TBCD_TICS, ToRect(rc));
		if(!(r & CDRF_SKIPDEFAULT))
		{
			HPEN pen = ::CreatePen(PS_SOLID, 1, ::GetTextColor(dc));
			HGDIOBJ oldPen = ::SelectObject(dc, pen);
			const bool before = (style & (TBS_TOP | TBS_BOTH)) != 0;
			const bool after = !(style & TBS_TOP) || (style & TBS_BOTH);
			for(std::size_t i = 0; i < tics.size(); i++)
			{
				const int c = TicCoord(hwnd, *s, tics[i]);
				const int len = (i == 0 || i + 1 == tics.size()) ? 4 : 3;
				if(vert)
				{
					if(before)
					{
						::MoveToEx(dc, thumb.left() - 2, c, nullptr);
						::LineTo(dc, thumb.left() - 2 - len, c);
					}
					if(after)
					{
						::MoveToEx(dc, thumb.right() + 2, c, nullptr);
						::LineTo(dc, thumb.right() + 2 + len, c);
					}
				} else
				{
					if(before)
					{
						::MoveToEx(dc, c, thumb.top() - 2, nullptr);
						::LineTo(dc, c, thumb.top() - 2 - len);
					}
					if(after)
					{
						::MoveToEx(dc, c, thumb.bottom() + 2, nullptr);
						::LineTo(dc, c, thumb.bottom() + 2 + len);
					}
				}
			}
			::SelectObject(dc, oldPen);
			::DeleteObject(pen);
		}
		partPost(TBCD_TICS, ToRect(rc), r);
	}

	QPainter *p = DCPainter(dc);
	if(p == nullptr)
		return;
	const LRESULT channelResult = partPre(TBCD_CHANNEL, groove);
	if(!(channelResult & CDRF_SKIPDEFAULT))
	{
		QStyleOptionSlider part = opt;
		part.subControls = QStyle::SC_SliderGroove;
		p->save();
		qs->drawComplexControl(QStyle::CC_Slider, &part, p, nullptr);
		p->restore();
		if((style & TBS_ENABLESELRANGE) && s->selEnd > s->selStart)
		{
			const int a = TicCoord(hwnd, *s, s->selStart), b = TicCoord(hwnd, *s, s->selEnd);
			const QRect sel = vert ? QRect(groove.left() + 1, a, std::max(1, groove.width() - 2), b - a + 1)
			                       : QRect(a, groove.top() + 1, b - a + 1, std::max(1, groove.height() - 2));
			p->fillRect(sel, opt.palette.color(QPalette::Highlight));
		}
	}
	partPost(TBCD_CHANNEL, groove, channelResult);
	if(!(style & TBS_NOTHUMB))
	{
		const LRESULT thumbResult = partPre(TBCD_THUMB, thumb);
		if(!(thumbResult & CDRF_SKIPDEFAULT))
		{
			QStyleOptionSlider part = opt;
			part.subControls = QStyle::SC_SliderHandle;
			p->save();
			qs->drawComplexControl(QStyle::CC_Slider, &part, p, nullptr);
			p->restore();
		}
		partPost(TBCD_THUMB, thumb, thumbResult);
	}
	if(pre & CDRF_NOTIFYPOSTPAINT)
	{
		cd.dwDrawStage = CDDS_POSTPAINT;
		cd.dwItemSpec = 0;
		cd.rc = rc;
		Notify(hwnd, cd.hdr, NM_CUSTOMDRAW);
	}
}

// Pages toward the click while the button is held and the thumb is not there.
void TrackbarPage(HWND hwnd, TrackbarState &s)
{
	if(s.pageDir == 0)
		return;
	const int target = ValueAt(hwnd, s, Vertical(hwnd) ? s.pageTarget.y : s.pageTarget.x);
	if((s.pageDir < 0 && s.pos <= target) || (s.pageDir > 0 && s.pos >= target))
		return;
	TrackbarStep(hwnd, s, s.pageDir < 0 ? TB_PAGEUP : TB_PAGEDOWN);
}

void EndTrackbarMouse(HWND hwnd, TrackbarState &s)
{
	const bool wasDragging = s.dragging;
	s.dragging = false;
	if(s.pageDir != 0)
		::KillTimer(hwnd, TrackbarPageTimer);
	s.pageDir = 0;
	if(wasDragging)
	{
		SendScroll(hwnd, TB_THUMBPOSITION, s.pos);
		QToolTip::hideText();
	}
	SendScroll(hwnd, TB_ENDTRACK);
	::InvalidateRect(hwnd, nullptr, FALSE);
}

LRESULT CALLBACK TrackbarProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	TrackbarState *s = Control<TrackbarState>(hwnd);
	switch(msg)
	{
	case WM_NCCREATE:
		if(WindowData *d = Data(hwnd))
			d->control = std::make_unique<TrackbarState>();
		return ::DefWindowProc(hwnd, msg, wp, lp);
	case WM_GETDLGCODE:
		return DLGC_WANTARROWS;
	case WM_ERASEBKGND:
		return 1;
	case WM_PAINT:
	case WM_PRINTCLIENT:
		return PaintMessage(hwnd, msg, wp, [hwnd](HDC dc) { PaintTrackbar(hwnd, dc); });
	case WM_ENABLE:
	case WM_SIZE:
		::InvalidateRect(hwnd, nullptr, FALSE);
		return 0;
	case WM_SETFOCUS:
	case WM_KILLFOCUS:
		if(s != nullptr)
		{
			s->focused = msg == WM_SETFOCUS;
			::InvalidateRect(hwnd, nullptr, FALSE);
			NotifySimple(hwnd, msg == WM_SETFOCUS ? NM_SETFOCUS : NM_KILLFOCUS);
		}
		return 0;
	case WM_LBUTTONDOWN:
	case WM_LBUTTONDBLCLK:
		if(s != nullptr)
		{
			::SetFocus(hwnd);
			if((s = Control<TrackbarState>(hwnd)) == nullptr)
				return 0;
			const QPoint pt(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
			const QRect thumb = ThumbRect(hwnd, *s, s->pos);
			const bool vert = Vertical(hwnd);
			::SetCapture(hwnd);
			if(!(StyleOf(hwnd) & TBS_NOTHUMB) && thumb.contains(pt))
			{
				s->dragging = true;
				s->dragOffset = vert ? pt.y() - thumb.top() : pt.x() - thumb.left();
				ShowValueTip(hwnd, *s);
				::InvalidateRect(hwnd, nullptr, FALSE);
			} else
			{
				const int c = vert ? pt.y() : pt.x();
				const int t = vert ? thumb.center().y() : thumb.center().x();
				s->pageDir = c < t ? -1 : 1;
				s->pageTarget = POINT{pt.x(), pt.y()};
				TrackbarPage(hwnd, *s);
				::SetTimer(hwnd, TrackbarPageTimer, 500, nullptr);
			}
		}
		return 0;
	case WM_MOUSEMOVE:
		if(s != nullptr && s->dragging)
		{
			const int value = ValueAt(hwnd, *s, (Vertical(hwnd) ? GET_Y_LPARAM(lp) : GET_X_LPARAM(lp)) - s->dragOffset);
			if(value != s->pos)
			{
				SetTrackbarPos(hwnd, *s, value);
				SendScroll(hwnd, TB_THUMBTRACK, s->pos);
				if((s = Control<TrackbarState>(hwnd)) != nullptr)
					ShowValueTip(hwnd, *s);
			}
		} else if(s != nullptr && s->pageDir != 0)
		{
			s->pageTarget = POINT{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
		}
		return 0;
	case WM_TIMER:
		if(s != nullptr && wp == TrackbarPageTimer)
		{
			::SetTimer(hwnd, TrackbarPageTimer, 100, nullptr);
			TrackbarPage(hwnd, *s);
			return 0;
		}
		return ::DefWindowProc(hwnd, msg, wp, lp);
	case WM_LBUTTONUP:
		if(s != nullptr && ::GetCapture() == hwnd)
		{
			::ReleaseCapture();
			if((s = Control<TrackbarState>(hwnd)) != nullptr && (s->dragging || s->pageDir != 0))
				EndTrackbarMouse(hwnd, *s);
			NotifySimple(hwnd, NM_RELEASEDCAPTURE);
		}
		return 0;
	case WM_CAPTURECHANGED:
		if(s != nullptr && (s->dragging || s->pageDir != 0))
			EndTrackbarMouse(hwnd, *s);
		return 0;
	case WM_MOUSEWHEEL:
		if(s != nullptr)
		{
			if(GET_KEYSTATE_WPARAM(wp) & (MK_SHIFT | MK_CONTROL))
				return ::DefWindowProc(hwnd, msg, wp, lp);
			UINT lines = 3;
			::SystemParametersInfo(SPI_GETWHEELSCROLLLINES, 0, &lines, 0);
			s->wheelDelta += GET_WHEEL_DELTA_WPARAM(wp);
			const int notches = s->wheelDelta / WHEEL_DELTA;
			if(notches != 0)
			{
				s->wheelDelta -= notches * WHEEL_DELTA;
				const int pos = std::clamp(s->pos - notches * s->line * static_cast<int>(lines), s->min, std::max(s->min, s->max));
				if(pos != s->pos)
				{
					SetTrackbarPos(hwnd, *s, pos);
					SendScroll(hwnd, notches > 0 ? TB_LINEUP : TB_LINEDOWN, pos);
					SendScroll(hwnd, TB_ENDTRACK);
				}
			}
		}
		return 0;
	case WM_KEYDOWN:
		if(s != nullptr)
		{
			const bool downIsLeft = (StyleOf(hwnd) & TBS_DOWNISLEFT) != 0;
			switch(wp)
			{
			case VK_LEFT: TrackbarStep(hwnd, *s, TB_LINEUP); break;
			case VK_RIGHT: TrackbarStep(hwnd, *s, TB_LINEDOWN); break;
			case VK_UP: TrackbarStep(hwnd, *s, downIsLeft ? TB_LINEDOWN : TB_LINEUP); break;
			case VK_DOWN: TrackbarStep(hwnd, *s, downIsLeft ? TB_LINEUP : TB_LINEDOWN); break;
			case VK_PRIOR: TrackbarStep(hwnd, *s, TB_PAGEUP); break;
			case VK_NEXT: TrackbarStep(hwnd, *s, TB_PAGEDOWN); break;
			case VK_HOME: TrackbarStep(hwnd, *s, TB_TOP); break;
			case VK_END: TrackbarStep(hwnd, *s, TB_BOTTOM); break;
			default: return ::DefWindowProc(hwnd, msg, wp, lp);
			}
		}
		return 0;
	case WM_KEYUP:
		switch(wp)
		{
		case VK_LEFT: case VK_RIGHT: case VK_UP: case VK_DOWN:
		case VK_PRIOR: case VK_NEXT: case VK_HOME: case VK_END:
			SendScroll(hwnd, TB_ENDTRACK);
			return 0;
		default:
			return ::DefWindowProc(hwnd, msg, wp, lp);
		}
	case WM_DESTROY:
		if(s != nullptr && s->dragging)
			QToolTip::hideText();
		return ::DefWindowProc(hwnd, msg, wp, lp);

	case TBM_GETPOS: return s ? s->pos : 0;
	case TBM_GETRANGEMIN: return s ? s->min : 0;
	case TBM_GETRANGEMAX: return s ? s->max : 0;
	case TBM_GETLINESIZE: return s ? s->line : 0;
	case TBM_GETPAGESIZE: return s ? s->page : 0;
	case TBM_GETSELSTART: return s ? s->selStart : 0;
	case TBM_GETSELEND: return s ? s->selEnd : 0;
	case TBM_GETTHUMBLENGTH:
		if(s != nullptr)
		{
			const QRect t = ThumbRect(hwnd, *s, s->pos);
			return Vertical(hwnd) ? t.width() : t.height();
		}
		return 0;
	case TBM_GETTOOLTIPS: return 0;
	case TBM_SETTOOLTIPS: return 0;
	case TBM_SETTIPSIDE:
		if(s != nullptr)
		{
			const int old = s->tipSide;
			s->tipSide = static_cast<int>(wp);
			return old;
		}
		return 0;
	case TBM_SETPOS:
	case TBM_SETPOSNOTIFY:
		if(s != nullptr)
		{
			SetTrackbarPos(hwnd, *s, static_cast<int>(lp));
			if(msg == TBM_SETPOSNOTIFY)
				SendScroll(hwnd, TB_THUMBPOSITION, s->pos);
		}
		return 0;
	case TBM_SETRANGE:
	case TBM_SETRANGEMIN:
	case TBM_SETRANGEMAX:
		if(s != nullptr)
		{
			if(msg == TBM_SETRANGE)
			{
				s->min = static_cast<short>(LOWORD(lp));
				s->max = static_cast<short>(HIWORD(lp));
			} else if(msg == TBM_SETRANGEMIN)
			{
				s->min = static_cast<int>(lp);
			} else
			{
				s->max = static_cast<int>(lp);
			}
			if(s->autoPage)
				s->page = std::max(1, (s->max - s->min) / 5);
			s->pos = std::clamp(s->pos, s->min, std::max(s->min, s->max));
			if(wp)
				::InvalidateRect(hwnd, nullptr, FALSE);
		}
		return 0;
	case TBM_SETLINESIZE:
		if(s != nullptr)
		{
			const int old = s->line;
			s->line = static_cast<int>(lp);
			return old;
		}
		return 0;
	case TBM_SETPAGESIZE:
		if(s != nullptr)
		{
			const int old = s->page;
			s->page = static_cast<int>(lp);
			s->autoPage = false;
			return old;
		}
		return 0;
	case TBM_SETTICFREQ:
		if(s != nullptr && (StyleOf(hwnd) & TBS_AUTOTICKS))
		{
			s->ticFreq = std::max(1, static_cast<int>(wp));
			::InvalidateRect(hwnd, nullptr, FALSE);
		}
		return 0;
	case TBM_SETTIC:
		if(s != nullptr)
		{
			const int v = static_cast<int>(lp);
			if(v < s->min || v > s->max)
				return FALSE;
			s->tics.insert(std::upper_bound(s->tics.begin(), s->tics.end(), v), v);
			::InvalidateRect(hwnd, nullptr, FALSE);
			return TRUE;
		}
		return FALSE;
	case TBM_CLEARTICS:
		if(s != nullptr)
		{
			s->tics.clear();
			if(wp)
				::InvalidateRect(hwnd, nullptr, FALSE);
		}
		return 0;
	case TBM_GETNUMTICS:
		return s ? static_cast<LRESULT>(TicValues(hwnd, *s).size()) : 0;
	case TBM_GETTIC:
		if(s != nullptr)
		{
			// Index among the tics between the ends.
			const std::vector<int> v = TicValues(hwnd, *s);
			const std::size_t i = static_cast<std::size_t>(wp) + 1;
			return (v.size() > 2 && i < v.size() - 1) ? v[i] : -1;
		}
		return -1;
	case TBM_GETTICPOS:
		if(s != nullptr)
		{
			const std::vector<int> v = TicValues(hwnd, *s);
			const std::size_t i = static_cast<std::size_t>(wp) + 1;
			return (v.size() > 2 && i < v.size() - 1) ? TicCoord(hwnd, *s, v[i]) : -1;
		}
		return -1;
	case TBM_GETPTICS:
		if(s != nullptr && !s->tics.empty() && !(StyleOf(hwnd) & TBS_AUTOTICKS))
			return reinterpret_cast<LRESULT>(s->tics.data());
		return 0;
	case TBM_SETSEL:
		if(s != nullptr)
		{
			s->selStart = static_cast<short>(LOWORD(lp));
			s->selEnd = static_cast<short>(HIWORD(lp));
			if(wp)
				::InvalidateRect(hwnd, nullptr, FALSE);
		}
		return 0;
	case TBM_SETSELSTART:
	case TBM_SETSELEND:
		if(s != nullptr)
		{
			(msg == TBM_SETSELSTART ? s->selStart : s->selEnd) = static_cast<int>(lp);
			if(wp)
				::InvalidateRect(hwnd, nullptr, FALSE);
		}
		return 0;
	case TBM_CLEARSEL:
		if(s != nullptr)
		{
			s->selStart = s->selEnd = 0;
			if(wp)
				::InvalidateRect(hwnd, nullptr, FALSE);
		}
		return 0;
	case TBM_GETTHUMBRECT:
		if(s != nullptr && lp != 0)
			*reinterpret_cast<RECT *>(lp) = ToRECT(ThumbRect(hwnd, *s, s->pos));
		return 0;
	case TBM_GETCHANNELRECT:
		if(s != nullptr && lp != 0)
		{
			// Always horizontal coordinates, as on Windows.
			QRect r = ChannelRect(hwnd, *s);
			if(Vertical(hwnd))
				r = QRect(r.top(), r.left(), r.height(), r.width());
			*reinterpret_cast<RECT *>(lp) = ToRECT(r);
		}
		return 0;
	case TBM_SETTHUMBLENGTH:
		if(s != nullptr)
			s->thumbLength = static_cast<int>(wp);
		return 0;
	case TBM_SETBUDDY:
		if(s != nullptr)
		{
			HWND &slot = s->buddy[wp ? 0 : 1];
			const HWND old = slot;
			slot = reinterpret_cast<HWND>(lp);
			PlaceBuddies(hwnd, *s);
			return reinterpret_cast<LRESULT>(old);
		}
		return 0;
	case TBM_GETBUDDY:
		return s ? reinterpret_cast<LRESULT>(s->buddy[wp ? 0 : 1]) : 0;
	default:
		return ::DefWindowProc(hwnd, msg, wp, lp);
	}
}

// ---- up-down controls ----------------------------------------------------------------

constexpr UINT_PTR UpDownRepeatTimer = 1;
constexpr int UpDownInitialDelay = 500;
constexpr int UpDownRepeatDelay = 50;

enum UpDownPart
{
	PartNone = 0,
	PartIncrement = 1,
	PartDecrement = 2,
};

struct UpDownState : ControlState
{
	int lower = 100; // UDM_SETRANGE's "lower" and "upper", as on Windows
	int upper = 0;
	int pos = 0;
	int base = 10;
	std::vector<UDACCEL> accel{{0, 1}};
	HWND buddy = nullptr;
	int pressed = PartNone;
	bool overPressed = false;
	QElapsedTimer pressTime;
	int wheelDelta = 0;
	bool keyRepeat = false;
};

// Buddies subclassed by an up-down control: the control, and the buddy's
// window procedure before.
struct BuddyLink
{
	HWND upDown;
	WNDPROC old;
};
std::map<HWND, BuddyLink> &BuddyLinks()
{
	static std::map<HWND, BuddyLink> links;
	return links;
}

bool Horizontal(HWND hwnd)
{
	return (StyleOf(hwnd) & UDS_HORZ) != 0;
}

bool InBounds(const UpDownState &s, int v)
{
	return v >= std::min(s.lower, s.upper) && v <= std::max(s.lower, s.upper);
}

bool IsEditBuddy(HWND buddy)
{
	WCHAR name[32]{};
	return buddy != nullptr && ::GetClassName(buddy, name, 32) > 0 && ToQString(name).compare(QLatin1String("Edit"), Qt::CaseInsensitive) == 0;
}

void SetBuddyInt(HWND hwnd, const UpDownState &s)
{
	if(!(StyleOf(hwnd) & UDS_SETBUDDYINT) || s.buddy == nullptr || !::IsWindow(s.buddy))
		return;
	QString text;
	if(s.base == 16)
	{
		text = QStringLiteral("0x%1").arg(static_cast<uint>(s.pos), 4, 16, QLatin1Char('0')).toUpper().replace(QLatin1String("0X"), QLatin1String("0x"));
	} else
	{
		text = QString::number(s.pos);
		if(!(StyleOf(hwnd) & UDS_NOTHOUSANDS) && std::abs(s.pos) >= 1000)
		{
			const QString sep = QLocale().groupSeparator();
			const int digitsStart = s.pos < 0 ? 1 : 0;
			for(int i = static_cast<int>(text.size()) - 3; i > digitsStart; i -= 3)
				text.insert(i, sep);
		}
	}
	const std::wstring w = ToWString(text);
	WCHAR current[64]{};
	::GetWindowText(s.buddy, current, 64);
	if(w != current)
		::SetWindowText(s.buddy, w.c_str());
}

// Takes the position from the buddy's text; false if it is not a number in range.
bool GetBuddyInt(HWND hwnd, UpDownState &s)
{
	if(!(StyleOf(hwnd) & UDS_SETBUDDYINT) || s.buddy == nullptr || !::IsWindow(s.buddy))
		return false;
	WCHAR buf[64]{};
	::GetWindowText(s.buddy, buf, 64);
	QString text = ToQString(buf).trimmed();
	text.remove(QLocale().groupSeparator());
	text.remove(QLatin1Char(','));
	bool ok = false;
	int value;
	if(s.base == 16)
	{
		if(text.startsWith(QLatin1String("0x"), Qt::CaseInsensitive))
			text = text.mid(2);
		value = text.isEmpty() ? 0 : text.toInt(&ok, 16);
		ok = ok || text.isEmpty();
	} else
	{
		value = text.isEmpty() ? 0 : text.toInt(&ok, 10);
		ok = ok || text.isEmpty();
	}
	if(!ok || !InBounds(s, value))
		return false;
	s.pos = value;
	return true;
}

// Steps the position by delta toward "upper"; notifies UDN_DELTAPOS first.
void UpDownAction(HWND hwnd, UpDownState &s, int delta)
{
	GetBuddyInt(hwnd, s);
	if(s.upper < s.lower)
		delta = -delta;
	NMUPDOWN nm{};
	nm.iPos = s.pos;
	nm.iDelta = delta;
	if(Notify(hwnd, nm.hdr, UDN_DELTAPOS) != 0)
		return;
	UpDownState *live = Control<UpDownState>(hwnd);
	if(live == nullptr)
		return;
	delta = nm.iDelta;
	if(delta != 0)
	{
		const int lo = std::min(live->lower, live->upper), hi = std::max(live->lower, live->upper);
		int v = live->pos + delta;
		if(v < lo || v > hi)
		{
			if(StyleOf(hwnd) & UDS_WRAP)
				v = v > hi ? lo : hi;
			else
				v = std::clamp(v, lo, hi);
		}
		live->pos = v;
		SetBuddyInt(hwnd, *live);
	}
	const HWND parent = ::GetParent(hwnd);
	if(parent != nullptr)
		::SendMessage(parent, Horizontal(hwnd) ? WM_HSCROLL : WM_VSCROLL, MAKEWPARAM(SB_THUMBPOSITION, static_cast<WORD>(live->pos)), reinterpret_cast<LPARAM>(hwnd));
}

int CurrentIncrement(const UpDownState &s)
{
	const qint64 seconds = s.pressTime.isValid() ? s.pressTime.elapsed() / 1000 : 0;
	int inc = s.accel.empty() ? 1 : static_cast<int>(s.accel.front().nInc);
	for(const UDACCEL &a : s.accel)
	{
		if(seconds >= static_cast<qint64>(a.nSec))
			inc = static_cast<int>(a.nInc);
	}
	return inc;
}

void SendEndScroll(HWND hwnd, const UpDownState &s)
{
	const HWND parent = ::GetParent(hwnd);
	if(parent != nullptr)
		::SendMessage(parent, Horizontal(hwnd) ? WM_HSCROLL : WM_VSCROLL, MAKEWPARAM(SB_ENDSCROLL, static_cast<WORD>(s.pos)), reinterpret_cast<LPARAM>(hwnd));
}

QRect PartRect(HWND hwnd, int part)
{
	const QRect r = ToRect(ClientRectOf(hwnd));
	if(Horizontal(hwnd))
	{
		const int half = r.width() / 2;
		// Left arrow decrements, right arrow increments.
		return part == PartDecrement ? QRect(r.left(), r.top(), half, r.height()) : QRect(r.left() + half, r.top(), r.width() - half, r.height());
	}
	const int half = r.height() / 2;
	return part == PartIncrement ? QRect(r.left(), r.top(), r.width(), half) : QRect(r.left(), r.top() + half, r.width(), r.height() - half);
}

int PartAt(HWND hwnd, POINT pt)
{
	const QPoint p(pt.x, pt.y);
	if(PartRect(hwnd, PartIncrement).contains(p))
		return PartIncrement;
	if(PartRect(hwnd, PartDecrement).contains(p))
		return PartDecrement;
	return PartNone;
}

void PaintUpDown(HWND hwnd, HDC dc)
{
	UpDownState *s = Control<UpDownState>(hwnd);
	if(s == nullptr)
		return;
	const RECT rc = ClientRectOf(hwnd);
	::FillRect(dc, &rc, ::GetSysColorBrush(COLOR_BTNFACE));
	QPainter *p = DCPainter(dc);
	if(p == nullptr)
		return;
	QStyle *qs = QApplication::style();
	const bool horz = Horizontal(hwnd);
	for(int part : {PartIncrement, PartDecrement})
	{
		const QRect r = PartRect(hwnd, part);
		QStyleOptionButton opt;
		InitOption(opt, hwnd, r);
		const bool down = s->pressed == part && s->overPressed;
		opt.state |= down ? QStyle::State_Sunken : QStyle::State_Raised;
		p->save();
		qs->drawPrimitive(QStyle::PE_PanelButtonBevel, &opt, p, nullptr);
		QStyleOption arrow = opt;
		const int inset = std::max(2, std::min(r.width(), r.height()) / 4);
		arrow.rect = r.adjusted(inset, inset, -inset, -inset);
		if(down)
			arrow.rect.translate(qs->pixelMetric(QStyle::PM_ButtonShiftHorizontal, &opt), qs->pixelMetric(QStyle::PM_ButtonShiftVertical, &opt));
		QStyle::PrimitiveElement pe;
		if(horz)
			pe = part == PartIncrement ? QStyle::PE_IndicatorArrowRight : QStyle::PE_IndicatorArrowLeft;
		else
			pe = part == PartIncrement ? QStyle::PE_IndicatorArrowUp : QStyle::PE_IndicatorArrowDown;
		qs->drawPrimitive(pe, &arrow, p, nullptr);
		p->restore();
	}
}

void SetBuddy(HWND hwnd, UpDownState &s, HWND buddy);

// The buddy's keys and wheel go to its up-down control.
LRESULT CALLBACK BuddySubclassProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	auto it = BuddyLinks().find(hwnd);
	if(it == BuddyLinks().end())
		return ::DefWindowProc(hwnd, msg, wp, lp);
	const BuddyLink link = it->second;
	if(::IsWindow(link.upDown))
	{
		switch(msg)
		{
		case WM_KEYDOWN:
		case WM_KEYUP:
			if((wp == VK_UP || wp == VK_DOWN) && (StyleOf(link.upDown) & UDS_ARROWKEYS))
			{
				::SendMessage(link.upDown, msg, wp, lp);
				return 0;
			}
			break;
		case WM_MOUSEWHEEL:
			::SendMessage(link.upDown, msg, wp, lp);
			return 0;
		default:
			break;
		}
	}
	if(msg == WM_NCDESTROY)
	{
		::SetWindowLongPtr(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(link.old));
		BuddyLinks().erase(hwnd);
		if(UpDownState *s = Control<UpDownState>(link.upDown); s != nullptr && s->buddy == hwnd)
			s->buddy = nullptr;
	}
	return ::CallWindowProc(link.old, hwnd, msg, wp, lp);
}

void Unsubclass(HWND buddy)
{
	auto it = BuddyLinks().find(buddy);
	if(it == BuddyLinks().end())
		return;
	if(::IsWindow(buddy) && ::GetWindowLongPtr(buddy, GWLP_WNDPROC) == reinterpret_cast<LONG_PTR>(&BuddySubclassProc))
		::SetWindowLongPtr(buddy, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(it->second.old));
	BuddyLinks().erase(it);
}

void SetBuddy(HWND hwnd, UpDownState &s, HWND buddy)
{
	if(s.buddy != nullptr)
		Unsubclass(s.buddy);
	s.buddy = (buddy != nullptr && ::IsWindow(buddy)) ? buddy : nullptr;
	if(s.buddy == nullptr)
		return;
	if(BuddyLinks().count(s.buddy) == 0)
	{
		BuddyLinks()[s.buddy] = BuddyLink{hwnd, reinterpret_cast<WNDPROC>(::GetWindowLongPtr(s.buddy, GWLP_WNDPROC))};
		::SetWindowLongPtr(s.buddy, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&BuddySubclassProc));
	}
	const LONG_PTR style = StyleOf(hwnd);
	if(style & (UDS_ALIGNLEFT | UDS_ALIGNRIGHT))
	{
		// The up-down takes its width from the buddy's side and its height.
		const HWND parent = ::GetParent(hwnd);
		RECT bud{};
		::GetWindowRect(s.buddy, &bud);
		::MapWindowPoints(HWND_DESKTOP, parent, reinterpret_cast<POINT *>(&bud), 2);
		RECT self{};
		::GetWindowRect(hwnd, &self);
		const int width = self.right - self.left;
		int x;
		if(style & UDS_ALIGNLEFT)
		{
			x = bud.left;
			bud.left += width;
		} else
		{
			bud.right -= width;
			x = bud.right;
		}
		::SetWindowPos(s.buddy, nullptr, bud.left, bud.top, bud.right - bud.left, bud.bottom - bud.top, SWP_NOACTIVATE | SWP_NOZORDER);
		::SetWindowPos(hwnd, nullptr, x, bud.top, width, bud.bottom - bud.top, SWP_NOACTIVATE | SWP_NOZORDER);
	}
}

void EndPress(HWND hwnd, UpDownState &s)
{
	if(s.pressed == PartNone)
		return;
	s.pressed = PartNone;
	s.pressTime.invalidate();
	::KillTimer(hwnd, UpDownRepeatTimer);
	::InvalidateRect(hwnd, nullptr, FALSE);
	SendEndScroll(hwnd, s);
	if(UpDownState *live = Control<UpDownState>(hwnd); live != nullptr && IsEditBuddy(live->buddy))
		::SendMessage(live->buddy, EM_SETSEL, 0, -1);
}

LRESULT CALLBACK UpDownProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	UpDownState *s = Control<UpDownState>(hwnd);
	switch(msg)
	{
	case WM_NCCREATE:
		if(WindowData *d = Data(hwnd))
			d->control = std::make_unique<UpDownState>();
		return ::DefWindowProc(hwnd, msg, wp, lp);
	case WM_CREATE:
		if(s != nullptr && (StyleOf(hwnd) & UDS_AUTOBUDDY))
			SetBuddy(hwnd, *s, ::GetWindow(hwnd, GW_HWNDPREV));
		return 0;
	case WM_DESTROY:
		if(s != nullptr)
		{
			Unsubclass(s->buddy);
			s->buddy = nullptr;
		}
		return ::DefWindowProc(hwnd, msg, wp, lp);
	case WM_ERASEBKGND:
		return 1;
	case WM_PAINT:
	case WM_PRINTCLIENT:
		return PaintMessage(hwnd, msg, wp, [hwnd](HDC dc) { PaintUpDown(hwnd, dc); });
	case WM_ENABLE:
	case WM_SIZE:
		::InvalidateRect(hwnd, nullptr, FALSE);
		return 0;
	case WM_LBUTTONDOWN:
	case WM_LBUTTONDBLCLK:
		if(s != nullptr && ::IsWindowEnabled(hwnd))
		{
			const int part = PartAt(hwnd, POINT{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)});
			if(part == PartNone)
				return 0;
			if(IsEditBuddy(s->buddy))
			{
				::SetFocus(s->buddy);
				if((s = Control<UpDownState>(hwnd)) == nullptr)
					return 0;
				::SendMessage(s->buddy, EM_SETSEL, 0, -1);
			}
			s->pressed = part;
			s->overPressed = true;
			s->pressTime.start();
			::SetCapture(hwnd);
			::InvalidateRect(hwnd, nullptr, FALSE);
			UpDownAction(hwnd, *s, part == PartIncrement ? CurrentIncrement(*s) : -CurrentIncrement(*s));
			if(Control<UpDownState>(hwnd) != nullptr)
				::SetTimer(hwnd, UpDownRepeatTimer, UpDownInitialDelay, nullptr);
		}
		return 0;
	case WM_MOUSEMOVE:
		if(s != nullptr && s->pressed != PartNone)
		{
			const bool over = PartAt(hwnd, POINT{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)}) == s->pressed;
			if(over != s->overPressed)
			{
				s->overPressed = over;
				::InvalidateRect(hwnd, nullptr, FALSE);
			}
		}
		return 0;
	case WM_TIMER:
		if(s != nullptr && wp == UpDownRepeatTimer)
		{
			if(s->pressed == PartNone)
			{
				::KillTimer(hwnd, UpDownRepeatTimer);
				return 0;
			}
			::SetTimer(hwnd, UpDownRepeatTimer, UpDownRepeatDelay, nullptr);
			if(s->overPressed)
				UpDownAction(hwnd, *s, s->pressed == PartIncrement ? CurrentIncrement(*s) : -CurrentIncrement(*s));
			return 0;
		}
		return ::DefWindowProc(hwnd, msg, wp, lp);
	case WM_LBUTTONUP:
		if(s != nullptr && ::GetCapture() == hwnd)
		{
			::ReleaseCapture();
			if((s = Control<UpDownState>(hwnd)) != nullptr)
				EndPress(hwnd, *s);
		}
		return 0;
	case WM_CAPTURECHANGED:
		if(s != nullptr)
			EndPress(hwnd, *s);
		return 0;
	case WM_KEYDOWN:
		if(s != nullptr && (StyleOf(hwnd) & UDS_ARROWKEYS) && ::IsWindowEnabled(hwnd) && (wp == VK_UP || wp == VK_DOWN))
		{
			if(!s->keyRepeat)
				s->pressTime.start();
			s->keyRepeat = true;
			UpDownAction(hwnd, *s, wp == VK_UP ? CurrentIncrement(*s) : -CurrentIncrement(*s));
			return 0;
		}
		return ::DefWindowProc(hwnd, msg, wp, lp);
	case WM_KEYUP:
		if(s != nullptr && s->keyRepeat && (wp == VK_UP || wp == VK_DOWN))
		{
			s->keyRepeat = false;
			s->pressTime.invalidate();
			SendEndScroll(hwnd, *s);
			return 0;
		}
		return ::DefWindowProc(hwnd, msg, wp, lp);
	case WM_MOUSEWHEEL:
		if(s != nullptr && ::IsWindowEnabled(hwnd))
		{
			s->wheelDelta += GET_WHEEL_DELTA_WPARAM(wp);
			const int notches = s->wheelDelta / WHEEL_DELTA;
			if(notches != 0)
			{
				s->wheelDelta -= notches * WHEEL_DELTA;
				const int inc = s->accel.empty() ? 1 : static_cast<int>(s->accel.front().nInc);
				UpDownAction(hwnd, *s, notches * inc);
				if((s = Control<UpDownState>(hwnd)) != nullptr)
					SendEndScroll(hwnd, *s);
			}
		}
		return 0;

	case UDM_SETRANGE:
		if(s != nullptr)
		{
			s->upper = static_cast<short>(LOWORD(lp));
			s->lower = static_cast<short>(HIWORD(lp));
		}
		return 0;
	case UDM_GETRANGE:
		return s ? static_cast<LRESULT>(static_cast<DWORD>(MAKELONG(static_cast<WORD>(s->upper), static_cast<WORD>(s->lower)))) : 0;
	case UDM_SETRANGE32:
		if(s != nullptr)
		{
			s->lower = static_cast<int>(wp);
			s->upper = static_cast<int>(lp);
		}
		return 0;
	case UDM_GETRANGE32:
		if(s != nullptr)
		{
			if(wp != 0)
				*reinterpret_cast<int *>(wp) = s->lower;
			if(lp != 0)
				*reinterpret_cast<int *>(lp) = s->upper;
		}
		return 0;
	case UDM_SETPOS:
	case UDM_SETPOS32:
		if(s != nullptr)
		{
			const int old = s->pos;
			int v = msg == UDM_SETPOS ? static_cast<short>(LOWORD(lp)) : static_cast<int>(lp);
			v = std::clamp(v, std::min(s->lower, s->upper), std::max(s->lower, s->upper));
			s->pos = v;
			SetBuddyInt(hwnd, *s);
			return msg == UDM_SETPOS ? static_cast<LRESULT>(static_cast<short>(old)) : old;
		}
		return 0;
	case UDM_GETPOS:
		if(s != nullptr)
		{
			const bool ok = GetBuddyInt(hwnd, *s);
			return static_cast<LRESULT>(static_cast<DWORD>(MAKELONG(static_cast<WORD>(s->pos), ok ? 0 : 1)));
		}
		return MAKELONG(0, 1);
	case UDM_GETPOS32:
		if(s != nullptr)
		{
			const bool ok = GetBuddyInt(hwnd, *s);
			if(lp != 0)
				*reinterpret_cast<BOOL *>(lp) = ok ? FALSE : TRUE;
			return s->pos;
		}
		return 0;
	case UDM_SETBUDDY:
		if(s != nullptr)
		{
			const HWND old = s->buddy;
			SetBuddy(hwnd, *s, reinterpret_cast<HWND>(wp));
			return reinterpret_cast<LRESULT>(old);
		}
		return 0;
	case UDM_GETBUDDY:
		return s ? reinterpret_cast<LRESULT>(s->buddy) : 0;
	case UDM_SETBASE:
		if(s != nullptr && (wp == 10 || wp == 16))
		{
			const int old = s->base;
			s->base = static_cast<int>(wp);
			return old;
		}
		return 0;
	case UDM_GETBASE:
		return s ? s->base : 10;
	case UDM_SETACCEL:
		if(s != nullptr && wp > 0 && lp != 0)
		{
			const auto *a = reinterpret_cast<const UDACCEL *>(lp);
			s->accel.assign(a, a + wp);
			return TRUE;
		}
		return FALSE;
	case UDM_GETACCEL:
		if(s != nullptr && lp != 0)
		{
			const std::size_t n = std::min(static_cast<std::size_t>(wp), s->accel.size());
			std::copy_n(s->accel.begin(), n, reinterpret_cast<UDACCEL *>(lp));
			return static_cast<LRESULT>(n);
		}
		return 0;
	default:
		return ::DefWindowProc(hwnd, msg, wp, lp);
	}
}

// ---- progress bars -------------------------------------------------------------------

constexpr UINT_PTR MarqueeTimer = 1;

struct ProgressState : ControlState
{
	int low = 0;
	int high = 100;
	int pos = 0;
	int step = 10;
	bool marquee = false;
	int marqueePhase = 0;
	int state = PBST_NORMAL;
	COLORREF barColor = CLR_DEFAULT;
	COLORREF bkColor = CLR_DEFAULT;
};

void PaintProgress(HWND hwnd, HDC dc)
{
	ProgressState *s = Control<ProgressState>(hwnd);
	if(s == nullptr)
		return;
	const RECT rc = ClientRectOf(hwnd);
	if(s->bkColor != CLR_DEFAULT)
	{
		HBRUSH b = ::CreateSolidBrush(s->bkColor);
		::FillRect(dc, &rc, b);
		::DeleteObject(b);
	} else
	{
		::FillRect(dc, &rc, ::GetSysColorBrush(COLOR_BTNFACE));
	}
	QPainter *p = DCPainter(dc);
	if(p == nullptr)
		return;
	QStyle *qs = QApplication::style();
	const LONG_PTR style = StyleOf(hwnd);
	const bool vert = (style & PBS_VERTICAL) != 0;
	QStyleOptionProgressBar opt;
	InitOption(opt, hwnd, ToRect(rc));
	if(!vert)
		opt.state |= QStyle::State_Horizontal;
	opt.textVisible = false;
	opt.bottomToTop = vert;
	opt.invertedAppearance = false;
	if(s->barColor != CLR_DEFAULT)
		opt.palette.setColor(QPalette::Highlight, ToQColor(s->barColor));
	else if(s->state == PBST_ERROR)
		opt.palette.setColor(QPalette::Highlight, QColor(0xDA, 0x26, 0x26));
	else if(s->state == PBST_PAUSED)
		opt.palette.setColor(QPalette::Highlight, QColor(0xDA, 0xCB, 0x26));
	if(s->bkColor != CLR_DEFAULT)
		opt.palette.setColor(QPalette::Base, ToQColor(s->bkColor));
	p->save();
	if((style & PBS_MARQUEE) && s->marquee)
	{
		// A block moving through the bar.
		opt.minimum = 0;
		opt.maximum = 1;
		opt.progress = 0;
		qs->drawControl(QStyle::CE_ProgressBarGroove, &opt, p, nullptr);
		const QRect contents = qs->subElementRect(QStyle::SE_ProgressBarContents, &opt, nullptr);
		const int length = vert ? contents.height() : contents.width();
		const int block = std::max(8, length / 4);
		const int offset = s->marqueePhase % (length + block) - block;
		QRect chunk = vert ? QRect(contents.left(), contents.bottom() - offset - block + 1, contents.width(), block)
		                   : QRect(contents.left() + offset, contents.top(), block, contents.height());
		chunk &= contents;
		p->fillRect(chunk, opt.palette.color(QPalette::Highlight));
	} else
	{
		opt.minimum = s->low;
		opt.maximum = std::max(s->low + 1, s->high);
		opt.progress = std::clamp(s->pos, opt.minimum, opt.maximum);
		qs->drawControl(QStyle::CE_ProgressBar, &opt, p, nullptr);
	}
	p->restore();
}

int SetProgressPos(HWND hwnd, ProgressState &s, int pos)
{
	const int old = s.pos;
	pos = std::clamp(pos, std::min(s.low, s.high), std::max(s.low, s.high));
	if(pos != s.pos)
	{
		s.pos = pos;
		::InvalidateRect(hwnd, nullptr, FALSE);
	}
	return old;
}

LRESULT CALLBACK ProgressProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	ProgressState *s = Control<ProgressState>(hwnd);
	switch(msg)
	{
	case WM_NCCREATE:
		if(WindowData *d = Data(hwnd))
			d->control = std::make_unique<ProgressState>();
		return ::DefWindowProc(hwnd, msg, wp, lp);
	case WM_ERASEBKGND:
		return 1;
	case WM_PAINT:
	case WM_PRINTCLIENT:
		return PaintMessage(hwnd, msg, wp, [hwnd](HDC dc) { PaintProgress(hwnd, dc); });
	case WM_ENABLE:
	case WM_SIZE:
	case WM_TIMER:
		if(s != nullptr && wp == MarqueeTimer)
		{
			s->marqueePhase += 4;
			::InvalidateRect(hwnd, nullptr, FALSE);
			return 0;
		}
		return ::DefWindowProc(hwnd, msg, wp, lp);
	case PBM_SETRANGE:
		if(s != nullptr)
		{
			const LRESULT old = MAKELONG(static_cast<WORD>(s->low), static_cast<WORD>(s->high));
			s->low = LOWORD(lp);
			s->high = HIWORD(lp);
			::InvalidateRect(hwnd, nullptr, FALSE);
			return old;
		}
		return 0;
	case PBM_SETRANGE32:
		if(s != nullptr)
		{
			const LRESULT old = MAKELONG(static_cast<WORD>(s->low), static_cast<WORD>(s->high));
			s->low = static_cast<int>(wp);
			s->high = static_cast<int>(lp);
			::InvalidateRect(hwnd, nullptr, FALSE);
			return old;
		}
		return 0;
	case PBM_GETRANGE:
		if(s != nullptr)
		{
			if(lp != 0)
				*reinterpret_cast<PBRANGE *>(lp) = PBRANGE{s->low, s->high};
			return wp ? s->low : s->high;
		}
		return 0;
	case PBM_SETPOS:
		return s ? SetProgressPos(hwnd, *s, static_cast<int>(wp)) : 0;
	case PBM_DELTAPOS:
		return s ? SetProgressPos(hwnd, *s, s->pos + static_cast<int>(wp)) : 0;
	case PBM_GETPOS:
		return s ? s->pos : 0;
	case PBM_SETSTEP:
		if(s != nullptr)
		{
			const int old = s->step;
			s->step = static_cast<int>(wp);
			return old;
		}
		return 0;
	case PBM_GETSTEP:
		return s ? s->step : 0;
	case PBM_STEPIT:
		if(s != nullptr)
		{
			const int old = s->pos;
			int v = s->pos + s->step;
			if(s->high > s->low && v > s->high)
				v = (v - s->low) % (s->high - s->low) + s->low;
			s->pos = v;
			::InvalidateRect(hwnd, nullptr, FALSE);
			return old;
		}
		return 0;
	case PBM_SETMARQUEE:
		if(s != nullptr)
		{
			s->marquee = wp != 0;
			if(s->marquee)
				::SetTimer(hwnd, MarqueeTimer, lp != 0 ? static_cast<UINT>(lp) : 30, nullptr);
			else
				::KillTimer(hwnd, MarqueeTimer);
			::InvalidateRect(hwnd, nullptr, FALSE);
			return TRUE;
		}
		return FALSE;
	case PBM_SETBARCOLOR:
	case PBM_SETBKCOLOR:
		if(s != nullptr)
		{
			COLORREF &c = msg == PBM_SETBARCOLOR ? s->barColor : s->bkColor;
			const COLORREF old = c;
			c = static_cast<COLORREF>(lp);
			::InvalidateRect(hwnd, nullptr, FALSE);
			return old;
		}
		return CLR_DEFAULT;
	case PBM_SETSTATE:
		if(s != nullptr)
		{
			const int old = s->state;
			s->state = static_cast<int>(wp);
			::InvalidateRect(hwnd, nullptr, FALSE);
			return old;
		}
		return 0;
	case PBM_GETSTATE:
		return s ? s->state : PBST_NORMAL;
	default:
		return ::DefWindowProc(hwnd, msg, wp, lp);
	}
}

void Register(LPCTSTR name, WNDPROC proc)
{
	WNDCLASS wc{};
	wc.style = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = proc;
	wc.hCursor = ::LoadCursor(nullptr, IDC_ARROW);
	wc.lpszClassName = name;
	::RegisterClass(&wc);
}

} // namespace

void RegisterCommonControlClasses()
{
	Register(TRACKBAR_CLASS, TrackbarProc);
	Register(UPDOWN_CLASS, UpDownProc);
	Register(PROGRESS_CLASS, ProgressProc);
}

} // namespace linopenmpt::qtwin

// The classes are there from the start, as when comctl32 is loaded.
void InitCommonControls()
{
}

BOOL InitCommonControlsEx(const INITCOMMONCONTROLSEX *)
{
	return TRUE;
}
