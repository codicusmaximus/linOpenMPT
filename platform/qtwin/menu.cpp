// Win32 menus on Qt. An HMENU is a list of items, as Windows keeps it; Qt
// menus are built from it each time one opens, after the owner window got
// WM_INITMENUPOPUP (where MFC updates the items' state through CCmdUI), so
// what is shown is what the program set. A window's menu bar is a QMenuBar
// in its non-client area.
//
// Messages are the ones Windows sends during menu tracking: WM_ENTERMENULOOP,
// WM_INITMENU, WM_INITMENUPOPUP, WM_MENUSELECT, WM_UNINITMENUPOPUP,
// WM_EXITMENULOOP, and a posted WM_COMMAND for the chosen item.
#include "QtWin.h"

#include <QtCore/QPointer>
#include <QtGui/QActionGroup>
#include <QtGui/QScreen>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMenuBar>

#include <algorithm>
#include <cstring>
#include <set>

namespace linopenmpt::qtwin
{
namespace
{

struct MenuItem
{
	UINT type = MFT_STRING; // MFT_*
	UINT state = 0;         // MFS_*
	UINT id = 0;
	HMENU sub = nullptr;
	std::wstring text;
	ULONG_PTR data = 0;
	HBITMAP checked = nullptr, unchecked = nullptr, bitmap = nullptr;
};

struct Menu
{
	std::vector<MenuItem> items;
	bool popup = false;
	HWND bar = nullptr; // the window whose menu bar this is
};

std::set<Menu *> &Menus()
{
	static auto *menus = new std::set<Menu *>;
	return *menus;
}

Menu *ToMenu(HMENU h)
{
	auto *m = reinterpret_cast<Menu *>(h);
	return (m != nullptr && Menus().count(m)) ? m : nullptr;
}

HMENU Handle(Menu *m)
{
	return reinterpret_cast<HMENU>(m);
}

HMENU NewMenu(bool popup)
{
	auto *m = new Menu;
	m->popup = popup;
	Menus().insert(m);
	return Handle(m);
}

// The item an id or position refers to; by command, submenus are searched too.
MenuItem *FindItem(HMENU h, UINT item, bool byPosition, Menu **owner = nullptr, int *index = nullptr)
{
	Menu *m = ToMenu(h);
	if(m == nullptr)
		return nullptr;
	if(byPosition)
	{
		if(item >= m->items.size())
			return nullptr;
		if(owner)
			*owner = m;
		if(index)
			*index = static_cast<int>(item);
		return &m->items[item];
	}
	for(std::size_t i = 0; i < m->items.size(); i++)
	{
		MenuItem &it = m->items[i];
		if(it.sub == nullptr && it.id == item)
		{
			if(owner)
				*owner = m;
			if(index)
				*index = static_cast<int>(i);
			return &it;
		}
	}
	for(MenuItem &it : m->items)
	{
		if(it.sub != nullptr)
		{
			if(MenuItem *found = FindItem(it.sub, item, false, owner, index))
				return found;
		}
	}
	// A popup item can be addressed by its submenu handle as its id.
	for(std::size_t i = 0; i < m->items.size(); i++)
	{
		if(m->items[i].sub != nullptr && reinterpret_cast<UINT_PTR>(m->items[i].sub) == item)
		{
			if(owner)
				*owner = m;
			if(index)
				*index = static_cast<int>(i);
			return &m->items[i];
		}
	}
	return nullptr;
}

// An item from the MF_* flags of AppendMenu, InsertMenu and ModifyMenu.
MenuItem ItemFromFlags(UINT flags, UINT_PTR id, LPCTSTR text)
{
	MenuItem it;
	if(flags & MF_SEPARATOR)
		it.type = MFT_SEPARATOR;
	else if(flags & MF_BITMAP)
		it.bitmap = reinterpret_cast<HBITMAP>(const_cast<TCHAR *>(text));
	else if(flags & MF_OWNERDRAW)
		it.type = MF_OWNERDRAW, it.data = reinterpret_cast<ULONG_PTR>(text);
	else if(text != nullptr)
		it.text = text;
	it.type |= flags & (MF_MENUBREAK | MF_MENUBARBREAK | MF_RIGHTJUSTIFY);
	it.state = flags & (MF_GRAYED | MF_DISABLED | MF_CHECKED | MF_HILITE | MF_DEFAULT);
	if(it.state & MF_DISABLED)
		it.state |= MF_GRAYED;
	if(flags & MF_POPUP)
		it.sub = reinterpret_cast<HMENU>(id);
	else
		it.id = static_cast<UINT>(id);
	return it;
}

// ---- building Qt menus -----------------------------------------------------------------

QString ItemText(const MenuItem &it)
{
	// "&Open...\tCtrl+O": Qt shows the part after the tab as the shortcut,
	// and uses & for mnemonics too.
	return ToQString(it.text.c_str());
}

// Menu tracking state: the window that gets the messages.
struct Tracking
{
	HWND owner = nullptr;
	UINT chosen = 0;
	bool returnCommand = false;
	bool notify = true;
};
Tracking *g_tracking = nullptr;

void SendMenuSelect(HWND owner, UINT idOrPos, UINT flags, HMENU menu)
{
	if(owner != nullptr && (g_tracking == nullptr || g_tracking->notify))
		::SendMessage(owner, WM_MENUSELECT, MAKEWPARAM(idOrPos, flags), reinterpret_cast<LPARAM>(menu));
}

void Fill(QMenu *qmenu, HMENU h, HWND owner);

QMenu *BuildPopup(QWidget *parent, HMENU h, HWND owner, int index, bool isSystem)
{
	auto *qmenu = new QMenu(parent);
	const QPointer<QMenu> guard(qmenu);
	QObject::connect(qmenu, &QMenu::aboutToShow, qmenu, [qmenu, h, owner, index, isSystem]()
	{
		if(owner != nullptr && (g_tracking == nullptr || g_tracking->notify))
			::SendMessage(owner, WM_INITMENUPOPUP, reinterpret_cast<WPARAM>(h), MAKELPARAM(index, isSystem ? TRUE : FALSE));
		Fill(qmenu, h, owner);
	});
	QObject::connect(qmenu, &QMenu::aboutToHide, qmenu, [h, owner]()
	{
		if(owner != nullptr && (g_tracking == nullptr || g_tracking->notify))
			::SendMessage(owner, WM_UNINITMENUPOPUP, reinterpret_cast<WPARAM>(h), 0);
	});
	QObject::connect(qmenu, &QMenu::hovered, qmenu, [h, owner](QAction *action)
	{
		const UINT pos = action->property("linopenmpt_pos").toUInt();
		const Menu *m = ToMenu(h);
		if(m == nullptr || pos >= m->items.size())
			return;
		const MenuItem &it = m->items[pos];
		UINT flags = MF_MOUSESELECT | MF_HILITE | (it.state & (MF_GRAYED | MF_CHECKED));
		if(it.sub != nullptr)
			SendMenuSelect(owner, pos, flags | MF_POPUP, h);
		else
			SendMenuSelect(owner, it.id, flags | (it.type & MF_SEPARATOR), h);
	});
	return qmenu;
}

void Choose(HWND owner, UINT id)
{
	if(g_tracking != nullptr)
	{
		g_tracking->chosen = id;
		if(g_tracking->returnCommand)
			return;
	}
	if(owner != nullptr)
		::PostMessage(owner, WM_COMMAND, MAKEWPARAM(id, 0), 0);
}

QAction *AddItem(QWidget *container, QMenu *asMenu, QMenuBar *asBar, HMENU h, UINT pos, HWND owner)
{
	const Menu *m = ToMenu(h);
	const MenuItem &it = m->items[pos];
	QAction *action = nullptr;
	if(it.type & MFT_SEPARATOR)
	{
		action = asMenu ? asMenu->addSeparator() : asBar->addSeparator();
	} else if(it.sub != nullptr)
	{
		QMenu *sub = BuildPopup(container, it.sub, owner, static_cast<int>(pos), false);
		sub->setTitle(ItemText(it));
		action = asMenu ? asMenu->addMenu(sub) : asBar->addMenu(sub);
	} else
	{
		action = new QAction(ItemText(it), container);
		const UINT id = it.id;
		QObject::connect(action, &QAction::triggered, action, [owner, id]() { Choose(owner, id); });
		if(asMenu)
			asMenu->addAction(action);
		else
			asBar->addAction(action);
	}
	action->setProperty("linopenmpt_pos", pos);
	if(!(it.type & MFT_SEPARATOR))
	{
		action->setEnabled(!(it.state & MFS_GRAYED));
		if(it.state & MFS_CHECKED)
		{
			action->setCheckable(true);
			action->setChecked(true);
			if(it.type & MFT_RADIOCHECK)
			{
				// Qt draws a radio mark for exclusive groups.
				auto *group = new QActionGroup(action);
				group->setExclusive(true);
				group->addAction(action);
			}
		}
		if(it.state & MFS_DEFAULT)
		{
			QFont f = action->font();
			f.setBold(true);
			action->setFont(f);
		}
		if(it.bitmap != nullptr)
		{
			const QImage image = BitmapImage(it.bitmap);
			if(!image.isNull())
				action->setIcon(QIcon(QPixmap::fromImage(image)));
		}
	}
	return action;
}

void Fill(QMenu *qmenu, HMENU h, HWND owner)
{
	// Old submenus go with their actions.
	for(QAction *a : qmenu->actions())
	{
		if(QMenu *sub = a->menu())
			sub->deleteLater();
	}
	qmenu->clear();
	const Menu *m = ToMenu(h);
	if(m == nullptr)
		return;
	for(UINT pos = 0; pos < m->items.size(); pos++)
		AddItem(qmenu, qmenu, nullptr, h, pos, owner);
}

// The key a mnemonic (&x) in a title gives.
QChar Mnemonic(const std::wstring &text)
{
	for(std::size_t i = 0; i + 1 < text.size(); i++)
	{
		if(text[i] == L'&')
		{
			if(text[i + 1] == L'&')
			{
				i++;
				continue;
			}
			return QChar(static_cast<char16_t>(text[i + 1])).toUpper();
		}
	}
	return QChar();
}

// Menu bars do not take keyboard shortcuts themselves: keys reach the window
// as messages first (upstream binds Alt+letter keys), and unhandled Alt+key
// presses open the menu through WM_SYSCOMMAND / SC_KEYMENU, as on Windows.
class MenuBar : public QMenuBar
{
public:
	using QMenuBar::QMenuBar;

protected:
	bool event(QEvent *e) override
	{
		if(e->type() == QEvent::Shortcut)
			return true; // swallowed: see above
		return QMenuBar::event(e);
	}
};

} // namespace

// ---- the menu bar of a window --------------------------------------------------------------


void LayoutMenuBar(Win32Widget *w)
{
	QMenuBar *bar = w->data.menuBar;
	if(bar == nullptr)
		return;
	const QMargins frame = FrameMargins(w->data.style, w->data.exStyle, (w->data.style & WS_CHILD) != 0);
	const QRect inside = w->rect().marginsRemoved(frame);
	bar->setGeometry(inside.left(), inside.top(), inside.width(), MenuBarHeight(w));
	bar->show();
	bar->raise();
}

namespace
{

// The menu bar takes non-client space.
void MenuBarChanged(Win32Widget *w)
{
	if(UpdateNonClient(w))
	{
		const QSize client = ClientRect(w).size();
		w->data.clientSize = client;
		::InvalidateRect(w->hwnd(), nullptr, TRUE);
		::SendMessage(w->hwnd(), WM_SIZE, SIZE_RESTORED, MAKELPARAM(client.width(), client.height()));
	}
}

} // namespace

void AttachMenuBar(Win32Widget *w, HMENU menu)
{
	if(Menu *old = ToMenu(w->data.menu); old && old->bar == w->hwnd())
		old->bar = nullptr;
	w->data.menu = menu;
	Menu *m = ToMenu(menu);
	if(m == nullptr)
	{
		if(w->data.menuBar)
		{
			w->data.menuBar->hide();
			w->data.menuBar->deleteLater();
			w->data.menuBar = nullptr;
		}
		MenuBarChanged(w);
		return;
	}
	m->bar = w->hwnd();
	if(w->data.menuBar == nullptr)
	{
		auto *bar = new MenuBar(w);
		bar->setNativeMenuBar(false);
		w->data.menuBar = bar;
		const HWND hwnd = w->hwnd();
		// WM_INITMENU when the user starts using the menu bar.
		QObject::connect(bar, &QMenuBar::hovered, bar, [hwnd](QAction *action)
		{
			WindowData *d = Data(hwnd);
			if(d == nullptr)
				return;
			const UINT pos = action->property("linopenmpt_pos").toUInt();
			const Menu *m = ToMenu(d->menu);
			if(m == nullptr || pos >= m->items.size())
				return;
			const MenuItem &it = m->items[pos];
			SendMenuSelect(hwnd, it.sub ? pos : it.id, MF_HILITE | (it.sub ? MF_POPUP : 0), d->menu);
		});
	}
	// Built now; DrawMenuBar rebuilds it after changes.
	QMenuBar *bar = w->data.menuBar;
	for(QAction *a : bar->actions())
	{
		if(QMenu *sub = a->menu())
			sub->deleteLater();
	}
	bar->clear();
	const HWND hwnd = w->hwnd();
	for(UINT pos = 0; pos < m->items.size(); pos++)
	{
		QAction *a = AddItem(bar, nullptr, bar, menu, pos, hwnd);
		if(QMenu *sub = a->menu())
		{
			// Before the first popup of a menu bar session.
			QObject::connect(sub, &QMenu::aboutToShow, bar, [hwnd]()
			{
				WindowData *d = Data(hwnd);
				if(d != nullptr && !d->menuBar->property("linopenmpt_inMenu").toBool())
				{
					d->menuBar->setProperty("linopenmpt_inMenu", true);
					::SendMessage(hwnd, WM_ENTERMENULOOP, FALSE, 0);
					::SendMessage(hwnd, WM_INITMENU, reinterpret_cast<WPARAM>(d->menu), 0);
				}
			}, Qt::DirectConnection);
			QObject::connect(sub, &QMenu::aboutToHide, bar, [hwnd]()
			{
				// Ends the session unless another popup of the bar opens.
				QTimer::singleShot(0, [hwnd]()
				{
					WindowData *d = Data(hwnd);
					if(d == nullptr || d->menuBar == nullptr || !d->menuBar->property("linopenmpt_inMenu").toBool())
						return;
					if(d->menuBar->activeAction() && d->menuBar->activeAction()->menu() && d->menuBar->activeAction()->menu()->isVisible())
						return;
					d->menuBar->setProperty("linopenmpt_inMenu", false);
					SendMenuSelect(hwnd, 0xFFFF, 0, nullptr);
					::SendMessage(hwnd, WM_EXITMENULOOP, FALSE, 0);
				});
			});
		}
	}
	MenuBarChanged(w);
}

bool OpenMenuByKey(Win32Widget *w, WCHAR key)
{
	// SC_KEYMENU: the window's (or its top-level window's) menu bar.
	Win32Widget *top = w;
	while(top != nullptr && top->data.menuBar == nullptr)
		top = dynamic_cast<Win32Widget *>(top->parentWidget());
	if(top == nullptr)
		return false;
	QMenuBar *bar = top->data.menuBar;
	const Menu *m = ToMenu(top->data.menu);
	if(m == nullptr || bar->actions().isEmpty())
		return false;
	const QList<QAction *> actions = bar->actions();
	if(key == 0 || key == L' ')
	{
		bar->setActiveAction(actions.first());
		return true;
	}
	const QChar wanted = QChar(static_cast<char16_t>(key)).toUpper();
	for(QAction *a : actions)
	{
		const UINT pos = a->property("linopenmpt_pos").toUInt();
		if(pos < m->items.size() && Mnemonic(m->items[pos].text) == wanted && a->isEnabled())
		{
			if(a->menu())
				bar->setActiveAction(a);
			else
				a->trigger();
			return true;
		}
	}
	::MessageBeep(0);
	return false;
}

} // namespace linopenmpt::qtwin

using namespace linopenmpt::qtwin;

// ---- the API ------------------------------------------------------------------------------

HMENU CreateMenu()
{
	return NewMenu(false);
}

HMENU CreatePopupMenu()
{
	return NewMenu(true);
}

BOOL IsMenu(HMENU menu)
{
	return ToMenu(menu) != nullptr;
}

BOOL DestroyMenu(HMENU h)
{
	Menu *m = ToMenu(h);
	if(m == nullptr)
		return FALSE;
	// Submenus are destroyed with their menu.
	for(MenuItem &it : m->items)
	{
		if(it.sub)
			DestroyMenu(it.sub);
	}
	if(m->bar != nullptr)
	{
		if(Win32Widget *w = FromHandle(m->bar); w && w->data.menu == h)
			AttachMenuBar(w, nullptr);
	}
	Menus().erase(m);
	delete m;
	return TRUE;
}

namespace
{
// Reads a standard menu template (MENUITEMTEMPLATE items).
const BYTE *ReadMenuItems(HMENU menu, const BYTE *p, const BYTE *end)
{
	for(;;)
	{
		if(p + 2 > end)
			return end;
		WORD flags;
		std::memcpy(&flags, p, 2);
		p += 2;
		WORD id = 0;
		if(!(flags & MF_POPUP))
		{
			std::memcpy(&id, p, 2);
			p += 2;
		}
		std::u16string text;
		while(p + 2 <= end)
		{
			char16_t c;
			std::memcpy(&c, p, 2);
			p += 2;
			if(c == 0)
				break;
			text.push_back(c);
		}
		const std::wstring wtext = QString::fromStdU16String(text).toStdWString();
		const UINT itemFlags = flags & ~(MF_END | MF_POPUP);
		if(flags & MF_POPUP)
		{
			HMENU sub = CreatePopupMenu();
			p = ReadMenuItems(sub, p, end);
			AppendMenu(menu, MF_POPUP | itemFlags, reinterpret_cast<UINT_PTR>(sub), wtext.c_str());
		} else if(id == 0 && wtext.empty() && !(flags & ~MF_END))
		{
			AppendMenu(menu, MF_SEPARATOR, 0, nullptr);
		} else
		{
			AppendMenu(menu, MF_STRING | itemFlags, id, wtext.c_str());
		}
		if(flags & MF_END)
			return p;
	}
}

// MENUEX_TEMPLATE_ITEM items (DWORD aligned).
const BYTE *ReadMenuExItems(HMENU menu, const BYTE *base, const BYTE *p, const BYTE *end)
{
	for(;;)
	{
		p = base + ((p - base + 3) & ~3);
		if(p + 14 > end)
			return end;
		DWORD type, state, id;
		WORD flags;
		std::memcpy(&type, p, 4);
		std::memcpy(&state, p + 4, 4);
		std::memcpy(&id, p + 8, 4);
		std::memcpy(&flags, p + 12, 2);
		p += 14;
		std::u16string text;
		while(p + 2 <= end)
		{
			char16_t c;
			std::memcpy(&c, p, 2);
			p += 2;
			if(c == 0)
				break;
			text.push_back(c);
		}
		const std::wstring wtext = QString::fromStdU16String(text).toStdWString();
		MENUITEMINFO info{};
		info.cbSize = sizeof(info);
		info.fMask = MIIM_FTYPE | MIIM_STATE | MIIM_ID | MIIM_STRING;
		info.fType = type;
		info.fState = state;
		info.wID = id;
		info.dwTypeData = const_cast<LPTSTR>(wtext.c_str());
		if(flags & 0x01)
		{
			p = base + ((p - base + 3) & ~3);
			p += 4; // help id
			HMENU sub = CreatePopupMenu();
			p = ReadMenuExItems(sub, base, p, end);
			info.fMask |= MIIM_SUBMENU;
			info.hSubMenu = sub;
		}
		InsertMenuItem(menu, static_cast<UINT>(GetMenuItemCount(menu)), TRUE, &info);
		if(flags & 0x80)
			return p;
	}
}
} // namespace

HMENU LoadMenuIndirect(const void *menuTemplate)
{
	const auto *p = static_cast<const BYTE *>(menuTemplate);
	if(p == nullptr)
		return nullptr;
	WORD version, offset;
	std::memcpy(&version, p, 2);
	std::memcpy(&offset, p + 2, 2);
	HMENU menu = CreateMenu();
	// Templates carry no size; items end at MF_END.
	const BYTE *end = p + (1 << 20);
	if(version == 1)
		ReadMenuExItems(menu, p, p + 4 + offset, end);
	else
		ReadMenuItems(menu, p + 4 + offset, end);
	return menu;
}

HMENU LoadMenu(HINSTANCE instance, LPCTSTR name)
{
	HRSRC res = FindResource(instance, name, RT_MENU);
	if(res == nullptr)
		return nullptr;
	return LoadMenuIndirect(LockResource(LoadResource(instance, res)));
}

BOOL InsertMenu(HMENU h, UINT position, UINT flags, UINT_PTR id, LPCTSTR text)
{
	Menu *m = ToMenu(h);
	if(m == nullptr)
		return FALSE;
	std::size_t index = m->items.size();
	if(flags & MF_BYPOSITION)
	{
		if(position < m->items.size())
			index = position;
	} else
	{
		Menu *owner = nullptr;
		int at = -1;
		if(FindItem(h, position, false, &owner, &at) != nullptr)
		{
			m = owner;
			index = static_cast<std::size_t>(at);
		}
	}
	m->items.insert(m->items.begin() + static_cast<std::ptrdiff_t>(index), ItemFromFlags(flags & ~MF_BYPOSITION, id, text));
	return TRUE;
}

BOOL AppendMenu(HMENU h, UINT flags, UINT_PTR id, LPCTSTR text)
{
	Menu *m = ToMenu(h);
	if(m == nullptr)
		return FALSE;
	m->items.push_back(ItemFromFlags(flags, id, text));
	return TRUE;
}

BOOL ModifyMenu(HMENU h, UINT position, UINT flags, UINT_PTR id, LPCTSTR text)
{
	MenuItem *it = FindItem(h, position, (flags & MF_BYPOSITION) != 0);
	if(it == nullptr)
		return FALSE;
	const HMENU oldSub = it->sub;
	*it = ItemFromFlags(flags & ~MF_BYPOSITION, id, text);
	if(oldSub != nullptr && oldSub != it->sub)
		DestroyMenu(oldSub); // replaced popups are destroyed, as on Windows
	return TRUE;
}

namespace
{
BOOL Remove(HMENU h, UINT position, UINT flags, bool destroy)
{
	Menu *owner = nullptr;
	int index = -1;
	MenuItem *it = FindItem(h, position, (flags & MF_BYPOSITION) != 0, &owner, &index);
	if(it == nullptr)
		return FALSE;
	const HMENU sub = it->sub;
	owner->items.erase(owner->items.begin() + index);
	if(destroy && sub)
		DestroyMenu(sub);
	return TRUE;
}
} // namespace

BOOL RemoveMenu(HMENU h, UINT position, UINT flags)
{
	return Remove(h, position, flags, false);
}

BOOL DeleteMenu(HMENU h, UINT position, UINT flags)
{
	return Remove(h, position, flags, true);
}

DWORD CheckMenuItem(HMENU h, UINT id, UINT check)
{
	MenuItem *it = FindItem(h, id, (check & MF_BYPOSITION) != 0);
	if(it == nullptr)
		return static_cast<DWORD>(-1);
	const DWORD old = it->state & MF_CHECKED;
	it->state = (it->state & ~MF_CHECKED) | (check & MF_CHECKED);
	return old;
}

BOOL CheckMenuRadioItem(HMENU h, UINT first, UINT last, UINT check, UINT flags)
{
	Menu *m = ToMenu(h);
	if(m == nullptr)
		return FALSE;
	bool found = false;
	if(flags & MF_BYPOSITION)
	{
		for(UINT i = first; i <= last && i < m->items.size(); i++)
		{
			MenuItem &it = m->items[i];
			it.state &= ~MF_CHECKED;
			if(i == check)
			{
				it.state |= MF_CHECKED;
				it.type |= MFT_RADIOCHECK;
				found = true;
			}
		}
	} else
	{
		for(MenuItem &it : m->items)
		{
			if(it.sub != nullptr || it.id < first || it.id > last)
				continue;
			it.state &= ~MF_CHECKED;
			if(it.id == check)
			{
				it.state |= MF_CHECKED;
				it.type |= MFT_RADIOCHECK;
				found = true;
			}
		}
	}
	return found ? TRUE : FALSE;
}

BOOL EnableMenuItem(HMENU h, UINT id, UINT enable)
{
	MenuItem *it = FindItem(h, id, (enable & MF_BYPOSITION) != 0);
	if(it == nullptr)
		return -1;
	const BOOL old = static_cast<BOOL>(it->state & (MF_GRAYED | MF_DISABLED));
	it->state = (it->state & ~(MF_GRAYED | MF_DISABLED)) | (enable & (MF_GRAYED | MF_DISABLED));
	if(it->state & MF_DISABLED)
		it->state |= MF_GRAYED;
	return old;
}

UINT GetMenuState(HMENU h, UINT id, UINT flags)
{
	MenuItem *it = FindItem(h, id, (flags & MF_BYPOSITION) != 0);
	if(it == nullptr)
		return static_cast<UINT>(-1);
	UINT state = it->state | (it->type & (MF_SEPARATOR | MF_MENUBREAK | MF_MENUBARBREAK | MF_OWNERDRAW));
	if(it->sub != nullptr)
		state = (state & 0xFF) | MF_POPUP | (static_cast<UINT>(GetMenuItemCount(it->sub)) << 8);
	return state;
}

int GetMenuItemCount(HMENU h)
{
	Menu *m = ToMenu(h);
	return m ? static_cast<int>(m->items.size()) : -1;
}

UINT GetMenuItemID(HMENU h, int pos)
{
	Menu *m = ToMenu(h);
	if(m == nullptr || pos < 0 || static_cast<std::size_t>(pos) >= m->items.size())
		return static_cast<UINT>(-1);
	const MenuItem &it = m->items[static_cast<std::size_t>(pos)];
	return it.sub ? static_cast<UINT>(-1) : it.id;
}

HMENU GetSubMenu(HMENU h, int pos)
{
	Menu *m = ToMenu(h);
	if(m == nullptr || pos < 0 || static_cast<std::size_t>(pos) >= m->items.size())
		return nullptr;
	return m->items[static_cast<std::size_t>(pos)].sub;
}

int GetMenuString(HMENU h, UINT id, LPTSTR text, int max, UINT flags)
{
	MenuItem *it = FindItem(h, id, (flags & MF_BYPOSITION) != 0);
	if(it == nullptr)
		return 0;
	if(text == nullptr || max <= 0)
		return static_cast<int>(it->text.size());
	const int n = std::min(static_cast<int>(it->text.size()), max - 1);
	std::wmemcpy(text, it->text.data(), n);
	text[n] = 0;
	return n;
}

BOOL GetMenuItemInfo(HMENU h, UINT item, BOOL byPosition, LPMENUITEMINFO info)
{
	MenuItem *it = FindItem(h, item, byPosition != FALSE);
	if(it == nullptr || info == nullptr)
		return FALSE;
	if(info->fMask & (MIIM_FTYPE | MIIM_TYPE))
		info->fType = it->type;
	if(info->fMask & MIIM_STATE)
		info->fState = it->state;
	if(info->fMask & MIIM_ID)
		info->wID = it->sub ? reinterpret_cast<UINT_PTR>(it->sub) : it->id;
	if(info->fMask & MIIM_SUBMENU)
		info->hSubMenu = it->sub;
	if(info->fMask & MIIM_CHECKMARKS)
	{
		info->hbmpChecked = it->checked;
		info->hbmpUnchecked = it->unchecked;
	}
	if(info->fMask & MIIM_DATA)
		info->dwItemData = it->data;
	if(info->fMask & MIIM_BITMAP)
		info->hbmpItem = it->bitmap;
	if(info->fMask & (MIIM_STRING | MIIM_TYPE))
	{
		if(info->dwTypeData != nullptr && info->cch > 0)
		{
			const UINT n = std::min<UINT>(static_cast<UINT>(it->text.size()), info->cch - 1);
			std::wmemcpy(info->dwTypeData, it->text.data(), n);
			info->dwTypeData[n] = 0;
			info->cch = n;
		} else
		{
			info->cch = static_cast<UINT>(it->text.size());
		}
	}
	return TRUE;
}

BOOL SetMenuItemInfo(HMENU h, UINT item, BOOL byPosition, LPCMENUITEMINFO info)
{
	MenuItem *it = FindItem(h, item, byPosition != FALSE);
	if(it == nullptr || info == nullptr)
		return FALSE;
	if(info->fMask & (MIIM_FTYPE | MIIM_TYPE))
		it->type = info->fType;
	if(info->fMask & MIIM_STATE)
		it->state = info->fState;
	if(info->fMask & MIIM_ID)
		it->id = info->wID;
	if(info->fMask & MIIM_SUBMENU)
		it->sub = info->hSubMenu;
	if(info->fMask & MIIM_CHECKMARKS)
	{
		it->checked = info->hbmpChecked;
		it->unchecked = info->hbmpUnchecked;
	}
	if(info->fMask & MIIM_DATA)
		it->data = info->dwItemData;
	if(info->fMask & MIIM_BITMAP)
		it->bitmap = info->hbmpItem;
	if((info->fMask & (MIIM_STRING | MIIM_TYPE)) && !(it->type & (MFT_SEPARATOR | MF_BITMAP | MF_OWNERDRAW)))
		it->text = info->dwTypeData ? info->dwTypeData : L"";
	return TRUE;
}

BOOL InsertMenuItem(HMENU h, UINT item, BOOL byPosition, LPCMENUITEMINFO info)
{
	Menu *m = ToMenu(h);
	if(m == nullptr || info == nullptr)
		return FALSE;
	std::size_t index = m->items.size();
	if(byPosition)
	{
		if(item < m->items.size())
			index = item;
	} else
	{
		Menu *owner = nullptr;
		int at = -1;
		if(FindItem(h, item, false, &owner, &at) != nullptr)
		{
			m = owner;
			index = static_cast<std::size_t>(at);
		}
	}
	m->items.insert(m->items.begin() + static_cast<std::ptrdiff_t>(index), MenuItem{});
	const HMENU target = reinterpret_cast<HMENU>(m);
	return SetMenuItemInfo(target, static_cast<UINT>(index), TRUE, info);
}

BOOL SetMenuDefaultItem(HMENU h, UINT item, UINT byPos)
{
	Menu *m = ToMenu(h);
	if(m == nullptr)
		return FALSE;
	for(MenuItem &it : m->items)
		it.state &= ~MFS_DEFAULT;
	if(item == static_cast<UINT>(-1))
		return TRUE;
	MenuItem *it = FindItem(h, item, byPos != 0);
	if(it == nullptr)
		return FALSE;
	it->state |= MFS_DEFAULT;
	return TRUE;
}

UINT GetMenuDefaultItem(HMENU h, UINT byPos, UINT)
{
	Menu *m = ToMenu(h);
	if(m == nullptr)
		return static_cast<UINT>(-1);
	for(std::size_t i = 0; i < m->items.size(); i++)
	{
		if(m->items[i].state & MFS_DEFAULT)
			return byPos ? static_cast<UINT>(i) : m->items[i].id;
	}
	return static_cast<UINT>(-1);
}

BOOL TrackPopupMenuEx(HMENU h, UINT flags, int x, int y, HWND hwnd, LPTPMPARAMS params)
{
	Menu *m = ToMenu(h);
	if(m == nullptr)
		return FALSE;
	Win32Widget *ownerWidget = FromHandle(hwnd);
	Tracking tracking;
	tracking.owner = hwnd;
	tracking.returnCommand = (flags & TPM_RETURNCMD) != 0;
	tracking.notify = !(flags & TPM_NONOTIFY);
	Tracking *previous = g_tracking;
	g_tracking = &tracking;
	if(tracking.notify && hwnd)
	{
		::SendMessage(hwnd, WM_ENTERMENULOOP, TRUE, 0);
		::SendMessage(hwnd, WM_INITMENU, reinterpret_cast<WPARAM>(h), 0);
	}
	{
		std::unique_ptr<QMenu> qmenu(BuildPopup(ownerWidget, h, hwnd, 0, false));
		// WM_INITMENUPOPUP comes before the menu is measured.
		if(tracking.notify && hwnd)
			::SendMessage(hwnd, WM_INITMENUPOPUP, reinterpret_cast<WPARAM>(h), MAKELPARAM(0, FALSE));
		Fill(qmenu.get(), h, hwnd);
		QObject::disconnect(qmenu.get(), &QMenu::aboutToShow, nullptr, nullptr);
		QObject::connect(qmenu.get(), &QMenu::aboutToShow, qmenu.get(), []() {});
		const QSize size = qmenu->sizeHint();
		QPoint pos(x, y);
		if(flags & TPM_RIGHTALIGN)
			pos.rx() -= size.width();
		else if(flags & TPM_CENTERALIGN)
			pos.rx() -= size.width() / 2;
		if(flags & TPM_BOTTOMALIGN)
			pos.ry() -= size.height();
		else if(flags & TPM_VCENTERALIGN)
			pos.ry() -= size.height() / 2;
		if(params != nullptr)
		{
			// Keep clear of the excluded rectangle (the button that opened it).
			const QRect exclude(QPoint(params->rcExclude.left, params->rcExclude.top), QPoint(params->rcExclude.right - 1, params->rcExclude.bottom - 1));
			if(QRect(pos, size).intersects(exclude))
				pos.setY(exclude.bottom() + 1);
		}
		qmenu->exec(pos);
	}
	g_tracking = previous;
	if(tracking.notify && hwnd && FromHandle(hwnd))
	{
		SendMenuSelect(hwnd, 0xFFFF, 0, nullptr);
		::SendMessage(hwnd, WM_EXITMENULOOP, TRUE, 0);
	}
	if(flags & TPM_RETURNCMD)
		return static_cast<BOOL>(tracking.chosen);
	return TRUE;
}

BOOL TrackPopupMenu(HMENU menu, UINT flags, int x, int y, int, HWND hwnd, const RECT *)
{
	return TrackPopupMenuEx(menu, flags, x, y, hwnd, nullptr);
}

HMENU GetMenu(HWND hwnd)
{
	WindowData *d = Data(hwnd);
	return (d && !(d->style & WS_CHILD)) ? d->menu : nullptr;
}

BOOL SetMenu(HWND hwnd, HMENU menu)
{
	Win32Widget *w = FromHandle(hwnd);
	if(w == nullptr || (w->data.style & WS_CHILD))
		return FALSE;
	AttachMenuBar(w, menu);
	return TRUE;
}

BOOL DrawMenuBar(HWND hwnd)
{
	Win32Widget *w = FromHandle(hwnd);
	if(w == nullptr || w->data.menuBar == nullptr)
		return FALSE;
	AttachMenuBar(w, w->data.menu);
	return TRUE;
}

HMENU GetSystemMenu(HWND, BOOL)
{
	// The window menu belongs to the window manager.
	return nullptr;
}

BOOL GetMenuBarInfo(HWND hwnd, LONG object, LONG item, MENUBARINFO *info)
{
	Win32Widget *w = FromHandle(hwnd);
	if(w == nullptr || info == nullptr || object != OBJID_MENU || w->data.menuBar == nullptr)
		return FALSE;
	QRect r = w->data.menuBar->geometry();
	if(item > 0)
	{
		const QList<QAction *> actions = w->data.menuBar->actions();
		if(item > actions.size())
			return FALSE;
		r = w->data.menuBar->actionGeometry(actions[item - 1]).translated(r.topLeft());
	}
	const QPoint g = w->mapToGlobal(r.topLeft());
	info->rcBar = RECT{g.x(), g.y(), g.x() + r.width(), g.y() + r.height()};
	info->hMenu = w->data.menu;
	info->hwndMenu = nullptr;
	info->fBarFocused = w->data.menuBar->hasFocus();
	info->fFocused = FALSE;
	return TRUE;
}

BOOL EndMenu()
{
	if(QWidget *popup = QApplication::activePopupWidget())
		popup->close();
	return TRUE;
}
