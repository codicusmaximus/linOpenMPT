/*
 * UpdateToolTip.h
 * ---------------
 * Purpose: Implementation of the update tooltip in the main toolbar.
 * Notes  : mfcless twin of upstream's mptrack/UpdateToolTip.h. The Windows
 *          balloon tooltip becomes a small popup frame with a title, the
 *          message with its link, and a close button, placed at the same
 *          point under the toolbar button.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "mfcless/Geometry.h"
#include <QtCore/QObject>
#include <QtCore/QPointer>

class QWidget;
class QFrame;

OPENMPT_NAMESPACE_BEGIN

class UpdateToolTip : public QObject
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
	QPointer<QFrame> m_popup;
	bool m_overLink = false;

public:
	~UpdateToolTip() override;
	bool ShowUpdate(QWidget &parent, const mpt::ustring &newVersion, const mpt::ustring &infoURL, const mfcless::Rect rectClient, const mfcless::Point ptScreen, const int buttonID);

protected:
	void SetResult(PopAction action);
	bool eventFilter(QObject *watched, QEvent *event) override;

	void OnLinkClick();
};

OPENMPT_NAMESPACE_END
