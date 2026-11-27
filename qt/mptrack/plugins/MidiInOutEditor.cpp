/*
 * MidiInOutEditor.cpp
 * -------------------
 * Purpose: Editor interface for the MidiInOut plugin.
 * Notes  : mfcless twin of upstream's mptrack/plugins/MidiInOutEditor.cpp.
 *          Hand-edited from the twin.py draft.
 * Authors: Johannes Schultz (OpenMPT Devs)
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#ifdef MODPLUG_TRACKER
#include "MidiInOutEditor.h"
#include "mfcless/Widgets.h"
#include "MidiInOut.h"
#include "../FileDialog.h"
#include "../Mptrack.h"
#include "../Reporting.h"
#include "../resource.h"
#include "../UpdateHints.h"
#include "../../soundlib/MIDIEvents.h"
#include "../../soundlib/MIDIMacroParser.h"
#include <rtmidi/RtMidi.h>


OPENMPT_NAMESPACE_BEGIN


void MidiInOutEditor::RegisterMessageMap()
{
	CAbstractVstEditor::RegisterMessageMap();
	AddNotify(IDC_COMBO1, mfcless::Notify::SelChange, &MidiInOutEditor::OnInputChanged);
	AddNotify(IDC_COMBO2, mfcless::Notify::SelChange, &MidiInOutEditor::OnOutputChanged);
	AddNotify(IDC_COMBO3, mfcless::Notify::SelChange, &MidiInOutEditor::OnParamChanged);
	AddNotify(IDC_EDIT1, mfcless::Notify::EditChange, &MidiInOutEditor::OnLatencyChanged);
	AddNotify(IDC_EDIT2, mfcless::Notify::EditChange, &MidiInOutEditor::OnMidiDumpChanged);
	AddNotify(IDC_EDIT3, mfcless::Notify::EditChange, &MidiInOutEditor::OnMacroChanged);
	AddCommand(IDC_CHECK1, &MidiInOutEditor::OnTimingMessagesChanged);
	AddCommand(IDC_CHECK2, &MidiInOutEditor::OnAlwaysSendDumpChanged);
	AddCommand(IDC_BUTTON1, &MidiInOutEditor::OnLoadMidiDump);
	AddCommand(IDC_BUTTON2, &MidiInOutEditor::OnSendDumpNow);
}


void MidiInOutEditor::DoDataExchange(DataExchange &dx)
{
	CAbstractVstEditor::DoDataExchange(dx);
	//{{AFX_DATA_MAP(MidiInOutEditor)
	ExchangeControl(dx, IDC_COMBO1, m_inputCombo);
	ExchangeControl(dx, IDC_COMBO2, m_outputCombo);
	ExchangeControl(dx, IDC_COMBO3, m_paramCombo);
	ExchangeControl(dx, IDC_EDIT1, m_latencyEdit);
	ExchangeControl(dx, IDC_EDIT2, m_dumpEdit);
	ExchangeControl(dx, IDC_EDIT3 , m_paramEdit);
	ExchangeControl(dx, IDC_SPIN1, m_latencySpin);
	//}}AFX_DATA_MAP
}


MidiInOutEditor::MidiInOutEditor(MidiInOut &plugin)
	: CAbstractVstEditor{plugin}
{
}


bool MidiInOutEditor::OpenEditor(QWidget *parent)
{
	Create(IDD_MIDI_IO_PLUGIN, parent);
	MidiInOut &plugin = static_cast<MidiInOut &>(m_VstPlugin);
	m_latencyEdit->SetAccessibleSuffix(UL_("milliseconds"));
	m_latencyEdit->AllowFractions(true);
	m_latencyEdit->AllowNegative(true);
	m_latencyEdit->SetDecimalValue(plugin.m_latency * 1000.0, 4);
	m_latencySpin->SetRange(mpt::saturate_round<int>(plugin.GetOutputLatency() * -1000.0), int32_max);
	PopulateList(m_inputCombo, plugin.m_midiIn,  plugin.m_inputDevice, true);
	PopulateList(m_outputCombo, plugin.m_midiOut, plugin.m_outputDevice, false);
	UpdateOutputPlugin();
	SetChecked(IDC_CHECK1, plugin.m_sendTimingInfo ? 1 : 0);
	SetChecked(IDC_CHECK2, plugin.m_alwaysSendInitialDump ? 0 : 1);
	UpdateMidiDump();
	m_paramCombo->setUpdatesEnabled(false);
	int numItems = MidiInOut::kMacroParamMax - MidiInOut::kMacroParamMin + 1;
	for(unsigned int i = MidiInOut::kMacroParamMin; i <= MidiInOut::kMacroParamMax; i++)
	{
		mfcless::AddItem(m_paramCombo, MPT_UFORMAT("Parameter {}")(i));
	}
	m_paramCombo->setUpdatesEnabled(true);
	m_paramCombo->setCurrentIndex(0);
	if(!plugin.m_parameterMacros.empty())
		mfcless::SetWindowText(m_paramEdit, mpt::ToUnicode(mpt::Charset::ASCII, plugin.m_parameterMacros[0].first));
	m_locked = false;
	return CAbstractVstEditor::OpenEditor(parent);
}


void MidiInOutEditor::UpdateView(UpdateHint hint)
{
	CAbstractVstEditor::UpdateView(hint);
	PluginHint pluginHint = hint.ToType<PluginHint>();
	if(pluginHint.GetType()[HINT_MODTYPE | HINT_PLUGINNAMES] || pluginHint.GetPlugin() == (m_VstPlugin.GetSlot() + 1))
		UpdateOutputPlugin();
}


void MidiInOutEditor::UpdateMidiDump()
{
	MidiInOut &plugin = static_cast<MidiInOut &>(m_VstPlugin);
	m_formattedDump.clear();
	m_formattedDump.reserve(plugin.m_initialMidiDump.size() * 3);
	MIDIMacroParser parser{mpt::as_span(plugin.m_initialMidiDump)};
	mpt::span<uint8> midiMsg;
	bool firstLine = true;
	while(parser.NextMessage(midiMsg, false))
	{
		bool firstChar = true;
		for(uint8 c : midiMsg)
		{
			if(firstChar && !firstLine)
				m_formattedDump += "\r\n";
			else if(!firstChar)
				m_formattedDump += ' ';
			firstChar = firstLine = false;
			m_formattedDump += mpt::afmt::HEX0<2>(c);
		}
	}
	mfcless::SetWindowText(m_dumpEdit, mpt::ToUnicode(mpt::Charset::ASCII, m_formattedDump));
}


// Update lists of available input / output devices
void MidiInOutEditor::PopulateList(QComboBox *combo, RtMidi &rtDevice, MidiDevice &midiDevice, bool isInput)
{
	combo->setUpdatesEnabled(false);
	combo->clear();

	// Add dummy device
	mfcless::AddItem(combo, UL_("<none>"), static_cast<uintptr_t>(MidiInOut::kNoDevice));
	if(!isInput)
	{
		mfcless::AddItem(combo, UL_("Internal OpenMPT Output"), static_cast<uintptr_t>(MidiInOut::kInternalDevice));
	}

	// Go through all RtMidi devices
	auto ports = rtDevice.getPortCount();
	int selectedItem = 0;
	mpt::ustring portName;
	for(unsigned int i = 0; i < ports; i++)
	{
		try
		{
			portName = theApp.GetFriendlyMIDIPortName(mpt::ToUnicode(mpt::Charset::UTF8, midiDevice.GetPortName(i)), isInput);
			int result = mfcless::AddItem(combo, portName, i);

			if(result != -1 && i == midiDevice.index)
				selectedItem = result;
		} catch(RtMidiError &)
		{
		}
	}

	combo->setCurrentIndex(selectedItem);
	combo->setUpdatesEnabled(true);
}


void MidiInOutEditor::UpdateOutputPlugin()
{
	MPT_ASSERT(mfcless::GetItemData(m_outputCombo, 1) == MidiInOut::kInternalDevice);
	const int sel = m_outputCombo->currentIndex();
	mpt::ustring outputPlugin;
	if(std::vector<IMixPlugin *> plug; m_VstPlugin.GetOutputPlugList(plug) && plug.front() != nullptr)
		outputPlugin = MPT_UFORMAT("FX{}: {}")(mpt::ufmt::dec0<2>(plug.front()->GetSlot() + 1), mpt::ToUnicode(m_VstPlugin.GetSoundFile().m_MixPlugins[plug.front()->GetSlot()].GetName()));
	else
		outputPlugin = UL_("No Plugin");
	m_outputCombo->removeItem(1);
	mfcless::SetItemData(m_outputCombo, mfcless::InsertItem(m_outputCombo, 1, MPT_UFORMAT("Internal OpenMPT Output ({})")(outputPlugin)), static_cast<uintptr_t>(MidiInOut::kInternalDevice));
	m_outputCombo->setCurrentIndex(sel);
}


// Refresh current input / output device in GUI
void MidiInOutEditor::SetCurrentDevice(QComboBox *combo, MidiDevice::ID device)
{
	int items = combo->count();
	for(int i = 0; i < items; i++)
	{
		if(static_cast<MidiDevice::ID>(mfcless::GetItemData(combo, i)) == device)
		{
			combo->setCurrentIndex(i);
			break;
		}
	}
}


void MidiInOutEditor::OnInputChanged()
{
	MidiDevice::ID newDevice = static_cast<MidiDevice::ID>(mfcless::GetItemData(m_inputCombo, m_inputCombo->currentIndex()));
	static_cast<MidiInOut &>(m_VstPlugin).OpenDevice(newDevice, true);
}


void MidiInOutEditor::OnOutputChanged()
{
	MidiDevice::ID newDevice = static_cast<MidiDevice::ID>(mfcless::GetItemData(m_outputCombo, m_outputCombo->currentIndex()));
	static_cast<MidiInOut &>(m_VstPlugin).OpenDevice(newDevice, false);
}


void MidiInOutEditor::OnLatencyChanged()
{
	MidiInOut &plugin = static_cast<MidiInOut &>(m_VstPlugin);
	double latency = 0.0;
	if(!m_locked && m_latencyEdit->GetDecimalValue(latency))
	{
		plugin.m_latency = latency * (1.0 / 1000.0);
		plugin.SetModified();
	}
}


void MidiInOutEditor::OnTimingMessagesChanged()
{
	if(!m_locked)
	{
		MidiInOut &plugin = static_cast<MidiInOut &>(m_VstPlugin);
		plugin.m_sendTimingInfo = IsChecked(IDC_CHECK1) != 0;
		plugin.SetModified();
	}
}


void MidiInOutEditor::OnMidiDumpChanged()
{
	if(m_locked)
		return;
	if(ValidateMacroString(*m_dumpEdit, m_formattedDump, false, false, true))
	{
		mpt::ustring dumpText;
		mfcless::GetWindowText(m_dumpEdit, dumpText);
		m_formattedDump = mpt::ToCharset(mpt::Charset::ASCII, dumpText);
		std::vector<uint8> dump;
		dump.reserve(static_cast<size_t>(m_formattedDump.size() / 3));
		bool firstNibble = true;
		uint8 b = 0;
		for(const char c : m_formattedDump)
		{
			if(c >= '0' && c <= '9')
				b |= static_cast<uint8>(c - '0');
			else if(c >= 'A' && c <= 'F')
				b |= static_cast<uint8>(c - 'A' + 0x0A);
			else if(c >= 'a' && c <= 'f')
				b |= static_cast<uint8>(c - 'a' + 0x0A);
			else
				continue;

			if(firstNibble)
			{
				b <<= 4;
			} else
			{
				dump.push_back(b);
				b = 0;
			}
			firstNibble = !firstNibble;
		}
		static_cast<MidiInOut &>(m_VstPlugin).SetInitialMidiDump(std::move(dump));
	}
}


void MidiInOutEditor::OnAlwaysSendDumpChanged()
{
	MidiInOut &plugin = static_cast<MidiInOut &>(m_VstPlugin);
	plugin.m_alwaysSendInitialDump = IsChecked(IDC_CHECK2) == 0;
	plugin.SetModified();
}


void MidiInOutEditor::OnLoadMidiDump()
{
	FileDialog dlg = OpenFileDialog()
						 .DefaultExtension(U_("syx"))
						 .ExtensionFilter(U_("SysEx Dumps (*.syx)|*.syx||"));
	if(!dlg.Show(this))
		return;
	mpt::IO::InputFile f(dlg.GetFirstFile());
	if(!f.IsValid())
		return;
	FileReader file = GetFileReader(f);
	std::vector<uint8> dump;
	file.ReadVector(dump, file.GetLength());
	if(!dump.empty() && dump[0] < 0x80)
	{
		if(Reporting::Confirm("This file is most likely not a valid SysEx dump. Load it anyway?") != cnfYes)
			return;
	}
	static_cast<MidiInOut &>(m_VstPlugin).SetInitialMidiDump(std::move(dump));
	m_locked = true;
	UpdateMidiDump();
	m_locked = false;
}


void MidiInOutEditor::OnSendDumpNow()
{
	static_cast<MidiInOut &>(m_VstPlugin).m_initialDumpSent = false;
}


void MidiInOutEditor::OnParamChanged()
{
	int i = m_paramCombo->currentIndex();
	if(i < 0)
		return;
	MidiInOut &plugin = static_cast<MidiInOut &>(m_VstPlugin);
	m_locked = true;
	if(static_cast<size_t>(i) < plugin.m_parameterMacros.size())
		mfcless::SetWindowText(m_paramEdit, mpt::ToUnicode(mpt::Charset::ASCII, plugin.m_parameterMacros[i].first));
	else
		mfcless::SetWindowText(m_paramEdit, UL_(""));
	m_locked = false;
}


void MidiInOutEditor::OnMacroChanged()
{
	int i = m_paramCombo->currentIndex();
	if(m_locked || i < 0)
		return;
	MidiInOut &plugin = static_cast<MidiInOut &>(m_VstPlugin);
	std::string_view prevMacro;
	if(static_cast<size_t>(i) < plugin.m_parameterMacros.size())
		prevMacro = plugin.m_parameterMacros[i].first;
	if(ValidateMacroString(*m_paramEdit, prevMacro, true, true, false))
	{
		mpt::ustring macroText;
		mfcless::GetWindowText(m_paramEdit, macroText);
		static_cast<MidiInOut &>(m_VstPlugin).SetMacro(static_cast<size_t>(MidiInOut::kMacroParamMin + i), mpt::ToCharset(mpt::Charset::ASCII, macroText));
	}
}


OPENMPT_NAMESPACE_END

#endif // MODPLUG_TRACKER
