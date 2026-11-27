/*
 * Ctrl_ins.h
 * ----------
 * Purpose: Instrument tab, upper panel.
 * Notes  : mfcless twin of upstream's mptrack/Ctrl_ins.h.
 *          Draft from qt/tools/twin.py, not yet edited by hand.
 * Authors: Olivier Lapicque
 *          OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "mfcless/Controls.h"
#include "mfcless/MessageMap.h"
#include <QtWidgets/QLabel>

#include "CDecimalSupport.h"
#include "Globals.h"
#include "PluginComboBox.h"
#include "../soundlib/modcommand.h"

class QAbstractButton;
class QComboBox;
class QEvent;
class QLineEdit;
class QObject;
class QPoint;
class QScrollBar;
class QSlider;
class QWidget;

OPENMPT_NAMESPACE_BEGIN

class CModDoc;
class CNoteMapWnd;
class CCtrlInstruments;

class CNoteMapWnd: public QLabel, public mfcless::MessageMap
{
	Q_OBJECT
protected:
	CModDoc &m_modDoc;
	CCtrlInstruments &m_pParent;
	uint32 m_nNote = (NOTE_MIDDLEC - NOTE_MIN), m_nOldNote = 0, m_nOldIns = 0;
	INSTRUMENTINDEX m_nInstrument = 0;
	int m_cxFont = 0, m_cyFont = 0;
	CHANNELINDEX m_noteChannel = 0;
	ModCommand::NOTE m_nPlayingNote = NOTE_NONE;

	bool m_bIns = false;
	bool m_undo = true;

private:
	void MapTranspose(int nAmount);
	void PrepareUndo(const char *description);

public:
	CNoteMapWnd(CCtrlInstruments &parent, CModDoc &document)
		: m_modDoc(document)
		, m_pParent(parent)
	{
		EnableActiveAccessibility();
	}
	void SetCurrentInstrument(INSTRUMENTINDEX nIns);
	void SetCurrentNote(uint32 nNote);
	void EnterNote(uint32 note);
	bool HandleChar(uintptr_t c);
	bool HandleNav(uintptr_t k);
	void PlayNote(uint32 note);
	void StopNote();

	void UpdateAccessibleTitle();

public:
	bool eventFilter(QObject *watched, QEvent *e) override;
	HRESULT get_accName(VARIANT varChild, BSTR *pszName) override;
protected:
	void OnLButtonDown(uint32, QPoint);
	void OnMButtonDown(uint32 flags, QPoint pt) { OnLButtonDown(flags, pt); }
	void OnRButtonUp(uint32, QPoint);
	void OnLButtonDblClk(uint32, QPoint);
	bool OnMouseWheel(uint32 nFlags, short zDelta, QPoint pt);
	void OnSetFocus(QWidget *pOldWnd);
	void OnKillFocus(QWidget *pNewWnd);
	bool OnEraseBkGnd(CDC *) { return true; }
	void OnPaint();
	void OnMapCopySample();
	void OnMapCopyNote();
	void OnMapTransposeUp();
	void OnMapTransposeDown();
	void OnMapReset();
	void OnTransposeSamples();
	void OnMapRemove();
	void OnEditSample(uint32 nID);
	void OnEditSampleMap();
	void OnInstrumentDuplicate();
	intptr_t OnCustomKeyMsg(uintptr_t, intptr_t);
	intptr_t OnDPIChangedAfterParent(uintptr_t, intptr_t);
	void RegisterMessageMap() override;
};


class CCtrlInstruments: public CModControlDlg
{
	Q_OBJECT
protected:
	CModControlBar m_ToolBar;
	mfcless::UpDown *m_SpinInstrument = nullptr, *m_SpinFadeOut = nullptr, *m_SpinGlobalVol = nullptr, *m_SpinPanning = nullptr;
	mfcless::UpDown *m_SpinMidiPR = nullptr, *m_SpinPPS = nullptr, *m_SpinMidiBK = nullptr, *m_SpinPWD = nullptr;
	QComboBox *m_ComboNNA = nullptr, *m_ComboDCT = nullptr, *m_ComboDCA = nullptr, *m_ComboPPC = nullptr, *m_CbnMidiCh = nullptr, *m_CbnResampling = nullptr, *m_CbnFilterMode = nullptr, *m_CbnPluginVolumeHandling = nullptr;
	PluginComboBox m_CbnMixPlug;
	QLineEdit *m_EditName = nullptr, *m_EditFileName = nullptr, *m_EditGlobalVol = nullptr, *m_EditPanning = nullptr, *m_EditFadeOut = nullptr;
	CNumberEdit m_EditPPS, m_EditPWD;
	QAbstractButton *m_CheckPanning = nullptr, *m_CheckCutOff = nullptr, *m_CheckResonance = nullptr, *velocityStyle = nullptr;
	QSlider *m_SliderVolSwing = nullptr, *m_SliderPanSwing = nullptr, *m_SliderCutSwing = nullptr, *m_SliderResSwing = nullptr, *m_SliderCutOff = nullptr, *m_SliderResonance = nullptr;
	CNoteMapWnd m_NoteMap;
	QSlider *m_SliderAttack = nullptr;
	mfcless::UpDown *m_SpinAttack = nullptr;
	//Tuning
	QComboBox *m_ComboTuning = nullptr;
	// Pitch/Tempo lock
	CNumberEdit m_EditPitchTempoLock;
	QAbstractButton *m_CheckPitchTempoLock = nullptr;

	INSTRUMENTINDEX m_nInstrument = 1;
	bool m_openendPluginListWithMouse = false;
	bool m_startedHScroll = false;
	bool m_startedEdit = false;

	void UpdateTuningComboBox();
	void BuildTuningComboBox();

public:
	CCtrlInstruments(CModControlView &parent, CModDoc &document);

public:
	void SetModified(InstrumentHint hint, bool updateAll);
	bool SetCurrentInstrument(uint32 nIns, bool bUpdNum=true);
	bool InsertInstrument(bool duplicate);
	bool OpenInstrument(const mpt::PathString &fileName);
	bool OpenInstrument(const CSoundFile &sndFile, INSTRUMENTINDEX nInstr);
	void SaveInstrument(bool doBatchSave);
	bool EditSample(uint32 nSample);
	void UpdateFilterText();

public:
	Setting<int32> &GetSplitPosRef() override;
	bool OnInitDialog() override;
	void OnDPIChanged() override;
	void DoDataExchange(DataExchange &dx) override;	// DDX/DDV support
	CRuntimeClass *GetAssociatedViewClass() override;
	void RecalcLayout() override;
	void OnActivatePage(intptr_t) override;
	void OnDeactivatePage() override;
	void UpdateView(UpdateHint hint, QObject *pObj = nullptr) override;
	intptr_t OnModCtrlMsg(uintptr_t wParam, intptr_t lParam) override;
	mpt::ustring GetToolTipText(uint32 uId, QWidget * hwnd) const override;
	bool eventFilter(QObject *watched, QEvent *e) override;
protected:
	void PrepareUndo(const char *description);
	void OnEditFocus();
	void OnVScroll(uint32 nCode, uint32 nPos, QScrollBar *pSB);
	void OnHScroll(uint32 nCode, uint32 nPos, QScrollBar *pSB);
	void OnTbnDropDownToolBar();
	void OnInstrumentChanged();
	void OnPrevInstrument();
	void OnNextInstrument();
	void OnInstrumentNew();
	void OnInstrumentDuplicate() { InsertInstrument(true); }
	void OnInstrumentOpen();
	void OnInstrumentSave();
	void OnInstrumentSaveOne() { SaveInstrument(false); }
	void OnInstrumentSaveAll() { SaveInstrument(true); }
	void OnInstrumentPlay();
	void OnNameChanged();
	void OnFileNameChanged();
	void OnFadeOutVolChanged();
	void OnGlobalVolChanged();
	void OnSetPanningChanged();
	void OnPanningChanged();
	void OnNNAChanged();
	void OnDCTChanged();
	void OnDCAChanged();
	void OnMPRChanged();
	void OnMPRKillFocus();
	void OnMBKChanged();
	void OnMCHChanged();
	void OnResamplingChanged();
	void OnMixPlugChanged();
	void OnPPSChanged();
	void OnPPCChanged();
	void OnFilterModeChanged();
	void OnPluginVelocityHandlingChanged();
	void OnPluginVolumeHandlingChanged();
	void OnPitchWheelDepthChanged();
	void OnOpenPluginList() { m_openendPluginListWithMouse = true; }
	void OnAttackChanged();
	void OnEnableCutOff();
	void OnEnableResonance();
	void OnEditSampleMap();
	void TogglePluginEditor();
	intptr_t OnCustomKeyMsg(uintptr_t, intptr_t);
	void OnCbnSelchangeCombotuning();
	void OnEnChangeEditPitchTempoLock();
	void OnBnClickedCheckPitchtempolock();
	void OnEnKillFocusEditPitchTempoLock();
	void OnEnKillFocusEditFadeOut();
	void OnXButtonUp(uint32 nFlags, uint32 nButton, QPoint point);
	void RegisterMessageMap() override;
};

OPENMPT_NAMESPACE_END
