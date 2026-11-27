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
	inline constexpr uint32_t String = 0x0, Grayed = 0x1, Disabled = 0x2, Checked = 0x8, Popup = 0x10,
		MenuBarBreak = 0x20, MenuBreak = 0x40, ByPosition = 0x400, Separator = 0x800, Radio = 0x200;
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
	Menu(const Menu &) = delete;
	Menu &operator=(const Menu &) = delete;

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
