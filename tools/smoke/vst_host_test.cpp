// Smoke test for native Linux VST2 hosting (step 4e): upstream's VST host
// (mptrack/Vstplug.cpp) loading a real .so plugin in-process.
//   - the plugin is recognised as native (ELF of this architecture) and a
//     file that is not a shared object is not;
//   - the plugin manager adds it, as scanning a plugin folder does;
//   - a module rendered with the plugin on the master output sounds, and
//     differently from the same module without it.
// Usage: vst_host_test <plugin.so> <module>
// Exits with 77 (skipped) when the plugin is not installed.

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

	if(!BridgeWrapper::IsPluginNative(pluginPath))
	{
		std::cerr << "plugin not recognised as native\n";
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
	return failures ? 1 : 0;
}
