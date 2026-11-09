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

// ---- CListCtrl -------------------------------------------------------------------------------

BOOL CListCtrl::Create(DWORD style, const RECT &rect, CWnd *parent, UINT id)
{
	return CWnd::Create(WC_LISTVIEW, nullptr, style, rect, parent, id);
}

void CListCtrl::SetItemCount(int count) { ::SendMessage(m_hWnd, LVM_SETITEMCOUNT, count, 0); }
BOOL CListCtrl::SetItemCountEx(int count, DWORD flags) { return static_cast<BOOL>(::SendMessage(m_hWnd, LVM_SETITEMCOUNT, count, flags)); }

int CListCtrl::InsertColumn(int col, LPCTSTR heading, int format, int width, int subItem)
{
	LVCOLUMN column{};
	column.mask = LVCF_TEXT | LVCF_FMT;
	column.pszText = const_cast<LPTSTR>(heading);
	column.fmt = format;
	if(width != -1)
	{
		column.mask |= LVCF_WIDTH;
		column.cx = width;
	}
	if(subItem != -1)
	{
		column.mask |= LVCF_SUBITEM;
		column.iSubItem = subItem;
	}
	return static_cast<int>(::SendMessage(m_hWnd, LVM_INSERTCOLUMN, col, reinterpret_cast<LPARAM>(&column)));
}

BOOL CListCtrl::DeleteColumn(int col) { return static_cast<BOOL>(::SendMessage(m_hWnd, LVM_DELETECOLUMN, col, 0)); }
int CListCtrl::GetColumnWidth(int col) const { return static_cast<int>(::SendMessage(m_hWnd, LVM_GETCOLUMNWIDTH, col, 0)); }
BOOL CListCtrl::SetColumnWidth(int col, int width) { return static_cast<BOOL>(::SendMessage(m_hWnd, LVM_SETCOLUMNWIDTH, col, MAKELPARAM(width, 0))); }
BOOL CListCtrl::GetColumn(int col, LVCOLUMN *column) const { return static_cast<BOOL>(::SendMessage(m_hWnd, LVM_GETCOLUMN, col, reinterpret_cast<LPARAM>(column))); }
BOOL CListCtrl::SetColumn(int col, const LVCOLUMN *column) { return static_cast<BOOL>(::SendMessage(m_hWnd, LVM_SETCOLUMN, col, reinterpret_cast<LPARAM>(column))); }
int CListCtrl::GetItemCount() const { return static_cast<int>(::SendMessage(m_hWnd, LVM_GETITEMCOUNT, 0, 0)); }

int CListCtrl::InsertItem(const LVITEM *item) { return static_cast<int>(::SendMessage(m_hWnd, LVM_INSERTITEM, 0, reinterpret_cast<LPARAM>(item))); }
int CListCtrl::InsertItem(int item, LPCTSTR text) { return InsertItem(LVIF_TEXT, item, text, 0, 0, 0, 0); }
int CListCtrl::InsertItem(int item, LPCTSTR text, int image) { return InsertItem(LVIF_TEXT | LVIF_IMAGE, item, text, 0, 0, image, 0); }
int CListCtrl::InsertItem(UINT mask, int item, LPCTSTR text, UINT state, UINT stateMask, int image, LPARAM lParam)
{
	LVITEM lvi{};
	lvi.mask = mask;
	lvi.iItem = item;
	lvi.pszText = const_cast<LPTSTR>(text);
	lvi.state = state;
	lvi.stateMask = stateMask;
	lvi.iImage = image;
	lvi.lParam = lParam;
	return InsertItem(&lvi);
}

BOOL CListCtrl::SetItem(const LVITEM *item) { return static_cast<BOOL>(::SendMessage(m_hWnd, LVM_SETITEM, 0, reinterpret_cast<LPARAM>(item))); }
BOOL CListCtrl::GetItem(LVITEM *item) const { return static_cast<BOOL>(::SendMessage(m_hWnd, LVM_GETITEM, 0, reinterpret_cast<LPARAM>(item))); }
BOOL CListCtrl::DeleteItem(int item) { return static_cast<BOOL>(::SendMessage(m_hWnd, LVM_DELETEITEM, item, 0)); }
BOOL CListCtrl::DeleteAllItems() { return static_cast<BOOL>(::SendMessage(m_hWnd, LVM_DELETEALLITEMS, 0, 0)); }

BOOL CListCtrl::SetItemText(int item, int subItem, LPCTSTR text)
{
	LVITEM lvi{};
	lvi.iSubItem = subItem;
	lvi.pszText = const_cast<LPTSTR>(text);
	return static_cast<BOOL>(::SendMessage(m_hWnd, LVM_SETITEMTEXT, item, reinterpret_cast<LPARAM>(&lvi)));
}

CString CListCtrl::GetItemText(int item, int subItem) const
{
	// Grow the buffer until the text fits, like MFC does.
	std::vector<TCHAR> buffer(256);
	for(;;)
	{
		LVITEM lvi{};
		lvi.iSubItem = subItem;
		lvi.pszText = buffer.data();
		lvi.cchTextMax = static_cast<int>(buffer.size());
		const auto length = static_cast<size_t>(::SendMessage(m_hWnd, LVM_GETITEMTEXT, item, reinterpret_cast<LPARAM>(&lvi)));
		if(length + 1 < buffer.size())
			return CString(buffer.data());
		buffer.resize(buffer.size() * 2);
	}
}

BOOL CListCtrl::SetItemData(int item, DWORD_PTR data)
{
	LVITEM lvi{};
	lvi.mask = LVIF_PARAM;
	lvi.iItem = item;
	lvi.lParam = static_cast<LPARAM>(data);
	return SetItem(&lvi);
}

DWORD_PTR CListCtrl::GetItemData(int item) const
{
	LVITEM lvi{};
	lvi.mask = LVIF_PARAM;
	lvi.iItem = item;
	GetItem(&lvi);
	return static_cast<DWORD_PTR>(lvi.lParam);
}

BOOL CListCtrl::SetItemState(int item, UINT state, UINT mask)
{
	LVITEM lvi{};
	lvi.state = state;
	lvi.stateMask = mask;
	return static_cast<BOOL>(::SendMessage(m_hWnd, LVM_SETITEMSTATE, item, reinterpret_cast<LPARAM>(&lvi)));
}

UINT CListCtrl::GetItemState(int item, UINT mask) const { return static_cast<UINT>(::SendMessage(m_hWnd, LVM_GETITEMSTATE, item, mask)); }
BOOL CListCtrl::SetCheck(int item, BOOL check) { return SetItemState(item, INDEXTOSTATEIMAGEMASK(check ? 2 : 1), LVIS_STATEIMAGEMASK); }
BOOL CListCtrl::GetCheck(int item) const { return ((GetItemState(item, LVIS_STATEIMAGEMASK) >> 12) - 1) > 0; }
UINT CListCtrl::GetSelectedCount() const { return static_cast<UINT>(::SendMessage(m_hWnd, LVM_GETSELECTEDCOUNT, 0, 0)); }
int CListCtrl::GetSelectionMark() const { return static_cast<int>(::SendMessage(m_hWnd, LVM_GETSELECTIONMARK, 0, 0)); }
int CListCtrl::SetSelectionMark(int item) { return static_cast<int>(::SendMessage(m_hWnd, LVM_SETSELECTIONMARK, 0, item)); }
int CListCtrl::GetNextItem(int item, int flags) const { return static_cast<int>(::SendMessage(m_hWnd, LVM_GETNEXTITEM, item, MAKELPARAM(flags, 0))); }

// MFC encodes the position as index + 1, so that nullptr ends the walk.
POSITION CListCtrl::GetFirstSelectedItemPosition() const
{
	return reinterpret_cast<POSITION>(static_cast<INT_PTR>(GetNextItem(-1, LVNI_SELECTED) + 1));
}

int CListCtrl::GetNextSelectedItem(POSITION &pos) const
{
	const int item = static_cast<int>(reinterpret_cast<INT_PTR>(pos)) - 1;
	pos = reinterpret_cast<POSITION>(static_cast<INT_PTR>(GetNextItem(item, LVNI_SELECTED) + 1));
	return item;
}

BOOL CListCtrl::EnsureVisible(int item, BOOL partialOK) { return static_cast<BOOL>(::SendMessage(m_hWnd, LVM_ENSUREVISIBLE, item, partialOK)); }
DWORD CListCtrl::GetExtendedStyle() const { return static_cast<DWORD>(::SendMessage(m_hWnd, LVM_GETEXTENDEDLISTVIEWSTYLE, 0, 0)); }
DWORD CListCtrl::SetExtendedStyle(DWORD style) { return static_cast<DWORD>(::SendMessage(m_hWnd, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, style)); }

CImageList *CListCtrl::SetImageList(CImageList *list, int type)
{
	const auto old = reinterpret_cast<HIMAGELIST>(::SendMessage(m_hWnd, LVM_SETIMAGELIST, type, reinterpret_cast<LPARAM>(list != nullptr ? list->GetSafeHandle() : nullptr)));
	return CImageList::FromHandle(old);
}

int CListCtrl::GetTopIndex() const { return static_cast<int>(::SendMessage(m_hWnd, LVM_GETTOPINDEX, 0, 0)); }
int CListCtrl::GetCountPerPage() const { return static_cast<int>(::SendMessage(m_hWnd, LVM_GETCOUNTPERPAGE, 0, 0)); }

BOOL CListCtrl::GetItemRect(int item, LPRECT rect, UINT code) const
{
	rect->left = static_cast<LONG>(code);
	return static_cast<BOOL>(::SendMessage(m_hWnd, LVM_GETITEMRECT, item, reinterpret_cast<LPARAM>(rect)));
}

int CListCtrl::HitTest(CPoint pt, UINT *flags) const
{
	LVHITTESTINFO info{};
	info.pt = pt;
	const int item = static_cast<int>(::SendMessage(m_hWnd, LVM_HITTEST, 0, reinterpret_cast<LPARAM>(&info)));
	if(flags != nullptr)
		*flags = info.flags;
	return item;
}

LRESULT CListCtrl::EnableGroupView(BOOL enable) { return ::SendMessage(m_hWnd, LVM_ENABLEGROUPVIEW, enable, 0); }
int CListCtrl::InsertGroup(int index, LVGROUP *group) { return static_cast<int>(::SendMessage(m_hWnd, LVM_INSERTGROUP, index, reinterpret_cast<LPARAM>(group))); }
void CListCtrl::RemoveAllGroups() { ::SendMessage(m_hWnd, LVM_REMOVEALLGROUPS, 0, 0); }

BOOL CListCtrl::SortItems(int (*compare)(LPARAM, LPARAM, LPARAM), DWORD_PTR data)
{
	return static_cast<BOOL>(::SendMessage(m_hWnd, LVM_SORTITEMS, static_cast<WPARAM>(data), reinterpret_cast<LPARAM>(compare)));
}

CEdit *CListCtrl::EditLabel(int item) { return reinterpret_cast<CEdit *>(CWnd::FromHandle(reinterpret_cast<HWND>(::SendMessage(m_hWnd, LVM_EDITLABEL, item, 0)))); }
CEdit *CListCtrl::GetEditControl() const { return reinterpret_cast<CEdit *>(CWnd::FromHandle(reinterpret_cast<HWND>(::SendMessage(m_hWnd, LVM_GETEDITCONTROL, 0, 0)))); }
CHeaderCtrl *CListCtrl::GetHeaderCtrl() const { return reinterpret_cast<CHeaderCtrl *>(CWnd::FromHandle(reinterpret_cast<HWND>(::SendMessage(m_hWnd, LVM_GETHEADER, 0, 0)))); }
BOOL CListCtrl::RedrawItems(int first, int last) { return static_cast<BOOL>(::SendMessage(m_hWnd, LVM_REDRAWITEMS, first, last)); }
BOOL CListCtrl::Update(int item) { return static_cast<BOOL>(::SendMessage(m_hWnd, LVM_UPDATE, item, 0)); }

BOOL CListCtrl::GetSubItemRect(int item, int subItem, int area, CRect &rect)
{
	RECT r{area, subItem, 0, 0};
	const BOOL ok = static_cast<BOOL>(::SendMessage(m_hWnd, LVM_GETSUBITEMRECT, item, reinterpret_cast<LPARAM>(&r)));
	rect = r;
	return ok;
}

int CListCtrl::SubItemHitTest(LVHITTESTINFO *info) { return static_cast<int>(::SendMessage(m_hWnd, LVM_SUBITEMHITTEST, 0, reinterpret_cast<LPARAM>(info))); }
COLORREF CListCtrl::GetBkColor() const { return static_cast<COLORREF>(::SendMessage(m_hWnd, LVM_GETBKCOLOR, 0, 0)); }
BOOL CListCtrl::SetBkColor(COLORREF color) { return static_cast<BOOL>(::SendMessage(m_hWnd, LVM_SETBKCOLOR, 0, static_cast<LPARAM>(color))); }
COLORREF CListCtrl::GetTextColor() const { return static_cast<COLORREF>(::SendMessage(m_hWnd, LVM_GETTEXTCOLOR, 0, 0)); }
BOOL CListCtrl::SetTextColor(COLORREF color) { return static_cast<BOOL>(::SendMessage(m_hWnd, LVM_SETTEXTCOLOR, 0, static_cast<LPARAM>(color))); }
COLORREF CListCtrl::GetTextBkColor() const { return static_cast<COLORREF>(::SendMessage(m_hWnd, LVM_GETTEXTBKCOLOR, 0, 0)); }
BOOL CListCtrl::SetTextBkColor(COLORREF color) { return static_cast<BOOL>(::SendMessage(m_hWnd, LVM_SETTEXTBKCOLOR, 0, static_cast<LPARAM>(color))); }
BOOL CListCtrl::GetViewRect(LPRECT rect) const { return static_cast<BOOL>(::SendMessage(m_hWnd, LVM_GETVIEWRECT, 0, reinterpret_cast<LPARAM>(rect))); }

CSize CListCtrl::ApproximateViewRect(CSize size, int count) const
{
	const auto r = static_cast<DWORD>(::SendMessage(m_hWnd, LVM_APPROXIMATEVIEWRECT, count, MAKELPARAM(size.cx, size.cy)));
	return CSize(LOWORD(r), HIWORD(r));
}

// ---- CMFCListCtrl ----------------------------------------------------------------------------
// Per-cell colours through custom draw, and sorting by a click on a column header.

BEGIN_MESSAGE_MAP(CMFCListCtrl, CListCtrl)
	ON_NOTIFY_REFLECT(NM_CUSTOMDRAW, &CMFCListCtrl::OnCustomDraw)
	ON_NOTIFY_EX(HDN_ITEMCLICK, 0, &CMFCListCtrl::OnHeaderItemClick)
END_MESSAGE_MAP()

int CMFCListCtrl::OnCompareItems(LPARAM, LPARAM, int) { return 0; }
COLORREF CMFCListCtrl::OnGetCellTextColor(int, int) { return GetTextColor(); }
COLORREF CMFCListCtrl::OnGetCellBkColor(int, int) { return GetBkColor(); }
void CMFCListCtrl::EnableMarkSortedColumn(BOOL mark, BOOL redraw)
{
	m_bMarkSortedColumn = mark;
	if(redraw && m_hWnd != nullptr)
		Invalidate();
}

int CALLBACK CMFCListCtrl::CompareProc(LPARAM item1, LPARAM item2, LPARAM self)
{
	auto *list = reinterpret_cast<CMFCListCtrl *>(self);
	const int result = list->OnCompareItems(item1, item2, list->m_iSortedColumn);
	return list->m_bAscending ? result : -result;
}

void CMFCListCtrl::Sort(int column, BOOL ascending, BOOL)
{
	m_iSortedColumn = column;
	m_bAscending = ascending;
	// Show the sort arrow on the header, as CMFCHeaderCtrl does.
	if(auto *header = GetHeaderCtrl())
	{
		const int count = header->GetItemCount();
		for(int i = 0; i < count; i++)
		{
			HDITEM item{};
			item.mask = HDI_FORMAT;
			header->GetItem(i, &item);
			item.fmt &= ~(HDF_SORTUP | HDF_SORTDOWN);
			if(i == column)
				item.fmt |= ascending ? HDF_SORTUP : HDF_SORTDOWN;
			header->SetItem(i, &item);
		}
	}
	SortItems(&CMFCListCtrl::CompareProc, reinterpret_cast<DWORD_PTR>(this));
}

// The header is the list's own child, so this works whatever the parent is.
// FALSE lets the list go on to send LVN_COLUMNCLICK.
BOOL CMFCListCtrl::OnHeaderItemClick(UINT, NMHDR *nmhdr, LRESULT *)
{
	const int column = reinterpret_cast<NMHEADER *>(nmhdr)->iItem;
	Sort(column, column == m_iSortedColumn ? !m_bAscending : TRUE);
	return FALSE;
}

void CMFCListCtrl::OnCustomDraw(NMHDR *nmhdr, LRESULT *result)
{
	auto *cd = reinterpret_cast<NMLVCUSTOMDRAW *>(nmhdr);
	switch(cd->nmcd.dwDrawStage)
	{
	case CDDS_PREPAINT:
		*result = CDRF_NOTIFYITEMDRAW;
		break;
	case CDDS_ITEMPREPAINT:
		*result = CDRF_NOTIFYSUBITEMDRAW;
		break;
	case CDDS_ITEMPREPAINT | CDDS_SUBITEM:
		{
			const int row = static_cast<int>(cd->nmcd.dwItemSpec);
			cd->clrText = OnGetCellTextColor(row, cd->iSubItem);
			cd->clrTextBk = OnGetCellBkColor(row, cd->iSubItem);
			*result = CDRF_DODEFAULT;
		}
		break;
	default:
		*result = CDRF_DODEFAULT;
		break;
	}
}

// ---- CHeaderCtrl -----------------------------------------------------------------------------

BOOL CHeaderCtrl::Create(DWORD style, const RECT &rect, CWnd *parent, UINT id) { return CWnd::Create(WC_HEADER, nullptr, style, rect, parent, id); }
int CHeaderCtrl::GetItemCount() const { return static_cast<int>(::SendMessage(m_hWnd, HDM_GETITEMCOUNT, 0, 0)); }
BOOL CHeaderCtrl::GetItemRect(int index, LPRECT rect) const { return static_cast<BOOL>(::SendMessage(m_hWnd, HDM_GETITEMRECT, index, reinterpret_cast<LPARAM>(rect))); }
BOOL CHeaderCtrl::GetItem(int index, HDITEM *item) const { return static_cast<BOOL>(::SendMessage(m_hWnd, HDM_GETITEM, index, reinterpret_cast<LPARAM>(item))); }
BOOL CHeaderCtrl::SetItem(int index, HDITEM *item) { return static_cast<BOOL>(::SendMessage(m_hWnd, HDM_SETITEM, index, reinterpret_cast<LPARAM>(item))); }
int CHeaderCtrl::InsertItem(int index, HDITEM *item) { return static_cast<int>(::SendMessage(m_hWnd, HDM_INSERTITEM, index, reinterpret_cast<LPARAM>(item))); }
BOOL CHeaderCtrl::DeleteItem(int index) { return static_cast<BOOL>(::SendMessage(m_hWnd, HDM_DELETEITEM, index, 0)); }
BOOL CHeaderCtrl::GetOrderArray(LPINT array, int count) const
{
	if(count < 0)
		count = GetItemCount();
	return static_cast<BOOL>(::SendMessage(m_hWnd, HDM_GETORDERARRAY, count, reinterpret_cast<LPARAM>(array)));
}
BOOL CHeaderCtrl::SetOrderArray(int count, LPINT array) { return static_cast<BOOL>(::SendMessage(m_hWnd, HDM_SETORDERARRAY, count, reinterpret_cast<LPARAM>(array))); }
int CHeaderCtrl::OrderToIndex(int order) const { return static_cast<int>(::SendMessage(m_hWnd, HDM_ORDERTOINDEX, order, 0)); }
int CHeaderCtrl::HitTest(LPHDHITTESTINFO info) { return static_cast<int>(::SendMessage(m_hWnd, HDM_HITTEST, 0, reinterpret_cast<LPARAM>(info))); }
CImageList *CHeaderCtrl::SetImageList(CImageList *list)
{
	return CImageList::FromHandle(reinterpret_cast<HIMAGELIST>(::SendMessage(m_hWnd, HDM_SETIMAGELIST, 0, reinterpret_cast<LPARAM>(list != nullptr ? list->GetSafeHandle() : nullptr))));
}
BOOL CHeaderCtrl::Layout(HDLAYOUT *layout) { return static_cast<BOOL>(::SendMessage(m_hWnd, HDM_LAYOUT, 0, reinterpret_cast<LPARAM>(layout))); }

// ---- CTreeCtrl -------------------------------------------------------------------------------

BOOL CTreeCtrl::Create(DWORD style, const RECT &rect, CWnd *parent, UINT id) { return CWnd::Create(WC_TREEVIEW, nullptr, style, rect, parent, id); }
HTREEITEM CTreeCtrl::InsertItem(LPTVINSERTSTRUCT insert) { return TreeView_InsertItem(m_hWnd, insert); }

HTREEITEM CTreeCtrl::InsertItem(UINT mask, LPCTSTR text, int image, int selectedImage, UINT state, UINT stateMask, LPARAM lParam, HTREEITEM parent, HTREEITEM after)
{
	TVINSERTSTRUCT is{};
	is.hParent = parent;
	is.hInsertAfter = after;
	is.item.mask = mask;
	is.item.pszText = const_cast<LPTSTR>(text);
	is.item.iImage = image;
	is.item.iSelectedImage = selectedImage;
	is.item.state = state;
	is.item.stateMask = stateMask;
	is.item.lParam = lParam;
	return InsertItem(&is);
}

HTREEITEM CTreeCtrl::InsertItem(LPCTSTR text, HTREEITEM parent, HTREEITEM after) { return InsertItem(TVIF_TEXT, text, 0, 0, 0, 0, 0, parent, after); }
HTREEITEM CTreeCtrl::InsertItem(LPCTSTR text, int image, int selectedImage, HTREEITEM parent, HTREEITEM after) { return InsertItem(TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE, text, image, selectedImage, 0, 0, 0, parent, after); }
BOOL CTreeCtrl::DeleteItem(HTREEITEM item) { return TreeView_DeleteItem(m_hWnd, item); }
BOOL CTreeCtrl::DeleteAllItems() { return TreeView_DeleteAllItems(m_hWnd); }
BOOL CTreeCtrl::Expand(HTREEITEM item, UINT code) { return TreeView_Expand(m_hWnd, item, code); }
HTREEITEM CTreeCtrl::GetNextItem(HTREEITEM item, UINT code) const { return TreeView_GetNextItem(m_hWnd, item, code); }
HTREEITEM CTreeCtrl::GetChildItem(HTREEITEM item) const { return TreeView_GetChild(m_hWnd, item); }
HTREEITEM CTreeCtrl::GetNextSiblingItem(HTREEITEM item) const { return TreeView_GetNextSibling(m_hWnd, item); }
HTREEITEM CTreeCtrl::GetPrevSiblingItem(HTREEITEM item) const { return TreeView_GetPrevSibling(m_hWnd, item); }
HTREEITEM CTreeCtrl::GetParentItem(HTREEITEM item) const { return TreeView_GetParent(m_hWnd, item); }
HTREEITEM CTreeCtrl::GetFirstVisibleItem() const { return TreeView_GetFirstVisible(m_hWnd); }
HTREEITEM CTreeCtrl::GetNextVisibleItem(HTREEITEM item) const { return TreeView_GetNextVisible(m_hWnd, item); }
HTREEITEM CTreeCtrl::GetPrevVisibleItem(HTREEITEM item) const { return TreeView_GetPrevVisible(m_hWnd, item); }
HTREEITEM CTreeCtrl::GetSelectedItem() const { return TreeView_GetSelection(m_hWnd); }
HTREEITEM CTreeCtrl::GetDropHilightItem() const { return TreeView_GetDropHilight(m_hWnd); }
HTREEITEM CTreeCtrl::GetRootItem() const { return TreeView_GetRoot(m_hWnd); }

BOOL CTreeCtrl::ItemHasChildren(HTREEITEM item) const
{
	TVITEM tvi{};
	tvi.mask = TVIF_CHILDREN | TVIF_HANDLE;
	tvi.hItem = item;
	if(!GetItem(&tvi))
		return FALSE;
	return tvi.cChildren != 0;
}

BOOL CTreeCtrl::GetItem(TVITEM *item) const { return TreeView_GetItem(m_hWnd, item); }
BOOL CTreeCtrl::SetItem(TVITEM *item) { return TreeView_SetItem(m_hWnd, item); }

BOOL CTreeCtrl::SetItem(HTREEITEM item, UINT mask, LPCTSTR text, int image, int selectedImage, UINT state, UINT stateMask, LPARAM lParam)
{
	TVITEM tvi{};
	tvi.hItem = item;
	tvi.mask = mask | TVIF_HANDLE;
	tvi.pszText = const_cast<LPTSTR>(text);
	tvi.iImage = image;
	tvi.iSelectedImage = selectedImage;
	tvi.state = state;
	tvi.stateMask = stateMask;
	tvi.lParam = lParam;
	return SetItem(&tvi);
}

CString CTreeCtrl::GetItemText(HTREEITEM item) const
{
	std::vector<TCHAR> buffer(256);
	for(;;)
	{
		TVITEM tvi{};
		tvi.hItem = item;
		tvi.mask = TVIF_TEXT | TVIF_HANDLE;
		tvi.pszText = buffer.data();
		tvi.cchTextMax = static_cast<int>(buffer.size());
		if(!GetItem(&tvi))
			return CString();
		if(std::char_traits<TCHAR>::length(buffer.data()) + 1 < buffer.size())
			return CString(buffer.data());
		buffer.resize(buffer.size() * 2);
	}
}

BOOL CTreeCtrl::SetItemText(HTREEITEM item, LPCTSTR text) { return SetItem(item, TVIF_TEXT, text, 0, 0, 0, 0, 0); }

BOOL CTreeCtrl::GetItemImage(HTREEITEM item, int &image, int &selectedImage) const
{
	TVITEM tvi{};
	tvi.hItem = item;
	tvi.mask = TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_HANDLE;
	const BOOL ok = GetItem(&tvi);
	image = tvi.iImage;
	selectedImage = tvi.iSelectedImage;
	return ok;
}

BOOL CTreeCtrl::SetItemImage(HTREEITEM item, int image, int selectedImage) { return SetItem(item, TVIF_IMAGE | TVIF_SELECTEDIMAGE, nullptr, image, selectedImage, 0, 0, 0); }
UINT CTreeCtrl::GetItemState(HTREEITEM item, UINT mask) const { return TreeView_GetItemState(m_hWnd, item, mask); }
BOOL CTreeCtrl::SetItemState(HTREEITEM item, UINT state, UINT mask) { return SetItem(item, TVIF_STATE, nullptr, 0, 0, state, mask, 0); }

DWORD_PTR CTreeCtrl::GetItemData(HTREEITEM item) const
{
	TVITEM tvi{};
	tvi.hItem = item;
	tvi.mask = TVIF_PARAM | TVIF_HANDLE;
	GetItem(&tvi);
	return static_cast<DWORD_PTR>(tvi.lParam);
}

BOOL CTreeCtrl::SetItemData(HTREEITEM item, DWORD_PTR data) { return SetItem(item, TVIF_PARAM, nullptr, 0, 0, 0, 0, static_cast<LPARAM>(data)); }
BOOL CTreeCtrl::GetItemRect(HTREEITEM item, LPRECT rect, BOOL textOnly) const { return TreeView_GetItemRect(m_hWnd, item, rect, textOnly); }
BOOL CTreeCtrl::Select(HTREEITEM item, UINT code) { return TreeView_Select(m_hWnd, item, code); }
BOOL CTreeCtrl::SelectItem(HTREEITEM item) { return TreeView_SelectItem(m_hWnd, item); }
BOOL CTreeCtrl::SelectDropTarget(HTREEITEM item) { return TreeView_SelectDropTarget(m_hWnd, item); }
BOOL CTreeCtrl::SelectSetFirstVisible(HTREEITEM item) { return TreeView_SelectSetFirstVisible(m_hWnd, item); }
BOOL CTreeCtrl::EnsureVisible(HTREEITEM item) { return TreeView_EnsureVisible(m_hWnd, item); }

HTREEITEM CTreeCtrl::HitTest(CPoint pt, UINT *flags) const
{
	TVHITTESTINFO info{};
	info.pt = pt;
	const HTREEITEM item = TreeView_HitTest(m_hWnd, &info);
	if(flags != nullptr)
		*flags = info.flags;
	return item;
}

HTREEITEM CTreeCtrl::HitTest(TVHITTESTINFO *info) const { return TreeView_HitTest(m_hWnd, info); }
CImageList *CTreeCtrl::SetImageList(CImageList *list, int type) { return CImageList::FromHandle(TreeView_SetImageList(m_hWnd, list != nullptr ? list->GetSafeHandle() : nullptr, type)); }
CImageList *CTreeCtrl::GetImageList(UINT type) const { return CImageList::FromHandle(TreeView_GetImageList(m_hWnd, type)); }
UINT CTreeCtrl::GetIndent() const { return TreeView_GetIndent(m_hWnd); }
void CTreeCtrl::SetIndent(UINT indent) { TreeView_SetIndent(m_hWnd, indent); }
UINT CTreeCtrl::GetCount() const { return TreeView_GetCount(m_hWnd); }
UINT CTreeCtrl::GetVisibleCount() const { return TreeView_GetVisibleCount(m_hWnd); }
CEdit *CTreeCtrl::EditLabel(HTREEITEM item) { return reinterpret_cast<CEdit *>(CWnd::FromHandle(TreeView_EditLabel(m_hWnd, item))); }
BOOL CTreeCtrl::EndEditLabelNow(BOOL cancel) { return TreeView_EndEditLabelNow(m_hWnd, cancel); }
CEdit *CTreeCtrl::GetEditControl() const { return reinterpret_cast<CEdit *>(CWnd::FromHandle(TreeView_GetEditControl(m_hWnd))); }
BOOL CTreeCtrl::SortChildren(HTREEITEM item) { return TreeView_SortChildren(m_hWnd, item, 0); }
DWORD CTreeCtrl::SetExtendedStyle(DWORD mask, DWORD style) { return TreeView_SetExtendedStyle(m_hWnd, style, mask); }
CToolTipCtrl *CTreeCtrl::GetToolTips() const { return reinterpret_cast<CToolTipCtrl *>(CWnd::FromHandle(TreeView_GetToolTips(m_hWnd))); }

// ---- CToolBarCtrl ----------------------------------------------------------------------------

BOOL CToolBarCtrl::Create(DWORD style, const RECT &rect, CWnd *parent, UINT id)
{
	if(!CWnd::Create(TOOLBARCLASSNAME, nullptr, style, rect, parent, id))
		return FALSE;
	SetButtonStructSize(sizeof(TBBUTTON));
	return TRUE;
}

void CToolBarCtrl::SetButtonStructSize(int size) { SendMessage(TB_BUTTONSTRUCTSIZE, size); }
BOOL CToolBarCtrl::AddButtons(int count, LPCTBBUTTON buttons) { return static_cast<BOOL>(SendMessage(TB_ADDBUTTONS, count, reinterpret_cast<LPARAM>(buttons))); }
BOOL CToolBarCtrl::InsertButton(int index, LPCTBBUTTON button) { return static_cast<BOOL>(SendMessage(TB_INSERTBUTTON, index, reinterpret_cast<LPARAM>(button))); }
BOOL CToolBarCtrl::DeleteButton(int index) { return static_cast<BOOL>(SendMessage(TB_DELETEBUTTON, index)); }
int CToolBarCtrl::GetButtonCount() const { return static_cast<int>(::SendMessage(m_hWnd, TB_BUTTONCOUNT, 0, 0)); }
BOOL CToolBarCtrl::GetButton(int index, LPTBBUTTON button) const { return static_cast<BOOL>(::SendMessage(m_hWnd, TB_GETBUTTON, index, reinterpret_cast<LPARAM>(button))); }
int CToolBarCtrl::CommandToIndex(UINT id) const { return static_cast<int>(::SendMessage(m_hWnd, TB_COMMANDTOINDEX, id, 0)); }
BOOL CToolBarCtrl::GetButtonInfo(int id, TBBUTTONINFO *info) const { return static_cast<BOOL>(::SendMessage(m_hWnd, TB_GETBUTTONINFO, id, reinterpret_cast<LPARAM>(info)) != -1); }
BOOL CToolBarCtrl::SetButtonInfo(int id, TBBUTTONINFO *info) { return static_cast<BOOL>(SendMessage(TB_SETBUTTONINFO, id, reinterpret_cast<LPARAM>(info))); }
BOOL CToolBarCtrl::CheckButton(int id, BOOL check) { return static_cast<BOOL>(SendMessage(TB_CHECKBUTTON, id, MAKELPARAM(check, 0))); }
BOOL CToolBarCtrl::EnableButton(int id, BOOL enable) { return static_cast<BOOL>(SendMessage(TB_ENABLEBUTTON, id, MAKELPARAM(enable, 0))); }
BOOL CToolBarCtrl::PressButton(int id, BOOL press) { return static_cast<BOOL>(SendMessage(TB_PRESSBUTTON, id, MAKELPARAM(press, 0))); }
BOOL CToolBarCtrl::HideButton(int id, BOOL hide) { return static_cast<BOOL>(SendMessage(TB_HIDEBUTTON, id, MAKELPARAM(hide, 0))); }
BOOL CToolBarCtrl::IsButtonChecked(int id) const { return ::SendMessage(m_hWnd, TB_ISBUTTONCHECKED, id, 0) > 0; }
BOOL CToolBarCtrl::IsButtonEnabled(int id) const { return ::SendMessage(m_hWnd, TB_ISBUTTONENABLED, id, 0) > 0; }
BOOL CToolBarCtrl::IsButtonPressed(int id) const { return ::SendMessage(m_hWnd, TB_ISBUTTONPRESSED, id, 0) > 0; }
BOOL CToolBarCtrl::SetState(int id, UINT state) { return static_cast<BOOL>(SendMessage(TB_SETSTATE, id, MAKELPARAM(state, 0))); }
int CToolBarCtrl::GetState(int id) const { return static_cast<int>(::SendMessage(m_hWnd, TB_GETSTATE, id, 0)); }
BOOL CToolBarCtrl::GetItemRect(int index, LPRECT rect) const { return static_cast<BOOL>(::SendMessage(m_hWnd, TB_GETITEMRECT, index, reinterpret_cast<LPARAM>(rect))); }
BOOL CToolBarCtrl::GetRect(int id, LPRECT rect) const { return static_cast<BOOL>(::SendMessage(m_hWnd, TB_GETRECT, id, reinterpret_cast<LPARAM>(rect))); }
BOOL CToolBarCtrl::SetBitmapSize(CSize size) { return static_cast<BOOL>(SendMessage(TB_SETBITMAPSIZE, 0, MAKELPARAM(size.cx, size.cy))); }
BOOL CToolBarCtrl::SetButtonSize(CSize size) { return static_cast<BOOL>(SendMessage(TB_SETBUTTONSIZE, 0, MAKELPARAM(size.cx, size.cy))); }

CSize CToolBarCtrl::GetButtonSize() const
{
	const auto r = static_cast<DWORD>(::SendMessage(m_hWnd, TB_GETBUTTONSIZE, 0, 0));
	return CSize(LOWORD(r), HIWORD(r));
}

BOOL CToolBarCtrl::SetIndent(int indent) { return static_cast<BOOL>(SendMessage(TB_SETINDENT, indent)); }
DWORD CToolBarCtrl::GetExtendedStyle() const { return static_cast<DWORD>(::SendMessage(m_hWnd, TB_GETEXTENDEDSTYLE, 0, 0)); }
DWORD CToolBarCtrl::SetExtendedStyle(DWORD style) { return static_cast<DWORD>(SendMessage(TB_SETEXTENDEDSTYLE, 0, style)); }
CImageList *CToolBarCtrl::SetImageList(CImageList *list) { return CImageList::FromHandle(reinterpret_cast<HIMAGELIST>(SendMessage(TB_SETIMAGELIST, 0, reinterpret_cast<LPARAM>(list != nullptr ? list->GetSafeHandle() : nullptr)))); }
CImageList *CToolBarCtrl::SetDisabledImageList(CImageList *list) { return CImageList::FromHandle(reinterpret_cast<HIMAGELIST>(SendMessage(TB_SETDISABLEDIMAGELIST, 0, reinterpret_cast<LPARAM>(list != nullptr ? list->GetSafeHandle() : nullptr)))); }
BOOL CToolBarCtrl::ChangeBitmap(int id, int bitmap) { return static_cast<BOOL>(SendMessage(TB_CHANGEBITMAP, id, MAKELPARAM(bitmap, 0))); }
void CToolBarCtrl::AutoSize() { SendMessage(TB_AUTOSIZE); }
int CToolBarCtrl::HitTest(LPPOINT pt) const { return static_cast<int>(::SendMessage(m_hWnd, TB_HITTEST, 0, reinterpret_cast<LPARAM>(pt))); }
int CToolBarCtrl::GetHotItem() const { return static_cast<int>(::SendMessage(m_hWnd, TB_GETHOTITEM, 0, 0)); }
CToolTipCtrl *CToolBarCtrl::GetToolTips() const { return reinterpret_cast<CToolTipCtrl *>(CWnd::FromHandle(reinterpret_cast<HWND>(::SendMessage(m_hWnd, TB_GETTOOLTIPS, 0, 0)))); }

// CComboBoxEx

BOOL CComboBoxEx::Create(DWORD style, const RECT &rect, CWnd *parent, UINT id)
{
	return CWnd::Create(WC_COMBOBOXEX, nullptr, style, rect, parent, id);
}

BOOL CComboBoxEx::CreateEx(DWORD exStyle, DWORD style, const RECT &rect, CWnd *parent, UINT id)
{
	return CWnd::CreateEx(exStyle, WC_COMBOBOXEX, nullptr, style, rect, parent, id);
}

DWORD CComboBoxEx::GetExtendedStyle() const { return static_cast<DWORD>(::SendMessage(m_hWnd, CBEM_GETEXTENDEDSTYLE, 0, 0)); }
DWORD CComboBoxEx::SetExtendedStyle(DWORD mask, DWORD style) { return static_cast<DWORD>(SendMessage(CBEM_SETEXTENDEDSTYLE, mask, style)); }
BOOL CComboBoxEx::HasEditChanged() { return static_cast<BOOL>(SendMessage(CBEM_HASEDITCHANGED, 0, 0)); }
int CComboBoxEx::InsertItem(const COMBOBOXEXITEM *item) { return static_cast<int>(SendMessage(CBEM_INSERTITEM, 0, reinterpret_cast<LPARAM>(item))); }
int CComboBoxEx::DeleteItem(int index) { return static_cast<int>(SendMessage(CBEM_DELETEITEM, static_cast<WPARAM>(index), 0)); }
BOOL CComboBoxEx::GetItem(COMBOBOXEXITEM *item) { return static_cast<BOOL>(SendMessage(CBEM_GETITEM, 0, reinterpret_cast<LPARAM>(item))); }
BOOL CComboBoxEx::SetItem(const COMBOBOXEXITEM *item) { return static_cast<BOOL>(SendMessage(CBEM_SETITEM, 0, reinterpret_cast<LPARAM>(item))); }
CImageList *CComboBoxEx::SetImageList(CImageList *list)
{
	return CImageList::FromHandle(reinterpret_cast<HIMAGELIST>(SendMessage(CBEM_SETIMAGELIST, 0, reinterpret_cast<LPARAM>(list != nullptr ? list->GetSafeHandle() : nullptr))));
}
CImageList *CComboBoxEx::GetImageList() const { return CImageList::FromHandle(reinterpret_cast<HIMAGELIST>(::SendMessage(m_hWnd, CBEM_GETIMAGELIST, 0, 0))); }
CEdit *CComboBoxEx::GetEditCtrl() const { return reinterpret_cast<CEdit *>(CWnd::FromHandle(reinterpret_cast<HWND>(::SendMessage(m_hWnd, CBEM_GETEDITCONTROL, 0, 0)))); }
CComboBox *CComboBoxEx::GetComboBoxCtrl() const { return reinterpret_cast<CComboBox *>(CWnd::FromHandle(reinterpret_cast<HWND>(::SendMessage(m_hWnd, CBEM_GETCOMBOCONTROL, 0, 0)))); }
