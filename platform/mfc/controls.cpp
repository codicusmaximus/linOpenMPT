// linOpenMPT MFC layer: the classes of the standard controls (CStatic,
// CButton, CEdit, CListBox, CCheckListBox, CComboBox, CScrollBar), which
// wrap the control messages as MFC's afxwin2.inl does, and reflect owner
// drawing back to the control (OnChildNotify), as in winctrl*.cpp.
#include "MfcInternal.h"

#include <algorithm>

namespace
{

HWND ParentHwnd(CWnd *parent)
{
	return parent != nullptr ? parent->m_hWnd : nullptr;
}

// WM_DRAWITEM and friends, reflected to the control's virtual functions.
template <typename Control>
BOOL OwnerDrawNotify(Control *control, UINT msg, LPARAM lp, LRESULT *result)
{
	switch(msg)
	{
	case WM_DRAWITEM:
		control->DrawItem(reinterpret_cast<LPDRAWITEMSTRUCT>(lp));
		break;
	case WM_MEASUREITEM:
		control->MeasureItem(reinterpret_cast<LPMEASUREITEMSTRUCT>(lp));
		break;
	case WM_COMPAREITEM:
		if(result != nullptr)
			*result = control->CompareItem(reinterpret_cast<LPCOMPAREITEMSTRUCT>(lp));
		return TRUE;
	case WM_DELETEITEM:
		control->DeleteItem(reinterpret_cast<LPDELETEITEMSTRUCT>(lp));
		break;
	default:
		return FALSE;
	}
	if(result != nullptr)
		*result = TRUE;
	return TRUE;
}

} // namespace

// ---- CStatic -------------------------------------------------------------------------------

BOOL CStatic::Create(LPCTSTR text, DWORD style, const RECT &rect, CWnd *parent, UINT id)
{
	return CWnd::Create(L"Static", text, style, rect, parent, id);
}

HICON CStatic::SetIcon(HICON icon) { return reinterpret_cast<HICON>(SendMessage(STM_SETICON, reinterpret_cast<WPARAM>(icon), 0)); }
HICON CStatic::GetIcon() const { return reinterpret_cast<HICON>(::SendMessage(m_hWnd, STM_GETICON, 0, 0)); }
HBITMAP CStatic::SetBitmap(HBITMAP bitmap) { return reinterpret_cast<HBITMAP>(SendMessage(STM_SETIMAGE, IMAGE_BITMAP, reinterpret_cast<LPARAM>(bitmap))); }
HBITMAP CStatic::GetBitmap() const { return reinterpret_cast<HBITMAP>(::SendMessage(m_hWnd, STM_GETIMAGE, IMAGE_BITMAP, 0)); }

BOOL CStatic::OnChildNotify(UINT msg, WPARAM wp, LPARAM lp, LRESULT *result)
{
	if(msg != WM_DRAWITEM)
		return CWnd::OnChildNotify(msg, wp, lp, result);
	DrawItem(reinterpret_cast<LPDRAWITEMSTRUCT>(lp));
	if(result != nullptr)
		*result = TRUE;
	return TRUE;
}

// ---- CButton -------------------------------------------------------------------------------

BOOL CButton::Create(LPCTSTR caption, DWORD style, const RECT &rect, CWnd *parent, UINT id)
{
	return CWnd::Create(L"Button", caption, style, rect, parent, id);
}

int CButton::GetCheck() const { return static_cast<int>(::SendMessage(m_hWnd, BM_GETCHECK, 0, 0)); }
void CButton::SetCheck(int check) { SendMessage(BM_SETCHECK, static_cast<WPARAM>(check), 0); }
UINT CButton::GetState() const { return static_cast<UINT>(::SendMessage(m_hWnd, BM_GETSTATE, 0, 0)); }
void CButton::SetState(BOOL highlight) { SendMessage(BM_SETSTATE, static_cast<WPARAM>(highlight), 0); }
UINT CButton::GetButtonStyle() const { return static_cast<UINT>(::GetWindowLongPtr(m_hWnd, GWL_STYLE) & 0xFFFF); }
void CButton::SetButtonStyle(UINT style, BOOL redraw) { SendMessage(BM_SETSTYLE, style, redraw); }
HICON CButton::SetIcon(HICON icon) { return reinterpret_cast<HICON>(SendMessage(BM_SETIMAGE, IMAGE_ICON, reinterpret_cast<LPARAM>(icon))); }
HICON CButton::GetIcon() const { return reinterpret_cast<HICON>(::SendMessage(m_hWnd, BM_GETIMAGE, IMAGE_ICON, 0)); }
HBITMAP CButton::SetBitmap(HBITMAP bitmap) { return reinterpret_cast<HBITMAP>(SendMessage(BM_SETIMAGE, IMAGE_BITMAP, reinterpret_cast<LPARAM>(bitmap))); }
HBITMAP CButton::GetBitmap() const { return reinterpret_cast<HBITMAP>(::SendMessage(m_hWnd, BM_GETIMAGE, IMAGE_BITMAP, 0)); }

BOOL CButton::OnChildNotify(UINT msg, WPARAM wp, LPARAM lp, LRESULT *result)
{
	if(msg != WM_DRAWITEM)
		return CWnd::OnChildNotify(msg, wp, lp, result);
	DrawItem(reinterpret_cast<LPDRAWITEMSTRUCT>(lp));
	if(result != nullptr)
		*result = TRUE;
	return TRUE;
}

// ---- CEdit ---------------------------------------------------------------------------------

BOOL CEdit::Create(DWORD style, const RECT &rect, CWnd *parent, UINT id)
{
	return CWnd::Create(L"Edit", nullptr, style, rect, parent, id);
}

void CEdit::SetSel(int start, int end, BOOL noScroll)
{
	SendMessage(EM_SETSEL, static_cast<WPARAM>(start), end);
	if(!noScroll)
		SendMessage(EM_SCROLLCARET, 0, 0);
}

void CEdit::GetSel(int &start, int &end) const
{
	DWORD s = 0, e = 0;
	::SendMessage(m_hWnd, EM_GETSEL, reinterpret_cast<WPARAM>(&s), reinterpret_cast<LPARAM>(&e));
	start = static_cast<int>(s);
	end = static_cast<int>(e);
}

void CEdit::ReplaceSel(LPCTSTR text, BOOL canUndo) { SendMessage(EM_REPLACESEL, static_cast<WPARAM>(canUndo), reinterpret_cast<LPARAM>(text)); }
void CEdit::SetLimitText(UINT max) { SendMessage(EM_SETLIMITTEXT, max, 0); }
UINT CEdit::GetLimitText() const { return static_cast<UINT>(::SendMessage(m_hWnd, EM_GETLIMITTEXT, 0, 0)); }
void CEdit::LimitText(int max) { SendMessage(EM_LIMITTEXT, static_cast<WPARAM>(max), 0); }
BOOL CEdit::SetReadOnly(BOOL ro) { return static_cast<BOOL>(SendMessage(EM_SETREADONLY, static_cast<WPARAM>(ro), 0)); }
int CEdit::GetLineCount() const { return static_cast<int>(::SendMessage(m_hWnd, EM_GETLINECOUNT, 0, 0)); }
int CEdit::LineIndex(int line) const { return static_cast<int>(::SendMessage(m_hWnd, EM_LINEINDEX, static_cast<WPARAM>(line), 0)); }
int CEdit::LineFromChar(int index) const { return static_cast<int>(::SendMessage(m_hWnd, EM_LINEFROMCHAR, static_cast<WPARAM>(index), 0)); }
int CEdit::LineLength(int line) const { return static_cast<int>(::SendMessage(m_hWnd, EM_LINELENGTH, static_cast<WPARAM>(line), 0)); }

int CEdit::GetLine(int index, LPTSTR buffer, int maxLength) const
{
	// EM_GETLINE takes the buffer size in its first WORD.
	if(buffer == nullptr || maxLength <= 0)
		return 0;
	*reinterpret_cast<WORD *>(buffer) = static_cast<WORD>(std::min(maxLength, 0xFFFF));
	return static_cast<int>(::SendMessage(m_hWnd, EM_GETLINE, static_cast<WPARAM>(index), reinterpret_cast<LPARAM>(buffer)));
}

void CEdit::LineScroll(int lines, int chars) { SendMessage(EM_LINESCROLL, static_cast<WPARAM>(chars), lines); }
int CEdit::GetFirstVisibleLine() const { return static_cast<int>(::SendMessage(m_hWnd, EM_GETFIRSTVISIBLELINE, 0, 0)); }
BOOL CEdit::Undo() { return static_cast<BOOL>(SendMessage(EM_UNDO, 0, 0)); }
BOOL CEdit::CanUndo() const { return static_cast<BOOL>(::SendMessage(m_hWnd, EM_CANUNDO, 0, 0)); }
void CEdit::Clear() { SendMessage(WM_CLEAR, 0, 0); }
void CEdit::Copy() { SendMessage(WM_COPY, 0, 0); }
void CEdit::Cut() { SendMessage(WM_CUT, 0, 0); }
void CEdit::Paste() { SendMessage(WM_PASTE, 0, 0); }
void CEdit::SetMargins(UINT left, UINT right) { SendMessage(EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELONG(left, right)); }
void CEdit::SetPasswordChar(TCHAR ch) { SendMessage(EM_SETPASSWORDCHAR, ch, 0); }
BOOL CEdit::GetModify() const { return static_cast<BOOL>(::SendMessage(m_hWnd, EM_GETMODIFY, 0, 0)); }
void CEdit::SetModify(BOOL modified) { SendMessage(EM_SETMODIFY, static_cast<WPARAM>(modified), 0); }
BOOL CEdit::FmtLines(BOOL addEOL) { return static_cast<BOOL>(SendMessage(EM_FMTLINES, static_cast<WPARAM>(addEOL), 0)); }

int CEdit::CharFromPos(CPoint pt) const
{
	return static_cast<int>(::SendMessage(m_hWnd, EM_CHARFROMPOS, 0, MAKELPARAM(pt.x, pt.y)));
}

CPoint CEdit::PosFromChar(UINT ch) const
{
	const LRESULT r = ::SendMessage(m_hWnd, EM_POSFROMCHAR, ch, 0);
	return CPoint(static_cast<short>(LOWORD(r)), static_cast<short>(HIWORD(r)));
}

// ---- CListBox ------------------------------------------------------------------------------

BOOL CListBox::Create(DWORD style, const RECT &rect, CWnd *parent, UINT id)
{
	return CWnd::Create(L"ListBox", nullptr, style, rect, parent, id);
}

BOOL CListBox::OnChildNotify(UINT msg, WPARAM wp, LPARAM lp, LRESULT *result)
{
	switch(msg)
	{
	case WM_VKEYTOITEM:
		if(result != nullptr)
			*result = VKeyToItem(LOWORD(wp), HIWORD(wp));
		return TRUE;
	case WM_CHARTOITEM:
		if(result != nullptr)
			*result = CharToItem(LOWORD(wp), HIWORD(wp));
		return TRUE;
	default:
		if(OwnerDrawNotify(this, msg, lp, result))
			return TRUE;
		return CWnd::OnChildNotify(msg, wp, lp, result);
	}
}

int CListBox::GetCount() const { return static_cast<int>(::SendMessage(m_hWnd, LB_GETCOUNT, 0, 0)); }
int CListBox::GetCurSel() const { return static_cast<int>(::SendMessage(m_hWnd, LB_GETCURSEL, 0, 0)); }
int CListBox::SetCurSel(int sel) { return static_cast<int>(SendMessage(LB_SETCURSEL, static_cast<WPARAM>(sel), 0)); }
int CListBox::AddString(LPCTSTR text) { return static_cast<int>(SendMessage(LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text))); }
int CListBox::InsertString(int index, LPCTSTR text) { return static_cast<int>(SendMessage(LB_INSERTSTRING, static_cast<WPARAM>(index), reinterpret_cast<LPARAM>(text))); }
int CListBox::DeleteString(UINT index) { return static_cast<int>(SendMessage(LB_DELETESTRING, index, 0)); }
void CListBox::ResetContent() { SendMessage(LB_RESETCONTENT, 0, 0); }
DWORD_PTR CListBox::GetItemData(int index) const { return static_cast<DWORD_PTR>(::SendMessage(m_hWnd, LB_GETITEMDATA, static_cast<WPARAM>(index), 0)); }
int CListBox::SetItemData(int index, DWORD_PTR data) { return static_cast<int>(SendMessage(LB_SETITEMDATA, static_cast<WPARAM>(index), static_cast<LPARAM>(data))); }
int CListBox::GetText(int index, LPTSTR buf) const { return static_cast<int>(::SendMessage(m_hWnd, LB_GETTEXT, static_cast<WPARAM>(index), reinterpret_cast<LPARAM>(buf))); }

void CListBox::GetText(int index, CString &text) const
{
	const int length = GetTextLen(index);
	if(length < 0)
	{
		text.Empty();
		return;
	}
	std::wstring buf(static_cast<std::size_t>(length) + 1, L'\0');
	GetText(index, buf.data());
	text = buf.c_str();
}

int CListBox::GetSel(int index) const { return static_cast<int>(::SendMessage(m_hWnd, LB_GETSEL, static_cast<WPARAM>(index), 0)); }
int CListBox::SetSel(int index, BOOL select) { return static_cast<int>(SendMessage(LB_SETSEL, static_cast<WPARAM>(select), index)); }
int CListBox::GetSelCount() const { return static_cast<int>(::SendMessage(m_hWnd, LB_GETSELCOUNT, 0, 0)); }
int CListBox::GetTopIndex() const { return static_cast<int>(::SendMessage(m_hWnd, LB_GETTOPINDEX, 0, 0)); }
int CListBox::SetTopIndex(int index) { return static_cast<int>(SendMessage(LB_SETTOPINDEX, static_cast<WPARAM>(index), 0)); }
void *CListBox::GetItemDataPtr(int index) const { return reinterpret_cast<void *>(GetItemData(index)); }
int CListBox::SetItemDataPtr(int index, void *data) { return SetItemData(index, reinterpret_cast<DWORD_PTR>(data)); }
int CListBox::GetItemRect(int index, LPRECT rect) const { return static_cast<int>(::SendMessage(m_hWnd, LB_GETITEMRECT, static_cast<WPARAM>(index), reinterpret_cast<LPARAM>(rect))); }
int CListBox::GetItemHeight(int index) const { return static_cast<int>(::SendMessage(m_hWnd, LB_GETITEMHEIGHT, static_cast<WPARAM>(index), 0)); }
int CListBox::SetItemHeight(int index, UINT height) { return static_cast<int>(SendMessage(LB_SETITEMHEIGHT, static_cast<WPARAM>(index), MAKELONG(height, 0))); }
int CListBox::FindString(int start, LPCTSTR text) const { return static_cast<int>(::SendMessage(m_hWnd, LB_FINDSTRING, static_cast<WPARAM>(start), reinterpret_cast<LPARAM>(text))); }
int CListBox::FindStringExact(int start, LPCTSTR text) const { return static_cast<int>(::SendMessage(m_hWnd, LB_FINDSTRINGEXACT, static_cast<WPARAM>(start), reinterpret_cast<LPARAM>(text))); }
int CListBox::GetTextLen(int index) const { return static_cast<int>(::SendMessage(m_hWnd, LB_GETTEXTLEN, static_cast<WPARAM>(index), 0)); }
int CListBox::GetCaretIndex() const { return static_cast<int>(::SendMessage(m_hWnd, LB_GETCARETINDEX, 0, 0)); }
int CListBox::SetCaretIndex(int index, BOOL scroll) { return static_cast<int>(SendMessage(LB_SETCARETINDEX, static_cast<WPARAM>(index), MAKELONG(scroll, 0))); }
int CListBox::GetSelItems(int maxItems, LPINT items) const { return static_cast<int>(::SendMessage(m_hWnd, LB_GETSELITEMS, static_cast<WPARAM>(maxItems), reinterpret_cast<LPARAM>(items))); }

int CListBox::SelItemRange(BOOL select, int first, int last)
{
	if(select)
		return static_cast<int>(SendMessage(LB_SELITEMRANGEEX, static_cast<WPARAM>(first), last));
	return static_cast<int>(SendMessage(LB_SELITEMRANGEEX, static_cast<WPARAM>(last), first));
}

UINT CListBox::ItemFromPoint(CPoint pt, BOOL &outside) const
{
	const LRESULT r = ::SendMessage(m_hWnd, LB_ITEMFROMPOINT, 0, MAKELPARAM(pt.x, pt.y));
	outside = HIWORD(r) != 0;
	return LOWORD(r);
}

// ---- CCheckListBox -------------------------------------------------------------------------

int CCheckListBox::CheckBoxSize() const
{
	// A check box as tall as the text, as MFC sizes it from the font.
	HDC dc = ::GetDC(m_hWnd);
	HGDIOBJ old = ::SelectObject(dc, reinterpret_cast<HFONT>(::SendMessage(m_hWnd, WM_GETFONT, 0, 0)));
	TEXTMETRIC tm{};
	::GetTextMetrics(dc, &tm);
	::SelectObject(dc, old);
	::ReleaseDC(m_hWnd, dc);
	return std::max(13, static_cast<int>(tm.tmHeight) - 2);
}

int CCheckListBox::MinItemHeight() const
{
	HDC dc = ::GetDC(m_hWnd);
	HGDIOBJ old = ::SelectObject(dc, reinterpret_cast<HFONT>(::SendMessage(m_hWnd, WM_GETFONT, 0, 0)));
	TEXTMETRIC tm{};
	::GetTextMetrics(dc, &tm);
	::SelectObject(dc, old);
	::ReleaseDC(m_hWnd, dc);
	return std::max(static_cast<int>(tm.tmHeight), CheckBoxSize() + 2);
}

void CCheckListBox::PreSubclassWindow()
{
	CListBox::PreSubclassWindow();
	// The checks of items added before (none, as a rule).
	m_checks.assign(static_cast<std::size_t>(std::max(0, GetCount())), CheckState{});
	if(GetStyle() & (LBS_OWNERDRAWFIXED | LBS_OWNERDRAWVARIABLE))
		CListBox::SetItemHeight(0, static_cast<UINT>(MinItemHeight()));
}

void CCheckListBox::SetCheckStyle(UINT style)
{
	m_nStyle = style;
}

void CCheckListBox::SetCheck(int index, int check)
{
	if(index < 0 || index >= static_cast<int>(m_checks.size()))
		return;
	if(check == BST_INDETERMINATE && m_nStyle != BS_3STATE && m_nStyle != BS_AUTO3STATE)
		return;
	m_checks[static_cast<std::size_t>(index)].check = check;
	InvalidateCheck(index);
}

int CCheckListBox::GetCheck(int index)
{
	if(index < 0 || index >= static_cast<int>(m_checks.size()))
		return -1;
	return m_checks[static_cast<std::size_t>(index)].check;
}

void CCheckListBox::Enable(int index, BOOL enabled)
{
	if(index < 0 || index >= static_cast<int>(m_checks.size()))
		return;
	m_checks[static_cast<std::size_t>(index)].enabled = enabled != FALSE;
	InvalidateCheck(index);
}

BOOL CCheckListBox::IsEnabled(int index)
{
	if(index < 0 || index >= static_cast<int>(m_checks.size()))
		return FALSE;
	return m_checks[static_cast<std::size_t>(index)].enabled;
}

void CCheckListBox::InvalidateCheck(int index)
{
	RECT rc{};
	if(m_hWnd != nullptr && GetItemRect(index, &rc) != LB_ERR)
		::InvalidateRect(m_hWnd, &rc, FALSE);
}

void CCheckListBox::ToggleCheck(int index)
{
	if(index < 0 || index >= static_cast<int>(m_checks.size()) || !m_checks[static_cast<std::size_t>(index)].enabled)
		return;
	int &check = m_checks[static_cast<std::size_t>(index)].check;
	if(m_nStyle == BS_AUTO3STATE || m_nStyle == BS_3STATE)
		check = (check + 1) % 3;
	else
		check = check == BST_CHECKED ? BST_UNCHECKED : BST_CHECKED;
	InvalidateCheck(index);
	if(HWND parent = ::GetParent(m_hWnd))
		::SendMessage(parent, WM_COMMAND, MAKEWPARAM(::GetDlgCtrlID(m_hWnd), CLBN_CHKCHANGE), reinterpret_cast<LPARAM>(m_hWnd));
}

void CCheckListBox::MeasureItem(LPMEASUREITEMSTRUCT mis)
{
	mis->itemHeight = static_cast<UINT>(MinItemHeight());
}

void CCheckListBox::DrawItem(LPDRAWITEMSTRUCT dis)
{
	if(static_cast<int>(dis->itemID) < 0)
	{
		if(dis->itemState & ODS_FOCUS)
			::DrawFocusRect(dis->hDC, &dis->rcItem);
		return;
	}
	const int index = static_cast<int>(dis->itemID);
	const CheckState state = index < static_cast<int>(m_checks.size()) ? m_checks[static_cast<std::size_t>(index)] : CheckState{};
	const bool enabled = state.enabled && IsWindowEnabled();
	const int box = CheckBoxSize();
	RECT check = dis->rcItem;
	check.left += 1;
	check.right = check.left + box;
	check.top += (dis->rcItem.bottom - dis->rcItem.top - box) / 2;
	check.bottom = check.top + box;
	HDC dc = dis->hDC;
	// The box sits on the list's background; only the text is highlighted.
	::FillRect(dc, &dis->rcItem, ::GetSysColorBrush(COLOR_WINDOW));
	UINT checkState = DFCS_BUTTONCHECK | DFCS_FLAT;
	if(state.check == BST_CHECKED)
		checkState |= DFCS_CHECKED;
	else if(state.check == BST_INDETERMINATE)
		checkState |= DFCS_CHECKED | DFCS_BUTTON3STATE;
	if(!enabled)
		checkState |= DFCS_INACTIVE;
	::DrawFrameControl(dc, &check, DFC_BUTTON, checkState);

	RECT text = dis->rcItem;
	text.left = check.right + 3;
	const bool selected = (dis->itemState & ODS_SELECTED) != 0;
	::FillRect(dc, &text, ::GetSysColorBrush(selected ? COLOR_HIGHLIGHT : COLOR_WINDOW));
	::SetBkMode(dc, TRANSPARENT);
	::SetTextColor(dc, ::GetSysColor(!enabled ? COLOR_GRAYTEXT : (selected ? COLOR_HIGHLIGHTTEXT : COLOR_WINDOWTEXT)));
	CString itemText;
	GetText(index, itemText);
	RECT textRect = text;
	textRect.left += 2;
	::DrawText(dc, itemText, -1, &textRect, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS);
	if(dis->itemState & ODS_FOCUS)
		::DrawFocusRect(dc, &text);
}

LRESULT CCheckListBox::WindowProc(UINT msg, WPARAM wp, LPARAM lp)
{
	switch(msg)
	{
	case LB_ADDSTRING:
	case LB_INSERTSTRING:
		{
			const LRESULT index = CListBox::WindowProc(msg, wp, lp);
			if(index >= 0)
				m_checks.insert(m_checks.begin() + std::min(static_cast<std::size_t>(index), m_checks.size()), CheckState{});
			return index;
		}
	case LB_DELETESTRING:
		{
			const LRESULT count = CListBox::WindowProc(msg, wp, lp);
			if(count != LB_ERR && wp < m_checks.size())
				m_checks.erase(m_checks.begin() + static_cast<std::ptrdiff_t>(wp));
			return count;
		}
	case LB_RESETCONTENT:
		m_checks.clear();
		break;
	case LB_SETITEMHEIGHT:
		// Never smaller than the check box (upstream sets 0 to force this).
		return CListBox::WindowProc(msg, wp, MAKELPARAM(std::max(static_cast<int>(LOWORD(lp)), MinItemHeight()), 0));
	case WM_LBUTTONDOWN:
	case WM_LBUTTONDBLCLK:
		{
			const CPoint pt(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
			BOOL outside = FALSE;
			const int index = static_cast<int>(ItemFromPoint(pt, outside));
			if(!outside && pt.x < CheckBoxSize() + 2)
			{
				// A click on the box toggles it (and selects the item too).
				const LRESULT r = CListBox::WindowProc(msg, wp, lp);
				ToggleCheck(index);
				return r;
			}
			if(!outside && msg == WM_LBUTTONDBLCLK)
			{
				const LRESULT r = CListBox::WindowProc(msg, wp, lp);
				ToggleCheck(index);
				return r;
			}
		}
		break;
	case WM_KEYDOWN:
		if(wp == VK_SPACE)
		{
			const int caret = GetCaretIndex();
			if(caret >= 0)
			{
				// Every selected item of a multiple-selection list follows.
				if((GetStyle() & (LBS_EXTENDEDSEL | LBS_MULTIPLESEL)) && GetSel(caret) > 0)
				{
					const int count = GetCount();
					const int newCheck = GetCheck(caret) == BST_CHECKED ? BST_UNCHECKED : BST_CHECKED;
					for(int i = 0; i < count; i++)
					{
						if(GetSel(i) > 0 && IsEnabled(i))
							SetCheck(i, newCheck);
					}
					if(HWND parent = ::GetParent(m_hWnd))
						::SendMessage(parent, WM_COMMAND, MAKEWPARAM(::GetDlgCtrlID(m_hWnd), CLBN_CHKCHANGE), reinterpret_cast<LPARAM>(m_hWnd));
				} else
				{
					ToggleCheck(caret);
				}
			}
			return 0;
		}
		break;
	default:
		break;
	}
	return CListBox::WindowProc(msg, wp, lp);
}

// ---- CComboBox -----------------------------------------------------------------------------

BOOL CComboBox::Create(DWORD style, const RECT &rect, CWnd *parent, UINT id)
{
	return CWnd::Create(L"ComboBox", nullptr, style, rect, parent, id);
}

BOOL CComboBox::OnChildNotify(UINT msg, WPARAM wp, LPARAM lp, LRESULT *result)
{
	if(OwnerDrawNotify(this, msg, lp, result))
		return TRUE;
	return CWnd::OnChildNotify(msg, wp, lp, result);
}

BOOL CComboBox::GetComboBoxInfo(PCOMBOBOXINFO info) const { return static_cast<BOOL>(::SendMessage(m_hWnd, CB_GETCOMBOBOXINFO, 0, reinterpret_cast<LPARAM>(info))); }
int CComboBox::InitStorage(int items, UINT bytes) { return static_cast<int>(SendMessage(CB_INITSTORAGE, static_cast<WPARAM>(items), bytes)); }
void CComboBox::Clear() { SendMessage(WM_CLEAR, 0, 0); }
void CComboBox::Copy() { SendMessage(WM_COPY, 0, 0); }
void CComboBox::Cut() { SendMessage(WM_CUT, 0, 0); }
void CComboBox::Paste() { SendMessage(WM_PASTE, 0, 0); }
BOOL CComboBox::LimitText(int max) { return static_cast<BOOL>(SendMessage(CB_LIMITTEXT, static_cast<WPARAM>(max), 0)); }
DWORD CComboBox::GetEditSel() const { return static_cast<DWORD>(::SendMessage(m_hWnd, CB_GETEDITSEL, 0, 0)); }
BOOL CComboBox::SetEditSel(int start, int end) { return static_cast<BOOL>(SendMessage(CB_SETEDITSEL, 0, MAKELONG(start, end))); }
int CComboBox::GetLBTextLen(int index) const { return static_cast<int>(::SendMessage(m_hWnd, CB_GETLBTEXTLEN, static_cast<WPARAM>(index), 0)); }
int CComboBox::GetLBText(int index, LPTSTR text) const { return static_cast<int>(::SendMessage(m_hWnd, CB_GETLBTEXT, static_cast<WPARAM>(index), reinterpret_cast<LPARAM>(text))); }

void CComboBox::GetLBText(int index, CString &text) const
{
	const int length = GetLBTextLen(index);
	if(length < 0)
	{
		text.Empty();
		return;
	}
	std::wstring buf(static_cast<std::size_t>(length) + 1, L'\0');
	GetLBText(index, buf.data());
	text = buf.c_str();
}

int CComboBox::FindString(int start, LPCTSTR text) const { return static_cast<int>(::SendMessage(m_hWnd, CB_FINDSTRING, static_cast<WPARAM>(start), reinterpret_cast<LPARAM>(text))); }
int CComboBox::FindStringExact(int start, LPCTSTR text) const { return static_cast<int>(::SendMessage(m_hWnd, CB_FINDSTRINGEXACT, static_cast<WPARAM>(start), reinterpret_cast<LPARAM>(text))); }
int CComboBox::SelectString(int start, LPCTSTR text) { return static_cast<int>(SendMessage(CB_SELECTSTRING, static_cast<WPARAM>(start), reinterpret_cast<LPARAM>(text))); }
void *CComboBox::GetItemDataPtr(int index) const { return reinterpret_cast<void *>(GetItemData(index)); }
int CComboBox::SetItemDataPtr(int index, void *data) { return SetItemData(index, reinterpret_cast<DWORD_PTR>(data)); }
void CComboBox::ShowDropDown(BOOL show) { SendMessage(CB_SHOWDROPDOWN, static_cast<WPARAM>(show), 0); }
BOOL CComboBox::GetDroppedState() const { return static_cast<BOOL>(::SendMessage(m_hWnd, CB_GETDROPPEDSTATE, 0, 0)); }
int CComboBox::SetDroppedWidth(UINT width) { return static_cast<int>(SendMessage(CB_SETDROPPEDWIDTH, width, 0)); }
int CComboBox::GetDroppedWidth() const { return static_cast<int>(::SendMessage(m_hWnd, CB_GETDROPPEDWIDTH, 0, 0)); }
int CComboBox::GetTopIndex() const { return static_cast<int>(::SendMessage(m_hWnd, CB_GETTOPINDEX, 0, 0)); }
int CComboBox::SetTopIndex(int index) { return static_cast<int>(SendMessage(CB_SETTOPINDEX, static_cast<WPARAM>(index), 0)); }
int CComboBox::SetItemHeight(int index, UINT height) { return static_cast<int>(SendMessage(CB_SETITEMHEIGHT, static_cast<WPARAM>(index), MAKELONG(height, 0))); }
int CComboBox::GetItemHeight(int index) const { return static_cast<int>(::SendMessage(m_hWnd, CB_GETITEMHEIGHT, static_cast<WPARAM>(index), 0)); }
int CComboBox::GetCount() const { return static_cast<int>(::SendMessage(m_hWnd, CB_GETCOUNT, 0, 0)); }
int CComboBox::GetCurSel() const { return static_cast<int>(::SendMessage(m_hWnd, CB_GETCURSEL, 0, 0)); }
int CComboBox::SetCurSel(int sel) { return static_cast<int>(SendMessage(CB_SETCURSEL, static_cast<WPARAM>(sel), 0)); }
int CComboBox::AddString(LPCTSTR text) { return static_cast<int>(SendMessage(CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text))); }
int CComboBox::InsertString(int index, LPCTSTR text) { return static_cast<int>(SendMessage(CB_INSERTSTRING, static_cast<WPARAM>(index), reinterpret_cast<LPARAM>(text))); }
int CComboBox::DeleteString(UINT index) { return static_cast<int>(SendMessage(CB_DELETESTRING, index, 0)); }
void CComboBox::ResetContent() { SendMessage(CB_RESETCONTENT, 0, 0); }
DWORD_PTR CComboBox::GetItemData(int index) const { return static_cast<DWORD_PTR>(::SendMessage(m_hWnd, CB_GETITEMDATA, static_cast<WPARAM>(index), 0)); }
int CComboBox::SetItemData(int index, DWORD_PTR data) { return static_cast<int>(SendMessage(CB_SETITEMDATA, static_cast<WPARAM>(index), static_cast<LPARAM>(data))); }

// ---- CScrollBar ----------------------------------------------------------------------------

BOOL CScrollBar::Create(DWORD style, const RECT &rect, CWnd *parent, UINT id)
{
	return CWnd::Create(L"ScrollBar", nullptr, style, rect, parent, id);
}

void CScrollBar::GetScrollRange(LPINT minPos, LPINT maxPos) const { ::GetScrollRange(m_hWnd, SB_CTL, minPos, maxPos); }

BOOL CScrollBar::SetScrollInfo(LPSCROLLINFO info, BOOL redraw)
{
	info->cbSize = sizeof(SCROLLINFO);
	::SetScrollInfo(m_hWnd, SB_CTL, info, redraw);
	return TRUE;
}

BOOL CScrollBar::GetScrollInfo(LPSCROLLINFO info, UINT mask)
{
	info->cbSize = sizeof(SCROLLINFO);
	info->fMask = mask;
	return ::GetScrollInfo(m_hWnd, SB_CTL, info);
}

int CScrollBar::GetScrollLimit()
{
	SCROLLINFO info{sizeof(SCROLLINFO), SIF_RANGE | SIF_PAGE};
	if(!::GetScrollInfo(m_hWnd, SB_CTL, &info))
		return 0;
	return std::max(info.nMin, info.nMax - std::max<int>(static_cast<int>(info.nPage) - 1, 0));
}

void CScrollBar::ShowScrollBar(BOOL show) { ::ShowScrollBar(m_hWnd, SB_CTL, show); }
int CScrollBar::GetScrollPos() const { return ::GetScrollPos(m_hWnd, SB_CTL); }
int CScrollBar::SetScrollPos(int pos, BOOL redraw) { return ::SetScrollPos(m_hWnd, SB_CTL, pos, redraw); }
void CScrollBar::SetScrollRange(int minPos, int maxPos, BOOL redraw) { ::SetScrollRange(m_hWnd, SB_CTL, minPos, maxPos, redraw); }
