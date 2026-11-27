/*
 * PatternFindReplaceDlg.h
 * -----------------------
 * Purpose: The find/replace dialog for pattern data.
 * Notes  : mfcless twin of upstream's mptrack/PatternFindReplaceDlg.h.
 *          Draft from qt/tools/twin.py, not yet edited by hand.
 * Authors: Olivier Lapicque
 *          OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */

#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "DialogBase.h"

#include "EffectInfo.h"
#include "PatternCursor.h"
#include "PatternFindReplace.h"
#include "PluginComboBox.h"

class QComboBox;

OPENMPT_NAMESPACE_BEGIN

/////////////////////////////////////////////////////////////////////////
// Search/Replace

class CFindReplaceTab: public PropertyPage
{
	Q_OBJECT
protected:
	QComboBox *m_cbnNote = nullptr, *m_cbnVolCmd = nullptr, *m_cbnVolume = nullptr, *m_cbnCommand = nullptr, *m_cbnParam = nullptr, *m_cbnPCParam = nullptr;
	PluginComboBox *m_cbnInstr = nullptr;

	const CSoundFile &m_sndFile;
	FindReplace &m_settings;
	EffectInfo m_effectInfo;
	ModCommand m_initialValues;
	const bool m_isReplaceTab;

	// Special ItemData values
	enum
	{
		kFindAny = INT_MAX - 1,
		kFindRange = INT_MAX - 2,

		kReplaceRelative = INT_MAX - 3,
		kReplaceMultiply = INT_MAX - 4,

		kReplaceNoteMinusOne = INT_MAX - 5,
		kReplaceNotePlusOne = INT_MAX - 6,
		kReplaceNoteMinusOctave = INT_MAX - 7,
		kReplaceNotePlusOctave = INT_MAX - 8,

		kReplaceInstrumentMinusOne = INT_MAX - 5,
		kReplaceInstrumentPlusOne = INT_MAX - 6,

		kBeginSpecial = kReplaceInstrumentPlusOne
	};

public:
	CFindReplaceTab(uint32 dlgID, bool isReplaceTab, const CSoundFile &sf, FindReplace &settings, const ModCommand &initialValues)
		: CPropertyPage{dlgID}
		, m_sndFile{sf}
		, m_settings{settings}
		, m_effectInfo{sf}
		, m_initialValues{initialValues}
		, m_isReplaceTab{isReplaceTab}
	{ }

protected:
	bool OnInitDialog() override;
	void OnOK() override;
	void DoDataExchange(DataExchange &dx) override;

	bool IsPCEvent() const;
	
	void UpdateInstrumentList();
	void UpdateVolumeList();
	void UpdateParamList();

	// When a combobox is focussed, check the corresponding checkbox.
	void CheckOnChange(int buttonID) { CheckDlgButton(buttonID, BST_CHECKED); CheckReplace(buttonID); };
	void OnNoteChanged();
	void OnInstrChanged();
	void OnVolCmdChanged();
	void OnVolumeChanged();
	void OnEffectChanged();
	void OnParamChanged();
	void OnPCParamChanged();
	// When a checkbox is checked, also check "Replace By".
	void OnCheckNote();
	void OnCheckInstr();
	void OnCheckVolCmd();
	void OnCheckVolume();
	void OnCheckEffect();
	void OnCheckParam();
	// Check "Replace By"
	void CheckReplace(int buttonID);

	void OnCheckChannelSearch();

	void RelativeOrMultiplyPrompt(QComboBox &comboBox, FindReplace::ReplaceMode &action, int &value, int range, bool isHex);
	void RegisterMessageMap() override;
};

OPENMPT_NAMESPACE_END
