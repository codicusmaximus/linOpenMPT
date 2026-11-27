/*
 * MPT_MIDI.cpp
 * ------------
 * Purpose: MIDI Input handling code.
 * Notes  : mfcless twin of upstream's mptrack/Mpt_midi.cpp.
 *          Hand-edited from the twin.py draft: MIDI input runs on RtMidi
 *          (the copy upstream bundles for its MIDI I/O plugin) instead of
 *          winmm, with the same device numbers (MidiInputNames).
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "Dlsbank.h"
#include "mfcless/Widgets.h"
#include "InputHandler.h"
#include "Mainfrm.h"
#include "resource.h"
#include "TrackerSettings.h"
#include "WindowMessages.h"
#include "../soundlib/MIDIEvents.h"
#include "mfcless/ModMessage.h"

#include "rtmidi/RtMidi.h"

#include <chrono>
#include <memory>



OPENMPT_NAMESPACE_BEGIN


#ifdef MPT_ALL_LOGGING
#define MPTMIDI_RECORDLOG
#endif

//Get Midi message(dwParam1), apply MIDI settings having effect on volume, and return
//the volume value [0, 256]. In addition value -1 is used as 'use default value'-indicator.
int CMainFrame::ApplyVolumeRelatedSettings(const uint32 &dwParam1, uint8 midiChannelVolume)
{
	int nVol = MIDIEvents::GetDataByte2FromEvent(dwParam1);
	const FlagSet<MidiSetup> midiSetup = TrackerSettings::Instance().midiSetup;
	if(midiSetup[MidiSetup::RecordVelocity])
	{
		if(!midiSetup[MidiSetup::ApplyChannelVolumeToVelocity])
			midiChannelVolume = 127;
		nVol = Util::muldivr_unsigned(CDLSBank::DLSMidiVolumeToLinear(nVol), TrackerSettings::Instance().midiVelocityAmp * midiChannelVolume, 100 * 127 * 256);
		Limit(nVol, 1, 256);
	} else
	{
		// Case: No velocity record.
		if(midiSetup[MidiSetup::ApplyChannelVolumeToVelocity])
			nVol = (midiChannelVolume + 1) * 2;
		else //Use default volume
			nVol = -1;
	}

	return nVol;
}


void ApplyTransposeKeyboardSetting(CMainFrame &rMainFrm, uint32 &midiMsg)
{
	const FlagSet<MidiSetup> midiSetup = TrackerSettings::Instance().midiSetup;
	if(midiSetup[MidiSetup::TransposeKeyboard]
		&& (MIDIEvents::GetChannelFromEvent(midiMsg) != 9))
	{
		int nTranspose = rMainFrm.GetBaseOctave() - 4;
		if (nTranspose)
		{
			int note = MIDIEvents::GetDataByte1FromEvent(midiMsg);
			if (note < 0x80)
			{
				note += nTranspose * 12;
				Limit(note, 0, NOTE_MAX - NOTE_MIN);

				midiMsg &= 0xffff00ff;

				midiMsg |= (note << 8);
			}
		}
	}
}


/////////////////////////////////////////////////////////////////////////////
// RtMidi Midi Record

namespace
{

struct MidiInPort
{
	RtMidiIn in;
	CMainFrame *mainFrame = nullptr;
	std::chrono::steady_clock::time_point startTime;

	MidiInPort() : in(RtMidi::UNSPECIFIED, "OpenMPT") { }
};

// Called on RtMidi's thread, as winmm calls its callback on a driver thread.
void MidiInCallBack(double, std::vector<unsigned char> *message, void *userData)
{
	auto &port = *static_cast<MidiInPort *>(userData);
	CMainFrame *pMainFrm = port.mainFrame;
	if(!pMainFrm || message == nullptr || message->empty())
		return;
	const auto timestamp = static_cast<uintptr_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - port.startTime).count());

	if((*message)[0] == 0xF0)
	{
		// SysEx
		std::lock_guard<mpt::mutex> guard{pMainFrm->midiInData.dataMutex};
		if(auto callback = pMainFrm->GetMidiSysexCallback())
			callback(mpt::const_byte_span{mpt::byte_cast<const std::byte *>(message->data()), message->size()});
		return;
	}

	// Short messages, packed status byte first as winmm does
	uint32 data = 0;
	for(std::size_t i = 0; i < std::min<std::size_t>(message->size(), 3); i++)
		data |= static_cast<uint32>((*message)[i]) << (8 * i);

#ifdef MPTMIDI_RECORDLOG
	uint32 dwMidiStatus = data & 0xFF;
	uint32 dwMidiByte1 = (data >> 8) & 0xFF;
	uint32 dwMidiByte2 = (data >> 16) & 0xFF;
	MPT_LOG_GLOBAL(LogDebug, "MIDI", MPT_UFORMAT("time={}ms status={} data={}.{}")(mpt::ufmt::dec(timestamp), mpt::ufmt::HEX0<2>(dwMidiStatus), mpt::ufmt::HEX0<2>(dwMidiByte1), mpt::ufmt::HEX0<2>(dwMidiByte2)));
#endif

	QWidget *hWndMidi = pMainFrm->GetMidiRecordWnd();
	if(hWndMidi)
	{
		switch(MIDIEvents::GetTypeFromEvent(data))
		{
		case MIDIEvents::evNoteOff:	// Note Off
		case MIDIEvents::evNoteOn:	// Note On
			ApplyTransposeKeyboardSetting(*pMainFrm, data);
			[[fallthrough]];
		default:
			mfcless::PostModMessage(hWndMidi, WM_MOD_MIDIMSG, data, timestamp);
			return;	// Message has been handled
		}
	}
	// Pass MIDI to keyboard handler
	CMainFrame::GetInputHandler()->HandleMIDIMessage(kCtxAllContexts, data);
}


bool OpenPort(CMainFrame &mainFrame, void *&handle)
{
	const uint32 device = TrackerSettings::Instance().GetCurrentMIDIDevice();
	try
	{
		auto port = std::make_unique<MidiInPort>();
		if(device >= port->in.getPortCount())
			return false;
		port->mainFrame = &mainFrame;
		// winmm passes on everything, including SysEx, clock and active sensing
		port->in.ignoreTypes(false, false, false);
		port->in.setCallback(MidiInCallBack, port.get());
		port->startTime = std::chrono::steady_clock::now();
		port->in.openPort(device, "MIDI In");
		handle = port.release();
		return true;
	} catch(const RtMidiError &)
	{
		return false;
	}
}

} // namespace


bool CMainFrame::midiOpenDevice(bool showSettings)
{
	if (midiInData.inHandle) return true;
	
	if(!OpenPort(*this, midiInData.inHandle))
	{
		midiInData.inHandle = nullptr;
		// Show MIDI configuration on fail.
		if(showSettings)
		{
			CMainFrame::m_nLastOptionsPage = OPTIONS_PAGE_MIDI;
			CMainFrame::GetMainFrame()->OnViewOptions();
		}
		// Let's see if the user updated the settings.
		if(!OpenPort(*this, midiInData.inHandle))
		{
			midiInData.inHandle = nullptr;
			return false;
		}
	}
	return true;
}


void CMainFrame::midiCloseDevice()
{
	if(midiInData.inHandle)
	{
		auto *port = static_cast<MidiInPort *>(midiInData.inHandle);
		midiInData.inHandle = nullptr;
		try
		{
			port->in.cancelCallback();
			port->in.closePort();
		} catch(const RtMidiError &)
		{
		}
		std::lock_guard<mpt::mutex> guard{midiInData.dataMutex};
		delete port;
	}
}


void CMainFrame::OnMidiRecord()
{
	if(midiInData.inHandle)
	{
		midiCloseDevice();
	} else
	{
		midiOpenDevice();
	}
}


void CMainFrame::OnUpdateMidiRecord(mfcless::CommandState &state)
{
	state.SetCheck((midiInData.inHandle) ? true : false);
}


OPENMPT_NAMESPACE_END
