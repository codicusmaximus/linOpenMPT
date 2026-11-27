// mfcless base: system facts that upstream asks Windows for.

#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "mpt/base/integer.hpp"

// Everything of mfcless is in the global namespace mfcless, never in
// OpenMPT's, so that "mfcless::" means the same inside and outside it.
namespace mfcless
{
using uint8 = std::uint8_t;
using uint32 = std::uint32_t;

// A colour in Windows' COLORREF layout (0x00BBGGRR), which OpenMPT's settings
// files and colour schemes store.
constexpr uint32 Rgb(uint8 r, uint8 g, uint8 b) noexcept
{
	return static_cast<uint32>(r) | (static_cast<uint32>(g) << 8) | (static_cast<uint32>(b) << 16);
}
constexpr uint8 RedOf(uint32 c) noexcept { return static_cast<uint8>(c & 0xFF); }
constexpr uint8 GreenOf(uint32 c) noexcept { return static_cast<uint8>((c >> 8) & 0xFF); }
constexpr uint8 BlueOf(uint32 c) noexcept { return static_cast<uint8>((c >> 16) & 0xFF); }

// Milliseconds since an arbitrary start, wrapping as Windows' tick counts do.
uint32 TickCount();
// The calling thread's ID (as the system numbers threads).
uint32 CurrentThreadId();
void SleepMs(uint32 milliseconds);
// The size of the desktop spanning all screens (SM_CXVIRTUALSCREEN, SM_CYVIRTUALSCREEN).
int VirtualScreenWidth();
int VirtualScreenHeight();

// The desktop's colours for the roles upstream reads with GetSysColor, from
// the Qt style's palette.
enum class SysColor
{
	ButtonFace,
	ButtonShadow,
	ButtonHighlight,
	ButtonText,
	Window,
	WindowText,
	Highlight,
	HighlightText,
	GrayText,
};
uint32 SystemColor(SysColor role);

// Where user data that is not configuration goes: $XDG_DATA_HOME/OpenMPT
// (normally ~/.local/share/OpenMPT).
OPENMPT_NAMESPACE::mpt::PathString DataDirectory();

} // namespace mfcless
