// Win32 windowing and GDI on Qt (platform/qtwin), run offscreen: a window
// procedure gets the messages Windows would send, and GDI drawing ends up
// in the window's pixels.
#include "QtWin.h"

#include <QtCore/QElapsedTimer>
#include <QtGui/QKeyEvent>
#include <QtGui/QMouseEvent>
#include <QtWidgets/QApplication>
#include <QtWidgets/QScrollBar>

#include <cstdio>
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

bool Got(UINT msg)
{
	for(const auto &r : g_log)
	{
		if(r.msg == msg)
			return true;
	}
	return false;
}

const Received *Last(UINT msg)
{
	for(auto it = g_log.rbegin(); it != g_log.rend(); ++it)
	{
		if(it->msg == msg)
			return &*it;
	}
	return nullptr;
}

constexpr COLORREF kRed = RGB(255, 0, 0);
constexpr COLORREF kBlue = RGB(0, 0, 255);
int g_paints = 0;
int g_cbtCreates = 0;
HHOOK g_hook = nullptr;

LRESULT CALLBACK CbtProc(int code, WPARAM wp, LPARAM lp)
{
	if(code == HCBT_CREATEWND)
		g_cbtCreates++;
	return CallNextHookEx(g_hook, code, wp, lp);
}

LRESULT CALLBACK TestProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	g_log.push_back({msg, wp, lp});
	switch(msg)
	{
	case WM_PAINT:
		{
			g_paints++;
			PAINTSTRUCT ps;
			HDC dc = BeginPaint(hwnd, &ps);
			HBRUSH red = CreateSolidBrush(kRed);
			RECT r{0, 0, 50, 50};
			FillRect(dc, &r, red);
			DeleteObject(red);
			SetBkMode(dc, TRANSPARENT);
			TextOut(dc, 60, 5, L"Hello", 5);
			EndPaint(hwnd, &ps);
		}
		return 0;
	case WM_USER + 1:
		return 42;
	}
	return DefWindowProc(hwnd, msg, wp, lp);
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
	// What Qt shows on screen, not just the backing image.
	const QImage shot = FromHandle(hwnd)->grab().toImage();
	return ToColorRef(shot.pixelColor(QPoint(x, y) * shot.devicePixelRatio()));
}

} // namespace

int main(int argc, char **argv)
{
	qputenv("QT_QPA_PLATFORM", "offscreen");
	QApplication app(argc, argv);

	WNDCLASS wc{};
	wc.style = CS_DBLCLKS;
	wc.lpfnWndProc = TestProc;
	wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
	wc.lpszClassName = L"LinOpenMPTTest";
	Check(RegisterClass(&wc) != 0, "RegisterClass");

	HWND hwnd = CreateWindowEx(0, L"LinOpenMPTTest", L"Test", WS_OVERLAPPEDWINDOW, 10, 10, 200, 100, nullptr, nullptr, nullptr, nullptr);
	Check(hwnd != nullptr && IsWindow(hwnd), "CreateWindowEx");
	Check(Got(WM_NCCREATE) && Got(WM_CREATE), "WM_NCCREATE and WM_CREATE");
	RECT client{};
	GetClientRect(hwnd, &client);
	Check(client.right == 200 && client.bottom == 100, "client size");

	ShowWindow(hwnd, SW_SHOW);
	UpdateWindow(hwnd);
	Pump(50);
	Check(g_paints >= 1 && Got(WM_ERASEBKGND), "WM_ERASEBKGND and WM_PAINT");
	Check(ScreenPixel(hwnd, 10, 10) == kRed, "FillRect in WM_PAINT reaches the screen");
	Check(ScreenPixel(hwnd, 150, 80) == GetSysColor(COLOR_WINDOW), "class background brush erases");

	// Pixels persist: an expose does not repaint.
	const int paints = g_paints;
	FromHandle(hwnd)->update();
	Pump(20);
	Check(g_paints == paints, "expose only blits, no WM_PAINT");
	InvalidateRect(hwnd, nullptr, TRUE);
	Pump(20);
	Check(g_paints == paints + 1, "InvalidateRect leads to WM_PAINT");

	// Messages.
	Check(SendMessage(hwnd, WM_USER + 1, 0, 0) == 42, "SendMessage returns the result");
	g_log.clear();
	PostMessage(hwnd, WM_USER + 2, 7, 8);
	Check(!Got(WM_USER + 2), "PostMessage is asynchronous");
	Pump(20);
	Check(Last(WM_USER + 2) && Last(WM_USER + 2)->wp == 7 && Last(WM_USER + 2)->lp == 8, "posted message arrives");

	// Timers.
	g_log.clear();
	SetTimer(hwnd, 5, 10, nullptr);
	Pump(100);
	KillTimer(hwnd, 5);
	Check(Last(WM_TIMER) && Last(WM_TIMER)->wp == 5, "WM_TIMER");

	// Keyboard.
	g_log.clear();
	{
		QKeyEvent press(QEvent::KeyPress, Qt::Key_Q, Qt::NoModifier, QStringLiteral("q"));
		QApplication::sendEvent(FromHandle(hwnd), &press);
	}
	Check(Last(WM_KEYDOWN) && Last(WM_KEYDOWN)->wp == 'Q', "WM_KEYDOWN with VK_Q");
	Check(Last(WM_CHAR) && Last(WM_CHAR)->wp == 'q', "WM_CHAR with the typed character");
	Check(GetKeyState('Q') < 0, "GetKeyState: Q down");
	{
		QKeyEvent release(QEvent::KeyRelease, Qt::Key_Q, Qt::NoModifier, QStringLiteral("q"));
		QApplication::sendEvent(FromHandle(hwnd), &release);
	}
	Check(Last(WM_KEYUP) && Last(WM_KEYUP)->wp == 'Q' && GetKeyState('Q') >= 0, "WM_KEYUP and key state");
	{
		QKeyEvent press(QEvent::KeyPress, Qt::Key_Up, Qt::NoModifier);
		QApplication::sendEvent(FromHandle(hwnd), &press);
	}
	Check(Last(WM_KEYDOWN) && Last(WM_KEYDOWN)->wp == VK_UP && (Last(WM_KEYDOWN)->lp & (1 << 24)), "arrow key: VK_UP, extended");
	{
		QKeyEvent press(QEvent::KeyPress, Qt::Key_8, Qt::KeypadModifier, QStringLiteral("8"));
		QApplication::sendEvent(FromHandle(hwnd), &press);
	}
	Check(Last(WM_KEYDOWN) && Last(WM_KEYDOWN)->wp == VK_NUMPAD8, "keypad: VK_NUMPAD8");
	{
		wchar_t name[32] = {};
		GetKeyNameText(static_cast<LONG>(0x48 << 16), name, 32);
		Check(std::wstring(name) == L"Num 8", "GetKeyNameText");
		Check(MapVirtualKey('Q', MAPVK_VK_TO_VSC) == 0x10, "MapVirtualKey");
	}

	// Mouse.
	g_log.clear();
	{
		QMouseEvent press(QEvent::MouseButtonPress, QPointF(5, 6), FromHandle(hwnd)->mapToGlobal(QPointF(5, 6)), Qt::LeftButton, Qt::LeftButton, Qt::ShiftModifier);
		QApplication::sendEvent(FromHandle(hwnd), &press);
	}
	const Received *down = Last(WM_LBUTTONDOWN);
	Check(down && GET_X_LPARAM(down->lp) == 5 && GET_Y_LPARAM(down->lp) == 6 && (down->wp & MK_LBUTTON) && (down->wp & MK_SHIFT), "WM_LBUTTONDOWN with position and keys");

	// Window scroll bars: the client area shrinks, and the bar reports
	// actions without moving until the window procedure says so.
	{
		g_hook = SetWindowsHookEx(WH_CBT, CbtProc, nullptr, GetCurrentThreadId());
		HWND sw = CreateWindowEx(0, L"LinOpenMPTTest", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL, 0, 0, 100, 100, hwnd, nullptr, nullptr, nullptr);
		UnhookWindowsHookEx(g_hook);
		Check(g_cbtCreates == 1, "CBT hook sees window creation");
		RECT rc{};
		GetClientRect(sw, &rc);
		const int extent = GetSystemMetrics(SM_CXVSCROLL);
		Check(extent > 0 && rc.right == 100 - extent && rc.bottom == 100, "WS_VSCROLL takes client width");
		SCROLLINFO si{sizeof(SCROLLINFO), SIF_RANGE | SIF_PAGE | SIF_POS, 0, 99, 10, 5, 0};
		SetScrollInfo(sw, SB_VERT, &si, TRUE);
		SCROLLINFO got{sizeof(SCROLLINFO), SIF_ALL};
		GetScrollInfo(sw, SB_VERT, &got);
		Check(got.nMin == 0 && got.nMax == 99 && got.nPage == 10 && got.nPos == 5, "Set/GetScrollInfo");
		g_log.clear();
		QScrollBar *bar = FromHandle(sw)->findChild<QScrollBar *>();
		Check(bar != nullptr && bar->isVisible() && bar->maximum() == 90, "scroll bar widget range");
		if(bar)
			bar->triggerAction(QAbstractSlider::SliderSingleStepAdd);
		Check(Last(WM_VSCROLL) && LOWORD(Last(WM_VSCROLL)->wp) == SB_ENDSCROLL, "WM_VSCROLL ends with SB_ENDSCROLL");
		bool sawLineDown = false;
		for(const auto &r : g_log)
			sawLineDown |= (r.msg == WM_VSCROLL && LOWORD(r.wp) == SB_LINEDOWN);
		Check(sawLineDown && GetScrollPos(sw, SB_VERT) == 5 && bar && bar->value() == 5, "SB_LINEDOWN; position stays until the window moves it");
		si.fMask = SIF_RANGE | SIF_PAGE;
		si.nMax = 5;
		SetScrollInfo(sw, SB_VERT, &si, TRUE);
		GetClientRect(sw, &rc);
		Check(rc.right == 100 && !bar->isVisible() && Last(WM_SIZE), "scroll bar hides when everything fits");
		DestroyWindow(sw);
	}

	// Memory DCs and blits.
	{
		HDC windowDC = GetDC(hwnd);
		HDC mem = CreateCompatibleDC(windowDC);
		HBITMAP bmp = CreateCompatibleBitmap(windowDC, 10, 10);
		HGDIOBJ old = SelectObject(mem, bmp);
		HBRUSH blue = CreateSolidBrush(kBlue);
		RECT r{0, 0, 10, 10};
		FillRect(mem, &r, blue);
		Check(GetPixel(mem, 3, 3) == kBlue, "FillRect into a memory bitmap");
		BitBlt(windowDC, 100, 10, 10, 10, mem, 0, 0, SRCCOPY);
		Check(GetPixel(windowDC, 105, 15) == kBlue, "BitBlt to a window DC");
		// XOR twice restores; alpha stays opaque.
		SelectObject(windowDC, GetStockObject(WHITE_BRUSH));
		PatBlt(windowDC, 100, 10, 10, 10, PATINVERT);
		Check(GetPixel(windowDC, 105, 15) == RGB(255, 255, 0), "PATINVERT");
		PatBlt(windowDC, 100, 10, 10, 10, PATINVERT);
		Check(GetPixel(windowDC, 105, 15) == kBlue, "PATINVERT twice restores");
		Check(qAlpha(FromHandle(hwnd)->surface.pixel(QPoint(105, 15) * FromHandle(hwnd)->surface.devicePixelRatio())) == 255, "raster ops keep pixels opaque");

		// DIB section: writes through the pointer show up.
		BITMAPINFO bi{};
		bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bi.bmiHeader.biWidth = 4;
		bi.bmiHeader.biHeight = -4;
		bi.bmiHeader.biPlanes = 1;
		bi.bmiHeader.biBitCount = 32;
		void *bits = nullptr;
		HBITMAP dib = CreateDIBSection(windowDC, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
		static_cast<uint32_t *>(bits)[4 * 2 + 1] = 0x00FF00; // green, top-down row 2, x 1
		SelectObject(mem, dib);
		BitBlt(windowDC, 120, 10, 4, 4, mem, 0, 0, SRCCOPY);
		Check(GetPixel(windowDC, 121, 12) == RGB(0, 255, 0), "DIB section bits blit");

		// Text measurement.
		SIZE size{};
		GetTextExtentPoint32(windowDC, L"Hello", 5, &size);
		Check(size.cx > 0 && size.cy > 0, "GetTextExtentPoint32");
		RECT calc{0, 0, 0, 0};
		const int h = DrawText(windowDC, L"Hello", -1, &calc, DT_CALCRECT | DT_SINGLELINE);
		Check(h > 0 && calc.right == size.cx, "DrawText DT_CALCRECT");

		SelectObject(mem, old);
		DeleteObject(dib);
		DeleteObject(bmp);
		DeleteObject(blue);
		DeleteDC(mem);
		ReleaseDC(hwnd, windowDC);
		Pump(20);
		Check(ScreenPixel(hwnd, 105, 15) == kBlue, "drawing outside WM_PAINT reaches the screen");
	}

	// Scrolling moves pixels and invalidates what scrolled in.
	{
		const int before = g_paints;
		ScrollWindowEx(hwnd, 0, 20, nullptr, nullptr, nullptr, nullptr, SW_INVALIDATE);
		Check(GetUpdateRect(hwnd, nullptr, FALSE), "ScrollWindowEx invalidates the uncovered strip");
		Pump(20);
		Check(g_paints == before + 1, "WM_PAINT after scrolling");
		Check(ScreenPixel(hwnd, 105, 35) == kBlue, "scrolled pixels moved");
	}

	// Child windows and destruction order.
	{
		HWND child = CreateWindowEx(0, L"LinOpenMPTTest", L"", WS_CHILD | WS_VISIBLE, 0, 0, 20, 20, hwnd, reinterpret_cast<HMENU>(1001), nullptr, nullptr);
		Check(GetParent(child) == hwnd && GetDlgItem(hwnd, 1001) == child && GetDlgCtrlID(child) == 1001, "child window and control ID");
		g_log.clear();
		DestroyWindow(hwnd);
		Check(!IsWindow(hwnd) && !IsWindow(child), "DestroyWindow destroys children");
		Check(Got(WM_DESTROY) && Got(WM_NCDESTROY), "WM_DESTROY and WM_NCDESTROY");
		Pump(10);
	}

	std::printf("%d failure(s)\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}
