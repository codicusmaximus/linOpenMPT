// linOpenMPT MFC layer: docking and floating control bars, as MFC's
// bardock.cpp, dockcont.cpp, winmini.cpp, winfrm2.cpp and dockstat.cpp.
//
// A frame that EnableDocking() gets a dock bar on each side; control bars
// dock in rows there or float in a CMiniDockFrameWnd. Dragging a bar shows
// where it would go (the outline MFC draws on the screen, here an overlay
// from the Win32 layer) and docks or floats it on release. The bar state is
// saved and loaded in the profile under MFC's own names, so the toolbar
// layout in an existing Windows OpenMPT ini file carries over.
#include "MfcInternal.h"

#include <afxres.h>

#include <algorithm>
#include <cwchar>

namespace
{

constexpr int CxBorder = 1;
constexpr int CyBorder = 1;
constexpr int CxBorder2 = 2;
constexpr int CyBorder2 = 2;

inline DWORD Horz(DWORD style) { return style & CBRS_ORIENT_HORZ; }
inline DWORD Vert(DWORD style) { return style & CBRS_ORIENT_VERT; }

// The dock bars of a frame and the alignment of each (winfrm2.cpp).
const DWORD DockBarMap[4][2] = {
	{AFX_IDW_DOCKBAR_TOP, CBRS_TOP},
	{AFX_IDW_DOCKBAR_BOTTOM, CBRS_BOTTOM},
	{AFX_IDW_DOCKBAR_LEFT, CBRS_LEFT},
	{AFX_IDW_DOCKBAR_RIGHT, CBRS_RIGHT},
};

CControlBar *PlaceHolder(UINT id)
{
	return reinterpret_cast<CControlBar *>(static_cast<UINT_PTR>(id));
}

void AdjustRectangle(CRect &rect, CPoint pt)
{
	// Moves rect so that pt is inside it.
	const int dx = pt.x < rect.left ? pt.x - rect.left : (pt.x > rect.right ? pt.x - rect.right : 0);
	const int dy = pt.y < rect.top ? pt.y - rect.top : (pt.y > rect.bottom ? pt.y - rect.bottom : 0);
	rect.OffsetRect(dx, dy);
}

} // namespace

// ---- CDockBar ------------------------------------------------------------------------

BEGIN_MESSAGE_MAP(CDockBar, CControlBar)
	ON_WM_NCCALCSIZE()
	ON_WM_NCPAINT()
	ON_MESSAGE(WM_SIZEPARENT, &CDockBar::OnSizeParent)
END_MESSAGE_MAP()

CDockBar::CDockBar(BOOL floating)
	: m_bFloating(floating)
{
	m_bAutoDelete = TRUE; // the frame's dock bars are created with new
	m_arrBars.push_back(nullptr);
	m_cxLeftBorder = m_cxRightBorder = m_cyBottomBorder = m_cyTopBorder = 0;
}

CDockBar::~CDockBar()
{
	for(std::size_t i = 0; i < m_arrBars.size(); i++)
	{
		CControlBar *bar = GetDockedControlBar(static_cast<int>(i));
		if(bar != nullptr && bar->m_pDockBar == this)
			bar->m_pDockBar = nullptr;
	}
	DestroyWindow();
}

BOOL CDockBar::Create(CWnd *parent, DWORD style, UINT id)
{
	m_dwStyle = style;
	static LPCTSTR cls = AfxRegisterWndClass(CS_DBLCLKS, ::LoadCursor(nullptr, IDC_ARROW), reinterpret_cast<HBRUSH>(static_cast<INT_PTR>(COLOR_BTNFACE + 1)));
	return CWnd::Create(cls, nullptr, style, CRect(0, 0, 0, 0), parent, id);
}

int CDockBar::GetDockedCount() const
{
	int count = 0;
	for(std::size_t i = 0; i < m_arrBars.size(); i++)
	{
		if(GetDockedControlBar(static_cast<int>(i)) != nullptr)
			count++;
	}
	return count;
}

int CDockBar::GetDockedVisibleCount() const
{
	int count = 0;
	for(std::size_t i = 0; i < m_arrBars.size(); i++)
	{
		CControlBar *bar = GetDockedControlBar(static_cast<int>(i));
		if(bar != nullptr && bar->IsVisible())
			count++;
	}
	return count;
}

int CDockBar::FindBar(CControlBar *bar, int posExclude) const
{
	for(int pos = 0; pos < static_cast<int>(m_arrBars.size()); pos++)
	{
		if(pos != posExclude && m_arrBars[static_cast<std::size_t>(pos)] == bar)
			return pos;
	}
	return -1;
}

void CDockBar::ShowAll(BOOL show)
{
	for(int pos = 0; pos < static_cast<int>(m_arrBars.size()); pos++)
	{
		if(CControlBar *bar = GetDockedControlBar(pos))
			bar->GetDockingFrame()->ShowControlBar(bar, show, TRUE);
	}
}

void CDockBar::DockControlBar(CControlBar *bar, LPCRECT lpRect)
{
	CRect barRect;
	bar->GetWindowRect(&barRect);
	if(bar->m_pDockBar == this && (lpRect == nullptr || barRect == *lpRect))
		return; // nothing changes

	if(m_bFloating && (bar->m_dwDockStyle & CBRS_FLOAT_MULTI))
		m_dwStyle |= CBRS_FLOAT_MULTI;
	m_dwStyle &= ~(CBRS_SIZE_FIXED | CBRS_SIZE_DYNAMIC);
	m_dwStyle |= bar->m_dwStyle & (CBRS_SIZE_FIXED | CBRS_SIZE_DYNAMIC);
	if(!(m_dwStyle & CBRS_FLOAT_MULTI))
	{
		CString title;
		bar->GetWindowText(title);
		AfxSetWindowText(m_hWnd, title);
	}

	// Aligned as this dock bar, with all borders.
	DWORD style = bar->GetBarStyle();
	style &= ~CBRS_ALIGN_ANY;
	style |= (m_dwStyle & CBRS_ALIGN_ANY) | CBRS_BORDER_ANY;
	if(m_bFloating)
		style |= CBRS_FLOATING;
	else
		style &= ~CBRS_FLOATING;
	bar->SetBarStyle(style);

	// Hidden while it moves to another dock bar.
	BOOL show = FALSE;
	if(bar->m_pDockBar != this && bar->IsWindowVisible())
	{
		bar->SetWindowPos(nullptr, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_HIDEWINDOW);
		show = TRUE;
	}

	int pos = -1;
	if(lpRect != nullptr)
	{
		// Into the row the rectangle is over.
		CRect rect(lpRect);
		ScreenToClient(&rect);
		const CPoint mid(rect.left + rect.Width() / 2, rect.top + rect.Height() / 2);
		pos = Insert(bar, rect, mid);
		bar->SetWindowPos(nullptr, rect.left, rect.top, rect.Width(), rect.Height(), SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS);
	} else
	{
		// A new row at the end.
		m_arrBars.push_back(bar);
		m_arrBars.push_back(nullptr);
		bar->SetWindowPos(nullptr, -CxBorder2, -CyBorder2, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS);
	}

	if(bar->GetParent() != this)
		bar->SetParent(this);
	if(bar->m_pDockBar == this)
		bar->m_pDockBar->RemoveControlBar(bar, pos);
	else if(bar->m_pDockBar != nullptr)
		bar->m_pDockBar->RemoveControlBar(bar, -1, m_bFloating && !bar->m_pDockBar->m_bFloating);
	bar->m_pDockBar = this;

	if(show)
		bar->SetWindowPos(nullptr, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);

	RemovePlaceHolder(bar);
	GetDockingFrame()->DelayRecalcLayout();
}

void CDockBar::RemovePlaceHolder(CControlBar *bar)
{
	if(!IsPlaceHolder(bar))
		bar = PlaceHolder(static_cast<UINT>(::GetDlgCtrlID(bar->m_hWnd)));
	const int old = FindBar(bar);
	if(old > 0)
	{
		m_arrBars.erase(m_arrBars.begin() + old);
		// An empty row goes too.
		if(m_arrBars[static_cast<std::size_t>(old - 1)] == nullptr && old < static_cast<int>(m_arrBars.size()) && m_arrBars[static_cast<std::size_t>(old)] == nullptr)
			m_arrBars.erase(m_arrBars.begin() + old);
	}
}

BOOL CDockBar::RemoveControlBar(CControlBar *bar, int posExclude, int addPlaceHolder)
{
	const int pos = FindBar(bar, posExclude);
	if(pos <= 0)
		return FALSE;
	if(addPlaceHolder == 1)
	{
		// Remember where it was, unless that is already remembered.
		m_arrBars[static_cast<std::size_t>(pos)] = PlaceHolder(static_cast<UINT>(::GetDlgCtrlID(bar->m_hWnd)));
		if(FindBar(m_arrBars[static_cast<std::size_t>(pos)], pos) > 0)
		{
			m_arrBars.erase(m_arrBars.begin() + pos);
			if(m_arrBars[static_cast<std::size_t>(pos - 1)] == nullptr && pos < static_cast<int>(m_arrBars.size()) && m_arrBars[static_cast<std::size_t>(pos)] == nullptr)
				m_arrBars.erase(m_arrBars.begin() + pos);
		}
	} else
	{
		m_arrBars.erase(m_arrBars.begin() + pos);
		if(m_arrBars[static_cast<std::size_t>(pos - 1)] == nullptr && pos < static_cast<int>(m_arrBars.size()) && m_arrBars[static_cast<std::size_t>(pos)] == nullptr)
			m_arrBars.erase(m_arrBars.begin() + pos);
		if(addPlaceHolder != -1)
			RemovePlaceHolder(bar);
	}

	// Nothing more while the bar is going away.
	if(bar->m_pDockContext == nullptr)
		return FALSE;

	CFrameWnd *frame = GetDockingFrame();
	if(m_bFloating && GetDockedVisibleCount() == 0)
	{
		if(GetDockedCount() == 0)
		{
			frame->DestroyWindow(); // an empty floating frame goes
			return TRUE;
		}
		frame->ShowWindow(SW_HIDE);
	} else
	{
		frame->DelayRecalcLayout();
	}
	return FALSE;
}

int CDockBar::Insert(CControlBar *barIns, CRect rect, CPoint mid)
{
	int posInsAfter = 0;
	int width = 0;
	int totalWidth = 0;
	const BOOL horz = (m_dwStyle & CBRS_ORIENT_HORZ) != 0;
	for(int pos = 0; pos < static_cast<int>(m_arrBars.size()); pos++)
	{
		CControlBar *entry = m_arrBars[static_cast<std::size_t>(pos)];
		CControlBar *bar = GetDockedControlBar(pos);
		if(entry == nullptr)
		{
			// The end of a row: is the middle of the rectangle above it?
			totalWidth += width - CyBorder2;
			width = 0;
			if((horz ? mid.y : mid.x) < totalWidth)
			{
				if(pos == 0)
					m_arrBars.insert(m_arrBars.begin() + posInsAfter + 1, nullptr);
				m_arrBars.insert(m_arrBars.begin() + posInsAfter + 1, barIns);
				return posInsAfter + 1;
			}
			posInsAfter = pos;
		} else if(bar != nullptr && bar->IsVisible())
		{
			CRect barRect;
			bar->GetWindowRect(&barRect);
			ScreenToClient(&barRect);
			width = std::max<int>(width, horz ? barRect.Height() : barRect.Width() - 1);
			if(horz ? rect.left > barRect.left : rect.top > barRect.top)
				posInsAfter = pos;
		}
	}
	// A new row.
	m_arrBars.insert(m_arrBars.begin() + posInsAfter + 1, nullptr);
	m_arrBars.insert(m_arrBars.begin() + posInsAfter + 1, barIns);
	return posInsAfter + 1;
}

CSize CDockBar::CalcFixedLayout(BOOL stretch, BOOL horz)
{
	CSize sizeFixed = CControlBar::CalcFixedLayout(stretch, horz);
	CSize sizeMax;
	if(!m_rectLayout.IsRectEmpty())
	{
		sizeMax = m_rectLayout.Size();
	} else
	{
		CRect frameRect;
		GetParentFrame()->GetClientRect(&frameRect);
		sizeMax = frameRect.Size();
	}

	AFX_SIZEPARENTPARAMS layout{};
	layout.hDWP = m_bLayoutQuery ? nullptr : ::BeginDeferWindowPos(static_cast<int>(m_arrBars.size()));
	CPoint pt(-CxBorder2, -CyBorder2);
	int width = 0;
	bool wrapped = false;

	for(int pos = 0; pos < static_cast<int>(m_arrBars.size()); pos++)
	{
		CControlBar *bar = GetDockedControlBar(pos);
		CControlBar *entry = m_arrBars[static_cast<std::size_t>(pos)];
		if(bar != nullptr)
		{
			if(bar->IsVisible())
			{
				DWORD mode = 0;
				if((bar->m_dwStyle & CBRS_SIZE_DYNAMIC) && (bar->m_dwStyle & CBRS_FLOATING))
					mode |= LM_HORZ | LM_MRUWIDTH;
				else if(bar->m_dwStyle & CBRS_ORIENT_HORZ)
					mode |= LM_HORZ | LM_HORZDOCK;
				else
					mode |= LM_VERTDOCK;
				const CSize sizeBar = bar->CalcDynamicLayout(-1, mode);
				CRect rect(pt, sizeBar);
				CRect barRect;
				bar->GetWindowRect(&barRect);
				ScreenToClient(&barRect);
				if(horz)
				{
					// Where it is now, if further right.
					if(barRect.left > rect.left && !m_bFloating)
						rect.OffsetRect(barRect.left - rect.left, 0);
					// Off the right edge: right justify.
					if(rect.right > sizeMax.cx && !m_bFloating)
					{
						const int x = std::max<int>(sizeMax.cx - (rect.Width() - CxBorder2), pt.x);
						rect.OffsetRect(x - rect.left, 0);
					}
					if(wrapped)
					{
						// Wrapped onto a row of its own: left justify.
						wrapped = false;
						rect.OffsetRect(-(rect.left + CxBorder2), 0);
					} else if(rect.left >= sizeMax.cx - CxBorder2 && pos > 0 && m_arrBars[static_cast<std::size_t>(pos - 1)] != nullptr)
					{
						// Not to be seen at all: wrap it.
						m_arrBars.insert(m_arrBars.begin() + pos, nullptr);
						bar = nullptr;
						entry = nullptr;
						wrapped = true;
					}
					if(!wrapped)
					{
						if(rect != barRect)
						{
							if(!m_bLayoutQuery && !(bar->m_dwStyle & CBRS_FLOATING) && bar->m_pDockContext != nullptr)
								bar->m_pDockContext->m_rectMRUDockPos = rect;
							AfxRepositionWindow(&layout, bar->m_hWnd, &rect);
						}
						pt.x = rect.left + sizeBar.cx - CxBorder2;
						width = std::max<int>(width, sizeBar.cy);
					}
				} else
				{
					if(barRect.top > rect.top && !m_bFloating)
						rect.OffsetRect(0, barRect.top - rect.top);
					if(rect.bottom > sizeMax.cy && !m_bFloating)
					{
						const int y = std::max<int>(sizeMax.cy - (rect.Height() - CyBorder2), pt.y);
						rect.OffsetRect(0, y - rect.top);
					}
					if(wrapped)
					{
						wrapped = false;
						rect.OffsetRect(0, -(rect.top + CyBorder2));
					} else if(rect.top >= sizeMax.cy - CyBorder2 && pos > 0 && m_arrBars[static_cast<std::size_t>(pos - 1)] != nullptr)
					{
						m_arrBars.insert(m_arrBars.begin() + pos, nullptr);
						bar = nullptr;
						entry = nullptr;
						wrapped = true;
					}
					if(!wrapped)
					{
						if(rect != barRect)
						{
							if(!m_bLayoutQuery && !(bar->m_dwStyle & CBRS_FLOATING) && bar->m_pDockContext != nullptr)
								bar->m_pDockContext->m_rectMRUDockPos = rect;
							AfxRepositionWindow(&layout, bar->m_hWnd, &rect);
						}
						pt.y = rect.top + sizeBar.cy - CyBorder2;
						width = std::max<int>(width, sizeBar.cx);
					}
				}
			}
			if(!wrapped && bar != nullptr)
				bar->RecalcDelayShow(&layout);
		}
		if(bar == nullptr && entry == nullptr && width != 0)
		{
			// The end of a row.
			if(horz)
			{
				pt.y += width - CyBorder2;
				sizeFixed.cx = std::max<LONG>(sizeFixed.cx, pt.x);
				sizeFixed.cy = std::max<LONG>(sizeFixed.cy, pt.y);
				pt.x = -CxBorder2;
			} else
			{
				pt.x += width - CxBorder2;
				sizeFixed.cx = std::max<LONG>(sizeFixed.cx, pt.x);
				sizeFixed.cy = std::max<LONG>(sizeFixed.cy, pt.y);
				pt.y = -CyBorder2;
			}
			width = 0;
		}
	}
	if(!m_bLayoutQuery && layout.hDWP != nullptr)
		::EndDeferWindowPos(layout.hDWP);

	// The dock bar's own borders.
	CRect rect(0, 0, 0, 0);
	CalcInsideRect(rect, horz);
	if((!stretch || !horz) && sizeFixed.cx != 0)
		sizeFixed.cx += -rect.right + rect.left;
	if((!stretch || horz) && sizeFixed.cy != 0)
		sizeFixed.cy += -rect.bottom + rect.top;
	return sizeFixed;
}

LRESULT CDockBar::OnSizeParent(WPARAM wp, LPARAM lp)
{
	auto *layout = reinterpret_cast<AFX_SIZEPARENTPARAMS *>(lp);
	const BOOL query = m_bLayoutQuery;
	const CRect rectLayout = m_rectLayout;
	m_bLayoutQuery = layout->hDWP == nullptr;
	m_rectLayout = layout->rect;
	const LRESULT result = CControlBar::OnSizeParent(wp, lp);
	m_bLayoutQuery = query;
	m_rectLayout = rectLayout;
	return result;
}

void CDockBar::OnNcCalcSize(BOOL, NCCALCSIZE_PARAMS *params)
{
	CRect rect(0, 0, 0, 0);
	CalcInsideRect(rect, (m_dwStyle & CBRS_ORIENT_HORZ) != 0);
	params->rgrc[0].left += rect.left;
	params->rgrc[0].top += rect.top;
	params->rgrc[0].right += rect.right;
	params->rgrc[0].bottom += rect.bottom;
}

void CDockBar::OnNcPaint()
{
	EraseNonClient();
}

void CDockBar::DoPaint(CDC *)
{
	// The borders are in the non-client area.
}

void CDockBar::GetBarInfo(CControlBarInfo *info)
{
	info->m_nBarID = static_cast<UINT>(::GetDlgCtrlID(m_hWnd));
	info->m_pBar = this;
	info->m_bVisible = IsVisible();
	info->m_nMRUWidth = m_nMRUWidth;
	info->m_bDockBar = TRUE;
	if(m_bFloating)
	{
		CRect rect;
		GetWindowRect(&rect);
		info->m_pointPos = rect.TopLeft();
		info->m_bFloating = TRUE;
		info->m_bHorz = (m_dwStyle & CBRS_ORIENT_HORZ) != 0;
	}
	// The rows: bar ids, 0 between rows, place holders with 0x10000 added.
	for(CControlBar *entry : m_arrBars)
	{
		if(entry == nullptr)
			info->m_arrBarID.push_back(0);
		else if(IsPlaceHolder(entry))
			info->m_arrBarID.push_back(MAKELONG(static_cast<WORD>(reinterpret_cast<UINT_PTR>(entry)), 1));
		else
			info->m_arrBarID.push_back(static_cast<UINT>(::GetDlgCtrlID(entry->m_hWnd)));
	}
}

void CDockBar::SetBarInfo(CControlBarInfo *info, CFrameWnd *frame)
{
	std::size_t size = info->m_arrBarID.size();
	// No trailing row ends.
	while(size != 0 && (info->m_arrBarID[size - 1] == 0 || info->m_arrBarID[size - 1] == static_cast<UINT>(MAKELONG(0, 1))))
		size--;
	// From 1: the leading row end is there already.
	for(std::size_t i = 1; i < size; i++)
	{
		const UINT value = info->m_arrBarID[i];
		const UINT id = LOWORD(value);
		const bool placeHolder = HIWORD(value) != 0;
		CControlBar *entry = nullptr;
		if(id != 0)
		{
			CControlBar *bar = placeHolder ? nullptr : frame->GetControlBar(id);
			if(bar != nullptr && bar != this)
			{
				if(bar->GetParent() != this)
					bar->SetParent(this);
				if(bar->m_pDockBar != nullptr)
					bar->m_pDockBar->RemoveControlBar(bar, -1, -1);
				bar->m_pDockBar = this;
				DWORD style = bar->GetBarStyle();
				style &= ~CBRS_ALIGN_ANY;
				style |= (m_dwStyle & CBRS_ALIGN_ANY) | CBRS_BORDER_ANY;
				if(m_bFloating)
					style |= CBRS_FLOATING;
				else
					style &= ~CBRS_FLOATING;
				bar->SetBarStyle(style);
				entry = bar;
				if(m_bFloating && !(m_dwStyle & CBRS_FLOAT_MULTI))
				{
					CString title;
					bar->GetWindowText(title);
					AfxSetWindowText(m_hWnd, title);
				}
			} else
			{
				// A bar that is not there (any more): where it would go.
				entry = PlaceHolder(id);
			}
		}
		const std::size_t at = std::min(i, m_arrBars.size());
		m_arrBars.insert(m_arrBars.begin() + static_cast<std::ptrdiff_t>(at), entry);
	}
}

// ---- CDockContext --------------------------------------------------------------------

CDockContext::CDockContext(CControlBar *bar)
	: m_pBar(bar)
	, m_pDockSite(bar->m_pDockSite)
{
	if(bar->m_dwStyle & CBRS_SIZE_DYNAMIC)
		m_dwMRUFloatStyle = bar->m_dwStyle & (CBRS_ALIGN_TOP | CBRS_SIZE_DYNAMIC);
	else if(bar->m_dwStyle & CBRS_ORIENT_HORZ)
		m_dwMRUFloatStyle = CBRS_ALIGN_TOP | (bar->m_dwStyle & CBRS_FLOAT_MULTI);
	else
		m_dwMRUFloatStyle = CBRS_ALIGN_LEFT | (bar->m_dwStyle & CBRS_FLOAT_MULTI);
	m_ptMRUFloatPos = CPoint(CW_USEDEFAULT, 0);
}

CDockContext::~CDockContext() = default;

void CDockContext::InitLoop()
{
	// Paint what is waiting first.
	::UpdateWindow(m_pDockSite->m_hWnd);
	m_dwDockStyle = m_pBar->m_dwDockStyle;
	m_dwStyle = m_pBar->m_dwStyle & CBRS_ALIGN_ANY;
	m_rectLast.SetRectEmpty();
	m_sizeLast = CSize(0, 0);
	m_bForceFrame = m_bFlip = m_bDitherLast = FALSE;
}

void CDockContext::CancelLoop()
{
	DrawFocusRect(TRUE);
	::ReleaseCapture();
}

void CDockContext::StartDrag(CPoint pt)
{
	m_bDragging = TRUE;
	InitLoop();
	CRect rect;
	m_pBar->GetWindowRect(rect);
	m_ptLast = pt;
	if(m_pBar->m_dwStyle & CBRS_SIZE_DYNAMIC)
	{
		const CSize sizeHorz = m_pBar->CalcDynamicLayout(0, LM_HORZ | LM_HORZDOCK);
		const CSize sizeVert = m_pBar->CalcDynamicLayout(0, LM_VERTDOCK);
		const CSize sizeFloat = m_pBar->CalcDynamicLayout(0, LM_HORZ | LM_MRUWIDTH);
		m_rectDragHorz = CRect(rect.TopLeft(), sizeHorz);
		m_rectDragVert = CRect(rect.TopLeft(), sizeVert);
		m_rectFrameDragHorz = CRect(rect.TopLeft(), sizeFloat);
		m_rectFrameDragVert = CRect(rect.TopLeft(), sizeFloat);
	} else if(m_pBar->m_dwStyle & CBRS_SIZE_FIXED)
	{
		const CSize sizeHorz = m_pBar->CalcDynamicLayout(-1, LM_HORZ | LM_HORZDOCK);
		const CSize sizeVert = m_pBar->CalcDynamicLayout(-1, LM_VERTDOCK);
		m_rectFrameDragHorz = m_rectDragHorz = CRect(rect.TopLeft(), sizeHorz);
		m_rectFrameDragVert = m_rectDragVert = CRect(rect.TopLeft(), sizeVert);
	} else
	{
		// The other orientation, turned around the mouse.
		const BOOL horz = Horz(m_dwStyle) != 0;
		const CSize size = m_pBar->CalcDynamicLayout(-1, !horz ? (LM_HORZ | LM_HORZDOCK) : LM_VERTDOCK);
		if(horz)
		{
			m_rectDragHorz = rect;
			m_rectDragVert = CRect(CPoint(pt.x - rect.Height() / 2, rect.top), size);
		} else
		{
			m_rectDragVert = rect;
			m_rectDragHorz = CRect(CPoint(rect.left, pt.y - rect.Width() / 2), size);
		}
		m_rectFrameDragHorz = m_rectDragHorz;
		m_rectFrameDragVert = m_rectDragVert;
	}
	CMiniFrameWnd::CalcBorders(&m_rectFrameDragHorz);
	CMiniFrameWnd::CalcBorders(&m_rectFrameDragVert);
	m_rectFrameDragHorz.InflateRect(-CxBorder2, -CyBorder2);
	m_rectFrameDragVert.InflateRect(-CxBorder2, -CyBorder2);

	AdjustRectangle(m_rectDragHorz, pt);
	AdjustRectangle(m_rectDragVert, pt);
	AdjustRectangle(m_rectFrameDragHorz, pt);
	AdjustRectangle(m_rectFrameDragVert, pt);

	m_dwOverDockStyle = CanDock();
	Move(pt);
	Track();
}

void CDockContext::Move(CPoint pt)
{
	const CPoint offset = pt - m_ptLast;
	m_rectDragHorz.OffsetRect(offset);
	m_rectFrameDragHorz.OffsetRect(offset);
	m_rectDragVert.OffsetRect(offset);
	m_rectFrameDragVert.OffsetRect(offset);
	m_ptLast = pt;
	// Ctrl keeps it from docking.
	m_dwOverDockStyle = m_bForceFrame ? 0 : CanDock();
	DrawFocusRect();
}

void CDockContext::OnKey(int key, BOOL down)
{
	if(key == VK_CONTROL)
		UpdateState(&m_bForceFrame, down);
	if(key == VK_SHIFT)
		UpdateState(&m_bFlip, down);
}

void CDockContext::UpdateState(BOOL *flag, BOOL value)
{
	if(*flag == value)
		return;
	*flag = value;
	m_bFlip = Horz(m_dwDockStyle) && Vert(m_dwDockStyle) && m_bFlip; // Shift
	m_dwOverDockStyle = m_bForceFrame ? 0 : CanDock();
	DrawFocusRect();
}

DWORD CDockContext::CanDock()
{
	// The current orientation first; then, unless Shift forces one, the
	// other if it is close enough.
	DWORD dock = 0;
	BOOL styleHorz = Horz(m_dwStyle) != 0;
	styleHorz = m_bFlip ? !styleHorz : styleHorz;
	if(styleHorz && Horz(m_dwDockStyle))
		dock = m_pDockSite->CanDock(m_rectDragHorz, m_dwDockStyle & ~CBRS_ORIENT_VERT);
	else if(Vert(m_dwDockStyle))
		dock = m_pDockSite->CanDock(m_rectDragVert, m_dwDockStyle & ~CBRS_ORIENT_HORZ);
	if(!m_bFlip)
	{
		if(dock == 0 && Horz(m_dwDockStyle))
		{
			const DWORD cur = m_pDockSite->CanDock(m_rectDragVert, m_dwDockStyle & ~CBRS_ORIENT_VERT);
			dock = m_pDockSite->CanDock(m_rectDragHorz, m_dwDockStyle & ~CBRS_ORIENT_VERT);
			dock = dock == cur ? dock : 0;
		}
		if(dock == 0 && Vert(m_dwDockStyle))
		{
			const DWORD cur = m_pDockSite->CanDock(m_rectDragHorz, m_dwDockStyle & ~CBRS_ORIENT_HORZ);
			dock = m_pDockSite->CanDock(m_rectDragVert, m_dwDockStyle & ~CBRS_ORIENT_HORZ);
			dock = dock == cur ? dock : 0;
		}
	}
	return dock;
}

CDockBar *CDockContext::GetDockBar(DWORD overDockStyle)
{
	CDockBar *bar = nullptr;
	if(Horz(overDockStyle))
		m_pDockSite->CanDock(m_rectDragHorz, Horz(overDockStyle), &bar);
	else if(Vert(overDockStyle))
		m_pDockSite->CanDock(m_rectDragVert, Vert(overDockStyle), &bar);
	return bar;
}

void CDockContext::DrawFocusRect(BOOL removeRect)
{
	// A thin frame where it would dock, a thick dithered one where it would
	// float.
	CSize size(CxBorder, CyBorder);
	CRect rect;
	BOOL dither = FALSE;
	if(!m_bDragging)
	{
		size = CSize(::GetSystemMetrics(SM_CXFRAME) - CxBorder, ::GetSystemMetrics(SM_CYFRAME) - CyBorder);
		rect = m_rectActualFrameSize;
		dither = TRUE;
	} else if(Horz(m_dwOverDockStyle))
	{
		rect = m_rectDragHorz;
	} else if(Vert(m_dwOverDockStyle))
	{
		rect = m_rectDragVert;
	} else
	{
		size = CSize(::GetSystemMetrics(SM_CXFRAME) - CxBorder, ::GetSystemMetrics(SM_CYFRAME) - CyBorder);
		if((Horz(m_dwStyle) && !m_bFlip) || (Vert(m_dwStyle) && m_bFlip))
			rect = m_rectFrameDragHorz;
		else
			rect = m_rectFrameDragVert;
		dither = TRUE;
	}
	if(m_bDragging && (Horz(m_dwOverDockStyle) || Vert(m_dwOverDockStyle)))
		rect.InflateRect(-CxBorder, -CyBorder); // looks pushed in
	if(removeRect)
		LinOpenMPTShowDragFrame(nullptr, SIZE{0, 0}, FALSE);
	else
		LinOpenMPTShowDragFrame(&rect, size, dither);
	m_rectLast = rect;
	m_sizeLast = removeRect ? CSize(0, 0) : size;
	m_bDitherLast = dither;
}

BOOL CDockContext::Track()
{
	if(::GetCapture() != nullptr)
		return FALSE;
	m_pBar->SetCapture();
	while(::GetCapture() == m_pBar->m_hWnd)
	{
		MSG msg;
		if(!::GetMessage(&msg, nullptr, 0, 0))
		{
			::PostQuitMessage(static_cast<int>(msg.wParam));
			break;
		}
		// Where the mouse is, from the message (relative to the bar).
		CPoint pt(GET_X_LPARAM(msg.lParam), GET_Y_LPARAM(msg.lParam));
		if(msg.hwnd != nullptr)
			::ClientToScreen(msg.hwnd, &pt);
		switch(msg.message)
		{
		case WM_LBUTTONUP:
			if(m_bDragging)
				EndDrag();
			else
				EndResize();
			return TRUE;
		case WM_MOUSEMOVE:
			if(m_bDragging)
				Move(pt);
			else
				Stretch(pt);
			break;
		case WM_KEYUP:
			if(m_bDragging)
				OnKey(static_cast<int>(msg.wParam), FALSE);
			break;
		case WM_KEYDOWN:
			if(m_bDragging)
				OnKey(static_cast<int>(msg.wParam), TRUE);
			if(msg.wParam == VK_ESCAPE)
			{
				CancelLoop();
				return FALSE;
			}
			break;
		case WM_RBUTTONDOWN:
			CancelLoop();
			return FALSE;
		default:
			::DispatchMessage(&msg);
			break;
		}
	}
	CancelLoop();
	return FALSE;
}

void CDockContext::EndDrag()
{
	CancelLoop();
	if(m_dwOverDockStyle != 0)
	{
		CDockBar *dockBar = GetDockBar(m_dwOverDockStyle);
		if(dockBar == nullptr)
			return;
		CRect rect = (m_dwOverDockStyle & CBRS_ORIENT_VERT) ? m_rectDragVert : m_rectDragHorz;
		const UINT id = static_cast<UINT>(::GetDlgCtrlID(dockBar->m_hWnd));
		if(id >= AFX_IDW_DOCKBAR_TOP && id <= AFX_IDW_DOCKBAR_BOTTOM)
		{
			m_uMRUDockID = id;
			m_rectMRUDockPos = rect;
			dockBar->ScreenToClient(&m_rectMRUDockPos);
		}
		// Docked where it was dropped; the layout snaps it into place.
		m_pDockSite->DockControlBar(m_pBar, dockBar, &rect);
		m_pDockSite->RecalcLayout();
	} else if((m_dwStyle & CBRS_SIZE_DYNAMIC) || (Horz(m_dwStyle) && !m_bFlip) || (Vert(m_dwStyle) && m_bFlip))
	{
		m_dwMRUFloatStyle = CBRS_ALIGN_TOP | (m_dwDockStyle & CBRS_FLOAT_MULTI);
		m_ptMRUFloatPos = m_rectFrameDragHorz.TopLeft();
		m_pDockSite->FloatControlBar(m_pBar, m_ptMRUFloatPos, m_dwMRUFloatStyle);
	} else
	{
		m_dwMRUFloatStyle = CBRS_ALIGN_LEFT | (m_dwDockStyle & CBRS_FLOAT_MULTI);
		m_ptMRUFloatPos = m_rectFrameDragVert.TopLeft();
		m_pDockSite->FloatControlBar(m_pBar, m_ptMRUFloatPos, m_dwMRUFloatStyle);
	}
}

void CDockContext::StartResize(int hitTest, CPoint pt)
{
	m_bDragging = FALSE;
	InitLoop();
	CRect rect;
	m_pBar->GetWindowRect(rect);
	m_ptLast = pt;
	m_nHitTest = hitTest;
	const CSize size = m_pBar->CalcDynamicLayout(0, LM_HORZ | LM_MRUWIDTH);
	m_rectRequestedSize = CRect(rect.TopLeft(), size);
	m_rectActualSize = CRect(rect.TopLeft(), size);
	m_rectActualFrameSize = CRect(rect.TopLeft(), size);
	CMiniFrameWnd::CalcBorders(&m_rectActualFrameSize);
	m_rectActualFrameSize.InflateRect(-CxBorder2, -CyBorder2);
	m_rectFrameBorders = CRect(CPoint(0, 0), m_rectActualFrameSize.Size() - m_rectActualSize.Size());
	m_dwOverDockStyle = 0;
	Stretch(pt);
	Track();
}

void CDockContext::Stretch(CPoint pt)
{
	const CPoint offset = pt - m_ptLast;
	int length = 0;
	DWORD mode = LM_HORZ;
	if(m_nHitTest == HTLEFT || m_nHitTest == HTRIGHT)
	{
		if(m_nHitTest == HTLEFT)
			m_rectRequestedSize.left += offset.x;
		else
			m_rectRequestedSize.right += offset.x;
		length = m_rectRequestedSize.Width();
	} else
	{
		mode |= LM_LENGTHY;
		if(m_nHitTest == HTTOP || m_nHitTest == HTTOPLEFT || m_nHitTest == HTTOPRIGHT)
			m_rectRequestedSize.top += offset.y;
		else
			m_rectRequestedSize.bottom += offset.y;
		length = m_rectRequestedSize.Height();
	}
	length = std::max(length, 0);
	const CSize size = m_pBar->CalcDynamicLayout(length, mode);

	CRect desk;
	::GetWindowRect(::GetDesktopWindow(), &desk);
	CRect temp = m_rectActualFrameSize;
	if(m_nHitTest == HTLEFT || m_nHitTest == HTTOP || m_nHitTest == HTTOPLEFT)
	{
		temp.left = temp.right - (size.cx + m_rectFrameBorders.Width());
		temp.top = temp.bottom - (size.cy + m_rectFrameBorders.Height());
		CRect test;
		if(test.IntersectRect(desk, temp))
		{
			m_rectActualSize.left = m_rectActualSize.right - size.cx;
			m_rectActualSize.top = m_rectActualSize.bottom - size.cy;
			m_rectActualFrameSize.left = temp.left;
			m_rectActualFrameSize.top = temp.top;
		}
	} else
	{
		temp.right = temp.left + (size.cx + m_rectFrameBorders.Width());
		temp.bottom = temp.top + (size.cy + m_rectFrameBorders.Height());
		CRect test;
		if(test.IntersectRect(desk, temp))
		{
			m_rectActualSize.right = m_rectActualSize.left + size.cx;
			m_rectActualSize.bottom = m_rectActualSize.top + size.cy;
			m_rectActualFrameSize.right = temp.right;
			m_rectActualFrameSize.bottom = temp.bottom;
		}
	}
	m_ptLast = pt;
	DrawFocusRect();
}

void CDockContext::EndResize()
{
	CancelLoop();
	m_pBar->CalcDynamicLayout(m_rectActualSize.Width(), LM_HORZ | LM_COMMIT);
	m_pDockSite->FloatControlBar(m_pBar, m_rectActualFrameSize.TopLeft(), CBRS_ALIGN_TOP | (m_dwDockStyle & CBRS_FLOAT_MULTI) | CBRS_SIZE_DYNAMIC);
}

void CDockContext::ToggleDocking()
{
	if(m_pBar->IsFloating())
	{
		// Back where it was docked last, if it may dock at all.
		if(m_pBar->m_dwDockStyle & CBRS_ALIGN_ANY)
		{
			CRect rect = m_rectMRUDockPos;
			CDockBar *dockBar = nullptr;
			if(m_uMRUDockID != 0)
			{
				dockBar = static_cast<CDockBar *>(m_pDockSite->GetControlBar(m_uMRUDockID));
				if(dockBar != nullptr)
					dockBar->ClientToScreen(&rect);
			}
			m_pDockSite->ReDockControlBar(m_pBar, dockBar, &rect);
			m_pDockSite->RecalcLayout();
		}
	} else
	{
		CPoint ptFloat = m_ptMRUFloatPos;
		if(ptFloat.x < 0 || ptFloat.y < 0)
		{
			ptFloat = m_rectMRUDockPos.TopLeft();
			m_pBar->GetParent()->ClientToScreen(&ptFloat);
		}
		m_pDockSite->FloatControlBar(m_pBar, ptFloat, m_dwMRUFloatStyle);
	}
}

// ---- CMiniFrameWnd -------------------------------------------------------------------

BEGIN_MESSAGE_MAP(CMiniFrameWnd, CFrameWnd)
	ON_WM_NCCALCSIZE()
	ON_WM_NCPAINT()
	ON_WM_NCHITTEST()
	ON_WM_NCLBUTTONDOWN()
	ON_WM_NCACTIVATE()
	ON_MESSAGE(WM_SETTEXT, &CMiniFrameWnd::OnSetText)
	ON_MESSAGE(WM_FLOATSTATUS, &CMiniFrameWnd::OnFloatStatus)
END_MESSAGE_MAP()

BOOL CMiniFrameWnd::Create(LPCTSTR className, LPCTSTR windowName, DWORD style, const RECT &rect, CWnd *parent, UINT id)
{
	return CreateEx(0, className, windowName, style, rect, parent, id);
}

BOOL CMiniFrameWnd::CreateEx(DWORD exStyle, LPCTSTR className, LPCTSTR windowName, DWORD style, const RECT &rect, CWnd *parent, UINT id)
{
	m_strTitle = windowName != nullptr ? windowName : _T("");
	if(className == nullptr)
		className = AfxRegisterWndClass(CS_DBLCLKS, ::LoadCursor(nullptr, IDC_ARROW));
	return CWnd::CreateEx(exStyle, className, windowName, style, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
		parent != nullptr ? parent->m_hWnd : nullptr, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(id)));
}

BOOL CMiniFrameWnd::PreCreateWindow(CREATESTRUCT &cs)
{
	// MFS_4THICKFRAME and MFS_THICKFRAME are a thick frame; a caption makes
	// a tool window.
	if(cs.style & (MFS_4THICKFRAME | MFS_THICKFRAME))
		cs.style |= WS_THICKFRAME;
	if(cs.style & WS_CAPTION)
		cs.dwExStyle |= WS_EX_TOOLWINDOW;
	CFrameWnd::PreCreateWindow(cs);
	cs.dwExStyle &= ~WS_EX_CLIENTEDGE;
	// The caption and frame are drawn here: the window itself has neither.
	m_dwFrameStyle = static_cast<DWORD>(cs.style);
	cs.style &= ~(WS_CAPTION | WS_THICKFRAME | WS_SYSMENU);
	return TRUE;
}

void CMiniFrameWnd::CalcBorders(LPRECT rect, DWORD style, DWORD exStyle)
{
	// A tool window's frame and small caption, as a child window's would be.
	::AdjustWindowRectEx(rect, (style & ~WS_POPUP) | WS_CHILD, FALSE, exStyle | WS_EX_TOOLWINDOW);
}

void CMiniFrameWnd::CalcWindowRect(LPRECT clientRect, UINT)
{
	CalcBorders(clientRect, m_dwFrameStyle & (WS_CAPTION | WS_THICKFRAME | WS_BORDER | WS_DLGFRAME), 0);
}

void CMiniFrameWnd::OnNcCalcSize(BOOL, NCCALCSIZE_PARAMS *params)
{
	CRect frame(0, 0, 0, 0);
	CalcWindowRect(frame);
	params->rgrc[0].left -= frame.left;
	params->rgrc[0].top -= frame.top;
	params->rgrc[0].right -= frame.right;
	params->rgrc[0].bottom -= frame.bottom;
}

CRect CMiniFrameWnd::GetCloseRect() const
{
	CRect frame(0, 0, 0, 0);
	const_cast<CMiniFrameWnd *>(this)->CalcWindowRect(frame);
	CRect window;
	GetWindowRect(window);
	const int border = -frame.left;
	const int caption = -frame.top - border;
	const int size = std::max(caption - 4, 4);
	const int right = window.Width() - border - 2;
	const int top = border + (caption - size) / 2;
	return CRect(right - size, top, right, top + size);
}

void CMiniFrameWnd::DrawFrame()
{
	CRect frame(0, 0, 0, 0);
	CalcWindowRect(frame);
	CWindowDC dc(this);
	CRect window;
	GetWindowRect(window);
	window.OffsetRect(-window.left, -window.top);
	const int border = -frame.left;
	// The frame.
	CRect rect = window;
	::DrawEdge(dc.m_hDC, rect, EDGE_RAISED, BF_RECT | BF_ADJUST);
	for(int i = 2; i < border; i++)
	{
		dc.Draw3dRect(rect, ::GetSysColor(COLOR_BTNFACE), ::GetSysColor(COLOR_BTNFACE));
		rect.DeflateRect(1, 1);
	}
	// The caption: title and close box.
	if(m_dwFrameStyle & WS_CAPTION)
	{
		const int caption = -frame.top - border;
		CRect cap(border, border, window.Width() - border, border + caption);
		const bool active = m_bActive != FALSE;
		dc.FillSolidRect(cap, ::GetSysColor(active ? COLOR_ACTIVECAPTION : COLOR_INACTIVECAPTION));
		CString title;
		GetWindowText(title);
		const HFONT font = static_cast<HFONT>(::GetStockObject(DEFAULT_GUI_FONT));
		HGDIOBJ old = ::SelectObject(dc.m_hDC, font);
		dc.SetBkMode(TRANSPARENT);
		dc.SetTextColor(::GetSysColor(active ? COLOR_CAPTIONTEXT : COLOR_INACTIVECAPTIONTEXT));
		CRect closeRect = GetCloseRect();
		CRect text = cap;
		text.left += 3;
		text.right = closeRect.left - 2;
		::DrawText(dc.m_hDC, title, title.GetLength(), text, DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_END_ELLIPSIS | DT_NOPREFIX);
		::SelectObject(dc.m_hDC, old);
		if(m_dwFrameStyle & WS_SYSMENU)
			::DrawFrameControl(dc.m_hDC, closeRect, DFC_CAPTION, DFCS_CAPTIONCLOSE | (m_bClosePressed ? DFCS_PUSHED : 0));
	}
}

void CMiniFrameWnd::OnNcPaint()
{
	DrawFrame();
}

LRESULT CMiniFrameWnd::OnNcHitTest(CPoint point)
{
	CRect window;
	GetWindowRect(window);
	if(!window.PtInRect(point))
		return HTNOWHERE;
	CRect frame(0, 0, 0, 0);
	CalcWindowRect(frame);
	const int border = -frame.left;
	const CPoint pt = point - window.TopLeft();
	const int w = window.Width(), h = window.Height();
	const bool left = pt.x < border, right = pt.x >= w - border, top = pt.y < border, bottom = pt.y >= h - border;
	if(left || right || top || bottom)
	{
		// A frame that does not size moves the window.
		if(!(m_dwFrameStyle & WS_THICKFRAME) || (m_dwFrameStyle & MFS_MOVEFRAME))
			return HTCAPTION;
		// The corners reach a caption's height along each side.
		const int corner = std::max(border, -frame.top);
		const bool nearLeft = pt.x < corner, nearRight = pt.x >= w - corner, nearTop = pt.y < corner, nearBottom = pt.y >= h - corner;
		if((top && nearLeft) || (left && nearTop))
			return HTTOPLEFT;
		if((top && nearRight) || (right && nearTop))
			return HTTOPRIGHT;
		if((bottom && nearLeft) || (left && nearBottom))
			return HTBOTTOMLEFT;
		if((bottom && nearRight) || (right && nearBottom))
			return HTBOTTOMRIGHT;
		return left ? HTLEFT : right ? HTRIGHT : top ? HTTOP : HTBOTTOM;
	}
	if(pt.y < -frame.top)
	{
		if((m_dwFrameStyle & WS_SYSMENU) && GetCloseRect().PtInRect(pt))
			return HTCLOSE;
		return HTCAPTION;
	}
	return HTCLIENT;
}

void CMiniFrameWnd::OnNcLButtonDown(UINT hitTest, CPoint point)
{
	if(hitTest != HTCLOSE)
	{
		CFrameWnd::OnNcLButtonDown(hitTest, point);
		return;
	}
	// The close box: pushed while the mouse is on it, closes on release.
	CRect window;
	GetWindowRect(window);
	const CRect closeRect = GetCloseRect();
	m_bClosePressed = TRUE;
	DrawFrame();
	SetCapture();
	bool close = false;
	while(::GetCapture() == m_hWnd)
	{
		MSG msg;
		if(!::GetMessage(&msg, nullptr, 0, 0))
		{
			::PostQuitMessage(static_cast<int>(msg.wParam));
			break;
		}
		if(msg.message == WM_MOUSEMOVE || msg.message == WM_LBUTTONUP)
		{
			CPoint pt(GET_X_LPARAM(msg.lParam), GET_Y_LPARAM(msg.lParam));
			::ClientToScreen(msg.hwnd, &pt);
			const BOOL over = closeRect.PtInRect(CPoint(pt.x - window.left, pt.y - window.top));
			if(msg.message == WM_LBUTTONUP)
			{
				close = over != FALSE;
				break;
			}
			if(over != m_bClosePressed)
			{
				m_bClosePressed = over;
				DrawFrame();
			}
		} else if(msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE)
		{
			break;
		} else
		{
			::DispatchMessage(&msg);
		}
	}
	::ReleaseCapture();
	m_bClosePressed = FALSE;
	DrawFrame();
	if(close)
		SendMessage(WM_SYSCOMMAND, SC_CLOSE, MAKELPARAM(point.x, point.y));
}

BOOL CMiniFrameWnd::OnNcActivate(BOOL active)
{
	// MFS_SYNCACTIVE: active while the frame it belongs to is
	// (WM_FLOATSTATUS).
	if((GetStyle() & MFS_SYNCACTIVE) == 0)
	{
		if(m_bActive != active)
		{
			m_bActive = active;
			DrawFrame();
		}
	} else if(m_nFlags & 0x0200) // WF_KEEPMINIACTIVE
	{
		return FALSE;
	}
	return TRUE;
}

LRESULT CMiniFrameWnd::OnSetText(WPARAM, LPARAM)
{
	const LRESULT result = Default();
	if(m_hWnd != nullptr)
		DrawFrame();
	return result;
}

LRESULT CMiniFrameWnd::OnFloatStatus(WPARAM wp, LPARAM)
{
	LRESULT result = 0;
	if((GetStyle() & MFS_SYNCACTIVE) && (wp & FS_SYNCACTIVE))
		result = 1;
	if(wp & (FS_SHOW | FS_HIDE))
		SetWindowPos(nullptr, 0, 0, 0, 0, ((wp & FS_SHOW) ? SWP_SHOWWINDOW : SWP_HIDEWINDOW) | SWP_NOZORDER | SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
	if(wp & (FS_ENABLE | FS_DISABLE))
		EnableWindow((wp & FS_ENABLE) != 0);
	if((wp & (FS_ACTIVATE | FS_DEACTIVATE)) && (GetStyle() & MFS_SYNCACTIVE))
	{
		const BOOL active = (wp & FS_ACTIVATE) != 0;
		if(m_bActive != active)
		{
			m_bActive = active;
			DrawFrame();
		}
	}
	return result;
}

// ---- CMiniDockFrameWnd ---------------------------------------------------------------

BEGIN_MESSAGE_MAP(CMiniDockFrameWnd, CMiniFrameWnd)
	ON_WM_CLOSE()
	ON_WM_NCLBUTTONDOWN()
	ON_WM_NCLBUTTONDBLCLK()
	ON_WM_MOUSEACTIVATE()
END_MESSAGE_MAP()

CMiniDockFrameWnd::CMiniDockFrameWnd()
	: m_wndDockBar(TRUE)
{
	m_wndDockBar.m_bAutoDelete = FALSE;
}

BOOL CMiniDockFrameWnd::Create(CWnd *parent, DWORD barStyle)
{
	// No layout until something is docked.
	m_bInRecalcLayout = TRUE;
	DWORD style = WS_POPUP | WS_CAPTION | WS_SYSMENU | MFS_MOVEFRAME | MFS_4THICKFRAME | MFS_SYNCACTIVE | MFS_BLOCKSYSMENU | FWS_SNAPTOBARS;
	if(barStyle & CBRS_SIZE_DYNAMIC)
		style &= ~MFS_MOVEFRAME;
	if(!CMiniFrameWnd::CreateEx(0, nullptr, _T(""), style, rectDefault, parent))
	{
		m_bInRecalcLayout = FALSE;
		return FALSE;
	}
	style = (barStyle & (CBRS_ALIGN_LEFT | CBRS_ALIGN_RIGHT)) ? CBRS_ALIGN_LEFT : CBRS_ALIGN_TOP;
	style |= barStyle & CBRS_FLOAT_MULTI;
	// Created in the frame (so that the frame lists it), then moved here.
	if(!m_wndDockBar.Create(parent, WS_CHILD | WS_VISIBLE | style, AFX_IDW_DOCKBAR_FLOAT))
	{
		m_bInRecalcLayout = FALSE;
		return FALSE;
	}
	m_wndDockBar.SetParent(this);
	m_bInRecalcLayout = FALSE;
	return TRUE;
}

void CMiniDockFrameWnd::RecalcLayout(BOOL notify)
{
	if(m_bInRecalcLayout)
		return;
	CMiniFrameWnd::RecalcLayout(notify);
	// The caption is the dock bar's title (the bar's).
	CString title;
	m_wndDockBar.GetWindowText(title);
	AfxSetWindowText(m_hWnd, title);
}

CControlBar *CMiniDockFrameWnd::FirstBar() const
{
	for(int pos = 1; pos < static_cast<int>(m_wndDockBar.m_arrBars.size()); pos++)
	{
		if(CControlBar *bar = m_wndDockBar.GetDockedControlBar(pos))
			return bar;
	}
	return nullptr;
}

void CMiniDockFrameWnd::OnClose()
{
	m_wndDockBar.ShowAll(FALSE);
}

void CMiniDockFrameWnd::OnNcLButtonDown(UINT hitTest, CPoint point)
{
	if(hitTest == HTCAPTION)
	{
		ActivateTopParent();
		// The caption drags the bar (unless several share the frame).
		if((m_wndDockBar.m_dwStyle & CBRS_FLOAT_MULTI) == 0)
		{
			CControlBar *bar = FirstBar();
			if(bar != nullptr && bar->m_pDockContext != nullptr)
			{
				bar->m_pDockContext->StartDrag(point);
				return;
			}
		}
	} else if(hitTest >= HTSIZEFIRST && hitTest <= HTSIZELAST)
	{
		ActivateTopParent();
		CControlBar *bar = FirstBar();
		if(bar != nullptr && bar->m_pDockContext != nullptr)
		{
			bar->m_pDockContext->StartResize(static_cast<int>(hitTest), point);
			return;
		}
	}
	CMiniFrameWnd::OnNcLButtonDown(hitTest, point);
}

void CMiniDockFrameWnd::OnNcLButtonDblClk(UINT hitTest, CPoint point)
{
	if(hitTest == HTCAPTION)
	{
		ActivateTopParent();
		if((m_wndDockBar.m_dwStyle & CBRS_FLOAT_MULTI) == 0)
		{
			CControlBar *bar = FirstBar();
			if(bar != nullptr && bar->m_pDockContext != nullptr)
			{
				bar->m_pDockContext->ToggleDocking();
				return;
			}
		}
	}
	CMiniFrameWnd::OnNcLButtonDblClk(hitTest, point);
}

int CMiniDockFrameWnd::OnMouseActivate(CWnd *topParent, UINT hitTest, UINT message)
{
	// The frame the bar belongs to comes forward too.
	ActivateTopParent();
	return CMiniFrameWnd::OnMouseActivate(topParent, hitTest, message);
}

// ---- CFrameWnd: docking --------------------------------------------------------------

void CFrameWnd::EnableDocking(DWORD dockStyle)
{
	m_pFloatingFrameClass = RUNTIME_CLASS(CMiniDockFrameWnd);
	for(const auto &entry : DockBarMap)
	{
		if(!(entry[1] & dockStyle & CBRS_ALIGN_ANY))
			continue;
		if(GetControlBar(entry[0]) == nullptr)
		{
			auto *dock = new CDockBar;
			if(!dock->Create(this, WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_CHILD | WS_VISIBLE | entry[1], entry[0]))
				delete dock;
		}
	}
}

void CFrameWnd::DockControlBar(CControlBar *bar, UINT dockBarID, LPCRECT rect)
{
	CDockBar *dockBar = dockBarID == 0 ? nullptr : static_cast<CDockBar *>(GetControlBar(dockBarID));
	DockControlBar(bar, dockBar, rect);
}

void CFrameWnd::DockControlBar(CControlBar *bar, CDockBar *dockBar, LPCRECT rect)
{
	if(bar == nullptr || bar->m_pDockContext == nullptr)
		return; // EnableDocking() first
	if(dockBar == nullptr)
	{
		// The dock bar of the bar's alignment.
		for(const auto &entry : DockBarMap)
		{
			if((entry[1] & CBRS_ALIGN_ANY) == (bar->m_dwStyle & CBRS_ALIGN_ANY))
			{
				dockBar = static_cast<CDockBar *>(GetControlBar(entry[0]));
				break;
			}
		}
	}
	if(dockBar != nullptr)
		dockBar->DockControlBar(bar, rect);
}

void CFrameWnd::ReDockControlBar(CControlBar *bar, CDockBar *dockBar, LPCRECT rect)
{
	if(bar == nullptr || bar->m_pDockContext == nullptr)
		return;
	if(dockBar == nullptr)
	{
		// Where a place holder says it was, or else the first dock bar it
		// may go to.
		CDockBar *first = nullptr;
		for(CControlBar *candidate : m_listControlBars)
		{
			if(!candidate->IsDockBar() || !candidate->IsWindowVisible())
				continue;
			auto *dock = static_cast<CDockBar *>(candidate);
			if(!(dock->m_dwStyle & bar->m_dwDockStyle & CBRS_ALIGN_ANY) || (dock->m_bFloating && !(bar->m_dwDockStyle & dock->m_dwStyle & CBRS_FLOAT_MULTI)))
				continue;
			if(dock->FindBar(PlaceHolder(static_cast<UINT>(::GetDlgCtrlID(bar->m_hWnd)))) > 0)
			{
				dock->DockControlBar(bar, rect);
				return;
			}
			if(first == nullptr)
				first = dock;
		}
		if(first != nullptr)
			first->DockControlBar(bar, rect);
	} else
	{
		dockBar->DockControlBar(bar, rect);
	}
}

void CFrameWnd::FloatControlBar(CControlBar *bar, CPoint point, DWORD style)
{
	// Alone in a floating frame of the same orientation: move the frame.
	if(bar->m_pDockSite != nullptr && bar->m_pDockBar != nullptr)
	{
		CDockBar *dockBar = bar->m_pDockBar;
		if(dockBar->m_bFloating && dockBar->GetDockedCount() == 1 && (style & dockBar->m_dwStyle & CBRS_ALIGN_ANY) != 0)
		{
			auto *frame = static_cast<CMiniDockFrameWnd *>(dockBar->GetParent());
			frame->SetWindowPos(nullptr, point.x, point.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
			frame->RecalcLayout(TRUE);
			frame->UpdateWindow();
			return;
		}
	}
	if(bar->m_dwStyle & CBRS_SIZE_DYNAMIC)
	{
		style |= CBRS_SIZE_DYNAMIC;
		if(style & CBRS_ORIENT_VERT)
		{
			style &= ~CBRS_ALIGN_ANY;
			style |= CBRS_ALIGN_TOP;
		}
	}
	CMiniDockFrameWnd *frame = CreateFloatingFrame(style);
	if(frame == nullptr)
		return;
	frame->SetWindowPos(nullptr, point.x, point.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
	if(frame->m_hWndOwner == nullptr)
		frame->m_hWndOwner = bar->m_hWnd;
	auto *dockBar = static_cast<CDockBar *>(frame->GetDlgItem(AFX_IDW_DOCKBAR_FLOAT));
	dockBar->DockControlBar(bar);
	frame->RecalcLayout(TRUE);
	if(GetStyle() & WS_VISIBLE)
	{
		frame->ShowWindow(SW_SHOWNA);
		frame->UpdateWindow();
	}
}

DWORD CFrameWnd::CanDock(CRect rect, DWORD dockStyle, CDockBar **dockBarOut)
{
	// Not into a floating frame unless CBRS_FLOAT_MULTI says so.
	dockStyle &= CBRS_ALIGN_ANY | CBRS_FLOAT_MULTI;
	if(dockBarOut != nullptr)
		*dockBarOut = nullptr;
	for(CControlBar *candidate : m_listControlBars)
	{
		if(!candidate->IsDockBar() || !candidate->IsWindowVisible())
			continue;
		auto *dock = static_cast<CDockBar *>(candidate);
		if(!(dock->m_dwStyle & dockStyle & CBRS_ALIGN_ANY) || (dock->m_bFloating && !(dockStyle & dock->m_dwStyle & CBRS_FLOAT_MULTI)))
			continue;
		CRect barRect;
		dock->GetWindowRect(&barRect);
		if(barRect.Width() == 0)
			barRect.right++;
		if(barRect.Height() == 0)
			barRect.bottom++;
		if(barRect.IntersectRect(barRect, rect))
		{
			if(dockBarOut != nullptr)
				*dockBarOut = dock;
			return dock->m_dwStyle & dockStyle;
		}
	}
	return 0;
}

CMiniDockFrameWnd *CFrameWnd::CreateFloatingFrame(DWORD style)
{
	CRuntimeClass *cls = m_pFloatingFrameClass != nullptr ? m_pFloatingFrameClass : RUNTIME_CLASS(CMiniDockFrameWnd);
	auto *frame = dynamic_cast<CMiniDockFrameWnd *>(cls->CreateObject());
	if(frame == nullptr)
		return nullptr;
	if(!frame->Create(this, style))
		return nullptr; // it deleted itself
	return frame;
}

void CFrameWnd::ShowControlBar(CControlBar *bar, BOOL show, BOOL delay)
{
	if(bar == nullptr || bar->m_hWnd == nullptr)
		return;
	CFrameWnd *parentFrame = bar->GetDockingFrame();
	if(delay)
	{
		bar->DelayShow(show);
		parentFrame->DelayRecalcLayout();
	} else
	{
		bar->SetWindowPos(nullptr, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | (show ? SWP_SHOWWINDOW : SWP_HIDEWINDOW));
		bar->DelayShow(show); // clears any delayed change
		if(show || !bar->IsFloating())
			parentFrame->RecalcLayout(FALSE);
	}
	// The floating frame shows with its first visible bar and hides with
	// its last.
	if(bar->IsFloating())
	{
		const int visible = bar->m_pDockBar != nullptr ? bar->m_pDockBar->GetDockedVisibleCount() : (show ? 1 : 0);
		if(visible == 1 && show)
		{
			parentFrame->m_nShowDelay = -1;
			if(delay)
			{
				parentFrame->m_nShowDelay = SW_SHOWNA;
				parentFrame->RecalcLayout(FALSE);
			} else
			{
				parentFrame->ShowWindow(SW_SHOWNA);
			}
		} else if(visible == 0)
		{
			parentFrame->m_nShowDelay = -1;
			if(delay)
				parentFrame->m_nShowDelay = SW_HIDE;
			else
				parentFrame->ShowWindow(SW_HIDE);
		} else if(!delay)
		{
			parentFrame->RecalcLayout(FALSE);
		}
	}
}

void CFrameWnd::DelayRecalcLayout(BOOL notify)
{
	m_nIdleFlags |= idleLayout | (notify ? idleNotify : 0);
}

void CFrameWnd::DestroyDockBars()
{
	// By window handle: the list changes while they go.
	std::vector<HWND> docks;
	for(CControlBar *bar : m_listControlBars)
	{
		if(bar->IsDockBar())
			docks.push_back(bar->m_hWnd);
	}
	for(HWND hwnd : docks)
	{
		auto *dock = static_cast<CDockBar *>(CWnd::FromHandlePermanent(hwnd));
		if(dock == nullptr)
			continue;
		if(dock->m_bFloating)
		{
			if(CFrameWnd *frame = dock->GetParentFrame())
				frame->DestroyWindow();
		} else
		{
			dock->DestroyWindow();
		}
	}
}

void CFrameWnd::NotifyFloatingWindows(DWORD flags)
{
	CFrameWnd *parent = (GetStyle() & WS_CHILD) ? this : GetTopLevelFrame();
	if(parent == nullptr)
		return;
	if(flags & (FS_DEACTIVATE | FS_ACTIVATE))
	{
		const BOOL activate = !(flags & FS_DEACTIVATE);
		if(activate && parent->IsWindowEnabled() && parent != this)
		{
			m_nFlags |= 0x0200; // WF_KEEPMINIACTIVE
			parent->SendMessage(WM_NCACTIVATE, TRUE);
			m_nFlags &= ~0x0200;
		} else
		{
			parent->SendMessage(WM_NCACTIVATE, activate);
		}
	}
	// The floating windows the frame owns.
	for(CFrameWnd *frame : linopenmpt::mfc::FrameList())
	{
		if(frame->m_hWnd == nullptr || frame == parent || (frame->GetStyle() & WS_CHILD))
			continue;
		for(HWND owner = ::GetWindow(frame->m_hWnd, GW_OWNER); owner != nullptr; owner = ::GetWindow(owner, GW_OWNER))
		{
			if(owner == parent->m_hWnd)
			{
				frame->SendMessage(WM_FLOATSTATUS, flags, 0);
				break;
			}
		}
	}
}

// ---- the bar state ------------------------------------------------------------------

namespace
{

CString Section(LPCTSTR format, LPCTSTR profile, int index = 0)
{
	CString section;
	section.Format(format, profile, index);
	return section;
}

} // namespace

BOOL CControlBarInfo::SaveState(LPCTSTR profileName, int index)
{
	const CString section = Section(_T("%s-Bar%d"), profileName, index);
	CWinApp *app = AfxGetApp();
	app->WriteProfileString(section, nullptr, nullptr); // a fresh section
	// A plain, docked, visible dock bar with nothing in it: nothing to say.
	if(m_bDockBar && m_bVisible && !m_bFloating && m_pointPos.x == -1 && m_pointPos.y == -1 && m_arrBarID.size() <= 1)
		return FALSE;
	app->WriteProfileInt(section, _T("BarID"), static_cast<int>(m_nBarID));
	if(!m_bVisible)
		app->WriteProfileInt(section, _T("Visible"), m_bVisible);
	if(m_bFloating)
	{
		app->WriteProfileInt(section, _T("Horz"), m_bHorz);
		app->WriteProfileInt(section, _T("Floating"), m_bFloating);
	}
	if(m_pointPos.x != -1)
		app->WriteProfileInt(section, _T("XPos"), m_pointPos.x);
	if(m_pointPos.y != -1)
		app->WriteProfileInt(section, _T("YPos"), m_pointPos.y);
	if(m_nMRUWidth != 32767)
		app->WriteProfileInt(section, _T("MRUWidth"), static_cast<int>(m_nMRUWidth));
	if(m_bDocking)
	{
		app->WriteProfileInt(section, _T("Docking"), m_bDocking);
		app->WriteProfileInt(section, _T("MRUDockID"), static_cast<int>(m_uMRUDockID));
		app->WriteProfileInt(section, _T("MRUDockLeftPos"), m_rectMRUDockPos.left);
		app->WriteProfileInt(section, _T("MRUDockTopPos"), m_rectMRUDockPos.top);
		app->WriteProfileInt(section, _T("MRUDockRightPos"), m_rectMRUDockPos.right);
		app->WriteProfileInt(section, _T("MRUDockBottomPos"), m_rectMRUDockPos.bottom);
		app->WriteProfileInt(section, _T("MRUFloatStyle"), static_cast<int>(m_dwMRUFloatStyle));
		app->WriteProfileInt(section, _T("MRUFloatXPos"), m_ptMRUFloatPos.x);
		app->WriteProfileInt(section, _T("MRUFloatYPos"), m_ptMRUFloatPos.y);
	}
	if(m_arrBarID.size() > 1)
	{
		app->WriteProfileInt(section, _T("Bars"), static_cast<int>(m_arrBarID.size()));
		for(std::size_t i = 0; i < m_arrBarID.size(); i++)
		{
			CString key;
			key.Format(_T("Bar#%d"), static_cast<int>(i));
			app->WriteProfileInt(section, key, static_cast<int>(m_arrBarID[i]));
		}
	}
	return TRUE;
}

BOOL CControlBarInfo::LoadState(LPCTSTR profileName, int index, CDockState *state)
{
	const CString section = Section(_T("%s-Bar%d"), profileName, index);
	CWinApp *app = AfxGetApp();
	m_nBarID = app->GetProfileInt(section, _T("BarID"), 0);
	m_bVisible = static_cast<BOOL>(app->GetProfileInt(section, _T("Visible"), TRUE));
	m_bHorz = static_cast<BOOL>(app->GetProfileInt(section, _T("Horz"), TRUE));
	m_bFloating = static_cast<BOOL>(app->GetProfileInt(section, _T("Floating"), FALSE));
	m_pointPos = CPoint(static_cast<int>(app->GetProfileInt(section, _T("XPos"), -1)), static_cast<int>(app->GetProfileInt(section, _T("YPos"), -1)));
	state->ScalePoint(m_pointPos);
	m_nMRUWidth = app->GetProfileInt(section, _T("MRUWidth"), 32767);
	m_bDocking = static_cast<BOOL>(app->GetProfileInt(section, _T("Docking"), 0));
	if(m_bDocking)
	{
		m_uMRUDockID = app->GetProfileInt(section, _T("MRUDockID"), 0);
		m_rectMRUDockPos = CRect(static_cast<int>(app->GetProfileInt(section, _T("MRUDockLeftPos"), 0)), static_cast<int>(app->GetProfileInt(section, _T("MRUDockTopPos"), 0)),
			static_cast<int>(app->GetProfileInt(section, _T("MRUDockRightPos"), 0)), static_cast<int>(app->GetProfileInt(section, _T("MRUDockBottomPos"), 0)));
		state->ScaleRectPos(m_rectMRUDockPos);
		m_dwMRUFloatStyle = app->GetProfileInt(section, _T("MRUFloatStyle"), 0);
		m_ptMRUFloatPos = CPoint(static_cast<int>(app->GetProfileInt(section, _T("MRUFloatXPos"), 0)), static_cast<int>(app->GetProfileInt(section, _T("MRUFloatYPos"), 0)));
		state->ScalePoint(m_ptMRUFloatPos);
	}
	const int bars = static_cast<int>(app->GetProfileInt(section, _T("Bars"), 0));
	for(int i = 0; i < bars; i++)
	{
		CString key;
		key.Format(_T("Bar#%d"), i);
		m_arrBarID.push_back(app->GetProfileInt(section, key, 0));
	}
	return m_nBarID != 0;
}

void CDockState::Clear()
{
	for(CControlBarInfo *info : m_arrBarInfo)
		delete info;
	m_arrBarInfo.clear();
}

CSize CDockState::GetScreenSize()
{
	return CSize(::GetSystemMetrics(SM_CXSCREEN), ::GetSystemMetrics(SM_CYSCREEN));
}

void CDockState::ScalePoint(CPoint &pt)
{
	if(!m_bScaling)
		return;
	const CSize device = GetScreenSize();
	pt.x = ::MulDiv(pt.x, device.cx, m_sizeDevice.cx);
	pt.y = ::MulDiv(pt.y, device.cy, m_sizeDevice.cy);
}

void CDockState::ScaleRectPos(CRect &rect)
{
	if(!m_bScaling)
		return;
	const CSize device = GetScreenSize();
	const CPoint pt(::MulDiv(rect.left, device.cx, m_sizeDevice.cx) - rect.left, ::MulDiv(rect.top, device.cy, m_sizeDevice.cy) - rect.top);
	rect.OffsetRect(pt);
}

void CDockState::LoadState(LPCTSTR profileName)
{
	CWinApp *app = AfxGetApp();
	const CString section = Section(_T("%s-Summary"), profileName);
	const int bars = static_cast<int>(app->GetProfileInt(section, _T("Bars"), 0));
	// Positions saved on a screen of another size scale to this one.
	m_sizeDevice = CSize(static_cast<int>(app->GetProfileInt(section, _T("ScreenCX"), 0)), static_cast<int>(app->GetProfileInt(section, _T("ScreenCY"), 0)));
	m_bScaling = m_sizeDevice != CSize(0, 0) && GetScreenSize() != m_sizeDevice;
	for(int i = 0; i < bars; i++)
	{
		auto *info = new CControlBarInfo;
		m_arrBarInfo.push_back(info);
		info->LoadState(profileName, i, this);
	}
}

void CDockState::SaveState(LPCTSTR profileName)
{
	CWinApp *app = AfxGetApp();
	int index = 0;
	for(CControlBarInfo *info : m_arrBarInfo)
	{
		if(info->SaveState(profileName, index))
			index++;
	}
	const CString section = Section(_T("%s-Summary"), profileName);
	app->WriteProfileInt(section, _T("Bars"), index);
	const CSize size = GetScreenSize();
	app->WriteProfileInt(section, _T("ScreenCX"), size.cx);
	app->WriteProfileInt(section, _T("ScreenCY"), size.cy);
}

void CFrameWnd::GetDockState(CDockState &state) const
{
	state.Clear();
	for(CControlBar *bar : m_listControlBars)
	{
		auto *info = new CControlBarInfo;
		bar->GetBarInfo(info);
		state.m_arrBarInfo.push_back(info);
	}
}

void CFrameWnd::SetDockState(const CDockState &state)
{
	// First: the bars, and floating frames for floating dock bars.
	for(CControlBarInfo *info : state.m_arrBarInfo)
	{
		if(info->m_bFloating)
		{
			CMiniDockFrameWnd *frame = CreateFloatingFrame(info->m_bHorz ? CBRS_ALIGN_TOP : CBRS_ALIGN_LEFT);
			if(frame == nullptr)
				continue;
			CRect rect(info->m_pointPos, CSize(10, 10));
			frame->CalcWindowRect(&rect);
			frame->SetWindowPos(nullptr, rect.left, rect.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
			info->m_pBar = static_cast<CDockBar *>(frame->GetDlgItem(AFX_IDW_DOCKBAR_FLOAT));
		} else
		{
			info->m_pBar = GetControlBar(info->m_nBarID);
			if(info->m_pBar == nullptr)
				continue;
		}
		info->m_pBar->m_nMRUWidth = info->m_nMRUWidth;
	}
	// Second: dock them all and set them up.
	for(CControlBarInfo *info : state.m_arrBarInfo)
	{
		if(info->m_pBar != nullptr)
			info->m_pBar->SetBarInfo(info, this);
	}
	// Last: show the floating frames that have something to show.
	for(CControlBarInfo *info : state.m_arrBarInfo)
	{
		if(info->m_pBar == nullptr || !info->m_bFloating)
			continue;
		CFrameWnd *frame = info->m_pBar->GetParentFrame();
		auto *dock = static_cast<CDockBar *>(info->m_pBar);
		if(dock->GetDockedCount() == 0)
		{
			frame->DestroyWindow(); // none of its bars exist any more
			continue;
		}
		if(dock->GetDockedVisibleCount() > 0)
		{
			frame->RecalcLayout();
			frame->ShowWindow(SW_SHOWNA);
		}
	}
	DelayRecalcLayout();
}

void CFrameWnd::LoadBarState(LPCTSTR profileName)
{
	CDockState state;
	state.LoadState(profileName);
	SetDockState(state);
}

void CFrameWnd::SaveBarState(LPCTSTR profileName) const
{
	CDockState state;
	GetDockState(state);
	state.SaveState(profileName);
}
