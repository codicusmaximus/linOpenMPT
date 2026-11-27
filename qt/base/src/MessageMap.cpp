/*
 * MessageMap.cpp
 * --------------
 * Purpose: Routes commands and control notifications to handlers.
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */

#include "mfcless/MessageMap.h"
#include "mfcless/Controls.h"
#include "mfcless/ModMessage.h"

#include <QtCore/QEvent>
#include <QtCore/QPointer>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QAbstractSlider>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QTabBar>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QTreeWidget>

namespace mfcless
{

// Later registrations (a derived class's) take precedence over earlier ones.
bool MessageMap::DispatchCommand(uint32_t id)
{
	for(auto it = m_commands.rbegin(); it != m_commands.rend(); ++it)
	{
		if(id >= it->first && id <= it->last && it->fn(id))
			return true;
	}
	return false;
}

bool MessageMap::DispatchCommandUpdate(uint32_t id, CommandState &state)
{
	for(auto it = m_updates.rbegin(); it != m_updates.rend(); ++it)
	{
		if(id >= it->first && id <= it->last)
		{
			state.id = id;
			it->fn(state);
			return true;
		}
	}
	return false;
}

bool MessageMap::DispatchMessage(uint32_t message, uintptr_t wParam, intptr_t lParam, intptr_t &result)
{
	for(auto it = m_messages.rbegin(); it != m_messages.rend(); ++it)
	{
		if(it->message == message)
		{
			result = it->fn(wParam, lParam);
			return true;
		}
	}
	return false;
}

bool MessageMap::HandleModMessage(QEvent *e)
{
	if(e->type() != ModMessage::EventType())
		return false;
	auto *m = static_cast<ModMessage *>(e);
	if(m->message == CommandMessage)
		m->result = DispatchCommand(static_cast<uint32_t>(m->wParam)) ? 1 : 0;
	else
		DispatchMessage(m->message, m->wParam, m->lParam, m->result);
	return true;
}

bool MessageMap::HasCommand(uint32_t id) const
{
	for(const auto &c : m_commands)
	{
		if(id >= c.first && id <= c.last)
			return true;
	}
	return false;
}

bool MessageMap::DispatchNotify(NotifyInfo &info)
{
	// Buttons send their clicks as commands, as on Windows.
	if(info.code == Notify::Clicked && DispatchCommand(info.id))
		return true;
	NotifyInfo *previous = m_current;
	m_current = &info;
	bool handled = false;
	for(auto it = m_notifies.rbegin(); it != m_notifies.rend() && !handled; ++it)
	{
		if(info.id >= it->first && info.id <= it->last && info.code == it->code)
			handled = it->fn(info);
	}
	m_current = previous;
	return handled;
}

bool MessageMap::DispatchScroll(Qt::Orientation orientation, ScrollCode code, int pos, QWidget *control)
{
	for(auto it = m_scrolls.rbegin(); it != m_scrolls.rend(); ++it)
	{
		if(it->orientation == orientation)
		{
			it->fn(code, pos, control);
			return true;
		}
	}
	return false;
}


namespace
{

void Send(MessageMap *target, uint32_t id, Notify code, QWidget *control, int item = -1)
{
	NotifyInfo info;
	info.id = id;
	info.code = code;
	info.control = control;
	info.item = item;
	target->DispatchNotify(info);
}

// Focus changes of a control, as SetFocus / KillFocus notifications.
class FocusWatcher : public QObject
{
public:
	FocusWatcher(QWidget *control, uint32_t id, MessageMap *target)
		: QObject(control), m_id(id), m_target(target) {}
	bool eventFilter(QObject *watched, QEvent *e) override
	{
		if(e->type() == QEvent::FocusIn)
			Send(m_target, m_id, Notify::SetFocus, static_cast<QWidget *>(parent()));
		else if(e->type() == QEvent::FocusOut)
			Send(m_target, m_id, Notify::KillFocus, static_cast<QWidget *>(parent()));
		return QObject::eventFilter(watched, e);
	}

private:
	uint32_t m_id;
	MessageMap *m_target;
};

} // namespace


void ConnectNotifications(QWidget *control, uint32_t id, MessageMap *target)
{
	QObject *context = control;
	if(auto *button = qobject_cast<QAbstractButton *>(control))
	{
		QObject::connect(button, &QAbstractButton::clicked, context, [=] { Send(target, id, Notify::Clicked, control); });
		return;
	}
	if(auto *slider = qobject_cast<QAbstractSlider *>(control))
	{
		// Windows sends scroll notifications for the user's actions only,
		// not when the program sets the position.
		auto pending = std::make_shared<std::optional<ScrollCode>>();
		QObject::connect(slider, &QAbstractSlider::actionTriggered, context, [=](int action) {
			switch(action)
			{
			case QAbstractSlider::SliderSingleStepAdd: *pending = ScrollCode::LineDown; break;
			case QAbstractSlider::SliderSingleStepSub: *pending = ScrollCode::LineUp; break;
			case QAbstractSlider::SliderPageStepAdd: *pending = ScrollCode::PageDown; break;
			case QAbstractSlider::SliderPageStepSub: *pending = ScrollCode::PageUp; break;
			case QAbstractSlider::SliderToMinimum: *pending = ScrollCode::Top; break;
			case QAbstractSlider::SliderToMaximum: *pending = ScrollCode::Bottom; break;
			case QAbstractSlider::SliderMove: *pending = ScrollCode::ThumbTrack; break;
			default: return;
			}
			// The value is applied after this signal; report once it is.
			QPointer<QAbstractSlider> guard(slider);
			QMetaObject::invokeMethod(slider, [=] {
				if(!guard || !*pending)
					return;
				const ScrollCode code = **pending;
				pending->reset();
				target->DispatchScroll(slider->orientation(), code, slider->value(), slider);
				if(code != ScrollCode::ThumbTrack)
					target->DispatchScroll(slider->orientation(), ScrollCode::EndScroll, slider->value(), slider);
			}, Qt::QueuedConnection);
		});
		QObject::connect(slider, &QAbstractSlider::sliderReleased, context, [=] {
			target->DispatchScroll(slider->orientation(), ScrollCode::ThumbPosition, slider->value(), slider);
			target->DispatchScroll(slider->orientation(), ScrollCode::EndScroll, slider->value(), slider);
		});
		return;
	}
	if(auto *combo = qobject_cast<QComboBox *>(control))
	{
		QObject::connect(combo, &QComboBox::activated, context, [=] { Send(target, id, Notify::SelChange, control); });
		if(combo->isEditable())
		{
			QObject::connect(combo, &QComboBox::editTextChanged, context, [=] {
				Send(target, id, Notify::EditUpdate, control);
				Send(target, id, Notify::EditChange, control);
			});
		}
		control->installEventFilter(new FocusWatcher(control, id, target));
		return;
	}
	if(auto *edit = qobject_cast<QLineEdit *>(control))
	{
		// Windows notifies changes the program makes too.
		QObject::connect(edit, &QLineEdit::textChanged, context, [=] {
			Send(target, id, Notify::EditUpdate, control);
			Send(target, id, Notify::EditChange, control);
		});
		control->installEventFilter(new FocusWatcher(control, id, target));
		return;
	}
	if(auto *edit = qobject_cast<QPlainTextEdit *>(control))
	{
		QObject::connect(edit, &QPlainTextEdit::textChanged, context, [=] {
			Send(target, id, Notify::EditUpdate, control);
			Send(target, id, Notify::EditChange, control);
		});
		control->installEventFilter(new FocusWatcher(control, id, target));
		return;
	}
	if(auto *edit = qobject_cast<QTextEdit *>(control))
	{
		QObject::connect(edit, &QTextEdit::textChanged, context, [=] { Send(target, id, Notify::EditChange, control); });
		control->installEventFilter(new FocusWatcher(control, id, target));
		return;
	}
	if(auto *list = qobject_cast<QListWidget *>(control))
	{
		QObject::connect(list, &QListWidget::currentRowChanged, context, [=](int row) { Send(target, id, Notify::SelChange, control, row); });
		QObject::connect(list, &QListWidget::itemDoubleClicked, context, [=](QListWidgetItem *item) { Send(target, id, Notify::DoubleClicked, control, list->row(item)); });
		QObject::connect(list, &QListWidget::itemChanged, context, [=](QListWidgetItem *item) { Send(target, id, Notify::CheckChange, control, list->row(item)); });
		return;
	}
	if(auto *tree = qobject_cast<QTreeWidget *>(control))
	{
		QObject::connect(tree, &QTreeWidget::itemSelectionChanged, context, [=] {
			const QModelIndex current = tree->currentIndex();
			Send(target, id, Notify::SelChange, control, current.isValid() ? current.row() : -1);
			Send(target, id, Notify::ItemChanged, control, current.isValid() ? current.row() : -1);
		});
		QObject::connect(tree, &QTreeWidget::itemDoubleClicked, context, [=](QTreeWidgetItem *item, int column) {
			NotifyInfo info;
			info.id = id;
			info.code = Notify::DoubleClicked;
			info.control = control;
			info.item = tree->indexOfTopLevelItem(item);
			info.subItem = column;
			target->DispatchNotify(info);
		});
		QObject::connect(tree, &QTreeWidget::itemClicked, context, [=](QTreeWidgetItem *item, int column) {
			NotifyInfo info;
			info.id = id;
			info.code = Notify::Click;
			info.control = control;
			info.item = tree->indexOfTopLevelItem(item);
			info.subItem = column;
			target->DispatchNotify(info);
		});
		QObject::connect(tree, &QTreeWidget::itemChanged, context, [=](QTreeWidgetItem *item, int) { Send(target, id, Notify::CheckChange, control, tree->indexOfTopLevelItem(item)); });
		return;
	}
	if(auto *upDown = qobject_cast<UpDown *>(control))
	{
		QObject::connect(upDown, &UpDown::deltaPos, context, [=](int pos, int delta, bool *accepted) {
			NotifyInfo info;
			info.id = id;
			info.code = Notify::DeltaPos;
			info.control = control;
			info.pos = pos;
			info.delta = delta;
			target->DispatchNotify(info);
			if(info.result != 0)
				*accepted = false;
		});
		return;
	}
	if(auto *tabs = qobject_cast<TabControl *>(control))
	{
		QObject::connect(tabs->tabBar(), &QTabBar::currentChanged, context, [=](int index) { Send(target, id, Notify::SelChange, control, index); });
		return;
	}
}

} // namespace mfcless
