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

#include <typeinfo>
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
	bool Create(uint32 nIDTemplate, QWidget *parent) { m_templateID = nIDTemplate; return Create(parent); }

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
	// mfcless: a control the dialog makes itself (CWnd::Create with a parent
	// and an ID), so that its notifications and commands reach the message
	// map. A later control with the same ID does not replace the first for
	// GetDlgItem.
	void AddDynamicItem(uint32 id, QWidget *control);
	void EnableItem(uint32 id, bool enable = true);
	void ShowItem(uint32 id, bool show = true);

	// Reads the controls into the members (save) or the other way round,
	// through DoDataExchange. False if a value did not validate.
	bool UpdateData(bool save = true);

protected:
	virtual void OnOK() { if(UpdateData(true)) accept(); }
	// Closes a modal dialog with this result (IDOK accepts it).
	void EndDialog(int result) { done(result == 1 /* IDOK */ ? QDialog::Accepted : result); }
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
				// The template's control is of another kind than the member
				// (a multi-line edit for a QLineEdit, say): the member stays null.
				qWarning("mfcless: dialog control %u is not a %s", static_cast<unsigned>(item.id), typeid(T).name());
				return made;
			}
		};
	}
	// SubclassDlgItem: makes the control with this ID again as the dialog's
	// own class (T), in the same place, after the template was built.
	template <typename T>
	bool SubclassDlgItem(uint32 id, T *&member)
	{
		QWidget *old = GetDlgItem(id);
		if(auto *t = dynamic_cast<T *>(old))
		{
			member = t;
			return true;
		}
		if constexpr(std::is_base_of_v<QWidget, T> && std::is_constructible_v<T, QWidget *> && !std::is_abstract_v<T>)
		{
			if(old)
			{
				T *t = new T(old->parentWidget());
				if(ReplaceItem(id, t))
				{
					member = t;
					return true;
				}
				delete t;
			}
		}
		member = nullptr;
		return false;
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
	friend class PropertySheet;
	void CreateFromTemplate();
	// Sets up replacement as the template's control id and puts it in the
	// place of the control made before, which is deleted.
	bool ReplaceItem(uint32 id, QWidget *replacement);

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
class PropertySheet;
OPENMPT_NAMESPACE_END
class QAbstractButton;
class QKeyEvent;
OPENMPT_NAMESPACE_BEGIN

class PropertyPage : public DialogBase
{
	Q_OBJECT
public:
	using DialogBase::DialogBase;

	// The sheet the page is on (upstream: its parent window).
	PropertySheet *GetSheet() const { return m_sheet; }

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
	friend class PropertySheet;
	PropertySheet *m_sheet = nullptr;
	bool m_modified = false;
};


// mfcless: the role of MFC's CPropertySheet: a dialog with a tab for each
// page and OK, Cancel and Apply buttons. Pages are made when first shown, and
// asked (OnKillActive) before another page is shown.
class PropertySheet : public mfcless::Window<QDialog>
{
	Q_OBJECT
public:
	PropertySheet(const mpt::ustring &caption, QWidget *parent = nullptr, uint32 selectPage = 0);
	~PropertySheet() override;

	void AddPage(PropertyPage *page);
	int GetPageCount() const { return static_cast<int>(m_pages.size()); }
	PropertyPage *GetPage(int index) const;
	int GetActiveIndex() const;
	PropertyPage *GetActivePage() const { return GetPage(GetActiveIndex()); }
	bool SetActivePage(int index);
	bool SetActivePage(PropertyPage *page);
	void SetTitle(const mpt::ustring &title);

	// IDOK (1), IDCANCEL (2), ID_APPLY_NOW (0x3021)
	// mfcless: a control the dialog makes itself (CWnd::Create with a parent
	// and an ID), so that its notifications and commands reach the message
	// map. A later control with the same ID does not replace the first for
	// GetDlgItem.
	void AddDynamicItem(uint32 id, QWidget *control);
	void EnableItem(uint32 id, bool enable = true);
	QAbstractButton *GetButton(uint32 id) const;

	int DoModal();

protected:
	virtual bool OnInitDialog() { return true; }
	// The Help button and F1 (upstream: the caption's ? button).
	virtual void OnHelp() {}
	void SetHelpButton(bool show);

	void keyPressEvent(QKeyEvent *e) override;

private:
	void Build();
	void Activate(int index);
	bool Apply();
	void UpdateApply();

	std::vector<PropertyPage *> m_pages;
	class QTabWidget *m_tabs = nullptr;
	class QDialogButtonBox *m_buttons = nullptr;
	int m_current = -1;
	uint32 m_startPage = 0;
	bool m_built = false, m_switching = false, m_hasHelp = false;
};

OPENMPT_NAMESPACE_END
