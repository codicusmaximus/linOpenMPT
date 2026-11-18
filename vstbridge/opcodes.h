// linOpenMPT plugin bridge: what each VST2 opcode's [ptr] carries, for both
// sides of the bridge (protocol.h). Include where Vst:: (upstream's
// mptrack/plugins/VstDefinitions.h) is visible.
#pragma once

#include "protocol.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace vstbridge
{

// Host to plugin (dispatcher).
inline DataKind PluginOpcodeData(int32_t opcode, std::size_t &size)
{
	size = 0;
	switch(opcode)
	{
	case Vst::effSetProgramName:
	case Vst::effCanDo:
		return DataKind::stringIn;
	case Vst::effGetProgramName:
	case Vst::effGetParamLabel:
	case Vst::effGetParamDisplay:
	case Vst::effGetParamName:
	case Vst::effGetProgramNameIndexed:
	case Vst::effGetEffectName:
	case Vst::effGetErrorText:
	case Vst::effGetVendorString:
	case Vst::effGetProductString:
	case Vst::effShellGetNextPlugin:
		size = kStringSize;
		return DataKind::stringOut;
	case Vst::effString2Parameter:
		size = kStringSize;
		return DataKind::stringInOut;
	case Vst::effGetInputProperties:
	case Vst::effGetOutputProperties:
		size = sizeof(Vst::VstPinProperties);
		return DataKind::structOut;
	case Vst::effGetParameterProperties:
		size = sizeof(Vst::VstParameterProperties);
		return DataKind::structOut;
	case Vst::effGetMidiProgramName:
	case Vst::effGetCurrentMidiProgram:
		size = sizeof(Vst::MidiProgramName);
		return DataKind::structInOut;
	case Vst::effGetMidiProgramCategory:
		size = sizeof(Vst::MidiProgramCategory);
		return DataKind::structInOut;
	case Vst::effGetMidiKeyName:
		size = sizeof(Vst::MidiKeyName);
		return DataKind::structInOut;
	case Vst::effBeginLoadBank:
	case Vst::effBeginLoadProgram:
		size = sizeof(Vst::VstPatchChunkInfo);
		return DataKind::structIn;
	case Vst::effEditGetRect:         // ERect ** out
	case Vst::effEditOpen:            // the parent window
	case Vst::effGetChunk:            // void ** out, the size returned
	case Vst::effSetChunk:            // value bytes in
	case Vst::effProcessEvents:       // VstEvents * in
	case Vst::effSetSpeakerArrangement:  // VstSpeakerArrangement * in [value] and [ptr]
	case Vst::effGetSpeakerArrangement:  // VstSpeakerArrangement ** out in [value] and [ptr]
	case Vst::effVendorSpecific:      // unknown: no data
		return DataKind::special;
	default:
		return DataKind::none;
	}
}

// Plugin to host (audioMaster).
inline DataKind HostOpcodeData(int32_t opcode, std::size_t &size)
{
	size = 0;
	switch(opcode)
	{
	case Vst::audioMasterCanDo:
		return DataKind::stringIn;
	case Vst::audioMasterGetVendorString:
	case Vst::audioMasterGetProductString:
		size = kStringSize;
		return DataKind::stringOut;
	case Vst::audioMasterGetTime:         // VstTimeInfo * returned
	case Vst::audioMasterProcessEvents:   // VstEvents * in
	case Vst::audioMasterGetDirectory:    // char * returned
		return DataKind::special;
	default:
		return DataKind::none;
	}
}

// Whether the plugin may tell the host this on its own (from its editor),
// so that it travels on the notify connection without an answer.
inline bool IsNotification(int32_t opcode)
{
	switch(opcode)
	{
	case Vst::audioMasterAutomate:
	case Vst::audioMasterBeginEdit:
	case Vst::audioMasterEndEdit:
	case Vst::audioMasterSizeWindow:
	case Vst::audioMasterUpdateDisplay:
	case Vst::audioMasterIOChanged:
	case Vst::audioMasterProcessEvents:
		return true;
	default:
		return false;
	}
}

inline void SerializeEvents(std::vector<char> &out, const Vst::VstEvents &events)
{
	Append(out, static_cast<int32_t>(events.numEvents));
	for(int32_t i = 0; i < events.numEvents; i++)
	{
		const Vst::VstEvent *event = events.events[i];
		if(event == nullptr)
			continue;
		EventHeader header{};
		header.type = static_cast<int32_t>(event->type);
		if(event->type == Vst::kVstSysExType)
		{
			// Only the VstEvent part travels: the rest holds pointers, of
			// another size in a 32-bit bridge; the dump follows it.
			const auto &sysex = *reinterpret_cast<const Vst::VstMidiSysexEvent *>(event);
			header.byteSize = sizeof(Vst::VstEvent);
			header.dumpSize = std::max(sysex.dumpBytes, 0);
			Append(out, header);
			AppendBytes(out, event, sizeof(Vst::VstEvent));
			if(header.dumpSize > 0 && sysex.sysexDump != nullptr)
				AppendBytes(out, sysex.sysexDump, header.dumpSize);
			else
				out.resize(out.size() + header.dumpSize);
		} else
		{
			// MIDI and other events: no pointers, the size they say (at
			// least the header), at most a MIDI event's.
			header.byteSize = static_cast<int32_t>(std::clamp<std::size_t>(sizeof(Vst::VstEvent) + std::max(event->byteSize, 0), sizeof(Vst::VstEvent), sizeof(Vst::VstMidiEvent)));
			Append(out, header);
			AppendBytes(out, event, header.byteSize);
		}
	}
}

// Rebuilds VstEvents in storage (which then holds them, and must outlive
// their use). Returns nullptr on malformed data.
inline Vst::VstEvents *DeserializeEvents(const char *data, std::size_t size, std::vector<char> &storage)
{
	if(size < sizeof(int32_t))
		return nullptr;
	int32_t count = 0;
	std::memcpy(&count, data, sizeof(count));
	if(count < 0)
		return nullptr;
	count = std::min<int32_t>(count, static_cast<int32_t>(Vst::VstEvents::MAX_EVENTS));
	// First pass: room for each event and dump, aligned.
	const auto align = [](std::size_t n) { return (n + 15) & ~std::size_t(15); };
	std::size_t needed = align(sizeof(Vst::VstEvents));
	std::size_t pos = sizeof(int32_t);
	for(int32_t i = 0; i < count; i++)
	{
		EventHeader header;
		if(pos + sizeof(header) > size)
			return nullptr;
		std::memcpy(&header, data + pos, sizeof(header));
		if(header.byteSize < 0 || header.dumpSize < 0 || pos + sizeof(header) + header.byteSize + header.dumpSize > size)
			return nullptr;
		needed += align(std::max<std::size_t>(header.byteSize, sizeof(Vst::VstMidiSysexEvent))) + align(header.dumpSize);
		pos += sizeof(header) + header.byteSize + header.dumpSize;
	}
	storage.assign(needed, 0);
	auto *events = reinterpret_cast<Vst::VstEvents *>(storage.data());
	events->numEvents = count;
	std::size_t out = align(sizeof(Vst::VstEvents));
	pos = sizeof(int32_t);
	for(int32_t i = 0; i < count; i++)
	{
		EventHeader header;
		std::memcpy(&header, data + pos, sizeof(header));
		pos += sizeof(header);
		char *event = storage.data() + out;
		std::memcpy(event, data + pos, header.byteSize);
		pos += header.byteSize;
		out += align(std::max<std::size_t>(header.byteSize, sizeof(Vst::VstMidiSysexEvent)));
		if(header.type == Vst::kVstSysExType && header.byteSize >= static_cast<int32_t>(sizeof(Vst::VstEvent)))
		{
			char *dump = storage.data() + out;
			std::memcpy(dump, data + pos, header.dumpSize);
			auto *sysex = reinterpret_cast<Vst::VstMidiSysexEvent *>(event);
			sysex->byteSize = static_cast<int32_t>(sizeof(Vst::VstMidiSysexEvent) - sizeof(Vst::VstEvent));
			sysex->dumpBytes = header.dumpSize;
			sysex->sysexDump = reinterpret_cast<const std::byte *>(dump);
		}
		pos += header.dumpSize;
		out += align(header.dumpSize);
		events->events[i] = reinterpret_cast<Vst::VstEvent *>(event);
	}
	return events;
}

} // namespace vstbridge
