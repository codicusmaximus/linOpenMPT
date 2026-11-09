// linOpenMPT MFC layer: the application object, the message loop and idle
// processing (CWinThread, CWinApp), as MFC implements them in thrdcore.cpp
// and appcore.cpp. The loop itself is Qt's (LinOpenMPTRunMessageLoop); MFC's
// PreTranslateMessage and OnIdle are plugged into it.
#include "MfcInternal.h"

#include "QtWin.h"

namespace
{

CWinApp *g_app = nullptr;
HINSTANCE g_resourceHandle = nullptr;


int g_waitCursorCount = 0;
HCURSOR g_cursorBeforeWait = nullptr;

BOOL PreTranslateHook(MSG *msg)
{
	CWinThread *thread = AfxGetThread();
	if(thread == nullptr)
		return FALSE;
	thread->m_msgCur = *msg;
	return thread->PreTranslateMessage(msg);
}

BOOL IdleHook(LONG count)
{
	CWinThread *thread = AfxGetThread();
	return thread != nullptr && thread->OnIdle(count);
}

void DoWaitCursor(int delta)
{
	g_waitCursorCount += delta;
	if(g_waitCursorCount < 0)
		g_waitCursorCount = 0;
	if(g_waitCursorCount > 0)
	{
		const HCURSOR previous = ::SetCursor(::LoadCursor(nullptr, IDC_WAIT));
		if(delta > 0 && g_waitCursorCount == 1)
			g_cursorBeforeWait = previous;
	} else if(delta < 0)
	{
		::SetCursor(g_cursorBeforeWait != nullptr ? g_cursorBeforeWait : ::LoadCursor(nullptr, IDC_ARROW));
	}
}

} // namespace

// ---- the application object ----------------------------------------------------------

CWinApp::CWinApp(LPCTSTR appName)
{
	g_app = this;
	m_pszAppName = appName;
	m_hInstance = ::GetModuleHandle(nullptr);
}

CWinApp::~CWinApp()
{
	if(g_app == this)
		g_app = nullptr;
}

CWinApp *AfxGetApp()
{
	return g_app;
}

CWinThread *AfxGetThread()
{
	// One UI thread, as in OpenMPT.
	return g_app;
}

CWnd *AfxGetMainWnd()
{
	CWinThread *thread = AfxGetThread();
	return thread != nullptr ? thread->GetMainWnd() : nullptr;
}

HINSTANCE AfxGetInstanceHandle()
{
	// The executable, whose resources are upstream's mptrack.rc.
	return ::GetModuleHandle(nullptr);
}

HINSTANCE AfxGetResourceHandle()
{
	return g_resourceHandle != nullptr ? g_resourceHandle : AfxGetInstanceHandle();
}

void AfxSetResourceHandle(HINSTANCE instance)
{
	g_resourceHandle = instance;
}

// ---- message loop ------------------------------------------------------------------------

namespace linopenmpt::mfc
{

namespace
{
int g_preTranslateDepth = 0;
}

void PushPreTranslate()
{
	if(g_preTranslateDepth++ == 0)
		LinOpenMPTSetPreTranslateHook(PreTranslateHook);
}

void PopPreTranslate()
{
	if(--g_preTranslateDepth == 0)
		LinOpenMPTSetPreTranslateHook(nullptr);
}

} // namespace linopenmpt::mfc

int CWinThread::Run()
{
	linopenmpt::mfc::PushPreTranslate();
	const int code = LinOpenMPTRunMessageLoop(IdleHook);
	linopenmpt::mfc::PopPreTranslate();
	m_msgCur.message = WM_QUIT;
	m_msgCur.wParam = static_cast<WPARAM>(code);
	return ExitInstance();
}

BOOL CWinThread::PreTranslateMessage(MSG *msg)
{
	if(msg->hwnd == nullptr)
		return FALSE; // thread messages: OpenMPT posts none
	CWnd *mainWnd = AfxGetMainWnd();
	if(CWnd::WalkPreTranslateTree(mainWnd != nullptr ? mainWnd->m_hWnd : nullptr, msg))
		return TRUE;
	// Modeless dialogs and other top-level windows: last chance through the
	// main window (its accelerators).
	if(mainWnd != nullptr && mainWnd->m_hWnd != nullptr)
	{
		CWnd *wnd = CWnd::FromHandle(msg->hwnd);
		if(wnd->GetTopLevelParent() != mainWnd)
			return mainWnd->PreTranslateMessage(msg);
	}
	return FALSE;
}

BOOL CWinThread::OnIdle(LONG count)
{
	if(count <= 0)
	{
		// Command UI of the main window and its permanent descendants
		// (toolbars, status bar), when it can be seen.
		CWnd *mainWnd = GetMainWnd();
		if(mainWnd != nullptr && mainWnd->m_hWnd != nullptr && mainWnd->IsWindowVisible() && !mainWnd->IsIconic())
		{
			AfxCallWndProc(mainWnd, mainWnd->m_hWnd, WM_IDLEUPDATECMDUI, TRUE, 0);
			mainWnd->SendMessageToDescendants(WM_IDLEUPDATECMDUI, TRUE, 0, TRUE, TRUE);
		}
	} else
	{
		linopenmpt::mfc::DeleteTempWindowMap();
		linopenmpt::mfc::DeleteTempGdiMaps();
	}
	return count < 0; // nothing more to do for count >= 0
}

BOOL CWinApp::OnIdle(LONG count)
{
	if(count <= 0)
		CWinThread::OnIdle(count);
	else if(count == 1)
		CWinThread::OnIdle(count);
	return count < 1; // one more round (temporary objects) after the UI update
}

LRESULT CWinThread::ProcessWndProcException(CException *e, const MSG *msg)
{
	LRESULT result = 0;
	if(msg != nullptr)
	{
		if(msg->message == WM_CREATE)
			result = -1; // abort creation
		else if(msg->message == WM_PAINT)
			::ValidateRect(msg->hwnd, nullptr); // or it repaints forever
	}
	if(e != nullptr)
	{
		e->ReportError();
		e->Delete();
	}
	return result;
}

// ---- wait cursor --------------------------------------------------------------------

void CCmdTarget::BeginWaitCursor()
{
	DoWaitCursor(1);
}

void CCmdTarget::EndWaitCursor()
{
	DoWaitCursor(-1);
}

void CCmdTarget::RestoreWaitCursor()
{
	DoWaitCursor(0);
}

// ---- resources of the application -------------------------------------------------

HCURSOR CWinApp::LoadCursor(UINT id) const
{
	if(HCURSOR cursor = ::LoadCursor(AfxGetResourceHandle(), MAKEINTRESOURCE(id)))
		return cursor;
	// MFC's own cursors (afxres.rc), which ship with MFC: the system's.
	switch(id)
	{
	case AFX_IDC_HSPLITBAR: return linopenmpt::qtwin::SystemCursor(Qt::SplitHCursor);
	case AFX_IDC_VSPLITBAR: return linopenmpt::qtwin::SystemCursor(Qt::SplitVCursor);
	case AFX_IDC_SMALLARROWS: return linopenmpt::qtwin::SystemCursor(Qt::SizeAllCursor);
	case AFX_IDC_CONTEXTHELP: return linopenmpt::qtwin::SystemCursor(Qt::WhatsThisCursor);
	case AFX_IDC_NODROPCRSR: return linopenmpt::qtwin::SystemCursor(Qt::ForbiddenCursor);
	default: return nullptr;
	}
}

HCURSOR CWinApp::LoadCursor(LPCTSTR name) const
{
	return ::LoadCursor(AfxGetResourceHandle(), name);
}

HCURSOR CWinApp::LoadStandardCursor(LPCTSTR name) const
{
	return ::LoadCursor(nullptr, name);
}

HICON CWinApp::LoadIcon(UINT id) const
{
	return ::LoadIcon(AfxGetResourceHandle(), MAKEINTRESOURCE(id));
}

HICON CWinApp::LoadIcon(LPCTSTR name) const
{
	return ::LoadIcon(AfxGetResourceHandle(), name);
}

HICON CWinApp::LoadStandardIcon(LPCTSTR name) const
{
	return ::LoadIcon(nullptr, name);
}

// CString::LoadString
bool LinOpenMPTLoadStringResource(UINT id, std::wstring &wide, std::string &utf8)
{
	LPCTSTR text = nullptr;
	const int length = ::LoadString(AfxGetResourceHandle(), id, reinterpret_cast<LPTSTR>(&text), 0);
	wide.assign(text != nullptr ? text : L"", static_cast<std::size_t>(length));
	utf8 = QString::fromStdWString(wide).toStdString();
	return length > 0;
}
