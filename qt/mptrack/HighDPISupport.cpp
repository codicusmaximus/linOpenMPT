/*
 * HighDPISupport.cpp
 * ------------------
 * Purpose: Various helpers for making high-DPI and mixed-DPI support easier.
 * Notes  : mfcless twin of upstream's mptrack/HighDPISupport.cpp.
 *          Draft from qt/tools/twin.py, not yet edited by hand.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "HighDPISupport.h"
#include "mfcless/Widgets.h"
#include "../misc/mptLibrary.h"


OPENMPT_NAMESPACE_BEGIN


struct HighDPISupportData
{
	HighDPISupportData()
		: m_user32{mpt::LibraryPath::System(P_("user32"))}
	{
		m_user32.Bind(m_SetThreadDpiAwarenessContext, "SetThreadDpiAwarenessContext");
		m_user32.Bind(m_GetDpiForWindow, "GetDpiForWindow");
		m_user32.Bind(m_GetSystemMetricsForDpi, "GetSystemMetricsForDpi");
#if MPT_WINNT_AT_LEAST(MPT_WIN_VISTA)  // Pre-Vista NONCLIENTMETRICS struct is not compatible with SystemParametersInfoForDpi
		m_user32.Bind(m_SystemParametersInfoForDpi, "SystemParametersInfoForDpi");
#endif
		m_user32.Bind(m_AdjustWindowRectExForDpi, "AdjustWindowRectExForDpi");
	}

	mpt::Library m_user32;

	using PSETTHREADDPIAWARENESSCONTEXT = void *(WINAPI *)(void *);
	PSETTHREADDPIAWARENESSCONTEXT m_SetThreadDpiAwarenessContext = nullptr;

	using PGETDPIFORWINDOW = uint32(WINAPI *)(QWidget *);
	PGETDPIFORWINDOW m_GetDpiForWindow = nullptr;

	using PGETSYSTEMMETRICSFORPDI = int(WINAPI *)(int, uint32);
	PGETSYSTEMMETRICSFORPDI m_GetSystemMetricsForDpi = nullptr;

	using PSYSTEMPARAMETERSINFOFORPDI = bool(WINAPI *)(uint32, uint32, void *, uint32, uint32);
	PSYSTEMPARAMETERSINFOFORPDI m_SystemParametersInfoForDpi = nullptr;

	using PADJUSTWINDOWRECTEXFORDPI = bool(WINAPI *)(mfcless::Rect *, uint32, bool, uint32, uint32);
	PADJUSTWINDOWRECTEXFORDPI m_AdjustWindowRectExForDpi = nullptr;
};


enum MPT_DPI_AWARENESS_CONTEXT : intptr_t
{
	MPT_DPI_AWARENESS_CONTEXT_UNAWARE = -1,
	MPT_DPI_AWARENESS_CONTEXT_SYSTEM_AWARE = -2,
	MPT_DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE = -3,
	MPT_DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 = -4,  // Windows 10 version 1703 and newer
	MPT_DPI_AWARENESS_CONTEXT_UNAWARE_GDISCALED = -5,     // Windows 10 version 1809 update and newer
};


static void * DPIModeToContext(HighDPISupport::Mode mode)
{
	switch (mode)
	{
	case HighDPISupport::Mode::LowDpi: return void *(MPT_DPI_AWARENESS_CONTEXT_UNAWARE);
	case HighDPISupport::Mode::LowDpiUpscaled: return void *(MPT_DPI_AWARENESS_CONTEXT_UNAWARE_GDISCALED);
	case HighDPISupport::Mode::HighDpiSystem: return void *(MPT_DPI_AWARENESS_CONTEXT_SYSTEM_AWARE);
	case HighDPISupport::Mode::HighDpiPerMonitor: return void *(MPT_DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
	}
	MPT_ASSERT_NOTREACHED();
	return void *(MPT_DPI_AWARENESS_CONTEXT_UNAWARE);
}


static HighDPISupportData *GetHighDPISupportData()
{
	static std::unique_ptr<HighDPISupportData> highDPISupportData;

	if(!highDPISupportData)
		highDPISupportData = std::make_unique<HighDPISupportData>();
	return highDPISupportData.get();
}


void HighDPISupport::SetDPIAwareness(Mode mode)
{
	auto instance = GetHighDPISupportData();
	// For Windows 10, Creators Update (1703) and newer
	if(instance->m_user32.IsValid())
	{
		using PSETPROCESSDPIAWARENESSCONTEXT = bool(WINAPI *)(void *);
		PSETPROCESSDPIAWARENESSCONTEXT SetProcessDpiAwarenessContext = nullptr;
		if(instance->m_user32.Bind(SetProcessDpiAwarenessContext, "SetProcessDpiAwarenessContext"))
		{
			if(SetProcessDpiAwarenessContext(DPIModeToContext(mode)) == true)
				return;
		}
	}

	// For Windows 8.1 and newer
	const bool highDPI = (mode == Mode::HighDpiSystem || mode == Mode::HighDpiPerMonitor);
	mpt::Library shcore(mpt::LibraryPath::System(P_("SHCore")));
	if(shcore.IsValid())
	{
		using PSETPROCESSDPIAWARENESS = HRESULT(WINAPI *)(int);
		PSETPROCESSDPIAWARENESS SetProcessDPIAwareness = nullptr;
		if(shcore.Bind(SetProcessDPIAwareness, "SetProcessDpiAwareness"))
		{
			if(SetProcessDPIAwareness(highDPI ? 2 : 0) == S_OK)
				return;
		}
	}
	
	// For Vista and newer
	if(highDPI && instance->m_user32.IsValid())
	{
		using PSETPROCESSDPIAWARE = bool(WINAPI *)();
		PSETPROCESSDPIAWARE SetProcessDPIAware = nullptr;
		if(instance->m_user32.Bind(SetProcessDPIAware, "SetProcessDPIAware"))
			SetProcessDPIAware();
	}
}


uint32 HighDPISupport::GetDpiForWindow(QWidget * hwnd)
{
	if(auto instance = GetHighDPISupportData(); instance->m_GetDpiForWindow)
		return instance->m_GetDpiForWindow(hwnd);

	mfcless::DC * dc = ::GetDC(hwnd);
	uint32 dpi = ::GetDeviceCaps(dc, LOGPIXELSX);
	::ReleaseDC(hwnd, dc);
	return dpi;
}


int HighDPISupport::GetSystemMetrics(int index, uint32 dpi)
{
	if(auto instance = GetHighDPISupportData(); instance->m_GetSystemMetricsForDpi)
		return instance->m_GetSystemMetricsForDpi(index, dpi);
	else
		return ::GetSystemMetrics(index);
}


int HighDPISupport::GetSystemMetrics(int index, QWidget * hwnd)
{
	if(auto instance = GetHighDPISupportData(); instance->m_GetSystemMetricsForDpi)
		return instance->m_GetSystemMetricsForDpi(index, HighDPISupport::GetDpiForWindow(hwnd));
	else
		return ::GetSystemMetrics(index);
}


bool HighDPISupport::SystemParametersInfo(uint32 uiAction, uint32 uiParam, void *pvParam, uint32 fWinIni, uint32 dpi)
{
	if(auto instance = GetHighDPISupportData(); instance->m_SystemParametersInfoForDpi)
		return instance->m_SystemParametersInfoForDpi(uiAction, uiParam, pvParam, fWinIni, dpi);
	else
		return ::SystemParametersInfo(uiAction, uiParam, pvParam, fWinIni);
}


bool HighDPISupport::SystemParametersInfo(uint32 uiAction, uint32 uiParam, void *pvParam, uint32 fWinIni, QWidget * hwnd)
{
	if(auto instance = GetHighDPISupportData(); instance->m_SystemParametersInfoForDpi)
		return instance->m_SystemParametersInfoForDpi(uiAction, uiParam, pvParam, fWinIni, HighDPISupport::GetDpiForWindow(hwnd));
	else
		return ::SystemParametersInfo(uiAction, uiParam, pvParam, fWinIni);
}


bool HighDPISupport::AdjustWindowRectEx(mfcless::Rect * lpRect, uint32 dwStyle, bool bMenu, uint32 dwExStyle, uint32 dpi)
{
	if(auto instance = GetHighDPISupportData(); instance->m_AdjustWindowRectExForDpi)
		return instance->m_AdjustWindowRectExForDpi(lpRect, dwStyle, bMenu, dwExStyle, dpi);
	else
		return ::AdjustWindowRectEx(lpRect, dwStyle, bMenu, dwExStyle);
}


void HighDPISupport::CreateGUIFont(QFont &font, QWidget * hwnd)
{
	NONCLIENTMETRICS metrics;
	metrics.cbSize = sizeof(metrics);
	HighDPISupport::SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0, hwnd);
	font.DeleteObject();
	font.CreateFontIndirect(&metrics.lfMessageFont);
}


bool HighDPISupport::SetWindowPlacement(QWidget * hwnd, const mfcless::WindowPlacement &wpl)
{
	// https://blogs.windows.com/windowsdeveloper/2016/10/24/high-dpi-scaling-improvements-for-desktop-applications-and-mixed-mode-dpi-scaling-in-the-windows-10-anniversary-update/
	// What is not mentioned there: The first SetWindowPlacement call will restore the window to the wrong dimensions if it's not on the main screen...
	HighDPISupport::DPIAwarenessBypass bypass{HighDPISupport::Mode::LowDpi};
	::SetWindowPlacement(hwnd, &wpl);
	return ::SetWindowPlacement(hwnd, &wpl) != false;
}


bool HighDPISupport::GetWindowPlacement(QWidget * hwnd, mfcless::WindowPlacement &wpl)
{
	// https://blogs.windows.com/windowsdeveloper/2016/10/24/high-dpi-scaling-improvements-for-desktop-applications-and-mixed-mode-dpi-scaling-in-the-windows-10-anniversary-update/
	HighDPISupport::DPIAwarenessBypass bypass{HighDPISupport::Mode::LowDpi};
	wpl.length = sizeof(mfcless::WindowPlacement);
	return ::GetWindowPlacement(hwnd, &wpl) != false;
}


HighDPISupport::DPIAwarenessBypass::DPIAwarenessBypass(Mode forceMode)
{
	if(auto instance = GetHighDPISupportData(); instance->m_SetThreadDpiAwarenessContext)
		m_previous = instance->m_SetThreadDpiAwarenessContext(DPIModeToContext(forceMode));
}


HighDPISupport::DPIAwarenessBypass::~DPIAwarenessBypass()
{
	if(auto instance = GetHighDPISupportData(); instance->m_SetThreadDpiAwarenessContext)
		instance->m_SetThreadDpiAwarenessContext(m_previous);
}


OPENMPT_NAMESPACE_END
