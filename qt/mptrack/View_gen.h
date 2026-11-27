/*
 * view_gen.h
 * ----------
 * Purpose: General tab, lower panel.
 * Notes  : mfcless twin of upstream's mptrack/View_gen.h.
 *          Hand-edited from the twin.py draft.
 * Authors: Olivier Lapicque
 *          OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "DialogBase.h"
#include "mfcless/Controls.h"
#include "mfcless/Geometry.h"
#include "DocumentView.h"
#include <QtCore/QPointer>
#include "AccessibleControls.h"
#include "ColorPickerButton.h"
#include "PluginComboBox.h"
#include "UpdateHints.h"

class QAbstractButton;
class QComboBox;
class QObject;
class QScrollArea;
class QScrollBar;
class QSlider;

OPENMPT_NAMESPACE_BEGIN

class CChildFrame;

//Note: Changing this won't increase the number of tabs in general view. Most
//of the code use plain number 4.
#define CHANNELS_IN_TAB	4

class CModDoc;
class IMixPlugin;

class CViewGlobals: public DialogBase, public mfcless::DocumentView
{
	Q_OBJECT
protected:
	mfcless::Rect m_rcClient;
	mfcless::TabControl *m_TabCtrl = nullptr;
	PluginComboBox *m_CbnEffects[CHANNELS_IN_TAB] = {};
	PluginComboBox *m_CbnPlugin = nullptr;
	QComboBox *m_CbnParam = nullptr, *m_CbnOutput = nullptr;

	QSlider *m_sbVolume[CHANNELS_IN_TAB] = {}, *m_sbPan[CHANNELS_IN_TAB] = {}, *m_sbValue = nullptr, *m_sbDryRatio = nullptr;
	ColorPickerButton *m_channelColor[CHANNELS_IN_TAB] = {};

	QComboBox *m_CbnPreset = nullptr;
	QSlider *m_sbWetDry = nullptr;
	mfcless::UpDown *m_spinVolume[CHANNELS_IN_TAB] = {}, *m_spinPan[CHANNELS_IN_TAB] = {};
	QAbstractButton *m_BtnSelect = nullptr, *m_BtnEdit = nullptr;
	int m_nLockCount = 1;
	PlugParamIndex m_nCurrentParam = 0;
	CHANNELINDEX m_nActiveTab = 0;
	CHANNELINDEX m_lastEdit = CHANNELINDEX_INVALID;
	PLUGINDEX m_nCurrentPlugin = 0;

	QComboBox *m_CbnSpecialMixProcessing = nullptr;
	mfcless::UpDown *m_SpinMixGain = nullptr;
	AccessibleButton *m_prevPluginButton = nullptr, *m_nextPluginButton = nullptr;

	enum {AdjustPattern = true, NoPatternAdjust = false};

protected:
	CViewGlobals();
public:
	~CViewGlobals() override;
	QWidget *ViewWidget() override;
protected:
	QScrollArea *m_scrollArea = nullptr;
	QPointer<CChildFrame> m_frame;
public:
	static const mfcless::ViewClass classCViewGlobals;
	const mfcless::ViewClass *GetRuntimeClass() const override { return &classCViewGlobals; }
public:
	CModDoc* GetDocument() const;
	void RecalcLayout();
	void LockControls() { m_nLockCount++; }
	void UnlockControls();
	bool IsLocked() const noexcept { return (m_nLockCount > 0); }
	int GetDlgItemIntEx(uint32 nID);
	void PopulateChannelPlugins(UpdateHint hint, const QObject *pObj = nullptr);
	void BuildEmptySlotList(std::vector<PLUGINDEX> &emptySlots);
	bool MovePlug(PLUGINDEX src, PLUGINDEX dest, bool bAdjustPat = AdjustPattern);

public:
	void OnInitialUpdate() override;
	void DoDataExchange(DataExchange &dx) override;
	void OnUpdate(mfcless::DocumentView *pSender, intptr_t lHint, QObject *pHint) override;
	mpt::ustring GetToolTipText(uint32 id, QWidget *hwnd) const override;

	void UpdateView(UpdateHint hint, QObject *pObj = nullptr);
	intptr_t OnModViewMsg(uintptr_t, intptr_t);
	intptr_t OnMidiMsg(uintptr_t midiData, intptr_t);

private:
	void PrepareUndo(CHANNELINDEX chnMod4);
	void UndoRedo(bool undo);

	void OnEditColor(const CHANNELINDEX chnMod4);
	void OnMute(const CHANNELINDEX chnMod4, const uint32 itemID);
	void OnSurround(const CHANNELINDEX chnMod4, const uint32 itemID);
	void OnEditVol(const CHANNELINDEX chnMod4, const uint32 itemID);
	void OnEditPan(const CHANNELINDEX chnMod4, const uint32 itemID);
	void OnEditName(const CHANNELINDEX chnMod4, const uint32 itemID);
	void OnFxChanged(const CHANNELINDEX chnMod4);

	IMixPlugin *GetCurrentPlugin() const;

	void FillPluginProgramBox(int32 firstProg, int32 lastProg);
	void SetPluginModified();

	void UpdateDryWetDisplay();

protected:
	void OnEditUndo();
	void OnEditRedo();
	void OnUpdateUndo(mfcless::CommandState &state);
	void OnUpdateRedo(mfcless::CommandState &state);

	void OnEditColor1();
	void OnEditColor2();
	void OnEditColor3();
	void OnEditColor4();
	void OnMute1();
	void OnMute2();
	void OnMute3();
	void OnMute4();
	void OnSurround1();
	void OnSurround2();
	void OnSurround3();
	void OnSurround4();
	void OnEditVol1();
	void OnEditVol2();
	void OnEditVol3();
	void OnEditVol4();
	void OnEditPan1();
	void OnEditPan2();
	void OnEditPan3();
	void OnEditPan4();
	void OnEditName1();
	void OnEditName2();
	void OnEditName3();
	void OnEditName4();
	void OnFx1Changed();
	void OnFx2Changed();
	void OnFx3Changed();
	void OnFx4Changed();
	void OnPluginChanged();
	void OnPluginNameChanged();
	void OnFillParamCombo();
	void OnParamChanged();
	void OnFocusParam();
	void OnFillProgramCombo();
	void OnProgramChanged();
	void OnLoadParam();
	void OnSaveParam();
	void OnSelectPlugin();
	void OnRemovePlugin();
	void OnSetParameter();
	void OnEditPlugin();
	void OnMixModeChanged();
	void OnBypassChanged();
	void OnDryMixChanged();
	void OnMovePlugToSlot();
	void OnInsertSlot();
	void OnClonePlug();
	intptr_t OnParamAutomated(uintptr_t plugin, intptr_t param);
	intptr_t OnDryWetRatioChangedFromPlayer(uintptr_t plugin, intptr_t);

	void OnWetDryExpandChanged();
	void OnAutoSuspendChanged();
	void OnSpecialMixProcessingChanged();

	void OnOutputRoutingChanged();
	void OnPrevPlugin();
	void OnNextPlugin();
	void OnDestroy();
	void OnHScroll(mfcless::ScrollCode nSBCode, int nPos, QWidget *pScrollBar);
	void OnVScroll(mfcless::ScrollCode nSBCode, int nPos, QWidget *pScrollBar);
	void OnSize(uint32 nType, int cx, int cy);
	void OnTabSelchange();
	intptr_t OnMDIDeactivate(uintptr_t, intptr_t);
	intptr_t OnUnlockControls(uintptr_t, intptr_t) { if (m_nLockCount > 0) m_nLockCount--; return 0; }
	void RegisterMessageMap() override;
};


OPENMPT_NAMESPACE_END
