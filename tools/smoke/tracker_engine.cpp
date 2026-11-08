// Smoke test for plan step 2: the playback engine in *tracker*
// configuration (MODPLUG_TRACKER, the same build OpenMPT.exe uses), with
// editing and saving compiled in.
//
// Loads a module, renders audio, edits a pattern cell, saves the module as
// IT, reloads the saved file and checks that the edit survived.
// Usage: tracker_engine <module> <output.it>

#include "stdafx.h"
#include "../../platform/Platform.h"
#include "../soundlib/Sndfile.h"
#include "../soundlib/AudioReadTarget.h"
#include "openmpt/soundbase/Dither.hpp"
#include "mpt/io_read/filecursor_stdstream.hpp"

#include <cmath>
#include <fstream>
#include <iostream>
#include <vector>

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

} // namespace

int main(int argc, char **argv)
{
	if(argc < 3)
	{
		std::cerr << "usage: tracker_engine <module> <output.it>\n";
		return 2;
	}

	// Start the application services (settings, sound devices) the engine
	// uses in tracker configuration, as OpenMPT.exe does at start-up.
	if(!linopenmpt::StartApp())
	{
		std::cerr << "FAIL: could not start application services\n";
		return 1;
	}
	struct AppExit { ~AppExit() { linopenmpt::StopApp(); } } appExit;

	auto sndFile = Load(argv[1]);
	if(!sndFile)
	{
		std::cerr << "FAIL: could not load " << argv[1] << "\n";
		return 1;
	}
	std::cout << "loaded:   " << sndFile->GetNumChannels() << " channels, "
	          << sndFile->Patterns.GetNumPatterns() << " patterns, "
	          << sndFile->GetNumSamples() << " samples\n";

	const double rms = RenderRMS(*sndFile, 2.0);
	std::cout << "rendered: rms " << rms << "\n";
	if(rms <= 0.0)
	{
		std::cerr << "FAIL: silent render\n";
		return 1;
	}

	// Convert to IT (as File > Save As IT does), then edit pattern 0, row 3,
	// channel 0: C-5, instrument 1, volume column v32, effect A06 (speed 6).
	sndFile->ChangeModTypeTo(MOD_TYPE_IT);
	if(!sndFile->Patterns.IsValidPat(0))
	{
		std::cerr << "FAIL: no pattern 0\n";
		return 1;
	}
	ModCommand *m = sndFile->Patterns[0].GetpModCommand(3, 0);
	m->note = NOTE_MIDDLEC;
	m->instr = 1;
	m->volcmd = VOLCMD_VOLUME;
	m->vol = 32;
	m->command = CMD_SPEED;
	m->param = 6;

	{
		std::ofstream out(argv[2], std::ios::binary | std::ios::trunc);
		if(!sndFile->SaveIT(out, mpt::PathString::FromUTF8(argv[2])))
		{
			std::cerr << "FAIL: SaveIT\n";
			return 1;
		}
	}
	std::cout << "saved:    " << argv[2] << "\n";

	auto reloaded = Load(argv[2]);
	if(!reloaded || reloaded->GetType() != MOD_TYPE_IT)
	{
		std::cerr << "FAIL: could not reload saved file as IT\n";
		return 1;
	}
	const ModCommand &r = *reloaded->Patterns[0].GetpModCommand(3, 0);
	if(r.note != NOTE_MIDDLEC || r.instr != 1 || r.volcmd != VOLCMD_VOLUME || r.vol != 32 || r.command != CMD_SPEED || r.param != 6)
	{
		std::cerr << "FAIL: edit did not survive save/reload (note " << int(r.note) << ", instr " << int(r.instr)
		          << ", volcmd " << int(r.volcmd) << ", vol " << int(r.vol) << ", command " << int(r.command) << ", param " << int(r.param) << ")\n";
		return 1;
	}
	std::cout << "reloaded: edit preserved\nOK\n";
	return 0;
}
