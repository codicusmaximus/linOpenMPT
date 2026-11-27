/*
 * GeneralConfigDlg.h
 * ------------------
 * Purpose: Implementation of the general settings dialog.
 * Notes  : mfcless twin of upstream's mptrack/GeneralConfigDlg.h.
 *          Draft from qt/tools/twin.py, not yet edited by hand.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "DialogBase.h"
#include "AccessibleControls.h"

class QComboBox;
class QLineEdit;
class QListWidget;

OPENMPT_NAMESPACE_BEGIN

class COptionsGeneral: public PropertyPage
{
	Q_OBJECT
protected:
	QLineEdit *m_defaultArtist = nullptr;
	QComboBox *m_defaultTemplate = nullptr, *m_defaultFormat = nullptr;
	QListWidget *m_CheckList = nullptr;
	AccessibleButton m_templateBrowseButton;

public:
	COptionsGeneral();

protected:
	bool OnInitDialog() override;
	void OnOK() override;
	bool OnSetActive() override;
	void DoDataExchange(DataExchange &dx) override;

	void OnOptionSelChanged();
	void OnSettingsChanged() { SetModified(true); }
	void OnBrowseTemplate();
	void OnDefaultTypeChanged();
	void OnTemplateChanged();

	void RegisterMessageMap() override;;
};

OPENMPT_NAMESPACE_END
