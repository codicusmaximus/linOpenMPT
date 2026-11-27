/*
 * ListCtrl.h
 * ----------
 * Purpose: A list view in report mode (rows of columns) as upstream's
 *          dialogs use it: items by index, with sub-item texts, a number
 *          attached to each, check boxes, groups and a selection mark.
 * Notes  : Items keep their index in insertion order also when they are
 *          shown in groups. A list with a virtual item count (owner data)
 *          asks GetItemText for the texts it shows.
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */

#pragma once

#include "mfcless/Geometry.h"
#include "mfcless/Text.h"
#include "mfcless/Window.h"

#include <QtWidgets/QTreeWidget>

#include <cstdint>
#include <vector>

class QLineEdit;

namespace mfcless
{

namespace list
{
	// Extended styles.
	inline constexpr uint32_t GridLines = 0x1, CheckBoxes = 0x4, FullRowSelect = 0x20, DoubleBuffer = 0x10000;
	// Item states.
	inline constexpr uint32_t Focused = 0x1, Selected = 0x2;
	// Column alignment.
	inline constexpr int AlignLeft = 0, AlignRight = 1, AlignCenter = 2;
	// SetColumnWidth: fit the texts, or the texts and the header.
	inline constexpr int AutoSize = -1, AutoSizeUseHeader = -2;
	// GetNextItem.
	inline constexpr uint32_t All = 0x0, NextFocused = 0x1, NextSelected = 0x2;
	// GetItemRect.
	inline constexpr int Bounds = 0, Icon = 1, Label = 2;
}

class ListCtrl : public Window<QTreeWidget>
{
	Q_OBJECT
public:
	explicit ListCtrl(QWidget *parent = nullptr);

	// The index of an item of the list, or -1 (also for group headers).
	int IndexOf(const QTreeWidgetItem *item) const;
	QTreeWidgetItem *ItemAt(int index) const;

	int InsertColumn(int column, const Text &heading, int format = list::AlignLeft, int width = -1);
	bool DeleteColumn(int column);
	int GetColumnCount() const { return columnCount(); }
	bool SetColumnWidth(int column, int width);
	int GetColumnWidth(int column) const { return columnWidth(column); }

	uint32_t GetExtendedStyle() const { return m_exStyle; }
	uint32_t SetExtendedStyle(uint32_t style);

	int GetItemCount() const;
	// Inserts at index (the end if it is past the end); returns the index.
	int InsertItem(int index, const Text &text, int image = -1);
	// Inserts into a group (see InsertGroup), with data attached.
	int InsertItem(int index, const Text &text, int image, int groupId, uintptr_t data);
	bool DeleteItem(int index);
	bool DeleteAllItems();
	bool SetItemText(int index, int subItem, const Text &text);
	virtual QString GetItemText(int index, int subItem) const;
	bool SetItemData(int index, uintptr_t data);
	uintptr_t GetItemData(int index) const;
	bool SetItemImage(int index, int image);

	// Groups, shown as headers with their items below. Returns the group's
	// index.
	bool EnableGroupView(bool enable);
	bool IsGroupViewEnabled() const { return m_groupView; }
	int InsertGroup(int groupId, const Text &header);
	void RemoveAllGroups();
	// Groups can be collapsed by the user (LVGS_COLLAPSIBLE).
	void SetGroupCollapsed(int groupId, bool collapsed);
	bool IsGroupCollapsed(int groupId) const;

	bool SetCheck(int index, bool check = true);
	bool GetCheck(int index) const;

	uint32_t GetItemState(int index, uint32_t mask) const;
	bool SetItemState(int index, uint32_t state, uint32_t mask);
	int GetSelectionMark() const;
	int SetSelectionMark(int index);
	int GetSelectedCount() const;
	// The next item after start (-1: from the first) with these states.
	int GetNextItem(int start, uint32_t flags) const;
	bool EnsureVisible(int index, bool partialOK = false);

	bool GetItemRect(int index, Rect *rect, int code) const;
	bool GetItemRect(int index, Rect &rect, int code) const { return GetItemRect(index, &rect, code); }
	bool GetViewRect(Rect *rect) const;
	bool GetViewRect(Rect &rect) const { return GetViewRect(&rect); }
	// Starts editing the label (column 0) of an item.
	void EditLabel(int index);
	// LVS_EDITLABELS: the first column's texts can be edited (F2, or a click
	// on the selected item). The edit is told as the BeginLabelEdit and
	// EndLabelEdit notifications; the list keeps its text unless the
	// handler sets it (as upstream's handlers do).
	void EnableLabelEdit(bool enable);
	// The editor of the label being edited, or nullptr.
	QLineEdit *GetEditControl() const;

Q_SIGNALS:
	void labelEditBegun(int index);
	// text is empty and accepted false when the edit was cancelled.
	void labelEditEnded(int index, const QString &text, bool accepted);

public:

	// Owner data: the list has count rows, whose texts come from
	// GetItemText (which a derived class overrides).
	void SetItemCount(int count);
	// Stops and restarts repainting during bulk updates.
	void SetRedraw(bool redraw);

	void SetImageList(const std::vector<QIcon> &icons) { m_icons = icons; }

	// Colours of a cell; 0xFFFFFFFF keeps the default. As a CMFCListCtrl's
	// virtual functions.
	virtual uint32_t OnGetCellTextColor(int row, int column);
	virtual uint32_t OnGetCellBkColor(int row, int column);

protected:
	bool edit(const QModelIndex &index, EditTrigger trigger, QEvent *event) override;
	QTreeWidgetItem *NewItem(int index, int groupId);
	void RefreshVirtual();

	uint32_t m_exStyle = 0;
	bool m_groupView = false;
	bool m_groupWatch = false;
	bool m_virtual = false;
	std::vector<QTreeWidgetItem *> m_items;
	std::vector<std::pair<int, QTreeWidgetItem *>> m_groups;
	std::vector<QIcon> m_icons;
	int m_selectionMark = -1;
	bool m_labelEdit = false;
	friend class ListDelegate;
	QWidget *m_editor = nullptr;
};

} // namespace mfcless
