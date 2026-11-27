/*
 * MessageMap.h
 * ------------
 * Purpose: Routes commands (menu items, toolbar buttons, dialog buttons) and
 *          control notifications to handler functions by resource ID.
 * Notes  : Upstream's windows list their handlers in a table per class. The
 *          twins keep that table, in upstream's order, as a function that
 *          registers each handler here (RegisterMessageMap), so a change to
 *          upstream's table maps to the same line of the twin.
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */

#pragma once

#include <QtCore/QPoint>
#include <QtCore/QString>
#include "mfcless/Text.h"
#include <QtCore/Qt>

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

class QEvent;
class QWidget;
class QTreeWidgetItem;

namespace mfcless
{

// Control notifications.
enum class Notify : uint16_t
{
	Clicked,        // a button was pressed
	DoubleClicked,  // list, tree or button
	SelChange,      // the user picked another item (combo box, list, tree, tab)
	SelChanging,    // the selection is about to change; result 1 keeps it
	EditChange,     // the text of an edit field (or editable combo box) changed
	EditUpdate,     // same, before it is shown
	SetFocus,
	KillFocus,
	DropDown,       // a combo box's list is about to open
	CloseUp,
	DeltaPos,       // an up-down control is stepped; delta in NotifyInfo
	ItemChanged,    // an item of a list view changed (selection, check)
	Click,          // a click on a list or tree
	RightClick,
	CheckChange,    // a check box in a list changed
	BeginLabelEdit,
	EndLabelEdit,
	BeginDrag,
	KeyDown,
	ItemExpanded,   // a tree item was expanded or collapsed (delta: 1 / 0)
	DeleteItem,     // a tree item is being deleted
};

// The codes of scroll notifications, as Windows numbers them.
enum class ScrollCode : int
{
	LineUp = 0, LineDown = 1, PageUp = 2, PageDown = 3,
	ThumbPosition = 4, ThumbTrack = 5, Top = 6, Bottom = 7, EndScroll = 8,
};

// What a notification handler needs to know beyond the control's ID.
struct NotifyInfo
{
	uint32_t id = 0;
	Notify code = Notify::Clicked;
	QWidget *control = nullptr;
	int delta = 0;    // DeltaPos: +1 / -1
	int pos = 0;      // DeltaPos: position before the step
	int item = -1;    // list and tree notifications: the item (row)
	int subItem = 0;  // column
	int key = 0;      // KeyDown: Windows virtual-key code
	QPoint point;     // RightClick: the mouse position, in screen coordinates
	QTreeWidgetItem *treeItem = nullptr;     // tree notifications: the item
	QTreeWidgetItem *oldTreeItem = nullptr;  // SelChange of a tree: the item before
	bool rightButton = false;  // BeginDrag
	intptr_t data = 0;         // DeleteItem: the item's data
	QString text;     // EndLabelEdit: the new text (empty if cancelled)
	bool textValid = false;
	intptr_t result = 0;  // set by the handler
};

// The state a command's menu item or toolbar button should show.
struct CommandState
{
	uint32_t id = 0;
	bool enabled = true;
	std::optional<bool> checked;
	bool radio = false;
	std::optional<QString> text;

	void Enable(bool on = true) { enabled = on; }
	void SetCheck(int check) { checked = (check != 0); }
	void SetRadio(bool on = true) { radio = true; checked = on; }
	void SetText(const Text &t) { text = t.value; }
};

class MessageMap
{
public:
	virtual ~MessageMap() = default;

	// True if a handler took the command.
	bool DispatchCommand(uint32_t id);
	// True if a handler set the state.
	bool DispatchCommandUpdate(uint32_t id, CommandState &state);
	bool DispatchNotify(NotifyInfo &info);
	bool DispatchScroll(Qt::Orientation orientation, ScrollCode code, int pos, QWidget *control);
	bool HasCommand(uint32_t id) const;
	// Where a command and its state requests go (upstream: OnCmdMsg). By
	// default to this map's handlers; frames pass them on to their views and
	// documents first.
	virtual bool RouteCommand(uint32_t id) { return DispatchCommand(id); }
	virtual bool RouteCommandUpdate(uint32_t id, CommandState &state) { return DispatchCommandUpdate(id, state); }
	// Window messages of the program's own (WM_MOD_*, mfcless::ModMessage).
	bool DispatchMessage(uint32_t message, uintptr_t wParam, intptr_t lParam, intptr_t &result);
	// For a widget's event(): hands a ModMessage (a command or one of the
	// program's messages) to the handlers. True if e was one.
	bool HandleModMessage(QEvent *e);

	// The notification being handled (one with id 0 outside of a handler).
	const NotifyInfo &CurrentNotify() const { return *m_current; }
	NotifyInfo &CurrentNotify() { return *m_current; }

protected:
	// Called once the window's controls exist. Overrides call their base
	// class's first, as upstream's tables chain to the base class's table.
	virtual void RegisterMessageMap() {}

	template <typename T>
	void AddCommand(uint32_t id, void (T::*fn)())
	{
		m_commands.push_back({id, id, [this, fn](uint32_t) { (static_cast<T *>(this)->*fn)(); return true; }});
	}
	// A handler that reports whether it handled the command.
	template <typename T>
	void AddCommand(uint32_t id, bool (T::*fn)(uint32_t))
	{
		m_commands.push_back({id, id, [this, fn](uint32_t cmd) { return (static_cast<T *>(this)->*fn)(cmd); }});
	}
	template <typename T>
	void AddCommandRange(uint32_t first, uint32_t last, void (T::*fn)(uint32_t))
	{
		m_commands.push_back({first, last, [this, fn](uint32_t cmd) { (static_cast<T *>(this)->*fn)(cmd); return true; }});
	}
	template <typename T>
	void AddCommandRange(uint32_t first, uint32_t last, bool (T::*fn)(uint32_t))
	{
		m_commands.push_back({first, last, [this, fn](uint32_t cmd) { return (static_cast<T *>(this)->*fn)(cmd); }});
	}
	template <typename T>
	void AddCommandUpdate(uint32_t id, void (T::*fn)(CommandState &))
	{
		m_updates.push_back({id, id, [this, fn](CommandState &s) { (static_cast<T *>(this)->*fn)(s); }});
	}
	template <typename T>
	void AddCommandUpdateRange(uint32_t first, uint32_t last, void (T::*fn)(CommandState &))
	{
		m_updates.push_back({first, last, [this, fn](CommandState &s) { (static_cast<T *>(this)->*fn)(s); }});
	}
	template <typename T>
	void AddNotify(uint32_t id, Notify code, void (T::*fn)())
	{
		m_notifies.push_back({id, id, code, [this, fn](NotifyInfo &) { (static_cast<T *>(this)->*fn)(); return true; }});
	}
	// A handler that reports whether it handled the notification.
	template <typename T>
	void AddNotify(uint32_t id, Notify code, bool (T::*fn)())
	{
		m_notifies.push_back({id, id, code, [this, fn](NotifyInfo &) { return (static_cast<T *>(this)->*fn)(); }});
	}
	template <typename T>
	void AddNotifyRange(uint32_t first, uint32_t last, Notify code, void (T::*fn)(uint32_t))
	{
		m_notifies.push_back({first, last, code, [this, fn](NotifyInfo &info) { (static_cast<T *>(this)->*fn)(info.id); return true; }});
	}
	template <typename T>
	void AddNotifyRange(uint32_t first, uint32_t last, Notify code, bool (T::*fn)(uint32_t))
	{
		m_notifies.push_back({first, last, code, [this, fn](NotifyInfo &info) { return (static_cast<T *>(this)->*fn)(info.id); }});
	}
	template <typename T>
	void AddMessage(uint32_t message, intptr_t (T::*fn)(uintptr_t, intptr_t))
	{
		m_messages.push_back({message, [this, fn](uintptr_t w, intptr_t l) { return (static_cast<T *>(this)->*fn)(w, l); }});
	}
	template <typename T>
	void AddScroll(Qt::Orientation orientation, void (T::*fn)(ScrollCode code, int pos, QWidget *control))
	{
		m_scrolls.push_back({orientation, [this, fn](ScrollCode c, int p, QWidget *w) { (static_cast<T *>(this)->*fn)(c, p, w); }});
	}

private:
	struct Command
	{
		uint32_t first, last;
		std::function<bool(uint32_t)> fn;
	};
	struct Update
	{
		uint32_t first, last;
		std::function<void(CommandState &)> fn;
	};
	struct NotifyEntry
	{
		uint32_t first, last;
		Notify code;
		std::function<bool(NotifyInfo &)> fn;
	};
	struct Scroll
	{
		Qt::Orientation orientation;
		std::function<void(ScrollCode, int, QWidget *)> fn;
	};
	std::vector<Command> m_commands;
	std::vector<Update> m_updates;
	std::vector<NotifyEntry> m_notifies;
	std::vector<Scroll> m_scrolls;
	struct MessageEntry
	{
		uint32_t message;
		std::function<intptr_t(uintptr_t, intptr_t)> fn;
	};
	std::vector<MessageEntry> m_messages;
	NotifyInfo m_none;
	NotifyInfo *m_current = &m_none;
};

// Connects a control's Qt signals to the notifications of its kind, sent
// to target with the control's resource ID.
void ConnectNotifications(QWidget *control, uint32_t id, MessageMap *target);

} // namespace mfcless
