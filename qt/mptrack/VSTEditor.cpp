/*
 * VSTEditor.cpp
 * -------------
 * Purpose: Implementation of the custom plugin editor window that is used if a plugin provides an own editor GUI.
 * Notes  : mfcless twin of upstream's mptrack/VSTEditor.cpp.
 *          Draft from qt/tools/twin.py, not yet edited by hand.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "VSTEditor.h"
#include "mfcless/Widgets.h"
#include "resource.h"
#include "Vstplug.h"


OPENMPT_NAMESPACE_BEGIN


#ifdef MPT_WITH_VST

void COwnerVstEditor::RegisterMessageMap()
{
	CAbstractVstEditor::RegisterMessageMap();
	// ON_WM_ERASEBKGND(): by hand (Qt event override)
	// ON_WM_PAINT() (mfcless::Window)
	// Messages from plugin bridge to check whether a key would be handled by OpenMPT
	// We need an offset to WM_USER because the editor window receives some spurious WM_USER messages (from MFC?) when it gets activated
	AddMessage(WM_USER + 4000 + mfcless::msg::KeyDown - mfcless::msg::KeyFirst, &COwnerVstEditor::OnPreTranslateKeyDown);
	AddMessage(WM_USER + 4000 + mfcless::msg::KeyUp - mfcless::msg::KeyFirst, &COwnerVstEditor::OnPreTranslateKeyUp);
	AddMessage(WM_USER + 4000 + mfcless::msg::SysKeyDown - mfcless::msg::KeyFirst, &COwnerVstEditor::OnPreTranslateSysKeyDown);
	AddMessage(WM_USER + 4000 + mfcless::msg::SysKeyUp - mfcless::msg::KeyFirst, &COwnerVstEditor::OnPreTranslateSysKeyUp);
}


COwnerVstEditor::COwnerVstEditor(CVstPlugin &plugin) : CAbstractVstEditor{plugin} {}


void COwnerVstEditor::OnPaint()
{
	CAbstractVstEditor::OnPaint();
	auto &plugin = static_cast<const CVstPlugin &>(m_VstPlugin);
	if(plugin.isBridged)
	{
		// Force redrawing for the plugin window in the bridged process.
		// Otherwise, bridged plugin GUIs will not always be refreshed properly (e.g. when restoring OpenMPT from minimized state).
		// Synth1 is a good candidate for testing this behaviour.
		mfcless::Rect rect;
		if(m_plugWindow->GetUpdateRect(&rect, false))
		{
			QWidget *child = m_plugWindow->GetWindow(GW_CHILD | GW_HWNDFIRST);
			if(child)
				child->RedrawWindow(&rect, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW);
		}
	} else
	{
		// For plugins that change their size without telling the host through audioMasterSizeWindow, e.g. Roland D-50
		mfcless::Rect rect;
		m_plugWindow->GetClientRect(rect);
		if(rect.Width() != m_width || rect.Height() != m_height)
		{
			SetSize(rect.Width(), rect.Height());
		}
	}
}


void COwnerVstEditor::UpdateParamDisplays()
{
	CAbstractVstEditor::UpdateParamDisplays();
	// We trust that the plugin GUI can update its display with a bit of idle time.
	static_cast<CVstPlugin &>(m_VstPlugin).Dispatch(Vst::effEditIdle, 0, 0, nullptr, 0.0f);
}

bool COwnerVstEditor::OpenEditor(QWidget *parent)
{
	Create(IDD_PLUGINEDITOR, parent);

	// Some plugins (e.g. ProteusVX) need to be planted into another control or else they will break our window proc, making the window unusable.
	m_plugWindow->Create(nullptr, WS_CHILD | WS_VISIBLE, mfcless::Rect(0, 0, 100, 100), this);

	// Set editor window size
	Vst::ERect rect{};
	Vst::ERect *pRect = nullptr;
	CVstPlugin &vstPlug = static_cast<CVstPlugin &>(m_VstPlugin);
	vstPlug.Dispatch(Vst::effEditGetRect, 0, 0, &pRect, 0);
	if(pRect) rect = *pRect;
#if MPT_OS_WINDOWS
	vstPlug.Dispatch(Vst::effEditOpen, 0, 0, m_plugWindow, 0);
#else
	vstPlug.Dispatch(Vst::effEditOpen, 0, 0, reinterpret_cast<void *>(MfcQtNativeWindowId(m_plugWindow)), 0);
#endif
	vstPlug.Dispatch(Vst::effEditGetRect, 0, 0, &pRect, 0);
	if(pRect) rect = *pRect;
	if(rect.right > rect.left && rect.bottom > rect.top)
	{
		// Plugin provided valid window size.
		SetSize(rect.Width(), rect.Height());
	}

	vstPlug.Dispatch(Vst::effEditTop, 0,0, nullptr, 0.0f);
	vstPlug.Dispatch(Vst::effEditIdle, 0,0, nullptr, 0.0f);

	// Set knob mode to linear (2) instead of circular (0) for those plugins that support it (e.g. Steinberg VB-1)
	vstPlug.Dispatch(Vst::effSetEditKnobMode, 0, 2, nullptr, 0.0f);

	return CAbstractVstEditor::OpenEditor(parent);
}


void COwnerVstEditor::DoClose()
{
	// Prevent some plugins from storing a bogus window size (e.g. Electri-Q)
	ShowWindow(false);
	if(m_isMinimized) OnNcLButtonDblClk(HTCAPTION, mfcless::Point(0, 0));
	static_cast<CVstPlugin &>(m_VstPlugin).Dispatch(Vst::effEditClose, 0, 0, nullptr, 0.0f);
	CAbstractVstEditor::DoClose();
}


bool COwnerVstEditor::SetSize(int contentWidth, int contentHeight)
{
	if(contentWidth < 0 || contentHeight < 0 || !this)
	{
		return false;
	}
	m_width = contentWidth;
	m_height = contentHeight;

	mfcless::Rect rcWnd, rcClient;

	// Get border / menu size.
	GetWindowRect(&rcWnd);
	GetClientRect(&rcClient);

	// Narrow plugin GUIs may force the number of menu bar lines (and thus the required window height) to change
	WindowSizeAdjuster adjuster(*this);

	const int windowWidth = rcWnd.Width() - rcClient.Width() + contentWidth;
	const int windowHeight = rcWnd.Height() - rcClient.Height() + contentHeight;
	SetWindowPos(NULL, 0, 0,
		windowWidth, windowHeight,
		mfcless::swp::NoZOrder | mfcless::swp::NoMove | mfcless::swp::NoActivate);
	m_plugWindow->SetWindowPos(NULL, 0, 0,
		contentWidth, contentHeight,
		mfcless::swp::NoZOrder | mfcless::swp::NoMove | mfcless::swp::NoActivate);

	return true;
}


#endif // MPT_WITH_VST


OPENMPT_NAMESPACE_END
