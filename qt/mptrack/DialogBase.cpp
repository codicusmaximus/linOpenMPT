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
#include "mfcless/ModMessage.h"
#include "mfcless/QtString.h"

#include <QtGui/QHelpEvent>
#include <QtGui/QKeyEvent>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QAbstractSlider>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QToolTip>

OPENMPT_NAMESPACE_BEGIN


DialogBase::DialogBase(QWidget *parent)
	: Window(parent)
{
}


DialogBase::DialogBase(uint32 nIDTemplate, QWidget *parent)
	: Window(parent)
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
	m_templateOrder = built.all;
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
	for(const auto &[id, w] : built.items)
	{
		mfcless::ConnectNotifications(w, id, this);
		w->installEventFilter(this);
	}
	RegisterMessageMap();
}


void DialogBase::RegisterMessageMap()
{
	// Buttons with IDOK and IDCANCEL close the dialog, as in Windows.
	AddCommand(1, &DialogBase::OnOK);
	AddCommand(2, &DialogBase::OnCancel);
}


void DialogBase::reject()
{
	// Escape and the window's close button come here; as on Windows, they
	// are the Cancel command, which a dialog may handle itself.
	if(!m_inCancel)
	{
		m_inCancel = true;
		DispatchCommand(2);
		m_inCancel = false;
	} else
	{
		QDialog::reject();
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
	UpdateData(false);
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


bool DialogBase::PreTranslateMessage(mfcless::Message *pMsg)
{
	if(pMsg)
	{
		if(pMsg->message >= mfcless::msg::KeyFirst && pMsg->message <= mfcless::msg::KeyLast)
			m_lastInputDevice = InputDevice::Keyboard;
		else if(pMsg->message >= mfcless::msg::MouseFirst && pMsg->message <= mfcless::msg::MouseLast)
			m_lastInputDevice = InputDevice::Mouse;

		if(HandleGlobalKeyMessage(*pMsg))
			return true;
	}

	return WindowBase::PreTranslateMessage(pMsg);
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
	Window::showEvent(e);
}


bool DialogBase::HandleGlobalKeyMessage(const mfcless::Message &msg)
{
	// We handle keypresses before Windows has a chance to handle them (for alt etc..)
	if(msg.message == mfcless::msg::KeyDown || msg.message == mfcless::msg::KeyUp || msg.message == mfcless::msg::SysKeyUp || msg.message == mfcless::msg::SysKeyDown)
	{
		if(CInputHandler *ih = CMainFrame::GetInputHandler())
		{
			const auto event = ih->Translate(msg);
			if(ih->KeyEvent(kCtxAllContexts, event) != kcNull)
			{
				// Special case: ESC is typically bound to stopping playback, but we also want to allow ESC to close dialogs
				if(msg.message != mfcless::msg::KeyDown || msg.wParam != mfcless::vk::ESCAPE)
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


void DialogBase::SetItemText(uint32 id, const mpt::ustring &text)
{
	QWidget *w = GetDlgItem(id);
	const QString t = mfcless::ToQString(text);
	if(auto *e = qobject_cast<QLineEdit *>(w))
		e->setText(t);
	else if(auto *l = qobject_cast<QLabel *>(w))
		l->setText(t);
	else if(auto *b = qobject_cast<QAbstractButton *>(w))
		b->setText(t);
	else if(auto *g = qobject_cast<QGroupBox *>(w))
		g->setTitle(t);
	else if(auto *c = qobject_cast<QComboBox *>(w))
	{
		if(c->isEditable())
			c->setEditText(t);
		else
			c->setCurrentIndex(c->findText(t));
	}
	else if(auto *p = qobject_cast<QPlainTextEdit *>(w))
		p->setPlainText(t);
	else if(auto *x = qobject_cast<QTextEdit *>(w))
		x->setPlainText(t);
	else if(w)
		w->setWindowTitle(t);
}


mpt::ustring DialogBase::GetItemText(uint32 id) const
{
	QWidget *w = GetDlgItem(id);
	QString t;
	if(auto *e = qobject_cast<QLineEdit *>(w))
		t = e->text();
	else if(auto *l = qobject_cast<QLabel *>(w))
		t = l->text();
	else if(auto *b = qobject_cast<QAbstractButton *>(w))
		t = b->text();
	else if(auto *g = qobject_cast<QGroupBox *>(w))
		t = g->title();
	else if(auto *c = qobject_cast<QComboBox *>(w))
		t = c->currentText();
	else if(auto *p = qobject_cast<QPlainTextEdit *>(w))
		t = p->toPlainText();
	else if(auto *x = qobject_cast<QTextEdit *>(w))
		t = x->toPlainText();
	else if(w)
		t = w->windowTitle();
	return mfcless::FromQString(t);
}


void DialogBase::SetItemInt(uint32 id, int64 value)
{
	SetItemText(id, mpt::ufmt::val(value));
}


int64 DialogBase::GetItemInt(uint32 id, bool *ok) const
{
	bool parsed = false;
	const int64 value = mfcless::ToQString(GetItemText(id)).trimmed().toLongLong(&parsed);
	if(ok)
		*ok = parsed;
	return parsed ? value : 0;
}


void DialogBase::SetChecked(uint32 id, int state)
{
	auto *b = qobject_cast<QAbstractButton *>(GetDlgItem(id));
	if(!b)
		return;
	QSignalBlocker block(b);
	if(auto *c = qobject_cast<QCheckBox *>(b); c && c->isTristate())
		c->setCheckState(state == 2 ? Qt::PartiallyChecked : (state ? Qt::Checked : Qt::Unchecked));
	else
		b->setChecked(state != 0);
}


int DialogBase::IsChecked(uint32 id) const
{
	auto *b = qobject_cast<QAbstractButton *>(GetDlgItem(id));
	if(!b)
		return 0;
	if(auto *c = qobject_cast<QCheckBox *>(b); c && c->checkState() == Qt::PartiallyChecked)
		return 2;
	return b->isChecked() ? 1 : 0;
}


void DialogBase::CheckRadio(uint32 first, uint32 last, uint32 id)
{
	for(uint32 i = first; i <= last; i++)
		SetChecked(i, i == id ? 1 : 0);
}


uint32 DialogBase::GetCheckedRadio(uint32 first, uint32 last) const
{
	for(uint32 i = first; i <= last; i++)
	{
		if(IsChecked(i))
			return i;
	}
	return 0;
}


void DialogBase::EnableItem(uint32 id, bool enable)
{
	if(QWidget *w = GetDlgItem(id))
		w->setEnabled(enable);
}


void DialogBase::ShowItem(uint32 id, bool show)
{
	if(QWidget *w = GetDlgItem(id))
		w->setVisible(show);
}


bool DialogBase::UpdateData(bool save)
{
	DataExchange dx;
	dx.save = save;
	DoDataExchange(dx);
	return !dx.failed;
}


void DialogBase::ExchangeText(DataExchange &dx, uint32 id, mpt::ustring &value)
{
	if(dx.save)
		value = GetItemText(id);
	else
		SetItemText(id, value);
}


void DialogBase::ExchangeText(DataExchange &dx, uint32 id, int32 &value)
{
	if(dx.save)
	{
		bool ok = false;
		const int64 v = GetItemInt(id, &ok);
		if(!ok || v < std::numeric_limits<int32>::min() || v > std::numeric_limits<int32>::max())
		{
			dx.failed = true;
			QMessageBox::warning(this, QString(), tr("Please enter an integer."));
			if(QWidget *w = GetDlgItem(id))
				w->setFocus();
			return;
		}
		value = static_cast<int32>(v);
	} else
	{
		SetItemInt(id, value);
	}
}


void DialogBase::ExchangeText(DataExchange &dx, uint32 id, uint32 &value)
{
	if(dx.save)
	{
		bool ok = false;
		const int64 v = GetItemInt(id, &ok);
		if(!ok || v < 0 || v > std::numeric_limits<uint32>::max())
		{
			dx.failed = true;
			QMessageBox::warning(this, QString(), tr("Please enter a positive integer."));
			if(QWidget *w = GetDlgItem(id))
				w->setFocus();
			return;
		}
		value = static_cast<uint32>(v);
	} else
	{
		SetItemInt(id, value);
	}
}


void DialogBase::ExchangeText(DataExchange &dx, uint32 id, double &value)
{
	if(dx.save)
	{
		bool ok = false;
		const double v = mfcless::ToQString(GetItemText(id)).trimmed().toDouble(&ok);
		if(!ok)
		{
			dx.failed = true;
			QMessageBox::warning(this, QString(), tr("Please enter a number."));
			if(QWidget *w = GetDlgItem(id))
				w->setFocus();
			return;
		}
		value = v;
	} else
	{
		SetItemText(id, mpt::ufmt::val(value));
	}
}


void DialogBase::ExchangeCheck(DataExchange &dx, uint32 id, int &value)
{
	if(dx.save)
		value = IsChecked(id);
	else
		SetChecked(id, value);
}


void DialogBase::ExchangeCheck(DataExchange &dx, uint32 id, bool &value)
{
	if(dx.save)
		value = IsChecked(id) != 0;
	else
		SetChecked(id, value ? 1 : 0);
}


void DialogBase::ExchangeRadio(DataExchange &dx, uint32 id, int &value)
{
	// The group runs from id to the next control that starts a group.
	QWidget *first = GetDlgItem(id);
	auto it = std::find(m_templateOrder.begin(), m_templateOrder.end(), first);
	if(it == m_templateOrder.end())
		return;
	int index = 0;
	if(dx.save)
		value = -1;
	for(; it != m_templateOrder.end(); ++it)
	{
		if(it != m_templateOrder.begin() && *it != first && (((*it)->property("mfcless_style").toUInt() & mfcless::Style::Group)))
			break;
		auto *radio = qobject_cast<QRadioButton *>(*it);
		if(!radio)
			continue;
		if(dx.save)
		{
			if(radio->isChecked())
				value = index;
		} else
		{
			QSignalBlocker block(radio);
			radio->setChecked(index == value);
		}
		index++;
	}
}


void DialogBase::ExchangeSlider(DataExchange &dx, uint32 id, int &value)
{
	auto *s = qobject_cast<QAbstractSlider *>(GetDlgItem(id));
	if(!s)
		return;
	if(dx.save)
		value = s->value();
	else
		s->setValue(value);
}


void DialogBase::ExchangeComboIndex(DataExchange &dx, uint32 id, int &value)
{
	auto *c = qobject_cast<QComboBox *>(GetDlgItem(id));
	if(!c)
		return;
	if(dx.save)
		value = c->currentIndex();
	else
		c->setCurrentIndex(value);
}


void DialogBase::ValidateMinMax(DataExchange &dx, int64 value, int64 min, int64 max)
{
	if(!dx.save || dx.failed)
		return;
	if(value < min || value > max)
	{
		dx.failed = true;
		QMessageBox::warning(this, QString(), tr("Please enter an integer between %1 and %2.").arg(min).arg(max));
	}
}


void PropertyPage::SetModified(bool changed)
{
	if(m_modified == changed)
		return;
	m_modified = changed;
	emit modifiedChanged(changed);
}


OPENMPT_NAMESPACE_END
