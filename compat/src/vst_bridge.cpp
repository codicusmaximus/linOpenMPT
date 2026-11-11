// linOpenMPT: the host side of upstream's plugin bridge (pluginBridge/
// BridgeWrapper.cpp) for Linux. The bridge runs Windows plugins in a
// separate Windows process, so it is never available here: Create() reports
// it as missing and upstream then loads the plugin in-process. What remains
// is telling native plugins (ELF shared objects of this process's
// architecture) from the rest.
// Compiled as part of mpt_common (tracker configuration).
#include "stdafx.h"

#ifdef MPT_WITH_VST

#include "pluginBridge/BridgeWrapper.h"

#include <fstream>

OPENMPT_NAMESPACE_BEGIN

std::size_t GetPluginArchPointerSize(PluginArch arch)
{
	switch(arch)
	{
	case PluginArch_x86: return 4;
	case PluginArch_amd64: return 8;
	case PluginArch_arm: return 4;
	case PluginArch_arm64: return 8;
	default: return 0;
	}
}

ComponentPluginBridge::ComponentPluginBridge(PluginArch arch, Generation generation)
	: ComponentBase(ComponentTypeBundled)
	, arch(arch)
	, generation(generation)
{
}

bool ComponentPluginBridge::DoInitialize()
{
	availability = AvailabilityMissing;
	return false;
}

PluginArch BridgeWrapper::GetNativePluginBinaryType()
{
#if defined(__x86_64__)
	return PluginArch_amd64;
#elif defined(__i386__)
	return PluginArch_x86;
#elif defined(__aarch64__)
	return PluginArch_arm64;
#elif defined(__arm__)
	return PluginArch_arm;
#else
	return PluginArch_unknown;
#endif
}

// The ELF header's machine field. Anything else (a Windows DLL, a text
// file) is of unknown architecture, which upstream treats as not native.
PluginArch BridgeWrapper::GetPluginBinaryType(const mpt::PathString &pluginPath)
{
	std::ifstream file(pluginPath.AsNative().c_str(), std::ios::binary);
	unsigned char header[20] = {};
	if(!file.read(reinterpret_cast<char *>(header), sizeof(header)))
		return PluginArch_unknown;
	if(header[0] != 0x7F || header[1] != 'E' || header[2] != 'L' || header[3] != 'F')
		return PluginArch_unknown;
	const bool is64 = header[4] == 2;
	const bool littleEndian = header[5] == 1;
	const uint16 machine = littleEndian ? static_cast<uint16>(header[18] | (header[19] << 8)) : static_cast<uint16>((header[18] << 8) | header[19]);
	switch(machine)
	{
	case 3: return is64 ? PluginArch_unknown : PluginArch_x86;      // EM_386
	case 62: return is64 ? PluginArch_amd64 : PluginArch_unknown;   // EM_X86_64
	case 40: return is64 ? PluginArch_unknown : PluginArch_arm;     // EM_ARM
	case 183: return is64 ? PluginArch_arm64 : PluginArch_unknown;  // EM_AARCH64
	default: return PluginArch_unknown;
	}
}

Vst::AEffect *BridgeWrapper::Create(const VSTPluginLib &, bool)
{
	throw BridgeNotFoundException();
}

OPENMPT_NAMESPACE_END

#endif // MPT_WITH_VST
