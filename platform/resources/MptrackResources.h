// Upstream's resources (mptrack.rc) linked into the binary.
#pragma once

// Makes them the executable's resources (GetModuleHandle(nullptr)), for
// FindResource(), LoadString(), LoadIcon() and the rest. Call once at startup.
void LinOpenMPTRegisterMptrackResources();
