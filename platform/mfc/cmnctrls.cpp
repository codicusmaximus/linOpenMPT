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

// ---- CImageList ------------------------------------------------------------------------------

#include <map>
#include <memory>

CImageList::~CImageList()
{
	DeleteImageList();
}

CImageList *CImageList::FromHandle(HIMAGELIST list)
{
	if(list == nullptr)
		return nullptr;
	// Wrappers for lists owned elsewhere, as MFC's temporary map; they never
	// destroy the list.
	struct Temp : CImageList
	{
		~Temp() override { Detach(); }
	};
	static std::map<HIMAGELIST, std::unique_ptr<Temp>> temps;
	auto &slot = temps[list];
	if(!slot)
	{
		slot = std::make_unique<Temp>();
		slot->Attach(list);
	}
	return slot.get();
}

BOOL CImageList::Create(int cx, int cy, UINT flags, int initial, int grow)
{
	return Attach(ImageList_Create(cx, cy, flags, initial, grow));
}

BOOL CImageList::Create(UINT bitmapID, int cx, int grow, COLORREF mask)
{
	return Attach(ImageList_LoadBitmap(AfxGetResourceHandle(), MAKEINTRESOURCE(bitmapID), cx, grow, mask));
}

BOOL CImageList::DeleteImageList()
{
	if(m_hImageList == nullptr)
		return FALSE;
	return ImageList_Destroy(Detach());
}

int CImageList::Add(CBitmap *bitmap, CBitmap *mask)
{
	return ImageList_Add(m_hImageList, bitmap != nullptr ? static_cast<HBITMAP>(bitmap->m_hObject) : nullptr, mask != nullptr ? static_cast<HBITMAP>(mask->m_hObject) : nullptr);
}

int CImageList::Add(CBitmap *bitmap, COLORREF mask)
{
	return ImageList_AddMasked(m_hImageList, bitmap != nullptr ? static_cast<HBITMAP>(bitmap->m_hObject) : nullptr, mask);
}

int CImageList::Add(HICON icon) { return ImageList_AddIcon(m_hImageList, icon); }
BOOL CImageList::Remove(int image) { return ImageList_Remove(m_hImageList, image); }
BOOL CImageList::Replace(int image, CBitmap *bitmap, CBitmap *mask)
{
	return ImageList_Replace(m_hImageList, image, bitmap != nullptr ? static_cast<HBITMAP>(bitmap->m_hObject) : nullptr, mask != nullptr ? static_cast<HBITMAP>(mask->m_hObject) : nullptr);
}
int CImageList::GetImageCount() const { return ImageList_GetImageCount(m_hImageList); }
BOOL CImageList::Draw(CDC *dc, int image, POINT pt, UINT style) { return ImageList_Draw(m_hImageList, image, dc != nullptr ? dc->GetSafeHdc() : nullptr, pt.x, pt.y, style); }
COLORREF CImageList::SetBkColor(COLORREF color) { return ImageList_SetBkColor(m_hImageList, color); }
BOOL CImageList::SetImageCount(UINT count) { return ImageList_SetImageCount(m_hImageList, count); }
HICON CImageList::ExtractIcon(int image) { return ImageList_GetIcon(m_hImageList, image, 0); }

// ---- CTabCtrl --------------------------------------------------------------------------------

BOOL CTabCtrl::Create(DWORD style, const RECT &rect, CWnd *parent, UINT id)
{
	return CWnd::Create(WC_TABCONTROL, nullptr, style, rect, parent, id);
}

LONG CTabCtrl::InsertItem(int item, TCITEM *tcItem) { return static_cast<LONG>(::SendMessage(m_hWnd, TCM_INSERTITEM, item, reinterpret_cast<LPARAM>(tcItem))); }
LONG CTabCtrl::InsertItem(UINT mask, int item, LPCTSTR text, int image, LPARAM lParam, DWORD state, DWORD stateMask)
{
	TCITEM tci{};
	tci.mask = mask;
	tci.pszText = const_cast<LPTSTR>(text);
	tci.iImage = image;
	tci.lParam = lParam;
	tci.dwState = state;
	tci.dwStateMask = stateMask;
	return InsertItem(item, &tci);
}
LONG CTabCtrl::InsertItem(UINT mask, int item, LPCTSTR text, int image, LPARAM lParam) { return InsertItem(mask, item, text, image, lParam, 0, 0); }
LONG CTabCtrl::InsertItem(int item, LPCTSTR text, int image) { return InsertItem(TCIF_TEXT | TCIF_IMAGE, item, text, image, 0); }
BOOL CTabCtrl::InsertItem(int item, LPCTSTR text) { return InsertItem(TCIF_TEXT, item, text, 0, 0) >= 0; }
BOOL CTabCtrl::DeleteItem(int item) { return static_cast<BOOL>(::SendMessage(m_hWnd, TCM_DELETEITEM, item, 0)); }
BOOL CTabCtrl::DeleteAllItems() { return static_cast<BOOL>(::SendMessage(m_hWnd, TCM_DELETEALLITEMS, 0, 0)); }
int CTabCtrl::GetItemCount() const { return static_cast<int>(::SendMessage(m_hWnd, TCM_GETITEMCOUNT, 0, 0)); }
int CTabCtrl::GetCurSel() const { return static_cast<int>(::SendMessage(m_hWnd, TCM_GETCURSEL, 0, 0)); }
int CTabCtrl::SetCurSel(int item) { return static_cast<int>(::SendMessage(m_hWnd, TCM_SETCURSEL, item, 0)); }
int CTabCtrl::GetCurFocus() const { return static_cast<int>(::SendMessage(m_hWnd, TCM_GETCURFOCUS, 0, 0)); }
BOOL CTabCtrl::GetItem(int item, TCITEM *tcItem) const { return static_cast<BOOL>(::SendMessage(m_hWnd, TCM_GETITEM, item, reinterpret_cast<LPARAM>(tcItem))); }
BOOL CTabCtrl::SetItem(int item, TCITEM *tcItem) { return static_cast<BOOL>(::SendMessage(m_hWnd, TCM_SETITEM, item, reinterpret_cast<LPARAM>(tcItem))); }
BOOL CTabCtrl::GetItemRect(int item, LPRECT rect) const { return static_cast<BOOL>(::SendMessage(m_hWnd, TCM_GETITEMRECT, item, reinterpret_cast<LPARAM>(rect))); }
void CTabCtrl::AdjustRect(BOOL larger, LPRECT rect) { ::SendMessage(m_hWnd, TCM_ADJUSTRECT, larger, reinterpret_cast<LPARAM>(rect)); }
CImageList *CTabCtrl::SetImageList(CImageList *list)
{
	return CImageList::FromHandle(reinterpret_cast<HIMAGELIST>(::SendMessage(m_hWnd, TCM_SETIMAGELIST, 0, reinterpret_cast<LPARAM>(list != nullptr ? list->m_hImageList : nullptr))));
}
