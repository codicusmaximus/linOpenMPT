/*
 * PlugNotFoundDlg.h
 * -----------------
 * Purpose: Dialog for handling missing plugins
 * Notes  : mfcless twin of upstream's mptrack/PlugNotFoundDlg.h.
 *          Hand-edited from the twin.py draft.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"

#include "ResizableDialog.h"
#include "CListCtrl.h"

class QWidget;

OPENMPT_NAMESPACE_BEGIN

struct VSTPluginLib;

class PlugNotFoundDialog : public ResizableDialog
{
	Q_OBJECT
	std::vector<VSTPluginLib *> &m_plugins;
	CListCtrlEx *m_List = nullptr;

public:
	PlugNotFoundDialog(std::vector<VSTPluginLib *> &plugins, QWidget *parent);

	void DoDataExchange(DataExchange &dx) override;
	bool OnInitDialog() override;

	void OnRemove();

	void RegisterMessageMap() override;;
};

OPENMPT_NAMESPACE_END
