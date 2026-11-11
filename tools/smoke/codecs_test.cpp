// Smoke test for step 4b: the optional libraries upstream's Windows build
// has. Renders two seconds of a module, writes it with each stream export
// encoder (FLAC, MP3, Opus, Vorbis) with the encoder's default settings,
// reads each file back as a sample and checks that sound survived. Then
// loads a module from a ZIP archive.
// Usage: codecs_test <module> <work dir> [<zip with a module>]

#include "stdafx.h"
#include "../../platform/Platform.h"
#include "../soundlib/Sndfile.h"
#include "../soundlib/AudioReadTarget.h"
#include "openmpt/soundbase/Dither.hpp"
#include "openmpt/soundfile_data/tags.hpp"
#include "openmpt/streamencoder/StreamEncoder.hpp"
#include "openmpt/streamencoder/StreamEncoderFLAC.hpp"
#include "openmpt/streamencoder/StreamEncoderMP3.hpp"
#include "openmpt/streamencoder/StreamEncoderOpus.hpp"
#include "openmpt/streamencoder/StreamEncoderVorbis.hpp"
#include "mpt/io_read/filecursor_stdstream.hpp"
#include "../common/ComponentManager.h"

#include <algorithm>
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

std::vector<float> Render(CSoundFile &sndFile, uint32 rate, double seconds)
{
	MixerSettings mixer = sndFile.m_MixerSettings;
	mixer.gdwMixingFreq = rate;
	mixer.gnChannels = 2;
	sndFile.SetMixerSettings(mixer);
	sndFile.InitPlayer(true);
	DithersWrapperOpenMPT dithers(mpt::global_prng(), DithersWrapperOpenMPT::DefaultDither, 2);
	std::vector<float> out;
	std::vector<float> buffer(2 * 1024);
	while(out.size() < static_cast<std::size_t>(seconds * rate) * 2)
	{
		AudioTargetBuffer<mpt::audio_span_interleaved<float>> target(mpt::audio_span_interleaved<float>(buffer.data(), 2, 1024), dithers);
		const auto n = sndFile.Read(1024, target);
		if(n == 0)
			break;
		out.insert(out.end(), buffer.begin(), buffer.begin() + 2 * n);
	}
	return out;
}

// Encodes with the encoder's defaults, as the export dialog starts out.
bool Encode(const EncoderFactoryBase &factory, const std::string &path, const std::vector<float> &audio)
{
	const Encoder::Traits &traits = factory.GetTraits();
	Encoder::Settings settings{};
	settings.Cues = false;
	settings.Tags = true;
	settings.Samplerate = traits.defaultSamplerate;
	settings.Channels = 2;
	settings.Mode = traits.defaultMode;
	settings.Bitrate = traits.defaultBitrate;
	settings.Quality = traits.defaultQuality;
	settings.Format = traits.defaultFormat;
	settings.Dither = traits.defaultDitherType;
	settings.Details = Encoder::StreamSettings{};
	FileTags tags;
	tags.title = U_("codecs_test");
	std::ofstream file(path, std::ios::binary | std::ios::trunc);
	mpt::any_engine_wrapper<std::remove_reference_t<decltype(mpt::global_prng())>, uint64> prng{mpt::global_prng()};
	std::unique_ptr<IAudioStreamEncoder> encoder = factory.ConstructStreamEncoder(file, settings, tags, prng);
	if(!encoder)
		return false;
	// In the encoder's sample format, as the export dialog writes it.
	const std::size_t frames = audio.size() / 2;
	const auto convert = [&](auto sample, double scale) {
		using T = decltype(sample);
		std::vector<T> out(audio.size());
		for(std::size_t i = 0; i < audio.size(); i++)
			out[i] = static_cast<T>(static_cast<int64>(std::clamp(static_cast<double>(audio[i]), -1.0, 1.0 - 1.0 / scale) * scale));
		encoder->WriteInterleaved(frames, out.data());
	};
	switch(settings.Format.GetSampleFormat())
	{
	case SampleFormat::Int8: convert(int8{}, 128.0); break;
	case SampleFormat::Int16: convert(int16{}, 32768.0); break;
	case SampleFormat::Int24: convert(int24{}, 8388608.0); break;
	case SampleFormat::Int32: convert(int32{}, 2147483648.0); break;
	default: encoder->WriteInterleaved(frames, audio.data()); break;
	}
	encoder->WriteFinalize();
	encoder.reset();
	return file.good();
}

// Reads the file back as sample 1 of an empty module; its RMS level.
double ReadBack(const std::string &path, SmpLength &length)
{
	std::ifstream f(path, std::ios::binary);
	auto sndFile = std::make_unique<CSoundFile>();
	sndFile->Create(MOD_TYPE_MPT, 4);
	sndFile->m_nSamples = 1;
	FileReader file = mpt::IO::make_FileCursor<mpt::PathString>(f);
	length = 0;
	if(!sndFile->ReadSampleFromFile(1, file, false, false))
		return -1.0;
	const ModSample &sample = sndFile->GetSample(1);
	length = sample.nLength;
	double energy = 0.0;
	const std::size_t count = static_cast<std::size_t>(sample.nLength) * sample.GetNumChannels();
	for(std::size_t i = 0; i < count; i++)
	{
		const double v = sample.uFlags[CHN_16BIT] ? sample.sample16()[i] / 32768.0 : sample.sample8()[i] / 128.0;
		energy += v * v;
	}
	return count ? std::sqrt(energy / count) : 0.0;
}

} // namespace

int main(int argc, char *argv[])
{
	if(argc < 3)
	{
		std::cerr << "usage: codecs_test <module> <work dir> [<zip with a module>]\n";
		return 2;
	}
	if(!linopenmpt::StartApp())
	{
		std::cerr << "cannot start application services\n";
		return 1;
	}
	struct AppExit { ~AppExit() { linopenmpt::StopApp(); } } appExit;
	// As CTrackApp::InitInstance: MP3 decoding goes through a component.
	ComponentManagerSettingsDefault componentSettings;
	ComponentManager::Init(componentSettings);
	auto sndFile = Load(argv[1]);
	if(!sndFile)
	{
		std::cerr << "cannot load " << argv[1] << "\n";
		return 1;
	}

	const FLACEncoder flac;
	const MP3Encoder mp3(MP3EncoderLame);
	const OggOpusEncoder opus;
	const VorbisEncoder vorbis;
	const struct
	{
		const char *name;
		const EncoderFactoryBase *factory;
	} encoders[] = {{"flac", &flac}, {"mp3", &mp3}, {"opus", &opus}, {"ogg", &vorbis}};

	int failures = 0;
	for(const auto &e : encoders)
	{
		if(!e.factory->IsAvailable())
		{
			std::cerr << e.name << ": encoder not available\n";
			failures++;
			continue;
		}
		const uint32 rate = e.factory->GetTraits().defaultSamplerate;
		// From the start of the module each time.
		sndFile = Load(argv[1]);
		const std::vector<float> audio = Render(*sndFile, rate, 2.0);
		const std::string path = std::string(argv[2]) + "/codecs_test." + e.name;
		if(!Encode(*e.factory, path, audio))
		{
			std::cerr << e.name << ": encoding failed\n";
			failures++;
			continue;
		}
		SmpLength length = 0;
		const double rms = ReadBack(path, length);
		std::cout << e.name << ": " << length << " frames, RMS " << rms << "\n";
		// Lossy encoders pad and trim a little; the length must be close.
		if(rms <= 0.001 || length < rate * 3 / 2 || length > rate * 5 / 2)
		{
			std::cerr << e.name << ": read back wrong\n";
			failures++;
		}
	}

	if(argc > 3)
	{
		auto zipped = Load(argv[3]);
		if(!zipped || zipped->Patterns.GetNumPatterns() == 0)
		{
			std::cerr << "zip: cannot load " << argv[3] << "\n";
			failures++;
		} else
		{
			std::cout << "zip: " << zipped->GetTitle() << "\n";
		}
	}
	return failures ? 1 : 0;
}
