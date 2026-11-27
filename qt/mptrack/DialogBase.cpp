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
#include "mfcless/StandardIds.h"
#include "HighDPISupport.h"
#include "InputHandler.h"
#include "Mainfrm.h"

#include "mfcless/DialogBuilder.h"
#include "mfcless/ModMessage.h"
#include "mfcless/QtString.h"
#include "mfcless/Resources.h"

#include <algorithm>

#include <QtGui/QHelpEvent>
#include <QtGui/QKeyEvent>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QAbstractSlider>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QVBoxLayout>
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
	DataExchange collect;
	collect.collect = true;
	DoDataExchange(collect);
	mfcless::BuiltDialog built;
	const bool ok = mfcless::BuildDialog(this, m_templateID, built, [&collect](const mfcless::res::DialogItem &item, QWidget *parent) -> QWidget * {
		auto it = collect.factories.find(item.id);
		return it != collect.factories.end() ? it->second(item, parent) : nullptr;
	});
	if(!ok)
		return;
	m_items.insert(built.items.begin(), built.items.end());
	m_templateOrder = built.all;
	const uint32 style = built.dlgTemplate.style;
	if(style & mfcless::Style::Child)
	{
		setWindowFlags(Qt::Widget);
		// A child dialog paints its background, as on Windows, and one with a
		// frame (DS_MODALFRAME, WS_DLGFRAME, WS_BORDER) shows it (Quick Start).
		setAutoFillBackground(true);
		if(style & (0x80 | 0x00400000 | mfcless::Style::Border))
		{
			m_childFrame = new QFrame(this);
			m_childFrame->setFrameStyle(QFrame::StyledPanel | QFrame::Raised);
			m_childFrame->setAttribute(Qt::WA_TransparentForMouseEvents);
			m_childFrame->setGeometry(rect());
			m_childFrame->lower();
			installEventFilter(this);
		}
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


void DialogBase::AddDynamicItem(uint32 id, QWidget *control)
{
	m_items.emplace(id, control);
	mfcless::ConnectNotifications(control, id, this);
	control->installEventFilter(this);
}


bool DialogBase::ReplaceItem(uint32 id, QWidget *replacement)
{
	auto it = m_items.find(id);
	if(it == m_items.end() || !m_templateID)
		return false;
	const auto tmpl = mfcless::res::Dialog(m_templateID);
	if(!tmpl)
		return false;
	const mfcless::res::DialogItem *item = nullptr;
	for(const auto &i : tmpl->items)
	{
		if(i.id == id)
		{
			item = &i;
			break;
		}
	}
	if(!item || mfcless::CreateControl(*item, replacement->parentWidget(), replacement) != replacement)
		return false;
	QWidget *old = it->second;
	replacement->setGeometry(old->geometry());
	replacement->setVisible(old->isVisibleTo(old->parentWidget()));
	replacement->setEnabled(old->isEnabled());
	replacement->setFont(old->font());
	replacement->stackUnder(old);
	QWidget::setTabOrder(old, replacement);
	it->second = replacement;
	std::replace(m_templateOrder.begin(), m_templateOrder.end(), old, replacement);
	mfcless::ConnectNotifications(replacement, id, this);
	replacement->installEventFilter(this);
	for(auto *layout : findChildren<mfcless::DialogLayout *>(Qt::FindDirectChildrenOnly))
		layout->Replace(old, replacement);
	old->hide();
	old->deleteLater();
	return true;
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


void DialogBase::done(int result)
{
	if(m_inModalInit)
		m_endedInInit = result;
	QDialog::done(result);
}


int DialogBase::DoModal()
{
	m_endedInInit.reset();
	if(!m_initialized)
	{
		CreateFromTemplate();
		m_initialized = true;
		m_inModalInit = true;
		if(OnInitDialog())
			SetFocusToFirstControl();
		m_inModalInit = false;
	}
	// IDOK, IDCANCEL, or what EndDialog gave.
	const int result = m_endedInInit ? *m_endedInInit : exec();
	return result == QDialog::Accepted ? IDOK : (result == QDialog::Rejected ? IDCANCEL : result);
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
	if(watched == this)
	{
		if(e->type() == QEvent::Resize && m_childFrame)
			m_childFrame->setGeometry(rect());
		return Window::eventFilter(watched, e);
	}
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
	if(dx.collect)
		return;
	if(dx.save)
		value = GetItemText(id);
	else
		SetItemText(id, value);
}


void DialogBase::ExchangeText(DataExchange &dx, uint32 id, int32 &value)
{
	if(dx.collect)
		return;
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
	if(dx.collect)
		return;
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
	if(dx.collect)
		return;
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
	if(dx.collect)
		return;
	if(dx.save)
		value = IsChecked(id);
	else
		SetChecked(id, value);
}


void DialogBase::ExchangeCheck(DataExchange &dx, uint32 id, bool &value)
{
	if(dx.collect)
		return;
	if(dx.save)
		value = IsChecked(id) != 0;
	else
		SetChecked(id, value ? 1 : 0);
}


void DialogBase::ExchangeRadio(DataExchange &dx, uint32 id, int &value)
{
	if(dx.collect)
		return;
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
	if(dx.collect)
		return;
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
	if(dx.collect)
		return;
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
	if(dx.collect)
		return;
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


PropertySheet::PropertySheet(const mpt::ustring &caption, QWidget *parent, uint32 selectPage)
	: Window(parent)
	, m_startPage(selectPage)
{
	setWindowTitle(mfcless::ToQString(caption));
	setWindowFlag(Qt::WindowContextHelpButtonHint, false);
	auto *layout = new QVBoxLayout(this);
	m_tabs = new QTabWidget(this);
	layout->addWidget(m_tabs);
	m_buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply, this);
	layout->addWidget(m_buttons);
	m_buttons->button(QDialogButtonBox::Apply)->setEnabled(false);
	connect(m_buttons, &QDialogButtonBox::clicked, this, [this](QAbstractButton *button)
	{
		switch(m_buttons->standardButton(button))
		{
		case QDialogButtonBox::Ok:
			if(Apply())
				accept();
			break;
		case QDialogButtonBox::Cancel:
			reject();
			break;
		case QDialogButtonBox::Apply:
			Apply();
			break;
		case QDialogButtonBox::Help:
			OnHelp();
			break;
		default:
			break;
		}
	});
	connect(this, &QDialog::rejected, this, [this]
	{
		for(PropertyPage *page : m_pages)
		{
			if(page->m_initialized)
				page->OnCancel();
		}
	});
	connect(m_tabs, &QTabWidget::currentChanged, this, [this](int index)
	{
		if(m_switching)
			return;
		if(m_current >= 0 && m_current != index && m_current < GetPageCount() && !m_pages[m_current]->OnKillActive())
		{
			m_switching = true;
			m_tabs->setCurrentIndex(m_current);
			m_switching = false;
			return;
		}
		Activate(index);
	});
}


PropertySheet::~PropertySheet()
{
	// The pages belong to the caller.
	for(PropertyPage *page : m_pages)
	{
		if(page->parent())
			page->setParent(nullptr);
		page->m_sheet = nullptr;
	}
}


void PropertySheet::AddPage(PropertyPage *page)
{
	page->m_sheet = this;
	m_pages.push_back(page);
	if(m_built)
	{
		auto *holder = new QWidget;
		new QVBoxLayout(holder);
		holder->layout()->setContentsMargins(0, 0, 0, 0);
		m_tabs->addTab(holder, page->windowTitle());
	}
}


PropertyPage *PropertySheet::GetPage(int index) const
{
	return (index >= 0 && index < GetPageCount()) ? m_pages[index] : nullptr;
}


int PropertySheet::GetActiveIndex() const
{
	return m_built ? m_tabs->currentIndex() : static_cast<int>(m_startPage);
}


bool PropertySheet::SetActivePage(int index)
{
	if(index < 0 || index >= GetPageCount())
		return false;
	if(!m_built)
	{
		m_startPage = index;
		return true;
	}
	m_tabs->setCurrentIndex(index);
	return m_tabs->currentIndex() == index;
}


bool PropertySheet::SetActivePage(PropertyPage *page)
{
	const auto it = std::find(m_pages.begin(), m_pages.end(), page);
	return it != m_pages.end() && SetActivePage(static_cast<int>(it - m_pages.begin()));
}


void PropertySheet::SetTitle(const mpt::ustring &title)
{
	setWindowTitle(mfcless::ToQString(title));
}


QAbstractButton *PropertySheet::GetButton(uint32 id) const
{
	switch(id)
	{
	case 1: return m_buttons->button(QDialogButtonBox::Ok);
	case 2: return m_buttons->button(QDialogButtonBox::Cancel);
	case 9: return m_buttons->button(QDialogButtonBox::Help);
	case 0x3021: return m_buttons->button(QDialogButtonBox::Apply);
	default: return nullptr;
	}
}


void PropertySheet::EnableItem(uint32 id, bool enable)
{
	if(QAbstractButton *button = GetButton(id))
		button->setEnabled(enable);
}


void PropertySheet::SetHelpButton(bool show)
{
	m_hasHelp = show;
	if(show && !m_buttons->button(QDialogButtonBox::Help))
		m_buttons->addButton(QDialogButtonBox::Help);
	else if(!show)
		m_buttons->setStandardButtons(m_buttons->standardButtons() & ~QDialogButtonBox::Help);
}


void PropertySheet::keyPressEvent(QKeyEvent *e)
{
	if(e->key() == Qt::Key_F1 && e->modifiers() == Qt::NoModifier)
	{
		OnHelp();
		return;
	}
	Window::keyPressEvent(e);
}


void PropertySheet::Build()
{
	if(m_built)
		return;
	m_built = true;
	// Every page's template is made now (as MFC does to find the largest
	// page), so that the tabs have their captions and the sheet its size.
	for(PropertyPage *page : m_pages)
	{
		auto *holder = new QWidget;
		auto *layout = new QVBoxLayout(holder);
		layout->setContentsMargins(0, 0, 0, 0);
		page->CreateFromTemplate();
		m_tabs->addTab(holder, page->windowTitle());
		connect(page, &PropertyPage::modifiedChanged, this, [this](bool) { UpdateApply(); });
	}
	OnInitDialog();
	int start = static_cast<int>(m_startPage);
	if(start < 0 || start >= GetPageCount())
		start = 0;
	m_switching = true;
	m_tabs->setCurrentIndex(start);
	m_switching = false;
	Activate(start);
}


void PropertySheet::Activate(int index)
{
	PropertyPage *page = GetPage(index);
	if(!page)
		return;
	QWidget *holder = m_tabs->widget(index);
	if(page->parentWidget() != holder)
	{
		page->setParent(holder, Qt::Widget);
		holder->layout()->addWidget(page);
	}
	if(!page->m_initialized)
	{
		page->m_initialized = true;
		if(page->OnInitDialog())
			page->SetFocusToFirstControl();
	}
	page->show();
	m_current = index;
	page->OnSetActive();
}


bool PropertySheet::Apply()
{
	if(PropertyPage *active = GetPage(m_current); active && !active->OnKillActive())
		return false;
	for(PropertyPage *page : m_pages)
	{
		if(!page->m_initialized)
			continue;
		if(!page->OnApply())
			return false;
		page->SetModified(false);
	}
	UpdateApply();
	return true;
}


void PropertySheet::UpdateApply()
{
	const bool modified = std::any_of(m_pages.begin(), m_pages.end(), [](const PropertyPage *page) { return page->IsModified(); });
	EnableItem(0x3021, modified);
}


int PropertySheet::DoModal()
{
	Build();
	// IDOK, IDCANCEL, or what EndDialog gave.
	const int result = exec();
	return result == QDialog::Accepted ? IDOK : (result == QDialog::Rejected ? IDCANCEL : result);
}


OPENMPT_NAMESPACE_END
