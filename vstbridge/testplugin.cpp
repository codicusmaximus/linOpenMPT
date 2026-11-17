// linOpenMPT: a small Windows VST2 effect for testing the Wine plugin bridge
// (built with MinGW, like the bridge). Stereo gain:
//   - parameter 0 "Gain" (default 0.25: half the volume, so the bridged
//     sound differs from the dry one);
//   - parameter 1 "Notes": how many MIDI note-ons it received, so a test
//     sees MIDI arrive;
//   - its state saves as a chunk, so a test sees chunks travel both ways;
//   - an editor that paints a colour gradient, so a test sees it embedded.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iterator>

#include "openmpt/all/BuildSettings.hpp"

namespace vstdefs
{
using int8 = std::int8_t;
using uint8 = std::uint8_t;
using int16 = std::int16_t;
using uint16 = std::uint16_t;
using int32 = std::int32_t;
using uint32 = std::uint32_t;
using int64 = std::int64_t;
using uint64 = std::uint64_t;
#include "mptrack/plugins/VstDefinitions.h"
}  // namespace vstdefs

namespace Vst = vstdefs::Vst;

namespace
{

constexpr int kWidth = 256, kHeight = 128;

struct Plugin
{
	Vst::AEffect effect{};
	Vst::AudioMasterCallbackFunc host = nullptr;
	float gain = 0.25f;
	int notes = 0;
	char chunk[64]{};
	Vst::ERect rect{0, 0, kHeight, kWidth};
	HWND editor = nullptr;
};

LRESULT CALLBACK EditorProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if(message == WM_PAINT)
	{
		PAINTSTRUCT ps;
		HDC dc = BeginPaint(hwnd, &ps);
		// A two-way gradient: many colours, which a screenshot can count.
		for(int y = 0; y < kHeight; y += 4)
		{
			for(int x = 0; x < kWidth; x += 4)
			{
				RECT cell{x, y, x + 4, y + 4};
				HBRUSH brush = CreateSolidBrush(RGB(x, y * 2, 255 - x));
				FillRect(dc, &cell, brush);
				DeleteObject(brush);
			}
		}
		EndPaint(hwnd, &ps);
		return 0;
	}
	if(message == WM_LBUTTONDOWN)
	{
		// As a knob would: the host hears about it.
		auto *plugin = reinterpret_cast<Plugin *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
		if(plugin != nullptr && plugin->host != nullptr)
		{
			plugin->gain = std::clamp(static_cast<float>(LOWORD(lParam)) / kWidth, 0.0f, 1.0f);
			plugin->host(&plugin->effect, Vst::audioMasterAutomate, 0, 0, nullptr, plugin->gain);
		}
		return 0;
	}
	return DefWindowProcW(hwnd, message, wParam, lParam);
}

intptr_t VSTCALLBACK Dispatcher(Vst::AEffect *effect, Vst::VstOpcodeToPlugin opcode, int32_t index, intptr_t value, void *ptr, float opt)
{
	auto *plugin = static_cast<Plugin *>(effect->object);
	switch(opcode)
	{
	case Vst::effClose:
		delete plugin;
		return 1;
	case Vst::effGetParamName:
		std::strcpy(static_cast<char *>(ptr), index == 0 ? "Gain" : "Notes");
		return 1;
	case Vst::effGetParamLabel:
		std::strcpy(static_cast<char *>(ptr), index == 0 ? "x" : "");
		return 1;
	case Vst::effGetParamDisplay:
		if(index == 0)
			std::snprintf(static_cast<char *>(ptr), 8, "%.2f", plugin->gain * 2.0f);
		else
			std::snprintf(static_cast<char *>(ptr), 8, "%d", plugin->notes);
		return 1;
	case Vst::effGetEffectName:
		std::strcpy(static_cast<char *>(ptr), "Bridge Test Gain");
		return 1;
	case Vst::effGetVendorString:
		std::strcpy(static_cast<char *>(ptr), "linOpenMPT");
		return 1;
	case Vst::effGetProductString:
		std::strcpy(static_cast<char *>(ptr), "Bridge Test Gain");
		return 1;
	case Vst::effGetVendorVersion:
		return 1;
	case Vst::effGetVstVersion:
		return Vst::kVstVersion;
	case Vst::effGetPlugCategory:
		return Vst::kPlugCategEffect;
	case Vst::effCanDo:
		return (!std::strcmp(static_cast<const char *>(ptr), "receiveVstEvents") || !std::strcmp(static_cast<const char *>(ptr), "receiveVstMidiEvent")) ? 1 : -1;
	case Vst::effProcessEvents:
	{
		const auto &events = *static_cast<const Vst::VstEvents *>(ptr);
		for(int32_t i = 0; i < events.numEvents; i++)
		{
			if(events.events[i]->type != Vst::kVstMidiType)
				continue;
			const auto &midi = *reinterpret_cast<const Vst::VstMidiEvent *>(events.events[i]);
			const uint8_t status = static_cast<uint8_t>(midi.midiData & 0xFF), velocity = static_cast<uint8_t>((midi.midiData >> 16) & 0xFF);
			if((status & 0xF0) == 0x90 && velocity > 0)
				plugin->notes++;
		}
		return 1;
	}
	case Vst::effGetChunk:
		std::snprintf(plugin->chunk, sizeof(plugin->chunk), "LTGAIN %.6f", plugin->gain);
		*static_cast<void **>(ptr) = plugin->chunk;
		return static_cast<intptr_t>(std::strlen(plugin->chunk) + 1);
	case Vst::effSetChunk:
	{
		float gain = 0.0f;
		if(value > 7 && std::sscanf(static_cast<const char *>(ptr), "LTGAIN %f", &gain) == 1)
		{
			plugin->gain = std::clamp(gain, 0.0f, 1.0f);
			return 1;
		}
		return 0;
	}
	case Vst::effEditGetRect:
		*static_cast<Vst::ERect **>(ptr) = &plugin->rect;
		return 1;
	case Vst::effEditOpen:
	{
		static bool registered = false;
		if(!registered)
		{
			WNDCLASSW windowClass{};
			windowClass.lpfnWndProc = EditorProc;
			windowClass.hInstance = GetModuleHandleW(nullptr);
			windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
			windowClass.lpszClassName = L"BridgeTestGainEditor";
			RegisterClassW(&windowClass);
			registered = true;
		}
		plugin->editor = CreateWindowExW(0, L"BridgeTestGainEditor", L"", WS_CHILD | WS_VISIBLE, 0, 0, kWidth, kHeight, static_cast<HWND>(ptr), nullptr, GetModuleHandleW(nullptr), nullptr);
		SetWindowLongPtrW(plugin->editor, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(plugin));
		return plugin->editor != nullptr ? 1 : 0;
	}
	case Vst::effEditClose:
		if(plugin->editor)
			DestroyWindow(plugin->editor);
		plugin->editor = nullptr;
		return 1;
	default:
		(void)opt;
		return 0;
	}
}

void VSTCALLBACK ProcessReplacing(Vst::AEffect *effect, float **inputs, float **outputs, int32_t frames)
{
	const auto *plugin = static_cast<const Plugin *>(effect->object);
	const float gain = plugin->gain * 2.0f;
	for(int channel = 0; channel < 2; channel++)
	{
		for(int32_t i = 0; i < frames; i++)
			outputs[channel][i] = inputs[channel][i] * gain;
	}
}

void VSTCALLBACK SetParameter(Vst::AEffect *effect, int32_t index, float value)
{
	if(index == 0)
		static_cast<Plugin *>(effect->object)->gain = std::clamp(value, 0.0f, 1.0f);
}

float VSTCALLBACK GetParameter(Vst::AEffect *effect, int32_t index)
{
	const auto *plugin = static_cast<const Plugin *>(effect->object);
	return index == 0 ? plugin->gain : std::min(plugin->notes, 127) / 127.0f;
}

}  // namespace

extern "C" __declspec(dllexport) Vst::AEffect *VSTCALLBACK VSTPluginMain(Vst::AudioMasterCallbackFunc host)
{
	if(host == nullptr || host(nullptr, Vst::audioMasterVersion, 0, 0, nullptr, 0.0f) == 0)
		return nullptr;
	auto *plugin = new Plugin;
	plugin->host = host;
	Vst::AEffect &effect = plugin->effect;
	effect.magic = Vst::kEffectMagic;
	effect.dispatcher = Dispatcher;
	effect.setParameter = SetParameter;
	effect.getParameter = GetParameter;
	effect.processReplacing = ProcessReplacing;
	effect.numPrograms = 1;
	effect.numParams = 2;
	effect.numInputs = 2;
	effect.numOutputs = 2;
	effect.flags = static_cast<Vst::VstFlags>(Vst::effFlagsHasEditor | Vst::effFlagsCanReplacing | Vst::effFlagsProgramChunks);
	effect.object = plugin;
	effect.uniqueID = 0x4C546761;  // 'LTga'
	effect.version = 1;
	return &effect;
}
