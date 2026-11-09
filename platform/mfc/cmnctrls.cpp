// linOpenMPT MFC layer: the classes of the common controls (afxcmn.h), which
// wrap the control messages as MFC's afxcmn.inl and winctrl2.cpp do.
#include "MfcInternal.h"


// ---- CSliderCtrl -----------------------------------------------------------------------------

BOOL CSliderCtrl::Create(DWORD style, const RECT &rect, CWnd *parent, UINT id)
{
	return CWnd::Create(TRACKBAR_CLASS, nullptr, style, rect, parent, id);
}

int CSliderCtrl::GetLineSize() const { return static_cast<int>(::SendMessage(m_hWnd, TBM_GETLINESIZE, 0, 0)); }
int CSliderCtrl::GetPageSize() const { return static_cast<int>(::SendMessage(m_hWnd, TBM_GETPAGESIZE, 0, 0)); }
void CSliderCtrl::GetRange(int &minPos, int &maxPos) const
{
	minPos = GetRangeMin();
	maxPos = GetRangeMax();
}
void CSliderCtrl::SetRangeMin(int minPos, BOOL redraw) { ::SendMessage(m_hWnd, TBM_SETRANGEMIN, redraw, minPos); }
void CSliderCtrl::SetRangeMax(int maxPos, BOOL redraw) { ::SendMessage(m_hWnd, TBM_SETRANGEMAX, redraw, maxPos); }
void CSliderCtrl::ClearTics(BOOL redraw) { ::SendMessage(m_hWnd, TBM_CLEARTICS, redraw, 0); }
BOOL CSliderCtrl::SetTic(int pos) { return static_cast<BOOL>(::SendMessage(m_hWnd, TBM_SETTIC, 0, pos)); }
void CSliderCtrl::SetSelection(int minPos, int maxPos)
{
	::SendMessage(m_hWnd, TBM_SETSELSTART, FALSE, minPos);
	::SendMessage(m_hWnd, TBM_SETSELEND, TRUE, maxPos);
}
void CSliderCtrl::ClearSel(BOOL redraw) { ::SendMessage(m_hWnd, TBM_CLEARSEL, redraw, 0); }
CToolTipCtrl *CSliderCtrl::GetToolTips() const
{
	return reinterpret_cast<CToolTipCtrl *>(CWnd::FromHandle(reinterpret_cast<HWND>(::SendMessage(m_hWnd, TBM_GETTOOLTIPS, 0, 0))));
}
int CSliderCtrl::GetPos() const { return static_cast<int>(::SendMessage(m_hWnd, TBM_GETPOS, 0, 0)); }
void CSliderCtrl::SetPos(int pos) { ::SendMessage(m_hWnd, TBM_SETPOS, TRUE, pos); }
void CSliderCtrl::SetRange(int minPos, int maxPos, BOOL redraw)
{
	// As MFC: two messages, so the range is not limited to 16 bits.
	::SendMessage(m_hWnd, TBM_SETRANGEMIN, FALSE, minPos);
	::SendMessage(m_hWnd, TBM_SETRANGEMAX, redraw, maxPos);
}
int CSliderCtrl::GetRangeMin() const { return static_cast<int>(::SendMessage(m_hWnd, TBM_GETRANGEMIN, 0, 0)); }
int CSliderCtrl::GetRangeMax() const { return static_cast<int>(::SendMessage(m_hWnd, TBM_GETRANGEMAX, 0, 0)); }
void CSliderCtrl::SetTicFreq(int freq) { ::SendMessage(m_hWnd, TBM_SETTICFREQ, freq, 0); }
void CSliderCtrl::SetPageSize(int size) { ::SendMessage(m_hWnd, TBM_SETPAGESIZE, 0, size); }
void CSliderCtrl::SetLineSize(int size) { ::SendMessage(m_hWnd, TBM_SETLINESIZE, 0, size); }

// ---- CSpinButtonCtrl -------------------------------------------------------------------------

BOOL CSpinButtonCtrl::Create(DWORD style, const RECT &rect, CWnd *parent, UINT id)
{
	return CWnd::Create(UPDOWN_CLASS, nullptr, style, rect, parent, id);
}

int CSpinButtonCtrl::GetPos32(BOOL *error) const { return static_cast<int>(::SendMessage(m_hWnd, UDM_GETPOS32, 0, reinterpret_cast<LPARAM>(error))); }
int CSpinButtonCtrl::SetPos32(int pos) { return static_cast<int>(::SendMessage(m_hWnd, UDM_SETPOS32, 0, pos)); }
int CSpinButtonCtrl::GetPos() const { return static_cast<int>(::SendMessage(m_hWnd, UDM_GETPOS, 0, 0)); }
int CSpinButtonCtrl::SetPos(int pos) { return static_cast<short>(::SendMessage(m_hWnd, UDM_SETPOS, 0, MAKELPARAM(pos, 0))); }
void CSpinButtonCtrl::SetRange(short lower, short upper) { ::SendMessage(m_hWnd, UDM_SETRANGE, 0, MAKELPARAM(upper, lower)); }
DWORD CSpinButtonCtrl::GetRange() const { return static_cast<DWORD>(::SendMessage(m_hWnd, UDM_GETRANGE, 0, 0)); }
void CSpinButtonCtrl::GetRange(int &lower, int &upper) const
{
	const DWORD r = GetRange();
	lower = static_cast<short>(HIWORD(r));
	upper = static_cast<short>(LOWORD(r));
}
void CSpinButtonCtrl::SetRange32(int lower, int upper) { ::SendMessage(m_hWnd, UDM_SETRANGE32, lower, upper); }
void CSpinButtonCtrl::GetRange32(int &lower, int &upper) const { ::SendMessage(m_hWnd, UDM_GETRANGE32, reinterpret_cast<WPARAM>(&lower), reinterpret_cast<LPARAM>(&upper)); }
CWnd *CSpinButtonCtrl::SetBuddy(CWnd *buddy)
{
	return CWnd::FromHandle(reinterpret_cast<HWND>(::SendMessage(m_hWnd, UDM_SETBUDDY, reinterpret_cast<WPARAM>(buddy != nullptr ? buddy->m_hWnd : nullptr), 0)));
}
CWnd *CSpinButtonCtrl::GetBuddy() const { return CWnd::FromHandle(reinterpret_cast<HWND>(::SendMessage(m_hWnd, UDM_GETBUDDY, 0, 0))); }
UINT CSpinButtonCtrl::SetBase(int base) { return static_cast<UINT>(::SendMessage(m_hWnd, UDM_SETBASE, base, 0)); }
BOOL CSpinButtonCtrl::SetAccel(int count, UDACCEL *accel) { return static_cast<BOOL>(::SendMessage(m_hWnd, UDM_SETACCEL, count, reinterpret_cast<LPARAM>(accel))); }

// ---- CProgressCtrl ---------------------------------------------------------------------------

BOOL CProgressCtrl::Create(DWORD style, const RECT &rect, CWnd *parent, UINT id)
{
	return CWnd::Create(PROGRESS_CLASS, nullptr, style, rect, parent, id);
}

void CProgressCtrl::SetRange32(int lower, int upper) { ::SendMessage(m_hWnd, PBM_SETRANGE32, lower, upper); }
void CProgressCtrl::SetRange(short lower, short upper) { ::SendMessage(m_hWnd, PBM_SETRANGE, 0, MAKELPARAM(lower, upper)); }
int CProgressCtrl::SetPos(int pos) { return static_cast<int>(::SendMessage(m_hWnd, PBM_SETPOS, pos, 0)); }
int CProgressCtrl::GetPos() const { return static_cast<int>(::SendMessage(m_hWnd, PBM_GETPOS, 0, 0)); }
int CProgressCtrl::OffsetPos(int pos) { return static_cast<int>(::SendMessage(m_hWnd, PBM_DELTAPOS, pos, 0)); }
int CProgressCtrl::SetStep(int step) { return static_cast<int>(::SendMessage(m_hWnd, PBM_SETSTEP, step, 0)); }
int CProgressCtrl::StepIt() { return static_cast<int>(::SendMessage(m_hWnd, PBM_STEPIT, 0, 0)); }
BOOL CProgressCtrl::SetMarquee(BOOL on, UINT interval) { return static_cast<BOOL>(::SendMessage(m_hWnd, PBM_SETMARQUEE, on, interval)); }
int CProgressCtrl::SetState(int state) { return static_cast<int>(::SendMessage(m_hWnd, PBM_SETSTATE, state, 0)); }
