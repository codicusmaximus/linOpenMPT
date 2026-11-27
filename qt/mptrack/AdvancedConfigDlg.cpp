/*
 * AdvancedConfigDlg.cpp
 * ---------------------
 * Purpose: Implementation of the advanced settings dialog.
 * Notes  : mfcless twin of upstream's mptrack/AdvancedConfigDlg.cpp.
 *          Hand-edited from the twin.py draft.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "AdvancedConfigDlg.h"
#include "mfcless/Widgets.h"
#include "dlg_misc.h"
#include "Mainfrm.h"
#include "Mptrack.h"
#include "resource.h"
#include "Settings.h"
#include "TrackerSettings.h"
#include "WindowMessages.h"

OPENMPT_NAMESPACE_BEGIN

void COptionsAdvanced::RegisterMessageMap()
{
	PropertyPage::RegisterMessageMap();
	AddNotify(IDC_LIST1, mfcless::Notify::DoubleClicked, &COptionsAdvanced::OnOptionDblClick);
	AddNotify(IDC_EDIT1, mfcless::Notify::EditChange, &COptionsAdvanced::OnFindStringChanged);
	AddCommand(IDC_BUTTON1, &COptionsAdvanced::OnSaveNow);
}

void COptionsAdvanced::DoDataExchange(DataExchange &dx)
{
	PropertyPage::DoDataExchange(dx);
	//{{AFX_DATA_MAP(COptionsAdvanced)
	ExchangeControl(dx, IDC_LIST1, m_List);
	//}}AFX_DATA_MAP
}


COptionsAdvanced::COptionsAdvanced() : PropertyPage{IDD_OPTIONS_ADVANCED} {}

COptionsAdvanced::~COptionsAdvanced() {}


bool COptionsAdvanced::PreTranslateMessage(mfcless::Message *msg)
{
	if(msg->message == mfcless::msg::KeyDown && msg->wParam == mfcless::vk::RETURN)
	{
		OnOptionDblClick();
		return true;
	}
	return false;
}


bool COptionsAdvanced::OnInitDialog()
{
	PropertyPage::OnInitDialog();

	m_List->SetIndexToPath(m_indexToPath);
	m_List->SetExtendedStyle(m_List->GetExtendedStyle() | mfcless::list::GridLines | mfcless::list::FullRowSelect);

	m_listGrouped = m_List->EnableGroupView();

	if(m_listGrouped)
	{
		static constexpr ListCtrl::Header headers[] =
		{
			{ UL_("Setting"), 170, mfcless::list::AlignLeft },
			{ UL_("Type"),    40,  mfcless::list::AlignLeft },
			{ UL_("Value"),   160, mfcless::list::AlignLeft },
			{ UL_("Default"), 76,  mfcless::list::AlignLeft },
		};
		m_List->SetHeaders(headers);
	} else
	{
		static constexpr ListCtrl::Header headers[] =
		{
			{ UL_("Setting"), 220, mfcless::list::AlignLeft },
			{ UL_("Type"),    40,  mfcless::list::AlignLeft },
			{ UL_("Value"),   120, mfcless::list::AlignLeft },
			{ UL_("Default"), 66,  mfcless::list::AlignLeft },
		};
		m_List->SetHeaders(headers);
	}

	ReInit();
	return true;
}


void COptionsAdvanced::ReInit()
{
	m_List->SetRedraw(false);
	m_List->DeleteAllItems();
	if(m_listGrouped)
	{
		m_List->RemoveAllGroups();
		m_List->EnableGroupView();
	}

	m_indexToPath.clear();
	m_indexToPath.reserve(theApp.GetSettings().size());
	m_groups.clear();
	int numGroups = 0;

	mpt::ustring findStr = mpt::ToLowerCase(GetWindowTextUnicode(GetDlgItem(IDC_EDIT1)));

	int i = 0;
	for(const auto &[path, state] : theApp.GetSettings())
	{
		const mpt::ustring &section = path.GetRefSection();
		const mpt::ustring &key = path.GetRefKey();
		const SettingValue &value = state.GetRefValue();
		const SettingValue &defaultValue = state.GetRefDefault();

		if(!findStr.empty())
		{
			mpt::ustring str = path.FormatAsString() + U_("=") + value.FormatValueAsString();
			str = mpt::ToLowerCase(str);
			if(str.find(findStr) == mpt::ustring::npos)
			{
				continue;
			}
		}

		int index;
		const uintptr_t data = m_indexToPath.size();

		if(m_listGrouped)
		{
			int groupId;
			auto gi = m_groups.find(section);
			if(gi == m_groups.end())
			{
				groupId = numGroups++;
				m_List->InsertGroup(groupId, section);
				m_groups.insert(std::make_pair(section, groupId));
			} else
			{
				groupId = gi->second;
			}
			index = m_List->InsertItem(i++, key, -1, groupId, data);
		} else
		{
			index = m_List->InsertItem(i++, path.FormatAsString(), -1, -1, data);
		}

		m_List->SetItemText(index, 1, value.FormatTypeAsString());
		m_List->SetItemText(index, 2, value.FormatValueAsString());
		m_List->SetItemText(index, 3, defaultValue.FormatValueAsString());
		m_indexToPath.push_back(path);
	}

	m_List->SetRedraw(true);
	m_List->update();
}


void COptionsAdvanced::OnOK()
{
	CSoundFile::SetDefaultNoteNames();
	CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
	if (pMainFrm) mfcless::PostModMessage(pMainFrm, WM_MOD_INVALIDATEPATTERNS, HINT_MPTOPTIONS);
	PropertyPage::OnOK();
}


bool COptionsAdvanced::OnSetActive()
{
	ReInit();
	CMainFrame::m_nLastOptionsPage = OPTIONS_PAGE_ADVANCED;
	return PropertyPage::OnSetActive();
}


uint32 CAdvancedSettingsList::OnGetCellBkColor(int nRow, int /* nColumn */ )
{
	if(!m_indexToPath)
		return CMFCListCtrlEx::OnGetCellBkColor(nRow, 0);
	const bool isDefault = theApp.GetSettings().GetMap().find((*m_indexToPath)[GetItemData(nRow)])->second.IsDefault();
	if(isDefault)
		return CMFCListCtrlEx::OnGetCellBkColor(nRow, 0);
	const QColor defColor = palette().color(QPalette::Base);
	const QColor txtColor = palette().color(QPalette::Text);
	return mfcless::Rgb(static_cast<uint8>(defColor.red() * 0.9 + txtColor.red() * 0.1), static_cast<uint8>(defColor.green() * 0.9 + txtColor.green() * 0.1), static_cast<uint8>(defColor.blue() * 0.9 + txtColor.blue() * 0.1));
}


uint32 CAdvancedSettingsList::OnGetCellTextColor(int nRow, int nColumn)
{
	return CMFCListCtrlEx::OnGetCellTextColor(nRow, nColumn);
}


void COptionsAdvanced::OnOptionDblClick()
{
	const int index = m_List->GetSelectionMark();
	if(index < 0)
		return;
	const SettingPath path = m_indexToPath[m_List->GetItemData(index)];
	SettingValue val = theApp.GetSettings().GetMap().find(path)->second;
	if(val.GetType() == SettingTypeBool)
	{
		val = !val.as<bool>();
	} else
	{
		CInputDlg inputDlg(this, UL_("Enter new value for ") + mpt::ToUnicode(path.FormatAsString()), mpt::ToUnicode(val.FormatValueAsString()));
		if(inputDlg.DoModal() != IDOK)
		{
			return;
		}
		val.SetFromString(inputDlg.resultAsString);
	}
	theApp.GetSettings().Write(path, val);
	m_List->SetItemText(index, 2, val.FormatValueAsString());
	m_List->SetSelectionMark(index);
	OnSettingsChanged();
}


void COptionsAdvanced::OnSaveNow()
{
	TrackerSettings::Instance().SaveSettings();
	theApp.GetSettings().Flush();
}


OPENMPT_NAMESPACE_END
