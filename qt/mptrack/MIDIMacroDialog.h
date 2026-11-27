/*
 * MIDIMacroDialog.h
 * -----------------
 * Purpose: MIDI Macro Configuration Dialog
 * Notes  : mfcless twin of upstream's mptrack/MIDIMacroDialog.h.
 *          Hand-edited from the twin.py draft.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"

#include "ColourEdit.h"
#include "DialogBase.h"
#include "PluginComboBox.h"
#include "../common/misc_util.h"
#include "../soundlib/MIDIMacros.h"
#include "mpt/base/alloc.hpp"

class QAbstractButton;
class QComboBox;
class QLineEdit;
class QWidget;

OPENMPT_NAMESPACE_BEGIN

class CSoundFile;

class CMidiMacroSetup : public DialogBase
{
	Q_OBJECT
protected:
	QComboBox *m_CbnSFx = nullptr, *m_CbnSFxPreset = nullptr, *m_CbnZxx = nullptr, *m_CbnZxxPreset = nullptr, *m_CbnMacroParam = nullptr, *m_CbnMacroCC = nullptr;
	PluginComboBox *m_CbnMacroPlug = nullptr;
	QLineEdit *m_EditSFx = nullptr, *m_EditZxx = nullptr;
	struct MacroEdit
	{
		QAbstractButton *Button = nullptr;
		CColourEdit *Value = nullptr;
		CColourEdit *Type = nullptr;
		QAbstractButton *ShowAll = nullptr;
	};
	std::vector<MacroEdit> m_EditMacro = std::vector<MacroEdit>(static_cast<int>(kSFxMacros));

	CSoundFile &m_SndFile;

public:
	CMidiMacroSetup(CSoundFile &sndFile, QWidget *parent = nullptr);
private:
	mpt::heap_value<MIDIMacroConfig> m_vMidiCfg;
public:
	MIDIMacroConfig & m_MidiCfg;

protected:
	bool OnInitDialog() override;
	void DoDataExchange(DataExchange &dx) override;

	void UpdateMacroList(int macro=-1);
	void ToggleBoxes(uint32 preset, uint32 sfx);
	void UpdateDialog();
	void OnSetAsDefault();
	void OnResetCfg();
	void OnMacroHelp();
	void OnSFxChanged();
	void OnSFxPresetChanged();
	void OnZxxPresetChanged();
	void OnSFxEditChanged();
	void OnZxxEditChanged();
	void UpdateZxxSelection();
	void OnPlugChanged();
	void OnPlugParamChanged();
	void OnCCChanged();

	void OnViewAllParams(uint32 id);
	void OnSetSFx(uint32 id);
	void RegisterMessageMap() override;
};

OPENMPT_NAMESPACE_END
