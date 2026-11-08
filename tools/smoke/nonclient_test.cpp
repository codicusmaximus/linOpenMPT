// The non-client area (borders, scroll bars, WM_NCCALCSIZE, WM_NCHITTEST,
// WM_NCPAINT) and menus (menu bars, popup menus, upstream's menu resources,
// CMenu), run offscreen.
#include <afxwin.h>

#include "MptrackResources.h"
#include "QtWin.h"
#include "resource.h"

#include <QtCore/QTimer>
#include <QtGui/QKeyEvent>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMenuBar>

#include <cstdio>
#include <cstring>
#include <vector>

using namespace linopenmpt::qtwin;

namespace
{
int g_failures = 0;

void Check(bool ok, const char *what)
{
	std::printf("%s %s\n", ok ? "ok  " : "FAIL", what);
	if(!ok)
		g_failures++;
}

struct Received
{
	UINT msg;
	WPARAM wp;
	LPARAM lp;
};
std::vector<Received> g_log;
int g_reserveTop = 0;

const Received *Last(UINT msg)
{
	for(auto it = g_log.rbegin(); it != g_log.rend(); ++it)
	{
		if(it->msg == msg)
			return &*it;
	}
	return nullptr;
}

LRESULT CALLBACK TestProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	g_log.push_back({msg, wp, lp});
	if(msg == WM_NCCALCSIZE && g_reserveTop > 0)
	{
		// A window that keeps a strip of its own, as MFC's control bars do.
		const LRESULT r = ::DefWindowProc(hwnd, msg, wp, lp);
		reinterpret_cast<RECT *>(lp)->top += g_reserveTop;
		return r;
	}
	return ::DefWindowProc(hwnd, msg, wp, lp);
}

void Pump()
{
	for(int i = 0; i < 5; i++)
		QApplication::processEvents();
}

// Runs once the next popup menu is open: picks the item at pos (or closes
// the menu when pos is negative).
void PickInNextPopup(int pos)
{
	QTimer::singleShot(0, [pos]()
	{
		auto *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
		if(menu == nullptr)
			return;
		for(QAction *a : menu->actions())
		{
			if(pos >= 0 && a->property("linopenmpt_pos").toInt() == pos)
				menu->setActiveAction(a);
		}
		QKeyEvent key(QEvent::KeyPress, pos >= 0 ? Qt::Key_Return : Qt::Key_Escape, Qt::NoModifier);
		QApplication::sendEvent(menu, &key);
	});
}

class CTestApp : public CWinApp
{
};

} // namespace

int main(int argc, char **argv)
{
	qputenv("QT_QPA_PLATFORM", "offscreen");
	QApplication qapp(argc, argv);
	CTestApp app;
	LinOpenMPTRegisterMptrackResources();
	const HINSTANCE instance = AfxGetResourceHandle();

	WNDCLASS wc{};
	wc.lpfnWndProc = TestProc;
	wc.hInstance = instance;
	wc.lpszClassName = L"NonClientTest";
	wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
	::RegisterClass(&wc);

	const int edge = GetSystemMetrics(SM_CXEDGE), border = GetSystemMetrics(SM_CXBORDER);
	HWND top = ::CreateWindowEx(0, L"NonClientTest", L"Top", WS_OVERLAPPEDWINDOW, 0, 0, 400, 300, nullptr, nullptr, instance, nullptr);
	::ShowWindow(top, SW_SHOW);
	Pump();

	// Borders and client edges of child windows.
	HWND framed = ::CreateWindowEx(WS_EX_CLIENTEDGE, L"NonClientTest", L"", WS_CHILD | WS_VISIBLE | WS_BORDER, 10, 20, 100, 80, top, nullptr, instance, nullptr);
	RECT rc{};
	::GetClientRect(framed, &rc);
	const int inset = edge + border;
	Check(rc.right == 100 - 2 * inset && rc.bottom == 80 - 2 * inset, "WS_BORDER and WS_EX_CLIENTEDGE are outside the client area");
	RECT wr{};
	::GetWindowRect(framed, &wr);
	POINT origin{0, 0};
	::ClientToScreen(framed, &origin);
	Check(origin.x - wr.left == inset && origin.y - wr.top == inset, "ClientToScreen starts inside the borders");
	POINT inParent{wr.left, wr.top};
	::ScreenToClient(top, &inParent);
	Check(inParent.x == 10 && inParent.y == 20, "children are placed in the parent's client coordinates");

	RECT adjusted{0, 0, 50, 50};
	::AdjustWindowRectEx(&adjusted, WS_CHILD | WS_BORDER, FALSE, WS_EX_CLIENTEDGE);
	Check(adjusted.left == -inset && adjusted.right == 50 + inset, "AdjustWindowRectEx agrees with WM_NCCALCSIZE");

	// A child inside a framed child: placed inside the parent's client area.
	HWND inner = ::CreateWindowEx(0, L"NonClientTest", L"", WS_CHILD | WS_VISIBLE, 0, 0, 10, 10, framed, nullptr, instance, nullptr);
	RECT innerRect{};
	::GetWindowRect(inner, &innerRect);
	Check(innerRect.left == origin.x && innerRect.top == origin.y, "a child at 0,0 sits at its parent's client origin");
	Check(::ChildWindowFromPoint(framed, POINT{2, 2}) == inner, "ChildWindowFromPoint takes client coordinates");

	// Painting: client DCs start at the client origin, the window DC at the border.
	::UpdateWindow(framed);
	Pump();
	HDC clientDC = ::GetDC(framed);
	HBRUSH red = ::CreateSolidBrush(RGB(255, 0, 0));
	RECT first{0, 0, 1, 1};
	::FillRect(clientDC, &first, red);
	::ReleaseDC(framed, clientDC);
	HDC windowDC = ::GetWindowDC(framed);
	Check(::GetPixel(windowDC, inset, inset) == RGB(255, 0, 0), "a client DC draws inside the borders");
	Check(::GetPixel(windowDC, 0, 0) == ::GetSysColor(COLOR_WINDOWFRAME), "WM_NCPAINT draws the border");
	Check(::GetPixel(windowDC, border, border) != RGB(255, 0, 0), "the client edge is drawn outside the client area");
	::ReleaseDC(framed, windowDC);
	::DeleteObject(red);

	// Hit testing.
	::GetWindowRect(framed, &wr);
	Check(::SendMessage(framed, WM_NCHITTEST, 0, MAKELPARAM(wr.left + 20, wr.top + 20)) == HTCLIENT, "WM_NCHITTEST: client area");
	Check(::SendMessage(framed, WM_NCHITTEST, 0, MAKELPARAM(wr.left, wr.top + 20)) == HTBORDER, "WM_NCHITTEST: border");

	// Scroll bars take space from the client area.
	::ShowScrollBar(framed, SB_VERT, TRUE);
	::GetClientRect(framed, &rc);
	Check(rc.right == 100 - 2 * inset - GetSystemMetrics(SM_CXVSCROLL), "a vertical scroll bar narrows the client area");
	Check(::SendMessage(framed, WM_NCHITTEST, 0, MAKELPARAM(wr.right - inset - 2, wr.top + 30)) == HTVSCROLL, "WM_NCHITTEST: scroll bar");
	::ShowScrollBar(framed, SB_VERT, FALSE);

	// A window procedure's own WM_NCCALCSIZE, applied by SWP_FRAMECHANGED.
	g_reserveTop = 12;
	g_log.clear();
	::SetWindowPos(framed, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
	::GetClientRect(framed, &rc);
	Check(rc.bottom == 80 - 2 * inset - 12, "WM_NCCALCSIZE of the window procedure");
	const Received *size = Last(WM_SIZE);
	Check(size != nullptr && HIWORD(size->lp) == rc.bottom, "WM_SIZE has the new client size");
	origin = POINT{0, 0};
	::ClientToScreen(framed, &origin);
	Check(origin.y - wr.top == inset + 12, "the client origin moves with it");
	g_reserveTop = 0;
	::SetWindowPos(framed, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

	// Menus from the resources.
	HMENU menu = ::LoadMenu(instance, MAKEINTRESOURCE(IDR_MAINFRAME));
	Check(menu != nullptr && ::GetMenuItemCount(menu) >= 6, "LoadMenu of upstream's main menu");
	HMENU file = ::GetSubMenu(menu, 0);
	HMENU fileNew = ::GetSubMenu(file, 0);
	Check(fileNew != nullptr && ::GetMenuItemID(fileNew, 0) == ID_FILE_NEWIT && ::GetMenuItemID(file, 0) == UINT(-1), "submenus and item ids");
	TCHAR text[64] = {};
	Check(::GetMenuString(file, ID_FILE_OPEN, text, 64, MF_BYCOMMAND) > 0 && std::wcscmp(text, L"&Open...\tCtrl+O") == 0, "GetMenuString");
	::CheckMenuItem(menu, ID_FILE_OPEN, MF_BYCOMMAND | MF_CHECKED);
	::EnableMenuItem(menu, ID_FILE_NEWXM, MF_BYCOMMAND | MF_GRAYED);
	Check((::GetMenuState(menu, ID_FILE_OPEN, MF_BYCOMMAND) & MF_CHECKED) && (::GetMenuState(menu, ID_FILE_NEWXM, MF_BYCOMMAND) & MF_GRAYED), "MF_BYCOMMAND finds items in submenus");
	::CheckMenuItem(menu, ID_FILE_OPEN, MF_BYCOMMAND | MF_UNCHECKED);
	::CheckMenuRadioItem(fileNew, 0, 2, 1, MF_BYPOSITION);
	Check(!(::GetMenuState(fileNew, 0, MF_BYPOSITION) & MF_CHECKED) && (::GetMenuState(fileNew, 1, MF_BYPOSITION) & MF_CHECKED), "CheckMenuRadioItem");

	// The menu bar is non-client.
	::GetClientRect(top, &rc);
	const int clientWithout = rc.bottom;
	::SetMenu(top, menu);
	Pump();
	::GetClientRect(top, &rc);
	auto *topWidget = FromHandle(top);
	Check(::GetMenu(top) == menu && topWidget->data.menuBar != nullptr, "SetMenu shows a menu bar");
	Check(rc.bottom == clientWithout - MenuBarHeight(topWidget) && rc.bottom < clientWithout, "the menu bar is outside the client area");
	Check(topWidget->data.menuBar->actions().size() == ::GetMenuItemCount(menu), "one menu bar entry per item");
	::GetWindowRect(top, &wr);
	Check(::SendMessage(top, WM_NCHITTEST, 0, MAKELPARAM(wr.left + 5, wr.top + 2)) == HTMENU, "WM_NCHITTEST: menu bar");

	// Popup menus.
	g_log.clear();
	PickInNextPopup(1); // Open...
	const BOOL chosen = ::TrackPopupMenu(file, TPM_RETURNCMD, 50, 50, 0, top, nullptr);
	Check(chosen == ID_FILE_OPEN, "TrackPopupMenu with TPM_RETURNCMD returns the choice");
	const Received *initPopup = Last(WM_INITMENUPOPUP);
	Check(Last(WM_INITMENU) != nullptr && initPopup != nullptr && reinterpret_cast<HMENU>(initPopup->wp) == file, "WM_INITMENU and WM_INITMENUPOPUP");
	Pump();
	Check(Last(WM_COMMAND) == nullptr, "TPM_RETURNCMD sends no WM_COMMAND");
	Check(Last(WM_EXITMENULOOP) != nullptr, "WM_EXITMENULOOP");

	g_log.clear();
	PickInNextPopup(1);
	::TrackPopupMenu(file, 0, 50, 50, 0, top, nullptr);
	Pump();
	const Received *command = Last(WM_COMMAND);
	Check(command != nullptr && LOWORD(command->wp) == ID_FILE_OPEN, "choosing a popup item posts WM_COMMAND");

	g_log.clear();
	PickInNextPopup(-1);
	Check(::TrackPopupMenu(file, TPM_RETURNCMD, 50, 50, 0, top, nullptr) == 0, "a dismissed menu returns 0");

	// Changes after WM_INITMENUPOPUP show in the menu (MFC's command UI).
	::EnableMenuItem(file, ID_FILE_OPEN, MF_BYCOMMAND | MF_GRAYED);
	bool grayedShown = false;
	QTimer::singleShot(0, [&grayedShown]()
	{
		auto *popup = qobject_cast<QMenu *>(QApplication::activePopupWidget());
		if(popup == nullptr)
			return;
		for(QAction *a : popup->actions())
		{
			if(a->property("linopenmpt_pos").toInt() == 1)
				grayedShown = !a->isEnabled();
		}
		popup->close();
	});
	::TrackPopupMenu(file, TPM_RETURNCMD, 50, 50, 0, top, nullptr);
	Check(grayedShown, "grayed items are disabled in the popup");
	::EnableMenuItem(file, ID_FILE_OPEN, MF_BYCOMMAND | MF_ENABLED);

	// Alt+F opens the File menu.
	::SetFocus(top);
	g_log.clear();
	::SendMessage(top, WM_SYSCHAR, 'f', 0x20000000);
	Pump();
	auto *opened = qobject_cast<QMenu *>(QApplication::activePopupWidget());
	Check(opened != nullptr, "Alt+F opens a menu");
	initPopup = Last(WM_INITMENUPOPUP);
	Check(initPopup != nullptr && reinterpret_cast<HMENU>(initPopup->wp) == file, "with WM_INITMENUPOPUP for the File menu");
	if(opened)
		opened->close();
	Pump();

	// CMenu.
	HMENU destroyed = nullptr;
	{
		CMenu popup;
		Check(popup.CreatePopupMenu() && popup.AppendMenu(MF_STRING, 100, _T("&One")) && popup.AppendMenu(MF_SEPARATOR) && popup.AppendMenu(MF_STRING, 101, _T("Two")), "CMenu::AppendMenu");
		CString itemText;
		Check(popup.GetMenuItemCount() == 3 && popup.GetMenuString(101, itemText, MF_BYCOMMAND) == 3 && itemText == _T("Two"), "CMenu::GetMenuString");
		CWnd *topWnd = CWnd::FromHandle(top);
		PickInNextPopup(2);
		Check(popup.TrackPopupMenu(TPM_RETURNCMD, 10, 10, topWnd) == 101, "CMenu::TrackPopupMenu");
		Check(topWnd->GetMenu() != nullptr && topWnd->GetMenu()->m_hMenu == menu, "CWnd::GetMenu");
		CMenu loaded;
		Check(loaded.LoadMenu(IDR_MAINFRAME) && loaded.GetSubMenu(0) != nullptr && loaded.GetSubMenu(0)->GetMenuItemID(1) == ID_FILE_OPEN, "CMenu::LoadMenu and GetSubMenu");
		destroyed = loaded.m_hMenu;
	}
	Check(!::IsMenu(destroyed), "CMenu destroys its menu");

	::SetMenu(top, nullptr);
	Pump();
	::GetClientRect(top, &rc);
	Check(rc.bottom == clientWithout && FromHandle(top)->data.menuBar == nullptr, "SetMenu(nullptr) gives the space back");
	::DestroyMenu(menu);
	::DestroyWindow(top);

	std::printf("%s\n", g_failures == 0 ? "all passed" : "FAILURES");
	return g_failures == 0 ? 0 : 1;
}
