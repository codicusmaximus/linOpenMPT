/*
 * AccessibleControls.h
 * --------------------
 * Purpose: Helper functions for improving accessibility of MFC controls
 * Notes  : mfcless twin of upstream's mptrack/AccessibleControls.h. Qt gives
 *          every widget an accessible name and description, so these wrap
 *          are widgets that the dialog template makes in place of its
 *          controls, and set those.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"

#include <QtWidgets/QPushButton>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QLineEdit>

OPENMPT_NAMESPACE_BEGIN

// mfcless: upstream's accessible controls are subclassed MFC controls; here
// they are the widgets themselves, made from the dialog template in their
// place (DDX_Control or SubclassDlgItem).
inline QString AccessibleQString(const mpt::ustring &s) { return QString::fromUtf8(reinterpret_cast<const char *>(s.data()), static_cast<qsizetype>(s.size())); }


class AccessibleButton : public QPushButton
{
public:
	explicit AccessibleButton(QWidget *parent = nullptr) : QPushButton(parent) {}
	void SetAccessibleText(const mpt::ustring &text) { if(!text.empty()) setAccessibleName(AccessibleQString(text)); }
};


class AccessibleEdit : public QLineEdit
{
public:
	explicit AccessibleEdit(QWidget *parent = nullptr) : QLineEdit(parent) {}
	void SetAccessibleName(const mpt::ustring &name) { if(!name.empty()) setAccessibleName(AccessibleQString(name)); }
	// Upstream appends the suffix (a unit, such as "ms") to the value that
	// screen readers read; Qt reads the description after the value.
	void SetAccessibleSuffix(const mpt::ustring &suffix) { if(!suffix.empty()) setAccessibleDescription(AccessibleQString(suffix)); }
	using QLineEdit::setAccessibleName;
};


class AccessibleComboBox : public QComboBox
{
public:
	explicit AccessibleComboBox(QWidget *parent = nullptr) : QComboBox(parent) {}
	void SetAccessibleName(const mpt::ustring &name) { if(!name.empty()) setAccessibleName(AccessibleQString(name)); }
	using QComboBox::setAccessibleName;
};


OPENMPT_NAMESPACE_END
