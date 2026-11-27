/*
 * PlugNotFoundDlg.cpp
 * -------------------
 * Purpose: Dialog for handling missing plugins
 * Notes  : mfcless twin of upstream's mptrack/PlugNotFoundDlg.cpp.
 *          Draft from qt/tools/twin.py, not yet edited by hand.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "PlugNotFoundDlg.h"
#include "mfcless/Widgets.h"
#include "../soundlib/plugins/PluginManager.h"
#include "Mptrack.h"
#include "resource.h"

OPENMPT_NAMESPACE_BEGIN

void PlugNotFoundDialog::RegisterMessageMap()
{
	ResizableDialog::RegisterMessageMap();
	AddCommand(IDC_BUTTON_REMOVE, &PlugNotFoundDialog::OnRemove);
}


void PlugNotFoundDialog::DoDataExchange(DataExchange &dx)
{
	ResizableDialog::DoDataExchange(dx);
	ExchangeControl(dx, IDC_LIST1, m_List);
}


PlugNotFoundDialog::PlugNotFoundDialog(std::vector<VSTPluginLib *> &plugins, QWidget *parent)
	: ResizableDialog(IDD_MISSINGPLUGS, parent)
	, m_plugins(plugins)
{
}


bool PlugNotFoundDialog::OnInitDialog()
{
	ResizableDialog::OnInitDialog();

	// Initialize table
	const CListCtrlEx::Header headers[] =
	{
		{ UL_("Plugin"),		128, LVCFMT_LEFT },
		{ UL_("File Path"),	308, LVCFMT_LEFT },
	};
	m_List->SetHeaders(headers);
	m_List->SetExtendedStyle(m_List->GetExtendedStyle() | LVS_EX_FULLROWSELECT | LVS_EX_CHECKBOXES);

	for(const auto &plug : m_plugins)
	{
		const int insertAt = m_List->InsertItem(m_List->GetItemCount(), plug->libraryName.ToUnicode());
		if(insertAt == -1)
			continue;
		m_List->SetItemText(insertAt, 1, plug->dllPath.ToUnicode());
		m_List->SetItemDataPtr(insertAt, plug);
		m_List->SetCheck(insertAt);
	}
	m_List->setFocus();

	return true;
}


void PlugNotFoundDialog::OnRemove()
{
	const int plugs = m_List->GetItemCount();
	for(int i = 0; i < plugs; i++)
	{
		if(m_List->GetCheck(i))
		{
			theApp.GetPluginManager()->RemovePlugin(static_cast<VSTPluginLib *>(m_List->GetItemDataPtr(i)));
		}
	}
	OnOK();
}

OPENMPT_NAMESPACE_END
