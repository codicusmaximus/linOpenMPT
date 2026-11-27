/*
 * CloseMainDialog.cpp
 * -------------------
 * Purpose: Dialog showing a list of unsaved documents, with the ability to choose which documents should be saved or not.
 * Notes  : mfcless twin of upstream's mptrack/CloseMainDialog.cpp.
 *          Draft from qt/tools/twin.py, not yet edited by hand.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "CloseMainDialog.h"
#include "mfcless/Widgets.h"
#include "Moddoc.h"
#include "Mptrack.h"
#include "resource.h"


OPENMPT_NAMESPACE_BEGIN


void CloseMainDialog::RegisterMessageMap()
{
	ResizableDialog::RegisterMessageMap();
	AddCommand(IDC_BUTTON1, &CloseMainDialog::OnSaveAll);
	AddCommand(IDC_BUTTON2, &CloseMainDialog::OnSaveNone);
	AddCommand(IDC_CHECK1, &CloseMainDialog::OnSwitchFullPaths);
}


void CloseMainDialog::DoDataExchange(DataExchange &dx)
{
	ResizableDialog::DoDataExchange(dx);
	//{{AFX_DATA_MAP(DoDataExchange)
	ExchangeControl(dx, IDC_LIST1, m_List);
	//}}AFX_DATA_MAP
}


CloseMainDialog::CloseMainDialog() : ResizableDialog(IDD_CLOSEDOCUMENTS) {}


mpt::ustring CloseMainDialog::FormatTitle(const CModDoc &modDoc, bool fullPath)
{
	return MPT_UFORMAT("{} ({})")
		(mpt::ToUnicode(modDoc.GetSoundFile().GetCharsetInternal(), modDoc.GetSoundFile().GetTitle()),
		(!fullPath || modDoc.GetPathNameMpt().empty()) ? modDoc.GetTitle() : modDoc.GetPathNameMpt().ToUnicode());
}


bool CloseMainDialog::OnInitDialog()
{
	ResizableDialog::OnInitDialog();

	// Create list of unsaved documents
	m_List->clear();

	SetChecked(IDC_CHECK1, 1);

	m_List->setUpdatesEnabled(false);
	for(CModDoc *modDoc : theApp.GetOpenDocuments())
	{
		if(modDoc->IsModified())
		{
			int item = mfcless::AddItem(m_List, FormatTitle(*modDoc, true));
			m_List->SetItemDataPtr(item, modDoc);
			m_List->SetSel(item, true);
		}
	}
	m_List->setUpdatesEnabled(true);

	if(m_List->count() == 0)
	{
		// No modified documents...
		OnOK();
	}

	return true;
}


void CloseMainDialog::OnOK()
{
	const int count = m_List->count();
	for(int i = 0; i < count; i++)
	{
		CModDoc *modDoc = static_cast<CModDoc *>(m_List->GetItemDataPtr(i));
		MPT_ASSERT(modDoc != nullptr);
		if(m_List->GetSel(i))
		{
			modDoc->ActivateWindow();
			if(modDoc->DoFileSave() == false)
			{
				// If something went wrong, or if the user decided to cancel saving (when using "Save As"), we'll better not proceed...
				OnCancel();
				return;
			}
		} else
		{
			modDoc->SetModified(false);
		}
	}

	ResizableDialog::OnOK();
}


void CloseMainDialog::OnSaveAll()
{
	if(m_List->count() == 1)
		m_List->SetSel(0, true);	// SelItemRange can't select one item: https://jeffpar.github.io/kbarchive/kb/129/Q129428/
	else
		m_List->SelItemRange(true, 0, m_List->count() - 1);
	OnOK();
}


void CloseMainDialog::OnSaveNone()
{
	if(m_List->count() == 1)
		m_List->SetSel(0, false);	// SelItemRange can't select one item: https://jeffpar.github.io/kbarchive/kb/129/Q129428/
	else
		m_List->SelItemRange(false, 0, m_List->count() - 1);
	OnOK();
}


// Switch between full path / filename only display
void CloseMainDialog::OnSwitchFullPaths()
{
	const int count = m_List->count();
	const bool fullPath = (IsChecked(IDC_CHECK1) == 1);
	m_List->setUpdatesEnabled(false);
	for(int i = 0; i < count; i++)
	{
		CModDoc *modDoc = static_cast<CModDoc *>(m_List->GetItemDataPtr(i));
		int item = mfcless::InsertItem(m_List, i + 1, FormatTitle(*modDoc, fullPath));
		m_List->SetItemDataPtr(item, modDoc);
		m_List->SetSel(item, m_List->GetSel(i));
		m_List->DeleteString(i);
	}
	m_List->setUpdatesEnabled(true);
}

OPENMPT_NAMESPACE_END
