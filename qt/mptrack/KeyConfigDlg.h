/*
 * KeyConfigDlg.h
 * --------------
 * Purpose: Implementation of OpenMPT's keyboard configuration dialog.
 * Notes  : mfcless twin of upstream's mptrack/KeyConfigDlg.h.
 *          Draft from qt/tools/twin.py, not yet edited by hand.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "DialogBase.h"
#include <QtWidgets/QLineEdit>
#include "CListCtrl.h"
#include "CommandSet.h"

class QAbstractButton;
class QComboBox;
class QEvent;
class QLabel;
class QListWidget;
class QObject;
class QPoint;
class QWidget;

OPENMPT_NAMESPACE_BEGIN

class COptionsKeyboard;

class CommandCategory
{
public:
	CommandCategory(const mpt::uchar *n, InputTargetContext ctx) : name{n}, id{ctx} {}

	const mpt::ustring name;
	const InputTargetContext id;
	
	struct Range
	{
		Range(CommandID f, CommandID l, const mpt::uchar *n) : first{f}, last{l}, name{n} {}
		const CommandID first, last;
		const mpt::ustring name;
	};
	std::vector<Range> commandRanges;
};


class CCustEdit: public QLineEdit
{
	Q_OBJECT
protected:
	COptionsKeyboard *m_pOptKeyDlg = nullptr;
	bool m_isFocussed = false;
	bool m_bypassed = false;

public:
	FlagSet<Modifiers> mod = ModNone;
	uint32 code = 0;

	void SetOwner(COptionsKeyboard &dlg) { m_pOptKeyDlg = &dlg; }
	void SetKey(FlagSet<Modifiers> mod, uint32 code);
	bool HasKey() const noexcept { return mod || code; }

	void Bypass(bool bypass) { m_bypassed = bypass; EnableWindow(bypass ? false : true); }
	bool IsBypassed() const { return m_bypassed; }
	
protected:
	bool eventFilter(QObject *watched, QEvent *e) override;
	
	void OnSetFocus(QWidget *pOldWnd);
	void OnKillFocus(QWidget *pNewWnd);
	intptr_t OnMidiMsg(uintptr_t, intptr_t);
};

class COptionsKeyboard: public PropertyPage
{
	Q_OBJECT
	friend class CCustEdit;

protected:
	QListWidget *m_lbnHotKeys = nullptr;
	CListCtrlEx m_lbnCommandKeys;
	QComboBox *m_cmbKeyChoice = nullptr;
	QComboBox *m_cmbCategory = nullptr;
	QAbstractButton *m_bKeyDown = nullptr, *m_bKeyHold = nullptr, *m_bKeyUp = nullptr;
	QAbstractButton *m_bnReset = nullptr;
	CCustEdit m_eCustHotKey, m_eFindHotKey;
	QLabel *m_warnIconCtl = nullptr, *m_warnText = nullptr;
	QLineEdit *m_eFind = nullptr;
	QLineEdit *m_eChordWaitTime = nullptr;
	QAbstractButton *m_restoreDefaultButton = nullptr;
	HICON m_infoIcon = nullptr, m_warnIcon = nullptr;
	
	mpt::ustring m_lastWarning;
	std::vector<CommandCategory> commandCategories;
	std::unique_ptr<CCommandSet> m_localCmdSet;
	mpt::PathString m_fullPathName;
	CommandID m_curCommand = kcNull;
	int m_curCategory = -1, m_curKeyChoice = -1;
	int m_lockCount = 0;
	bool m_forceUpdate = false;
	bool m_listGrouped = false;

public:
	COptionsKeyboard();

protected:
	bool OnInitDialog() override;
	void OnOK() override;
	bool OnSetActive() override;
	void DoDataExchange();

	void DefineCommandCategories();
	void ForceUpdateGUI(bool updateAllKeys = false);
	void UpdateNoteRepeatCheckbox();
	void InsertGroup(const mpt::uchar *title, int groupId);
	void UpdateShortcutList(int category = -1);
	void UpdateCategory();
	int GetCategoryFromCommandID(CommandID command) const;
	void OnCancelKeyChoice(const QWidget *source);
	void OnSetKeyChoice(const QWidget *source);

	void LockControls() { m_lockCount++; }
	void UnlockControls() { m_lockCount--; MPT_ASSERT(m_lockCount >= 0); }
	bool IsLocked() const noexcept { return m_lockCount != 0; }

	void EnableKeyChoice(bool enable);

	void UpdateWarning(mpt::ustring text = {}, bool notify = false);
	void ShowRestoreKeymapMenu();
	void RestoreKeymap(KeyboardPreset preset);

	intptr_t OnDPIChangedAfterParent(uintptr_t, intptr_t);
	void UpdateDialog();
	void OnKeyboardChanged();
	void OnKeyChoiceSelect();
	void OnCommandKeySelChanged(NMHDR *pNMHDR = nullptr, intptr_t *pResult = nullptr);
	void OnListenForKeysFromList();
	void OnCategorySelChanged();
	void OnSearchTermChanged();
	void OnChordWaitTimeChanged();
	void OnSettingsChanged() { SetModified(true); }
	void OnCheck() { OnSetKeyChoice(&m_eCustHotKey); };
	void OnToggleNotesRepeat();
	void OnListenForKeys();
	void OnDeleteKeyChoice();
	void OnRestoreKeyChoice();
	void OnLoad();
	void OnSave();
	void OnRestoreDefaultKeymap();
	void OnRestoreKeymapDropdown();
	void OnRestoreMPTKeymap() { RestoreKeymap(KeyboardPreset::MPT); }
	void OnRestoreITKeymap() { RestoreKeymap(KeyboardPreset::IT); }
	void OnRestoreFT2Keymap() { RestoreKeymap(KeyboardPreset::FT2); }
	void OnClearHotKey();
	void OnClearSearch();
	void OnEnableFindHotKey();
	void OnFindHotKey();
	void OnLButtonDblClk(uint32 flags, QPoint point);
	void OnDestroy();
};

OPENMPT_NAMESPACE_END
