// Hand-edited from the twin.py draft.
/*
 * View_tre.cpp
 * ------------
 * Purpose: Tree view for managing open songs, sound files, file browser, ...
 * Notes  : There are two instances of this class, one for the upper half and one for the lower half of the tree view.
 *          The lower half is referred to the sample browser in the code, i.e. IsSampleBrowser() returns true for code
 *          running in the lower half.
 * Authors: Olivier Lapicque
 *          OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "View_tre.h"
#include "mfcless/Widgets.h"
#include "Childfrm.h"
#include "dlg_misc.h"
#include "Dlsbank.h"
#include "ExternalSamples.h"
#include "FileDialog.h"
#include "FolderScanner.h"
#include "Globals.h"
#include "HighDPISupport.h"
#include "ImageLists.h"
#include "InputHandler.h"
#include "LinkResolver.h"
#include "Mainfrm.h"
#include "Moddoc.h"
#include "MPTrackUtil.h"
#include "Reporting.h"
#include "resource.h"
#include "TrackerSettings.h"
#include "WindowMessages.h"
#include "../common/FileReader.h"
#include "../common/mptFileIO.h"
#include "../soundlib/MIDIEvents.h"
#include "../soundlib/mod_specifications.h"
#include "../soundlib/plugins/PlugInterface.h"
#include "mpt/fs/fs.hpp"
#include "mpt/io_file/inputfile.hpp"
#include "mpt/io_file_read/inputfile_filecursor.hpp"
#include "mpt/parse/parse.hpp"

#include <QtCore/QDateTime>
#include <QtCore/QCollator>
#include <QtCore/QDir>
#include <QtCore/QRegularExpression>
#include <QtCore/QFile>
#include <QtCore/QLocale>
#include <QtCore/QStorageInfo>
#include <QtCore/QMimeData>
#include <QtCore/QUrl>



OPENMPT_NAMESPACE_BEGIN


CSoundFile *CModTree::m_SongFile = nullptr;
CModTree::LibrarySortOrder CModTree::m_librarySort = LibrarySortOrder::Name;

namespace
{
// mfcless: WindowFromPoint, resolved to the window that handles messages
// (a scroll view's viewport resolves to the view).
QWidget *DropTargetFromPoint(QPoint screenPt)
{
	QWidget *widget = QApplication::widgetAt(screenPt);
	while(widget != nullptr && dynamic_cast<mfcless::MessageMap *>(widget) == nullptr)
		widget = widget->parentWidget();
	return widget;
}
}  // namespace

/////////////////////////////////////////////////////////////////////////////
// CModTreeDropTarget


bool CModTreeDropTarget::Register(CModTree *pWnd)
{
	m_pModTree = pWnd;
	return mfcless::DropTarget::Register(pWnd);
}


uint32 CModTreeDropTarget::OnDragEnter(QWidget *pWnd, const QMimeData *pDataObject, uint32 dwKeyState, mfcless::Point point)
{
	if((m_pModTree) && (m_pModTree == pWnd))
		return m_pModTree->OnDragEnter(pDataObject, dwKeyState, point);
	return mfcless::drop::None;
}


uint32 CModTreeDropTarget::OnDragOver(QWidget *pWnd, const QMimeData *pDataObject, uint32 dwKeyState, mfcless::Point point)
{
	if((m_pModTree) && (m_pModTree == pWnd))
		return m_pModTree->OnDragOver(pDataObject, dwKeyState, point);
	return mfcless::drop::None;
}


bool CModTreeDropTarget::OnDrop(QWidget *pWnd, const QMimeData *pDataObject, uint32 dropEffect, mfcless::Point point)
{
	if((m_pModTree) && (m_pModTree == pWnd))
		return m_pModTree->OnDrop(pDataObject, dropEffect, point);
	return false;
}


ModTreeDocInfo::ModTreeDocInfo(CModDoc &modDoc)
    : modDoc(modDoc)
{
	const CSoundFile &sndFile = modDoc.GetSoundFile();
	tiPatterns.resize(sndFile.Patterns.Size(), nullptr);
	tiOrders.resize(sndFile.Order.GetNumSequences());
	tiSequences.resize(sndFile.Order.GetNumSequences(), nullptr);
}


/////////////////////////////////////////////////////////////////////////////
// CModTree

void CModTree::RegisterMessageMap()
{
	mfcless::TreeCtrl::RegisterMessageMap();
	// ON_WM_DESTROY(): by hand (Qt event override)
	// ON_WM_KILLFOCUS() (mfcless::Window)
	// ON_WM_SETFOCUS() (mfcless::Window)
	// ON_WM_MOUSEMOVE() (mfcless::Window)
	// ON_WM_LBUTTONUP() (mfcless::Window)
	// ON_WM_RBUTTONUP() (mfcless::Window)
	// ON_WM_XBUTTONUP() (mfcless::Window)
	// ON_WM_DROPFILES() (mfcless::Window)
	// WM_DPICHANGED_AFTERPARENT: OnDPIChanged
	// mfcless: the tree's own notifications (ON_NOTIFY_REFLECT) come to the tree itself, with ID 0.
	// NM_RETURN is handled in PreTranslateMessage; TVN_GETDISPINFO is not needed, as items hold their text.
	mfcless::ConnectNotifications(this, 0, this);
	connect(this, &mfcless::TreeCtrl::labelEditorReady, this, [this](QTreeWidgetItem *, QLineEdit *editor) { OnLabelEditorReady(editor); });
	AddNotify(0, mfcless::Notify::DoubleClicked, &CModTree::OnItemDblClk);
	AddNotify(0, mfcless::Notify::RightClick, &CModTree::OnItemRightClick);
	AddNotify(0, mfcless::Notify::Click, &CModTree::OnItemLeftClick);
	AddNotify(0, mfcless::Notify::ItemExpanded, &CModTree::OnItemExpanded);
	AddNotify(0, mfcless::Notify::BeginDrag, &CModTree::OnBeginDrag);
	AddNotify(0, mfcless::Notify::BeginLabelEdit, &CModTree::OnBeginLabelEdit);
	AddNotify(0, mfcless::Notify::EndLabelEdit, &CModTree::OnEndLabelEdit);
	AddNotify(0, mfcless::Notify::SelChange, &CModTree::OnSelChanged);
	AddCommand(ID_MODTREE_REFRESH, &CModTree::OnRefreshTree);
	AddCommand(ID_MODTREE_EXECUTE, &CModTree::OnExecuteItem);
	AddCommand(ID_MODTREE_REMOVE, &CModTree::OnDeleteTreeItem);
	AddCommand(ID_MODTREE_PLAY, &CModTree::OnPlayTreeItem);
	AddCommand(ID_MODTREE_REFRESHINSTRLIB, &CModTree::OnRefreshInstrLib);
	AddCommand(ID_MODTREE_OPENITEM, &CModTree::OnOpenTreeItem);
	AddCommand(ID_MODTREE_MUTE, &CModTree::OnMuteTreeItem);
	AddCommand(ID_MODTREE_MUTE_ONLY_EFFECTS, &CModTree::OnMuteOnlyEffects);
	AddCommand(ID_MODTREE_SOLO, &CModTree::OnSoloTreeItem);
	AddCommand(ID_MODTREE_UNMUTEALL, &CModTree::OnUnmuteAllTreeItem);
	AddCommand(ID_MODTREE_DUPLICATE, &CModTree::OnDuplicateTreeItem);
	AddCommand(ID_MODTREE_INSERT, &CModTree::OnInsertTreeItem);
	AddCommand(ID_MODTREE_SWITCHTO, &CModTree::OnSwitchToTreeItem);
	AddCommand(ID_MODTREE_CLOSE, &CModTree::OnCloseItem);
	AddCommand(ID_MODTREE_SETPATH, &CModTree::OnSetItemPath);
	AddCommand(ID_MODTREE_SAVEITEM, &CModTree::OnSaveItem);
	AddCommand(ID_MODTREE_SAVEALL, &CModTree::OnSaveAll);
	AddCommand(ID_MODTREE_RELOADITEM, &CModTree::OnReloadItem);
	AddCommand(ID_MODTREE_RELOADALL, &CModTree::OnReloadAll);
	AddCommand(ID_MODTREE_FINDMISSING, &CModTree::OnFindMissing);
	AddCommand(ID_MODTREE_RENAME, &CModTree::OnRenameItem);
	AddCommand(ID_ADD_SOUNDBANK, &CModTree::OnAddDlsBank);
	AddCommand(ID_IMPORT_MIDILIB, &CModTree::OnImportMidiLib);
	AddCommand(ID_EXPORT_MIDILIB, &CModTree::OnExportMidiLib);
	AddCommand(ID_SOUNDBANK_PROPERTIES, &CModTree::OnSoundBankProperties);
	AddCommand(ID_MODTREE_SHOWDIRS, &CModTree::OnShowDirectories);
	AddCommand(ID_MODTREE_SHOWALLFILES, &CModTree::OnShowAllFiles);
	AddCommand(ID_MODTREE_SOUNDFILESONLY, &CModTree::OnShowSoundFiles);
	AddCommand(ID_MODTREE_GOTO_INSDIR, &CModTree::OnGotoInstrumentDir);
	AddCommand(ID_MODTREE_GOTO_SMPDIR, &CModTree::OnGotoSampleDir);
	AddCommand(ID_OPEN_LIBRARY_FILTER, &CModTree::OnOpenInstrumentLibraryFilter);
	AddCommand(ID_MODTREE_SORT_BY_NAME, &CModTree::OnSortByName);
	AddCommand(ID_MODTREE_SORT_BY_DATE, &CModTree::OnSortByDate);
	AddCommand(ID_MODTREE_SORT_BY_SIZE, &CModTree::OnSortBySize);
	AddMessage(WM_MOD_KEYCOMMAND, &CModTree::OnCustomKeyMsg);
	AddMessage(WM_MOD_MIDIMSG, &CModTree::OnMidiMsg);
}


/////////////////////////////////////////////////////////////////////////////
// CViewModTree construction/destruction

CModTree::CModTree(CModTree *pDataTree)
	: m_pDataTree(pDataTree)
{
	if(m_pDataTree != nullptr)
	{
		// Set up instrument library monitoring
		m_watchDirWatcher = new QFileSystemWatcher(this);
		m_watchDirTimer = new QTimer(this);
		m_watchDirTimer->setSingleShot(true);
		connect(m_watchDirWatcher, &QFileSystemWatcher::directoryChanged, this, [this] { OnWatchDirChanged(); });
		connect(m_watchDirTimer, &QTimer::timeout, this, [this]
		{
			m_watchDirLastRefresh = Util::GetTickCount64();
			mfcless::PostCommand(this, ID_MODTREE_REFRESHINSTRLIB);
		});
	}
	MemsetZero(m_tiMidi);
	MemsetZero(m_tiPerc);

}


CModTree::~CModTree()
{
	delete m_SongFile;
	m_SongFile = nullptr;

	if(m_pDataTree != nullptr)
	{
		// The watcher and its timer are children of the tree.
	}
}


void CModTree::Init()
{
	m_modExtensions = CSoundFile::GetSupportedExtensions(false);
	m_MediaFoundationExtensions = FileType(CSoundFile::GetMediaFoundationFileTypes()).GetExtensions();

	uint32 dwRemove = mfcless::tree::SingleExpand;
	uint32 dwAdd = mfcless::tree::EditLabels | mfcless::tree::HasLines | mfcless::tree::LinesAtRoot | mfcless::tree::HasButtons | mfcless::tree::ShowSelAlways;

	if(IsSampleBrowser())
	{
		dwRemove |= (mfcless::tree::HasLines | mfcless::tree::LinesAtRoot | mfcless::tree::HasButtons);
		dwAdd &= ~(mfcless::tree::HasLines | mfcless::tree::LinesAtRoot | mfcless::tree::HasButtons);
	}
	if(TrackerSettings::Instance().patternSetup & PatternSetup::SingleClickToExpand)
	{
		dwRemove &= ~mfcless::tree::SingleExpand;
		dwAdd |= mfcless::tree::SingleExpand;
		m_dwStatus |= TREESTATUS_SINGLEEXPAND;
	}
	ModifyStyle(dwRemove, dwAdd);
	DragAcceptFiles(true);

	if(!IsSampleBrowser())
	{
		const mpt::ustring curDir = mfcless::FromQString(QDir::currentPath());
		mpt::PathString dirs[] =
		{
			TrackerSettings::Instance().PathSamples.GetDefaultDir(),
			TrackerSettings::Instance().PathInstruments.GetDefaultDir(),
			TrackerSettings::Instance().PathSongs.GetDefaultDir(),
			mpt::PathString::FromUnicode(curDir)
		};
		for(auto &path : dirs)
		{
			m_InstrLibPath = std::move(path);
			if(!m_InstrLibPath.empty())
				break;
		}
		m_InstrLibPath = m_InstrLibPath.WithTrailingSlash();
		m_pDataTree->InsLibSetFullPath(m_InstrLibPath, mpt::PathString());
	}

	SetImageList(CMainFrame::GetMainFrame()->m_MiscIcons.GetIcons());
	if(!IsSampleBrowser())
	{
		// Create Midi Library
		m_hMidiLib = InsertItem(UL_("MIDI Library"), IMAGE_FOLDER, IMAGE_FOLDER, nullptr, mfcless::tree::Last);
		for(uint32 iMidGrp = 0; iMidGrp < 17; iMidGrp++)
		{
			InsertItem(mfcless::tree::MaskText | mfcless::tree::MaskImage | mfcless::tree::MaskSelectedImage | mfcless::tree::MaskParam, mpt::ToUnicode(mpt::Charset::ASCII, szMidiGroupNames[iMidGrp]), IMAGE_FOLDER, IMAGE_FOLDER, 0, 0, (MODITEM_HDR_MIDIGROUP << MIDILIB_SHIFT) | iMidGrp, m_hMidiLib, mfcless::tree::Last);
		}
	}
	m_hInsLib = InsertItem(UL_("Instrument Library"), IMAGE_FOLDER, IMAGE_FOLDER, nullptr, mfcless::tree::Last);
	RefreshMidiLibrary();
	RefreshDlsBanks();
	RefreshInstrumentLibrary();
	m_DropTarget.Register(this);
}


void CModTree::OnDestroy()
{
	if(CMainFrame::GetMainFrame()->GetMidiRecordWnd() == this)
	{
		CMainFrame::GetMainFrame()->SetMidiRecordWnd(nullptr);
	}
}


void CModTree::OnDPIChanged()
{
	SetImageList(CMainFrame::GetMainFrame()->m_MiscIcons.GetIcons());
	SetIndent(0);  // Set to minimum after going back from high-DPI to low-DPI
}


bool CModTree::PreTranslateMessage(mfcless::Message *pMsg)
{
	if(!pMsg)
		return true;

	if(m_doLabelEdit)
	{
		if(pMsg->message == mfcless::msg::KeyDown && (pMsg->wParam == mfcless::vk::RETURN || pMsg->wParam == mfcless::vk::ESCAPE))
		{
			EndEditLabelNow((pMsg->wParam == mfcless::vk::ESCAPE) ? true : false);
			return true;
		}
		return WindowBase::PreTranslateMessage(pMsg);
	}

	if(pMsg->message == mfcless::msg::KeyDown)
	{
		switch(pMsg->wParam)
		{
		case mfcless::vk::RETURN:
			// mfcless: NM_RETURN
			OnItemReturn();
			return true;
		case mfcless::vk::ESCAPE:
			mfcless::PostCommand(parentWidget(), ID_CLOSE_LIBRARY_FILTER);
			break;
		}
	} else if(pMsg->message == mfcless::msg::Char)
	{
		ModItem item = GetModItem(GetSelectedItem());
		switch(item.type)
		{
		case MODITEM_SAMPLE:
		case MODITEM_INSTRUMENT:
		case MODITEM_MIDIINSTRUMENT:
		case MODITEM_MIDIPERCUSSION:
		case MODITEM_INSLIB_SAMPLE:
		case MODITEM_INSLIB_INSTRUMENT:
		case MODITEM_DLSBANK_INSTRUMENT:
			// Avoid cycling through tree-view elements on key hold
			return true;
		default:
			break;
		}
	}

	// We handle keypresses before Windows has a chance to handle them (for alt etc..)
	if ((pMsg->message == mfcless::msg::SysKeyUp)   || (pMsg->message == mfcless::msg::KeyUp) ||
		(pMsg->message == mfcless::msg::SysKeyDown) || (pMsg->message == mfcless::msg::KeyDown))
	{
		CInputHandler *ih = CMainFrame::GetInputHandler();
		const auto event = ih->Translate(*pMsg);
		if(ih->KeyEvent(kCtxViewTree, event, this) != kcNull)
			return true;  // Mapped to a command, no need to pass message on.

		// For context menu shortcut
		if(ih->KeyEvent(kCtxAllContexts, event, this) != kcNull)
			return true;  // Mapped to a command, no need to pass message on.
	}
	return WindowBase::PreTranslateMessage(pMsg);
}


mpt::PathString CModTree::InsLibGetFullPath(QTreeWidgetItem * hItem) const
{
	mpt::PathString fullPath = m_InstrLibPath;
	fullPath = fullPath.WithTrailingSlash();
	return fullPath + mpt::PathString::FromUnicode(GetItemText(hItem));
}


bool CModTree::InsLibSetFullPath(const mpt::PathString &libPath, const mpt::PathString &songName)
{
	if(!songName.empty() && mpt::PathCompareNoCase(m_SongFileName, songName))
	{
		// Load module for previewing its instruments
		mpt::IO::InputFile f(libPath + songName, TrackerSettings::Instance().MiscCacheCompleteFileBeforeLoading);
		if(f.IsValid())
		{
			FileReader file = GetFileReader(f);
			if(file.IsValid())
			{
				if(m_SongFile != nullptr)
				{
					m_SongFile->Destroy();
				} else
				{
					m_SongFile = new(std::nothrow) CSoundFile;
				}
				if(m_SongFile != nullptr)
				{
					try
					{
						if(!m_SongFile->Create(file, CSoundFile::loadNoPatternOrPluginData, nullptr))
						{
							return false;
						}
					} catch(mpt::out_of_memory e)
					{
						mpt::delete_out_of_memory(e);
						return false;
					} catch(const std::exception &)
					{
						return false;
					}
					// Destroy some stuff that we're not going to use anyway.
					m_SongFile->Patterns.DestroyPatterns();
					m_SongFile->m_songMessage.clear();
				}
			}
		} else
		{
			return false;
		}
	}
	m_InstrLibPath = libPath;
	m_SongFileName = songName;
	return true;
}


bool CModTree::SetSoundFile(FileReader &file)
{
	std::unique_ptr<CSoundFile> sndFile;
	try
	{
		sndFile = std::make_unique<CSoundFile>();
		if(!sndFile->Create(file, CSoundFile::loadNoPatternOrPluginData))
		{
			return false;
		}
	} catch(mpt::out_of_memory e)
	{
		mpt::delete_out_of_memory(e);
		return false;
	} catch(const std::exception &)
	{
		return false;
	}

	if(m_SongFile != nullptr)
	{
		m_SongFile->Destroy();
		delete m_SongFile;
	}
	m_SongFile = sndFile.release();
	m_SongFile->Patterns.DestroyPatterns();
	m_SongFile->m_songMessage.clear();
	const mpt::PathString fileName = file.GetOptionalFileName().value_or(P_(""));
	m_InstrLibPath = fileName.GetDirectoryWithDrive();
	m_SongFileName = fileName.GetFilename();
	RefreshInstrumentLibrary();
	return true;
}


CModTree* CModTree::GetOtherView()
{
	if(this == CMainFrame::GetMainFrame()->GetUpperTreeview())
		return CMainFrame::GetMainFrame()->GetLowerTreeview();
	else
		return CMainFrame::GetMainFrame()->GetUpperTreeview();
}

void CModTree::OnOptionsChanged()
{
	uint32 dwRemove = mfcless::tree::SingleExpand, dwAdd = 0;
	m_dwStatus &= ~TREESTATUS_SINGLEEXPAND;
	if(TrackerSettings::Instance().patternSetup & PatternSetup::SingleClickToExpand)
	{
		dwRemove = 0;
		dwAdd = mfcless::tree::SingleExpand;
		m_dwStatus |= TREESTATUS_SINGLEEXPAND;
	}
	ModifyStyle(dwRemove, dwAdd);
}


void CModTree::AddDocument(CModDoc &modDoc)
{
	const auto [it, inserted] = m_docInfo.try_emplace(&modDoc, modDoc);
	if(!inserted)
		return;

	auto &info = it->second;
	UpdateView(info, UpdateHint().ModType());
	if(info.hSong)
	{
		Expand(info.hSong, mfcless::tree::ExpandIt);
		EnsureVisible(info.hSong);
		SelectItem(info.hSong);
	}
}


void CModTree::RemoveDocument(const CModDoc &modDoc)
{
	auto doc = m_docInfo.find(&modDoc);
	if(doc == m_docInfo.end())
		return;

	DeleteItem(doc->second.hSong);
	m_docInfo.erase(doc);
}


// Get CModDoc that is associated with a tree item
ModTreeDocInfo *CModTree::GetDocumentInfoFromItem(QTreeWidgetItem * hItem)
{
	hItem = GetParentRootItem(hItem);
	if(hItem != nullptr)
	{
		// Root item has moddoc pointer
		const auto doc = m_docInfo.find(reinterpret_cast<const CModDoc *>(GetItemData(hItem)));
		if(doc != m_docInfo.end() && hItem == doc->second.hSong)
		{
			return &doc->second;
		}
	}
	return nullptr;
}


// Get modtree doc information for a given CModDoc
ModTreeDocInfo *CModTree::GetDocumentInfoFromModDoc(CModDoc &modDoc)
{
	auto doc = m_docInfo.find(&modDoc);
	if(doc != m_docInfo.end())
		return &doc->second;
	else
		return nullptr;
}


size_t CModTree::GetDLSBankIndexFromItem(QTreeWidgetItem * hItem) const
{
	return static_cast<size_t>(std::distance(m_tiDLS.begin(), std::find(m_tiDLS.begin(), m_tiDLS.end(), GetParentRootItem(hItem))));
}


CDLSBank *CModTree::GetDLSBankFromItem(QTreeWidgetItem * hItem) const
{
	const auto bank = GetDLSBankIndexFromItem(hItem);
	if(bank < CTrackApp::gpDLSBanks.size())
		return CTrackApp::gpDLSBanks[bank].get();
	else
		return nullptr;
}


/////////////////////////////////////////////////////////////////////////////
// CViewModTree drawing

void CModTree::RefreshMidiLibrary()
{
	mpt::ustring s;
	mfcless::TreeItemInfo tvi;
	const MidiLibrary &midiLib = CTrackApp::GetMidiLibrary();

	if(IsSampleBrowser())
		return;
	// Midi Programs
	QTreeWidgetItem * parent = GetChildItem(m_hMidiLib);
	for(uint32 iMidi = 0; iMidi < 128; iMidi++)
	{
		int image = IMAGE_INSTRMUTE;
		s = mpt::ufmt::val(iMidi) + UL_(": ") + mpt::ToUnicode(mpt::Charset::ASCII, szMidiProgramNames[iMidi]);
		const intptr_t param = (MODITEM_MIDIINSTRUMENT << MIDILIB_SHIFT) | iMidi;
		if(midiLib[iMidi] && !midiLib[iMidi]->empty())
		{
			s += UL_(": ") + midiLib[iMidi]->GetFilename().ToUnicode();
			image = IMAGE_INSTRUMENTS;
		}
		if(!m_tiMidi[iMidi])
		{
			m_tiMidi[iMidi] = InsertItem(mfcless::tree::MaskText | mfcless::tree::MaskImage | mfcless::tree::MaskSelectedImage | mfcless::tree::MaskParam,
							s, image, image, 0, 0, param, parent, mfcless::tree::Last);
		} else
		{
			tvi.mask = mfcless::tree::MaskText | mfcless::tree::MaskImage | mfcless::tree::MaskSelectedImage | mfcless::tree::MaskParam;
			tvi.item = m_tiMidi[iMidi];
			tvi.image = tvi.selectedImage = image;
			GetItem(tvi);
			if(tvi.image != image || mfcless::Text(s).value != tvi.text)
			{
				SetItem(m_tiMidi[iMidi], mfcless::tree::MaskText | mfcless::tree::MaskImage | mfcless::tree::MaskSelectedImage | mfcless::tree::MaskParam,
					s, image, image, 0, 0, param);
			}
		}
		if((iMidi % 8u) == 7u)
		{
			parent = GetNextSiblingItem(parent);
		}
	}
	// Midi Percussions
	for(uint32 iPerc = 24; iPerc <= 84; iPerc++)
	{
		int image = IMAGE_NOSAMPLE;
		s = mpt::ToUnicode(CSoundFile::GetNoteName((ModCommand::NOTE)(iPerc + NOTE_MIN), CSoundFile::GetDefaultNoteNames()))
		    + UL_(": ") + mpt::ToUnicode(mpt::Charset::ASCII, szMidiPercussionNames[iPerc - 24]);
		const intptr_t param = (MODITEM_MIDIPERCUSSION << MIDILIB_SHIFT) | iPerc;
		if(midiLib[iPerc | 0x80] && !midiLib[iPerc | 0x80]->empty())
		{
			s += UL_(": ") + midiLib[iPerc | 0x80]->GetFilename().ToUnicode();
			image = IMAGE_SAMPLES;
		}
		if(!m_tiPerc[iPerc])
		{
			m_tiPerc[iPerc] = InsertItem(mfcless::tree::MaskText | mfcless::tree::MaskImage | mfcless::tree::MaskSelectedImage | mfcless::tree::MaskParam,
							s, image, image, 0, 0, param, parent, mfcless::tree::Last);
		} else
		{
			tvi.mask = mfcless::tree::MaskText | mfcless::tree::MaskImage | mfcless::tree::MaskSelectedImage | mfcless::tree::MaskParam;
			tvi.item = m_tiPerc[iPerc];
			tvi.image = tvi.selectedImage = image;
			GetItem(tvi);
			if(tvi.image != image || mfcless::Text(s).value != tvi.text)
			{
				SetItem(m_tiPerc[iPerc], mfcless::tree::MaskText | mfcless::tree::MaskImage | mfcless::tree::MaskSelectedImage,
							s, image, image, 0, 0, param);
			}
		}
	}
}


#if MPT_COMPILER_MSVC
// silence static analyzer false positive for FindFirstFile
#pragma warning(push)
#pragma warning(disable:6387) // 'void *' could be '0'
#endif // MPT_COMPILER_MSVC
void CModTree::RefreshDlsBanks()
{
	const mpt::Charset charset = mpt::Charset::Locale;

	if(IsSampleBrowser())
		return;

	if(m_tiDLS.size() < CTrackApp::gpDLSBanks.size())
		m_tiDLS.resize(CTrackApp::gpDLSBanks.size(), nullptr);

	LockRedraw();
	QTreeWidgetItem * hInsertAfter = m_hMidiLib;
	for(size_t iDls = 0; iDls < CTrackApp::gpDLSBanks.size(); iDls++)
	{
		if(!CTrackApp::gpDLSBanks[iDls])
		{
			// Was this bank removed?
			if(m_tiDLS[iDls])
			{
				DeleteItem(m_tiDLS[iDls]);
				m_tiDLS[iDls] = nullptr;
			}
			continue;
		}

		if(m_tiDLS[iDls] != nullptr)
		{
			hInsertAfter = m_tiDLS[iDls];
			continue;
		}

		// Add DLS file folder
		const CDLSBank &dlsBank = *CTrackApp::gpDLSBanks[iDls];
		if(m_tiDLS[iDls])
			DeleteChildren(m_tiDLS[iDls]);
		else
			m_tiDLS[iDls] = InsertItem(mfcless::tree::MaskText | mfcless::tree::MaskImage | mfcless::tree::MaskSelectedImage | mfcless::tree::MaskParam,
				dlsBank.GetFileName().GetFilename().ToUnicode(), IMAGE_FOLDER, IMAGE_FOLDER, 0, 0, iDls, nullptr, hInsertAfter);

		std::map<uint16, QTreeWidgetItem *> banks;
		QTreeWidgetItem * hDrums = nullptr;

		// Add Instruments (done backwards to improve performance as per https://devblogs.microsoft.com/oldnewthing/20111125-00/?p=9033)
		MPT_ASSERT(dlsBank.GetNumInstruments() <= 0x10000);
		for(auto iIns = dlsBank.GetNumInstruments(); iIns > 0;)
		{
			iIns--;
			const DLSINSTRUMENT *pDlsIns = dlsBank.GetInstrument(iIns);
			if(!pDlsIns)
				continue;

			mpt::ustring szName;
			const intptr_t lParamInstr = DlsItem::ToLPARAM(static_cast<uint16>(iIns), (pDlsIns->ulInstrument & 0x7F), false);
			if(pDlsIns->ulBank & F_INSTRUMENT_DRUMS)
			{
				// Drum Kit
				QTreeWidgetItem * hKit = nullptr;
				MPT_ASSERT(pDlsIns->Regions.size() <= 0x8000);
				for(auto region = static_cast<uint32>(pDlsIns->Regions.size()); region > 0;)
				{
					region--;
					if(pDlsIns->Regions[region].IsDummy())
						continue;

					uint32 keymin = pDlsIns->Regions[region].uKeyMin;
					uint32 keymax = pDlsIns->Regions[region].uKeyMax;

					const char *regionName = dlsBank.GetRegionName(iIns, region);
					if(regionName == nullptr || !regionName[0])
					{
						if(keymin >= 24 && keymin <= 84)
							regionName = szMidiPercussionNames[keymin - 24];
						else
							regionName = "";
					}

					if(!hKit)
					{
						if(!hDrums)
							hDrums = InsertItem(mfcless::tree::MaskText | mfcless::tree::MaskImage | mfcless::tree::MaskSelectedImage | mfcless::tree::MaskParam, UL_("Drum Kits"), IMAGE_FOLDER, IMAGE_FOLDER, 0, 0, DLS_DRUM_FOLDER_LPARAM, m_tiDLS[iDls], mfcless::tree::Last);

						szName = mfcless::PrintF(UL_("%u: %s"), pDlsIns->ulInstrument & 0x7F, mpt::ToUnicode(charset, pDlsIns->szName).c_str());
						hKit = InsertItem(mfcless::tree::MaskText | mfcless::tree::MaskImage | mfcless::tree::MaskSelectedImage | mfcless::tree::MaskParam, szName, IMAGE_FOLDER, IMAGE_FOLDER, 0, 0, lParamInstr, hDrums, mfcless::tree::First);
					}

					const auto regionNameStr = mpt::ToUnicode(charset, regionName);
					if(keymin >= keymax)
					{
						szName = mfcless::PrintF(UL_("%s%u: %s"),
							mpt::ToUnicode(CSoundFile::GetDefaultNoteName(keymin % 12)).c_str(),
							keymin / 12,
							regionNameStr.c_str());
					} else
					{
						szName = mfcless::PrintF(UL_("%s%u-%s%u: %s"),
							mpt::ToUnicode(CSoundFile::GetDefaultNoteName(keymin % 12)).c_str(),
							keymin / 12,
							mpt::ToUnicode(CSoundFile::GetDefaultNoteName(keymax % 12)).c_str(),
							keymax / 12,
							regionNameStr.c_str());
					}

					intptr_t lParam = DlsItem::ToLPARAM(static_cast<uint16>(iIns), static_cast<uint16>(region), true);
					InsertItem(mfcless::tree::MaskText | mfcless::tree::MaskImage | mfcless::tree::MaskSelectedImage | mfcless::tree::MaskParam,
							szName, IMAGE_INSTRUMENTS, IMAGE_INSTRUMENTS, 0, 0, lParam, hKit, mfcless::tree::First);
				}
			} else
			{
				// Melodic
				uint16 mbank = (pDlsIns->ulBank & 0x7F7F);
				auto hbank = banks.find(mbank);
				if(hbank == banks.end())
				{
					szName = mfcless::PrintF(mbank ? UL_("Melodic Bank %02d.%02d") : UL_("Melodic"), mbank >> 8, mbank & 0x7F);
					hbank = banks.insert(std::make_pair(mbank, nullptr)).first;
					hbank->second = InsertItem(mfcless::tree::MaskText | mfcless::tree::MaskImage | mfcless::tree::MaskSelectedImage,
						szName, IMAGE_FOLDER, IMAGE_FOLDER, 0, 0, 0,
						m_tiDLS[iDls], mfcless::tree::First);
				}

				const auto instrName = mpt::ToUnicode(charset, pDlsIns->szName);
				szName = mfcless::PrintF(UL_("%u: %s"), pDlsIns->ulInstrument & 0x7F, instrName.c_str());
				InsertItem(mfcless::tree::MaskText | mfcless::tree::MaskImage | mfcless::tree::MaskSelectedImage | mfcless::tree::MaskParam,
					szName, IMAGE_INSTRUMENTS, IMAGE_INSTRUMENTS, 0, 0, lParamInstr, hbank->second, mfcless::tree::First);
			}
		}
		hInsertAfter = m_tiDLS[iDls];
	}
	UnlockRedraw();
}
#if MPT_COMPILER_MSVC
#pragma warning(pop)
#endif // MPT_COMPILER_MSVC


void CModTree::RefreshInstrumentLibrary()
{
	LockRedraw();
	// Check if the currently selected item should be selected after refreshing
	mpt::ustring selectedName;
	if((IsSampleBrowser() || GetParentRootItem(GetSelectedItem()) == m_hInsLib)
	   && GetItemText(GetSampleBrowser()->m_hInsLib) == (m_SongFileName.empty() ? m_InstrLibPath : m_SongFileName).ToUnicode())
	{
		selectedName = GetItemText(GetSelectedItem());
	}
	if(!m_InstrLibHighlightPath.empty())
	{
		selectedName = m_InstrLibHighlightPath.ToUnicode();
	}
	m_InstrLibHighlightPath = {};
	FillInstrumentLibrary(selectedName.c_str());
	auto selectedItem = GetSelectedItem();
	if(selectedItem)
		EnsureVisible(selectedItem);
	UnlockRedraw();
	if(!IsSampleBrowser())
	{
		m_pDataTree->InsLibSetFullPath(m_InstrLibPath, m_SongFileName);
		m_pDataTree->RefreshInstrumentLibrary();
	}
}


void CModTree::UpdateView(ModTreeDocInfo &info, UpdateHint hint)
{
	mfcless::TreeItemInfo tvi;
	const FlagSet<HintType> hintType = hint.GetType();
	if(IsSampleBrowser() || hintType == HINT_NONE)
		return;

	const CModDoc &modDoc = info.modDoc;
	const CSoundFile &sndFile = modDoc.GetSoundFile();

	// Create headers
	const GeneralHint generalHint = hint.ToType<GeneralHint>();
	if(generalHint.GetType()[HINT_MODTYPE | HINT_MODGENERAL] || (!info.hSong))
	{
		// Module folder + sub folders
		mpt::ustring name = modDoc.GetPathNameMpt().GetFilename().ToUnicode();
		if(name.empty())
			name = mpt::SanitizePathComponent(modDoc.GetTitle());

		if(!info.hSong)
		{
			info.hSong = InsertItem(mfcless::tree::MaskText | mfcless::tree::MaskImage | mfcless::tree::MaskSelectedImage | mfcless::tree::MaskParam, name, IMAGE_FOLDER, IMAGE_FOLDER, 0, 0, reinterpret_cast<intptr_t>(&info.modDoc), nullptr, mfcless::tree::First);
			info.hOrders = InsertItem(UL_("Sequence"), IMAGE_FOLDER, IMAGE_FOLDER, info.hSong, mfcless::tree::Last);
			info.hPatterns = InsertItem(UL_("Patterns"), IMAGE_FOLDER, IMAGE_FOLDER, info.hSong, mfcless::tree::Last);
			info.hSamples = InsertItem(UL_("Samples"), IMAGE_FOLDER, IMAGE_FOLDER, info.hSong, mfcless::tree::Last);
		} else if(generalHint.GetType()[HINT_MODGENERAL | HINT_MODTYPE])
		{
			if(name != GetItemText(info.hSong))
			{
				SetItemText(info.hSong, name);
			}
		}
	}

	if(sndFile.GetModSpecifications().instrumentsMax > 0)
	{
		if(!info.hInstruments)
			info.hInstruments = InsertItem(UL_("Instruments"), IMAGE_FOLDER, IMAGE_FOLDER, info.hSong, info.hSamples);
	} else
	{
		if(info.hInstruments)
		{
			DeleteItem(info.hInstruments);
			info.hInstruments = NULL;
		}
	}
	if(!info.hComments)
		info.hComments = InsertItem(UL_("Comments"), IMAGE_COMMENTS, IMAGE_COMMENTS, info.hSong, mfcless::tree::Last);
	// Add effects
	const PluginHint pluginHint = hint.ToType<PluginHint>();
	if(pluginHint.GetType()[HINT_MODTYPE | HINT_PLUGINNAMES])
	{
		QTreeWidgetItem * hItem = info.hEffects ? GetChildItem(info.hEffects) : nullptr;
		PLUGINDEX firstPlug = 0, lastPlug = MAX_MIXPLUGINS - 1;
		if(pluginHint.GetPlugin() && hItem)
		{
			// Only update one specific plugin name
			firstPlug = lastPlug = pluginHint.GetPlugin() - 1;
			while(hItem && GetItemData(hItem) != firstPlug)
			{
				hItem = GetNextSiblingItem(hItem);
			}
		}
		for(PLUGINDEX i = firstPlug; i <= lastPlug; i++)
		{
			const SNDMIXPLUGIN &plugin = sndFile.m_MixPlugins[i];
			if(plugin.IsValidPlugin())
			{
				// Now we can be sure that we want to create this folder.
				if(!info.hEffects)
				{
					info.hEffects = InsertItem(UL_("Plugins"), IMAGE_FOLDER, IMAGE_FOLDER, info.hSong, info.hInstruments ? info.hInstruments : info.hSamples);
				}

				mpt::ustring s = MPT_UFORMAT("FX{}: {}")(i + 1, mpt::ToUnicode(plugin.GetName()));
				int nImage = IMAGE_NOPLUGIN;
				if(plugin.pMixPlugin != nullptr)
					nImage = (plugin.pMixPlugin->IsInstrument()) ? IMAGE_PLUGININSTRUMENT : IMAGE_EFFECTPLUGIN;

				if(hItem)
				{
					// Replace existing item
					tvi.mask = mfcless::tree::MaskText | mfcless::tree::MaskHandle | mfcless::tree::MaskImage | mfcless::tree::MaskSelectedImage | mfcless::tree::MaskParam;
					tvi.item = hItem;
					GetItem(tvi);
					if(tvi.image != nImage || tvi.data != i || mfcless::Text(s).value != tvi.text)
					{
						SetItem(hItem, mfcless::tree::MaskText | mfcless::tree::MaskHandle | mfcless::tree::MaskImage | mfcless::tree::MaskSelectedImage | mfcless::tree::MaskParam, s, nImage, nImage, 0, 0, i);
					}
					hItem = GetNextSiblingItem(hItem);
				} else
				{
					InsertItem(mfcless::tree::MaskText | mfcless::tree::MaskHandle | mfcless::tree::MaskImage | mfcless::tree::MaskSelectedImage | mfcless::tree::MaskParam, s, nImage, nImage, 0, 0, i, info.hEffects, mfcless::tree::Last);
				}
			}
		}
		if(!sndFile.m_loadedPlugins && info.hEffects)
		{
			DeleteItem(info.hEffects);
			info.hEffects = nullptr;
		} else if(!pluginHint.GetPlugin())
		{
			// Delete superfluous tree items
			while(hItem)
			{
				QTreeWidgetItem * nextItem = GetNextSiblingItem(hItem);
				DeleteItem(hItem);
				hItem = nextItem;
			}
		}
	}
	// Add Orders
	const PatternHint patternHint = hint.ToType<PatternHint>();
	const SequenceHint seqHint = hint.ToType<SequenceHint>();
	if(info.hOrders && (seqHint.GetType()[HINT_MODTYPE | HINT_MODSEQUENCE | HINT_SEQNAMES] || patternHint.GetType()[HINT_PATNAMES]))
	{
		const PATTERNINDEX nPat = patternHint.GetPattern();
		bool adjustParentNode = false;  // adjust sequence name of "Sequence" node?

		// (only one seq remaining || previously only one sequence): update parent item
		if((info.tiSequences.size() > 1 && sndFile.Order.GetNumSequences() == 1) || (info.tiSequences.size() == 1 && sndFile.Order.GetNumSequences() > 1))
		{
			for(auto &seq : info.tiOrders)
			{
				for(auto &ord : seq)
				{
					if(ord)
						DeleteItem(ord);
					ord = nullptr;
				}
			}
			for(auto &seq : info.tiSequences)
			{
				if(seq)
					DeleteItem(seq);
				seq = nullptr;
			}
			info.tiOrders.resize(sndFile.Order.GetNumSequences());
			info.tiSequences.resize(sndFile.Order.GetNumSequences(), nullptr);
			adjustParentNode = true;
		}

		// If there are too many sequences, delete them.
		for(size_t seq = sndFile.Order.GetNumSequences(); seq < info.tiSequences.size(); seq++) if (info.tiSequences[seq])
		{
			for(auto &ord : info.tiOrders[seq]) if (ord)
			{
				DeleteItem(ord); ord = nullptr;
			}
			DeleteItem(info.tiSequences[seq]); info.tiSequences[seq] = nullptr;
		}
		if (info.tiSequences.size() < sndFile.Order.GetNumSequences()) // Resize tiSequences if needed.
		{
			info.tiSequences.resize(sndFile.Order.GetNumSequences(), nullptr);
			info.tiOrders.resize(sndFile.Order.GetNumSequences());
		}

		QTreeWidgetItem * hAncestorNode = info.hOrders;

		SEQUENCEINDEX nSeqMin = 0, nSeqMax = sndFile.Order.GetNumSequences() - 1;
		SEQUENCEINDEX nHintParam = seqHint.GetSequence();
		if(seqHint.GetType()[HINT_SEQNAMES] && (nHintParam <= nSeqMax))
			nSeqMin = nSeqMax = nHintParam;

		// Adjust caption of the "Sequence" node (if only one sequence exists, it should be labeled with the sequence name)
		if((seqHint.GetType()[HINT_SEQNAMES] && sndFile.Order.GetNumSequences() == 1) || adjustParentNode)
		{
			mpt::ustring seqName = mpt::ToUnicode(sndFile.Order(0).GetName());
			if(seqName.empty() || sndFile.Order.GetNumSequences() > 1)
				seqName = UL_("Sequence");
			else
				seqName = UL_("Sequence: ") + seqName;
			SetItem(info.hOrders, mfcless::tree::MaskText, seqName, 0, 0, 0, 0, 0);
		}

		// go through all sequences
		mpt::ustring seqName;
		for(SEQUENCEINDEX seq = nSeqMin; seq <= nSeqMax; seq++)
		{
			if(sndFile.Order.GetNumSequences() > 1)
			{
				// more than one sequence -> add folder
				if(sndFile.Order(seq).GetName().empty())
				{
					seqName = MPT_UFORMAT("Sequence {}")(seq + 1);
				} else
				{
					seqName = MPT_UFORMAT("{}: ")(seq + 1);
					seqName += mpt::ToUnicode(sndFile.Order(seq).GetName());
				}

				uint32 state = (seq == sndFile.Order.GetCurrentSequenceIndex()) ? mfcless::tree::Bold : 0;

				if(info.tiSequences[seq] == NULL)
				{
					info.tiSequences[seq] = InsertItem(seqName, IMAGE_FOLDER, IMAGE_FOLDER, info.hOrders, mfcless::tree::Last);
				}
				// Update bold item
				tvi.mask = mfcless::tree::MaskText | mfcless::tree::MaskHandle | mfcless::tree::MaskState | mfcless::tree::MaskParam;
				tvi.state = 0;
				tvi.stateMask = mfcless::tree::Bold;
				tvi.item = info.tiSequences[seq];
				intptr_t param = (seq << SEQU_SHIFT) | ORDERINDEX_INVALID;
				GetItem(tvi);
				if(tvi.state != state || tvi.text != mfcless::Text(seqName).value || tvi.data != param)
					SetItem(info.tiSequences[seq], mfcless::tree::MaskText | mfcless::tree::MaskState | mfcless::tree::MaskParam, seqName, 0, 0, state, mfcless::tree::Bold, param);

				hAncestorNode = info.tiSequences[seq];
			}

			const ORDERINDEX ordLength = sndFile.Order(seq).GetLengthTailTrimmed();
			// If there are items past the new sequence length, delete them.
			for(size_t nOrd = ordLength; nOrd < info.tiOrders[seq].size(); nOrd++) if (info.tiOrders[seq][nOrd])
			{
				DeleteItem(info.tiOrders[seq][nOrd]); info.tiOrders[seq][nOrd] = NULL;
			}
			if (info.tiOrders[seq].size() < ordLength) // Resize tiOrders if needed.
				info.tiOrders[seq].resize(ordLength, nullptr);
			const bool patNamesOnly = patternHint.GetType()[HINT_PATNAMES];

			//if (hintFlagPart == HINT_PATNAMES) && (dwHintParam < sndFile.Order().GetLength())) imin = imax = dwHintParam;
			mpt::ustring patName;
			for(ORDERINDEX iOrd = 0; iOrd < ordLength; iOrd++)
			{
				mpt::ustring s;
				s[0] = 0;
				if(patNamesOnly && sndFile.Order(seq)[iOrd] != nPat)
					continue;
				uint32 state = (iOrd == info.ordSel && seq == info.seqSel) ? mfcless::tree::Bold : 0;
				if(sndFile.Order(seq)[iOrd] < sndFile.Patterns.Size())
				{
					patName = mpt::ToUnicode(sndFile.GetCharsetInternal(), sndFile.Patterns[sndFile.Order(seq)[iOrd]].GetName());
					const bool hexOrders = (TrackerSettings::Instance().patternSetup & PatternSetup::RowAndOrderNumbersHex);
					if(!patName.empty())
					{
						s = mfcless::PrintF(hexOrders ? UL_("[%02Xh] %u: ") : UL_("[%02u] %u: "),
							iOrd, sndFile.Order(seq)[iOrd]);
						s += patName;
					} else
					{
						s = mfcless::PrintF(hexOrders ? UL_("[%02Xh] Pattern %u") : UL_("[%02u] Pattern %u"),
							iOrd, sndFile.Order(seq)[iOrd]);
					}
				} else
				{
					if(sndFile.Order(seq)[iOrd] == PATTERNINDEX_SKIP)
					{
						// +++ Item
						s = mfcless::PrintF(UL_("[%02u] Skip"), iOrd);
					} else
					{
						// --- Item
						s = mfcless::PrintF(UL_("[%02u] Stop"), iOrd);
					}
				}

				intptr_t param = (seq << SEQU_SHIFT) | iOrd;
				if(info.tiOrders[seq][iOrd])
				{
					tvi.mask = mfcless::tree::MaskText | mfcless::tree::MaskHandle | mfcless::tree::MaskState;
					tvi.state = 0;
					tvi.stateMask = mfcless::tree::Bold;
					tvi.item = info.tiOrders[seq][iOrd];
					GetItem(tvi);
					if(tvi.state != state || (mfcless::Text(s).value != tvi.text))
						SetItem(info.tiOrders[seq][iOrd], mfcless::tree::MaskText | mfcless::tree::MaskState | mfcless::tree::MaskParam, s, 0, 0, state, mfcless::tree::Bold, param);
				} else
				{
					info.tiOrders[seq][iOrd] = InsertItem(mfcless::tree::MaskHandle | mfcless::tree::MaskText | mfcless::tree::MaskImage | mfcless::tree::MaskSelectedImage | mfcless::tree::MaskParam, s, IMAGE_PARTITION, IMAGE_PARTITION, 0, 0, param, hAncestorNode, mfcless::tree::Last);
				}
			}
		}
	}
	// Add Patterns
	if(info.hPatterns && patternHint.GetType()[HINT_MODTYPE | HINT_PATNAMES])
	{
		const PATTERNINDEX nPat = patternHint.GetPattern();
		PATTERNINDEX minPat = 0, maxPat = sndFile.Patterns.Size();
		if(patternHint.GetType()[HINT_PATNAMES] && nPat < sndFile.Patterns.Size())
		{
			minPat = nPat;
			maxPat = nPat + 1;
		}

		for(size_t pat = sndFile.Patterns.Size(); pat < info.tiPatterns.size(); pat++)
		{
			if(info.tiPatterns[pat])
				DeleteItem(info.tiPatterns[pat]);
		}
		info.tiPatterns.resize(sndFile.Patterns.Size(), nullptr);

		mpt::ustring patName;
		for(PATTERNINDEX pat = minPat; pat < maxPat; pat++)
		{
			mpt::ustring s;
			s[0] = 0;
			if(sndFile.Patterns.IsValidPat(pat))
			{
				patName = mpt::ToUnicode(sndFile.GetCharsetInternal(), sndFile.Patterns[pat].GetName());
				s = mfcless::PrintF(UL_("%u"), pat);
				if(!patName.empty())
				{
					s += UL_(": ");
					s += patName;
				}
				if(info.tiPatterns[pat])
				{
					tvi.mask = mfcless::tree::MaskText | mfcless::tree::MaskHandle;
					tvi.item = info.tiPatterns[pat];
					GetItem(tvi);
					if((mfcless::Text(s).value != tvi.text))
						SetItem(info.tiPatterns[pat], mfcless::tree::MaskText, s, 0, 0, 0, 0, 0);
				} else
				{
					info.tiPatterns[pat] = InsertItem(s, IMAGE_PATTERNS, IMAGE_PATTERNS, info.hPatterns, mfcless::tree::Last);
				}
				SetItemData(info.tiPatterns[pat], pat);
			} else if(pat < info.tiPatterns.size() && info.tiPatterns[pat])
			{
				DeleteItem(info.tiPatterns[pat]);
				info.tiPatterns[pat] = nullptr;
			}
		}
	}
	// Add Samples
	const SampleHint sampleHint = hint.ToType<SampleHint>();
	if(info.hSamples && sampleHint.GetType()[HINT_MODTYPE | HINT_SMPNAMES | HINT_SAMPLEINFO | HINT_SAMPLEDATA])
	{
		const SAMPLEINDEX hintSmp = sampleHint.GetSample();
		SAMPLEINDEX smin = 1, smax = MAX_SAMPLES - 1;
		if(sampleHint.GetType()[HINT_SMPNAMES | HINT_SAMPLEINFO | HINT_SAMPLEDATA] && hintSmp > 0 && hintSmp < MAX_SAMPLES)
		{
			smin = smax = hintSmp;
		}
		QTreeWidgetItem * hChild = GetNthChildItem(info.hSamples, smin - 1);
		for(SAMPLEINDEX nSmp = smin; nSmp <= smax; nSmp++)
		{
			mpt::ustring s;
			s[0] = 0;
			QTreeWidgetItem * hNextChild = GetNextSiblingItem(hChild);
			if(nSmp <= sndFile.GetNumSamples())
			{
				const ModSample &sample = sndFile.GetSample(nSmp);
				const bool sampleExists = (sample.HasSampleData());

				static constexpr int Images[] =
				{
					IMAGE_NOSAMPLE,  IMAGE_NOSAMPLE,        IMAGE_NOSAMPLE,
					IMAGE_SAMPLES,   IMAGE_SAMPLEACTIVE,    IMAGE_SAMPLEMUTE,
					IMAGE_EXTSAMPLE, IMAGE_EXTSAMPLEACTIVE, IMAGE_EXTSAMPLEMUTE,
					IMAGE_OPLINSTR,  IMAGE_OPLINSTRACTIVE,  IMAGE_OPLINSTRMUTE,
				};

				int image = 0;
				if(sampleExists)
					image = 3;
				if(sample.uFlags[SMP_KEEPONDISK])
					image = 6;
				if(sample.uFlags[CHN_ADLIB])
					image = 9;

				if(info.modDoc.IsSampleMuted(nSmp))
					image += 2;
				else if(info.samplesPlaying[nSmp])
					image++;

				if(sample.uFlags[SMP_KEEPONDISK] && !sampleExists)
					image = IMAGE_EXTSAMPLEMISSING;
				else
					image = Images[image];

				if(sndFile.GetType() == MOD_TYPE_MPT)
				{
					const mpt::uchar *status = UL_("");
					if(sample.uFlags[SMP_KEEPONDISK])
					{
						status = sampleExists ? UL_(" [external]") : UL_(" [MISSING]");
					}
					s = mfcless::PrintF(UL_("%3d: %s%s%s"), nSmp, sample.uFlags.test_all(SMP_MODIFIED | SMP_KEEPONDISK) ? UL_("* ") : UL_(""), mpt::ToUnicode(sndFile.GetCharsetInternal(), sndFile.m_szNames[nSmp]).c_str(), status);
				} else
				{
					s = mfcless::PrintF(UL_("%3d: %s"), nSmp, mpt::ToUnicode(sndFile.GetCharsetInternal(), sndFile.m_szNames[nSmp]).c_str());
				}

				if(!hChild)
				{
					hChild = InsertItem(mfcless::tree::MaskText | mfcless::tree::MaskImage | mfcless::tree::MaskSelectedImage | mfcless::tree::MaskParam, s, image, image, 0, 0, nSmp, info.hSamples, mfcless::tree::Last);
				} else
				{
					tvi.mask = mfcless::tree::MaskText | mfcless::tree::MaskHandle | mfcless::tree::MaskImage;
					tvi.item = hChild;
					tvi.image = tvi.selectedImage = image;
					GetItem(tvi);
#if MPT_COMPILER_MSVC
#pragma warning(push)
#pragma warning(disable:6054) // String 'stmp' might not be zero-terminated.
#endif // MPT_COMPILER_MSVC
					if(tvi.image != image || (mfcless::Text(s).value != tvi.text) || GetItemData(hChild) != nSmp)
#if MPT_COMPILER_MSVC
#pragma warning(pop)
#endif // MPT_COMPILER_MSVC
					{
						SetItem(hChild, mfcless::tree::MaskText | mfcless::tree::MaskImage | mfcless::tree::MaskSelectedImage | mfcless::tree::MaskParam, s, image, image, 0, 0, nSmp);
					}
				}
			} else if(hChild != nullptr)
			{
				DeleteItem(hChild);
			} else
			{
				break;
			}
			hChild = hNextChild;
		}
	}
	// Add Instruments
	const InstrumentHint instrHint = hint.ToType<InstrumentHint>();
	if(info.hInstruments && instrHint.GetType()[HINT_MODTYPE | HINT_INSNAMES | HINT_INSTRUMENT])
	{
		INSTRUMENTINDEX smin = 1, smax = MAX_INSTRUMENTS - 1;
		const INSTRUMENTINDEX hintIns = instrHint.GetInstrument();
		if(instrHint.GetType()[HINT_INSNAMES | HINT_INSTRUMENT] && hintIns > 0 && hintIns < MAX_INSTRUMENTS)
		{
			smin = smax = hintIns;
		}
		QTreeWidgetItem * hChild = GetNthChildItem(info.hInstruments, smin - 1);
		for(INSTRUMENTINDEX nIns = smin; nIns <= smax; nIns++)
		{
			mpt::ustring s;
			s[0] = 0;
			QTreeWidgetItem * hNextChild = GetNextSiblingItem(hChild);
			if(nIns <= sndFile.GetNumInstruments())
			{
				s = mfcless::PrintF(UL_("%3u: %s"), nIns, mpt::ToUnicode(sndFile.GetCharsetInternal(), sndFile.GetInstrumentName(nIns)).c_str());

				int nImage = IMAGE_INSTRUMENTS;
				if(info.instrumentsPlaying[nIns])
					nImage = IMAGE_INSTRACTIVE;
				if(!sndFile.Instruments[nIns] || info.modDoc.IsInstrumentMuted(nIns))
					nImage = IMAGE_INSTRMUTE;

				if(!hChild)
				{
					hChild = InsertItem(mfcless::tree::MaskText | mfcless::tree::MaskImage | mfcless::tree::MaskSelectedImage | mfcless::tree::MaskParam, s, nImage, nImage, 0, 0, nIns, info.hInstruments, mfcless::tree::Last);
				} else
				{
					tvi.mask = mfcless::tree::MaskText | mfcless::tree::MaskHandle | mfcless::tree::MaskImage;
					tvi.item = hChild;
					tvi.image = tvi.selectedImage = nImage;
					GetItem(tvi);
					if(tvi.image != nImage || (mfcless::Text(s).value != tvi.text) || GetItemData(hChild) != nIns)
					{
						SetItem(hChild, mfcless::tree::MaskText | mfcless::tree::MaskImage | mfcless::tree::MaskSelectedImage | mfcless::tree::MaskParam, s, nImage, nImage, 0, 0, nIns);
					}
				}
			} else if(hChild != nullptr)
			{
				DeleteItem(hChild);
			} else
			{
				break;
			}
			hChild = hNextChild;
		}
	}
}


CModTree::ModItem CModTree::GetModItem(QTreeWidgetItem * hItem)
{
	if(!hItem)
		return ModItem(MODITEM_NULL);
	// First, test root items
	if(hItem == m_hInsLib)
		return ModItem(MODITEM_HDR_INSTRUMENTLIB);
	if(hItem == m_hMidiLib)
		return ModItem(MODITEM_HDR_MIDILIB);

	// The immediate parent of the item (NULL if this item is on the root level of the tree)
	QTreeWidgetItem * hItemParent = GetParentItem(hItem);
	// Parent of the parent.
	QTreeWidgetItem * hItemParentParent = GetParentItem(hItemParent);
	// Get the root parent of the selected item, which can be the item itself.
	QTreeWidgetItem * hRootParent = hItem;
	if(!IsSampleBrowser())
	{
		hRootParent = GetParentRootItem(hItem);
	}

	uint32 itemData = static_cast<uint32>(GetItemData(hItem));
	uint32 rootItemData = static_cast<uint32>(GetItemData(hRootParent));

	// Midi Library
	if(hRootParent == m_hMidiLib && hRootParent != hItem && !IsSampleBrowser())
	{
		return ModItem(static_cast<ModItemType>(itemData >> MIDILIB_SHIFT), itemData & MIDILIB_MASK);
	}
	// Instrument Library
	if(hRootParent == m_hInsLib || (IsSampleBrowser() && hItem != m_hInsLib))
	{
		mfcless::TreeItemInfo tvi;
		tvi.mask = mfcless::tree::MaskImage | mfcless::tree::MaskHandle;
		tvi.item = hItem;
		tvi.image = 0;
		if(GetItem(tvi))
		{
			switch(tvi.image)
			{
			case IMAGE_SAMPLES:
			case IMAGE_OPLINSTR:
				// Sample
				return ModItem(MODITEM_INSLIB_SAMPLE);
			case IMAGE_INSTRUMENTS:
				// Instrument
				return ModItem(MODITEM_INSLIB_INSTRUMENT);
			case IMAGE_FOLDERSONG:
				// Song
				return ModItem(MODITEM_INSLIB_SONG);
			default:
				return ModItem(MODITEM_INSLIB_FOLDER);
			}
		}
		return ModItem(MODITEM_NULL);
	}
	if(IsSampleBrowser())
		return ModItem(MODITEM_NULL);
	// Songs
	if(auto info = GetDocumentInfoFromItem(hRootParent); info != nullptr)
	{
		m_selectedDoc = &info->modDoc;

		if(hItem == info->hSong)
			return ModItem(MODITEM_HDR_SONG);
		if(hRootParent == info->hSong)
		{
			if(hItem == info->hPatterns)
				return ModItem(MODITEM_HDR_PATTERNS);
			if(hItem == info->hOrders)
				return ModItem(MODITEM_HDR_ORDERS);
			if(hItem == info->hSamples)
				return ModItem(MODITEM_HDR_SAMPLES);
			if(hItem == info->hInstruments)
				return ModItem(MODITEM_HDR_INSTRUMENTS);
			if(hItem == info->hEffects)
				return ModItem(MODITEM_HDR_EFFECTS);
			if(hItem == info->hComments)
				return ModItem(MODITEM_COMMENTS);
			// Order List or Sequence item?
			if((hItemParent == info->hOrders) || (hItemParentParent == info->hOrders))
			{
				const auto ord = static_cast<ORDERINDEX>(itemData & SEQU_MASK);
				const auto seq = static_cast<SEQUENCEINDEX>(itemData >> SEQU_SHIFT);
				if(ord == ORDERINDEX_INVALID)
					return ModItem(MODITEM_SEQUENCE, seq);
				else
					return ModItem(MODITEM_ORDER, ord, seq);
			}

			ModItem modItem(MODITEM_NULL, itemData);
			if(hItemParent == info->hPatterns)
			{
				// Pattern
				modItem.type = MODITEM_PATTERN;
			} else if(hItemParent == info->hSamples)
			{
				// Sample
				modItem.type = MODITEM_SAMPLE;
			} else if(hItemParent == info->hInstruments)
			{
				// Instrument
				modItem.type = MODITEM_INSTRUMENT;
			} else if(hItemParent == info->hEffects)
			{
				// Effect
				modItem.type = MODITEM_EFFECT;
			}
			return modItem;
		}
	}

	// DLS banks
	if(itemData < m_tiDLS.size() && hItem == m_tiDLS[itemData])
		return ModItem(MODITEM_DLSBANK_FOLDER, itemData);

	// DLS Instruments
	if(hRootParent != nullptr)
	{
		if(rootItemData < m_tiDLS.size() && m_tiDLS[rootItemData] == hRootParent)
		{
			int image = 0, selImage = 0;
			const bool isFolder = GetItemImage(hItem, image, selImage) && (image == IMAGE_FOLDER || image == IMAGE_OPENFOLDER);
			if(!isFolder || GetItemData(hItemParent) == DLS_DRUM_FOLDER_LPARAM)
				return DlsItem::FromLPARAM(itemData);
		}
	}
	return ModItem(MODITEM_NULL);
}


bool CModTree::ExecuteItem(QTreeWidgetItem * hItem)
{
	if(hItem)
	{
		const ModItem modItem = GetModItem(hItem);
		uint32 modItemID = modItem.val1;
		CModDoc *modDoc = m_docInfo.count(m_selectedDoc) ? m_selectedDoc : nullptr;

		switch(modItem.type)
		{
		case MODITEM_COMMENTS:
			if(modDoc)
				modDoc->ActivateView(IDD_CONTROL_COMMENTS, 0);
			return true;

		case MODITEM_SEQUENCE:
			if(modDoc
				&& modItemID < modDoc->GetSoundFile().Order.GetNumSequences()
				&& !modDoc->GetSoundFile().Order(static_cast<SEQUENCEINDEX>(modItemID)).GetLengthTailTrimmed())
				modDoc->ActivateView(IDD_CONTROL_PATTERNS, (modItemID << SEQU_SHIFT) | SEQU_INDICATOR);
			return true;

		case MODITEM_ORDER:
			if(modDoc)
				modDoc->ActivateView(IDD_CONTROL_PATTERNS, modItemID | (uint32(modItem.val2) << SEQU_SHIFT) | SEQU_INDICATOR);
			return true;

		case MODITEM_PATTERN:
			if(modDoc)
				modDoc->ActivateView(IDD_CONTROL_PATTERNS, modItemID);
			return true;

		case MODITEM_SAMPLE:
			if(modDoc)
				modDoc->ActivateView(IDD_CONTROL_SAMPLES, modItemID);
			return true;

		case MODITEM_INSTRUMENT:
			if(modDoc)
				modDoc->ActivateView(IDD_CONTROL_INSTRUMENTS, modItemID);
			return true;

		case MODITEM_MIDIPERCUSSION:
			modItemID |= 0x80;
			[[fallthrough]];
		case MODITEM_MIDIINSTRUMENT:
			OpenMidiInstrument(modItemID);
			return true;

		case MODITEM_EFFECT:
		case MODITEM_INSLIB_SAMPLE:
		case MODITEM_INSLIB_INSTRUMENT:
			PlayItem(hItem, NOTE_MIDDLEC);
			return true;

		case MODITEM_INSLIB_SONG:
		case MODITEM_INSLIB_FOLDER:
			// If it's a shell link, resolve the link target
			if(auto file = LinkResolver().Resolve(m_InstrLibPath.ToUnicode() + GetItemText(hItem)); !file.empty())
			{
				SetFullInstrumentLibraryPath(file);
			} else
			{
				InstrumentLibraryChDir(mpt::PathString::FromUnicode(GetItemText(hItem)), modItem.type == MODITEM_INSLIB_SONG);
			}
			return true;

		case MODITEM_HDR_SONG:
			if(modDoc)
				modDoc->ActivateWindow();
			return true;

		case MODITEM_DLSBANK_INSTRUMENT:
			if(GetItemData(GetParentItem(hItem)) != DLS_DRUM_FOLDER_LPARAM)
				PlayItem(hItem, NOTE_MIDDLEC);
			return true;

		case MODITEM_HDR_INSTRUMENTLIB:
			if(IsSampleBrowser())
			{
				BrowseForFolder dlg(m_InstrLibPath, UL_("Select a new instrument library folder..."));
				if(dlg.Show())
				{
					SetFullInstrumentLibraryPath(dlg.GetDirectory());
				}
				return true;
			}
			break;

		default:
			break;
		}
	}
	return false;
}


void CModTree::PlayDLSItem(const CDLSBank &dlsBank, const DlsItem &item, ModCommand::NOTE note)
{
	uint32 rgn, instr = item.GetInstr();
	if(item.IsPercussion())
		rgn = item.GetRegion();
	else
		rgn = dlsBank.GetRegionFromKey(instr, note - NOTE_MIN);
	CMainFrame::GetMainFrame()->PlayDLSInstrument(dlsBank, instr, rgn, note);
}


bool CModTree::PlayItem(QTreeWidgetItem * hItem, ModCommand::NOTE note, int volume)
{
	if(hItem)
	{
		const ModItem modItem = GetModItem(hItem);
		uint32 modItemID = modItem.val1;
		CModDoc *modDoc = m_docInfo.count(m_selectedDoc) ? m_selectedDoc : nullptr;

		switch(modItem.type)
		{
		case MODITEM_SAMPLE:
			if(modDoc)
			{
				if(note == NOTE_NOTECUT)
				{
					modDoc->NoteOff(0, true);  // cut previous playing samples
				} else if(note & 0x80)
				{
					modDoc->NoteOff(note & 0x7F, true);
				} else
				{
					modDoc->NoteOff(0, true);  // cut previous playing samples
					modDoc->PlayNote(PlayNoteParam(note & 0x7F).Sample(static_cast<SAMPLEINDEX>(modItemID)).Volume(volume));
				}
			}
			return true;

		case MODITEM_INSTRUMENT:
			if(modDoc)
			{
				if(note == NOTE_NOTECUT)
				{
					modDoc->NoteOff(0, true);
				} else if(note & 0x80)
				{
					modDoc->NoteOff(note & 0x7F, true);
				} else
				{
					modDoc->NoteOff(0, true);
					modDoc->PlayNote(PlayNoteParam(note & 0x7F).Instrument(static_cast<INSTRUMENTINDEX>(modItemID)).Volume(volume));
				}
			}
			return true;

		case MODITEM_EFFECT:
			if((modDoc) && (modItemID < MAX_MIXPLUGINS))
			{
				modDoc->TogglePluginEditor(modItemID);
			}
			return true;

		case MODITEM_INSLIB_SAMPLE:
		case MODITEM_INSLIB_INSTRUMENT:
			if(note != NOTE_NOTECUT)
			{
				CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
				if(!m_SongFileName.empty())
				{
					// Preview sample / instrument in module
					const size_t n = mpt::parse<size_t>(GetItemText(hItem));
					if(pMainFrm && m_SongFile)
					{
						if(modItem.type == MODITEM_INSLIB_INSTRUMENT)
						{
							pMainFrm->PlaySoundFile(*m_SongFile, static_cast<INSTRUMENTINDEX>(n), SAMPLEINDEX_INVALID, note, volume);
						} else
						{
							pMainFrm->PlaySoundFile(*m_SongFile, INSTRUMENTINDEX_INVALID, static_cast<SAMPLEINDEX>(n), note, volume);
						}
					}
				} else
				{
					// Preview sample / instrument file
					auto file = InsLibGetFullPath(hItem);
					// If it's a shell link, resolve the link target
					if(auto resolvedName = LinkResolver().Resolve(file.ToUnicode()); !resolvedName.empty())
						file = std::move(resolvedName);

					if(pMainFrm)
						pMainFrm->PlaySoundFile(file, note, volume);
				}
			} else
			{
				CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
				if(pMainFrm)
					pMainFrm->StopPreview();
			}

			return true;

		case MODITEM_MIDIPERCUSSION:
			modItemID |= 0x80;
			[[fallthrough]];
		case MODITEM_MIDIINSTRUMENT:
			{
				const MidiLibrary &midiLib = CTrackApp::GetMidiLibrary();
				if(modItemID < midiLib.size() && midiLib[modItemID] && !midiLib[modItemID]->empty())
				{
					CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
					CDLSBank *dlsBank = nullptr;
					if(!mpt::PathCompareNoCase(m_cachedBankName, *midiLib[modItemID]))
					{
						dlsBank = m_cachedBank.get();
					}
					if(dlsBank == nullptr && CDLSBank::IsDLSBank(*midiLib[modItemID]))
					{
						m_cachedBank = std::make_unique<CDLSBank>();
						if(m_cachedBank->Open(*midiLib[modItemID]))
						{
							m_cachedBankName = *midiLib[modItemID];
							dlsBank = m_cachedBank.get();
						}
					}
					if(dlsBank != nullptr)
					{
						uint32 item = 0;
						if(modItemID < 0x80)
						{
							if(dlsBank->FindInstrument(false, 0xFFFF, modItemID, note - NOTE_MIN, &item))
								PlayDLSItem(*dlsBank, DlsItem(static_cast<uint16>(item)), note);
						} else
						{
							if(dlsBank->FindInstrument(true, 0xFFFF, 0xFF, modItemID & 0x7F, &item))
								PlayDLSItem(*dlsBank, DlsItem(static_cast<uint16>(item), static_cast<uint16>(dlsBank->GetRegionFromKey(item, modItemID & 0x7F))), note);
						}
					} else
					{
						pMainFrm->PlaySoundFile(*midiLib[modItemID], note, volume);
					}
				}
			}
			return true;

		case MODITEM_DLSBANK_INSTRUMENT:
			{
				const DlsItem &item = static_cast<const DlsItem &>(modItem);
				CDLSBank *dlsBank = GetDLSBankFromItem(hItem);
				if(dlsBank != nullptr)
				{
					PlayDLSItem(*dlsBank, item, note);
					return true;
				}
			}
			break;

		default:
			break;
		}
	}
	return false;
}


bool CModTree::SetMidiInstrument(uint32 nIns, const mpt::PathString &fileName)
{
	MidiLibrary &midiLib = CTrackApp::GetMidiLibrary();
	if(nIns < 128)
	{
		midiLib[nIns] = fileName;
		RefreshMidiLibrary();
		return true;
	}
	return false;
}


bool CModTree::SetMidiPercussion(uint32 nPerc, const mpt::PathString &fileName)
{
	MidiLibrary &midiLib = CTrackApp::GetMidiLibrary();
	if(nPerc < 128)
	{
		uint32 nIns = nPerc | 0x80;
		midiLib[nIns] = fileName;
		RefreshMidiLibrary();
		return true;
	}
	return false;
}


static mpt::ustring TreeDeletionString(const mpt::uchar *type, uint32 id, const mpt::ustring &name)
{
	mpt::ustring s = MPT_UFORMAT("Remove {} {}")(mpt::ustring(type), id);
	if(!name.empty())
		s += U_(": ") + name;
	s.append(1, UC_('?'));
	return s;
}


void CModTree::DeleteTreeItem(QTreeWidgetItem * hItem, const bool permanently)
{
	const ModItem modItem = GetModItem(hItem);
	uint32 modItemID = modItem.val1;

	CModDoc *modDoc = m_docInfo.count(m_selectedDoc) ? m_selectedDoc : nullptr;
	CSoundFile *sndFile = modDoc ? &modDoc->GetSoundFile() : nullptr;
	if(modItem.IsSongItem() && modDoc == nullptr)
	{
		return;
	}

	switch(modItem.type)
	{
	case MODITEM_SEQUENCE:
		if(sndFile)
		{
			const SEQUENCEINDEX seq = static_cast<SEQUENCEINDEX>(modItemID);
			if(Reporting::Confirm(TreeDeletionString(UL_("sequence"), seq + 1, sndFile->Order(seq).GetName()), false, true) == cnfNo) break;
			sndFile->Order.RemoveSequence(seq);
			modDoc->UpdateAllViews(nullptr, SequenceHint().Data());
		}
		break;

	case MODITEM_ORDER:
		// might be slightly annoying to ask for confirmation here, and it's rather easy to restore the orderlist anyway.
		if(modDoc && modDoc->RemoveOrder(static_cast<SEQUENCEINDEX>(modItem.val2), static_cast<ORDERINDEX>(modItem.val1)))
		{
			modDoc->UpdateAllViews(nullptr, SequenceHint().Data());
		}
		break;

	case MODITEM_PATTERN:
		if(modDoc && sndFile)
		{
			const PATTERNINDEX pat = static_cast<PATTERNINDEX>(modItemID);
			bool isUsed = false;
			// First, find all used patterns in all sequences.
			for(const auto &sequence : sndFile->Order)
			{
				if(sequence.FindOrder(pat) != ORDERINDEX_INVALID)
				{
					isUsed = true;
					break;
				}
			}
			mpt::ustring s = TreeDeletionString(UL_("pattern"), modItemID, mpt::ToUnicode(sndFile->GetCharsetInternal(), sndFile->Patterns[pat].GetName()));
			s += MPT_UFORMAT("\nThis pattern is currently {}used.")(isUsed ? U_("") : U_("un"));
			if(Reporting::Confirm(s, false, isUsed) == cnfYes && modDoc->RemovePattern(pat))
			{
				modDoc->UpdateAllViews(nullptr, PatternHint(pat).Data().Names());
				if(isUsed)
					modDoc->UpdateAllViews(nullptr, SequenceHint().Data());  // Pattern color will change in sequence
			}
		}
		break;

	case MODITEM_SAMPLE:
		if(modDoc && sndFile)
		{
			if(!sndFile->GetSample(static_cast<SAMPLEINDEX>(modItemID)).HasSampleData()
				 || Reporting::Confirm(TreeDeletionString(UL_("sample"), modItemID, mpt::ToUnicode(sndFile->GetCharsetInternal(), sndFile->m_szNames[modItemID])), false, true) == cnfYes)
			{
				const SAMPLEINDEX smp = static_cast<SAMPLEINDEX>(modItemID);
				modDoc->GetSampleUndo().PrepareUndo(smp, sundo_replace, "Delete");
				const SAMPLEINDEX oldNumSamples = modDoc->GetNumSamples();
				if(modDoc->RemoveSample(smp))
				{
					modDoc->UpdateAllViews(nullptr, SampleHint(modDoc->GetNumSamples() != oldNumSamples ? 0 : smp).Info().Data().Names());
				}
			}
		}
		break;

	case MODITEM_INSTRUMENT:
		if(modDoc && sndFile)
		{
			if(sndFile->Instruments[modItemID] == nullptr
				 || Reporting::Confirm(TreeDeletionString(UL_("instrument"), modItemID, mpt::ToUnicode(sndFile->GetCharsetInternal(), sndFile->Instruments[modItemID]->name)), false, true) == cnfYes)
			{
				const INSTRUMENTINDEX ins = static_cast<INSTRUMENTINDEX>(modItemID);
				modDoc->GetInstrumentUndo().PrepareUndo(ins, "Delete");
				const INSTRUMENTINDEX oldNumInstrs = modDoc->GetNumInstruments();
				if(modDoc->RemoveInstrument(ins))
				{
					modDoc->UpdateAllViews(nullptr, InstrumentHint(modDoc->GetNumInstruments() != oldNumInstrs ? 0 : ins).Info().Envelope().ModType());
				}
			}
		}
		break;

	case MODITEM_EFFECT:
		if(modDoc && Reporting::Confirm(TreeDeletionString(UL_("plugin FX"), modItemID + 1, sndFile->m_MixPlugins[modItemID].GetName()), false, true) == cnfYes)
		{
			modDoc->RemovePlugin(static_cast<PLUGINDEX>(modItemID));
		}
		break;

	case MODITEM_MIDIINSTRUMENT:
		SetMidiInstrument(modItemID, P_(""));
		RefreshMidiLibrary();
		break;
	case MODITEM_MIDIPERCUSSION:
		SetMidiPercussion(modItemID, P_(""));
		RefreshMidiLibrary();
		break;

	case MODITEM_DLSBANK_FOLDER:
		CTrackApp::RemoveDLSBank(modItemID);
		RefreshDlsBanks();
		break;

	case MODITEM_INSLIB_SONG:
	case MODITEM_INSLIB_SAMPLE:
	case MODITEM_INSLIB_INSTRUMENT:
		{
			// mfcless: to the trash (Windows' recycle bin), or for good
			const QString fullPath = mfcless::ToQString(InsLibGetFullPath(hItem).ToUnicode());
			if(permanently ? QFile::remove(fullPath) : QFile::moveToTrash(fullPath))
			{
				QTreeWidgetItem * newSel = GetNextSiblingItem(hItem);
				if(!newSel) newSel = GetPrevSiblingItem(hItem);
				SelectItem(newSel);
				RefreshInstrumentLibrary();
				setFocus();
			}
		}
		break;

	default:
		break;
	}
}


bool CModTree::OpenTreeItem(QTreeWidgetItem * hItem)
{
	const ModItem modItem = GetModItem(hItem);

	switch(modItem.type)
	{
	case MODITEM_HDR_SONG:
		if(const auto pathName = GetDocumentFromItem(hItem)->GetPathNameMpt(); !pathName.empty())
			CTrackApp::OpenDirectory(pathName);
		break;
	case MODITEM_INSLIB_SONG:
		theApp.OpenDocumentFile(InsLibGetFullPath(hItem));
		break;
	case MODITEM_HDR_INSTRUMENTLIB:
		CTrackApp::OpenDirectory(m_InstrLibPath + m_SongFileName);
		break;
	case MODITEM_INSLIB_FOLDER:
	case MODITEM_INSLIB_INSTRUMENT:
	case MODITEM_INSLIB_SAMPLE:
		// Open path in Explorer
		CTrackApp::OpenDirectory(InsLibGetFullPath(hItem));
		break;
	default:
		break;
	}
	return true;
}


bool CModTree::OpenMidiInstrument(uint32 dwItem)
{
	std::vector<FileType> mediaFoundationTypes = CSoundFile::GetMediaFoundationFileTypes();
	FileDialog dlg = OpenFileDialog()
		.EnableAudioPreview()
		.ExtensionFilter(
			"All Instruments and Banks (*.xi,*.pat,*.iti,*.sfz,*.dls,*.sf2,...)|*.xi;*.pat;*.iti;*.sfz;*.wav;*.w64;*.caf;*.aif;*.aiff;*.sbk;*.sf2;*.sf3;*.sf4;*.dls;*.mss;*.flac;*.opus;*.ogg;*.oga;*.mp1;*.mp2;*.mp3" + ToFilterOnlyString(mediaFoundationTypes, true).ToLocale() + "|"
			"FastTracker II Instruments (*.xi)|*.xi|"
			"GF1 Patches (*.pat)|*.pat|"
			"Wave Files (*.wav)|*.wav|"
			"Wave64 Files (*.w64)|*.w64|"
			"CAF Files (*.caf)|*.caf|"
	#ifdef MPT_WITH_FLAC
			"FLAC Files (*.flac,*.oga)|*.flac;*.oga|"
	#endif // MPT_WITH_FLAC
	#if defined(MPT_WITH_OPUSFILE)
			"Opus Files (*.opus,*.oga)|*.opus;*.oga|"
	#endif // MPT_WITH_OPUSFILE
	#if defined(MPT_WITH_VORBISFILE) || defined(MPT_WITH_STBVORBIS)
			"Ogg Vorbis Files (*.ogg,*.oga)|*.ogg;*.oga|"
	#endif // VORBIS
	#if defined(MPT_ENABLE_MP3_SAMPLES)
			"MPEG Files (*.mp1,*.mp2,*.mp3)|*.mp1;*.mp2;*.mp3|"
	#endif // MPT_ENABLE_MP3_SAMPLES
	#if defined(MPT_WITH_MEDIAFOUNDATION)
			+ ToFilterString(mediaFoundationTypes, FileTypeFormatShowExtensions).ToLocale() +
	#endif
			"Impulse Tracker Instruments (*.iti)|*.iti;*.its|"
			"SFZ Instruments (*.sfz)|*.sfz|"
			"SoundFont 2.0 Banks (*.sf2)|*.sbk;*.sf2;*.sf3;*.sf4|"
			"DLS Sound Banks (*.dls;*.mss)|*.dls;*.mss|"
			"All Files (*.*)|*.*||");
	if(!dlg.Show()) return false;

	if(dwItem & 0x80)
		return SetMidiPercussion(dwItem & 0x7F, dlg.GetFirstFile());
	else
		return SetMidiInstrument(dwItem, dlg.GetFirstFile());
}


// Refresh Instrument Library
void CModTree::FillInstrumentLibrary(const mpt::uchar *selectedItem)
{
	if(!m_hInsLib && !IsSampleBrowser())
		return;

	LockRedraw();

	if(!IsSampleBrowser())
	{
		DeleteChildren(m_hInsLib);
	} else
	{
		DeleteItem(nullptr);
		m_hInsLib = nullptr;
	}

	m_fileBrowserEntries.clear();

	if(!m_SongFileName.empty() && IsSampleBrowser() && m_SongFile)
	{
		// Fill browser with samples / instruments of module file
		for(INSTRUMENTINDEX ins = 1; ins <= m_SongFile->GetNumInstruments(); ins++)
		{
			ModInstrument *pIns = m_SongFile->Instruments[ins];
			if(pIns)
			{
				mpt::ustring s;
				s = mfcless::PrintF(UL_("%3d: %s"), ins, mpt::ToUnicode(m_SongFile->GetCharsetInternal(), pIns->name).c_str());
				m_fileBrowserEntries.push_back({s, 0, 0, IMAGE_INSTRUMENTS, false});
			}
		}
		for(SAMPLEINDEX smp = 1; smp <= m_SongFile->GetNumSamples(); smp++)
		{
			const ModSample &sample = m_SongFile->GetSample(smp);
			if(sample.HasSampleData() || sample.uFlags[CHN_ADLIB])
			{
				mpt::ustring s;
				s = mfcless::PrintF(UL_("%3d: %s"), smp, mpt::ToUnicode(m_SongFile->GetCharsetInternal(), m_SongFile->m_szNames[smp]).c_str());
				m_fileBrowserEntries.push_back({s, sample.GetSampleSizeInBytes(), 0, static_cast<uint32>(sample.uFlags[CHN_ADLIB] ? IMAGE_OPLINSTR : IMAGE_SAMPLES), false});
			}
		}
	} else if(!m_InstrLibPath.empty())
	{
		if(!IsSampleBrowser())
		{
			SetItemText(m_hInsLib, UL_("Instrument Library (") + m_InstrLibPath.ToUnicode() + UL_(")"));
		}

		// Enumerating Drives...
		if(!IsSampleBrowser())
		{
			// mfcless: no drive letters: the file system's root, the home folder
			// and mounted drives instead, as absolute paths with a trailing slash.
			std::vector<QString> places{QStringLiteral("/"), QDir::homePath()};
			for(const QStorageInfo &volume : QStorageInfo::mountedVolumes())
			{
				if(volume.isValid() && volume.isReady() && !volume.isRoot() && (volume.rootPath().startsWith(QStringLiteral("/media/")) || volume.rootPath().startsWith(QStringLiteral("/run/media/")) || volume.rootPath().startsWith(QStringLiteral("/mnt/"))))
					places.push_back(volume.rootPath());
			}
			for(QString place : places)
			{
				if(!place.endsWith(QLatin1Char('/')))
					place += QLatin1Char('/');
				m_fileBrowserEntries.push_back({mfcless::FromQString(place), 0, 0, static_cast<uint32>(IMAGE_FOLDER), false});
			}
		}

		// Enumerating Directories and samples/instruments
		const mpt::PathString path = m_InstrLibPath + P_("*.*");
		const bool showDirs = !IsSampleBrowser() || TrackerSettings::Instance().showDirsInSampleBrowser;
		const bool showInstrs = IsSampleBrowser();

		enum
		{
			FILTER_FIRST_VALID = 0,
			FILTER_REJECT_FILE = -1,
			FILTER_UNKNOWN_FILE = -2,
		};

		const auto FilterFile = [this, showInstrs, showDirs](const mpt::PathString &fileName) -> int
		{

			static constexpr auto instrExts = {"xi", "iti", "sfz", "sf2", "sf3", "sf4", "sbk", "dls", "mss", "pat"};
			static constexpr auto sampleExts = {"wav", "flac", "ogg", "opus", "mp1", "mp2", "mp3", "smp", "raw", "s3i", "its", "aif", "aiff", "au", "snd", "svx", "voc", "8sv", "8svx", "16sv", "16svx", "w64", "caf", "sb0", "sb2", "sbi", "brr"};
			// These are hidden even when "show all files" is enabled, as it would be extremely unlikely that they contain any sample data
			static constexpr auto hideFileExts = {"txt", "diz", "nfo", "doc", "ini", "pdf", "zip", "rar", "lha", "exe", "dll", "lnk", "url"};

			// Get lower-case file extension without dot.
			mpt::PathString extPS = fileName.GetFilenameExtension();
			std::string ext = extPS.ToUTF8();
			if(!ext.empty())
			{
				ext.erase(0, 1);
				ext = mpt::ToLowerCaseAscii(ext);
				extPS = mpt::PathString::FromUTF8(ext);
			}

			bool knownExtension = true;
			if(mpt::contains(instrExts, ext))
			{
				if(showInstrs)
					return IMAGE_INSTRUMENTS;
			} else if(mpt::contains(sampleExts, ext))
			{
				if(showInstrs)
					return IMAGE_SAMPLES;
			} else if(mpt::contains(m_modExtensions, ext))
			{
				if(showDirs || m_showAllFiles)
					return IMAGE_FOLDERSONG;
			} else if(!extPS.empty() && mpt::contains(m_MediaFoundationExtensions, extPS))
			{
				if(showInstrs)
					return IMAGE_SAMPLES;
			} else
			{
				if(showDirs)
				{
					// Amiga-style prefix (i.e. mod.songname)
					std::string prefixExt = fileName.ToUTF8();
					const auto dotPos = prefixExt.find('.');
					if(dotPos != std::string::npos && mpt::contains(m_modExtensions, prefixExt.erase(dotPos)))
						return IMAGE_FOLDERSONG;
				}
				knownExtension = false;
			}

			if(m_showAllFiles && !mpt::contains(hideFileExts, ext))
				return IMAGE_SAMPLES;

			// Avoid marking zip files as unknown files, as Windows may try to parse those zip files in our LinkResolver.
			// Unless someone has a *very* peculiar file extension setup, these files cannot contain a link anyway, so we can speed up our directory listing.
			return (knownExtension || ext == "zip") ? FILTER_REJECT_FILE : FILTER_UNKNOWN_FILE;
		};

		// mfcless: Explorer's "show hidden files" setting; dot files stay hidden.
		const bool showHidden = false;

		LinkResolver linkResolver;
		MPT_UNUSED(path);
		const QDir dir(mfcless::ToQString(m_InstrLibPath.ToUnicode()));
		const QFileInfoList entries = dir.entryInfoList(QDir::AllEntries | QDir::NoDot | QDir::Hidden | QDir::System, QDir::NoSort);
		for(const QFileInfo &entry : entries)
		{
			const mpt::ustring fileName = mfcless::FromQString(entry.fileName());
			// Up Directory
			int type = FILTER_REJECT_FILE;
			if(entry.fileName() == QStringLiteral(".."))
			{
				if(showDirs)
					type = IMAGE_FOLDERPARENT;
			} else if(entry.isHidden() && !showHidden)
			{
				// Only show hidden files if configured to do so
				continue;
			} else if(entry.isDir())
			{
				// Directory
				if(showDirs)
					type = IMAGE_FOLDER;
			} else if(entry.size() >= 9)
			{
				type = FilterFile(mpt::PathString::FromUnicode(fileName));
				if(type == FILTER_UNKNOWN_FILE)
				{
					// Try resolving file as link if it wasn't a module or instrument
					const auto nativePath = (m_InstrLibPath.ToUnicode() + fileName);
					if(const auto resolvedName = linkResolver.Resolve(nativePath.c_str()); !resolvedName.empty())
					{
						if(mpt::native_fs{}.is_directory(resolvedName) && showDirs)
							type = IMAGE_FOLDER;
						else if(mpt::native_fs{}.is_file(resolvedName))
							type = FilterFile(resolvedName);
					}
				}
			}
			if(type >= FILTER_FIRST_VALID)
			{
				// modtime: milliseconds since 1970
				m_fileBrowserEntries.push_back({fileName,
					static_cast<uint64>(entry.size()),
					static_cast<uint64>(entry.lastModified().toMSecsSinceEpoch()),
					static_cast<uint32>(type),
					entry.isHidden()});
			}
		}
	}

	SortInstrumentLibrary();
	FilterInstrumentLibrary(m_filterString, selectedItem);

	UnlockRedraw();

	bool switchWatchDir = false;
	{
		mpt::lock_guard<mpt::mutex> l(m_WatchDirMutex);
		if(m_InstrLibPath != m_WatchDir)
		{
			m_WatchDir = m_InstrLibPath;
			switchWatchDir = true;
		}
	}
	if(switchWatchDir)
		MonitorInstrumentLibrary();
}


void CModTree::SortInstrumentLibrary()
{
	QCollator collator;
	collator.setCaseSensitivity(Qt::CaseInsensitive);
	collator.setNumericMode(true);
	std::sort(m_fileBrowserEntries.begin(), m_fileBrowserEntries.end(), [this, &collator](const FileBrowserEntry &left, const FileBrowserEntry &right)
	{
		const int sortL = ImageToSortOrder(left.image), sortR = ImageToSortOrder(right.image);
		if (sortL != sortR)
			return sortL < sortR;

		if (m_librarySort == LibrarySortOrder::Date && left.modtime != right.modtime)
			return left.modtime > right.modtime;
		else if (m_librarySort == LibrarySortOrder::Size && left.size != right.size)
			return left.size > right.size;

		// mfcless: CompareString with NORM_IGNORECASE and SORT_DIGITSASNUMBERS
		return collator.compare(mfcless::ToQString(left.name), mfcless::ToQString(right.name)) < 0;
	});
}


void CModTree::FilterInstrumentLibrary(mpt::ustring filter, const mpt::uchar *selectedItem)
{
	LockRedraw();

	if(!filter.empty())
		filter = UL_("*") + filter + UL_("*");

	QTreeWidgetItem * item = GetNextItem(m_hInsLib, IsSampleBrowser() ? mfcless::tree::Next : mfcless::tree::Child);
	while(item != nullptr)
	{
		QTreeWidgetItem * nextItem = GetNextSiblingItem(item);
		if(nextItem == nullptr)
			break;
		item = nextItem;
	}

	// TODO: Maybe first delete front-to-back, then insert back-to-front?

	// mfcless: PathMatchSpec (wildcards, ignoring case)
	const QRegularExpression filterSpec(QRegularExpression::wildcardToRegularExpression(mfcless::ToQString(filter), QRegularExpression::DefaultWildcardConversion), QRegularExpression::CaseInsensitiveOption);

	// Insert items in reverse, as insertion via mfcless::tree::First is faster than mfcless::tree::Last as per https://devblogs.microsoft.com/oldnewthing/20111125-00/?p=9033
	uintptr_t entryID = m_fileBrowserEntries.size();
	QTreeWidgetItem * selectedTreeItem = nullptr;
	for(auto entry = m_fileBrowserEntries.crbegin(); entry != m_fileBrowserEntries.crend(); entry++, entryID--)
	{
		const bool add = filter.empty() || filterSpec.match(mfcless::ToQString(entry->name)).hasMatch();

		if(add)
		{
			while(item && GetItemData(item) > entryID)
			{
				item = GetPrevSiblingItem(item);
			}

			if(!item || GetItemData(item) != entryID)
			{
				int state = entry->hidden ? mfcless::tree::Cut : 0;
				item = InsertItem(mfcless::tree::MaskImage | mfcless::tree::MaskSelectedImage | mfcless::tree::MaskText | mfcless::tree::MaskState | mfcless::tree::MaskParam, entry->name, entry->image, entry->image, state, state, entryID, IsSampleBrowser() ? nullptr : m_hInsLib, item ? item : mfcless::tree::First);
			}

			if(selectedItem != nullptr && entry->name == selectedItem)
			{
				selectedTreeItem = item;
			}

			item = GetPrevSiblingItem(item);
		} else
		{
			while(item && GetItemData(item) >= entryID)
			{
				QTreeWidgetItem * prevItem = GetPrevSiblingItem(item);
				DeleteItem(item);
				if(item == m_hInsLib)
					m_hInsLib = nullptr;
				item = prevItem;
			}
		}
	}

	if(IsSampleBrowser())
	{
		if(m_hInsLib)
			DeleteItem(m_hInsLib);
		if(!m_SongFileName.empty() && m_SongFile)
			m_hInsLib = InsertItem(m_SongFileName.ToUnicode(), IMAGE_FOLDERSONG, IMAGE_FOLDERSONG, nullptr, mfcless::tree::First);
		else
			m_hInsLib = InsertItem(m_InstrLibPath.ToUnicode(), IMAGE_FOLDER, IMAGE_FOLDER, nullptr, mfcless::tree::First);
	}

	if(!filter.empty() && !selectedTreeItem)
	{
		if(IsSampleBrowser())
		{
			selectedTreeItem = GetFirstVisibleItem();
			if(selectedTreeItem == m_hInsLib)
				selectedTreeItem = GetNextItem(m_hInsLib, mfcless::tree::Next);
		} else
		{
			selectedTreeItem = GetChildItem(m_hInsLib);
		}
	}

	UnlockRedraw();

	if(selectedTreeItem)
	{
		SelectItem(selectedTreeItem);
		EnsureVisible(selectedTreeItem);  // TVM_ENSUREVISIBLE (posted upstream, as sending it did not work there)
	}
}


void CModTree::SetInstrumentLibraryFilter(const mpt::ustring &filter)
{
	if(m_filterString == filter)
		return;

	m_filterString = filter;
	FilterInstrumentLibrary(m_filterString, GetItemText(GetSelectedItem()).c_str());
}


void CModTree::SetInstrumentLibraryFilterSortOrder(LibrarySortOrder sortType)
{
	if(m_librarySort == sortType)
		return;

	m_librarySort = sortType;
	SortInstrumentLibrary();
	FilterInstrumentLibrary(m_filterString, GetItemText(GetSelectedItem()).c_str());
	GetOtherView()->SortInstrumentLibrary();
	GetOtherView()->FilterInstrumentLibrary(GetOtherView()->m_filterString, GetItemText(GetSelectedItem()).c_str());
}


void CModTree::OnGetDispInfo()
{
	// mfcless: no text callbacks (LPSTR_TEXTCALLBACK); FilterInstrumentLibrary
	// gives the items their names.
}


int CModTree::ImageToSortOrder(int image) const
{
	// Item image indicates sort order
	switch(image)
	{
		case IMAGE_FOLDERPARENT:
			return 1;
		case IMAGE_FOLDER:
			return 2;
		case IMAGE_FOLDERSONG:
			return 3;
		case IMAGE_SAMPLES:
		case IMAGE_OPLINSTR:
			// Only group instruments and samples separately if we're browsing inside a module file
			if(!m_SongFileName.empty())
				return 5;
			[[fallthrough]];
		case IMAGE_INSTRUMENTS:
		default:
			return 4;
	}
}


// Monitor changes in the instrument library folder.
// mfcless: watches m_WatchDir (call it when that changes); OnWatchDirChanged
// refreshes the library, at most every FSUpdateInterval milliseconds.
void CModTree::MonitorInstrumentLibrary()
{
	if(!m_watchDirWatcher)
		return;
	mpt::PathString watchDir;
	{
		mpt::lock_guard<mpt::mutex> l(m_WatchDirMutex);
		watchDir = m_WatchDir;
	}
	if(const QStringList dirs = m_watchDirWatcher->directories(); !dirs.isEmpty())
		m_watchDirWatcher->removePaths(dirs);
	if(!watchDir.empty())
		m_watchDirWatcher->addPath(mfcless::ToQString(watchDir.ToUnicode()));
}


void CModTree::OnWatchDirChanged()
{
	if(m_watchDirTimer->isActive())
		return;
	const uint64 now = Util::GetTickCount64();
	const uint32 interval = TrackerSettings::Instance().FSUpdateInterval;
	if(now - m_watchDirLastRefresh >= interval)
	{
		m_watchDirLastRefresh = now;
		mfcless::PostCommand(this, ID_MODTREE_REFRESHINSTRLIB);
	} else
	{
		m_watchDirTimer->start(static_cast<int>(interval - (now - m_watchDirLastRefresh)));
	}
}


void CModTree::SetFullInstrumentLibraryPath(mpt::PathString path)
{
	if(mpt::native_fs{}.is_directory(path))
	{
		path = path.WithTrailingSlash();
		InstrumentLibraryChDir(path, false);
	} else if(mpt::native_fs{}.is_file(path))
	{
		// Browse module contents
		CModTree *dirBrowser = CMainFrame::GetMainFrame()->GetUpperTreeview();
		dirBrowser->m_InstrLibPath = path.GetDirectoryWithDrive();
		dirBrowser->RefreshInstrumentLibrary();
		dirBrowser->InstrumentLibraryChDir(path.GetFilename(), true);
	}
}


void CModTree::InstrumentLibraryChDir(mpt::PathString dir, bool isSong)
{
	if(dir.empty())
		return;

	if(IsSampleBrowser())
	{
		CMainFrame::GetMainFrame()->GetUpperTreeview()->InstrumentLibraryChDir(dir, isSong);
		return;
	}

	mfcless::PostCommand(parentWidget(), ID_CLOSE_LIBRARY_FILTER);
	m_filterString.clear();
	m_pDataTree->m_filterString.clear();

	mfcless::BeginWaitCursor();

	bool ok = false;
	const bool goUp = (dir == P_(".."));
	m_previousPath = {};
	if(isSong && !goUp)
	{
		ok = m_pDataTree->InsLibSetFullPath(m_InstrLibPath, dir);
		if(ok)
		{
			m_pDataTree->RefreshInstrumentLibrary();
			m_InstrLibHighlightPath = dir;
		}
	} else
	{
		if(goUp)
		{
			if(isSong)
			{
				// Leave song
				m_InstrLibHighlightPath = std::move(m_pDataTree->m_SongFileName);
				dir = m_InstrLibPath;
			} else
			{
				// Go one dir up.
				mpt::ustring prevDir = m_InstrLibPath.GetDirectoryWithDrive().ToUnicode();
				mpt::ustring::size_type pos = prevDir.find_last_of(UL_("\\/"), prevDir.length() - 2);
				if(pos != mpt::ustring::npos)
				{
					m_InstrLibHighlightPath = mpt::PathString::FromUnicode(prevDir.substr(pos + 1, prevDir.length() - pos - 2));  // Highlight previously accessed directory
					prevDir = prevDir.substr(0, pos + 1);
				}
				m_previousPath = m_InstrLibHighlightPath;
				dir = mpt::PathString::FromUnicode(prevDir);
			}
		} else
		{
			// Drives are formatted like "E:\", folders are just folder name without slash.
			do
			{
				if(!dir.HasTrailingSlash())
				{
					dir = m_InstrLibPath + dir;
					dir = dir.WithTrailingSlash();
				}
				m_InstrLibHighlightPath = P_("..");  // Highlight first entry

				FolderScanner scan(dir, FolderScanner::kFilesAndDirectories);
				mpt::PathString name;
				if(scan.Next(name) && !scan.Next(name) && mpt::native_fs{}.is_directory(name))
				{
					// There is only one directory and nothing else in the path,
					// so skip this directory and automatically descend further down into the tree.
					dir = name;
					dir = dir.WithTrailingSlash();
					continue;
				}
			} while(false);
		}

		if(mpt::native_fs{}.is_directory(dir))
		{
			m_SongFileName = P_("");
			delete m_SongFile;
			m_SongFile = nullptr;
			m_InstrLibPath = dir;
			GetSampleBrowser()->m_InstrLibHighlightPath = m_InstrLibHighlightPath;
			mfcless::PostCommand(this, ID_MODTREE_REFRESHINSTRLIB);
			ok = true;
		}
	}

	mfcless::EndWaitCursor();

	if(ok)
	{
		mpt::lock_guard<mpt::mutex> l(m_WatchDirMutex);
		m_WatchDir = mpt::PathString();
	} else
	{
		Reporting::Error(MPT_UFORMAT("Unable to browse to \"{}\"")(dir), UL_("Instrument Library"));
	}
}


bool CModTree::GetDropInfo(DRAGONDROP &dropInfo, mpt::PathString &fullPath)
{
	const auto dragDoc = m_docInfo.find(m_dragDoc);
	dropInfo.sndFile = dragDoc != m_docInfo.end() ? &dragDoc->second.modDoc.GetSoundFile() : nullptr;
	dropInfo.dropType = DRAGONDROP_NOTHING;
	dropInfo.dropItem = m_itemDrag.val1;
	dropInfo.dropParam = 0;
	switch(m_itemDrag.type)
	{
	case MODITEM_ORDER:
		dropInfo.dropType = DRAGONDROP_ORDER;
		break;

	case MODITEM_PATTERN:
		dropInfo.dropType = DRAGONDROP_PATTERN;
		break;

	case MODITEM_SAMPLE:
		dropInfo.dropType = DRAGONDROP_SAMPLE;
		break;

	case MODITEM_INSTRUMENT:
		dropInfo.dropType = DRAGONDROP_INSTRUMENT;
		break;

	case MODITEM_SEQUENCE:
	case MODITEM_HDR_ORDERS:
		dropInfo.dropType = DRAGONDROP_SEQUENCE;
		break;

	case MODITEM_INSLIB_SAMPLE:
	case MODITEM_INSLIB_INSTRUMENT:
		if(!m_SongFileName.empty())
		{
			const uint32 n = mpt::parse<uint32>(GetItemText(m_hItemDrag));
			dropInfo.dropType = (m_itemDrag.type == MODITEM_INSLIB_SAMPLE) ? DRAGONDROP_SAMPLE : DRAGONDROP_INSTRUMENT;
			dropInfo.dropItem = n;
			dropInfo.sndFile = m_SongFile;
			dropInfo.dropParam = 0;
		} else
		{
			fullPath = InsLibGetFullPath(m_hItemDrag);
			dropInfo.dropType = DRAGONDROP_SOUNDFILE;
			dropInfo.dropParam = reinterpret_cast<uintptr_t>(&fullPath);
		}
		break;

	case MODITEM_MIDIPERCUSSION:
		dropInfo.dropItem |= 0x80;
		[[fallthrough]];
	case MODITEM_MIDIINSTRUMENT:
		if(dropInfo.dropItem < CTrackApp::GetMidiLibrary().size())
		{
			const auto &libItem = CTrackApp::GetMidiLibrary()[dropInfo.dropItem];
			if(libItem && !libItem->empty())
			{
				fullPath = *libItem;
				dropInfo.dropType = DRAGONDROP_MIDIINSTR;
				dropInfo.dropParam = reinterpret_cast<uintptr_t>(&fullPath);
			}
		}
		break;

	case MODITEM_INSLIB_SONG:
		fullPath = InsLibGetFullPath(m_hItemDrag);
		dropInfo.sndFile = nullptr;
		dropInfo.dropType = DRAGONDROP_SONG;
		dropInfo.dropItem = 0;
		dropInfo.dropParam = reinterpret_cast<uintptr_t>(&fullPath);
		break;

	case MODITEM_DLSBANK_INSTRUMENT:
		{
			dropInfo.dropType = DRAGONDROP_DLS;
			dropInfo.dropItem = static_cast<uint32>(GetDLSBankIndexFromItem(m_hItemDrag));  // bank #
			// Melodic: (Instrument)
			// Drums:   (0x80000000) | (Region << 16) | (Instrument)
			dropInfo.dropParam = m_itemDrag.val1;
		}
		break;

	default:
		break;
	}
	return (dropInfo.dropType != DRAGONDROP_NOTHING);
}


bool CModTree::CanDrop(QTreeWidgetItem * hItem, bool doDrop)
{
	const ModItem modItemDrop = GetModItem(hItem);
	const uint32 modItemDropID = modItemDrop.val1;
	const uint32 modItemDragID = m_itemDrag.val1;

	const auto dragIter = m_docInfo.find(m_dragDoc);
	const auto selIter = m_docInfo.find(m_selectedDoc);
	const ModTreeDocInfo *infoDrag = dragIter != m_docInfo.end() ? &dragIter->second : nullptr;
	const ModTreeDocInfo *infoDrop = selIter != m_docInfo.end() ? &selIter->second : nullptr;
	CModDoc *modDoc = infoDrop ? &infoDrop->modDoc : nullptr;
	CSoundFile *sndFile = modDoc ? &modDoc->GetSoundFile() : nullptr;
	const bool sameModDoc = infoDrag && (modDoc == &infoDrag->modDoc);
	const bool sameItem = modItemDrop == m_itemDrag && sameModDoc;

	switch(modItemDrop.type)
	{
	case MODITEM_ORDER:
	case MODITEM_SEQUENCE:
		if(m_itemDrag.type == MODITEM_ORDER && modDoc && sameModDoc)
		{
			// Drop an order somewhere
			if(doDrop)
			{
				SEQUENCEINDEX seqFrom = static_cast<SEQUENCEINDEX>(m_itemDrag.val2), seqTo = static_cast<SEQUENCEINDEX>(modItemDrop.val2);
				ORDERINDEX ordFrom = static_cast<ORDERINDEX>(m_itemDrag.val1), ordTo = static_cast<ORDERINDEX>(modItemDrop.val1);
				if(modItemDrop.type == MODITEM_SEQUENCE)
				{
					// Drop on sequence -> attach
					seqTo = static_cast<SEQUENCEINDEX>(modItemDrop.val1);
					ordTo = sndFile->Order(seqTo).GetLengthTailTrimmed();
				}

				if(seqFrom != seqTo || ordFrom != ordTo)
				{
					if(modDoc->MoveOrder(ordFrom, ordTo, true, false, seqFrom, seqTo) == true)
					{
						modDoc->SetModified();
					}
				}
			}
			return true;
		} else if(modItemDrop.type == MODITEM_SEQUENCE && m_itemDrag.type == MODITEM_SEQUENCE && modDoc && sameModDoc)
		{
			if(doDrop && !sameItem)
			{
				// Rearrange sequences
				const SEQUENCEINDEX from = static_cast<SEQUENCEINDEX>(modItemDragID), to = static_cast<SEQUENCEINDEX>(modItemDropID);

				std::vector<SEQUENCEINDEX> newOrder(sndFile->Order.GetNumSequences());
				std::iota(newOrder.begin(), newOrder.end(), SEQUENCEINDEX(0));
				newOrder.erase(newOrder.begin() + from);
				newOrder.insert(newOrder.begin() + to, from);

				modDoc->ReArrangeSequences(newOrder);

				auto curSeq = sndFile->Order.GetCurrentSequenceIndex();
				if(curSeq == from)
					curSeq = to;
				else if(from > curSeq && to <= curSeq)
					curSeq++;
				else if(from < curSeq && to >= curSeq)
					curSeq--;
				sndFile->Order.SetSequence(curSeq);

				modDoc->UpdateAllViews(nullptr, SequenceHint(SEQUENCEINDEX_INVALID).Names().Data());
				modDoc->SetModified();

				SelectItem(hItem);
			}
			return true;
		}
		break;

	case MODITEM_HDR_ORDERS:
		// Drop your sequences here.
		// At the moment, only dropping sequences into another module is possible and it doesn't copy the patterns themselves.
		if((m_itemDrag.type == MODITEM_SEQUENCE || m_itemDrag.type == MODITEM_HDR_ORDERS) && modDoc && sndFile && infoDrag && !sameModDoc)
		{
			if(doDrop && infoDrag != nullptr)
			{
				// copy mod sequence over.
				CSoundFile &dragSndFile = infoDrag->modDoc.GetSoundFile();
				const SEQUENCEINDEX origSeqId = static_cast<SEQUENCEINDEX>(modItemDragID);
				const ModSequence &origSeq = dragSndFile.Order(origSeqId);
				SEQUENCEINDEX sequenceHint = SEQUENCEINDEX_INVALID;

				if(sndFile->GetModSpecifications().sequencesMax > 1)
				{
					sequenceHint = sndFile->Order.AddSequence();
				} else
				{
					if(Reporting::Confirm(UL_("Replace the current orderlist?"), UL_("Sequence import")) == cnfNo)
						return false;
					sequenceHint = 0;
				}
				sndFile->Order().resize(std::min(sndFile->GetModSpecifications().ordersMax, origSeq.GetLength()), PATTERNINDEX_INVALID);
				for(ORDERINDEX nOrd = 0; nOrd < std::min(sndFile->GetModSpecifications().ordersMax, origSeq.GetLengthTailTrimmed()); nOrd++)
				{
					PATTERNINDEX pat = dragSndFile.Order(origSeqId)[nOrd];
					// translate pattern index
					if(pat == PATTERNINDEX_SKIP && sndFile->GetModSpecifications().hasIgnoreIndex)
						pat = PATTERNINDEX_SKIP;
					else if(pat >= sndFile->GetModSpecifications().patternsMax)
						pat = PATTERNINDEX_INVALID;
					
					sndFile->Order()[nOrd] = pat;
				}
				modDoc->UpdateAllViews(nullptr, SequenceHint(sequenceHint).Data());
				modDoc->SetModified();
			}
			return true;
		}
		break;

	case MODITEM_SAMPLE:
		if(m_itemDrag.type == MODITEM_SAMPLE && modDoc && infoDrag != nullptr)
		{
			if(doDrop)
			{
				if(sameModDoc)
				{
					// Reorder samples in a module
					if(sameItem)
						return true;
					const SAMPLEINDEX from = static_cast<SAMPLEINDEX>(modItemDragID - 1), to = static_cast<SAMPLEINDEX>(modItemDropID - 1);

					std::vector<SAMPLEINDEX> newOrder(modDoc->GetNumSamples());
					std::iota(newOrder.begin(), newOrder.end(), SAMPLEINDEX(1));
					newOrder.erase(newOrder.begin() + from);
					newOrder.insert(newOrder.begin() + to, from + 1);

					modDoc->ReArrangeSamples(newOrder);
				} else
				{
					// Load sample into other module
					sndFile->ReadSampleFromSong(static_cast<SAMPLEINDEX>(modItemDropID), infoDrag->modDoc.GetSoundFile(), static_cast<SAMPLEINDEX>(modItemDragID));
				}
				modDoc->UpdateAllViews(nullptr, SampleHint().Info().Data().Names());
				modDoc->UpdateAllViews(nullptr, PatternHint().Data());
				modDoc->UpdateAllViews(nullptr, InstrumentHint().Info());
				modDoc->SetModified();
				SelectItem(hItem);
			}
			return true;
		}
		break;

	case MODITEM_INSTRUMENT:
		if(m_itemDrag.type == MODITEM_INSTRUMENT && modDoc && infoDrag != nullptr)
		{
			if(doDrop)
			{
				if(sameModDoc)
				{
					// Reorder instruments in a module
					if(sameItem)
						return true;
					const INSTRUMENTINDEX from = static_cast<INSTRUMENTINDEX>(modItemDragID - 1), to = static_cast<INSTRUMENTINDEX>(modItemDropID - 1);

					std::vector<INSTRUMENTINDEX> newOrder(modDoc->GetNumInstruments());
					std::iota(newOrder.begin(), newOrder.end(), INSTRUMENTINDEX(1));
					newOrder.erase(newOrder.begin() + from);
					newOrder.insert(newOrder.begin() + to, from + 1);

					modDoc->ReArrangeInstruments(newOrder);
				} else
				{
					// Load instrument into other module
					if(sndFile->ReadInstrumentFromSong(static_cast<INSTRUMENTINDEX>(modItemDropID), infoDrag->modDoc.GetSoundFile(), static_cast<INSTRUMENTINDEX>(modItemDragID)))
					{
						if(sndFile->Instruments[modItemDropID] && sndFile->Instruments[modItemDropID]->pTuning)
							modDoc->UpdateAllViews(nullptr, GeneralHint().Tunings());
					}
				}
				modDoc->UpdateAllViews(nullptr, InstrumentHint().Info().Envelope().Names());
				modDoc->UpdateAllViews(nullptr, PatternHint().Data());
				modDoc->SetModified();
				SelectItem(hItem);
			}
			return true;
		}
		break;

	case MODITEM_MIDIINSTRUMENT:
	case MODITEM_MIDIPERCUSSION:
		if((m_itemDrag.type == MODITEM_INSLIB_SAMPLE) || (m_itemDrag.type == MODITEM_INSLIB_INSTRUMENT))
		{
			if(doDrop)
			{
				mpt::PathString fullPath = InsLibGetFullPath(m_hItemDrag);
				if(modItemDrop.type == MODITEM_MIDIINSTRUMENT)
					SetMidiInstrument(modItemDropID, fullPath);
				else
					SetMidiPercussion(modItemDropID, fullPath);
			}
			return true;
		}
		break;

	default:
		break;
	}
	return false;
}


void CModTree::UpdatePlayPos(CModDoc &modDoc, Notification *pNotify)
{
	ModTreeDocInfo *info = GetDocumentInfoFromModDoc(modDoc);
	if(info == nullptr)
		return;

	const CSoundFile &sndFile = modDoc.GetSoundFile();
	ORDERINDEX nNewOrd = (pNotify) ? pNotify->order : ORDERINDEX_INVALID;
	SEQUENCEINDEX nNewSeq = sndFile.Order.GetCurrentSequenceIndex();
	if(nNewOrd != info->ordSel || nNewSeq != info->seqSel)
	{
		// Remove bold state from old item
		if(info->seqSel < info->tiOrders.size() && info->ordSel < info->tiOrders[info->seqSel].size())
			SetItemState(info->tiOrders[info->seqSel][info->ordSel], 0, mfcless::tree::Bold);

		info->ordSel = nNewOrd;
		info->seqSel = nNewSeq;
		if(info->seqSel < info->tiOrders.size() && info->ordSel < info->tiOrders[info->seqSel].size())
			SetItemState(info->tiOrders[info->seqSel][info->ordSel], mfcless::tree::Bold, mfcless::tree::Bold);
		else
			UpdateView(*info, SequenceHint().Data());
	}

	// Update sample / instrument playing status icons (will only detect instruments with samples, though)

	if(!(TrackerSettings::Instance().patternSetup & PatternSetup::LiveUpdateTreeView))
		return;
	// TODO: Is there a way to find out if the treeview is actually visible?
	/*static int nUpdateCount = 0;
	nUpdateCount++;
	if(nUpdateCount < 5) return; // don't update too often
	nUpdateCount = 0;*/

	// check whether the lists are actually visible (don't waste resources)
	const bool updateSamples = IsItemExpanded(info->hSamples), updateInstruments = IsItemExpanded(info->hInstruments);

	info->samplesPlaying.reset();
	info->instrumentsPlaying.reset();

	if(!updateSamples && !updateInstruments)
		return;

	for(const auto &chn : sndFile.m_PlayState.Chn)
	{
		if(chn.pCurrentSample != nullptr && chn.nLength != 0 && chn.IsSamplePlaying())
		{
			if(updateSamples)
			{
				for(SAMPLEINDEX nSmp = sndFile.GetNumSamples(); nSmp >= 1; nSmp--)
				{
					if(chn.pModSample == &sndFile.GetSample(nSmp))
					{
						info->samplesPlaying.set(nSmp);
						break;
					}
				}
			}
			if(updateInstruments)
			{
				for(INSTRUMENTINDEX nIns = sndFile.GetNumInstruments(); nIns >= 1; nIns--)
				{
					if(chn.pModInstrument == sndFile.Instruments[nIns])
					{
						info->instrumentsPlaying.set(nIns);
						break;
					}
				}
			}
		}
	}
	// what should be updated?
	if(updateSamples)
		UpdateView(*info, SampleHint().Info());
	if(updateInstruments)
		UpdateView(*info, InstrumentHint().Info());
}



/////////////////////////////////////////////////////////////////////////////
// CViewModTree message handlers


void CModTree::OnUpdate(CModDoc *pModDoc, UpdateHint hint, QObject *pHint)
{
	if(pHint == this)
		return;

	for(auto &[doc, docInfo] : m_docInfo)
	{
		if(doc == pModDoc || !pModDoc)
		{
			UpdateView(docInfo, hint);
			if(pModDoc)
				break;
		}
	}
}

void CModTree::OnItemExpanded()
{
	const mfcless::NotifyInfo *pnm = &CurrentNotify();
	int image = 0, selectedImage = 0;
	GetItemImage(pnm->treeItem, image, selectedImage);
	if((image == IMAGE_FOLDER) || (image == IMAGE_OPENFOLDER))
	{
		int iNewImage = pnm->delta ? IMAGE_OPENFOLDER : IMAGE_FOLDER;
		SetItemImage(pnm->treeItem, iNewImage, iNewImage);
	}
	CurrentNotify().result = true;
}


void CModTree::OnBeginDrag(QTreeWidgetItem * hItem, bool bLeft, intptr_t *pResult)
{
	if(!(m_dwStatus & TREESTATUS_DRAGGING))
	{
		bool bDrag = false;

		m_hDropWnd = NULL;
		m_hItemDrag = hItem;
		if(m_hItemDrag != NULL)
		{
			if(!ItemHasChildren(m_hItemDrag))
				SelectItem(m_hItemDrag);
		}
		m_itemDrag = GetModItem(m_hItemDrag);
		m_dragDoc = m_selectedDoc;
		switch(m_itemDrag.type)
		{
		case MODITEM_ORDER:
		case MODITEM_PATTERN:
		case MODITEM_SAMPLE:
		case MODITEM_INSTRUMENT:
		case MODITEM_SEQUENCE:
		case MODITEM_MIDIINSTRUMENT:
		case MODITEM_MIDIPERCUSSION:
		case MODITEM_INSLIB_SAMPLE:
		case MODITEM_INSLIB_INSTRUMENT:
		case MODITEM_INSLIB_SONG:
			bDrag = true;
			break;
		case MODITEM_HDR_ORDERS:
			// can we drag an order header? (only in MPTM format and if there's only one sequence)
			{
				const CModDoc *pModDoc = m_docInfo.count(m_dragDoc) ? m_dragDoc : nullptr;
				if(pModDoc && pModDoc->GetSoundFile().Order.GetNumSequences() == 1)
					bDrag = true;
			}
			break;
		default:
			if(m_itemDrag.type == MODITEM_DLSBANK_INSTRUMENT)
				bDrag = true;
		}
		if(bDrag)
		{
			m_dwStatus |= (bLeft) ? TREESTATUS_LDRAG : TREESTATUS_RDRAG;
			m_hItemDrop = NULL;
			SetCapture();
		}
	}
}


void CModTree::OnBeginDrag()
{
	const mfcless::NotifyInfo *pnmtv = &CurrentNotify();
	OnBeginDrag(pnmtv->treeItem, !pnmtv->rightButton, nullptr);
}


void CModTree::OnItemDblClk()
{
	mfcless::Point pt;
	mfcless::GetCursorPos(&pt);
	ScreenToClient(&pt);
	QTreeWidgetItem * hItem = GetSelectedItem();
	if((hItem) && (hItem == HitTest(pt)))
	{
		ExecuteItem(hItem);
	}
}


void CModTree::OnItemReturn()
{
	QTreeWidgetItem * hItem = GetSelectedItem();
	if(hItem)
		ExecuteItem(hItem);
}


void CModTree::OnItemRightClick()
{
	mfcless::Point pt, ptClient;
	uint32 flags = 0;

	mfcless::GetCursorPos(&pt);
	ptClient = pt;
	ScreenToClient(&ptClient);
	OnItemRightClick(HitTest(ptClient, &flags), pt);
}


void CModTree::OnItemRightClick(QTreeWidgetItem * hItem, mfcless::Point pt)
{
	if(m_dwStatus & TREESTATUS_LDRAG)
	{
		if(ItemHasChildren(hItem))
		{
			Expand(hItem, mfcless::tree::Toggle);
		} else
		{
			m_hItemDrop = NULL;
			m_hDropWnd = NULL;
			OnEndDrag(TREESTATUS_DRAGGING);
		}
	} else
	{
		if(m_dwStatus & TREESTATUS_DRAGGING)
		{
			m_hItemDrop = NULL;
			m_hDropWnd = NULL;
			OnEndDrag(TREESTATUS_DRAGGING);
		}
		mfcless::Menu hMenu = mfcless::Menu::CreatePopup(), hSubMenu;
		if(!hMenu)
			return;

		const CModDoc *modDoc = GetDocumentFromItem(hItem);
		const CSoundFile *sndFile = modDoc != nullptr ? &modDoc->GetSoundFile() : nullptr;
		const CInputHandler *ih = CMainFrame::GetInputHandler();

		uint32 defaultID = 0;
		bool addSeparator = false;

		const ModItem modItem = GetModItem(hItem);
		const uint32 modItemID = modItem.val1;

		SelectItem(hItem);
		switch(modItem.type)
		{
		case MODITEM_HDR_SONG:
			defaultID = ID_MODTREE_EXECUTE;
			hMenu.AppendMenu(mfcless::menu::String, defaultID, ih->GetKeyTextFromCommand(kcTreeViewOpen, UL_("&View")));
			hMenu.AppendMenu(mfcless::menu::String, ID_MODTREE_CLOSE, UL_("&Close"));
			hMenu.AppendMenu(mfcless::menu::String, ID_MODTREE_RENAME, ih->GetKeyTextFromCommand(kcTreeViewRename, UL_("Re&name")));
			hMenu.AppendMenu(mfcless::menu::String | ((!modDoc || modDoc->GetPathNameMpt().empty()) ? mfcless::menu::Grayed : 0), ID_MODTREE_OPENITEM, UL_("&Open in Explorer"));
			break;

		case MODITEM_COMMENTS:
			defaultID = ID_MODTREE_EXECUTE;
			hMenu.AppendMenu(mfcless::menu::String, defaultID, ih->GetKeyTextFromCommand(kcTreeViewOpen, UL_("&View Comments")));
			break;

		case MODITEM_ORDER:
		case MODITEM_PATTERN:
			defaultID = ID_MODTREE_EXECUTE;
			hMenu.AppendMenu(mfcless::menu::String, defaultID, ih->GetKeyTextFromCommand(kcTreeViewOpen, UL_("&Edit Pattern")));
			hMenu.AppendMenu(mfcless::menu::String, ID_MODTREE_REMOVE,
							ih->GetKeyTextFromCommand(kcTreeViewDelete, (modItem.type == MODITEM_ORDER) ? UL_("&Delete from list") : UL_("&Delete Pattern")));
			if(modItem.type == MODITEM_PATTERN && sndFile && sndFile->GetModSpecifications().hasPatternNames)
				hMenu.AppendMenu(mfcless::menu::String, ID_MODTREE_RENAME, ih->GetKeyTextFromCommand(kcTreeViewRename, UL_("Re&name Pattern")));
			else if(modItem.type == MODITEM_ORDER)
				hMenu.AppendMenu(mfcless::menu::String, ID_MODTREE_RENAME, ih->GetKeyTextFromCommand(kcTreeViewRename, UL_("&Set Pattern")));
			break;

		case MODITEM_SEQUENCE:
			if(sndFile)
			{
				bool isCurSeq = false;
				if(sndFile->GetModSpecifications().sequencesMax > 1)
				{
					if(sndFile->Order((SEQUENCEINDEX)modItemID).GetLength() == 0)
					{
						defaultID = ID_MODTREE_SWITCHTO;
					}
					isCurSeq = (sndFile->Order.GetCurrentSequenceIndex() == (SEQUENCEINDEX)modItemID);
				}

				if(!isCurSeq)
				{
					hMenu.AppendMenu(mfcless::menu::String, ID_MODTREE_SWITCHTO, UL_("&Switch to Seqeuence"));
				}
				hMenu.AppendMenu(mfcless::menu::String | (sndFile->Order.GetNumSequences() < MAX_SEQUENCES ? 0 : mfcless::menu::Grayed), ID_MODTREE_INSERT, ih->GetKeyTextFromCommand(kcTreeViewInsert, UL_("&Insert Sequence")));
				hMenu.AppendMenu(mfcless::menu::String | (sndFile->Order.GetNumSequences() < MAX_SEQUENCES ? 0 : mfcless::menu::Grayed), ID_MODTREE_DUPLICATE , ih->GetKeyTextFromCommand(kcTreeViewDuplicate,  UL_("D&uplicate Sequence")));
				hMenu.AppendMenu(mfcless::menu::String, ID_MODTREE_REMOVE, ih->GetKeyTextFromCommand(kcTreeViewDelete, UL_("&Delete Sequence")));
				hMenu.AppendMenu(mfcless::menu::String, ID_MODTREE_RENAME, ih->GetKeyTextFromCommand(kcTreeViewRename, UL_("Re&name Sequence")));
			}
			break;


		case MODITEM_HDR_ORDERS:
			if(sndFile && sndFile->GetModSpecifications().sequencesMax > 1)
			{
				hMenu.AppendMenu(mfcless::menu::String, ID_MODTREE_INSERT, ih->GetKeyTextFromCommand(kcTreeViewInsert, UL_("&Insert Sequence")));
				if(sndFile->Order.GetNumSequences() == 1)
				{
					// This is a sequence
					hMenu.AppendMenu(mfcless::menu::String, ID_MODTREE_DUPLICATE, ih->GetKeyTextFromCommand(kcTreeViewDuplicate, UL_("D&uplicate Sequence")));
					hMenu.AppendMenu(mfcless::menu::String, ID_MODTREE_RENAME, ih->GetKeyTextFromCommand(kcTreeViewRename, UL_("Re&name Sequence")));
				}
			}
			break;

		case MODITEM_SAMPLE:
			defaultID = ID_MODTREE_EXECUTE;
			hMenu.AppendMenu(mfcless::menu::String, defaultID, ih->GetKeyTextFromCommand(kcTreeViewOpen, UL_("&View Sample")));
			hMenu.AppendMenu(mfcless::menu::String, ID_MODTREE_PLAY, ih->GetKeyTextFromCommand(kcTreeViewPlay, UL_("&Play Sample")));
			hMenu.AppendMenu(mfcless::menu::String, ID_MODTREE_INSERT, ih->GetKeyTextFromCommand(kcTreeViewInsert, UL_("&Insert Sample")));
			hMenu.AppendMenu(mfcless::menu::String, ID_MODTREE_DUPLICATE, ih->GetKeyTextFromCommand(kcTreeViewDuplicate, UL_("D&uplicate Sample")));
			hMenu.AppendMenu(mfcless::menu::String, ID_MODTREE_REMOVE, ih->GetKeyTextFromCommand(kcTreeViewDelete, UL_("&Delete Sample")));
			hMenu.AppendMenu(mfcless::menu::String, ID_MODTREE_RENAME, ih->GetKeyTextFromCommand(kcTreeViewRename, UL_("Re&name Sample")));
			if(modDoc && !modDoc->GetNumInstruments())
			{
				hMenu.AppendMenu(mfcless::menu::Separator, 0, UL_(""));
				hMenu.AppendMenu((modDoc->IsSampleMuted((SAMPLEINDEX)modItemID) ? mfcless::menu::Checked : 0) | mfcless::menu::String, ID_MODTREE_MUTE, UL_("&Mute Sample"));
				hMenu.AppendMenu(mfcless::menu::String, ID_MODTREE_SOLO, UL_("S&olo Sample"));
				hMenu.AppendMenu(mfcless::menu::String, ID_MODTREE_UNMUTEALL, UL_("&Unmute all"));
			}
			if(sndFile != nullptr)
			{
				SAMPLEINDEX smpID = static_cast<SAMPLEINDEX>(modItem.val1);
				const ModSample &sample = sndFile->GetSample(smpID);
				const bool hasPath = sndFile->SampleHasPath(smpID);
				const bool menuForThisSample = (sample.HasSampleData() && sndFile->GetType() == MOD_TYPE_MPT) || hasPath;

				bool anyPath = false, anyModified = false, anyMissing = false;
				for(SAMPLEINDEX smp = 1; smp <= sndFile->GetNumSamples(); smp++)
				{
					if(sndFile->SampleHasPath(smp) && smp != smpID)
					{
						anyPath = true;
						if(sndFile->GetSample(smp).HasSampleData() && sndFile->GetSample(smp).uFlags[SMP_MODIFIED])
						{
							anyModified = true;
						}
					}
					if(sndFile->IsExternalSampleMissing(smp))
					{
						anyMissing = true;
					}
					if(anyPath && anyModified && anyMissing) break;
				}

				if(menuForThisSample || anyPath || anyModified)
				{
					hMenu.AppendMenu(mfcless::menu::Separator, 0, UL_(""));
					if(menuForThisSample) hMenu.AppendMenu(mfcless::menu::String | ((sndFile->GetType() == MOD_TYPE_MPT || hasPath) ? 0 : mfcless::menu::Grayed), ID_MODTREE_SETPATH, UL_("Set P&ath"));
					if(menuForThisSample) hMenu.AppendMenu(mfcless::menu::String | ((hasPath && sample.HasSampleData() && sample.uFlags[SMP_MODIFIED]) ? 0 : mfcless::menu::Grayed), ID_MODTREE_SAVEITEM, UL_("&Save"));
					if(anyModified) hMenu.AppendMenu(mfcless::menu::String, ID_MODTREE_SAVEALL, UL_("&Save All"));
					if(menuForThisSample) hMenu.AppendMenu(mfcless::menu::String | (hasPath ? 0 : mfcless::menu::Grayed), ID_MODTREE_RELOADITEM, UL_("&Reload"));
					if(anyPath) hMenu.AppendMenu(mfcless::menu::String, ID_MODTREE_RELOADALL, UL_("&Reload All"));
					if(anyMissing) hMenu.AppendMenu(mfcless::menu::String, ID_MODTREE_FINDMISSING, UL_("&Find Missing Samples"));
				}
			}
			break;

		case MODITEM_INSTRUMENT:
			defaultID = ID_MODTREE_EXECUTE;
			hMenu.AppendMenu(mfcless::menu::String, defaultID, ih->GetKeyTextFromCommand(kcTreeViewOpen, UL_("&View Instrument")));
			hMenu.AppendMenu(mfcless::menu::String, ID_MODTREE_PLAY, ih->GetKeyTextFromCommand(kcTreeViewPlay, UL_("&Play Instrument")));
			hMenu.AppendMenu(mfcless::menu::String, ID_MODTREE_INSERT, ih->GetKeyTextFromCommand(kcTreeViewInsert, UL_("&Insert Instrument")));
			hMenu.AppendMenu(mfcless::menu::String, ID_MODTREE_DUPLICATE, ih->GetKeyTextFromCommand(kcTreeViewDuplicate, UL_("D&uplicate Instrument")));
			hMenu.AppendMenu(mfcless::menu::String, ID_MODTREE_REMOVE, ih->GetKeyTextFromCommand(kcTreeViewDelete, UL_("&Delete Instrument")));
			hMenu.AppendMenu(mfcless::menu::String, ID_MODTREE_RENAME, ih->GetKeyTextFromCommand(kcTreeViewRename, UL_("Re&name Instrument")));
			if(modDoc)
			{
				hMenu.AppendMenu(mfcless::menu::Separator, 0, UL_(""));
				hMenu.AppendMenu((modDoc->IsInstrumentMuted((INSTRUMENTINDEX)modItemID) ? mfcless::menu::Checked : 0) | mfcless::menu::String, ID_MODTREE_MUTE, UL_("&Mute Instrument"));
				hMenu.AppendMenu(mfcless::menu::String, ID_MODTREE_SOLO, UL_("S&olo Instrument"));
				hMenu.AppendMenu(mfcless::menu::String, ID_MODTREE_UNMUTEALL, UL_("&Unmute all"));
			}
			break;

		case MODITEM_HDR_EFFECTS:
			if(sndFile && sndFile->m_loadedPlugins)
			{
				hMenu.AppendMenu(mfcless::menu::String | (AllPluginsBypassed(*sndFile, false) ? mfcless::menu::Checked : 0), ID_MODTREE_MUTE, UL_("B&ypass All Plugins"));
				if(HasEffectPlugins(*sndFile))
					hMenu.AppendMenu(mfcless::menu::String | (AllPluginsBypassed(*sndFile, true) ? mfcless::menu::Checked : 0), ID_MODTREE_MUTE_ONLY_EFFECTS, UL_("Bypass All &Effects"));
			}
			break;

		case MODITEM_EFFECT:
			defaultID = ID_MODTREE_EXECUTE;
			hMenu.AppendMenu(mfcless::menu::String, defaultID, ih->GetKeyTextFromCommand(kcTreeViewOpen, UL_("&Edit")));

			if(modDoc != nullptr)
			{
				hMenu.AppendMenu((modDoc->GetSoundFile().m_MixPlugins[modItemID].IsBypassed() ? mfcless::menu::Checked : 0) | mfcless::menu::String, ID_MODTREE_MUTE, UL_("&Bypass"));
				hMenu.AppendMenu(mfcless::menu::String, ID_MODTREE_REMOVE, ih->GetKeyTextFromCommand(kcTreeViewDelete, UL_("&Delete Plugin")));
			}
			break;

		case MODITEM_MIDIINSTRUMENT:
		case MODITEM_MIDIPERCUSSION:
			defaultID = ID_MODTREE_EXECUTE;
			hMenu.AppendMenu(mfcless::menu::String, defaultID, ih->GetKeyTextFromCommand(kcTreeViewOpen, UL_("&Map Instrument")));
			hMenu.AppendMenu(mfcless::menu::String, ID_MODTREE_PLAY, ih->GetKeyTextFromCommand(kcTreeViewPlay, UL_("&Play Instrument")));
			hMenu.AppendMenu(mfcless::menu::String, ID_MODTREE_REMOVE, UL_("&Unmap Instrument"));
			hMenu.AppendMenu(mfcless::menu::Separator, 0, UL_(""));
			[[fallthrough]];
		case MODITEM_HDR_MIDILIB:
		case MODITEM_HDR_MIDIGROUP:
			hMenu.AppendMenu(mfcless::menu::String, ID_IMPORT_MIDILIB, UL_("&Import MIDI Library"));
			hMenu.AppendMenu(mfcless::menu::String, ID_EXPORT_MIDILIB, UL_("E&xport MIDI Library"));
			addSeparator = true;
			break;

		case MODITEM_HDR_INSTRUMENTLIB:
			if(!IsSampleBrowser())
			{
				hSubMenu = AddLibraryFindAndSortMenus(hMenu);
				break;
			}
			if(!m_SongFileName.empty())
				hMenu.AppendMenu(mfcless::menu::String, ID_MODTREE_CLOSE, UL_("&Close Song"));
			[[fallthrough]];
		case MODITEM_INSLIB_FOLDER:
			defaultID = ID_MODTREE_EXECUTE;
			hMenu.AppendMenu(mfcless::menu::String, defaultID, ih->GetKeyTextFromCommand(kcTreeViewOpen, UL_("&Browse...")));
			hMenu.AppendMenu(mfcless::menu::String, ID_MODTREE_RENAME, ih->GetKeyTextFromCommand(kcTreeViewRename, UL_("Set &Path")));
			hMenu.AppendMenu(mfcless::menu::String, ID_MODTREE_OPENITEM, UL_("&Open in Explorer"));
			hSubMenu = AddLibraryFindAndSortMenus(hMenu);

			{
				auto insDir = TrackerSettings::Instance().PathInstruments.GetDefaultDir();
				auto smpDir = TrackerSettings::Instance().PathSamples.GetDefaultDir();
				if(!insDir.empty() && insDir != m_InstrLibPath)
					hMenu.AppendMenu(mfcless::menu::String, ID_MODTREE_GOTO_INSDIR, UL_("Go to &Instrument directory"));
				if(!smpDir.empty() && smpDir != insDir && smpDir != m_InstrLibPath)
					hMenu.AppendMenu(mfcless::menu::String, ID_MODTREE_GOTO_SMPDIR, UL_("Go to Sa&mple directory"));
			}
			break;

		case MODITEM_INSLIB_SONG:
			defaultID = ID_MODTREE_EXECUTE;
			hMenu.AppendMenu(mfcless::menu::String, defaultID, ih->GetKeyTextFromCommand(kcTreeViewOpen, UL_("&Browse Song...")));
			hMenu.AppendMenu(mfcless::menu::String, ID_MODTREE_OPENITEM, UL_("&Edit Song"));
			hSubMenu = AddLibraryFindAndSortMenus(hMenu);
			hMenu.AppendMenu(mfcless::menu::Separator, 0, UL_(""));
			hMenu.AppendMenu(mfcless::menu::String, ID_MODTREE_REMOVE, ih->GetKeyTextFromCommand(kcTreeViewDelete, UL_("&Delete")));
			break;

		case MODITEM_INSLIB_SAMPLE:
		case MODITEM_INSLIB_INSTRUMENT:
			defaultID = ID_MODTREE_PLAY;
			if(!m_SongFileName.empty())
			{
				hMenu.AppendMenu(mfcless::menu::String, ID_MODTREE_PLAY, ih->GetKeyTextFromCommand(kcTreeViewPlay, UL_("&Play")));
			} else
			{
				hMenu.AppendMenu(mfcless::menu::String, ID_MODTREE_PLAY, ih->GetKeyTextFromCommand(kcTreeViewPlay, UL_("&Play File")));
				hMenu.AppendMenu(mfcless::menu::Separator, 0, UL_(""));
				hMenu.AppendMenu(mfcless::menu::String, ID_MODTREE_OPENITEM, UL_("&Open in Explorer"));
				hMenu.AppendMenu(mfcless::menu::String, ID_MODTREE_REMOVE, ih->GetKeyTextFromCommand(kcTreeViewDelete, UL_("&Delete")));
			}
			hSubMenu = AddLibraryFindAndSortMenus(hMenu);
			break;

		case MODITEM_DLSBANK_FOLDER:
			defaultID = ID_SOUNDBANK_PROPERTIES;
			hMenu.AppendMenu(mfcless::menu::String, defaultID, UL_("&Properties"));
			hMenu.AppendMenu(mfcless::menu::String, ID_MODTREE_REMOVE, UL_("Re&move this bank"));
			[[fallthrough]];
		case MODITEM_NULL:
			hMenu.AppendMenu(mfcless::menu::String, ID_ADD_SOUNDBANK, UL_("Add Sound &Bank..."));
			addSeparator = true;
			break;

		case MODITEM_DLSBANK_INSTRUMENT:
			defaultID = ID_MODTREE_PLAY;
			hMenu.AppendMenu(mfcless::menu::String, ID_MODTREE_PLAY, ih->GetKeyTextFromCommand(kcTreeViewPlay, UL_("&Play Instrument")));
			break;

		default:
			break;
		}

		if(defaultID)
			hMenu.SetDefaultItem(defaultID, false);
		
		if((modItem.type == MODITEM_INSLIB_FOLDER)
			|| (modItem.type == MODITEM_INSLIB_SONG)
			|| (modItem.type == MODITEM_HDR_INSTRUMENTLIB))
		{
			if(addSeparator || defaultID)
				hMenu.AppendMenu(mfcless::menu::Separator, 0, UL_(""));
			hMenu.AppendMenu(TrackerSettings::Instance().showDirsInSampleBrowser ? (mfcless::menu::String|mfcless::menu::Checked) : mfcless::menu::String, ID_MODTREE_SHOWDIRS, UL_("Show &Directories in Sample Browser"));
			hMenu.AppendMenu((m_showAllFiles) ? (mfcless::menu::String|mfcless::menu::Checked) : mfcless::menu::String, ID_MODTREE_SHOWALLFILES, UL_("Show &All Files"));
			hMenu.AppendMenu((m_showAllFiles) ? mfcless::menu::String : (mfcless::menu::String|mfcless::menu::Checked), ID_MODTREE_SOUNDFILESONLY, UL_("Show &Sound Files"));
			addSeparator = true;
		}

		if(addSeparator || defaultID)
			hMenu.AppendMenu(mfcless::menu::Separator, 0, UL_(""));
		hMenu.AppendMenu(mfcless::menu::String, ID_MODTREE_REFRESH, UL_("&Refresh"));

		hMenu.TrackPopupMenu(mfcless::menu::LeftAlign | mfcless::menu::RightButton, pt.x + 4, pt.y, this);
		hMenu.DestroyMenu();
		if(hSubMenu)
			hSubMenu.DestroyMenu();
	}
}


mfcless::Menu CModTree::AddLibraryFindAndSortMenus(mfcless::Menu hMenu) const
{
	const CInputHandler *ih = CMainFrame::GetInputHandler();
	hMenu.AppendMenu(mfcless::menu::String, ID_OPEN_LIBRARY_FILTER, ih->GetKeyTextFromCommand(kcTreeViewFind, UL_("&Find...")));

	mfcless::Menu hSubMenu = mfcless::Menu::CreatePopup();
	hSubMenu.AppendMenu(mfcless::menu::String | (m_librarySort == LibrarySortOrder::Name ? mfcless::menu::Checked : 0), ID_MODTREE_SORT_BY_NAME, ih->GetKeyTextFromCommand(kcTreeViewSortByName, UL_("&Name")));
	hSubMenu.AppendMenu(mfcless::menu::String | (m_librarySort == LibrarySortOrder::Date ? mfcless::menu::Checked : 0), ID_MODTREE_SORT_BY_DATE, ih->GetKeyTextFromCommand(kcTreeViewSortByDate, UL_("&Date")));
	hSubMenu.AppendMenu(mfcless::menu::String | (m_librarySort == LibrarySortOrder::Size ? mfcless::menu::Checked : 0), ID_MODTREE_SORT_BY_SIZE, ih->GetKeyTextFromCommand(kcTreeViewSortBySize, UL_("&Size")));
	hMenu.AppendMenu(mfcless::menu::Popup, hSubMenu, UL_("Sort B&y"));
	return hSubMenu;
}


void CModTree::OnItemLeftClick()
{
	if(!(m_dwStatus & TREESTATUS_RDRAG))
	{
		mfcless::Point pt;
		uint32 flags = 0;
		mfcless::GetCursorPos(&pt);
		ScreenToClient(&pt);
		QTreeWidgetItem * hItem = HitTest(pt, &flags);
		if(hItem != NULL)
		{
			const ModItem modItem = GetModItem(hItem);
			const uint32 modItemID = modItem.val1;

			switch(modItem.type)
			{
			case MODITEM_INSLIB_FOLDER:
			case MODITEM_INSLIB_SONG:
				if(m_dwStatus & TREESTATUS_SINGLEEXPAND)
					ExecuteItem(hItem);
				break;

			case MODITEM_SAMPLE:
			case MODITEM_INSTRUMENT:
				{
					CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
					CChildFrame *pFrame = pMainFrm->MDIGetActive();
					if (pFrame)
					{
						pFrame->SendMessage(WM_MOD_INSTRSELECTED,
							(modItem.type == MODITEM_INSTRUMENT) ? true : false,
							(intptr_t)modItemID);
					}
				}
				break;

			case MODITEM_HDR_SONG:
				ExecuteItem(hItem);
				break;

			default:
				break;
			}
		}
	}
}


void CModTree::OnSelChanged()
{
	if(m_redrawLockCount)
		return;
	QTreeWidgetItem * hItem = GetSelectedItem();
	const auto type = GetModItem(hItem).type;
	switch(type)
	{
	case MODITEM_INSLIB_SONG:
	case MODITEM_INSLIB_SAMPLE:
	case MODITEM_INSLIB_INSTRUMENT:
		if(uint32 itemData = static_cast<uint32>(GetItemData(hItem)); itemData > 0 && itemData <= m_fileBrowserEntries.size())
		{
			const auto &entry = m_fileBrowserEntries[itemData - 1];
			m_HelpText = MPT_UFORMAT("Size: {}")(FormatFileSize(entry.size));
			if(!m_SongFile)
			{
				const QDateTime modtime = QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(entry.modtime));
				m_HelpText += MPT_UFORMAT(", last modified: {}")(mfcless::FromQString(QLocale::c().toString(modtime, QStringLiteral("dd MMM yyyy, HH:mm:ss"))));
			} else if(type != MODITEM_INSLIB_SAMPLE)
			{
				m_HelpText.clear();
			}
			CMainFrame::GetMainFrame()->SetHelpText(m_HelpText);
		}
		break;

	default:
		if(CMainFrame::GetMainFrame()->GetHelpText() == m_HelpText)
		{
			CMainFrame::GetMainFrame()->SetHelpText(UL_(""));
			m_HelpText.clear();
		}
		break;
	}
}


void CModTree::OnEndDrag(uint32 dwMask)
{
	if(m_dwStatus & dwMask)
	{
		m_dwStatus &= ~dwMask;
		if(!(m_dwStatus & TREESTATUS_DRAGGING))
		{
			ReleaseCapture();
			setCursor(CMainFrame::curArrow);
			SelectDropTarget(nullptr);
			if(m_hItemDrop != nullptr)
			{
				CanDrop(m_hItemDrop, true);
			} else if(m_hDropWnd)
			{
				DRAGONDROP dropinfo;
				mpt::PathString fullPath;
				if(GetDropInfo(dropinfo, fullPath))
				{
					if(dropinfo.dropType == DRAGONDROP_SONG)
					{
						theApp.OpenDocumentFile(fullPath);
					} else
					{
						mfcless::SendModMessage(m_hDropWnd, WM_MOD_DRAGONDROPPING, true, (intptr_t)&dropinfo);
					}
				}
			}
		}
	}
}


void CModTree::OnLButtonUp(uint32 nFlags, mfcless::Point point)
{
	OnEndDrag(TREESTATUS_LDRAG);
	Window::OnLButtonUp(nFlags, point);
}


void CModTree::OnRButtonUp(uint32 nFlags, mfcless::Point point)
{
	OnEndDrag(TREESTATUS_RDRAG);
	Window::OnRButtonUp(nFlags, point);
}


void CModTree::OnXButtonUp(uint32 nFlags, uint32 nButton, mfcless::Point point)
{
	bool isSampleBrowser = IsSampleBrowser();
	if(!isSampleBrowser)
	{
		// In the upper panel, only do folder navigation if the mouse cursor is somewhere below the "Instrument Library" item
		mfcless::Rect rect;
		GetItemRect(m_hInsLib, rect, false);
		if(point.y > rect.top)
			isSampleBrowser = true;
	}
	if(isSampleBrowser)
	{
		if(nButton == mfcless::XButton::Back)
		{
			InstrumentLibraryChDir(P_(".."), !m_SongFileName.empty());
		} else if(nButton == mfcless::XButton::Forward)
		{
			const auto &previousPath = CMainFrame::GetMainFrame()->GetUpperTreeview()->m_previousPath;
			InstrumentLibraryChDir(previousPath, mpt::native_fs{}.is_file(m_InstrLibPath + previousPath));
		}
	}
	Window::OnXButtonUp(nFlags, nButton, point);
}


void CModTree::OnMouseMove(uint32 nFlags, mfcless::Point point)
{
	if(m_dwStatus & TREESTATUS_DRAGGING)
	{
		QTreeWidgetItem * hItem;
		uint32 flags = 0;

		// Bug?
		if(!(nFlags & (mfcless::MouseKey::Left | mfcless::MouseKey::Right)))
		{
			m_itemDrag = ModItem(MODITEM_NULL);
			m_hItemDrag = NULL;
			OnEndDrag(TREESTATUS_DRAGGING);
			return;
		}
		CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
		if(pMainFrm)
		{
			mfcless::Rect rect;
			GetClientRect(&rect);
			if(rect.PtInRect(point))
			{
				m_hDropWnd = this;
				bool bCanDrop = CanDrop(HitTest(point, &flags), false);
				setCursor((bCanDrop) ? CMainFrame::curDragging : CMainFrame::curNoDrop2);
			} else
			{
				mfcless::Point screenPt = point;
				ClientToScreen(&screenPt);
				QWidget * hwnd = DropTargetFromPoint(screenPt);
				if(hwnd != m_hDropWnd)
				{
					bool canDrop = false;
					m_hDropWnd = hwnd;
					if(hwnd == this)
					{
						canDrop = true;
					} else if(hwnd != NULL)
					{
						DRAGONDROP dropinfo;
						mpt::PathString fullPath;
						if(GetDropInfo(dropinfo, fullPath))
						{
							if(dropinfo.dropType == DRAGONDROP_SONG)
							{
								canDrop = true;
							} else if(mfcless::SendModMessage(hwnd, WM_MOD_DRAGONDROPPING, false, (intptr_t)&dropinfo))
							{
								canDrop = true;
							}
						}
					}
					setCursor(canDrop ? CMainFrame::curDragging : CMainFrame::curNoDrop);
					if(canDrop)
					{
						if(GetDropHilightItem() != m_hItemDrag)
						{
							SelectDropTarget(m_hItemDrag);
						}
						m_hItemDrop = NULL;
						return;
					}
				}
			}

			if((point.x >= -1) && (point.x <= rect.right + HighDPISupport::GetSystemMetrics(HighDPISupport::SystemMetric::CxVScroll, this)))
			{
				if(point.y <= 0)
				{
					QTreeWidgetItem * hfirst = GetFirstVisibleItem();
					if(hfirst != NULL)
					{
						QTreeWidgetItem * hprev = GetPrevVisibleItem(hfirst);
						if(hprev != NULL)
							scrollToItem(hprev, QAbstractItemView::PositionAtTop);
					}
				} else if(point.y >= rect.bottom - 1)
				{
					hItem = HitTest(point, &flags);
					QTreeWidgetItem * hNext = GetNextItem(hItem, mfcless::tree::NextVisible);
					if(hNext != NULL)
					{
						EnsureVisible(hNext);
					}
				}
			}
			if((hItem = HitTest(point, &flags)) != NULL)
			{
				SelectDropTarget(hItem);
				m_hItemDrop = hItem;
			}
		}
	}
	Window::OnMouseMove(nFlags, point);
}


void CModTree::OnRefreshTree()
{
	mfcless::BeginWaitCursor();
	for(auto &doc : m_docInfo)
	{
		UpdateView(doc.second, UpdateHint().ModType());
	}
	RefreshMidiLibrary();
	RefreshDlsBanks();
	RefreshInstrumentLibrary();
	mfcless::EndWaitCursor();
}


void CModTree::OnExecuteItem()
{
	ExecuteItem(GetSelectedItem());
}


void CModTree::OnDeleteTreeItem()
{
	DeleteTreeItem(GetSelectedItem(), CInputHandler::ShiftPressed());
}


void CModTree::OnPlayTreeItem()
{
	PlayItem(GetSelectedItem(), NOTE_MIDDLEC);
}


void CModTree::OnOpenTreeItem()
{
	OpenTreeItem(GetSelectedItem());
}


void CModTree::OnMuteTreeItem()
{
	QTreeWidgetItem * hItem = GetSelectedItem();

	const ModItem modItem = GetModItem(hItem);
	const uint32 modItemID = modItem.val1;

	ModTreeDocInfo *info = GetDocumentInfoFromItem(hItem);
	if(info)
	{
		CModDoc &modDoc = info->modDoc;
		if((modItem.type == MODITEM_SAMPLE) && !modDoc.GetNumInstruments())
		{
			modDoc.MuteSample((SAMPLEINDEX)modItemID, !modDoc.IsSampleMuted((SAMPLEINDEX)modItemID));
			UpdateView(*info, SampleHint((SAMPLEINDEX)modItemID).Info().Names());
		} else if((modItem.type == MODITEM_INSTRUMENT) && modDoc.GetNumInstruments())
		{
			modDoc.MuteInstrument((INSTRUMENTINDEX)modItemID, !modDoc.IsInstrumentMuted((INSTRUMENTINDEX)modItemID));
			UpdateView(*info, InstrumentHint((INSTRUMENTINDEX)modItemID).Info().Names());
		} else if(modItem.type == MODITEM_EFFECT)
		{
			IMixPlugin *pPlugin = modDoc.GetSoundFile().m_MixPlugins[modItemID].pMixPlugin;
			if(pPlugin == nullptr)
				return;
			pPlugin->ToggleBypass();
			if(modDoc.GetSoundFile().GetModSpecifications().supportsPlugins)
				modDoc.SetModified();
			//UpdateView(*info, PluginHint(static_cast<PLUGINDEX>(modItemID + 1)));
		} else if(modItem.type == MODITEM_HDR_EFFECTS)
		{
			auto &sndFile = modDoc.GetSoundFile();
			BypassAllPlugins(sndFile, !AllPluginsBypassed(sndFile, false), false);
		}
	}
}


void CModTree::OnMuteOnlyEffects()
{
	QTreeWidgetItem * hItem = GetSelectedItem();

	const ModItem modItem = GetModItem(hItem);

	ModTreeDocInfo *info = GetDocumentInfoFromItem(hItem);
	if(info)
	{
		CModDoc &modDoc = info->modDoc;
		if(modItem.type == MODITEM_HDR_EFFECTS)
		{
			auto &sndFile = modDoc.GetSoundFile();
			BypassAllPlugins(sndFile, !AllPluginsBypassed(sndFile, true), true);
		}
	}
}


void CModTree::OnSoloTreeItem()
{
	QTreeWidgetItem * hItem = GetSelectedItem();

	const ModItem modItem = GetModItem(hItem);
	const uint32 modItemID = modItem.val1;

	ModTreeDocInfo *info = GetDocumentInfoFromItem(hItem);
	if(info)
	{
		CModDoc &modDoc = info->modDoc;
		INSTRUMENTINDEX nInstruments = modDoc.GetNumInstruments();
		if((modItem.type == MODITEM_SAMPLE) && (!nInstruments))
		{
			for(SAMPLEINDEX nSmp = 1; nSmp <= modDoc.GetNumSamples(); nSmp++)
			{
				modDoc.MuteSample(nSmp, nSmp != modItemID);
			}
			UpdateView(*info, SampleHint().Info().Names());
		} else if((modItem.type == MODITEM_INSTRUMENT) && (nInstruments))
		{
			for(INSTRUMENTINDEX nIns = 1; nIns <= nInstruments; nIns++)
			{
				modDoc.MuteInstrument(nIns, nIns != modItemID);
			}
			UpdateView(*info, InstrumentHint().Info().Names());
		}
	}
}


void CModTree::OnUnmuteAllTreeItem()
{
	QTreeWidgetItem * hItem = GetSelectedItem();

	const ModItem modItem = GetModItem(hItem);

	ModTreeDocInfo *info = GetDocumentInfoFromItem(hItem);
	if(info)
	{
		CModDoc &modDoc = info->modDoc;
		if((modItem.type == MODITEM_SAMPLE) || (modItem.type == MODITEM_INSTRUMENT))
		{
			for(SAMPLEINDEX nSmp = 1; nSmp <= modDoc.GetNumSamples(); nSmp++)
			{
				modDoc.MuteSample(nSmp, false);
			}
			UpdateView(*info, SampleHint().Info().Names());
			for(INSTRUMENTINDEX nIns = 1; nIns <= modDoc.GetNumInstruments(); nIns++)
			{
				modDoc.MuteInstrument(nIns, false);
			}
			UpdateView(*info, InstrumentHint().Info().Names());
		}
	}
}


bool CModTree::HasEffectPlugins(const CSoundFile &sndFile)
{
	for(const auto &plugin : sndFile.m_MixPlugins)
	{
		if(!plugin.pMixPlugin)
			continue;
		if(!plugin.pMixPlugin->IsInstrument())
			return true;
	}
	return false;

}


bool CModTree::AllPluginsBypassed(const CSoundFile &sndFile, bool onlyEffects)
{
	for(const auto &plugin : sndFile.m_MixPlugins)
	{
		if(!plugin.pMixPlugin)
			continue;
		if(onlyEffects && plugin.pMixPlugin->IsInstrument())
			continue;
		if(!plugin.IsBypassed())
			return false;
	}
	return true;
}


void CModTree::BypassAllPlugins(CSoundFile &sndFile, bool bypass, bool onlyEffects)
{
	bool modified = false;
	for(auto &plugin : sndFile.m_MixPlugins)
	{
		if(!plugin.pMixPlugin)
			continue;
		if(onlyEffects && plugin.pMixPlugin->IsInstrument())
			continue;
		if(plugin.IsBypassed() != bypass)
		{
			plugin.pMixPlugin->Bypass(bypass);
			modified = true;
		}
	}
	if(modified && sndFile.GetModSpecifications().supportsPlugins && sndFile.GetpModDoc())
		sndFile.GetpModDoc()->SetModified();
}


// Helper function for generating an insert vector for samples/instruments/sequences
template <typename T>
static std::vector<T> GenerateInsertVector(size_t howMany, size_t insertPos, T insertId, T startId)
{
	std::vector<T> newOrder(howMany);
	std::iota(newOrder.begin(), newOrder.end(), startId);
	newOrder.insert(newOrder.begin() + insertPos, insertId);
	return newOrder;
}


void CModTree::InsertOrDupItem(bool insert)
{
	QTreeWidgetItem * hItem = GetSelectedItem();

	const ModItem modItem = GetModItem(hItem);
	const uint32 modItemID = modItem.val1;

	ModTreeDocInfo *info = GetDocumentInfoFromItem(hItem);
	if(info)
	{
		CModDoc &modDoc = info->modDoc;
		CSoundFile &sndFile = modDoc.GetSoundFile();
		if(modItem.type == MODITEM_SEQUENCE || modItem.type == MODITEM_HDR_ORDERS)
		{
			// Duplicate / insert sequence
			const SEQUENCEINDEX newIndex = (modItem.type == MODITEM_HDR_ORDERS) ? sndFile.Order.GetNumSequences() : static_cast<SEQUENCEINDEX>(modItemID + 1);
			std::vector<SEQUENCEINDEX> newOrder = GenerateInsertVector<SEQUENCEINDEX>(sndFile.Order.GetNumSequences(), newIndex, static_cast<SEQUENCEINDEX>(insert ? SEQUENCEINDEX_INVALID : modItemID), 0);
			if(modDoc.ReArrangeSequences(newOrder) != SEQUENCEINDEX_INVALID)
			{
				sndFile.Order.SetSequence(newIndex);
				if(const auto name = sndFile.Order().GetName(); !insert && !name.empty())
					sndFile.Order().SetName(name + U_(" (Copy)"));
				modDoc.UpdateAllViews(nullptr, SequenceHint(SEQUENCEINDEX_INVALID).Names().Data());
				modDoc.SetModified();
			} else
			{
				Reporting::Error("Maximum number of sequences reached.");
			}
		} else if(modItem.type == MODITEM_SAMPLE)
		{
			// Duplicate / insert sample
			std::vector<SAMPLEINDEX> newOrder = GenerateInsertVector<SAMPLEINDEX>(sndFile.GetNumSamples(), modItemID, static_cast<SAMPLEINDEX>(insert ? 0 : modItemID), 1);
			if(modDoc.ReArrangeSamples(newOrder) != SAMPLEINDEX_INVALID)
			{
				modDoc.SetModified();
				modDoc.UpdateAllViews(nullptr, SampleHint().Info().Data().Names());
				modDoc.UpdateAllViews(nullptr, PatternHint().Data());
			} else
			{
				Reporting::Error("Maximum number of samples reached.");
			}
		} else if(modItem.type == MODITEM_INSTRUMENT)
		{
			// Duplicate / insert instrument
			std::vector<INSTRUMENTINDEX> newOrder = GenerateInsertVector<INSTRUMENTINDEX>(sndFile.GetNumInstruments(), modItemID, static_cast<INSTRUMENTINDEX>(insert ? 0 : modItemID), 1);
			if(modDoc.ReArrangeInstruments(newOrder) != INSTRUMENTINDEX_INVALID)
			{
				modDoc.UpdateAllViews(nullptr, InstrumentHint().Info().Envelope().Names());
				modDoc.UpdateAllViews(nullptr, PatternHint().Data());
				modDoc.SetModified();
			} else
			{
				Reporting::Error("Maximum number of instruments reached.");
			}
		}
	}
}


void CModTree::OnSwitchToTreeItem()
{
	QTreeWidgetItem * hItem = GetSelectedItem();

	const ModItem modItem = GetModItem(hItem);

	CModDoc *pModDoc = GetDocumentFromItem(hItem);
	if(pModDoc && (modItem.type == MODITEM_SEQUENCE))
	{
		pModDoc->ActivateView(IDD_CONTROL_PATTERNS, uint32(modItem.val1 << SEQU_SHIFT) | SEQU_INDICATOR);
	}
}


void CModTree::OnSetItemPath()
{
	QTreeWidgetItem * hItem = GetSelectedItem();

	const ModItem modItem = GetModItem(hItem);
	CModDoc *pModDoc = GetDocumentFromItem(hItem);

	if(pModDoc && modItem.val1)
	{
		SAMPLEINDEX smpID = static_cast<SAMPLEINDEX>(modItem.val1);
		const mpt::PathString path = pModDoc->GetSoundFile().GetSamplePath(smpID);
		FileDialog dlg = OpenFileDialog()
			.ExtensionFilter(U_("All Samples|*.wav;*.flac|All files(*.*)|*.*||"));	// Only show samples that we actually can save as well.
		if(path.empty())
			dlg.WorkingDirectory(TrackerSettings::Instance().PathSamples.GetWorkingDir());
		else
			dlg.DefaultFilename(path);
		if(!dlg.Show())
			return;
		TrackerSettings::Instance().PathSamples.SetWorkingDir(dlg.GetWorkingDirectory());

		if(dlg.GetFirstFile() != pModDoc->GetSoundFile().GetSamplePath(smpID))
		{
			pModDoc->GetSoundFile().SetSamplePath(smpID, dlg.GetFirstFile());
			pModDoc->SetModified();
		}
		OnReloadItem();
	}
}


void CModTree::OnSaveItem()
{
	QTreeWidgetItem * hItem = GetSelectedItem();

	const ModItem modItem = GetModItem(hItem);
	CModDoc *pModDoc = GetDocumentFromItem(hItem);

	if(pModDoc && modItem.val1)
	{
		SAMPLEINDEX smpID = static_cast<SAMPLEINDEX>(modItem.val1);
		pModDoc->SaveSample(smpID);
		if(pModDoc)
			pModDoc->UpdateAllViews(nullptr, SampleHint(smpID).Info());
		OnRefreshTree();
	}
}


void CModTree::OnSaveAll()
{
	CModDoc *pModDoc = GetDocumentFromItem(GetSelectedItem());
	if(pModDoc != nullptr)
	{
		pModDoc->SaveAllSamples(false);
		if(pModDoc)
			pModDoc->UpdateAllViews(nullptr, SampleHint().Info());
		OnRefreshTree();
	}
}


void CModTree::OnReloadItem()
{
	QTreeWidgetItem * hItem = GetSelectedItem();

	const ModItem modItem = GetModItem(hItem);
	CModDoc *pModDoc = GetDocumentFromItem(hItem);

	if(pModDoc && modItem.val1)
	{
		SAMPLEINDEX smpID = static_cast<SAMPLEINDEX>(modItem.val1);
		CSoundFile &sndFile = pModDoc->GetSoundFile();
		pModDoc->GetSampleUndo().PrepareUndo(smpID, sundo_replace, "Replace");
		if(!sndFile.LoadExternalSample(smpID, sndFile.GetSamplePath(smpID)))
		{
			pModDoc->GetSampleUndo().RemoveLastUndoStep(smpID);
			Reporting::Error(UL_("Unable to load sample:\n") + sndFile.GetSamplePath(smpID).ToUnicode());
		} else
		{
			if(!sndFile.GetSample(smpID).uFlags[SMP_KEEPONDISK])
			{
				pModDoc->SetModified();
			}
			pModDoc->UpdateAllViews(nullptr, SampleHint(smpID).Info().Data().Names());
		}

		OnRefreshTree();
	}
}


void CModTree::OnReloadAll()
{
	CModDoc *pModDoc = GetDocumentFromItem(GetSelectedItem());
	if(pModDoc != nullptr)
	{
		CSoundFile &sndFile = pModDoc->GetSoundFile();
		bool anyMissing = false;
		for(SAMPLEINDEX smp = 1; smp <= sndFile.GetNumSamples(); smp++)
		{
			const mpt::PathString &path = sndFile.GetSamplePath(smp);
			if(path.empty())
				continue;

			pModDoc->GetSampleUndo().PrepareUndo(smp, sundo_replace, "Replace");
			if(!sndFile.LoadExternalSample(smp, path))
			{
				pModDoc->GetSampleUndo().RemoveLastUndoStep(smp);
				anyMissing = true;
			} else
			{
				if(!sndFile.GetSample(smp).uFlags[SMP_KEEPONDISK])
				{
					pModDoc->SetModified();
				}
			}
		}
		pModDoc->UpdateAllViews(nullptr, SampleHint().Info().Data().Names());
		OnRefreshTree();
		if(anyMissing)
		{
			OnFindMissing();
		}
	}
}


// Find missing external samples
void CModTree::OnFindMissing()
{
	CModDoc *pModDoc = GetDocumentFromItem(GetSelectedItem());
	if(pModDoc == nullptr)
	{
		return;
	}
	MissingExternalSamplesDlg dlg(*pModDoc, CMainFrame::GetMainFrame());
	dlg.DoModal();
}


void CModTree::OnAddDlsBank()
{
	CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
	if(pMainFrm)
		pMainFrm->OnAddDlsBank();
}


void CModTree::OnImportMidiLib()
{
	CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
	if(pMainFrm)
		pMainFrm->OnImportMidiLib();
}


void CModTree::OnExportMidiLib()
{
	FileDialog dlg = SaveFileDialog()
		.DefaultExtension(U_("ini"))
		.DefaultFilename(P_("mptrack.ini"))
		.ExtensionFilter(U_("Text and INI files (*.txt,*.ini)|*.txt;*.ini|All Files (*.*)|*.*||"));
	if(!dlg.Show()) return;

	CTrackApp::ExportMidiConfig(dlg.GetFirstFile());
}


///////////////////////////////////////////////////////////////////////
// Drop support

uint32 CModTree::OnDragEnter(const QMimeData *, uint32, mfcless::Point)
{
	return mfcless::drop::Link;
}


uint32 CModTree::OnDragOver(const QMimeData *, uint32, mfcless::Point point)
{
	uint32 flags = 0;
	QTreeWidgetItem * hItem = HitTest(point, &flags);

	const ModItem modItem = GetModItem(hItem);

	switch(modItem.type)
	{
	case MODITEM_MIDIINSTRUMENT:
	case MODITEM_MIDIPERCUSSION:
		if(hItem != GetDropHilightItem())
		{
			SelectDropTarget(hItem);
			EnsureVisible(hItem);
		}
		m_hItemDrag = hItem;
		m_itemDrag = modItem;
		return mfcless::drop::Link;
	// Folders:
	case MODITEM_HDR_MIDILIB:
	case MODITEM_HDR_MIDIGROUP:
		EnsureVisible(GetChildItem(hItem));
		break;
	default:
		break;
	}
	return mfcless::drop::None;
}


bool CModTree::OnDrop(const QMimeData *pDataObject, uint32, mfcless::Point)
{
	bool bOk = false;

	if(!pDataObject || !pDataObject->hasUrls())
		return false;
	const QList<QUrl> urls = pDataObject->urls();
	if(!urls.isEmpty() && urls.front().isLocalFile())
	{
		const mpt::PathString fileName = mpt::PathString::FromUnicode(mfcless::FromQString(urls.front().toLocalFile()));
		{
			switch(m_itemDrag.type)
			{
			case MODITEM_MIDIINSTRUMENT:
				bOk = SetMidiInstrument(m_itemDrag.val1, fileName);
				break;
			case MODITEM_MIDIPERCUSSION:
				bOk = SetMidiPercussion(m_itemDrag.val1, fileName);
				break;
			default:
				break;
			}
		}
	}
	// After dropping some file on the MIDI library, we will need to do this or you
	// won't be able to select a new item until you started a new (internal) dragondrop.
	if(m_hItemDrag)
	{
		OnBeginDrag(m_hItemDrag, true, nullptr);
		OnEndDrag(TREESTATUS_DRAGGING);
	}
	m_itemDrag = ModItem(MODITEM_NULL);
	m_hItemDrag = NULL;
	return bOk;
}


void CModTree::OnRefreshInstrLib()
{
	mfcless::BeginWaitCursor();
	RefreshInstrumentLibrary();
	mfcless::EndWaitCursor();
}


void CModTree::OnOpenInstrumentLibraryFilter()
{
	static_cast<CModTreeBar*>(parentWidget())->StartTreeFilter(*this);
}


void CModTree::OnShowDirectories()
{
	TrackerSettings::Instance().showDirsInSampleBrowser = !TrackerSettings::Instance().showDirsInSampleBrowser;
	OnRefreshInstrLib();
}


void CModTree::OnShowAllFiles()
{
	if(!m_showAllFiles)
	{
		m_showAllFiles = true;
		OnRefreshInstrLib();
	}
}


void CModTree::OnShowSoundFiles()
{
	if(m_showAllFiles)
	{
		m_showAllFiles = false;
		OnRefreshInstrLib();
	}
}


void CModTree::OnGotoInstrumentDir()
{
	SetFullInstrumentLibraryPath(TrackerSettings::Instance().PathInstruments.GetDefaultDir());
}


void CModTree::OnGotoSampleDir()
{
	SetFullInstrumentLibraryPath(TrackerSettings::Instance().PathSamples.GetDefaultDir());
}


void CModTree::OnSoundBankProperties()
{
	const ModItem modItem = GetModItem(GetSelectedItem());
	if(modItem.type == MODITEM_DLSBANK_FOLDER
	   && modItem.val1 < CTrackApp::gpDLSBanks.size() && CTrackApp::gpDLSBanks[modItem.val1])
	{
		CSoundBankProperties dlg(*CTrackApp::gpDLSBanks[modItem.val1], this);
		dlg.DoModal();
	}
}


intptr_t CModTree::OnCustomKeyMsg(uintptr_t wParam, intptr_t /*lParam*/)
{
	CMainFrame *pMainFrm = CMainFrame::GetMainFrame();

	ModCommand::NOTE note = NOTE_NONE;
	const bool start = wParam >= kcTreeViewStartNotes && wParam <= kcTreeViewEndNotes;
	const bool stop = wParam >= kcTreeViewStartNoteStops && wParam <= kcTreeViewEndNoteStops && !IsSampleBrowser();

	switch(wParam)
	{
	case kcContextMenu:
		if(QTreeWidgetItem * item = GetSelectedItem())
		{
			mfcless::Rect rect;
			GetItemRect(item, &rect, false);
			ClientToScreen(&rect);
			OnItemRightClick(item, rect.TopLeft() + mfcless::Point{ rect.Height() / 2, rect.Height() / 2 });
			return wParam;
		}
		break;

	case kcTreeViewStopPreview:
		note = NOTE_NOTECUT;
		break;

	case kcTreeViewSwitchViews:
		// Tab: Switch between folder and file view.
		GetOtherView()->setFocus();
		return wParam;

	case kcTreeViewOpen:
		if(QTreeWidgetItem * hItem = GetSelectedItem(); hItem)
		{
			if(!ExecuteItem(hItem))
			{
				if(ItemHasChildren(hItem))
				{
					Expand(hItem, mfcless::tree::Toggle);
				}
			}
		}
		return wParam;

	case kcTreeViewPlay:
		OnPlayTreeItem();
		return wParam;

	case kcTreeViewInsert:
	case kcTreeViewDuplicate:
		InsertOrDupItem(wParam == kcTreeViewInsert);
		return wParam;

	case kcTreeViewDelete:
	case kcTreeViewDeletePermanently:
		DeleteTreeItem(GetSelectedItem(), wParam == kcTreeViewDeletePermanently);
		return wParam;

	case kcTreeViewFolderUp:
		// Backspace: Go up one directory
		if(GetParentRootItem(GetSelectedItem()) == m_hInsLib || IsSampleBrowser())
		{
			InstrumentLibraryChDir(P_(".."), !m_SongFileName.empty());
			return wParam;
		}
		return kcNull;

	case kcTreeViewFind:
		OnOpenInstrumentLibraryFilter();
		return wParam;

	case kcTreeViewRename:
	case kcTreeViewSendToEditorInsertNew:
		if(QTreeWidgetItem * hItem = GetSelectedItem(); hItem)
		{
			const ModItem modItem = GetModItem(hItem);
			static constexpr ModItemType instrumentTypes[] = {MODITEM_INSLIB_SAMPLE, MODITEM_INSLIB_INSTRUMENT, MODITEM_MIDIINSTRUMENT, MODITEM_MIDIPERCUSSION, MODITEM_DLSBANK_INSTRUMENT};
			if(mpt::contains(instrumentTypes, modItem.type))
			{
				// Load sample into currently selected (or new) sample or instrument slot
				CModScrollView *view = static_cast<CModScrollView *>(CMainFrame::GetMainFrame()->GetActiveView());
				if(view)
				{
					const char *className = view->GetRuntimeClass()->m_lpszClassName;
					const bool isSampleView = !strcmp("CViewSample", className);
					const bool isInstrumentView = !strcmp("CViewInstrument", className);
					mpt::PathString fullPath = InsLibGetFullPath(hItem);
					DRAGONDROP dropInfo;
					m_hItemDrag = hItem;
					m_itemDrag = modItem;
					if((isSampleView || isInstrumentView) && GetDropInfo(dropInfo, fullPath))
					{
						dropInfo.insertType = (wParam == kcTreeViewSendToEditorInsertNew) ? DRAGONDROP::InsertType::InsertNew : DRAGONDROP::InsertType::Replace;
						mfcless::SendModMessage(view, WM_MOD_DRAGONDROPPING, true, reinterpret_cast<intptr_t>(&dropInfo));
						// In case a message box like "create instrument for sample?" showed up
						setFocus();
					}
				}
			} else if(!IsSampleBrowser() && wParam != kcTreeViewSendToEditorInsertNew)
			{
				EditLabel(hItem);
			}
		}
		return wParam;

	case kcTreeViewSortByName:
		OnSortByName();
		return wParam;
	case kcTreeViewSortByDate:
		OnSortByDate();
		return wParam;
	case kcTreeViewSortBySize:
		OnSortBySize();
		return wParam;

	default:
		if(start || stop)
		{
			const ModItem modItem = GetModItem(GetSelectedItem());
			CModDoc *modDoc = m_docInfo.count(m_selectedDoc) ? m_selectedDoc : nullptr;
			const int noteOffset = static_cast<int>(wParam - (start ? kcTreeViewStartNotes : kcTreeViewStartNoteStops));
			note = static_cast<ModCommand::NOTE>(Clamp(NOTE_MIN + pMainFrm->GetBaseOctave() * 12 + noteOffset, NOTE_MIN, NOTE_MAX));
			if(modDoc && modItem.type == MODITEM_INSTRUMENT)
				note = modDoc->GetNoteWithBaseOctave(noteOffset, static_cast<INSTRUMENTINDEX>(modItem.val1));
		}
		break;
	}

	if(note != NOTE_NONE)
	{
		if(stop)
			note |= 0x80;

		if(PlayItem(GetSelectedItem(), note))
			return wParam;
		else
			return kcNull;
	}

	return kcNull;
}


intptr_t CModTree::OnMidiMsg(uintptr_t midiData_, intptr_t)
{
	uint32 midiData = static_cast<uint32>(midiData_);
	// Handle MIDI messages assigned to shortcuts
	CInputHandler *ih = CMainFrame::GetInputHandler();
	if(ih->HandleMIDIMessage(kCtxViewTree, midiData) == kcNull)
	    ih->HandleMIDIMessage(kCtxAllContexts, midiData);

	uint8 midiByte1 = MIDIEvents::GetDataByte1FromEvent(midiData);
	int volume;
	switch(MIDIEvents::GetTypeFromEvent(midiData))
	{
	case MIDIEvents::evNoteOn:
		volume = MIDIEvents::GetDataByte2FromEvent(midiData);
		if(volume > 0)
		{
			PlayItem(GetSelectedItem(), midiByte1 + NOTE_MIN, Util::muldivr(volume, 256, 127));
			return 1;
		}
		[[fallthrough]];
	case MIDIEvents::evNoteOff:
		PlayItem(GetSelectedItem(), NOTE_NOTECUT);
		return 1;
	default:
		return 0;
	}
}


void CModTree::OnKillFocus(QWidget *pNewWnd)
{
	if(CMainFrame::GetMainFrame()->GetHelpText() == m_HelpText)
	{
		CMainFrame::GetMainFrame()->SetHelpText(UL_(""));
		m_HelpText.clear();
	}
	Window::OnKillFocus(pNewWnd);
	CMainFrame::GetMainFrame()->m_bModTreeHasFocus = false;
	// Required to immediately redirect MIDI input focus after drag&drop from tree view to editor
	if(pNewWnd != nullptr)
		CMainFrame::GetMainFrame()->SetMidiRecordWnd(pNewWnd);
}


void CModTree::OnSetFocus(QWidget *pOldWnd)
{
	Window::OnSetFocus(pOldWnd);
	CMainFrame::GetMainFrame()->m_bModTreeHasFocus = true;
	CMainFrame::GetMainFrame()->SetMidiRecordWnd(this);
}


bool CModTree::IsItemExpanded(QTreeWidgetItem * hItem) const
{
	// checks if a treeview item is expanded.
	if(hItem == nullptr)
		return false;
	return GetItemState(hItem, mfcless::tree::Expanded) != 0;
}


void CModTree::OnCloseItem()
{
	QTreeWidgetItem * hItem = GetSelectedItem();
	if(hItem == m_hInsLib && !m_SongFileName.empty())
	{
		InstrumentLibraryChDir(P_(".."), true);
		return;
	}
	CModDoc *pModDoc = GetDocumentFromItem(hItem);
	if(pModDoc == nullptr)
		return;
	// Spam our message to the first available view
	if(pModDoc->GetViews().empty())
		return;
	mfcless::DocumentView *pView = pModDoc->GetViews().front();
	if(pView)
		mfcless::PostCommand(pView->ViewWidget(), ID_FILE_CLOSE);
}


// Delete all children of a tree item
void CModTree::DeleteChildren(QTreeWidgetItem * hItem)
{
	if(hItem != nullptr)
	{
		QTreeWidgetItem * hChildItem;
		while((hChildItem = GetChildItem(hItem)) != nullptr)
		{
			DeleteItem(hChildItem);
		}
	}
}


// Get the n-th child of a tree node
QTreeWidgetItem * CModTree::GetNthChildItem(QTreeWidgetItem * hItem, int index) const
{
	QTreeWidgetItem * hChildItem = nullptr;
	if(hItem != nullptr && ItemHasChildren(hItem))
	{
		hChildItem = GetChildItem(hItem);
		while(index-- > 0)
		{
			hChildItem = GetNextSiblingItem(hChildItem);
		}
	}
	return hChildItem;
}


// Gets the root parent of an item, i.e. if C is a child of B and B is a child of A, GetParentRootItem(C) returns A.
// A root item is considered to be its own parent, i.e. the returned value is only ever NULL if the input value was NULL.
QTreeWidgetItem * CModTree::GetParentRootItem(QTreeWidgetItem * hItem) const
{
	while(hItem != nullptr)
	{
		QTreeWidgetItem *const h = GetParentItem(hItem);
		if(h == nullptr || h == hItem)
			break;
		hItem = h;
	}
	return hItem;
}


void CModTree::OnRenameItem()
{
	EditLabel(GetSelectedItem());
}


// Editing sample, instrument, order, pattern, etc. labels
void CModTree::OnBeginLabelEdit()
{
	QTreeWidgetItem *editItem = CurrentNotify().treeItem;
	// mfcless: the editor does not exist yet; its text and length are applied in OnLabelEditorReady.
	struct
	{
		void setMaxLength(int length) { maxLength = length; }
		int maxLength = 0;
	} editSettings, *editCtrl = &editSettings;

	const ModItem modItem = GetModItem(editItem);
	const CModDoc *modDoc = modItem.IsSongItem() ? GetDocumentFromItem(editItem) : nullptr;

	mpt::ustring text;
	m_doLabelEdit = false;

	if(modDoc != nullptr)
	{
		const CSoundFile &sndFile = modDoc->GetSoundFile();
		const CModSpecifications &modSpecs = sndFile.GetModSpecifications();

		switch(modItem.type)
		{
		case MODITEM_HDR_SONG:
			text = mpt::ToUnicode(sndFile.GetCharsetInternal(), sndFile.m_songName);
			editCtrl->setMaxLength(modSpecs.modNameLengthMax);
			m_doLabelEdit = true;
			break;

		case MODITEM_ORDER:
			{
				PATTERNINDEX pat = sndFile.Order(static_cast<SEQUENCEINDEX>(modItem.val2)).at(static_cast<ORDERINDEX>(modItem.val1));
				if(pat == PATTERNINDEX_INVALID)
					text = UL_("---");
				else if(pat == PATTERNINDEX_SKIP)
					text = UL_("+++");
				else
					text = mpt::ufmt::val(pat);
				m_doLabelEdit = true;
			}
			break;

		case MODITEM_HDR_ORDERS:
			if(sndFile.Order.GetNumSequences() != 1 || sndFile.GetModSpecifications().sequencesMax <= 1)
			{
				break;
			}
			[[fallthrough]];
		case MODITEM_SEQUENCE:
			if(modItem.val1 < sndFile.Order.GetNumSequences())
			{
				text = sndFile.Order(static_cast<SEQUENCEINDEX>(modItem.val1)).GetName();
				m_doLabelEdit = true;
			}
			break;

		case MODITEM_PATTERN:
			if(modItem.val1 < sndFile.Patterns.GetNumPatterns() && modSpecs.hasPatternNames)
			{
				text = mpt::ToUnicode(sndFile.GetCharsetInternal(), sndFile.Patterns[modItem.val1].GetName());
				editCtrl->setMaxLength(MAX_PATTERNNAME - 1);
				m_doLabelEdit = true;
			}
			break;

		case MODITEM_SAMPLE:
			if(modItem.val1 <= sndFile.GetNumSamples())
			{
				text = mpt::ToUnicode(sndFile.GetCharsetInternal(), sndFile.m_szNames[modItem.val1]);
				editCtrl->setMaxLength(modSpecs.sampleNameLengthMax);
				m_doLabelEdit = true;
			}
			break;

		case MODITEM_INSTRUMENT:
			if(modItem.val1 <= sndFile.GetNumInstruments() && sndFile.Instruments[modItem.val1] != nullptr)
			{
				text = mpt::ToUnicode(sndFile.GetCharsetInternal(), sndFile.Instruments[modItem.val1]->name);
				editCtrl->setMaxLength(modSpecs.instrNameLengthMax);
				m_doLabelEdit = true;
			}
			break;

		default:
			break;
		}
	} else if(modItem.type == MODITEM_HDR_INSTRUMENTLIB)
	{
		text = (m_InstrLibPath + m_SongFileName).ToUnicode();
		m_doLabelEdit = true;
	}

	if(m_doLabelEdit)
	{
		CMainFrame::GetInputHandler()->Bypass(true);
		m_labelEditText = text;
		m_labelEditMaxLength = editSettings.maxLength;
	}
	CurrentNotify().result = m_doLabelEdit ? false : true;
}


void CModTree::OnLabelEditorReady(QLineEdit *editor)
{
	if(editor == nullptr || !m_doLabelEdit)
		return;
	if(m_labelEditMaxLength > 0)
		editor->setMaxLength(m_labelEditMaxLength);
	editor->setText(mfcless::ToQString(m_labelEditText));
	editor->selectAll();
}


// End editing sample, instrument, order, pattern, etc. labels
void CModTree::OnEndLabelEdit()
{
	CMainFrame::GetInputHandler()->Bypass(false);
	m_doLabelEdit = false;

	const mfcless::NotifyInfo &info = CurrentNotify();
	const ModItem modItem = GetModItem(info.treeItem);
	CModDoc *modDoc = modItem.IsSongItem() ? GetDocumentFromItem(info.treeItem) : nullptr;

	CurrentNotify().result = false;
	if(!info.textValid)
		return;

	if(modDoc != nullptr)
	{
		CSoundFile &sndFile = modDoc->GetSoundFile();
		const CModSpecifications &modSpecs = sndFile.GetModSpecifications();

		const mpt::ustring itemText = mfcless::FromQString(info.text);
		switch(modItem.type)
		{
		case MODITEM_HDR_SONG:
			if(mpt::ToUnicode(sndFile.GetCharsetInternal(), sndFile.m_songName) != itemText)
			{
				sndFile.m_songName = mpt::truncate(mpt::ToCharset(sndFile.GetCharsetInternal(), itemText), modSpecs.modNameLengthMax);
				modDoc->SetModified();
				modDoc->UpdateAllViews(nullptr, GeneralHint().General());
			}
			break;

		case MODITEM_ORDER:
			if(!itemText.empty())
			{
				PATTERNINDEX pat = mpt::parse<PATTERNINDEX>(itemText);
				bool valid = true;
				if(itemText[0] == UC_('-'))
				{
					pat = PATTERNINDEX_INVALID;
				} else if(itemText[0] == UC_('+'))
				{
					if(modSpecs.hasIgnoreIndex)
						pat = PATTERNINDEX_SKIP;
					else
						valid = false;
				} else
				{
					valid = (pat < sndFile.Patterns.GetNumPatterns());
				}
				PATTERNINDEX &target = sndFile.Order(static_cast<SEQUENCEINDEX>(modItem.val2))[static_cast<ORDERINDEX>(modItem.val1)];
				if(valid && pat != target)
				{
					target = pat;
					modDoc->SetModified();
					modDoc->UpdateAllViews(nullptr, SequenceHint().Data());
				}
			} else
			{
				mfcless::MessageBeep();
			}
			break;

		case MODITEM_HDR_ORDERS:
		case MODITEM_SEQUENCE:
			if(modItem.val1 < sndFile.Order.GetNumSequences() && sndFile.Order(static_cast<SEQUENCEINDEX>(modItem.val1)).GetName() != itemText)
			{
				sndFile.Order(static_cast<SEQUENCEINDEX>(modItem.val1)).SetName(itemText);
				modDoc->SetModified();
				modDoc->UpdateAllViews(nullptr, SequenceHint(static_cast<SEQUENCEINDEX>(modItem.val1)).Names());
			}
			break;

		case MODITEM_PATTERN:
			if(modItem.val1 < sndFile.Patterns.GetNumPatterns() && modSpecs.hasPatternNames && mpt::ToUnicode(sndFile.GetCharsetInternal(), sndFile.Patterns[modItem.val1].GetName()) != itemText)
			{
				sndFile.Patterns[modItem.val1].SetName(mpt::ToCharset(sndFile.GetCharsetInternal(), itemText));
				modDoc->SetModified();
				modDoc->UpdateAllViews(nullptr, PatternHint(static_cast<PATTERNINDEX>(modItem.val1)).Data().Names());
			}
			break;

		case MODITEM_SAMPLE:
			if(modItem.val1 <= sndFile.GetNumSamples() && mpt::ToUnicode(sndFile.GetCharsetInternal(), sndFile.m_szNames[modItem.val1]) != itemText)
			{
				sndFile.m_szNames[modItem.val1] = mpt::truncate(mpt::ToCharset(sndFile.GetCharsetInternal(), itemText), modSpecs.sampleNameLengthMax);
				modDoc->SetModified();
				modDoc->UpdateAllViews(nullptr, SampleHint(static_cast<SAMPLEINDEX>(modItem.val1)).Info().Names());
			}
			break;

		case MODITEM_INSTRUMENT:
			if(modItem.val1 <= sndFile.GetNumInstruments() && sndFile.Instruments[modItem.val1] != nullptr && mpt::ToUnicode(sndFile.GetCharsetInternal(), sndFile.Instruments[modItem.val1]->name) != itemText)
			{
				sndFile.Instruments[modItem.val1]->name = mpt::truncate(mpt::ToCharset(sndFile.GetCharsetInternal(), itemText), modSpecs.instrNameLengthMax);
				modDoc->SetModified();
				modDoc->UpdateAllViews(nullptr, InstrumentHint(static_cast<INSTRUMENTINDEX>(modItem.val1)).Info().Names());
			}
			break;

		default:
			break;
		}
	} else if(modItem.type == MODITEM_HDR_INSTRUMENTLIB)
	{
		const auto newPath = mpt::PathString::FromUnicode(mfcless::FromQString(info.text));
		if(mpt::PathCompareNoCase(newPath, m_InstrLibPath + m_SongFileName))
			SetFullInstrumentLibraryPath(newPath);
	}
}


// Drop files from Windows
void CModTree::OnDropFiles(const QStringList &files)
{
	bool refreshDLS = false;
	CMainFrame::GetMainFrame()->activateWindow();
	for(const QString &fileName : files)
	{
		{
			mpt::PathString file(mpt::PathString::FromUnicode(mfcless::FromQString(fileName)));
			if(IsSampleBrowser())
			{
				// Set sample browser location to this directory or file
				SetFullInstrumentLibraryPath(file);
				break;
			} else
			{
				if(CTrackApp::AddDLSBank(file))
				{
					refreshDLS = true;
				} else
				{
					// Pass message on
					theApp.OpenDocumentFile(file);
				}
			}
		}
	}
	if(refreshDLS)
	{
		RefreshDlsBanks();
	}
}


OPENMPT_NAMESPACE_END
