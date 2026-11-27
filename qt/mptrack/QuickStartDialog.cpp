/*
 * QuickStartDialog.cpp
 * --------------------
 * Purpose: Dialog to show inside the MDI client area when no modules are loaded.
 * Notes  : mfcless twin of upstream's mptrack/QuickStartDialog.cpp.
 *          Draft from qt/tools/twin.py, not yet edited by hand.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "QuickStartDialog.h"
#include "Image.h"
#include "ImageLists.h"
#include "Mainfrm.h"
#include "Mptrack.h"
#include "MPTrackUtil.h"
#include "resource.h"
#include "TrackerSettings.h"


OPENMPT_NAMESPACE_BEGIN

void QuickStartDlg::RegisterMessageMap()
{
	ResizableDialog::RegisterMessageMap();
	// ON_WM_DESTROY(): by hand (Qt event override)
	AddCommand(IDC_BUTTON1, &QuickStartDlg::OnNew);
	AddCommand(IDC_BUTTON2, &QuickStartDlg::OnOpen);
	AddCommand(ID_REMOVE, &QuickStartDlg::OnRemoveMRUItem);
	AddCommand(ID_REMOVE_ALL, &QuickStartDlg::OnRemoveAllMRUItems);
	AddNotify(IDC_EDIT1, mfcless::Notify::EditChange, &QuickStartDlg::OnUpdateFilter);
	AddNotify(IDC_LIST1, mfcless::Notify::DoubleClicked, &QuickStartDlg::OnOpenFile);
	AddNotify(IDC_LIST1, mfcless::Notify::RightClick, &QuickStartDlg::OnRightClickFile);
	AddNotify(IDC_LIST1, mfcless::Notify::ItemChanged, &QuickStartDlg::OnItemChanged);
}


void QuickStartDlg::DoDataExchange(DataExchange &dx)
{
	ResizableDialog::DoDataExchange(dx);
	ExchangeControl(dx, IDC_BUTTON1, m_newButton);
	ExchangeControl(dx, IDC_BUTTON2, m_openButton);
	ExchangeControl(dx, IDC_LIST1, m_list);
	ExchangeControl(dx, IDC_EDIT1, m_find);
}


QuickStartDlg::QuickStartDlg(const std::vector<mpt::PathString> &templates, const std::vector<mpt::PathString> &examples, QWidget *parent)
{
	m_newButton->SetAccessibleText(U_("New Module"));
	m_openButton->SetAccessibleText(U_("Open Module"));
	Create(IDD_QUICKSTART, parent);

	m_groupsEnabled = m_list->EnableGroupView();
	m_list->setUpdatesEnabled(false);
	m_list->SetExtendedStyle(m_list->GetExtendedStyle() | LVS_EX_FULLROWSELECT);
	m_list->InsertColumn(0, U_("File"), LVCFMT_LEFT);
	m_list->InsertColumn(1, U_("Location"), LVCFMT_LEFT);
	
	const std::pair<const std::vector<mpt::PathString> &, const mpt::uchar *> PathGroups[] =
	{
		{TrackerSettings::Instance().mruFiles, U_("Recent Files")},
		{templates, U_("Templates")},
		{examples, U_("Example Modules")},
	};
	static_assert(mpt::array_size<decltype(PathGroups)>::size == mpt::array_size<decltype(m_paths)>::size);
	for(size_t groupId = 0; groupId < std::size(PathGroups); groupId++)
	{
		if(m_groupsEnabled)
		{
			LVGROUP group{};
#if MPT_WINNT_AT_LEAST(MPT_WIN_VISTA)
			group.cbSize = LVGROUP_V5_SIZE;
#else
			group.cbSize = sizeof(group);
#endif
			group.mask = LVGF_HEADER | LVGF_GROUPID;
#if defined(UNICODE)
			group.pszHeader = const_cast<mpt::uchar *>(PathGroups[groupId].second);
#else
			std::wstring titlew = mpt::ToWide(mpt::ustring(PathGroups[groupId].second));
			group.pszHeader = const_cast<WCHAR *>(titlew.c_str());
#endif
			group.cchHeader = 0;
			group.pszFooter = nullptr;
			group.cchFooter = 0;
			group.iGroupId = static_cast<int>(groupId);
			group.stateMask = 0;
			group.state = 0;
			group.uAlign = LVGA_HEADER_LEFT;
			ListView_InsertGroup(m_list->m_hWnd, -1, &group);
			ListView_SetGroupState(m_list->m_hWnd, group.iGroupId, LVGS_COLLAPSIBLE, LVGS_COLLAPSIBLE);
			if((TrackerSettings::Instance().quickStartGroupsCollapsed & (1 << groupId)) != 0)
			{
				ListView_SetGroupState(m_list->m_hWnd, group.iGroupId, LVGS_COLLAPSED, LVGS_COLLAPSED);
			}
		}

		m_paths[groupId] = PathGroups[groupId].first;
	}
	UpdateFileList();
	m_list->SetColumnWidth(0, LVSCW_AUTOSIZE);
	m_list->SetColumnWidth(1, LVSCW_AUTOSIZE_USEHEADER);
	m_list->setUpdatesEnabled(true);
	m_list->update(false);
	OnItemChanged(nullptr, nullptr);
	UpdateHeight();
}


bool QuickStartDlg::OnInitDialog()
{
	ResizableDialog::OnInitDialog();
	OnDPIChanged();
	return true;
}


void QuickStartDlg::OnDPIChanged()
{
	const double scaling = GetDPI() / 96.0;

	if(m_prevDPI)
	{
		// We don't use this as a stand-alone dialog but rather embed it in the MDI child area, and it is not resized automatically.
		MPT_ASSERT(GetStyle() & WS_CHILD);
		mfcless::Rect windowRect;
		GetClientRect(windowRect);
		windowRect.right = Util::muldiv(windowRect.right, GetDPI(), m_prevDPI);
		windowRect.bottom = Util::muldiv(windowRect.bottom, GetDPI(), m_prevDPI);
		HighDPISupport::AdjustWindowRectEx(windowRect, GetStyle(), false, GetExStyle(), GetDPI());
		SetWindowPos(nullptr, 0, 0, windowRect.Width(), windowRect.Height(), SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOMOVE);
	}
	m_prevDPI = GetDPI();

	m_list->SetImageList(&CMainFrame::GetMainFrame()->m_PatternIcons, LVSIL_SMALL);
	m_list->SetColumnWidth(0, LVSCW_AUTOSIZE);
	m_list->SetColumnWidth(1, LVSCW_AUTOSIZE_USEHEADER);

	m_bmpNew.DeleteObject();
	m_bmpOpen.DeleteObject();

	std::unique_ptr<RawGDIDIB> bitmapNew, bitmapOpen;
	try
	{
		bitmapNew = LoadPixelImage(GetResource(MAKEINTRESOURCE(IDB_NEW_BIG), U_("PNG")), scaling);
		bitmapOpen = LoadPixelImage(GetResource(MAKEINTRESOURCE(IDB_OPEN_BIG), U_("PNG")), scaling);
	} catch(...)
	{
		return;
	}
	auto *dc = GetDC();
	CopyToCompatibleBitmap(m_bmpNew, *dc, *bitmapNew);
	CopyToCompatibleBitmap(m_bmpOpen, *dc, *bitmapOpen);
	ReleaseDC(dc);
	m_newButton->SetBitmap(m_bmpNew);
	m_openButton->SetBitmap(m_bmpOpen);
}


bool QuickStartDlg::PreTranslateMessage(mfcless::Message *pMsg)
{
	// Use up/down keys to navigate in list, even if search field is focussed. This also skips the group headers during navigation
	if(pMsg->message == mfcless::msg::KeyDown && (pMsg->wParam == mfcless::vk::UP || pMsg->wParam == mfcless::vk::DOWN) && GetFocus() == &m_find)
	{
		const int curSel = m_list->GetSelectionMark();
		const int selItem = std::clamp(curSel + (pMsg->wParam == mfcless::vk::UP ? -1 : 1), 0, m_list->GetItemCount() - 1);
		if(curSel != selItem)
		{
			if(curSel != -1)
				m_list->SetItemState(curSel, 0, LVIS_SELECTED);
			m_list->SetItemState(selItem, LVIS_SELECTED, LVIS_SELECTED);
			m_list->SetSelectionMark(selItem);
		}
		return true;
	}

	return ResizableDialog::PreTranslateMessage(pMsg);
}


void QuickStartDlg::UpdateHeight()
{
	// Try to make the view tall enough to view the entire list contents, but only up to 90% of the MDI client area
	mfcless::Rect listRect, viewRect;
	m_list->GetClientRect(listRect);
	m_list->GetViewRect(viewRect);

	mfcless::Rect windowRect, parentRect, itemRect;
	GetClientRect(windowRect);
	GetParent()->GetClientRect(parentRect);
	m_list->GetItemRect(0, itemRect, LVIR_BOUNDS);
	viewRect.bottom += itemRect.Height() * 2;  // View height calculation seems to be a bit off and would still cause a vertical scrollbar to appear without this adjustment
	if(viewRect.bottom > listRect.bottom)
		windowRect.bottom += viewRect.bottom - listRect.bottom;
	const int maxHeight = std::max(HighDPISupport::ScalePixels(154, m_hWnd), Util::muldiv(parentRect.bottom, 9, 10));
	LimitMax(windowRect.bottom, maxHeight);
	HighDPISupport::AdjustWindowRectEx(windowRect, GetStyle(), false, GetExStyle(), GetDPI());
	SetWindowPos(nullptr, 0, 0, windowRect.Width(), windowRect.Height(), SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOMOVE);
}


void QuickStartDlg::OnNew()
{
	theApp.NewDocument();
}


void QuickStartDlg::OnOpen()
{
	CMainFrame::GetMainFrame()->SendMessage(WM_COMMAND, ID_FILE_OPEN);
}


void QuickStartDlg::OnOK()
{
	OnOpenFile(nullptr, nullptr);
}


void QuickStartDlg::OnRemoveMRUItem()
{
	auto &mruFiles = TrackerSettings::Instance().mruFiles;
	int i = -1;
	while((i = m_list->GetNextItem(i, LVNI_SELECTED | LVNI_ALL)) != -1)
	{
		if(GetItemGroup(i) != 0)
			continue;
		auto &path = m_paths[0][GetItemIndex(i)];
		if(auto it = std::find(mruFiles.begin(), mruFiles.end(), path); it != mruFiles.end())
			mruFiles.erase(it);
		path = {};
		m_list->DeleteItem(i);
		i--;
	}
	CMainFrame::GetMainFrame()->UpdateMRUList();
}


void QuickStartDlg::OnRemoveAllMRUItems()
{
	TrackerSettings::Instance().mruFiles.clear();
	m_paths[0].clear();
	for(int i = m_list->GetItemCount() - 1; i >= 0; i--)
	{
		if(GetItemGroup(i) == 0)
			m_list->DeleteItem(i);
	}
	CMainFrame::GetMainFrame()->UpdateMRUList();
}


void QuickStartDlg::OnUpdateFilter()
{
	m_list->setUpdatesEnabled(false);
	intptr_t highlight = intptr_t(-1);
	if(int sel = m_list->GetSelectionMark(); sel != -1)
		highlight = m_list->GetItemData(sel);
	m_list->DeleteAllItems();
	mpt::ustring filter;
	m_find->GetWindowText(filter);
	UpdateFileList(highlight, filter);
	m_list->setUpdatesEnabled(true);
}


void QuickStartDlg::UpdateFileList(intptr_t highlight, mpt::ustring filter)
{
	const bool applyFilter = !filter.IsEmpty();
	if(applyFilter)
		filter = U_("*") + filter + U_("*");

	LVITEM lvi;
	lvi.mask = LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE;
	if(m_groupsEnabled)
		lvi.mask |= LVIF_GROUPID;
	lvi.iSubItem = 0;
	lvi.state = 0;
	lvi.stateMask = 0;
	lvi.cchTextMax = 0;
	lvi.iImage = TIMAGE_MODULE_FILE;
	lvi.iIndent = 0;

	bool highlightFound = false;
	int itemId = -1;
	for(size_t groupId = 0; groupId < m_paths.size(); groupId++)
	{
		lvi.iGroupId = static_cast<int>(groupId);
		for(size_t i = 0; i < m_paths[groupId].size(); i++)
		{
			if(m_paths[groupId][i].empty() || (applyFilter && !PathMatchSpec(m_paths[groupId][i].ToCString(), filter)))
				continue;
			const auto filename = m_paths[groupId][i].GetFilename().ToWide();
			lvi.iItem = ++itemId;
			lvi.lParam = static_cast<intptr_t>(i | (groupId << 24));
			lvi.pszText = const_cast<mpt::uchar *>(filename.c_str());
			m_list->InsertItem(&lvi);
			m_list->SetItemText(itemId, 1, m_paths[groupId][i].GetDirectoryWithDrive().ToCString());
			if(lvi.lParam == highlight)
			{
				m_list->SetItemState(itemId, LVIS_SELECTED, LVIS_SELECTED);
				m_list->SetSelectionMark(itemId);
				highlightFound = true;
			}
		}
	}
	if(!highlightFound)
	{
		m_list->SetItemState(0, LVIS_SELECTED, LVIS_SELECTED);
		m_list->SetSelectionMark(0);
	}
}


void QuickStartDlg::OnOpenFile()
{
	struct OpenItem
	{
		mpt::PathString path;  // Must create copy because dialog will get destroyed after successfully loading the first file
		int item = 0;
		int group = 0;
	};
	std::vector<OpenItem> files;
	int i = -1;
	while((i = m_list->GetNextItem(i, LVNI_SELECTED | LVNI_ALL)) != -1)
	{
		const int group = GetItemGroup(i);
		const size_t index = GetItemIndex(i);
		files.push_back({std::move(m_paths[group][index]), i, group});
	}
	bool success = false;
	for(const auto &item : files)
	{
		if(item.group != 1)
			success |= (theApp.OpenDocumentFile(item.path.ToCString()) != nullptr);
		else
			success |= (theApp.OpenTemplateFile(item.path) != nullptr);
	}

	if(!success)
	{
		// If at least one item managed to load, the dialog will now be destroyed, and there are no items to delete from the list
		for(auto it = files.rbegin(); it != files.rend(); it++)
		{
			m_list->DeleteItem(it->item);
		}
	}
}


void QuickStartDlg::OnRightClickFile()
{
	const NMITEMACTIVATE *item = reinterpret_cast<NMITEMACTIVATE *>(nmhdr);
	if(item->iItem < 0)
		return;
	mfcless::Point point = item->ptAction;
	m_list->ClientToScreen(&point);
	// Can only remove MRU items
	if(GetItemGroup(item->iItem) != 0)
		return;
	mfcless::Menu menu;
	menu.CreatePopupMenu();
	menu.AppendMenu(mfcless::menu::String, ID_REMOVE, U_("&Remove from Recent File List"));
	menu.AppendMenu(mfcless::menu::String, ID_REMOVE_ALL, U_("&Clear Recent File List"));
	menu.TrackPopupMenu(mfcless::menu::LeftAlign | mfcless::menu::RightButton, point.x, point.y, this);
	menu.DestroyMenu();
}


void QuickStartDlg::OnItemChanged()
{
	EnableItem(IDOK, m_list->GetSelectedCount() != 0 ? true : false);
}


void QuickStartDlg::OnDestroy()
{
	if(m_groupsEnabled)
	{
		uint8 state = 0;
		for(size_t groupId = 0; groupId < m_paths.size(); groupId++)
		{
			if((ListView_GetGroupState(m_list->m_hWnd, groupId, LVGS_COLLAPSED) & LVGS_COLLAPSED) != 0)
				state |= static_cast<uint8>(1 << groupId);
		}
		TrackerSettings::Instance().quickStartGroupsCollapsed = state;
	}
	ResizableDialog::OnDestroy();
}

OPENMPT_NAMESPACE_END
