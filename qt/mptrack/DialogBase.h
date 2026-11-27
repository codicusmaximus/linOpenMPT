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

#include "mfcless/DialogBuilder.h"
#include "mfcless/Window.h"

#include <QtWidgets/QDialog>

#include <map>
#include <type_traits>

OPENMPT_NAMESPACE_BEGIN

// mfcless: the direction of a DoDataExchange pass: true reads the controls
// into the members, false shows the members in the controls.
// mfcless: before the controls are made, a first pass (collect) only
// gathers the controls that the dialog binds with ExchangeControl, so that
// those of a class of the dialog's own are made as that class.
struct DataExchange
{
	bool save = false;
	bool failed = false;
	bool collect = false;
	std::map<uint32, mfcless::ControlFactory> factories;
};

class DialogBase : public mfcless::Window<QDialog>
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

	bool PreTranslateMessage(mfcless::Message *pMsg) override;

	static bool HandleGlobalKeyMessage(const mfcless::Message &msg);

	int GetDPI() const { return m_dpi; }

	void SetFocusToFirstControl();

	// mfcless: GetDlgItem. The control created for this resource ID, or
	// nullptr.
	QWidget *GetDlgItem(uint32 id) const;
	template <typename T>
	T *Item(uint32 id) const { return qobject_cast<T *>(GetDlgItem(id)); }
	uint32 GetTemplateID() const { return m_templateID; }

	// mfcless: helpers for the controls by resource ID.
	template <typename T>
	void Bind(uint32 id, T *&member) { member = qobject_cast<T *>(GetDlgItem(id)); }
	void SetItemText(uint32 id, const mpt::ustring &text);
	mpt::ustring GetItemText(uint32 id) const;
	void SetItemInt(uint32 id, int64 value);
	int64 GetItemInt(uint32 id, bool *ok = nullptr) const;
	// 0 unchecked, 1 checked, 2 indeterminate
	void SetChecked(uint32 id, int state);
	int IsChecked(uint32 id) const;
	// Checks id and unchecks the other radio buttons from first to last.
	void CheckRadio(uint32 first, uint32 last, uint32 id);
	// The checked radio button from first to last, or 0.
	uint32 GetCheckedRadio(uint32 first, uint32 last) const;
	void EnableItem(uint32 id, bool enable = true);
	void ShowItem(uint32 id, bool show = true);

	// Reads the controls into the members (save) or the other way round,
	// through DoDataExchange. False if a value did not validate.
	bool UpdateData(bool save = true);

protected:
	virtual void OnOK() { if(UpdateData(true)) accept(); }
	virtual void OnCancel() { reject(); }
	virtual void DoDataExchange(DataExchange & /*dx*/) {}
	void RegisterMessageMap() override;

	// DDX_Control: binds member to the control with this ID. A control of a
	// class of the dialog's own (a derived control) is made as that class.
	template <typename T>
	void ExchangeControl(DataExchange &dx, uint32 id, T *&member)
	{
		if(!dx.collect)
		{
			if(!member)
				member = dynamic_cast<T *>(GetDlgItem(id));
			return;
		}
		dx.factories[id] = [&member](const mfcless::res::DialogItem &item, QWidget *parent) -> QWidget * {
			QWidget *made = mfcless::CreateControl(item, parent);
			if(auto *t = dynamic_cast<T *>(made))
			{
				member = t;
				return made;
			}
			if constexpr(std::is_constructible_v<T, QWidget *> && !std::is_abstract_v<T>)
			{
				delete made;
				member = new T(parent);
				return member;
			} else
			{
				return made;
			}
		};
	}
	// The same for a control that is a member of the dialog.
	template <typename T>
	void ExchangeControl(DataExchange &dx, uint32 id, T &member)
	{
		if(dx.collect)
			dx.factories[id] = [&member](const mfcless::res::DialogItem &, QWidget *) -> QWidget * { return &member; };
	}

	void ExchangeText(DataExchange &dx, uint32 id, mpt::ustring &value);
	void ExchangeText(DataExchange &dx, uint32 id, int32 &value);
	void ExchangeText(DataExchange &dx, uint32 id, uint32 &value);
	void ExchangeText(DataExchange &dx, uint32 id, double &value);
	void ExchangeCheck(DataExchange &dx, uint32 id, int &value);
	void ExchangeCheck(DataExchange &dx, uint32 id, bool &value);
	// The index of the checked button of the radio group that starts at id.
	void ExchangeRadio(DataExchange &dx, uint32 id, int &value);
	void ExchangeSlider(DataExchange &dx, uint32 id, int &value);
	void ExchangeComboIndex(DataExchange &dx, uint32 id, int &value);
	// Fails the exchange if value is outside min..max (and says so).
	void ValidateMinMax(DataExchange &dx, int64 value, int64 min, int64 max);

	virtual void OnDPIChanged() {}
	void UpdateDPI();

	virtual mpt::ustring GetToolTipText(uint32 /*id*/, QWidget * /*hwnd*/) const { return {}; }

	void reject() override;
	bool eventFilter(QObject *watched, QEvent *e) override;
	void showEvent(QShowEvent *e) override;

private:
	void CreateFromTemplate();

	uint32 m_templateID = 0;
	std::map<uint32, QWidget *> m_items;
	std::vector<QWidget *> m_templateOrder;
	bool m_initialized = false;
	bool m_inCancel = false;
	int m_dpi = 96;
protected:
	InputDevice m_lastInputDevice = InputDevice::Unknown;
};


// mfcless: the role of MFC's CPropertyPage (one page of a tabbed dialog such
// as the options). Created from its dialog template like any dialog; the
// sheet calls these as MFC's property sheet does.
class PropertyPage : public DialogBase
{
	Q_OBJECT
public:
	using DialogBase::DialogBase;

	virtual bool OnSetActive() { return true; }
	virtual bool OnKillActive() { return true; }
	virtual bool OnApply() { OnOK(); return true; }
	void OnOK() override { UpdateData(true); }
	void OnCancel() override { }
	// Marks the page as changed, which enables the sheet's Apply button.
	void SetModified(bool changed = true);
	bool IsModified() const { return m_modified; }

signals:
	void modifiedChanged(bool changed);

private:
	bool m_modified = false;
};

OPENMPT_NAMESPACE_END
