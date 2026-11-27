/*
 * TreeCtrl.cpp
 * ------------
 * Purpose: A tree view with item handles, images and states, as upstream
 *          uses it.
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */

#include "mfcless/TreeCtrl.h"

#include <QtGui/QMouseEvent>
#include <QtWidgets/QApplication>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QStyledItemDelegate>
#include <QtWidgets/QTreeWidgetItemIterator>

#include <algorithm>
#include <set>

namespace mfcless
{

namespace
{
constexpr int DataRole = Qt::UserRole + 1;
constexpr int ImageRole = Qt::UserRole + 2;
constexpr int SelImageRole = Qt::UserRole + 3;
constexpr int StateRole = Qt::UserRole + 4;
}

// Label editing: the tree's handler decides what becomes of the text.
class TreeDelegate : public QStyledItemDelegate
{
public:
	explicit TreeDelegate(TreeCtrl *tree) : QStyledItemDelegate(tree), m_tree(tree) {}

	QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override
	{
		QWidget *editor = QStyledItemDelegate::createEditor(parent, option, index);
		QTreeWidgetItem *item = m_tree->itemFromIndex(index);
		m_tree->m_editor = editor;
		m_tree->m_editItem = item;
		m_tree->m_editCommitted = false;
		connect(editor, &QObject::destroyed, m_tree, [tree = m_tree, editor] {
			if(tree->m_editor != editor)
				return;
			QTreeWidgetItem *edited = tree->m_editItem;
			tree->m_editor = nullptr;
			tree->m_editItem = nullptr;
			if(!tree->m_editCommitted && edited)
			{
				bool keep = false;
				Q_EMIT tree->labelEditEnded(edited, QString(), false, &keep);
			}
		});
		return editor;
	}
	void setModelData(QWidget *editor, QAbstractItemModel *, const QModelIndex &index) const override
	{
		m_tree->m_editCommitted = true;
		QTreeWidgetItem *item = m_tree->itemFromIndex(index);
		const QString text = editor->property("text").toString();
		bool keep = false;
		Q_EMIT m_tree->labelEditEnded(item, text, true, &keep);
		if(keep && item)
			item->setText(0, text);
	}
	void setEditorData(QWidget *editor, const QModelIndex &index) const override
	{
		QStyledItemDelegate::setEditorData(editor, index);
		Q_EMIT m_tree->labelEditorReady(m_tree->itemFromIndex(index), qobject_cast<QLineEdit *>(editor));
	}

private:
	TreeCtrl *m_tree;
};


TreeCtrl::TreeCtrl(QWidget *parent)
	: Window(parent)
{
	setHeaderHidden(true);
	setColumnCount(1);
	setSelectionMode(QAbstractItemView::SingleSelection);
	setEditTriggers(QAbstractItemView::NoEditTriggers);
	setRootIsDecorated(false);
	setUniformRowHeights(true);
	header()->setStretchLastSection(true);
	setItemDelegate(new TreeDelegate(this));
	connect(this, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem *current, QTreeWidgetItem *previous) {
		if(previous)
			ApplyIcon(previous, false);
		if(current)
			ApplyIcon(current, true);
		Q_EMIT selChanged(current, previous);
	});
	connect(this, &QTreeWidget::itemExpanded, this, [this](QTreeWidgetItem *item) {
		if((m_style & tree::SingleExpand) && !m_singleExpanding)
		{
			// Only one branch of each parent open at a time.
			m_singleExpanding = true;
			QTreeWidgetItem *parent = item->parent();
			const int count = parent ? parent->childCount() : topLevelItemCount();
			for(int i = 0; i < count; i++)
			{
				QTreeWidgetItem *sibling = parent ? parent->child(i) : topLevelItem(i);
				if(sibling != item && sibling->isExpanded())
					sibling->setExpanded(false);
			}
			m_singleExpanding = false;
		}
		Q_EMIT itemExpandedChanged(item, true);
	});
	connect(this, &QTreeWidget::itemCollapsed, this, [this](QTreeWidgetItem *item) { Q_EMIT itemExpandedChanged(item, false); });
}


void TreeCtrl::SetStyle(uint32_t style)
{
	m_style = style;
	setRootIsDecorated((style & tree::LinesAtRoot) != 0);
	setEditTriggers((style & tree::EditLabels) ? QAbstractItemView::EditKeyPressed : QAbstractItemView::NoEditTriggers);
}


void TreeCtrl::SetImageList(std::vector<QIcon> icons)
{
	m_icons = std::move(icons);
	if(!m_icons.empty())
	{
		const QList<QSize> sizes = m_icons.front().availableSizes();
		if(!sizes.isEmpty())
			setIconSize(sizes.front());
	}
	for(QTreeWidgetItemIterator it(this); *it; ++it)
		ApplyIcon(*it, *it == currentItem());
}


void TreeCtrl::ApplyIcon(QTreeWidgetItem *item, bool selected) const
{
	const QVariant image = item->data(0, selected ? SelImageRole : ImageRole);
	const int i = image.isValid() ? image.toInt() : -1;
	if(i >= 0 && i < static_cast<int>(m_icons.size()))
		item->setIcon(0, m_icons[i]);
	else
		item->setIcon(0, QIcon());
}


void TreeCtrl::ApplyState(QTreeWidgetItem *item, uint32_t state) const
{
	const uint32_t old = item->data(0, StateRole).toUInt();
	item->setData(0, StateRole, state & (tree::Bold | tree::Cut));
	if((old ^ state) & tree::Bold)
	{
		QFont f = font();
		f.setBold((state & tree::Bold) != 0);
		item->setFont(0, f);
	}
	if((old ^ state) & tree::Cut)
	{
		if(state & tree::Cut)
			item->setForeground(0, palette().brush(QPalette::Disabled, QPalette::Text));
		else
			item->setData(0, Qt::ForegroundRole, QVariant());
	}
}


QTreeWidgetItem *TreeCtrl::Place(QTreeWidgetItem *item, QTreeWidgetItem *parent, QTreeWidgetItem *after)
{
	const int count = parent ? parent->childCount() : topLevelItemCount();
	auto childAt = [&](int i) { return parent ? parent->child(i) : topLevelItem(i); };
	int pos = count;
	if(after == tree::First)
	{
		pos = 0;
	} else if(after == tree::Sort)
	{
		pos = 0;
		while(pos < count && QString::compare(childAt(pos)->text(0), item->text(0), Qt::CaseInsensitive) <= 0)
			pos++;
	} else if(after != tree::Last && after != nullptr)
	{
		const int i = parent ? parent->indexOfChild(after) : indexOfTopLevelItem(after);
		if(i >= 0)
			pos = i + 1;
	}
	if(parent)
		parent->insertChild(pos, item);
	else
		insertTopLevelItem(pos, item);
	return item;
}


QTreeWidgetItem *TreeCtrl::InsertItem(const Text &text, QTreeWidgetItem *parent, QTreeWidgetItem *after)
{
	return InsertItem(tree::MaskText, text, 0, 0, 0, 0, 0, parent, after);
}


QTreeWidgetItem *TreeCtrl::InsertItem(const Text &text, int image, int selectedImage, QTreeWidgetItem *parent, QTreeWidgetItem *after)
{
	return InsertItem(tree::MaskText | tree::MaskImage | tree::MaskSelectedImage, text, image, selectedImage, 0, 0, 0, parent, after);
}


QTreeWidgetItem *TreeCtrl::InsertItem(uint32_t mask, const Text &text, int image, int selectedImage, uint32_t state, uint32_t stateMask, intptr_t data, QTreeWidgetItem *parent, QTreeWidgetItem *after)
{
	auto *item = new QTreeWidgetItem;
	item->setFlags(item->flags() | Qt::ItemIsEditable);
	if(mask & tree::MaskText)
		item->setText(0, text.value);
	item->setData(0, ImageRole, (mask & tree::MaskImage) ? image : 0);
	item->setData(0, SelImageRole, (mask & tree::MaskSelectedImage) ? selectedImage : ((mask & tree::MaskImage) ? image : 0));
	item->setData(0, DataRole, QVariant::fromValue<qlonglong>((mask & tree::MaskParam) ? data : 0));
	Place(item, parent, after);
	ApplyIcon(item, false);
	if(mask & tree::MaskState)
		SetItemState(item, state, stateMask);
	return item;
}


void TreeCtrl::NotifyDeleted(QTreeWidgetItem *item)
{
	for(int i = 0; i < item->childCount(); i++)
		NotifyDeleted(item->child(i));
	if(item == m_dropTarget)
		m_dropTarget = nullptr;
	if(item == m_pressItem)
		m_pressItem = nullptr;
	if(item == m_editItem)
		m_editItem = nullptr;
	Q_EMIT itemDeleted(item, GetItemData(item));
}


bool TreeCtrl::DeleteItem(QTreeWidgetItem *item)
{
	if(!item)
		return DeleteAllItems();
	NotifyDeleted(item);
	delete item;
	return true;
}


bool TreeCtrl::DeleteAllItems()
{
	for(int i = 0; i < topLevelItemCount(); i++)
		NotifyDeleted(topLevelItem(i));
	clear();
	m_dropTarget = nullptr;
	m_pressItem = nullptr;
	return true;
}


QTreeWidgetItem *TreeCtrl::GetChildItem(QTreeWidgetItem *item) const
{
	if(!item)
		return GetRootItem();
	return item->childCount() ? item->child(0) : nullptr;
}


QTreeWidgetItem *TreeCtrl::GetNextSiblingItem(QTreeWidgetItem *item) const
{
	if(!item)
		return nullptr;
	QTreeWidgetItem *parent = item->parent();
	const int i = parent ? parent->indexOfChild(item) : indexOfTopLevelItem(item);
	const int count = parent ? parent->childCount() : topLevelItemCount();
	if(i < 0 || i + 1 >= count)
		return nullptr;
	return parent ? parent->child(i + 1) : topLevelItem(i + 1);
}


QTreeWidgetItem *TreeCtrl::GetPrevSiblingItem(QTreeWidgetItem *item) const
{
	if(!item)
		return nullptr;
	QTreeWidgetItem *parent = item->parent();
	const int i = parent ? parent->indexOfChild(item) : indexOfTopLevelItem(item);
	if(i <= 0)
		return nullptr;
	return parent ? parent->child(i - 1) : topLevelItem(i - 1);
}


QTreeWidgetItem *TreeCtrl::GetFirstVisibleItem() const
{
	return itemAt(QPoint(1, 1));
}


QTreeWidgetItem *TreeCtrl::GetNextItem(QTreeWidgetItem *item, uint32_t code) const
{
	switch(code)
	{
	case tree::Root: return GetRootItem();
	case tree::Next: return GetNextSiblingItem(item);
	case tree::Previous: return GetPrevSiblingItem(item);
	case tree::Parent: return GetParentItem(item);
	case tree::Child: return GetChildItem(item);
	case tree::FirstVisible: return GetFirstVisibleItem();
	case tree::NextVisible: return GetNextVisibleItem(item);
	case tree::PreviousVisible: return GetPrevVisibleItem(item);
	case tree::DropHilite: return m_dropTarget;
	case tree::Caret: return currentItem();
	case tree::LastVisible:
	{
		QTreeWidgetItem *last = GetRootItem();
		while(last && itemBelow(last))
			last = itemBelow(last);
		return last;
	}
	}
	return nullptr;
}


int TreeCtrl::GetCount() const
{
	int count = 0;
	for(QTreeWidgetItemIterator it(const_cast<TreeCtrl *>(this)); *it; ++it)
		count++;
	return count;
}


bool TreeCtrl::ItemHasChildren(QTreeWidgetItem *item) const
{
	if(!item)
		return topLevelItemCount() > 0;
	return item->childCount() > 0 || item->childIndicatorPolicy() == QTreeWidgetItem::ShowIndicator;
}


bool TreeCtrl::SelectItem(QTreeWidgetItem *item)
{
	if(item)
	{
		for(QTreeWidgetItem *p = item->parent(); p; p = p->parent())
			p->setExpanded(true);
		setCurrentItem(item);
		scrollToItem(item);
	} else
	{
		setCurrentItem(nullptr);
		clearSelection();
	}
	return true;
}


bool TreeCtrl::SelectDropTarget(QTreeWidgetItem *item)
{
	if(m_dropTarget == item)
		return true;
	if(m_dropTarget)
	{
		m_dropTarget->setData(0, Qt::BackgroundRole, QVariant());
		if(!(m_dropTarget->data(0, StateRole).toUInt() & tree::Cut))
			m_dropTarget->setData(0, Qt::ForegroundRole, QVariant());
	}
	m_dropTarget = item;
	if(item)
	{
		item->setBackground(0, palette().brush(QPalette::Highlight));
		item->setForeground(0, palette().brush(QPalette::HighlightedText));
	}
	return true;
}


bool TreeCtrl::Select(QTreeWidgetItem *item, uint32_t code)
{
	switch(code)
	{
	case tree::Caret: return SelectItem(item);
	case tree::DropHilite: return SelectDropTarget(item);
	case tree::FirstVisible:
		if(item)
			scrollToItem(item, QAbstractItemView::PositionAtTop);
		return item != nullptr;
	}
	return false;
}


bool TreeCtrl::Expand(QTreeWidgetItem *item, uint32_t code)
{
	if(!item)
		return false;
	switch(code & 0x3)
	{
	case tree::Collapse: item->setExpanded(false); break;
	case tree::ExpandIt: item->setExpanded(true); break;
	case tree::Toggle: item->setExpanded(!item->isExpanded()); break;
	default: return false;
	}
	return true;
}


bool TreeCtrl::EnsureVisible(QTreeWidgetItem *item)
{
	if(!item)
		return false;
	for(QTreeWidgetItem *p = item->parent(); p; p = p->parent())
		p->setExpanded(true);
	scrollToItem(item);
	return true;
}


bool TreeCtrl::SetItemText(QTreeWidgetItem *item, const Text &text)
{
	if(!item)
		return false;
	if(item->text(0) != text.value)
		item->setText(0, text.value);
	return true;
}


bool TreeCtrl::SetItemImage(QTreeWidgetItem *item, int image, int selectedImage)
{
	if(!item)
		return false;
	item->setData(0, ImageRole, image);
	item->setData(0, SelImageRole, selectedImage);
	ApplyIcon(item, item == currentItem());
	return true;
}


bool TreeCtrl::GetItemImage(QTreeWidgetItem *item, int &image, int &selectedImage) const
{
	if(!item)
		return false;
	image = item->data(0, ImageRole).toInt();
	selectedImage = item->data(0, SelImageRole).toInt();
	return true;
}


bool TreeCtrl::SetItemData(QTreeWidgetItem *item, intptr_t data)
{
	if(!item)
		return false;
	item->setData(0, DataRole, QVariant::fromValue<qlonglong>(data));
	return true;
}


intptr_t TreeCtrl::GetItemData(QTreeWidgetItem *item) const
{
	return item ? static_cast<intptr_t>(item->data(0, DataRole).toLongLong()) : 0;
}


bool TreeCtrl::SetItemState(QTreeWidgetItem *item, uint32_t state, uint32_t stateMask)
{
	if(!item)
		return false;
	const uint32_t old = item->data(0, StateRole).toUInt();
	const uint32_t kept = (old & ~stateMask) | (state & stateMask);
	ApplyState(item, kept);
	if(stateMask & tree::Expanded)
		item->setExpanded((state & tree::Expanded) != 0);
	if(stateMask & tree::Selected)
	{
		if(state & tree::Selected)
			setCurrentItem(item);
		else
			item->setSelected(false);
	}
	if(stateMask & tree::DropHilited)
		SelectDropTarget((state & tree::DropHilited) ? item : (m_dropTarget == item ? nullptr : m_dropTarget));
	return true;
}


uint32_t TreeCtrl::GetItemState(QTreeWidgetItem *item, uint32_t stateMask) const
{
	if(!item)
		return 0;
	uint32_t state = item->data(0, StateRole).toUInt();
	if(item->isExpanded())
		state |= tree::Expanded;
	if(item->isSelected() || item == currentItem())
		state |= tree::Selected;
	if(item == m_dropTarget)
		state |= tree::DropHilited;
	return state & stateMask;
}


bool TreeCtrl::GetItem(TreeItemInfo &info) const
{
	QTreeWidgetItem *item = info.item;
	if(!item)
		return false;
	if(info.mask & tree::MaskText)
		info.text = item->text(0);
	if(info.mask & tree::MaskImage)
		info.image = item->data(0, ImageRole).toInt();
	if(info.mask & tree::MaskSelectedImage)
		info.selectedImage = item->data(0, SelImageRole).toInt();
	if(info.mask & tree::MaskParam)
		info.data = GetItemData(item);
	if(info.mask & tree::MaskState)
		info.state = GetItemState(item, info.stateMask ? info.stateMask : 0xFFFFFFFF);
	if(info.mask & tree::MaskChildren)
		info.children = ItemHasChildren(item) ? 1 : 0;
	return true;
}


bool TreeCtrl::SetItem(const TreeItemInfo &info)
{
	QTreeWidgetItem *item = info.item;
	if(!item)
		return false;
	if(info.mask & tree::MaskText)
		SetItemText(item, info.text);
	if(info.mask & (tree::MaskImage | tree::MaskSelectedImage))
	{
		if(info.mask & tree::MaskImage)
			item->setData(0, ImageRole, info.image);
		if(info.mask & tree::MaskSelectedImage)
			item->setData(0, SelImageRole, info.selectedImage);
		ApplyIcon(item, item == currentItem());
	}
	if(info.mask & tree::MaskParam)
		SetItemData(item, info.data);
	if(info.mask & tree::MaskState)
		SetItemState(item, info.state, info.stateMask);
	if(info.mask & tree::MaskChildren)
		item->setChildIndicatorPolicy(info.children ? QTreeWidgetItem::ShowIndicator : QTreeWidgetItem::DontShowIndicatorWhenChildless);
	return true;
}


bool TreeCtrl::SetItem(QTreeWidgetItem *item, uint32_t mask, const Text &text, int image, int selectedImage, uint32_t state, uint32_t stateMask, intptr_t data)
{
	TreeItemInfo info;
	info.mask = mask;
	info.item = item;
	info.text = text.value;
	info.image = image;
	info.selectedImage = selectedImage;
	info.state = state;
	info.stateMask = stateMask;
	info.data = data;
	return SetItem(info);
}


QTreeWidgetItem *TreeCtrl::HitTest(Point pt, uint32_t *flags) const
{
	const QPoint vp = viewport()->mapFrom(this, QPoint(pt));
	uint32_t where = 0;
	QTreeWidgetItem *item = nullptr;
	if(vp.y() < 0)
		where = tree::Above;
	else if(vp.y() >= viewport()->height())
		where = tree::Below;
	if(vp.x() < 0)
		where |= tree::ToLeft;
	else if(vp.x() >= viewport()->width())
		where |= tree::ToRight;
	if(!where)
	{
		item = itemAt(vp);
		if(!item)
		{
			where = tree::Nowhere;
		} else
		{
			const QRect r = visualItemRect(item);
			const int iconWidth = item->icon(0).isNull() ? 0 : iconSize().width() + 4;
			const int textWidth = fontMetrics().horizontalAdvance(item->text(0)) + 6;
			if(vp.x() < r.left())
				where = (vp.x() >= r.left() - indentation() && ItemHasChildren(item)) ? tree::OnItemButton : tree::OnItemIndent;
			else if(vp.x() < r.left() + iconWidth)
				where = tree::OnItemIcon;
			else if(vp.x() < r.left() + iconWidth + textWidth)
				where = tree::OnItemLabel;
			else
				where = tree::OnItemRight;
		}
	}
	if(flags)
		*flags = where;
	return item;
}


bool TreeCtrl::GetItemRect(QTreeWidgetItem *item, Rect &rect, bool textOnly) const
{
	if(!item)
		return false;
	QRect r = visualItemRect(item);
	if(r.isEmpty())
		return false;
	if(textOnly)
	{
		const int iconWidth = item->icon(0).isNull() ? 0 : iconSize().width() + 4;
		r.setLeft(r.left() + iconWidth);
		r.setWidth(fontMetrics().horizontalAdvance(item->text(0)) + 6);
	} else
	{
		r.setLeft(0);
		r.setRight(viewport()->width() - 1);
	}
	rect = Rect(r.translated(viewport()->pos()));
	return true;
}


bool TreeCtrl::edit(const QModelIndex &index, EditTrigger trigger, QEvent *event)
{
	if(index.column() != 0)
		return false;
	QTreeWidgetItem *item = itemFromIndex(index);
	if(!item || m_editor)
		return Window::edit(index, trigger, event);
	if(trigger != QAbstractItemView::AllEditTriggers && !(m_style & tree::EditLabels))
		return false;
	if(!(editTriggers() & trigger) && trigger != QAbstractItemView::AllEditTriggers)
		return false;
	bool allow = true;
	Q_EMIT labelEditBegun(item, &allow);
	if(!allow)
		return false;
	return Window::edit(index, trigger, event);
}


QLineEdit *TreeCtrl::EditLabel(QTreeWidgetItem *item)
{
	if(!item)
		return nullptr;
	setCurrentItem(item);
	scrollToItem(item);
	edit(indexFromItem(item, 0), QAbstractItemView::AllEditTriggers, nullptr);
	return GetEditControl();
}


QLineEdit *TreeCtrl::GetEditControl() const
{
	return qobject_cast<QLineEdit *>(m_editor);
}


bool TreeCtrl::EndEditLabelNow(bool cancel)
{
	QWidget *editor = m_editor;
	if(!editor)
		return false;
	if(!cancel)
		commitData(editor);
	closeEditor(editor, QAbstractItemDelegate::NoHint);
	return true;
}


namespace
{
void SaveExpanded(QTreeWidgetItem *item, std::set<QTreeWidgetItem *> &expanded)
{
	for(int i = 0; i < item->childCount(); i++)
	{
		QTreeWidgetItem *child = item->child(i);
		if(child->isExpanded())
			expanded.insert(child);
		SaveExpanded(child, expanded);
	}
}
void RestoreExpanded(QTreeWidgetItem *item, const std::set<QTreeWidgetItem *> &expanded)
{
	for(int i = 0; i < item->childCount(); i++)
	{
		QTreeWidgetItem *child = item->child(i);
		if(expanded.count(child))
			child->setExpanded(true);
		RestoreExpanded(child, expanded);
	}
}
}


bool TreeCtrl::SortChildren(QTreeWidgetItem *item)
{
	return SortChildren(item, nullptr);
}


bool TreeCtrl::SortChildren(QTreeWidgetItem *item, const std::function<int(intptr_t, intptr_t)> &compare)
{
	QTreeWidgetItem *parent = item ? item : invisibleRootItem();
	if(parent->childCount() < 2)
		return true;
	std::set<QTreeWidgetItem *> expanded;
	SaveExpanded(parent, expanded);
	QTreeWidgetItem *current = currentItem();
	QList<QTreeWidgetItem *> children = parent->takeChildren();
	std::stable_sort(children.begin(), children.end(), [&](QTreeWidgetItem *a, QTreeWidgetItem *b) {
		if(compare)
			return compare(GetItemData(a), GetItemData(b)) < 0;
		return QString::compare(a->text(0), b->text(0), Qt::CaseInsensitive) < 0;
	});
	parent->addChildren(children);
	RestoreExpanded(parent, expanded);
	if(current)
		setCurrentItem(current);
	return true;
}


void TreeCtrl::mousePressEvent(QMouseEvent *event)
{
	m_pressPos = event->position().toPoint();
	m_pressButton = event->button();
	m_pressItem = itemAt(m_pressPos);
	// A right click doesn't move the selection, as on Windows.
	if(event->button() == Qt::RightButton)
	{
		event->accept();
		return;
	}
	Window::mousePressEvent(event);
}


void TreeCtrl::mouseMoveEvent(QMouseEvent *event)
{
	if(m_pressItem && m_pressButton != Qt::NoButton && (event->buttons() & m_pressButton)
		&& (event->position().toPoint() - m_pressPos).manhattanLength() >= QApplication::startDragDistance())
	{
		QTreeWidgetItem *item = m_pressItem;
		const bool right = (m_pressButton == Qt::RightButton);
		m_pressItem = nullptr;
		m_pressButton = Qt::NoButton;
		Q_EMIT beginDrag(item, right);
		return;
	}
	if(m_pressButton == Qt::RightButton)
	{
		// The drag (if any) is the handler's.
		event->accept();
		return;
	}
	Window::mouseMoveEvent(event);
}


void TreeCtrl::mouseReleaseEvent(QMouseEvent *event)
{
	const bool right = (event->button() == Qt::RightButton);
	m_pressItem = nullptr;
	m_pressButton = Qt::NoButton;
	if(right)
	{
		event->accept();
		return;
	}
	Window::mouseReleaseEvent(event);
}

} // namespace mfcless
