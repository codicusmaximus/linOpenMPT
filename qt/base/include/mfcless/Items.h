/*
 * Items.h
 * -------
 * Purpose: Short forms for the item lists of combo boxes and list widgets
 *          that upstream's dialogs fill with a text and a number per item.
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */

#pragma once

#include "mfcless/QtString.h"

#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QListWidget>

namespace mfcless
{

// Adds an item with a number attached; returns its index.
// A combo box made with CBS_SORT keeps its items in order (case-insensitive,
// as Windows sorts them).
inline int AddItem(QComboBox *combo, const QString &text, uintptr_t data = 0)
{
	int index = combo->count();
	if(combo->property("mfclessSorted").toBool())
	{
		index = 0;
		while(index < combo->count() && QString::compare(combo->itemText(index), text, Qt::CaseInsensitive) <= 0)
			index++;
	}
	combo->insertItem(index, text, QVariant::fromValue<qulonglong>(data));
	return index;
}
inline int AddItem(QComboBox *combo, const ::mpt::ustring &text, uintptr_t data = 0) { return AddItem(combo, ToQString(text), data); }
inline int InsertItem(QComboBox *combo, int index, const ::mpt::ustring &text, uintptr_t data = 0)
{
	if(index < 0 || index > combo->count())
		index = combo->count();
	combo->insertItem(index, ToQString(text), QVariant::fromValue<qulonglong>(data));
	return index;
}
inline void SetItemData(QComboBox *combo, int index, uintptr_t data) { combo->setItemData(index, QVariant::fromValue<qulonglong>(data)); }
inline uintptr_t GetItemData(const QComboBox *combo, int index) { return static_cast<uintptr_t>(combo->itemData(index).toULongLong()); }
// The data of the current item, or fallback if there is none.
inline uintptr_t CurrentData(const QComboBox *combo, uintptr_t fallback = 0)
{
	return combo->currentIndex() >= 0 ? GetItemData(combo, combo->currentIndex()) : fallback;
}
// The index of the first item with this data, or -1.
inline int FindItemData(const QComboBox *combo, uintptr_t data)
{
	for(int i = 0; i < combo->count(); i++)
	{
		if(GetItemData(combo, i) == data)
			return i;
	}
	return -1;
}
// Selects the first item with this data; false if there is none.
inline bool SelectItemData(QComboBox *combo, uintptr_t data)
{
	const int i = FindItemData(combo, data);
	combo->setCurrentIndex(i);
	return i >= 0;
}
inline ::mpt::ustring ItemText(const QComboBox *combo, int index) { return FromQString(combo->itemText(index)); }

inline int AddItem(QListWidget *list, const QString &text, uintptr_t data = 0)
{
	auto *item = new QListWidgetItem(text, list);
	item->setData(Qt::UserRole, QVariant::fromValue<qulonglong>(data));
	return list->row(item);
}
inline int AddItem(QListWidget *list, const ::mpt::ustring &text, uintptr_t data = 0) { return AddItem(list, ToQString(text), data); }
inline int InsertItem(QListWidget *list, int index, const ::mpt::ustring &text, uintptr_t data = 0)
{
	if(index < 0 || index > list->count())
		index = list->count();
	auto *item = new QListWidgetItem(ToQString(text));
	item->setData(Qt::UserRole, QVariant::fromValue<qulonglong>(data));
	list->insertItem(index, item);
	return index;
}
inline void SetItemData(QListWidget *list, int index, uintptr_t data)
{
	if(QListWidgetItem *item = list->item(index))
		item->setData(Qt::UserRole, QVariant::fromValue<qulonglong>(data));
}
inline uintptr_t GetItemData(const QListWidget *list, int index)
{
	const QListWidgetItem *item = list->item(index);
	return item ? static_cast<uintptr_t>(item->data(Qt::UserRole).toULongLong()) : 0;
}
inline ::mpt::ustring ItemText(const QListWidget *list, int index)
{
	const QListWidgetItem *item = list->item(index);
	return item ? FromQString(item->text()) : ::mpt::ustring();
}

// String literals (UL_) pick the ustring forms.
inline int AddItem(QComboBox *combo, const char8_t *text, uintptr_t data = 0) { return AddItem(combo, ::mpt::ustring(text), data); }
inline int AddItem(QListWidget *list, const char8_t *text, uintptr_t data = 0) { return AddItem(list, ::mpt::ustring(text), data); }
inline int AddItem(QComboBox *combo, const char *text, uintptr_t data = 0) { return AddItem(combo, QString::fromUtf8(text), data); }
inline int AddItem(QListWidget *list, const char *text, uintptr_t data = 0) { return AddItem(list, QString::fromUtf8(text), data); }

// A pointer attached to an item.
inline void SetItemDataPtr(QComboBox *combo, int index, const void *p) { SetItemData(combo, index, reinterpret_cast<uintptr_t>(p)); }
inline void *GetItemDataPtr(const QComboBox *combo, int index) { return reinterpret_cast<void *>(GetItemData(combo, index)); }
inline void SetItemDataPtr(QListWidget *list, int index, const void *p) { SetItemData(list, index, reinterpret_cast<uintptr_t>(p)); }
inline void *GetItemDataPtr(const QListWidget *list, int index) { return reinterpret_cast<void *>(GetItemData(list, index)); }

// The selection of a list box with several selected items.
inline void SetSel(QListWidget *list, int index, bool select = true)
{
	if(index < 0)
	{
		for(int i = 0; i < list->count(); i++)
			list->item(i)->setSelected(select);
	} else if(QListWidgetItem *item = list->item(index))
	{
		item->setSelected(select);
	}
}
inline bool GetSel(const QListWidget *list, int index)
{
	const QListWidgetItem *item = list->item(index);
	return item && item->isSelected();
}
inline void SelItemRange(QListWidget *list, bool select, int first, int last)
{
	for(int i = first; i <= last && i < list->count(); i++)
		SetSel(list, i, select);
}
inline int GetSelCount(const QListWidget *list) { return static_cast<int>(list->selectedItems().size()); }
inline void DeleteString(QListWidget *list, int index) { delete list->takeItem(index); }

// An edit field's selection (from start to before end; end -1: to the end).
inline void SetSel(QLineEdit *edit, int start, int end, bool /*noScroll*/ = false)
{
	const int length = static_cast<int>(edit->text().size());
	if(start < 0)
	{
		edit->deselect();
		return;
	}
	if(end < 0 || end > length)
		end = length;
	edit->setSelection(start, end - start);
}
inline void GetSel(const QLineEdit *edit, int &start, int &end)
{
	if(edit->hasSelectedText())
	{
		start = edit->selectionStart();
		end = start + static_cast<int>(edit->selectedText().size());
	} else
	{
		start = end = edit->cursorPosition();
	}
}

// Check marks: of a list box's items (a check list box) or of a button
// (0 unchecked, 1 checked, 2 indeterminate).
inline void SetCheck(QListWidget *list, int index, int check)
{
	if(QListWidgetItem *item = list->item(index))
	{
		item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
		item->setCheckState(check == 2 ? Qt::PartiallyChecked : (check ? Qt::Checked : Qt::Unchecked));
	}
}
inline int GetCheck(const QListWidget *list, int index)
{
	const QListWidgetItem *item = list->item(index);
	if(!item)
		return 0;
	return item->checkState() == Qt::Checked ? 1 : (item->checkState() == Qt::PartiallyChecked ? 2 : 0);
}
inline void SetCheck(QAbstractButton *button, int check)
{
	if(auto *box = qobject_cast<QCheckBox *>(button); box && check == 2)
	{
		box->setTristate(true);
		box->setCheckState(Qt::PartiallyChecked);
	} else
	{
		button->setChecked(check != 0);
	}
}
inline int GetCheck(const QAbstractButton *button)
{
	if(auto *box = qobject_cast<const QCheckBox *>(button); box && box->checkState() == Qt::PartiallyChecked)
		return 2;
	return button->isChecked() ? 1 : 0;
}

} // namespace mfcless
