// Smoke test: load a module with the upstream soundlib (player configuration)
// and render it to memory. Proves the upstream core compiles, links and plays
// correctly on Linux. Usage: render_module <file> [seconds] [--allow-silent]
// Some upstream test files (test.xm, test.mptm) are structural tests and are
// silent by design (the official openmpt123 renders them as silence too), so
// they are checked with --allow-silent.

#include <libopenmpt/libopenmpt.hpp>

#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char **argv)
{
	if(argc < 2)
	{
		std::cerr << "usage: render_module <file> [seconds]\n";
		return 2;
	}
	const double seconds = argc > 2 ? std::atof(argv[2]) : 5.0;
	const bool allowSilent = argc > 3 && std::string(argv[3]) == "--allow-silent";
	std::ifstream file(argv[1], std::ios::binary);
	if(!file)
	{
		std::cerr << "cannot open " << argv[1] << "\n";
		return 1;
	}
	try
	{
		openmpt::module mod(file);
		std::cout << "type:     " << mod.get_metadata("type_long") << "\n"
		          << "title:    " << mod.get_metadata("title") << "\n"
		          << "channels: " << mod.get_num_channels() << "\n"
		          << "patterns: " << mod.get_num_patterns() << "\n"
		          << "samples:  " << mod.get_num_samples() << "\n"
		          << "duration: " << mod.get_duration_seconds() << " s\n";
		constexpr int rate = 48000;
		std::vector<float> left(1024), right(1024);
		std::size_t frames = 0;
		double energy = 0.0;
		while(frames < static_cast<std::size_t>(seconds * rate))
		{
			std::size_t n = mod.read(rate, left.size(), left.data(), right.data());
			if(n == 0)
				break;
			for(std::size_t i = 0; i < n; ++i)
				energy += left[i] * left[i] + right[i] * right[i];
			frames += n;
		}
		const double rms = frames ? std::sqrt(energy / (2.0 * frames)) : 0.0;
		std::cout << "rendered: " << frames << " frames, rms " << rms << "\n";
		if(frames == 0 || (rms <= 0.0 && !allowSilent))
		{
			std::cerr << "FAIL: no audio rendered\n";
			return 1;
		}
		std::cout << "OK\n";
		return 0;
	} catch(const std::exception &e)
	{
		std::cerr << "FAIL: " << e.what() << "\n";
		return 1;
	}
}
