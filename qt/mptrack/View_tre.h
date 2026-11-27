/*
 * View_tre.h
 * ----------
 * Purpose: Tree view for managing open songs, sound files, file browser, ...
 * Notes  : mfcless twin of upstream's mptrack/View_tre.h.
 *          Draft from qt/tools/twin.py, not yet edited by hand.
 * Authors: Olivier Lapicque
 *          OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "mfcless/Menus.h"
#include "mfcless/Window.h"
#include <QtCore/QObject>
#include <QtWidgets/QTreeWidget>

#include "../soundlib/modcommand.h"
#include "../soundlib/Snd_defs.h"

#include "Mptrack.h"
#include "Notification.h"
#include "UpdateHints.h"

#include <vector>
#include <bitset>

class QTreeWidgetItem;
class QWidget;

OPENMPT_NAMESPACE_BEGIN

class CModDoc;
class CModTree;
class CSoundFile;
class CDLSBank;

struct ModTreeDocInfo
{
	// Tree state variables
	std::vector<std::vector<QTreeWidgetItem *>> tiOrders;
	std::vector<QTreeWidgetItem *> tiSequences, tiPatterns;
	CModDoc &modDoc;
	QTreeWidgetItem *hSong = nullptr, *hPatterns = nullptr, *hSamples = nullptr, *hInstruments = nullptr, *hComments = nullptr, *hOrders = nullptr, *hEffects = nullptr;

	// Module information
	ORDERINDEX ordSel = ORDERINDEX_INVALID;
	SEQUENCEINDEX seqSel = SEQUENCEINDEX_INVALID;

	std::bitset<MAX_SAMPLES> samplesPlaying;
	std::bitset<MAX_INSTRUMENTS> instrumentsPlaying;

	ModTreeDocInfo(CModDoc &modDoc);
};


class CModTreeDropTarget: public QObject
{
	Q_OBJECT
protected:
	CModTree *m_pModTree = nullptr;

public:
	CModTreeDropTarget() = default;
	bool Register(CModTree *pWnd);

public:
	DROPEFFECT OnDragEnter(QWidget *pWnd, COleDataObject* pDataObject, uint32 dwKeyState, mfcless::Point point) override;
	DROPEFFECT OnDragOver(QWidget *pWnd, COleDataObject* pDataObject, uint32 dwKeyState, mfcless::Point point) override;
	bool OnDrop(QWidget *pWnd, COleDataObject* pDataObject, DROPEFFECT dropEffect, mfcless::Point point) override;
};


class CModTree: public mfcless::Window<QTreeWidget>
{
	Q_OBJECT
public:
	enum ModItemType : uint8
	{
		MODITEM_NULL = 0,

		MODITEM_BEGIN_SONGITEMS,
		MODITEM_ORDER = MODITEM_BEGIN_SONGITEMS,
		MODITEM_PATTERN,
		MODITEM_SAMPLE,
		MODITEM_INSTRUMENT,
		MODITEM_COMMENTS,
		MODITEM_EFFECT,
		MODITEM_SEQUENCE,
		MODITEM_HDR_SONG,
		MODITEM_HDR_ORDERS,
		MODITEM_HDR_PATTERNS,
		MODITEM_HDR_SAMPLES,
		MODITEM_HDR_INSTRUMENTS,
		MODITEM_HDR_EFFECTS,
		MODITEM_END_SONGITEMS = MODITEM_HDR_EFFECTS,

		MODITEM_HDR_INSTRUMENTLIB,
		MODITEM_HDR_MIDILIB,
		MODITEM_HDR_MIDIGROUP,
		MODITEM_MIDIINSTRUMENT,
		MODITEM_MIDIPERCUSSION,
		MODITEM_INSLIB_FOLDER,
		MODITEM_INSLIB_SAMPLE,
		MODITEM_INSLIB_INSTRUMENT,
		MODITEM_INSLIB_SONG,
		MODITEM_DLSBANK_FOLDER,
		MODITEM_DLSBANK_INSTRUMENT,
	};

protected:
	enum TreeStatus
	{
		TREESTATUS_RDRAG = 0x01,
		TREESTATUS_LDRAG = 0x02,
		TREESTATUS_SINGLEEXPAND = 0x04,
		TREESTATUS_DRAGGING = (TREESTATUS_RDRAG | TREESTATUS_LDRAG)
	};

	// Bit mask magic
	enum : unsigned int
	{
		MIDILIB_SHIFT = 16,
		MIDILIB_MASK  = (1 << MIDILIB_SHIFT) - 1,

		// Must be consistent with CCtrlPatterns::OnActivatePage
		SEQU_SHIFT     = 16,
		SEQU_MASK      = (1 << SEQU_SHIFT) - 1,
		SEQU_INDICATOR = 0x80000000,

		// Soundbank instrument identification (must be consistent with CViewInstrument::OnDragonDrop / CViewSample::OnDragonDrop)
		DLS_TYPEPERC    = 0x80000000,
		DLS_INSTRMASK   = 0x0000FFFF,
		DLS_REGIONMASK  = 0x7FFF0000,  // Drum region
		DLS_REGIONSHIFT = 16,

		DLS_DRUM_FOLDER_LPARAM = 0x12345678,
	};
	static_assert((ORDERINDEX_INVALID & SEQU_MASK) == ORDERINDEX_INVALID, "ORDERINDEX doesn't fit in GetItemData() parameter");
	static_assert((ORDERINDEX_MAX & SEQU_MASK) == ORDERINDEX_MAX, "ORDERINDEX doesn't fit in GetItemData() parameter");
	static_assert((((SEQUENCEINDEX_INVALID << SEQU_SHIFT) & ~SEQU_INDICATOR) >> SEQU_SHIFT) == SEQUENCEINDEX_INVALID, "SEQUENCEINDEX doesn't fit in GetItemData() parameter");

	struct ModItem
	{
		uint32 val1;
		uint16 val2;
		ModItemType type;

		ModItem(ModItemType t = MODITEM_NULL, uint32 v1 = 0, uint16 v2 = 0) : val1(v1), val2(v2), type(t) { }
		bool IsSongItem() const noexcept { return type >= MODITEM_BEGIN_SONGITEMS && type <= MODITEM_END_SONGITEMS; }
		bool operator==(const ModItem &other) const noexcept { return val1 == other.val1 && val2 == other.val2 && type == other.type; }
		bool operator!=(const ModItem &other) const noexcept { return !(*this == other); }
	};

	struct DlsItem : public ModItem
	{
		explicit DlsItem(uint16 instr) : ModItem(MODITEM_DLSBANK_INSTRUMENT, instr, 0) { }
		DlsItem(uint16 instr, uint16 region) : ModItem(MODITEM_DLSBANK_INSTRUMENT, (instr & DLS_INSTRMASK) | ((region << DLS_REGIONSHIFT) & DLS_REGIONMASK) | DLS_TYPEPERC, 0) { }

		uint32 GetRegion() const noexcept { return (val1 & DLS_REGIONMASK) >> DLS_REGIONSHIFT; }
		uint32 GetInstr() const noexcept { return (val1 & DLS_INSTRMASK); }
		bool IsPercussion() const noexcept { return ((val1 & DLS_TYPEPERC) == DLS_TYPEPERC); }
		bool IsMelodic() const noexcept { return !IsPercussion(); }

		static ModItem FromLPARAM(uint32 lparam) noexcept { return ModItem{MODITEM_DLSBANK_INSTRUMENT, lparam, 0}; }
		static intptr_t ToLPARAM(uint16 instr, uint16 region, bool isPerc) noexcept { return (instr & DLS_INSTRMASK) | ((region << DLS_REGIONSHIFT) & DLS_REGIONMASK) | (isPerc ? DLS_TYPEPERC : 0); }
	};

	static CSoundFile *m_SongFile;  // For browsing samples and instruments inside modules on disk
	CModTreeDropTarget m_DropTarget;
	CModTree *m_pDataTree = nullptr;  // Pointer to instrument browser (lower part of tree view) - if it's a nullptr, this object is the instrument browser itself.
	QWidget * m_hDropWnd = nullptr;
	mpt::mutex m_WatchDirMutex;
	HANDLE m_hSwitchWatchDir = nullptr;
	mpt::PathString m_WatchDir;
	HANDLE m_hWatchDirKillThread = nullptr;
	std::thread m_WatchDirThread;
	ModItem m_itemDrag;
	uint32 m_dwStatus = 0;
	CModDoc *m_selectedDoc = nullptr, *m_dragDoc = nullptr;
	QTreeWidgetItem *m_hItemDrag = nullptr, *m_hItemDrop = nullptr;
	QTreeWidgetItem *m_hInsLib = nullptr, *m_hMidiLib = nullptr;
	QTreeWidgetItem * m_tiMidi[128];
	QTreeWidgetItem * m_tiPerc[128];
	std::vector<QTreeWidgetItem *> m_tiDLS;
	std::map<const CModDoc *, ModTreeDocInfo> m_docInfo;
	mpt::ustring m_HelpText;

	std::unique_ptr<CDLSBank> m_cachedBank;
	mpt::PathString m_cachedBankName;

	uint32 m_stringCompareFlags = NORM_IGNORECASE | NORM_IGNOREWIDTH;

	// Instrument library
	mpt::PathString m_InstrLibPath;           // Current path to be explored
	mpt::PathString m_InstrLibHighlightPath;  // Folder to highlight in browser after a refresh
	mpt::PathString m_SongFileName;           // Name of open module, without path (== m_InstrLibPath).
	mpt::PathString m_previousPath;           // The folder from which we came from when navigating one folder up
	
	std::vector<const char*> m_modExtensions;                  // Cached in order to avoid querying too often when changing browsed folder
	std::vector<mpt::PathString> m_MediaFoundationExtensions;  // Cached in order to avoid querying too often when changing browsed folder

	struct FileBrowserEntry
	{
		mpt::ustring name;
		uint64 size;
		uint64 modtime;
		uint32 image;
		bool hidden;
	};
	std::vector<FileBrowserEntry> m_fileBrowserEntries;
	mpt::ustring m_filterString;

	enum class LibrarySortOrder
	{
		Name,
		Date,
		Size,
	};

	static LibrarySortOrder m_librarySort;

	int m_redrawLockCount = 0;

	bool m_showAllFiles = false;
	bool m_doLabelEdit = false;

public:
	CModTree(CModTree *pDataTree);
	~CModTree();

	void Init();
	bool InsLibSetFullPath(const mpt::PathString &libPath, const mpt::PathString &songFolder);
	mpt::PathString InsLibGetFullPath(QTreeWidgetItem * hItem) const;
	bool SetSoundFile(FileReader &file);
	void RefreshMidiLibrary();
	void RefreshDlsBanks();
	void RefreshInstrumentLibrary();
	void MonitorInstrumentLibrary();
	ModItem GetModItem(QTreeWidgetItem * hItem);
	bool SetMidiInstrument(uint32 nIns, const mpt::PathString &fileName);
	bool SetMidiPercussion(uint32 nPerc, const mpt::PathString &fileName);
	bool ExecuteItem(QTreeWidgetItem * hItem);
	void DeleteTreeItem(QTreeWidgetItem * hItem, const bool permanently);
	static void PlayDLSItem(const CDLSBank &dlsBank, const DlsItem &item, ModCommand::NOTE note);
	bool PlayItem(QTreeWidgetItem * hItem, ModCommand::NOTE nParam, int volume = -1);
	bool OpenTreeItem(QTreeWidgetItem * hItem);
	bool OpenMidiInstrument(uint32 dwItem);
	void SetFullInstrumentLibraryPath(mpt::PathString path);
	void InstrumentLibraryChDir(mpt::PathString dir, bool isSong);
	bool GetDropInfo(DRAGONDROP &dropInfo, mpt::PathString &fullPath);
	void OnOptionsChanged();
	void AddDocument(CModDoc &modDoc);
	void RemoveDocument(const CModDoc &modDoc);
	void UpdateView(ModTreeDocInfo &info, UpdateHint hint);
	void OnUpdate(CModDoc *pModDoc, UpdateHint hint, QObject *pHint);
	bool CanDrop(QTreeWidgetItem * hItem, bool bDoDrop);
	void UpdatePlayPos(CModDoc &modDoc, Notification *pNotify);
	bool IsItemExpanded(QTreeWidgetItem * hItem) const;
	void DeleteChildren(QTreeWidgetItem * hItem);
	QTreeWidgetItem * GetNthChildItem(QTreeWidgetItem * hItem, int index) const;
	QTreeWidgetItem * GetParentRootItem(QTreeWidgetItem * hItem) const;

	bool IsSampleBrowser() const { return m_pDataTree == nullptr; }
	CModTree *GetSampleBrowser() { return IsSampleBrowser() ? this : m_pDataTree; }
	CModTree *GetOtherView();

	void SetInstrumentLibraryFilter(const mpt::ustring &filter);
	void SetInstrumentLibraryFilterSortOrder(LibrarySortOrder sortType);
	void SortInstrumentLibrary();

// Overrides
	// ClassWizard generated virtual function overrides
public:
	bool PreTranslateMessage(mfcless::Message *pMsg) override;
// Drag & Drop operations
public:
	DROPEFFECT OnDragEnter(COleDataObject* pDataObject, uint32 dwKeyState, mfcless::Point point);
	DROPEFFECT OnDragOver(COleDataObject* pDataObject, uint32 dwKeyState, mfcless::Point point);
	bool OnDrop(COleDataObject* pDataObject, DROPEFFECT dropEffect, mfcless::Point point);

protected:
	int ImageToSortOrder(int image) const;
	ModTreeDocInfo *GetDocumentInfoFromItem(QTreeWidgetItem * hItem);
	CModDoc *GetDocumentFromItem(QTreeWidgetItem * hItem) { ModTreeDocInfo *info = GetDocumentInfoFromItem(hItem); return info ? &info->modDoc : nullptr; }
	ModTreeDocInfo *GetDocumentInfoFromModDoc(CModDoc &modDoc);

	size_t GetDLSBankIndexFromItem(QTreeWidgetItem * hItem) const;
	CDLSBank *GetDLSBankFromItem(QTreeWidgetItem * hItem) const;

	void InsertOrDupItem(bool insert);
	void OnItemRightClick(QTreeWidgetItem * hItem, mfcless::Point pt);

	static bool HasEffectPlugins(const CSoundFile &sndFile);
	static bool AllPluginsBypassed(const CSoundFile &sndFile, bool onlyEffects);
	static void BypassAllPlugins(CSoundFile &sndFile, bool bypass, bool onlyEffects);

	void FillInstrumentLibrary(const mpt::uchar *selectedItem = nullptr);
	void FilterInstrumentLibrary(mpt::ustring filter, const mpt::uchar *selectedItem = nullptr);

	mfcless::Menu AddLibraryFindAndSortMenus(mfcless::Menu hMenu) const;

	void LockRedraw()
	{
		if(!m_redrawLockCount++)
			SetRedraw(false);
	}

	void UnlockRedraw()
	{
		if(!--m_redrawLockCount)
			SetRedraw(true);
	}

protected:
	intptr_t OnDPIChangedAfterParent(uintptr_t, intptr_t);
	void OnLButtonUp(uint32 nFlags, mfcless::Point point);
	void OnRButtonUp(uint32 nFlags, mfcless::Point point);
	void OnXButtonUp(uint32 nFlags, uint32 nButton, mfcless::Point point);
	void OnMouseMove(uint32 nFlags, mfcless::Point point);
	void OnBeginDrag(QTreeWidgetItem *, bool bLeft, intptr_t *pResult);
	void OnBeginLDrag(LPNMHDR, intptr_t *pResult);
	void OnBeginRDrag(LPNMHDR, intptr_t *pResult);
	void OnEndDrag(uint32 dwMask);
	void OnItemDblClk(LPNMHDR phdr, intptr_t *pResult);
	void OnItemReturn(LPNMHDR, intptr_t *pResult);
	void OnItemLeftClick(LPNMHDR pNMHDR, intptr_t *pResult);
	void OnItemRightClick(LPNMHDR, intptr_t *pResult);
	void OnItemExpanded(LPNMHDR pnmhdr, intptr_t *pResult);
	void OnGetDispInfo(LPNMHDR pnmhdr, intptr_t *pResult);
	void OnSelChanged(LPNMHDR pnmhdr, intptr_t *pResult);
	void OnRefreshTree();
	void OnExecuteItem();
	void OnPlayTreeItem();
	void OnDeleteTreeItem();
	void OnOpenTreeItem();
	void OnMuteTreeItem();
	void OnMuteOnlyEffects();
	void OnSoloTreeItem();
	void OnUnmuteAllTreeItem();
	void OnDuplicateTreeItem() { InsertOrDupItem(false); }
	void OnInsertTreeItem() { InsertOrDupItem(true); }
	void OnSwitchToTreeItem();	// hack for sequence items to avoid double-click action
	void OnCloseItem();
	void OnRenameItem();
	void OnBeginLabelEdit();
	void OnEndLabelEdit();
	void OnDropFiles(const QStringList &files);
	void OnKillFocus(QWidget *pNewWnd);
	void OnSetFocus(QWidget *pOldWnd);
	void OnDestroy();

	void OnSetItemPath();
	void OnSaveItem();
	void OnSaveAll();
	void OnReloadItem();
	void OnReloadAll();
	void OnFindMissing();

	void OnAddDlsBank();
	void OnImportMidiLib();
	void OnExportMidiLib();
	void OnSoundBankProperties();
	void OnRefreshInstrLib();
	void OnShowDirectories();
	void OnShowAllFiles();
	void OnShowSoundFiles();

	void OnGotoInstrumentDir();
	void OnGotoSampleDir();

	void OnOpenInstrumentLibraryFilter();
	void OnSortByName() { SetInstrumentLibraryFilterSortOrder(LibrarySortOrder::Name); }
	void OnSortByDate() { SetInstrumentLibraryFilterSortOrder(LibrarySortOrder::Date); }
	void OnSortBySize() { SetInstrumentLibraryFilterSortOrder(LibrarySortOrder::Size); }

	intptr_t OnCustomKeyMsg(uintptr_t, intptr_t);
	intptr_t OnMidiMsg(uintptr_t midiData, intptr_t);
	void RegisterMessageMap() override;
};


OPENMPT_NAMESPACE_END
