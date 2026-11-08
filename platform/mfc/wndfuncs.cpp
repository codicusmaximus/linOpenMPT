// linOpenMPT MFC layer: CWnd members that wrap one Win32 call each, as in
// MFC's afxwin inlines.
#include "MfcInternal.h"

#include <algorithm>
#include <cwchar>
#include <string>
#include <vector>

// ---- state ---------------------------------------------------------------------------

BOOL CWnd::IsWindowVisible() const { return ::IsWindowVisible(m_hWnd); }
BOOL CWnd::IsWindowEnabled() const { return ::IsWindowEnabled(m_hWnd); }
BOOL CWnd::IsWindowEnabled() { return ::IsWindowEnabled(m_hWnd); }
BOOL CWnd::EnableWindow(BOOL enable) { return ::EnableWindow(m_hWnd, enable); }
BOOL CWnd::ShowWindow(int cmd) { return ::ShowWindow(m_hWnd, cmd); }
BOOL CWnd::IsIconic() const { return ::IsIconic(m_hWnd); }
BOOL CWnd::IsZoomed() const { return ::IsZoomed(m_hWnd); }
DWORD CWnd::GetStyle() const { return static_cast<DWORD>(::GetWindowLongPtr(m_hWnd, GWL_STYLE)); }
DWORD CWnd::GetExStyle() const { return static_cast<DWORD>(::GetWindowLongPtr(m_hWnd, GWL_EXSTYLE)); }
int CWnd::GetDlgCtrlID() const { return ::GetDlgCtrlID(m_hWnd); }
void CWnd::NotifyWinEvent(DWORD event, LONG idObjectType, LONG idObject) { ::NotifyWinEvent(event, m_hWnd, idObjectType, idObject); }
void CWnd::SetWindowContextHelpId(DWORD) { }

namespace
{
BOOL ModifyStyleImpl(HWND hwnd, int index, DWORD remove, DWORD add, UINT flags)
{
	const auto style = static_cast<DWORD>(::GetWindowLongPtr(hwnd, index));
	const DWORD newStyle = (style & ~remove) | add;
	if(style == newStyle)
		return FALSE;
	::SetWindowLongPtr(hwnd, index, static_cast<LONG_PTR>(newStyle));
	if(flags != 0)
		::SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | flags);
	return TRUE;
}
} // namespace

BOOL CWnd::ModifyStyle(DWORD remove, DWORD add, UINT flags) { return ModifyStyleImpl(m_hWnd, GWL_STYLE, remove, add, flags); }
BOOL CWnd::ModifyStyleEx(DWORD remove, DWORD add, UINT flags) { return ModifyStyleImpl(m_hWnd, GWL_EXSTYLE, remove, add, flags); }

// ---- painting --------------------------------------------------------------------------

void CWnd::Invalidate(BOOL erase) { ::InvalidateRect(m_hWnd, nullptr, erase); }
void CWnd::InvalidateRect(LPCRECT rect, BOOL erase) { ::InvalidateRect(m_hWnd, rect, erase); }
void CWnd::ValidateRect(LPCRECT rect) { ::ValidateRect(m_hWnd, rect); }
void CWnd::UpdateWindow() { ::UpdateWindow(m_hWnd); }
void CWnd::SetRedraw(BOOL redraw) { ::SendMessage(m_hWnd, WM_SETREDRAW, redraw, 0); }
BOOL CWnd::RedrawWindow(LPCRECT update, CRgn *updateRgn, UINT flags)
{
	return ::RedrawWindow(m_hWnd, update, updateRgn ? static_cast<HRGN>(updateRgn->GetSafeHandle()) : nullptr, flags);
}
void CWnd::ScrollWindow(int dx, int dy, LPCRECT rect, LPCRECT clip)
{
	// ScrollWindow() also moves the children when no rectangle is given.
	::ScrollWindowEx(m_hWnd, dx, dy, rect, clip, nullptr, nullptr, SW_INVALIDATE | SW_ERASE | (rect == nullptr ? SW_SCROLLCHILDREN : 0));
}
int CWnd::ScrollWindowEx(int dx, int dy, LPCRECT scroll, LPCRECT clip, CRgn *update, LPRECT updateRect, UINT flags)
{
	return ::ScrollWindowEx(m_hWnd, dx, dy, scroll, clip, update ? static_cast<HRGN>(update->GetSafeHandle()) : nullptr, updateRect, flags);
}

CDC *CWnd::GetDC() { return CDC::FromHandle(::GetDC(m_hWnd)); }
CDC *CWnd::GetWindowDC() { return CDC::FromHandle(::GetWindowDC(m_hWnd)); }
int CWnd::ReleaseDC(CDC *dc) { return dc ? ::ReleaseDC(m_hWnd, dc->m_hDC) : 0; }

// ---- geometry --------------------------------------------------------------------------

void CWnd::GetClientRect(LPRECT rect) const { ::GetClientRect(m_hWnd, rect); }
void CWnd::GetWindowRect(LPRECT rect) const { ::GetWindowRect(m_hWnd, rect); }
void CWnd::MoveWindow(int x, int y, int w, int h, BOOL repaint) { ::MoveWindow(m_hWnd, x, y, w, h, repaint); }
void CWnd::MoveWindow(LPCRECT rect, BOOL repaint) { ::MoveWindow(m_hWnd, rect->left, rect->top, rect->right - rect->left, rect->bottom - rect->top, repaint); }
BOOL CWnd::SetWindowPos(const CWnd *after, int x, int y, int cx, int cy, UINT flags)
{
	return ::SetWindowPos(m_hWnd, after ? after->m_hWnd : nullptr, x, y, cx, cy, flags);
}
BOOL CWnd::GetWindowPlacement(WINDOWPLACEMENT *placement) const { return ::GetWindowPlacement(m_hWnd, placement); }
BOOL CWnd::SetWindowPlacement(const WINDOWPLACEMENT *placement) { return ::SetWindowPlacement(m_hWnd, placement); }
void CWnd::ScreenToClient(LPPOINT p) const { ::ScreenToClient(m_hWnd, p); }
void CWnd::ClientToScreen(LPPOINT p) const { ::ClientToScreen(m_hWnd, p); }
void CWnd::ScreenToClient(LPRECT r) const
{
	::ScreenToClient(m_hWnd, reinterpret_cast<LPPOINT>(&r->left));
	::ScreenToClient(m_hWnd, reinterpret_cast<LPPOINT>(&r->right));
}
void CWnd::ClientToScreen(LPRECT r) const
{
	::ClientToScreen(m_hWnd, reinterpret_cast<LPPOINT>(&r->left));
	::ClientToScreen(m_hWnd, reinterpret_cast<LPPOINT>(&r->right));
}
void CWnd::MapWindowPoints(CWnd *to, LPPOINT points, UINT count) const { ::MapWindowPoints(m_hWnd, to ? to->m_hWnd : nullptr, points, count); }
void CWnd::MapWindowPoints(CWnd *to, LPRECT rect) const { ::MapWindowPoints(m_hWnd, to ? to->m_hWnd : nullptr, reinterpret_cast<LPPOINT>(rect), 2); }
void CWnd::BringWindowToTop() { ::SetWindowPos(m_hWnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE); }

void CWnd::CenterWindow(CWnd *alternateOwner)
{
	// Over the owner (or parent), or the work area for top-level windows
	// without one, as MFC does.
	RECT rc{};
	GetWindowRect(&rc);
	RECT area{};
	HWND center = alternateOwner ? alternateOwner->m_hWnd : ::GetParent(m_hWnd);
	const bool child = (GetStyle() & WS_CHILD) != 0;
	if(center != nullptr && (child || ::IsWindowVisible(center)))
	{
		if(child)
			::GetClientRect(center, &area);
		else
			::GetWindowRect(center, &area);
	} else
	{
		::SystemParametersInfo(SPI_GETWORKAREA, 0, &area, 0);
	}
	const int w = rc.right - rc.left;
	const int h = rc.bottom - rc.top;
	const int x = (area.left + area.right - w) / 2;
	const int y = (area.top + area.bottom - h) / 2;
	::SetWindowPos(m_hWnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

CWnd *CWnd::ChildWindowFromPoint(POINT point) const { return FromHandle(::ChildWindowFromPoint(m_hWnd, point)); }
CWnd *CWnd::WindowFromPoint(POINT point) { return FromHandle(::WindowFromPoint(point)); }

// ---- text --------------------------------------------------------------------------------

void CWnd::SetWindowText(LPCTSTR text) { ::SetWindowText(m_hWnd, text); }
int CWnd::GetWindowText(LPTSTR buf, int max) const { return ::GetWindowText(m_hWnd, buf, max); }
void CWnd::GetWindowText(CString &text) const
{
	const int length = ::GetWindowTextLength(m_hWnd);
	std::vector<TCHAR> buf(static_cast<std::size_t>(length) + 1);
	::GetWindowText(m_hWnd, buf.data(), length + 1);
	text = buf.data();
}
int CWnd::GetWindowTextLength() const { return ::GetWindowTextLength(m_hWnd); }
void CWnd::SetFont(CFont *font, BOOL redraw) { ::SendMessage(m_hWnd, WM_SETFONT, reinterpret_cast<WPARAM>(font ? font->GetSafeHandle() : nullptr), redraw); }
CFont *CWnd::GetFont() const { return CFont::FromHandle(reinterpret_cast<HFONT>(::SendMessage(m_hWnd, WM_GETFONT, 0, 0))); }
HICON CWnd::SetIcon(HICON icon, BOOL bigIcon) { return reinterpret_cast<HICON>(::SendMessage(m_hWnd, WM_SETICON, bigIcon, reinterpret_cast<LPARAM>(icon))); }
HICON CWnd::GetIcon(BOOL bigIcon) const { return reinterpret_cast<HICON>(::SendMessage(m_hWnd, WM_GETICON, bigIcon, 0)); }

// ---- hierarchy -----------------------------------------------------------------------------

CWnd *CWnd::GetParent() const { return FromHandle(::GetParent(m_hWnd)); }
CWnd *CWnd::SetParent(CWnd *newParent) { return FromHandle(::SetParent(m_hWnd, newParent ? newParent->m_hWnd : nullptr)); }
CWnd *CWnd::GetDlgItem(int id) const { return FromHandle(::GetDlgItem(m_hWnd, id)); }
CWnd *CWnd::GetWindow(UINT cmd) const { return FromHandle(::GetWindow(m_hWnd, cmd)); }
CWnd *CWnd::GetNextWindow(UINT flag) const { return FromHandle(::GetWindow(m_hWnd, flag)); }
CWnd *CWnd::GetNextDlgTabItem(CWnd *ctrl, BOOL previous) const { return FromHandle(::GetNextDlgTabItem(m_hWnd, ctrl ? ctrl->m_hWnd : nullptr, previous)); }
BOOL CWnd::IsChild(const CWnd *wnd) const { return wnd && ::IsChild(m_hWnd, wnd->m_hWnd); }
CWnd *CWnd::GetDesktopWindow() { return FromHandle(::GetDesktopWindow()); }
CWnd *CWnd::GetOwner() const { return GetParent(); }
void CWnd::SetOwner(CWnd *) { }

CWnd *CWnd::GetTopLevelParent() const
{
	HWND hwnd = m_hWnd;
	while(hwnd != nullptr && (::GetWindowLongPtr(hwnd, GWL_STYLE) & WS_CHILD))
		hwnd = ::GetParent(hwnd);
	return FromHandle(hwnd);
}

CFrameWnd *CWnd::GetParentFrame() const
{
	for(CWnd *parent = GetParent(); parent != nullptr; parent = parent->GetParent())
	{
		if(parent->IsKindOf(RUNTIME_CLASS(CFrameWnd)))
			return static_cast<CFrameWnd *>(parent);
	}
	return nullptr;
}

CWnd *CWnd::GetDescendantWindow(int id, BOOL onlyPerm) const
{
	for(HWND child = ::GetWindow(m_hWnd, GW_CHILD); child != nullptr; child = ::GetWindow(child, GW_HWNDNEXT))
	{
		if(::GetDlgCtrlID(child) == id && (!onlyPerm || FromHandlePermanent(child)))
			return FromHandle(child);
		if(CWnd *found = FromHandle(child)->GetDescendantWindow(id, onlyPerm))
			return found;
	}
	return nullptr;
}

BOOL CWnd::ShowOwnedPopups(BOOL show)
{
	// Owned windows are Qt child windows of their owner.
	for(HWND child = ::GetWindow(m_hWnd, GW_CHILD); child != nullptr; child = ::GetWindow(child, GW_HWNDNEXT))
	{
		if(!(::GetWindowLongPtr(child, GWL_STYLE) & WS_CHILD))
			::ShowWindow(child, show ? SW_SHOWNA : SW_HIDE);
	}
	return TRUE;
}

// ---- focus, activation, capture ------------------------------------------------------------

CWnd *CWnd::SetFocus() { return FromHandle(::SetFocus(m_hWnd)); }
CWnd *CWnd::GetFocus() { return FromHandle(::GetFocus()); }
CWnd *CWnd::GetActiveWindow() { return FromHandle(::GetActiveWindow()); }
CWnd *CWnd::SetActiveWindow() { return FromHandle(::SetActiveWindow(m_hWnd)); }
CWnd *CWnd::GetForegroundWindow() { return FromHandle(::GetForegroundWindow()); }
BOOL CWnd::SetForegroundWindow() { return ::SetForegroundWindow(m_hWnd); }
CWnd *CWnd::SetCapture() { return FromHandle(::SetCapture(m_hWnd)); }
CWnd *CWnd::GetCapture() { return FromHandle(::GetCapture()); }

// ---- messages ----------------------------------------------------------------------------------

BOOL CWnd::PostMessage(UINT msg, WPARAM wp, LPARAM lp) { return ::PostMessage(m_hWnd, msg, wp, lp); }
LRESULT CWnd::SendMessage(UINT msg, WPARAM wp, LPARAM lp) { return ::SendMessage(m_hWnd, msg, wp, lp); }
BOOL CWnd::SendNotifyMessage(UINT msg, WPARAM wp, LPARAM lp) { return ::SendNotifyMessage(m_hWnd, msg, wp, lp); }

void CWnd::SendMessageToDescendants(UINT msg, WPARAM wp, LPARAM lp, BOOL deep, BOOL onlyPerm)
{
	for(HWND child = ::GetWindow(m_hWnd, GW_CHILD); child != nullptr;)
	{
		const HWND next = ::GetWindow(child, GW_HWNDNEXT);
		if(onlyPerm)
		{
			if(CWnd *wnd = FromHandlePermanent(child))
				AfxCallWndProc(wnd, child, msg, wp, lp);
		} else
		{
			::SendMessage(child, msg, wp, lp);
		}
		if(deep && ::IsWindow(child))
			FromHandle(child)->SendMessageToDescendants(msg, wp, lp, deep, onlyPerm);
		child = next;
	}
}

UINT_PTR CWnd::SetTimer(UINT_PTR id, UINT elapse, void *proc) { return ::SetTimer(m_hWnd, id, elapse, proc); }
BOOL CWnd::KillTimer(UINT_PTR id) { return ::KillTimer(m_hWnd, id); }

// ---- dialog items ---------------------------------------------------------------------------------

void CWnd::CheckDlgButton(int id, UINT check) { ::CheckDlgButton(m_hWnd, id, check); }
UINT CWnd::IsDlgButtonChecked(int id) const { return ::IsDlgButtonChecked(m_hWnd, id); }
void CWnd::CheckRadioButton(int firstID, int lastID, int checkID)
{
	for(int id = firstID; id <= lastID; id++)
		::CheckDlgButton(m_hWnd, id, id == checkID ? BST_CHECKED : BST_UNCHECKED);
}
int CWnd::GetCheckedRadioButton(int firstID, int lastID)
{
	for(int id = firstID; id <= lastID; id++)
	{
		if(::IsDlgButtonChecked(m_hWnd, id))
			return id;
	}
	return 0;
}
int CWnd::GetDlgItemText(int id, LPTSTR buf, int max) const { return ::GetDlgItemText(m_hWnd, id, buf, max); }
int CWnd::GetDlgItemText(int id, CString &text) const
{
	text.Empty();
	if(CWnd *item = GetDlgItem(id))
		item->GetWindowText(text);
	return text.GetLength();
}
void CWnd::SetDlgItemText(int id, LPCTSTR text) { ::SetDlgItemText(m_hWnd, id, text); }

UINT CWnd::GetDlgItemInt(int id, BOOL *ok, BOOL isSigned) const
{
	CString text;
	GetDlgItemText(id, text);
	const std::wstring s(text.GetString());
	wchar_t *end = nullptr;
	const wchar_t *begin = s.c_str();
	while(*begin == L' ')
		begin++;
	long long value = isSigned ? std::wcstoll(begin, &end, 10) : static_cast<long long>(std::wcstoull(begin, &end, 10));
	const bool valid = end != begin && end != nullptr;
	if(ok)
		*ok = valid;
	return valid ? static_cast<UINT>(value) : 0;
}

void CWnd::SetDlgItemInt(int id, UINT value, BOOL isSigned)
{
	const std::wstring text = isSigned ? std::to_wstring(static_cast<int>(value)) : std::to_wstring(value);
	::SetDlgItemText(m_hWnd, id, text.c_str());
}

BOOL CWnd::UpdateData(BOOL saveAndValidate)
{
	CDataExchange dx;
	dx.m_bSaveAndValidate = saveAndValidate;
	DoDataExchange(&dx);
	return TRUE;
}

// ---- scroll bars ------------------------------------------------------------------------------------

int CWnd::SetScrollPos(int bar, int pos, BOOL redraw) { return ::SetScrollPos(m_hWnd, bar, pos, redraw); }
int CWnd::GetScrollPos(int bar) const { return ::GetScrollPos(m_hWnd, bar); }
void CWnd::SetScrollRange(int bar, int minPos, int maxPos, BOOL redraw) { ::SetScrollRange(m_hWnd, bar, minPos, maxPos, redraw); }
void CWnd::GetScrollRange(int bar, LPINT minPos, LPINT maxPos) const { ::GetScrollRange(m_hWnd, bar, minPos, maxPos); }
BOOL CWnd::SetScrollInfo(int bar, SCROLLINFO *info, BOOL redraw)
{
	info->cbSize = sizeof(SCROLLINFO);
	::SetScrollInfo(m_hWnd, bar, info, redraw);
	return TRUE;
}
BOOL CWnd::GetScrollInfo(int bar, SCROLLINFO *info, UINT mask)
{
	info->cbSize = sizeof(SCROLLINFO);
	info->fMask = mask;
	return ::GetScrollInfo(m_hWnd, bar, info);
}
int CWnd::GetScrollLimit(int bar)
{
	SCROLLINFO info{sizeof(SCROLLINFO), SIF_RANGE | SIF_PAGE};
	if(!::GetScrollInfo(m_hWnd, bar, &info))
		return 0;
	return std::max(info.nMin, info.nMax - std::max<int>(static_cast<int>(info.nPage) - 1, 0));
}
void CWnd::ShowScrollBar(UINT bar, BOOL show) { ::ShowScrollBar(m_hWnd, static_cast<int>(bar), show); }
BOOL CWnd::EnableScrollBar(int bar, UINT arrows) { return ::EnableScrollBar(m_hWnd, static_cast<UINT>(bar), arrows); }
void CWnd::EnableScrollBarCtrl(int bar, BOOL enable)
{
	// No sibling scroll bar controls (GetScrollBarCtrl): the window's own.
	if(bar == SB_BOTH)
	{
		EnableScrollBarCtrl(SB_HORZ, enable);
		EnableScrollBarCtrl(SB_VERT, enable);
		return;
	}
	::ShowScrollBar(m_hWnd, bar, enable);
}
