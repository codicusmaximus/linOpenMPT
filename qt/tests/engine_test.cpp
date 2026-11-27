// mfcless: the engine in tracker configuration with no MFC. Loads a module,
// renders it, edits a pattern cell, saves it as IT and loads it back
// (the same checks as tools/smoke/tracker_engine for the MFC build).
// Usage: engine_test <module> <output.it>

#include "stdafx.h"
#include "../soundlib/Sndfile.h"
#include "../soundlib/AudioReadTarget.h"
#include "openmpt/soundbase/Dither.hpp"
#include "mpt/io_read/filecursor_stdstream.hpp"

#include <cmath>
#include <fstream>
#include <iostream>
#include <vector>

OPENMPT_NAMESPACE_BEGIN
namespace
{

std::unique_ptr<CSoundFile> Load(const char *path)
{
	std::ifstream f(path, std::ios::binary);
	if(!f)
		return nullptr;
	auto sndFile = std::make_unique<CSoundFile>();
	if(!sndFile->Create(mpt::IO::make_FileCursor<mpt::PathString>(f), CSoundFile::loadCompleteModule))
		return nullptr;
	return sndFile;
}

double RenderRMS(CSoundFile &sndFile, double seconds)
{
	MixerSettings mixer = sndFile.m_MixerSettings;
	mixer.gdwMixingFreq = 48000;
	mixer.gnChannels = 2;
	sndFile.SetMixerSettings(mixer);
	sndFile.InitPlayer(true);
	DithersWrapperOpenMPT dithers(mpt::global_prng(), DithersWrapperOpenMPT::DefaultDither, 2);
	std::vector<float> buffer(2 * 1024);
	double energy = 0.0;
	std::size_t frames = 0;
	while(frames < static_cast<std::size_t>(seconds * 48000))
	{
		AudioTargetBuffer<mpt::audio_span_interleaved<float>> target(mpt::audio_span_interleaved<float>(buffer.data(), 2, 1024), dithers);
		const auto n = sndFile.Read(1024, target);
		if(n == 0)
			break;
		for(std::size_t i = 0; i < 2 * n; ++i)
			energy += buffer[i] * buffer[i];
		frames += n;
	}
	return frames ? std::sqrt(energy / (2.0 * frames)) : 0.0;
}

int Run(int argc, char **argv)
{
	if(argc < 3)
	{
		std::cerr << "usage: engine_test <module> <output.it>\n";
		return 2;
	}
	auto sndFile = Load(argv[1]);
	if(!sndFile)
	{
		std::cerr << "FAIL: could not load " << argv[1] << "\n";
		return 1;
	}
	std::cout << "loaded: " << mpt::ToCharset(mpt::Charset::UTF8, sndFile->GetTitle().empty() ? mpt::ustring() : mpt::ToUnicode(sndFile->GetCharsetInternal(), sndFile->GetTitle())) << "\n";
	const double rms = RenderRMS(*sndFile, 2.0);
	std::cout << "rms: " << rms << "\n";
	if(!sndFile->ChangeModTypeTo(MOD_TYPE_IT))
	{
		std::cerr << "FAIL: could not convert to IT\n";
		return 1;
	}
	ModCommand &m = *sndFile->Patterns[0].GetpModCommand(0, 0);
	m.note = NOTE_MIDDLEC;
	m.instr = 1;
	{
		mpt::IO::ofstream out(mpt::PathString::FromNative(argv[2]), std::ios::binary);
		if(!sndFile->SaveIT(out, mpt::PathString::FromNative(argv[2])))
		{
			std::cerr << "FAIL: could not save\n";
			return 1;
		}
	}
	auto reloaded = Load(argv[2]);
	if(!reloaded || reloaded->Patterns[0].GetpModCommand(0, 0)->note != NOTE_MIDDLEC)
	{
		std::cerr << "FAIL: the edit did not survive saving\n";
		return 1;
	}
	std::cout << "PASS\n";
	return 0;
}

} // namespace
OPENMPT_NAMESPACE_END

int main(int argc, char **argv)
{
	return OPENMPT_NAMESPACE::Run(argc, argv);
}
