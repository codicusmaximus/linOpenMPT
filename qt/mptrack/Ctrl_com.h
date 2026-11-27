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
#include "Globals.h"

class QEvent;
class QLineEdit;
class QObject;

OPENMPT_NAMESPACE_BEGIN

class CCtrlComments final : public CModControlDlg
{
protected:
	QLineEdit *m_EditComments = nullptr;
	int charWidth = 0;
	bool m_Reformatting = false;

public:
	CCtrlComments(CModControlView &parent, CModDoc &document);
	Setting<int32> &GetSplitPosRef() override;
	bool OnInitDialog() override;
	void OnDPIChanged() override;
	void DoDataExchange();  // DDX/DDV support
	void RecalcLayout() override;
	void UpdateView(UpdateHint hint, QObject *pObj = nullptr) override;
	CRuntimeClass *GetAssociatedViewClass() override;
	void OnActivatePage(intptr_t) override;
	void OnDeactivatePage() override;
	bool eventFilter(QObject *watched, QEvent *e) override;
protected:
	void OnCommentsUpdated();
	void OnCommentsChanged();
};

OPENMPT_NAMESPACE_END
