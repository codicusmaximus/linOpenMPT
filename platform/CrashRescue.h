// linOpenMPT: saving the modified modules when the tracker crashes, as
// upstream's crash handler does on Windows (DebugReporter::RescueFiles).
// The crash handler (ExceptionHandler.cpp) makes the folder and calls the
// application's rescue function, which saves into it.
// Include after stdafx.h.
#pragma once

OPENMPT_NAMESPACE_BEGIN

namespace linopenmpt
{

// Saves the modified modules into folder (which ends with a slash) and
// returns how many it saved.
using CrashRescue = int (*)(const mpt::PathString &folder);
void SetCrashRescue(CrashRescue rescue);

} // namespace linopenmpt

OPENMPT_NAMESPACE_END
