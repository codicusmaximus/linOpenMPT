// linOpenMPT MFC layer: views and documents, as MFC's viewcore.cpp,
// viewscrl.cpp, viewform.cpp, doccore.cpp, doctempl.cpp, docmulti.cpp and
// docmgr.cpp have them.
//
// - CView: a child of a frame (or a splitter) that shows a document; it
//   adds itself to the document of the CCreateContext it is created with,
//   paints through OnPrepareDC and OnDraw, and becomes the frame's active
//   view when clicked;
// - CScrollView: the window scroll bars sized from SetScrollSizes (MM_TEXT
//   only: upstream uses nothing else), the viewport origin in OnPrepareDC;
// - CFormView: a modeless dialog template as the view window;
// - CDocument: its views, its title and path, modified state and the
//   "save changes" prompt, closing (which closes its frames);
// - CDocTemplate and CMultiDocTemplate: the document strings, shared menu
//   and accelerators of a resource id, creating a document with its frame
//   and view, untitled names;
// - CDocManager and CWinApp's document functions on top of it.
//
// Views and documents delete themselves as in MFC: a view when its window
// goes, a document (m_bAutoDelete) when its last view goes.
#include "MfcInternal.h"

#include <algorithm>
#include <climits>
#include <filesystem>

namespace
{

// POSITION for the vectors below: the 1-based index, so null is the end.
POSITION ToPosition(std::size_t index) { return reinterpret_cast<POSITION>(index + 1); }

template <typename T>
T *Next(const std::vector<T *> &list, POSITION &pos)
{
	const auto index = reinterpret_cast<std::size_t>(pos) - 1;
	T *item = index < list.size() ? list[index] : nullptr;
	pos = index + 1 < list.size() ? ToPosition(index + 1) : nullptr;
	return item;
}

template <typename T>
POSITION First(const std::vector<T *> &list)
{
	return list.empty() ? nullptr : ToPosition(0);
}

CString FileTitle(const CString &path)
{
	const int slash = std::max(path.ReverseFind(_T('/')), path.ReverseFind(_T('\\')));
	return slash >= 0 ? path.Mid(slash + 1) : path;
}

} // namespace

// ---- CView -----------------------------------------------------------------------

BEGIN_MESSAGE_MAP(CView, CWnd)
	ON_WM_CREATE()
	ON_WM_DESTROY()
	ON_WM_PAINT()
	ON_WM_MOUSEACTIVATE()
	ON_MESSAGE(WM_INITIALUPDATE, &CView::HandleInitialUpdate)
END_MESSAGE_MAP()

CView::~CView()
{
	if(m_pDocument != nullptr)
		m_pDocument->RemoveView(this);
}

BOOL CView::PreCreateWindow(CREATESTRUCT &cs)
{
	if(cs.lpszClass == nullptr)
		cs.lpszClass = AfxRegisterWndClass(CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW, ::LoadCursor(nullptr, IDC_ARROW), reinterpret_cast<HBRUSH>(static_cast<INT_PTR>(COLOR_WINDOW + 1)), nullptr);
	// The border of a view is a 3D one.
	if(cs.style & WS_BORDER)
	{
		cs.dwExStyle |= WS_EX_CLIENTEDGE;
		cs.style &= ~WS_BORDER;
	}
	return TRUE;
}

int CView::OnCreate(LPCREATESTRUCT cs)
{
	if(CWnd::OnCreate(cs) == -1)
		return -1;
	auto *context = static_cast<CCreateContext *>(cs->lpCreateParams);
	if(context != nullptr && context->m_pCurrentDoc != nullptr)
		context->m_pCurrentDoc->AddView(this);
	return 0;
}

void CView::OnDestroy()
{
	CFrameWnd *frame = GetParentFrame();
	if(frame != nullptr && frame->GetActiveView() == this)
		frame->SetActiveView(nullptr); // no active view while it goes
	CWnd::OnDestroy();
}

void CView::PostNcDestroy()
{
	delete this;
}

void CView::OnPaint()
{
	CPaintDC dc(this);
	OnPrepareDC(&dc);
	OnDraw(&dc);
}

int CView::OnMouseActivate(CWnd *topParent, UINT hitTest, UINT message)
{
	const int result = CWnd::OnMouseActivate(topParent, hitTest, message);
	if(result == MA_NOACTIVATE || result == MA_NOACTIVATEANDEAT)
		return result;
	if(CFrameWnd *frame = GetParentFrame())
	{
		const HWND focus = ::GetFocus();
		if(frame->GetActiveView() == this && m_hWnd != focus && !::IsChild(m_hWnd, focus))
			OnActivateView(TRUE, this, this); // the focus comes back to the view
		else
			frame->SetActiveView(this);
	}
	return result;
}

void CView::OnActivateView(BOOL activate, CView *, CView *)
{
	if(activate && IsTopParentActive())
		SetFocus();
}

LRESULT CView::HandleInitialUpdate(WPARAM, LPARAM)
{
	OnInitialUpdate();
	return 0;
}

void CView::OnInitialUpdate()
{
	OnUpdate(nullptr, 0, nullptr);
}

void CView::OnUpdate(CView *, LPARAM, CObject *)
{
	Invalidate(TRUE);
}

BOOL CView::OnCmdMsg(UINT id, int code, void *extra, void *handlerInfo)
{
	if(CWnd::OnCmdMsg(id, code, extra, handlerInfo))
		return TRUE;
	return m_pDocument != nullptr && m_pDocument->OnCmdMsg(id, code, extra, handlerInfo);
}

CSplitterWnd *CView::GetParentSplitter(const CWnd *wnd, BOOL anyState)
{
	if(wnd == nullptr)
		return nullptr;
	auto *splitter = dynamic_cast<CSplitterWnd *>(CWnd::FromHandlePermanent(::GetParent(wnd->m_hWnd)));
	if(splitter == nullptr)
		return nullptr;
	if(!anyState)
	{
		// Splitters in minimized windows do not count.
		for(HWND parent = ::GetParent(wnd->m_hWnd); parent != nullptr; parent = ::GetParent(parent))
		{
			if(::IsIconic(parent))
				return nullptr;
		}
	}
	return splitter;
}

// ---- CScrollView -----------------------------------------------------------------

const SIZE CScrollView::sizeDefault = {0, 0};

BEGIN_MESSAGE_MAP(CScrollView, CView)
	ON_WM_SIZE()
	ON_WM_HSCROLL()
	ON_WM_VSCROLL()
	ON_WM_MOUSEWHEEL()
END_MESSAGE_MAP()

void CScrollView::SetScrollSizes(int mapMode, SIZE total, const SIZE &page, const SIZE &line)
{
	const int oldMapMode = m_nMapMode;
	m_nMapMode = mapMode;
	m_totalLog = total;
	// MM_TEXT: logical and device units are the same.
	m_totalDev = CSize(std::abs(total.cx), std::abs(total.cy));
	m_pageDev = CSize(std::abs(page.cx), std::abs(page.cy));
	m_lineDev = CSize(std::abs(line.cx), std::abs(line.cy));
	if(m_pageDev.cx == 0)
		m_pageDev.cx = m_totalDev.cx / 10;
	if(m_pageDev.cy == 0)
		m_pageDev.cy = m_totalDev.cy / 10;
	if(m_lineDev.cx == 0)
		m_lineDev.cx = m_pageDev.cx / 10;
	if(m_lineDev.cy == 0)
		m_lineDev.cy = m_pageDev.cy / 10;
	if(m_hWnd != nullptr)
	{
		UpdateBars();
		if(oldMapMode != m_nMapMode)
			Invalidate(TRUE);
	}
}

void CScrollView::GetScrollBarSizes(CSize &sizeSb)
{
	sizeSb.cx = sizeSb.cy = 0;
	const DWORD style = GetStyle();
	if(GetScrollBarCtrl(SB_VERT) == nullptr)
	{
		sizeSb.cx = ::GetSystemMetrics(SM_CXVSCROLL);
		if(style & WS_BORDER)
			sizeSb.cx -= ::GetSystemMetrics(SM_CXBORDER);
	}
	if(GetScrollBarCtrl(SB_HORZ) == nullptr)
	{
		sizeSb.cy = ::GetSystemMetrics(SM_CYHSCROLL);
		if(style & WS_BORDER)
			sizeSb.cy -= ::GetSystemMetrics(SM_CYBORDER);
	}
}

BOOL CScrollView::GetTrueClientSize(CSize &size, CSize &sizeSb)
{
	CRect rect;
	GetClientRect(&rect);
	size.cx = rect.right;
	size.cy = rect.bottom;
	const DWORD style = GetStyle();
	GetScrollBarSizes(sizeSb);
	// The client area without the bars that are shown now.
	if(sizeSb.cx != 0 && (style & WS_VSCROLL))
		size.cx += sizeSb.cx;
	if(sizeSb.cy != 0 && (style & WS_HSCROLL))
		size.cy += sizeSb.cy;
	return size.cx > sizeSb.cx && size.cy > sizeSb.cy;
}

void CScrollView::GetScrollBarState(CSize sizeClient, CSize &needSb, CSize &sizeRange, CPoint &ptMove, BOOL insideClient)
{
	CSize sizeSb;
	GetScrollBarSizes(sizeSb);
	sizeRange = CSize(m_totalDev.cx - sizeClient.cx, m_totalDev.cy - sizeClient.cy);
	ptMove = GetDeviceScrollPosition();
	BOOL needH = sizeRange.cx > 0;
	if(!needH)
		ptMove.x = 0;
	else if(insideClient)
		sizeRange.cy += sizeSb.cy;
	const BOOL needV = sizeRange.cy > 0;
	if(!needV)
		ptMove.y = 0;
	else if(insideClient)
		sizeRange.cx += sizeSb.cx;
	if(needV && !needH && sizeRange.cx > 0)
	{
		// The vertical bar takes enough room to need the horizontal one.
		needH = TRUE;
		sizeRange.cy += sizeSb.cy;
	}
	if(sizeRange.cx > 0 && ptMove.x >= sizeRange.cx)
		ptMove.x = sizeRange.cx;
	if(sizeRange.cy > 0 && ptMove.y >= sizeRange.cy)
		ptMove.y = sizeRange.cy;
	needSb.cx = needH;
	needSb.cy = needV;
}

void CScrollView::UpdateBars()
{
	// Showing or hiding a bar resizes the client area, and calls this again.
	if(m_bInsideUpdate)
		return;
	m_bInsideUpdate = TRUE;
	CSize sizeClient, sizeSb;
	if(!GetTrueClientSize(sizeClient, sizeSb))
	{
		// No room for the bars.
		CRect rect;
		GetClientRect(&rect);
		if(rect.right > 0 && rect.bottom > 0)
			EnableScrollBarCtrl(SB_BOTH, FALSE);
		m_bInsideUpdate = FALSE;
		return;
	}
	CSize sizeRange, needSb;
	CPoint ptMove;
	GetScrollBarState(sizeClient, needSb, sizeRange, ptMove, TRUE);
	if(needSb.cx)
		sizeClient.cy -= sizeSb.cy;
	if(needSb.cy)
		sizeClient.cx -= sizeSb.cx;
	ScrollToDevicePosition(ptMove);

	SCROLLINFO info{};
	info.cbSize = sizeof(info);
	info.fMask = SIF_PAGE | SIF_RANGE;
	info.nMin = 0;
	EnableScrollBarCtrl(SB_HORZ, needSb.cx);
	if(needSb.cx)
	{
		info.nPage = static_cast<UINT>(sizeClient.cx);
		info.nMax = m_totalDev.cx - 1;
		SetScrollInfo(SB_HORZ, &info, TRUE);
	}
	EnableScrollBarCtrl(SB_VERT, needSb.cy);
	if(needSb.cy)
	{
		info.nPage = static_cast<UINT>(sizeClient.cy);
		info.nMax = m_totalDev.cy - 1;
		SetScrollInfo(SB_VERT, &info, TRUE);
	}
	m_bInsideUpdate = FALSE;
}

void CScrollView::OnSize(UINT type, int cx, int cy)
{
	CView::OnSize(type, cx, cy);
	UpdateBars();
}

int CScrollView::GetScrollPos(int bar) const { return CWnd::GetScrollPos(bar); }
int CScrollView::SetScrollPos(int bar, int pos, BOOL redraw) { return CWnd::SetScrollPos(bar, pos, redraw); }

CPoint CScrollView::GetDeviceScrollPosition() const
{
	return CPoint(GetScrollPos(SB_HORZ), GetScrollPos(SB_VERT));
}

CPoint CScrollView::GetScrollPosition() const
{
	return GetDeviceScrollPosition();
}

void CScrollView::GetDeviceScrollSizes(int &mapMode, SIZE &total, SIZE &page, SIZE &line) const
{
	mapMode = m_nMapMode;
	total = m_totalDev;
	page = m_pageDev;
	line = m_lineDev;
}

void CScrollView::ScrollToDevicePosition(POINT pt)
{
	// Not through OnScrollBy: the position is not limited to the range.
	const int xOrig = GetScrollPos(SB_HORZ);
	SetScrollPos(SB_HORZ, pt.x);
	const int yOrig = GetScrollPos(SB_VERT);
	SetScrollPos(SB_VERT, pt.y);
	// What the bars took (a hidden bar keeps no position) is what scrolls.
	const int dx = xOrig - GetScrollPos(SB_HORZ), dy = yOrig - GetScrollPos(SB_VERT);
	if(dx != 0 || dy != 0)
		ScrollWindow(dx, dy);
}

void CScrollView::ScrollToPosition(POINT pt)
{
	const int xMax = GetScrollLimit(SB_HORZ);
	pt.x = std::clamp(static_cast<int>(pt.x), 0, std::max(xMax, 0));
	const int yMax = GetScrollLimit(SB_VERT);
	pt.y = std::clamp(static_cast<int>(pt.y), 0, std::max(yMax, 0));
	ScrollToDevicePosition(pt);
}

void CScrollView::OnPrepareDC(CDC *dc, CPrintInfo *info)
{
	dc->SetMapMode(m_nMapMode > 0 ? m_nMapMode : MM_TEXT);
	const CPoint pos = GetDeviceScrollPosition();
	dc->SetViewportOrg(-pos.x, -pos.y);
	CView::OnPrepareDC(dc, info);
}

void CScrollView::OnHScroll(UINT code, UINT pos, CScrollBar *scrollBar)
{
	if(scrollBar != GetScrollBarCtrl(SB_HORZ))
		return; // a scroll bar control of the view's own
	if(code == SB_THUMBTRACK || code == SB_THUMBPOSITION)
	{
		SCROLLINFO si{sizeof(si), SIF_TRACKPOS};
		if(::GetScrollInfo(m_hWnd, SB_HORZ, &si))
			pos = static_cast<UINT>(si.nTrackPos);
	}
	OnScroll(MAKEWORD(code, 0xFF), pos);
}

void CScrollView::OnVScroll(UINT code, UINT pos, CScrollBar *scrollBar)
{
	if(scrollBar != GetScrollBarCtrl(SB_VERT))
		return;
	if(code == SB_THUMBTRACK || code == SB_THUMBPOSITION)
	{
		SCROLLINFO si{sizeof(si), SIF_TRACKPOS};
		if(::GetScrollInfo(m_hWnd, SB_VERT, &si))
			pos = static_cast<UINT>(si.nTrackPos);
	}
	OnScroll(MAKEWORD(0xFF, code), pos);
}

BOOL CScrollView::OnScroll(UINT scrollCode, UINT pos, BOOL doScroll)
{
	auto step = [&](int code, int current, int line, int page) {
		switch(code)
		{
		case SB_TOP: return 0;
		case SB_BOTTOM: return INT_MAX;
		case SB_LINEUP: return current - line;
		case SB_LINEDOWN: return current + line;
		case SB_PAGEUP: return current - page;
		case SB_PAGEDOWN: return current + page;
		case SB_THUMBTRACK: return static_cast<int>(pos);
		default: return current;
		}
	};
	const int xOrig = GetScrollPos(SB_HORZ), yOrig = GetScrollPos(SB_VERT);
	const int x = step(LOBYTE(scrollCode), xOrig, m_lineDev.cx, m_pageDev.cx);
	const int y = step(HIBYTE(scrollCode), yOrig, m_lineDev.cy, m_pageDev.cy);
	// INT_MAX - xOrig cannot overflow; OnScrollBy limits it to the range.
	const BOOL result = OnScrollBy(CSize(x == INT_MAX ? INT_MAX - std::max(xOrig, 0) : x - xOrig, y == INT_MAX ? INT_MAX - std::max(yOrig, 0) : y - yOrig), doScroll);
	if(result && doScroll)
		UpdateWindow();
	return result;
}

BOOL CScrollView::OnScrollBy(CSize sizeScroll, BOOL doScroll)
{
	const DWORD style = GetStyle();
	// No scrolling without a bar.
	if(!(style & WS_VSCROLL))
		sizeScroll.cy = 0;
	if(!(style & WS_HSCROLL))
		sizeScroll.cx = 0;
	const int xOrig = GetScrollPos(SB_HORZ);
	const int xMax = GetScrollLimit(SB_HORZ);
	const int x = static_cast<int>(std::clamp<long long>(static_cast<long long>(xOrig) + sizeScroll.cx, 0, std::max(xMax, 0)));
	const int yOrig = GetScrollPos(SB_VERT);
	const int yMax = GetScrollLimit(SB_VERT);
	const int y = static_cast<int>(std::clamp<long long>(static_cast<long long>(yOrig) + sizeScroll.cy, 0, std::max(yMax, 0)));
	if(x == xOrig && y == yOrig)
		return FALSE;
	if(doScroll)
	{
		ScrollWindow(-(x - xOrig), -(y - yOrig));
		if(x != xOrig)
			SetScrollPos(SB_HORZ, x);
		if(y != yOrig)
			SetScrollPos(SB_VERT, y);
	}
	return TRUE;
}

BOOL CScrollView::OnMouseWheel(UINT flags, short delta, CPoint point)
{
	// Only plain scrolling, and not in a splitter (MFC leaves it to the
	// splitter, which does not scroll panes without shared scroll bars).
	if(flags & (MK_SHIFT | MK_CONTROL))
		return FALSE;
	if(GetParentSplitter(this, TRUE) != nullptr)
		return FALSE;
	return DoMouseWheel(flags, delta, point);
}

BOOL CScrollView::DoMouseWheel(UINT, short delta, CPoint)
{
	const DWORD style = GetStyle();
	const bool hasVert = (style & WS_VSCROLL) != 0, hasHorz = (style & WS_HSCROLL) != 0;
	if(!hasVert && !hasHorz)
		return FALSE;
	UINT lines = 3;
	::SystemParametersInfo(SPI_GETWHEELSCROLLLINES, 0, &lines, 0);
	const int toScroll = ::MulDiv(-delta, static_cast<int>(lines), WHEEL_DELTA);
	auto displacement = [&](int page, int line) {
		if(toScroll == -1 || lines == WHEEL_PAGESCROLL)
			return delta > 0 ? -page : page;
		return std::min(toScroll * line, page);
	};
	BOOL result;
	if(hasVert)
		result = OnScrollBy(CSize(0, displacement(m_pageDev.cy, m_lineDev.cy)), TRUE);
	else
		result = OnScrollBy(CSize(displacement(m_pageDev.cx, m_lineDev.cx), 0), TRUE);
	if(result)
		UpdateWindow();
	return result;
}

// ---- CFormView -------------------------------------------------------------------

BEGIN_MESSAGE_MAP(CFormView, CScrollView)
	ON_WM_CREATE()
	ON_WM_SETFOCUS()
END_MESSAGE_MAP()

BOOL CFormView::Create(LPCTSTR, LPCTSTR, DWORD style, const RECT &rect, CWnd *parent, UINT id, CCreateContext *context)
{
	// The context reaches OnCreate through a member: CreateDialog has no
	// parameter for it.
	m_pCreateContext = context;
	if(!CreateDlg(m_lpszTemplateName, parent))
		return FALSE;
	m_pCreateContext = nullptr;
	// The template's style, but the border as asked.
	ModifyStyle(WS_BORDER | WS_CAPTION, style & (WS_BORDER | WS_CAPTION));
	ModifyStyleEx(WS_EX_CLIENTEDGE, (style & WS_BORDER) ? WS_EX_CLIENTEDGE : 0, SWP_FRAMECHANGED);
	if(GetExStyle() & WS_EX_CLIENTEDGE)
		ModifyStyle(WS_BORDER, 0, SWP_FRAMECHANGED);
	::SetWindowLongPtr(m_hWnd, GWLP_ID, static_cast<LONG_PTR>(id));
	CRect templateRect;
	GetWindowRect(&templateRect);
	SetScrollSizes(MM_TEXT, templateRect.Size());
	ExecuteDlgInit(m_lpszTemplateName);
	SetWindowPos(nullptr, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, SWP_NOZORDER | SWP_NOACTIVATE);
	if(style & WS_VISIBLE)
		ShowWindow(SW_NORMAL);
	return TRUE;
}

int CFormView::OnCreate(LPCREATESTRUCT cs)
{
	cs->lpCreateParams = m_pCreateContext;
	return CScrollView::OnCreate(cs);
}

void CFormView::OnInitialUpdate()
{
	UpdateData(FALSE);
	CScrollView::OnInitialUpdate();
}

BOOL CFormView::SaveFocusControl()
{
	const HWND focus = ::GetFocus();
	if(focus != nullptr && ::IsChild(m_hWnd, focus))
	{
		m_hWndFocus = focus;
		return TRUE;
	}
	return FALSE;
}

void CFormView::OnActivateView(BOOL activate, CView *activateView, CView *deactiveView)
{
	if(SaveFocusControl())
		return; // the focus stays on the control
	CScrollView::OnActivateView(activate, activateView, deactiveView);
}

void CFormView::OnActivateFrame(UINT state, CFrameWnd *frame)
{
	if(state == WA_INACTIVE)
		SaveFocusControl();
	CScrollView::OnActivateFrame(state, frame);
}

void CFormView::OnSetFocus(CWnd *oldWnd)
{
	if(!::IsWindow(m_hWndFocus) || !::IsChild(m_hWnd, m_hWndFocus))
	{
		m_hWndFocus = nullptr;
		// The first control in the tab order.
		if(CWnd *first = GetNextDlgTabItem(nullptr))
			m_hWndFocus = first->m_hWnd;
	}
	if(m_hWndFocus != nullptr)
		::SetFocus(m_hWndFocus);
	else
		CScrollView::OnSetFocus(oldWnd);
}

BOOL CFormView::PreTranslateMessage(MSG *msg)
{
	if(CScrollView::PreTranslateMessage(msg))
		return TRUE;
	// The frames first: IsDialogMessage would take their accelerators.
	for(CFrameWnd *frame = GetParentFrame(); frame != nullptr; frame = frame->GetParentFrame())
	{
		if(frame->PreTranslateMessage(msg))
			return TRUE;
	}
	if(::GetWindow(m_hWnd, GW_CHILD) == nullptr)
		return FALSE;
	return PreTranslateInput(msg);
}

// ---- CDocument -------------------------------------------------------------------

BEGIN_MESSAGE_MAP(CDocument, CCmdTarget)
	ON_COMMAND(ID_FILE_CLOSE, &CDocument::OnFileClose)
	ON_COMMAND(ID_FILE_SAVE, &CDocument::OnFileSave)
	ON_COMMAND(ID_FILE_SAVE_AS, &CDocument::OnFileSaveAs)
END_MESSAGE_MAP()

CDocument::~CDocument()
{
	// Views that are left keep no pointer to it.
	for(CView *view : m_viewList)
		view->m_pDocument = nullptr;
	m_viewList.clear();
	if(m_pDocTemplate != nullptr)
		m_pDocTemplate->RemoveDocument(this);
}

BOOL CDocument::OnCmdMsg(UINT id, int code, void *extra, void *handlerInfo)
{
	if(CCmdTarget::OnCmdMsg(id, code, extra, handlerInfo))
		return TRUE;
	return m_pDocTemplate != nullptr && m_pDocTemplate->OnCmdMsg(id, code, extra, handlerInfo);
}

void CDocument::SetTitle(LPCTSTR title)
{
	m_strTitle = title;
	UpdateFrameCounts();
}

void CDocument::SetPathName(LPCTSTR path, BOOL addToMRU)
{
	m_strPathName = path;
	SetTitle(FileTitle(m_strPathName));
	if(addToMRU)
	{
		if(CWinApp *app = AfxGetApp())
			app->AddToRecentFileList(m_strPathName);
	}
}

BOOL CDocument::OnNewDocument()
{
	DeleteContents();
	m_strPathName.Empty();
	SetModifiedFlag(FALSE);
	OnDocumentEvent(onAfterNewDocument);
	return TRUE;
}

BOOL CDocument::OnOpenDocument(LPCTSTR)
{
	// MFC serializes the file here (CArchive); documents that load files
	// override this, as upstream's does.
	DeleteContents();
	SetModifiedFlag(FALSE);
	return TRUE;
}

BOOL CDocument::OnSaveDocument(LPCTSTR)
{
	SetModifiedFlag(FALSE);
	return TRUE;
}

void CDocument::OnCloseDocument()
{
	// The document stays while its frames close.
	const BOOL autoDelete = m_bAutoDelete;
	m_bAutoDelete = FALSE;
	while(!m_viewList.empty())
	{
		CView *view = m_viewList.front();
		CFrameWnd *frame = view->GetParentFrame();
		if(frame != nullptr)
		{
			PreCloseFrame(frame);
			frame->DestroyWindow(); // the view goes with it
		} else
		{
			view->DestroyWindow();
		}
		// A view without a window of its own would stay in the list.
		if(!m_viewList.empty() && m_viewList.front() == view)
			RemoveView(view);
	}
	m_bAutoDelete = autoDelete;
	DeleteContents();
	OnDocumentEvent(onAfterCloseDocument);
	if(m_bAutoDelete)
		delete this;
}

BOOL CDocument::SaveModified()
{
	if(!IsModified())
		return TRUE;
	CString name = m_strPathName.IsEmpty() ? m_strTitle : FileTitle(m_strPathName);
	if(name.IsEmpty())
		name = _T("Untitled");
	CString prompt;
	prompt.Format(_T("Save changes to %s?"), name.GetString());
	switch(AfxMessageBox(prompt, MB_YESNOCANCEL | MB_ICONEXCLAMATION))
	{
	case IDCANCEL:
		return FALSE;
	case IDYES:
		return DoFileSave();
	default:
		return TRUE;
	}
}

BOOL CDocument::DoFileSave()
{
	// MFC also asks for a new name when the file is read-only.
	bool readOnly = false;
	if(!m_strPathName.IsEmpty())
	{
		std::error_code ec;
		const auto status = std::filesystem::status(std::filesystem::path(std::wstring(m_strPathName.GetString())), ec);
		readOnly = !ec && std::filesystem::exists(status) && (status.permissions() & std::filesystem::perms::owner_write) == std::filesystem::perms::none;
	}
	if(m_strPathName.IsEmpty() || readOnly)
		return DoSave(nullptr);
	return DoSave(m_strPathName);
}

BOOL CDocument::DoSave(LPCTSTR path, BOOL replace)
{
	CString newName = path;
	if(newName.IsEmpty())
	{
		newName = m_strPathName;
		if(replace && newName.IsEmpty())
		{
			newName = m_strTitle;
			// The template's extension for an untitled document.
			CString ext;
			if(m_pDocTemplate != nullptr && m_pDocTemplate->GetDocString(ext, CDocTemplate::filterExt) && !ext.IsEmpty())
			{
				const int semicolon = ext.Find(_T(';'));
				if(semicolon >= 0)
					ext = ext.Left(semicolon);
				newName += ext;
			}
		}
		CWinApp *app = AfxGetApp();
		if(app == nullptr || app->m_pDocManager == nullptr
			|| !app->m_pDocManager->DoPromptFileName(newName, 0, OFN_HIDEREADONLY | OFN_PATHMUSTEXIST, FALSE, m_pDocTemplate))
			return FALSE;
	}
	if(!OnSaveDocument(newName))
		return FALSE;
	if(replace)
		SetPathName(newName);
	OnDocumentEvent(onAfterSaveDocument);
	return TRUE;
}

BOOL CDocument::CanCloseFrame(CFrameWnd *frameArg)
{
	for(CView *view : m_viewList)
	{
		CFrameWnd *frame = view->GetParentFrame();
		if(frame != nullptr && frame != frameArg)
			return TRUE; // another frame shows the document
	}
	return SaveModified();
}

void CDocument::OnChangedViewList()
{
	if(m_bAutoDelete && m_viewList.empty())
	{
		OnCloseDocument();
		return;
	}
	UpdateFrameCounts();
}

void CDocument::AddView(CView *view)
{
	if(std::find(m_viewList.begin(), m_viewList.end(), view) != m_viewList.end())
		return;
	m_viewList.push_back(view);
	view->m_pDocument = this;
	OnChangedViewList();
}

void CDocument::RemoveView(CView *view)
{
	auto it = std::find(m_viewList.begin(), m_viewList.end(), view);
	if(it == m_viewList.end())
		return;
	m_viewList.erase(it);
	view->m_pDocument = nullptr;
	OnChangedViewList();
}

void CDocument::UpdateAllViews(CView *sender, LPARAM hint, CObject *hintObj)
{
	const std::vector<CView *> views = m_viewList;
	for(CView *view : views)
	{
		if(view != sender)
			view->OnUpdate(sender, hint, hintObj);
	}
}

void CDocument::UpdateFrameCounts()
{
	// Number the frames that show the document (":1", ":2" in their
	// titles), or none when there is only one.
	std::vector<CFrameWnd *> frames;
	for(CView *view : m_viewList)
	{
		if(!view->IsWindowVisible())
			continue;
		CFrameWnd *frame = view->GetParentFrame();
		if(frame != nullptr && std::find(frames.begin(), frames.end(), frame) == frames.end())
			frames.push_back(frame);
	}
	for(std::size_t i = 0; i < frames.size(); i++)
	{
		frames[i]->m_nWindow = frames.size() == 1 ? 0 : static_cast<int>(i + 1);
		frames[i]->OnUpdateFrameTitle(TRUE);
	}
}

POSITION CDocument::GetFirstViewPosition() const { return First(m_viewList); }
CView *CDocument::GetNextView(POSITION &pos) const { return Next(m_viewList, pos); }

void CDocument::OnFileClose()
{
	if(!SaveModified())
		return;
	OnCloseDocument();
}

void CDocument::OnFileSave() { DoFileSave(); }
void CDocument::OnFileSaveAs() { DoSave(nullptr); }

// ---- CDocTemplate ----------------------------------------------------------------

void CDocTemplate::LoadTemplate()
{
	if(m_strDocStrings.IsEmpty())
		m_strDocStrings.LoadString(m_nIDResource);
}

void CDocTemplate::AddDocument(CDocument *doc)
{
	doc->m_pDocTemplate = this;
}

void CDocTemplate::RemoveDocument(CDocument *doc)
{
	doc->m_pDocTemplate = nullptr;
}

BOOL CDocTemplate::GetDocString(CString &string, DocStringIndex index) const
{
	return AfxExtractSubString(string, m_strDocStrings, static_cast<int>(index));
}

CDocument *CDocTemplate::CreateNewDocument()
{
	if(m_pDocClass == nullptr)
		return nullptr;
	auto *doc = dynamic_cast<CDocument *>(m_pDocClass->CreateObject());
	if(doc != nullptr)
		AddDocument(doc);
	return doc;
}

CFrameWnd *CDocTemplate::CreateNewFrame(CDocument *doc, CFrameWnd *other)
{
	CCreateContext context;
	context.m_pCurrentFrame = other;
	context.m_pCurrentDoc = doc;
	context.m_pNewViewClass = m_pViewClass;
	context.m_pNewDocTemplate = this;
	if(m_pFrameClass == nullptr)
		return nullptr;
	auto *frame = dynamic_cast<CFrameWnd *>(m_pFrameClass->CreateObject());
	if(frame == nullptr)
		return nullptr;
	if(!frame->LoadFrame(m_nIDResource, WS_OVERLAPPEDWINDOW | FWS_ADDTOTITLE, nullptr, &context))
		return nullptr; // the frame deleted itself
	return frame;
}

void CDocTemplate::InitialUpdateFrame(CFrameWnd *frame, CDocument *doc, BOOL makeVisible)
{
	frame->InitialUpdateFrame(doc, makeVisible);
}

BOOL CDocTemplate::SaveAllModified()
{
	for(POSITION pos = GetFirstDocPosition(); pos != nullptr;)
	{
		if(!GetNextDoc(pos)->SaveModified())
			return FALSE;
	}
	return TRUE;
}

void CDocTemplate::CloseAllDocuments(BOOL)
{
	// Closing a document takes it off the list.
	for(POSITION pos = GetFirstDocPosition(); pos != nullptr; pos = GetFirstDocPosition())
	{
		CDocument *doc = GetNextDoc(pos);
		const POSITION before = GetFirstDocPosition();
		doc->OnCloseDocument();
		if(GetFirstDocPosition() == before && before != nullptr)
		{
			POSITION p = before;
			if(GetNextDoc(p) == doc)
				RemoveDocument(doc); // not auto-deleted: it leaves the list
		}
	}
}

CDocTemplate::Confidence CDocTemplate::MatchDocType(LPCTSTR pathName, CDocument *&match)
{
	match = nullptr;
	for(POSITION pos = GetFirstDocPosition(); pos != nullptr;)
	{
		CDocument *doc = GetNextDoc(pos);
		if(!doc->GetPathName().IsEmpty() && doc->GetPathName() == pathName)
		{
			match = doc;
			return yesAlreadyOpen;
		}
	}
	CString exts;
	if(GetDocString(exts, CDocTemplate::filterExt) && !exts.IsEmpty())
	{
		const CString path = pathName;
		const int dot = path.ReverseFind(_T('.'));
		const int slash = std::max(path.ReverseFind(_T('/')), path.ReverseFind(_T('\\')));
		if(dot > slash)
		{
			const CString ext = path.Mid(dot);
			// filterExt may hold several extensions separated by ';'.
			for(int i = 0;; i++)
			{
				CString one;
				if(!AfxExtractSubString(one, exts, i, _T(';')))
					break;
				one.Trim();
				if(!one.IsEmpty() && one.CompareNoCase(ext) == 0)
					return yesAttemptNative;
			}
		}
	}
	return yesAttemptForeign;
}

// ---- CMultiDocTemplate -----------------------------------------------------------

CMultiDocTemplate::~CMultiDocTemplate()
{
	// Documents still open lose their template.
	for(CDocument *doc : m_docList)
		doc->m_pDocTemplate = nullptr;
	if(m_hMenuShared != nullptr)
		::DestroyMenu(m_hMenuShared);
	if(m_hAccelTable != nullptr)
		::DestroyAcceleratorTable(m_hAccelTable);
}

void CMultiDocTemplate::LoadTemplate()
{
	CDocTemplate::LoadTemplate();
	if(m_nIDResource != 0 && m_hMenuShared == nullptr)
	{
		const HINSTANCE instance = AfxGetResourceHandle();
		m_hMenuShared = ::LoadMenu(instance, MAKEINTRESOURCE(m_nIDResource));
		m_hAccelTable = ::LoadAccelerators(instance, MAKEINTRESOURCE(m_nIDResource));
	}
}

void CMultiDocTemplate::AddDocument(CDocument *doc)
{
	CDocTemplate::AddDocument(doc);
	m_docList.push_back(doc);
}

void CMultiDocTemplate::RemoveDocument(CDocument *doc)
{
	CDocTemplate::RemoveDocument(doc);
	m_docList.erase(std::remove(m_docList.begin(), m_docList.end(), doc), m_docList.end());
}

POSITION CMultiDocTemplate::GetFirstDocPosition() const { return First(m_docList); }
CDocument *CMultiDocTemplate::GetNextDoc(POSITION &pos) const { return Next(m_docList, pos); }

void CMultiDocTemplate::SetDefaultTitle(CDocument *doc)
{
	CString name;
	if(GetDocString(name, docName) && !name.IsEmpty())
	{
		CString number;
		number.Format(_T("%u"), m_nUntitledCount + 1);
		name += number;
	} else if(!name.LoadString(AFX_IDS_UNTITLED))
	{
		name = _T("Untitled");
	}
	doc->SetTitle(name);
}

CDocument *CMultiDocTemplate::OpenDocumentFile(LPCTSTR pathName, BOOL addToMRU, BOOL makeVisible)
{
	CDocument *doc = CreateNewDocument();
	if(doc == nullptr)
		return nullptr;
	// The document stays if something goes wrong with the frame.
	const BOOL autoDelete = doc->m_bAutoDelete;
	doc->m_bAutoDelete = FALSE;
	CFrameWnd *frame = CreateNewFrame(doc, nullptr);
	doc->m_bAutoDelete = autoDelete;
	if(frame == nullptr)
	{
		delete doc;
		return nullptr;
	}
	if(pathName == nullptr)
	{
		SetDefaultTitle(doc);
		if(!makeVisible)
			doc->m_bEmbedded = TRUE;
		if(!doc->OnNewDocument())
		{
			frame->DestroyWindow(); // takes the document with it
			return nullptr;
		}
		m_nUntitledCount++;
	} else
	{
		BeginWaitCursor();
		const BOOL opened = doc->OnOpenDocument(pathName);
		EndWaitCursor();
		if(!opened)
		{
			frame->DestroyWindow();
			return nullptr;
		}
		doc->SetPathName(pathName, addToMRU);
		doc->OnDocumentEvent(CDocument::onAfterOpenDocument);
	}
	InitialUpdateFrame(frame, doc, makeVisible);
	return doc;
}

// ---- CDocManager -----------------------------------------------------------------

CDocManager::~CDocManager()
{
	for(CDocTemplate *tmpl : m_templateList)
		delete tmpl;
	m_templateList.clear();
}

void CDocManager::AddDocTemplate(CDocTemplate *tmpl)
{
	if(tmpl == nullptr)
		return;
	tmpl->LoadTemplate();
	m_templateList.push_back(tmpl);
}

POSITION CDocManager::GetFirstDocTemplatePosition() const { return First(m_templateList); }
CDocTemplate *CDocManager::GetNextDocTemplate(POSITION &pos) const { return Next(m_templateList, pos); }

CDocument *CDocManager::OpenDocumentFile(LPCTSTR fileName, BOOL addToMRU)
{
	if(fileName == nullptr)
		return nullptr;
	CDocTemplate::Confidence best = CDocTemplate::noAttempt;
	CDocTemplate *bestTemplate = nullptr;
	CDocument *open = nullptr;
	for(CDocTemplate *tmpl : m_templateList)
	{
		CDocument *match = nullptr;
		const CDocTemplate::Confidence confidence = tmpl->MatchDocType(fileName, match);
		if(confidence > best)
		{
			best = confidence;
			bestTemplate = tmpl;
		}
		if(confidence == CDocTemplate::yesAlreadyOpen)
		{
			open = match;
			break;
		}
	}
	if(open != nullptr)
	{
		// Already open: its frame comes to the front.
		POSITION pos = open->GetFirstViewPosition();
		if(pos != nullptr)
		{
			if(CFrameWnd *frame = open->GetNextView(pos)->GetParentFrame())
			{
				frame->ActivateFrame();
				CWinApp *app = AfxGetApp();
				auto *appFrame = app != nullptr ? dynamic_cast<CFrameWnd *>(app->m_pMainWnd) : nullptr;
				if(frame->GetParent() != nullptr && appFrame != nullptr && appFrame != frame)
					appFrame->ActivateFrame();
			}
		}
		return open;
	}
	if(bestTemplate == nullptr)
		return nullptr;
	return bestTemplate->OpenDocumentFile(fileName, addToMRU, TRUE);
}

BOOL CDocManager::SaveAllModified()
{
	for(CDocTemplate *tmpl : m_templateList)
	{
		if(!tmpl->SaveAllModified())
			return FALSE;
	}
	return TRUE;
}

void CDocManager::CloseAllDocuments(BOOL endSession)
{
	for(CDocTemplate *tmpl : m_templateList)
		tmpl->CloseAllDocuments(endSession);
}

int CDocManager::GetOpenDocumentCount()
{
	int count = 0;
	for(CDocTemplate *tmpl : m_templateList)
	{
		for(POSITION pos = tmpl->GetFirstDocPosition(); pos != nullptr; tmpl->GetNextDoc(pos))
			count++;
	}
	return count;
}

BOOL CDocManager::DoPromptFileName(CString &, UINT, DWORD, BOOL, CDocTemplate *)
{
	// MFC shows a CFileDialog with the template's filter here. File dialogs
	// come with the dialogs of step 4; upstream asks for file names with
	// dialogs of its own (FileDialog.h) and does not reach this.
	return FALSE;
}

void CDocManager::OnFileNew()
{
	// MFC asks which kind of document when there are several templates;
	// upstream has one.
	if(m_templateList.empty())
		return;
	m_templateList.front()->OpenDocumentFile(nullptr);
}

void CDocManager::OnFileOpen()
{
	CString name;
	if(!DoPromptFileName(name, 0, OFN_HIDEREADONLY | OFN_FILEMUSTEXIST, TRUE, nullptr))
		return;
	if(CWinApp *app = AfxGetApp())
		app->OpenDocumentFile(name);
}

// ---- CWinApp: documents ----------------------------------------------------------

void CWinApp::AddDocTemplate(CDocTemplate *tmpl)
{
	if(m_pDocManager == nullptr)
		m_pDocManager = new CDocManager;
	m_pDocManager->AddDocTemplate(tmpl);
}

CDocument *CWinApp::OpenDocumentFile(LPCTSTR path)
{
	return m_pDocManager != nullptr ? m_pDocManager->OpenDocumentFile(path) : nullptr;
}

CDocument *CWinApp::OpenDocumentFile(LPCTSTR path, BOOL addToMRU)
{
	return m_pDocManager != nullptr ? m_pDocManager->OpenDocumentFile(path, addToMRU) : nullptr;
}

BOOL CWinApp::SaveAllModified()
{
	return m_pDocManager == nullptr || m_pDocManager->SaveAllModified();
}

void CWinApp::CloseAllDocuments(BOOL endSession)
{
	if(m_pDocManager != nullptr)
		m_pDocManager->CloseAllDocuments(endSession);
}

int CWinApp::GetOpenDocumentCount()
{
	return m_pDocManager != nullptr ? m_pDocManager->GetOpenDocumentCount() : 0;
}

void CWinApp::OnFileNew()
{
	if(m_pDocManager != nullptr)
		m_pDocManager->OnFileNew();
}

void CWinApp::OnFileOpen()
{
	if(m_pDocManager != nullptr)
		m_pDocManager->OnFileOpen();
}
