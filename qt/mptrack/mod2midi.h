/*
 * mod2midi.h
 * ----------
 * Purpose: Module to MIDI conversion (dialog + conversion code).
 * Notes  : mfcless twin of upstream's mptrack/mod2midi.h.
 *          Draft from qt/tools/twin.py, not yet edited by hand.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "mfcless/Controls.h"

#ifndef NO_PLUGINS
#include "AccessibleControls.h"
#include "DialogBase.h"
#include "ProgressDialog.h"
#include "../soundlib/Snd_defs.h"

class QComboBox;
class QScrollBar;
class QWidget;

OPENMPT_NAMESPACE_BEGIN

class CModDoc;
struct SubSong;

namespace MidiExport
{
	struct Mod2MidiInstr
	{
		uint8 channel = MidiMappedChannel; // See enum MidiChannel
		uint8 program = 0;
	};
	using InstrMap = std::vector<Mod2MidiInstr>;
}


class CModToMidi : public CProgressDialog
{
protected:
	QComboBox *m_CbnInstrument = nullptr, *m_CbnChannel = nullptr, *m_CbnProgram = nullptr;
	AccessibleEdit m_EditSubSong;
	mfcless::UpDown *m_SpinInstrument = nullptr;
	CModDoc &m_modDoc;
	MidiExport::InstrMap m_instrMap;
	std::vector<SubSong> m_subSongs;
	size_t m_selectedSong = 0;
	size_t m_currentInstr = 1;
	bool m_percussion = false;
	bool m_conversionRunning = false;
	bool m_locked = true;
public:
	static bool s_overlappingInstruments;

public:
	CModToMidi(CModDoc &modDoc, QWidget *parent = nullptr);
	~CModToMidi();

protected:
	void Run() override {};
	
	void UpdateSubsongName();
	void DoConversion(const mpt::PathString &fileName);

	void OnOK() override;
	void OnCancel() override;
	bool OnInitDialog() override;
	void DoDataExchange(DataExchange &dx) override;
	void FillProgramBox(bool percussion);
	void OnVScroll(uint32 nSBCode, uint32 nPos, QScrollBar * pScrollBar);
	void UpdateDialog();
	void OnChannelChanged();
	void OnProgramChanged();
	void OnOverlapChanged();
	void OnSubsongChanged();

	void RegisterMessageMap() override;;
};


OPENMPT_NAMESPACE_END

#endif // NO_PLUGINS
