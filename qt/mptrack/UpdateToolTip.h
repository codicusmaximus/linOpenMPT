/*
 * UpdateToolTip.h
 * ---------------
 * Purpose: Implementation of the update tooltip in the main toolbar.
 * Notes  : mfcless twin of upstream's mptrack/UpdateToolTip.h.
 *          Draft from qt/tools/twin.py, not yet edited by hand.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "mfcless/Geometry.h"
#include "mfcless/MessageMap.h"
#include <QtCore/QObject>

class QWidget;

OPENMPT_NAMESPACE_BEGIN

class UpdateToolTip : public QObject, public mfcless::MessageMap
{
	Q_OBJECT
public:
	enum class PopAction
	{
		Undetermined,
		CloseButton,
		ClickLink,
		ClickBubble,
	};
protected:
	mpt::ustring m_infoURL;
	PopAction m_popAction = PopAction::Undetermined;

public:
	bool ShowUpdate(QWidget &parent, const mpt::ustring &newVersion, const mpt::ustring &infoURL, const mfcless::Rect rectClient, const mfcless::Point ptScreen, const int buttonID);

protected:
	void SetResult(PopAction action);

	void OnLButtonUp(uint32 nFlags, mfcless::Point point);
	void OnPop();
	void OnShow();
	void OnLinkClick();
	void RegisterMessageMap() override;
};

OPENMPT_NAMESPACE_END
