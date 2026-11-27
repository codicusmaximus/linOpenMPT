/*
 * TreeCtrl.h
 * ----------
 * Purpose: A tree view as upstream uses it: items by handle (the
 *          QTreeWidgetItem), each with a text, an image from an image list
 *          (one for the normal and one for the selected look), a number
 *          attached and states (bold, cut, expanded, drop target).
 * Notes  : Item handles stay valid until the item is deleted. The special
 *          handles tree::First, tree::Last and tree::Sort tell InsertItem
 *          where to put an item among its siblings.
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */

#pragma once

#include "mfcless/Geometry.h"
#include "mfcless/Text.h"
#include "mfcless/Window.h"

#include <QtGui/QIcon>
#include <QtWidgets/QTreeWidget>

#include <cstdint>
#include <functional>
#include <vector>

class QLineEdit;

namespace mfcless
{

namespace tree
{
	// Styles (SetStyle / ModifyStyle).
	inline constexpr uint32_t HasButtons = 0x1, HasLines = 0x2, LinesAtRoot = 0x4, EditLabels = 0x8,
		ShowSelAlways = 0x20, SingleExpand = 0x400;
	// Which members of ItemInfo are meant.
	inline constexpr uint32_t MaskText = 0x1, MaskImage = 0x2, MaskParam = 0x4, MaskState = 0x8,
		MaskHandle = 0x10, MaskSelectedImage = 0x20, MaskChildren = 0x40;
	// Item states.
	inline constexpr uint32_t Selected = 0x2, Cut = 0x4, DropHilited = 0x8, Bold = 0x10, Expanded = 0x20;
	// GetNextItem.
	inline constexpr uint32_t Root = 0x0, Next = 0x1, Previous = 0x2, Parent = 0x3, Child = 0x4,
		FirstVisible = 0x5, NextVisible = 0x6, PreviousVisible = 0x7, DropHilite = 0x8, Caret = 0x9, LastVisible = 0xA;
	// Expand.
	inline constexpr uint32_t Collapse = 0x1, ExpandIt = 0x2, Toggle = 0x3;
	// HitTest flags.
	inline constexpr uint32_t Nowhere = 0x1, OnItemIcon = 0x2, OnItemLabel = 0x4, OnItemIndent = 0x8,
		OnItemButton = 0x10, OnItemRight = 0x20, OnItemStateIcon = 0x40, OnItem = OnItemIcon | OnItemLabel | OnItemStateIcon,
		Above = 0x100, Below = 0x200, ToRight = 0x400, ToLeft = 0x800;

	// Where InsertItem puts an item among its siblings.
	QTreeWidgetItem *const First = reinterpret_cast<QTreeWidgetItem *>(static_cast<intptr_t>(-0x0FFFF));
	QTreeWidgetItem *const Last = reinterpret_cast<QTreeWidgetItem *>(static_cast<intptr_t>(-0x0FFFE));
	QTreeWidgetItem *const Sort = reinterpret_cast<QTreeWidgetItem *>(static_cast<intptr_t>(-0x0FFFD));
}

// An item's members, for GetItem and SetItem.
struct TreeItemInfo
{
	uint32_t mask = 0;
	QTreeWidgetItem *item = nullptr;
	uint32_t state = 0, stateMask = 0;
	QString text;
	int image = 0, selectedImage = 0;
	int children = 0;
	intptr_t data = 0;
};

class TreeCtrl : public Window<QTreeWidget>
{
	Q_OBJECT
public:
	explicit TreeCtrl(QWidget *parent = nullptr);

	uint32_t GetStyle() const { return m_style; }
	void SetStyle(uint32_t style);
	void ModifyStyle(uint32_t remove, uint32_t add) { SetStyle((m_style & ~remove) | add); }

	void SetImageList(std::vector<QIcon> icons);
	void SetIndent(int indent) { setIndentation(indent); }
	int GetIndent() const { return indentation(); }

	QTreeWidgetItem *InsertItem(const Text &text, QTreeWidgetItem *parent = nullptr, QTreeWidgetItem *after = tree::Last);
	QTreeWidgetItem *InsertItem(const Text &text, int image, int selectedImage, QTreeWidgetItem *parent = nullptr, QTreeWidgetItem *after = tree::Last);
	// The full form: the mask says which of the other values count.
	QTreeWidgetItem *InsertItem(uint32_t mask, const Text &text, int image, int selectedImage, uint32_t state, uint32_t stateMask, intptr_t data, QTreeWidgetItem *parent, QTreeWidgetItem *after);
	bool DeleteItem(QTreeWidgetItem *item);
	bool DeleteAllItems();

	QTreeWidgetItem *GetRootItem() const { return topLevelItemCount() ? topLevelItem(0) : nullptr; }
	QTreeWidgetItem *GetChildItem(QTreeWidgetItem *item) const;
	QTreeWidgetItem *GetNextSiblingItem(QTreeWidgetItem *item) const;
	QTreeWidgetItem *GetPrevSiblingItem(QTreeWidgetItem *item) const;
	QTreeWidgetItem *GetParentItem(QTreeWidgetItem *item) const { return item ? item->parent() : nullptr; }
	QTreeWidgetItem *GetNextItem(QTreeWidgetItem *item, uint32_t code) const;
	QTreeWidgetItem *GetSelectedItem() const { return currentItem(); }
	QTreeWidgetItem *GetFirstVisibleItem() const;
	QTreeWidgetItem *GetNextVisibleItem(QTreeWidgetItem *item) const { return item ? itemBelow(item) : nullptr; }
	QTreeWidgetItem *GetPrevVisibleItem(QTreeWidgetItem *item) const { return item ? itemAbove(item) : nullptr; }
	QTreeWidgetItem *GetDropHilightItem() const { return m_dropTarget; }
	int GetCount() const;
	bool ItemHasChildren(QTreeWidgetItem *item) const;

	bool SelectItem(QTreeWidgetItem *item);
	bool SelectDropTarget(QTreeWidgetItem *item);
	bool Select(QTreeWidgetItem *item, uint32_t code);
	bool Expand(QTreeWidgetItem *item, uint32_t code);
	bool EnsureVisible(QTreeWidgetItem *item);

	bool SetItemText(QTreeWidgetItem *item, const Text &text);
	QString GetItemText(QTreeWidgetItem *item) const { return item ? item->text(0) : QString(); }
	bool SetItemImage(QTreeWidgetItem *item, int image, int selectedImage);
	bool GetItemImage(QTreeWidgetItem *item, int &image, int &selectedImage) const;
	bool SetItemData(QTreeWidgetItem *item, intptr_t data);
	intptr_t GetItemData(QTreeWidgetItem *item) const;
	bool SetItemState(QTreeWidgetItem *item, uint32_t state, uint32_t stateMask);
	uint32_t GetItemState(QTreeWidgetItem *item, uint32_t stateMask) const;
	bool GetItem(TreeItemInfo &info) const;
	bool SetItem(const TreeItemInfo &info);
	bool SetItem(QTreeWidgetItem *item, uint32_t mask, const Text &text, int image, int selectedImage, uint32_t state, uint32_t stateMask, intptr_t data);

	// The item at a point in client coordinates, and where on it the point is.
	QTreeWidgetItem *HitTest(Point pt, uint32_t *flags = nullptr) const;
	bool GetItemRect(QTreeWidgetItem *item, Rect &rect, bool textOnly) const;
	bool GetItemRect(QTreeWidgetItem *item, Rect *rect, bool textOnly) const { return rect && GetItemRect(item, *rect, textOnly); }

	QLineEdit *EditLabel(QTreeWidgetItem *item);
	QLineEdit *GetEditControl() const;
	// Ends a label edit, keeping the text unless cancel is set.
	bool EndEditLabelNow(bool cancel);

	bool SortChildren(QTreeWidgetItem *item);
	// compare(data1, data2) < 0 puts the item with data1 first.
	bool SortChildren(QTreeWidgetItem *item, const std::function<int(intptr_t, intptr_t)> &compare);

Q_SIGNALS:
	void selChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);
	void itemExpandedChanged(QTreeWidgetItem *item, bool expanded);
	void beginDrag(QTreeWidgetItem *item, bool rightButton);
	void labelEditBegun(QTreeWidgetItem *item, bool *allow);
	// The editor exists and holds the item's text; a handler may change its text or length.
	void labelEditorReady(QTreeWidgetItem *item, QLineEdit *editor);
	// *keep = true makes the new text the item's text.
	void labelEditEnded(QTreeWidgetItem *item, const QString &text, bool accepted, bool *keep);
	void itemDeleted(QTreeWidgetItem *item, intptr_t data);

protected:
	void mousePressEvent(QMouseEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;
	bool edit(const QModelIndex &index, EditTrigger trigger, QEvent *event) override;

private:
	friend class TreeDelegate;
	void ApplyIcon(QTreeWidgetItem *item, bool selected) const;
	void ApplyState(QTreeWidgetItem *item, uint32_t state) const;
	void NotifyDeleted(QTreeWidgetItem *item);
	void SetExpandedChecked(QTreeWidgetItem *item, bool expand);
	QTreeWidgetItem *Place(QTreeWidgetItem *item, QTreeWidgetItem *parent, QTreeWidgetItem *after);

	std::vector<QIcon> m_icons;
	uint32_t m_style = 0;
	QTreeWidgetItem *m_dropTarget = nullptr;
	QWidget *m_editor = nullptr;
	QTreeWidgetItem *m_editItem = nullptr;
	bool m_editCommitted = false;
	bool m_singleExpanding = false;
	// Drag detection.
	QPoint m_pressPos;
	Qt::MouseButton m_pressButton = Qt::NoButton;
	QTreeWidgetItem *m_pressItem = nullptr;
};

} // namespace mfcless
