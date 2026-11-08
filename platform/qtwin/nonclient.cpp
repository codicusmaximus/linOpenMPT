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
	w->data.nonClient = margins;
	LayoutScrollBars(w);
	LayoutMenuBar(w);
	if(changed)
	{
		w->data.ncDirty = true;
		w->update();
	}
	return changed;
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
			const bool active = ::GetFocus() == w->hwnd() || ::IsChild(w->hwnd(), ::GetFocus());
			::FillRect(dc, &caption, ::GetSysColorBrush(active ? COLOR_ACTIVECAPTION : COLOR_INACTIVECAPTION));
			::SetBkMode(dc, TRANSPARENT);
			::SetTextColor(dc, ::GetSysColor(active ? COLOR_CAPTIONTEXT : COLOR_INACTIVECAPTIONTEXT));
			HGDIOBJ oldFont = ::SelectObject(dc, ::GetStockObject(DEFAULT_GUI_FONT));
			caption.left += 4;
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
