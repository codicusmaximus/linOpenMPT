// Writes modules for the screenshot pass's plugin entries: upstream's
// test.mptm with its first plugin slots replaced by OpenMPT's built-in
// plugins, and one with a plugin that cannot be found.
//   <output.mptm>          FX1 LFO, FX2 Echo (the DirectX Media Object
//                          emulation)
// Not MIDI Input Output: it needs the ALSA sequencer (/dev/snd/seq), which
// containers and CI machines usually lack, and without it upstream reports
// the plugin as not found.
//   <missing.mptm>         FX1 a VST plugin that is not installed
// Usage: make_plugin_module <test.mptm> <output.mptm> <missing.mptm>

#include "stdafx.h"
#include "../../platform/Platform.h"
#include "../soundlib/Sndfile.h"
#include "../soundlib/plugins/PluginStructs.h"
#include "mpt/io_read/filecursor_stdstream.hpp"

#include <fstream>
#include <iostream>

namespace
{

constexpr int32 Magic(char a, char b, char c, char d)
{
	return static_cast<int32>(static_cast<uint32>(static_cast<uint8>(a)) | (static_cast<uint32>(static_cast<uint8>(b)) << 8) | (static_cast<uint32>(static_cast<uint8>(c)) << 16) | (static_cast<uint32>(static_cast<uint8>(d)) << 24));
}

// As PluginManager.cpp's PLUGMAGIC: big-endian four-character codes.
constexpr int32 PlugMagic(char a, char b, char c, char d)
{
	return static_cast<int32>((static_cast<uint32>(static_cast<uint8>(a)) << 24) | (static_cast<uint32>(static_cast<uint8>(b)) << 16) | (static_cast<uint32>(static_cast<uint8>(c)) << 8) | static_cast<uint32>(static_cast<uint8>(d)));
}

void SetPlugin(CSoundFile &sndFile, PLUGINDEX slot, int32 id1, int32 id2, const char *name, const char *library)
{
	SNDMIXPLUGIN &plugin = sndFile.m_MixPlugins[slot];
	plugin = SNDMIXPLUGIN();
	plugin.Info.dwPluginId1 = id1;
	plugin.Info.dwPluginId2 = id2;
	plugin.Info.routingFlags = SNDMIXPLUGININFO::irApplyToMaster;
	plugin.Info.gain = 10;
	plugin.Info.szName = name;
	plugin.Info.szLibraryName = library;
}

} // namespace

int main(int argc, char *argv[])
{
	if(argc < 4)
	{
		std::cerr << "usage: make_plugin_module <test.mptm> <output.mptm> <missing.mptm>\n";
		return 2;
	}
	if(!linopenmpt::StartApp())
		return 1;
	struct AppExit { ~AppExit() { linopenmpt::StopApp(); } } appExit;

	const auto save = [&](const char *path, auto setup) {
		std::ifstream in(argv[1], std::ios::binary);
		auto sndFile = std::make_unique<CSoundFile>();
		if(!in || !sndFile->Create(mpt::IO::make_FileCursor<mpt::PathString>(in), CSoundFile::loadCompleteModule) || sndFile->GetType() != MOD_TYPE_MPT)
		{
			std::cerr << "cannot load " << argv[1] << " as MPTM\n";
			return false;
		}
		// The external sample (test.flac) is saved inside, so that the
		// module does not depend on where it is written.
		for(SAMPLEINDEX smp = 1; smp <= sndFile->GetNumSamples(); smp++)
			sndFile->GetSample(smp).uFlags.reset(SMP_KEEPONDISK);
		for(PLUGINDEX slot = 0; slot < 4; slot++)
			sndFile->m_MixPlugins[slot] = SNDMIXPLUGIN();
		setup(*sndFile);
		std::ofstream out(path, std::ios::binary | std::ios::trunc);
		if(!sndFile->SaveIT(out, mpt::PathString::FromUTF8(path)))
		{
			std::cerr << "cannot save " << path << "\n";
			return false;
		}
		return true;
	};
	const bool ok = save(argv[2], [](CSoundFile &sndFile) {
		SetPlugin(sndFile, 0, Magic('O', 'M', 'P', 'T'), Magic('L', 'F', 'O', ' '), "LFO", "LFO");
		SetPlugin(sndFile, 1, PlugMagic('D', 'X', 'M', 'O'), static_cast<int32>(0xEF3E932C), "Echo", "{EF3E932C-D40B-4F51-8CCF-3F98F1B29D5D}");
	}) && save(argv[3], [](CSoundFile &sndFile) {
		SetPlugin(sndFile, 0, PlugMagic('V', 's', 't', 'P'), PlugMagic('l', 'n', 'X', 'X'), "Not Installed", "Not Installed Synth");
	});
	if(!ok)
		return 1;
	return 0;
}
