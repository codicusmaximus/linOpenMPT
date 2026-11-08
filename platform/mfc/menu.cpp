// linOpenMPT MFC layer: CMenu and the menu functions of CWnd, thin wrappers
// of the Win32 menu API as in MFC's winmenu.cpp and wincore.cpp.
#include "MfcInternal.h"

CMenu::~CMenu()
{
	DestroyMenu();
}

BOOL CMenu::CreateMenu() { return Attach(::CreateMenu()); }
BOOL CMenu::CreatePopupMenu() { return Attach(::CreatePopupMenu()); }
BOOL CMenu::LoadMenu(UINT id) { return Attach(::LoadMenu(AfxGetResourceHandle(), MAKEINTRESOURCE(id))); }
BOOL CMenu::LoadMenu(LPCTSTR name) { return Attach(::LoadMenu(AfxGetResourceHandle(), name)); }

BOOL CMenu::DestroyMenu()
{
	if(m_hMenu == nullptr)
		return FALSE;
	return ::DestroyMenu(Detach());
}

BOOL CMenu::AppendMenu(UINT flags, UINT_PTR id, LPCTSTR text) { return ::AppendMenu(m_hMenu, flags, id, text); }
BOOL CMenu::InsertMenu(UINT position, UINT flags, UINT_PTR id, LPCTSTR text) { return ::InsertMenu(m_hMenu, position, flags, id, text); }
BOOL CMenu::InsertMenuItem(UINT item, LPMENUITEMINFO info, BOOL byPos) { return ::InsertMenuItem(m_hMenu, item, byPos, info); }
BOOL CMenu::ModifyMenu(UINT position, UINT flags, UINT_PTR id, LPCTSTR text) { return ::ModifyMenu(m_hMenu, position, flags, id, text); }
BOOL CMenu::RemoveMenu(UINT position, UINT flags) { return ::RemoveMenu(m_hMenu, position, flags); }
BOOL CMenu::DeleteMenu(UINT position, UINT flags) { return ::DeleteMenu(m_hMenu, position, flags); }
UINT CMenu::CheckMenuItem(UINT id, UINT check) { return static_cast<UINT>(::CheckMenuItem(m_hMenu, id, check)); }
BOOL CMenu::CheckMenuRadioItem(UINT first, UINT last, UINT item, UINT flags) { return ::CheckMenuRadioItem(m_hMenu, first, last, item, flags); }
UINT CMenu::EnableMenuItem(UINT id, UINT enable) { return static_cast<UINT>(::EnableMenuItem(m_hMenu, id, enable)); }
UINT CMenu::GetMenuState(UINT id, UINT flags) const { return ::GetMenuState(m_hMenu, id, flags); }
int CMenu::GetMenuItemCount() const { return ::GetMenuItemCount(m_hMenu); }
UINT CMenu::GetMenuItemID(int pos) const { return ::GetMenuItemID(m_hMenu, pos); }
int CMenu::GetMenuString(UINT id, LPTSTR text, int max, UINT flags) const { return ::GetMenuString(m_hMenu, id, text, max, flags); }

int CMenu::GetMenuString(UINT id, CString &text, UINT flags) const
{
	const int length = ::GetMenuString(m_hMenu, id, nullptr, 0, flags);
	if(length <= 0)
	{
		text.Empty();
		return 0;
	}
	std::wstring buffer(static_cast<std::size_t>(length) + 1, L'\0');
	::GetMenuString(m_hMenu, id, buffer.data(), length + 1, flags);
	buffer.resize(static_cast<std::size_t>(length));
	text = buffer.c_str();
	return length;
}

BOOL CMenu::GetMenuItemInfo(UINT item, LPMENUITEMINFO info, BOOL byPos) { return ::GetMenuItemInfo(m_hMenu, item, byPos, info); }
BOOL CMenu::SetMenuItemInfo(UINT item, LPMENUITEMINFO info, BOOL byPos) { return ::SetMenuItemInfo(m_hMenu, item, byPos, info); }
BOOL CMenu::SetDefaultItem(UINT item, BOOL byPos) { return ::SetMenuDefaultItem(m_hMenu, item, byPos); }
CMenu *CMenu::GetSubMenu(int pos) const { return CMenu::FromHandle(::GetSubMenu(m_hMenu, pos)); }

BOOL CMenu::TrackPopupMenu(UINT flags, int x, int y, CWnd *wnd, LPCRECT rect)
{
	return ::TrackPopupMenu(m_hMenu, flags, x, y, 0, wnd != nullptr ? wnd->m_hWnd : nullptr, rect);
}

BOOL CMenu::TrackPopupMenuEx(UINT flags, int x, int y, CWnd *wnd, LPTPMPARAMS params)
{
	return ::TrackPopupMenuEx(m_hMenu, flags, x, y, wnd != nullptr ? wnd->m_hWnd : nullptr, params);
}

// ---- CWnd ------------------------------------------------------------------------------

CMenu *CWnd::GetMenu() const { return CMenu::FromHandle(::GetMenu(m_hWnd)); }
CMenu *CWnd::GetSystemMenu(BOOL revert) const { return CMenu::FromHandle(::GetSystemMenu(m_hWnd, revert)); }
BOOL CWnd::SetMenu(CMenu *menu) { return ::SetMenu(m_hWnd, menu != nullptr ? menu->m_hMenu : nullptr); }
void CWnd::DrawMenuBar() { ::DrawMenuBar(m_hWnd); }
