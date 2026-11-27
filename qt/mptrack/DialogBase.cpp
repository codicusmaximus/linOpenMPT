/*
 * DialogBase.cpp
 * --------------
 * Purpose: Base class for dialogs that adds functionality on top of CDialog.
 * Notes  : mfcless twin of upstream's mptrack/DialogBase.cpp. The controls
 *          come from upstream's dialog template (mfcless::BuildDialog), at
 *          the template's positions.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "DialogBase.h"
#include "HighDPISupport.h"
#include "InputHandler.h"
#include "Mainfrm.h"

#include "mfcless/DialogBuilder.h"
#include "mfcless/QtString.h"

#include <QtGui/QHelpEvent>
#include <QtGui/QKeyEvent>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QApplication>
#include <QtWidgets/QToolTip>

OPENMPT_NAMESPACE_BEGIN


DialogBase::DialogBase(QWidget *parent)
	: QDialog(parent)
{
}


DialogBase::DialogBase(uint32 nIDTemplate, QWidget *parent)
	: QDialog(parent)
	, m_templateID(nIDTemplate)
{
}


void DialogBase::CreateFromTemplate()
{
	if(!m_templateID || !m_items.empty())
		return;
	mfcless::BuiltDialog built;
	if(!mfcless::BuildDialog(this, m_templateID, built))
		return;
	m_items.insert(built.items.begin(), built.items.end());
	const uint32 style = built.dlgTemplate.style;
	if(style & mfcless::Style::Child)
	{
		setWindowFlags(Qt::Widget);
	} else if(!(style & mfcless::Style::ThickFrame))
	{
		setFixedSize(built.size);
	} else
	{
		setMinimumSize(built.size);
		new mfcless::DialogLayout(this, built, mfcless::res::DialogLayout(m_templateID));
	}
	// Buttons with IDOK and IDCANCEL close the dialog, as in Windows.
	for(const auto &[id, w] : built.items)
	{
		if(auto *button = qobject_cast<QAbstractButton *>(w))
		{
			if(id == 1)
				connect(button, &QAbstractButton::clicked, this, [this] { OnOK(); });
			else if(id == 2)
				connect(button, &QAbstractButton::clicked, this, [this] { OnCancel(); });
		}
		w->installEventFilter(this);
	}
}


int DialogBase::DoModal()
{
	if(!m_initialized)
	{
		CreateFromTemplate();
		m_initialized = true;
		if(OnInitDialog())
			SetFocusToFirstControl();
	}
	return exec() == QDialog::Accepted ? 1 : 2;  // IDOK, IDCANCEL
}


bool DialogBase::Create(QWidget *parent)
{
	if(parent)
		setParent(parent, windowFlags());
	if(!m_initialized)
	{
		CreateFromTemplate();
		m_initialized = true;
		if(OnInitDialog())
			SetFocusToFirstControl();
	}
	return true;
}


bool DialogBase::OnInitDialog()
{
	UpdateDPI();
	return true;
}


QWidget *DialogBase::GetDlgItem(uint32 id) const
{
	auto it = m_items.find(id);
	return it != m_items.end() ? it->second : nullptr;
}


bool DialogBase::eventFilter(QObject *watched, QEvent *e)
{
	switch(e->type())
	{
	case QEvent::KeyPress:
	case QEvent::KeyRelease:
		m_lastInputDevice = InputDevice::Keyboard;
		if(HandleGlobalKeyMessage(*static_cast<QKeyEvent *>(e)))
			return true;
		break;
	case QEvent::MouseButtonPress:
	case QEvent::MouseButtonRelease:
	case QEvent::MouseMove:
		m_lastInputDevice = InputDevice::Mouse;
		break;
	case QEvent::ToolTip:
		if(auto *w = qobject_cast<QWidget *>(watched))
		{
			const QVariant id = w->property("mfcless_id");
			const mpt::ustring text = GetToolTipText(id.isValid() ? id.toUInt() : 0, w);
			if(!text.empty())
			{
				QToolTip::showText(static_cast<QHelpEvent *>(e)->globalPos(), mfcless::ToQString(text), w);
				return true;
			}
		}
		break;
	default:
		break;
	}
	return QDialog::eventFilter(watched, e);
}


bool DialogBase::event(QEvent *e)
{
	if(e->type() == QEvent::KeyPress || e->type() == QEvent::KeyRelease)
	{
		m_lastInputDevice = InputDevice::Keyboard;
		if(HandleGlobalKeyMessage(*static_cast<QKeyEvent *>(e)))
			return true;
	}
	return QDialog::event(e);
}


void DialogBase::showEvent(QShowEvent *e)
{
	if(!m_initialized)
	{
		CreateFromTemplate();
		m_initialized = true;
		if(OnInitDialog())
			SetFocusToFirstControl();
	}
	QDialog::showEvent(e);
}


bool DialogBase::HandleGlobalKeyMessage(const QKeyEvent &msg)
{
	// We handle keypresses before Qt has a chance to handle them (for alt etc..)
	if(msg.type() == QEvent::KeyPress || msg.type() == QEvent::KeyRelease)
	{
		if(CInputHandler *ih = CMainFrame::GetInputHandler())
		{
			const auto event = ih->Translate(msg);
			if(ih->KeyEvent(kCtxAllContexts, event) != kcNull)
			{
				// Special case: ESC is typically bound to stopping playback, but we also want to allow ESC to close dialogs
				if(msg.type() != QEvent::KeyPress || msg.key() != Qt::Key_Escape)
					return true;  // Mapped to a command, no need to pass message on.
			}
		}
	}

	return false;
}


void DialogBase::SetFocusToFirstControl()
{
	// Template order is tab order; the first control that takes focus.
	if(QWidget *first = nextInFocusChain(); first && first != this && isAncestorOf(first))
	{
		QWidget *w = first;
		do
		{
			if(w->isEnabled() && w->isVisibleTo(this) && (w->focusPolicy() & Qt::TabFocus))
			{
				w->setFocus(Qt::TabFocusReason);
				return;
			}
			w = w->nextInFocusChain();
		} while(w && w != first && isAncestorOf(w));
	}
}


void DialogBase::UpdateDPI()
{
	m_dpi = HighDPISupport::GetDpiForWindow(this);
}


void PropertyPage::SetModified(bool changed)
{
	if(m_modified == changed)
		return;
	m_modified = changed;
	emit modifiedChanged(changed);
}


OPENMPT_NAMESPACE_END
