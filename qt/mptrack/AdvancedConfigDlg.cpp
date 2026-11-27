/*
 * AdvancedConfigDlg.cpp
 * ---------------------
 * Purpose: Implementation of the advanced settings dialog.
 * Notes  : mfcless twin of upstream's mptrack/AdvancedConfigDlg.cpp.
 *          Draft from qt/tools/twin.py, not yet edited by hand.
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
	// #ifdef _AFX_NO_MFC_CONTROLS_IN_DIALOGS
	// ON_NOTIFY(NM_CUSTOMDRAW, IDC_LIST1, &COptionsAdvanced::OnCustomDrawList): by hand (Qt event override)
	// #endif // _AFX_NO_MFC_CONTROLS_IN_DIALOGS
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


#ifndef _AFX_NO_MFC_CONTROLS_IN_DIALOGS
COptionsAdvanced::COptionsAdvanced() : CPropertyPage{IDD_OPTIONS_ADVANCED}, m_List{m_indexToPath} {}
#else   // _AFX_NO_MFC_CONTROLS_IN_DIALOGS
COptionsAdvanced::COptionsAdvanced() : CPropertyPage{IDD_OPTIONS_ADVANCED} {}
#endif  // !_AFX_NO_MFC_CONTROLS_IN_DIALOGS

COptionsAdvanced::~COptionsAdvanced() {}


bool COptionsAdvanced::PreTranslateMessage(mfcless::Message *msg)
{
	if(msg->message == mfcless::msg::KeyDown && msg->wParam == mfcless::vk::RETURN)
	{
		OnOptionDblClick(nullptr, nullptr);
		return true;
	}
	return false;
}


bool COptionsAdvanced::OnInitDialog()
{
	PropertyPage::OnInitDialog();

	m_List.SetExtendedStyle(m_List.GetExtendedStyle() | LVS_EX_GRIDLINES | LVS_EX_FULLROWSELECT);

	m_listGrouped = m_List.EnableGroupView();

	if(m_listGrouped)
	{
		static constexpr ListCtrl::Header headers[] =
		{
			{ UL_("Setting"), 170, LVCFMT_LEFT },
			{ UL_("Type"),    40,  LVCFMT_LEFT },
			{ UL_("Value"),   160, LVCFMT_LEFT },
			{ UL_("Default"), 76,  LVCFMT_LEFT },
		};
		m_List.SetHeaders(headers);
	} else
	{
		static constexpr ListCtrl::Header headers[] =
		{
			{ UL_("Setting"), 220, LVCFMT_LEFT },
			{ UL_("Type"),    40,  LVCFMT_LEFT },
			{ UL_("Value"),   120, LVCFMT_LEFT },
			{ UL_("Default"), 66,  LVCFMT_LEFT },
		};
		m_List.SetHeaders(headers);
	}

	ReInit();
	return true;
}


void COptionsAdvanced::ReInit()
{
	m_List.SetRedraw(false);
	m_List.DeleteAllItems();
	if(m_listGrouped)
	{
		ListView_RemoveAllGroups(m_List.m_hWnd);
		m_List.EnableGroupView();
	}
	m_List.SetItemCount(static_cast<int>(theApp.GetSettings().size()));

	m_indexToPath.clear();
	m_indexToPath.reserve(theApp.GetSettings().size());
	m_groups.clear();
	int numGroups = 0;

	mpt::ustring findStr = mpt::ToLowerCase(GetWindowTextUnicode(*GetDlgItem(IDC_EDIT1)));

	LVITEMW lvi;
	lvi.mask = LVIF_TEXT | LVIF_PARAM;
	lvi.mask |= (m_listGrouped ? LVIF_GROUPID : 0);
	lvi.iSubItem = 0;
	lvi.state = 0;
	lvi.stateMask = 0;
	lvi.cchTextMax = 0;
	lvi.iImage = 0;
	lvi.iIndent = 0;
	lvi.iGroupId = 0;

	int i = 0;
	for(const auto &[path, state] : theApp.GetSettings())
	{
		// In MPT_USTRING_MODE_WIDE mode,
		// this loop is heavily optimized to avoid as much string copies as possible
		// in order to perform ok-ish in debug builds.
		// MPT_USTRING_MODE_UTF8 is not optimized as we (currently) do not build in
		// this mode by default.
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
		lvi.iItem = i++;
		lvi.lParam = m_indexToPath.size();

		if(m_listGrouped)
		{

			auto gi = m_groups.find(section);
			if(gi == m_groups.end())
			{
				LVGROUP group{};
#if MPT_WINNT_AT_LEAST(MPT_WIN_VISTA)
				group.cbSize = LVGROUP_V5_SIZE;
#else
				group.cbSize = sizeof(group);
#endif
				group.mask = LVGF_HEADER | LVGF_GROUPID;
#if MPT_USTRING_MODE_WIDE
				group.pszHeader = const_cast<wchar_t *>(section.c_str());
#else
				const std::wstring wsection = mpt::ToWide(section);
				group.pszHeader = const_cast<wchar_t *>(wsection.c_str());
#endif
				group.cchHeader = 0;
				group.pszFooter = nullptr;
				group.cchFooter = 0;
				group.iGroupId = lvi.iGroupId = numGroups++;
				group.stateMask = 0;
				group.state = 0;
				group.uAlign = LVGA_HEADER_LEFT;
				ListView_InsertGroup(m_List.m_hWnd, -1, &group);
				ListView_SetGroupState(m_List.m_hWnd, group.iGroupId, LVGS_COLLAPSIBLE, LVGS_COLLAPSIBLE);
				m_groups.insert(std::make_pair(section, lvi.iGroupId));
			} else
			{
				lvi.iGroupId = gi->second;
			}

#if MPT_USTRING_MODE_WIDE
			lvi.pszText = const_cast<wchar_t *>(key.c_str());
#else
			const std::wstring wkey = mpt::ToWide(key);
			lvi.pszText = const_cast<wchar_t *>(wkey.c_str());
#endif
			index = static_cast<int>(m_List.SendMessage(LVM_INSERTITEMW, 0, (intptr_t)(&lvi)));

		} else
		{

			const mpt::ustring sectionAndKey = path.FormatAsString();
#if MPT_USTRING_MODE_WIDE
			lvi.pszText = const_cast<wchar_t *>(sectionAndKey.c_str());
#else
			const std::wstring wsectionAndKey = mpt::ToWide(sectionAndKey);
			lvi.pszText = const_cast<wchar_t *>(wsectionAndKey.c_str());
#endif
			index = static_cast<int>(m_List.SendMessage(LVM_INSERTITEMW, 0, (intptr_t)(&lvi)));

		}

#if MPT_USTRING_MODE_WIDE
		m_List.SetItemText(index, 1, value.FormatTypeAsString().c_str());
		m_List.SetItemText(index, 2, value.FormatValueAsString().c_str());
		m_List.SetItemText(index, 3, defaultValue.FormatValueAsString().c_str());
#else
		m_List.SetItemText(index, 1, mpt::ToUnicode(value.FormatTypeAsString()));
		m_List.SetItemText(index, 2, mpt::ToUnicode(value.FormatValueAsString()));
		m_List.SetItemText(index, 3, mpt::ToUnicode(defaultValue.FormatValueAsString()));
#endif
		m_indexToPath.push_back(path);
	}

	m_List.SetItemCount(i);
	m_List.SetRedraw(true);
	m_List.Invalidate(false);
}


void COptionsAdvanced::OnOK()
{
	CSoundFile::SetDefaultNoteNames();
	CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
	if (pMainFrm) pMainFrm->PostMessage(WM_MOD_INVALIDATEPATTERNS, HINT_MPTOPTIONS);
	PropertyPage::OnOK();
}


bool COptionsAdvanced::OnSetActive()
{
	ReInit();
	CMainFrame::m_nLastOptionsPage = OPTIONS_PAGE_ADVANCED;
	return PropertyPage::OnSetActive();
}


#ifndef _AFX_NO_MFC_CONTROLS_IN_DIALOGS


uint32 CAdvancedSettingsList::OnGetCellBkColor(int nRow, int /* nColumn */ )
{
	const bool isDefault = theApp.GetSettings().GetMap().find(m_indexToPath[GetItemData(nRow)])->second.IsDefault();
	uint32 defColor = GetBkColor();
	uint32 txtColor = GetTextColor();
	uint32 modColor = mfcless::Rgb(mfcless::RedOf(defColor) * 0.9 + mfcless::RedOf(txtColor) * 0.1, mfcless::GreenOf(defColor) * 0.9 + mfcless::GreenOf(txtColor) * 0.1, mfcless::BlueOf(defColor) * 0.9 + mfcless::BlueOf(txtColor) * 0.1);
	return isDefault ? defColor : modColor;
}


uint32 CAdvancedSettingsList::OnGetCellTextColor(int nRow, int nColumn)
{
	return CMFCListCtrlEx::OnGetCellTextColor(nRow, nColumn);
}


#else // _AFX_NO_MFC_CONTROLS_IN_DIALOGS


void COptionsAdvanced::OnCustomDrawList()
{
	NMLVCUSTOMDRAW *pLVCD = reinterpret_cast<NMLVCUSTOMDRAW*>(pNMHDR);
	CurrentNotify().result = CDRF_DODEFAULT;
	switch(pLVCD->nmcd.dwDrawStage)
	{
	case CDDS_PREPAINT:
		CurrentNotify().result = CDRF_NOTIFYITEMDRAW;
		break;
	case CDDS_ITEMPREPAINT:
		{
			const bool isDefault = theApp.GetSettings().GetMap().find(m_indexToPath[pLVCD->nmcd.lItemlParam])->second.IsDefault();
			uint32 defColor = m_List.GetBkColor();
			uint32 txtColor = m_List.GetTextColor();
			uint32 modColor = mfcless::Rgb(mfcless::RedOf(defColor) * 0.9 + mfcless::RedOf(txtColor) * 0.1, mfcless::GreenOf(defColor) * 0.9 + mfcless::GreenOf(txtColor) * 0.1, mfcless::BlueOf(defColor) * 0.9 + mfcless::BlueOf(txtColor) * 0.1);
			pLVCD->clrTextBk = isDefault ? defColor : modColor;
		}
		break;
	}
}


#endif // !_AFX_NO_MFC_CONTROLS_IN_DIALOGS


void COptionsAdvanced::OnOptionDblClick()
{
	const int index = m_List.GetSelectionMark();
	if(index < 0)
		return;
	const SettingPath path = m_indexToPath[m_List.GetItemData(index)];
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
#if MPT_USTRING_MODE_WIDE
	m_List.SetItemText(index, 2, val.FormatValueAsString().c_str());
#else
	m_List.SetItemText(index, 2, mpt::ToUnicode(val.FormatValueAsString()));
#endif
	m_List.SetSelectionMark(index);
	OnSettingsChanged();
}


void COptionsAdvanced::OnSaveNow()
{
	TrackerSettings::Instance().SaveSettings();
	theApp.GetSettings().Flush();
}


OPENMPT_NAMESPACE_END
