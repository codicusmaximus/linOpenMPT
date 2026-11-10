// Control bars, run offscreen: MFC's toolbar, status bar and dialog bar in a
// frame's layout, their idle command updates and flyby help, docking and
// floating (also by dragging, through the posted mouse messages the drag
// loop reads), the bar state saved to the profile and loaded again, and
// what CWinApp::InitInstance uses: the command line, the shell commands it
// asks for, the profile, files dropped on the main window.
#include <afxwin.h>

#include "MptrackResources.h"
#include "QtWin.h"
#include "resource.h"

#include <QtCore/QFile>
#include <QtCore/QMimeData>
#include <QtCore/QTemporaryDir>
#include <QtCore/QUrl>
#include <QtGui/QDropEvent>
#include <QtWidgets/QApplication>

#include <cstdio>
#include <cwchar>
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

// A window's rectangle in the client coordinates of another.
CRect RectIn(CWnd *wnd, CWnd *in)
{
	CRect rc;
	wnd->GetWindowRect(&rc);
	in->ScreenToClient(&rc);
	return rc;
}

enum : UINT
{
	ID_TEST_DISABLED = 0x8201,
	ID_TEST_CHECKED,
	ID_TEST_PLAIN,
	ID_TEST_NOHANDLER,
	IDW_TEST_DIALOGBAR = 0xE820,
};

std::vector<std::wstring> g_opened;
int g_plainCommands = 0;

class CTestApp : public CWinApp
{
public:
	CDocument *OpenDocumentFile(LPCTSTR path) override
	{
		g_opened.push_back(path);
		return nullptr;
	}
	CDocument *OpenDocumentFile(LPCTSTR path, BOOL) override { return OpenDocumentFile(path); }
};

class CTestFrame : public CFrameWnd
{
public:
	CToolBar m_wndToolBar;
	CStatusBar m_wndStatusBar;
	CDialogBar m_wndDialogBar;
	CWnd m_wndPane;
	afx_msg void OnUpdateDisabled(CCmdUI *ui) { ui->Enable(FALSE); }
	afx_msg void OnUpdateChecked(CCmdUI *ui) { ui->SetCheck(1); }
	afx_msg void OnPlain() { g_plainCommands++; }
	DECLARE_MESSAGE_MAP()
};
BEGIN_MESSAGE_MAP(CTestFrame, CFrameWnd)
	ON_UPDATE_COMMAND_UI(ID_TEST_DISABLED, &CTestFrame::OnUpdateDisabled)
	ON_UPDATE_COMMAND_UI(ID_TEST_CHECKED, &CTestFrame::OnUpdateChecked)
	ON_COMMAND(ID_TEST_CHECKED, &CTestFrame::OnPlain)
	ON_COMMAND(ID_TEST_PLAIN, &CTestFrame::OnPlain)
END_MESSAGE_MAP()

const UINT g_buttons[] = {ID_TEST_DISABLED, ID_TEST_CHECKED, ID_SEPARATOR, ID_TEST_PLAIN, ID_TEST_NOHANDLER, ID_SEPARATOR, ID_VIEW_TOOLBAR};
const UINT g_indicators[] = {ID_SEPARATOR, ID_INDICATOR_CAPS, ID_INDICATOR_NUM, ID_INDICATOR_SCRL};

// What the message loop does when the queue is empty: idle command
// updates, and the layout docking asked for (DelayRecalcLayout).
void Idle()
{
	Pump();
	AfxGetApp()->OnIdle(0);
	Pump();
}

std::wstring PaneControlText(CStatusBar &sb, int index)
{
	wchar_t buffer[256] = {};
	sb.SendMessage(SB_GETTEXTW, static_cast<WPARAM>(index), reinterpret_cast<LPARAM>(buffer));
	return buffer;
}

bool IsDockedAt(CControlBar &bar, UINT dockBarId)
{
	CWnd *parent = bar.GetParent();
	return parent != nullptr && !bar.IsFloating() && static_cast<UINT>(parent->GetDlgCtrlID()) == dockBarId;
}

// A drag from the middle of a bar to a point on the screen, as the drag
// loop sees it: the mouse messages are waiting when it starts.
void DragBar(CControlBar &bar, CPoint to)
{
	CRect rc;
	bar.GetWindowRect(&rc);
	const CPoint from = rc.CenterPoint();
	CPoint mid((from.x + to.x) / 2, (from.y + to.y) / 2);
	CPoint end = to;
	bar.ScreenToClient(&mid);
	bar.ScreenToClient(&end);
	bar.PostMessage(WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(mid.x, mid.y));
	bar.PostMessage(WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(end.x, end.y));
	bar.PostMessage(WM_LBUTTONUP, 0, MAKELPARAM(end.x, end.y));
	bar.m_pDockContext->StartDrag(from);
	Idle();
}

void CommandLine()
{
	// The command line the test was started with (see main).
	CCommandLineInfo info;
	AfxGetApp()->ParseCommandLine(info);
	Check(info.m_nShellCommand == CCommandLineInfo::FileOpen && info.m_strFileName == _T("/tmp/linopenmpt-test/song.it"), "ParseCommandLine: a file to open (an absolute path is no flag)");
	Check(info.m_bShowSplash, "... with the splash screen");

	CCommandLineInfo none;
	none.ParseParam(_T("Embedding"), TRUE, FALSE);
	none.ParseParam(_T("other.it"), FALSE, TRUE);
	Check(none.m_bRunEmbedded && !none.m_bShowSplash && none.m_nShellCommand == CCommandLineInfo::FileOpen, "-Embedding: embedded, no splash, the file still opened");
	CCommandLineInfo print;
	print.ParseParam(_T("pt"), TRUE, FALSE);
	print.ParseParam(_T("a.it"), FALSE, FALSE);
	print.ParseParam(_T("printer"), FALSE, FALSE);
	print.ParseParam(_T("driver"), FALSE, FALSE);
	print.ParseParam(_T("port"), FALSE, TRUE);
	Check(print.m_nShellCommand == CCommandLineInfo::FilePrintTo && print.m_strPrinterName == _T("printer") && print.m_strDriverName == _T("driver") && print.m_strPortName == _T("port"), "-pt: the file, printer, driver and port");
	CCommandLineInfo reg;
	reg.ParseParam(_T("REGSERVER"), TRUE, TRUE);
	Check(reg.m_nShellCommand == CCommandLineInfo::AppRegister, "-RegServer, in any case");
	CCommandLineInfo upper;
	upper.ParseParam(_T("P"), TRUE, TRUE);
	Check(upper.m_nShellCommand == CCommandLineInfo::FileNew, "... but -P is not -p: shell commands are case sensitive");

	g_opened.clear();
	Check(AfxGetApp()->ProcessShellCommand(info) == FALSE && g_opened.size() == 1 && g_opened[0] == L"/tmp/linopenmpt-test/song.it", "ProcessShellCommand: FileOpen opens the file (and fails here, as nothing opened)");
	Check(AfxGetApp()->ProcessShellCommand(reg) == FALSE, "... AppRegister: nothing more to do");
}

void Profile(CTestApp &app, const QString &ini)
{
	app.m_pszProfileName = _wcsdup(ToWString(ini).c_str());
	Check(app.GetProfileInt(_T("Test"), _T("Missing"), 17) == 17, "GetProfileInt: the default when missing");
	Check(app.WriteProfileInt(_T("Test"), _T("Value"), -42) && static_cast<int>(app.GetProfileInt(_T("Test"), _T("Value"), 0)) == -42, "WriteProfileInt, GetProfileInt");
	Check(app.WriteProfileString(_T("Test"), _T("Name"), _T("linOpenMPT")) && app.GetProfileString(_T("Test"), _T("Name")) == _T("linOpenMPT"), "WriteProfileString, GetProfileString");
	app.WriteProfileString(_T("Test"), _T("Name"), nullptr);
	Check(app.GetProfileString(_T("Test"), _T("Name"), _T("gone")) == _T("gone"), "... a null value removes the entry");
	app.WriteProfileString(_T("Recent File List"), _T("File1"), _T("/music/a.it"));
	app.WriteProfileString(_T("Recent File List"), _T("File2"), _T("/music/b.xm"));
	app.WriteProfileString(_T("Recent File List"), _T("File5"), _T("/music/beyond.mod"));
	app.LoadStdProfileSettings(4);
	Check(app.m_recentFiles.size() == 2 && app.m_recentFiles[0] == _T("/music/a.it") && app.m_recentFiles[1] == _T("/music/b.xm"), "LoadStdProfileSettings: the recent file list, up to the count asked for");
}

void Bars(CTestApp &app, const QString &ini)
{
	auto *frame = new CTestFrame;
	if(!frame->Create(nullptr, _T("Bars"), WS_OVERLAPPEDWINDOW, CRect(0, 0, 800, 560)))
	{
		Check(false, "CFrameWnd::Create");
		return;
	}
	app.m_pMainWnd = frame;
	frame->m_wndPane.CreateEx(WS_EX_CLIENTEDGE, AfxRegisterWndClass(0, nullptr, ::GetSysColorBrush(COLOR_WINDOW)), _T(""), WS_CHILD | WS_VISIBLE, CRect(0, 0, 10, 10), frame, AFX_IDW_PANE_FIRST);

	// The toolbar.
	CToolBar &tb = frame->m_wndToolBar;
	Check(tb.CreateEx(frame, TBSTYLE_FLAT, WS_CHILD | WS_VISIBLE | CBRS_TOP | CBRS_TOOLTIPS | CBRS_FLYBY | CBRS_SIZE_DYNAMIC) && tb.GetDlgCtrlID() == AFX_IDW_TOOLBAR, "CToolBar::CreateEx, with the toolbar's id");
	Check(tb.SetButtons(g_buttons, static_cast<int>(std::size(g_buttons))) && tb.GetToolBarCtrl().GetButtonCount() == 7, "SetButtons: buttons and separators");
	Check(tb.CommandToIndex(ID_TEST_PLAIN) == 3 && tb.GetItemID(2) == ID_SEPARATOR && (tb.GetButtonStyle(2) & TBBS_SEPARATOR), "CommandToIndex, GetItemID, a separator's style");
	Check(tb.LoadBitmap(IDB_PATTERNVIEW) && tb.m_hbmImageWell != nullptr, "LoadBitmap: the images from a bitmap resource");
	tb.SetButtonText(3, _T("Plain"));
	Check(tb.GetButtonText(3) == _T("Plain"), "SetButtonText, GetButtonText");
	const CSize fixed = tb.CalcFixedLayout(FALSE, TRUE);
	Check(fixed.cy >= tb.m_sizeButton.cy && fixed.cx > 5 * tb.m_sizeButton.cx, "CalcFixedLayout: one row of buttons");
	const CSize vert = tb.CalcDynamicLayout(0, LM_VERTDOCK);
	Check(vert.cy > 5 * tb.m_sizeButton.cy && vert.cx < 2 * tb.m_sizeButton.cx + 8, "CalcDynamicLayout(LM_VERTDOCK): a column, the rows wrapped");

	// The status bar.
	CStatusBar &sb = frame->m_wndStatusBar;
	Check(sb.Create(frame) && sb.GetDlgCtrlID() == AFX_IDW_STATUS_BAR, "CStatusBar::Create");
	Check(sb.SetIndicators(g_indicators, static_cast<int>(std::size(g_indicators))) && sb.SendMessage(SB_GETPARTS, 0, 0) == 4, "SetIndicators: one part for each");
	Check(sb.GetPaneText(1) == _T("CAP") && sb.GetPaneText(2) == _T("NUM"), "... the indicators named from their strings");
	Check((sb.GetPaneStyle(0) & SBPS_STRETCH) && sb.CommandToIndex(ID_INDICATOR_SCRL) == 3, "the first pane stretches; CommandToIndex");
	sb.SetPaneText(0, _T("Hello"));
	Check(sb.GetPaneText(0) == _T("Hello") && PaneControlText(sb, 0) == L"Hello", "SetPaneText, in the control too");

	// The dialog bar.
	CDialogBar &db = frame->m_wndDialogBar;
	Check(db.Create(frame, IDD_CONTROL_COMMENTS, CBRS_BOTTOM, IDW_TEST_DIALOGBAR) && db.GetDlgItem(IDC_EDIT_COMMENTS) != nullptr, "CDialogBar::Create, with the template's controls");
	CRect dlu(0, 0, 435, 119);
	::MapDialogRect(db.m_hWnd, &dlu);
	Check(db.m_sizeDefault == dlu.Size(), "... its size the template's");

	// The layout: toolbar on top, status bar at the bottom, the dialog bar
	// above it, the pane in the rest.
	frame->ShowWindow(SW_SHOW);
	frame->RecalcLayout();
	Pump();
	CRect client;
	frame->GetClientRect(&client);
	const CRect tbRect = RectIn(&tb, frame), sbRect = RectIn(&sb, frame), dbRect = RectIn(&db, frame), paneRect = RectIn(&frame->m_wndPane, frame);
	Check(tbRect.top == 0 && tbRect.left == 0 && tbRect.right == client.right, "RecalcLayout: the toolbar across the top");
	Check(sbRect.bottom == client.bottom && sbRect.left == 0 && sbRect.right == client.right, "... the status bar across the bottom");
	Check(dbRect.left == 0 && dbRect.right == client.right && dbRect.bottom == sbRect.top && dbRect.Height() == dlu.Height(), "... the dialog bar above it, stretched across");
	Check(paneRect == CRect(0, tbRect.bottom, client.right, dbRect.top), "... the pane in the rest");
	SaveShot(frame->m_hWnd, "bars_layout");

	// Idle updates: the frame's handlers enable, disable and check buttons;
	// without a handler a button is disabled.
	Idle();
	CToolBarCtrl &ctrl = tb.GetToolBarCtrl();
	Check(!ctrl.IsButtonEnabled(ID_TEST_DISABLED), "idle update: ON_UPDATE_COMMAND_UI disables");
	Check(ctrl.IsButtonChecked(ID_TEST_CHECKED) && ctrl.IsButtonEnabled(ID_TEST_CHECKED), "... and checks");
	Check(ctrl.IsButtonEnabled(ID_TEST_PLAIN) && !ctrl.IsButtonEnabled(ID_TEST_NOHANDLER), "... with a command handler enabled, without disabled");
	Check(ctrl.IsButtonChecked(ID_VIEW_TOOLBAR), "... ID_VIEW_TOOLBAR checked while the toolbar shows");
	const bool caps = (::GetKeyState(VK_CAPITAL) & 1) != 0;
	Check(((sb.GetPaneStyle(1) & SBPS_DISABLED) == 0) == caps, "... the CAP indicator follows the key");
	frame->SendMessage(WM_COMMAND, ID_TEST_PLAIN);
	Check(g_plainCommands == 1, "a toolbar command reaches the frame");

	// Flyby help: the message string of a command in the first pane, until
	// the idle message comes back.
	frame->SendMessage(WM_SETMESSAGESTRING, ID_VIEW_TOOLBAR);
	Check(sb.GetPaneText(0) == _T("Show or hide the main toolbar"), "WM_SETMESSAGESTRING: the command's prompt, up to the newline");
	frame->SendMessage(WM_SETMESSAGESTRING, AFX_IDS_IDLEMESSAGE);
	Check(sb.GetPaneText(0) == _T("Ready"), "... and the idle message");
	frame->SetMessageText(_T("Custom"));
	Check(sb.GetPaneText(0) == _T("Custom"), "SetMessageText");
	{
		TOOLTIPTEXTW tip{};
		tip.hdr.hwndFrom = ctrl.m_hWnd;
		tip.hdr.idFrom = ID_VIEW_TOOLBAR;
		tip.hdr.code = TTN_NEEDTEXTW;
		LRESULT result = 0;
		frame->OnToolTipText(0, &tip.hdr, &result);
		Check(std::wcscmp(tip.szText, L"Toggle ToolBar") == 0, "OnToolTipText: the tip after the newline");
	}

	// Hiding a bar through its command.
	frame->SendMessage(WM_COMMAND, ID_VIEW_STATUS_BAR);
	Pump();
	Check(!sb.IsWindowVisible() && RectIn(&db, frame).bottom == client.bottom, "ID_VIEW_STATUS_BAR hides the status bar, the dialog bar takes its place");
	frame->SendMessage(WM_COMMAND, ID_VIEW_STATUS_BAR);
	Pump();
	Check(sb.IsWindowVisible() && RectIn(&db, frame) == dbRect, "... and shows it again");

	// Docking.
	frame->EnableDocking(CBRS_ALIGN_ANY);
	tb.EnableDocking(CBRS_ALIGN_ANY);
	frame->DockControlBar(&tb);
	Idle();
	Check(IsDockedAt(tb, AFX_IDW_DOCKBAR_TOP) && tb.m_pDockContext != nullptr && tb.GetDockingFrame() == frame, "DockControlBar: in the top dock bar");
	CWnd *topDock = tb.GetParent();
	Check(RectIn(topDock, frame).top == 0 && RectIn(&frame->m_wndPane, frame).top == RectIn(topDock, frame).bottom, "... which takes the top of the layout");
	frame->DockControlBar(&tb, AFX_IDW_DOCKBAR_LEFT);
	Idle();
	Check(IsDockedAt(tb, AFX_IDW_DOCKBAR_LEFT) && (tb.m_dwStyle & CBRS_ORIENT_VERT) && RectIn(&tb, frame).Height() > RectIn(&tb, frame).Width(), "DockControlBar(AFX_IDW_DOCKBAR_LEFT): standing up");
	Check(RectIn(&frame->m_wndPane, frame).left == RectIn(tb.GetParent(), frame).right, "... the pane to the right of its dock bar (which clips the bar's outer borders, as in MFC)");
	SaveShot(frame->m_hWnd, "bars_docked_left");

	CRect frameRect;
	frame->GetWindowRect(&frameRect);
	frame->FloatControlBar(&tb, CPoint(frameRect.left + 300, frameRect.top + 200));
	Idle();
	CFrameWnd *floating = tb.GetParentFrame();
	Check(tb.IsFloating() && floating != nullptr && floating != frame && floating->IsKindOf(RUNTIME_CLASS(CMiniDockFrameWnd)), "FloatControlBar: in a mini frame");
	Check(floating != nullptr && floating->IsWindowVisible() && (floating->GetStyle() & WS_POPUP) && ::GetWindow(floating->m_hWnd, GW_OWNER) == frame->m_hWnd, "... shown, a popup the main frame owns");
	Check(floating != nullptr && floating->GetOwner() == &tb && floating->GetTopLevelParent() == frame, "... its commands routed through the bar (MFC's m_hWndOwner), its top-level parent the frame");
	if(floating != nullptr)
	{
		CRect fr, tbr;
		floating->GetWindowRect(&fr);
		tb.GetWindowRect(&tbr);
		Check(fr.left == frameRect.left + 300 && fr.top == frameRect.top + 200, "... where it was asked to be");
		Check(fr.Width() > tbr.Width() && fr.Height() > tbr.Height() + 8 && (tb.m_dwStyle & CBRS_ORIENT_HORZ), "... with a caption and frame around the bar, lying down");
		SaveShot(floating->m_hWnd, "bars_floating");
		Check(::SendMessage(floating->m_hWnd, WM_NCHITTEST, 0, MAKELPARAM(fr.left + fr.Width() / 2, fr.top + 4)) == HTCAPTION, "the mini frame's caption");
	}

	// Dragging it back: over the top dock bar it docks there.
	const HWND floatingWnd = floating != nullptr ? floating->m_hWnd : nullptr;
	{
		CRect paneScreen;
		frame->m_wndPane.GetWindowRect(&paneScreen);
		CRect tbr;
		tb.GetWindowRect(&tbr);
		DragBar(tb, CPoint(paneScreen.left + 40 + tbr.Width() / 2, paneScreen.top - 2));
		Check(IsDockedAt(tb, AFX_IDW_DOCKBAR_TOP), "a drag from the floating bar to the top docks it there");
		Check(!::IsWindow(floatingWnd) || !::IsWindowVisible(floatingWnd), "... and the empty mini frame goes away");
		RECT outline{};
		Check(!LinOpenMPTGetDragFrame(&outline), "... the drag outline gone");
	}
	// And dragged out into the middle, it floats.
	{
		CRect paneScreen;
		frame->m_wndPane.GetWindowRect(&paneScreen);
		DragBar(tb, paneScreen.CenterPoint() + CPoint(0, 80));
		Check(tb.IsFloating(), "a drag into the middle floats it");
	}

	// The bar state, saved to the profile and loaded again.
	app.m_pszProfileName = _wcsdup(ToWString(ini).c_str());
	frame->DockControlBar(&tb, AFX_IDW_DOCKBAR_LEFT);
	frame->ShowControlBar(&db, FALSE, FALSE);
	frame->SaveBarState(_T("Bars"));
	{
		QFile file(ini);
		file.open(QIODevice::ReadOnly);
		const QByteArray text = file.readAll();
		Check(text.contains("[Bars-Summary]") && text.contains("[Bars-Bar0]") && text.contains("BarID="), "SaveBarState: MFC's sections and keys");
	}
	frame->DockControlBar(&tb, AFX_IDW_DOCKBAR_BOTTOM);
	frame->ShowControlBar(&db, TRUE, FALSE);
	Pump();
	Check(IsDockedAt(tb, AFX_IDW_DOCKBAR_BOTTOM) && db.IsWindowVisible(), "(moved and shown again)");
	frame->LoadBarState(_T("Bars"));
	Idle();
	Check(IsDockedAt(tb, AFX_IDW_DOCKBAR_LEFT), "LoadBarState: back where it was saved");
	Check(!db.IsWindowVisible(), "... and the dialog bar hidden again");

	frame->FloatControlBar(&tb, CPoint(frameRect.left + 120, frameRect.top + 90));
	Idle();
	frame->SaveBarState(_T("Floating"));
	frame->DockControlBar(&tb, AFX_IDW_DOCKBAR_TOP);
	Idle();
	frame->LoadBarState(_T("Floating"));
	Idle();
	floating = tb.GetParentFrame();
	CRect fr;
	if(floating != nullptr)
		floating->GetWindowRect(&fr);
	Check(tb.IsFloating() && floating != frame && fr.left == frameRect.left + 120 && fr.top == frameRect.top + 90, "... floating, at the same place");

	// Files dropped on the main window open as documents.
	frame->DragAcceptFiles(TRUE);
	Check((frame->GetExStyle() & WS_EX_ACCEPTFILES) != 0, "DragAcceptFiles: WS_EX_ACCEPTFILES");
	{
		Win32Widget *w = FromHandle(frame->m_hWnd);
		QMimeData mime;
		mime.setUrls({QUrl::fromLocalFile(QStringLiteral("/music/one.it")), QUrl::fromLocalFile(QStringLiteral("/music/two.mptm"))});
		const QPoint at = w->rect().center();
		QDragEnterEvent enter(at, Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
		QApplication::sendEvent(w, &enter);
		Check(enter.isAccepted(), "a drag of files is accepted");
		QDropEvent drop(QPointF(at), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
		g_opened.clear();
		QApplication::sendEvent(w, &drop);
		Check(g_opened.size() == 2 && g_opened[0] == L"/music/one.it" && g_opened[1] == L"/music/two.mptm", "WM_DROPFILES: CFrameWnd::OnDropFiles opens each file");
	}

	frame->DestroyWindow();
	Pump();
	app.m_pMainWnd = nullptr;
}

} // namespace

int main(int, char **)
{
	// A command line with a file to open, for ParseCommandLine.
	static char arg0[] = "bars_test";
	static char arg1[] = "/tmp/linopenmpt-test/song.it";
	static char *argv[] = {arg0, arg1, nullptr};
	static int argc = 2;
	QApplication qapp(argc, argv);
	LinOpenMPTRegisterMptrackResources();
	QTemporaryDir dir;
	CTestApp app;
	CommandLine();
	Profile(app, dir.filePath(QStringLiteral("profile.ini")));
	Bars(app, dir.filePath(QStringLiteral("bars.ini")));
	std::printf("passed: %d failure(s)\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}
