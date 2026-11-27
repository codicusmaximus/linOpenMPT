/*
 * MIDIMappingDialog.h
 * -------------------
 * Purpose: Implementation of OpenMPT's MIDI mapping dialog, for mapping incoming MIDI messages to plugin parameters.
 * Notes  : mfcless twin of upstream's mptrack/MIDIMappingDialog.h.
 *          Draft from qt/tools/twin.py, not yet edited by hand.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "mfcless/Controls.h"

#ifndef NO_PLUGINS

#include "CListCtrl.h"
#include "MIDIMapping.h"
#include "ResizableDialog.h"
#include "PluginComboBox.h"


class QComboBox;
class QWidget;

OPENMPT_NAMESPACE_BEGIN

class CSoundFile;
class CMIDIMapper;

class CMIDIMappingDialog : public ResizableDialog
{
	Q_OBJECT
public:
	CMIDIMappingDirective m_Setting;

protected:
	CSoundFile &m_sndFile;
	CMIDIMapper &m_rMIDIMapper;
	QWidget * oldMIDIRecondWnd;

	// Dialog Data
	QComboBox *m_ControllerCBox = nullptr;
	PluginComboBox *m_PluginCBox = nullptr;
	QComboBox *m_PlugParamCBox = nullptr;
	QComboBox *m_ChannelCBox = nullptr;
	QComboBox *m_EventCBox = nullptr;
	CListCtrlEx *m_List = nullptr;
	mfcless::UpDown *m_SpinMoveMapping = nullptr;

	uint8 m_lastCC = uint8_max;

public:
	CMIDIMappingDialog(QWidget *pParent, CSoundFile &rSndfile);
	~CMIDIMappingDialog();

protected:
	void UpdateDialog(int selItem = -1);
	void UpdateEvent();
	void UpdateParameters();
	int InsertItem(const CMIDIMappingDirective &m, int insertAt);
	void SelectItem(int i);

	void SetModified();

	bool OnInitDialog() override;
	void DoDataExchange(DataExchange &dx) override;	// DDX/DDV support
	mpt::ustring GetToolTipText(uint32 id, QWidget * hwnd) const override;

	void RegisterMessageMap() override;

	void OnSelectionChanged(NMHDR *pNMHDR = nullptr, intptr_t *pResult = nullptr);
	
	void OnBnClickedCheckactive();
	void OnBnClickedCheckCapture();
	void OnCbnSelchangeComboController();
	void OnCbnSelchangeComboChannel();
	void OnCbnSelchangeComboPlugin();
	void OnCbnSelchangeComboParam();
	void OnCbnSelchangeComboEvent();
	void OnBnClickedButtonAdd();
	void OnBnClickedButtonReplace();
	void OnBnClickedButtonRemove();
	intptr_t OnMidiMsg(uintptr_t, intptr_t);
	void OnDeltaposSpinmovemapping();
	void OnBnClickedCheckPatRecord();
};

OPENMPT_NAMESPACE_END

#endif // NO_PLUGINS
