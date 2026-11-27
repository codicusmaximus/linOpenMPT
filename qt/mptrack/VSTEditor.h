/*
 * VSTEditor.h
 * -----------
 * Purpose: Implementation of the custom plugin editor window that is used if a plugin provides an own editor GUI.
 * Notes  : mfcless twin of upstream's mptrack/VSTEditor.h.
 *          Draft from qt/tools/twin.py, not yet edited by hand.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "mfcless/Keyboard.h"

#include "AbstractVstEditor.h"

class QLabel;
class QWidget;

OPENMPT_NAMESPACE_BEGIN

#ifdef MPT_WITH_VST

class CVstPlugin;

class COwnerVstEditor : public CAbstractVstEditor
{
	Q_OBJECT
protected:
	QLabel *m_plugWindow = nullptr;
	int m_width = 0, m_height = 0;

public:
	COwnerVstEditor(CVstPlugin &plugin);
	~COwnerVstEditor() override = default;

	// Plugins may request to change the GUI size.
	bool IsResizable() const override { return true; }
	bool SetSize(int contentWidth, int contentHeight) override;

	void UpdateParamDisplays() override;

	bool OpenEditor(QWidget *parent) override;
	void DoClose() override;

protected:
	bool OnEraseBkgnd(CDC *) { return true; }
	void OnPaint();

	intptr_t OnPreTranslateKeyDown(uintptr_t wParam, intptr_t lParam) { return HandlePreTranslateMessage(mfcless::msg::KeyDown, wParam, lParam); }
	intptr_t OnPreTranslateKeyUp(uintptr_t wParam, intptr_t lParam) { return HandlePreTranslateMessage(mfcless::msg::KeyUp, wParam, lParam); }
	intptr_t OnPreTranslateSysKeyDown(uintptr_t wParam, intptr_t lParam) { return HandlePreTranslateMessage(mfcless::msg::SysKeyDown, wParam, lParam); }
	intptr_t OnPreTranslateSysKeyUp(uintptr_t wParam, intptr_t lParam) { return HandlePreTranslateMessage(mfcless::msg::SysKeyUp, wParam, lParam); }
	intptr_t HandlePreTranslateMessage(uint32 message, uintptr_t wParam, intptr_t lParam)
	{
		MSG msg = {m_plugWindow, message, wParam, lParam, 0, {}};
		return HandleKeyMessage(msg, true);
	}

	void RegisterMessageMap() override;
};

#endif // MPT_WITH_VST

OPENMPT_NAMESPACE_END
