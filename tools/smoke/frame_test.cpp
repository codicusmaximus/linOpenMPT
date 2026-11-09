// Frames, run offscreen: the multiple-document interface of user32 (the
// MDI client, DefFrameProc, DefMDIChildProc, child windows that move, size,
// maximize and minimize inside it), then MFC's frames and document/view on
// top of it.
#include <afxwin.h>

#include "MptrackResources.h"
#include "QtWin.h"
#include "resource.h"

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

// ---- MFC: frames, documents, views, splitters ------------------------------------

enum : UINT
{
	ID_TEST_APP = 0x8101,
	ID_TEST_FRAME,
	ID_TEST_CHILD,
	ID_TEST_DOC,
	ID_TEST_VIEW,
	ID_TEST_NONE,
};

struct Counts
{
	int app = 0, frame = 0, child = 0, doc = 0, view = 0;
	int docsDeleted = 0, viewsDeleted = 0, framesDeleted = 0;
	int updates = 0, initialUpdates = 0, paints = 0;
} g_counts;

class CTestApp : public CWinApp
{
public:
	afx_msg void OnTestApp() { g_counts.app++; }
	DECLARE_MESSAGE_MAP()
};
BEGIN_MESSAGE_MAP(CTestApp, CWinApp)
	ON_COMMAND(ID_TEST_APP, &CTestApp::OnTestApp)
END_MESSAGE_MAP()

class CTestMainFrame : public CMDIFrameWnd
{
public:
	~CTestMainFrame() override { g_counts.framesDeleted++; }
	afx_msg void OnTestFrame() { g_counts.frame++; }
	DECLARE_MESSAGE_MAP()
};
BEGIN_MESSAGE_MAP(CTestMainFrame, CMDIFrameWnd)
	ON_COMMAND(ID_TEST_FRAME, &CTestMainFrame::OnTestFrame)
END_MESSAGE_MAP()

class CTestDoc : public CDocument
{
	DECLARE_DYNCREATE(CTestDoc)
public:
	~CTestDoc() override { g_counts.docsDeleted++; }
	afx_msg void OnTestDoc() { g_counts.doc++; }
	afx_msg void OnUpdateTestDoc(CCmdUI *ui) { ui->SetCheck(1); }
	DECLARE_MESSAGE_MAP()
};
IMPLEMENT_DYNCREATE(CTestDoc, CDocument)
BEGIN_MESSAGE_MAP(CTestDoc, CDocument)
	ON_COMMAND(ID_TEST_DOC, &CTestDoc::OnTestDoc)
	ON_UPDATE_COMMAND_UI(ID_TEST_DOC, &CTestDoc::OnUpdateTestDoc)
END_MESSAGE_MAP()

class CTestView : public CView
{
	DECLARE_DYNCREATE(CTestView)
public:
	~CTestView() override { g_counts.viewsDeleted++; }
	void OnDraw(CDC *dc) override
	{
		g_counts.paints++;
		CRect rc;
		GetClientRect(&rc);
		dc->FillSolidRect(&rc, RGB(0xE8, 0xF0, 0xFF));
		dc->TextOut(8, 8, GetDocument() != nullptr ? GetDocument()->GetTitle() : CString(_T("?")));
	}
	void OnInitialUpdate() override { g_counts.initialUpdates++; CView::OnInitialUpdate(); }
	void OnUpdate(CView *sender, LPARAM hint, CObject *obj) override { g_counts.updates++; CView::OnUpdate(sender, hint, obj); }
	afx_msg void OnTestView() { g_counts.view++; }
	DECLARE_MESSAGE_MAP()
};
IMPLEMENT_DYNCREATE(CTestView, CView)
BEGIN_MESSAGE_MAP(CTestView, CView)
	ON_COMMAND(ID_TEST_VIEW, &CTestView::OnTestView)
END_MESSAGE_MAP()

class CTestScrollView : public CScrollView
{
	DECLARE_DYNCREATE(CTestScrollView)
public:
	~CTestScrollView() override { g_counts.viewsDeleted++; }
	CPoint lastOrigin{-1, -1};
	void OnInitialUpdate() override
	{
		SetScrollSizes(MM_TEXT, CSize(1000, 2000), CSize(100, 200), CSize(10, 20));
		CScrollView::OnInitialUpdate();
	}
	void OnDraw(CDC *dc) override
	{
		lastOrigin = dc->GetViewportOrg();
		dc->FillSolidRect(0, 0, 1000, 2000, RGB(0xFF, 0xFF, 0xF0));
		for(int y = 0; y < 2000; y += 100)
			dc->FillSolidRect(0, y, 1000, 1, RGB(0xC0, 0xC0, 0xC0));
		dc->TextOut(8, 8, _T("scroll view"));
	}
};
IMPLEMENT_DYNCREATE(CTestScrollView, CScrollView)

class CTestForm : public CFormView
{
	DECLARE_DYNCREATE(CTestForm)
public:
	CTestForm() : CFormView(IDD_VIEW_GLOBALS) { }
	~CTestForm() override { g_counts.viewsDeleted++; }
	bool initialized = false;
	void OnInitialUpdate() override { initialized = true; CFormView::OnInitialUpdate(); }
};
IMPLEMENT_DYNCREATE(CTestForm, CFormView)

class CTestChild : public CMDIChildWnd
{
	DECLARE_DYNCREATE(CTestChild)
public:
	~CTestChild() override { g_counts.framesDeleted++; }
	CSplitterWnd m_wndSplitter;
	BOOL OnCreateClient(LPCREATESTRUCT, CCreateContext *context) override
	{
		// As upstream's CChildFrame: two rows, the document's view on top.
		if(!m_wndSplitter.CreateStatic(this, 2, 1))
			return FALSE;
		if(!m_wndSplitter.CreateView(0, 0, context->m_pNewViewClass, CSize(0, 120), context))
			return FALSE;
		return m_wndSplitter.CreateView(1, 0, RUNTIME_CLASS(CTestScrollView), CSize(0, 0), context);
	}
	afx_msg void OnTestChild() { g_counts.child++; }
	DECLARE_MESSAGE_MAP()
};
IMPLEMENT_DYNCREATE(CTestChild, CMDIChildWnd)
BEGIN_MESSAGE_MAP(CTestChild, CMDIChildWnd)
	ON_COMMAND(ID_TEST_CHILD, &CTestChild::OnTestChild)
END_MESSAGE_MAP()

CRect PaneRect(CWnd *pane)
{
	CRect rc;
	pane->GetWindowRect(&rc);
	pane->GetParent()->ScreenToClient(&rc);
	return rc;
}

void MfcFrames()
{
	CTestApp app;
	auto *tmpl = new CMultiDocTemplate(IDR_MODULETYPE, RUNTIME_CLASS(CTestDoc), RUNTIME_CLASS(CTestChild), RUNTIME_CLASS(CTestView));
	app.AddDocTemplate(tmpl);
	CString docName;
	Check(tmpl->GetDocString(docName, CDocTemplate::docName) && docName == _T("Module"), "CDocTemplate: the document strings of the resource id");

	auto *frame = new CTestMainFrame;
	if(!frame->LoadFrame(IDR_MAINFRAME))
	{
		Check(false, "CMDIFrameWnd::LoadFrame(IDR_MAINFRAME)");
		return;
	}
	app.m_pMainWnd = frame;
	frame->SetWindowPos(nullptr, 0, 0, 900, 650, SWP_NOZORDER);
	frame->ShowWindow(SW_SHOW);
	Pump();
	Check(Text(frame->m_hWnd) == L"OpenMPT", "the frame's title from the resource string");
	Check(::GetMenu(frame->m_hWnd) != nullptr && ::GetMenu(frame->m_hWnd) == frame->m_hMenuDefault, "the frame's menu from the resource");
	Check(frame->m_hWndMDIClient != nullptr && ::GetDlgCtrlID(frame->m_hWndMDIClient) == AFX_IDW_PANE_FIRST
		&& (::GetWindowLongPtr(frame->m_hWndMDIClient, GWL_EXSTYLE) & WS_EX_CLIENTEDGE), "the MDI client: the frame's pane, sunken");

	// A new document: its frame, its views, its title.
	app.OnFileNew();
	Pump();
	Check(app.GetOpenDocumentCount() == 1, "OnFileNew: one document");
	CMDIChildWnd *child = frame->MDIGetActive();
	auto *testChild = dynamic_cast<CTestChild *>(child);
	Check(testChild != nullptr && child->IsWindowVisible(), "... in a child frame of the template's class, shown");
	CDocument *doc = child != nullptr ? child->GetActiveDocument() : nullptr;
	Check(doc != nullptr && doc->GetTitle() == _T("Module1"), "... titled from the template (Module1)");
	Check(Text(child->m_hWnd) == L"Module1" && Text(frame->m_hWnd) == L"Module1 - OpenMPT", "the frames' titles (FWS_PREFIXTITLE, as MFC sets it)");
	int views = 0;
	for(POSITION pos = doc ? doc->GetFirstViewPosition() : nullptr; pos != nullptr; doc->GetNextView(pos))
		views++;
	Check(views == 2, "the document has both splitter panes as views");
	if(testChild == nullptr || doc == nullptr)
		return;
	CSplitterWnd &splitter = testChild->m_wndSplitter;
	auto *topView = dynamic_cast<CTestView *>(splitter.GetPane(0, 0));
	auto *scrollView = dynamic_cast<CTestScrollView *>(splitter.GetPane(1, 0));
	Check(topView != nullptr && scrollView != nullptr, "CSplitterWnd: the panes are the views");
	Check(child->GetActiveView() == topView, "InitialUpdateFrame: the first pane is the active view");
	Check(g_counts.initialUpdates == 1 && ::GetFocus() == topView->m_hWnd, "... with its initial update, and the focus");
	if(topView == nullptr || scrollView == nullptr)
		return;

	// The splitter's layout.
	CRect client;
	splitter.GetClientRect(&client);
	const CRect top = PaneRect(topView), bottom = PaneRect(scrollView);
	Check(top.left == 2 && top.top == 2 && top.Height() == 120 && top.right == client.right - 2, "CSplitterWnd: the top pane, its ideal height inside the border");
	Check(bottom.top == top.bottom + splitter.m_cySplitterGap && bottom.bottom == client.bottom - 2, "... the bottom pane takes the rest");
	{
		CRect childClient;
		child->GetClientRect(&childClient);
		Check(PaneRect(&splitter) == childClient, "the splitter fills the child frame");
	}
	// Dragging the bar.
	{
		const QPoint bar(50, top.bottom + 3);
		::SendMessage(splitter.m_hWnd, WM_SETCURSOR, reinterpret_cast<WPARAM>(splitter.m_hWnd), MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
		Mouse(splitter.m_hWnd, QEvent::MouseButtonPress, bar);
		Mouse(splitter.m_hWnd, QEvent::MouseMove, bar + QPoint(0, 40));
		Mouse(splitter.m_hWnd, QEvent::MouseButtonRelease, bar + QPoint(0, 40));
		int cur = 0, min = 0;
		splitter.GetRowInfo(0, cur, min);
		Check(cur == 160 && PaneRect(topView).Height() == 160 && PaneRect(scrollView).top == 162 + splitter.m_cySplitterGap, "dragging the splitter bar sizes the panes");
		splitter.SetRowInfo(0, 100, 15);
		splitter.RecalcLayout();
		Check(PaneRect(topView).Height() == 100, "SetRowInfo and RecalcLayout");
	}

	// CScrollView.
	{
		const DWORD style = scrollView->GetStyle();
		Check((style & WS_VSCROLL) && (style & WS_HSCROLL), "CScrollView: scroll bars for a size larger than the view");
		SCROLLINFO si{sizeof(si), SIF_ALL};
		::GetScrollInfo(scrollView->m_hWnd, SB_VERT, &si);
		CRect rc;
		scrollView->GetClientRect(&rc);
		Check(si.nMax == 1999 && static_cast<int>(si.nPage) == rc.Height(), "... the range is the size, the page the client area");
		scrollView->SendMessage(WM_VSCROLL, SB_PAGEDOWN, 0);
		Check(scrollView->GetScrollPosition() == CPoint(0, 200), "SB_PAGEDOWN scrolls by the page size given");
		scrollView->SendMessage(WM_VSCROLL, SB_LINEDOWN, 0);
		Check(scrollView->GetScrollPosition().y == 220, "SB_LINEDOWN by the line size");
		scrollView->SendMessage(WM_VSCROLL, SB_BOTTOM, 0);
		Check(scrollView->GetScrollPosition().y == 2000 - rc.Height(), "SB_BOTTOM: to the end");
		scrollView->ScrollToPosition(CPoint(30, 400));
		scrollView->Invalidate();
		scrollView->UpdateWindow();
		Check(scrollView->GetScrollPosition() == CPoint(30, 400) && scrollView->lastOrigin == CPoint(-30, -400), "OnPrepareDC: the viewport follows the scroll position");
		scrollView->SetScrollSizes(MM_TEXT, CSize(10, 10));
		Check(!(scrollView->GetStyle() & (WS_VSCROLL | WS_HSCROLL)) && scrollView->GetScrollPosition() == CPoint(0, 0), "... and the bars go when it fits");
		scrollView->SetScrollSizes(MM_TEXT, CSize(1000, 2000), CSize(100, 200), CSize(10, 20));
	}

	// Command routing: active view, its document, the child frame, the main
	// frame, the application.
	for(UINT id : {ID_TEST_VIEW, ID_TEST_DOC, ID_TEST_CHILD, ID_TEST_FRAME, ID_TEST_APP})
		frame->SendMessage(WM_COMMAND, id, 0);
	Check(g_counts.view == 1 && g_counts.doc == 1 && g_counts.child == 1 && g_counts.frame == 1 && g_counts.app == 1, "WM_COMMAND to the main frame: view, document, child frame, main frame, application");
	// Menu items through CCmdUI.
	{
		HMENU popup = ::CreatePopupMenu();
		::AppendMenu(popup, MF_STRING, ID_TEST_DOC, L"Doc");
		::AppendMenu(popup, MF_STRING, ID_TEST_NONE, L"None");
		::AppendMenu(popup, MF_STRING, ID_TEST_FRAME, L"Frame");
		frame->SendMessage(WM_INITMENUPOPUP, reinterpret_cast<WPARAM>(popup), 0);
		Check((::GetMenuState(popup, ID_TEST_DOC, MF_BYCOMMAND) & MF_CHECKED) != 0, "WM_INITMENUPOPUP: the document's update handler checks its item");
		Check((::GetMenuState(popup, ID_TEST_NONE, MF_BYCOMMAND) & MF_GRAYED) != 0, "... an item nobody handles is disabled");
		Check((::GetMenuState(popup, ID_TEST_FRAME, MF_BYCOMMAND) & MF_GRAYED) == 0, "... an item with a handler stays enabled");
		::DestroyMenu(popup);
	}

	// UpdateAllViews: the other views.
	g_counts.updates = 0;
	doc->UpdateAllViews(scrollView);
	Check(g_counts.updates == 1, "UpdateAllViews: every view but the sender");

	// A form view in the bottom pane, as upstream's CChildFrame changes it.
	{
		splitter.DeleteView(1, 0);
		Check(splitter.GetPane(1, 0) == nullptr && g_counts.viewsDeleted == 1, "CSplitterWnd::DeleteView: the view goes");
		CCreateContext context;
		context.m_pCurrentDoc = doc;
		context.m_pNewViewClass = RUNTIME_CLASS(CTestForm);
		Check(splitter.CreateView(1, 0, RUNTIME_CLASS(CTestForm), CSize(0, 0), &context) != FALSE, "CreateView: a CFormView from a dialog template");
		splitter.RecalcLayout();
		auto *form = dynamic_cast<CTestForm *>(splitter.GetPane(1, 0));
		Check(form != nullptr && form->GetDocument() == doc && ::GetWindow(form->m_hWnd, GW_CHILD) != nullptr, "... the form, its controls, and its document");
		if(form != nullptr)
		{
			form->SendMessage(WM_INITIALUPDATE);
			Check(form->initialized && PaneRect(form).top == 102 + splitter.m_cySplitterGap, "... laid out in the pane");
			Check((form->GetStyle() & WS_VSCROLL) != 0, "... with a scroll bar when the pane is smaller than the template");
		}
	}
	child->UpdateWindow();
	SaveShot(frame->m_hWnd, "mfc_mdi_splitter");

	// A second document, in its own frame.
	app.OnFileNew();
	Pump();
	CDocument *doc2 = frame->MDIGetActive() != nullptr ? frame->MDIGetActive()->GetActiveDocument() : nullptr;
	Check(app.GetOpenDocumentCount() == 2 && doc2 != doc && doc2 != nullptr && doc2->GetTitle() == _T("Module2"), "a second document: Module2");
	Check(Text(frame->m_hWnd) == L"Module2 - OpenMPT", "the main frame's title follows the active child");
	frame->MDIActivate(child);
	Check(frame->MDIGetActive() == child && child->GetActiveDocument() == doc && Text(frame->m_hWnd) == L"Module1 - OpenMPT", "activating the first child: its document, its title");
	frame->MDIMaximize(child);
	frame->SendMessage(WM_IDLEUPDATECMDUI); // the title follows at idle time
	Check(Text(frame->m_hWnd) == L"OpenMPT - [Module1]", "a maximized child's title in brackets");
	frame->MDIRestore(child);
	frame->SendMessage(WM_IDLEUPDATECMDUI);
	Check(Text(frame->m_hWnd) == L"Module1 - OpenMPT", "... and back");

	// Command routing reaches the active child's document only.
	g_counts.doc = 0;
	frame->SendMessage(WM_COMMAND, ID_TEST_DOC, 0);
	Check(g_counts.doc == 1, "commands go to the active document");

	// Closing a child frame closes its document (unmodified: no question).
	const int docsBefore = g_counts.docsDeleted, framesBefore = g_counts.framesDeleted;
	const HWND childWnd = child->m_hWnd;
	::SendMessage(childWnd, WM_CLOSE, 0, 0);
	Pump();
	Check(!::IsWindow(childWnd) && g_counts.docsDeleted == docsBefore + 1 && g_counts.framesDeleted == framesBefore + 1, "WM_CLOSE on a child frame: frame, views and document go");
	Check(g_counts.viewsDeleted == 3 && app.GetOpenDocumentCount() == 1, "... every view, and the document leaves the template");

	app.CloseAllDocuments(FALSE);
	Pump();
	Check(app.GetOpenDocumentCount() == 0 && frame->MDIGetActive() == nullptr && Text(frame->m_hWnd) == L"OpenMPT", "CloseAllDocuments: no document, no child, the title back");
	frame->DestroyWindow();
	Check(g_counts.framesDeleted == 3, "the main frame deletes itself (PostNcDestroy)");
	app.m_pMainWnd = nullptr;
	// The quit message the main frame posted.
	MSG msg;
	while(::PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
	{
	}
}

} // namespace

int main(int argc, char **argv)
{
	QApplication app(argc, argv);
	Win32Mdi();
	LinOpenMPTRegisterMptrackResources();
	MfcFrames();
	std::printf("passed: %d failure(s)\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}
