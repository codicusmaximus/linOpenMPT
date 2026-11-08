// Smoke test: enumerate audio devices through OpenMPT's own sound device
// layer (src/openmpt/sounddevice), built natively for Linux.
// Lists JACK / PulseAudio / ALSA devices found via RtAudio and PulseAudio.
// Exits 0 even when no device is found (CI machines have no audio hardware).

#include "openmpt/all/BuildSettings.hpp"
#include "openmpt/sounddevice/SoundDevice.hpp"
#include "openmpt/sounddevice/SoundDeviceManager.hpp"
#include "Logging.h"
#include "mpt/osinfo/class.hpp"
#include "mpt/string_transcode/transcode.hpp"

#include <iostream>

int main()
{
	mpt::log::GlobalLogger logger;
	SoundDevice::SysInfo sysInfo(mpt::osinfo::get_class());
	SoundDevice::AppInfo appInfo;
	appInfo.SetName(MPT_USTRING("linOpenMPT"));
	SoundDevice::Manager manager(logger, sysInfo, appInfo);
	manager.ReEnumerate(true);
	std::size_t count = 0;
	for(const auto &info : manager)
	{
		std::cout << mpt::transcode<std::string>(mpt::common_encoding::utf8, info.apiName) << " | "
		          << mpt::transcode<std::string>(mpt::common_encoding::utf8, info.name)
		          << (info.default_ != SoundDevice::Info::Default::None ? "  (default)" : "") << "\n";
		++count;
	}
	std::cout << count << " device(s) found\n";
	return 0;
}
