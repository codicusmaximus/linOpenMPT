/*
 * DialogBase.h
 * ------------
 * Purpose: Base class for dialogs that adds functionality on top of CDialog.
 * Notes  : mfcless twin of upstream's mptrack/DialogBase.h. The base is a
 *          QDialog whose controls are created from upstream's dialog template
 *          (the resource ID), at the template's positions, by the code that
 *          qt/resources/ generates from mptrack.rc. Controls are found by
 *          their resource IDs, as with GetDlgItem.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "Snd_defs.h"

#include <QtWidgets/QDialog>

#include <map>

OPENMPT_NAMESPACE_BEGIN

class DialogBase : public QDialog
{
	Q_OBJECT
public:
	enum class InputDevice : uint8
	{
		Unknown,
		Mouse,
		Keyboard,
	};

	DialogBase(QWidget *parent = nullptr);
	DialogBase(uint32 nIDTemplate, QWidget *parent = nullptr);

	// mfcless: CDialog::DoModal and Create.
	int DoModal();
	bool Create(QWidget *parent = nullptr);

	virtual bool OnInitDialog();

	static bool HandleGlobalKeyMessage(const QKeyEvent &msg);

	int GetDPI() const { return m_dpi; }

	void SetFocusToFirstControl();

	// mfcless: GetDlgItem. The control created for this resource ID, or
	// nullptr.
	QWidget *GetDlgItem(uint32 id) const;
	template <typename T>
	T *Item(uint32 id) const { return qobject_cast<T *>(GetDlgItem(id)); }
	uint32 GetTemplateID() const { return m_templateID; }

protected:
	virtual void OnOK() { accept(); }
	virtual void OnCancel() { reject(); }

	virtual void OnDPIChanged() {}
	void UpdateDPI();

	virtual mpt::ustring GetToolTipText(uint32 /*id*/, QWidget * /*hwnd*/) const { return {}; }

	bool event(QEvent *e) override;
	bool eventFilter(QObject *watched, QEvent *e) override;
	void showEvent(QShowEvent *e) override;

private:
	void CreateFromTemplate();

	uint32 m_templateID = 0;
	std::map<uint32, QWidget *> m_items;
	bool m_initialized = false;
	int m_dpi = 96;
protected:
	InputDevice m_lastInputDevice = InputDevice::Unknown;
};

OPENMPT_NAMESPACE_END
