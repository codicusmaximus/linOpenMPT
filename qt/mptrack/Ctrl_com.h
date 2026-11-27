/*
 * Ctrl_com.h
 * ----------
 * Purpose: Song comments tab, upper panel.
 * Notes  : mfcless twin of upstream's mptrack/Ctrl_com.h.
 *          Draft from qt/tools/twin.py, not yet edited by hand.
 * Authors: Olivier Lapicque
 *          OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "mfcless/Keyboard.h"
#include "DocumentView.h"
#include "Globals.h"

class QPlainTextEdit;
class QObject;

OPENMPT_NAMESPACE_BEGIN

class CCtrlComments final : public CModControlDlg
{
protected:
	QPlainTextEdit *m_EditComments = nullptr;
	int charWidth = 0;
	bool m_Reformatting = false;

public:
	CCtrlComments(CModControlView &parent, CModDoc &document);
	Setting<int32> &GetSplitPosRef() override;
	bool OnInitDialog() override;
	void OnDPIChanged() override;
	void DoDataExchange(DataExchange &dx) override;  // DDX/DDV support
	void RecalcLayout() override;
	void UpdateView(UpdateHint hint, QObject *pObj = nullptr) override;
	const mfcless::ViewClass *GetAssociatedViewClass() override;
	void OnActivatePage(intptr_t) override;
	void OnDeactivatePage() override;
	bool PreTranslateMessage(mfcless::Message *pMsg) override;
protected:
	void OnCommentsUpdated();
	void OnCommentsChanged();
	void RegisterMessageMap() override;
};

OPENMPT_NAMESPACE_END
