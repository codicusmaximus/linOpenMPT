/*
 * view_com.h
 * ----------
 * Purpose: Song comments tab, lower panel.
 * Notes  : mfcless twin of upstream's mptrack/view_com.h.
 *          Draft from qt/tools/twin.py, not yet edited by hand.
 * Authors: Olivier Lapicque
 *          OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"

#include "CListCtrl.h"
#include "Globals.h"
#include "../soundlib/modcommand.h"

class QEvent;
class QFont;
class QObject;

OPENMPT_NAMESPACE_BEGIN

class CViewComments: public CModScrollView
{
	Q_OBJECT
public:
	CViewComments() = default;
	Q_OBJECT
protected:
	CModControlBar m_ToolBar;
	CListCtrlEx m_ItemList;
	QFont m_fixedFont;
	HFONT m_oldFont = nullptr;
	int m_nCurrentListId = 0, m_nListId = 0;
	ModCommand::NOTE m_lastNote = NOTE_NONE;
	CHANNELINDEX m_noteChannel = CHANNELINDEX_INVALID;
	INSTRUMENTINDEX m_noteInstr = INSTRUMENTINDEX_INVALID;
	bool m_editLabel = false;

public:
	void RecalcLayout();
	void UpdateButtonState();

public:
	void OnInitialUpdate() override;
	void OnDPIChanged() override;
	bool eventFilter(QObject *watched, QEvent *e) override;
	intptr_t OnModViewMsg(uintptr_t wParam, intptr_t lParam) override;
	void UpdateView(UpdateHint hint, QObject *pObject = nullptr) override;
protected:
	bool SwitchToList(int list);
	// cppcheck-suppress duplInheritedMember
	void OnDestroy();
	void OnSize(uint32 nType, int cx, int cy);
	void OnShowSamples();
	void OnShowInstruments();
	void OnShowPatterns();
	void OnEndLabelEdit(LPNMHDR pnmhdr, intptr_t *pLResult);
	void OnBeginLabelEdit(LPNMHDR pnmhdr, intptr_t *pLResult);
	void OnDblClickListItem();
	void OnRClickListItem();
	void OnCustomDrawList();
	void OnCopyNames();
	void OnPasteNames();
	intptr_t OnMidiMsg(uintptr_t midiData, intptr_t);
	intptr_t OnCustomKeyMsg(uintptr_t, intptr_t);
	void RegisterMessageMap() override;
};


OPENMPT_NAMESPACE_END
