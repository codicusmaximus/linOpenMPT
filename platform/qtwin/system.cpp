// linOpenMPT platform layer: window scroll bars, cursors, system metrics and
// dialog navigation helpers for Win32 on Qt.
//
// Window scroll bars (WS_HSCROLL / WS_VSCROLL) are QScrollBar children along
// the right and bottom edges. The client area is what they leave free, so
// client coordinates still start at the widget's top-left corner. As on
// Windows, a scroll bar does not move by itself: it sends WM_HSCROLL /
// WM_VSCROLL and shows whatever position the window procedure then sets.
#include "QtWin.h"

#include <QtGui/QCursor>
#include <QtGui/QGuiApplication>
#include <QtGui/QScreen>
#include <QtGui/QStyleHints>
#include <QtWidgets/QApplication>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QStyle>

#include <algorithm>
#include <cstring>

namespace linopenmpt::qtwin
{

namespace
{

int ScrollBarExtent()
{
	return QApplication::style()->pixelMetric(QStyle::PM_ScrollBarExtent);
}

ScrollState *State(HWND hwnd, int bar)
{
	WindowData *d = Data(hwnd);
	if(d == nullptr || (bar != SB_HORZ && bar != SB_VERT))
		return nullptr;
	return &d->scroll[bar];
}

int MaxPos(const ScrollState &s)
{
	return std::max(s.min, s.max - std::max<int>(static_cast<int>(s.page), 1) + 1);
}

bool Needed(const ScrollState &s)
{
	return s.page == 0 ? s.max > s.min : (s.max - s.min + 1) > static_cast<int>(s.page);
}

void Sync(Win32Widget *w, int bar)
{
	ScrollState &s = w->data.scroll[bar];
	QScrollBar *sb = s.bar;
	if(sb == nullptr)
		return;
	const QSignalBlocker block(sb);
	sb->setRange(s.min, MaxPos(s));
	sb->setPageStep(std::max<int>(static_cast<int>(s.page), 1));
	sb->setSingleStep(1);
	if(!s.tracking)
		sb->setValue(s.pos);
	sb->setEnabled(s.enabled && Needed(s) && w->isEnabled());
}

void SendScroll(HWND hwnd, int bar, UINT code, int pos)
{
	::SendMessage(hwnd, bar == SB_VERT ? WM_VSCROLL : WM_HSCROLL, MAKEWPARAM(code, static_cast<WORD>(pos)), 0);
}

QScrollBar *EnsureBar(Win32Widget *w, int bar)
{
	ScrollState &s = w->data.scroll[bar];
	if(s.bar)
		return s.bar;
	auto *sb = new QScrollBar(bar == SB_VERT ? Qt::Vertical : Qt::Horizontal, w);
	sb->setFocusPolicy(Qt::NoFocus);
	s.bar = sb;
	const HWND hwnd = w->hwnd();
	QObject::connect(sb, &QAbstractSlider::actionTriggered, sb, [hwnd, bar, sb](int action)
	{
		ScrollState *s = State(hwnd, bar);
		if(s == nullptr)
			return;
		UINT code;
		switch(action)
		{
		case QAbstractSlider::SliderSingleStepAdd: code = SB_LINEDOWN; break;
		case QAbstractSlider::SliderSingleStepSub: code = SB_LINEUP; break;
		case QAbstractSlider::SliderPageStepAdd: code = SB_PAGEDOWN; break;
		case QAbstractSlider::SliderPageStepSub: code = SB_PAGEUP; break;
		case QAbstractSlider::SliderToMinimum: code = SB_TOP; break;
		case QAbstractSlider::SliderToMaximum: code = SB_BOTTOM; break;
		case QAbstractSlider::SliderMove:
			// The thumb follows the mouse while it is dragged.
			s->tracking = true;
			s->trackPos = sb->sliderPosition();
			SendScroll(hwnd, bar, SB_THUMBTRACK, s->trackPos);
			return;
		default:
			return;
		}
		SendScroll(hwnd, bar, code, 0);
		if((s = State(hwnd, bar)) != nullptr)
		{
			SendScroll(hwnd, bar, SB_ENDSCROLL, 0);
			if((s = State(hwnd, bar)) != nullptr)
				sb->setSliderPosition(s->pos);
		}
	});
	QObject::connect(sb, &QAbstractSlider::sliderReleased, sb, [hwnd, bar]()
	{
		ScrollState *s = State(hwnd, bar);
		if(s == nullptr || !s->tracking)
			return;
		SendScroll(hwnd, bar, SB_THUMBPOSITION, s->trackPos);
		if((s = State(hwnd, bar)) == nullptr)
			return;
		SendScroll(hwnd, bar, SB_ENDSCROLL, 0);
		if((s = State(hwnd, bar)) == nullptr)
			return;
		s->tracking = false;
		Sync(FromHandle(hwnd), bar);
	});
	return sb;
}

// Shows or hides a scroll bar; the client area changes size with it.
void SetShown(Win32Widget *w, int bar, bool shown)
{
	ScrollState &s = w->data.scroll[bar];
	if(s.shown == shown)
		return;
	s.shown = shown;
	const DWORD styleBit = bar == SB_VERT ? WS_VSCROLL : WS_HSCROLL;
	if(shown)
		w->data.style |= styleBit;
	else
		w->data.style &= ~styleBit;
	LayoutScrollBars(w);
	const QRect client = ClientRect(w);
	::InvalidateRect(w->hwnd(), nullptr, TRUE);
	::SendMessage(w->hwnd(), WM_SIZE, SIZE_RESTORED, MAKELPARAM(client.width(), client.height()));
}

} // namespace

QRect ClientRect(const Win32Widget *w)
{
	QRect r = w->rect();
	const int extent = ScrollBarExtent();
	if(w->data.scroll[SB_VERT].shown)
		r.setWidth(std::max(0, r.width() - extent));
	if(w->data.scroll[SB_HORZ].shown)
		r.setHeight(std::max(0, r.height() - extent));
	return r;
}

void LayoutScrollBars(Win32Widget *w)
{
	const int extent = ScrollBarExtent();
	const bool v = w->data.scroll[SB_VERT].shown;
	const bool h = w->data.scroll[SB_HORZ].shown;
	for(int bar : {SB_HORZ, SB_VERT})
	{
		ScrollState &s = w->data.scroll[bar];
		if(!s.shown)
		{
			if(s.bar)
				s.bar->hide();
			continue;
		}
		QScrollBar *sb = EnsureBar(w, bar);
		if(bar == SB_VERT)
			sb->setGeometry(w->width() - extent, 0, extent, std::max(0, w->height() - (h ? extent : 0)));
		else
			sb->setGeometry(0, w->height() - extent, std::max(0, w->width() - (v ? extent : 0)), extent);
		Sync(w, bar);
		sb->show();
		sb->raise();
	}
}

void InitScrollBars(Win32Widget *w)
{
	// Windows created with the styles start with a 0..100 range.
	for(int bar : {SB_HORZ, SB_VERT})
	{
		ScrollState &s = w->data.scroll[bar];
		s = ScrollState{};
		s.shown = (w->data.style & (bar == SB_VERT ? WS_VSCROLL : WS_HSCROLL)) != 0;
	}
	LayoutScrollBars(w);
}

void ScrollStylesChanged(Win32Widget *w, DWORD oldStyle)
{
	for(int bar : {SB_HORZ, SB_VERT})
	{
		const DWORD bit = bar == SB_VERT ? WS_VSCROLL : WS_HSCROLL;
		if((oldStyle & bit) != (w->data.style & bit))
		{
			w->data.scroll[bar].shown = !(w->data.style & bit);
			SetShown(w, bar, (w->data.style & bit) != 0);
		}
	}
}

HCURSOR SystemCursor(Qt::CursorShape shape)
{
	static std::map<int, IconData *> cursors;
	auto &c = cursors[shape];
	if(c == nullptr)
	{
		c = new IconData;
		c->isCursor = true;
		c->sharedCursorShape = shape;
	}
	return reinterpret_cast<HCURSOR>(c);
}

QCursor ToQCursor(HCURSOR cursor)
{
	auto *data = reinterpret_cast<IconData *>(cursor);
	if(data == nullptr)
		return QCursor(Qt::BlankCursor);
	if(data->sharedCursorShape >= 0)
		return QCursor(static_cast<Qt::CursorShape>(data->sharedCursorShape));
	if(!data->image.isNull())
		return QCursor(QPixmap::fromImage(data->image), data->hotspot.x(), data->hotspot.y());
	return QCursor(Qt::ArrowCursor);
}

namespace
{
HCURSOR g_cursor = nullptr;
int g_cursorShowCount = 0;
} // namespace

} // namespace linopenmpt::qtwin

using namespace linopenmpt::qtwin;

// ---- scroll bars -----------------------------------------------------------------

int SetScrollInfo(HWND hwnd, int bar, LPCSCROLLINFO info, BOOL)
{
	Win32Widget *w = FromHandle(hwnd);
	ScrollState *s = State(hwnd, bar);
	if(w == nullptr || s == nullptr || info == nullptr)
		return 0;
	if(info->fMask & SIF_RANGE)
	{
		s->min = info->nMin;
		s->max = std::max(info->nMin, info->nMax);
	}
	if(info->fMask & SIF_PAGE)
		s->page = std::min<UINT>(info->nPage, static_cast<UINT>(s->max - s->min + 1));
	if(info->fMask & SIF_POS)
		s->pos = info->nPos;
	s->pos = std::clamp(s->pos, s->min, MaxPos(*s));
	if(info->fMask & (SIF_RANGE | SIF_PAGE))
	{
		const bool needed = Needed(*s);
		SetShown(w, bar, needed || (info->fMask & SIF_DISABLENOSCROLL));
		if((s = State(hwnd, bar)) == nullptr)
			return 0;
	}
	Sync(w, bar);
	return s->pos;
}

BOOL GetScrollInfo(HWND hwnd, int bar, LPSCROLLINFO info)
{
	ScrollState *s = State(hwnd, bar);
	if(s == nullptr || info == nullptr)
		return FALSE;
	if(info->fMask & SIF_RANGE)
	{
		info->nMin = s->min;
		info->nMax = s->max;
	}
	if(info->fMask & SIF_PAGE)
		info->nPage = s->page;
	if(info->fMask & SIF_POS)
		info->nPos = s->pos;
	if(info->fMask & SIF_TRACKPOS)
		info->nTrackPos = s->tracking ? s->trackPos : s->pos;
	return TRUE;
}

int GetScrollPos(HWND hwnd, int bar)
{
	ScrollState *s = State(hwnd, bar);
	return s ? s->pos : 0;
}

int SetScrollPos(HWND hwnd, int bar, int pos, BOOL redraw)
{
	ScrollState *s = State(hwnd, bar);
	if(s == nullptr)
		return 0;
	const int old = s->pos;
	SCROLLINFO info{sizeof(SCROLLINFO), SIF_POS, 0, 0, 0, pos, 0};
	SetScrollInfo(hwnd, bar, &info, redraw);
	return old;
}

BOOL SetScrollRange(HWND hwnd, int bar, int minPos, int maxPos, BOOL redraw)
{
	SCROLLINFO info{sizeof(SCROLLINFO), SIF_RANGE, minPos, maxPos, 0, 0, 0};
	return SetScrollInfo(hwnd, bar, &info, redraw) >= 0 && State(hwnd, bar) != nullptr;
}

BOOL GetScrollRange(HWND hwnd, int bar, LPINT minPos, LPINT maxPos)
{
	ScrollState *s = State(hwnd, bar);
	if(s == nullptr)
		return FALSE;
	if(minPos)
		*minPos = s->min;
	if(maxPos)
		*maxPos = s->max;
	return TRUE;
}

BOOL ShowScrollBar(HWND hwnd, int bar, BOOL show)
{
	Win32Widget *w = FromHandle(hwnd);
	if(w == nullptr)
		return FALSE;
	if(bar == SB_BOTH || bar == SB_HORZ)
		SetShown(w, SB_HORZ, show != FALSE);
	if((bar == SB_BOTH || bar == SB_VERT) && FromHandle(hwnd))
		SetShown(w, SB_VERT, show != FALSE);
	return TRUE;
}

BOOL EnableScrollBar(HWND hwnd, UINT bar, UINT arrows)
{
	Win32Widget *w = FromHandle(hwnd);
	if(w == nullptr)
		return FALSE;
	for(int b : {SB_HORZ, SB_VERT})
	{
		if(bar == static_cast<UINT>(b) || bar == SB_BOTH)
		{
			w->data.scroll[b].enabled = (arrows != ESB_DISABLE_BOTH);
			Sync(w, b);
		}
	}
	return TRUE;
}

// ---- cursors -----------------------------------------------------------------------

HCURSOR LoadCursor(HINSTANCE instance, LPCTSTR name)
{
	if(instance != nullptr)
		return LoadResourceIcon(instance, name, true, GetSystemMetrics(SM_CXCURSOR), GetSystemMetrics(SM_CYCURSOR), true);
	if(!IS_INTRESOURCE(name))
		return nullptr;
	switch(static_cast<WORD>(reinterpret_cast<ULONG_PTR>(name)))
	{
	case 32512: return SystemCursor(Qt::ArrowCursor);
	case 32513: return SystemCursor(Qt::IBeamCursor);
	case 32514: return SystemCursor(Qt::WaitCursor);
	case 32515: return SystemCursor(Qt::CrossCursor);
	case 32516: return SystemCursor(Qt::UpArrowCursor);
	case 32642: return SystemCursor(Qt::SizeFDiagCursor);
	case 32643: return SystemCursor(Qt::SizeBDiagCursor);
	case 32644: return SystemCursor(Qt::SizeHorCursor);
	case 32645: return SystemCursor(Qt::SizeVerCursor);
	case 32646: return SystemCursor(Qt::SizeAllCursor);
	case 32648: return SystemCursor(Qt::ForbiddenCursor);
	case 32649: return SystemCursor(Qt::PointingHandCursor);
	case 32650: return SystemCursor(Qt::BusyCursor); // IDC_APPSTARTING
	default: return nullptr;
	}
}

HCURSOR SetCursor(HCURSOR cursor)
{
	const HCURSOR old = g_cursor;
	g_cursor = cursor;
	// Win32 sets the cursor for wherever the mouse is now; Qt keeps a cursor
	// per widget, so set it on the window under the mouse (or with capture).
	Win32Widget *target = FromHandle(::GetCapture());
	if(target == nullptr)
		target = dynamic_cast<Win32Widget *>(QApplication::widgetAt(QCursor::pos()));
	if(target != nullptr && g_cursorShowCount >= 0)
		target->setCursor(ToQCursor(cursor));
	return old;
}

HCURSOR GetCursor()
{
	return g_cursor;
}

int ShowCursor(BOOL show)
{
	g_cursorShowCount += show ? 1 : -1;
	if(g_cursorShowCount < 0)
		QGuiApplication::setOverrideCursor(QCursor(Qt::BlankCursor));
	else if(!show || g_cursorShowCount == 0)
		QGuiApplication::restoreOverrideCursor();
	return g_cursorShowCount;
}

HICON LoadIcon(HINSTANCE instance, LPCTSTR name)
{
	if(instance != nullptr)
		return LoadResourceIcon(instance, name, false, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), true);
	if(!IS_INTRESOURCE(name))
		return nullptr;
	QStyle::StandardPixmap pixmap;
	switch(static_cast<WORD>(reinterpret_cast<ULONG_PTR>(name)))
	{
	case 32512: pixmap = QStyle::SP_DesktopIcon; break;          // IDI_APPLICATION
	case 32513: pixmap = QStyle::SP_MessageBoxCritical; break;   // IDI_ERROR
	case 32514: pixmap = QStyle::SP_MessageBoxQuestion; break;   // IDI_QUESTION
	case 32515: pixmap = QStyle::SP_MessageBoxWarning; break;    // IDI_WARNING
	case 32516: pixmap = QStyle::SP_MessageBoxInformation; break; // IDI_INFORMATION
	case 32517: pixmap = QStyle::SP_DesktopIcon; break;          // IDI_WINLOGO
	case 32518: pixmap = QStyle::SP_MessageBoxWarning; break;    // IDI_SHIELD
	default: return nullptr;
	}
	static std::map<int, IconData *> icons;
	auto &icon = icons[pixmap];
	if(icon == nullptr)
	{
		icon = new IconData;
		icon->shared = true;
		const int size = GetSystemMetrics(SM_CXICON);
		icon->image = QApplication::style()->standardIcon(pixmap).pixmap(size, size).toImage();
	}
	return reinterpret_cast<HICON>(icon);
}

BOOL DestroyCursor(HCURSOR cursor)
{
	auto *data = reinterpret_cast<IconData *>(cursor);
	if(data == nullptr || data->shared || data->sharedCursorShape >= 0)
		return TRUE; // shared
	delete data;
	return TRUE;
}

BOOL DestroyIcon(HICON icon)
{
	return DestroyCursor(icon);
}

// ---- system metrics and parameters --------------------------------------------------

int GetSystemMetrics(int index)
{
	QStyle *style = QApplication::style();
	const QScreen *screen = QGuiApplication::primaryScreen();
	const QRect geometry = screen ? screen->geometry() : QRect(0, 0, 1920, 1080);
	const QRect virtualGeometry = screen ? screen->virtualGeometry() : geometry;
	const QRect available = screen ? screen->availableGeometry() : geometry;
	switch(index)
	{
	case SM_CXSCREEN: return geometry.width();
	case SM_CYSCREEN: return geometry.height();
	case SM_CXVSCROLL:
	case SM_CYHSCROLL:
	case SM_CYVSCROLL:
	case SM_CXHSCROLL:
		return style->pixelMetric(QStyle::PM_ScrollBarExtent);
	case SM_CYVTHUMB:
	case SM_CXHTHUMB:
		return style->pixelMetric(QStyle::PM_ScrollBarSliderMin);
	case SM_CYCAPTION: return style->pixelMetric(QStyle::PM_TitleBarHeight);
	case SM_CXBORDER:
	case SM_CYBORDER: return 1;
	case SM_CXDLGFRAME:
	case SM_CYDLGFRAME:
	case SM_CXFRAME:
	case SM_CYFRAME: return style->pixelMetric(QStyle::PM_DefaultFrameWidth) + 2;
	case SM_CXEDGE:
	case SM_CYEDGE: return 2;
	case SM_CXICON:
	case SM_CYICON: return style->pixelMetric(QStyle::PM_LargeIconSize);
	case SM_CXSMICON:
	case SM_CYSMICON: return style->pixelMetric(QStyle::PM_SmallIconSize);
	case SM_CXCURSOR:
	case SM_CYCURSOR: return 32;
	case SM_CYMENU: return style->pixelMetric(QStyle::PM_MenuBarItemSpacing) + QApplication::fontMetrics().height() + 8;
	case SM_CXFULLSCREEN: return available.width();
	case SM_CYFULLSCREEN: return available.height();
	case SM_SWAPBUTTON: return 0;
	case SM_CXMIN: return 112;
	case SM_CYMIN: return 27;
	case SM_CXSIZE:
	case SM_CYSIZE: return style->pixelMetric(QStyle::PM_TitleBarButtonSize);
	case SM_CXDOUBLECLK:
	case SM_CYDOUBLECLK: return 4;
	case SM_CXMENUCHECK:
	case SM_CYMENUCHECK: return style->pixelMetric(QStyle::PM_IndicatorWidth);
	case SM_XVIRTUALSCREEN: return virtualGeometry.x();
	case SM_YVIRTUALSCREEN: return virtualGeometry.y();
	case SM_CXVIRTUALSCREEN: return virtualGeometry.width();
	case SM_CYVIRTUALSCREEN: return virtualGeometry.height();
	case SM_CMONITORS: return static_cast<int>(QGuiApplication::screens().size());
	case SM_CXPADDEDBORDER: return 0;
	default: return 0;
	}
}

UINT GetDoubleClickTime()
{
	return static_cast<UINT>(QGuiApplication::styleHints()->mouseDoubleClickInterval());
}

BOOL SystemParametersInfo(UINT action, UINT, PVOID data, UINT)
{
	switch(action)
	{
	case SPI_GETWORKAREA:
		if(data)
		{
			const QScreen *screen = QGuiApplication::primaryScreen();
			const QRect r = screen ? screen->availableGeometry() : QRect(0, 0, 1920, 1080);
			*static_cast<RECT *>(data) = RECT{r.left(), r.top(), r.left() + r.width(), r.top() + r.height()};
		}
		return TRUE;
	case SPI_GETNONCLIENTMETRICS:
		if(data)
		{
			auto *m = static_cast<NONCLIENTMETRICS *>(data);
			const UINT size = m->cbSize;
			std::memset(m, 0, sizeof(NONCLIENTMETRICS));
			m->cbSize = size;
			m->iBorderWidth = 1;
			m->iScrollWidth = m->iScrollHeight = GetSystemMetrics(SM_CXVSCROLL);
			m->iCaptionWidth = m->iCaptionHeight = GetSystemMetrics(SM_CYCAPTION);
			m->iSmCaptionWidth = m->iSmCaptionHeight = GetSystemMetrics(SM_CYCAPTION);
			m->iMenuWidth = m->iMenuHeight = GetSystemMetrics(SM_CYMENU);
			// Every UI font is the application font.
			LOGFONT lf{};
			const QFont font = QApplication::font();
			lf.lfHeight = -QFontInfo(font).pixelSize();
			lf.lfWeight = font.weight();
			lf.lfCharSet = DEFAULT_CHARSET;
			const std::wstring face = ToWString(QFontInfo(font).family());
			std::wcsncpy(lf.lfFaceName, face.c_str(), LF_FACESIZE - 1);
			m->lfCaptionFont = m->lfSmCaptionFont = m->lfMenuFont = m->lfStatusFont = m->lfMessageFont = lf;
		}
		return TRUE;
	case SPI_GETKEYBOARDDELAY:
		if(data)
			*static_cast<int *>(data) = 1; // 0..3, about 500 ms
		return TRUE;
	case SPI_GETKEYBOARDSPEED:
		if(data)
			*static_cast<DWORD *>(data) = 31;
		return TRUE;
	case SPI_GETWHEELSCROLLLINES:
	case SPI_GETWHEELSCROLLCHARS:
		if(data)
			*static_cast<UINT *>(data) = static_cast<UINT>(QApplication::wheelScrollLines());
		return TRUE;
	case SPI_GETFLATMENU:
		if(data)
			*static_cast<BOOL *>(data) = TRUE;
		return TRUE;
	case SPI_GETHIGHCONTRAST:
		return FALSE;
	default:
		return FALSE;
	}
}

BOOL AdjustWindowRectEx(LPRECT rect, DWORD style, BOOL, DWORD)
{
	// Frames and menu bars belong to the window manager and Qt, outside the
	// window's own geometry; only scroll bars take client space.
	if(rect == nullptr)
		return FALSE;
	if(style & WS_VSCROLL)
		rect->right += GetSystemMetrics(SM_CXVSCROLL);
	if(style & WS_HSCROLL)
		rect->bottom += GetSystemMetrics(SM_CYHSCROLL);
	return TRUE;
}

BOOL AdjustWindowRect(LPRECT rect, DWORD style, BOOL menu)
{
	return AdjustWindowRectEx(rect, style, menu, 0);
}

// ---- dialog navigation ----------------------------------------------------------------

namespace
{
std::vector<HWND> TabOrder(HWND dialog)
{
	std::vector<HWND> items;
	for(HWND c = ::GetWindow(dialog, GW_CHILD); c != nullptr; c = ::GetWindow(c, GW_HWNDNEXT))
		items.push_back(c);
	return items;
}

bool Usable(HWND hwnd)
{
	return ::IsWindowVisible(hwnd) && ::IsWindowEnabled(hwnd);
}
} // namespace

HWND GetNextDlgTabItem(HWND dialog, HWND control, BOOL previous)
{
	const std::vector<HWND> items = TabOrder(dialog);
	if(items.empty())
		return nullptr;
	const auto n = static_cast<std::ptrdiff_t>(items.size());
	std::ptrdiff_t start = std::find(items.begin(), items.end(), control) - items.begin();
	if(start >= n)
		start = previous ? 0 : n - 1;
	for(std::ptrdiff_t step = 1; step <= n; step++)
	{
		const std::ptrdiff_t i = ((start + (previous ? -step : step)) % n + n) % n;
		const HWND c = items[static_cast<std::size_t>(i)];
		if((::GetWindowLongPtr(c, GWL_STYLE) & WS_TABSTOP) && Usable(c))
			return c;
	}
	return control;
}

HWND GetNextDlgGroupItem(HWND dialog, HWND control, BOOL previous)
{
	// A group runs from a WS_GROUP control up to the next one.
	const std::vector<HWND> items = TabOrder(dialog);
	auto it = std::find(items.begin(), items.end(), control);
	if(it == items.end())
		return nullptr;
	auto groupStart = it;
	while(groupStart != items.begin() && !(::GetWindowLongPtr(*groupStart, GWL_STYLE) & WS_GROUP))
		--groupStart;
	auto groupEnd = it + 1;
	while(groupEnd != items.end() && !(::GetWindowLongPtr(*groupEnd, GWL_STYLE) & WS_GROUP))
		++groupEnd;
	const auto n = groupEnd - groupStart;
	const auto pos = it - groupStart;
	for(std::ptrdiff_t step = 1; step <= n; step++)
	{
		const auto i = ((pos + (previous ? -step : step)) % n + n) % n;
		const HWND c = *(groupStart + i);
		if(Usable(c))
			return c;
	}
	return control;
}

void NotifyWinEvent(DWORD, HWND, LONG, LONG)
{
	// Accessibility comes from Qt's own widgets.
}
