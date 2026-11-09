// linOpenMPT MFC layer: CPropertySheet and CPropertyPage, as MFC implements
// them in dlgprop.cpp, on the property sheets of comctl32 (propsheet.cpp of
// the Qt layer). As in MFC, DoModal() makes a modeless sheet and runs its own
// modal loop, which ends when the sheet has no current page any more; pages
// are attached to their CPropertyPage while comctl32 creates them (the
// PSPCB_CREATE callback hooks the creation).
#include "MfcInternal.h"

#include <cstring>

using namespace linopenmpt::mfc;

namespace
{

// The dialog procedure of every page: WM_INITDIALOG calls OnInitDialog().
INT_PTR CALLBACK AfxPropPageDlgProc(HWND hwnd, UINT msg, WPARAM, LPARAM)
{
	if(msg == WM_INITDIALOG)
	{
		// The page is attached by now.
		UnhookWindowCreate();
		if(auto *page = dynamic_cast<CDialog *>(CWnd::FromHandlePermanent(hwnd)))
			return page->OnInitDialog();
		return 1;
	}
	return 0;
}

UINT CALLBACK AfxPropPageCallback(HWND, UINT msg, PROPSHEETPAGEW *psp)
{
	if(msg == PSPCB_CREATE)
	{
		auto *page = reinterpret_cast<CPropertyPage *>(psp->lParam);
		if(page == nullptr)
			return FALSE;
		HookWindowCreate(page);
		return TRUE;
	}
	return 0;
}

bool IsPushButton(HWND ctl)
{
	if(!(::SendMessage(ctl, WM_GETDLGCODE, 0, 0) & (DLGC_BUTTON | DLGC_DEFPUSHBUTTON)))
		return false;
	const auto type = ::GetWindowLongPtr(ctl, GWL_STYLE) & 0x0F;
	return type == BS_PUSHBUTTON || type == BS_DEFPUSHBUTTON || type == BS_USERBUTTON || type == BS_OWNERDRAW;
}

} // namespace

// ---- CPropertyPage -------------------------------------------------------------------------

CPropertyPage::CPropertyPage()
{
	Construct(static_cast<LPCTSTR>(nullptr), 0);
}

CPropertyPage::CPropertyPage(UINT id, UINT caption, DWORD)
{
	Construct(id, caption);
}

CPropertyPage::CPropertyPage(LPCTSTR templateName, UINT caption, DWORD)
{
	Construct(templateName, caption);
}

void CPropertyPage::Construct(UINT id, UINT caption)
{
	Construct(MAKEINTRESOURCE(id), caption);
}

void CPropertyPage::Construct(LPCTSTR templateName, UINT caption)
{
	m_psp = PROPSHEETPAGE{};
	m_psp.dwSize = sizeof(m_psp);
	m_psp.dwFlags = PSP_USECALLBACK;
	m_psp.hInstance = AfxGetResourceHandle();
	m_psp.pszTemplate = templateName;
	m_psp.pfnDlgProc = AfxPropPageDlgProc;
	m_psp.lParam = reinterpret_cast<LPARAM>(this);
	m_psp.pfnCallback = AfxPropPageCallback;
	m_lpszTemplateName = templateName;
	if(templateName != nullptr && IS_INTRESOURCE(templateName))
		m_nIDHelp = static_cast<UINT>(reinterpret_cast<UINT_PTR>(templateName));
	if(caption != 0 && m_strCaption.LoadString(caption))
	{
		m_psp.pszTitle = m_strCaption;
		m_psp.dwFlags |= PSP_USETITLE;
	}
	m_bFirstSetActive = TRUE;
}

CPropertySheet *CPropertyPage::GetParentSheet()
{
	return dynamic_cast<CPropertySheet *>(GetParent());
}

BOOL CPropertyPage::OnSetActive()
{
	// The first time, OnInitDialog() has just filled in the controls.
	if(m_bFirstSetActive)
		m_bFirstSetActive = FALSE;
	else
		UpdateData(FALSE);
	return TRUE;
}

BOOL CPropertyPage::OnKillActive()
{
	return UpdateData(TRUE);
}

BOOL CPropertyPage::OnApply()
{
	OnOK();
	return TRUE;
}

void CPropertyPage::OnReset()
{
	OnCancel();
}

BOOL CPropertyPage::OnQueryCancel()
{
	return TRUE;
}

void CPropertyPage::OnOK()
{
	// Pages do not end anything: the sheet does.
}

void CPropertyPage::OnCancel()
{
}

void CPropertyPage::CancelToClose()
{
	if(HWND parent = ::GetParent(m_hWnd))
		::SendMessage(parent, PSM_CANCELTOCLOSE, 0, 0);
}

void CPropertyPage::SetModified(BOOL changed)
{
	if(m_hWnd == nullptr)
		return;
	if(HWND parent = ::GetParent(m_hWnd))
		::SendMessage(parent, changed ? PSM_CHANGED : PSM_UNCHANGED, reinterpret_cast<WPARAM>(m_hWnd), 0);
}

LRESULT CPropertyPage::QuerySiblings(WPARAM wp, LPARAM lp)
{
	HWND parent = ::GetParent(m_hWnd);
	return parent != nullptr ? ::SendMessage(parent, PSM_QUERYSIBLINGS, wp, lp) : 0;
}

BOOL CPropertyPage::OnNotify(WPARAM wp, LPARAM lp, LRESULT *result)
{
	// The message map first.
	if(CDialog::OnNotify(wp, lp, result))
		return TRUE;
	const auto *hdr = reinterpret_cast<const NMHDR *>(lp);
	if(hdr == nullptr || (hdr->hwndFrom != m_hWnd && hdr->hwndFrom != ::GetParent(m_hWnd)))
		return FALSE;
	switch(hdr->code)
	{
	case PSN_SETACTIVE:
		*result = OnSetActive() ? 0 : -1;
		break;
	case PSN_KILLACTIVE:
		*result = !OnKillActive();
		break;
	case PSN_APPLY:
		*result = OnApply() ? PSNRET_NOERROR : PSNRET_INVALID_NOCHANGEPAGE;
		break;
	case PSN_RESET:
		OnReset();
		break;
	case PSN_QUERYCANCEL:
		*result = !OnQueryCancel();
		break;
	case PSN_HELP:
		SendMessage(WM_COMMAND, ID_HELP, 0);
		break;
	default:
		return FALSE;
	}
	return TRUE;
}

BOOL CPropertyPage::PreTranslateMessage(MSG *msg)
{
	// Escape is the sheet's, not CDialog's.
	if(msg->message == WM_KEYDOWN && msg->wParam == VK_ESCAPE)
		return FALSE;
	if(CWnd::PreTranslateMessage(msg))
		return TRUE;
	// The sheet switches pages on Ctrl+Tab, Ctrl+Shift+Tab, Ctrl+Page Up and Ctrl+Page Down.
	HWND parent = ::GetParent(m_hWnd);
	if(parent != nullptr && msg->message == WM_KEYDOWN && (::GetKeyState(VK_CONTROL) & 0x8000) && (msg->wParam == VK_TAB || msg->wParam == VK_PRIOR || msg->wParam == VK_NEXT))
	{
		if(::SendMessage(parent, PSM_ISDIALOGMESSAGE, 0, reinterpret_cast<LPARAM>(msg)))
			return TRUE;
	}
	// The rest goes to the sheet's dialog manager.
	return FALSE;
}

// ---- CPropertySheet ----------------------------------------------------------------------

BEGIN_MESSAGE_MAP(CPropertySheet, CWnd)
	ON_MESSAGE(WM_INITDIALOG, &CPropertySheet::HandleInitDialog)
	ON_WM_CLOSE()
	ON_WM_SYSCOMMAND()
END_MESSAGE_MAP()

CPropertySheet::CPropertySheet()
{
	Construct(static_cast<LPCTSTR>(nullptr));
}

CPropertySheet::CPropertySheet(UINT captionID, CWnd *parent, UINT selectPage)
{
	Construct(captionID, parent, selectPage);
}

CPropertySheet::CPropertySheet(LPCTSTR caption, CWnd *parent, UINT selectPage)
{
	Construct(caption, parent, selectPage);
}

CPropertySheet::~CPropertySheet() = default;

void CPropertySheet::Construct(UINT captionID, CWnd *parent, UINT selectPage)
{
	CString caption;
	caption.LoadString(captionID);
	Construct(caption, parent, selectPage);
}

void CPropertySheet::Construct(LPCTSTR caption, CWnd *parent, UINT selectPage)
{
	m_psh = PROPSHEETHEADER{};
	m_psh.dwSize = sizeof(m_psh);
	m_psh.dwFlags = PSH_PROPSHEETPAGE;
	m_psh.hInstance = AfxGetResourceHandle();
	m_psh.nStartPage = selectPage;
	m_strCaption = caption != nullptr ? caption : _T("");
	m_psh.pszCaption = m_strCaption;
	m_pParentWnd = parent;
}

void CPropertySheet::BuildPropPageArray()
{
	std::size_t total = 0;
	for(CPropertyPage *page : m_pages)
		total += page->m_psp.dwSize;
	m_pageArray.assign(total, 0);
	BYTE *p = m_pageArray.data();
	for(CPropertyPage *page : m_pages)
	{
		std::memcpy(p, &page->m_psp, page->m_psp.dwSize);
		p += page->m_psp.dwSize;
	}
	m_psh.dwFlags |= PSH_PROPSHEETPAGE;
	m_psh.ppsp = reinterpret_cast<LPCPROPSHEETPAGE>(m_pageArray.data());
	m_psh.nPages = static_cast<UINT>(m_pages.size());
	if(m_psh.nStartPage >= m_psh.nPages)
		m_psh.nStartPage = 0;
	m_psh.pszCaption = m_strCaption;
}

INT_PTR CPropertySheet::DoModal()
{
	// The owner: the given parent's top-level window, or the main window.
	HWND owner = m_pParentWnd != nullptr ? m_pParentWnd->m_hWnd : nullptr;
	if(owner == nullptr)
	{
		if(CWnd *main = AfxGetMainWnd())
			owner = main->m_hWnd;
	}
	while(owner != nullptr && (::GetWindowLongPtr(owner, GWL_STYLE) & WS_CHILD))
		owner = ::GetParent(owner);
	m_bModeless = FALSE;
	m_nModalResult = 0;
	m_psh.dwFlags |= PSH_MODELESS;
	m_psh.hwndParent = owner;
	BuildPropPageArray();
	bool enableOwner = false;
	if(owner != nullptr && ::IsWindowEnabled(owner))
	{
		::EnableWindow(owner, FALSE);
		enableOwner = true;
	}
	m_nFlags |= WF_CONTINUEMODAL;
	HookWindowCreate(this);
	const INT_PTR created = ::PropertySheet(&m_psh);
	UnhookWindowCreate();
	m_psh.dwFlags &= ~PSH_MODELESS;
	INT_PTR result = -1;
	if(created != 0 && created != -1 && m_hWnd != nullptr)
	{
		result = m_nModalResult;
		if(ContinueModal())
		{
			DWORD flags = MLF_SHOWONIDLE;
			if(GetStyle() & DS_NOIDLEMSG)
				flags |= MLF_NOIDLEMSG;
			result = RunModalLoop(flags);
		}
		if(m_hWnd != nullptr)
			SetWindowPos(nullptr, 0, 0, 0, 0, SWP_HIDEWINDOW | SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOZORDER);
	}
	m_nFlags &= ~WF_CONTINUEMODAL;
	if(enableOwner)
		::EnableWindow(owner, TRUE);
	if(owner != nullptr && m_hWnd != nullptr && ::GetActiveWindow() == m_hWnd)
		::SetActiveWindow(owner);
	DestroyWindow();
	return result;
}

BOOL CPropertySheet::Create(CWnd *parent, DWORD style, DWORD exStyle)
{
	(void)style;
	(void)exStyle;
	if(parent == nullptr)
		parent = m_pParentWnd;
	m_bModeless = TRUE;
	m_psh.dwFlags |= PSH_MODELESS;
	m_psh.hwndParent = parent != nullptr ? parent->m_hWnd : nullptr;
	BuildPropPageArray();
	HookWindowCreate(this);
	const INT_PTR created = ::PropertySheet(&m_psh);
	UnhookWindowCreate();
	return created != 0 && created != -1 && m_hWnd != nullptr;
}

BOOL CPropertySheet::ContinueModal()
{
	// The loop ends when the sheet has closed: no current page any more.
	if(!CWnd::ContinueModal())
		return FALSE;
	return m_hWnd != nullptr && ::SendMessage(m_hWnd, PSM_GETCURRENTPAGEHWND, 0, 0) != 0;
}

LRESULT CPropertySheet::HandleInitDialog(WPARAM, LPARAM)
{
	return OnInitDialog();
}

BOOL CPropertySheet::OnInitDialog()
{
	// comctl32 makes the tabs and the first page.
	return static_cast<BOOL>(Default());
}

BOOL CPropertySheet::OnCommand(WPARAM wp, LPARAM lp)
{
	// The message map first.
	if(CWnd::OnCommand(wp, lp))
		return TRUE;
	// A push button's ID is the result, as in MFC.
	const auto ctl = reinterpret_cast<HWND>(lp);
	if(ctl != nullptr && HIWORD(wp) == BN_CLICKED && IsPushButton(ctl))
		m_nModalResult = LOWORD(wp);
	// comctl32 handles OK, Cancel and Apply; that may have closed the sheet.
	Default();
	if((m_nFlags & WF_MODALLOOP) && !ContinueModal())
		EndModalLoop(m_nModalResult);
	return TRUE;
}

void CPropertySheet::OnClose()
{
	m_nModalResult = IDCANCEL;
	if(m_bModeless)
	{
		DestroyWindow();
		return;
	}
	Default();
	if((m_nFlags & WF_MODALLOOP) && !ContinueModal())
		EndModalLoop(m_nModalResult);
}

void CPropertySheet::OnSysCommand(UINT id, LPARAM)
{
	if((id & 0xFFF0) == SC_CLOSE)
		m_nModalResult = IDCANCEL;
	Default();
	if((m_nFlags & WF_MODALLOOP) && !ContinueModal())
		EndModalLoop(m_nModalResult);
}

BOOL CPropertySheet::PreTranslateMessage(MSG *msg)
{
	if(CWnd::PreTranslateMessage(msg))
		return TRUE;
	// Ctrl+Tab, Ctrl+Shift+Tab, Ctrl+Page Up and Ctrl+Page Down switch pages.
	if(msg->message == WM_KEYDOWN && (::GetKeyState(VK_CONTROL) & 0x8000) && (msg->wParam == VK_TAB || msg->wParam == VK_PRIOR || msg->wParam == VK_NEXT))
	{
		if(SendMessage(PSM_ISDIALOGMESSAGE, 0, reinterpret_cast<LPARAM>(msg)))
			return TRUE;
	}
	return PreTranslateInput(msg);
}

void CPropertySheet::AddPage(CPropertyPage *page)
{
	if(page == nullptr)
		return;
	m_pages.push_back(page);
	if(m_hWnd != nullptr)
	{
		HPROPSHEETPAGE hpage = ::CreatePropertySheetPage(&page->m_psp);
		if(hpage == nullptr || !SendMessage(PSM_ADDPAGE, 0, reinterpret_cast<LPARAM>(hpage)))
		{
			if(hpage != nullptr)
				::DestroyPropertySheetPage(hpage);
			m_pages.pop_back();
		}
	}
}

void CPropertySheet::RemovePage(CPropertyPage *page)
{
	const int index = GetPageIndex(page);
	if(index >= 0)
		RemovePage(index);
}

void CPropertySheet::RemovePage(int page)
{
	if(page < 0 || static_cast<std::size_t>(page) >= m_pages.size())
		return;
	if(m_hWnd != nullptr)
		SendMessage(PSM_REMOVEPAGE, static_cast<WPARAM>(page), 0);
	m_pages.erase(m_pages.begin() + page);
}

int CPropertySheet::GetPageCount() const
{
	return static_cast<int>(m_pages.size());
}

CPropertyPage *CPropertySheet::GetPage(int page) const
{
	return page >= 0 && static_cast<std::size_t>(page) < m_pages.size() ? m_pages[page] : nullptr;
}

int CPropertySheet::GetPageIndex(CPropertyPage *page)
{
	for(std::size_t i = 0; i < m_pages.size(); i++)
	{
		if(m_pages[i] == page)
			return static_cast<int>(i);
	}
	return -1;
}

int CPropertySheet::GetActiveIndex() const
{
	if(m_hWnd == nullptr)
		return static_cast<int>(m_psh.nStartPage);
	const auto tab = reinterpret_cast<HWND>(::SendMessage(m_hWnd, PSM_GETTABCONTROL, 0, 0));
	return tab != nullptr ? static_cast<int>(::SendMessage(tab, TCM_GETCURSEL, 0, 0)) : -1;
}

CPropertyPage *CPropertySheet::GetActivePage() const
{
	if(m_hWnd != nullptr)
	{
		const auto hwnd = reinterpret_cast<HWND>(::SendMessage(m_hWnd, PSM_GETCURRENTPAGEHWND, 0, 0));
		if(auto *page = dynamic_cast<CPropertyPage *>(CWnd::FromHandlePermanent(hwnd)))
			return page;
	}
	return GetPage(GetActiveIndex());
}

BOOL CPropertySheet::SetActivePage(int page)
{
	if(m_hWnd == nullptr)
	{
		m_psh.nStartPage = static_cast<UINT>(page);
		return TRUE;
	}
	return static_cast<BOOL>(SendMessage(PSM_SETCURSEL, static_cast<WPARAM>(page), 0));
}

BOOL CPropertySheet::SetActivePage(CPropertyPage *page)
{
	const int index = GetPageIndex(page);
	return index >= 0 ? SetActivePage(index) : FALSE;
}

void CPropertySheet::SetTitle(LPCTSTR text, UINT style)
{
	m_psh.dwFlags = (m_psh.dwFlags & ~PSH_PROPTITLE) | (style & PSH_PROPTITLE);
	m_strCaption = text != nullptr ? text : _T("");
	m_psh.pszCaption = m_strCaption;
	if(m_hWnd != nullptr)
		SendMessage(PSM_SETTITLE, style, reinterpret_cast<LPARAM>(static_cast<LPCTSTR>(m_strCaption)));
}

CTabCtrl *CPropertySheet::GetTabControl() const
{
	if(m_hWnd == nullptr)
		return nullptr;
	return static_cast<CTabCtrl *>(CWnd::FromHandle(reinterpret_cast<HWND>(::SendMessage(m_hWnd, PSM_GETTABCONTROL, 0, 0))));
}

BOOL CPropertySheet::PressButton(int button)
{
	return m_hWnd != nullptr && ::PostMessage(m_hWnd, PSM_PRESSBUTTON, static_cast<WPARAM>(button), 0);
}

void CPropertySheet::EndDialog(int result)
{
	m_nModalResult = result;
	if(m_bModeless)
		DestroyWindow();
	else
		PostMessage(PSM_PRESSBUTTON, PSBTN_CANCEL, 0);
}

void CPropertySheet::MapDialogRect(LPRECT rect) const
{
	::MapDialogRect(m_hWnd, rect);
}
