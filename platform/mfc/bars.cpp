// linOpenMPT MFC layer: control bars, as MFC's barcore.cpp, bartool.cpp,
// barstat.cpp and bardlg.cpp.
//
// A control bar is laid out by its frame (WM_SIZEPARENT from RepositionBars),
// draws its borders and gripper in its non-client area, updates its buttons
// or panes at idle time through OnUpdateCmdUI, shows flyby help for what the
// mouse is over in the frame's status bar, and passes the notifications of
// its children on to its owner. Toolbars and status bars are the common
// controls (ToolbarWindow32, msctls_statusbar32) of the Win32 layer,
// subclassed, exactly as MFC does it; the layout code follows MFC's.
#include "MfcInternal.h"

#include <afxres.h>

#include <algorithm>
#include <cwchar>

namespace
{

// MFC's afxData: borders are SM_CXBORDER (1) twice.
constexpr int CxBorder = 1;
constexpr int CyBorder = 1;
constexpr int CxBorder2 = 2 * CxBorder;
constexpr int CyBorder2 = 2 * CyBorder;
// The gripper of a docked bar (barcore.cpp).
constexpr int CxGripper = 3;
constexpr int CyGripper = 3;
constexpr int CxBorderGripper = 2;
constexpr int CyBorderGripper = 2;
// Flyby help timers (barcore.cpp).
constexpr UINT_PTR IdTimerWait = 0xE000;
constexpr UINT_PTR IdTimerCheck = 0xE001;
// The width of a drop-down arrow: the toolbar control's (platform/qtwin).
constexpr int DropDownWidth = 14;
// Space around the text of a status bar pane (barstat.cpp).
constexpr int CxPaneBorder = 6;
// A status bar pane's text needs to be set again.
constexpr UINT SbpfUpdate = 0x0001;

// MFC's module thread state for flyby help.
INT_PTR g_lastStatus = -1;
CControlBar *g_lastStatusBar = nullptr;

} // namespace

// Strings MFC itself provides (afxres.rc), for applications that leave them
// out of their own resources.
bool LinOpenMPTLoadMfcString(UINT id, CString &text)
{
	if(text.LoadString(id))
		return true;
	switch(id)
	{
	case AFX_IDS_IDLEMESSAGE: text = _T("Ready"); return true;
	case AFX_IDS_HELPMODEMESSAGE: text = _T("Select an object on which to get Help"); return true;
	case ID_INDICATOR_EXT: text = _T("EXT"); return true;
	case ID_INDICATOR_CAPS: text = _T("CAP"); return true;
	case ID_INDICATOR_NUM: text = _T("NUM"); return true;
	case ID_INDICATOR_SCRL: text = _T("SCRL"); return true;
	case ID_INDICATOR_OVR: text = _T("OVR"); return true;
	case ID_INDICATOR_REC: text = _T("REC"); return true;
	default: return false;
	}
}

// ---- CControlBar ---------------------------------------------------------------------

BEGIN_MESSAGE_MAP(CControlBar, CWnd)
	ON_WM_TIMER()
	ON_WM_CREATE()
	ON_WM_DESTROY()
	ON_WM_PAINT()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONDBLCLK()
	ON_WM_MOUSEACTIVATE()
	ON_MESSAGE(WM_IDLEUPDATECMDUI, &CControlBar::OnIdleUpdateCmdUI)
	ON_MESSAGE(WM_SIZEPARENT, &CControlBar::OnSizeParent)
END_MESSAGE_MAP()

CControlBar::~CControlBar()
{
	DestroyWindow(); // while this is still a control bar
	if(m_pDockSite != nullptr)
		m_pDockSite->RemoveControlBar(this);
	CDockContext *context = m_pDockContext;
	m_pDockContext = nullptr;
	delete context;
	if(g_lastStatusBar == this)
	{
		g_lastStatusBar = nullptr;
		g_lastStatus = -1;
	}
}

BOOL CControlBar::DestroyWindow()
{
	// A floating bar goes with its floating frame.
	if(m_hWnd != nullptr && IsFloating())
		return GetDockingFrame()->DestroyWindow();
	return CWnd::DestroyWindow();
}

void CControlBar::PostNcDestroy()
{
	if(m_bAutoDelete)
		delete this;
}

void CControlBar::SetBorders(int left, int top, int right, int bottom)
{
	m_cxLeftBorder = left;
	m_cyTopBorder = top;
	m_cxRightBorder = right;
	m_cyBottomBorder = bottom;
}

void CControlBar::SetBarStyle(DWORD style)
{
	if(m_dwStyle != style)
	{
		const DWORD old = m_dwStyle;
		m_dwStyle = style;
		OnBarStyleChange(old, style);
	}
}

BOOL CControlBar::PreCreateWindow(CREATESTRUCT &cs)
{
	if(!CWnd::PreCreateWindow(cs))
		return FALSE;
	cs.style |= WS_CLIPSIBLINGS;
	// The borders of Windows 95 and later, unless CBRS_BORDER_3D says
	// otherwise.
	if((m_dwStyle & CBRS_BORDER_3D) == 0)
	{
		DWORD border = 0;
		switch(m_dwStyle & (CBRS_BORDER_ANY | CBRS_ALIGN_ANY))
		{
		case CBRS_LEFT: border = CBRS_BORDER_TOP | CBRS_BORDER_BOTTOM; break;
		case CBRS_TOP: border = CBRS_BORDER_TOP; break;
		case CBRS_RIGHT: border = CBRS_BORDER_TOP | CBRS_BORDER_BOTTOM; break;
		case CBRS_BOTTOM: border = CBRS_BORDER_BOTTOM; break;
		}
		if(border != 0)
		{
			m_dwStyle &= ~CBRS_BORDER_ANY;
			m_dwStyle |= border | CBRS_BORDER_3D;
		}
	}
	return TRUE;
}

CSize CControlBar::CalcFixedLayout(BOOL stretch, BOOL horz)
{
	return CSize(stretch && horz ? 32767 : 0, stretch && !horz ? 32767 : 0);
}

CSize CControlBar::CalcDynamicLayout(int, DWORD mode)
{
	return CalcFixedLayout((mode & LM_STRETCH) != 0, (mode & LM_HORZ) != 0);
}

CFrameWnd *CControlBar::GetDockingFrame() const
{
	CFrameWnd *frame = GetParentFrame();
	return frame != nullptr ? frame : m_pDockSite;
}

BOOL CControlBar::IsFloating() const
{
	if(IsDockBar())
		return static_cast<const CDockBar *>(this)->m_bFloating;
	return m_pDockBar != nullptr && m_pDockBar->m_bFloating;
}

BOOL CControlBar::IsVisible() const
{
	if(m_nStateFlags & delayHide)
		return FALSE;
	return (m_nStateFlags & delayShow) || (GetStyle() & WS_VISIBLE);
}

void CControlBar::DelayShow(BOOL show)
{
	m_nStateFlags &= ~(delayHide | delayShow);
	if(show && (GetStyle() & WS_VISIBLE) == 0)
		m_nStateFlags |= delayShow;
	else if(!show && (GetStyle() & WS_VISIBLE) != 0)
		m_nStateFlags |= delayHide;
}

void CControlBar::EnableDocking(DWORD dockStyle)
{
	m_dwDockStyle = dockStyle;
	if(m_pDockContext == nullptr)
		m_pDockContext = new CDockContext(this);
	// The owner stays the frame wherever the bar goes.
	if(m_hWndOwner == nullptr)
		m_hWndOwner = ::GetParent(m_hWnd);
}

int CControlBar::OnCreate(LPCREATESTRUCT cs)
{
	if(CWnd::OnCreate(cs) == -1)
		return -1;
	if(m_dwStyle & CBRS_TOOLTIPS)
		EnableToolTips();
	CWnd *parent = GetParent();
	if(parent != nullptr && parent->IsFrameWnd())
	{
		m_pDockSite = static_cast<CFrameWnd *>(parent);
		m_pDockSite->AddControlBar(this);
	}
	return 0;
}

void CControlBar::OnDestroy()
{
	if(g_lastStatusBar == this)
		SetStatusText(-1);
	if(m_pDockSite != nullptr)
	{
		m_pDockSite->RemoveControlBar(this);
		m_pDockSite = nullptr;
	}
	CWnd::OnDestroy();
}

LRESULT CControlBar::WindowProc(UINT msg, WPARAM wp, LPARAM lp)
{
	switch(msg)
	{
	case WM_NOTIFY:
	case WM_COMMAND:
	case WM_DRAWITEM:
	case WM_MEASUREITEM:
	case WM_DELETEITEM:
	case WM_COMPAREITEM:
	case WM_VKEYTOITEM:
	case WM_CHARTOITEM:
	{
		// What the bar does not handle goes to its owner.
		LRESULT result = 0;
		if(OnWndMsg(msg, wp, lp, &result))
			return result;
		CWnd *owner = GetOwner();
		if(owner == nullptr)
			return CWnd::WindowProc(msg, wp, lp);
		result = owner->SendMessage(msg, wp, lp);
		if(msg == WM_NOTIFY)
		{
			// A tooltip the owner has no text for: the bar's own turn.
			const auto *nm = reinterpret_cast<const NMHDR *>(lp);
			if(nm != nullptr && nm->code == TTN_NEEDTEXTW)
			{
				const auto *ttt = reinterpret_cast<const TOOLTIPTEXTW *>(lp);
				if((ttt->lpszText == nullptr || ttt->lpszText[0] == 0) && ttt->szText[0] == 0)
					result = CWnd::WindowProc(msg, wp, lp);
			}
		}
		return result;
	}
	default:
		return CWnd::WindowProc(msg, wp, lp);
	}
}

BOOL CControlBar::PreTranslateMessage(MSG *msg)
{
	if(CWnd::PreTranslateMessage(msg))
		return TRUE;

	const UINT message = msg->message;
	// Flyby help in the status bar (CBRS_FLYBY); a click always counts.
	if(((m_dwStyle & CBRS_FLYBY) || message == WM_LBUTTONDOWN || message == WM_LBUTTONUP) &&
		((message >= WM_MOUSEFIRST && message <= WM_MOUSELAST) || (message >= WM_NCMOUSEMOVE && message <= WM_NCMBUTTONDBLCLK)))
	{
		CPoint point = msg->pt;
		ScreenToClient(&point);
		TOOLINFO ti{};
		ti.cbSize = sizeof(ti);
		INT_PTR hit = OnToolHitTest(point, &ti);
		if(ti.lpszText != nullptr && ti.lpszText != LPSTR_TEXTCALLBACK && !IS_INTRESOURCE(ti.lpszText))
			std::free(ti.lpszText);
		const BOOL notButton = message == WM_LBUTTONDOWN && (ti.uFlags & TTF_NOTBUTTON);
		if(message != WM_LBUTTONDOWN && ::GetKeyState(VK_LBUTTON) < 0)
			hit = g_lastStatus;
		if(hit < 0 || notButton)
		{
			if(::GetKeyState(VK_LBUTTON) >= 0 || notButton)
			{
				SetStatusText(-1);
				KillTimer(IdTimerCheck);
			}
		} else if(message == WM_LBUTTONUP)
		{
			SetStatusText(-1);
			ResetTimer(static_cast<UINT>(IdTimerCheck), 200);
		} else if((m_nStateFlags & statusSet) || ::GetKeyState(VK_LBUTTON) < 0)
		{
			SetStatusText(hit);
		} else if(hit != g_lastStatus)
		{
			ResetTimer(static_cast<UINT>(IdTimerWait), 300);
		}
		g_lastStatus = hit;
	}

	// The frames see the message before IsDialogMessage would eat their
	// accelerators.
	for(CWnd *owner = GetOwner(); owner != nullptr; owner = owner->GetParentFrame())
	{
		if(owner->PreTranslateMessage(msg))
			return TRUE;
	}
	return PreTranslateInput(msg);
}

BOOL CControlBar::SetStatusText(INT_PTR hit)
{
	CWnd *owner = GetOwner();
	if(hit == -1)
	{
		g_lastStatusBar = nullptr;
		if(m_nStateFlags & statusSet)
		{
			if(owner != nullptr)
				owner->SendMessage(WM_POPMESSAGESTRING, AFX_IDS_IDLEMESSAGE);
			m_nStateFlags &= ~statusSet;
			return TRUE;
		}
		KillTimer(IdTimerWait);
	} else if(!(m_nStateFlags & statusSet) || g_lastStatus != hit)
	{
		g_lastStatusBar = this;
		if(owner != nullptr)
			owner->SendMessage(WM_SETMESSAGESTRING, static_cast<WPARAM>(hit));
		m_nStateFlags |= statusSet;
		ResetTimer(static_cast<UINT>(IdTimerCheck), 200);
		return TRUE;
	}
	return FALSE;
}

void CControlBar::ResetTimer(UINT event, UINT time)
{
	KillTimer(IdTimerWait);
	KillTimer(IdTimerCheck);
	SetTimer(event, time, nullptr);
}

void CControlBar::OnTimer(UINT_PTR event)
{
	if(::GetKeyState(VK_LBUTTON) < 0)
		return;
	CPoint point;
	::GetCursorPos(&point);
	ScreenToClient(&point);
	INT_PTR hit = OnToolHitTest(point, nullptr);
	if(hit >= 0)
	{
		// Not while another application or a capture has the mouse.
		CWnd *top = GetTopLevelParent();
		if(!IsTopParentActive() || top == nullptr || !top->IsWindowEnabled())
			hit = -1;
		const HWND capture = ::GetCapture();
		if(capture != nullptr && capture != m_hWnd && top != nullptr && ::GetAncestor(capture, GA_ROOT) == top->m_hWnd)
			hit = -1;
	} else
	{
		g_lastStatus = -1;
	}
	if(hit >= 0)
	{
		ClientToScreen(&point);
		const HWND under = ::WindowFromPoint(point);
		if(under == nullptr || (under != m_hWnd && !::IsChild(m_hWnd, under)))
		{
			hit = -1;
			g_lastStatus = -1;
		}
	}
	if(hit < 0)
	{
		if(g_lastStatus == -1)
			KillTimer(IdTimerCheck);
		SetStatusText(-1);
	}
	if(event == IdTimerWait)
	{
		KillTimer(IdTimerWait);
		if(hit >= 0)
			SetStatusText(hit);
	}
}

void CControlBar::OnLButtonDown(UINT flags, CPoint point)
{
	// A press on the bar itself (not a button) drags it.
	if(m_pDockBar != nullptr && m_pDockContext != nullptr && OnToolHitTest(point, nullptr) == -1)
	{
		ClientToScreen(&point);
		m_pDockContext->StartDrag(point);
	} else
	{
		CWnd::OnLButtonDown(flags, point);
	}
}

void CControlBar::OnLButtonDblClk(UINT flags, CPoint point)
{
	if(m_pDockBar != nullptr && m_pDockContext != nullptr && OnToolHitTest(point, nullptr) == -1)
		m_pDockContext->ToggleDocking();
	else
		CWnd::OnLButtonDblClk(flags, point);
}

int CControlBar::OnMouseActivate(CWnd *topParent, UINT hitTest, UINT message)
{
	if(!IsFloating())
		return CWnd::OnMouseActivate(topParent, hitTest, message);
	ActivateTopParent();
	return MA_NOACTIVATE;
}

LRESULT CControlBar::OnIdleUpdateCmdUI(WPARAM wp, LPARAM)
{
	// Delayed showing and hiding.
	const BOOL visible = (GetStyle() & WS_VISIBLE) != 0;
	UINT swp = 0;
	if((m_nStateFlags & delayHide) && visible)
		swp = SWP_HIDEWINDOW;
	else if((m_nStateFlags & delayShow) && !visible)
		swp = SWP_SHOWWINDOW;
	m_nStateFlags &= ~(delayShow | delayHide);
	if(swp != 0)
		SetWindowPos(nullptr, 0, 0, 0, 0, swp | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);

	if((GetStyle() & WS_VISIBLE) && (m_pDockBar == nullptr || (m_pDockBar->GetStyle() & WS_VISIBLE)))
	{
		CWnd *owner = GetOwner();
		CFrameWnd *target = owner != nullptr && owner->IsFrameWnd() ? static_cast<CFrameWnd *>(owner) : GetParentFrame();
		if(target != nullptr)
			OnUpdateCmdUI(target, static_cast<BOOL>(wp));
	}
	return 0;
}

LRESULT CControlBar::OnSizeParent(WPARAM, LPARAM lp)
{
	auto *layout = reinterpret_cast<AFX_SIZEPARENTPARAMS *>(lp);
	const DWORD style = RecalcDelayShow(layout);
	if((style & WS_VISIBLE) && (style & CBRS_ALIGN_ANY) != 0)
	{
		CRect rect(layout->rect);
		const CSize avail = rect.Size();
		DWORD mode = layout->bStretch ? LM_STRETCH : 0;
		if((m_dwStyle & CBRS_SIZE_DYNAMIC) && (m_dwStyle & CBRS_FLOATING))
			mode |= LM_HORZ | LM_MRUWIDTH;
		else if(style & CBRS_ORIENT_HORZ)
			mode |= LM_HORZ | LM_HORZDOCK;
		else
			mode |= LM_VERTDOCK;
		CSize size = CalcDynamicLayout(-1, mode);
		size.cx = std::min(size.cx, avail.cx);
		size.cy = std::min(size.cy, avail.cy);
		if(style & CBRS_ORIENT_HORZ)
		{
			layout->sizeTotal.cy += size.cy;
			layout->sizeTotal.cx = std::max(layout->sizeTotal.cx, size.cx);
			if(style & CBRS_ALIGN_TOP)
			{
				layout->rect.top += size.cy;
			} else if(style & CBRS_ALIGN_BOTTOM)
			{
				rect.top = rect.bottom - size.cy;
				layout->rect.bottom -= size.cy;
			}
		} else if(style & CBRS_ORIENT_VERT)
		{
			layout->sizeTotal.cx += size.cx;
			layout->sizeTotal.cy = std::max(layout->sizeTotal.cy, size.cy);
			if(style & CBRS_ALIGN_LEFT)
			{
				layout->rect.left += size.cx;
			} else if(style & CBRS_ALIGN_RIGHT)
			{
				rect.left = rect.right - size.cx;
				layout->rect.right -= size.cx;
			}
		}
		rect.right = rect.left + size.cx;
		rect.bottom = rect.top + size.cy;
		if(layout->hDWP != nullptr)
			AfxRepositionWindow(layout, m_hWnd, &rect);
	}
	return 0;
}

DWORD CControlBar::RecalcDelayShow(AFX_SIZEPARENTPARAMS *layout)
{
	DWORD style = (m_dwStyle & (CBRS_ALIGN_ANY | CBRS_BORDER_ANY)) | (GetStyle() & WS_VISIBLE);
	if(m_nStateFlags & (delayHide | delayShow))
	{
		UINT swp = 0;
		if(m_nStateFlags & delayHide)
		{
			if(style & WS_VISIBLE)
				swp = SWP_HIDEWINDOW;
		} else if((style & WS_VISIBLE) == 0)
		{
			swp = SWP_SHOWWINDOW;
		}
		if(swp != 0)
		{
			// As it will be once the layout is done.
			style ^= WS_VISIBLE;
			if(layout->hDWP != nullptr)
			{
				m_nStateFlags &= ~(delayShow | delayHide);
				layout->hDWP = ::DeferWindowPos(layout->hDWP, m_hWnd, nullptr, 0, 0, 0, 0, swp | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
			}
		} else
		{
			m_nStateFlags &= ~(delayShow | delayHide);
		}
	}
	return style;
}

void CControlBar::CalcInsideRect(CRect &rect, BOOL horz) const
{
	const DWORD style = m_dwStyle;
	if(style & CBRS_BORDER_LEFT)
		rect.left += CxBorder2;
	if(style & CBRS_BORDER_TOP)
		rect.top += CyBorder2;
	if(style & CBRS_BORDER_RIGHT)
		rect.right -= CxBorder2;
	if(style & CBRS_BORDER_BOTTOM)
		rect.bottom -= CyBorder2;
	const bool gripper = (style & (CBRS_GRIPPER | CBRS_FLOATING)) == CBRS_GRIPPER;
	if(horz)
	{
		rect.left += m_cxLeftBorder;
		rect.top += m_cyTopBorder;
		rect.right -= m_cxRightBorder;
		rect.bottom -= m_cyBottomBorder;
		if(gripper)
			rect.left += CxBorderGripper + CxGripper + CxBorderGripper;
	} else
	{
		rect.left += m_cyTopBorder;
		rect.top += m_cxLeftBorder;
		rect.right -= m_cyBottomBorder;
		rect.bottom -= m_cxRightBorder;
		if(gripper)
			rect.top += CyBorderGripper + CyGripper + CyBorderGripper;
	}
}

void CControlBar::OnPaint()
{
	CPaintDC dc(this);
	if(IsVisible())
		DoPaint(&dc);
}

void CControlBar::DoPaint(CDC *dc)
{
	CRect rect;
	GetClientRect(rect);
	DrawBorders(dc, rect);
	DrawGripper(dc, rect);
}

void CControlBar::DrawBorders(CDC *dc, CRect &rect)
{
	const DWORD style = m_dwStyle;
	if(!(style & CBRS_BORDER_ANY))
		return;
	CRect rect1 = rect, rect2 = rect;
	COLORREF color = ::GetSysColor(COLOR_BTNSHADOW);
	// The dark line, one pixel back and up.
	if(style & CBRS_BORDER_3D)
	{
		rect1.right -= CxBorder;
		rect1.bottom -= CyBorder;
	}
	if(style & CBRS_BORDER_TOP)
		rect2.top += CyBorder2;
	if(style & CBRS_BORDER_BOTTOM)
		rect2.bottom -= CyBorder2;
	if(style & CBRS_BORDER_LEFT)
		dc->FillSolidRect(0, rect2.top, CxBorder, rect2.Height(), color);
	if(style & CBRS_BORDER_TOP)
		dc->FillSolidRect(0, 0, rect.right, CyBorder, color);
	if(style & CBRS_BORDER_RIGHT)
		dc->FillSolidRect(rect1.right - CxBorder, rect2.top, CxBorder, rect2.Height(), color);
	if(style & CBRS_BORDER_BOTTOM)
		dc->FillSolidRect(0, rect1.bottom - CyBorder, rect.right, CyBorder, color);
	if(style & CBRS_BORDER_3D)
	{
		// The light line.
		color = ::GetSysColor(COLOR_BTNHIGHLIGHT);
		if(style & CBRS_BORDER_LEFT)
			dc->FillSolidRect(1, rect2.top, CxBorder, rect2.Height(), color);
		if(style & CBRS_BORDER_TOP)
			dc->FillSolidRect(0, 1, rect.right, CyBorder, color);
		if(style & CBRS_BORDER_RIGHT)
			dc->FillSolidRect(rect.right - CxBorder, rect2.top, CxBorder, rect2.Height(), color);
		if(style & CBRS_BORDER_BOTTOM)
			dc->FillSolidRect(0, rect.bottom - CyBorder, rect.right, CyBorder, color);
	}
	if(style & CBRS_BORDER_LEFT)
		rect.left += CxBorder2;
	if(style & CBRS_BORDER_TOP)
		rect.top += CyBorder2;
	if(style & CBRS_BORDER_RIGHT)
		rect.right -= CxBorder2;
	if(style & CBRS_BORDER_BOTTOM)
		rect.bottom -= CyBorder2;
}

void CControlBar::DrawGripper(CDC *dc, const CRect &rect)
{
	if((m_dwStyle & (CBRS_GRIPPER | CBRS_FLOATING)) != CBRS_GRIPPER)
		return;
	const COLORREF light = ::GetSysColor(COLOR_BTNHIGHLIGHT), dark = ::GetSysColor(COLOR_BTNSHADOW);
	if(m_dwStyle & CBRS_ORIENT_HORZ)
		dc->Draw3dRect(rect.left + CxBorderGripper, rect.top + m_cyTopBorder, CxGripper, rect.Height() - m_cyTopBorder - m_cyBottomBorder, light, dark);
	else
		dc->Draw3dRect(rect.left + m_cyTopBorder, rect.top + CyBorderGripper, rect.Width() - m_cyTopBorder - m_cyBottomBorder, CyGripper, light, dark);
}

void CControlBar::EraseNonClient()
{
	// The borders and gripper live in the non-client area.
	CWindowDC dc(this);
	CRect client;
	GetClientRect(client);
	CRect window;
	GetWindowRect(window);
	ScreenToClient(window);
	client.OffsetRect(-window.left, -window.top);
	dc.ExcludeClipRect(client);
	window.OffsetRect(-window.left, -window.top);
	DrawBorders(&dc, window);
	dc.IntersectClipRect(window);
	SendMessage(WM_ERASEBKGND, reinterpret_cast<WPARAM>(dc.m_hDC));
	DrawGripper(&dc, window);
}

void CControlBar::GetBarInfo(CControlBarInfo *info)
{
	info->m_nBarID = static_cast<UINT>(::GetDlgCtrlID(m_hWnd));
	info->m_pBar = this;
	info->m_bVisible = IsVisible();
	info->m_nMRUWidth = m_nMRUWidth;
	if(m_pDockBar != nullptr)
	{
		CRect rect;
		GetWindowRect(&rect);
		m_pDockBar->ScreenToClient(&rect);
		info->m_pointPos = rect.TopLeft();
		if(m_pDockContext != nullptr)
		{
			info->m_bDocking = TRUE;
			info->m_uMRUDockID = m_pDockContext->m_uMRUDockID;
			info->m_rectMRUDockPos = m_pDockContext->m_rectMRUDockPos;
			info->m_dwMRUFloatStyle = m_pDockContext->m_dwMRUFloatStyle;
			info->m_ptMRUFloatPos = m_pDockContext->m_ptMRUFloatPos;
		}
	}
}

void CControlBar::SetBarInfo(CControlBarInfo *info, CFrameWnd *)
{
	UINT flags = (info->m_bVisible ? SWP_SHOWWINDOW : SWP_HIDEWINDOW) | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE;
	if(info->m_pointPos.x == -1 && info->m_pointPos.y == -1)
		flags |= SWP_NOMOVE;
	m_nMRUWidth = info->m_nMRUWidth;
	CalcDynamicLayout(0, LM_HORZ | LM_MRUWIDTH | LM_COMMIT);
	if(info->m_bDocking && m_pDockContext != nullptr)
	{
		m_pDockContext->m_uMRUDockID = info->m_uMRUDockID;
		m_pDockContext->m_rectMRUDockPos = info->m_rectMRUDockPos;
		m_pDockContext->m_dwMRUFloatStyle = info->m_dwMRUFloatStyle;
		m_pDockContext->m_ptMRUFloatPos = info->m_ptMRUFloatPos;
	}
	SetWindowPos(nullptr, info->m_pointPos.x, info->m_pointPos.y, 0, 0, flags);
	// A hidden bar that is the last in its floating frame hides the frame.
	if(IsFloating())
	{
		if(CFrameWnd *frame = GetDockingFrame())
			frame->ShowControlBar(this, info->m_bVisible, TRUE);
	}
}

void AFXAPI AfxRepositionWindow(AFX_SIZEPARENTPARAMS *layout, HWND hwnd, LPCRECT rect)
{
	const HWND parent = ::GetParent(hwnd);
	if(layout != nullptr && layout->hDWP == nullptr)
		return;
	CRect old;
	::GetWindowRect(hwnd, &old);
	::MapWindowPoints(nullptr, parent, reinterpret_cast<POINT *>(&old), 2);
	if(::EqualRect(&old, rect))
		return;
	if(layout != nullptr)
		layout->hDWP = ::DeferWindowPos(layout->hDWP, hwnd, nullptr, rect->left, rect->top, rect->right - rect->left, rect->bottom - rect->top, SWP_NOACTIVATE | SWP_NOZORDER);
	else
		::SetWindowPos(hwnd, nullptr, rect->left, rect->top, rect->right - rect->left, rect->bottom - rect->top, SWP_NOACTIVATE | SWP_NOZORDER);
}

// ---- CToolBar ------------------------------------------------------------------------

namespace
{

// The command UI of a toolbar button.
class CToolCmdUI : public CCmdUI
{
public:
	void Enable(BOOL on) override
	{
		m_bEnableChanged = TRUE;
		auto *bar = static_cast<CToolBar *>(m_pOther);
		UINT style = bar->GetButtonStyle(static_cast<int>(m_nIndex)) & ~TBBS_DISABLED;
		if(!on)
			style = (style | TBBS_DISABLED) & ~TBBS_PRESSED; // a disabled button is never pressed
		bar->SetButtonStyle(static_cast<int>(m_nIndex), style);
	}
	void SetCheck(int check) override
	{
		auto *bar = static_cast<CToolBar *>(m_pOther);
		UINT style = bar->GetButtonStyle(static_cast<int>(m_nIndex)) & ~(TBBS_CHECKED | TBBS_INDETERMINATE);
		if(check == 1)
			style |= TBBS_CHECKED;
		else if(check == 2)
			style |= TBBS_INDETERMINATE;
		bar->SetButtonStyle(static_cast<int>(m_nIndex), style | TBBS_CHECKBOX);
	}
	void SetText(LPCTSTR) override { }
};

} // namespace

BEGIN_MESSAGE_MAP(CToolBar, CControlBar)
	ON_WM_CREATE()
	ON_WM_NCCALCSIZE()
	ON_WM_NCPAINT()
	ON_WM_NCHITTEST()
	ON_WM_PAINT()
	ON_WM_ERASEBKGND()
	ON_MESSAGE(TB_SETBUTTONSIZE, &CToolBar::OnSetButtonSize)
	ON_MESSAGE(TB_SETBITMAPSIZE, &CToolBar::OnSetBitmapSize)
END_MESSAGE_MAP()

CToolBar::CToolBar()
{
	// A little more to grab on top and bottom.
	m_cyTopBorder = 3;
	m_cyBottomBorder = 3;
}

CToolBar::~CToolBar()
{
	DestroyWindow();
	if(m_hbmImageWell != nullptr)
		::DeleteObject(m_hbmImageWell);
}

BOOL CToolBar::Create(CWnd *parent, DWORD style, UINT id)
{
	return CreateEx(parent, 0, style, CRect(m_cxLeftBorder, m_cyTopBorder, m_cxRightBorder, m_cyBottomBorder), id);
}

BOOL CToolBar::CreateEx(CWnd *parent, DWORD ctrlStyle, DWORD style, CRect borders, UINT id)
{
	SetBorders(borders);
	m_dwStyle = style & CBRS_ALL;
	if(id == AFX_IDW_TOOLBAR)
		m_dwStyle |= CBRS_HIDE_INPLACE;
	style &= ~CBRS_ALL;
	style |= CCS_NOPARENTALIGN | CCS_NOMOVEY | CCS_NODIVIDER | CCS_NORESIZE;
	style |= ctrlStyle;
	if(!CWnd::Create(TOOLBARCLASSNAME, nullptr, style, CRect(0, 0, 0, 0), parent, id))
		return FALSE;
	SetSizes(m_sizeButton, m_sizeImage);
	return TRUE;
}

int CToolBar::OnCreate(LPCREATESTRUCT cs)
{
	if(CControlBar::OnCreate(cs) == -1)
		return -1;
	if(m_hWndOwner != nullptr)
		DefWindowProc(TB_SETPARENT, reinterpret_cast<WPARAM>(m_hWndOwner), 0);
	DefWindowProc(TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);
	return 0;
}

void CToolBar::SetOwner(CWnd *owner)
{
	if(m_hWnd != nullptr)
		DefWindowProc(TB_SETPARENT, reinterpret_cast<WPARAM>(owner != nullptr ? owner->m_hWnd : nullptr), 0);
	CControlBar::SetOwner(owner);
}

CToolBarCtrl &CToolBar::GetToolBarCtrl() const
{
	return *reinterpret_cast<CToolBarCtrl *>(const_cast<CToolBar *>(this));
}

void CToolBar::SetSizes(SIZE button, SIZE image)
{
	if(::IsWindow(m_hWnd))
	{
		SendMessage(TB_SETBITMAPSIZE, 0, MAKELONG(image.cx, image.cy));
		SendMessage(TB_SETBUTTONSIZE, 0, MAKELONG(button.cx, button.cy));
		// What the control made of it (common controls 6).
		const DWORD size = static_cast<DWORD>(SendMessage(TB_GETBUTTONSIZE, 0, 0));
		m_sizeButton.cx = LOWORD(size);
		m_sizeButton.cy = HIWORD(size);
		Invalidate();
	} else
	{
		m_sizeImage = image;
		m_sizeButton = button;
	}
}

LRESULT CToolBar::OnSetSizeHelper(CSize &size, LPARAM lp)
{
	// MFC turns TBSTYLE_TRANSPARENT on for these so that the control allows
	// a zero border.
	const DWORD style = GetStyle();
	::SetWindowLongPtr(m_hWnd, GWL_STYLE, style | TBSTYLE_TRANSPARENT);
	const LRESULT result = Default();
	if(result)
		size = CSize(LOWORD(lp), HIWORD(lp));
	::SetWindowLongPtr(m_hWnd, GWL_STYLE, style);
	return result;
}

LRESULT CToolBar::OnSetButtonSize(WPARAM, LPARAM lp)
{
	return OnSetSizeHelper(m_sizeButton, lp);
}

LRESULT CToolBar::OnSetBitmapSize(WPARAM, LPARAM lp)
{
	return OnSetSizeHelper(m_sizeImage, lp);
}

void CToolBar::SetHeight(int height)
{
	const int total = height;
	if(m_dwStyle & CBRS_BORDER_TOP)
		height -= CyBorder2;
	if(m_dwStyle & CBRS_BORDER_BOTTOM)
		height -= CyBorder2;
	m_cyBottomBorder = std::max(0, (height - static_cast<int>(m_sizeButton.cy)) / 2);
	m_cyTopBorder = std::max(0, height - static_cast<int>(m_sizeButton.cy) - m_cyBottomBorder);
	if(::IsWindow(m_hWnd))
		SetWindowPos(nullptr, 0, 0, 0, total, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_DRAWFRAME);
}

BOOL CToolBar::LoadBitmap(LPCTSTR name)
{
	HBITMAP bitmap = ::LoadBitmap(AfxGetResourceHandle(), name);
	if(bitmap == nullptr)
		return FALSE;
	if(!SetBitmap(bitmap))
	{
		::DeleteObject(bitmap);
		return FALSE;
	}
	return TRUE;
}

BOOL CToolBar::SetBitmap(HBITMAP bitmap)
{
	BITMAP bm{};
	if(bitmap == nullptr || !::GetObject(bitmap, sizeof(bm), &bm))
		return FALSE;
	TBADDBITMAP add{nullptr, reinterpret_cast<UINT_PTR>(bitmap)};
	const int images = m_sizeImage.cx > 0 ? bm.bmWidth / m_sizeImage.cx : 0;
	if(m_hbmImageWell != nullptr)
	{
		TBREPLACEBITMAP replace{};
		replace.hInstOld = nullptr;
		replace.nIDOld = reinterpret_cast<UINT_PTR>(m_hbmImageWell);
		replace.hInstNew = nullptr;
		replace.nIDNew = reinterpret_cast<UINT_PTR>(bitmap);
		replace.nButtons = images;
		if(!DefWindowProc(TB_REPLACEBITMAP, 0, reinterpret_cast<LPARAM>(&replace)))
			return FALSE;
		::DeleteObject(m_hbmImageWell);
	} else if(DefWindowProc(TB_ADDBITMAP, images, reinterpret_cast<LPARAM>(&add)) == -1)
	{
		return FALSE;
	}
	m_hbmImageWell = bitmap;
	Invalidate();
	return TRUE;
}

BOOL CToolBar::LoadToolBar(LPCTSTR name)
{
	// The RT_TOOLBAR resource: version, image size, item count, then the
	// command ids (0 for separators), all 16-bit.
	HRSRC res = ::FindResource(AfxGetResourceHandle(), name, RT_TOOLBAR);
	if(res == nullptr)
		return FALSE;
	HGLOBAL global = ::LoadResource(AfxGetResourceHandle(), res);
	const auto *data = static_cast<const WORD *>(::LockResource(global));
	if(data == nullptr || data[0] != 1)
		return FALSE;
	const int width = data[1], height = data[2], count = data[3];
	std::vector<UINT> ids(static_cast<std::size_t>(count));
	for(int i = 0; i < count; i++)
		ids[static_cast<std::size_t>(i)] = data[4 + i];
	if(!SetButtons(ids.data(), count))
		return FALSE;
	// The buttons are 7 and 7 pixels larger than their images.
	SetSizes(CSize(width + 7, height + 7), CSize(width, height));
	return LoadBitmap(name);
}

BOOL CToolBar::SetButtons(const UINT *ids, int count)
{
	int existing = static_cast<int>(DefWindowProc(TB_BUTTONCOUNT, 0, 0));
	while(existing--)
		DefWindowProc(TB_DELETEBUTTON, 0, 0);
	TBBUTTON button{};
	button.iString = -1;
	if(ids != nullptr)
	{
		int image = 0;
		for(int i = 0; i < count; i++)
		{
			button.fsState = TBSTATE_ENABLED;
			if((button.idCommand = static_cast<int>(*ids++)) == 0)
			{
				button.fsStyle = TBSTYLE_SEP;
				// The width of a separator (flat toolbars: 6).
				button.iBitmap = (GetStyle() & TBSTYLE_FLAT) ? 6 : 8;
			} else
			{
				button.fsStyle = TBSTYLE_BUTTON;
				button.iBitmap = image++;
			}
			if(!DefWindowProc(TB_ADDBUTTONS, 1, reinterpret_cast<LPARAM>(&button)))
				return FALSE;
		}
	} else
	{
		button.fsState = TBSTATE_ENABLED;
		for(int i = 0; i < count; i++)
		{
			if(!DefWindowProc(TB_ADDBUTTONS, 1, reinterpret_cast<LPARAM>(&button)))
				return FALSE;
		}
	}
	m_nCount = static_cast<int>(DefWindowProc(TB_BUTTONCOUNT, 0, 0));
	m_bDelayedButtonLayout = TRUE;
	return TRUE;
}

void CToolBar::_GetButton(int index, TBBUTTON *button) const
{
	const_cast<CToolBar *>(this)->DefWindowProc(TB_GETBUTTON, index, reinterpret_cast<LPARAM>(button));
	// TBSTATE_ENABLED is TBBS_DISABLED, the other way around.
	button->fsState ^= TBSTATE_ENABLED;
}

void CToolBar::_SetButton(int index, TBBUTTON *button)
{
	TBBUTTON old{};
	DefWindowProc(TB_GETBUTTON, index, reinterpret_cast<LPARAM>(&old));
	button->fsState ^= TBSTATE_ENABLED;
	if(old.idCommand == button->idCommand && old.iBitmap == button->iBitmap && old.fsState == button->fsState && old.fsStyle == button->fsStyle
		&& old.dwData == button->dwData && old.iString == button->iString)
		return;
	DefWindowProc(TB_DELETEBUTTON, index, 0);
	DefWindowProc(TB_INSERTBUTTON, index, reinterpret_cast<LPARAM>(button));
	if(((button->fsStyle ^ old.fsStyle) & TBSTYLE_SEP) || ((button->fsStyle & TBSTYLE_SEP) && button->iBitmap != old.iBitmap))
	{
		Invalidate(); // a separator changed
	} else
	{
		CRect rect;
		if(DefWindowProc(TB_GETITEMRECT, index, reinterpret_cast<LPARAM>(&rect)))
			InvalidateRect(rect);
	}
}

int CToolBar::CommandToIndex(UINT id) const
{
	return static_cast<int>(const_cast<CToolBar *>(this)->DefWindowProc(TB_COMMANDTOINDEX, id, 0));
}

UINT CToolBar::GetItemID(int index) const
{
	TBBUTTON button{};
	_GetButton(index, &button);
	return static_cast<UINT>(button.idCommand);
}

void CToolBar::GetItemRect(int index, LPRECT rect) const
{
	if(m_bDelayedButtonLayout)
		const_cast<CToolBar *>(this)->Layout();
	if(!const_cast<CToolBar *>(this)->DefWindowProc(TB_GETITEMRECT, index, reinterpret_cast<LPARAM>(rect)))
		::SetRectEmpty(rect);
}

void CToolBar::Layout()
{
	m_bDelayedButtonLayout = FALSE;
	const BOOL horz = (m_dwStyle & CBRS_ORIENT_HORZ) != 0;
	if((m_dwStyle & CBRS_FLOATING) && (m_dwStyle & CBRS_SIZE_DYNAMIC))
		CalcDynamicLayout(0, LM_HORZ | LM_MRUWIDTH | LM_COMMIT);
	else if(horz)
		CalcDynamicLayout(0, LM_HORZ | LM_HORZDOCK | LM_COMMIT);
	else
		CalcDynamicLayout(0, LM_VERTDOCK | LM_COMMIT);
}

UINT CToolBar::GetButtonStyle(int index) const
{
	TBBUTTON button{};
	_GetButton(index, &button);
	return MAKELONG(button.fsStyle, button.fsState);
}

void CToolBar::SetButtonStyle(int index, UINT style)
{
	TBBUTTON button{};
	_GetButton(index, &button);
	if(button.fsStyle != static_cast<BYTE>(LOWORD(style)) || button.fsState != static_cast<BYTE>(HIWORD(style)))
	{
		button.fsStyle = static_cast<BYTE>(LOWORD(style));
		button.fsState = static_cast<BYTE>(HIWORD(style));
		_SetButton(index, &button);
		m_bDelayedButtonLayout = TRUE;
	}
}

void CToolBar::GetButtonInfo(int index, UINT &id, UINT &style, int &image) const
{
	TBBUTTON button{};
	_GetButton(index, &button);
	id = static_cast<UINT>(button.idCommand);
	style = MAKELONG(button.fsStyle, button.fsState);
	image = button.iBitmap;
}

void CToolBar::SetButtonInfo(int index, UINT id, UINT style, int image)
{
	TBBUTTON button{};
	_GetButton(index, &button);
	const TBBUTTON save = button;
	button.idCommand = static_cast<int>(id);
	button.iBitmap = image;
	button.fsStyle = static_cast<BYTE>(LOWORD(style));
	button.fsState = static_cast<BYTE>(HIWORD(style));
	if(save.idCommand != button.idCommand || save.iBitmap != button.iBitmap || save.fsStyle != button.fsStyle || save.fsState != button.fsState)
	{
		_SetButton(index, &button);
		m_bDelayedButtonLayout = TRUE;
	}
}

BOOL CToolBar::SetButtonText(int index, LPCTSTR text)
{
	TBBUTTONINFO info{};
	info.cbSize = sizeof(info);
	info.dwMask = TBIF_TEXT | TBIF_BYINDEX;
	info.pszText = const_cast<LPTSTR>(text);
	if(!DefWindowProc(TB_SETBUTTONINFO, index, reinterpret_cast<LPARAM>(&info)))
		return FALSE;
	m_bDelayedButtonLayout = TRUE;
	return TRUE;
}

void CToolBar::GetButtonText(int index, CString &text) const
{
	TCHAR buffer[256] = {};
	TBBUTTONINFO info{};
	info.cbSize = sizeof(info);
	info.dwMask = TBIF_TEXT | TBIF_BYINDEX;
	info.pszText = buffer;
	info.cchText = 256;
	const_cast<CToolBar *>(this)->DefWindowProc(TB_GETBUTTONINFO, index, reinterpret_cast<LPARAM>(&info));
	text = buffer;
}

CString CToolBar::GetButtonText(int index) const
{
	CString text;
	GetButtonText(index, text);
	return text;
}

CSize CToolBar::CalcSize(TBBUTTON *data, int count)
{
	CPoint cur(0, 0);
	CSize result(0, 0);
	const DWORD exStyle = static_cast<DWORD>(DefWindowProc(TB_GETEXTENDEDSTYLE, 0, 0));
	for(int i = 0; i < count; i++)
	{
		// The space a wrapped separator leaves (2/3 of it unless flat).
		int cySep = data[i].iBitmap;
		if(!(GetStyle() & TBSTYLE_FLAT))
			cySep = cySep * 2 / 3;
		if(data[i].fsState & TBSTATE_HIDDEN)
			continue;
		int cx = m_sizeButton.cx;
		if(data[i].fsStyle & TBSTYLE_SEP)
		{
			// A separator is a height or a width.
			if(data[i].fsState & TBSTATE_WRAP)
				result.cy = std::max<LONG>(cur.y + m_sizeButton.cy + cySep, result.cy);
			else
				result.cx = std::max<LONG>(cur.x + data[i].iBitmap, result.cx);
		} else
		{
			if((data[i].fsStyle & TBSTYLE_DROPDOWN) && (exStyle & TBSTYLE_EX_DRAWDDARROWS))
				cx += DropDownWidth;
			result.cx = std::max<LONG>(cur.x + cx, result.cx);
			result.cy = std::max<LONG>(cur.y + m_sizeButton.cy, result.cy);
		}
		if(data[i].fsStyle & TBSTYLE_SEP)
			cur.x += data[i].iBitmap;
		else
			cur.x += cx;
		if(data[i].fsState & TBSTATE_WRAP)
		{
			cur.x = 0;
			cur.y += m_sizeButton.cy;
			if(data[i].fsStyle & TBSTYLE_SEP)
				cur.y += cySep;
		}
	}
	return result;
}

int CToolBar::WrapToolBar(TBBUTTON *data, int count, int width)
{
	const DWORD exStyle = static_cast<DWORD>(DefWindowProc(TB_GETEXTENDEDSTYLE, 0, 0));
	int wraps = 0;
	int x = 0;
	for(int i = 0; i < count; i++)
	{
		data[i].fsState &= ~TBSTATE_WRAP;
		if(data[i].fsState & TBSTATE_HIDDEN)
			continue;
		int dx, dxNext;
		if(data[i].fsStyle & TBSTYLE_SEP)
		{
			dx = data[i].iBitmap;
			dxNext = dx;
		} else
		{
			dx = m_sizeButton.cx;
			if((data[i].fsStyle & TBSTYLE_DROPDOWN) && (exStyle & TBSTYLE_EX_DRAWDDARROWS))
				dx += DropDownWidth;
			dxNext = dx;
		}
		if(x + dx > width)
		{
			bool found = false;
			// The last separator that is visible (and not a control).
			for(int j = i; j >= 0 && !(data[j].fsState & TBSTATE_WRAP); j--)
			{
				if((data[j].fsStyle & TBSTYLE_SEP) && data[j].idCommand == 0 && !(data[j].fsState & TBSTATE_HIDDEN))
				{
					found = true;
					i = j;
					x = 0;
					data[j].fsState |= TBSTATE_WRAP;
					wraps++;
					break;
				}
			}
			if(!found)
			{
				for(int j = i - 1; j >= 0 && !(data[j].fsState & TBSTATE_WRAP); j--)
				{
					// Never after something hidden or a control.
					if((data[j].fsState & TBSTATE_HIDDEN) || ((data[j].fsStyle & TBSTYLE_SEP) && data[j].idCommand != 0))
						continue;
					found = true;
					i = j;
					x = 0;
					data[j].fsState |= TBSTATE_WRAP;
					wraps++;
					break;
				}
				if(!found)
					x += dxNext;
			}
		} else
		{
			x += dxNext;
		}
	}
	return wraps + 1;
}

void CToolBar::SizeToolBar(TBBUTTON *data, int count, int length, BOOL vert)
{
	if(!vert)
	{
		// As few rows as length allows, then as narrow as possible with them.
		int maxWidth = length;
		const int target = WrapToolBar(data, count, maxWidth);
		int minWidth = 0;
		int current = WrapToolBar(data, count, minWidth);
		if(current != target)
		{
			while(minWidth < maxWidth)
			{
				const int mid = (minWidth + maxWidth) / 2;
				current = WrapToolBar(data, count, mid);
				if(current == target)
				{
					maxWidth = mid;
				} else
				{
					if(minWidth == mid)
					{
						WrapToolBar(data, count, maxWidth);
						break;
					}
					minWidth = mid;
				}
			}
		}
		const CSize size = CalcSize(data, count);
		WrapToolBar(data, count, size.cx);
	} else
	{
		WrapToolBar(data, count, 0);
		CSize sizeMin = CalcSize(data, count);
		WrapToolBar(data, count, 32767);
		CSize sizeMax = CalcSize(data, count);
		while(sizeMin.cx < sizeMax.cx)
		{
			CSize sizeMid;
			sizeMid.cx = (sizeMin.cx + sizeMax.cx) / 2;
			WrapToolBar(data, count, sizeMid.cx);
			sizeMid = CalcSize(data, count);
			if(length < sizeMid.cy)
			{
				if(sizeMin == sizeMid)
				{
					WrapToolBar(data, count, sizeMax.cx);
					return;
				}
				sizeMin = sizeMid;
			} else if(length > sizeMid.cy)
			{
				sizeMax = sizeMid;
			} else
			{
				return;
			}
		}
	}
}

CSize CToolBar::CalcLayout(DWORD mode, int length)
{
	CSize result(0, 0);
	const int count = static_cast<int>(DefWindowProc(TB_BUTTONCOUNT, 0, 0));
	if(count > 0)
	{
		std::vector<TBBUTTON> data(static_cast<std::size_t>(count));
		for(int i = 0; i < count; i++)
			_GetButton(i, &data[static_cast<std::size_t>(i)]);
		if(!(m_dwStyle & CBRS_SIZE_FIXED))
		{
			const bool dynamic = (m_dwStyle & CBRS_SIZE_DYNAMIC) != 0;
			if(dynamic && (mode & LM_MRUWIDTH))
				SizeToolBar(data.data(), count, static_cast<int>(m_nMRUWidth));
			else if(dynamic && (mode & LM_HORZDOCK))
				SizeToolBar(data.data(), count, 32767);
			else if(dynamic && (mode & LM_VERTDOCK))
				SizeToolBar(data.data(), count, 0);
			else if(dynamic && length != -1)
			{
				CRect rect(0, 0, 0, 0);
				CalcInsideRect(rect, (mode & LM_HORZ) != 0);
				const BOOL vert = (mode & LM_LENGTHY) != 0;
				SizeToolBar(data.data(), count, length + (vert ? rect.Height() : rect.Width()), vert);
			} else if(dynamic && (m_dwStyle & CBRS_FLOATING))
				SizeToolBar(data.data(), count, static_cast<int>(m_nMRUWidth));
			else
				SizeToolBar(data.data(), count, (mode & LM_HORZ) ? 32767 : 0);
		}
		result = CalcSize(data.data(), count);
		if(mode & LM_COMMIT)
		{
			// Controls on the bar (separators with an id) move with their
			// places.
			struct ControlPos
			{
				int index;
				int id;
				CRect oldPos;
			};
			std::vector<ControlPos> controls;
			const BOOL delayed = m_bDelayedButtonLayout;
			m_bDelayedButtonLayout = FALSE;
			for(int i = 0; i < count; i++)
			{
				if((data[static_cast<std::size_t>(i)].fsStyle & TBSTYLE_SEP) && data[static_cast<std::size_t>(i)].idCommand != 0)
				{
					CRect rect;
					GetItemRect(i, &rect);
					ClientToScreen(&rect);
					controls.push_back({i, data[static_cast<std::size_t>(i)].idCommand, rect});
				}
			}
			if((m_dwStyle & CBRS_FLOATING) && (m_dwStyle & CBRS_SIZE_DYNAMIC))
				m_nMRUWidth = static_cast<UINT>(result.cx);
			for(int i = 0; i < count; i++)
				_SetButton(i, &data[static_cast<std::size_t>(i)]);
			for(const ControlPos &c : controls)
			{
				CWnd *wnd = GetDlgItem(c.id);
				if(wnd == nullptr)
					continue;
				CRect rect;
				wnd->GetWindowRect(&rect);
				CPoint pt = rect.TopLeft() - c.oldPos.TopLeft();
				GetItemRect(c.index, &rect);
				pt = rect.TopLeft() + pt;
				wnd->SetWindowPos(nullptr, pt.x, pt.y, 0, 0, SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOZORDER);
			}
			m_bDelayedButtonLayout = delayed;
		}
	}
	// The borders around the buttons.
	CRect rect(0, 0, 0, 0);
	CalcInsideRect(rect, (mode & LM_HORZ) != 0);
	result.cy -= rect.Height();
	result.cx -= rect.Width();
	const CSize size = CControlBar::CalcFixedLayout((mode & LM_STRETCH) != 0, (mode & LM_HORZ) != 0);
	result.cx = std::max(result.cx, size.cx);
	result.cy = std::max(result.cy, size.cy);
	return result;
}

CSize CToolBar::CalcFixedLayout(BOOL stretch, BOOL horz)
{
	return CalcLayout((stretch ? LM_STRETCH : 0) | (horz ? LM_HORZ : 0));
}

CSize CToolBar::CalcDynamicLayout(int length, DWORD mode)
{
	if(length == -1 && !(mode & LM_MRUWIDTH) && !(mode & LM_COMMIT) && ((mode & LM_HORZDOCK) || (mode & LM_VERTDOCK)))
		return CalcFixedLayout((mode & LM_STRETCH) != 0, (mode & LM_HORZDOCK) != 0);
	return CalcLayout(mode, length);
}

void CToolBar::OnBarStyleChange(DWORD oldStyle, DWORD newStyle)
{
	if(m_hWnd != nullptr && ((oldStyle & CBRS_BORDER_ANY) != (newStyle & CBRS_BORDER_ANY)))
		SetWindowPos(nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_DRAWFRAME);
	m_bDelayedButtonLayout = TRUE;
}

void CToolBar::OnNcCalcSize(BOOL, NCCALCSIZE_PARAMS *params)
{
	CRect rect(0, 0, 0, 0);
	CControlBar::CalcInsideRect(rect, (m_dwStyle & CBRS_ORIENT_HORZ) != 0);
	params->rgrc[0].left += rect.left;
	params->rgrc[0].top += rect.top;
	params->rgrc[0].right += rect.right;
	params->rgrc[0].bottom += rect.bottom;
}

void CToolBar::OnNcPaint()
{
	EraseNonClient();
}

LRESULT CToolBar::OnNcHitTest(CPoint)
{
	// The borders and gripper are part of the bar for the mouse: a press
	// there drags it.
	return HTCLIENT;
}

void CToolBar::OnPaint()
{
	if(m_bDelayedButtonLayout)
		Layout();
	Default();
}

BOOL CToolBar::OnEraseBkgnd(CDC *)
{
	return static_cast<BOOL>(Default());
}

void CToolBar::OnUpdateCmdUI(CFrameWnd *target, BOOL disableIfNoHandler)
{
	CToolCmdUI state;
	state.m_pOther = this;
	state.m_nIndexMax = static_cast<UINT>(DefWindowProc(TB_BUTTONCOUNT, 0, 0));
	for(state.m_nIndex = 0; state.m_nIndex < state.m_nIndexMax; state.m_nIndex++)
	{
		TBBUTTON button{};
		_GetButton(static_cast<int>(state.m_nIndex), &button);
		state.m_nID = static_cast<UINT>(button.idCommand);
		if(button.fsStyle & TBSTYLE_SEP)
			continue;
		// Reflected handlers, then the bar's own, then the target's.
		if(CWnd::OnCmdMsg(0, MAKELONG(static_cast<int>(CN_UPDATE_COMMAND_UI), WM_COMMAND + WM_REFLECT_BASE), &state, nullptr))
			continue;
		if(CWnd::OnCmdMsg(state.m_nID, static_cast<int>(CN_UPDATE_COMMAND_UI), &state, nullptr))
			continue;
		state.DoUpdate(target, disableIfNoHandler);
	}
	// Controls on the bar.
	UpdateDialogControls(target, disableIfNoHandler);
}

INT_PTR CToolBar::OnToolHitTest(CPoint point, TOOLINFO *ti) const
{
	// Child windows (controls on the bar) first.
	const INT_PTR childHit = CControlBar::OnToolHitTest(point, ti);
	if(childHit != -1)
		return childHit;
	auto *self = const_cast<CToolBar *>(this);
	const int buttons = static_cast<int>(self->DefWindowProc(TB_BUTTONCOUNT, 0, 0));
	for(int i = 0; i < buttons; i++)
	{
		CRect rect;
		TBBUTTON button{};
		if(!self->DefWindowProc(TB_GETITEMRECT, i, reinterpret_cast<LPARAM>(&rect)))
			continue;
		++rect.bottom;
		++rect.right;
		if(rect.PtInRect(point) && self->DefWindowProc(TB_GETBUTTON, i, reinterpret_cast<LPARAM>(&button)) && !(button.fsStyle & TBSTYLE_SEP))
		{
			const INT_PTR hit = static_cast<INT_PTR>(GetItemID(i));
			if(ti != nullptr)
			{
				ti->hwnd = m_hWnd;
				ti->rect = rect;
				ti->uId = static_cast<UINT_PTR>(hit);
				ti->lpszText = LPSTR_TEXTCALLBACK;
			}
			return hit != 0 ? hit : -1;
		}
	}
	return -1;
}

// ---- CStatusBar ----------------------------------------------------------------------

namespace
{

class CStatusCmdUI : public CCmdUI
{
public:
	void Enable(BOOL on) override
	{
		m_bEnableChanged = TRUE;
		auto *bar = static_cast<CStatusBar *>(m_pOther);
		UINT style = bar->GetPaneStyle(static_cast<int>(m_nIndex)) & ~SBPS_DISABLED;
		if(!on)
			style |= SBPS_DISABLED;
		bar->SetPaneStyle(static_cast<int>(m_nIndex), style);
	}
	void SetCheck(int check) override
	{
		// Checking pops the text out.
		auto *bar = static_cast<CStatusBar *>(m_pOther);
		UINT style = bar->GetPaneStyle(static_cast<int>(m_nIndex)) & ~SBPS_POPOUT;
		if(check != 0)
			style |= SBPS_POPOUT;
		bar->SetPaneStyle(static_cast<int>(m_nIndex), style);
	}
	void SetText(LPCTSTR text) override
	{
		static_cast<CStatusBar *>(m_pOther)->SetPaneText(static_cast<int>(m_nIndex), text);
	}
};

} // namespace

BEGIN_MESSAGE_MAP(CStatusBar, CControlBar)
	ON_WM_NCHITTEST()
	ON_WM_NCPAINT()
	ON_WM_PAINT()
	ON_WM_NCCALCSIZE()
	ON_WM_SIZE()
	ON_MESSAGE(WM_SETTEXT, &CStatusBar::OnSetText)
	ON_MESSAGE(WM_GETTEXT, &CStatusBar::OnGetText)
	ON_MESSAGE(WM_GETTEXTLENGTH, &CStatusBar::OnGetTextLength)
	ON_MESSAGE(SB_SETMINHEIGHT, &CStatusBar::OnSetMinHeight)
END_MESSAGE_MAP()

CStatusBar::CStatusBar()
{
	m_cyTopBorder = 2;
	m_cxLeftBorder = 0;
	m_cxRightBorder = 0;
	m_cyBottomBorder = 0;
}

CStatusBar::~CStatusBar()
{
	DestroyWindow();
}

BOOL CStatusBar::Create(CWnd *parent, DWORD style, UINT id)
{
	return CreateEx(parent, 0, style, id);
}

BOOL CStatusBar::CreateEx(CWnd *parent, DWORD ctrlStyle, DWORD style, UINT id)
{
	m_dwStyle = style & CBRS_ALL;
	style &= ~CBRS_ALL;
	style |= CCS_NOPARENTALIGN | CCS_NOMOVEY | CCS_NODIVIDER | CCS_NORESIZE;
	if(parent->GetStyle() & WS_THICKFRAME)
		style |= SBARS_SIZEGRIP;
	style |= ctrlStyle;
	return CWnd::Create(STATUSCLASSNAME, nullptr, style, CRect(0, 0, 0, 0), parent, id);
}

BOOL CStatusBar::PreCreateWindow(CREATESTRUCT &cs)
{
	// A status bar at the bottom has no border: the frame has it.
	if((m_dwStyle & (CBRS_ALIGN_ANY | CBRS_BORDER_ANY)) == CBRS_BOTTOM)
		m_dwStyle &= ~(CBRS_BORDER_ANY | CBRS_BORDER_3D);
	return CControlBar::PreCreateWindow(cs);
}

CStatusBarCtrl &CStatusBar::GetStatusBarCtrl() const
{
	return *reinterpret_cast<CStatusBarCtrl *>(const_cast<CStatusBar *>(this));
}

BOOL CStatusBar::SetIndicators(const UINT *ids, int count)
{
	m_panes.assign(static_cast<std::size_t>(std::max(count, 0)), AFX_STATUSPANE{});
	m_nCount = count;
	BOOL ok = TRUE;
	if(ids != nullptr)
	{
		const HFONT font = reinterpret_cast<HFONT>(SendMessage(WM_GETFONT));
		CClientDC dc(nullptr);
		HGDIOBJ oldFont = font != nullptr ? ::SelectObject(dc.m_hDC, font) : nullptr;
		for(int i = 0; i < count; i++)
		{
			AFX_STATUSPANE &pane = m_panes[static_cast<std::size_t>(i)];
			pane.nID = *ids++;
			pane.nFlags |= SbpfUpdate;
			if(pane.nID != 0)
			{
				if(!LinOpenMPTLoadMfcString(pane.nID, pane.strText))
				{
					ok = FALSE;
					break;
				}
				pane.cxText = dc.GetTextExtent(pane.strText).cx;
				if(!SetPaneText(i, pane.strText, FALSE))
				{
					ok = FALSE;
					break;
				}
			} else
			{
				// No indicator: a quarter of the screen, and the first one
				// stretches.
				pane.cxText = ::GetSystemMetrics(SM_CXSCREEN) / 4;
				if(i == 0)
					pane.nStyle |= SBPS_STRETCH | SBPS_NOBORDERS;
			}
		}
		if(oldFont != nullptr)
			::SelectObject(dc.m_hDC, oldFont);
	}
	UpdateAllPanes(TRUE, TRUE);
	return ok;
}

void CStatusBar::CalcInsideRect(CRect &rect, BOOL horz) const
{
	CControlBar::CalcInsideRect(rect, horz);
	// The size grip.
	if((GetStyle() & SBARS_SIZEGRIP) && !::IsZoomed(::GetParent(m_hWnd)))
	{
		int borders[3] = {};
		const_cast<CStatusBar *>(this)->DefWindowProc(SB_GETBORDERS, 0, reinterpret_cast<LPARAM>(borders));
		rect.right -= borders[0] + ::GetSystemMetrics(SM_CXVSCROLL) + ::GetSystemMetrics(SM_CXBORDER) * 2;
	}
}

void CStatusBar::UpdateAllPanes(BOOL updateRects, BOOL updateText)
{
	if(updateRects && m_nCount > 0)
	{
		CRect rect;
		GetWindowRect(rect);
		rect.OffsetRect(-rect.left, -rect.top);
		CalcInsideRect(rect, TRUE);
		int borders[3] = {};
		DefWindowProc(SB_GETBORDERS, 0, reinterpret_cast<LPARAM>(borders));
		// The space left over for stretching panes.
		int extra = rect.Width() + borders[2];
		int stretchy = 0;
		for(const AFX_STATUSPANE &pane : m_panes)
		{
			if(pane.nStyle & SBPS_STRETCH)
				++stretchy;
			extra -= pane.cxText + CxPaneBorder + borders[2];
		}
		std::vector<int> rights(m_panes.size());
		int right = borders[0];
		for(std::size_t i = 0; i < m_panes.size(); i++)
		{
			const AFX_STATUSPANE &pane = m_panes[i];
			right += pane.cxText + CxPaneBorder;
			if((pane.nStyle & SBPS_STRETCH) && extra > 0)
			{
				const int add = extra / stretchy;
				right += add;
				--stretchy;
				extra -= add;
			}
			rights[i] = right;
			right += borders[2];
		}
		DefWindowProc(SB_SETPARTS, static_cast<WPARAM>(m_nCount), reinterpret_cast<LPARAM>(rights.data()));
	}
	if(updateText)
	{
		for(int i = 0; i < m_nCount; i++)
		{
			if(m_panes[static_cast<std::size_t>(i)].nFlags & SbpfUpdate)
				SetPaneText(i, m_panes[static_cast<std::size_t>(i)].strText);
		}
	}
}

BOOL CStatusBar::SetPaneText(int index, LPCTSTR text, BOOL update)
{
	if(index < 0 || index >= m_nCount)
		return FALSE;
	AFX_STATUSPANE &pane = m_panes[static_cast<std::size_t>(index)];
	if(!(pane.nFlags & SbpfUpdate) && ((text == nullptr && pane.strText.IsEmpty()) || (text != nullptr && pane.strText.Compare(text) == 0)))
		return TRUE; // nothing changes
	if(text != pane.strText.GetString())
	{
		if(text != nullptr)
			pane.strText = text;
		else
			pane.strText.Empty();
	}
	if(!update)
	{
		pane.nFlags |= SbpfUpdate;
		return TRUE;
	}
	pane.nFlags &= ~SbpfUpdate;
	DefWindowProc(SB_SETTEXTW, static_cast<WORD>(pane.nStyle) | static_cast<UINT>(index),
		(pane.nStyle & SBPS_DISABLED) ? 0 : reinterpret_cast<LPARAM>(pane.strText.GetString()));
	return TRUE;
}

CString CStatusBar::GetPaneText(int index) const
{
	if(index < 0 || index >= m_nCount)
		return CString();
	return m_panes[static_cast<std::size_t>(index)].strText;
}

void CStatusBar::GetPaneText(int index, CString &text) const
{
	text = GetPaneText(index);
}

void CStatusBar::SetPaneInfo(int index, UINT id, UINT style, int width)
{
	if(index < 0 || index >= m_nCount)
		return;
	bool changed = false;
	AFX_STATUSPANE &pane = m_panes[static_cast<std::size_t>(index)];
	pane.nID = id;
	if(pane.nStyle != style)
	{
		if((pane.nStyle ^ style) & SBPS_STRETCH)
		{
			changed = true;
		} else
		{
			pane.nStyle = style;
			pane.nFlags |= SbpfUpdate;
			SetPaneText(index, pane.strText);
		}
		pane.nStyle = style;
	}
	if(width != pane.cxText)
	{
		pane.cxText = width;
		changed = true;
	}
	if(changed)
		UpdateAllPanes(TRUE, FALSE);
}

void CStatusBar::GetPaneInfo(int index, UINT &id, UINT &style, int &width) const
{
	const AFX_STATUSPANE &pane = m_panes.at(static_cast<std::size_t>(index));
	id = pane.nID;
	style = pane.nStyle;
	width = pane.cxText;
}

UINT CStatusBar::GetPaneStyle(int index) const
{
	return m_panes.at(static_cast<std::size_t>(index)).nStyle;
}

void CStatusBar::SetPaneStyle(int index, UINT style)
{
	AFX_STATUSPANE &pane = m_panes.at(static_cast<std::size_t>(index));
	if(pane.nStyle == style)
		return;
	if((pane.nStyle ^ style) & SBPS_STRETCH)
	{
		pane.nStyle = style;
		UpdateAllPanes(TRUE, FALSE);
	}
	pane.nStyle = style;
	pane.nFlags |= SbpfUpdate;
	SetPaneText(index, pane.strText);
}

int CStatusBar::CommandToIndex(UINT id) const
{
	for(int i = 0; i < m_nCount; i++)
	{
		if(m_panes[static_cast<std::size_t>(i)].nID == id)
			return i;
	}
	return -1;
}

UINT CStatusBar::GetItemID(int index) const
{
	return m_panes.at(static_cast<std::size_t>(index)).nID;
}

void CStatusBar::GetItemRect(int index, LPRECT rect) const
{
	if(!const_cast<CStatusBar *>(this)->DefWindowProc(SB_GETRECT, index, reinterpret_cast<LPARAM>(rect)))
		::SetRectEmpty(rect);
}

CSize CStatusBar::CalcFixedLayout(BOOL, BOOL horz)
{
	TEXTMETRIC tm{};
	{
		CClientDC dc(nullptr);
		const HFONT font = reinterpret_cast<HFONT>(SendMessage(WM_GETFONT));
		HGDIOBJ old = font != nullptr ? ::SelectObject(dc.m_hDC, font) : nullptr;
		::GetTextMetrics(dc.m_hDC, &tm);
		if(old != nullptr)
			::SelectObject(dc.m_hDC, old);
	}
	CRect rect(0, 0, 0, 0);
	CalcInsideRect(rect, horz);
	int borders[3] = {};
	DefWindowProc(SB_GETBORDERS, 0, reinterpret_cast<LPARAM>(borders));
	CSize size;
	size.cx = 32767;
	size.cy = tm.tmHeight - tm.tmInternalLeading - 1 + borders[1] * 2 + ::GetSystemMetrics(SM_CYBORDER) * 2 - rect.Height();
	if(size.cy < m_nMinHeight)
		size.cy = m_nMinHeight;
	return size;
}

void CStatusBar::OnNcCalcSize(BOOL, NCCALCSIZE_PARAMS *params)
{
	CRect rect(0, 0, 0, 0);
	CControlBar::CalcInsideRect(rect, TRUE);
	params->rgrc[0].left += rect.left;
	params->rgrc[0].top += rect.top - 2;
	params->rgrc[0].right += rect.right;
	params->rgrc[0].bottom += rect.bottom;
}

void CStatusBar::OnBarStyleChange(DWORD oldStyle, DWORD newStyle)
{
	if(m_hWnd != nullptr && ((oldStyle & CBRS_BORDER_ANY) != (newStyle & CBRS_BORDER_ANY)))
		SetWindowPos(nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_DRAWFRAME);
}

void CStatusBar::OnNcPaint()
{
	EraseNonClient();
}

LRESULT CStatusBar::OnNcHitTest(CPoint point)
{
	const LRESULT hit = CControlBar::OnNcHitTest(point);
	return hit == HTBOTTOMRIGHT ? HTBOTTOMRIGHT : HTCLIENT;
}

BOOL CStatusBar::OnChildNotify(UINT msg, WPARAM wp, LPARAM lp, LRESULT *result)
{
	if(msg != WM_DRAWITEM)
		return CControlBar::OnChildNotify(msg, wp, lp, result);
	DrawItem(reinterpret_cast<LPDRAWITEMSTRUCT>(lp));
	return TRUE;
}

void CStatusBar::OnPaint()
{
	UpdateAllPanes(FALSE, TRUE);
	Default();
}

void CStatusBar::OnSize(UINT type, int cx, int cy)
{
	CControlBar::OnSize(type, cx, cy);
	// The stretching pane follows the width.
	UpdateAllPanes(TRUE, FALSE);
}

LRESULT CStatusBar::OnSetText(WPARAM, LPARAM lp)
{
	const int index = CommandToIndex(0);
	if(index < 0)
		return -1;
	return SetPaneText(index, reinterpret_cast<LPCTSTR>(lp)) ? 0 : -1;
}

LRESULT CStatusBar::OnGetText(WPARAM wp, LPARAM lp)
{
	const int max = static_cast<int>(wp);
	if(max == 0)
		return 0;
	auto *dest = reinterpret_cast<LPTSTR>(lp);
	int length = 0;
	const int index = CommandToIndex(0);
	if(index >= 0)
	{
		const CString &text = m_panes[static_cast<std::size_t>(index)].strText;
		length = std::min(text.GetLength(), max - 1);
		std::wmemcpy(dest, text.GetString(), static_cast<std::size_t>(length));
	}
	dest[length] = 0;
	return length;
}

LRESULT CStatusBar::OnGetTextLength(WPARAM, LPARAM)
{
	const int index = CommandToIndex(0);
	return index >= 0 ? m_panes[static_cast<std::size_t>(index)].strText.GetLength() : 0;
}

LRESULT CStatusBar::OnSetMinHeight(WPARAM wp, LPARAM)
{
	const LRESULT result = Default();
	m_nMinHeight = static_cast<int>(wp);
	return result;
}

void CStatusBar::OnUpdateCmdUI(CFrameWnd *target, BOOL disableIfNoHandler)
{
	CStatusCmdUI state;
	state.m_pOther = this;
	state.m_nIndexMax = static_cast<UINT>(m_nCount);
	for(state.m_nIndex = 0; state.m_nIndex < state.m_nIndexMax; state.m_nIndex++)
	{
		state.m_nID = m_panes[state.m_nIndex].nID;
		if(CWnd::OnCmdMsg(state.m_nID, static_cast<int>(CN_UPDATE_COMMAND_UI), &state, nullptr))
			continue;
		state.DoUpdate(target, FALSE);
	}
	UpdateDialogControls(target, disableIfNoHandler);
}

// The status bar common control, through MFC's CStatusBarCtrl.
BOOL CStatusBarCtrl::SetParts(int parts, int *widths)
{
	return static_cast<BOOL>(SendMessage(SB_SETPARTS, parts, reinterpret_cast<LPARAM>(widths)));
}

BOOL CStatusBarCtrl::SetText(LPCTSTR text, int pane, int type)
{
	return static_cast<BOOL>(SendMessage(SB_SETTEXTW, static_cast<WPARAM>(pane | type), reinterpret_cast<LPARAM>(text)));
}

BOOL CStatusBarCtrl::SetMinHeight(int height)
{
	SendMessage(SB_SETMINHEIGHT, height);
	return TRUE;
}

// ---- CDialogBar ----------------------------------------------------------------------

BEGIN_MESSAGE_MAP(CDialogBar, CControlBar)
	ON_MESSAGE(WM_INITDIALOG, &CDialogBar::HandleInitDialog)
END_MESSAGE_MAP()

CDialogBar::CDialogBar() = default;

CDialogBar::~CDialogBar()
{
	DestroyWindow();
}

BOOL CDialogBar::Create(CWnd *parent, LPCTSTR templateName, UINT style, UINT id)
{
	m_dwStyle = style & CBRS_ALL;
	CREATESTRUCT cs{};
	cs.style = static_cast<LONG>(style | WS_CHILD);
	cs.hMenu = reinterpret_cast<HMENU>(static_cast<UINT_PTR>(id));
	cs.hInstance = AfxGetInstanceHandle();
	cs.hwndParent = parent->GetSafeHwnd();
	if(!PreCreateWindow(cs))
		return FALSE;
	m_lpszTemplateName = templateName;
	const BOOL ok = CreateDlg(templateName, parent);
	m_lpszTemplateName = nullptr;
	if(!ok)
		return FALSE;
	// The template says it is an invisible child; its size is the bar's.
	::SetWindowLongPtr(m_hWnd, GWLP_ID, static_cast<LONG_PTR>(id));
	CRect rect;
	GetWindowRect(&rect);
	m_sizeDefault = rect.Size();
	ModifyStyle(0, WS_CLIPSIBLINGS);
	if(!ExecuteDlgInit(templateName))
		return FALSE;
	// Sized by the frame's layout later.
	SetWindowPos(nullptr, 0, 0, 0, 0, SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);
	return TRUE;
}

CSize CDialogBar::CalcFixedLayout(BOOL stretch, BOOL horz)
{
	if(stretch)
		return CSize(horz ? 32767 : m_sizeDefault.cx, horz ? m_sizeDefault.cy : 32767);
	return m_sizeDefault;
}

void CDialogBar::OnUpdateCmdUI(CFrameWnd *target, BOOL disableIfNoHandler)
{
	UpdateDialogControls(target, disableIfNoHandler);
}

LRESULT CDialogBar::HandleInitDialog(WPARAM, LPARAM)
{
	Default();
	return TRUE;
}

// ---- CFrameWnd: flyby help, the message bar and its indicators ---------------------------

LRESULT CFrameWnd::OnSetMessageString(WPARAM wp, LPARAM lp)
{
	const UINT last = m_nIDLastMessage;
	m_nFlags &= ~WF_NOPOPMSG;
	if(CWnd *bar = GetMessageBar())
	{
		LPCTSTR text = nullptr;
		CString message;
		if(lp != 0)
		{
			text = reinterpret_cast<LPCTSTR>(lp);
		} else if(wp != 0)
		{
			GetMessageString(static_cast<UINT>(wp), message);
			text = message;
		}
		bar->SetWindowText(text);
		if(CFrameWnd *frame = bar->GetParentFrame())
		{
			frame->m_nIDLastMessage = static_cast<UINT>(wp);
			frame->m_nIDTracking = static_cast<UINT>(wp);
		}
	}
	m_nIDLastMessage = static_cast<UINT>(wp);
	m_nIDTracking = static_cast<UINT>(wp);
	return last;
}

LRESULT CFrameWnd::OnPopMessageString(WPARAM wp, LPARAM lp)
{
	if(m_nFlags & WF_NOPOPMSG)
		return 0;
	return SendMessage(WM_SETMESSAGESTRING, wp, lp);
}

void CFrameWnd::GetMessageString(UINT id, CString &message) const
{
	// The prompt is what comes before the first newline.
	CString full;
	if(LinOpenMPTLoadMfcString(id, full))
	{
		const int newline = full.Find(_T('\n'));
		message = newline >= 0 ? full.Left(newline) : full;
	} else
	{
		message.Empty();
	}
}

CWnd *CFrameWnd::GetMessageBar()
{
	return GetDescendantWindow(AFX_IDW_STATUS_BAR, TRUE);
}

void CFrameWnd::SetMessageText(LPCTSTR text)
{
	SendMessage(WM_SETMESSAGESTRING, 0, reinterpret_cast<LPARAM>(text));
}

void CFrameWnd::SetMessageText(UINT id)
{
	SendMessage(WM_SETMESSAGESTRING, id);
}

void CFrameWnd::OnMenuSelect(UINT item, UINT flags, HMENU)
{
	CFrameWnd *top = GetTopLevelFrame();
	if(flags == 0xFFFF)
	{
		// The menu closed: back to idle.
		m_nFlags &= ~WF_NOPOPMSG;
		m_nIDTracking = AFX_IDS_IDLEMESSAGE;
		SendMessage(WM_SETMESSAGESTRING, m_nIDTracking);
		if(CWnd *bar = GetMessageBar())
			bar->UpdateWindow();
	} else
	{
		if(item == 0 || (flags & (MF_SEPARATOR | MF_POPUP)))
			m_nIDTracking = 0;
		else if(item >= 0xF000 && item < 0xF1F0)
			m_nIDTracking = ((item - 0xF000) >> 4) + 0xEF00; // AFX_IDS_SCFIRST + SC index
		else if(item >= 0xFF00)
			m_nIDTracking = 0xEF1F; // AFX_IDS_MDICHILD
		else
			m_nIDTracking = item;
		if(top != nullptr)
			top->m_nFlags |= WF_NOPOPMSG;
	}
	// MFC shows the prompt at idle (WM_ENTERIDLE) while the menu runs its
	// loop; the menus here run Qt's, so it shows now.
	if(m_nIDTracking != m_nIDLastMessage)
		SetMessageText(m_nIDTracking);
}

void CFrameWnd::OnEnterIdle(UINT why, CWnd *)
{
	if(why != MSGF_MENU || m_nIDTracking == m_nIDLastMessage)
		return;
	SetMessageText(m_nIDTracking);
}

void CFrameWnd::OnUpdateKeyIndicator(CCmdUI *cmdUI)
{
	int key = 0;
	switch(cmdUI->m_nID)
	{
	case ID_INDICATOR_CAPS: key = VK_CAPITAL; break;
	case ID_INDICATOR_NUM: key = VK_NUMLOCK; break;
	case ID_INDICATOR_SCRL: key = VK_SCROLL; break;
	default:
		cmdUI->ContinueRouting();
		return;
	}
	// Shown while the lock is on.
	cmdUI->Enable((::GetKeyState(key) & 1) != 0);
}

BOOL CFrameWnd::OnToolTipText(UINT, NMHDR *nmhdr, LRESULT *result)
{
	auto *ttt = reinterpret_cast<TOOLTIPTEXTW *>(nmhdr);
	UINT_PTR id = nmhdr->idFrom;
	if(ttt->uFlags & TTF_IDISHWND)
		id = static_cast<UINT_PTR>(::GetDlgCtrlID(reinterpret_cast<HWND>(id)));
	CString tip;
	if(id != 0)
	{
		// The tip is what comes after the prompt's newline.
		CString full;
		if(!full.LoadString(static_cast<UINT>(id)))
			return FALSE;
		AfxExtractSubString(tip, full, 1, _T('\n'));
	}
	const int length = std::min(tip.GetLength(), static_cast<int>(std::size(ttt->szText)) - 1);
	std::wmemcpy(ttt->szText, tip.GetString(), static_cast<std::size_t>(length));
	ttt->szText[length] = 0;
	*result = 0;
	::SetWindowPos(nmhdr->hwndFrom, HWND_TOP, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOMOVE | SWP_NOOWNERZORDER);
	return TRUE;
}
