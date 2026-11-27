/*
 * AdvancedConfigDlg.h
 * -------------------
 * Purpose: Implementation of the advanced settings dialog.
 * Notes  : mfcless twin of upstream's mptrack/AdvancedConfigDlg.h.
 *          Draft from qt/tools/twin.py, not yet edited by hand.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "DialogBase.h"
#include "mfcless/Keyboard.h"

#include "CListCtrl.h"
#if MPT_USTRING_MODE_WIDE
#include <unordered_map>
#else
#include <map>
#endif

OPENMPT_NAMESPACE_BEGIN

class SettingPath;

#ifndef _AFX_NO_MFC_CONTROLS_IN_DIALOGS

class CAdvancedSettingsList : public CMFCListCtrlEx
{
	Q_OBJECT
private:
	const std::vector<SettingPath> *m_indexToPath = nullptr;
public:
	CAdvancedSettingsList(QWidget *parent = nullptr) : CMFCListCtrlEx(parent) {}
	void SetIndexToPath(const std::vector<SettingPath> &indexToPath) { m_indexToPath = &indexToPath; }
	uint32 OnGetCellBkColor(int nRow, int nColumn) override;
	uint32 OnGetCellTextColor(int nRow, int nColumn) override;
};

#endif // !_AFX_NO_MFC_CONTROLS_IN_DIALOGS


class COptionsAdvanced: public PropertyPage
{
	Q_OBJECT
#ifndef _AFX_NO_MFC_CONTROLS_IN_DIALOGS
	using ListCtrl = CAdvancedSettingsList;
#else  // _AFX_NO_MFC_CONTROLS_IN_DIALOGS
	using ListCtrl = CListCtrlEx;
#endif // !_AFX_NO_MFC_CONTROLS_IN_DIALOGS

protected:
	ListCtrl *m_List = nullptr;
#if MPT_USTRING_MODE_WIDE
	using GroupMap = std::unordered_map<mpt::ustring, int>;
#else
	using GroupMap = std::map<mpt::ustring, int>;
#endif
	std::vector<SettingPath> m_indexToPath;
	GroupMap m_groups;
	bool m_listGrouped = false;

public:
	COptionsAdvanced();
	~COptionsAdvanced();

protected:
	bool OnInitDialog() override;
	void OnOK() override;
	bool OnSetActive() override;
	void DoDataExchange(DataExchange &dx) override;
	bool PreTranslateMessage(mfcless::Message *msg) override;

	void OnOptionDblClick();
	void OnSettingsChanged() { SetModified(true); }
	void OnFindStringChanged() { ReInit(); }
	void OnSaveNow();

	void ReInit();

	void RegisterMessageMap() override;
};

OPENMPT_NAMESPACE_END
