/*
 * Ctrl_pat.h
 * ----------
 * Purpose: Pattern tab, upper panel.
 * Notes  : mfcless twin of upstream's mptrack/Ctrl_pat.h.
 *          Draft from qt/tools/twin.py, not yet edited by hand.
 * Authors: Olivier Lapicque
 *          OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "mfcless/Controls.h"
#include "mfcless/Graphics.h"
#include "mfcless/Window.h"
#include "DocumentView.h"
#include <QtWidgets/QWidget>

#include "AccessibleControls.h"
#include "Globals.h"

class QComboBox;
class QFont;
class QLineEdit;
class QObject;
class QScrollBar;
class QString;

OPENMPT_NAMESPACE_BEGIN

class COrderList;
class CCtrlPatterns;
class ModSequence;

struct OrdSelection
{
	ORDERINDEX firstOrd = 0, lastOrd = 0;
	ORDERINDEX GetSelCount() const { return lastOrd - firstOrd + 1; }
};

class COrderList: public mfcless::Window<QWidget>
{
	Q_OBJECT
	friend class CCtrlPatterns;
protected:
	QFont m_hFont;
	int m_cxFont = 0, m_cyFont = 0;

	CModDoc &m_modDoc;
	CCtrlPatterns &m_pParent;

	ORDERINDEX m_nXScroll = 0;                        // Index of the leftmost displayed order list item
	ORDERINDEX m_nScrollPos = 0;                      // First selection point / selected order
	ORDERINDEX m_nScrollPos2nd = ORDERINDEX_INVALID;  // 2nd selection point if multiple orders are selected (not neccessarily the higher order - GetCurSel() is taking care of that.)
	ORDERINDEX m_nDropPos = ORDERINDEX_INVALID, m_nMouseDownPos = ORDERINDEX_INVALID, m_playPos = ORDERINDEX_INVALID;
	ORDERINDEX m_nDragOrder = ORDERINDEX_INVALID, m_menuOrder = ORDERINDEX_INVALID;
	//To tell how many orders('orderboxes') to show at least
	//on both sides of current order(when updating orderslist position).
	int m_nOrderlistMargins = 0;
	bool m_bScrolling = false, m_bDragging = false;

public:
	COrderList(CCtrlPatterns &parent, CModDoc &document);

public:
	bool Init(const mfcless::Rect &);
	void UpdateView(UpdateHint hint, QObject *pObj = nullptr);
	void InvalidateSelection();
	PATTERNINDEX GetCurrentPattern() const;
	// make the current selection the secondary selection (used for keyboard orderlist navigation)
	inline void SetCurSelTo2ndSel(bool isSelectionKeyPressed)
	{
		if(isSelectionKeyPressed && m_nScrollPos2nd == ORDERINDEX_INVALID)
			m_nScrollPos2nd = m_nScrollPos;
		else if(!isSelectionKeyPressed && m_nScrollPos2nd != ORDERINDEX_INVALID)
			m_nScrollPos2nd = ORDERINDEX_INVALID;
	};
	void SetSelection(ORDERINDEX firstOrd, ORDERINDEX lastOrd = ORDERINDEX_INVALID);
	// Why VC wants to inline this huge function is beyond my understanding...
	MPT_NOINLINE bool SetCurSel(ORDERINDEX sel, bool setPlayPos = true, bool shiftClick = false, bool ignoreCurSel = false);
	void UpdateScrollInfo();
	void UpdateInfoText();
	int GetFontWidth();

	void QueuePattern(ORDERINDEX order, OrderTransitionMode transitionMode);

	// Check if this module is currently playing
	bool IsPlaying() const;

	ORDERINDEX GetOrderFromPoint(const mfcless::Point &pt) const;
	mfcless::Rect GetRectFromOrder(ORDERINDEX ord) const;

	// Get the currently selected pattern(s).
	// Set ignoreSelection to true if only the first selected point is important.
	OrdSelection GetCurSel(bool ignoreSelection = false) const;

	// Sets target margin value and returns the effective margin value.
	ORDERINDEX SetMargins(int);

	// Returns the effective margin value.
	ORDERINDEX GetMargins() { return GetMargins(GetMarginsMax()); }

	// Returns the effective margin value.
	ORDERINDEX GetMargins(const ORDERINDEX maxMargins) const { return std::min(maxMargins, static_cast<ORDERINDEX>(m_nOrderlistMargins)); }

	// Returns maximum margin value given current window width.
	ORDERINDEX GetMarginsMax() { return GetMarginsMax(GetLength()); }

	// Returns maximum margin value when shown sequence has nLength orders.
	// For example: If length is 4 orders -> maxMargins = 4/2 - 1 = 1;
	// if maximum is 5 -> maxMargins = (int)5/2 = 2
	ORDERINDEX GetMarginsMax(const ORDERINDEX length) const { return (length > 0 && length % 2 == 0) ? length / 2 - 1 : length / 2; }

	// Returns the number of sequence items visible in the list.
	ORDERINDEX GetLength();

	// Return true if given order is in margins given that first shown order
	// is 'startOrder'. Begin part of the whole sequence
	// is not interpreted to be in margins regardless of the margin value.
	bool IsOrderInMargins(int order, int startOrder);

	// Ensure that a given order index is visible in the orderlist view.
	void EnsureVisible(ORDERINDEX order);

	// Set given sqeuence and update orderlist display.
	void SelectSequence(const SEQUENCEINDEX nSeq);

	// Helper function for entering pattern number
	void EnterPatternNum(int enterNum);

	void OnCopy(bool onlyOrders);

	// Update play state and order lock ranges after inserting order items.
	void InsertUpdatePlaystate(ORDERINDEX first, ORDERINDEX last);
	// Update play state and order lock ranges after deleting order items.
	void DeleteUpdatePlaystate(ORDERINDEX first, ORDERINDEX last);
	bool PreTranslateMessage(mfcless::Message *pMsg) override;
	intptr_t OnToolHitTest(mfcless::Point point, mfcless::ToolInfo* pTI) const override;
	QString GetAccessibleName() override;
protected:
	ModSequence& Order();
	const ModSequence& Order() const;

	void SetScrollPos(int pos);
	int GetScrollPos(bool getTrackPos = false);

	// Resizes the order list if the specified order is past the order list length
	bool EnsureEditable(ORDERINDEX ord);
	intptr_t OnDPIChangedAfterParent(uintptr_t, intptr_t);
	void OnPaint();
	bool OnEraseBkgnd(mfcless::DC *) { return true; }
	void OnSetFocus(QWidget *);
	void OnKillFocus(QWidget *);
	void OnLButtonDown(uint32, mfcless::Point);
	void OnLButtonDblClk(uint32, mfcless::Point);
	void OnRButtonUp(uint32, mfcless::Point);
	void OnLButtonUp(uint32, mfcless::Point);
	void OnMButtonDown(uint32, mfcless::Point);
	void OnMouseMove(uint32, mfcless::Point);
	void OnHScroll(uint32 nSBCode, uint32 nPos, QScrollBar *);
	void OnSize(uint32 nType, int cx, int cy);
	void OnSwitchToView();
	void OnInsertOrder();
	void OnInsertSeparatorPattern();
	void OnDeleteOrder();
	void OnRenderOrder();
	void OnPatternProperties();
	void OnPlayerPlay();
	void OnPlayerPause();
	void OnPlayerPlayFromStart();
	void OnPatternPlayFromStart();
	void OnCreateNewPattern();
	void OnDuplicatePattern();
	void OnMergePatterns();
	void OnPatternCopy();
	void OnPatternPaste();
	void OnSetRestartPos();
	void OnEditCopy() { OnCopy(false); }
	void OnEditCopyOrders() { OnCopy(true); }
	void OnEditCut();
	intptr_t OnDragonDropping(uintptr_t bDoDrop, intptr_t lParam);
	void OnSelectSequence(uint32 nid);
	intptr_t OnCustomKeyMsg(uintptr_t, intptr_t);
	void OnLockPlayback();
	void OnUnlockPlayback();
	void OnQueueAtPatternEnd() { QueuePattern(m_menuOrder, OrderTransitionMode::AtPatternEnd); }
	void OnQueueAtMeasureEnd() { QueuePattern(m_menuOrder, OrderTransitionMode::AtMeasureEnd); }
	void OnQueueAtBeatEnd() { QueuePattern(m_menuOrder, OrderTransitionMode::AtBeatEnd); }
	void OnQueueAtRowEnd() { QueuePattern(m_menuOrder, OrderTransitionMode::AtRowEnd); }
	bool OnToolTipText(uint32 );
	void RegisterMessageMap() override;
};


class CCtrlPatterns: public CModControlDlg
{
	Q_OBJECT
	friend class COrderList;
protected:
	COrderList *m_OrderList = nullptr;
	AccessibleButton *m_BtnPrev = nullptr, *m_BtnNext = nullptr;
	QComboBox *m_CbnInstrument = nullptr;
	QLineEdit *m_EditSpacing = nullptr, *m_EditPatName = nullptr, *m_EditSequence = nullptr;
	mfcless::UpDown *m_SpinInstrument = nullptr, *m_SpinSpacing = nullptr, *m_SpinSequence = nullptr;
	CModControlBar *m_ToolBar = nullptr;
	INSTRUMENTINDEX m_nInstrument = 0;
	bool m_bRecord = false, m_bVUMeters = false, m_bPluginNames = false;
	bool m_instrDropdownOpen = false;

public:
	CCtrlPatterns(CModControlView &parent, CModDoc &document);

public:
	const ModSequence &Order() const;
	ModSequence &Order();

	void SetCurrentPattern(PATTERNINDEX nPat);
	bool SetCurrentInstrument(uint32 nIns);
	bool GetFollowSong() const;
	bool GetLoopPattern() const;
	COrderList &GetOrderList() { return *m_OrderList; }
	Setting<int32> &GetSplitPosRef() override;
	bool OnInitDialog() override;
	void DoDataExchange(DataExchange &dx) override;	// DDX/DDV support
	void RecalcLayout() override;
	void UpdateView(UpdateHint hint = UpdateHint(), QObject *pObj = nullptr) override;
	const mfcless::ViewClass *GetAssociatedViewClass() override;
	intptr_t OnModCtrlMsg(uintptr_t wParam, intptr_t lParam) override;
	void OnActivatePage(intptr_t) override;
	void OnDeactivatePage() override;
	mpt::ustring GetToolTipText(uint32 id, QWidget * hwnd) const override;
	void OnDPIChanged() override;
protected:
	void OnVScroll(uint32 nSBCode, uint32 nPos, QScrollBar *pScrollBar);
	void OnTbnDropDownToolBar();
	void OnSequenceNext();
	void OnSequencePrev();
	void OnChannelManager();
	void OnKeyDown(uint32 nChar, uint32 nRepCnt, uint32 nFlags);
	void OnPlayerPause();
	void OnPatternNew();
	void OnPatternDuplicate();
	void OnPatternMerge();
	void OnPatternStop();
	void OnPatternPlay();
	void OnPatternPlayRow();
	void OnPatternPlayFromStart();
	void OnPatternRecord();
	void OnPatternVUMeters();
	void OnPatternViewPlugNames();
	void OnPatternProperties();
	void OnPatternExpand();
	void OnPatternShrink();
	void OnPatternAmplify();
	void OnPatternPaste();
	void OnToggleMetronome();
	void OnMetronomeSettings();
	void OnFollowSong();
	void OnChangeLoopStatus();
	// cppcheck-suppress duplInheritedMember
	void OnSwitchToView();
	void OnInstrumentChanged();
	void OnPrevInstrument();
	void OnNextInstrument();
	void OnSpacingChanged();
	void OnPatternNameChanged();
	void OnSequenceNameChanged();
	void OnChordEditor();
	void OnDetailSwitch();
	void OnDetailInstr();
	void OnDetailVolume();
	void OnDetailEffect();
	void OnEditUndo();
	void OnUpdateRecord(mfcless::CommandState &state);
	void TogglePluginEditor();
	void OnToggleOverflowPaste();
	void OnSequenceNumChanged();
	void OnOpenInstrumentDropdown() { m_instrDropdownOpen = true; }
	void OnCancelInstrumentDropdown() { m_instrDropdownOpen = false; }
	void RegisterMessageMap() override;
private:
	bool HasValidPlug(INSTRUMENTINDEX instr) const;
public:
	bool OnMouseWheel(uint32 nFlags, short zDelta, mfcless::Point pt);
	void OnXButtonUp(uint32 nFlags, uint32 nButton, mfcless::Point point);
};

OPENMPT_NAMESPACE_END
