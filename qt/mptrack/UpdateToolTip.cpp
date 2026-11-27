/*
 * UpdateToolTip.cpp
 * -----------------
 * Purpose: Implementation of the update tooltip in the main toolbar.
 * Notes  : mfcless twin of upstream's mptrack/UpdateToolTip.cpp. See the
 *          header for how the balloon is drawn.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "UpdateToolTip.h"
#include "mfcless/Widgets.h"
#include "mfcless/ModMessage.h"
#include "HighDPISupport.h"
#include "Mptrack.h"
#include "Mainfrm.h"
#include "WindowMessages.h"

#include <QtGui/QMouseEvent>
#include <QtWidgets/QApplication>
#include <QtWidgets/QFrame>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QStyle>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QToolTip>
#include <QtWidgets/QVBoxLayout>


OPENMPT_NAMESPACE_BEGIN


UpdateToolTip::~UpdateToolTip()
{
	delete m_popup;
}


bool UpdateToolTip::ShowUpdate(QWidget &parent, const mpt::ustring &newVersion, const mpt::ustring &infoURL, const mfcless::Rect /*rectClient*/, const mfcless::Point ptScreen, const int /*buttonID*/)
{
	delete m_popup;
	m_infoURL = infoURL;

	auto *popup = new QFrame(&parent, Qt::ToolTip | Qt::FramelessWindowHint);
	popup->setFrameShape(QFrame::Box);
	popup->setAttribute(Qt::WA_DeleteOnClose);
	popup->setBackgroundRole(QPalette::ToolTipBase);
	popup->setForegroundRole(QPalette::ToolTipText);
	popup->setAutoFillBackground(true);
	popup->setPalette(QToolTip::palette());

	auto *icon = new QLabel(popup);
	const int iconSize = popup->style()->pixelMetric(QStyle::PM_SmallIconSize);
	icon->setPixmap(popup->style()->standardIcon(QStyle::SP_MessageBoxInformation).pixmap(iconSize, iconSize));
	auto *title = new QLabel(QStringLiteral("<b>Update Available</b>"), popup);
	auto *close = new QToolButton(popup);
	close->setAutoRaise(true);
	close->setIcon(popup->style()->standardIcon(QStyle::SP_TitleBarCloseButton));
	auto *message = new QLabel(popup);
	message->setTextFormat(Qt::RichText);
	message->setWordWrap(true);
	message->setMaximumWidth(HighDPISupport::ScalePixels(400, parent));
	message->setText(mfcless::ToQString(MPT_UFORMAT("OpenMPT {} has been released.<br>Click this message to install the update,<br>or <a href=\"#\">click here to see what's new.</a>")(newVersion)));

	auto *head = new QHBoxLayout;
	head->addWidget(icon);
	head->addWidget(title, 1);
	head->addWidget(close);
	auto *layout = new QVBoxLayout(popup);
	layout->addLayout(head);
	layout->addWidget(message);

	connect(close, &QToolButton::clicked, this, [this] { SetResult(PopAction::CloseButton); });
	connect(message, &QLabel::linkActivated, this, [this] { OnLinkClick(); });
	connect(message, &QLabel::linkHovered, this, [this](const QString &link) { m_overLink = !link.isEmpty(); });
	m_overLink = false;
	popup->installEventFilter(this);
	message->installEventFilter(this);
	title->installEventFilter(this);

	m_popup = popup;
	m_popAction = PopAction::Undetermined;
	popup->adjustSize();
	popup->move(QPoint(ptScreen.x, ptScreen.y));
	popup->show();
	return true;
}


bool UpdateToolTip::eventFilter(QObject *watched, QEvent *event)
{
	// A click anywhere on the bubble but the link and the close button
	if(event->type() == QEvent::MouseButtonRelease && static_cast<QMouseEvent *>(event)->button() == Qt::LeftButton)
	{
		if(m_overLink)
			return false;
		if(m_popAction == PopAction::Undetermined)
			SetResult(PopAction::ClickBubble);
		return true;
	}
	return QObject::eventFilter(watched, event);
}


void UpdateToolTip::SetResult(PopAction action)
{
	m_popAction = action;
	if(m_popup)
		m_popup->close();
	if(action != PopAction::CloseButton)
		mfcless::SendModMessage(CMainFrame::GetMainFrame(), WM_MOD_UPDATENOTIFY, static_cast<uintptr_t>(action));
}


void UpdateToolTip::OnLinkClick()
{
	CTrackApp::OpenURL(m_infoURL);
	SetResult(PopAction::ClickLink);
}


OPENMPT_NAMESPACE_END
