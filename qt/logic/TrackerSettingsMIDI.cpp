// mfcless: TrackerSettings' MIDI input device functions (upstream's
// mptrack/TrackerSettings.cpp, SetMIDIDevice and GetCurrentMIDIDevice), on
// RtMidi's port list instead of winmm's. Same logic: the setting keeps both
// the device number and its name, and the name wins when devices move.

#include "stdafx.h"
#include "TrackerSettings.h"

#include "rtmidi/RtMidi.h"

OPENMPT_NAMESPACE_BEGIN


namespace
{
std::vector<mpt::ustring> MidiInputNames()
{
	std::vector<mpt::ustring> names;
	try
	{
		RtMidiIn in;
		const unsigned int count = in.getPortCount();
		for(unsigned int i = 0; i < count; i++)
			names.push_back(mpt::ToUnicode(mpt::Charset::UTF8, in.getPortName(i)));
	} catch(const RtMidiError &)
	{
	}
	return names;
}
} // namespace


void TrackerSettings::SetMIDIDevice(uint32 id)
{
	m_nMidiDevice = id;
	const auto names = MidiInputNames();
	if(id < names.size())
	{
		midiDeviceName = names[id];
	}
}


uint32 TrackerSettings::GetCurrentMIDIDevice()
{
	if(midiDeviceName.Get().empty())
		return m_nMidiDevice;

	const mpt::ustring deviceName = mpt::trim_right(midiDeviceName.Get());

	const auto names = MidiInputNames();
	uint32 candidate = m_nMidiDevice, numDevs = static_cast<uint32>(names.size());
	for(uint32 i = 0; i < numDevs && i < names.size(); i++)
	{
		// Some device names have trailing spaces (e.g. "USB MIDI Interface "), but those may get lost in our settings framework.
		if(mpt::trim_right(names[i]) == deviceName)
		{
			candidate = i;
			numDevs = m_nMidiDevice + 1;
			// If the same device name exists twice, try to match both device number and name
			if(candidate == m_nMidiDevice)
				return candidate;
		}
	}
	// If the device changed its ID, update it now.
	m_nMidiDevice = candidate;
	return candidate;
}


OPENMPT_NAMESPACE_END
