// linOpenMPT MFC layer: CSplitterWnd, as MFC's winsplit.cpp has it for
// static splitters (CreateStatic), which is what upstream uses: a grid of
// panes (views, ids AFX_IDW_PANE_FIRST + row * 16 + col) with bars between
// them that the mouse drags.
//
// Each pane gets a 2-pixel sunken border drawn by the splitter, and the bars
// are m_cxSplitterGap wide, as MFC draws them since Windows 95. MFC drags an
// inverted tracker and resizes on release; here the panes follow the mouse.
#include "MfcInternal.h"

#include <algorithm>
#include <climits>

BEGIN_MESSAGE_MAP(CSplitterWnd, CWnd)
	ON_WM_SIZE()
	ON_WM_PAINT()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_MOUSEMOVE()
	ON_WM_SETCURSOR()
	ON_WM_CAPTURECHANGED()
END_MESSAGE_MAP()

BOOL CSplitterWnd::CreateStatic(CWnd *parent, int rows, int cols, DWORD style, UINT id)
{
	if(parent == nullptr || rows < 1 || cols < 1 || rows > 16 || cols > 16)
		return FALSE;
	m_rows.assign(static_cast<std::size_t>(rows), RowColInfo{});
	m_cols.assign(static_cast<std::size_t>(cols), RowColInfo{});
	// No background: the panes and OnPaint cover everything.
	const LPCTSTR cls = AfxRegisterWndClass(CS_DBLCLKS, nullptr, nullptr, nullptr);
	return CreateEx(0, cls, nullptr, style | WS_CHILD, CRect(0, 0, 0, 0), parent, id, nullptr);
}

BOOL CSplitterWnd::CreateView(int row, int col, CRuntimeClass *viewClass, SIZE initialSize, CCreateContext *context)
{
	if(row < 0 || row >= GetRowCount() || col < 0 || col >= GetColumnCount() || viewClass == nullptr)
		return FALSE;
	if(GetDlgItem(IdFromRowCol(row, col)) != nullptr)
		return FALSE; // the pane is taken
	m_cols[static_cast<std::size_t>(col)].nIdealSize = initialSize.cx;
	m_rows[static_cast<std::size_t>(row)].nIdealSize = initialSize.cy;

	// Without a context, the new view shows the document of the active one,
	// and gets its initial update here.
	BOOL sendInitialUpdate = FALSE;
	CCreateContext contextT;
	if(context == nullptr)
	{
		if(auto *oldView = dynamic_cast<CView *>(GetActivePane()))
		{
			contextT.m_pLastView = oldView;
			contextT.m_pCurrentDoc = oldView->GetDocument();
			if(contextT.m_pCurrentDoc != nullptr)
				contextT.m_pNewDocTemplate = contextT.m_pCurrentDoc->GetDocTemplate();
		}
		context = &contextT;
		sendInitialUpdate = TRUE;
	}

	auto *wnd = dynamic_cast<CWnd *>(viewClass->CreateObject());
	if(wnd == nullptr)
		return FALSE;
	const CRect rect(CPoint(0, 0), initialSize);
	if(!wnd->Create(nullptr, nullptr, AFX_WS_DEFAULT_VIEW & ~WS_BORDER, rect, this, static_cast<UINT>(IdFromRowCol(row, col)), context))
		return FALSE; // the view deleted itself
	if(sendInitialUpdate)
		wnd->SendMessage(WM_INITIALUPDATE);
	return TRUE;
}

void CSplitterWnd::DeleteView(int row, int col)
{
	CWnd *pane = GetPane(row, col);
	if(pane == nullptr)
		return;
	// The frame's active view moves to another pane first.
	if(CFrameWnd *frame = GetParentFrame())
	{
		if(frame->GetActiveView() == pane)
		{
			CView *next = nullptr;
			for(int r = 0; r < GetRowCount() && next == nullptr; r++)
			{
				for(int c = 0; c < GetColumnCount() && next == nullptr; c++)
				{
					if(r != row || c != col)
						next = dynamic_cast<CView *>(GetPane(r, c));
				}
			}
			frame->SetActiveView(next);
		}
	}
	pane->DestroyWindow();
}

CWnd *CSplitterWnd::GetPane(int row, int col) const
{
	if(m_hWnd == nullptr)
		return nullptr;
	const HWND pane = ::GetDlgItem(m_hWnd, IdFromRowCol(row, col));
	if(pane == nullptr)
		return nullptr;
	if(CWnd *perm = CWnd::FromHandlePermanent(pane))
		return perm;
	return CWnd::FromHandle(pane);
}

void CSplitterWnd::SetRowInfo(int row, int ideal, int min)
{
	if(row < 0 || row >= GetRowCount())
		return;
	m_rows[static_cast<std::size_t>(row)].nIdealSize = ideal;
	m_rows[static_cast<std::size_t>(row)].nMinSize = min;
}

void CSplitterWnd::GetRowInfo(int row, int &cur, int &min) const
{
	cur = min = 0;
	if(row < 0 || row >= GetRowCount())
		return;
	cur = m_rows[static_cast<std::size_t>(row)].nCurSize;
	min = m_rows[static_cast<std::size_t>(row)].nMinSize;
}

void CSplitterWnd::SetColumnInfo(int col, int ideal, int min)
{
	if(col < 0 || col >= GetColumnCount())
		return;
	m_cols[static_cast<std::size_t>(col)].nIdealSize = ideal;
	m_cols[static_cast<std::size_t>(col)].nMinSize = min;
}

void CSplitterWnd::GetColumnInfo(int col, int &cur, int &min) const
{
	cur = min = 0;
	if(col < 0 || col >= GetColumnCount())
		return;
	cur = m_cols[static_cast<std::size_t>(col)].nCurSize;
	min = m_cols[static_cast<std::size_t>(col)].nMinSize;
}

BOOL CSplitterWnd::IsChildPane(CWnd *wnd, int *row, int *col)
{
	if(wnd == nullptr || ::GetParent(wnd->m_hWnd) != m_hWnd)
		return FALSE;
	const int id = ::GetDlgCtrlID(wnd->m_hWnd);
	if(id < AFX_IDW_PANE_FIRST || id > AFX_IDW_PANE_LAST)
		return FALSE;
	if(row != nullptr)
		*row = (id - AFX_IDW_PANE_FIRST) / 16;
	if(col != nullptr)
		*col = (id - AFX_IDW_PANE_FIRST) % 16;
	return TRUE;
}

CWnd *CSplitterWnd::GetActivePane(int *row, int *col)
{
	CWnd *pane = nullptr;
	if(CFrameWnd *frame = GetParentFrame())
		pane = frame->GetActiveView();
	if(pane == nullptr)
		pane = CWnd::FromHandle(::GetFocus());
	if(pane != nullptr && !IsChildPane(pane, row, col))
		pane = nullptr;
	return pane;
}

void CSplitterWnd::SetActivePane(int row, int col, CWnd *wnd)
{
	CWnd *pane = wnd != nullptr ? wnd : GetPane(row, col);
	if(pane == nullptr)
		return;
	if(auto *view = dynamic_cast<CView *>(pane))
	{
		if(CFrameWnd *frame = GetParentFrame())
		{
			frame->SetActiveView(view);
			return;
		}
	}
	pane->SetFocus();
}

namespace
{

// MFC's _AfxLayoutRowCol.
template <typename Info>
void Layout(std::vector<Info> &info, int size, int splitterGap)
{
	constexpr int border2 = 2; // afxData.cxBorder2
	const int count = static_cast<int>(info.size());
	if(size < 0)
		size = 0;
	for(int i = 0; i < count - 1; i++)
	{
		if(info[i].nIdealSize < info[i].nMinSize)
			info[i].nIdealSize = 0; // too small to see
		info[i].nCurSize = info[i].nIdealSize;
	}
	info[count - 1].nCurSize = INT_MAX; // the rest
	for(int i = 0; i < count; i++)
	{
		if(size == 0)
		{
			info[i].nCurSize = 0;
			continue;
		} else if(size < info[i].nMinSize && i != 0)
		{
			// Panes below their minimum go, and their room goes to the one
			// before.
			info[i].nCurSize = 0;
			info[i - 1].nCurSize += size + border2;
			size = 0;
		} else
		{
			if(info[i].nCurSize == 0)
			{
				// too small to see
			} else if(size < info[i].nCurSize)
			{
				info[i].nCurSize = size;
				size = 0;
			} else
			{
				size -= info[i].nCurSize;
			}
		}
		if(i != count - 1)
		{
			if(size > splitterGap)
			{
				size -= splitterGap;
			} else
			{
				info[i].nCurSize += size;
				if(info[i].nCurSize > splitterGap - border2)
					info[i].nCurSize -= splitterGap - border2;
				size = 0;
			}
		}
	}
}

} // namespace

CRect CSplitterWnd::PaneRect(int row, int col) const
{
	int x = m_cxBorder, y = m_cyBorder;
	for(int c = 0; c < col; c++)
		x += m_cols[static_cast<std::size_t>(c)].nCurSize + m_cxSplitterGap;
	for(int r = 0; r < row; r++)
		y += m_rows[static_cast<std::size_t>(r)].nCurSize + m_cySplitterGap;
	return CRect(x, y, x + m_cols[static_cast<std::size_t>(col)].nCurSize, y + m_rows[static_cast<std::size_t>(row)].nCurSize);
}

void CSplitterWnd::RecalcLayout()
{
	if(m_hWnd == nullptr || m_rows.empty() || m_cols.empty())
		return;
	CRect inside;
	GetClientRect(&inside);
	inside.InflateRect(-m_cxBorder, -m_cyBorder);
	Layout(m_cols, inside.Width(), m_cxSplitterGap);
	Layout(m_rows, inside.Height(), m_cySplitterGap);

	for(int row = 0; row < GetRowCount(); row++)
	{
		for(int col = 0; col < GetColumnCount(); col++)
		{
			CWnd *pane = GetPane(row, col);
			if(pane == nullptr)
				continue;
			CRect rect = PaneRect(row, col);
			// A pane with a 3D border of its own covers the splitter's.
			if(pane->GetExStyle() & WS_EX_CLIENTEDGE)
				rect.InflateRect(2, 2);
			CRect current;
			pane->GetWindowRect(&current);
			ScreenToClient(&current);
			if(current != rect)
				pane->SetWindowPos(nullptr, rect.left, rect.top, rect.Width(), rect.Height(), SWP_NOZORDER | SWP_NOACTIVATE);
		}
	}
	Invalidate(TRUE);
}

void CSplitterWnd::OnSize(UINT type, int cx, int cy)
{
	if(type != SIZE_MINIMIZED && cx > 0 && cy > 0)
		RecalcLayout();
	CWnd::OnSize(type, cx, cy);
}

void CSplitterWnd::OnPaint()
{
	CPaintDC dc(this);
	CRect client;
	GetClientRect(&client);
	dc.FillSolidRect(&client, ::GetSysColor(COLOR_BTNFACE));
	// A sunken border around each pane (MFC's splitBorder).
	for(int row = 0; row < GetRowCount(); row++)
	{
		for(int col = 0; col < GetColumnCount(); col++)
		{
			CRect rect = PaneRect(row, col);
			if(rect.IsRectEmpty())
				continue;
			rect.InflateRect(m_cxBorder, m_cyBorder);
			dc.Draw3dRect(&rect, ::GetSysColor(COLOR_BTNSHADOW), ::GetSysColor(COLOR_BTNHIGHLIGHT));
			rect.InflateRect(-1, -1);
			dc.Draw3dRect(&rect, ::GetSysColor(COLOR_WINDOWFRAME), ::GetSysColor(COLOR_BTNFACE));
		}
	}
}

int CSplitterWnd::HitTestBar(CPoint point, BOOL &row) const
{
	// The bar after row (or column) i spans the gap below (right of) it.
	int y = m_cyBorder;
	for(int r = 0; r + 1 < GetRowCount(); r++)
	{
		y += m_rows[static_cast<std::size_t>(r)].nCurSize;
		if(point.y >= y && point.y < y + m_cySplitterGap)
		{
			row = TRUE;
			return r;
		}
		y += m_cySplitterGap;
	}
	int x = m_cxBorder;
	for(int c = 0; c + 1 < GetColumnCount(); c++)
	{
		x += m_cols[static_cast<std::size_t>(c)].nCurSize;
		if(point.x >= x && point.x < x + m_cxSplitterGap)
		{
			row = FALSE;
			return c;
		}
		x += m_cxSplitterGap;
	}
	return -1;
}

BOOL CSplitterWnd::OnSetCursor(CWnd *wnd, UINT hitTest, UINT message)
{
	if(hitTest == HTCLIENT && wnd == this)
	{
		CPoint point;
		::GetCursorPos(&point);
		ScreenToClient(&point);
		BOOL row = FALSE;
		if(m_trackRow >= 0 || m_trackCol >= 0 || HitTestBar(point, row) >= 0)
		{
			const bool vertical = m_trackRow >= 0 || (m_trackCol < 0 && row);
			::SetCursor(::LoadCursor(nullptr, vertical ? IDC_SIZENS : IDC_SIZEWE));
			return TRUE;
		}
	}
	return CWnd::OnSetCursor(wnd, hitTest, message);
}

void CSplitterWnd::OnLButtonDown(UINT flags, CPoint point)
{
	BOOL row = FALSE;
	const int bar = HitTestBar(point, row);
	if(bar < 0)
	{
		CWnd::OnLButtonDown(flags, point);
		return;
	}
	if(row)
	{
		m_trackRow = bar;
		m_trackStartSize = m_rows[static_cast<std::size_t>(bar)].nCurSize;
	} else
	{
		m_trackCol = bar;
		m_trackStartSize = m_cols[static_cast<std::size_t>(bar)].nCurSize;
	}
	m_trackStart = point;
	SetCapture();
}

void CSplitterWnd::TrackTo(CPoint point)
{
	// MFC's TrackRowSize / TrackColumnSize: the new ideal size, or none
	// when the pane gets smaller than its minimum.
	auto track = [&](std::vector<RowColInfo> &info, int index, int size) {
		RowColInfo &pane = info[static_cast<std::size_t>(index)];
		const int total = pane.nCurSize + info[static_cast<std::size_t>(index) + 1].nCurSize;
		size = std::clamp(size, 0, std::max(total, 0));
		pane.nIdealSize = size < pane.nMinSize ? 0 : size;
	};
	if(m_trackRow >= 0)
		track(m_rows, m_trackRow, m_trackStartSize + point.y - m_trackStart.y);
	else if(m_trackCol >= 0)
		track(m_cols, m_trackCol, m_trackStartSize + point.x - m_trackStart.x);
	else
		return;
	RecalcLayout();
	UpdateWindow();
}

void CSplitterWnd::OnMouseMove(UINT flags, CPoint point)
{
	if((m_trackRow >= 0 || m_trackCol >= 0) && ::GetCapture() == m_hWnd)
		TrackTo(point);
	else
		CWnd::OnMouseMove(flags, point);
}

void CSplitterWnd::OnLButtonUp(UINT flags, CPoint point)
{
	if(m_trackRow >= 0 || m_trackCol >= 0)
	{
		TrackTo(point);
		m_trackRow = m_trackCol = -1;
		if(::GetCapture() == m_hWnd)
			::ReleaseCapture();
		return;
	}
	CWnd::OnLButtonUp(flags, point);
}

void CSplitterWnd::OnCaptureChanged(CWnd *wnd)
{
	m_trackRow = m_trackCol = -1;
	CWnd::OnCaptureChanged(wnd);
}

BOOL CSplitterWnd::OnCommand(WPARAM wp, LPARAM lp)
{
	if(CWnd::OnCommand(wp, lp))
		return TRUE;
	// Commands to the splitter go to its frame.
	CFrameWnd *frame = GetParentFrame();
	return frame != nullptr && frame->SendMessage(WM_COMMAND, wp, lp) != 0;
}

BOOL CSplitterWnd::OnNotify(WPARAM wp, LPARAM lp, LRESULT *result)
{
	if(CWnd::OnNotify(wp, lp, result))
		return TRUE;
	CFrameWnd *frame = GetParentFrame();
	if(frame == nullptr)
		return FALSE;
	*result = frame->SendMessage(WM_NOTIFY, wp, lp);
	return TRUE;
}
