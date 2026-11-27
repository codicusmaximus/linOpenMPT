/*
 * QuickStartDialog.cpp
 * --------------------
 * Purpose: Dialog to show inside the MDI client area when no modules are loaded.
 * Notes  : mfcless twin of upstream's mptrack/QuickStartDialog.cpp.
 *          Hand-edited from the twin.py draft.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "QuickStartDialog.h"
#include "mfcless/Widgets.h"

#include <QtCore/QRegularExpression>
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
	Create(IDD_QUICKSTART, parent);
	m_newButton->SetAccessibleText(UL_("New Module"));
	m_openButton->SetAccessibleText(UL_("Open Module"));

	m_groupsEnabled = m_list->EnableGroupView();
	m_list->setUpdatesEnabled(false);
	m_list->SetExtendedStyle(m_list->GetExtendedStyle() | mfcless::list::FullRowSelect);
	m_list->InsertColumn(0, UL_("File"), mfcless::list::AlignLeft);
	m_list->InsertColumn(1, UL_("Location"), mfcless::list::AlignLeft);
	
	const std::pair<const std::vector<mpt::PathString> &, const mpt::uchar *> PathGroups[] =
	{
		{TrackerSettings::Instance().mruFiles, UL_("Recent Files")},
		{templates, UL_("Templates")},
		{examples, UL_("Example Modules")},
	};
	static_assert(mpt::array_size<decltype(PathGroups)>::size == mpt::array_size<decltype(m_paths)>::size);
	for(size_t groupId = 0; groupId < std::size(PathGroups); groupId++)
	{
		if(m_groupsEnabled)
		{
			m_list->InsertGroup(static_cast<int>(groupId), PathGroups[groupId].second);
			if((TrackerSettings::Instance().quickStartGroupsCollapsed & (1 << groupId)) != 0)
			{
				m_list->SetGroupCollapsed(static_cast<int>(groupId), true);
			}
		}

		m_paths[groupId] = PathGroups[groupId].first;
	}
	UpdateFileList();
	m_list->SetColumnWidth(0, mfcless::list::AutoSize);
	m_list->SetColumnWidth(1, mfcless::list::AutoSizeUseHeader);
	m_list->setUpdatesEnabled(true);
	m_list->update();
	OnItemChanged();
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

	// mfcless: Qt scales the dialog with the screen; nothing to resize here.
	m_prevDPI = GetDPI();

	m_list->SetImageList(CMainFrame::GetMainFrame()->m_PatternIcons.GetIcons());
	m_list->SetColumnWidth(0, mfcless::list::AutoSize);
	m_list->SetColumnWidth(1, mfcless::list::AutoSizeUseHeader);

	std::unique_ptr<RawGDIDIB> bitmapNew, bitmapOpen;
	try
	{
		bitmapNew = LoadPixelImage(GetResource(IDB_NEW_BIG, UL_("PNG")), scaling);
		bitmapOpen = LoadPixelImage(GetResource(IDB_OPEN_BIG, UL_("PNG")), scaling);
	} catch(...)
	{
		return;
	}
	const QPixmap pixmapNew = QPixmap::fromImage(ToQImage(*bitmapNew)), pixmapOpen = QPixmap::fromImage(ToQImage(*bitmapOpen));
	m_newButton->setIcon(pixmapNew);
	m_newButton->setIconSize(pixmapNew.size());
	m_openButton->setIcon(pixmapOpen);
	m_openButton->setIconSize(pixmapOpen.size());
}


bool QuickStartDlg::PreTranslateMessage(mfcless::Message *pMsg)
{
	// Use up/down keys to navigate in list, even if search field is focussed. This also skips the group headers during navigation
	if(pMsg->message == mfcless::msg::KeyDown && (pMsg->wParam == mfcless::vk::UP || pMsg->wParam == mfcless::vk::DOWN) && mfcless::GetFocus() == m_find)
	{
		const int curSel = m_list->GetSelectionMark();
		const int selItem = std::clamp(curSel + (pMsg->wParam == mfcless::vk::UP ? -1 : 1), 0, m_list->GetItemCount() - 1);
		if(curSel != selItem)
		{
			if(curSel != -1)
				m_list->SetItemState(curSel, 0, mfcless::list::Selected);
			m_list->SetItemState(selItem, mfcless::list::Selected, mfcless::list::Selected);
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
	parentRect = mfcless::Rect(parentWidget() ? parentWidget()->rect() : QRect());
	m_list->GetItemRect(0, itemRect, mfcless::list::Bounds);
	viewRect.bottom += itemRect.Height() * 2;  // View height calculation seems to be a bit off and would still cause a vertical scrollbar to appear without this adjustment
	if(viewRect.bottom > listRect.bottom)
		windowRect.bottom += viewRect.bottom - listRect.bottom;
	const int maxHeight = std::max(HighDPISupport::ScalePixels(154, this), Util::muldiv(parentRect.bottom, 9, 10));
	LimitMax(windowRect.bottom, maxHeight);
	SetWindowPos(nullptr, 0, 0, windowRect.Width(), windowRect.Height(), mfcless::swp::NoZOrder | mfcless::swp::NoActivate | mfcless::swp::NoMove);
}


void QuickStartDlg::OnNew()
{
	theApp.NewDocument();
}


void QuickStartDlg::OnOpen()
{
	mfcless::SendCommand(CMainFrame::GetMainFrame(), ID_FILE_OPEN);
}


void QuickStartDlg::OnOK()
{
	OnOpenFile();
}


void QuickStartDlg::OnRemoveMRUItem()
{
	auto &mruFiles = TrackerSettings::Instance().mruFiles;
	int i = -1;
	while((i = m_list->GetNextItem(i, mfcless::list::NextSelected | mfcless::list::All)) != -1)
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
		highlight = mfcless::GetItemData(m_list, sel);
	m_list->DeleteAllItems();
	mpt::ustring filter;
	mfcless::GetWindowText(m_find, filter);
	UpdateFileList(highlight, filter);
	m_list->setUpdatesEnabled(true);
}


void QuickStartDlg::UpdateFileList(intptr_t highlight, mpt::ustring filter)
{
	const bool applyFilter = !filter.empty();
	if(applyFilter)
		filter = UL_("*") + filter + UL_("*");

	bool highlightFound = false;
	int itemId = -1;
	for(size_t groupId = 0; groupId < m_paths.size(); groupId++)
	{
		for(size_t i = 0; i < m_paths[groupId].size(); i++)
		{
			if(m_paths[groupId][i].empty() || (applyFilter && !QRegularExpression(QRegularExpression::wildcardToRegularExpression(mfcless::ToQString(filter), QRegularExpression::UnanchoredWildcardConversion), QRegularExpression::CaseInsensitiveOption).match(mfcless::ToQString(m_paths[groupId][i].ToUnicode())).hasMatch()))
				continue;
			const auto filename = m_paths[groupId][i].GetFilename().ToUnicode();
			const intptr_t lParam = static_cast<intptr_t>(i | (groupId << 24));
			m_list->InsertItem(++itemId, filename, TIMAGE_MODULE_FILE, m_groupsEnabled ? static_cast<int>(groupId) : -1, static_cast<uintptr_t>(lParam));
			m_list->SetItemText(itemId, 1, m_paths[groupId][i].GetDirectoryWithDrive().ToUnicode());
			if(lParam == highlight)
			{
				m_list->SetItemState(itemId, mfcless::list::Selected, mfcless::list::Selected);
				m_list->SetSelectionMark(itemId);
				highlightFound = true;
			}
		}
	}
	if(!highlightFound)
	{
		m_list->SetItemState(0, mfcless::list::Selected, mfcless::list::Selected);
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
	while((i = m_list->GetNextItem(i, mfcless::list::NextSelected | mfcless::list::All)) != -1)
	{
		const int group = GetItemGroup(i);
		const size_t index = GetItemIndex(i);
		files.push_back({std::move(m_paths[group][index]), i, group});
	}
	bool success = false;
	for(const auto &item : files)
	{
		if(item.group != 1)
			success |= (theApp.OpenDocumentFile(item.path) != nullptr);
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
	const mfcless::NotifyInfo &item = CurrentNotify();
	if(item.item < 0)
		return;
	const mfcless::Point point{item.point.x(), item.point.y()};
	// Can only remove MRU items
	if(GetItemGroup(item.item) != 0)
		return;
	mfcless::Menu menu;
	menu.CreatePopupMenu();
	menu.AppendMenu(mfcless::menu::String, ID_REMOVE, UL_("&Remove from Recent File List"));
	menu.AppendMenu(mfcless::menu::String, ID_REMOVE_ALL, UL_("&Clear Recent File List"));
	menu.TrackPopupMenu(mfcless::menu::LeftAlign | mfcless::menu::RightButton, point.x, point.y, this);
	menu.DestroyMenu();
}


void QuickStartDlg::OnItemChanged()
{
	EnableItem(IDOK, m_list->GetSelectedCount() != 0 ? true : false);
}


void QuickStartDlg::OnDestroy()
{
	if(m_destroyed)
		return;
	m_destroyed = true;
	if(m_groupsEnabled && m_list)
	{
		uint8 state = 0;
		for(size_t groupId = 0; groupId < m_paths.size(); groupId++)
		{
			if(m_list->IsGroupCollapsed(static_cast<int>(groupId)))
				state |= static_cast<uint8>(1 << groupId);
		}
		TrackerSettings::Instance().quickStartGroupsCollapsed = state;
	}
}

OPENMPT_NAMESPACE_END
