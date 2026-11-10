// linOpenMPT compatibility layer: winmm's MIDI input on RtMidi (the copy
// upstream bundles for its MIDI I/O plugin), so OpenMPT's own MIDI input
// code (Mpt_midi.cpp) runs unchanged. Devices are RtMidi's input ports
// (ALSA sequencer, or JACK); messages arrive on RtMidi's thread, as winmm
// calls its callback on a driver thread.
#include "linopenmpt/win32_api.h"

#include <rtmidi/RtMidi.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace
{

using MidiInProc = void (*)(HMIDIIN, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR);

struct MidiInDevice
{
	HMIDIIN__ handle{};
	std::unique_ptr<RtMidiIn> in;
	MidiInProc callback = nullptr;
	DWORD_PTR instance = 0;
	bool started = false;
	std::chrono::steady_clock::time_point startTime;
	std::mutex mutex;
	std::deque<MIDIHDR *> buffers; // for system exclusive messages
};

MidiInDevice *DeviceOf(HMIDIIN handle)
{
	// The handle is the first member.
	return reinterpret_cast<MidiInDevice *>(handle);
}

std::unique_ptr<RtMidiIn> NewIn()
{
	try
	{
		return std::make_unique<RtMidiIn>(RtMidi::UNSPECIFIED, "OpenMPT");
	} catch(const RtMidiError &)
	{
		return nullptr;
	}
}

void Notify(MidiInDevice &device, UINT msg, DWORD_PTR param1, DWORD_PTR param2)
{
	if(device.callback != nullptr)
		device.callback(&device.handle, msg, device.instance, param1, param2);
}

// Hands every waiting system exclusive buffer back, as done (midiInReset).
void ReturnBuffers(MidiInDevice &device)
{
	for(;;)
	{
		MIDIHDR *header = nullptr;
		{
			std::lock_guard<std::mutex> lock(device.mutex);
			if(device.buffers.empty())
				return;
			header = device.buffers.front();
			device.buffers.pop_front();
		}
		header->dwFlags = (header->dwFlags & ~MHDR_INQUEUE) | MHDR_DONE;
		Notify(device, MIM_LONGDATA, reinterpret_cast<DWORD_PTR>(header), 0);
	}
}

void OnMessage(double, std::vector<unsigned char> *message, void *user)
{
	auto &device = *static_cast<MidiInDevice *>(user);
	if(message == nullptr || message->empty() || !device.started)
		return;
	const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - device.startTime).count();
	const DWORD_PTR timestamp = static_cast<DWORD_PTR>(elapsed);
	if((*message)[0] == 0xF0)
	{
		// System exclusive: into the next buffer the application gave.
		MIDIHDR *header = nullptr;
		{
			std::lock_guard<std::mutex> lock(device.mutex);
			if(!device.buffers.empty())
			{
				header = device.buffers.front();
				device.buffers.pop_front();
			}
		}
		if(header == nullptr)
			return; // winmm drops it too
		const DWORD length = static_cast<DWORD>(std::min<std::size_t>(message->size(), header->dwBufferLength));
		std::memcpy(header->lpData, message->data(), length);
		header->dwBytesRecorded = length;
		header->dwFlags = (header->dwFlags & ~MHDR_INQUEUE) | MHDR_DONE;
		Notify(device, MIM_LONGDATA, reinterpret_cast<DWORD_PTR>(header), timestamp);
		return;
	}
	// Short messages, packed status first.
	DWORD packed = 0;
	for(std::size_t i = 0; i < std::min<std::size_t>(message->size(), 3); i++)
		packed |= static_cast<DWORD>((*message)[i]) << (8 * i);
	Notify(device, MIM_DATA, packed, timestamp);
}

} // namespace

UINT midiInGetNumDevs()
{
	auto in = NewIn();
	return in != nullptr ? in->getPortCount() : 0;
}

MMRESULT midiInGetDevCaps(UINT_PTR id, MIDIINCAPS *caps, UINT size)
{
	if(caps == nullptr || size < sizeof(MIDIINCAPS))
		return MMSYSERR_INVALPARAM;
	auto in = NewIn();
	if(in == nullptr || id >= in->getPortCount())
		return MMSYSERR_BADDEVICEID;
	*caps = MIDIINCAPS{};
	std::string name;
	try
	{
		name = in->getPortName(static_cast<unsigned int>(id));
	} catch(const RtMidiError &)
	{
		return MMSYSERR_BADDEVICEID;
	}
	// The name is UTF-8; winmm's is limited to 31 characters.
	std::wstring wide;
	std::mbstate_t state{};
	const char *src = name.c_str();
	wchar_t buffer[MAXPNAMELEN] = {};
	std::mbsrtowcs(buffer, &src, MAXPNAMELEN - 1, &state);
	std::wcsncpy(caps->szPname, buffer, MAXPNAMELEN - 1);
	return MMSYSERR_NOERROR;
}

MMRESULT midiInOpen(LPHMIDIIN handle, UINT deviceID, DWORD_PTR callback, DWORD_PTR instance, DWORD flags)
{
	if(handle == nullptr)
		return MMSYSERR_INVALPARAM;
	*handle = nullptr;
	if((flags & CALLBACK_FUNCTION) != CALLBACK_FUNCTION && flags != CALLBACK_NULL)
		return MMSYSERR_INVALFLAG; // window and thread callbacks are not used upstream
	auto device = std::make_unique<MidiInDevice>();
	device->in = NewIn();
	if(device->in == nullptr || deviceID >= device->in->getPortCount())
		return MMSYSERR_BADDEVICEID;
	try
	{
		device->in->openPort(deviceID, "OpenMPT MIDI In");
		// System exclusive in, clock and active sensing out, as winmm.
		device->in->ignoreTypes(false, true, true);
		device->in->setCallback(OnMessage, device.get());
	} catch(const RtMidiError &)
	{
		return MMSYSERR_ALLOCATED;
	}
	device->callback = reinterpret_cast<MidiInProc>(callback);
	device->instance = instance;
	*handle = &device->handle;
	MidiInDevice &opened = *device.release();
	Notify(opened, MIM_OPEN, 0, 0);
	return MMSYSERR_NOERROR;
}

MMRESULT midiInClose(HMIDIIN handle)
{
	MidiInDevice *device = DeviceOf(handle);
	if(device == nullptr)
		return MMSYSERR_INVALHANDLE;
	device->started = false;
	try
	{
		device->in->cancelCallback();
		device->in->closePort();
	} catch(const RtMidiError &)
	{
	}
	ReturnBuffers(*device);
	Notify(*device, MIM_CLOSE, 0, 0);
	delete device;
	return MMSYSERR_NOERROR;
}

MMRESULT midiInStart(HMIDIIN handle)
{
	MidiInDevice *device = DeviceOf(handle);
	if(device == nullptr)
		return MMSYSERR_INVALHANDLE;
	if(!device->started)
		device->startTime = std::chrono::steady_clock::now();
	device->started = true;
	return MMSYSERR_NOERROR;
}

MMRESULT midiInStop(HMIDIIN handle)
{
	MidiInDevice *device = DeviceOf(handle);
	if(device == nullptr)
		return MMSYSERR_INVALHANDLE;
	device->started = false;
	return MMSYSERR_NOERROR;
}

MMRESULT midiInReset(HMIDIIN handle)
{
	MidiInDevice *device = DeviceOf(handle);
	if(device == nullptr)
		return MMSYSERR_INVALHANDLE;
	device->started = false;
	ReturnBuffers(*device);
	return MMSYSERR_NOERROR;
}

MMRESULT midiInPrepareHeader(HMIDIIN handle, LPMIDIHDR header, UINT size)
{
	if(DeviceOf(handle) == nullptr)
		return MMSYSERR_INVALHANDLE;
	if(header == nullptr || size < sizeof(MIDIHDR))
		return MMSYSERR_INVALPARAM;
	header->dwFlags |= MHDR_PREPARED;
	return MMSYSERR_NOERROR;
}

MMRESULT midiInUnprepareHeader(HMIDIIN handle, LPMIDIHDR header, UINT size)
{
	if(DeviceOf(handle) == nullptr)
		return MMSYSERR_INVALHANDLE;
	if(header == nullptr || size < sizeof(MIDIHDR))
		return MMSYSERR_INVALPARAM;
	if(header->dwFlags & MHDR_INQUEUE)
		return MIDIERR_STILLPLAYING;
	header->dwFlags &= ~MHDR_PREPARED;
	return MMSYSERR_NOERROR;
}

MMRESULT midiInAddBuffer(HMIDIIN handle, LPMIDIHDR header, UINT size)
{
	MidiInDevice *device = DeviceOf(handle);
	if(device == nullptr)
		return MMSYSERR_INVALHANDLE;
	if(header == nullptr || size < sizeof(MIDIHDR) || header->lpData == nullptr)
		return MMSYSERR_INVALPARAM;
	if(!(header->dwFlags & MHDR_PREPARED))
		return MIDIERR_UNPREPARED;
	header->dwFlags = (header->dwFlags & ~MHDR_DONE) | MHDR_INQUEUE;
	header->dwBytesRecorded = 0;
	std::lock_guard<std::mutex> lock(device->mutex);
	device->buffers.push_back(header);
	return MMSYSERR_NOERROR;
}
