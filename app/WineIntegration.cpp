// linOpenMPT: the Wine integration of upstream's Windows build
// (mptrack/MPTrackWine.cpp), on a native Linux build. That integration lets
// OpenMPT running under Wine reach the host's sound APIs through a wrapper
// library built at runtime; a native build reaches them directly, so the
// wrapper component exists but is never available, as on Windows proper,
// and upstream's WineSoundDeviceStub (compiled unchanged) lists no devices.
#include "stdafx.h"

#include "MPTrackWine.h"

OPENMPT_NAMESPACE_BEGIN

namespace WineIntegration
{

// Upstream calls these only when mpt::OS::Windows::IsWine().
void Initialize() { }
bool IsSupported() { return false; }
bool IsCompiled() { return false; }
void Load() { }

} // namespace WineIntegration

ComponentWineWrapper::ComponentWineWrapper()
	: ComponentLibrary(ComponentTypeBundled)
{
}

bool ComponentWineWrapper::DoInitialize()
{
	return false;
}

ComponentWineWrapper::~ComponentWineWrapper() = default;

std::string ComponentWineWrapper::result_as_string(char *str) const
{
	std::string result = str != nullptr ? str : "";
	if(str != nullptr && OpenMPT_Wine_Wrapper_String_Free != nullptr)
		OpenMPT_Wine_Wrapper_String_Free(str);
	return result;
}

OPENMPT_NAMESPACE_END
