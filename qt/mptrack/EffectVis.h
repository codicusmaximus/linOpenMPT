/*
 * EffectVis.h
 * -----------
 * Purpose: Implementation of parameter visualisation dialog.
 * Notes  : mfcless twin of upstream's mptrack/EffectVis.h.
 *          Hand-edited from the twin.py draft.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "mfcless/Graphics.h"

#include "DialogBase.h"
#include "EffectInfo.h"

class QComboBox;
class QLineEdit;
class QWidget;

OPENMPT_NAMESPACE_BEGIN

class CViewPattern;
class CModDoc;
class CSoundFile;

// EffectVis dialog
class CEffectVis : public DialogBase
{
	Q_OBJECT
public:
	enum class Action
	{
		OverwriteFX,
		OverwriteFXWithNote,
		FillFX,
		OverwritePC,
		FillPC,
		Preserve
	};

	CEffectVis(CViewPattern *pViewPattern, ROWINDEX startRow, ROWINDEX endRow, CHANNELINDEX nchn, CModDoc &modDoc, PATTERNINDEX pat);

	void UpdateSelection(ROWINDEX startRow, ROWINDEX endRow, CHANNELINDEX nchn, PATTERNINDEX pat);
	void Update();
	void OpenEditor(QWidget *parent);
	void SetPlayCursor(PATTERNINDEX nPat, ROWINDEX nRow);
	void DoClose();

	void OnSize(uint32 nType, int cx, int cy);

protected:
	bool OnInitDialog() override;
	void OnOK() override;
	void OnCancel() override;
	void DoDataExchange(DataExchange &dx) override;  // DDX/DDV support
	void PostNcDestroy() override;

	void UpdateEffectList();
	void MakeChange(ROWINDEX currentRow, int newY);
	
	void DrawNodes();
	void DrawGrid();
	void ShowVis(mfcless::DC *pDC);
	void ShowVisImage(mfcless::DC *pDC);
	void InvalidateRow(int row);
	
	std::pair<ROWINDEX, ROWINDEX> GetTimeSignature() const;
	int RowToScreenX(ROWINDEX row) const;
	int RowToScreenY(ROWINDEX row) const;
	int PCParamToScreenY(uint16 param) const;
	int FXParamToScreenY(uint16 param) const;
	uint16 GetParam(ROWINDEX row) const;
	EffectCommand GetCommand(ROWINDEX row) const;
	void SetParamFromY(ROWINDEX row, int y);
	void SetCommand(ROWINDEX row, EffectCommand cmd);
	ModCommand::PARAM ScreenYToFXParam(int y) const;
	uint16 ScreenYToPCParam(int y) const;
	ROWINDEX ScreenXToRow(int x) const;
	bool IsPcNote(ROWINDEX row) const;
	void SetPcNote(ROWINDEX row);

	void OnClose();
	void OnPaint();
	void OnLButtonDown(uint32 nFlags, mfcless::Point point);
	void OnLButtonUp(uint32 nFlags, mfcless::Point point);
	void OnMouseMove(uint32 nFlags, mfcless::Point point);
	void OnRButtonDown(uint32 nFlags, mfcless::Point point);
	void OnRButtonUp(uint32 nFlags, mfcless::Point point);
	void OnEffectChanged();
	void OnActionChanged();
	bool OnEraseBkgnd(mfcless::DC *) { return true; }

	void RegisterMessageMap() override;

protected:
	CModDoc &m_ModDoc;
	CSoundFile &m_SndFile;
	CViewPattern *m_pViewPattern = nullptr;
	mfcless::Rect m_rcDraw;
	mfcless::Rect m_rcFullWin;

	QComboBox *m_cmbEffectList = nullptr, *m_cmbActionList = nullptr;
	QLineEdit *m_edVisStatus = nullptr;
	mfcless::Bitmap m_bGrid, m_bNodes, m_bPlayPos;
	mfcless::Bitmap *m_pbOldGrid = nullptr, *m_pbOldNodes = nullptr, *m_pbOldPlayPos = nullptr;
	mfcless::DC m_dcGrid, m_dcNodes, m_dcPlayPos;

	EffectInfo m_effectInfo;

	ROWINDEX m_nLastDrawnRow = ROWINDEX_INVALID;  // for interpolation
	int m_nLastDrawnY = -1;                       // for interpolation
	int m_nRowToErase = -1;
	int m_nParamToErase = -1;

	int m_nodeSizeHalf;  // Half width of a node
	int m_marginBottom;
	int m_innerBorder;

	ROWINDEX m_nOldPlayPos = ROWINDEX_INVALID;
	ModCommand m_templatePCNote;

	ROWINDEX m_startRow;
	ROWINDEX m_endRow;
	ROWINDEX m_nRows;
	CHANNELINDEX m_nChan;
	PATTERNINDEX m_nPattern;
	int m_nFillEffect;
	static Action m_nAction;

	ROWINDEX m_nDragItem = 0;
	uint32 m_nBtnMouseOver;
	
	enum
	{
		FXVSTATUS_LDRAGGING = 0x01,
		FXVSTATUS_RDRAGGING = 0x02,
	};
	uint32 m_dwStatus = 0;

	float m_pixelsPerRow = 1, m_pixelsPerFXParam = 1, m_pixelsPerPCParam = 1;

	bool m_forceRedraw = true;
};

OPENMPT_NAMESPACE_END
