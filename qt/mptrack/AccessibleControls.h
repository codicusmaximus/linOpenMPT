/*
 * AccessibleControls.h
 * --------------------
 * Purpose: Helper functions for improving accessibility of MFC controls
 * Notes  : mfcless twin of upstream's mptrack/AccessibleControls.h. Qt gives
 *          every widget an accessible name and description, so these wrap
 *          the control the dialog template created (Attach() takes the place
 *          of DDX_Control's subclassing) and set those.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"

#include <QtCore/QPointer>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QLineEdit>

OPENMPT_NAMESPACE_BEGIN

template <typename TWidget>
class AccessibleControl
{
public:
	void Attach(TWidget *widget) { m_widget = widget; Update(); }
	TWidget *operator->() const { return m_widget; }
	TWidget *get() const { return m_widget; }
	operator TWidget *() const { return m_widget; }

protected:
	virtual ~AccessibleControl() = default;
	virtual void Update() { }
	QPointer<TWidget> m_widget;
};


class AccessibleButton : public AccessibleControl<QAbstractButton>
{
public:
	void SetAccessibleText(const mpt::ustring &text) { m_text = text; Update(); }

private:
	void Update() override
	{
		if(m_widget && !m_text.empty())
			m_widget->setAccessibleName(QString::fromStdString(mpt::ToCharset(mpt::Charset::UTF8, m_text)));
	}
	mpt::ustring m_text;
};


class AccessibleEdit : public AccessibleControl<QLineEdit>
{
public:
	void SetAccessibleName(const mpt::ustring &name) { m_name = name; Update(); }
	// Upstream appends the suffix (a unit, such as "ms") to the value that
	// screen readers read; Qt reads the description after the value.
	void SetAccessibleSuffix(const mpt::ustring &suffix) { m_suffix = suffix; Update(); }

private:
	void Update() override
	{
		if(!m_widget)
			return;
		if(!m_name.empty())
			m_widget->setAccessibleName(QString::fromStdString(mpt::ToCharset(mpt::Charset::UTF8, m_name)));
		if(!m_suffix.empty())
			m_widget->setAccessibleDescription(QString::fromStdString(mpt::ToCharset(mpt::Charset::UTF8, m_suffix)));
	}
	mpt::ustring m_name;
	mpt::ustring m_suffix;
};


class AccessibleComboBox : public AccessibleControl<QComboBox>
{
public:
	void SetAccessibleName(const mpt::ustring &name) { m_name = name; Update(); }

private:
	void Update() override
	{
		if(m_widget && !m_name.empty())
			m_widget->setAccessibleName(QString::fromStdString(mpt::ToCharset(mpt::Charset::UTF8, m_name)));
	}
	mpt::ustring m_name;
};


OPENMPT_NAMESPACE_END
