/*
 * VSTEditor.cpp
 * -------------
 * Purpose: Implementation of the custom plugin editor window that is used if a plugin provides an own editor GUI.
 * Notes  : mfcless twin of upstream's mptrack/VSTEditor.cpp.
 *          Hand-edited from the twin.py draft.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "VSTEditor.h"
#include "mfcless/Widgets.h"
#include "resource.h"
#include "Vstplug.h"
#include "mfcless/ForeignWindows.h"


OPENMPT_NAMESPACE_BEGIN


#ifdef MPT_WITH_VST

void COwnerVstEditor::RegisterMessageMap()
{
	CAbstractVstEditor::RegisterMessageMap();
	// Messages from plugin bridge to check whether a key would be handled by OpenMPT
	// (upstream: WM_USER + 4000 + the key message's offset from WM_KEYFIRST).
	constexpr uint32 WM_USER = 0x0400;
	AddMessage(WM_USER + 4000 + mfcless::msg::KeyDown - mfcless::msg::KeyFirst, &COwnerVstEditor::OnPreTranslateKeyDown);
	AddMessage(WM_USER + 4000 + mfcless::msg::KeyUp - mfcless::msg::KeyFirst, &COwnerVstEditor::OnPreTranslateKeyUp);
	AddMessage(WM_USER + 4000 + mfcless::msg::SysKeyDown - mfcless::msg::KeyFirst, &COwnerVstEditor::OnPreTranslateSysKeyDown);
	AddMessage(WM_USER + 4000 + mfcless::msg::SysKeyUp - mfcless::msg::KeyFirst, &COwnerVstEditor::OnPreTranslateSysKeyUp);
}


COwnerVstEditor::COwnerVstEditor(CVstPlugin &plugin) : CAbstractVstEditor{plugin} {}


void COwnerVstEditor::OnPaint()
{
	CAbstractVstEditor::OnPaint();
	// mfcless: a plugin editor draws into its own X11 window, which the X
	// server redraws by itself; upstream's forced redraw of a bridged
	// plugin's window is not needed. A plugin that changes the size of its
	// window without telling the host through audioMasterSizeWindow resizes
	// ours (the window it was given) here.
	auto &plugin = static_cast<const CVstPlugin &>(m_VstPlugin);
	if(!plugin.isBridged && m_plugWindow)
	{
		const QSize size = m_plugWindow->size();
		if(size.width() != m_width || size.height() != m_height)
			SetSize(size.width(), size.height());
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
	m_plugWindow = new QWidget(this);
	m_plugWindow->setGeometry(0, 0, 100, 100);
	m_plugWindow->show();

	// Set editor window size
	Vst::ERect rect{};
	Vst::ERect *pRect = nullptr;
	CVstPlugin &vstPlug = static_cast<CVstPlugin &>(m_VstPlugin);
	vstPlug.Dispatch(Vst::effEditGetRect, 0, 0, &pRect, 0);
	if(pRect) rect = *pRect;
	vstPlug.Dispatch(Vst::effEditOpen, 0, 0, reinterpret_cast<void *>(mfcless::NativeWindowId(m_plugWindow)), 0);
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
	if(m_isMinimized) showNormal();
	static_cast<CVstPlugin &>(m_VstPlugin).Dispatch(Vst::effEditClose, 0, 0, nullptr, 0.0f);
	CAbstractVstEditor::DoClose();
}


bool COwnerVstEditor::SetSize(int contentWidth, int contentHeight)
{
	if(contentWidth < 0 || contentHeight < 0 || !m_plugWindow)
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
	m_plugWindow->resize(contentWidth, contentHeight);

	return true;
}


#endif // MPT_WITH_VST


OPENMPT_NAMESPACE_END
