// Smoke test for VST2 hosting: upstream's VST host (mptrack/Vstplug.cpp)
// loading a real plugin, a native .so in-process (step 4e) or a Windows
// .dll through the Wine bridge (vstbridge/).
//   - a .so is recognised as native (ELF of this architecture), a .dll and
//     a file that is not a plugin are not;
//   - the plugin manager adds it, as scanning a plugin folder does;
//   - a module rendered with the plugin on the master output sounds, and
//     differently from the same module without it.
// With the bridge's test plugin (vstbridge/testplugin.cpp), also its
// parameters, MIDI and chunks across the bridge.
// Usage: vst_host_test <plugin.so|plugin.dll> <module>
// Exits with 77 (skipped) when the plugin is not installed, or for a .dll
// when Wine is not.

#include "stdafx.h"
#include "../../platform/Platform.h"
#include "Mptrack.h"
#include "../soundlib/Sndfile.h"
#include "../soundlib/AudioReadTarget.h"
#include "../soundlib/plugins/PluginManager.h"
#include "../soundlib/plugins/PlugInterface.h"
#include "../pluginBridge/BridgeWrapper.h"
#include "openmpt/soundbase/Dither.hpp"
#include "mpt/io_read/filecursor_stdstream.hpp"

#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace
{

std::unique_ptr<CSoundFile> Load(const std::string &path)
{
	std::ifstream f(path, std::ios::binary);
	if(!f)
		return nullptr;
	auto sndFile = std::make_unique<CSoundFile>();
	if(!sndFile->Create(mpt::IO::make_FileCursor<mpt::PathString>(f), CSoundFile::loadCompleteModule))
		return nullptr;
	return sndFile;
}

std::vector<float> Render(CSoundFile &sndFile, double seconds)
{
	MixerSettings mixer = sndFile.m_MixerSettings;
	mixer.gdwMixingFreq = 44100;
	mixer.gnChannels = 2;
	sndFile.SetMixerSettings(mixer);
	sndFile.InitPlayer(true);
	DithersWrapperOpenMPT dithers(mpt::global_prng(), DithersWrapperOpenMPT::DefaultDither, 2);
	std::vector<float> out;
	std::vector<float> buffer(2 * 1024);
	while(out.size() < static_cast<std::size_t>(seconds * 44100) * 2)
	{
		AudioTargetBuffer<mpt::audio_span_interleaved<float>> target(mpt::audio_span_interleaved<float>(buffer.data(), 2, 1024), dithers);
		const auto n = sndFile.Read(1024, target);
		if(n == 0)
			break;
		out.insert(out.end(), buffer.begin(), buffer.begin() + 2 * n);
	}
	return out;
}

double Rms(const std::vector<float> &audio)
{
	double energy = 0.0;
	for(float v : audio)
		energy += static_cast<double>(v) * v;
	return audio.empty() ? 0.0 : std::sqrt(energy / audio.size());
}

} // namespace

int main(int argc, char *argv[])
{
	if(argc < 3)
	{
		std::cerr << "usage: vst_host_test <plugin.so> <module>\n";
		return 2;
	}
	// Without the plugin installed, the test is skipped (ctest's
	// SKIP_RETURN_CODE).
	if(!std::ifstream(argv[1]))
	{
		std::cout << argv[1] << " is not installed, skipped\n";
		return 77;
	}
	if(!linopenmpt::StartApp())
		return 1;
	struct AppExit { ~AppExit() { linopenmpt::StopApp(); } } appExit;
	const mpt::PathString pluginPath = mpt::PathString::FromUTF8(argv[1]);
	int failures = 0;

	const std::string pluginName = argv[1];
	const bool windows = pluginName.size() > 4 && pluginName.compare(pluginName.size() - 4, 4, ".dll") == 0;
	const bool bridgeTestPlugin = pluginName.find("BridgeTestGain") != std::string::npos;
	if(windows && std::system("command -v wine >/dev/null 2>&1") != 0)
	{
		std::cout << "Wine is not installed, skipped\n";
		return 77;
	}
	if(BridgeWrapper::IsPluginNative(pluginPath) == windows)
	{
		std::cerr << (windows ? "Windows plugin recognised as native\n" : "plugin not recognised as native\n");
		failures++;
	}
	if(BridgeWrapper::IsPluginNative(mpt::PathString::FromUTF8(argv[2])))
	{
		std::cerr << "a module recognised as a native plugin\n";
		failures++;
	}

	CVstPluginManager &manager = *theApp.GetPluginManager();
	const std::vector<VSTPluginLib *> added = manager.AddPlugin(pluginPath, false, false);
	if(added.size() != 1)
	{
		std::cerr << "AddPlugin found " << added.size() << " plugins\n";
		return 1;
	}
	const VSTPluginLib &lib = *added.front();
	std::cout << "plugin: " << lib.libraryName.ToUTF8() << " by " << mpt::ToCharset(mpt::Charset::UTF8, lib.vendor)
		<< ", " << mpt::ToCharset(mpt::Charset::UTF8, lib.GetDllArchName()) << (lib.isInstrument ? ", instrument" : ", effect") << "\n";

	auto dry = Load(argv[2]);
	auto wet = Load(argv[2]);
	if(!dry || !wet)
	{
		std::cerr << "cannot load " << argv[2] << "\n";
		return 1;
	}
	SNDMIXPLUGIN &slot = wet->m_MixPlugins[0];
	slot = SNDMIXPLUGIN();
	slot.Info.dwPluginId1 = lib.pluginId1;
	slot.Info.dwPluginId2 = lib.pluginId2;
	slot.Info.routingFlags = SNDMIXPLUGININFO::irApplyToMaster;
	slot.Info.gain = 10;
	slot.Info.szName = lib.libraryName.ToUTF8();
	slot.Info.szLibraryName = lib.libraryName.ToUTF8();
	if(!manager.CreateMixPlugin(slot, *wet) || slot.pMixPlugin == nullptr)
	{
		std::cerr << "cannot create the plugin\n";
		return 1;
	}
	IMixPlugin &plugin = *slot.pMixPlugin;
	std::cout << "parameters: " << plugin.GetNumParameters();
	if(plugin.GetNumParameters() > 0)
		std::cout << ", first \"" << mpt::ToCharset(mpt::Charset::UTF8, plugin.GetParamName(0)) << "\" = " << plugin.GetParameter(0);
	std::cout << "\n";

	if(bridgeTestPlugin)
	{
		// Parameters both ways, then the state as a chunk both ways.
		const float defaultGain = plugin.GetParameter(0);
		plugin.SetParameter(0, 0.75f);
		const float setGain = plugin.GetParameter(0);
		const auto chunkSpan = plugin.GetChunk(false);
		const std::vector<std::byte> chunk(chunkSpan.begin(), chunkSpan.end());
		const std::string chunkText(reinterpret_cast<const char *>(chunk.data()), chunk.size());
		plugin.SetParameter(0, 0.1f);
		plugin.SetChunk(mpt::as_span(chunk), false);
		const float restoredGain = plugin.GetParameter(0);
		std::cout << "gain " << defaultGain << ", set " << setGain << ", chunk \"" << chunkText.c_str() << "\", restored " << restoredGain << "\n";
		if(std::abs(defaultGain - 0.25f) > 1e-4 || std::abs(setGain - 0.75f) > 1e-4 || std::abs(restoredGain - 0.75f) > 1e-4 || chunkText.rfind("LTGAIN", 0) != 0)
		{
			std::cerr << "parameters or chunks did not cross the bridge\n";
			failures++;
		}
		plugin.SetParameter(0, 0.25f);
		if(mpt::ToCharset(mpt::Charset::UTF8, plugin.GetParamName(0)) != "Gain")
		{
			std::cerr << "wrong parameter name\n";
			failures++;
		}
		// A note-on, which the plugin counts when it gets it with the audio.
		plugin.MidiSend(0x7F3C90);
	}

	const std::vector<float> dryAudio = Render(*dry, 3.0);
	const std::vector<float> wetAudio = Render(*wet, 3.0);
	double difference = 0.0;
	for(std::size_t i = 0; i < std::min(dryAudio.size(), wetAudio.size()); i++)
		difference += std::abs(static_cast<double>(dryAudio[i]) - wetAudio[i]);
	difference /= std::max<std::size_t>(1, std::min(dryAudio.size(), wetAudio.size()));
	std::cout << "RMS dry " << Rms(dryAudio) << ", with plugin " << Rms(wetAudio) << ", mean difference " << difference << "\n";
	if(Rms(wetAudio) <= 0.001)
	{
		std::cerr << "silent with the plugin\n";
		failures++;
	}
	if(difference <= 1e-4)
	{
		std::cerr << "the plugin did not change the sound\n";
		failures++;
	}
	if(bridgeTestPlugin && plugin.GetParameter(1) <= 0.0f)
	{
		std::cerr << "the note did not reach the plugin\n";
		failures++;
	}
	return failures ? 1 : 0;
}
