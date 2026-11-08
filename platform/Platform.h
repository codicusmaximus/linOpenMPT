// linOpenMPT platform layer: entry points for programs built on the
// tracker engine (the smoke tests now, the Qt application later).
#pragma once

namespace linopenmpt
{
// Start the application services the engine relies on in tracker
// configuration (settings, tracker settings, sound device manager), the
// same work OpenMPT.exe does at start-up, without any windows.
// Settings live in $LINOPENMPT_CONFIG_DIR, else $XDG_CONFIG_HOME/linOpenMPT.
bool StartApp();
// Save settings and shut the services down.
void StopApp();
} // namespace linopenmpt
