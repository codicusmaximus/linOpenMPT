// The common controls (trackbars, up-down controls, progress bars), run
// offscreen: their messages, the notifications they send their parent, and
// mouse and key input, checked against comctl32's behaviour.
#include <afxwin.h>
#include <afxcmn.h>

#include "QtWin.h"

#include <QtGui/QKeyEvent>
#include <QtGui/QMouseEvent>
#include <QtWidgets/QApplication>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
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

struct Scroll
{
	UINT msg;
	WORD code;
	short pos;
	HWND from;
};
std::vector<Scroll> g_scrolls;
std::vector<NMUPDOWN> g_deltas;
std::vector<UINT> g_notifies;
int g_vetoDelta = 0;      // UDN_DELTAPOS answers nonzero
int g_changeDelta = 0;    // UDN_DELTAPOS changes iDelta to this
LRESULT g_customDraw = CDRF_DODEFAULT;
std::vector<DWORD> g_drawStages;
extern int g_refuseChange;
std::vector<NMLISTVIEW> g_lvChanges;
std::wstring g_editedText;
bool g_editAccept = true;
std::vector<NMTREEVIEW> g_tvEvents; // TVN_SELCHANGED, TVN_ITEMEXPANDED, TVN_DELETEITEM, TVN_BEGINDRAG
std::vector<HTREEITEM> g_selectedAtClick; // the selection when NM_CLICK arrives
HWND g_tree = nullptr;
int g_tvVeto = 0;

LRESULT CALLBACK ParentProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	switch(msg)
	{
	case WM_HSCROLL:
	case WM_VSCROLL:
		g_scrolls.push_back(Scroll{msg, LOWORD(wp), static_cast<short>(HIWORD(wp)), reinterpret_cast<HWND>(lp)});
		return 0;
	case WM_NOTIFY:
		{
			const auto *hdr = reinterpret_cast<const NMHDR *>(lp);
			g_notifies.push_back(hdr->code);
			if(hdr->code == UDN_DELTAPOS)
			{
				auto *nm = reinterpret_cast<NMUPDOWN *>(lp);
				g_deltas.push_back(*nm);
				if(g_changeDelta != 0)
					nm->iDelta = g_changeDelta;
				return g_vetoDelta;
			}
			if(hdr->code == NM_CUSTOMDRAW)
			{
				const auto *cd = reinterpret_cast<const NMCUSTOMDRAW *>(lp);
				g_drawStages.push_back(cd->dwDrawStage | (cd->dwDrawStage & CDDS_ITEM ? static_cast<DWORD>(cd->dwItemSpec) << 8 : 0));
				return cd->dwDrawStage == CDDS_PREPAINT ? g_customDraw : CDRF_DODEFAULT;
			}
			if(hdr->code == TCN_SELCHANGING)
				return g_refuseChange;
			if(hdr->code == LVN_ITEMCHANGED)
				g_lvChanges.push_back(*reinterpret_cast<const NMLISTVIEW *>(lp));
			if(hdr->code == LVN_GETDISPINFO)
			{
				// Owner data: the text is the item and subitem numbers.
				auto *di = reinterpret_cast<NMLVDISPINFO *>(lp);
				if((di->item.mask & LVIF_TEXT) && di->item.pszText != nullptr)
				{
					const std::wstring text = L"row " + std::to_wstring(di->item.iItem) + L"." + std::to_wstring(di->item.iSubItem);
					wcsncpy(di->item.pszText, text.c_str(), di->item.cchTextMax - 1);
					di->item.pszText[di->item.cchTextMax - 1] = 0;
				}
				return 0;
			}
			if(hdr->code == TVN_SELCHANGING || hdr->code == TVN_ITEMEXPANDING)
				return g_tvVeto;
			if(hdr->code == TVN_SELCHANGED || hdr->code == TVN_ITEMEXPANDED || hdr->code == TVN_DELETEITEM || hdr->code == TVN_BEGINDRAG || hdr->code == TVN_BEGINRDRAG)
				g_tvEvents.push_back(*reinterpret_cast<const NMTREEVIEW *>(lp));
			if(hdr->code == NM_CLICK && hdr->hwndFrom == g_tree)
				g_selectedAtClick.push_back(TreeView_GetSelection(g_tree));
			if(hdr->code == TVN_GETDISPINFO)
			{
				auto *di = reinterpret_cast<NMTVDISPINFO *>(lp);
				if(di->item.mask & TVIF_TEXT)
					wcsncpy(di->item.pszText, (L"callback " + std::to_wstring(di->item.lParam)).c_str(), di->item.cchTextMax - 1);
				if(di->item.mask & TVIF_CHILDREN)
					di->item.cChildren = 1;
				return 0;
			}
			if(hdr->code == TVN_ENDLABELEDIT)
			{
				const auto *di = reinterpret_cast<const NMTVDISPINFO *>(lp);
				g_editedText = di->item.pszText != nullptr ? di->item.pszText : L"(cancelled)";
				return g_editAccept ? TRUE : FALSE;
			}
			if(hdr->code == LVN_ENDLABELEDIT)
			{
				const auto *di = reinterpret_cast<const NMLVDISPINFO *>(lp);
				g_editedText = di->item.pszText != nullptr ? di->item.pszText : L"(cancelled)";
				return g_editAccept ? TRUE : FALSE;
			}
			return 0;
		}
	default:
		return ::DefWindowProc(hwnd, msg, wp, lp);
	}
}

void Mouse(HWND hwnd, QEvent::Type type, int x, int y)
{
	Win32Widget *w = FromHandle(hwnd);
	if(w == nullptr)
		return;
	const QPoint local = QPoint(x, y) + ClientOrigin(w);
	const Qt::MouseButtons buttons = type == QEvent::MouseButtonRelease ? Qt::NoButton : Qt::LeftButton;
	const Qt::MouseButton button = type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton;
	QMouseEvent e(type, QPointF(local), QPointF(w->mapToGlobal(local)), button, buttons, Qt::NoModifier);
	QApplication::sendEvent(w, &e);
	Pump();
}

void Wheel(HWND hwnd, int delta)
{
	Win32Widget *w = FromHandle(hwnd);
	if(w == nullptr)
		return;
	const QPointF local(2, 2);
	QWheelEvent e(local, w->mapToGlobal(local), QPoint(), QPoint(0, delta), Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
	QApplication::sendEvent(w, &e);
	Pump();
}

void Key(int key)
{
	QWidget *target = QApplication::focusWidget();
	if(target == nullptr)
		return;
	QKeyEvent press(QEvent::KeyPress, key, Qt::NoModifier);
	QApplication::sendEvent(target, &press);
	QKeyEvent release(QEvent::KeyRelease, key, Qt::NoModifier);
	if(QWidget *again = QApplication::focusWidget())
		QApplication::sendEvent(again, &release);
	Pump();
}

void Type(char ch)
{
	QWidget *target = QApplication::focusWidget();
	if(target == nullptr)
		return;
	const QString text(QChar::fromLatin1(ch));
	QKeyEvent press(QEvent::KeyPress, Qt::Key_A + (QChar::fromLatin1(ch).toUpper().unicode() - 'A'), Qt::NoModifier, text);
	QApplication::sendEvent(target, &press);
	QKeyEvent release(QEvent::KeyRelease, press.key(), Qt::NoModifier, text);
	QApplication::sendEvent(target, &release);
	Pump();
}

// With LINOPENMPT_SHOTS=<dir>, a picture of the window, to look at.
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

bool HasScroll(WORD code)
{
	for(const Scroll &s : g_scrolls)
	{
		if(s.code == code)
			return true;
	}
	return false;
}

void Trackbars(HWND parent)
{
	HWND tb = ::CreateWindowEx(0, TRACKBAR_CLASS, nullptr, WS_CHILD | WS_VISIBLE | WS_TABSTOP | TBS_AUTOTICKS, 10, 10, 200, 30, parent, reinterpret_cast<HMENU>(100), nullptr, nullptr);
	Check(tb != nullptr, "trackbar: created");
	Check(::SendMessage(tb, TBM_GETRANGEMIN, 0, 0) == 0 && ::SendMessage(tb, TBM_GETRANGEMAX, 0, 0) == 100, "trackbar: default range 0..100");
	Check(::SendMessage(tb, TBM_GETPAGESIZE, 0, 0) == 20 && ::SendMessage(tb, TBM_GETLINESIZE, 0, 0) == 1, "trackbar: default page 20, line 1");
	::SendMessage(tb, TBM_SETRANGE, TRUE, MAKELPARAM(0, 50));
	Check(::SendMessage(tb, TBM_GETPAGESIZE, 0, 0) == 10, "trackbar: the page follows the range (a fifth)");
	::SendMessage(tb, TBM_SETPAGESIZE, 0, 7);
	::SendMessage(tb, TBM_SETRANGEMAX, TRUE, 200);
	Check(::SendMessage(tb, TBM_GETPAGESIZE, 0, 0) == 7, "trackbar: a page size set stays");
	::SendMessage(tb, TBM_SETPOS, TRUE, 500);
	Check(::SendMessage(tb, TBM_GETPOS, 0, 0) == 200, "trackbar: TBM_SETPOS clamps");
	::SendMessage(tb, TBM_SETRANGEMIN, TRUE, -50);
	::SendMessage(tb, TBM_SETPOS, TRUE, -60);
	Check(::SendMessage(tb, TBM_GETPOS, 0, 0) == -50, "trackbar: negative ranges");
	Check(g_scrolls.empty(), "trackbar: TBM_SETPOS does not notify");

	::SendMessage(tb, TBM_SETRANGEMIN, FALSE, 0);
	::SendMessage(tb, TBM_SETRANGEMAX, FALSE, 10);
	::SendMessage(tb, TBM_SETTICFREQ, 2, 0);
	Check(::SendMessage(tb, TBM_GETNUMTICS, 0, 0) == 6, "trackbar: TBS_AUTOTICKS tics (0, 2, 4, 6, 8, 10)");
	Check(::SendMessage(tb, TBM_GETTIC, 0, 0) == 2, "trackbar: TBM_GETTIC skips the first tic");
	Check(::SendMessage(tb, TBM_GETTICPOS, 0, 0) > 0, "trackbar: TBM_GETTICPOS");

	// Keys: lines and pages, nothing at the end, TB_ENDTRACK on release.
	::SendMessage(tb, TBM_SETPOS, TRUE, 0);
	::SetFocus(tb);
	Pump();
	Check(::GetFocus() == tb, "trackbar: takes the focus");
	Check(::SendMessage(tb, WM_GETDLGCODE, 0, 0) & DLGC_WANTARROWS, "trackbar: wants arrows");
	g_scrolls.clear();
	Key(Qt::Key_Right);
	Check(::SendMessage(tb, TBM_GETPOS, 0, 0) == 1, "trackbar: Right is a line");
	Check(g_scrolls.size() == 2 && g_scrolls[0].msg == WM_HSCROLL && g_scrolls[0].code == TB_LINEDOWN && g_scrolls[1].code == TB_ENDTRACK && g_scrolls[0].from == tb,
		"trackbar: WM_HSCROLL TB_LINEDOWN, then TB_ENDTRACK");
	Key(Qt::Key_PageDown);
	Check(::SendMessage(tb, TBM_GETPOS, 0, 0) == 8, "trackbar: PageDown is a page (7, as set)");
	Key(Qt::Key_End);
	Check(::SendMessage(tb, TBM_GETPOS, 0, 0) == 10, "trackbar: End");
	g_scrolls.clear();
	Key(Qt::Key_Right);
	Check(g_scrolls.size() == 1 && g_scrolls[0].code == TB_ENDTRACK, "trackbar: at the end, only TB_ENDTRACK");
	Key(Qt::Key_Home);
	Key(Qt::Key_Up);
	Check(::SendMessage(tb, TBM_GETPOS, 0, 0) == 0, "trackbar: Up is a line up");

	// The thumb: dragging tracks, releasing gives the position.
	RECT thumb{};
	::SendMessage(tb, TBM_GETTHUMBRECT, 0, reinterpret_cast<LPARAM>(&thumb));
	Check(thumb.right > thumb.left && thumb.bottom > thumb.top, "trackbar: TBM_GETTHUMBRECT");
	RECT channel{};
	::SendMessage(tb, TBM_GETCHANNELRECT, 0, reinterpret_cast<LPARAM>(&channel));
	Check(channel.right - channel.left > 100, "trackbar: TBM_GETCHANNELRECT");
	g_scrolls.clear();
	const int cy = (thumb.top + thumb.bottom) / 2;
	const int cx = (thumb.left + thumb.right) / 2;
	Mouse(tb, QEvent::MouseButtonPress, cx, cy);
	Mouse(tb, QEvent::MouseMove, channel.right, cy);
	Check(::SendMessage(tb, TBM_GETPOS, 0, 0) == 10, "trackbar: dragging the thumb to the end");
	Check(HasScroll(TB_THUMBTRACK) && g_scrolls.back().pos == 10, "trackbar: TB_THUMBTRACK with the position");
	Mouse(tb, QEvent::MouseMove, (channel.left + channel.right) / 2, cy);
	const LRESULT middle = ::SendMessage(tb, TBM_GETPOS, 0, 0);
	Check(middle >= 4 && middle <= 6, "trackbar: dragging to the middle");
	Mouse(tb, QEvent::MouseButtonRelease, (channel.left + channel.right) / 2, cy);
	Check(g_scrolls.size() >= 3 && g_scrolls[g_scrolls.size() - 2].code == TB_THUMBPOSITION && g_scrolls[g_scrolls.size() - 2].pos == middle && g_scrolls.back().code == TB_ENDTRACK,
		"trackbar: TB_THUMBPOSITION, then TB_ENDTRACK");
	Check(::GetCapture() == nullptr, "trackbar: capture released");

	// A click in the channel pages toward it.
	::SendMessage(tb, TBM_SETPAGESIZE, 0, 2);
	::SendMessage(tb, TBM_SETPOS, TRUE, 0);
	g_scrolls.clear();
	Mouse(tb, QEvent::MouseButtonPress, channel.right - 2, cy);
	Mouse(tb, QEvent::MouseButtonRelease, channel.right - 2, cy);
	Check(::SendMessage(tb, TBM_GETPOS, 0, 0) == 2, "trackbar: a click in the channel is a page");
	Check(!g_scrolls.empty() && g_scrolls.front().code == TB_PAGEDOWN && g_scrolls.back().code == TB_ENDTRACK, "trackbar: TB_PAGEDOWN, then TB_ENDTRACK");

	// The wheel: up moves toward the minimum.
	::SendMessage(tb, TBM_SETPOS, TRUE, 5);
	g_scrolls.clear();
	Wheel(tb, -WHEEL_DELTA);
	Check(::SendMessage(tb, TBM_GETPOS, 0, 0) == 8, "trackbar: wheel down is three lines toward the maximum");
	Check(!g_scrolls.empty() && g_scrolls.front().code == TB_LINEDOWN, "trackbar: wheel notifies TB_LINEDOWN");

	// Custom draw.
	g_customDraw = CDRF_NOTIFYITEMDRAW;
	g_drawStages.clear();
	::InvalidateRect(tb, nullptr, TRUE);
	::UpdateWindow(tb);
	Pump();
	auto has = [](DWORD stage)
	{
		return std::find(g_drawStages.begin(), g_drawStages.end(), stage) != g_drawStages.end();
	};
	Check(has(CDDS_PREPAINT) && has(CDDS_ITEMPREPAINT | (TBCD_CHANNEL << 8)) && has(CDDS_ITEMPREPAINT | (TBCD_THUMB << 8)) && has(CDDS_ITEMPREPAINT | (TBCD_TICS << 8)),
		"trackbar: NM_CUSTOMDRAW for the tics, channel and thumb");
	g_customDraw = CDRF_DODEFAULT;

	SaveShot(parent, "trackbar");
	// Vertical: WM_VSCROLL, minimum at the top.
	HWND vt = ::CreateWindowEx(0, TRACKBAR_CLASS, nullptr, WS_CHILD | WS_VISIBLE | TBS_VERT, 220, 10, 30, 150, parent, reinterpret_cast<HMENU>(101), nullptr, nullptr);
	::SendMessage(vt, TBM_SETRANGE, TRUE, MAKELPARAM(0, 10));
	RECT top{}, bottom{};
	::SendMessage(vt, TBM_GETTHUMBRECT, 0, reinterpret_cast<LPARAM>(&top));
	::SendMessage(vt, TBM_SETPOS, TRUE, 10);
	::SendMessage(vt, TBM_GETTHUMBRECT, 0, reinterpret_cast<LPARAM>(&bottom));
	Check(top.top < bottom.top, "trackbar: vertical minimum at the top");
	g_scrolls.clear();
	::SetFocus(vt);
	Pump();
	Key(Qt::Key_Up);
	Check(!g_scrolls.empty() && g_scrolls[0].msg == WM_VSCROLL && g_scrolls[0].code == TB_LINEUP, "trackbar: vertical sends WM_VSCROLL");

	// MFC's wrapper.
	CSliderCtrl slider;
	slider.Attach(tb);
	slider.SetRange(-1000, 100000);
	Check(slider.GetRangeMin() == -1000 && slider.GetRangeMax() == 100000, "CSliderCtrl: 32-bit ranges");
	slider.SetPos(1234);
	Check(slider.GetPos() == 1234, "CSliderCtrl: SetPos and GetPos");
	slider.Detach();
	::DestroyWindow(vt);
	::DestroyWindow(tb);
}

void UpDowns(HWND parent)
{
	HWND edit = ::CreateWindowEx(WS_EX_CLIENTEDGE, L"Edit", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 10, 60, 80, 22, parent, reinterpret_cast<HMENU>(200), nullptr, nullptr);
	HWND ud = ::CreateWindowEx(0, UPDOWN_CLASS, nullptr, WS_CHILD | WS_VISIBLE | UDS_AUTOBUDDY | UDS_SETBUDDYINT | UDS_ALIGNRIGHT | UDS_ARROWKEYS, 0, 0, 16, 10, parent, reinterpret_cast<HMENU>(201), nullptr, nullptr);
	Check(ud != nullptr, "up-down: created");
	Check(reinterpret_cast<HWND>(::SendMessage(ud, UDM_GETBUDDY, 0, 0)) == edit, "up-down: UDS_AUTOBUDDY takes the previous control");
	RECT er{}, ur{};
	::GetWindowRect(edit, &er);
	::GetWindowRect(ud, &ur);
	Check(er.right - er.left == 64 && ur.left == er.right && ur.top == er.top && ur.bottom == er.bottom, "up-down: UDS_ALIGNRIGHT sits at the buddy's right, as tall as it");
	const LRESULT range = ::SendMessage(ud, UDM_GETRANGE, 0, 0);
	Check(static_cast<short>(HIWORD(range)) == 100 && static_cast<short>(LOWORD(range)) == 0, "up-down: default range 100..0");

	::SendMessage(ud, UDM_SETRANGE32, 0, 2000);
	::SendMessage(ud, UDM_SETPOS32, 0, 1500);
	Check(Text(edit) == ToWString(QStringLiteral("1") + QLocale().groupSeparator() + QStringLiteral("500")), "up-down: UDS_SETBUDDYINT writes the buddy, with thousands");
	::SetWindowText(edit, L"42");
	BOOL error = TRUE;
	Check(::SendMessage(ud, UDM_GETPOS32, 0, reinterpret_cast<LPARAM>(&error)) == 42 && !error, "up-down: UDM_GETPOS32 reads the buddy");
	::SetWindowText(edit, L"abc");
	Check(::SendMessage(ud, UDM_GETPOS32, 0, reinterpret_cast<LPARAM>(&error)) == 42 && error, "up-down: an invalid buddy text is an error");
	::SetWindowText(edit, L"5000");
	::SendMessage(ud, UDM_GETPOS32, 0, reinterpret_cast<LPARAM>(&error));
	Check(error, "up-down: an out of range buddy text is an error");
	::SendMessage(ud, UDM_SETPOS32, 0, 9999);
	Check(::SendMessage(ud, UDM_GETPOS32, 0, 0) == 2000, "up-down: UDM_SETPOS32 clamps");

	// The arrows: UDN_DELTAPOS, then the scroll message with the position.
	::SendMessage(ud, UDM_SETPOS32, 0, 10);
	RECT client{};
	::GetClientRect(ud, &client);
	g_scrolls.clear();
	g_deltas.clear();
	Mouse(ud, QEvent::MouseButtonPress, client.right / 2, 2);
	Check(::GetFocus() == edit, "up-down: a click focuses the edit buddy");
	Check(g_deltas.size() == 1 && g_deltas[0].iPos == 10 && g_deltas[0].iDelta == 1, "up-down: UDN_DELTAPOS before the change");
	Check(::SendMessage(ud, UDM_GETPOS32, 0, 0) == 11 && Text(edit) == L"11", "up-down: the up arrow increments");
	Check(g_scrolls.size() == 1 && g_scrolls[0].msg == WM_VSCROLL && g_scrolls[0].code == SB_THUMBPOSITION && g_scrolls[0].pos == 11 && g_scrolls[0].from == ud,
		"up-down: WM_VSCROLL SB_THUMBPOSITION with the position");
	Mouse(ud, QEvent::MouseButtonRelease, client.right / 2, 2);
	Check(g_scrolls.size() == 2 && g_scrolls[1].code == SB_ENDSCROLL, "up-down: SB_ENDSCROLL on release");
	Mouse(ud, QEvent::MouseButtonPress, client.right / 2, client.bottom - 2);
	Mouse(ud, QEvent::MouseButtonRelease, client.right / 2, client.bottom - 2);
	Check(::SendMessage(ud, UDM_GETPOS32, 0, 0) == 10, "up-down: the down arrow decrements");

	// A reversed range: up goes toward "upper".
	::SendMessage(ud, UDM_SETRANGE32, 20, 0);
	::SendMessage(ud, UDM_SETPOS32, 0, 10);
	g_deltas.clear();
	Mouse(ud, QEvent::MouseButtonPress, client.right / 2, 2);
	Mouse(ud, QEvent::MouseButtonRelease, client.right / 2, 2);
	Check(::SendMessage(ud, UDM_GETPOS32, 0, 0) == 9 && !g_deltas.empty() && g_deltas[0].iDelta == -1, "up-down: a reversed range counts down");

	// The parent can veto or change the step.
	::SendMessage(ud, UDM_SETRANGE32, 0, 100);
	::SendMessage(ud, UDM_SETPOS32, 0, 10);
	g_vetoDelta = 1;
	Mouse(ud, QEvent::MouseButtonPress, client.right / 2, 2);
	Mouse(ud, QEvent::MouseButtonRelease, client.right / 2, 2);
	g_vetoDelta = 0;
	Check(::SendMessage(ud, UDM_GETPOS32, 0, 0) == 10, "up-down: a nonzero UDN_DELTAPOS answer vetoes");
	g_changeDelta = 5;
	Mouse(ud, QEvent::MouseButtonPress, client.right / 2, 2);
	Mouse(ud, QEvent::MouseButtonRelease, client.right / 2, 2);
	g_changeDelta = 0;
	Check(::SendMessage(ud, UDM_GETPOS32, 0, 0) == 15, "up-down: the parent can change iDelta");

	// Acceleration.
	UDACCEL accel[] = {{0, 10}, {2, 100}};
	Check(::SendMessage(ud, UDM_SETACCEL, 2, reinterpret_cast<LPARAM>(accel)) == TRUE, "up-down: UDM_SETACCEL");
	UDACCEL back[2]{};
	Check(::SendMessage(ud, UDM_GETACCEL, 2, reinterpret_cast<LPARAM>(back)) == 2 && back[1].nInc == 100, "up-down: UDM_GETACCEL");
	Mouse(ud, QEvent::MouseButtonPress, client.right / 2, 2);
	Mouse(ud, QEvent::MouseButtonRelease, client.right / 2, 2);
	Check(::SendMessage(ud, UDM_GETPOS32, 0, 0) == 25, "up-down: the first acceleration step");

	// The buddy's arrow keys and the clamp at the end; UDS_WRAP wraps.
	::SetFocus(edit);
	Pump();
	Key(Qt::Key_Up);
	Check(::SendMessage(ud, UDM_GETPOS32, 0, 0) == 35, "up-down: UDS_ARROWKEYS through the buddy");
	::SendMessage(ud, UDM_SETPOS32, 0, 95);
	Key(Qt::Key_Up);
	Check(::SendMessage(ud, UDM_GETPOS32, 0, 0) == 100, "up-down: stops at the end");
	::SetWindowLongPtr(ud, GWL_STYLE, ::GetWindowLongPtr(ud, GWL_STYLE) | UDS_WRAP);
	Key(Qt::Key_Up);
	Check(::SendMessage(ud, UDM_GETPOS32, 0, 0) == 0, "up-down: UDS_WRAP wraps");

	// Base 16.
	Check(::SendMessage(ud, UDM_SETBASE, 16, 0) == 10, "up-down: UDM_SETBASE");
	::SendMessage(ud, UDM_SETPOS32, 0, 26);
	Check(Text(edit) == L"0x001A", "up-down: hexadecimal buddy text");

	// MFC's wrapper.
	CSpinButtonCtrl spin;
	spin.Attach(ud);
	spin.SetBase(10);
	spin.SetRange(1, 9);
	int lower = 0, upper = 0;
	spin.GetRange(lower, upper);
	Check(lower == 1 && upper == 9, "CSpinButtonCtrl: SetRange and GetRange");
	spin.SetPos(7);
	Check(LOWORD(spin.GetPos()) == 7 && HIWORD(spin.GetPos()) == 0, "CSpinButtonCtrl: SetPos and GetPos");
	Check(spin.GetBuddy() != nullptr && spin.GetBuddy()->m_hWnd == edit, "CSpinButtonCtrl: GetBuddy");
	spin.Detach();
	::DestroyWindow(ud);
	::DestroyWindow(edit);
}

void Progress(HWND parent)
{
	HWND pb = ::CreateWindowEx(0, PROGRESS_CLASS, nullptr, WS_CHILD | WS_VISIBLE, 10, 100, 200, 16, parent, reinterpret_cast<HMENU>(300), nullptr, nullptr);
	Check(pb != nullptr, "progress: created");
	PBRANGE r{};
	::SendMessage(pb, PBM_GETRANGE, 0, reinterpret_cast<LPARAM>(&r));
	Check(r.iLow == 0 && r.iHigh == 100, "progress: default range 0..100");
	Check(::SendMessage(pb, PBM_SETPOS, 50, 0) == 0 && ::SendMessage(pb, PBM_GETPOS, 0, 0) == 50, "progress: PBM_SETPOS returns the old position");
	::SendMessage(pb, PBM_SETPOS, 500, 0);
	Check(::SendMessage(pb, PBM_GETPOS, 0, 0) == 100, "progress: PBM_SETPOS clamps");
	::SendMessage(pb, PBM_SETRANGE32, 0, 1000000);
	::SendMessage(pb, PBM_SETPOS, 0, 0);
	::SendMessage(pb, PBM_DELTAPOS, 250000, 0);
	Check(::SendMessage(pb, PBM_GETPOS, 0, 0) == 250000, "progress: 32-bit ranges and PBM_DELTAPOS");
	::SendMessage(pb, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
	::SendMessage(pb, PBM_SETPOS, 95, 0);
	::SendMessage(pb, PBM_STEPIT, 0, 0);
	Check(::SendMessage(pb, PBM_GETPOS, 0, 0) == 5, "progress: PBM_STEPIT wraps past the end");
	::SetWindowLongPtr(pb, GWL_STYLE, ::GetWindowLongPtr(pb, GWL_STYLE) | PBS_MARQUEE);
	Check(::SendMessage(pb, PBM_SETMARQUEE, TRUE, 30) == TRUE, "progress: PBM_SETMARQUEE");
	::UpdateWindow(pb);
	Pump();
	CProgressCtrl progress;
	progress.Attach(pb);
	progress.SetRange32(10, 20);
	progress.SetPos(15);
	Check(progress.GetPos() == 15, "CProgressCtrl: SetRange32 and SetPos");
	progress.Detach();
	::DestroyWindow(pb);
}


void ImageLists()
{
	HIMAGELIST list = ImageList_Create(4, 4, ILC_COLOR32 | ILC_MASK, 2, 2);
	Check(list != nullptr, "image list: created");
	// A strip of two 4x4 images, magenta as the mask colour.
	BITMAPINFO bi{};
	bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bi.bmiHeader.biWidth = 8;
	bi.bmiHeader.biHeight = -4;
	bi.bmiHeader.biPlanes = 1;
	bi.bmiHeader.biBitCount = 32;
	void *bits = nullptr;
	HDC screen = ::GetDC(nullptr);
	HBITMAP strip = ::CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
	auto *px = static_cast<uint32_t *>(bits);
	for(int i = 0; i < 32; i++)
		px[i] = (i % 8) < 4 ? 0x0000FF : 0xFF00FF; // blue, then magenta
	px[8 + 5] = 0x00FF00; // a green pixel in the second image
	Check(ImageList_AddMasked(list, strip, RGB(255, 0, 255)) == 0, "image list: AddMasked returns the first index");
	Check(ImageList_GetImageCount(list) == 2, "image list: a strip adds one image per width");
	int cx = 0, cy = 0;
	Check(ImageList_GetIconSize(list, &cx, &cy) && cx == 4 && cy == 4, "image list: icon size");
	// Drawn over red: the mask colour is transparent.
	HDC mem = ::CreateCompatibleDC(screen);
	HBITMAP target = ::CreateCompatibleBitmap(screen, 8, 4);
	HGDIOBJ old = ::SelectObject(mem, target);
	RECT all{0, 0, 8, 4};
	HBRUSH red = ::CreateSolidBrush(RGB(255, 0, 0));
	::FillRect(mem, &all, red);
	ImageList_Draw(list, 0, mem, 0, 0, ILD_NORMAL);
	ImageList_Draw(list, 1, mem, 4, 0, ILD_TRANSPARENT);
	Check(::GetPixel(mem, 1, 1) == RGB(0, 0, 255), "image list: draws the image");
	Check(::GetPixel(mem, 4, 0) == RGB(255, 0, 0), "image list: the mask colour is transparent");
	Check(::GetPixel(mem, 5, 1) == RGB(0, 255, 0), "image list: the second image of the strip");
	::SelectObject(mem, old);
	::DeleteObject(red);
	::DeleteObject(target);
	::DeleteDC(mem);
	::ReleaseDC(nullptr, screen);
	Check(ImageList_Remove(list, 0) && ImageList_GetImageCount(list) == 1, "image list: Remove");
	ImageList_SetIconSize(list, 8, 8);
	Check(ImageList_GetImageCount(list) == 0, "image list: SetIconSize empties it");
	::DeleteObject(strip);
	CImageList wrapper;
	Check(wrapper.Create(16, 16, ILC_COLOR32 | ILC_MASK, 1, 1) && wrapper.GetImageCount() == 0, "CImageList: Create");
	Check(wrapper.SetImageCount(3) && wrapper.GetImageCount() == 3, "CImageList: SetImageCount");
	ImageList_Destroy(list);
}

int g_selChanging = 0, g_selChange = 0, g_refuseChange = 0;

void Tabs(HWND parent)
{
	HWND tab = ::CreateWindowEx(0, WC_TABCONTROL, nullptr, WS_CHILD | WS_VISIBLE | WS_TABSTOP, 10, 130, 300, 120, parent, reinterpret_cast<HMENU>(400), nullptr, nullptr);
	Check(tab != nullptr, "tab: created");
	Check(::SendMessage(tab, TCM_GETCURSEL, 0, 0) == -1, "tab: nothing selected when empty");
	TCITEM item{};
	item.mask = TCIF_TEXT | TCIF_PARAM;
	item.pszText = const_cast<LPTSTR>(L"General");
	item.lParam = 11;
	Check(::SendMessage(tab, TCM_INSERTITEM, 0, reinterpret_cast<LPARAM>(&item)) == 0, "tab: TCM_INSERTITEM returns the index");
	Check(::SendMessage(tab, TCM_GETCURSEL, 0, 0) == 0, "tab: the first item is selected");
	item.pszText = const_cast<LPTSTR>(L"Patterns");
	item.lParam = 22;
	::SendMessage(tab, TCM_INSERTITEM, 1, reinterpret_cast<LPARAM>(&item));
	item.pszText = const_cast<LPTSTR>(L"Samples");
	item.lParam = 33;
	Check(::SendMessage(tab, TCM_INSERTITEM, 99, reinterpret_cast<LPARAM>(&item)) == 2, "tab: an index past the end appends");
	Check(::SendMessage(tab, TCM_GETITEMCOUNT, 0, 0) == 3, "tab: TCM_GETITEMCOUNT");
	wchar_t text[32] = {};
	TCITEM get{};
	get.mask = TCIF_TEXT | TCIF_PARAM;
	get.pszText = text;
	get.cchTextMax = 32;
	Check(::SendMessage(tab, TCM_GETITEM, 1, reinterpret_cast<LPARAM>(&get)) && std::wstring(text) == L"Patterns" && get.lParam == 22, "tab: TCM_GETITEM");

	// Rectangles and hit testing.
	RECT r0{}, r1{};
	::SendMessage(tab, TCM_GETITEMRECT, 0, reinterpret_cast<LPARAM>(&r0));
	::SendMessage(tab, TCM_GETITEMRECT, 1, reinterpret_cast<LPARAM>(&r1));
	Check(r0.right <= r1.left && r0.top == r1.top && r1.right - r1.left >= 54, "tab: items side by side, at least the minimum width");
	TCHITTESTINFO hit{};
	hit.pt = POINT{(r1.left + r1.right) / 2, (r1.top + r1.bottom) / 2};
	Check(::SendMessage(tab, TCM_HITTEST, 0, reinterpret_cast<LPARAM>(&hit)) == 1 && (hit.flags & TCHT_ONITEM), "tab: TCM_HITTEST");
	RECT display{0, 0, 300, 120};
	::SendMessage(tab, TCM_ADJUSTRECT, FALSE, reinterpret_cast<LPARAM>(&display));
	Check(display.left == 4 && display.right == 296 && display.bottom == 116 && display.top == 4 + (r0.bottom - r0.top), "tab: TCM_ADJUSTRECT takes off the tabs and the border");
	::SendMessage(tab, TCM_ADJUSTRECT, TRUE, reinterpret_cast<LPARAM>(&display));
	Check(display.left == 0 && display.top == 0 && display.right == 300 && display.bottom == 120, "tab: TCM_ADJUSTRECT back");

	// A click: TCN_SELCHANGING, then TCN_SELCHANGE; the parent can refuse.
	g_notifies.clear();
	Mouse(tab, QEvent::MouseButtonPress, hit.pt.x, hit.pt.y);
	Mouse(tab, QEvent::MouseButtonRelease, hit.pt.x, hit.pt.y);
	Check(::SendMessage(tab, TCM_GETCURSEL, 0, 0) == 1, "tab: a click selects");
	Check(g_notifies.size() >= 3 && g_notifies[0] == TCN_SELCHANGING && g_notifies[1] == TCN_SELCHANGE && g_notifies[2] == NM_CLICK, "tab: TCN_SELCHANGING, TCN_SELCHANGE, NM_CLICK");
	g_refuseChange = 1;
	::SendMessage(tab, TCM_GETITEMRECT, 2, reinterpret_cast<LPARAM>(&r1));
	Mouse(tab, QEvent::MouseButtonPress, (r1.left + r1.right) / 2, (r1.top + r1.bottom) / 2);
	Mouse(tab, QEvent::MouseButtonRelease, (r1.left + r1.right) / 2, (r1.top + r1.bottom) / 2);
	g_refuseChange = 0;
	Check(::SendMessage(tab, TCM_GETCURSEL, 0, 0) == 1, "tab: a nonzero TCN_SELCHANGING answer refuses");

	// Keys.
	::SetFocus(tab);
	Pump();
	g_notifies.clear();
	Key(Qt::Key_Right);
	Check(::SendMessage(tab, TCM_GETCURSEL, 0, 0) == 2, "tab: Right selects the next tab");
	Check(!g_notifies.empty() && g_notifies[0] == TCN_KEYDOWN, "tab: TCN_KEYDOWN first");

	// TCM_SETCURSEL does not notify.
	g_notifies.clear();
	Check(::SendMessage(tab, TCM_SETCURSEL, 0, 0) == 2 && ::SendMessage(tab, TCM_GETCURSEL, 0, 0) == 0, "tab: TCM_SETCURSEL returns the old selection");
	Check(g_notifies.empty(), "tab: TCM_SETCURSEL does not notify");
	Check(::SendMessage(tab, TCM_SETCURSEL, 7, 0) == -1, "tab: TCM_SETCURSEL out of range fails");

	// Deleting.
	::SendMessage(tab, TCM_SETCURSEL, 2, 0);
	::SendMessage(tab, TCM_DELETEITEM, 0, 0);
	Check(::SendMessage(tab, TCM_GETCURSEL, 0, 0) == 1 && ::SendMessage(tab, TCM_GETITEMCOUNT, 0, 0) == 2, "tab: deleting before the selection moves it");

	// Many tabs on one row scroll; with TCS_MULTILINE they wrap.
	for(int i = 0; i < 10; i++)
	{
		item.pszText = const_cast<LPTSTR>(L"Another tab");
		::SendMessage(tab, TCM_INSERTITEM, 99, reinterpret_cast<LPARAM>(&item));
	}
	Check(::SendMessage(tab, TCM_GETROWCOUNT, 0, 0) == 1, "tab: one row by default");
	::SendMessage(tab, TCM_SETCURSEL, 11, 0);
	RECT last{};
	::SendMessage(tab, TCM_GETITEMRECT, 11, reinterpret_cast<LPARAM>(&last));
	Check(last.right <= 300, "tab: TCM_SETCURSEL scrolls the tab into view");
	SaveShot(parent, "tabs_scrolled");
	::DestroyWindow(tab);
	HWND multi = ::CreateWindowEx(0, WC_TABCONTROL, nullptr, WS_CHILD | WS_VISIBLE | TCS_MULTILINE, 10, 130, 300, 120, parent, reinterpret_cast<HMENU>(401), nullptr, nullptr);
	for(int i = 0; i < 8; i++)
	{
		item.pszText = const_cast<LPTSTR>(L"Another tab");
		::SendMessage(multi, TCM_INSERTITEM, 99, reinterpret_cast<LPARAM>(&item));
	}
	Check(::SendMessage(multi, TCM_GETROWCOUNT, 0, 0) > 1, "tab: TCS_MULTILINE wraps");
	::SendMessage(multi, TCM_SETCURSEL, 2, 0);
	SaveShot(parent, "tabs_multiline");
	RECT first{}, lastInRow{};
	::SendMessage(multi, TCM_GETITEMRECT, 0, reinterpret_cast<LPARAM>(&first));
	Check(first.left == 2, "tab: rows start at the left");
	(void)lastInRow;

	CTabCtrl wrapper;
	wrapper.Attach(multi);
	wrapper.DeleteAllItems();
	Check(wrapper.GetItemCount() == 0 && wrapper.GetCurSel() == -1, "CTabCtrl: DeleteAllItems");
	wrapper.InsertItem(0, L"One");
	wrapper.InsertItem(TCIF_TEXT | TCIF_PARAM, 1, L"Two", 0, 42);
	TCITEM w{};
	w.mask = TCIF_PARAM;
	Check(wrapper.GetItem(1, &w) && w.lParam == 42, "CTabCtrl: InsertItem with data");
	wrapper.Detach();
	::DestroyWindow(multi);
}


int CALLBACK CompareText(LPARAM a, LPARAM b, LPARAM descending)
{
	const int result = static_cast<int>(a) - static_cast<int>(b);
	return descending ? -result : result;
}

class SortedList : public CMFCListCtrl
{
public:
	int OnCompareItems(LPARAM a, LPARAM b, int) override { return static_cast<int>(a) - static_cast<int>(b); }
	COLORREF OnGetCellBkColor(int row, int) override { return row == 1 ? RGB(255, 220, 220) : GetBkColor(); }
};

void ListViews(HWND parent)
{
	HWND lv = ::CreateWindowEx(WS_EX_CLIENTEDGE, WC_LISTVIEW, nullptr, WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_EDITLABELS, 10, 10, 360, 200, parent, reinterpret_cast<HMENU>(500), nullptr, nullptr);
	Check(lv != nullptr, "list view: created");
	HWND header = ListView_GetHeader(lv);
	Check(header != nullptr && ::GetParent(header) == lv, "list view: a header child in report view");
	CListCtrl list;
	list.Attach(lv);
	Check(list.InsertColumn(0, L"Name", LVCFMT_LEFT, 120) == 0 && list.InsertColumn(1, L"Size", LVCFMT_RIGHT, 80) == 1 && list.InsertColumn(2, L"Type", LVCFMT_LEFT, 90) == 2, "list view: InsertColumn");
	Check(Header_GetItemCount(header) == 3 && list.GetColumnWidth(1) == 80, "list view: columns become header items");
	const wchar_t *names[] = {L"kick.wav", L"snare.wav", L"hihat.wav", L"bass.wav"};
	const int sizes[] = {40, 10, 30, 20};
	for(int i = 0; i < 4; i++)
	{
		const int index = list.InsertItem(LVIF_TEXT | LVIF_PARAM, i, names[i], 0, 0, 0, sizes[i]);
		list.SetItemText(index, 1, std::to_wstring(sizes[i]).c_str());
		list.SetItemText(index, 2, L"Sample");
	}
	Check(list.GetItemCount() == 4, "list view: InsertItem");
	Check(list.GetItemText(1, 0) == CString(L"snare.wav") && list.GetItemText(1, 1) == CString(L"10"), "list view: GetItemText, item and subitem");
	Check(list.GetItemData(2) == 30, "list view: GetItemData");

	// Rows go down from below the header.
	RECT h{}, r0{}, r1{};
	::GetWindowRect(header, &h);
	list.GetItemRect(0, &r0, LVIR_BOUNDS);
	list.GetItemRect(1, &r1, LVIR_BOUNDS);
	Check(r0.top == h.bottom - h.top && r1.top == r0.bottom && r0.right - r0.left == 290, "list view: rows below the header, as wide as the columns");
	CRect cell;
	list.GetSubItemRect(1, 1, LVIR_LABEL, cell);
	Check(cell.left == 120 && cell.right == 200 && cell.top == r1.top, "list view: GetSubItemRect");

	// A click selects, with LVN_ITEMCHANGING and LVN_ITEMCHANGED.
	g_lvChanges.clear();
	g_notifies.clear();
	::SetFocus(lv);
	Pump();
	Mouse(lv, QEvent::MouseButtonPress, 30, (r1.top + r1.bottom) / 2);
	Mouse(lv, QEvent::MouseButtonRelease, 30, (r1.top + r1.bottom) / 2);
	Check(list.GetSelectedCount() == 1 && list.GetItemState(1, LVIS_SELECTED | LVIS_FOCUSED) == (LVIS_SELECTED | LVIS_FOCUSED), "list view: a click selects and focuses");
	Check(std::find(g_notifies.begin(), g_notifies.end(), static_cast<UINT>(LVN_ITEMCHANGING)) != g_notifies.end() && std::find(g_notifies.begin(), g_notifies.end(), static_cast<UINT>(NM_CLICK)) != g_notifies.end(), "list view: LVN_ITEMCHANGING and NM_CLICK");
	const bool selectedNotified = std::any_of(g_lvChanges.begin(), g_lvChanges.end(), [](const NMLISTVIEW &nm) { return nm.iItem == 1 && (nm.uNewState & LVIS_SELECTED) && !(nm.uOldState & LVIS_SELECTED); });
	Check(selectedNotified, "list view: LVN_ITEMCHANGED for the new selection");
	Check(list.GetSelectionMark() == 1, "list view: the click sets the selection mark");
	POSITION pos = list.GetFirstSelectedItemPosition();
	Check(pos != nullptr && list.GetNextSelectedItem(pos) == 1 && pos == nullptr, "list view: GetFirstSelectedItemPosition walk");

	// Keys move the focus and the selection.
	Key(Qt::Key_Down);
	Check(list.GetItemState(2, LVIS_SELECTED) && !list.GetItemState(1, LVIS_SELECTED), "list view: Down moves the selection");
	Key(Qt::Key_End);
	Check(list.GetItemState(3, LVIS_SELECTED | LVIS_FOCUSED) == (LVIS_SELECTED | LVIS_FOCUSED), "list view: End");
	Key(Qt::Key_Home);
	Check(list.GetItemState(0, LVIS_SELECTED) && list.GetSelectedCount() == 1, "list view: Home");

	// Multiple selection, and LVM_GETNEXTITEM.
	list.SetItemState(-1, LVIS_SELECTED, LVIS_SELECTED);
	Check(list.GetSelectedCount() == 4, "list view: item -1 sets the state of all");
	Check(list.GetNextItem(1, LVNI_SELECTED) == 2 && list.GetNextItem(3, LVNI_SELECTED) == -1, "list view: LVM_GETNEXTITEM");
	list.SetItemState(-1, 0, LVIS_SELECTED);
	Check(list.GetSelectedCount() == 0, "list view: clear the selection");

	// Hit testing.
	LVHITTESTINFO hit{};
	hit.pt = POINT{150, (r1.top + r1.bottom) / 2};
	Check(list.SubItemHitTest(&hit) == 1 && hit.iItem == 1 && hit.iSubItem == 1, "list view: LVM_SUBITEMHITTEST");
	UINT flags = 0;
	Check(list.HitTest(CPoint(30, 190), &flags) == -1 && (flags & LVHT_NOWHERE), "list view: LVM_HITTEST below the items");

	// Sorting by lParam.
	list.SortItems(CompareText, 0);
	Check(list.GetItemText(0, 0) == CString(L"snare.wav") && list.GetItemText(3, 0) == CString(L"kick.wav"), "list view: LVM_SORTITEMS");
	list.SortItems(CompareText, 1);
	Check(list.GetItemText(0, 0) == CString(L"kick.wav"), "list view: LVM_SORTITEMS descending");

	// Check boxes.
	list.SetExtendedStyle(list.GetExtendedStyle() | LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
	Check(!list.GetCheck(0), "list view: LVS_EX_CHECKBOXES starts unchecked");
	list.SetCheck(2, TRUE);
	Check(list.GetCheck(2) && !list.GetCheck(1), "list view: SetCheck");
	::SetFocus(lv);
	list.SetItemState(1, LVIS_FOCUSED | LVIS_SELECTED, LVIS_FOCUSED | LVIS_SELECTED);
	Key(Qt::Key_Space);
	Check(list.GetCheck(1), "list view: Space toggles the check box");
	SaveShot(parent, "listview_report");

	// Label editing: the edit takes the text, Enter ends it, and the parent accepts.
	CEdit *edit = list.EditLabel(0);
	Pump();
	Check(edit != nullptr && list.GetEditControl() != nullptr, "list view: EditLabel opens an edit");
	if(edit != nullptr)
	{
		::SetWindowText(edit->m_hWnd, L"crash.wav");
		::SetFocus(edit->m_hWnd);
		Pump();
		Key(Qt::Key_Return);
	}
	Check(g_editedText == L"crash.wav" && list.GetItemText(0, 0) == CString(L"crash.wav"), "list view: LVN_ENDLABELEDIT, accepted");
	Check(list.GetEditControl() == nullptr, "list view: the edit goes away");
	g_editAccept = false;
	edit = list.EditLabel(0);
	Pump();
	if(edit != nullptr)
	{
		::SetWindowText(edit->m_hWnd, L"ride.wav");
		::SetFocus(edit->m_hWnd);
		Pump();
		Key(Qt::Key_Return);
	}
	g_editAccept = true;
	Check(list.GetItemText(0, 0) == CString(L"crash.wav"), "list view: a FALSE answer keeps the old text");

	// Delete.
	list.DeleteItem(0);
	Check(list.GetItemCount() == 3, "list view: DeleteItem");
	list.DeleteAllItems();
	Check(list.GetItemCount() == 0, "list view: DeleteAllItems");

	// Groups.
	ListView_EnableGroupView(lv, TRUE);
	Check(ListView_IsGroupViewEnabled(lv), "list view: group view on");
	LVGROUP group{};
	group.cbSize = sizeof(group);
	group.mask = LVGF_HEADER | LVGF_GROUPID;
	group.pszHeader = const_cast<LPWSTR>(L"Drums");
	group.iGroupId = 1;
	ListView_InsertGroup(lv, -1, &group);
	group.pszHeader = const_cast<LPWSTR>(L"Synths");
	group.iGroupId = 2;
	ListView_InsertGroup(lv, -1, &group);
	for(int i = 0; i < 4; i++)
	{
		LVITEM item{};
		item.mask = LVIF_TEXT | LVIF_GROUPID;
		item.iItem = i;
		item.pszText = const_cast<LPWSTR>(names[i]);
		item.iGroupId = i < 2 ? 2 : 1;
		ListView_InsertItem(lv, &item);
	}
	RECT g0{}, g3{};
	list.GetItemRect(0, &g0, LVIR_BOUNDS);
	list.GetItemRect(3, &g3, LVIR_BOUNDS);
	Check(g3.top < g0.top, "list view: groups in group order (Drums before Synths)");
	SaveShot(parent, "listview_groups");
	ListView_RemoveAllGroups(lv);
	ListView_EnableGroupView(lv, FALSE);
	list.Detach();
	::DestroyWindow(lv);

	// Owner data: the parent supplies the text.
	HWND od = ::CreateWindowEx(0, WC_LISTVIEW, nullptr, WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_OWNERDATA, 10, 10, 360, 200, parent, reinterpret_cast<HMENU>(501), nullptr, nullptr);
	LVCOLUMN rowColumn{};
	rowColumn.mask = LVCF_TEXT | LVCF_WIDTH;
	rowColumn.cx = 150;
	rowColumn.pszText = const_cast<LPWSTR>(L"Row");
	ListView_InsertColumn(od, 0, &rowColumn);
	ListView_SetItemCountEx(od, 100000, 0);
	Check(ListView_GetItemCount(od) == 100000, "owner data: LVM_SETITEMCOUNT");
	wchar_t text[64] = {};
	ListView_GetItemText(od, 76543, 0, text, 64);
	Check(std::wstring(text) == L"row 76543.0", "owner data: LVN_GETDISPINFO supplies the text");
	ListView_EnsureVisible(od, 99999, FALSE);
	Check(ListView_GetTopIndex(od) + ListView_GetCountPerPage(od) >= 99999, "owner data: LVM_ENSUREVISIBLE scrolls to the end");
	SaveShot(parent, "listview_ownerdata");
	::DestroyWindow(od);

	// CMFCListCtrl: per-cell colours and sorting by a column click.
	SortedList sorted;
	sorted.Create(WS_CHILD | WS_VISIBLE | LVS_REPORT, CRect(10, 10, 370, 210), CWnd::FromHandle(parent), 502);
	sorted.InsertColumn(0, L"Name", LVCFMT_LEFT, 150);
	for(int i = 0; i < 4; i++)
		sorted.InsertItem(LVIF_TEXT | LVIF_PARAM, i, names[i], 0, 0, 0, sizes[i]);
	g_drawStages.clear();
	::UpdateWindow(sorted.m_hWnd);
	Pump();
	HWND sortedHeader = ListView_GetHeader(sorted.m_hWnd);
	RECT col{};
	Header_GetItemRect(sortedHeader, 0, &col);
	Mouse(sortedHeader, QEvent::MouseButtonPress, 40, (col.top + col.bottom) / 2);
	Mouse(sortedHeader, QEvent::MouseButtonRelease, 40, (col.top + col.bottom) / 2);
	Check(sorted.GetSortColumn() == 0 && sorted.IsAscending() && sorted.GetItemText(0, 0) == CString(L"snare.wav"), "CMFCListCtrl: a header click sorts ascending");
	HDITEM hdi{};
	hdi.mask = HDI_FORMAT;
	Header_GetItem(sortedHeader, 0, &hdi);
	Check(hdi.fmt & HDF_SORTUP, "CMFCListCtrl: the header shows the sort arrow");
	Mouse(sortedHeader, QEvent::MouseButtonPress, 40, (col.top + col.bottom) / 2);
	Mouse(sortedHeader, QEvent::MouseButtonRelease, 40, (col.top + col.bottom) / 2);
	Check(!sorted.IsAscending() && sorted.GetItemText(0, 0) == CString(L"kick.wav"), "CMFCListCtrl: a second click sorts descending");
	SaveShot(parent, "listview_mfc");
	sorted.DestroyWindow();
}

size_t CountEvents(UINT code)
{
	return static_cast<size_t>(std::count_if(g_tvEvents.begin(), g_tvEvents.end(), [code](const NMTREEVIEW &nm) { return nm.hdr.code == code; }));
}

void TreeViews(HWND parent)
{
	HWND tv = ::CreateWindowEx(WS_EX_CLIENTEDGE, WC_TREEVIEW, nullptr, WS_CHILD | WS_VISIBLE | WS_TABSTOP | TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS | TVS_SHOWSELALWAYS | TVS_EDITLABELS, 10, 10, 360, 200, parent, reinterpret_cast<HMENU>(600), nullptr, nullptr);
	Check(tv != nullptr, "tree view: created");
	g_tree = tv;
	CTreeCtrl tree;
	tree.Attach(tv);
	HTREEITEM songs = tree.InsertItem(L"Songs");
	HTREEITEM song = tree.InsertItem(TVIF_TEXT | TVIF_PARAM | TVIF_STATE, L"demo.it", 0, 0, TVIS_BOLD, TVIS_BOLD, 7, songs, TVI_LAST);
	HTREEITEM patterns = tree.InsertItem(L"Patterns", song);
	HTREEITEM samples = tree.InsertItem(L"Samples", song);
	for(const wchar_t *name : {L"Pattern 2", L"Pattern 0", L"Pattern 1"})
		tree.InsertItem(name, patterns, TVI_SORT);
	HTREEITEM library = tree.InsertItem(L"Instrument Library");
	Check(tree.GetCount() == 8, "tree view: TVM_GETCOUNT");
	Check(tree.GetRootItem() == songs && tree.GetNextSiblingItem(songs) == library && tree.GetPrevSiblingItem(library) == songs, "tree view: siblings");
	Check(tree.GetParentItem(song) == songs && tree.GetParentItem(songs) == nullptr && tree.GetChildItem(song) == patterns, "tree view: parent and child");
	HTREEITEM p0 = tree.GetChildItem(patterns);
	Check(tree.GetItemText(p0) == CString(L"Pattern 0") && tree.GetItemText(tree.GetNextSiblingItem(p0)) == CString(L"Pattern 1"), "tree view: TVI_SORT inserts in order");
	Check(tree.GetItemData(song) == 7 && (tree.GetItemState(song, TVIS_BOLD) & TVIS_BOLD), "tree view: data and state");
	Check(tree.ItemHasChildren(song) && !tree.ItemHasChildren(samples), "tree view: ItemHasChildren");

	// Only the top level shows until something expands.
	Check(tree.GetFirstVisibleItem() == songs && tree.GetNextVisibleItem(songs) == library, "tree view: collapsed items hide their children");
	RECT r{};
	Check(!tree.GetItemRect(song, &r, FALSE), "tree view: no rectangle for a hidden item");

	// TVM_EXPAND notifies the first time only.
	g_tvEvents.clear();
	tree.Expand(songs, TVE_EXPAND);
	Check(CountEvents(TVN_ITEMEXPANDED) == 1 && (tree.GetItemState(songs, TVIS_EXPANDED | TVIS_EXPANDEDONCE) == (TVIS_EXPANDED | TVIS_EXPANDEDONCE)), "tree view: TVM_EXPAND, notified, once");
	tree.Expand(songs, TVE_COLLAPSE);
	tree.Expand(songs, TVE_EXPAND);
	Check(CountEvents(TVN_ITEMEXPANDED) == 1, "tree view: TVM_EXPAND later is quiet");
	Check(tree.GetNextVisibleItem(songs) == song, "tree view: expanded children show");
	g_tvVeto = 1;
	Check(!tree.Expand(song, TVE_EXPAND) && !(tree.GetItemState(song, TVIS_EXPANDED) & TVIS_EXPANDED), "tree view: TVN_ITEMEXPANDING can refuse");
	g_tvVeto = 0;

	// EnsureVisible expands the parents.
	tree.EnsureVisible(p0);
	Check((tree.GetItemState(patterns, TVIS_EXPANDED) & TVIS_EXPANDED) && tree.GetItemRect(p0, &r, TRUE), "tree view: TVM_ENSUREVISIBLE expands the parents");
	RECT rs{}, rp{};
	tree.GetItemRect(song, &rs, TRUE);
	tree.GetItemRect(patterns, &rp, TRUE);
	Check(rp.left - rs.left == static_cast<LONG>(tree.GetIndent()) && rp.top == rs.bottom, "tree view: a child is one indent in, one row down");

	// Selection: TVN_SELCHANGING can refuse; TVN_SELCHANGED says old and new.
	g_tvEvents.clear();
	Check(tree.SelectItem(song) && tree.GetSelectedItem() == song, "tree view: SelectItem");
	Check(CountEvents(TVN_SELCHANGED) == 1 && g_tvEvents.back().itemNew.hItem == song && g_tvEvents.back().itemOld.hItem == nullptr && g_tvEvents.back().action == TVC_UNKNOWN, "tree view: TVN_SELCHANGED");
	Check(tree.GetItemState(song, TVIS_SELECTED) & TVIS_SELECTED, "tree view: TVIS_SELECTED");
	g_tvVeto = 1;
	Check(!tree.SelectItem(samples) && tree.GetSelectedItem() == song, "tree view: TVN_SELCHANGING can refuse");
	g_tvVeto = 0;

	// A click: NM_CLICK comes before the selection changes.
	::SetFocus(tv);
	Pump();
	g_selectedAtClick.clear();
	g_tvEvents.clear();
	tree.GetItemRect(samples, &r, TRUE);
	Mouse(tv, QEvent::MouseButtonPress, r.left + 4, (r.top + r.bottom) / 2);
	Mouse(tv, QEvent::MouseButtonRelease, r.left + 4, (r.top + r.bottom) / 2);
	Check(tree.GetSelectedItem() == samples, "tree view: a click selects");
	Check(g_selectedAtClick.size() == 1 && g_selectedAtClick[0] == song, "tree view: NM_CLICK before the selection moves");
	Check(!g_tvEvents.empty() && g_tvEvents.back().action == TVC_BYMOUSE, "tree view: TVC_BYMOUSE");

	// A click on the button toggles, with notifications.
	g_tvEvents.clear();
	tree.GetItemRect(patterns, &r, FALSE);
	RECT label{};
	tree.GetItemRect(patterns, &label, TRUE);
	const int buttonX = label.left - static_cast<int>(tree.GetIndent()) / 2;
	Mouse(tv, QEvent::MouseButtonPress, buttonX, (r.top + r.bottom) / 2);
	Mouse(tv, QEvent::MouseButtonRelease, buttonX, (r.top + r.bottom) / 2);
	Check(!(tree.GetItemState(patterns, TVIS_EXPANDED) & TVIS_EXPANDED) && CountEvents(TVN_ITEMEXPANDED) == 1, "tree view: the button collapses");
	TVHITTESTINFO hit{};
	hit.pt = POINT{buttonX, (r.top + r.bottom) / 2};
	Check(tree.HitTest(&hit) == patterns && (hit.flags & TVHT_ONITEMBUTTON), "tree view: TVHT_ONITEMBUTTON");
	hit.pt = POINT{label.left + 2, (r.top + r.bottom) / 2};
	Check(tree.HitTest(&hit) == patterns && (hit.flags & TVHT_ONITEMLABEL), "tree view: TVHT_ONITEMLABEL");

	// Keys.
	tree.SelectItem(song);
	::SetFocus(tv);
	Pump();
	Key(Qt::Key_Down);
	Check(tree.GetSelectedItem() == patterns, "tree view: Down");
	Key(Qt::Key_Right);
	Check(tree.GetItemState(patterns, TVIS_EXPANDED) & TVIS_EXPANDED, "tree view: Right expands");
	Key(Qt::Key_Right);
	Check(tree.GetSelectedItem() == p0, "tree view: Right again goes to the first child");
	Key(Qt::Key_Left);
	Check(tree.GetSelectedItem() == patterns, "tree view: Left goes to the parent");
	Key(Qt::Key_Left);
	Check(!(tree.GetItemState(patterns, TVIS_EXPANDED) & TVIS_EXPANDED), "tree view: Left collapses");
	Key(Qt::Key_End);
	Check(tree.GetSelectedItem() == library, "tree view: End");
	Key(Qt::Key_Home);
	Check(tree.GetSelectedItem() == songs, "tree view: Home");
	Type('i');
	Check(tree.GetSelectedItem() == library, "tree view: typing searches");

	// A selection inside a branch that collapses moves up to it.
	tree.SelectItem(p0);
	tree.Expand(song, TVE_COLLAPSE);
	Check(tree.GetSelectedItem() == song, "tree view: collapsing takes the selection up");

	// Callbacks.
	HTREEITEM cb = tree.InsertItem(TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN, LPSTR_TEXTCALLBACK, 0, 0, 0, 0, 42, library, TVI_LAST);
	TVITEM item{};
	item.mask = TVIF_CHILDREN | TVIF_HANDLE;
	item.hItem = cb;
	item.cChildren = I_CHILDRENCALLBACK;
	tree.SetItem(&item);
	Check(tree.GetItemText(cb) == CString(L"callback 42"), "tree view: LPSTR_TEXTCALLBACK asks TVN_GETDISPINFO");
	tree.Expand(library, TVE_EXPAND);
	SaveShot(parent, "treeview");

	// Label editing.
	tree.SelectItem(samples);
	CEdit *edit = tree.EditLabel(samples);
	Pump();
	Check(edit != nullptr && tree.GetEditControl() != nullptr, "tree view: EditLabel opens an edit");
	if(edit != nullptr)
	{
		::SetWindowText(edit->m_hWnd, L"Sounds");
		::SetFocus(edit->m_hWnd);
		Pump();
		Key(Qt::Key_Return);
	}
	Check(g_editedText == L"Sounds" && tree.GetItemText(samples) == CString(L"Sounds"), "tree view: TVN_ENDLABELEDIT, accepted");

	// Dragging.
	g_tvEvents.clear();
	tree.EnsureVisible(songs);
	tree.GetItemRect(songs, &r, TRUE);
	const int y = (r.top + r.bottom) / 2;
	Mouse(tv, QEvent::MouseButtonPress, r.left + 4, y);
	Mouse(tv, QEvent::MouseMove, r.left + 30, y + 30);
	Mouse(tv, QEvent::MouseButtonRelease, r.left + 30, y + 30);
	Check(CountEvents(TVN_BEGINDRAG) == 1 && g_tvEvents.back().itemNew.hItem == songs, "tree view: TVN_BEGINDRAG");

	// Sorting with a callback.
	tree.SetItemData(song, 2);
	HTREEITEM other = tree.InsertItem(TVIF_TEXT | TVIF_PARAM, L"another.xm", 0, 0, 0, 0, 1, songs, TVI_LAST);
	TVSORTCB sort{songs, CompareText, 0};
	TreeView_SortChildrenCB(tv, &sort, FALSE);
	Check(tree.GetChildItem(songs) == other, "tree view: TVM_SORTCHILDRENCB");

	// Deleting: children first, each with TVN_DELETEITEM; the selection moves.
	tree.SelectItem(song);
	g_tvEvents.clear();
	tree.DeleteItem(song);
	const auto lastDelete = std::find_if(g_tvEvents.rbegin(), g_tvEvents.rend(), [](const NMTREEVIEW &nm) { return nm.hdr.code == TVN_DELETEITEM; });
	Check(CountEvents(TVN_DELETEITEM) == 6 && lastDelete != g_tvEvents.rend() && lastDelete->itemOld.hItem == song, "tree view: TVN_DELETEITEM for each, the item last");
	Check(tree.GetSelectedItem() == songs, "tree view: the selection moves to the parent when no sibling follows");
	Check(tree.GetCount() == 4, "tree view: the subtree is gone");
	tree.DeleteAllItems();
	Check(tree.GetCount() == 0 && tree.GetSelectedItem() == nullptr, "tree view: DeleteAllItems");
	tree.Detach();
	g_tree = nullptr;
	::DestroyWindow(tv);
}

} // namespace

int main(int argc, char **argv)
{
	QApplication app(argc, argv);
	WNDCLASS wc{};
	wc.lpfnWndProc = ParentProc;
	wc.lpszClassName = L"CommCtrlTestParent";
	wc.hbrBackground = ::GetSysColorBrush(COLOR_BTNFACE);
	::RegisterClass(&wc);
	::InitCommonControls();
	HWND parent = ::CreateWindowEx(0, L"CommCtrlTestParent", L"common controls", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 400, 300, nullptr, nullptr, nullptr, nullptr);
	if(Win32Widget *w = FromHandle(parent))
		w->activateWindow();
	Pump();
	Trackbars(parent);
	UpDowns(parent);
	Progress(parent);
	ImageLists();
	Tabs(parent);
	ListViews(parent);
	TreeViews(parent);
	::DestroyWindow(parent);
	std::printf("passed: %d failure(s)\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}
