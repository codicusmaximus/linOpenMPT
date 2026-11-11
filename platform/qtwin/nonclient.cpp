// The non-client area of windows: borders, the menu bar and window scroll
// bars, around the client area that WM_PAINT, client DCs, mouse messages
// and child windows use.
//
// As on Windows, the client area comes from WM_NCCALCSIZE (so MFC's control
// bars can reserve their own borders), the frame is drawn in WM_NCPAINT, and
// WM_NCHITTEST says what the mouse is over. Captions and frames of top-level
// windows are the window manager's, so a top-level window's non-client area
// is its menu bar and scroll bars; child windows draw their own borders.
#include "QtWin.h"

#include <QtWidgets/QApplication>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QStyle>

#include <algorithm>

namespace linopenmpt::qtwin
{

QMargins FrameMargins(DWORD style, DWORD exStyle, bool child)
{
	if(!child)
		return QMargins();
	int edge = 0;
	if(style & WS_THICKFRAME)
		edge += GetSystemMetrics(SM_CXFRAME);
	else if((style & WS_CAPTION) == WS_DLGFRAME || (style & WS_CAPTION) == WS_CAPTION || (exStyle & WS_EX_DLGMODALFRAME))
		edge += GetSystemMetrics(SM_CXDLGFRAME);
	else if(style & WS_BORDER)
		edge += GetSystemMetrics(SM_CXBORDER);
	if(exStyle & WS_EX_CLIENTEDGE)
		edge += GetSystemMetrics(SM_CXEDGE);
	if(exStyle & WS_EX_STATICEDGE)
		edge += GetSystemMetrics(SM_CXBORDER);
	QMargins m(edge, edge, edge, edge);
	if((style & WS_CAPTION) == WS_CAPTION)
		m.setTop(m.top() + GetSystemMetrics((exStyle & WS_EX_TOOLWINDOW) ? SM_CYSMCAPTION : SM_CYCAPTION));
	return m;
}

namespace
{

bool IsChildWindow(const Win32Widget *w)
{
	return (w->data.style & WS_CHILD) != 0;
}

} // namespace

int MenuBarHeight(const Win32Widget *w)
{
	if(w->data.menuBar == nullptr)
		return 0;
	const QRect frame = w->rect().marginsRemoved(FrameMargins(w->data.style, w->data.exStyle, IsChildWindow(w)));
	// Menu bars wrap to more rows when the window is narrow, as on Windows.
	const int h = w->data.menuBar->heightForWidth(std::max(frame.width(), 1));
	return h > 0 ? h : w->data.menuBar->sizeHint().height();
}

QRect ClientRect(const Win32Widget *w)
{
	QRect r = w->rect().marginsRemoved(w->data.nonClient);
	if(r.width() < 0)
		r.setWidth(0);
	if(r.height() < 0)
		r.setHeight(0);
	return r;
}

QPoint ClientOrigin(const QWidget *widget)
{
	auto *w = dynamic_cast<const Win32Widget *>(widget);
	return w ? QPoint(w->data.nonClient.left(), w->data.nonClient.top()) : QPoint();
}

bool UpdateNonClient(Win32Widget *w)
{
	// WM_NCCALCSIZE works in the parent's client coordinates.
	const QPoint parentOrigin = w->isWindow() ? QPoint() : ClientOrigin(w->parentWidget());
	const QRect window = w->isWindow() ? QRect(QPoint(), w->size()) : w->geometry().translated(-parentOrigin);
	NCCALCSIZE_PARAMS params{};
	params.rgrc[0] = RECT{window.left(), window.top(), window.left() + window.width(), window.top() + window.height()};
	::SendMessage(w->hwnd(), WM_NCCALCSIZE, FALSE, reinterpret_cast<LPARAM>(&params.rgrc[0]));
	const RECT &c = params.rgrc[0];
	const QMargins margins(std::max(0, static_cast<int>(c.left - window.left())), std::max(0, static_cast<int>(c.top - window.top())),
		std::max(0, static_cast<int>(window.left() + window.width() - c.right)), std::max(0, static_cast<int>(window.top() + window.height() - c.bottom)));
	const bool changed = margins != w->data.nonClient;
	// Child windows are placed in client coordinates: when the client area
	// moves inside the window (a menu bar added by SetMenu, a frame style
	// changed), they move with it, as on Windows.
	const QPoint shift(margins.left() - w->data.nonClient.left(), margins.top() - w->data.nonClient.top());
	w->data.nonClient = margins;
	LayoutScrollBars(w);
	LayoutMenuBar(w);
	if(changed)
	{
		w->data.ncDirty = true;
		w->update();
		for(QObject *o : w->children())
		{
			if(auto *c = dynamic_cast<Win32Widget *>(o))
			{
				if(!shift.isNull() && !c->isWindow())
					c->move(c->pos() + shift);
				ClipToParentClient(c);
			}
		}
	}
	return changed;
}

void ClipToParentClient(Win32Widget *w)
{
	auto *parent = w->isWindow() ? nullptr : dynamic_cast<Win32Widget *>(w->parentWidget());
	const QRect full(QPoint(0, 0), w->size());
	QRegion visible = w->data.shape.isEmpty() ? QRegion(full) : w->data.shape;
	// A top-level window (perhaps a child once) is not clipped to a parent.
	if(parent != nullptr)
	{
		const QRect client = ClientRect(parent), g = w->geometry();
		if(!client.contains(g))
			visible &= client.intersected(g).translated(-g.topLeft());
	}
	if(visible == QRegion(full))
	{
		if(w->data.clipMask)
		{
			w->clearMask();
			w->data.clipMask = false;
		}
		return;
	}
	// An empty mask would mean no mask: one pixel outside hides it all.
	w->setMask(visible.isEmpty() ? QRegion(-1, -1, 1, 1) : visible);
	w->data.clipMask = true;
}

void SetShape(Win32Widget *w, const QRegion &shape)
{
	if(w == nullptr || w->data.shape == shape)
		return;
	w->data.shape = shape;
	ClipToParentClient(w);
}

void DefNcCalcSize(Win32Widget *w, RECT *rect)
{
	if(rect == nullptr)
		return;
	const QMargins frame = FrameMargins(w->data.style, w->data.exStyle, IsChildWindow(w));
	rect->left += frame.left();
	rect->top += frame.top();
	rect->right -= frame.right();
	rect->bottom -= frame.bottom();
	if(w->data.menuBar != nullptr && !IsChildWindow(w))
		rect->top += MenuBarHeight(w);
	// Scroll bars are non-client, but only while there is room for them.
	if(w->data.scroll[SB_VERT].shown && rect->right - rect->left >= GetSystemMetrics(SM_CXVSCROLL))
		rect->right -= GetSystemMetrics(SM_CXVSCROLL);
	if(w->data.scroll[SB_HORZ].shown && rect->bottom - rect->top >= GetSystemMetrics(SM_CYHSCROLL))
		rect->bottom -= GetSystemMetrics(SM_CYHSCROLL);
	rect->right = std::max(rect->right, rect->left);
	rect->bottom = std::max(rect->bottom, rect->top);
}

void DefNcPaint(Win32Widget *w)
{
	if(w->data.nonClient.isNull())
		return;
	HDC dc = ::GetWindowDC(w->hwnd());
	if(dc == nullptr)
		return;
	const DWORD style = w->data.style, exStyle = w->data.exStyle;
	RECT r{0, 0, w->width(), w->height()};
	if(IsChildWindow(w))
	{
		if(style & WS_THICKFRAME)
		{
			::DrawEdge(dc, &r, EDGE_RAISED, BF_RECT | BF_ADJUST);
			const int inner = GetSystemMetrics(SM_CXFRAME) - 2;
			for(int i = 0; i < inner; i++)
			{
				::FrameRect(dc, &r, ::GetSysColorBrush(COLOR_BTNFACE));
				::InflateRect(&r, -1, -1);
			}
		} else if((style & WS_CAPTION) == WS_DLGFRAME || (style & WS_CAPTION) == WS_CAPTION || (exStyle & WS_EX_DLGMODALFRAME))
		{
			::DrawEdge(dc, &r, EDGE_RAISED, BF_RECT | BF_ADJUST);
			::FrameRect(dc, &r, ::GetSysColorBrush(COLOR_BTNFACE));
			::InflateRect(&r, -1, -1);
		} else if(style & WS_BORDER)
		{
			::FrameRect(dc, &r, ::GetSysColorBrush(COLOR_WINDOWFRAME));
			::InflateRect(&r, -1, -1);
		}
		if(exStyle & WS_EX_CLIENTEDGE)
			::DrawEdge(dc, &r, EDGE_SUNKEN, BF_RECT | BF_ADJUST);
		if(exStyle & WS_EX_STATICEDGE)
			::DrawEdge(dc, &r, BDR_SUNKENOUTER, BF_RECT | BF_ADJUST);
		if((style & WS_CAPTION) == WS_CAPTION)
		{
			// A child window caption (floating panes inside a frame).
			RECT caption = r;
			caption.bottom = caption.top + GetSystemMetrics((exStyle & WS_EX_TOOLWINDOW) ? SM_CYSMCAPTION : SM_CYCAPTION);
			const bool active = (exStyle & WS_EX_MDICHILD) ? MdiIsActiveChild(w->hwnd()) : (::GetFocus() == w->hwnd() || ::IsChild(w->hwnd(), ::GetFocus()));
			::FillRect(dc, &caption, ::GetSysColorBrush(active ? COLOR_ACTIVECAPTION : COLOR_INACTIVECAPTION));
			::SetBkMode(dc, TRANSPARENT);
			::SetTextColor(dc, ::GetSysColor(active ? COLOR_CAPTIONTEXT : COLOR_INACTIVECAPTIONTEXT));
			HGDIOBJ oldFont = ::SelectObject(dc, ::GetStockObject(DEFAULT_GUI_FONT));
			caption.left += 4;
			for(LRESULT button : {HTMINBUTTON, HTMAXBUTTON, HTCLOSE})
			{
				const QRect b = CaptionButtonRect(w, button);
				if(b.isEmpty())
					continue;
				caption.right = std::min(caption.right, static_cast<LONG>(b.left() - 2));
				RECT br{b.left(), b.top(), b.right() + 1, b.bottom() + 1};
				UINT state = button == HTCLOSE ? DFCS_CAPTIONCLOSE : button == HTMINBUTTON ? (w->data.childShow == SIZE_MINIMIZED ? DFCS_CAPTIONRESTORE : DFCS_CAPTIONMIN)
					: (w->data.childShow == SIZE_MAXIMIZED ? DFCS_CAPTIONRESTORE : DFCS_CAPTIONMAX);
				if(PushedCaptionButton(w) == button)
					state |= DFCS_PUSHED;
				::DrawFrameControl(dc, &br, DFC_CAPTION, state);
			}
			::DrawText(dc, w->data.text.c_str(), -1, &caption, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
			::SelectObject(dc, oldFont);
		}
	}
	// The box between two scroll bars.
	if(w->data.scroll[SB_VERT].shown && w->data.scroll[SB_HORZ].shown)
	{
		const QRect client = ClientRect(w);
		RECT box{client.right() + 1, client.bottom() + 1, client.right() + 1 + GetSystemMetrics(SM_CXVSCROLL), client.bottom() + 1 + GetSystemMetrics(SM_CYHSCROLL)};
		::FillRect(dc, &box, ::GetSysColorBrush(COLOR_BTNFACE));
	}
	::ReleaseDC(w->hwnd(), dc);
}

LRESULT DefNcHitTest(Win32Widget *w, QPoint screen)
{
	const QPoint p = w->mapFromGlobal(screen);
	if(!w->rect().contains(p))
		return HTNOWHERE;
	const QRect client = ClientRect(w);
	if(client.contains(p))
		return HTCLIENT;
	if(w->data.menuBar != nullptr && w->data.menuBar->isVisible() && w->data.menuBar->geometry().contains(p))
		return HTMENU;
	if(w->data.scroll[SB_VERT].shown && p.x() > client.right() && p.y() >= client.top() && p.y() <= client.bottom())
		return HTVSCROLL;
	if(w->data.scroll[SB_HORZ].shown && p.y() > client.bottom() && p.x() >= client.left() && p.x() <= client.right())
		return HTHSCROLL;
	if(w->data.scroll[SB_VERT].shown && w->data.scroll[SB_HORZ].shown && p.x() > client.right() && p.y() > client.bottom())
		return HTSIZE;
	const QMargins frame = FrameMargins(w->data.style, w->data.exStyle, IsChildWindow(w));
	for(LRESULT button : {HTCLOSE, HTMAXBUTTON, HTMINBUTTON})
	{
		if(CaptionButtonRect(w, button).contains(p))
			return button;
	}
	if(IsChildWindow(w) && (w->data.style & WS_CAPTION) == WS_CAPTION)
	{
		const QRect inside = w->rect().marginsRemoved(QMargins(frame.left(), frame.left(), frame.right(), frame.bottom()));
		if(p.y() >= inside.top() && p.y() < frame.top())
			return HTCAPTION;
	}
	if(IsChildWindow(w) && (w->data.style & WS_THICKFRAME))
	{
		const int edge = GetSystemMetrics(SM_CXFRAME);
		const bool left = p.x() < edge, right = p.x() >= w->width() - edge;
		const bool top = p.y() < edge, bottom = p.y() >= w->height() - edge;
		if(top && left) return HTTOPLEFT;
		if(top && right) return HTTOPRIGHT;
		if(bottom && left) return HTBOTTOMLEFT;
		if(bottom && right) return HTBOTTOMRIGHT;
		if(left) return HTLEFT;
		if(right) return HTRIGHT;
		if(top) return HTTOP;
		if(bottom) return HTBOTTOM;
	}
	return HTBORDER;
}

QRect CaptionButtonRect(const Win32Widget *w, LRESULT hit)
{
	const DWORD style = w->data.style, exStyle = w->data.exStyle;
	if(!IsChildWindow(w) || (style & WS_CAPTION) != WS_CAPTION || !(style & WS_SYSMENU))
		return QRect();
	// Right to left: close, then maximize and minimize (both shown, one
	// disabled, when either style is there), as Windows lays them out.
	const QMargins frame = FrameMargins(style, exStyle, true);
	const int captionHeight = GetSystemMetrics((exStyle & WS_EX_TOOLWINDOW) ? SM_CYSMCAPTION : SM_CYCAPTION);
	const int size = std::max(captionHeight - 4, 4);
	const int top = frame.top() - captionHeight + (captionHeight - size) / 2;
	int right = w->width() - frame.right() - 2;
	const bool minMax = (style & (WS_MINIMIZEBOX | WS_MAXIMIZEBOX)) != 0 && !(exStyle & WS_EX_TOOLWINDOW);
	for(LRESULT button : {HTCLOSE, HTMAXBUTTON, HTMINBUTTON})
	{
		if(button != HTCLOSE && !minMax)
			break;
		const QRect r(right - size, top, size, size);
		if(button == hit)
			return r;
		right -= size + (button == HTCLOSE ? 2 : 0);
	}
	return QRect();
}

namespace
{
// The mouse is moving or sizing a child window by its frame, or holding a
// caption button.
struct NcTracking
{
	Win32Widget *widget = nullptr;
	LRESULT hit = HTNOWHERE;
	QPoint start;     // where the mouse went down, on the screen
	QRect startRect;  // the window, in the parent's client coordinates
	bool over = false; // still over the pressed button
};
NcTracking g_tracking;
} // namespace

LRESULT PushedCaptionButton(const Win32Widget *w)
{
	return g_tracking.widget == w && g_tracking.over ? g_tracking.hit : HTNOWHERE;
}

QRect ChildWindowRect(const Win32Widget *w)
{
	return QRect(w->pos() - ClientOrigin(w->parentWidget()), w->size());
}

void BeginNonClientTracking(Win32Widget *w, LRESULT hit, QPoint screen)
{
	g_tracking = NcTracking{w, hit, screen, ChildWindowRect(w), true};
	if(hit == HTCLOSE || hit == HTMINBUTTON || hit == HTMAXBUTTON)
	{
		w->data.ncDirty = true;
		w->update();
	} else
	{
		::SendMessage(w->hwnd(), WM_ENTERSIZEMOVE, 0, 0);
	}
}

bool NonClientTrackingEvent(Win32Widget *w, QMouseEvent *e)
{
	if(g_tracking.widget != w)
		return false;
	const HWND hwnd = w->hwnd();
	const QPoint screen = e->globalPosition().toPoint();
	const LRESULT hit = g_tracking.hit;
	const bool release = e->type() == QEvent::MouseButtonRelease;
	if(hit == HTCLOSE || hit == HTMINBUTTON || hit == HTMAXBUTTON)
	{
		const bool over = CaptionButtonRect(w, hit).contains(w->mapFromGlobal(screen));
		if(over != g_tracking.over || release)
		{
			g_tracking.over = over && !release;
			w->data.ncDirty = true;
			w->update();
		}
		if(release)
		{
			g_tracking = NcTracking{};
			if(over)
			{
				WPARAM command = SC_CLOSE;
				if(hit == HTMAXBUTTON)
					command = w->data.childShow == SIZE_MAXIMIZED ? SC_RESTORE : SC_MAXIMIZE;
				else if(hit == HTMINBUTTON)
					command = w->data.childShow == SIZE_MINIMIZED ? SC_RESTORE : SC_MINIMIZE;
				::SendMessage(hwnd, WM_SYSCOMMAND, command, MAKELPARAM(static_cast<WORD>(static_cast<short>(screen.x())), static_cast<WORD>(static_cast<short>(screen.y()))));
			}
		}
		return true;
	}
	if(e->type() == QEvent::MouseMove || release)
	{
		const QPoint delta = screen - g_tracking.start;
		QRect r = g_tracking.startRect;
		const int minW = std::max(GetSystemMetrics(SM_CXMINTRACK), 0), minH = std::max(GetSystemMetrics(SM_CYMINTRACK), 0);
		if(hit == HTCAPTION)
			r.translate(delta);
		if(hit == HTLEFT || hit == HTTOPLEFT || hit == HTBOTTOMLEFT)
			r.setLeft(std::min(r.left() + delta.x(), r.right() + 1 - minW));
		if(hit == HTRIGHT || hit == HTTOPRIGHT || hit == HTBOTTOMRIGHT)
			r.setRight(std::max(r.right() + delta.x(), r.left() + minW - 1));
		if(hit == HTTOP || hit == HTTOPLEFT || hit == HTTOPRIGHT)
			r.setTop(std::min(r.top() + delta.y(), r.bottom() + 1 - minH));
		if(hit == HTBOTTOM || hit == HTBOTTOMLEFT || hit == HTBOTTOMRIGHT)
			r.setBottom(std::max(r.bottom() + delta.y(), r.top() + minH - 1));
		if(r != ChildWindowRect(w))
		{
			RECT proposed{r.left(), r.top(), r.right() + 1, r.bottom() + 1};
			::SendMessage(hwnd, hit == HTCAPTION ? WM_MOVING : WM_SIZING, hit == HTCAPTION ? 0 : static_cast<WPARAM>(hit - HTLEFT + 1), reinterpret_cast<LPARAM>(&proposed));
			::SetWindowPos(hwnd, nullptr, proposed.left, proposed.top, proposed.right - proposed.left, proposed.bottom - proposed.top, SWP_NOZORDER | SWP_NOACTIVATE | (hit == HTCAPTION ? SWP_NOSIZE : 0));
		}
		if(release)
		{
			g_tracking = NcTracking{};
			::SendMessage(hwnd, WM_EXITSIZEMOVE, 0, 0);
		}
	}
	return true;
}

void LayoutScrollBars(Win32Widget *w)
{
	const QRect client = ClientRect(w);
	const int cx = GetSystemMetrics(SM_CXVSCROLL), cy = GetSystemMetrics(SM_CYHSCROLL);
	for(int bar : {SB_HORZ, SB_VERT})
	{
		ScrollState &s = w->data.scroll[bar];
		if(!s.shown)
		{
			if(s.bar)
				s.bar->hide();
			continue;
		}
		QScrollBar *sb = EnsureScrollBar(w, bar);
		// Next to the client area, as Windows places them.
		if(bar == SB_VERT)
			sb->setGeometry(client.right() + 1, client.top(), cx, client.height());
		else
			sb->setGeometry(client.left(), client.bottom() + 1, client.width(), cy);
		SyncScrollBar(w, bar);
		sb->show();
		sb->raise();
	}
}

} // namespace linopenmpt::qtwin
