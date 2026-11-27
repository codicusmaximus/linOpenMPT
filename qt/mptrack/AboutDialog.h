/*
 * AboutDialog.h
 * -------------
 * Purpose: About dialog with credits, system information and a fancy demo effect.
 * Notes  : mfcless twin of upstream's mptrack/AboutDialog.h.
 *          Draft from qt/tools/twin.py, not yet edited by hand.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */

#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "mfcless/Controls.h"
#include "mfcless/Graphics.h"
#include "mfcless/Window.h"
#include <QtWidgets/QWidget>
#include "DialogBase.h"

class QLineEdit;

OPENMPT_NAMESPACE_BEGIN

class RawGDIDIB;

class CRippleBitmap: public mfcless::Window<QWidget>
{
	Q_OBJECT

public:

	static constexpr uint32 UPDATE_INTERVAL = 15; // milliseconds

protected:

	BITMAPINFOHEADER m_bi;
	std::unique_ptr<RawGDIDIB> m_bitmapSrc, m_bitmapTarget;
	std::vector<int32> m_offset1, m_offset2;
	int32 *m_frontBuf, *m_backBuf;
	uint32 m_lastFrame = 0;	// Time of last frame
	uint32 m_lastRipple = 0;	// Time of last added ripple
	bool m_frame = false;		// Backbuffer toggle
	bool m_damp = true;		// Ripple damping status
	bool m_activity = true;	// There are actually some ripples
	bool m_showMouse = true;

public:

	CRippleBitmap();
	~CRippleBitmap();
	bool Animate();

protected:
	void OnPaint();
	bool OnEraseBkgnd(mfcless::DC *) { return true; }

	void OnMouseMove(uint32 nFlags, mfcless::Point point);
	void OnMouseHover(uint32 nFlags, mfcless::Point point) { OnMouseMove(nFlags, point); }
	void OnMouseLeave();

	void RegisterMessageMap() override;
};


class CAboutDlg : public DialogBase
{
	Q_OBJECT
protected:
	CRippleBitmap *m_bmp = nullptr;
	mfcless::TabControl *m_Tab = nullptr;
	QLineEdit *m_TabEdit = nullptr;
	uintptr_t m_TimerID = 0;
	static constexpr uintptr_t TIMERID_ABOUT_DEFAULT = 3;

public:
	static CAboutDlg *instance;

	~CAboutDlg();

	// Implementation
protected:
	bool OnInitDialog() override;
	void OnOK() override;
	void OnCancel() override;
	void RegisterMessageMap() override;;
	void DoDataExchange(DataExchange &dx) override;
	void OnTabChange();
	void OnTimer(uintptr_t nIDEvent);
public:
	static mpt::ustring GetTabText(int tab);
};

OPENMPT_NAMESPACE_END
