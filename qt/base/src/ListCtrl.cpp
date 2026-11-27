/*
 * ListCtrl.cpp
 * ------------
 * Purpose: A list view in report mode as upstream's dialogs use it.
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */

#include "mfcless/ListCtrl.h"
#include "mfcless/Graphics.h"

#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QStyledItemDelegate>

#include <algorithm>

namespace mfcless
{

constexpr int DataRole = Qt::UserRole + 1;
constexpr uint32_t DefaultColor = 0xFFFFFFFF;

// Draws cells in the colours the list asks for, and the texts of a list
// with owner data.
class ListDelegate : public QStyledItemDelegate
{
public:
	explicit ListDelegate(ListCtrl *list) : QStyledItemDelegate(list), m_list(list) {}

	// Label editing: the list's handler decides what becomes of the text.
	QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override
	{
		QWidget *editor = QStyledItemDelegate::createEditor(parent, option, index);
		m_list->m_editor = editor;
		const int row = m_list->IndexOf(m_list->itemFromIndex(index));
		m_editRow = row;
		m_committed = false;
		QMetaObject::invokeMethod(m_list, [list = m_list, row] { Q_EMIT list->labelEditBegun(row); }, Qt::QueuedConnection);
		connect(editor, &QObject::destroyed, m_list, [list = m_list, this, row] {
			list->m_editor = nullptr;
			if(!m_committed)
				Q_EMIT list->labelEditEnded(row, QString(), false);
		});
		return editor;
	}
	void setModelData(QWidget *editor, QAbstractItemModel *, const QModelIndex &) const override
	{
		m_committed = true;
		const QString text = editor->property("text").toString();
		Q_EMIT m_list->labelEditEnded(m_editRow, text, true);
	}

protected:
	void initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const override
	{
		QStyledItemDelegate::initStyleOption(option, index);
		const int row = m_list->IndexOf(m_list->itemFromIndex(index));
		if(row < 0)
			return;
		const uint32_t text = m_list->OnGetCellTextColor(row, index.column());
		const uint32_t bk = m_list->OnGetCellBkColor(row, index.column());
		if(text != DefaultColor)
			option->palette.setColor(QPalette::Text, ToQColor(text));
		if(bk != DefaultColor)
			option->backgroundBrush = QBrush(ToQColor(bk));
	}

private:
	ListCtrl *m_list;
	mutable int m_editRow = -1;
	mutable bool m_committed = false;
};


ListCtrl::ListCtrl(QWidget *parent)
	: Window(parent)
{
	setRootIsDecorated(false);
	setUniformRowHeights(true);
	setAllColumnsShowFocus(true);
	// Columns keep the widths given them, as a report view's do.
	header()->setStretchLastSection(false);
	setColumnCount(0);
	setItemDelegate(new ListDelegate(this));
	connect(this, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem *current) {
		if(const int i = IndexOf(current); i >= 0)
			m_selectionMark = i;
	});
}


int ListCtrl::IndexOf(const QTreeWidgetItem *item) const
{
	if(!item)
		return -1;
	auto it = std::find(m_items.begin(), m_items.end(), item);
	return it == m_items.end() ? -1 : static_cast<int>(it - m_items.begin());
}


QTreeWidgetItem *ListCtrl::ItemAt(int index) const
{
	return (index >= 0 && index < static_cast<int>(m_items.size())) ? m_items[index] : nullptr;
}


int ListCtrl::InsertColumn(int column, const Text &heading, int format, int width)
{
	const int count = columnCount();
	column = std::clamp(column, 0, count);
	QTreeWidgetItem *header = headerItem();
	// Shift the headings after the new column.
	setColumnCount(count + 1);
	for(int c = count; c > column; c--)
	{
		header->setText(c, header->text(c - 1));
		header->setTextAlignment(c, header->textAlignment(c - 1));
		setColumnWidth(c, columnWidth(c - 1));
	}
	header->setText(column, heading.value);
	const Qt::Alignment align = (format & 0x3) == list::AlignRight ? Qt::AlignRight : (format & 0x3) == list::AlignCenter ? Qt::AlignHCenter : Qt::AlignLeft;
	header->setTextAlignment(column, align | Qt::AlignVCenter);
	if(width >= 0)
		setColumnWidth(column, width);
	else
		resizeColumnToContents(column);
	return column;
}


bool ListCtrl::DeleteColumn(int column)
{
	const int count = columnCount();
	if(column < 0 || column >= count)
		return false;
	QTreeWidgetItem *header = headerItem();
	for(int c = column; c < count - 1; c++)
	{
		header->setText(c, header->text(c + 1));
		for(QTreeWidgetItem *item : m_items)
			item->setText(c, item->text(c + 1));
	}
	setColumnCount(count - 1);
	return true;
}


bool ListCtrl::SetColumnWidth(int column, int width)
{
	if(column < 0 || column >= columnCount())
		return false;
	if(width == list::AutoSize)
	{
		resizeColumnToContents(column);
	} else if(width == list::AutoSizeUseHeader)
	{
		if(column == columnCount() - 1)
		{
			// The last column fills the rest of the list, as on Windows.
			int used = 0;
			for(int c = 0; c < column; c++)
				used += columnWidth(c);
			resizeColumnToContents(column);
			setColumnWidth(column, std::max(columnWidth(column), viewport()->width() - used));
		} else
		{
			resizeColumnToContents(column);
			setColumnWidth(column, std::max(columnWidth(column), header()->sectionSizeHint(column)));
		}
	} else
	{
		setColumnWidth(column, width);
	}
	return true;
}


uint32_t ListCtrl::SetExtendedStyle(uint32_t style)
{
	const uint32_t old = m_exStyle;
	m_exStyle = style;
	if((old ^ style) & list::CheckBoxes)
	{
		for(QTreeWidgetItem *item : m_items)
		{
			if(style & list::CheckBoxes)
			{
				item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
				if(!item->data(0, Qt::CheckStateRole).isValid())
					item->setCheckState(0, Qt::Unchecked);
			} else
			{
				item->setData(0, Qt::CheckStateRole, QVariant());
			}
		}
	}
	setSelectionBehavior(QAbstractItemView::SelectRows);
	setStyleSheet((style & list::GridLines) ? QStringLiteral("QTreeView::item { border-right: 1px solid palette(midlight); border-bottom: 1px solid palette(midlight); }") : QString());
	return old;
}


int ListCtrl::GetItemCount() const
{
	return static_cast<int>(m_items.size());
}


QTreeWidgetItem *ListCtrl::NewItem(int index, int groupId)
{
	auto *item = new QTreeWidgetItem;
	if(m_exStyle & list::CheckBoxes)
	{
		item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
		item->setCheckState(0, Qt::Unchecked);
	}
	item->setData(0, DataRole, QVariant::fromValue<qulonglong>(0));
	if(m_labelEdit)
		item->setFlags(item->flags() | Qt::ItemIsEditable);
	QTreeWidgetItem *group = nullptr;
	if(m_groupView)
	{
		for(const auto &[id, header] : m_groups)
		{
			if(id == groupId)
				group = header;
		}
	}
	if(group)
	{
		group->addChild(item);
	} else
	{
		// Shown at its index among the items outside groups.
		int pos = 0;
		for(int i = 0; i < index && i < static_cast<int>(m_items.size()); i++)
		{
			if(!m_items[i]->parent())
				pos++;
		}
		if(m_groups.empty())
			insertTopLevelItem(pos, item);
		else
			addTopLevelItem(item);
	}
	m_items.insert(m_items.begin() + index, item);
	return item;
}


int ListCtrl::InsertItem(int index, const Text &text, int image)
{
	return InsertItem(index, text, image, -1, 0);
}


int ListCtrl::InsertItem(int index, const Text &text, int image, int groupId, uintptr_t data)
{
	index = std::clamp(index, 0, GetItemCount());
	QTreeWidgetItem *item = NewItem(index, groupId);
	item->setText(0, text.value);
	item->setData(0, DataRole, QVariant::fromValue<qulonglong>(data));
	if(image >= 0 && image < static_cast<int>(m_icons.size()))
		item->setIcon(0, m_icons[image]);
	if(m_selectionMark >= index)
		m_selectionMark++;
	return index;
}


bool ListCtrl::DeleteItem(int index)
{
	QTreeWidgetItem *item = ItemAt(index);
	if(!item)
		return false;
	m_items.erase(m_items.begin() + index);
	delete item;
	if(m_selectionMark == index)
		m_selectionMark = -1;
	else if(m_selectionMark > index)
		m_selectionMark--;
	return true;
}


bool ListCtrl::DeleteAllItems()
{
	m_items.clear();
	m_virtual = false;
	clear();
	m_groups.clear();
	m_selectionMark = -1;
	return true;
}


bool ListCtrl::SetItemText(int index, int subItem, const Text &text)
{
	QTreeWidgetItem *item = ItemAt(index);
	if(!item || subItem < 0)
		return false;
	item->setText(subItem, text.value);
	return true;
}


QString ListCtrl::GetItemText(int index, int subItem) const
{
	const QTreeWidgetItem *item = ItemAt(index);
	return item ? item->text(subItem) : QString();
}


bool ListCtrl::SetItemData(int index, uintptr_t data)
{
	QTreeWidgetItem *item = ItemAt(index);
	if(!item)
		return false;
	item->setData(0, DataRole, QVariant::fromValue<qulonglong>(data));
	return true;
}


uintptr_t ListCtrl::GetItemData(int index) const
{
	const QTreeWidgetItem *item = ItemAt(index);
	return item ? static_cast<uintptr_t>(item->data(0, DataRole).toULongLong()) : 0;
}


bool ListCtrl::SetItemImage(int index, int image)
{
	QTreeWidgetItem *item = ItemAt(index);
	if(!item)
		return false;
	item->setIcon(0, (image >= 0 && image < static_cast<int>(m_icons.size())) ? m_icons[image] : QIcon());
	return true;
}


bool ListCtrl::EnableGroupView(bool enable)
{
	m_groupView = enable;
	// The groups' headers have the arrows that collapse them.
	setRootIsDecorated(enable);
	setIndentation(enable ? 8 : 0);
	if(enable && !m_groupWatch)
	{
		// As on Windows, a group without items has no header either.
		m_groupWatch = true;
		auto update = [this] {
			for(auto &[id, header] : m_groups)
				header->setHidden(header->childCount() == 0);
		};
		connect(model(), &QAbstractItemModel::rowsInserted, this, update);
		connect(model(), &QAbstractItemModel::rowsRemoved, this, update);
	}
	return true;
}


int ListCtrl::InsertGroup(int groupId, const Text &header)
{
	auto *item = new QTreeWidgetItem;
	item->setText(0, header.value);
	item->setFirstColumnSpanned(false);
	QFont f = font();
	f.setBold(true);
	item->setFont(0, f);
	item->setFlags(Qt::ItemIsEnabled);
	addTopLevelItem(item);
	item->setFirstColumnSpanned(true);
	item->setExpanded(true);
	item->setHidden(true);  // until it has items
	m_groups.emplace_back(groupId, item);
	return static_cast<int>(m_groups.size()) - 1;
}


void ListCtrl::RemoveAllGroups()
{
	for(auto &[id, header] : m_groups)
	{
		// Items of the group move to the top level.
		while(header->childCount())
			addTopLevelItem(header->takeChild(0));
		delete header;
	}
	m_groups.clear();
}


void ListCtrl::SetGroupCollapsed(int groupId, bool collapsed)
{
	for(auto &[id, header] : m_groups)
	{
		if(id == groupId)
			header->setExpanded(!collapsed);
	}
}


bool ListCtrl::IsGroupCollapsed(int groupId) const
{
	for(const auto &[id, header] : m_groups)
	{
		if(id == groupId)
			return !header->isExpanded();
	}
	return false;
}


bool ListCtrl::SetCheck(int index, bool check)
{
	QTreeWidgetItem *item = ItemAt(index);
	if(!item)
		return false;
	item->setCheckState(0, check ? Qt::Checked : Qt::Unchecked);
	return true;
}


bool ListCtrl::GetCheck(int index) const
{
	const QTreeWidgetItem *item = ItemAt(index);
	return item && item->checkState(0) == Qt::Checked;
}


uint32_t ListCtrl::GetItemState(int index, uint32_t mask) const
{
	const QTreeWidgetItem *item = ItemAt(index);
	if(!item)
		return 0;
	uint32_t state = 0;
	if(item->isSelected())
		state |= list::Selected;
	if(currentItem() == item)
		state |= list::Focused;
	return state & mask;
}


bool ListCtrl::SetItemState(int index, uint32_t state, uint32_t mask)
{
	if(index == -1)
	{
		// All items.
		for(int i = 0; i < GetItemCount(); i++)
			SetItemState(i, state, mask);
		return true;
	}
	QTreeWidgetItem *item = ItemAt(index);
	if(!item)
		return false;
	if(mask & list::Focused)
	{
		if(state & list::Focused)
			setCurrentItem(item, 0, QItemSelectionModel::NoUpdate);
	}
	if(mask & list::Selected)
	{
		if((state & list::Selected) && selectionMode() == QAbstractItemView::SingleSelection)
			setCurrentItem(item);
		else
			item->setSelected(state & list::Selected);
	}
	return true;
}


int ListCtrl::GetSelectionMark() const
{
	return m_selectionMark;
}


int ListCtrl::SetSelectionMark(int index)
{
	const int old = m_selectionMark;
	m_selectionMark = index;
	return old;
}


int ListCtrl::GetSelectedCount() const
{
	int count = 0;
	for(const QTreeWidgetItem *item : m_items)
	{
		if(item->isSelected())
			count++;
	}
	return count;
}


int ListCtrl::GetNextItem(int start, uint32_t flags) const
{
	for(int i = start + 1; i < GetItemCount(); i++)
	{
		const QTreeWidgetItem *item = m_items[i];
		if((flags & list::NextSelected) && !item->isSelected())
			continue;
		if((flags & list::NextFocused) && currentItem() != item)
			continue;
		return i;
	}
	return -1;
}


bool ListCtrl::EnsureVisible(int index, bool /*partialOK*/)
{
	QTreeWidgetItem *item = ItemAt(index);
	if(!item)
		return false;
	scrollToItem(item);
	return true;
}


bool ListCtrl::GetItemRect(int index, Rect *rect, int code) const
{
	const QTreeWidgetItem *item = ItemAt(index);
	if(!item || !rect)
		return false;
	QRect r = visualItemRect(item);
	if(code == list::Label || code == list::Icon)
		r.setWidth(columnWidth(0));
	*rect = Rect(r);
	return true;
}


bool ListCtrl::GetViewRect(Rect *rect) const
{
	if(!rect)
		return false;
	int height = 0;
	for(const QTreeWidgetItem *item : m_items)
		height += visualItemRect(item).height();
	int width = 0;
	for(int c = 0; c < columnCount(); c++)
		width += columnWidth(c);
	*rect = Rect(0, 0, width, height);
	return true;
}


void ListCtrl::EditLabel(int index)
{
	if(QTreeWidgetItem *item = ItemAt(index))
	{
		item->setFlags(item->flags() | Qt::ItemIsEditable);
		editItem(item, 0);
	}
}


void ListCtrl::EnableLabelEdit(bool enable)
{
	m_labelEdit = enable;
	setEditTriggers(enable ? (QAbstractItemView::EditKeyPressed | QAbstractItemView::SelectedClicked) : QAbstractItemView::NoEditTriggers);
	for(QTreeWidgetItem *item : m_items)
	{
		if(enable)
			item->setFlags(item->flags() | Qt::ItemIsEditable);
		else
			item->setFlags(item->flags() & ~Qt::ItemIsEditable);
	}
}


bool ListCtrl::edit(const QModelIndex &index, EditTrigger trigger, QEvent *event)
{
	// Only the first column is the label.
	if(index.column() != 0 && trigger != QAbstractItemView::AllEditTriggers)
		return false;
	return Window::edit(index, trigger, event);
}


QLineEdit *ListCtrl::GetEditControl() const
{
	return qobject_cast<QLineEdit *>(m_editor);
}


void ListCtrl::SetItemCount(int count)
{
	// Owner data: rows whose texts the list asks for.
	m_virtual = true;
	setUpdatesEnabled(false);
	while(GetItemCount() > count)
		DeleteItem(GetItemCount() - 1);
	while(GetItemCount() < count)
		NewItem(GetItemCount(), -1);
	m_virtual = true;
	RefreshVirtual();
	setUpdatesEnabled(true);
}


void ListCtrl::RefreshVirtual()
{
	if(!m_virtual)
		return;
	for(int i = 0; i < GetItemCount(); i++)
	{
		for(int c = 0; c < columnCount(); c++)
			m_items[i]->setText(c, GetItemText(i, c));
	}
}


void ListCtrl::SetRedraw(bool redraw)
{
	setUpdatesEnabled(redraw);
	if(redraw)
		viewport()->update();
}


uint32_t ListCtrl::OnGetCellTextColor(int, int)
{
	return DefaultColor;
}


uint32_t ListCtrl::OnGetCellBkColor(int, int)
{
	return DefaultColor;
}

} // namespace mfcless
