/*
 * MoveFXSlotDialog.h
 * ------------------
 * Purpose: Implementationof OpenMPT's move plugin dialog.
 * Notes  : mfcless twin of upstream's mptrack/MoveFXSlotDialog.h.
 *          Draft from qt/tools/twin.py, not yet edited by hand.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "DialogBase.h"
#include "../soundlib/Snd_defs.h"

class QComboBox;
class QWidget;

OPENMPT_NAMESPACE_BEGIN

class CMoveFXSlotDialog : public DialogBase
{
	Q_OBJECT
protected:
	const std::vector<PLUGINDEX> &m_EmptySlots;
	mpt::ustring m_csPrompt, m_csTitle, m_csChain;
	size_t m_nToSlot;
	PLUGINDEX m_nDefaultSlot;
	bool moveChain;

	QComboBox *m_CbnEmptySlots = nullptr;

public:
	CMoveFXSlotDialog(QWidget *pParent, PLUGINDEX currentSlot, const std::vector<PLUGINDEX> &emptySlots, PLUGINDEX defaultIndex, bool clone, bool hasChain);
	PLUGINDEX GetSlot() const { return m_EmptySlots[m_nToSlot]; }
	size_t GetSlotIndex() const { return m_nToSlot; }
	bool DoMoveChain() const { return moveChain; }

protected:
	void DoDataExchange(DataExchange &dx) override;  // DDX/DDV support

	void OnOK() override;
	bool OnInitDialog() override;
};

OPENMPT_NAMESPACE_END
