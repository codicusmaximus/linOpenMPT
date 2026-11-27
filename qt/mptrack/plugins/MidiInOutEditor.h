/*
 * MidiInOutEditor.h
 * -----------------
 * Purpose: Editor interface for the MidiInOut plugin.
 * Notes  : mfcless twin of upstream's mptrack/plugins/MidiInOutEditor.h.
 *          Hand-edited from the twin.py draft.
 * Authors: Johannes Schultz (OpenMPT Devs)
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "mfcless/Controls.h"

#ifdef MODPLUG_TRACKER

#include "MidiInOut.h"
#include "../AbstractVstEditor.h"
#include "../CDecimalSupport.h"

class QComboBox;
class QLineEdit;
class QWidget;

OPENMPT_NAMESPACE_BEGIN

class MidiInOutEditor : public CAbstractVstEditor
{
	Q_OBJECT
protected:
	QComboBox *m_inputCombo = nullptr, *m_outputCombo = nullptr, *m_paramCombo = nullptr;
	CNumberEdit *m_latencyEdit = nullptr;
	QLineEdit *m_dumpEdit = nullptr, *m_paramEdit = nullptr;
	mfcless::UpDown *m_latencySpin = nullptr;
	std::string m_formattedDump;
	bool m_locked = true;

public:

	MidiInOutEditor(MidiInOut &plugin);

	// Refresh current input / output device in GUI
	void SetCurrentDevice(bool asInputDevice, MidiDevice::ID device)
	{
		QComboBox *combo = asInputDevice ? m_inputCombo : m_outputCombo;
		SetCurrentDevice(combo, device);
	}

	bool OpenEditor(QWidget *parent) override;
	void UpdateView(UpdateHint hint) override;
	bool IsResizable() const override { return false; }
	bool SetSize(int, int) override { return false; }

protected:

	// Update lists of available input / output devices
	static void PopulateList(QComboBox *combo, RtMidi &rtDevice, MidiDevice &midiDevice, bool isInput);
	void UpdateOutputPlugin();
	// Refresh current input / output device in GUI
	void SetCurrentDevice(QComboBox *combo, MidiDevice::ID device);

	void UpdateMidiDump();

	void DoDataExchange(DataExchange &dx) override;

	void OnInputChanged();
	void OnOutputChanged();
	void OnLatencyChanged();
	void OnMidiDumpChanged();
	void OnTimingMessagesChanged();
	void OnAlwaysSendDumpChanged();
	void OnLoadMidiDump();
	void OnSendDumpNow();
	void OnParamChanged();
	void OnMacroChanged();

	void RegisterMessageMap() override;
};

OPENMPT_NAMESPACE_END

#endif // MODPLUG_TRACKER
