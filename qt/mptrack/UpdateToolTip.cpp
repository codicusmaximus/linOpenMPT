/*
 * UpdateToolTip.cpp
 * -----------------
 * Purpose: Implementation of the update tooltip in the main toolbar.
 * Notes  : mfcless twin of upstream's mptrack/UpdateToolTip.cpp.
 *          Draft from qt/tools/twin.py, not yet edited by hand.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "UpdateToolTip.h"
#include "HighDPISupport.h"
#include "Mptrack.h"
#include "Mainfrm.h"
#include "WindowMessages.h"


OPENMPT_NAMESPACE_BEGIN


void UpdateToolTip::RegisterMessageMap()
{
	QObject::RegisterMessageMap();
	// ON_WM_LBUTTONUP() (mfcless::Window)
	// ON_NOTIFY_REFLECT(TTN_POP, &UpdateToolTip::OnPop): by hand (Qt event override)
	// ON_NOTIFY_REFLECT(TTN_SHOW, &UpdateToolTip::OnShow): by hand (Qt event override)
	// ON_NOTIFY_REFLECT(TTN_LINKCLICK, &UpdateToolTip::OnLinkClick): by hand (Qt event override)
}


bool UpdateToolTip::ShowUpdate(QWidget &parent, const mpt::ustring &newVersion, const mpt::ustring &infoURL, const mfcless::Rect rectClient, const mfcless::Point ptScreen, const int buttonID)
{
	if(m_hWnd)
		DestroyWindow();
	Create(&parent, TTS_NOPREFIX | TTS_BALLOON | TTS_CLOSE | TTS_NOFADE);

	m_infoURL = infoURL;

	mpt::ustring message = MPT_UFORMAT("OpenMPT {} has been released.\nClick this message to install the update,\nor <a>click here to see what's new.</a>")(newVersion);
	mfcless::ToolInfo ti{};
	ti.cbSize = TTTOOLINFO_V1_SIZE;
	ti.uFlags = TTF_TRACK | TTF_PARSELINKS;
	ti.hwnd = parent;
	ti.lpszText = message.GetBuffer();
	ti.uId = buttonID;
	ti.rect = rectClient;
	if(!SendMessage(TTM_ADDTOOL, 0, reinterpret_cast<intptr_t>(&ti)))
		return false;

	SetTitle(TTI_INFO, U_("Update Available"));
	SetMaxTipWidth(HighDPISupport::ScalePixels(400, m_hWnd));  // When using automatic maximum width, only the first line of the text appears to be used for determining the width, causing subsequent lines to be cut off if they are longer.#
	SendMessage(TTM_TRACKPOSITION, 0, static_cast<intptr_t>(MAKELONG(ptScreen.x, ptScreen.y)));
	SendMessage(TTM_TRACKACTIVATE, true, reinterpret_cast<intptr_t>(&ti));
	return true;
}


void UpdateToolTip::SetResult(PopAction action)
{
	m_popAction = action;
	SendMessage(TTM_TRACKACTIVATE, false, 0);
	if(action != PopAction::CloseButton)
		CMainFrame::GetMainFrame()->SendMessage(WM_MOD_UPDATENOTIFY, static_cast<uintptr_t>(action));
}


void UpdateToolTip::OnLButtonUp(uint32 nFlags, mfcless::Point point)
{
	Window::OnLButtonUp(nFlags, point);
	if(m_popAction == PopAction::Undetermined)
		SetResult(PopAction::ClickBubble);
}


void UpdateToolTip::OnPop()
{
	if(pNMHDR->idFrom == uintptr_t(-1))
		SetResult(PopAction::CloseButton);
}


void UpdateToolTip::OnShow()
{
	m_popAction = PopAction::Undetermined;
}


void UpdateToolTip::OnLinkClick()
{
	CTrackApp::OpenURL(m_infoURL);
	SetResult(PopAction::ClickLink);
}


OPENMPT_NAMESPACE_END
