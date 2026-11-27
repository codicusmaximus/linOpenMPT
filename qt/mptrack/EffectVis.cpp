// Draft from qt/tools/twin.py, not yet edited by hand.
/*
 * EffectVis.cpp
 * -------------
 * Purpose: Implementation of parameter visualisation dialog.
 * Notes  : (currenlty none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "EffectVis.h"
#include "mfcless/Widgets.h"
#include "Childfrm.h"
#include "Globals.h"
#include "HighDPISupport.h"
#include "InputHandler.h"
#include "Mainfrm.h"
#include "Moddoc.h"
#include "Mptrack.h"
#include "resource.h"
#include "View_pat.h"


OPENMPT_NAMESPACE_BEGIN

CEffectVis::Action CEffectVis::m_nAction = CEffectVis::Action::OverwriteFX;

void CEffectVis::RegisterMessageMap()
{
	DialogBase::RegisterMessageMap();
	// ON_WM_ERASEBKGND(): by hand (Qt event override)
	// ON_WM_PAINT() (mfcless::Window)
	// ON_WM_SIZE() (mfcless::Window)
	// ON_WM_LBUTTONDOWN() (mfcless::Window)
	// ON_WM_LBUTTONUP() (mfcless::Window)
	// ON_WM_MOUSEMOVE() (mfcless::Window)
	// ON_WM_RBUTTONDOWN() (mfcless::Window)
	// ON_WM_RBUTTONUP() (mfcless::Window)
	AddNotify(IDC_VISACTION, mfcless::Notify::SelChange, &CEffectVis::OnActionChanged);
	AddNotify(IDC_VISEFFECTLIST, mfcless::Notify::SelChange, &CEffectVis::OnEffectChanged);
}


CEffectVis::CEffectVis(CViewPattern *pViewPattern, ROWINDEX startRow, ROWINDEX endRow, CHANNELINDEX nchn, CModDoc &modDoc, PATTERNINDEX pat)
	: m_ModDoc(modDoc)
	, m_SndFile(modDoc.GetSoundFile())
	, m_pViewPattern(pViewPattern)
	, m_effectInfo(modDoc.GetSoundFile())
{
	m_nFillEffect = m_effectInfo.GetIndexFromEffect(CMD_SMOOTHMIDI, 0);
	m_templatePCNote.Set(NOTE_PCS, 1, 0, 0);
	UpdateSelection(startRow, endRow, nchn, pat);
}


void CEffectVis::DoDataExchange(DataExchange &dx)
{
	DialogBase::DoDataExchange(dx);
	ExchangeControl(dx, IDC_VISSTATUS, m_edVisStatus);
	ExchangeControl(dx, IDC_VISACTION, m_cmbActionList);
}


void CEffectVis::OnActionChanged()
{
	const auto oldActionWasPC = m_nAction == Action::FillPC || m_nAction == Action::OverwritePC;
	m_nAction = static_cast<Action>(mfcless::GetItemData(m_cmbActionList, m_cmbActionList->currentIndex()));
	const auto newActionIsPC = m_nAction == Action::FillPC || m_nAction == Action::OverwritePC;

	m_cmbEffectList->setEnabled((m_nAction == Action::Preserve) ? false : true);

	if(oldActionWasPC != newActionIsPC)
		UpdateEffectList();
}


void CEffectVis::OnEffectChanged()
{
	if(m_nAction == Action::FillPC || m_nAction == Action::OverwritePC)
		m_templatePCNote.SetValueVolCol(static_cast<uint16>(m_cmbEffectList->currentIndex()));
	else
		m_nFillEffect = static_cast<uint32>(mfcless::GetItemData(m_cmbEffectList, m_cmbEffectList->currentIndex()));
}


void CEffectVis::OnPaint()
{
	mfcless::PaintDC dc(this); // device context for painting
	ShowVis(&dc);
}


uint16 CEffectVis::GetParam(ROWINDEX row) const
{
	uint16 paramValue = 0;

	if(m_SndFile.Patterns.IsValidPat(m_nPattern))
	{
		const ModCommand &m = *m_SndFile.Patterns[m_nPattern].GetpModCommand(row, m_nChan);
		if (m.IsPcNote())
		{
			paramValue = m.GetValueEffectCol();
		} else
		{
			paramValue = m.param;
		}
	}

	return paramValue;
}


// Sets a row's param value based on the vertical cursor position.
// Sets either plain pattern effect parameter or PC note parameter
// as appropriate, depending on contents of row.
void CEffectVis::SetParamFromY(ROWINDEX row, int y)
{
	if(!m_SndFile.Patterns.IsValidPat(m_nPattern))
		return;

	ModCommand &m = *m_SndFile.Patterns[m_nPattern].GetpModCommand(row, m_nChan);
	if(IsPcNote(row))
	{
		uint16 param = ScreenYToPCParam(y);
		m.SetValueEffectCol(param);
	} else
	{
		ModCommand::PARAM param = ScreenYToFXParam(y);
		// Cap the parameter value as appropriate, based on effect type (e.g. Zxx gets capped to [0x00,0x7F])
		m_effectInfo.GetEffectFromIndex(m_effectInfo.GetIndexFromEffect(m.command, param), param);
		m.param = param;
	}
}



EffectCommand CEffectVis::GetCommand(ROWINDEX row) const
{
	if(m_SndFile.Patterns.IsValidPat(m_nPattern))
		return static_cast<EffectCommand>(m_SndFile.Patterns[m_nPattern].GetpModCommand(row, m_nChan)->command);
	else
		return CMD_NONE;
}


void CEffectVis::SetCommand(ROWINDEX row, EffectCommand command)
{
	if(m_SndFile.Patterns.IsValidPat(m_nPattern))
	{
		ModCommand &m = *m_SndFile.Patterns[m_nPattern].GetpModCommand(row, m_nChan);
		if(m.IsPcNote())
		{
			// Clear PC note
			m.note = 0;
			m.instr = 0;
			m.volcmd = VOLCMD_NONE;
			m.vol = 0;
		}
		m.command = command;
	}
}


std::pair<ROWINDEX, ROWINDEX> CEffectVis::GetTimeSignature() const
{
	ROWINDEX beat = m_SndFile.m_nDefaultRowsPerBeat, measure = m_SndFile.m_nDefaultRowsPerMeasure;
	if(m_SndFile.Patterns.IsValidIndex(m_nPattern) && m_SndFile.Patterns[m_nPattern].GetOverrideSignature())
	{
		beat = m_SndFile.Patterns[m_nPattern].GetRowsPerBeat();
		measure = m_SndFile.Patterns[m_nPattern].GetRowsPerMeasure();
	}
	return std::make_pair(beat, measure);
}


int CEffectVis::RowToScreenX(ROWINDEX row) const
{
	if ((row >= m_startRow) || (row <= m_endRow))
		return mpt::saturate_round<int>(m_rcDraw.left + m_innerBorder + (row - m_startRow) * m_pixelsPerRow);
	return -1;
}


int CEffectVis::RowToScreenY(ROWINDEX row) const
{
	int screenY = -1;

	if(m_SndFile.Patterns.IsValidPat(m_nPattern))
	{
		const ModCommand &m = *m_SndFile.Patterns[m_nPattern].GetpModCommand(row, m_nChan);
		if(m.IsPcNote())
		{
			uint16 paramValue = m.GetValueEffectCol();
			screenY = PCParamToScreenY(paramValue);
		} else
		{
			uint16 paramValue = m.param;
			screenY = FXParamToScreenY(paramValue);
		}
	}

	return screenY;
}


int CEffectVis::FXParamToScreenY(uint16 param) const
{
	if(param >= 0x00 && param <= 0xFF)
		return mpt::saturate_round<int>(m_rcDraw.bottom - param * m_pixelsPerFXParam);
	return -1;
}


int CEffectVis::PCParamToScreenY(uint16 param) const
{
	if(param >= 0x00 && param <= ModCommand::maxColumnValue)
		return mpt::saturate_round<int>(m_rcDraw.bottom - param*m_pixelsPerPCParam);
	return -1;
}


ModCommand::PARAM CEffectVis::ScreenYToFXParam(int y) const
{
	if(y <= FXParamToScreenY(0xFF))
		return 0xFF;

	if(y >= FXParamToScreenY(0x00))
		return 0x00;

	return mpt::saturate_round<ModCommand::PARAM>((m_rcDraw.bottom - y) / m_pixelsPerFXParam);
}


uint16 CEffectVis::ScreenYToPCParam(int y) const
{
	if(y <= PCParamToScreenY(ModCommand::maxColumnValue))
		return ModCommand::maxColumnValue;

	if(y >= PCParamToScreenY(0x00))
		return 0x00;

	return mpt::saturate_round<uint16>((m_rcDraw.bottom - y) / m_pixelsPerPCParam);
}


ROWINDEX CEffectVis::ScreenXToRow(int x) const
{
	if(x <= RowToScreenX(m_startRow))
		return m_startRow;

	if(x >= RowToScreenX(m_endRow))
		return m_endRow;

	return mpt::saturate_round<ROWINDEX>(m_startRow + (x - m_innerBorder) / m_pixelsPerRow);
}


void CEffectVis::DrawGrid()
{
	// Lots of room for optimisation here.
	// Draw vertical grid lines
	const auto [beat, measure] = GetTimeSignature();

	m_dcGrid.FillSolidRect(&m_rcDraw, 0);
	auto oldPen = m_dcGrid.SelectStockObject(mfcless::Stock::DCPen);
	for(ROWINDEX row = m_startRow; row <= m_endRow; row++)
	{
		if(row % measure == 0)
			m_dcGrid.SetDCPenColor(mfcless::Rgb(0xFF, 0xFF, 0xFF));
		else if(row % beat == 0)
			m_dcGrid.SetDCPenColor(mfcless::Rgb(0x99, 0x99, 0x99));
		else
			m_dcGrid.SetDCPenColor(mfcless::Rgb(0x55, 0x55, 0x55));
		int x1 = RowToScreenX(row);
		m_dcGrid.MoveTo(x1, m_rcDraw.top);
		m_dcGrid.LineTo(x1, m_rcDraw.bottom);
	}

	// Draw horizontal grid lines
	constexpr uint32 numHorizontalLines = 4;
	for(uint32 i = 0; i < numHorizontalLines; i++)
	{
		uint32 c = 0;
		switch(i % 4)
		{
		case 0: c = mfcless::Rgb(0x00, 0x00, 0x00); break;
		case 1: c = mfcless::Rgb(0x40, 0x40, 0x40); break;
		case 2: c = mfcless::Rgb(0x80, 0x80, 0x80); break;
		case 3: c = mfcless::Rgb(0xCC, 0xCC, 0xCC); break;
		}
		m_dcGrid.SetDCPenColor(c);
		int y1 = m_rcDraw.bottom / numHorizontalLines * i;
		m_dcGrid.MoveTo(m_rcDraw.left + m_innerBorder, y1);
		m_dcGrid.LineTo(m_rcDraw.right - m_innerBorder, y1);
	}
	m_dcGrid.SelectObject(oldPen);
}


void CEffectVis::SetPlayCursor(PATTERNINDEX nPat, ROWINDEX nRow)
{
	if(nPat == m_nPattern && nRow == m_nOldPlayPos)
		return;

	if(m_nOldPlayPos >= m_startRow && m_nOldPlayPos <= m_endRow)
	{
		// erase current playpos
		int x1 = RowToScreenX(m_nOldPlayPos);
		m_dcPlayPos.SelectStockObject(mfcless::Stock::BlackPen);
		m_dcPlayPos.MoveTo(x1,m_rcDraw.top);
		m_dcPlayPos.LineTo(x1,m_rcDraw.bottom);
	}

	if((nRow < m_startRow) || (nRow > m_endRow) || (nPat != m_nPattern))
		return;

	int x1 = RowToScreenX(nRow);
	m_dcPlayPos.SelectStockObject(mfcless::Stock::DCPen);
	m_dcPlayPos.SetDCPenColor(TrackerSettings::Instance().rgbCustomColors[MODCOLOR_SAMPLE]);
	m_dcPlayPos.MoveTo(x1,m_rcDraw.top);
	m_dcPlayPos.LineTo(x1,m_rcDraw.bottom);

	m_nOldPlayPos = nRow;
	InvalidateRect(NULL, false);
}


void CEffectVis::ShowVis(mfcless::DC *pDC)
{
	if (m_forceRedraw)
	{
		m_forceRedraw = false;

		// if we already have a memory dc, destroy it (this occurs for a re-size)
		if (m_dcGrid.m_hDC)
		{
			m_dcGrid.SelectObject(m_pbOldGrid);
			m_dcGrid.DeleteDC();

			m_dcNodes.SelectObject(m_pbOldNodes);
			m_dcNodes.DeleteDC();

			m_dcPlayPos.SelectObject(m_pbOldPlayPos);
			m_dcPlayPos.DeleteDC();

			m_bPlayPos.DeleteObject();
			m_bGrid.DeleteObject();
			m_bNodes.DeleteObject();
		}

		// create a memory based dc for drawing the grid
		m_dcGrid.CreateCompatibleDC(pDC);
		m_bGrid.CreateCompatibleBitmap(pDC, m_rcDraw.Width(), m_rcDraw.Height());
		m_pbOldGrid = *m_dcGrid.SelectObject(&m_bGrid);

		// create a memory based dc for drawing the nodes
		m_dcNodes.CreateCompatibleDC(pDC);
		m_bNodes.CreateCompatibleBitmap(pDC, m_rcDraw.Width(), m_rcDraw.Height());
		m_pbOldNodes = *m_dcNodes.SelectObject(&m_bNodes);

		// create a memory based dc for drawing the nodes
		m_dcPlayPos.CreateCompatibleDC(pDC);
		m_bPlayPos.CreateCompatibleBitmap(pDC, m_rcDraw.Width(), m_rcDraw.Height());
		m_pbOldPlayPos = *m_dcPlayPos.SelectObject(&m_bPlayPos);

		SetPlayCursor(m_nPattern, m_nOldPlayPos);
		DrawGrid();
		DrawNodes();
	}

	// display the new image, combining the nodes with the grid
	ShowVisImage(pDC);

}


void CEffectVis::ShowVisImage(mfcless::DC *pDC)
{
	// to avoid flicker, establish a memory dc, draw to it
	// and then BitBlt it to the destination "pDC"
	mfcless::DC memDC;
	memDC.CreateCompatibleDC(pDC);
	if (!memDC)
		return;

	mfcless::Bitmap memBitmap;
	memBitmap.CreateCompatibleBitmap(pDC, m_rcDraw.Width(), m_rcDraw.Height());
	mfcless::Bitmap *oldBitmap = memDC.SelectObject(&memBitmap);

	// make sure we have the bitmaps
	if (!m_dcGrid.m_hDC)
		return;
	if (!m_dcNodes.m_hDC)
		return;
	if (!m_dcPlayPos.m_hDC)
		return;

	if (memDC.m_hDC != nullptr)
	{
		// draw the grid
		memDC.BitBlt(0, 0, m_rcDraw.Width(), m_rcDraw.Height(), &m_dcGrid, 0, 0, mfcless::rop::SrcCopy);

		// merge the nodes image with the grid
		memDC.TransparentBlt(0, 0, m_rcDraw.Width(), m_rcDraw.Height(), &m_dcNodes, 0, 0, m_rcDraw.Width(), m_rcDraw.Height(), 0x00000000);
		// further merge the playpos
		memDC.TransparentBlt(0, 0, m_rcDraw.Width(), m_rcDraw.Height(), &m_dcPlayPos, 0, 0, m_rcDraw.Width(), m_rcDraw.Height(), 0x00000000);

		// copy the resulting bitmap to the destination
		pDC->BitBlt(0, 0, m_rcDraw.Width(), m_rcDraw.Height(), &memDC, 0, 0, mfcless::rop::SrcCopy);
	}

	memDC.SelectObject(oldBitmap);

}


void CEffectVis::DrawNodes()
{
	if(m_rcDraw.IsRectEmpty())
		return;

	if(m_nDragItem < m_startRow || m_nDragItem > m_endRow)
		m_nDragItem = m_startRow;

	//Draw
	const int lineWidth = HighDPISupport::ScalePixels(1, this);
	const int nodeSizeHalf = m_nodeSizeHalf;
	const int nodeSizeHalf2 = nodeSizeHalf - lineWidth + 1;
	const int nodeSize = 2 * nodeSizeHalf + 1;

	//erase
	if ((ROWINDEX)m_nRowToErase < m_startRow || m_nParamToErase < 0)
	{
		m_dcNodes.FillSolidRect(&m_rcDraw, mfcless::Rgb(0, 0, 0));
	} else
	{
		int x = RowToScreenX(m_nRowToErase);
		mfcless::Rect r(x - nodeSizeHalf, m_rcDraw.top, x + nodeSizeHalf + 1, m_rcDraw.bottom);
		m_dcNodes.FillSolidRect(&r, mfcless::Rgb(0, 0, 0));
	}

	for (ROWINDEX row = m_startRow; row <= m_endRow; row++)
	{
		uint32 col = IsPcNote(row) ? mfcless::Rgb(0xFF, 0xFF, 0x00) : mfcless::Rgb(0xD0, 0xFF, 0xFF);
		int x = RowToScreenX(row);
		int y = RowToScreenY(row);
		m_dcNodes.FillSolidRect(x - nodeSizeHalf, y - nodeSizeHalf, nodeSize, lineWidth, col);  // Top
		m_dcNodes.FillSolidRect(x + nodeSizeHalf2, y - nodeSizeHalf, lineWidth, nodeSize, col); // Right
		m_dcNodes.FillSolidRect(x - nodeSizeHalf, y + nodeSizeHalf2, nodeSize, lineWidth, col); // Bottom
		m_dcNodes.FillSolidRect(x - nodeSizeHalf, y - nodeSizeHalf, lineWidth, nodeSize, col);  // Left
	}

}

void CEffectVis::InvalidateRow(int row)
{
	if (((uint32)row < m_startRow) ||  ((uint32)row > m_endRow)) return;

//It seems this optimisation doesn't work properly yet.	Disable in Update()

	int x = RowToScreenX(row);
	mfcless::Rect invalidated;
	invalidated.bottom = m_rcDraw.bottom;
	invalidated.top = m_rcDraw.top;
	invalidated.left = x - m_nodeSizeHalf;
	invalidated.right = x + m_nodeSizeHalf + 1;
	InvalidateRect(&invalidated, false);
}


void CEffectVis::OpenEditor(QWidget *parent)
{
	Create(IDD_EFFECTVISUALIZER, parent);
	m_forceRedraw = true;

	if(TrackerSettings::Instance().effectVisWidth > 0 && TrackerSettings::Instance().effectVisHeight > 0)
	{
		mfcless::WindowPlacement wnd;
		wnd.length = sizeof(wnd);
		GetWindowPlacement(&wnd);
		wnd.showCmd = mfcless::show::ShowNoActivate;
		mfcless::Rect rect = wnd.rcNormalPosition;
		const auto dpi = HighDPISupport::GetDpiForWindow(this);
		if(TrackerSettings::Instance().effectVisX > int32_min && TrackerSettings::Instance().effectVisY > int32_min)
		{
			mfcless::Rect mainRect;
			CMainFrame::GetMainFrame()->GetWindowRect(mainRect);
			rect.left = mainRect.left + mfcless::MulDiv(TrackerSettings::Instance().effectVisX, dpi, 96);
			rect.top = mainRect.top + mfcless::MulDiv(TrackerSettings::Instance().effectVisY, dpi, 96);
		}
		rect.right = rect.left + mfcless::MulDiv(TrackerSettings::Instance().effectVisWidth, dpi, 96);
		rect.bottom = rect.top + mfcless::MulDiv(TrackerSettings::Instance().effectVisHeight, dpi, 96);
		wnd.rcNormalPosition = rect;
		SetWindowPlacement(&wnd);
	}

	ShowWindow(true);
}


void CEffectVis::OnClose()
{
	DoClose();
}


void CEffectVis::OnOK()
{
	OnClose();
}


void CEffectVis::OnCancel()
{
	OnClose();
}


void CEffectVis::DoClose()
{
	mfcless::WindowPlacement wnd;
	wnd.length = sizeof(wnd);
	GetWindowPlacement(&wnd);
	mfcless::Rect mainRect;
	CMainFrame::GetMainFrame()->GetWindowRect(mainRect);

	mfcless::Rect rect = wnd.rcNormalPosition;
	rect.MoveToXY(rect.left - mainRect.left, rect.top - mainRect.top);
	const auto dpi = HighDPISupport::GetDpiForWindow(this);
	TrackerSettings::Instance().effectVisWidth = mfcless::MulDiv(rect.Width(), 96, dpi);
	TrackerSettings::Instance().effectVisHeight = mfcless::MulDiv(rect.Height(), 96, dpi);
	TrackerSettings::Instance().effectVisX = mfcless::MulDiv(rect.left, 96, dpi);
	TrackerSettings::Instance().effectVisY = mfcless::MulDiv(rect.top, 96, dpi);

	m_dcGrid.SelectObject(m_pbOldGrid);
	m_dcGrid.DeleteDC();
	m_dcNodes.SelectObject(m_pbOldNodes);
	m_dcNodes.DeleteDC();
	m_dcPlayPos.SelectObject(m_pbOldPlayPos);
	m_dcPlayPos.DeleteDC();

	m_bGrid.DeleteObject();
	m_bNodes.DeleteObject();
	m_bPlayPos.DeleteObject();

	DestroyWindow();
}


void CEffectVis::PostNcDestroy()
{
	m_pViewPattern->m_pEffectVis = nullptr;
}


void CEffectVis::OnSize(uint32 nType, int cx, int cy)
{
	MPT_UNREFERENCED_PARAMETER(nType);
	MPT_UNREFERENCED_PARAMETER(cx);
	MPT_UNREFERENCED_PARAMETER(cy);

	m_nodeSizeHalf = HighDPISupport::ScalePixels(3, this);
	m_marginBottom = HighDPISupport::ScalePixels(20, this);
	m_innerBorder = HighDPISupport::ScalePixels(4, this);

	GetClientRect(&m_rcFullWin);
	m_rcDraw.SetRect(m_rcFullWin.left, m_rcFullWin.top, m_rcFullWin.right, m_rcFullWin.bottom - m_marginBottom);

	const int actionListWidth = HighDPISupport::ScalePixels(170, this);
	const int commandListWidth = HighDPISupport::ScalePixels(160, this);

	auto dwp = ::BeginDeferWindowPos(3);
	if (IsWindow(m_edVisStatus))
		::DeferWindowPos(dwp, m_edVisStatus, nullptr, m_rcFullWin.left, m_rcDraw.bottom, m_rcFullWin.right - commandListWidth - actionListWidth, m_rcFullWin.bottom - m_rcDraw.bottom, mfcless::swp::NoActivate | mfcless::swp::ShowWindow | mfcless::swp::NoZOrder);
	if (IsWindow(m_cmbActionList))
		::DeferWindowPos(dwp, m_cmbActionList, nullptr, m_rcFullWin.right - commandListWidth - actionListWidth, m_rcDraw.bottom, actionListWidth, m_rcFullWin.bottom - m_rcDraw.bottom, mfcless::swp::NoActivate | mfcless::swp::ShowWindow | mfcless::swp::NoZOrder);
	if (IsWindow(m_cmbEffectList))
		::DeferWindowPos(dwp, m_cmbEffectList, nullptr, m_rcFullWin.right - commandListWidth, m_rcDraw.bottom, commandListWidth, m_rcFullWin.bottom - m_rcDraw.bottom, mfcless::swp::NoActivate | mfcless::swp::ShowWindow | mfcless::swp::NoZOrder);
	::EndDeferWindowPos(dwp);

	if(m_nRows)
		m_pixelsPerRow = (float)(m_rcDraw.Width() - m_innerBorder * 2) / (float)m_nRows;
	else
		m_pixelsPerRow = 1;
	m_pixelsPerFXParam = (float)(m_rcDraw.Height())/(float)0xFF;
	m_pixelsPerPCParam = (float)(m_rcDraw.Height())/(float)ModCommand::maxColumnValue;
	m_forceRedraw = true;
	InvalidateRect(NULL, false);	//redraw everything
}

void CEffectVis::Update()
{
	DrawNodes();
	if (::IsWindow(this))
	{
		OnPaint();
		if (m_nRowToErase<0)
			InvalidateRect(NULL, false);	// redraw everything
		else
		{
			InvalidateRow(m_nRowToErase);
			m_nParamToErase=-1;
			m_nRowToErase=-1;
		}

	}
}

void CEffectVis::UpdateSelection(ROWINDEX startRow, ROWINDEX endRow, CHANNELINDEX nchn, PATTERNINDEX pat)
{
	m_startRow = startRow;
	m_endRow = endRow;
	m_nRows = endRow - startRow;
	m_nChan = nchn;
	m_nPattern = pat;

	//Check pattern, start row and channel exist
	if(!m_SndFile.Patterns.IsValidPat(m_nPattern) || !m_SndFile.Patterns[m_nPattern].IsValidRow(m_startRow) || m_nChan >= m_SndFile.GetNumChannels())
	{
		DoClose();
		return;
	}

	//Check end exists
	if(!m_SndFile.Patterns[m_nPattern].IsValidRow(m_endRow))
	{
		m_endRow = m_SndFile.Patterns[m_nPattern].GetNumRows() - 1;
		m_nRows = m_endRow - startRow;
	}

	if(m_nDragItem < m_startRow || m_nDragItem > m_endRow)
		m_nDragItem = m_startRow;

	if(m_nRows)
		m_pixelsPerRow = (float)(m_rcDraw.Width() - m_innerBorder * 2) / (float)m_nRows;
	else
		m_pixelsPerRow = 1;
	m_pixelsPerFXParam = (float)(m_rcDraw.Height())/(float)0xFF;
	m_pixelsPerPCParam = (float)(m_rcDraw.Height())/(float)ModCommand::maxColumnValue;

	m_forceRedraw = true;
	Update();

}

void CEffectVis::OnRButtonDown(uint32 nFlags, mfcless::Point point)
{
	if (!(m_dwStatus & FXVSTATUS_LDRAGGING))
	{
		setFocus();
		SetCapture();

		m_nDragItem = ScreenXToRow(point.x);
		m_dwStatus |= FXVSTATUS_RDRAGGING;
		m_ModDoc.GetPatternUndo().PrepareUndo(static_cast<PATTERNINDEX>(m_nPattern), m_nChan, m_nDragItem, 1, 1, "Parameter Editor entry");
		OnMouseMove(nFlags, point);
	}

	DialogBase::OnRButtonDown(nFlags, point);
}

void CEffectVis::OnRButtonUp(uint32 nFlags, mfcless::Point point)
{
	ReleaseCapture();
	m_dwStatus = 0x00;
	DialogBase::OnRButtonUp(nFlags, point);
}

void CEffectVis::OnMouseMove(uint32 nFlags, mfcless::Point point)
{
	DialogBase::OnMouseMove(nFlags, point);

	ROWINDEX row = ScreenXToRow(point.x);

	if(m_dwStatus & FXVSTATUS_RDRAGGING)
	{
		m_nRowToErase = static_cast<int>(m_nDragItem);
		m_nParamToErase = GetParam(m_nDragItem);

		MakeChange(m_nDragItem, point.y);
		DrawNodes();
	} else if(m_dwStatus & FXVSTATUS_LDRAGGING)
	{
		// Interpolate if we detect that rows have been skipped but the left mouse button was not released.
		// This ensures we produce a smooth curve even when we are not notified of mouse movements at a high frequency (e.g. if CPU usage is high)
		const int steps = std::abs((int)row - (int)m_nLastDrawnRow);
		if (m_nLastDrawnRow != ROWINDEX_INVALID && m_nLastDrawnRow > m_startRow && steps > 1)
		{
			int direction = ((int)(row - m_nLastDrawnRow) > 0) ? 1 : -1;
			float factor = (float)(point.y - m_nLastDrawnY)/(float)steps + 0.5f;

			int currentRow;
			for (int i=1; i<=steps; i++)
			{
				currentRow = m_nLastDrawnRow+(direction*i);
				int interpolatedY = mpt::saturate_round<int>(m_nLastDrawnY + ((float)i * factor));
				MakeChange(currentRow, interpolatedY);
			}

			//Don't use single value update
			m_nRowToErase = -1;
			m_nParamToErase = -1;
		} else
		{
			m_nRowToErase = -1;
			m_nParamToErase = -1;
			MakeChange(row, point.y);
		}

		// Remember last modified point in case we need to interpolate
		m_nLastDrawnRow = row;
		m_nLastDrawnY = point.y;
		DrawNodes();
	}

	//update status bar
	mpt::ustring status;
	mpt::ustring effectName;
	uint16 paramValue;

	if (IsPcNote(row))
	{
		paramValue = ScreenYToPCParam(point.y);
		effectName = mfcless::PrintF(UL_("%s"), UL_("Param Control")); // TODO - show smooth & plug+param
	} else
	{
		paramValue = ScreenYToFXParam(point.y);
		m_effectInfo.GetEffectInfo(m_effectInfo.GetIndexFromEffect(GetCommand(row), ModCommand::PARAM(GetParam(row))), &effectName, true);
	}

	status = mfcless::PrintF(UL_("Pat: %d\tChn: %d\tRow: %d\tVal: %02X (%03d) [%s]"),
				m_nPattern, m_nChan+1, static_cast<signed int>(row), paramValue, paramValue, effectName.c_str());
	mfcless::SetWindowText(m_edVisStatus, status);
}

void CEffectVis::OnLButtonDown(uint32 nFlags, mfcless::Point point)
{
	if (!(m_dwStatus & FXVSTATUS_RDRAGGING))
	{
		setFocus();
		SetCapture();

		m_nDragItem = ScreenXToRow(point.x);
		m_dwStatus |= FXVSTATUS_LDRAGGING;
		m_ModDoc.GetPatternUndo().PrepareUndo(static_cast<PATTERNINDEX>(m_nPattern), m_nChan, m_startRow, 1, m_endRow - m_startRow + 1, "Parameter Editor entry");
		OnMouseMove(nFlags, point);
	}

	DialogBase::OnLButtonDown(nFlags, point);
}

void CEffectVis::OnLButtonUp(uint32 nFlags, mfcless::Point point)
{
	ReleaseCapture();
	m_dwStatus = 0x00;
	DialogBase::OnLButtonUp(nFlags, point);
	m_nLastDrawnRow = ROWINDEX_INVALID;
}


bool CEffectVis::OnInitDialog()
{
	DialogBase::OnInitDialog();

	// If first selected row is a PC event (or some other row but there aren't any other effects), default to PC note overwrite mode
	// and use it as a template for new PC notes that will be created via the visualiser.
	bool isPCevent = IsPcNote(m_startRow);
	ROWINDEX templatePCRow = m_startRow;
	if(!isPCevent)
	{
		for(ROWINDEX row = m_startRow; row <= m_endRow; row++)
		{
			if(IsPcNote(row))
			{
				isPCevent = true;
				templatePCRow = row;
			} else if(GetCommand(row) != CMD_NONE)
			{
				isPCevent = false;
				break;
			}
		}
	}

	if(isPCevent)
	{
		m_nAction = Action::OverwritePC;
		if(m_SndFile.Patterns.IsValidPat(m_nPattern))
		{
			ModCommand &m = *m_SndFile.Patterns[m_nPattern].GetpModCommand(templatePCRow, m_nChan);
			m_templatePCNote.Set(m.note, m.instr, m.GetValueVolCol(), 0);
		}
	} else
	{
		// Otherwise, default to FX overwrite and
		// use effect of first selected row as default effect type
		m_nAction = Action::OverwriteFX;
		m_nFillEffect = m_effectInfo.GetIndexFromEffect(GetCommand(m_startRow), ModCommand::PARAM(GetParam(m_startRow)));
		if(m_nFillEffect < 0 || m_nFillEffect >= MAX_EFFECTS)
			m_nFillEffect = m_effectInfo.GetIndexFromEffect(CMD_SMOOTHMIDI, 0);
	}


	UpdateEffectList();

	m_cmbActionList->clear();
	mfcless::SetItemData(m_cmbActionList, mfcless::AddItem(m_cmbActionList, UL_("Overwrite with effect:")), static_cast<uintptr_t>(Action::OverwriteFX));
	mfcless::SetItemData(m_cmbActionList, mfcless::AddItem(m_cmbActionList, UL_("Overwrite effect next to note:")), static_cast<uintptr_t>(Action::OverwriteFXWithNote));
	mfcless::SetItemData(m_cmbActionList, mfcless::AddItem(m_cmbActionList, UL_("Fill blanks with effect:")), static_cast<uintptr_t>(Action::FillFX));
	if(m_ModDoc.GetModType() == MOD_TYPE_MPT || isPCevent)
	{
		mfcless::SetItemData(m_cmbActionList, mfcless::AddItem(m_cmbActionList, UL_("Overwrite with PC note")), static_cast<uintptr_t>(Action::OverwritePC));
		mfcless::SetItemData(m_cmbActionList, mfcless::AddItem(m_cmbActionList, UL_("Fill blanks with PC note")), static_cast<uintptr_t>(Action::FillPC));
	}
	mfcless::SetItemData(m_cmbActionList, mfcless::AddItem(m_cmbActionList, UL_("Never change effect type")), static_cast<uintptr_t>(Action::Preserve));

	m_cmbActionList->setCurrentIndex(static_cast<int>(m_nAction));
	return true;
}


void CEffectVis::UpdateEffectList()
{
	const bool fillPlugParams = m_nAction == Action::FillPC || m_nAction == Action::OverwritePC;
	mfcless::Rect rect{0, 0, 16, HighDPISupport::ScalePixels(16, this)};
	if(m_cmbEffectList)
	{
		m_cmbEffectList->GetWindowRect(rect);
		ScreenToClient(rect);
		m_cmbEffectList->DestroyWindow();
	}
	// Can't change CBS_SORT style after creation...
	m_cmbEffectList->Create(CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP | WS_VISIBLE | (fillPlugParams ? 0 : CBS_SORT), rect, this, IDC_VISEFFECTLIST);
	m_cmbEffectList->setUpdatesEnabled(false);
	m_cmbEffectList->SetFont(GetFont(), false);
	mpt::ustring s;
	if(fillPlugParams)
	{
		IMixPlugin *plugin = nullptr;
		if(m_templatePCNote.instr > 0 && m_templatePCNote.instr <= MAX_MIXPLUGINS)
			plugin = m_SndFile.m_MixPlugins[m_templatePCNote.instr - 1].pMixPlugin;

		if(plugin)
		{
			AddPluginParameternamesToCombobox(m_cmbEffectList, *plugin);
		} else
		{
			for(PlugParamIndex i = 0; i < ModCommand::maxColumnValue; i++)
			{
				s = mfcless::PrintF(UL_("Parameter %u"), i);
				mfcless::SetItemData(m_cmbEffectList, mfcless::AddItem(m_cmbEffectList, s), i);
			}
		}
		m_cmbEffectList->setCurrentIndex(m_templatePCNote.GetValueVolCol());
	} else
	{
		const uint32 numfx = m_effectInfo.GetNumEffects();
		for(uint32 i = 0; i < numfx; i++)
		{
			if(m_effectInfo.GetEffectInfo(i, &s, true))
			{
				int k = mfcless::AddItem(m_cmbEffectList, s);
				mfcless::SetItemData(m_cmbEffectList, k, i);
				if(static_cast<int>(i) == m_nFillEffect)
					m_cmbEffectList->setCurrentIndex(k);
			}
		}
	}
	m_cmbEffectList->setUpdatesEnabled(true);
	m_cmbEffectList->update();
}

void CEffectVis::MakeChange(ROWINDEX row, int y)
{
	if(!m_SndFile.Patterns.IsValidPat(m_nPattern))
		return;

	ModCommand &m = *m_SndFile.Patterns[m_nPattern].GetpModCommand(row, m_nChan);

	switch(m_nAction)
	{
		case Action::FillFX:
			// Only set command if there isn't a command already at this row and it's not a PC note
			if (GetCommand(row) == CMD_NONE && !IsPcNote(row))
			{
				SetCommand(row, m_effectInfo.GetEffectFromIndex(m_nFillEffect));
			}
			// Always set param
			SetParamFromY(row, y);
			break;

		case Action::OverwriteFXWithNote:
			if(!m_SndFile.Patterns.IsValidPat(m_nPattern))
				break;
			if(!m_SndFile.Patterns[m_nPattern].GetpModCommand(row, m_nChan)->IsNote())
				break;
			[[fallthrough]];
		case Action::OverwriteFX:
			// Always set command and param. Blows away any PC notes.
			SetCommand(row, m_effectInfo.GetEffectFromIndex(m_nFillEffect));
			SetParamFromY(row, y);
			break;

		case Action::FillPC:
			// Fill only empty slots with PC notes - leave other slots alone.
			if (m.empty())
			{
				SetPcNote(row);
			}
			// Always set param
			SetParamFromY(row, y);
			break;

		case Action::OverwritePC:
			// Always convert to PC Note and set param value
			SetPcNote(row);
			SetParamFromY(row, y);
			break;

		case Action::Preserve:
			if (GetCommand(row) != CMD_NONE || IsPcNote(row))
			{
				// Only set param if we have an effect type or if this is a PC note.
				// Never change the effect type.
				SetParamFromY(row, y);
			}
			break;

	}

	m_ModDoc.SetModified();
	m_ModDoc.UpdateAllViews(nullptr, RowHint(row), this);
	InvalidateRow(row);  // Only invalidate, actual redrawing is triggered in OnMouseMove as we may modify multiple rows with one mouse event
}

void CEffectVis::SetPcNote(ROWINDEX row)
{
	if(!m_SndFile.Patterns.IsValidPat(m_nPattern))
		return;

	ModCommand &m = *m_SndFile.Patterns[m_nPattern].GetpModCommand(row, m_nChan);
	m.Set(m_templatePCNote.note, m_templatePCNote.instr, m_templatePCNote.GetValueVolCol(), 0);
}

bool CEffectVis::IsPcNote(ROWINDEX row) const
{
	if(m_SndFile.Patterns.IsValidPat(m_nPattern))
		return m_SndFile.Patterns[m_nPattern].GetpModCommand(row, m_nChan)->IsPcNote();
	else
		return false;
}


OPENMPT_NAMESPACE_END
