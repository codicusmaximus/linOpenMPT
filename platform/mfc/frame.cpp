// linOpenMPT MFC layer: frame windows, as MFC's winfrm.cpp, winmdi.cpp and
// cmdtarg.cpp have them.
//
// - CFrameWnd: creation from a resource id (LoadFrame: menu, title,
//   accelerators), the client (a view from the CCreateContext), control bar
//   layout (RecalcLayout through WM_SIZEPARENT), the active view, the title
//   (FWS_ADDTOTITLE), command routing (view, then frame, then application)
//   and menu item state through CCmdUI;
// - CMDIFrameWnd and CMDIChildWnd on user32's MDI (platform/qtwin/mdi.cpp):
//   the MDI client, child frames created through WM_MDICREATE, activation,
//   the shared menus of document templates.
//
// Frames delete themselves when their window goes (PostNcDestroy), as in
// MFC, so they are created with new.
#include "MfcInternal.h"

#include <algorithm>

// ---- CCmdUI ------------------------------------------------------------------------

void CCmdUI::Enable(BOOL on)
{
	if(m_pMenu != nullptr)
	{
		if(m_pSubMenu != nullptr)
			return; // popups are not changed through their first item
		m_pMenu->EnableMenuItem(m_nIndex, MF_BYPOSITION | (on ? MF_ENABLED : (MF_DISABLED | MF_GRAYED)));
	} else if(m_pOther != nullptr)
	{
		// The focus moves on before a control loses it.
		if(!on && ::GetFocus() == m_pOther->m_hWnd)
		{
			if(CWnd *parent = m_pOther->GetParent())
			{
				if(CWnd *next = parent->GetNextDlgTabItem(m_pOther))
					next->SetFocus();
			}
		}
		m_pOther->EnableWindow(on);
	}
	m_bEnableChanged = TRUE;
}

void CCmdUI::SetCheck(int check)
{
	if(m_pMenu != nullptr)
	{
		if(m_pSubMenu != nullptr)
			return;
		m_pMenu->CheckMenuItem(m_nIndex, MF_BYPOSITION | (check ? MF_CHECKED : MF_UNCHECKED));
	} else if(m_pOther != nullptr)
	{
		if(m_pOther->SendMessage(WM_GETDLGCODE) & DLGC_BUTTON)
			m_pOther->SendMessage(BM_SETCHECK, static_cast<WPARAM>(check));
	}
}

void CCmdUI::SetRadio(BOOL on)
{
	SetCheck(on ? 1 : 0);
}

void CCmdUI::SetText(LPCTSTR text)
{
	if(m_pMenu != nullptr)
	{
		if(m_pSubMenu != nullptr)
			return;
		const UINT state = m_pMenu->GetMenuState(m_nIndex, MF_BYPOSITION) & ~(MF_BITMAP | MF_OWNERDRAW | MF_SEPARATOR | MF_POPUP);
		m_pMenu->ModifyMenu(m_nIndex, MF_BYPOSITION | MF_STRING | (state & 0xFF), m_nID, text);
	} else if(m_pOther != nullptr)
	{
		AfxSetWindowText(m_pOther->m_hWnd, text);
	}
}

BOOL CCmdUI::DoUpdate(CCmdTarget *target, BOOL disableIfNoHandler)
{
	if(m_nID == 0 || LOWORD(m_nID) == 0xFFFF || target == nullptr)
		return TRUE;
	m_bEnableChanged = FALSE;
	const BOOL result = target->OnCmdMsg(m_nID, CN_UPDATE_COMMAND_UI, this, nullptr);
	if(disableIfNoHandler && !m_bEnableChanged)
	{
		AFX_CMDHANDLERINFO info{};
		const BOOL handler = target->OnCmdMsg(m_nID, CN_COMMAND, this, &info);
		Enable(handler);
	}
	return result;
}

// ---- helpers ---------------------------------------------------------------------------

void AFXAPI AfxSetWindowText(HWND hwnd, LPCTSTR text)
{
	if(hwnd == nullptr)
		return;
	// Only when it changes, so the caption does not flicker.
	const int length = ::GetWindowTextLength(hwnd);
	std::wstring old(static_cast<std::size_t>(length) + 1, L'\0');
	::GetWindowText(hwnd, old.data(), length + 1);
	old.resize(static_cast<std::size_t>(length));
	if(old != (text ? text : L""))
		::SetWindowText(hwnd, text);
}

BOOL AFXAPI AfxExtractSubString(CString &part, LPCTSTR fullString, int index, TCHAR separator)
{
	part.Empty();
	if(fullString == nullptr)
		return FALSE;
	const TCHAR *p = fullString;
	while(index-- > 0)
	{
		p = std::wcschr(p, separator);
		if(p == nullptr)
			return FALSE;
		p++;
	}
	const TCHAR *end = std::wcschr(p, separator);
	part = CString(p, end ? static_cast<int>(end - p) : static_cast<int>(std::wcslen(p)));
	return TRUE;
}

void CWnd::RepositionBars(UINT idFirst, UINT idLast, UINT idLeftOver, UINT flag, LPRECT param, LPCRECT clientRect, BOOL stretch)
{
	AFX_SIZEPARENTPARAMS layout{};
	HWND leftOver = nullptr;
	layout.bStretch = stretch;
	layout.sizeTotal = SIZE{0, 0};
	if(clientRect != nullptr)
		layout.rect = *clientRect;
	else
		::GetClientRect(m_hWnd, &layout.rect);
	if(flag != reposQuery)
		layout.hDWP = ::BeginDeferWindowPos(8);
	// Each control bar takes its part of the rectangle.
	for(HWND child = ::GetWindow(m_hWnd, GW_CHILD); child != nullptr; child = ::GetWindow(child, GW_HWNDNEXT))
	{
		const UINT id = static_cast<UINT>(::GetDlgCtrlID(child));
		CWnd *wnd = CWnd::FromHandlePermanent(child);
		if(id == idLeftOver)
			leftOver = child;
		else if(wnd != nullptr && id >= idFirst && id <= idLast)
			::SendMessage(child, WM_SIZEPARENT, 0, reinterpret_cast<LPARAM>(&layout));
	}
	if(flag == reposQuery)
	{
		// What is left (or what it would take, with stretch off).
		if(param != nullptr)
		{
			if(stretch)
				::CopyRect(param, &layout.rect);
			else
				*param = RECT{0, 0, layout.sizeTotal.cx, layout.sizeTotal.cy};
		}
		return;
	}
	CRect leftOverRect(layout.rect);
	if(flag == reposExtra && param != nullptr)
	{
		// Extra borders around the left-over window.
		leftOverRect.left += param->left;
		leftOverRect.top += param->top;
		leftOverRect.right -= param->right;
		leftOverRect.bottom -= param->bottom;
	}
	if(leftOver != nullptr && !(flag & reposNoPosLeftOver))
		layout.hDWP = ::DeferWindowPos(layout.hDWP, leftOver, nullptr, leftOverRect.left, leftOverRect.top, leftOverRect.Width(), leftOverRect.Height(), SWP_NOZORDER | SWP_NOACTIVATE);
	if(layout.hDWP != nullptr)
		::EndDeferWindowPos(layout.hDWP);
}

BOOL CWnd::IsTopParentActive() const
{
	CWnd *top = GetTopLevelParent();
	const HWND active = ::GetActiveWindow();
	if(top == nullptr)
		return FALSE;
	// Owned windows (a floating palette) count as their owner.
	for(HWND hwnd = active; hwnd != nullptr; hwnd = ::GetWindow(hwnd, GW_OWNER))
	{
		if(hwnd == top->m_hWnd)
			return TRUE;
	}
	return FALSE;
}

// ---- CFrameWnd ---------------------------------------------------------------------

const CRect CFrameWnd::rectDefault(CW_USEDEFAULT, CW_USEDEFAULT, 0, 0);

BEGIN_MESSAGE_MAP(CFrameWnd, CWnd)
	ON_WM_CREATE()
	ON_WM_DESTROY()
	ON_WM_CLOSE()
	ON_WM_SIZE()
	ON_WM_SETFOCUS()
	ON_WM_ERASEBKGND()
	ON_WM_INITMENUPOPUP()
	ON_MESSAGE(WM_IDLEUPDATECMDUI, &CFrameWnd::OnIdleUpdateCmdUI)
	ON_WM_ACTIVATE()
	ON_WM_ENABLE()
	ON_WM_MENUSELECT()
	ON_WM_DROPFILES()
	ON_MESSAGE(WM_SETMESSAGESTRING, &CFrameWnd::OnSetMessageString)
	ON_MESSAGE(WM_POPMESSAGESTRING, &CFrameWnd::OnPopMessageString)
	// The standard bars and indicators (MFC's winfrm.cpp).
	ON_UPDATE_COMMAND_UI(ID_VIEW_STATUS_BAR, &CFrameWnd::OnUpdateControlBarMenu)
	ON_COMMAND_EX(ID_VIEW_STATUS_BAR, &CFrameWnd::OnBarCheck)
	ON_UPDATE_COMMAND_UI(ID_VIEW_TOOLBAR, &CFrameWnd::OnUpdateControlBarMenu)
	ON_COMMAND_EX(ID_VIEW_TOOLBAR, &CFrameWnd::OnBarCheck)
	ON_UPDATE_COMMAND_UI(ID_INDICATOR_CAPS, &CFrameWnd::OnUpdateKeyIndicator)
	ON_UPDATE_COMMAND_UI(ID_INDICATOR_NUM, &CFrameWnd::OnUpdateKeyIndicator)
	ON_UPDATE_COMMAND_UI(ID_INDICATOR_SCRL, &CFrameWnd::OnUpdateKeyIndicator)
	ON_NOTIFY_EX_RANGE(TTN_NEEDTEXTW, 0, 0xFFFF, &CFrameWnd::OnToolTipText)
END_MESSAGE_MAP()

// Every frame window, as MFC's frame list: idle-time updates reach floating
// frames through it.
std::vector<CFrameWnd *> &linopenmpt::mfc::FrameList()
{
	static std::vector<CFrameWnd *> frames;
	return frames;
}

CFrameWnd::CFrameWnd()
{
	linopenmpt::mfc::FrameList().push_back(this);
}

CFrameWnd::~CFrameWnd()
{
	auto &frames = linopenmpt::mfc::FrameList();
	frames.erase(std::remove(frames.begin(), frames.end(), this), frames.end());
}

namespace
{

LPCTSTR FrameClass(UINT resourceID, HICON *iconOut = nullptr)
{
	// One class per icon, as MFC's GetIconWndClass.
	HICON icon = resourceID ? ::LoadIcon(AfxGetResourceHandle(), MAKEINTRESOURCE(resourceID)) : nullptr;
	if(iconOut)
		*iconOut = icon;
	return AfxRegisterWndClass(CS_DBLCLKS, ::LoadCursor(nullptr, IDC_ARROW), reinterpret_cast<HBRUSH>(static_cast<INT_PTR>(COLOR_WINDOW + 1)), icon);
}

} // namespace

BOOL CFrameWnd::PreCreateWindow(CREATESTRUCT &cs)
{
	if(cs.lpszClass == nullptr)
		cs.lpszClass = FrameClass(0);
	if(cs.style & FWS_ADDTOTITLE)
		cs.style |= FWS_PREFIXTITLE;
	cs.dwExStyle |= WS_EX_CLIENTEDGE;
	return TRUE;
}

BOOL CFrameWnd::Create(LPCTSTR className, LPCTSTR windowName, DWORD style, const RECT &rect, CWnd *parent, LPCTSTR menuName, DWORD exStyle, CCreateContext *context)
{
	HMENU menu = nullptr;
	if(menuName != nullptr)
	{
		menu = ::LoadMenu(AfxGetResourceHandle(), menuName);
		if(menu == nullptr)
		{
			PostNcDestroy();
			return FALSE;
		}
	}
	m_strTitle = windowName ? windowName : L"";
	if(!CreateEx(exStyle, className, windowName, style, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
		parent ? parent->GetSafeHwnd() : nullptr, menu, context))
	{
		if(menu != nullptr)
			::DestroyMenu(menu);
		return FALSE;
	}
	return TRUE;
}

BOOL CFrameWnd::LoadFrame(UINT resourceID, DWORD style, CWnd *parent, CCreateContext *context)
{
	m_nIDHelp = resourceID;
	// The title is the first part of the resource string.
	CString full;
	full.LoadString(resourceID);
	AfxExtractSubString(m_strTitle, full, 0);
	const CString title = m_strTitle;
	if(!Create(FrameClass(resourceID), title, style, rectDefault, parent, MAKEINTRESOURCE(resourceID), 0, context))
		return FALSE;
	m_hMenuDefault = ::GetMenu(m_hWnd);
	if(m_hAccelTable == nullptr)
		m_hAccelTable = ::LoadAccelerators(AfxGetResourceHandle(), MAKEINTRESOURCE(resourceID));
	if(context == nullptr)
		SendMessageToDescendants(WM_INITIALUPDATE, 0, 0, TRUE, TRUE);
	return TRUE;
}

int CFrameWnd::OnCreate(LPCREATESTRUCT cs)
{
	return OnCreateHelper(cs, static_cast<CCreateContext *>(cs->lpCreateParams));
}

int CFrameWnd::OnCreateHelper(LPCREATESTRUCT cs, CCreateContext *context)
{
	if(CWnd::OnCreate(cs) == -1)
		return -1;
	if(!OnCreateClient(cs, context))
		return -1;
	RecalcLayout();
	return 0;
}

BOOL CFrameWnd::OnCreateClient(LPCREATESTRUCT, CCreateContext *context)
{
	if(context != nullptr && context->m_pNewViewClass != nullptr)
		return CreateView(context, AFX_IDW_PANE_FIRST) != nullptr;
	return TRUE;
}

CWnd *CFrameWnd::CreateView(CCreateContext *context, UINT id)
{
	auto *view = context && context->m_pNewViewClass ? dynamic_cast<CWnd *>(context->m_pNewViewClass->CreateObject()) : nullptr;
	if(view == nullptr)
		return nullptr;
	// Views are made visible by InitialUpdateFrame, as in MFC.
	if(!view->Create(nullptr, nullptr, AFX_WS_DEFAULT_VIEW, CRect(0, 0, 0, 0), this, id, context))
		return nullptr; // the view deleted itself (PostNcDestroy)
	if(view->GetExStyle() & WS_EX_CLIENTEDGE)
		ModifyStyleEx(WS_EX_CLIENTEDGE, 0, SWP_FRAMECHANGED);
	return view;
}

void CFrameWnd::PostNcDestroy()
{
	delete this;
}

void CFrameWnd::OnDestroy()
{
	DestroyDockBars();
	// The shared menu of a document goes back to the frame's own before the
	// window takes it with it.
	if(m_hMenuDefault != nullptr && ::GetMenu(m_hWnd) != m_hMenuDefault)
		::SetMenu(m_hWnd, m_hMenuDefault);
	CWinApp *app = AfxGetApp();
	if(app != nullptr && app->m_pMainWnd == this)
		::PostQuitMessage(0);
	CWnd::OnDestroy();
}

void CFrameWnd::OnClose()
{
	CDocument *doc = GetActiveDocument();
	if(doc != nullptr && !doc->CanCloseFrame(this))
		return;
	CWinApp *app = AfxGetApp();
	if(app != nullptr && app->m_pMainWnd == this)
	{
		// The main window: every document has to agree.
		if(doc == nullptr && !app->SaveAllModified())
			return;
		app->CloseAllDocuments(FALSE);
		if(app->m_pMainWnd == nullptr)
			return; // closing the documents closed the window
	}
	// The last frame of a document closes the document instead.
	if(doc != nullptr && doc->m_bAutoDelete)
	{
		bool otherFrame = false;
		POSITION pos = doc->GetFirstViewPosition();
		while(pos != nullptr)
		{
			CView *view = doc->GetNextView(pos);
			if(view->GetParentFrame() != this)
			{
				otherFrame = true;
				break;
			}
		}
		if(!otherFrame)
		{
			doc->OnCloseDocument();
			return;
		}
		doc->PreCloseFrame(this);
	}
	DestroyWindow();
}

void CFrameWnd::OnSize(UINT type, int cx, int cy)
{
	CWnd::OnSize(type, cx, cy);
	if(type != SIZE_MINIMIZED)
		RecalcLayout();
}

void CFrameWnd::OnSetFocus(CWnd *oldWnd)
{
	if(m_pViewActive != nullptr && m_pViewActive->m_hWnd != nullptr)
		m_pViewActive->SetFocus();
	else
		CWnd::OnSetFocus(oldWnd);
}

BOOL CFrameWnd::OnEraseBkgnd(CDC *dc)
{
	// The view covers the client area; no need to paint under it.
	if(m_pViewActive != nullptr)
		return TRUE;
	return CWnd::OnEraseBkgnd(dc);
}

void CFrameWnd::RecalcLayout(BOOL notify)
{
	if(m_bInRecalcLayout)
		return;
	m_bInRecalcLayout = TRUE;
	if(m_nIdleFlags & idleNotify)
		notify = TRUE;
	m_nIdleFlags &= ~(idleLayout | idleNotify);
	if(GetStyle() & FWS_SNAPTOBARS)
	{
		// The frame takes the size of its bars (floating frames).
		CRect rect(0, 0, 32767, 32767);
		RepositionBars(0, 0xFFFF, AFX_IDW_PANE_FIRST, reposQuery, &rect, &rect, FALSE);
		RepositionBars(0, 0xFFFF, AFX_IDW_PANE_FIRST, reposExtra, &m_rectBorder, &rect, TRUE);
		CalcWindowRect(&rect);
		SetWindowPos(nullptr, 0, 0, rect.Width(), rect.Height(), SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOZORDER);
	} else
	{
		RepositionBars(0, 0xFFFF, AFX_IDW_PANE_FIRST, reposExtra, &m_rectBorder);
	}
	m_bInRecalcLayout = FALSE;
}

void CFrameWnd::ActivateFrame(int cmdShow)
{
	if(cmdShow == -1)
	{
		if(!IsWindowVisible())
			cmdShow = SW_SHOWNORMAL;
		else if(IsIconic())
			cmdShow = SW_RESTORE;
	}
	if(cmdShow != -1)
	{
		ShowWindow(cmdShow);
		BringWindowToTop();
	}
}

void CFrameWnd::InitialUpdateFrame(CDocument *doc, BOOL makeVisible)
{
	CView *view = nullptr;
	if(GetActiveView() == nullptr)
	{
		CWnd *pane = GetDescendantWindow(AFX_IDW_PANE_FIRST, TRUE);
		if(pane != nullptr && pane->IsKindOf(RUNTIME_CLASS(CView)))
		{
			view = static_cast<CView *>(pane);
			SetActiveView(view, FALSE);
		}
	}
	if(makeVisible)
	{
		SendMessageToDescendants(WM_INITIALUPDATE, 0, 0, TRUE, TRUE);
		if(view != nullptr)
			view->OnActivateFrame(WA_INACTIVE, this);
		int cmdShow = -1;
		CWinApp *app = AfxGetApp();
		if(app != nullptr && app->m_pMainWnd == this)
		{
			cmdShow = app->m_nCmdShow;
			app->m_nCmdShow = -1;
		}
		ActivateFrame(cmdShow);
		if(view != nullptr)
			view->OnActivateView(TRUE, view, view);
	}
	if(doc != nullptr)
		doc->UpdateFrameCounts();
	OnUpdateFrameTitle(TRUE);
}

void CFrameWnd::SetActiveView(CView *view, BOOL notify)
{
	CView *old = m_pViewActive;
	if(view == old)
		return;
	m_pViewActive = view;
	if(notify && old != nullptr)
		old->OnActivateView(FALSE, view, old);
	if(notify && view != nullptr && view == m_pViewActive)
		view->OnActivateView(TRUE, view, old);
}

CDocument *CFrameWnd::GetActiveDocument()
{
	CView *view = GetActiveView();
	return view != nullptr ? view->GetDocument() : nullptr;
}

CFrameWnd *CFrameWnd::GetTopLevelFrame() const
{
	auto *frame = const_cast<CFrameWnd *>(this);
	while(frame->GetStyle() & WS_CHILD)
	{
		CFrameWnd *parent = frame->GetParentFrame();
		if(parent == nullptr)
			break;
		frame = parent;
	}
	return frame;
}

void CFrameWnd::OnUpdateFrameTitle(BOOL addToTitle)
{
	if(!(GetStyle() & FWS_ADDTOTITLE))
		return;
	CDocument *doc = GetActiveDocument();
	if(addToTitle && doc != nullptr)
		UpdateFrameTitleForDocument(doc->GetTitle());
	else
		UpdateFrameTitleForDocument(nullptr);
}

void CFrameWnd::UpdateFrameTitleForDocument(LPCTSTR docName)
{
	CString text;
	auto appendDoc = [&]()
	{
		text += docName;
		if(m_nWindow > 0)
		{
			CString n;
			n.Format(_T(":%d"), m_nWindow);
			text += n;
		}
	};
	if(GetStyle() & FWS_PREFIXTITLE)
	{
		if(docName != nullptr)
		{
			appendDoc();
			text += _T(" - ");
		}
		text += m_strTitle;
	} else
	{
		text += m_strTitle;
		if(docName != nullptr)
		{
			text += _T(" - ");
			appendDoc();
		}
	}
	// An MDI frame's text has " - [child]" after it while a child is
	// maximized (DefFrameProc): that is still this title.
	TCHAR current[512] = {};
	::GetWindowText(m_hWnd, current, 512);
	const CString now = current;
	if(now != text && !(dynamic_cast<CMDIFrameWnd *>(this) != nullptr && now.Left(text.GetLength() + 4) == text + _T(" - [")))
		AfxSetWindowText(m_hWnd, text);
}

void CFrameWnd::OnUpdateFrameMenu(HMENU menuAlt)
{
	if(menuAlt == nullptr)
	{
		CDocument *doc = GetActiveDocument();
		if(doc != nullptr)
			menuAlt = doc->GetDefaultMenu();
		if(menuAlt == nullptr)
			menuAlt = m_hMenuDefault;
	}
	if(menuAlt != nullptr && ::GetMenu(m_hWnd) != menuAlt)
		::SetMenu(m_hWnd, menuAlt);
}

HACCEL CFrameWnd::GetDefaultAccelerator()
{
	HACCEL accel = nullptr;
	if(CDocument *doc = GetActiveDocument())
		accel = doc->GetDefaultAccelerator();
	return accel != nullptr ? accel : m_hAccelTable;
}

BOOL CFrameWnd::PreTranslateMessage(MSG *msg)
{
	if(CWnd::PreTranslateMessage(msg))
		return TRUE;
	if(msg->message >= WM_KEYFIRST && msg->message <= WM_KEYLAST)
	{
		HACCEL accel = GetDefaultAccelerator();
		return accel != nullptr && ::TranslateAccelerator(m_hWnd, accel, msg);
	}
	return FALSE;
}

BOOL CFrameWnd::OnCmdMsg(UINT id, int code, void *extra, void *handlerInfo)
{
	// The active view (and its document) first, then the frame, then the
	// application.
	CView *view = GetActiveView();
	if(view != nullptr && view->OnCmdMsg(id, code, extra, handlerInfo))
		return TRUE;
	if(CWnd::OnCmdMsg(id, code, extra, handlerInfo))
		return TRUE;
	CWinApp *app = AfxGetApp();
	return app != nullptr && app->OnCmdMsg(id, code, extra, handlerInfo);
}

BOOL CFrameWnd::OnCommand(WPARAM wp, LPARAM lp)
{
	return CWnd::OnCommand(wp, lp);
}

void CFrameWnd::OnInitMenuPopup(CMenu *menu, UINT, BOOL sysMenu)
{
	if(sysMenu || menu == nullptr)
		return;
	CCmdUI state;
	state.m_pMenu = menu;
	// The menu bar holding this popup, if it is one of its items.
	if(CWnd *top = GetTopLevelParent())
	{
		if(HMENU bar = ::GetMenu(top->m_hWnd))
		{
			for(int i = 0, n = ::GetMenuItemCount(bar); i < n; i++)
			{
				if(::GetSubMenu(bar, i) == menu->m_hMenu)
				{
					state.m_pParentMenu = CMenu::FromHandle(bar);
					break;
				}
			}
		}
	}
	state.m_nIndexMax = static_cast<UINT>(menu->GetMenuItemCount());
	for(state.m_nIndex = 0; state.m_nIndex < state.m_nIndexMax; state.m_nIndex++)
	{
		state.m_nID = menu->GetMenuItemID(static_cast<int>(state.m_nIndex));
		if(state.m_nID == 0)
			continue; // separators
		if(state.m_nID == static_cast<UINT>(-1))
		{
			// A popup: updated through its first item, never disabled.
			state.m_pSubMenu = menu->GetSubMenu(static_cast<int>(state.m_nIndex));
			if(state.m_pSubMenu == nullptr || (state.m_nID = state.m_pSubMenu->GetMenuItemID(0)) == 0 || state.m_nID == static_cast<UINT>(-1))
				continue;
			state.DoUpdate(this, FALSE);
		} else
		{
			state.m_pSubMenu = nullptr;
			state.DoUpdate(this, m_bAutoMenuEnable && state.m_nID < 0xF000);
		}
		// Handlers may have added or removed items.
		const UINT count = static_cast<UINT>(menu->GetMenuItemCount());
		if(count < state.m_nIndexMax)
		{
			state.m_nIndex -= (state.m_nIndexMax - count);
			while(state.m_nIndex < count && menu->GetMenuItemID(static_cast<int>(state.m_nIndex)) == state.m_nID)
				state.m_nIndex++;
		}
		state.m_nIndexMax = count;
	}
}

LRESULT CFrameWnd::OnIdleUpdateCmdUI(WPARAM, LPARAM)
{
	// The title follows the active document.
	if(GetStyle() & FWS_ADDTOTITLE)
	{
		CFrameWnd *top = GetTopLevelFrame();
		if(top == this)
			OnUpdateFrameTitle(TRUE);
	}
	// The layout asked for since (DelayRecalcLayout).
	if(m_nIdleFlags & idleLayout)
	{
		RecalcLayout((m_nIdleFlags & idleNotify) != 0);
		UpdateWindow();
	}
	// The prompt of what the menu is on.
	if(m_nIDTracking != m_nIDLastMessage)
		SetMessageText(m_nIDTracking);
	m_nIdleFlags = 0;
	return 0;
}

void CFrameWnd::OnActivate(UINT state, CWnd *other, BOOL minimized)
{
	CWnd::OnActivate(state, other, minimized);
	// Floating bars look active while the frame they belong to is.
	CFrameWnd *top = (GetStyle() & WS_CHILD) ? this : GetTopLevelFrame();
	if(top == nullptr)
		return;
	CWnd *active = state == WA_INACTIVE ? other : this;
	BOOL stayActive = FALSE;
	if(active != nullptr)
	{
		CFrameWnd *activeTop = active->IsFrameWnd() ? static_cast<CFrameWnd *>(active)->GetTopLevelFrame() : nullptr;
		stayActive = top == active || (top == activeTop && active->SendMessage(WM_FLOATSTATUS, FS_SYNCACTIVE) != 0);
	}
	top->m_nFlags &= ~WF_STAYACTIVE;
	if(stayActive)
		top->m_nFlags |= WF_STAYACTIVE;
	NotifyFloatingWindows(stayActive ? FS_ACTIVATE : FS_DEACTIVATE);
	// The active view hears of it.
	CView *view = GetActiveView();
	if(view == nullptr && GetActiveFrame() != nullptr)
		view = GetActiveFrame()->GetActiveView();
	if(view != nullptr)
	{
		if(state != WA_INACTIVE && !minimized)
			view->OnActivateView(TRUE, view, view);
		view->OnActivateFrame(state, this);
	}
}

void CFrameWnd::OnEnable(BOOL enable)
{
	// Floating bars are disabled with their frame (by a modal dialog).
	if(GetParent() != nullptr)
		return;
	NotifyFloatingWindows(enable ? FS_ENABLE : FS_DISABLE);
}

void CFrameWnd::OnDropFiles(HDROP drop)
{
	SetActiveWindow();
	const UINT files = ::DragQueryFile(drop, 0xFFFFFFFF, nullptr, 0);
	CWinApp *app = AfxGetApp();
	for(UINT i = 0; i < files && app != nullptr; i++)
	{
		const UINT length = ::DragQueryFile(drop, i, nullptr, 0);
		std::wstring name(static_cast<std::size_t>(length) + 1, L'\0');
		::DragQueryFile(drop, i, name.data(), length + 1);
		app->OpenDocumentFile(name.c_str());
	}
	::DragFinish(drop);
}

// Control bars: the list the frame lays out. Docking comes with the bars.
void CFrameWnd::AddControlBar(CControlBar *bar)
{
	if(bar != nullptr && std::find(m_listControlBars.begin(), m_listControlBars.end(), bar) == m_listControlBars.end())
		m_listControlBars.push_back(bar);
}

void CFrameWnd::RemoveControlBar(CControlBar *bar)
{
	m_listControlBars.erase(std::remove(m_listControlBars.begin(), m_listControlBars.end(), bar), m_listControlBars.end());
}

CControlBar *CFrameWnd::GetControlBar(UINT id)
{
	if(id == 0)
		return nullptr;
	for(CControlBar *bar : m_listControlBars)
	{
		if(bar->m_hWnd != nullptr && static_cast<UINT>(bar->GetDlgCtrlID()) == id)
			return bar;
	}
	return nullptr;
}

BOOL CFrameWnd::OnBarCheck(UINT id)
{
	CControlBar *bar = GetControlBar(id);
	if(bar == nullptr)
		return FALSE;
	ShowControlBar(bar, (bar->GetStyle() & WS_VISIBLE) == 0, FALSE);
	return TRUE;
}

void CFrameWnd::OnUpdateControlBarMenu(CCmdUI *cmdUI)
{
	CControlBar *bar = GetControlBar(cmdUI->m_nID);
	if(bar != nullptr)
	{
		cmdUI->SetCheck((bar->GetStyle() & WS_VISIBLE) != 0);
		return;
	}
	cmdUI->ContinueRouting();
}

// ---- CMDIFrameWnd --------------------------------------------------------------------

// The first id of the window menu's child list, as MFC's AFX_IDM_FIRST_MDICHILD.
constexpr UINT AfxFirstMdiChild = 0xFF00;

BEGIN_MESSAGE_MAP(CMDIFrameWnd, CFrameWnd)
	ON_WM_DESTROY()
	ON_UPDATE_COMMAND_UI(ID_WINDOW_ARRANGE, &CMDIFrameWnd::OnUpdateMDIWindowCmd)
	ON_UPDATE_COMMAND_UI(ID_WINDOW_CASCADE, &CMDIFrameWnd::OnUpdateMDIWindowCmd)
	ON_UPDATE_COMMAND_UI(ID_WINDOW_TILE_HORZ, &CMDIFrameWnd::OnUpdateMDIWindowCmd)
	ON_UPDATE_COMMAND_UI(ID_WINDOW_TILE_VERT, &CMDIFrameWnd::OnUpdateMDIWindowCmd)
	ON_COMMAND_EX(ID_WINDOW_ARRANGE, &CMDIFrameWnd::OnMDIWindowCmd)
	ON_COMMAND_EX(ID_WINDOW_CASCADE, &CMDIFrameWnd::OnMDIWindowCmd)
	ON_COMMAND_EX(ID_WINDOW_TILE_HORZ, &CMDIFrameWnd::OnMDIWindowCmd)
	ON_COMMAND_EX(ID_WINDOW_TILE_VERT, &CMDIFrameWnd::OnMDIWindowCmd)
	ON_COMMAND(ID_WINDOW_NEW, &CMDIFrameWnd::OnWindowNew)
END_MESSAGE_MAP()

BOOL CMDIFrameWnd::PreCreateWindow(CREATESTRUCT &cs)
{
	if(!CFrameWnd::PreCreateWindow(cs))
		return FALSE;
	// The MDI client has the edge.
	cs.dwExStyle &= ~WS_EX_CLIENTEDGE;
	return TRUE;
}

BOOL CMDIFrameWnd::LoadFrame(UINT resourceID, DWORD style, CWnd *parent, CCreateContext *context)
{
	if(!CFrameWnd::LoadFrame(resourceID, style, parent, context))
		return FALSE;
	m_hMenuDefault = ::GetMenu(m_hWnd);
	return TRUE;
}

BOOL CMDIFrameWnd::OnCreateClient(LPCREATESTRUCT cs, CCreateContext *)
{
	// The Window menu is the second-to-last popup of the menu bar.
	HMENU bar = m_hMenuDefault != nullptr ? m_hMenuDefault : ::GetMenu(m_hWnd);
	CMenu *windowMenu = nullptr;
	if(bar != nullptr)
	{
		if(HMENU popup = GetWindowMenuPopup(bar))
			windowMenu = CMenu::FromHandle(popup);
		else if(const int count = ::GetMenuItemCount(bar); count >= 2)
			windowMenu = CMenu::FromHandle(::GetSubMenu(bar, count - 2));
	}
	return CreateClient(cs, windowMenu);
}

BOOL CMDIFrameWnd::CreateClient(LPCREATESTRUCT cs, CMenu *windowMenu)
{
	DWORD style = WS_VISIBLE | WS_CHILD | WS_BORDER | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | MDIS_ALLCHILDSTYLES;
	// Scroll bars move from the frame to the client.
	style |= static_cast<DWORD>(cs->style) & (WS_HSCROLL | WS_VSCROLL);
	ModifyStyle(WS_HSCROLL | WS_VSCROLL, 0, SWP_FRAMECHANGED);
	style &= ~WS_BORDER;
	CLIENTCREATESTRUCT ccs{windowMenu ? windowMenu->GetSafeHmenu() : nullptr, AfxFirstMdiChild};
	m_hWndMDIClient = ::CreateWindowEx(WS_EX_CLIENTEDGE, L"MDIClient", nullptr, style, 0, 0, 0, 0, m_hWnd,
		reinterpret_cast<HMENU>(static_cast<UINT_PTR>(AFX_IDW_PANE_FIRST)), AfxGetInstanceHandle(), &ccs);
	if(m_hWndMDIClient == nullptr)
		return FALSE;
	::SetWindowPos(m_hWndMDIClient, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
	return TRUE;
}

HMENU CMDIFrameWnd::GetWindowMenuPopup(HMENU menuBar)
{
	if(menuBar == nullptr)
		return nullptr;
	for(int i = ::GetMenuItemCount(menuBar) - 1; i >= 0; i--)
	{
		HMENU popup = ::GetSubMenu(menuBar, i);
		if(popup == nullptr)
			continue;
		for(int j = 0, n = ::GetMenuItemCount(popup); j < n; j++)
		{
			const UINT id = ::GetMenuItemID(popup, j);
			if(id >= ID_WINDOW_NEW && id <= ID_WINDOW_SPLIT)
				return popup;
		}
	}
	return nullptr;
}

LRESULT CMDIFrameWnd::DefWindowProc(UINT msg, WPARAM wp, LPARAM lp)
{
	return ::DefFrameProc(m_hWnd, m_hWndMDIClient, msg, wp, lp);
}

void CMDIFrameWnd::OnDestroy()
{
	if(m_hWndMDIClient != nullptr)
	{
		::DestroyWindow(m_hWndMDIClient);
		m_hWndMDIClient = nullptr;
	}
	CFrameWnd::OnDestroy();
}

BOOL CMDIFrameWnd::PreTranslateMessage(MSG *msg)
{
	if(CWnd::PreTranslateMessage(msg))
		return TRUE;
	// The active child first (its document's accelerators).
	CMDIChildWnd *child = MDIGetActive();
	if(child != nullptr && child->PreTranslateMessage(msg))
		return TRUE;
	if(msg->message >= WM_KEYFIRST && msg->message <= WM_KEYLAST)
	{
		if(m_hAccelTable != nullptr && ::TranslateAccelerator(m_hWnd, m_hAccelTable, msg))
			return TRUE;
		if((msg->message == WM_KEYDOWN || msg->message == WM_SYSKEYDOWN) && GetActiveView() == nullptr)
		{
			if(::TranslateMDISysAccel(m_hWndMDIClient, msg))
				return TRUE;
		}
	}
	return FALSE;
}

BOOL CMDIFrameWnd::OnCmdMsg(UINT id, int code, void *extra, void *handlerInfo)
{
	if(CMDIChildWnd *child = MDIGetActive())
	{
		if(child->OnCmdMsg(id, code, extra, handlerInfo))
			return TRUE;
	}
	return CFrameWnd::OnCmdMsg(id, code, extra, handlerInfo);
}

BOOL CMDIFrameWnd::OnCommand(WPARAM wp, LPARAM lp)
{
	// The active child's window procedure first, as MFC sends it.
	CMDIChildWnd *child = MDIGetActive();
	if(child != nullptr && AfxCallWndProc(child, child->m_hWnd, WM_COMMAND, wp, lp) != 0)
		return TRUE;
	if(CFrameWnd::OnCommand(wp, lp))
		return TRUE;
	// The window menu's child list, and system commands: DefFrameProc.
	if(lp == 0)
	{
		const UINT id = LOWORD(wp);
		if(id >= AfxFirstMdiChild || id >= 0xF000)
		{
			DefWindowProc(WM_COMMAND, wp, lp);
			return TRUE;
		}
	}
	return FALSE;
}

void CMDIFrameWnd::OnUpdateFrameTitle(BOOL addToTitle)
{
	if(!(GetStyle() & FWS_ADDTOTITLE))
		return;
	CMDIChildWnd *child = nullptr;
	CDocument *doc = GetActiveDocument();
	if(addToTitle && (child = MDIGetActive()) != nullptr && !(child->GetStyle() & WS_MAXIMIZE)
		&& (doc != nullptr || (doc = child->GetActiveDocument()) != nullptr))
	{
		UpdateFrameTitleForDocument(doc->GetTitle());
	} else
	{
		CString childTitle;
		LPCTSTR title = nullptr;
		if(child != nullptr && !(child->GetStyle() & WS_MAXIMIZE))
		{
			childTitle = child->GetTitle();
			if(!childTitle.IsEmpty())
				title = childTitle;
		}
		UpdateFrameTitleForDocument(title);
	}
}

void CMDIFrameWnd::OnUpdateFrameMenu(HMENU menuAlt)
{
	if(CMDIChildWnd *child = MDIGetActive())
	{
		child->OnUpdateFrameMenu(TRUE, child, menuAlt);
	} else
	{
		if(menuAlt == nullptr)
			menuAlt = m_hMenuDefault;
		::SendMessage(m_hWndMDIClient, WM_MDISETMENU, reinterpret_cast<WPARAM>(menuAlt), reinterpret_cast<LPARAM>(GetWindowMenuPopup(menuAlt)));
	}
}

CFrameWnd *CMDIFrameWnd::GetActiveFrame()
{
	CMDIChildWnd *child = MDIGetActive();
	return child != nullptr ? static_cast<CFrameWnd *>(child) : this;
}

CMDIChildWnd *CMDIFrameWnd::MDIGetActive(BOOL *maximized) const
{
	if(m_hWndMDIClient == nullptr)
	{
		if(maximized)
			*maximized = FALSE;
		return nullptr;
	}
	const HWND active = reinterpret_cast<HWND>(::SendMessage(m_hWndMDIClient, WM_MDIGETACTIVE, 0, reinterpret_cast<LPARAM>(maximized)));
	CWnd *wnd = CWnd::FromHandlePermanent(active);
	if(wnd == nullptr || !wnd->IsKindOf(RUNTIME_CLASS(CMDIChildWnd)))
	{
		if(maximized)
			*maximized = FALSE;
		return nullptr;
	}
	return static_cast<CMDIChildWnd *>(wnd);
}

void CMDIFrameWnd::MDIActivate(CWnd *child) { ::SendMessage(m_hWndMDIClient, WM_MDIACTIVATE, reinterpret_cast<WPARAM>(child->GetSafeHwnd()), 0); }
void CMDIFrameWnd::MDIMaximize(CWnd *child) { ::SendMessage(m_hWndMDIClient, WM_MDIMAXIMIZE, reinterpret_cast<WPARAM>(child->GetSafeHwnd()), 0); }
void CMDIFrameWnd::MDIRestore(CWnd *child) { ::SendMessage(m_hWndMDIClient, WM_MDIRESTORE, reinterpret_cast<WPARAM>(child->GetSafeHwnd()), 0); }
void CMDIFrameWnd::MDINext() { ::SendMessage(m_hWndMDIClient, WM_MDINEXT, 0, 0); }
void CMDIFrameWnd::MDIPrev() { ::SendMessage(m_hWndMDIClient, WM_MDINEXT, 0, 1); }
void CMDIFrameWnd::MDICascade() { ::SendMessage(m_hWndMDIClient, WM_MDICASCADE, 0, 0); }
void CMDIFrameWnd::MDICascade(int type) { ::SendMessage(m_hWndMDIClient, WM_MDICASCADE, static_cast<WPARAM>(type), 0); }
void CMDIFrameWnd::MDITile() { ::SendMessage(m_hWndMDIClient, WM_MDITILE, 0, 0); }
void CMDIFrameWnd::MDITile(int type) { ::SendMessage(m_hWndMDIClient, WM_MDITILE, static_cast<WPARAM>(type), 0); }
void CMDIFrameWnd::MDIIconArrange() { ::SendMessage(m_hWndMDIClient, WM_MDIICONARRANGE, 0, 0); }

CMenu *CMDIFrameWnd::MDISetMenu(CMenu *frameMenu, CMenu *windowMenu)
{
	const HMENU old = reinterpret_cast<HMENU>(::SendMessage(m_hWndMDIClient, WM_MDISETMENU, reinterpret_cast<WPARAM>(frameMenu->GetSafeHmenu()), reinterpret_cast<LPARAM>(windowMenu->GetSafeHmenu())));
	return CMenu::FromHandle(old);
}

void CMDIFrameWnd::OnUpdateMDIWindowCmd(CCmdUI *cmdUI)
{
	cmdUI->Enable(MDIGetActive() != nullptr);
}

BOOL CMDIFrameWnd::OnMDIWindowCmd(UINT id)
{
	switch(id)
	{
	case ID_WINDOW_ARRANGE: MDIIconArrange(); return TRUE;
	case ID_WINDOW_CASCADE: MDICascade(); return TRUE;
	case ID_WINDOW_TILE_HORZ: MDITile(MDITILE_HORIZONTAL); return TRUE;
	case ID_WINDOW_TILE_VERT: MDITile(MDITILE_VERTICAL); return TRUE;
	default: return FALSE;
	}
}

void CMDIFrameWnd::OnWindowNew()
{
	// Another frame on the active document.
	CMDIChildWnd *active = MDIGetActive();
	CDocument *doc = active ? active->GetActiveDocument() : nullptr;
	CDocTemplate *tmpl = doc ? doc->GetDocTemplate() : nullptr;
	if(tmpl == nullptr)
		return;
	if(CFrameWnd *frame = tmpl->CreateNewFrame(doc, active))
		tmpl->InitialUpdateFrame(frame, doc);
}

// ---- CMDIChildWnd -----------------------------------------------------------------------

BEGIN_MESSAGE_MAP(CMDIChildWnd, CFrameWnd)
	ON_WM_CREATE()
	ON_WM_MDIACTIVATE()
	ON_WM_MOUSEACTIVATE()
END_MESSAGE_MAP()

BOOL CMDIChildWnd::PreCreateWindow(CREATESTRUCT &cs)
{
	if(!CFrameWnd::PreCreateWindow(cs))
		return FALSE;
	cs.style |= WS_CHILD;
	return TRUE;
}

BOOL CMDIChildWnd::Create(LPCTSTR className, LPCTSTR windowName, DWORD style, const RECT &rect, CMDIFrameWnd *parent, CCreateContext *context)
{
	if(parent == nullptr)
	{
		CWnd *main = AfxGetMainWnd();
		parent = main && main->IsKindOf(RUNTIME_CLASS(CMDIFrameWnd)) ? static_cast<CMDIFrameWnd *>(main) : nullptr;
	}
	if(parent == nullptr || parent->m_hWndMDIClient == nullptr)
	{
		PostNcDestroy();
		return FALSE;
	}
	// Default processing for WM_MDIACTIVATE before the first one arrives.
	parent->RecalcLayout();
	CREATESTRUCT cs{};
	cs.style = static_cast<LONG>(style | WS_CHILD);
	cs.dwExStyle = WS_EX_MDICHILD;
	cs.hInstance = AfxGetInstanceHandle();
	cs.hwndParent = parent->m_hWnd;
	cs.lpszClass = className;
	cs.lpszName = windowName;
	cs.x = rect.left;
	cs.y = rect.top;
	cs.cx = rect.right - rect.left;
	cs.cy = rect.bottom - rect.top;
	cs.lpCreateParams = context;
	if(!PreCreateWindow(cs))
	{
		PostNcDestroy();
		return FALSE;
	}
	m_strTitle = cs.lpszName ? cs.lpszName : L"";
	MDICREATESTRUCT mcs{cs.lpszClass, cs.lpszName, cs.hInstance, cs.x, cs.y, cs.cx, cs.cy,
		static_cast<DWORD>(cs.style) & ~(WS_MAXIMIZE | WS_VISIBLE), reinterpret_cast<LPARAM>(context)};
	// MFC's CBT hook attaches the object when user32 creates the window.
	linopenmpt::mfc::HookWindowCreate(this);
	const HWND hwnd = reinterpret_cast<HWND>(::SendMessage(parent->m_hWndMDIClient, WM_MDICREATE, 0, reinterpret_cast<LPARAM>(&mcs)));
	if(!linopenmpt::mfc::UnhookWindowCreate())
		PostNcDestroy();
	if(hwnd == nullptr)
		return FALSE;
	// Created hidden; showing activates it.
	if(cs.style & WS_VISIBLE)
	{
		::SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
		if(cs.style & WS_MINIMIZE)
			ShowWindow(SW_SHOWMINIMIZED);
		else if(cs.style & WS_MAXIMIZE)
			ShowWindow(SW_SHOWMAXIMIZED);
		else
			ShowWindow(SW_SHOWNORMAL);
		parent->MDIActivate(this);
		::SendMessage(parent->m_hWndMDIClient, WM_MDIREFRESHMENU, 0, 0);
	}
	return TRUE;
}

BOOL CMDIChildWnd::LoadFrame(UINT resourceID, DWORD style, CWnd *parent, CCreateContext *context)
{
	m_nIDHelp = resourceID;
	CMDIFrameWnd *frame = parent && parent->IsKindOf(RUNTIME_CLASS(CMDIFrameWnd)) ? static_cast<CMDIFrameWnd *>(parent) : nullptr;
	// The shared menu of the document template, set when the child activates.
	if(context != nullptr && context->m_pNewDocTemplate != nullptr)
	{
		if(auto *multi = dynamic_cast<CMultiDocTemplate *>(context->m_pNewDocTemplate))
		{
			m_hMenuShared = multi->m_hMenuShared;
			if(m_hAccelTable == nullptr)
				m_hAccelTable = multi->m_hAccelTable;
		}
	}
	const CString title = m_strTitle;
	if(!Create(FrameClass(resourceID), title, style, rectDefault, frame, context))
		return FALSE;
	return TRUE;
}

int CMDIChildWnd::OnCreate(LPCREATESTRUCT cs)
{
	// The context comes through the MDICREATESTRUCT.
	const auto *mcs = static_cast<const MDICREATESTRUCT *>(cs->lpCreateParams);
	return OnCreateHelper(cs, mcs ? reinterpret_cast<CCreateContext *>(mcs->lParam) : nullptr);
}

LRESULT CMDIChildWnd::DefWindowProc(UINT msg, WPARAM wp, LPARAM lp)
{
	if(m_pfnSuper != nullptr)
		return ::CallWindowProc(m_pfnSuper, m_hWnd, msg, wp, lp);
	return ::DefMDIChildProc(m_hWnd, msg, wp, lp);
}

BOOL CMDIChildWnd::DestroyWindow()
{
	if(m_hWnd == nullptr)
		return FALSE;
	CMDIFrameWnd *frame = GetMDIFrame();
	if(frame != nullptr && frame->m_hWndMDIClient != nullptr)
	{
		MDIDestroy();
		return TRUE;
	}
	return CWnd::DestroyWindow();
}

BOOL CMDIChildWnd::PreTranslateMessage(MSG *msg)
{
	if(CWnd::PreTranslateMessage(msg))
		return TRUE;
	// The document's accelerators work for the MDI frame, which has the menus.
	if(msg->message >= WM_KEYFIRST && msg->message <= WM_KEYLAST)
	{
		HACCEL accel = GetDefaultAccelerator();
		CMDIFrameWnd *frame = GetMDIFrame();
		return accel != nullptr && frame != nullptr && ::TranslateAccelerator(frame->m_hWnd, accel, msg);
	}
	return FALSE;
}

void CMDIChildWnd::ActivateFrame(int cmdShow)
{
	CMDIFrameWnd *frame = GetMDIFrame();
	if(frame == nullptr)
		return;
	BOOL maximized = FALSE;
	CMDIChildWnd *active = frame->MDIGetActive(&maximized);
	if(cmdShow == -1)
	{
		// Showing for the first time: maximized if the active one is.
		if(!IsWindowVisible())
			cmdShow = (active != nullptr && maximized) ? SW_SHOWMAXIMIZED : SW_SHOWNORMAL;
		else if(IsIconic())
			cmdShow = SW_RESTORE;
	}
	if(cmdShow != -1)
		ShowWindow(cmdShow);
	frame->MDIActivate(this);
	::SendMessage(frame->m_hWndMDIClient, WM_MDIREFRESHMENU, 0, 0);
	if(CView *view = GetActiveView())
	{
		if(::GetFocus() != view->m_hWnd && !::IsChild(view->m_hWnd, ::GetFocus()))
			view->SetFocus();
	}
}

void CMDIChildWnd::OnUpdateFrameTitle(BOOL addToTitle)
{
	if(CMDIFrameWnd *frame = GetMDIFrame())
		frame->OnUpdateFrameTitle(addToTitle);
	if(!(GetStyle() & FWS_ADDTOTITLE))
		return;
	if(addToTitle)
	{
		CDocument *doc = GetActiveDocument();
		CString text = doc != nullptr ? doc->GetTitle() : m_strTitle;
		if(m_nWindow > 0)
		{
			CString n;
			n.Format(_T(":%d"), m_nWindow);
			text += n;
		}
		AfxSetWindowText(m_hWnd, text);
	}
}

void CMDIChildWnd::OnUpdateFrameMenu(BOOL activate, CWnd *activateWnd, HMENU menuAlt)
{
	CMDIFrameWnd *frame = GetMDIFrame();
	if(frame == nullptr)
		return;
	if(menuAlt == nullptr && activate)
	{
		if(CDocument *doc = GetActiveDocument())
			menuAlt = doc->GetDefaultMenu();
	}
	if(menuAlt == nullptr && activate)
		menuAlt = m_hMenuShared;
	if(menuAlt != nullptr && activate)
	{
		::SendMessage(frame->m_hWndMDIClient, WM_MDISETMENU, reinterpret_cast<WPARAM>(menuAlt), reinterpret_cast<LPARAM>(frame->GetWindowMenuPopup(menuAlt)));
	} else if(!activate && activateWnd == nullptr)
	{
		// The last child goes: the frame's own menu again.
		::SendMessage(frame->m_hWndMDIClient, WM_MDISETMENU, reinterpret_cast<WPARAM>(frame->m_hMenuDefault), reinterpret_cast<LPARAM>(frame->GetWindowMenuPopup(frame->m_hMenuDefault)));
	} else
	{
		::SendMessage(frame->m_hWndMDIClient, WM_MDIREFRESHMENU, 0, 0);
	}
}

void CMDIChildWnd::OnMDIActivate(BOOL activate, CWnd *activateWnd, CWnd *)
{
	CView *view = GetActiveView();
	if(!activate && view != nullptr)
		view->OnActivateView(FALSE, view, view);
	// The menu: this child's, or the frame's own when none is left.
	if(activate || activateWnd == nullptr || !activateWnd->IsKindOf(RUNTIME_CLASS(CMDIChildWnd)))
		OnUpdateFrameMenu(activate, activateWnd, nullptr);
	if(activate && view != nullptr)
		view->OnActivateView(TRUE, view, view);
	if(CMDIFrameWnd *frame = GetMDIFrame())
	{
		frame->OnUpdateFrameTitle(TRUE);
		::DrawMenuBar(frame->m_hWnd);
	}
}

int CMDIChildWnd::OnMouseActivate(CWnd *topParent, UINT hitTest, UINT message)
{
	const int result = CFrameWnd::OnMouseActivate(topParent, hitTest, message);
	if(result == MA_NOACTIVATE || result == MA_NOACTIVATEANDEAT)
		return result;
	CMDIFrameWnd *frame = GetMDIFrame();
	if(frame != nullptr && frame->MDIGetActive() != this)
		MDIActivate();
	return result;
}

CMDIFrameWnd *CMDIChildWnd::GetMDIFrame()
{
	// The MDI client's parent.
	const HWND client = ::GetParent(m_hWnd);
	CWnd *frame = client ? CWnd::FromHandlePermanent(::GetParent(client)) : nullptr;
	return frame && frame->IsKindOf(RUNTIME_CLASS(CMDIFrameWnd)) ? static_cast<CMDIFrameWnd *>(frame) : nullptr;
}

void CMDIChildWnd::MDIActivate()
{
	if(CMDIFrameWnd *frame = GetMDIFrame())
		frame->MDIActivate(this);
}

void CMDIChildWnd::MDIDestroy()
{
	if(CMDIFrameWnd *frame = GetMDIFrame())
		::SendMessage(frame->m_hWndMDIClient, WM_MDIDESTROY, reinterpret_cast<WPARAM>(m_hWnd), 0);
	else
		CWnd::DestroyWindow();
}

void CMDIChildWnd::MDIMaximize()
{
	if(CMDIFrameWnd *frame = GetMDIFrame())
		frame->MDIMaximize(this);
}

void CMDIChildWnd::MDIRestore()
{
	if(CMDIFrameWnd *frame = GetMDIFrame())
		frame->MDIRestore(this);
}

// Docking and bar state come with the control bars (step 3f, part 2).
