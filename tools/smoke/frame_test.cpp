// Frames, run offscreen: the multiple-document interface of user32 (the
// MDI client, DefFrameProc, DefMDIChildProc, child windows that move, size,
// maximize and minimize inside it), then MFC's frames and document/view on
// top of it.
#include <afxwin.h>

#include "QtWin.h"

#include <QtGui/QKeyEvent>
#include <QtGui/QMouseEvent>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QToolButton>

#include <cstdio>
#include <string>
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

void Pump()
{
	for(int i = 0; i < 5; i++)
		QApplication::processEvents();
}

void SaveShot(HWND hwnd, const char *name)
{
	const QByteArray dir = qgetenv("LINOPENMPT_SHOTS");
	Win32Widget *w = FromHandle(hwnd);
	if(dir.isEmpty() || w == nullptr)
		return;
	Pump();
	w->grab().save(QString::fromLocal8Bit(dir) + QStringLiteral("/%1.png").arg(QLatin1String(name)));
}

std::wstring Text(HWND hwnd)
{
	wchar_t buf[256] = {};
	::GetWindowText(hwnd, buf, 256);
	return buf;
}

// The window rectangle in its parent's client coordinates.
RECT ChildRect(HWND hwnd)
{
	RECT rc{};
	::GetWindowRect(hwnd, &rc);
	::MapWindowPoints(nullptr, ::GetParent(hwnd), reinterpret_cast<POINT *>(&rc), 2);
	return rc;
}

// Mouse input as Qt delivers it, at a point in the widget.
// A drag gives the screen position it started from: the window moves under
// the mouse, which stays where it is on the screen.
void Mouse(HWND hwnd, QEvent::Type type, QPoint pos, QPoint screenOrigin = QPoint(-1, -1))
{
	Win32Widget *w = FromHandle(hwnd);
	if(w == nullptr)
		return;
	const Qt::MouseButtons buttons = type == QEvent::MouseButtonRelease ? Qt::NoButton : Qt::MouseButtons(Qt::LeftButton);
	const QPoint global = screenOrigin == QPoint(-1, -1) ? w->mapToGlobal(pos) : screenOrigin + pos;
	QMouseEvent e(type, QPointF(w->mapFromGlobal(global)), QPointF(global), type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton, buttons, Qt::NoModifier);
	QApplication::sendEvent(w, &e);
	Pump();
}

void Click(HWND hwnd, QPoint pos)
{
	Mouse(hwnd, QEvent::MouseButtonPress, pos);
	if(::IsWindow(hwnd))
		Mouse(hwnd, QEvent::MouseButtonRelease, pos);
}

// Holds Ctrl down (for GetKeyState) while it lives.
struct CtrlDown
{
	QWidget *target;
	CtrlDown() : target(QApplication::focusWidget())
	{
		if(target)
		{
			QKeyEvent press(QEvent::KeyPress, Qt::Key_Control, Qt::ControlModifier);
			QApplication::sendEvent(target, &press);
		}
	}
	~CtrlDown()
	{
		if(target)
		{
			QKeyEvent release(QEvent::KeyRelease, Qt::Key_Control, Qt::NoModifier);
			QApplication::sendEvent(target, &release);
		}
	}
};

// ---- Win32 MDI ------------------------------------------------------------------

HWND g_client = nullptr;
struct Activation
{
	HWND to, deactivated, activated;
};
std::vector<Activation> g_activations;

LRESULT CALLBACK FrameProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	return ::DefFrameProc(hwnd, g_client, msg, wp, lp);
}

LRESULT CALLBACK ChildProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	if(msg == WM_MDIACTIVATE)
		g_activations.push_back({hwnd, reinterpret_cast<HWND>(wp), reinterpret_cast<HWND>(lp)});
	return ::DefMDIChildProc(hwnd, msg, wp, lp);
}

constexpr UINT FirstChild = 0xFF00;

int WindowMenuChildren(HMENU menu, HWND *checked = nullptr, std::wstring *firstText = nullptr)
{
	int count = 0;
	for(int i = 0; i < ::GetMenuItemCount(menu); i++)
	{
		const UINT id = ::GetMenuItemID(menu, i);
		if(id < FirstChild || id >= FirstChild + 9)
			continue;
		if(count == 0 && firstText)
		{
			wchar_t buf[128] = {};
			::GetMenuString(menu, i, buf, 128, MF_BYPOSITION);
			*firstText = buf;
		}
		count++;
		if(checked && (::GetMenuState(menu, i, MF_BYPOSITION) & MF_CHECKED))
			*checked = reinterpret_cast<HWND>(static_cast<UINT_PTR>(id));
	}
	return count;
}

void Win32Mdi()
{
	WNDCLASS wc{};
	wc.lpfnWndProc = FrameProc;
	wc.lpszClassName = L"MdiTestFrame";
	wc.hbrBackground = reinterpret_cast<HBRUSH>(static_cast<INT_PTR>(COLOR_BTNFACE + 1));
	::RegisterClass(&wc);
	wc.lpfnWndProc = ChildProc;
	wc.lpszClassName = L"MdiTestChild";
	wc.style = CS_DBLCLKS;
	wc.hbrBackground = reinterpret_cast<HBRUSH>(static_cast<INT_PTR>(COLOR_WINDOW + 1));
	::RegisterClass(&wc);

	HMENU menu = ::CreateMenu();
	HMENU fileMenu = ::CreatePopupMenu();
	::AppendMenu(fileMenu, MF_STRING, 100, L"&New");
	HMENU windowMenu = ::CreatePopupMenu();
	::AppendMenu(windowMenu, MF_STRING, 101, L"&Cascade");
	::AppendMenu(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(fileMenu), L"&File");
	::AppendMenu(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(windowMenu), L"&Window");
	HWND frame = ::CreateWindowEx(0, L"MdiTestFrame", L"Frame", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 700, 500, nullptr, menu, nullptr, nullptr);
	if(Win32Widget *w = FromHandle(frame))
		w->activateWindow();
	CLIENTCREATESTRUCT ccs{windowMenu, FirstChild};
	g_client = ::CreateWindowEx(WS_EX_CLIENTEDGE, L"MDIClient", nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_VSCROLL | WS_HSCROLL, 0, 0, 0, 0, frame, nullptr, nullptr, &ccs);
	Check(g_client != nullptr, "MDIClient: created with a CLIENTCREATESTRUCT");
	// DefFrameProc fits the client to the frame.
	{
		RECT rc;
		::GetClientRect(frame, &rc);
		::SendMessage(frame, WM_SIZE, SIZE_RESTORED, MAKELPARAM(rc.right, rc.bottom));
	}
	Pump();
	RECT clientArea;
	::GetClientRect(g_client, &clientArea);

	HWND c[3];
	for(int i = 0; i < 3; i++)
	{
		const std::wstring title = L"Child " + std::to_wstring(i + 1);
		c[i] = ::CreateMDIWindow(L"MdiTestChild", title.c_str(), WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, g_client, nullptr, 0);
	}
	Pump();
	Check(c[0] && c[1] && c[2], "CreateMDIWindow: three children");
	Check(::SendMessage(g_client, WM_MDIGETACTIVE, 0, 0) == reinterpret_cast<LRESULT>(c[2]), "WM_MDIGETACTIVE: the newest child is active");
	Check(::GetWindowLongPtr(c[0], GWLP_ID) == FirstChild && ::GetWindowLongPtr(c[2], GWLP_ID) == FirstChild + 2, "MDI children: ids from idFirstChild");
	Check((::GetWindowLongPtr(c[0], GWL_STYLE) & (WS_CAPTION | WS_THICKFRAME | WS_MAXIMIZEBOX)) == (WS_CAPTION | WS_THICKFRAME | WS_MAXIMIZEBOX), "MDI children: caption, frame and buttons");
	const RECT r0 = ChildRect(c[0]), r1 = ChildRect(c[1]);
	Check(r1.left > r0.left && r1.top > r0.top && r0.right - r0.left == r1.right - r1.left, "CW_USEDEFAULT: children cascade");
	HWND checked = nullptr;
	std::wstring first;
	Check(WindowMenuChildren(windowMenu, &checked, &first) == 3 && checked == reinterpret_cast<HWND>(static_cast<UINT_PTR>(FirstChild + 2)) && first == L"&1 Child 1", "the window menu lists the children, the active one checked");
	Check(::GetFocus() == c[2], "the active child has the focus");

	// Activation messages.
	g_activations.clear();
	::SendMessage(g_client, WM_MDIACTIVATE, reinterpret_cast<WPARAM>(c[0]), 0);
	Check(g_activations.size() == 2 && g_activations[0].to == c[2] && g_activations[0].deactivated == c[2] && g_activations[0].activated == c[0]
		&& g_activations[1].to == c[0] && g_activations[1].deactivated == c[2] && g_activations[1].activated == c[0], "WM_MDIACTIVATE: to the old and the new child, with both handles");
	Check(::GetFocus() == c[0], "WM_MDIACTIVATE: the focus follows");

	// A click into another child activates it.
	Click(c[1], QPoint(40, 60));
	Check(::SendMessage(g_client, WM_MDIGETACTIVE, 0, 0) == reinterpret_cast<LRESULT>(c[1]), "a click in a child activates it (WM_MOUSEACTIVATE)");
	// The window menu activates too.
	::SendMessage(frame, WM_COMMAND, FirstChild + 2, 0);
	Check(::SendMessage(g_client, WM_MDIGETACTIVE, 0, 0) == reinterpret_cast<LRESULT>(c[2]), "DefFrameProc: a window menu item activates its child");
	// Ctrl+F6 cycles.
	::SendMessage(g_client, WM_MDINEXT, 0, 0);
	const HWND afterNext = reinterpret_cast<HWND>(::SendMessage(g_client, WM_MDIGETACTIVE, 0, 0));
	Check(afterNext != c[2] && afterNext != nullptr, "WM_MDINEXT: another child");
	::SendMessage(g_client, WM_MDINEXT, 0, 1);
	Check(::SendMessage(g_client, WM_MDIGETACTIVE, 0, 0) == reinterpret_cast<LRESULT>(c[2]), "WM_MDINEXT (previous): back to the one sent to the back");

	// Maximize.
	const RECT before = ChildRect(c[2]);
	::SendMessage(c[2], WM_SYSCOMMAND, SC_MAXIMIZE, 0);
	Pump();
	BOOL maximized = FALSE;
	::SendMessage(g_client, WM_MDIGETACTIVE, 0, reinterpret_cast<LPARAM>(&maximized));
	RECT childClient;
	::GetClientRect(c[2], &childClient);
	Check(::IsZoomed(c[2]) && maximized, "SC_MAXIMIZE: the child is maximized");
	Check(childClient.right == clientArea.right && childClient.bottom == clientArea.bottom, "a maximized child's client area is the MDI client's");
	Check(Text(frame) == L"Frame - [Child 3]", "the frame's title shows the maximized child");
	QMenuBar *bar = Data(frame)->menuBar;
	QWidget *corner = bar ? bar->cornerWidget(Qt::TopRightCorner) : nullptr;
	Check(corner != nullptr && corner->isVisible() && corner->findChildren<QToolButton *>().size() == 3, "the frame's menu bar gets minimize, restore and close buttons");
	SaveShot(frame, "mdi_maximized");
	// Another child takes the maximized state.
	::SendMessage(g_client, WM_MDIACTIVATE, reinterpret_cast<WPARAM>(c[0]), 0);
	Pump();
	Check(::IsZoomed(c[0]) && !::IsZoomed(c[2]), "activating another child maximizes it, and restores the old one");
	const RECT restored = ChildRect(c[2]);
	Check(::EqualRect(&restored, &before), "the old one goes back to where it was");
	Check(Text(frame) == L"Frame - [Child 1]", "the title follows");
	// The restore button in the menu bar.
	if(corner)
	{
		if(auto *restore = corner->findChild<QToolButton *>(QStringLiteral("restore")))
			restore->click();
	}
	Pump();
	Check(!::IsZoomed(c[0]) && Text(frame) == L"Frame" && corner && !corner->isVisible(), "the menu bar's restore button restores the child, and the title");
	::SetWindowText(frame, L"New Frame");
	::SendMessage(c[0], WM_SYSCOMMAND, SC_MAXIMIZE, 0);
	Check(Text(frame) == L"New Frame - [Child 1]", "DefFrameProc: WM_SETTEXT keeps the frame's own title");
	::SendMessage(g_client, WM_MDIRESTORE, reinterpret_cast<WPARAM>(c[0]), 0);
	Check(Text(frame) == L"New Frame", "WM_MDIRESTORE");

	// Minimize.
	::SendMessage(c[1], WM_SYSCOMMAND, SC_MINIMIZE, 0);
	Pump();
	const RECT icon = ChildRect(c[1]);
	Check(::IsIconic(c[1]) && icon.bottom == clientArea.bottom && icon.left == 0 && icon.bottom - icon.top == ::GetSystemMetrics(SM_CYMINIMIZED), "SC_MINIMIZE: the child lines up at the bottom");
	WINDOWPLACEMENT wp{};
	wp.length = sizeof(wp);
	::GetWindowPlacement(c[1], &wp);
	Check(wp.showCmd == SW_SHOWMINIMIZED && wp.rcNormalPosition.right - wp.rcNormalPosition.left == r1.right - r1.left, "GetWindowPlacement: minimized, with the normal rectangle");

	// Tile and cascade the other two.
	::SendMessage(g_client, WM_MDITILE, MDITILE_VERTICAL, 0);
	Pump();
	const RECT t0 = ChildRect(c[0]), t2 = ChildRect(c[2]);
	Check(t0.top == 0 && t2.top == 0 && (t0.right == t2.left || t2.right == t0.left) && t0.bottom == clientArea.bottom - ::GetSystemMetrics(SM_CYMINIMIZED), "WM_MDITILE vertical: side by side, above the minimized one");
	::SendMessage(g_client, WM_MDITILE, MDITILE_HORIZONTAL, 0);
	const RECT h0 = ChildRect(c[0]), h2 = ChildRect(c[2]);
	Check(h0.left == 0 && h2.left == 0 && h0.right == clientArea.right && (h0.bottom == h2.top || h2.bottom == h0.top), "WM_MDITILE horizontal: one above the other");
	::SendMessage(g_client, WM_MDICASCADE, 0, 0);
	const RECT k0 = ChildRect(c[0]), k2 = ChildRect(c[2]);
	Check(k0.left != k2.left && k0.right - k0.left == k2.right - k2.left, "WM_MDICASCADE");
	::SendMessage(c[1], WM_SYSCOMMAND, SC_RESTORE, 0);
	Check(!::IsIconic(c[1]) && ::SendMessage(g_client, WM_MDIGETACTIVE, 0, 0) == reinterpret_cast<LRESULT>(c[1]), "SC_RESTORE: back, and active");

	// Scroll bars, while a child reaches outside.
	{
		const WindowData *cd = Data(g_client);
		Check(!cd->scroll[SB_HORZ].shown && !cd->scroll[SB_VERT].shown, "MDI client: no scroll bars while the children fit");
		RECT area;
		::GetClientRect(g_client, &area);
		::SetWindowPos(c[1], nullptr, area.right - 50, 10, 200, 100, SWP_NOZORDER);
		Check(cd->scroll[SB_HORZ].shown && !cd->scroll[SB_VERT].shown, "MDI client: a horizontal scroll bar when a child reaches out on the right");
		SCROLLINFO si{};
		si.cbSize = sizeof(si);
		si.fMask = SIF_ALL;
		::GetScrollInfo(g_client, SB_HORZ, &si);
		::SendMessage(g_client, WM_HSCROLL, SB_BOTTOM, 0);
		::SendMessage(g_client, WM_HSCROLL, SB_ENDSCROLL, 0);
		const RECT scrolled = ChildRect(c[1]);
		::GetClientRect(g_client, &area);
		Check(scrolled.right == area.right, "MDI client: scrolling to the end brings the child in");
		::SetWindowPos(c[1], nullptr, 20, 20, 300, 200, SWP_NOZORDER);
		::SetWindowPos(c[0], nullptr, 0, 0, 200, 150, SWP_NOZORDER);
		::SetWindowPos(c[2], nullptr, 40, 40, 200, 150, SWP_NOZORDER);
		Check(!cd->scroll[SB_HORZ].shown && !cd->scroll[SB_VERT].shown, "MDI client: the scroll bar goes when the children fit again");
	}

	// Moving and sizing with the mouse.
	{
		::SetWindowPos(c[1], nullptr, 20, 20, 300, 200, SWP_NOZORDER);
		Win32Widget *w = FromHandle(c[1]);
		const QMargins m = FrameMargins(Data(c[1])->style, Data(c[1])->exStyle, true);
		const QPoint caption(60, m.top() - ::GetSystemMetrics(SM_CYCAPTION) / 2);
		Check(::SendMessage(c[1], WM_NCHITTEST, 0, MAKELPARAM(w->mapToGlobal(caption).x(), w->mapToGlobal(caption).y())) == HTCAPTION, "WM_NCHITTEST: the caption");
		QPoint origin = w->mapToGlobal(QPoint(0, 0));
		Mouse(c[1], QEvent::MouseButtonPress, caption, origin);
		Mouse(c[1], QEvent::MouseMove, caption + QPoint(30, 25), origin);
		Mouse(c[1], QEvent::MouseButtonRelease, caption + QPoint(30, 25), origin);
		const RECT moved = ChildRect(c[1]);
		Check(moved.left == 50 && moved.top == 45 && moved.right - moved.left == 300, "dragging the caption moves the child");
		const QPoint corner(w->width() - 2, w->height() - 2);
		origin = w->mapToGlobal(QPoint(0, 0));
		Mouse(c[1], QEvent::MouseButtonPress, corner, origin);
		Mouse(c[1], QEvent::MouseMove, corner + QPoint(40, 30), origin);
		Mouse(c[1], QEvent::MouseButtonRelease, corner + QPoint(40, 30), origin);
		const RECT sized = ChildRect(c[1]);
		Check(sized.left == 50 && sized.right - sized.left == 340 && sized.bottom - sized.top == 230, "dragging the corner sizes the child");
		// Double-clicking the caption maximizes.
		Mouse(c[1], QEvent::MouseButtonPress, caption);
		Mouse(c[1], QEvent::MouseButtonRelease, caption);
		Mouse(c[1], QEvent::MouseButtonDblClick, caption);
		Mouse(c[1], QEvent::MouseButtonRelease, caption);
		Check(::IsZoomed(c[1]), "double-clicking the caption maximizes");
		::SendMessage(g_client, WM_MDIRESTORE, reinterpret_cast<WPARAM>(c[1]), 0);
		SaveShot(frame, "mdi_children");
		// The close button.
		const QRect close = CaptionButtonRect(w, HTCLOSE);
		Check(!close.isEmpty() && !CaptionButtonRect(w, HTMAXBUTTON).isEmpty() && !CaptionButtonRect(w, HTMINBUTTON).isEmpty(), "caption buttons: close, maximize, minimize");
		Click(c[1], close.center());
		Check(!::IsWindow(c[1]), "the close button closes the child");
	}
	Check(WindowMenuChildren(windowMenu) == 2, "the window menu loses the closed child");
	Check(::GetWindowLongPtr(c[2], GWLP_ID) == FirstChild + 1, "the children are numbered again");

	// Ctrl+F4 closes the active child; a maximized one hands its state on.
	const HWND active = reinterpret_cast<HWND>(::SendMessage(g_client, WM_MDIGETACTIVE, 0, 0));
	const HWND other = active == c[0] ? c[2] : c[0];
	::SendMessage(active, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
	{
		CtrlDown ctrl;
		MSG msg{active, WM_KEYDOWN, VK_F4, 0};
		Check(::TranslateMDISysAccel(g_client, &msg) && !::IsWindow(active), "TranslateMDISysAccel: Ctrl+F4 closes the active child");
	}
	Check(::SendMessage(g_client, WM_MDIGETACTIVE, 0, 0) == reinterpret_cast<LRESULT>(other) && ::IsZoomed(other), "the next child becomes active, and maximized");
	::SendMessage(g_client, WM_MDIDESTROY, reinterpret_cast<WPARAM>(other), 0);
	Check(!::IsWindow(other) && ::SendMessage(g_client, WM_MDIGETACTIVE, 0, 0) == 0 && Text(frame) == L"New Frame" && WindowMenuChildren(windowMenu) == 0, "WM_MDIDESTROY: the last child gone, no active child, the title back");
	Check(::GetMenuItemCount(windowMenu) == 1, "... and the window menu as it was");
	::DestroyWindow(frame);
	g_client = nullptr;
}

} // namespace

int main(int argc, char **argv)
{
	QApplication app(argc, argv);
	Win32Mdi();
	std::printf("passed: %d failure(s)\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}
