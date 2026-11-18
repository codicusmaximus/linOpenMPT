// The MFC layer (platform/mfc) on Qt, run offscreen: windows created through
// CWnd get their messages through message maps, notifications are reflected
// to the child that sent them, CDC drawing reaches the window, and
// CWinApp::Run() runs the message loop with PreTranslateMessage and idle
// processing until WM_QUIT. Touch screen input reaches CWnd's gesture
// functions (OnGesturePan, OnGestureZoom), or the mouse handlers.
#include <afxwin.h>

#include "QtWin.h"

#include <QtCore/QElapsedTimer>
#include <QtGui/QAccessible>
#include <QtTest/QTest>
#include <QtWidgets/QApplication>

#include <cstdio>

namespace
{
int g_failures = 0;

void Check(bool ok, const char *what)
{
	std::printf("%s %s\n", ok ? "ok  " : "FAIL", what);
	if(!ok)
		g_failures++;
}

void Pump(int ms = 0)
{
	QElapsedTimer t;
	t.start();
	do
	{
		QApplication::processEvents(QEventLoop::AllEvents, 10);
	} while(t.elapsed() < ms);
}

COLORREF ScreenPixel(HWND hwnd, int x, int y)
{
	const QImage shot = linopenmpt::qtwin::FromHandle(hwnd)->grab().toImage();
	return linopenmpt::qtwin::ToColorRef(shot.pixelColor(QPoint(x, y) * shot.devicePixelRatio()));
}

constexpr UINT ID_TEST_COMMAND = 40001;
constexpr UINT WM_TEST_USER = WM_USER + 5;
constexpr UINT kChildId = 100;
constexpr COLORREF kRed = RGB(255, 0, 0);
constexpr COLORREF kGreen = RGB(0, 160, 0);

class CTestChild : public CWnd
{
public:
	int reflectedClicks = 0;

protected:
	afx_msg void OnClickReflect(NMHDR *, LRESULT *result)
	{
		reflectedClicks++;
		*result = 5;
	}
	DECLARE_MESSAGE_MAP()
};

BEGIN_MESSAGE_MAP(CTestChild, CWnd)
	ON_NOTIFY_REFLECT(NM_CLICK, &CTestChild::OnClickReflect)
END_MESSAGE_MAP()

class CTestWnd : public CWnd
{
public:
	bool createdAttached = false;
	int paints = 0;
	CPoint lastClick{-1, -1};
	int commands = 0;
	int idleUpdates = 0;
	int timerTicks = 0;
	int eatenKeys = 0;
	int keysSeen = 0;
	CTestChild child;
	// Gestures: whether the window takes pans, and what it got.
	bool takesPans = true;
	CPoint panned{0, 0};
	long zoomed = 0;
	int rightClicks = 0;
	int clicks = 0;

	BOOL OnGesturePan(CPoint from, CPoint to) override
	{
		if(!takesPans)
			return FALSE;
		panned += to - from;
		return TRUE;
	}
	// What a screen reader hears for this window (as upstream's editors).
	HRESULT get_accName(VARIANT child, BSTR *name) override
	{
		if(child.lVal != CHILDID_SELF)
			return CWnd::get_accName(child, name);
		*name = CString(_T("Row 5, Note C-5")).AllocSysString();
		return S_OK;
	}
	BOOL OnGestureZoom(CPoint, long delta) override
	{
		zoomed += delta;
		return TRUE;
	}

	BOOL PreTranslateMessage(MSG *msg) override
	{
		if(msg->message == WM_KEYDOWN && msg->wParam == VK_F5)
		{
			eatenKeys++;
			return TRUE;
		}
		return CWnd::PreTranslateMessage(msg);
	}

protected:
	afx_msg int OnCreate(LPCREATESTRUCT cs)
	{
		// The window is attached before WM_CREATE (by the CBT hook), as in MFC.
		createdAttached = (CWnd::FromHandlePermanent(m_hWnd) == this);
		child.Create(nullptr, _T("child"), WS_CHILD | WS_VISIBLE, CRect(100, 0, 150, 50), this, kChildId);
		return 0;
	}
	afx_msg void OnPaint()
	{
		paints++;
		CPaintDC dc(this);
		dc.FillSolidRect(0, 0, 40, 40, kRed);
		CBrush green(kGreen);
		CRect r(50, 0, 90, 40);
		dc.FillRect(&r, &green);
	}
	afx_msg void OnLButtonDown(UINT, CPoint point)
	{
		lastClick = point;
		clicks++;
	}
	afx_msg void OnRButtonDown(UINT, CPoint point)
	{
		lastClick = point;
		rightClicks++;
	}
	afx_msg void OnKeyDown(UINT, UINT, UINT)
	{
		keysSeen++;
	}
	afx_msg void OnTimer(UINT_PTR id)
	{
		timerTicks++;
		KillTimer(id);
		PostMessage(WM_COMMAND, ID_TEST_COMMAND);
	}
	afx_msg void OnTestCommand()
	{
		commands++;
		PostQuitMessage(3);
	}
	afx_msg LRESULT OnTestUser(WPARAM wp, LPARAM lp)
	{
		return 70 + static_cast<LRESULT>(wp);
	}
	afx_msg LRESULT OnIdleUpdateCmdUI(WPARAM, LPARAM)
	{
		idleUpdates++;
		return 0;
	}
	DECLARE_MESSAGE_MAP()
};

BEGIN_MESSAGE_MAP(CTestWnd, CWnd)
	ON_WM_CREATE()
	ON_WM_PAINT()
	ON_WM_LBUTTONDOWN()
	ON_WM_RBUTTONDOWN()
	ON_WM_KEYDOWN()
	ON_WM_TIMER()
	ON_COMMAND(ID_TEST_COMMAND, &CTestWnd::OnTestCommand)
	ON_MESSAGE(WM_TEST_USER, &CTestWnd::OnTestUser)
	ON_MESSAGE(WM_IDLEUPDATECMDUI, &CTestWnd::OnIdleUpdateCmdUI)
END_MESSAGE_MAP()

class CTestApp : public CWinApp
{
public:
	int idleCalls[3] = {};
	CTestApp() : CWinApp(_T("mfc_test")) { }
	BOOL OnIdle(LONG count) override
	{
		if(count < 3)
			idleCalls[count]++;
		return CWinApp::OnIdle(count);
	}
};

} // namespace

int main(int argc, char **argv)
{
	qputenv("QT_QPA_PLATFORM", "offscreen");
	QApplication qapp(argc, argv);
	CTestApp app;
	Check(AfxGetApp() == &app && AfxGetThread() == &app, "AfxGetApp");

	CTestWnd wnd;
	const LPCTSTR cls = AfxRegisterWndClass(CS_DBLCLKS, ::LoadCursor(nullptr, IDC_ARROW), reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1));
	Check(cls != nullptr, "AfxRegisterWndClass");
	Check(wnd.CreateEx(0, cls, _T("MFC test"), WS_OVERLAPPEDWINDOW, 10, 10, 200, 100, nullptr, nullptr) != FALSE, "CWnd::CreateEx");
	Check(wnd.createdAttached, "attached before WM_CREATE");
	Check(CWnd::FromHandlePermanent(wnd.m_hWnd) == &wnd && CWnd::FromHandle(wnd.m_hWnd) == &wnd, "permanent handle map");
	Check(wnd.child.m_hWnd != nullptr && wnd.child.GetParent() == &wnd && wnd.child.GetDlgCtrlID() == static_cast<int>(kChildId), "child CWnd");
	app.m_pMainWnd = &wnd;

	wnd.ShowWindow(SW_SHOW);
	wnd.UpdateWindow();
	Pump(50);
	Check(wnd.paints >= 1, "ON_WM_PAINT");
	Check(ScreenPixel(wnd.m_hWnd, 10, 10) == kRed, "CDC::FillSolidRect");
	Check(ScreenPixel(wnd.m_hWnd, 60, 10) == kGreen, "CDC::FillRect with CBrush");

	Check(wnd.SendMessage(WM_TEST_USER, 7) == 77, "ON_MESSAGE");
	wnd.SendMessage(WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(12, 34));
	Check(wnd.lastClick == CPoint(12, 34), "ON_WM_LBUTTONDOWN cracks the point");

	// Touch: a drag pans, two fingers zoom, a tap clicks, a long press
	// right-clicks; a window that does not take pans gets a mouse drag.
	{
		QWidget *widget = linopenmpt::qtwin::FromHandle(wnd.m_hWnd);
		QPointingDevice *screen = QTest::createTouchDevice(QInputDevice::DeviceType::TouchScreen);
		CPoint origin(0, 0);
		wnd.ClientToScreen(&origin);
		const QPoint client = widget->mapFromGlobal(QPoint(origin.x, origin.y));  // client (0,0) in the widget
		const int clicks = wnd.clicks;
		QTest::touchEvent(widget, screen).press(0, client + QPoint(20, 20));
		QTest::touchEvent(widget, screen).move(0, client + QPoint(20, 40));
		QTest::touchEvent(widget, screen).move(0, client + QPoint(25, 50));
		QTest::touchEvent(widget, screen).release(0, client + QPoint(25, 50));
		Check(wnd.panned == CPoint(5, 30) && wnd.clicks == clicks, "touch drag: OnGesturePan, no click");

		QTest::touchEvent(widget, screen).press(0, client + QPoint(20, 20)).press(1, client + QPoint(60, 20));
		QTest::touchEvent(widget, screen).move(0, client + QPoint(10, 20)).move(1, client + QPoint(70, 20));
		QTest::touchEvent(widget, screen).release(0, client + QPoint(10, 20)).release(1, client + QPoint(70, 20));
		Check(wnd.zoomed == 20, "two fingers: OnGestureZoom with the change in distance");

		QTest::touchEvent(widget, screen).press(0, client + QPoint(30, 15));
		QTest::touchEvent(widget, screen).release(0, client + QPoint(30, 15));
		Check(wnd.clicks == clicks + 1 && wnd.lastClick == CPoint(30, 15), "tap: a click where the finger was");

		QTest::touchEvent(widget, screen).press(0, client + QPoint(31, 16));
		QTest::qWait(900);
		QTest::touchEvent(widget, screen).release(0, client + QPoint(31, 16));
		Check(wnd.rightClicks == 1 && wnd.lastClick == CPoint(31, 16), "long press: a right click");

		wnd.takesPans = false;
		const CPoint panned = wnd.panned;
		QTest::touchEvent(widget, screen).press(0, client + QPoint(20, 20));
		QTest::touchEvent(widget, screen).move(0, client + QPoint(20, 40));
		QTest::touchEvent(widget, screen).release(0, client + QPoint(20, 40));
		Check(wnd.panned == panned && wnd.clicks == clicks + 2 && wnd.lastClick == CPoint(20, 20), "pan not taken: a mouse drag instead");
		wnd.takesPans = true;
	}

	// Screen readers: the window's own name, and for controls their role,
	// text, label and state.
	{
		QAccessibleInterface *self = QAccessible::queryAccessibleInterface(linopenmpt::qtwin::FromHandle(wnd.m_hWnd));
		Check(self != nullptr && self->text(QAccessible::Name) == QStringLiteral("Row 5, Note C-5"), "get_accName reaches the screen reader");
		CButton check;
		check.Create(_T("&Loop"), WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, CRect(0, 0, 60, 20), &wnd, 2001);
		check.SetCheck(BST_CHECKED);
		QAccessibleInterface *box = QAccessible::queryAccessibleInterface(linopenmpt::qtwin::FromHandle(check.m_hWnd));
		Check(box != nullptr && box->role() == QAccessible::CheckBox && box->text(QAccessible::Name) == QStringLiteral("Loop") && box->state().checked, "check box: role, name without mnemonic, checked");
		CStatic label;
		label.Create(_T("&Tempo:"), WS_CHILD | WS_VISIBLE, CRect(0, 30, 40, 50), &wnd, 2002);
		CEdit edit;
		edit.Create(WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, CRect(40, 30, 90, 50), &wnd, 2003);
		edit.SetWindowText(_T("125"));
		QAccessibleInterface *field = QAccessible::queryAccessibleInterface(linopenmpt::qtwin::FromHandle(edit.m_hWnd));
		Check(field != nullptr && field->role() == QAccessible::EditableText && field->text(QAccessible::Name) == QStringLiteral("Tempo:") && field->text(QAccessible::Value) == QStringLiteral("125"), "edit: named by its label, valued by its text");
		edit.DestroyWindow();
		label.DestroyWindow();
		check.DestroyWindow();
	}

	NMHDR nm{};
	nm.hwndFrom = wnd.child.m_hWnd;
	nm.idFrom = kChildId;
	nm.code = NM_CLICK;
	const LRESULT notifyResult = wnd.SendMessage(WM_NOTIFY, kChildId, reinterpret_cast<LPARAM>(&nm));
	Check(wnd.child.reflectedClicks == 1 && notifyResult == 5, "ON_NOTIFY_REFLECT");

	// An unattached window gets a temporary CWnd, deleted at idle.
	HWND plain = ::CreateWindowEx(0, cls, _T("plain"), WS_CHILD, 0, 0, 10, 10, wnd.m_hWnd, nullptr, nullptr, nullptr);
	CWnd *temp = CWnd::FromHandle(plain);
	Check(temp != nullptr && temp->m_hWnd == plain && CWnd::FromHandlePermanent(plain) == nullptr, "temporary CWnd");

	// Posted input goes through PreTranslateMessage; then a timer posts a
	// command that quits the loop.
	wnd.PostMessage(WM_KEYDOWN, VK_F5, 0);
	wnd.PostMessage(WM_KEYDOWN, VK_F6, 0);
	wnd.SetTimer(1, 30, nullptr);
	const int exitCode = app.Run();
	Check(exitCode == 3, "CWinApp::Run returns the WM_QUIT code");
	Check(wnd.eatenKeys == 1 && wnd.keysSeen == 1, "PreTranslateMessage eats a key, others are dispatched");
	Check(wnd.timerTicks == 1 && wnd.commands == 1, "ON_WM_TIMER and ON_COMMAND");
	Check(app.idleCalls[0] >= 1 && app.idleCalls[1] >= 1, "OnIdle runs while the loop waits");
	Check(wnd.idleUpdates >= 1, "WM_IDLEUPDATECMDUI reaches the main window");

	wnd.DestroyWindow();
	Check(wnd.m_hWnd == nullptr && wnd.child.m_hWnd == nullptr, "DestroyWindow detaches parent and child");
	Check(!::IsWindow(plain), "children are destroyed with the parent");

	std::printf("%s\n", g_failures == 0 ? "all passed" : "FAILURES");
	return g_failures == 0 ? 0 : 1;
}
