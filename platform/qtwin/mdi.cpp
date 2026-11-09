// The multiple-document interface of user32: the "MDIClient" window class,
// DefFrameProc, DefMDIChildProc, and the WM_MDI* messages.
//
// MDI children are child windows of the client with captions, which they
// draw themselves (nonclient.cpp) and move, size, minimize and maximize
// inside it. As on Windows:
// - one child is active; activating another sends WM_MDIACTIVATE to both,
//   and the focus moving into a child (a click, a key) activates it;
// - a maximized child fills the client with its frame outside, the frame's
//   menu bar shows its minimize, restore and close buttons, and the frame's
//   title becomes "Frame - [Child]"; the next child activated is maximized
//   in turn;
// - minimized children line up along the bottom of the client;
// - the "Window" menu lists the children (ids from idFirstChild), with the
//   active one checked;
// - Ctrl+F4 closes and Ctrl+F6 / Ctrl+Tab cycles (TranslateMDISysAccel).
#include "QtWin.h"

#include <QtWidgets/QApplication>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QStyle>
#include <QtWidgets/QToolButton>

#include <QtCore/QPointer>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace linopenmpt::qtwin
{

namespace
{

constexpr UINT MaxWindowMenuItems = 9; // "&1 ..." to "&9 ...", as Windows lists them

struct MdiClientState : ControlState
{
	HMENU windowMenu = nullptr;
	UINT idFirstChild = 0;
	std::vector<HWND> children; // creation order: the window menu, the ids
	std::vector<HWND> zorder;   // most recently active first
	HWND active = nullptr;
	std::wstring frameTitle;    // the frame's own title, without "- [Child]"
	bool haveFrameTitle = false;
	int cascade = 0;            // the next default position
	QPointer<QWidget> corner;   // the maximized child's buttons in the frame's menu bar
	bool menuSeparator = false; // added to windowMenu before the children
	DWORD scrollStyle = 0;      // WS_HSCROLL / WS_VSCROLL the client was created with
	bool updatingScroll = false;
};

MdiClientState *ClientState(HWND client)
{
	return Control<MdiClientState>(client);
}

// The client of an MDI child.
HWND ClientOf(HWND child)
{
	const HWND parent = ::GetParent(child);
	return parent != nullptr && ClientState(parent) != nullptr ? parent : nullptr;
}

HWND FrameOf(HWND client)
{
	return ::GetParent(client);
}

QRect ClientArea(HWND client)
{
	RECT rc{};
	::GetClientRect(client, &rc);
	return QRect(0, 0, rc.right, rc.bottom);
}

// A maximized child fills the client area, with its frame and caption
// outside it.
QRect MaximizedRect(HWND client, HWND child)
{
	const WindowData *d = Data(child);
	const QMargins m = FrameMargins(d->style, d->exStyle, true);
	const QRect area = ClientArea(client);
	return area.marginsAdded(m);
}

bool IsMaximized(HWND child)
{
	const WindowData *d = Data(child);
	return d != nullptr && d->childShow == SIZE_MAXIMIZED;
}

HWND MaximizedChild(const MdiClientState *s)
{
	return s->active != nullptr && IsMaximized(s->active) ? s->active : nullptr;
}

// ---- the frame: title and menu bar buttons ----------------------------------------

std::wstring WindowText(HWND hwnd)
{
	const WindowData *d = Data(hwnd);
	return d != nullptr ? d->text : std::wstring();
}

void UpdateFrameTitle(HWND client)
{
	MdiClientState *s = ClientState(client);
	const HWND frame = FrameOf(client);
	if(s == nullptr || frame == nullptr)
		return;
	if(!s->haveFrameTitle)
	{
		s->frameTitle = WindowText(frame);
		s->haveFrameTitle = true;
	}
	std::wstring title = s->frameTitle;
	if(const HWND maximized = MaximizedChild(s))
	{
		const std::wstring child = WindowText(maximized);
		if(!child.empty())
			title += L" - [" + child + L"]";
	}
	if(title != WindowText(frame))
		::DefWindowProc(frame, WM_SETTEXT, 0, reinterpret_cast<LPARAM>(title.c_str()));
}

void UpdateCornerButtons(HWND client)
{
	MdiClientState *s = ClientState(client);
	const HWND frame = FrameOf(client);
	WindowData *fd = frame ? Data(frame) : nullptr;
	if(s == nullptr || fd == nullptr || fd->menuBar == nullptr)
		return;
	QMenuBar *bar = fd->menuBar;
	const HWND maximized = MaximizedChild(s);
	if(maximized == nullptr)
	{
		if(s->corner)
			s->corner->hide();
		return;
	}
	if(!s->corner || bar->cornerWidget(Qt::TopRightCorner) != s->corner)
	{
		// Within the menu bar's height, as on Windows.
		const int barHeight = bar->sizeHint().height();
		const int size = std::max(barHeight - 6, 8);
		auto *corner = new QWidget(bar);
		auto *layout = new QHBoxLayout(corner);
		layout->setContentsMargins(0, 0, 2, 0);
		layout->setSpacing(0);
		const struct
		{
			QStyle::StandardPixmap icon;
			WPARAM command;
			const char *name;
		} buttons[] = {
			{QStyle::SP_TitleBarMinButton, SC_MINIMIZE, "minimize"},
			{QStyle::SP_TitleBarNormalButton, SC_RESTORE, "restore"},
			{QStyle::SP_TitleBarCloseButton, SC_CLOSE, "close"},
		};
		for(const auto &b : buttons)
		{
			auto *button = new QToolButton(corner);
			button->setObjectName(QString::fromLatin1(b.name));
			button->setAutoRaise(true);
			button->setFocusPolicy(Qt::NoFocus);
			button->setIcon(QApplication::style()->standardIcon(b.icon));
			button->setFixedSize(size, size);
			button->setIconSize(QSize(size - 4, size - 4));
			const WPARAM command = b.command;
			QObject::connect(button, &QToolButton::clicked, corner, [client, command]()
			{
				if(MdiClientState *st = ClientState(client); st != nullptr && st->active != nullptr)
					::PostMessage(st->active, WM_SYSCOMMAND, command, 0);
			});
			layout->addWidget(button);
		}
		bar->setCornerWidget(corner, Qt::TopRightCorner);
		s->corner = corner;
	}
	s->corner->show();
	bar->adjustSize();
	LayoutMenuBar(FromHandle(frame));
}

// ---- the window menu ----------------------------------------------------------------

void RefreshWindowMenu(HWND client)
{
	MdiClientState *s = ClientState(client);
	if(s == nullptr || s->windowMenu == nullptr || !::IsMenu(static_cast<HMENU>(s->windowMenu)))
		return;
	HMENU menu = s->windowMenu;
	// Remove what we added before.
	for(int i = ::GetMenuItemCount(menu) - 1; i >= 0; i--)
	{
		const UINT id = ::GetMenuItemID(menu, i);
		if(id >= s->idFirstChild && id < s->idFirstChild + MaxWindowMenuItems)
			::DeleteMenu(menu, i, MF_BYPOSITION);
	}
	if(s->menuSeparator)
	{
		const int last = ::GetMenuItemCount(menu) - 1;
		MENUITEMINFO info{};
		info.cbSize = sizeof(info);
		info.fMask = MIIM_FTYPE;
		if(last >= 0 && ::GetMenuItemInfo(menu, last, TRUE, &info) && (info.fType & MFT_SEPARATOR))
			::DeleteMenu(menu, last, MF_BYPOSITION);
		s->menuSeparator = false;
	}
	std::vector<HWND> listed;
	for(HWND child : s->children)
	{
		if(::IsWindowVisible(child) && listed.size() < MaxWindowMenuItems)
			listed.push_back(child);
	}
	if(!listed.empty())
	{
		if(::GetMenuItemCount(menu) > 0)
		{
			::AppendMenu(menu, MF_SEPARATOR, 0, nullptr);
			s->menuSeparator = true;
		}
		for(std::size_t i = 0; i < listed.size(); i++)
		{
			const std::wstring text = L"&" + std::to_wstring(i + 1) + L" " + WindowText(listed[i]);
			const UINT id = static_cast<UINT>(::GetWindowLongPtr(listed[i], GWLP_ID));
			::AppendMenu(menu, MF_STRING | (listed[i] == s->active ? MF_CHECKED : MF_UNCHECKED), id, text.c_str());
		}
	}
	if(const HWND frame = FrameOf(client))
		::DrawMenuBar(frame);
}

void Renumber(MdiClientState *s)
{
	for(std::size_t i = 0; i < s->children.size(); i++)
		::SetWindowLongPtr(s->children[i], GWLP_ID, static_cast<LONG_PTR>(s->idFirstChild + i));
}

void UpdateScrollBars(HWND client);

void FrameChanged(HWND client)
{
	UpdateScrollBars(client);
	UpdateFrameTitle(client);
	UpdateCornerButtons(client);
	RefreshWindowMenu(client);
}

// ---- arranging ----------------------------------------------------------------------

int IconHeight()
{
	return GetSystemMetrics(SM_CYMINIMIZED);
}

UINT ArrangeIcons(HWND client)
{
	MdiClientState *s = ClientState(client);
	if(s == nullptr)
		return 0;
	const QRect area = ClientArea(client);
	const int cx = GetSystemMetrics(SM_CXMINIMIZED), cy = IconHeight();
	int x = 0, y = area.bottom() + 1 - cy;
	UINT count = 0;
	for(HWND child : s->children)
	{
		if(!::IsIconic(child) || !::IsWindowVisible(child))
			continue;
		if(x + cx > area.width() && x > 0)
		{
			x = 0;
			y -= cy;
		}
		::SetWindowPos(child, nullptr, x, y, cx, cy, SWP_NOZORDER | SWP_NOACTIVATE);
		x += cx;
		count++;
	}
	return count;
}

// The client area without the rows of minimized children.
QRect ArrangeArea(HWND client)
{
	MdiClientState *s = ClientState(client);
	QRect area = ClientArea(client);
	const bool icons = std::any_of(s->children.begin(), s->children.end(), [](HWND c) { return ::IsIconic(c) && ::IsWindowVisible(c); });
	if(icons)
		area.setBottom(area.bottom() - IconHeight());
	return area;
}

void Activate(HWND client, HWND child);
void Restore(HWND client, HWND child);

std::vector<HWND> Arrangeable(HWND client, UINT how)
{
	MdiClientState *s = ClientState(client);
	std::vector<HWND> list;
	for(HWND child : s->zorder)
	{
		if(!::IsWindowVisible(child) || ::IsIconic(child))
			continue;
		if((how & MDITILE_SKIPDISABLED) && !::IsWindowEnabled(child))
			continue;
		list.push_back(child);
	}
	return list;
}

int CascadeStep()
{
	return GetSystemMetrics(SM_CYCAPTION) + GetSystemMetrics(SM_CYFRAME);
}

UINT Cascade(HWND client, UINT how)
{
	MdiClientState *s = ClientState(client);
	if(const HWND maximized = MaximizedChild(s))
		Restore(client, maximized);
	std::vector<HWND> list = Arrangeable(client, how);
	const QRect area = ArrangeArea(client);
	const int step = CascadeStep();
	const int n = static_cast<int>(list.size());
	const int w = std::max(area.width() - step * (n - 1), area.width() / 2);
	const int h = std::max(area.height() - step * (n - 1), area.height() / 2);
	// Back to front, so the active window ends up on top at the far end.
	for(int i = 0; i < n; i++)
	{
		const HWND child = list[n - 1 - i];
		const int offset = (step * i) % std::max(1, std::min(area.width() - w, area.height() - h) + 1);
		::SetWindowPos(child, HWND_TOP, area.left() + offset, area.top() + offset, w, h, SWP_NOACTIVATE);
	}
	return static_cast<UINT>(n);
}

UINT Tile(HWND client, UINT how)
{
	MdiClientState *s = ClientState(client);
	if(const HWND maximized = MaximizedChild(s))
		Restore(client, maximized);
	std::vector<HWND> list = Arrangeable(client, how);
	const QRect area = ArrangeArea(client);
	const int n = static_cast<int>(list.size());
	if(n == 0)
		return 0;
	// Up to three side by side (vertical) or one above another
	// (horizontal); more make a grid.
	// Columns of windows (rows, tiling horizontally).
	const int cols = n <= 3 ? n : static_cast<int>(std::floor(std::sqrt(static_cast<double>(n))));
	// The last columns take the windows left over.
	int index = 0;
	const int base = n / cols, extra = n % cols;
	for(int c = 0; c < cols && index < n; c++)
	{
		const int count = base + (c >= cols - extra ? 1 : 0);
		if(count == 0)
			continue;
		const int x0 = area.left() + area.width() * c / cols, x1 = area.left() + area.width() * (c + 1) / cols;
		for(int r = 0; r < count && index < n; r++, index++)
		{
			const int y0 = area.top() + area.height() * r / count, y1 = area.top() + area.height() * (r + 1) / count;
			// Horizontal tiling swaps the axes.
			if(how & MDITILE_HORIZONTAL)
			{
				const int ty0 = area.top() + area.height() * c / cols, ty1 = area.top() + area.height() * (c + 1) / cols;
				const int tx0 = area.left() + area.width() * r / count, tx1 = area.left() + area.width() * (r + 1) / count;
				::SetWindowPos(list[index], nullptr, tx0, ty0, tx1 - tx0, ty1 - ty0, SWP_NOZORDER | SWP_NOACTIVATE);
			} else
			{
				::SetWindowPos(list[index], nullptr, x0, y0, x1 - x0, y1 - y0, SWP_NOZORDER | SWP_NOACTIVATE);
			}
		}
	}
	return static_cast<UINT>(n);
}

// ---- scroll bars: shown while children reach outside the client area -------------

void UpdateScrollBars(HWND client)
{
	MdiClientState *s = ClientState(client);
	if(s == nullptr || s->updatingScroll || s->scrollStyle == 0)
		return;
	s->updatingScroll = true;
	// Twice: showing one bar can make the other necessary.
	for(int pass = 0; pass < 2; pass++)
	{
		const QRect area = ClientArea(client);
		QRect bounds;
		if(MaximizedChild(s) == nullptr)
		{
			for(HWND child : s->children)
			{
				if(::IsWindowVisible(child))
					bounds |= ChildWindowRect(FromHandle(child));
			}
		}
		for(int bar : {SB_HORZ, SB_VERT})
		{
			if(!(s->scrollStyle & (bar == SB_HORZ ? WS_HSCROLL : WS_VSCROLL)))
				continue;
			const int lo = bar == SB_HORZ ? bounds.left() : bounds.top();
			const int hi = bar == SB_HORZ ? bounds.left() + bounds.width() : bounds.top() + bounds.height();
			const int extent = bar == SB_HORZ ? area.width() : area.height();
			const bool needed = !bounds.isEmpty() && (lo < 0 || hi > extent);
			if(needed)
			{
				SCROLLINFO si{};
				si.cbSize = sizeof(si);
				si.fMask = SIF_ALL;
				si.nMin = std::min(lo, 0);
				si.nMax = std::max(hi, extent) - 1;
				si.nPage = static_cast<UINT>(extent);
				si.nPos = 0;
				::SetScrollInfo(client, bar, &si, TRUE);
			}
			::ShowScrollBar(client, bar, needed);
		}
	}
	s->updatingScroll = false;
}

void Scroll(HWND client, int bar, WPARAM wp)
{
	MdiClientState *s = ClientState(client);
	SCROLLINFO si{};
	si.cbSize = sizeof(si);
	si.fMask = SIF_ALL;
	if(s == nullptr || !::GetScrollInfo(client, bar, &si))
		return;
	const int line = 10;
	int pos = si.nPos;
	switch(LOWORD(wp))
	{
	case SB_LINEUP: pos -= line; break;
	case SB_LINEDOWN: pos += line; break;
	case SB_PAGEUP: pos -= static_cast<int>(si.nPage); break;
	case SB_PAGEDOWN: pos += static_cast<int>(si.nPage); break;
	case SB_THUMBTRACK:
	case SB_THUMBPOSITION: pos = si.nTrackPos; break;
	case SB_TOP: pos = si.nMin; break;
	case SB_BOTTOM: pos = si.nMax; break;
	case SB_ENDSCROLL: UpdateScrollBars(client); return;
	default: return;
	}
	pos = std::clamp(pos, si.nMin, std::max(si.nMin, si.nMax - static_cast<int>(si.nPage) + 1));
	const int delta = pos - si.nPos;
	if(delta == 0)
		return;
	// The children move; the bar keeps its range until the scroll ends.
	s->updatingScroll = true;
	for(HWND child : s->children)
	{
		const QRect r = ChildWindowRect(FromHandle(child));
		::SetWindowPos(child, nullptr, r.left() - (bar == SB_HORZ ? delta : 0), r.top() - (bar == SB_VERT ? delta : 0), 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
	}
	s->updatingScroll = false;
	si.fMask = SIF_POS;
	si.nPos = pos;
	::SetScrollInfo(client, bar, &si, TRUE);
	if(LOWORD(wp) != SB_THUMBTRACK)
		UpdateScrollBars(client);
}

// ---- states ---------------------------------------------------------------------------

void Maximize(HWND client, HWND child)
{
	MdiClientState *s = ClientState(client);
	if(s == nullptr || child == nullptr)
		return;
	for(HWND other : s->children)
	{
		if(other != child && IsMaximized(other))
			SetChildShowState(FromHandle(other), SIZE_RESTORED, QRect());
	}
	if(Win32Widget *w = FromHandle(child))
		SetChildShowState(w, SIZE_MAXIMIZED, MaximizedRect(client, child));
	ArrangeIcons(client);
	FrameChanged(client);
}

void Restore(HWND client, HWND child)
{
	Win32Widget *w = FromHandle(child);
	if(w == nullptr || w->data.childShow == SIZE_RESTORED)
		return;
	SetChildShowState(w, SIZE_RESTORED, QRect());
	ArrangeIcons(client);
	FrameChanged(client);
}

void Minimize(HWND client, HWND child)
{
	Win32Widget *w = FromHandle(child);
	if(w == nullptr)
		return;
	const int cx = GetSystemMetrics(SM_CXMINIMIZED), cy = IconHeight();
	SetChildShowState(w, SIZE_MINIMIZED, QRect(0, ClientArea(client).bottom() + 1 - cy, cx, cy));
	ArrangeIcons(client);
	FrameChanged(client);
}

void Activate(HWND client, HWND child)
{
	MdiClientState *s = ClientState(client);
	if(s == nullptr)
		return;
	if(child != nullptr && std::find(s->children.begin(), s->children.end(), child) == s->children.end())
		return;
	const HWND old = s->active;
	if(child == old)
	{
		if(child != nullptr)
			::SetWindowPos(child, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
		return;
	}
	const bool wasMaximized = old != nullptr && IsMaximized(old);
	s->active = child;
	if(child != nullptr)
	{
		s->zorder.erase(std::remove(s->zorder.begin(), s->zorder.end(), child), s->zorder.end());
		s->zorder.insert(s->zorder.begin(), child);
	}
	if(old != nullptr && ::IsWindow(old))
	{
		::SendMessage(old, WM_NCACTIVATE, FALSE, 0);
		::SendMessage(old, WM_MDIACTIVATE, reinterpret_cast<WPARAM>(old), reinterpret_cast<LPARAM>(child));
		if(Win32Widget *w = FromHandle(old))
		{
			w->data.ncDirty = true;
			w->update();
		}
	}
	if(child != nullptr && s->active == child)
	{
		// The maximized state goes with the activation.
		if(wasMaximized && ::IsWindow(old) && ::IsWindowVisible(child))
		{
			Maximize(client, child);
		}
		::SetWindowPos(child, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
		::SendMessage(child, WM_NCACTIVATE, TRUE, 0);
		::SendMessage(child, WM_MDIACTIVATE, reinterpret_cast<WPARAM>(old), reinterpret_cast<LPARAM>(child));
		if(Win32Widget *w = FromHandle(child))
		{
			w->data.ncDirty = true;
			w->update();
		}
		// The focus goes into the child, unless it is already there.
		const HWND focus = ::GetFocus();
		if(s->active == child && focus != child && !::IsChild(child, focus) && ::IsWindowVisible(child))
			::SetFocus(child);
	}
	FrameChanged(client);
}

void Next(HWND client, HWND from, bool previous)
{
	MdiClientState *s = ClientState(client);
	std::vector<HWND> list;
	for(HWND child : s->zorder)
	{
		if(::IsWindowVisible(child) && ::IsWindowEnabled(child))
			list.push_back(child);
	}
	if(list.size() < 2)
		return;
	if(from == nullptr)
		from = s->active;
	auto it = std::find(list.begin(), list.end(), from);
	if(it == list.end())
		return;
	if(previous)
	{
		// The window at the back comes to the front.
		Activate(client, list.back());
	} else
	{
		// The next one comes forward, and this one goes to the back.
		const HWND next = (it + 1 == list.end()) ? list.front() : *(it + 1);
		Activate(client, next);
		s->zorder.erase(std::remove(s->zorder.begin(), s->zorder.end(), from), s->zorder.end());
		s->zorder.push_back(from);
		if(!IsMaximized(from))
			::SetWindowPos(from, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
	}
}

// Activates the MDI child the focus moved into.
void WatchFocus()
{
	static bool connected = false;
	if(connected || qApp == nullptr)
		return;
	connected = true;
	QObject::connect(qApp, &QApplication::focusChanged, qApp, [](QWidget *, QWidget *now)
	{
		for(HWND hwnd = now ? WindowOf(now) : nullptr; hwnd != nullptr; hwnd = ::GetParent(hwnd))
		{
			const WindowData *d = Data(hwnd);
			if(d == nullptr || !(d->style & WS_CHILD))
				break;
			if(d->exStyle & WS_EX_MDICHILD)
			{
				if(const HWND client = ClientOf(hwnd))
				{
					if(ClientState(client)->active != hwnd)
						Activate(client, hwnd);
				}
				break;
			}
		}
	});
}

LRESULT CALLBACK MdiClientProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	switch(msg)
	{
	case WM_NCCREATE:
		if(WindowData *d = Data(hwnd))
			d->control = std::make_unique<MdiClientState>();
		WatchFocus();
		break;
	case WM_NCDESTROY:
		if(WindowData *d = Data(hwnd))
		{
			if(MdiClientState *s = ClientState(hwnd); s && s->corner)
				s->corner->deleteLater();
			d->control.reset();
		}
		break;
	case WM_CREATE:
		if(MdiClientState *s = ClientState(hwnd))
		{
			const auto *cs = reinterpret_cast<const CREATESTRUCT *>(lp);
			if(const auto *ccs = static_cast<const CLIENTCREATESTRUCT *>(cs->lpCreateParams))
			{
				s->windowMenu = static_cast<HMENU>(ccs->hWindowMenu);
				s->idFirstChild = ccs->idFirstChild;
			}
			s->scrollStyle = static_cast<DWORD>(cs->style) & (WS_HSCROLL | WS_VSCROLL);
			UpdateScrollBars(hwnd);
		}
		return 0;
	case WM_MDICREATE:
		{
			const auto *mcs = reinterpret_cast<const MDICREATESTRUCT *>(lp);
			if(mcs == nullptr)
				return 0;
			DWORD style = mcs->style | WS_CHILD | WS_CLIPSIBLINGS;
			if(!(::GetWindowLongPtr(hwnd, GWL_STYLE) & MDIS_ALLCHILDSTYLES))
				style |= WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
			const bool maximize = (style & WS_MAXIMIZE) != 0;
			const bool minimize = (style & WS_MINIMIZE) != 0;
			const bool visible = (style & WS_VISIBLE) != 0;
			style &= ~(WS_MAXIMIZE | WS_MINIMIZE | WS_VISIBLE);
			const HWND child = ::CreateWindowEx(WS_EX_MDICHILD, mcs->szClass, mcs->szTitle, style, mcs->x, mcs->y, mcs->cx, mcs->cy, hwnd, nullptr, static_cast<HINSTANCE>(mcs->hOwner), const_cast<MDICREATESTRUCT *>(mcs));
			if(child == nullptr)
				return 0;
			if(visible || maximize || minimize)
				::ShowWindow(child, maximize ? SW_SHOWMAXIMIZED : minimize ? SW_SHOWMINIMIZED : SW_SHOW);
			return reinterpret_cast<LRESULT>(child);
		}
	case WM_MDIDESTROY:
		::DestroyWindow(reinterpret_cast<HWND>(wp));
		return 0;
	case WM_MDIACTIVATE:
		Activate(hwnd, reinterpret_cast<HWND>(wp));
		return 0;
	case WM_MDINEXT:
		Next(hwnd, reinterpret_cast<HWND>(wp), lp != 0);
		return 0;
	case WM_MDIMAXIMIZE:
		Maximize(hwnd, reinterpret_cast<HWND>(wp));
		return 0;
	case WM_MDIRESTORE:
		Restore(hwnd, reinterpret_cast<HWND>(wp));
		return 0;
	case WM_MDITILE:
		return Tile(hwnd, static_cast<UINT>(wp));
	case WM_MDICASCADE:
		return Cascade(hwnd, static_cast<UINT>(wp));
	case WM_MDIICONARRANGE:
		ArrangeIcons(hwnd);
		return 0;
	case WM_MDIGETACTIVE:
		{
			MdiClientState *s = ClientState(hwnd);
			if(lp != 0)
				*reinterpret_cast<BOOL *>(lp) = s && MaximizedChild(s) ? TRUE : FALSE;
			return reinterpret_cast<LRESULT>(s ? s->active : nullptr);
		}
	case WM_MDISETMENU:
		{
			MdiClientState *s = ClientState(hwnd);
			const HWND frame = FrameOf(hwnd);
			const HMENU old = frame ? ::GetMenu(frame) : nullptr;
			if(s != nullptr && lp != 0 && reinterpret_cast<HMENU>(lp) != s->windowMenu)
			{
				// Our items leave the old window menu.
				std::vector<HWND> children;
				children.swap(s->children);
				RefreshWindowMenu(hwnd);
				s->children.swap(children);
				s->windowMenu = reinterpret_cast<HMENU>(lp);
				s->menuSeparator = false;
			}
			if(wp != 0 && frame != nullptr && reinterpret_cast<HMENU>(wp) != old)
				::SetMenu(frame, reinterpret_cast<HMENU>(wp));
			FrameChanged(hwnd);
			return reinterpret_cast<LRESULT>(old);
		}
	case WM_MDIREFRESHMENU:
		FrameChanged(hwnd);
		return reinterpret_cast<LRESULT>(FrameOf(hwnd) ? ::GetMenu(FrameOf(hwnd)) : nullptr);
	case WM_SIZE:
		if(MdiClientState *s = ClientState(hwnd))
		{
			if(const HWND maximized = MaximizedChild(s))
			{
				const QRect r = MaximizedRect(hwnd, maximized);
				::SetWindowPos(maximized, nullptr, r.left(), r.top(), r.width(), r.height(), SWP_NOZORDER | SWP_NOACTIVATE);
			}
			ArrangeIcons(hwnd);
			UpdateScrollBars(hwnd);
		}
		break;
	case WM_HSCROLL:
		Scroll(hwnd, SB_HORZ, wp);
		return 0;
	case WM_VSCROLL:
		Scroll(hwnd, SB_VERT, wp);
		return 0;
	case WM_SETFOCUS:
		if(MdiClientState *s = ClientState(hwnd); s && s->active && ::IsWindowVisible(s->active))
			::SetFocus(s->active);
		return 0;
	case WM_ERASEBKGND:
		{
			RECT rc;
			::GetClientRect(hwnd, &rc);
			::FillRect(reinterpret_cast<HDC>(wp), &rc, ::GetSysColorBrush(COLOR_APPWORKSPACE));
			return 1;
		}
	default:
		break;
	}
	return ::DefWindowProc(hwnd, msg, wp, lp);
}

} // namespace

bool MdiIsActiveChild(HWND child)
{
	const HWND client = ClientOf(child);
	return client != nullptr && ClientState(client)->active == child;
}

bool MdiShowWindow(HWND child, int cmd)
{
	const HWND client = ClientOf(child);
	Win32Widget *w = FromHandle(child);
	if(client == nullptr || w == nullptr)
		return false;
	switch(cmd)
	{
	case SW_SHOWMAXIMIZED:
		w->show();
		Activate(client, child);
		Maximize(client, child);
		return true;
	case SW_MINIMIZE:
	case SW_SHOWMINIMIZED:
	case SW_SHOWMINNOACTIVE:
		w->show();
		Minimize(client, child);
		if(cmd == SW_SHOWMINIMIZED)
			Activate(client, child);
		return true;
	case SW_RESTORE:
	case SW_SHOWNORMAL:
		w->show();
		Restore(client, child);
		Activate(client, child);
		return true;
	case SW_SHOW:
		w->show();
		Activate(client, child);
		FrameChanged(client);
		return true;
	case SW_HIDE:
		{
			MdiClientState *s = ClientState(client);
			w->hide();
			if(s->active == child)
			{
				HWND next = nullptr;
				for(HWND c : s->zorder)
				{
					if(c != child && ::IsWindowVisible(c))
					{
						next = c;
						break;
					}
				}
				const bool maximized = IsMaximized(child);
				Activate(client, next);
				if(maximized && next)
					Maximize(client, next);
			}
			FrameChanged(client);
		}
		return true;
	default:
		return false;
	}
}

void MdiChildCreated(HWND child)
{
	const HWND client = ClientOf(child);
	if(client == nullptr)
		return;
	MdiClientState *s = ClientState(client);
	s->children.push_back(child);
	s->zorder.push_back(child);
	Renumber(s);
	if(::IsWindowVisible(child))
		Activate(client, child);
	else
		FrameChanged(client);
}

void MdiChildDestroying(HWND child)
{
	const HWND client = ClientOf(child);
	if(client == nullptr)
		return;
	MdiClientState *s = ClientState(client);
	const bool wasActive = s->active == child;
	const bool maximized = IsMaximized(child);
	s->children.erase(std::remove(s->children.begin(), s->children.end(), child), s->children.end());
	s->zorder.erase(std::remove(s->zorder.begin(), s->zorder.end(), child), s->zorder.end());
	Renumber(s);
	if(wasActive)
	{
		s->active = nullptr;
		::SendMessage(child, WM_MDIACTIVATE, reinterpret_cast<WPARAM>(child), 0);
		HWND next = nullptr;
		for(HWND c : s->zorder)
		{
			if(::IsWindowVisible(c))
			{
				next = c;
				break;
			}
		}
		if(next != nullptr)
		{
			Activate(client, next);
			if(maximized)
				Maximize(client, next);
		}
	}
	FrameChanged(client);
}

void MdiDefaultRect(HWND client, int &x, int &y, int &w, int &h)
{
	MdiClientState *s = ClientState(client);
	if(s == nullptr)
		return;
	const QRect area = ClientArea(client);
	const int step = CascadeStep();
	if(x == CW_USEDEFAULT)
	{
		const int span = std::max(1, std::min(area.width(), area.height()) / 2 / std::max(step, 1));
		x = y = (s->cascade++ % span) * step;
	}
	if(w == CW_USEDEFAULT)
	{
		w = std::max(area.width() * 3 / 4, GetSystemMetrics(SM_CXMINTRACK));
		h = std::max(area.height() * 3 / 4, GetSystemMetrics(SM_CYMINTRACK));
	}
}

void RegisterMdiClientClass()
{
	WNDCLASS wc{};
	wc.style = 0;
	wc.lpfnWndProc = MdiClientProc;
	wc.hCursor = ::LoadCursor(nullptr, IDC_ARROW);
	wc.hbrBackground = reinterpret_cast<HBRUSH>(static_cast<INT_PTR>(COLOR_APPWORKSPACE + 1));
	wc.lpszClassName = L"MDIClient";
	::RegisterClass(&wc);
}

} // namespace linopenmpt::qtwin

using namespace linopenmpt::qtwin;

LRESULT DefFrameProc(HWND frame, HWND client, UINT msg, WPARAM wp, LPARAM lp)
{
	MdiClientState *s = client ? ClientState(client) : nullptr;
	if(s == nullptr)
		return ::DefWindowProc(frame, msg, wp, lp);
	switch(msg)
	{
	case WM_COMMAND:
		{
			const UINT id = LOWORD(wp);
			if(id >= s->idFirstChild && id < s->idFirstChild + s->children.size())
			{
				const HWND child = s->children[id - s->idFirstChild];
				if(::IsIconic(child))
					Restore(client, child);
				Activate(client, child);
				return 0;
			}
			if(id >= 0xF000)
			{
				// System commands from the maximized child's menu bar buttons.
				if(const HWND maximized = MaximizedChild(s))
					return ::SendMessage(maximized, WM_SYSCOMMAND, wp, lp);
			}
		}
		break;
	case WM_SETTEXT:
		s->frameTitle = lp ? reinterpret_cast<LPCWSTR>(lp) : L"";
		s->haveFrameTitle = true;
		UpdateFrameTitle(client);
		return TRUE;
	case WM_SIZE:
		{
			RECT rc;
			::GetClientRect(frame, &rc);
			::MoveWindow(client, 0, 0, rc.right, rc.bottom, TRUE);
		}
		return 0;
	case WM_SETFOCUS:
		::SetFocus(client);
		return 0;
	case WM_SYSCOMMAND:
		if((wp & 0xFFF0) == SC_NEXTWINDOW || (wp & 0xFFF0) == SC_PREVWINDOW)
		{
			Next(client, nullptr, (wp & 0xFFF0) == SC_PREVWINDOW);
			return 0;
		}
		break;
	default:
		break;
	}
	return ::DefWindowProc(frame, msg, wp, lp);
}

LRESULT DefMDIChildProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	const HWND client = ClientOf(hwnd);
	if(client == nullptr)
		return ::DefWindowProc(hwnd, msg, wp, lp);
	switch(msg)
	{
	case WM_SYSCOMMAND:
		switch(wp & 0xFFF0)
		{
		case SC_MAXIMIZE:
			if(::GetWindowLongPtr(hwnd, GWL_STYLE) & WS_MAXIMIZEBOX)
			{
				Activate(client, hwnd);
				Maximize(client, hwnd);
			}
			return 0;
		case SC_MINIMIZE:
			if(::GetWindowLongPtr(hwnd, GWL_STYLE) & WS_MINIMIZEBOX)
				Minimize(client, hwnd);
			return 0;
		case SC_RESTORE:
			Restore(client, hwnd);
			Activate(client, hwnd);
			return 0;
		case SC_NEXTWINDOW:
			Next(client, hwnd, false);
			return 0;
		case SC_PREVWINDOW:
			Next(client, hwnd, true);
			return 0;
		default:
			break;
		}
		break;
	case WM_MOUSEACTIVATE:
		{
			const LRESULT r = ::DefWindowProc(hwnd, msg, wp, lp);
			if(r != MA_NOACTIVATE && r != MA_NOACTIVATEANDEAT && ::IsWindow(hwnd))
				Activate(client, hwnd);
			return r;
		}
	case WM_CHILDACTIVATE:
		Activate(client, hwnd);
		return 0;
	case WM_SETFOCUS:
		if(ClientState(client)->active != hwnd)
			Activate(client, hwnd);
		return 0;
	case WM_SETTEXT:
		{
			const LRESULT r = ::DefWindowProc(hwnd, msg, wp, lp);
			if(Win32Widget *w = FromHandle(hwnd))
			{
				w->data.ncDirty = true;
				w->update();
			}
			FrameChanged(client);
			return r;
		}
	case WM_SIZE:
		// A maximized child keeps the frame's buttons and title current.
		if(wp == SIZE_MAXIMIZED || wp == SIZE_RESTORED)
			UpdateCornerButtons(client);
		UpdateScrollBars(client);
		break;
	case WM_MOVE:
		UpdateScrollBars(client);
		break;
	default:
		break;
	}
	return ::DefWindowProc(hwnd, msg, wp, lp);
}

HWND CreateMDIWindow(LPCTSTR className, LPCTSTR title, DWORD style, int x, int y, int w, int h, HWND client, HINSTANCE instance, LPARAM lp)
{
	MDICREATESTRUCT mcs{className, title, instance, x, y, w, h, style, lp};
	return reinterpret_cast<HWND>(::SendMessage(client, WM_MDICREATE, 0, reinterpret_cast<LPARAM>(&mcs)));
}

BOOL TranslateMDISysAccel(HWND client, LPMSG msg)
{
	MdiClientState *s = client ? ClientState(client) : nullptr;
	if(s == nullptr || msg == nullptr || s->active == nullptr)
		return FALSE;
	if(msg->message != WM_KEYDOWN && msg->message != WM_SYSKEYDOWN)
		return FALSE;
	if(!(::GetKeyState(VK_CONTROL) & 0x8000) || (::GetKeyState(VK_MENU) & 0x8000))
		return FALSE;
	const bool shift = (::GetKeyState(VK_SHIFT) & 0x8000) != 0;
	WPARAM command = 0;
	switch(msg->wParam)
	{
	case VK_F4: command = SC_CLOSE; break;
	case VK_F6:
	case VK_TAB: command = shift ? SC_PREVWINDOW : SC_NEXTWINDOW; break;
	default: return FALSE;
	}
	if(!::IsWindowEnabled(s->active))
		return FALSE;
	::SendMessage(s->active, WM_SYSCOMMAND, command, 0);
	return TRUE;
}

WORD CascadeWindows(HWND parent, UINT how, const RECT *, UINT, const HWND *)
{
	if(parent != nullptr && ClientState(parent) != nullptr)
		return static_cast<WORD>(Cascade(parent, how));
	return 0;
}

WORD TileWindows(HWND parent, UINT how, const RECT *, UINT, const HWND *)
{
	if(parent != nullptr && ClientState(parent) != nullptr)
		return static_cast<WORD>(Tile(parent, how));
	return 0;
}

UINT ArrangeIconicWindows(HWND parent)
{
	if(parent != nullptr && ClientState(parent) != nullptr)
		return ArrangeIcons(parent);
	return 0;
}
