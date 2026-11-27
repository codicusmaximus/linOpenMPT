/*
 * WelcomeDialog.cpp
 * -----------------
 * Purpose: "First run" OpenMPT welcome dialog
 * Notes  : mfcless twin of upstream's mptrack/WelcomeDialog.h.
 *          Hand-edited from the twin.py draft.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"

#include "DialogBase.h"
#include "../common/mptPathString.h"

class QWidget;

OPENMPT_NAMESPACE_BEGIN

class WelcomeDlg : public DialogBase
{
	Q_OBJECT
protected:
	mpt::PathString m_vstPath;

public:
	WelcomeDlg(QWidget *parent);

protected:
	bool OnInitDialog() override;
	void OnOK() override;
	void OnCancel() override;
	void PostNcDestroy() override { DialogBase::PostNcDestroy(); delete this; }

	void OnOptions();
	void OnScanPlugins();

	void RegisterMessageMap() override;
};

OPENMPT_NAMESPACE_END
