/*
 * Items.h
 * -------
 * Purpose: Short forms for the item lists of combo boxes and list widgets
 *          that upstream's dialogs fill with a text and a number per item.
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */

#pragma once

#include "mfcless/QtString.h"

#include <QtWidgets/QComboBox>
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

} // namespace mfcless
