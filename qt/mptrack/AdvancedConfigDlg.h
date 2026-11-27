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
#include <QtWidgets/QTreeWidget>

#include "CListCtrl.h"
#if MPT_USTRING_MODE_WIDE
#include <unordered_map>
#else
#include <map>
#endif

class QEvent;
class QObject;

OPENMPT_NAMESPACE_BEGIN

class SettingPath;

#ifndef _AFX_NO_MFC_CONTROLS_IN_DIALOGS

class CAdvancedSettingsList : public QTreeWidget
{
	Q_OBJECT
private:
	std::vector<SettingPath> & m_indexToPath;
public:
	CAdvancedSettingsList(std::vector<SettingPath> & indexToPath) : m_indexToPath(indexToPath) {}
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
	ListCtrl m_List;
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
	void DoDataExchange();
	bool eventFilter(QObject *watched, QEvent *e) override;

	void OnOptionDblClick();
	void OnSettingsChanged() { SetModified(true); }
	void OnFindStringChanged() { ReInit(); }
	void OnSaveNow();
#ifdef _AFX_NO_MFC_CONTROLS_IN_DIALOGS
	void OnCustomDrawList();
#endif // _AFX_NO_MFC_CONTROLS_IN_DIALOGS

	void ReInit();

	DECLARE_MESSAGE_MAP();
};

OPENMPT_NAMESPACE_END
