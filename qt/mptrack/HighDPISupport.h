/*
 * HighDPISupport.h
 * ----------------
 * Purpose: Various helpers for making the GUI scale properly on high-DPI displays.
 * Notes  : mfcless twin of upstream's mptrack/HighDPISupport.h. Qt measures
 *          widgets in device-independent pixels and scales them to the
 *          screen itself, so for the twins every window is at 96 DPI and the
 *          scaling helpers return their input. Window placement is Qt's
 *          saveGeometry() (CMainFrame::SaveWindowGeometry).
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"

class QFont;
class QWidget;

OPENMPT_NAMESPACE_BEGIN

namespace HighDPISupport
{
	enum class Mode
	{
		LowDpi,
		LowDpiUpscaled,
		HighDpiSystem,
		HighDpiPerMonitor,
	};

	inline void SetDPIAwareness(Mode) { }

	inline uint32 GetDpiForWindow(const QWidget *) { return 96; }

	void CreateGUIFont(QFont &font, const QWidget *hwnd);

	// Applies DPI scaling factor to some given size
	MPT_FORCEINLINE int ScalePixels(int pixels, const QWidget *)
	{
		return pixels;
	}

	// Removes DPI scaling factor from some given size
	MPT_FORCEINLINE int ScalePixelsInv(int pixels, const QWidget *)
	{
		return pixels;
	}

	class DPIAwarenessBypass
	{
	public:
		DPIAwarenessBypass(Mode = Mode::HighDpiSystem) { }
		~DPIAwarenessBypass() { }
	};
};

OPENMPT_NAMESPACE_END
