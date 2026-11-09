// linOpenMPT MFC layer: dynamic layouts (CMFCDynamicLayout, AFX_DIALOG_LAYOUT
// resources), as MFC implements them in afxlayout.cpp: each control keeps
// its rectangle from when the layout was made and moves or grows by its
// percentage of how much the window's client area has grown since.
#include "MfcInternal.h"

namespace
{

RECT ChildRect(HWND host, HWND child)
{
	RECT rc{};
	::GetWindowRect(child, &rc);
	::MapWindowPoints(nullptr, host, reinterpret_cast<POINT *>(&rc), 2);
	return rc;
}

} // namespace

BOOL CMFCDynamicLayout::Create(CWnd *host)
{
	if(host == nullptr || host->m_hWnd == nullptr)
		return FALSE;
	m_pHostWnd = host;
	RECT rc{};
	::GetClientRect(host->m_hWnd, &rc);
	m_initialClient = SIZE{rc.right, rc.bottom};
	m_items.clear();
	return TRUE;
}

BOOL CMFCDynamicLayout::AddItem(HWND hwnd, MoveSettings move, SizeSettings size)
{
	if(m_pHostWnd == nullptr || hwnd == nullptr || ::GetParent(hwnd) != m_pHostWnd->m_hWnd || HasItem(hwnd))
		return FALSE;
	m_items.push_back(Item{hwnd, ChildRect(m_pHostWnd->m_hWnd, hwnd), move, size});
	return TRUE;
}

BOOL CMFCDynamicLayout::AddItem(int id, MoveSettings move, SizeSettings size)
{
	return m_pHostWnd != nullptr && AddItem(::GetDlgItem(m_pHostWnd->m_hWnd, id), move, size);
}

BOOL CMFCDynamicLayout::HasItem(HWND hwnd) const
{
	for(const Item &item : m_items)
	{
		if(item.hwnd == hwnd)
			return TRUE;
	}
	return FALSE;
}

void CMFCDynamicLayout::Adjust()
{
	if(m_pHostWnd == nullptr || m_items.empty())
		return;
	const HWND host = m_pHostWnd->m_hWnd;
	RECT rc{};
	::GetClientRect(host, &rc);
	const int dx = rc.right - m_initialClient.cx, dy = rc.bottom - m_initialClient.cy;
	HDWP dwp = ::BeginDeferWindowPos(static_cast<int>(m_items.size()));
	for(const Item &item : m_items)
	{
		if(!::IsWindow(item.hwnd))
			continue;
		RECT r = item.initial;
		const int moveX = item.move.IsHorizontal() ? dx * item.move.m_nXRatio / 100 : 0;
		const int moveY = item.move.IsVertical() ? dy * item.move.m_nYRatio / 100 : 0;
		const int growX = item.size.IsHorizontal() ? dx * item.size.m_nXRatio / 100 : 0;
		const int growY = item.size.IsVertical() ? dy * item.size.m_nYRatio / 100 : 0;
		r.left += moveX;
		r.top += moveY;
		r.right += moveX + growX;
		r.bottom += moveY + growY;
		dwp = ::DeferWindowPos(dwp, item.hwnd, nullptr, r.left, r.top, std::max(0L, static_cast<long>(r.right - r.left)), std::max(0L, static_cast<long>(r.bottom - r.top)), SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS);
	}
	::EndDeferWindowPos(dwp);
}

BOOL CMFCDynamicLayout::LoadResource(CWnd *host, LPVOID resource, DWORD size)
{
	if(host == nullptr || host->m_hWnd == nullptr || resource == nullptr)
		return FALSE;
	struct Reader : CMFCDynamicLayoutData
	{
		std::vector<Item> Items() const
		{
			std::vector<Item> items;
			for(POSITION pos = m_listCtrls.GetHeadPosition(); pos != nullptr;)
				items.push_back(m_listCtrls.GetNext(pos));
			return items;
		}
	} data;
	if(!data.ReadResource(resource, size))
		return FALSE;
	const auto items = data.Items();
	bool any = false;
	for(const auto &item : items)
		any = any || !item.m_moveSettings.IsNone() || !item.m_sizeSettings.IsNone();
	if(!any)
		return TRUE;
	host->EnableDynamicLayout(TRUE);
	CMFCDynamicLayout *layout = host->GetDynamicLayout();
	// One entry per child window, in the dialog's order.
	HWND child = ::GetWindow(host->m_hWnd, GW_CHILD);
	for(const auto &item : items)
	{
		if(child == nullptr)
			break;
		if(!item.m_moveSettings.IsNone() || !item.m_sizeSettings.IsNone())
			layout->AddItem(child, item.m_moveSettings, item.m_sizeSettings);
		child = ::GetWindow(child, GW_HWNDNEXT);
	}
	return TRUE;
}

// ---- CWnd ------------------------------------------------------------------------------

void CWnd::EnableDynamicLayout(BOOL enable)
{
	delete m_pDynamicLayout;
	m_pDynamicLayout = nullptr;
	if(enable)
	{
		m_pDynamicLayout = new CMFCDynamicLayout;
		if(!m_pDynamicLayout->Create(this))
		{
			delete m_pDynamicLayout;
			m_pDynamicLayout = nullptr;
		}
	}
}

BOOL CWnd::LoadDynamicLayoutResource(LPCTSTR resourceName)
{
	if(m_hWnd == nullptr || resourceName == nullptr)
		return FALSE;
	const HINSTANCE instance = AfxGetResourceHandle();
	HRSRC res = ::FindResource(instance, resourceName, RT_DIALOG_LAYOUT);
	if(res == nullptr)
		return FALSE;
	LPVOID data = ::LockResource(::LoadResource(instance, res));
	return CMFCDynamicLayout::LoadResource(this, data, ::SizeofResource(instance, res));
}

void CWnd::ResizeDynamicLayout()
{
	if(m_pDynamicLayout != nullptr && !::IsIconic(m_hWnd))
		m_pDynamicLayout->Adjust();
}
