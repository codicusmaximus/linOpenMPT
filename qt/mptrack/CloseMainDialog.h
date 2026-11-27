/*
 * CloseMainDialog.h
 * -----------------
 * Purpose: Dialog showing a list of unsaved documents, with the ability to choose which documents should be saved or not.
 * Notes  : mfcless twin of upstream's mptrack/CloseMainDialog.h.
 *          Hand-edited from the twin.py draft.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "mfcless/Geometry.h"
#include "InputHandler.h"
#include "ResizableDialog.h"

class QListWidget;

OPENMPT_NAMESPACE_BEGIN

class CModDoc;

class CloseMainDialog: public ResizableDialog
{
	Q_OBJECT
protected:
	QListWidget *m_List = nullptr;
	mfcless::Point m_minSize;
	BypassInputHandler m_bih;

	static mpt::ustring FormatTitle(const CModDoc &modDoc, bool fullPath);

public:
	CloseMainDialog();

protected:
	void DoDataExchange(DataExchange &dx) override;
	bool OnInitDialog() override;
	void OnOK() override;

	void OnSaveAll();
	void OnSaveNone();
	void OnSwitchFullPaths();

	void RegisterMessageMap() override;
};

OPENMPT_NAMESPACE_END
