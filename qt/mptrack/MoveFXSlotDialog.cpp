/*
 * MoveFXSlotDialog.h
 * ------------------
 * Purpose: Implementationof OpenMPT's move plugin dialog.
 * Notes  : mfcless twin of upstream's mptrack/MoveFXSlotDialog.cpp.
 *          Hand-edited from the twin.py draft.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "MoveFXSlotDialog.h"
#include "mfcless/Widgets.h"
#include "Mptrack.h"
#include "Reporting.h"
#include "resource.h"


OPENMPT_NAMESPACE_BEGIN


void CMoveFXSlotDialog::DoDataExchange(DataExchange &dx)
{
	DialogBase::DoDataExchange(dx);
	ExchangeControl(dx, IDC_COMBO1, m_CbnEmptySlots);
}


CMoveFXSlotDialog::CMoveFXSlotDialog(QWidget *pParent, PLUGINDEX currentSlot, const std::vector<PLUGINDEX> &emptySlots, PLUGINDEX defaultIndex, bool clone, bool hasChain) :
	DialogBase(IDD_MOVEFXSLOT, pParent),
	m_EmptySlots(emptySlots),
	m_nDefaultSlot(defaultIndex),
	moveChain(hasChain)
{
	if(clone)
	{
		m_csPrompt = mfcless::PrintF(UL_("Clone plugin in slot %d to the following empty slot:"), currentSlot + 1);
		m_csTitle = UL_("Clone To Slot...");
		m_csChain = UL_("&Clone follow-up plugin chain if possible");
	} else
	{
		m_csPrompt = mfcless::PrintF(UL_("Move plugin in slot %d to the following empty slot:"), currentSlot + 1);
		m_csTitle = UL_("Move To Slot...");
		m_csChain = UL_("&Move follow-up plugin chain if possible");
	}
}


bool CMoveFXSlotDialog::OnInitDialog()
{
	DialogBase::OnInitDialog();
	SetItemText(IDC_STATIC1, m_csPrompt);
	SetItemText(IDC_CHECK1, m_csChain);
	SetWindowText(m_csTitle);

	if(m_EmptySlots.empty())
	{
		Reporting::Error("No empty plugin slots are availabe.");
		OnCancel();
		return true;
	}

	mpt::ustring slotText;
	std::size_t defaultSlot = 0;
	bool foundDefault = false;
	for(size_t nSlot = 0; nSlot < m_EmptySlots.size(); nSlot++)
	{
		slotText = mfcless::PrintF(UL_("FX%d"), m_EmptySlots[nSlot] + 1);
		mfcless::SetItemData(m_CbnEmptySlots, mfcless::AddItem(m_CbnEmptySlots, slotText), nSlot);
		if(m_EmptySlots[nSlot] >= m_nDefaultSlot && !foundDefault)
		{
			defaultSlot = nSlot;
			foundDefault = true;
		}
	}
	m_CbnEmptySlots->setCurrentIndex(static_cast<int>(defaultSlot));

	EnableItem(IDC_CHECK1, moveChain ? true : false);
	SetChecked(IDC_CHECK1, moveChain ? 1 : 0);

	return true;
}


void CMoveFXSlotDialog::OnOK()
{
	m_nToSlot = mfcless::GetItemData(m_CbnEmptySlots, m_CbnEmptySlots->currentIndex());
	moveChain = IsChecked(IDC_CHECK1) != 0;
	DialogBase::OnOK();
}


OPENMPT_NAMESPACE_END
