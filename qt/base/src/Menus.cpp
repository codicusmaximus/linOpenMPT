/*
 * Menus.cpp
 * ---------
 * Purpose: Menus from resources and built item by item, by command ID.
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */

#include "mfcless/Menus.h"
#include "mfcless/ModMessage.h"
#include "mfcless/Resources.h"

#include <QtGui/QAction>
#include <QtGui/QActionGroup>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMenuBar>

namespace mfcless
{

namespace
{

const char *const IdProperty = "mfcless_command";
const char *const ConnectedProperty = "mfcless_connected";

void SetCommand(QAction *action, uint32_t id)
{
	action->setData(QVariant::fromValue<uint>(id));
}

void ApplyFlags(QAction *action, uint32_t flags)
{
	action->setEnabled(!(flags & (menu::Grayed | menu::Disabled)));
	if(flags & menu::Checked)
	{
		action->setCheckable(true);
		action->setChecked(true);
	} else if(action->isCheckable())
	{
		action->setChecked(false);
	}
}

QMenu *NewMenu(QWidget *parent = nullptr)
{
	auto *m = new QMenu(parent);
	m->setToolTipsVisible(false);
	return m;
}

void Fill(QMenu *menu, const std::vector<res::MenuItem> &items)
{
	for(const res::MenuItem &item : items)
	{
		const QString text = QString::fromStdU16String(item.text);
		if(item.separator || (item.flags & menu::Separator))
		{
			menu->addSeparator();
		} else if(!item.children.empty() || (item.flags & menu::Popup))
		{
			QMenu *sub = NewMenu(menu);
			sub->setTitle(text);
			Fill(sub, item.children);
			QAction *a = menu->addMenu(sub);
			a->setEnabled(!(item.flags & (menu::Grayed | menu::Disabled)));
		} else
		{
			QAction *a = menu->addAction(text);
			SetCommand(a, item.id);
			ApplyFlags(a, item.flags);
		}
	}
}

// The action at a position, or of a command (searching sub menus, as
// Windows does for MF_BYCOMMAND).
QAction *Find(const QMenu *menu, uint32_t item, uint32_t flags)
{
	if(!menu)
		return nullptr;
	const auto actions = menu->actions();
	if(flags & menu::ByPosition)
		return item < static_cast<uint32_t>(actions.size()) ? actions[item] : nullptr;
	for(QAction *a : actions)
	{
		if(a->menu())
		{
			if(QAction *found = Find(a->menu(), item, flags))
				return found;
		} else if(!a->isSeparator() && ActionCommand(a) == item)
		{
			return a;
		}
	}
	return nullptr;
}

QAction *MakeItem(QMenu *parent, uint32_t flags, uint32_t id, const QString &text)
{
	QAction *a = new QAction(parent);
	if(flags & menu::Separator)
	{
		a->setSeparator(true);
	} else
	{
		a->setText(text);
		SetCommand(a, id);
		ApplyFlags(a, flags);
	}
	return a;
}

QAction *MakePopupItem(QMenu *parent, uint32_t flags, QMenu *sub, const QString &text)
{
	sub->setParent(parent, sub->windowFlags());
	sub->setTitle(text);
	QAction *a = sub->menuAction();
	a->setEnabled(!(flags & (menu::Grayed | menu::Disabled)));
	return a;
}

void InsertAt(QMenu *menu, QAction *before, QAction *a)
{
	if(before)
		menu->insertAction(before, a);
	else
		menu->addAction(a);
}

} // namespace

uint32_t ActionCommand(const QAction *action)
{
	if(!action || action->isSeparator() || action->menu())
		return 0;
	return action->data().toUInt();
}

void ApplyCommandState(QAction *action, const CommandState &state)
{
	action->setEnabled(state.enabled);
	if(state.checked)
	{
		action->setCheckable(true);
		action->setChecked(*state.checked);
	}
	if(state.text)
		action->setText(*state.text);
}

void UpdateMenuItems(QMenu *menu, const CommandUpdater &update)
{
	if(!update)
		return;
	for(QAction *a : menu->actions())
	{
		const uint32_t id = ActionCommand(a);
		if(id == 0)
			continue;
		CommandState state;
		state.id = id;
		state.checked.reset();
		if(update(id, state))
			ApplyCommandState(a, state);
	}
}

void ConnectMenu(QMenu *menu, std::function<void(uint32_t)> onCommand, CommandUpdater update)
{
	if(!menu || menu->property(ConnectedProperty).toBool())
		return;
	menu->setProperty(ConnectedProperty, true);
	if(onCommand)
	{
		QObject::connect(menu, &QMenu::triggered, menu, [menu, onCommand](QAction *a)
		{
			// Sub menus pass their items' triggers on to their parents; only
			// the outermost connected menu reports them.
			for(QWidget *p = menu->parentWidget(); p; p = p->parentWidget())
			{
				if(p->property(ConnectedProperty).toBool() && qobject_cast<QMenu *>(p))
					return;
			}
			if(const uint32_t id = ActionCommand(a))
				onCommand(id);
		});
	}
	QObject::connect(menu, &QMenu::aboutToShow, menu, [menu, update]
	{
		UpdateMenuItems(menu, update);
		// Sub menus added since: connected for their state only.
		for(QAction *a : menu->actions())
		{
			if(a->menu())
				ConnectMenu(a->menu(), {}, update);
		}
	});
}

void BuildMenuBar(QMenuBar *bar, uint32_t resourceId, std::function<void(uint32_t)> onCommand, CommandUpdater update)
{
	const auto items = res::Menu(resourceId);
	if(!items)
		return;
	for(const res::MenuItem &item : *items)
	{
		const QString text = QString::fromStdU16String(item.text);
		if(!item.children.empty() || (item.flags & menu::Popup))
		{
			QMenu *sub = NewMenu(bar);
			sub->setTitle(text);
			Fill(sub, item.children);
			bar->addMenu(sub);
			ConnectMenu(sub, {}, update);
		} else if(!item.separator)
		{
			QAction *a = bar->addAction(text);
			SetCommand(a, item.id);
		}
	}
	if(onCommand)
	{
		QObject::connect(bar, &QMenuBar::triggered, bar, [onCommand](QAction *a)
		{
			if(const uint32_t id = ActionCommand(a))
				onCommand(id);
		});
	}
}

QAction *FindMenuAction(const QWidget *menuOrBar, uint32_t id)
{
	if(!menuOrBar)
		return nullptr;
	for(QAction *a : menuOrBar->actions())
	{
		if(a->menu())
		{
			if(QAction *found = Find(a->menu(), id, 0))
				return found;
		} else if(ActionCommand(a) == id && id != 0)
		{
			return a;
		}
	}
	return nullptr;
}


Menu::~Menu()
{
	Release();
}

Menu::Menu(Menu &&other) noexcept
	: m_menu(other.m_menu), m_owned(other.m_owned)
{
	other.m_menu = nullptr;
	other.m_owned = false;
}

Menu &Menu::operator=(Menu &&other) noexcept
{
	if(this != &other)
	{
		Release();
		m_menu = other.m_menu;
		m_owned = other.m_owned;
		other.m_menu = nullptr;
		other.m_owned = false;
	}
	return *this;
}

void Menu::Release()
{
	if(m_owned && m_menu && !m_menu->parent())
		delete m_menu.data();
	m_menu = nullptr;
	m_owned = false;
}

Menu Menu::CreatePopup()
{
	return Menu(NewMenu(), true);
}

Menu Menu::Load(uint32_t resourceId)
{
	const auto items = res::Menu(resourceId);
	if(!items)
		return Menu();
	QMenu *m = NewMenu();
	Fill(m, *items);
	return Menu(m, true);
}

Menu Menu::Wrap(QMenu *menu)
{
	return Menu(menu, false);
}

void Menu::Append(uint32_t flags, uint32_t id, const Text &text)
{
	if(m_menu)
		m_menu->addAction(MakeItem(m_menu, flags, id, text));
}

void Menu::AppendPopup(uint32_t flags, Menu &sub, const Text &text)
{
	if(!m_menu || !sub.m_menu)
		return;
	m_menu->addAction(MakePopupItem(m_menu, flags, sub.m_menu, text));
	sub.m_owned = false;
}

void Menu::Insert(uint32_t pos, uint32_t flags, uint32_t id, const Text &text)
{
	if(m_menu)
		InsertAt(m_menu, FindAction(pos, flags & menu::ByPosition), MakeItem(m_menu, flags & ~menu::ByPosition, id, text));
}

void Menu::InsertPopup(uint32_t pos, uint32_t flags, Menu &sub, const Text &text)
{
	if(!m_menu || !sub.m_menu)
		return;
	InsertAt(m_menu, FindAction(pos, flags & menu::ByPosition), MakePopupItem(m_menu, flags, sub.m_menu, text));
	sub.m_owned = false;
}

bool Menu::Delete(uint32_t item, uint32_t flags)
{
	QAction *a = FindAction(item, flags);
	if(!a)
		return false;
	if(QMenu *sub = a->menu())
	{
		if(QWidget *w = qobject_cast<QWidget *>(a->parent()))
			w->removeAction(a);
		sub->deleteLater();
	} else
	{
		for(QObject *w : a->associatedObjects())
		{
			if(auto *widget = qobject_cast<QWidget *>(w))
				widget->removeAction(a);
		}
		a->deleteLater();
	}
	return true;
}

bool Menu::Remove(uint32_t item, uint32_t flags)
{
	QAction *a = FindAction(item, flags);
	if(!a)
		return false;
	for(QObject *w : a->associatedObjects())
	{
		if(auto *widget = qobject_cast<QWidget *>(w))
			widget->removeAction(a);
	}
	if(!a->menu())
		a->deleteLater();
	return true;
}

bool Menu::Modify(uint32_t item, uint32_t flags, uint32_t newId, const Text &text)
{
	QAction *a = FindAction(item, flags & menu::ByPosition);
	if(!a)
		return false;
	if(flags & menu::Separator)
	{
		a->setSeparator(true);
		return true;
	}
	a->setText(text);
	if(!a->menu())
		SetCommand(a, newId);
	ApplyFlags(a, flags);
	return true;
}

bool Menu::ModifyPopup(uint32_t item, uint32_t flags, Menu &sub, const Text &text)
{
	QAction *a = FindAction(item, flags & menu::ByPosition);
	if(!a || !sub.m_menu)
		return false;
	QMenu *parent = qobject_cast<QMenu *>(a->parent());
	if(!parent)
		parent = m_menu;
	parent->insertAction(a, MakePopupItem(parent, flags, sub.m_menu, text));
	parent->removeAction(a);
	if(a->menu())
		a->menu()->deleteLater();
	else
		a->deleteLater();
	sub.m_owned = false;
	return true;
}

void Menu::Check(uint32_t item, bool checked, uint32_t flags)
{
	if(QAction *a = FindAction(item, flags))
	{
		a->setCheckable(true);
		a->setChecked(checked);
	}
}

void Menu::Enable(uint32_t item, bool enabled, uint32_t flags)
{
	if(QAction *a = FindAction(item, flags))
		a->setEnabled(enabled);
}

bool Menu::IsChecked(uint32_t item, uint32_t flags) const
{
	QAction *a = FindAction(item, flags);
	return a && a->isChecked();
}

void Menu::SetDefault(uint32_t item, uint32_t flags)
{
	if(QAction *a = FindAction(item, flags))
	{
		QFont f = a->font();
		f.setBold(true);
		a->setFont(f);
		if(m_menu)
			m_menu->setDefaultAction(a);
	}
}

int Menu::ItemCount() const
{
	return m_menu ? static_cast<int>(m_menu->actions().size()) : 0;
}

int Menu::ItemID(int pos) const
{
	if(!m_menu || pos < 0 || pos >= m_menu->actions().size())
		return -1;
	QAction *a = m_menu->actions()[pos];
	if(a->menu())
		return -1;
	return static_cast<int>(ActionCommand(a));
}

QString Menu::ItemText(uint32_t item, uint32_t flags) const
{
	QAction *a = FindAction(item, flags);
	return a ? a->text() : QString();
}

Menu Menu::SubMenu(int pos) const
{
	if(!m_menu || pos < 0 || pos >= m_menu->actions().size())
		return Menu();
	return Menu(m_menu->actions()[pos]->menu(), false);
}

QAction *Menu::FindAction(uint32_t item, uint32_t flags) const
{
	return Find(m_menu, item, flags);
}

uint32_t Menu::TrackPopup(QPoint screenPos, QObject *target, Track mode, const CommandUpdater &update) const
{
	if(!m_menu)
		return 0;
	if(update)
		ConnectMenu(m_menu, {}, update);
	QAction *chosen = m_menu->exec(screenPos);
	const uint32_t id = ActionCommand(chosen);
	if(id && mode == Track::SendCommand)
		PostCommand(target, id);
	return id;
}

} // namespace mfcless
