/*
 * SampleConfigDlg.h
 * -------------------
 * Purpose: Implementation of the sample/instrument editor settings dialog.
 * Notes  : mfcless twin of upstream's mptrack/SampleConfigDlg.h.
 *          Draft from qt/tools/twin.py, not yet edited by hand.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "DialogBase.h"
#include "AccessibleControls.h"

class QComboBox;
class QScrollBar;

OPENMPT_NAMESPACE_BEGIN

class COptionsSampleEditor : public PropertyPage
{
	Q_OBJECT
public:
	COptionsSampleEditor();

protected:
	bool OnInitDialog() override;
	void OnOK() override;
	void DoDataExchange(DataExchange &dx) override;
	bool OnSetActive() override;

	void RecalcUndoSize();

	void OnHScroll(uint32 /*nSBCode*/, uint32 /*nPos*/, QScrollBar * /*pScrollBar*/) { OnSettingsChanged(); }
	void OnSettingsChanged() { SetModified(true); }
	void OnUndoSizeChanged();
	void RegisterMessageMap() override;;

	QComboBox *m_cbnDefaultSampleFormat = nullptr, *m_cbnDefaultVolumeHandling = nullptr;
	AccessibleEdit *m_undoBufferEdit = nullptr, *m_finetuneEdit = nullptr;
};

OPENMPT_NAMESPACE_END
