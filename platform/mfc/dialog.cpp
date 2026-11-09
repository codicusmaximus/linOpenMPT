// linOpenMPT MFC layer: dialogs (CDialog, modal loops, dialog data
// exchange), as MFC implements them in dlgcore.cpp, wincore.cpp and
// dlgdata.cpp, on the Win32 dialog manager.
#include "MfcInternal.h"

#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <string>
#include <vector>

using namespace linopenmpt::mfc;

namespace
{

// The dialog procedure of every MFC dialog: WM_INITDIALOG calls
// OnInitDialog(); everything else goes through the message map first.
INT_PTR CALLBACK AfxDlgProc(HWND hwnd, UINT msg, WPARAM, LPARAM)
{
	if(msg == WM_INITDIALOG)
	{
		if(auto *dlg = dynamic_cast<CDialog *>(CWnd::FromHandlePermanent(hwnd)))
			return dlg->OnInitDialog();
		return 1;
	}
	return 0;
}

// The windows running RunModalLoop(), innermost last.
struct ModalState
{
	CWnd *wnd;
	DWORD flags;
	bool showOnIdle;
	HWND parent;
};
std::vector<ModalState> g_modal;

BOOL ModalIdle(LONG count)
{
	if(g_modal.empty())
		return FALSE;
	ModalState &m = g_modal.back();
	CWnd *wnd = m.wnd;
	if(wnd->m_hWnd == nullptr || !::IsWindow(wnd->m_hWnd))
		return FALSE;
	// ContinueModal() may say no without EndModalLoop() (property sheets).
	if(!wnd->ContinueModal())
	{
		wnd->EndModalLoop(wnd->m_nModalResult);
		return FALSE;
	}
	// The dialog shows when the queue first goes idle, as in MFC.
	if(m.showOnIdle)
	{
		m.showOnIdle = false;
		wnd->ShowWindow(SW_SHOWNORMAL);
		wnd->UpdateWindow();
	}
	if(!(m.flags & MLF_NOIDLEMSG) && m.parent != nullptr && count == 0)
		::SendMessage(m.parent, WM_ENTERIDLE, MSGF_DIALOGBOX, reinterpret_cast<LPARAM>(wnd->m_hWnd));
	if(m.flags & MLF_NOKICKIDLE)
		return FALSE;
	return wnd->SendMessage(WM_KICKIDLE, MSGF_DIALOGBOX, static_cast<LPARAM>(count)) != 0;
}

LPCDLGTEMPLATE LoadTemplate(LPCTSTR name)
{
	const HINSTANCE instance = AfxGetResourceHandle();
	HRSRC res = ::FindResource(instance, name, RT_DIALOG);
	if(res == nullptr)
		return nullptr;
	return static_cast<LPCDLGTEMPLATE>(::LockResource(::LoadResource(instance, res)));
}

} // namespace

// ---- modal loops ---------------------------------------------------------------------------

int CWnd::RunModalLoop(DWORD flags)
{
	const bool showOnIdle = (flags & MLF_SHOWONIDLE) && !(GetStyle() & WS_VISIBLE);
	m_nFlags |= WF_MODALLOOP;
	g_modal.push_back({this, flags, showOnIdle, ::GetParent(m_hWnd)});
	PushPreTranslate();
	if(ContinueModal())
		LinOpenMPTRunModalLoopFor(this, ModalIdle);
	PopPreTranslate();
	g_modal.pop_back();
	m_nFlags &= ~(WF_MODALLOOP | WF_CONTINUEMODAL);
	return m_nModalResult;
}

BOOL CWnd::ContinueModal()
{
	return (m_nFlags & WF_CONTINUEMODAL) != 0;
}

void CWnd::EndModalLoop(int result)
{
	m_nModalResult = result;
	if(m_nFlags & WF_CONTINUEMODAL)
	{
		m_nFlags &= ~WF_CONTINUEMODAL;
		LinOpenMPTEndModalLoopFor(this, result);
	}
}

BOOL CWnd::IsDialogMessage(LPMSG msg)
{
	return ::IsDialogMessage(m_hWnd, msg);
}

BOOL CWnd::PreTranslateInput(LPMSG msg)
{
	// Keyboard and mouse input only.
	if((msg->message < WM_KEYFIRST || msg->message > WM_KEYLAST) && (msg->message < WM_MOUSEFIRST || msg->message > WM_MOUSELAST))
		return FALSE;
	return IsDialogMessage(msg);
}

// ---- creating dialogs -------------------------------------------------------------------------

BOOL CWnd::CreateDlg(LPCTSTR templateName, CWnd *parent)
{
	LPCDLGTEMPLATE dlgTemplate = LoadTemplate(templateName);
	if(dlgTemplate == nullptr)
		return FALSE;
	return CreateDlgIndirect(dlgTemplate, parent, AfxGetResourceHandle());
}

BOOL CWnd::CreateDlgIndirect(LPCDLGTEMPLATE dlgTemplate, CWnd *parent, HINSTANCE instance)
{
	if(dlgTemplate == nullptr)
		return FALSE;
	if(instance == nullptr)
		instance = AfxGetResourceHandle();
	m_nModalResult = -1;
	m_nFlags |= WF_CONTINUEMODAL;
	HookWindowCreate(this);
	HWND hwnd = ::CreateDialogIndirectParam(instance, dlgTemplate, parent != nullptr ? parent->m_hWnd : nullptr, AfxDlgProc, 0);
	if(!UnhookWindowCreate())
		PostNcDestroy();
	// EndDialog() during OnInitDialog() ends it right there.
	if(hwnd != nullptr && !(m_nFlags & WF_CONTINUEMODAL))
	{
		::DestroyWindow(hwnd);
		hwnd = nullptr;
	}
	if(hwnd == nullptr && m_hWnd != nullptr)
		Detach();
	return hwnd != nullptr;
}

BOOL CWnd::ExecuteDlgInit(LPCTSTR templateName)
{
	if(templateName == nullptr)
		return TRUE;
	const HINSTANCE instance = AfxGetResourceHandle();
	HRSRC res = ::FindResource(instance, templateName, RT_DLGINIT);
	if(res == nullptr)
		return TRUE; // nothing to do
	void *data = ::LockResource(::LoadResource(instance, res));
	return data != nullptr ? ExecuteDlgInit(data) : FALSE;
}

BOOL CWnd::ExecuteDlgInit(LPVOID resource)
{
	if(resource == nullptr)
		return TRUE;
	// { WORD control; WORD message; DWORD length; BYTE data[length]; } ...,
	// up to a zero control ID. The strings are 8-bit (the code page of the
	// resource script, ASCII in mptrack.rc).
	constexpr WORD Win16LbAddString = 0x0401;
	constexpr WORD Win16CbAddString = 0x0403;
	constexpr WORD AfxCbAddString = 0x1234;
	BOOL ok = TRUE;
	const BYTE *p = static_cast<const BYTE *>(resource);
	auto word = [](const BYTE *at) { return static_cast<WORD>(at[0] | (at[1] << 8)); };
	while(word(p) != 0)
	{
		const WORD id = word(p);
		WORD msg = word(p + 2);
		const DWORD length = static_cast<DWORD>(word(p + 4) | (word(p + 6) << 16));
		const BYTE *data = p + 8;
		if(msg == Win16LbAddString)
			msg = LB_ADDSTRING;
		else if(msg == Win16CbAddString || msg == AfxCbAddString)
			msg = CB_ADDSTRING;
		if(msg == LB_ADDSTRING || msg == CB_ADDSTRING)
		{
			std::wstring text;
			for(DWORD i = 0; i < length && data[i] != 0; i++)
				text += static_cast<wchar_t>(data[i]);
			if(::SendDlgItemMessage(m_hWnd, id, msg, 0, reinterpret_cast<LPARAM>(text.c_str())) < 0)
				ok = FALSE;
		}
		p = data + length;
	}
	// The control notifications those caused are not the user's.
	return ok;
}

// ---- data exchange --------------------------------------------------------------------------------

BOOL CWnd::UpdateData(BOOL saveAndValidate)
{
	CDataExchange dx(this, saveAndValidate);
	// Controls' notifications while their values are set are not passed on.
	ThreadState &state = State();
	const HWND oldLockout = state.lockoutNotifyWindow;
	state.lockoutNotifyWindow = m_hWnd;
	BOOL ok = FALSE;
	try
	{
		DoDataExchange(&dx);
		ok = TRUE;
	} catch(CUserException *e)
	{
		// The user has already been told what is wrong.
		e->Delete();
	} catch(CException *e)
	{
		e->ReportError();
		e->Delete();
	}
	state.lockoutNotifyWindow = oldLockout;
	return ok;
}

HWND CDataExchange::PrepareCtrl(int id)
{
	HWND ctl = ::GetDlgItem(m_pDlgWnd->m_hWnd, id);
	m_idLastControl = static_cast<UINT>(id);
	m_bEditLastControl = FALSE;
	return ctl;
}

HWND CDataExchange::PrepareEditCtrl(int id)
{
	HWND ctl = PrepareCtrl(id);
	m_bEditLastControl = TRUE;
	return ctl;
}

void CDataExchange::Fail()
{
	if(!m_bSaveAndValidate)
	{
		// Setting values never fails: the program is wrong.
		std::fprintf(stderr, "linOpenMPT: DDX failure while setting dialog values\n");
	} else if(m_idLastControl != 0)
	{
		HWND ctl = ::GetDlgItem(m_pDlgWnd->m_hWnd, static_cast<int>(m_idLastControl));
		if(ctl != nullptr)
		{
			::SetFocus(ctl);
			if(m_bEditLastControl)
				::SendMessage(ctl, EM_SETSEL, 0, -1);
		}
	}
	AfxThrowUserException();
}

void DDX_Control(CDataExchange *dx, int id, CWnd &control)
{
	if(control.m_hWnd != nullptr)
		return;
	HWND ctl = dx->PrepareCtrl(id);
	if(ctl == nullptr || !control.SubclassWindow(ctl))
	{
		std::fprintf(stderr, "linOpenMPT: DDX_Control: no control %d to subclass\n", id);
		AfxThrowNotSupportedException();
	}
}

namespace
{

std::wstring ControlText(HWND ctl)
{
	const int length = ::GetWindowTextLength(ctl);
	std::wstring text(static_cast<std::size_t>(length) + 1, L'\0');
	::GetWindowText(ctl, text.data(), length + 1);
	text.resize(std::wcslen(text.c_str()));
	return text;
}

void BadInput(CDataExchange *dx, UINT messageId, const wchar_t *fallback)
{
	CString text;
	if(!text.LoadString(messageId))
		text = fallback;
	AfxMessageBox(text, MB_ICONEXCLAMATION);
	dx->Fail();
}

template <typename T, typename Parse>
void TextNumber(CDataExchange *dx, int id, T &value, Parse parse, const wchar_t *format)
{
	HWND ctl = dx->PrepareEditCtrl(id);
	if(dx->m_bSaveAndValidate)
	{
		const std::wstring text = ControlText(ctl);
		wchar_t *end = nullptr;
		const auto parsed = parse(text.c_str(), &end);
		while(end != nullptr && (*end == L' ' || *end == L'\t'))
			end++;
		if(text.empty() || end == text.c_str() || (end != nullptr && *end != 0))
			BadInput(dx, AFX_IDP_PARSE_INT, L"Please enter a number.");
		value = static_cast<T>(parsed);
	} else
	{
		wchar_t buf[64];
		std::swprintf(buf, 64, format, value);
		::SetWindowText(ctl, buf);
	}
}

} // namespace

void DDX_Text(CDataExchange *dx, int id, CString &value)
{
	HWND ctl = dx->PrepareEditCtrl(id);
	if(dx->m_bSaveAndValidate)
		value = ControlText(ctl).c_str();
	else
		::SetWindowText(ctl, value);
}

void DDX_Text(CDataExchange *dx, int id, int &value)
{
	TextNumber(dx, id, value, [](const wchar_t *s, wchar_t **end) { return std::wcstol(s, end, 10); }, L"%d");
}

void DDX_Text(CDataExchange *dx, int id, UINT &value)
{
	TextNumber(dx, id, value, [](const wchar_t *s, wchar_t **end) { return std::wcstoul(s, end, 10); }, L"%u");
}

void DDX_Text(CDataExchange *dx, int id, long &value)
{
	TextNumber(dx, id, value, [](const wchar_t *s, wchar_t **end) { return std::wcstol(s, end, 10); }, L"%ld");
}

void DDX_Text(CDataExchange *dx, int id, float &value)
{
	double d = value;
	TextNumber(dx, id, d, [](const wchar_t *s, wchar_t **end) { return std::wcstod(s, end); }, L"%.7g");
	value = static_cast<float>(d);
}

void DDX_Text(CDataExchange *dx, int id, double &value)
{
	TextNumber(dx, id, value, [](const wchar_t *s, wchar_t **end) { return std::wcstod(s, end); }, L"%.15g");
}

void DDX_Check(CDataExchange *dx, int id, int &value)
{
	HWND ctl = dx->PrepareCtrl(id);
	if(dx->m_bSaveAndValidate)
		value = static_cast<int>(::SendMessage(ctl, BM_GETCHECK, 0, 0));
	else
		::SendMessage(ctl, BM_SETCHECK, static_cast<WPARAM>(value), 0);
}

void DDX_Radio(CDataExchange *dx, int id, int &value)
{
	// The group of radio buttons that starts at id.
	HWND ctl = dx->PrepareCtrl(id);
	if(dx->m_bSaveAndValidate)
		value = -1;
	int index = 0;
	for(HWND button = ctl; button != nullptr; button = ::GetWindow(button, GW_HWNDNEXT), index++)
	{
		if(button != ctl && (::GetWindowLongPtr(button, GWL_STYLE) & WS_GROUP))
			break;
		if(!(::SendMessage(button, WM_GETDLGCODE, 0, 0) & DLGC_RADIOBUTTON))
		{
			index--;
			continue;
		}
		if(dx->m_bSaveAndValidate)
		{
			if(::SendMessage(button, BM_GETCHECK, 0, 0) == BST_CHECKED)
				value = index;
		} else
		{
			::SendMessage(button, BM_SETCHECK, index == value ? BST_CHECKED : BST_UNCHECKED, 0);
		}
	}
}

void DDX_CBIndex(CDataExchange *dx, int id, int &index)
{
	HWND ctl = dx->PrepareCtrl(id);
	if(dx->m_bSaveAndValidate)
		index = static_cast<int>(::SendMessage(ctl, CB_GETCURSEL, 0, 0));
	else
		::SendMessage(ctl, CB_SETCURSEL, static_cast<WPARAM>(index), 0);
}

void DDX_CBString(CDataExchange *dx, int id, CString &value)
{
	HWND ctl = dx->PrepareCtrl(id);
	if(dx->m_bSaveAndValidate)
	{
		value = ControlText(ctl).c_str();
	} else if(::SendMessage(ctl, CB_SELECTSTRING, static_cast<WPARAM>(-1), reinterpret_cast<LPARAM>(static_cast<LPCTSTR>(value))) == CB_ERR)
	{
		// Drop-down combo boxes take any text.
		::SetWindowText(ctl, value);
	}
}

void DDX_LBIndex(CDataExchange *dx, int id, int &index)
{
	HWND ctl = dx->PrepareCtrl(id);
	if(dx->m_bSaveAndValidate)
		index = static_cast<int>(::SendMessage(ctl, LB_GETCURSEL, 0, 0));
	else
		::SendMessage(ctl, LB_SETCURSEL, static_cast<WPARAM>(index), 0);
}

void DDX_Slider(CDataExchange *dx, int id, int &value)
{
	HWND ctl = dx->PrepareCtrl(id);
	if(dx->m_bSaveAndValidate)
		value = static_cast<int>(::SendMessage(ctl, TBM_GETPOS, 0, 0));
	else
		::SendMessage(ctl, TBM_SETPOS, TRUE, value);
}

void DDV_MaxChars(CDataExchange *dx, const CString &value, int maxChars)
{
	if(dx->m_bSaveAndValidate && value.GetLength() > maxChars)
	{
		wchar_t text[128];
		std::swprintf(text, 128, L"Enter no more than %d characters.", maxChars);
		AfxMessageBox(text, MB_ICONEXCLAMATION);
		dx->Fail();
	} else if(!dx->m_bSaveAndValidate && dx->m_idLastControl != 0 && dx->m_bEditLastControl)
	{
		::SendDlgItemMessage(dx->m_pDlgWnd->m_hWnd, static_cast<int>(dx->m_idLastControl), EM_LIMITTEXT, static_cast<WPARAM>(maxChars), 0);
	}
}

void DDV_MinMaxInt(CDataExchange *dx, int value, int minVal, int maxVal)
{
	if(dx->m_bSaveAndValidate && (value < minVal || value > maxVal))
	{
		wchar_t text[128];
		std::swprintf(text, 128, L"Enter an integer between %d and %d.", minVal, maxVal);
		AfxMessageBox(text, MB_ICONEXCLAMATION);
		dx->Fail();
	}
}

void DDV_MinMaxUInt(CDataExchange *dx, UINT value, UINT minVal, UINT maxVal)
{
	if(dx->m_bSaveAndValidate && (value < minVal || value > maxVal))
	{
		wchar_t text[128];
		std::swprintf(text, 128, L"Enter a positive integer between %u and %u.", minVal, maxVal);
		AfxMessageBox(text, MB_ICONEXCLAMATION);
		dx->Fail();
	}
}

// ---- CDialog --------------------------------------------------------------------------------

BEGIN_MESSAGE_MAP(CDialog, CWnd)
	ON_COMMAND(IDOK, &CDialog::OnOK)
	ON_COMMAND(IDCANCEL, &CDialog::OnCancel)
	ON_MESSAGE(WM_INITDIALOG, &CDialog::HandleInitDialog)
	ON_MESSAGE(WM_SETFONT, &CDialog::HandleSetFont)
END_MESSAGE_MAP()

BOOL CDialog::Create(LPCTSTR templateName, CWnd *parent)
{
	m_lpszTemplateName = templateName;
	if(IS_INTRESOURCE(templateName) && m_nIDHelp == 0)
		m_nIDHelp = static_cast<UINT>(reinterpret_cast<UINT_PTR>(templateName));
	LPCDLGTEMPLATE dlgTemplate = LoadTemplate(templateName);
	if(dlgTemplate == nullptr)
		return FALSE;
	return CreateIndirect(dlgTemplate, parent, nullptr);
}

BOOL CDialog::Create(UINT id, CWnd *parent)
{
	return Create(MAKEINTRESOURCE(id), parent);
}

BOOL CDialog::CreateIndirect(LPCDLGTEMPLATE dlgTemplate, CWnd *parent, void *dialogInit)
{
	if(parent == nullptr)
		parent = AfxGetMainWnd();
	m_lpDialogInit = dialogInit;
	return CreateDlgIndirect(dlgTemplate, parent, AfxGetResourceHandle());
}

BOOL CDialog::InitModalIndirect(LPCDLGTEMPLATE dlgTemplate, CWnd *parent, void *dialogInit)
{
	m_lpDialogTemplate = dlgTemplate;
	m_pParentWnd = parent;
	m_lpDialogInit = dialogInit;
	return TRUE;
}

HWND CDialog::PreModal()
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
	m_hWndTop = nullptr;
	return owner;
}

void CDialog::PostModal()
{
	m_hWndTop = nullptr;
}

INT_PTR CDialog::DoModal()
{
	LPCDLGTEMPLATE dlgTemplate = m_lpDialogTemplate;
	if(dlgTemplate == nullptr && m_lpszTemplateName != nullptr)
		dlgTemplate = LoadTemplate(m_lpszTemplateName);
	if(dlgTemplate == nullptr)
		return -1;
	const HWND owner = PreModal();
	bool enableOwner = false;
	if(owner != nullptr && ::IsWindowEnabled(owner))
	{
		::EnableWindow(owner, FALSE);
		enableOwner = true;
	}
	try
	{
		if(CreateDlgIndirect(dlgTemplate, CWnd::FromHandle(owner), AfxGetResourceHandle()))
		{
			if(m_nFlags & WF_CONTINUEMODAL)
			{
				DWORD flags = MLF_SHOWONIDLE;
				if(GetStyle() & DS_NOIDLEMSG)
					flags |= MLF_NOIDLEMSG;
				RunModalLoop(flags);
			}
			if(m_hWnd != nullptr)
				SetWindowPos(nullptr, 0, 0, 0, 0, SWP_HIDEWINDOW | SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOZORDER);
		}
	} catch(CException *e)
	{
		e->Delete();
		m_nModalResult = -1;
	}
	if(enableOwner)
		::EnableWindow(owner, TRUE);
	if(owner != nullptr && ::GetActiveWindow() == m_hWnd)
		::SetActiveWindow(owner);
	DestroyWindow();
	PostModal();
	return m_nModalResult;
}

void CDialog::EndDialog(int result)
{
	if(m_nFlags & (WF_MODALLOOP | WF_CONTINUEMODAL))
		EndModalLoop(result);
	if(m_hWnd != nullptr)
		::EndDialog(m_hWnd, result);
}

LRESULT CDialog::HandleInitDialog(WPARAM, LPARAM)
{
	PreInitDialog();
	// Default() calls the dialog procedure, and so OnInitDialog().
	return Default();
}

LRESULT CDialog::HandleSetFont(WPARAM wp, LPARAM)
{
	// The dialog's font, before WM_INITDIALOG.
	OnSetFont(CFont::FromHandle(reinterpret_cast<HFONT>(wp)));
	return Default();
}

BOOL CDialog::OnInitDialog()
{
	// The dynamic layout of the template (AFX_DIALOG_LAYOUT), if it has one.
	if(m_lpszTemplateName != nullptr)
		LoadDynamicLayoutResource(m_lpszTemplateName);
	// List and combo box strings from the resources, then the data.
	const BOOL init = m_lpDialogInit != nullptr ? ExecuteDlgInit(m_lpDialogInit) : ExecuteDlgInit(m_lpszTemplateName);
	if(!init)
	{
		EndDialog(-1);
		return FALSE;
	}
	if(!UpdateData(FALSE))
	{
		EndDialog(-1);
		return FALSE;
	}
	// A help button only works with a help ID.
	if(HWND help = ::GetDlgItem(m_hWnd, ID_HELP))
		::EnableWindow(help, m_nIDHelp != 0 && AfxGetApp() != nullptr);
	return TRUE;
}

void CDialog::OnOK()
{
	if(!UpdateData(TRUE))
		return;
	EndDialog(IDOK);
}

void CDialog::OnCancel()
{
	EndDialog(IDCANCEL);
}

BOOL CDialog::PreTranslateMessage(MSG *msg)
{
	if(CWnd::PreTranslateMessage(msg))
		return TRUE;
	// Escape in a multi-line edit cancels the dialog (the dialog manager
	// leaves it to the edit), unless Cancel is disabled.
	if(msg->message == WM_KEYDOWN && (msg->wParam == VK_ESCAPE || msg->wParam == VK_CANCEL) && (::GetWindowLongPtr(msg->hwnd, GWL_STYLE) & ES_MULTILINE))
	{
		wchar_t cls[16] = {};
		::GetClassName(msg->hwnd, cls, 16);
		if(_tcsicmp(cls, L"Edit") == 0)
		{
			HWND cancel = ::GetDlgItem(m_hWnd, IDCANCEL);
			if(cancel == nullptr || ::IsWindowEnabled(cancel))
			{
				SendMessage(WM_COMMAND, IDCANCEL, 0);
				return TRUE;
			}
		}
	}
	return PreTranslateInput(msg);
}

void CDialog::GotoDlgCtrl(CWnd *ctrl)
{
	if(ctrl != nullptr)
		SendMessage(WM_NEXTDLGCTL, reinterpret_cast<WPARAM>(ctrl->m_hWnd), TRUE);
}

void CDialog::NextDlgCtrl() const
{
	::SendMessage(m_hWnd, WM_NEXTDLGCTL, 0, 0);
}

void CDialog::PrevDlgCtrl() const
{
	::SendMessage(m_hWnd, WM_NEXTDLGCTL, 1, 0);
}

void CDialog::SetDefID(UINT id)
{
	::SendMessage(m_hWnd, DM_SETDEFID, id, 0);
}

DWORD CDialog::GetDefID() const
{
	return static_cast<DWORD>(::SendMessage(m_hWnd, DM_GETDEFID, 0, 0));
}

void CDialog::MapDialogRect(LPRECT rect) const
{
	::MapDialogRect(m_hWnd, rect);
}

// ---- message boxes ----------------------------------------------------------------------------

int AfxMessageBox(LPCTSTR text, UINT type, UINT)
{
	// Owned by the innermost modal window, else the main window, and titled
	// with the application's name, as CWinApp::DoMessageBox does.
	HWND owner = nullptr;
	if(!g_modal.empty())
		owner = g_modal.back().wnd->m_hWnd;
	else if(CWnd *main = AfxGetMainWnd())
		owner = main->m_hWnd;
	CWinApp *app = AfxGetApp();
	LPCTSTR caption = app != nullptr && app->m_pszAppName != nullptr ? app->m_pszAppName : L"";
	if((type & MB_ICONMASK) == 0)
	{
		switch(type & MB_TYPEMASK)
		{
		case MB_OK:
		case MB_OKCANCEL:
			type |= MB_ICONEXCLAMATION;
			break;
		case MB_YESNO:
		case MB_YESNOCANCEL:
			type |= MB_ICONQUESTION;
			break;
		case MB_ABORTRETRYIGNORE:
		case MB_RETRYCANCEL:
			type |= MB_ICONEXCLAMATION;
			break;
		default:
			break;
		}
	}
	return ::MessageBox(owner, text, caption, type);
}

int CWnd::MessageBox(LPCTSTR text, LPCTSTR caption, UINT type)
{
	if(caption == nullptr)
	{
		CWinApp *app = AfxGetApp();
		caption = app != nullptr && app->m_pszAppName != nullptr ? app->m_pszAppName : L"";
	}
	return ::MessageBox(m_hWnd, text, caption, type);
}
