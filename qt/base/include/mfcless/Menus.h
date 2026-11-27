/*
 * Menus.h
 * -------
 * Purpose: Menus as upstream builds and changes them: from the menu
 *          resources of mptrack.rc, or item by item for context menus, with
 *          the items named by command ID.
 * Notes  : A chosen item is sent to the menu's command target as a command
 *          (mfcless::SendCommand), as Windows sends WM_COMMAND to the window
 *          that tracks a menu. Before a menu opens, its items' state is asked
 *          for with the update handler (the MFC frame's UPDATE_COMMAND_UI).
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */

#pragma once

#include "mfcless/MessageMap.h"
#include "mfcless/Text.h"

#include <QtCore/QPoint>
#include <QtCore/QPointer>
#include <QtCore/QString>
#include <QtWidgets/QMenu>

#include <cstddef>
#include <functional>

class QAction;
class QMenu;
class QMenuBar;
class QObject;
class QWidget;

namespace mfcless
{

// Item flags, numbered as the menu resources store them.
namespace menu
{
	inline constexpr uint32_t String = 0x0, Enabled = 0x0, Unchecked = 0x0, ByCommand = 0x0, Grayed = 0x1, Disabled = 0x2,
		Checked = 0x8, Popup = 0x10, MenuBarBreak = 0x20, MenuBreak = 0x40, ByPosition = 0x400, Separator = 0x800, Radio = 0x200;
	// TrackPopupMenu flags; ReturnCmd returns the chosen ID instead of
	// sending it.
	inline constexpr uint32_t LeftAlign = 0x0, LeftButton = 0x0, TopAlign = 0x0, RightButton = 0x2, CenterAlign = 0x4,
		RightAlign = 0x8, VCenterAlign = 0x10, BottomAlign = 0x20, NoNotify = 0x80, ReturnCmd = 0x100;
}

// What to do with a chosen item.
enum class Track
{
	SendCommand,    // send it to the target
	ReturnCommand,  // only return its ID
};

using CommandUpdater = std::function<bool(uint32_t id, CommandState &state)>;

class Menu
{
public:
	Menu() = default;
	~Menu();
	Menu(Menu &&other) noexcept;
	Menu &operator=(Menu &&other) noexcept;
	// A copy refers to the same menu without owning it (as a copied menu
	// handle does).
	Menu(const Menu &other) : m_menu(other.m_menu), m_owned(false) {}
	Menu &operator=(const Menu &other)
	{
		if(this != &other)
		{
			Release();
			m_menu = other.m_menu;
			m_owned = false;
		}
		return *this;
	}
	bool operator==(std::nullptr_t) const { return m_menu == nullptr; }
	bool operator!=(std::nullptr_t) const { return m_menu != nullptr; }

	// An empty popup menu, owned by this object.
	static Menu CreatePopup();
	// A MENU resource. Its top-level popups are the sub menus.
	static Menu Load(uint32_t resourceId);
	// Refers to a menu owned elsewhere.
	static Menu Wrap(QMenu *menu);

	QMenu *Get() const { return m_menu; }
	explicit operator bool() const { return m_menu != nullptr; }

	// Appends an item, a separator (menu::Separator) or, with menu::Popup,
	// the sub menu sub, which the menu then owns.
	void Append(uint32_t flags, uint32_t id, const Text &text);
	void AppendSeparator() { Append(menu::Separator, 0, Text()); }
	void AppendPopup(uint32_t flags, Menu &sub, const Text &text);
	// pos is a position with menu::ByPosition, else the ID of the item to
	// insert before; past the end appends.
	void Insert(uint32_t pos, uint32_t flags, uint32_t id, const Text &text);
	void InsertPopup(uint32_t pos, uint32_t flags, Menu &sub, const Text &text);
	// Delete destroys a sub menu, Remove hands it back (still owned by
	// whoever owns its Menu, or deleted with the parent).
	bool Delete(uint32_t item, uint32_t flags);
	bool Remove(uint32_t item, uint32_t flags);
	// Changes an item's text and flags (and ID).
	bool Modify(uint32_t item, uint32_t flags, uint32_t newId, const Text &text);
	bool ModifyPopup(uint32_t item, uint32_t flags, Menu &sub, const Text &text);

	void Check(uint32_t item, bool checked, uint32_t flags = 0);
	void Enable(uint32_t item, bool enabled, uint32_t flags = 0);
	bool IsChecked(uint32_t item, uint32_t flags = 0) const;
	void SetDefault(uint32_t item, uint32_t flags = 0);

	int ItemCount() const;
	// 0 for separators, -1 for popups and positions out of range.
	int ItemID(int pos) const;
	QString ItemText(uint32_t item, uint32_t flags = 0) const;
	Menu SubMenu(int pos) const;

	// Shows the menu at a screen position, and sends the chosen item to
	// target (or returns it). Returns the chosen ID, 0 if none.
	uint32_t TrackPopup(QPoint screenPos, QObject *target, Track mode = Track::SendCommand, const CommandUpdater &update = {}) const;

	// The action of an item.
	QAction *FindAction(uint32_t item, uint32_t flags = 0) const;

	// The same, by upstream's names and with its flag arguments.
	bool CreatePopupMenu() { *this = CreatePopup(); return true; }
	bool LoadMenu(uint32_t resourceId) { *this = Load(resourceId); return m_menu != nullptr; }
	bool DestroyMenu() { Release(); return true; }
	bool AppendMenu(uint32_t flags, uint32_t id = 0, const Text &text = {}) { Append(flags, id, text); return true; }
	bool AppendMenu(uint32_t flags, Menu &sub, const Text &text) { AppendPopup(flags, sub, text); return true; }
	bool InsertMenu(uint32_t pos, uint32_t flags, uint32_t id = 0, const Text &text = {}) { Insert(pos, flags, id, text); return true; }
	bool InsertMenu(uint32_t pos, uint32_t flags, Menu &sub, const Text &text) { InsertPopup(pos, flags, sub, text); return true; }
	bool DeleteMenu(uint32_t item, uint32_t flags) { return Delete(item, flags); }
	bool RemoveMenu(uint32_t item, uint32_t flags) { return Remove(item, flags); }
	bool ModifyMenu(uint32_t item, uint32_t flags, uint32_t newId = 0, const Text &text = {}) { return Modify(item, flags, newId, text); }
	bool ModifyMenu(uint32_t item, uint32_t flags, Menu &sub, const Text &text) { return ModifyPopup(item, flags, sub, text); }
	// flags: menu::Checked or menu::Unchecked, with menu::ByPosition or not.
	uint32_t CheckMenuItem(uint32_t item, uint32_t flags) { Check(item, (flags & menu::Checked) != 0, flags & menu::ByPosition); return 0; }
	uint32_t EnableMenuItem(uint32_t item, uint32_t flags) { Enable(item, !(flags & (menu::Grayed | menu::Disabled)), flags & menu::ByPosition); return 0; }
	bool SetDefaultItem(uint32_t item, bool byPosition = false) { SetDefault(item, byPosition ? menu::ByPosition : 0); return true; }
	int GetMenuItemCount() const { return ItemCount(); }
	int GetMenuItemID(int pos) const { return ItemID(pos); }
	Menu GetSubMenu(int pos) const { return SubMenu(pos); }
	// With menu::ReturnCmd, the chosen ID; otherwise it is sent to target and
	// the result is whether one was chosen.
	uint32_t TrackPopupMenu(uint32_t flags, int x, int y, QObject *target, const CommandUpdater &update = {}) const
	{
		const uint32_t id = TrackPopup(QPoint(x, y), target, (flags & menu::ReturnCmd) ? Track::ReturnCommand : Track::SendCommand, update);
		return (flags & menu::ReturnCmd) ? id : (id != 0 ? 1 : 0);
	}

	// So that code written for menu pointers (GetSubMenu(0)->...) works on
	// the handle.
	Menu *operator->() { return this; }
	const Menu *operator->() const { return this; }

private:
	explicit Menu(QMenu *menu, bool owned) : m_menu(menu), m_owned(owned) {}
	void Release();

	QPointer<QMenu> m_menu;
	bool m_owned = false;
};

// The command ID of a menu action; 0 for separators and popups.
uint32_t ActionCommand(const QAction *action);
void ApplyCommandState(QAction *action, const CommandState &state);
// Asks update for the state of each item of the menu (not of sub menus)
// and shows it.
void UpdateMenuItems(QMenu *menu, const CommandUpdater &update);

// Fills a menu bar from a MENU resource: chosen items go to onCommand, and
// each popup asks update for its items' state before it opens.
void BuildMenuBar(QMenuBar *bar, uint32_t resourceId, std::function<void(uint32_t)> onCommand, CommandUpdater update);
// The same for the popups of a menu bar built by hand.
void ConnectMenu(QMenu *menu, std::function<void(uint32_t)> onCommand, CommandUpdater update);
// The action anywhere in a menu bar or menu with this command ID.
QAction *FindMenuAction(const QWidget *menuOrBar, uint32_t id);

} // namespace mfcless
