/*
 * PatternGotoDialog.h
 * -------------------
 * Purpose: Implementation of pattern "go to" dialog.
 * Notes  : mfcless twin of upstream's mptrack/PatternGotoDialog.h.
 *          Draft from qt/tools/twin.py, not yet edited by hand.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "mfcless/Controls.h"
#include "DialogBase.h"
#include "../soundlib/Snd_defs.h"

class QWidget;

OPENMPT_NAMESPACE_BEGIN

class CSoundFile;

// CPatternGotoDialog dialog

class CPatternGotoDialog : public DialogBase
{
	Q_OBJECT
	CSoundFile &m_SndFile;
	mfcless::UpDown *m_SpinRow = nullptr, *m_SpinChannel = nullptr, *m_SpinPattern = nullptr, *m_SpinOrder = nullptr;

public:
	ROWINDEX m_nRow;
	CHANNELINDEX m_nChannel;
	PATTERNINDEX m_nPattern;
	ORDERINDEX m_nOrder, m_nActiveOrder;

public:
	CPatternGotoDialog(QWidget *pParent, ROWINDEX row, CHANNELINDEX chan, PATTERNINDEX pat, ORDERINDEX ord, CSoundFile &sndFile);
	bool OnInitDialog() override;

protected:
	bool m_controlLock = true;

	inline bool ControlsLocked() const { return m_controlLock; }
	inline void LockControls() { m_controlLock = true; }
	inline void UnlockControls() { m_controlLock = false; }

	void UpdateNumRows();
	void UpdateTime();

	void DoDataExchange(DataExchange &dx) override;
	void OnOK() override;

	void OnPatternChanged();
	void OnOrderChanged();
	void OnRowChanged();
	void OnTimeChanged();

	void RegisterMessageMap() override;
};

OPENMPT_NAMESPACE_END
