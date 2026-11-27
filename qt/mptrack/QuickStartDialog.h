/*
 * QuickStartDialog.h
 * ------------------
 * Purpose: Dialog to show inside the MDI client area when no modules are loaded.
 * Notes  : mfcless twin of upstream's mptrack/QuickStartDialog.h.
 *          Draft from qt/tools/twin.py, not yet edited by hand.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "mfcless/Keyboard.h"

#include "AccessibleControls.h"
#include "CListCtrl.h"
#include "ResizableDialog.h"
#include "../common/mptPathString.h"

class QLineEdit;
class QWidget;

OPENMPT_NAMESPACE_BEGIN

class QuickStartDlg : public ResizableDialog
{
	Q_OBJECT
public:
	QuickStartDlg(const std::vector<mpt::PathString> &templates, const std::vector<mpt::PathString> &examples, QWidget *parent);
	~QuickStartDlg() override { DestroyWindow(); }

	void UpdateHeight();

protected:
	void DoDataExchange(DataExchange &dx) override;
	bool OnInitDialog() override;
	void OnDPIChanged() override;
	bool PreTranslateMessage(mfcless::Message *pMsg) override;
	void OnOK() override;
	void OnCancel() override { DestroyWindow(); }

	void UpdateFileList(intptr_t highlight = intptr_t(-1), mpt::ustring filter = {});
	size_t GetItemIndex(int index) const { return static_cast<size_t>(m_list.GetItemData(index) & 0x00FF'FFFF); }
	int GetItemGroup(int index) const { return static_cast<int>(m_list.GetItemData(index) >> 24); }

	void OnNew();
	void OnOpen();
	void OnRemoveMRUItem();
	void OnRemoveAllMRUItems();
	void OnUpdateFilter();
	void OnOpenFile();
	void OnRightClickFile();
	void OnItemChanged();
	void OnDestroy();

	void RegisterMessageMap() override;

	CListCtrlEx *m_list = nullptr;
	QLineEdit *m_find = nullptr;
	AccessibleButton *m_newButton = nullptr, *m_openButton = nullptr;
	CBitmap m_bmpNew, m_bmpOpen;
	std::array<std::vector<mpt::PathString>, 3> m_paths;
	int m_prevDPI = 0;
	bool m_groupsEnabled = false;
};

OPENMPT_NAMESPACE_END
