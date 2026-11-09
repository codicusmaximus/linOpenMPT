// linOpenMPT MFC layer: CToolTipCtrl, and MFC's own tooltips for windows
// that call EnableToolTips (tooltip.cpp and wincore.cpp in MFC): one tooltip
// per thread, fed by PreTranslateMessage, with OnToolHitTest saying which
// tool the mouse is on and TTN_NEEDTEXT asking the window for the text.
#include "MfcInternal.h"

#include <cstdlib>

namespace
{

struct ToolTipThreadState
{
	CToolTipCtrl *tip = nullptr;
	HWND lastWnd = nullptr;
	INT_PTR lastHit = -1;
	TOOLINFO lastInfo{};
};

ToolTipThreadState &TipState()
{
	static thread_local ToolTipThreadState state;
	return state;
}

// A child window under a point of the parent's client area, visible ones
// only but disabled ones too (WindowFromPoint would skip those).
HWND ChildFromPoint(HWND parent, POINT pt)
{
	::ClientToScreen(parent, &pt);
	for(HWND child = ::GetWindow(parent, GW_CHILD); child != nullptr; child = ::GetWindow(child, GW_HWNDNEXT))
	{
		if(static_cast<WORD>(::GetDlgCtrlID(child)) != static_cast<WORD>(-1) && (::GetWindowLongPtr(child, GWL_STYLE) & WS_VISIBLE))
		{
			RECT r{};
			::GetWindowRect(child, &r);
			if(::PtInRect(&r, pt))
				return child;
		}
	}
	return nullptr;
}

void FreeText(TOOLINFO &ti)
{
	// OnToolHitTest overrides allocate their text with malloc, and MFC frees it.
	if(ti.lpszText != nullptr && ti.lpszText != LPSTR_TEXTCALLBACK && ti.hinst == nullptr && !IS_INTRESOURCE(ti.lpszText))
		std::free(ti.lpszText);
	ti.lpszText = nullptr;
}

bool IsMouseMessage(UINT msg)
{
	return msg == WM_MOUSEMOVE || msg == WM_NCMOUSEMOVE || msg == WM_LBUTTONUP || msg == WM_RBUTTONUP || msg == WM_MBUTTONUP;
}

} // namespace

// ---- CWnd's tooltips -----------------------------------------------------------------------

BOOL CWnd::EnableToolTips(BOOL enable)
{
	const UINT old = m_nFlags & WF_TOOLTIPS;
	if(enable)
	{
		m_nFlags |= WF_TOOLTIPS;
	} else
	{
		m_nFlags &= ~WF_TOOLTIPS;
		ToolTipThreadState &state = TipState();
		if(state.lastWnd == m_hWnd && state.tip != nullptr && ::IsWindow(state.tip->m_hWnd))
		{
			state.tip->SendMessage(TTM_DELTOOL, 0, reinterpret_cast<LPARAM>(&state.lastInfo));
			state.lastWnd = nullptr;
			state.lastHit = -1;
		}
	}
	return old != 0;
}

BOOL CWnd::EnableTrackingToolTips(BOOL enable)
{
	const UINT old = m_nFlags & WF_TRACKINGTOOLTIPS;
	if(enable)
		m_nFlags |= WF_TRACKINGTOOLTIPS;
	else
		m_nFlags &= ~WF_TRACKINGTOOLTIPS;
	return old != 0;
}

INT_PTR CWnd::OnToolHitTest(CPoint point, TOOLINFO *ti) const
{
	HWND child = ChildFromPoint(m_hWnd, point);
	if(child == nullptr)
		return -1;
	const INT_PTR hit = ::GetDlgCtrlID(child);
	if(ti != nullptr && ti->cbSize >= TTTOOLINFO_V1_SIZE)
	{
		ti->hwnd = m_hWnd;
		ti->uId = reinterpret_cast<UINT_PTR>(child);
		ti->uFlags |= TTF_IDISHWND;
		ti->lpszText = LPSTR_TEXTCALLBACK;
		// Tips for controls other than buttons sit centred below them.
		if(!(::SendMessage(child, WM_GETDLGCODE, 0, 0) & DLGC_BUTTON))
			ti->uFlags |= TTF_NOTBUTTON | TTF_CENTERTIP;
	}
	return hit;
}

void CWnd::CancelToolTips(BOOL)
{
	ToolTipThreadState &state = TipState();
	if(state.tip != nullptr && ::IsWindow(state.tip->m_hWnd))
		state.tip->SendMessage(TTM_ACTIVATE, FALSE);
}

void CWnd::FilterToolTipMessage(MSG *msg)
{
	const UINT message = msg->message;
	// Only the innermost window with tooltips handles them.
	CWnd *owner = nullptr;
	for(HWND hwnd = msg->hwnd; hwnd != nullptr; hwnd = ::GetParent(hwnd))
	{
		CWnd *wnd = FromHandlePermanent(hwnd);
		if(wnd != nullptr && (wnd->m_nFlags & (WF_TOOLTIPS | WF_TRACKINGTOOLTIPS)))
		{
			owner = wnd;
			break;
		}
		if(::GetWindowLongPtr(hwnd, GWL_STYLE) & WS_POPUP)
			break;
	}
	if(owner != this)
		return;

	ToolTipThreadState &state = TipState();
	const bool buttonsUp = ::GetKeyState(VK_LBUTTON) >= 0 && ::GetKeyState(VK_RBUTTON) >= 0 && ::GetKeyState(VK_MBUTTON) >= 0;
	if(IsMouseMessage(message) && buttonsUp)
	{
		// The tooltip belongs to the top-level window.
		HWND top = ::GetAncestor(m_hWnd, GA_ROOT);
		if(state.tip != nullptr && (!::IsWindow(state.tip->m_hWnd) || ::GetWindow(state.tip->m_hWnd, GW_OWNER) != top))
		{
			if(::IsWindow(state.tip->m_hWnd))
				state.tip->DestroyWindow();
			delete state.tip;
			state.tip = nullptr;
			state.lastWnd = nullptr;
			state.lastHit = -1;
			state.lastInfo = TOOLINFO{};
		}
		if(state.tip == nullptr)
		{
			state.tip = new CToolTipCtrl;
			if(!state.tip->Create(CWnd::FromHandle(top), TTS_ALWAYSTIP))
			{
				delete state.tip;
				state.tip = nullptr;
				return;
			}
			state.tip->SendMessage(TTM_ACTIVATE, FALSE);
			// Let '\n' break lines.
			state.tip->SendMessage(TTM_SETMAXTIPWIDTH, 0, SHRT_MAX);
		}
		CToolTipCtrl *tip = state.tip;

		TOOLINFO hitInfo{};
		hitInfo.cbSize = TTTOOLINFO_V1_SIZE;
		POINT pt = msg->pt;
		if(message == WM_MOUSEMOVE || message == WM_LBUTTONUP || message == WM_RBUTTONUP || message == WM_MBUTTONUP)
		{
			pt = POINT{GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam)};
			::ClientToScreen(msg->hwnd, &pt);
		}
		::ScreenToClient(m_hWnd, &pt);
		const INT_PTR hit = OnToolHitTest(CPoint(pt), &hitInfo);
		const bool changed = state.lastHit != hit || state.lastWnd != hitInfo.hwnd || state.lastInfo.uId != hitInfo.uId || state.lastInfo.hwnd != hitInfo.hwnd;
		if(changed)
		{
			if(hit != -1)
			{
				hitInfo.uFlags &= ~(TTF_NOTBUTTON | TTF_ALWAYSTIP);
				if(m_nFlags & WF_TRACKINGTOOLTIPS)
					hitInfo.uFlags |= TTF_TRACK;
				tip->SendMessage(TTM_ADDTOOL, 0, reinterpret_cast<LPARAM>(&hitInfo));
				const HWND active = ::GetActiveWindow();
				if(active == nullptr || active == top || ::GetAncestor(active, GA_ROOT) == top || (hitInfo.uFlags & TTF_ALWAYSTIP))
				{
					tip->SendMessage(TTM_ACTIVATE, TRUE);
					if(m_nFlags & WF_TRACKINGTOOLTIPS)
						tip->SendMessage(TTM_TRACKACTIVATE, TRUE, reinterpret_cast<LPARAM>(&hitInfo));
				}
			} else
			{
				tip->SendMessage(TTM_ACTIVATE, FALSE);
			}
			// Relay before the old tool goes, then remove it.
			tip->SendMessage(TTM_RELAYEVENT, 0, reinterpret_cast<LPARAM>(msg));
			if(state.lastInfo.cbSize >= TTTOOLINFO_V1_SIZE)
				tip->SendMessage(TTM_DELTOOL, 0, reinterpret_cast<LPARAM>(&state.lastInfo));
			state.lastWnd = hitInfo.hwnd;
			state.lastHit = hit;
			FreeText(hitInfo);
			state.lastInfo = hitInfo;
			if(hit == -1)
				state.lastInfo.cbSize = 0;
		} else
		{
			if(hit != -1)
				tip->SendMessage(TTM_RELAYEVENT, 0, reinterpret_cast<LPARAM>(msg));
			FreeText(hitInfo);
		}
		return;
	}
	// Keys and clicks put the tip away.
	const bool keys = (message >= WM_KEYFIRST && message <= WM_KEYLAST);
	const bool clicks = message == WM_LBUTTONDOWN || message == WM_LBUTTONDBLCLK || message == WM_RBUTTONDOWN || message == WM_RBUTTONDBLCLK || message == WM_MBUTTONDOWN || message == WM_MBUTTONDBLCLK || message == WM_NCLBUTTONDOWN || message == WM_NCLBUTTONDBLCLK || message == WM_NCRBUTTONDOWN || message == WM_NCRBUTTONDBLCLK || message == WM_NCMBUTTONDOWN || message == WM_NCMBUTTONDBLCLK;
	if(!(m_nFlags & WF_TRACKINGTOOLTIPS) && (keys || clicks))
		CancelToolTips(keys);
}

// ---- CToolTipCtrl --------------------------------------------------------------------------

namespace
{
TOOLINFO FillInToolInfo(CWnd *wnd, UINT_PTR id)
{
	TOOLINFO ti{};
	ti.cbSize = TTTOOLINFO_V1_SIZE;
	const HWND hwnd = wnd != nullptr ? wnd->m_hWnd : nullptr;
	if(id == 0)
	{
		ti.hwnd = ::GetParent(hwnd);
		ti.uFlags = TTF_IDISHWND;
		ti.uId = reinterpret_cast<UINT_PTR>(hwnd);
	} else
	{
		ti.hwnd = hwnd;
		ti.uId = id;
	}
	return ti;
}
} // namespace

BOOL CToolTipCtrl::Create(CWnd *parent, DWORD style)
{
	return CreateEx(parent, style, 0);
}

BOOL CToolTipCtrl::CreateEx(CWnd *parent, DWORD style, DWORD exStyle)
{
	return CWnd::CreateEx(exStyle, TOOLTIPS_CLASS, nullptr, WS_POPUP | style, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, parent != nullptr ? parent->m_hWnd : nullptr, nullptr);
}

BOOL CToolTipCtrl::AddTool(CWnd *wnd, LPCTSTR text, LPCRECT rect, UINT_PTR id)
{
	TOOLINFO ti = FillInToolInfo(wnd, id);
	if(rect != nullptr)
		ti.rect = *rect;
	ti.lpszText = const_cast<LPTSTR>(text != nullptr ? text : LPSTR_TEXTCALLBACK);
	return static_cast<BOOL>(SendMessage(TTM_ADDTOOL, 0, reinterpret_cast<LPARAM>(&ti)));
}

BOOL CToolTipCtrl::AddTool(CWnd *wnd, UINT textID, LPCRECT rect, UINT_PTR id)
{
	TOOLINFO ti = FillInToolInfo(wnd, id);
	if(rect != nullptr)
		ti.rect = *rect;
	ti.hinst = AfxGetResourceHandle();
	ti.lpszText = MAKEINTRESOURCE(textID);
	return static_cast<BOOL>(SendMessage(TTM_ADDTOOL, 0, reinterpret_cast<LPARAM>(&ti)));
}

void CToolTipCtrl::DelTool(CWnd *wnd, UINT_PTR id)
{
	TOOLINFO ti = FillInToolInfo(wnd, id);
	SendMessage(TTM_DELTOOL, 0, reinterpret_cast<LPARAM>(&ti));
}

void CToolTipCtrl::Activate(BOOL activate) { SendMessage(TTM_ACTIVATE, activate); }
void CToolTipCtrl::RelayEvent(LPMSG msg) { SendMessage(TTM_RELAYEVENT, 0, reinterpret_cast<LPARAM>(msg)); }

void CToolTipCtrl::UpdateTipText(LPCTSTR text, CWnd *wnd, UINT_PTR id)
{
	TOOLINFO ti = FillInToolInfo(wnd, id);
	ti.lpszText = const_cast<LPTSTR>(text);
	SendMessage(TTM_UPDATETIPTEXT, 0, reinterpret_cast<LPARAM>(&ti));
}

void CToolTipCtrl::SetToolRect(CWnd *wnd, UINT_PTR id, LPCRECT rect)
{
	TOOLINFO ti = FillInToolInfo(wnd, id);
	ti.rect = *rect;
	SendMessage(TTM_NEWTOOLRECT, 0, reinterpret_cast<LPARAM>(&ti));
}

int CToolTipCtrl::SetMaxTipWidth(int width) { return static_cast<int>(SendMessage(TTM_SETMAXTIPWIDTH, 0, width)); }
void CToolTipCtrl::SetDelayTime(UINT delay) { SendMessage(TTM_SETDELAYTIME, TTDT_AUTOMATIC, MAKELPARAM(delay, 0)); }
void CToolTipCtrl::SetDelayTime(DWORD duration, int time) { SendMessage(TTM_SETDELAYTIME, duration, MAKELPARAM(time, 0)); }
void CToolTipCtrl::Pop() { SendMessage(TTM_POP); }
void CToolTipCtrl::Update() { SendMessage(TTM_UPDATE); }
BOOL CToolTipCtrl::SetTitle(UINT icon, LPCTSTR title) { return static_cast<BOOL>(SendMessage(TTM_SETTITLE, icon, reinterpret_cast<LPARAM>(title))); }
int CToolTipCtrl::GetToolCount() const { return static_cast<int>(::SendMessage(m_hWnd, TTM_GETTOOLCOUNT, 0, 0)); }
