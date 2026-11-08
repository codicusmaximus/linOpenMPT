// linOpenMPT compatibility layer: <Dbt.h> (WM_DEVICECHANGE events). The
// tracker reopens MIDI input when devices change; the platform layer sends
// DBT_DEVNODES_CHANGED when ALSA reports a new or removed MIDI port.
#pragma once
#include "linopenmpt/win32_api.h"
#define DBT_DEVNODES_CHANGED 0x0007
#define DBT_DEVICEARRIVAL 0x8000
#define DBT_DEVICEREMOVECOMPLETE 0x8004
