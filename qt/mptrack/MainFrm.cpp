/*
 * MainFrm.cpp
 * -----------
 * Purpose: Implementation of OpenMPT's main window code.
 * Notes  : mfcless twin of upstream's mptrack/MainFrm.cpp. The window is a
 *          QMainWindow: the toolbar and status bar are Qt's, the tree bar and
 *          the MDI area share a splitter in the middle. What CMDIFrameWnd
 *          did (the menu, routing commands to the active document, the
 *          title) is done here too.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "Mainfrm.h"
#include "AdvancedConfigDlg.h"
#include "AutoSaver.h"
#include "ChannelManagerDlg.h"
#include "Childfrm.h"
#include "CloseMainDialog.h"
#include "ColorConfigDlg.h"
#include "dlg_misc.h"
#include "Dlsbank.h"
#include "FileDialog.h"
#include "FolderScanner.h"
#include "GeneralConfigDlg.h"
#include "Globals.h"
#include "HighDPISupport.h"
#include "ImageLists.h"
#include "InputHandler.h"
#include "IPCWindow.h"
#include "KeyConfigDlg.h"
#include "Moddoc.h"
#include "ModDocTemplate.h"
#include "Mpdlgs.h"
#include "Mptrack.h"
#include "PathConfigDlg.h"
#include "PatternClipboard.h"
#include "PatternFont.h"
#include "ProgressDialog.h"
#include "QuickStartDialog.h"
#include "Reporting.h"
#include "resource.h"
#include "SampleConfigDlg.h"
#include "SelectPluginDialog.h"
#include "UpdateCheck.h"
#include "WindowMessages.h"
#include "View_pat.h"
#include "View_tre.h"

#include "../common/FileReader.h"
#include "../common/mptFileIO.h"
#include "../common/Profiler.h"
#include "../common/version.h"
#include "../soundlib/AudioReadTarget.h"
#include "../soundlib/Tables.h"
#include "../soundlib/PlaybackTest.h"
#include "mpt/audio/span.hpp"
#include "mpt/base/alloc.hpp"
#include "mpt/fs/fs.hpp"
#include "mpt/io_file/fstream.hpp"
#include "mpt/io_file/inputfile.hpp"
#include "mpt/io_file_read/inputfile_filecursor.hpp"
#include "mpt/string/utility.hpp"
#include "openmpt/sounddevice/SoundDevice.hpp"
#include "openmpt/sounddevice/SoundDeviceBuffer.hpp"
#include "openmpt/sounddevice/SoundDeviceManager.hpp"

#ifdef MPT_ENABLE_PLAYBACK_TEST_MENU
#include "../unarchiver/ungzip.h"
#include "../common/GzipWriter.h"
#endif

#include "mfcless/ModMessage.h"
#include "mfcless/QtString.h"
#include "mfcless/Resources.h"

#include <QtCore/QCollator>
#include <QtCore/QDir>
#include <QtCore/QUrl>
#include <QtGui/QAccessible>
#include <QtGui/QCloseEvent>
#include <QtGui/QInputMethod>
#include <QtGui/QCursor>
#include <QtGui/QDesktopServices>
#include <QtGui/QGuiApplication>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMdiArea>
#include <QtWidgets/QMdiSubWindow>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QSplitter>
#include <QtWidgets/QStatusBar>


OPENMPT_NAMESPACE_BEGIN

static constexpr uint32 TIMERID_GUI = 1;
static constexpr uint32 TIMERID_NOTIFY = 2;

static constexpr uint32 MPTTIMER_PERIOD = 100;


/////////////////////////////////////////////////////////////////////////////
// CMainFrame

void CMainFrame::RegisterMessageMap()
{
	mfcless::Window<QMainWindow>::RegisterMessageMap();
	// ON_WM_TIMER() (mfcless::Window)
	// ON_WM_CLOSE(): closeEvent
	// ON_WM_CREATE(): OnCreate, called by Initialize
	// ON_WM_RBUTTONDOWN() (mfcless::Window)
	// ON_WM_DEVICECHANGE(): RtMidi notices lost devices itself
	// ON_WM_DROPFILES() (mfcless::Window)
	// ON_WM_QUERYENDSESSION(): QGuiApplication::commitDataRequest
	// ON_WM_MOUSEWHEEL() (mfcless::Window)
	// ON_WM_SHOWWINDOW() (mfcless::Window)
	// ON_WM_ACTIVATEAPP(), ON_WM_ACTIVATE(): QGuiApplication::applicationStateChanged
	AddCommand(ID_VIEW_OPTIONS, &CMainFrame::OnViewOptions);
	AddCommand(ID_PLUGIN_SETUP, &CMainFrame::OnPluginManager);
	AddCommand(ID_CLIPBOARD_MANAGER, &CMainFrame::OnClipboardManager);
	//ON_COMMAND(ID_HELP,              &CMDIFrameWnd::OnHelp)
	AddCommand(ID_REPORT_BUG, &CMainFrame::OnReportBug);
	AddCommand(ID_NEXTOCTAVE, &CMainFrame::OnNextOctave);
	AddCommand(ID_PREVOCTAVE, &CMainFrame::OnPrevOctave);
	AddCommand(ID_ADD_SOUNDBANK, &CMainFrame::OnAddDlsBank);
	AddCommand(ID_IMPORT_MIDILIB, &CMainFrame::OnImportMidiLib);
	AddCommand(ID_MIDI_RECORD, &CMainFrame::OnMidiRecord);
	AddCommand(ID_PANIC, &CMainFrame::OnPanic);
	AddCommand(ID_PLAYER_PAUSE, &CMainFrame::OnPlayerPause);
	AddCommand(IDD_TREEVIEW, &CMainFrame::OnBarCheck);
	// CFrameWnd's own: the toolbar's View menu item.
	AddCommand(ID_VIEW_TOOLBAR, &CMainFrame::OnBarCheck);
	AddCommandUpdate(ID_VIEW_TOOLBAR, &CMainFrame::OnUpdateControlBarMenu);
	AddCommand(ID_NETLINK_MODPLUG, &CMainFrame::OnInternetLink);
	AddCommand(ID_NETLINK_TOP_PICKS, &CMainFrame::OnInternetLink);
	AddMessage(WM_MOD_UPDATEPOSITION, &CMainFrame::OnUpdatePosition);
	AddMessage(WM_MOD_INVALIDATEPATTERNS, &CMainFrame::OnInvalidatePatterns);
	AddMessage(WM_MOD_KEYCOMMAND, &CMainFrame::OnCustomKeyMsg);
	AddMessage(WM_MOD_MIDIMAPPING, &CMainFrame::OnViewMIDIMapping);
	AddMessage(WM_MOD_UPDATEVIEWS, &CMainFrame::OnUpdateViews);
	AddMessage(WM_MOD_SETMODIFIED, &CMainFrame::OnSetModified);
#if defined(MPT_ENABLE_UPDATE)
	AddMessage(WM_MOD_UPDATENOTIFY, &CMainFrame::OnToolbarUpdateIndicatorClick);
#endif // MPT_ENABLE_UPDATE
	AddCommand(ID_INTERNETUPDATE, &CMainFrame::OnInternetUpdate);
	AddCommand(ID_UPDATE_AVAILABLE, &CMainFrame::OnUpdateAvailable);
#if defined(MPT_ENABLE_UPDATE)
	AddMessage(MPT_WM_APP_UPDATECHECK_START, &CMainFrame::OnUpdateCheckStart);
	AddMessage(MPT_WM_APP_UPDATECHECK_PROGRESS, &CMainFrame::OnUpdateCheckProgress);
	AddMessage(MPT_WM_APP_UPDATECHECK_CANCELED, &CMainFrame::OnUpdateCheckCanceled);
	AddMessage(MPT_WM_APP_UPDATECHECK_FAILURE, &CMainFrame::OnUpdateCheckFailure);
	AddMessage(MPT_WM_APP_UPDATECHECK_SUCCESS, &CMainFrame::OnUpdateCheckSuccess);
#endif // MPT_ENABLE_UPDATE
	AddCommand(ID_HELP_SHOWSETTINGSFOLDER, &CMainFrame::OnShowSettingsFolder);
	AddCommand(ID_HELPSHOW, &CMainFrame::OnHelp);
	AddCommand(ID_MAINBAR_SHOW_OCTAVE, &CMainFrame::OnToggleMainBarShowOctave);
	AddCommand(ID_MAINBAR_SHOW_TEMPO, &CMainFrame::OnToggleMainBarShowTempo);
	AddCommand(ID_MAINBAR_SHOW_SPEED, &CMainFrame::OnToggleMainBarShowSpeed);
	AddCommand(ID_MAINBAR_SHOW_ROWSPERBEAT, &CMainFrame::OnToggleMainBarShowRowsPerBeat);
	AddCommand(ID_MAINBAR_SHOW_GLOBALVOLUME, &CMainFrame::OnToggleMainBarShowGlobalVolume);
	AddCommand(ID_MAINBAR_SHOW_VUMETER, &CMainFrame::OnToggleMainBarShowVUMeter);
	AddCommand(ID_MAINBAR_SHOW_FILE_ICONS, &CMainFrame::OnToggleMainBarShowFileIcons);
	AddCommand(ID_MAINBAR_SHOW_EDIT_ICONS, &CMainFrame::OnToggleMainBarShowEditIcons);
	AddCommand(ID_MAINBAR_SHOW_PLAY_ICONS, &CMainFrame::OnToggleMainBarShowPlayIcons);
	AddCommand(ID_MAINBAR_SHOW_MISC_ICONS, &CMainFrame::OnToggleMainBarShowMiscIcons);
	AddCommand(ID_TREEVIEW_ON_LEFT, &CMainFrame::OnToggleTreeViewOnLeft);
#ifdef MPT_ENABLE_PLAYBACK_TEST_MENU
	AddCommand(ID_CREATE_MIXERDUMP, &CMainFrame::OnCreateMixerDump);
	AddCommand(ID_VERIFY_MIXERDUMP, &CMainFrame::OnVerifyMixerDump);
	AddCommand(ID_CONVERT_MIXERDUMP, &CMainFrame::OnConvertMixerDumpToText);
#endif // ENABLE_PLAYBACK_TEST_MENU
	AddCommandRange(ID_FILE_OPENTEMPLATE, ID_FILE_OPENTEMPLATE_LASTINRANGE, &CMainFrame::OnOpenTemplateModule);
	AddCommandRange(ID_EXAMPLE_MODULES, ID_EXAMPLE_MODULES_LASTINRANGE, &CMainFrame::OnExampleSong);
	AddCommandRange(ID_MRU_LIST_FIRST, ID_MRU_LIST_LAST, &CMainFrame::OnOpenMRUItem);
	AddCommandUpdate(ID_MRU_LIST_FIRST, &CMainFrame::OnUpdateMRUItem);
	AddCommandUpdate(ID_MIDI_RECORD, &CMainFrame::OnUpdateMidiRecord);
	AddCommandUpdate(ID_INDICATOR_TIME, &CMainFrame::OnUpdateTime);
	AddCommandUpdate(ID_INDICATOR_USER, &CMainFrame::OnUpdateUser);
	AddCommandUpdate(ID_INDICATOR_INFO, &CMainFrame::OnUpdateInfo);
	AddCommandUpdate(ID_INDICATOR_XINFO, &CMainFrame::OnUpdateXInfo);
	AddCommandUpdate(IDD_TREEVIEW, &CMainFrame::OnUpdateControlBarMenu);
}

// Globals
OptionsPage CMainFrame::m_nLastOptionsPage = OPTIONS_PAGE_DEFAULT;

// GDI
QIcon CMainFrame::m_hIcon;
QFont CMainFrame::m_hGUIFont;
mfcless::Pen CMainFrame::penDarkGray;
mfcless::Pen CMainFrame::penGray99;
mfcless::Pen CMainFrame::penHalfDarkGray;

QCursor CMainFrame::curDragging;
QCursor CMainFrame::curArrow;
QCursor CMainFrame::curNoDrop;
QCursor CMainFrame::curNoDrop2;
QCursor CMainFrame::curVSplit;
MODPLUGDIB *CMainFrame::bmpNotes = nullptr;
uint32 CMainFrame::gcolrefVuMeter[NUM_VUMETER_PENS * 2];

static constexpr uint32 StatusBarIndicators[] =
{
	ID_SEPARATOR,  // status line indicator
	ID_INDICATOR_XINFO,
	ID_INDICATOR_INFO,
	ID_INDICATOR_USER,
	ID_INDICATOR_TIME,
};

static CMainFrame *g_mainFrame = nullptr;


/////////////////////////////////////////////////////////////////////////////
// CMainFrame construction/destruction
CMainFrame::CMainFrame()
	: SoundDevice::CallbackBufferHandler<DithersOpenMPT>(theApp.PRNG())
	, m_SoundDeviceFillBufferCriticalSection(CriticalSection::InitialState::Unlocked)
	, m_InputHandler{this}
{
	g_mainFrame = this;
	MemsetZero(gcolrefVuMeter);
	// The title comes from the string table, as CFrameWnd::LoadFrame takes it.
	m_strTitle = mfcless::FromQString(QString::fromStdU16String(mfcless::res::String(IDR_MAINFRAME)));
	setAttribute(Qt::WA_DeleteOnClose, false);
	setAcceptDrops(true);
	setWindowIcon(QIcon(QPixmap::fromImage(mfcless::res::Icon(IDR_MAINFRAME))));
}


void CMainFrame::Initialize()
{
	// mfcless: CFrameWnd::LoadFrame created the window (OnCreate) before
	// upstream's InitInstance called this.
	OnCreate();

	//Adding version number to the frame title
	mpt::ustring title = GetTitle();
	title += U_(" ") + mpt::ufmt::val(Version::Current());
	#if defined(MPT_BUILD_RETRO)
		title += U_(" RETRO");
	#endif // MPT_BUILD_RETRO
	if(Build::IsDebugBuild())
	{
		title += U_(" DEBUG");
	}
	#ifndef MPT_WITH_VST
		title += U_(" NO_VST");
	#endif
	#ifndef MPT_WITH_DMO
		title += U_(" NO_DMO");
	#endif
	#ifdef NO_PLUGINS
		title += U_(" NO_PLUGINS");
	#endif
	SetTitle(title);
	OnUpdateFrameTitle(false);

	// Check for valid sound device
	SoundDevice::Identifier dev = TrackerSettings::Instance().GetSoundDeviceIdentifier();
	if(!theApp.GetSoundDevicesManager()->FindDeviceInfo(dev).IsValid())
	{
		dev = theApp.GetSoundDevicesManager()->FindDeviceInfoBestMatch(dev).GetIdentifier();
		TrackerSettings::Instance().SetSoundDeviceIdentifier(dev);
	}

	// Setup timer
	{
		mfcless::CommandState state;
		OnUpdateUser(state);
	}
	SetTimer(TIMERID_GUI, MPTTIMER_PERIOD);
	m_nTimer = TIMERID_GUI;

	// Setup Keyboard Hook
	// mfcless: the focus changes come from QApplication instead of a CBT hook.
	connect(qApp, &QApplication::focusChanged, this, &CMainFrame::OnFocusChanged);
	connect(QGuiApplication::inputMethod(), &QInputMethod::localeChanged, this, &CMainFrame::OnInputLanguageChanged);
	connect(qApp, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState state) { OnApplicationStateChanged(state == Qt::ApplicationActive); });

	// Update the tree
	m_wndTree.Init();

	CreateExampleModulesMenu();
	CreateTemplateModulesMenu();
	UpdateMRUList();

	auto [toolbarMenu, toolbarMenuStartPos] = FindMenuItemByCommand(*GetMenu(), ID_VIEW_TOOLBAR);
	MPT_ASSERT(toolbarMenu);
	if(toolbarMenu)
	{
		toolbarMenu->DeleteMenu(toolbarMenuStartPos, mfcless::menu::ByPosition);
		AddToolBarMenuEntries(toolbarMenu);
	}

	m_InputHandler->UpdateMainMenu();

	LoadMetronomeSamples();

#ifdef MPT_ENABLE_PLAYBACK_TEST_MENU
	mfcless::Menu debugMenu;
	debugMenu.CreatePopupMenu();
	debugMenu.AppendMenu(mfcless::menu::String, ID_CREATE_MIXERDUMP, U_("Create Mixer Dump for &File(s)..."));
	debugMenu.AppendMenu(mfcless::menu::String, ID_VERIFY_MIXERDUMP, U_("&Verify File(s)..."));
	debugMenu.AppendMenu(mfcless::menu::String, ID_CONVERT_MIXERDUMP, U_("Convert Mixer Dump to &TSV..."));
	GetMenu()->AppendMenu(mfcless::menu::String | mfcless::menu::Popup, debugMenu, U_("Debug"));
#endif  // ENABLE_PLAYBACK_TEST_MENU
	DrawMenuBar();
}


CMainFrame::~CMainFrame()
{
	CChannelManagerDlg::DestroySharedInstance();
	m_metronomeMeasure.FreeSample();
	m_metronomeBeat.FreeSample();
	if(g_mainFrame == this)
		g_mainFrame = nullptr;
}


int CMainFrame::OnCreate()
{
	// Load resources
	m_hIcon = QIcon(QPixmap::fromImage(mfcless::res::Icon(IDR_MAINFRAME)));

	RecreateImageLists();

	penDarkGray = mfcless::Pen(mfcless::pen::Solid, 0, mfcless::SystemColor(mfcless::SysColor::ButtonShadow));
	penGray99 = mfcless::Pen(mfcless::pen::Solid, 0, mfcless::Rgb(0x99, 0x99, 0x99));
	penHalfDarkGray = mfcless::Pen(mfcless::pen::Dot, 0, mfcless::SystemColor(mfcless::SysColor::ButtonShadow));

	// Cursors
	curDragging = mfcless::res::Cursor(IDC_DRAGGING);
	curArrow = QCursor(Qt::ArrowCursor);
	curNoDrop = QCursor(Qt::ForbiddenCursor);
	curNoDrop2 = mfcless::res::Cursor(IDC_NODRAG);
	curVSplit = QCursor(Qt::SplitVCursor);  // AFX_IDC_HSPLITBAR

	// Pattern note bitmap
	bmpNotes = LoadDib(IDB_PATTERNVIEW);

	// The menu (CFrameWnd::LoadFrame loads IDR_MAINFRAME)
	m_menu = mfcless::Menu::Load(IDR_MAINFRAME);
	mfcless::ConnectMenu(m_menu.Get(), {}, [this](uint32 id, mfcless::CommandState &state) { return UpdateCommandState(id, state); });
	connect(menuBar(), &QMenuBar::triggered, this, [this](QAction *action)
	{
		if(const uint32 id = mfcless::ActionCommand(action))
			RouteCommand(id);
	});

	// The MDI client and the tree bar, side by side
	m_mdiArea = new QMdiArea;
	m_mdiArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	m_mdiArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	m_mdiArea->setActivationOrder(QMdiArea::ActivationHistoryOrder);
	connect(m_mdiArea, &QMdiArea::subWindowActivated, this, [this](QMdiSubWindow *window)
	{
		static QPointer<CChildFrame> previous;
		auto *frame = qobject_cast<CChildFrame *>(window);
		if(previous && previous != frame)
			previous->OnMDIActivate(false);
		previous = frame;
		if(frame)
			frame->OnMDIActivate(true);
		OnUpdateFrameTitle(true);
	});
	m_centralSplitter = new QSplitter(Qt::Horizontal);
	m_centralSplitter->setChildrenCollapsible(false);
	m_centralSplitter->addWidget(&m_wndTree);
	m_centralSplitter->addWidget(m_mdiArea);
	m_centralSplitter->setStretchFactor(0, 0);
	m_centralSplitter->setStretchFactor(1, 1);
	setCentralWidget(m_centralSplitter);

	// Toolbars
	if (!m_wndToolBar.Create(this)) return -1;
	addToolBar(Qt::TopToolBarArea, &m_wndToolBar);
	if (!m_wndStatusBar.Create(this)) return -1;
	m_wndTree.SetBarOnLeft(TrackerSettings::Instance().treeViewOnLeft);
	m_wndStatusBar.SetIndicators(StatusBarIndicators, static_cast<int>(std::size(StatusBarIndicators)));
	SetupStatusBarSizes();
	m_wndToolBar.Init(this);
	m_wndTree.RecalcLayout();

	// Restore toobar positions
	// mfcless: Qt keeps the toolbar's place (saveState), in Display/ToolbarState.
	{
		const std::string state = theApp.GetSettings().Read<std::string>(U_("Display"), U_("ToolbarState"), std::string());
		if(!state.empty())
			restoreState(QByteArray::fromBase64(QByteArray::fromStdString(state)));
	}

	UpdateColors();

	if(TrackerSettings::Instance().midiSetup & MidiSetup::EnableMidiInOnStartup)
		midiOpenDevice(false);

	RegisterMessageMap();

	return 0;
}


intptr_t CMainFrame::OnDPIChanged(uintptr_t, intptr_t)
{
	// mfcless: Qt moves and scales the window itself.
	RecreateImageLists();
	SetupStatusBarSizes();
	// WM_DPICHANGED_AFTERPARENT of the tree views
	for(CModTree *tree : {m_wndTree.m_pModTree, m_wndTree.m_pModTreeData})
	{
		if(tree)
			tree->OnDPIChanged();
	}
	m_wndTree.RecalcLayout();
	return 0;
}


void CMainFrame::RecreateImageLists()
{
	// Toolbar and other icons
	const double scaling = HighDPISupport::GetDpiForWindow(this) / 96.0;
	static constexpr int miscIconsInvert[] = {IMAGE_PATTERNS, IMAGE_OPLINSTRACTIVE, IMAGE_OPLINSTRMUTE};
	static constexpr int patternIconsInvert[] = {TIMAGE_PREVIEW, TIMAGE_MACROEDITOR, TIMAGE_PATTERN_OVERFLOWPASTE, TIMAGE_PATTERN_PLUGINS, TIMAGE_SAMPLE_AMPLIFY, TIMAGE_SAMPLE_UNSIGN};
	static constexpr int envelopeIconsInvert[] = {IIMAGE_VOLENV, IIMAGE_PANENV, IIMAGE_CHECKED, IIMAGE_VOLSWITCH, IIMAGE_PANSWITCH, IIMAGE_PITCHSWITCH, IIMAGE_FILTERSWITCH, IIMAGE_NOPITCHSWITCH, IIMAGE_NOFILTERSWITCH, IIMAGE_GRID};
	m_MiscIcons.Create(IDB_IMAGELIST, 16, 16, IMGLIST_NUMIMAGES, 1, scaling, false, miscIconsInvert);
	m_MiscIconsDisabled.Create(IDB_IMAGELIST, 16, 16, IMGLIST_NUMIMAGES, 1, scaling, true, miscIconsInvert);
	m_PatternIcons.Create(IDB_PATTERNS, 16, 16, PATTERNIMG_NUMIMAGES, 1, scaling, false, patternIconsInvert);
	m_PatternIconsDisabled.Create(IDB_PATTERNS, 16, 16, PATTERNIMG_NUMIMAGES, 1, scaling, true, patternIconsInvert);
	m_EnvelopeIcons.Create(IDB_ENVTOOLBAR, 20, 18, ENVIMG_NUMIMAGES, 1, scaling, false, envelopeIconsInvert);
	m_SampleIcons.Create(IDB_SMPTOOLBAR, 20, 18, SAMPLEIMG_NUMIMAGES, 1, scaling, false);

	HighDPISupport::CreateGUIFont(m_hGUIFont, this);
}


void CMainFrame::SetupStatusBarSizes()
{
	m_wndStatusBar.SetPaneInfo(0, ID_SEPARATOR, mfcless::pane::Stretch, HighDPISupport::ScalePixels(256, this));

	// GetFont() seem to give us the font for the previous DPI right after the DPI change
	const QFontMetrics metrics(m_hGUIFont);
	for(size_t i = 0; i < std::size(StatusBarIndicators); i++)
	{
		if(StatusBarIndicators[i] == ID_SEPARATOR)
			continue;
		const QString text = QString::fromStdU16String(mfcless::res::String(StatusBarIndicators[i]));
		MPT_ASSERT(!text.isEmpty());
		m_wndStatusBar.SetPaneInfo(static_cast<int>(i), StatusBarIndicators[i], mfcless::pane::Normal, metrics.horizontalAdvance(text));
	}
}


bool CMainFrame::DestroyWindow()
{
	m_unloading = true;

	// Kill Timer
	if(m_nTimer)
	{
		KillTimer(m_nTimer);
		m_nTimer = 0;
	}
	if(midiInData.inHandle)
		midiCloseDevice();
	// Delete bitmaps
	delete bmpNotes;
	bmpNotes = nullptr;

	PatternFont::DeleteFontData();

	// Kill GDI Objects
	m_hGUIFont = QFont();
	penDarkGray.DeleteObject();
	penGray99.DeleteObject();
	penHalfDarkGray.DeleteObject();

	return mfcless::Window<QMainWindow>::DestroyWindow();
}


void CMainFrame::closeEvent(QCloseEvent *e)
{
	// The window stays (closing it ends the program); OnClose decides.
	e->ignore();
	OnClose();
}


void CMainFrame::OnClose()
{
	MPT_TRACE_SCOPE();
	if(!(TrackerSettings::Instance().patternSetup & PatternSetup::NoCustomCloseDialog))
	{
		// Show modified documents window
		CloseMainDialog dlg;
		if(dlg.DoModal() != IDOK)
		{
			return;
		}
	}

#if defined(MPT_ENABLE_UPDATE)
	m_cancelUpdateCheck = true;
#endif // MPT_ENABLE_UPDATE

	CChildFrame *pMDIActive = MDIGetActive();

	BeginWaitCursor();
	if (IsPlaying()) PauseMod();
	if (pMDIActive) pMDIActive->SavePosition(true);

	if(gpSoundDevice)
	{
		gpSoundDevice->Stop();
		gpSoundDevice->Close();
		delete gpSoundDevice;
		gpSoundDevice = nullptr;
	}

	// Save Settings
	theApp.GetSettings().Write<std::string>(U_("Display"), U_("ToolbarState"), saveState().toBase64().toStdString());
	TrackerSettings::Instance().SaveSettings();

	if(m_InputHandler->m_activeCommandSet)
	{
		m_InputHandler->m_activeCommandSet->SaveFile(TrackerSettings::Instance().m_szKbdFile);
	}

#if defined(MPT_ENABLE_UPDATE)
	CUpdateCheck::WaitForUpdateCheckFinished();
#endif // MPT_ENABLE_UPDATE

	EndWaitCursor();

	// mfcless: CFrameWnd::OnClose closes all documents (they were saved or
	// discarded by the dialog above) and ends the program.
	theApp.GetModDocTemplate()->CloseAllDocuments(true);
	DestroyWindow();
	QApplication::quit();
}


// Drop files from Windows
void CMainFrame::OnDropFiles(const QStringList &files)
{
	activateWindow();
	raise();
#ifdef MPT_BUILD_DEBUG
	const bool scanAll = CInputHandler::GetModifierMask().test_all(ModCtrl | ModShift | ModAlt);
#endif
	for(const QString &fileName : files)
	{
		{
			const mpt::PathString file = mpt::PathString::FromUnicode(mfcless::FromQString(fileName));
#ifdef MPT_BUILD_DEBUG
			// Debug Hack: Quickly scan a folder containing module files (without running out of window handles ;)
			if(scanAll && mpt::native_fs{}.is_directory(file))
			{
				FolderScanner scanner(file, FolderScanner::kOnlyFiles | FolderScanner::kFindInSubDirectories);
				mpt::PathString scanName;
				size_t failed = 0, total = 0;
				while(scanner.Next(scanName))
				{
					mpt::IO::InputFile inputFile(scanName, TrackerSettings::Instance().MiscCacheCompleteFileBeforeLoading);
					if(!inputFile.IsValid())
						continue;
					SetHelpText(scanName.GetFilename().ToUnicode());
					auto sndFile = std::make_unique<CSoundFile>();
					MPT_LOG_GLOBAL(LogDebug, "info", U_("Loading ") + scanName.ToUnicode());
					if(!sndFile->Create(GetFileReader(inputFile), CSoundFile::loadCompleteModule, nullptr))
					{
						MPT_LOG_GLOBAL(LogDebug, "info", U_("FAILED: ") + scanName.ToUnicode());
						failed++;
					}
					total++;
				}
				SetHelpText(U_(""));
				Reporting::Information(MPT_UFORMAT("Scanned {} files, {} failed")(total, failed));
				continue;
			} else if(scanAll)
			{
				mpt::IO::InputFile inputFile(file, TrackerSettings::Instance().MiscCacheCompleteFileBeforeLoading);
				if(!inputFile.IsValid())
					continue;
				auto sndFile = std::make_unique<CSoundFile>();
				SetHelpText(file.GetFilename().ToUnicode());
				MPT_LOG_GLOBAL(LogDebug, "info", U_("Loading ") + file.ToUnicode());
				if(!sndFile->Create(GetFileReader(inputFile), CSoundFile::loadCompleteModule, nullptr))
					MPT_LOG_GLOBAL(LogDebug, "info", U_("FAILED: ") + file.ToUnicode());
				SetHelpText(U_(""));
				continue;
			}
#endif
			theApp.OpenDocumentFile(file);
		}
	}
}


void CMainFrame::OnApplicationStateChanged(bool active)
{
	if(active)
		IPCWindow::UpdateLastUsed();
	// Avoid keyboard focus being trapped on MDI client area
	if(active && m_quickStartDlg && m_quickStartDlg->isVisible() && QApplication::focusWidget() == m_mdiArea)
		m_quickStartDlg->setFocus();
}


void CMainFrame::OnFocusChanged(QWidget *lostFocusWnd, QWidget *gainFocusWnd)
{
	// Hook to keep track of last focussed GUI item. This solves various focus issues when switching between
	// CModControlDlg / CModScrollView via keyboard shortcuts, or when switching to another application and back.
	// See https://bugs.openmpt.org/view.php?id=1795 / https://bugs.openmpt.org/view.php?id=1799 / https://bugs.openmpt.org/view.php?id=1800
	if(!lostFocusWnd || !gainFocusWnd)
		return;

	QWidget *const lostFocus = lostFocusWnd, *const gainFocus = gainFocusWnd;
	const QWidget *mainWnd = this;
	CModControlDlg *parentCtrl = nullptr;
	CModScrollView *parentScroll = nullptr;
	do
	{
		// Does the window that lost focus belong to the upper or lower half of a module view?
		if(parentCtrl = dynamic_cast<CModControlDlg *>(lostFocusWnd); parentCtrl != nullptr)
			break;
		else if(parentScroll = dynamic_cast<CModScrollView *>(lostFocusWnd); parentScroll != nullptr)
			break;

		lostFocusWnd = lostFocusWnd->parentWidget();
	} while(lostFocusWnd && lostFocusWnd != mainWnd);

	if(parentCtrl || parentScroll)
	{
		// Focus was lost inside an MDI view. Check if the new focus item inside the same view.
		// If both are part of the same view, store the new focus item, otherwise the old one.
		bool sameParent = false;
		do
		{
			if((parentCtrl && gainFocusWnd == parentCtrl) || (parentScroll && gainFocusWnd == parentScroll))
			{
				sameParent = true;
				break;
			}

			gainFocusWnd = gainFocusWnd->parentWidget();
		} while(gainFocusWnd && gainFocusWnd != mainWnd);

		QWidget *lastFocus = sameParent ? gainFocus : lostFocus;
		if(parentCtrl && parentCtrl != lastFocus)
			parentCtrl->SaveLastFocusItem(lastFocus);
		else if(parentScroll && parentScroll != lastFocus)
			parentScroll->SaveLastFocusItem(lastFocus);
	}
}


bool CMainFrame::PreTranslateMessage(mfcless::Message *pMsg)
{
	// Right-click menu to disable/enable tree view and main toolbar when right-clicking on either the menu strip or main toolbar
	// mfcless: QMainWindow offers that menu itself (createPopupMenu); ours replaces it.

	// We handle keypresses before Windows has a chance to handle them (for alt etc..)
	if(pMsg->message == mfcless::msg::KeyDown || pMsg->message == mfcless::msg::KeyUp || pMsg->message == mfcless::msg::SysKeyUp || pMsg->message == mfcless::msg::SysKeyDown)
	{
		CInputHandler *ih = CMainFrame::GetInputHandler();
		const auto event = ih->Translate(*pMsg);
		if(ih->KeyEvent(kCtxAllContexts, event) != kcNull)
			return true;  // Mapped to a command, no need to pass message on.
	}

	return WindowBase::PreTranslateMessage(pMsg);
}


void CMainFrame::OnUpdateFrameTitle(bool bAddToTitle)
{
	CChildFrame * pActiveChild = nullptr;
	CModDoc *pDocument = GetActiveDoc();
	if (bAddToTitle &&
	  (pActiveChild = MDIGetActive()) != nullptr &&
	  !pActiveChild->isMaximized() &&
	  (pDocument != nullptr ||
	   (pDocument = pActiveChild->GetActiveDocument()) != nullptr))
	{
		mpt::ustring title = pDocument->GetTitle();
		if(pDocument->IsModified())
			title.push_back(u8'*');
		UpdateFrameTitleForDocument(title);
	} else
	{
		mpt::ustring strTitle;

		if (pActiveChild != nullptr)
		{
			strTitle = mfcless::FromQString(pActiveChild->windowTitle());
		}
		UpdateFrameTitleForDocument(strTitle);
	}
}


void CMainFrame::UpdateFrameTitleForDocument(const mpt::ustring &docTitle)
{
	// As CFrameWnd does: "document - application" (FWS_PREFIXTITLE).
	QString title = mfcless::ToQString(m_strTitle);
	if(!docTitle.empty())
		title = mfcless::ToQString(docTitle) + QStringLiteral(" - ") + title;
	setWindowTitle(title);
}


void CMainFrame::RecalcLayout(bool)
{
	if(m_quickStartDlg && m_quickStartDlg->isVisible())
	{
		m_quickStartDlg->UpdateHeight();
		m_quickStartDlg->CenterWindow();
	}
}


bool CMainFrame::InGuiThread() const noexcept
{
	return theApp.InGuiThread();
}


CMainFrame *CMainFrame::GetMainFrame() noexcept
{
	return g_mainFrame;
}


// mfcless: CFrameWnd's menu, command routing and MDI client

void CMainFrame::DrawMenuBar()
{
	QMenuBar *bar = menuBar();
	bar->clear();
	if(m_menu)
	{
		for(QAction *action : m_menu.Get()->actions())
			bar->addAction(action);
	}
}


bool CMainFrame::RouteCommand(uint32 id)
{
	if(CChildFrame *child = MDIGetActive(); child && child->RouteCommand(id))
		return true;
	if(DispatchCommand(id))
		return true;
	return theApp.RouteCommand(id);
}


bool CMainFrame::RouteCommandUpdate(uint32 id, mfcless::CommandState &state)
{
	if(CChildFrame *child = MDIGetActive(); child && child->RouteCommandUpdate(id, state))
		return true;
	if(DispatchCommandUpdate(id, state))
		return true;
	return theApp.RouteCommandUpdate(id, state);
}


bool CMainFrame::UpdateCommandState(uint32 id, mfcless::CommandState &state)
{
	// MFC disables items that nothing would handle (m_bAutoMenuEnable).
	bool handled = false;
	if(CChildFrame *child = MDIGetActive())
	{
		mfcless::DocumentView *view = child->GetActiveView() ? child->GetActiveView() : child->GetModControlView();
		if(view && view->GetMessageMap() && view->GetMessageMap()->HasCommand(id))
			handled = true;
		else if(child->GetActiveDocument() && child->GetActiveDocument()->HasCommand(id))
			handled = true;
		else if(child->HasCommand(id))
			handled = true;
	}
	if(!handled)
		handled = HasCommand(id) || theApp.HasCommand(id);
	state.enabled = handled;
	RouteCommandUpdate(id, state);
	return true;
}


CChildFrame *CMainFrame::MDIGetActive() const
{
	if(!m_mdiArea)
		return nullptr;
	if(auto *frame = qobject_cast<CChildFrame *>(m_mdiArea->activeSubWindow()))
		return frame;
	// While the main window is inactive, QMdiArea has no active child; MDI
	// still has one.
	if(auto *frame = qobject_cast<CChildFrame *>(m_mdiArea->currentSubWindow()))
		return frame;
	return nullptr;
}


void CMainFrame::MDIActivate(CChildFrame *frame)
{
	if(m_mdiArea && frame)
		m_mdiArea->setActiveSubWindow(frame);
}


void CMainFrame::MDINext()
{
	if(m_mdiArea)
		m_mdiArea->activateNextSubWindow();
}


void CMainFrame::MDIPrev()
{
	if(m_mdiArea)
		m_mdiArea->activatePreviousSubWindow();
}


bool CMainFrame::OnBarCheck(uint32 id)
{
	if(id == IDD_TREEVIEW)
	{
		m_wndTree.setVisible(!m_wndTree.isVisible());
		return true;
	} else if(id == ID_VIEW_TOOLBAR)
	{
		m_wndToolBar.setVisible(!m_wndToolBar.isVisible());
		return true;
	}
	return false;
}


void CMainFrame::OnUpdateControlBarMenu(mfcless::CommandState &state)
{
	if(state.id == IDD_TREEVIEW)
	{
		state.Enable(true);
		state.SetCheck(m_wndTree.isVisible());
	} else if(state.id == ID_VIEW_TOOLBAR)
	{
		state.Enable(true);
		state.SetCheck(m_wndToolBar.isVisible());
	}
}


void CMainFrame::BeginWaitCursor()
{
	QApplication::setOverrideCursor(Qt::WaitCursor);
}


void CMainFrame::EndWaitCursor()
{
	QApplication::restoreOverrideCursor();
}


/////////////////////////////////////////////////////////////////////////////
// CMainFrame Sound Library


void CMainFrame::OnTimerNotify()
{
	MPT_TRACE_SCOPE();
	MPT_ASSERT(InGuiThread());
	MPT_ASSERT(!InNotifyHandler());
	m_InNotifyHandler = true;
	Notification PendingNotification;
	bool found = false;
	int64 currenttotalsamples = 0;
	if(gpSoundDevice)
	{
		currenttotalsamples = gpSoundDevice->GetStreamPosition().Frames;
	}
	{
		// advance to the newest notification, drop the obsolete ones
		mpt::lock_guard<mpt::mutex> lock(m_NotificationBufferMutex);
		const Notification * pnotify = nullptr;
		const Notification * p = m_NotifyBuffer.peek_p();
		if(p && currenttotalsamples >= p->timestampSamples)
		{
			pnotify = p;
			while(m_NotifyBuffer.peek_next_p() && currenttotalsamples >= m_NotifyBuffer.peek_next_p()->timestampSamples)
			{
				m_NotifyBuffer.pop();
				p = m_NotifyBuffer.peek_p();
				pnotify = p;
			}
		}
		if(pnotify)
		{
			PendingNotification = *pnotify; // copy notification so that we can free the buffer
			found = true;
			{
				m_NotifyBuffer.pop();
			}
		}
	}
	if(found)
	{
		OnUpdatePosition(0, (intptr_t)&PendingNotification);
	}
	m_InNotifyHandler = false;
	MPT_ASSERT(!InNotifyHandler());
}


void CMainFrame::SoundDeviceMessage(LogLevel level, const mpt::ustring &str)
{
	MPT_TRACE();
	Reporting::Message(level, str);
}


void CMainFrame::SoundCallbackPreStart()
{
	MPT_TRACE();
	m_SoundDeviceClock.SetResolution(1);
}


void CMainFrame::SoundCallbackPostStop()
{
	MPT_TRACE();
	m_SoundDeviceClock.SetResolution(0);
}


uint64 CMainFrame::SoundCallbackGetReferenceClockNowNanoseconds() const
{
	MPT_TRACE();
	MPT_ASSERT(!InAudioThread());
	return m_SoundDeviceClock.NowNanoseconds();
}


uint64 CMainFrame::SoundCallbackLockedGetReferenceClockNowNanoseconds() const
{
	MPT_TRACE();
	MPT_ASSERT(InAudioThread());
	return m_SoundDeviceClock.NowNanoseconds();
}


bool CMainFrame::SoundCallbackIsLockedByCurrentThread() const
{
	MPT_TRACE();
	return theApp.GetGlobalMutexRef().IsLockedByCurrentThread();
}


void CMainFrame::SoundCallbackLock()
{
	MPT_TRACE_SCOPE();
	m_SoundDeviceFillBufferCriticalSection.Enter();
	MPT_ASSERT_ALWAYS(m_pSndFile != nullptr);
	m_AudioThreadId = mfcless::CurrentThreadId();
	mpt::log::Trace::SetThreadId(mpt::log::Trace::ThreadKindAudio, m_AudioThreadId);
}


#if MPT_COMPILER_MSVC
// silence static analyzer warning
#pragma warning(push)
#pragma warning(disable:26110) // Caller failing to hold lock
#endif // MPT_COMPILER_MSVC
void CMainFrame::SoundCallbackUnlock()
{
	MPT_TRACE_SCOPE();
	MPT_ASSERT_ALWAYS(m_pSndFile != nullptr);
	m_AudioThreadId = 0;
	m_SoundDeviceFillBufferCriticalSection.Leave();
}
#if MPT_COMPILER_MSVC
#pragma warning(pop)
#endif // MPT_COMPILER_MSVC


class BufferInputWrapper
	: public IAudioSource
{
private:
	SoundDevice::CallbackBuffer<DithersOpenMPT> &callbackBuffer;
public:
	inline BufferInputWrapper(SoundDevice::CallbackBuffer<DithersOpenMPT> &callbackBuffer_)
		: callbackBuffer(callbackBuffer_)
	{
		return;
	}
	inline void Process(mpt::audio_span_planar<MixSampleInt> buffer) override
	{
		callbackBuffer.template ReadFixedPoint<MixSampleIntTraits::mix_fractional_bits>(buffer);
	}
	inline void Process(mpt::audio_span_planar<MixSampleFloat> buffer) override
	{
		callbackBuffer.Read(buffer);
	}
};


class BufferOutputWrapper
	: public IAudioTarget
{
private:
	SoundDevice::CallbackBuffer<DithersOpenMPT> &callbackBuffer;
public:
	inline BufferOutputWrapper(SoundDevice::CallbackBuffer<DithersOpenMPT> &callbackBuffer_)
		: callbackBuffer(callbackBuffer_)
	{
		return;
	}
	inline void Process(mpt::audio_span_interleaved<MixSampleInt> buffer) override
	{
		callbackBuffer.template WriteFixedPoint<MixSampleIntTraits::mix_fractional_bits>(buffer);
	}
	inline void Process(mpt::audio_span_interleaved<MixSampleFloat> buffer) override
	{
		callbackBuffer.Write(buffer);
	}
};


void CMainFrame::SoundCallbackLockedProcessPrepare(SoundDevice::TimeInfo timeInfo)
{
	MPT_TRACE_SCOPE();
	MPT_ASSERT(InAudioThread());
	TimingInfo timingInfo;
	timingInfo.OutputLatency = timeInfo.Latency;
	timingInfo.StreamFrames = timeInfo.SyncPointStreamFrames;
	timingInfo.SystemTimestamp = timeInfo.SyncPointSystemTimestamp;
	timingInfo.Speed = timeInfo.Speed;
	m_pSndFile->m_TimingInfo = timingInfo;
}


void CMainFrame::SoundCallbackLockedCallback(SoundDevice::CallbackBuffer<DithersOpenMPT> &buffer)
{
	MPT_TRACE_SCOPE();
	MPT_ASSERT(InAudioThread());
	OPENMPT_PROFILE_FUNCTION(Profiler::Audio);
	BufferInputWrapper source(buffer);
	BufferOutputWrapper target(buffer);
	MPT_ASSERT(buffer.GetNumFrames() <= std::numeric_limits<samplecount_t>::max());
	samplecount_t framesToRender = static_cast<samplecount_t>(buffer.GetNumFrames());
	MPT_ASSERT(framesToRender > 0);
	samplecount_t renderedFrames = m_pSndFile->Read(framesToRender, target, source, std::ref(m_VUMeterOutput), std::ref(m_VUMeterInput));
	MPT_ASSERT(renderedFrames <= framesToRender);
	[[maybe_unused]] samplecount_t remainingFrames = framesToRender - renderedFrames;
	MPT_ASSERT(remainingFrames >= 0); // remaining buffer is filled with silence automatically
}


void CMainFrame::SoundCallbackLockedProcessDone(SoundDevice::TimeInfo timeInfo)
{
	MPT_TRACE_SCOPE();
	MPT_ASSERT(InAudioThread());
	OPENMPT_PROFILE_FUNCTION(Profiler::Notify);
	MPT_ASSERT((timeInfo.RenderStreamPositionAfter.Frames - timeInfo.RenderStreamPositionBefore.Frames) < std::numeric_limits<samplecount_t>::max());
	samplecount_t framesRendered = static_cast<samplecount_t>(timeInfo.RenderStreamPositionAfter.Frames - timeInfo.RenderStreamPositionBefore.Frames);
	int64 streamPosition = timeInfo.RenderStreamPositionAfter.Frames;
	DoNotification(framesRendered, streamPosition);
	//m_pSndFile->m_TimingInfo = TimingInfo(); // reset
}


bool CMainFrame::IsAudioDeviceOpen() const
{
	MPT_TRACE_SCOPE();
	return gpSoundDevice && gpSoundDevice->IsOpen();
}


bool CMainFrame::audioOpenDevice()
{
	MPT_TRACE_SCOPE();
	const SoundDevice::Identifier deviceIdentifier = TrackerSettings::Instance().GetSoundDeviceIdentifier();
	if(!TrackerSettings::Instance().GetMixerSettings().IsValid())
	{
		Reporting::Error(MPT_UFORMAT("Unable to open sound device '{}': Invalid mixer settings.")(deviceIdentifier));
		return false;
	}
	if(gpSoundDevice && (gpSoundDevice->GetDeviceInfo().GetIdentifier() != deviceIdentifier))
	{
		gpSoundDevice->Stop();
		gpSoundDevice->Close();
		delete gpSoundDevice;
		gpSoundDevice = nullptr;
	}
	if(IsAudioDeviceOpen())
	{
		return true;
	}
	if(!gpSoundDevice)
	{
		gpSoundDevice = theApp.GetSoundDevicesManager()->CreateSoundDevice(deviceIdentifier);
	}
	if(!gpSoundDevice)
	{
		Reporting::Error(MPT_UFORMAT("Unable to open sound device '{}': Could not find sound device.")(deviceIdentifier));
		return false;
	}
	gpSoundDevice->SetMessageReceiver(this);
	gpSoundDevice->SetCallback(this);
	SoundDevice::Settings deviceSettings = TrackerSettings::Instance().GetSoundDeviceSettings(deviceIdentifier);
	if(!gpSoundDevice->Open(deviceSettings))
	{
		if(!gpSoundDevice->IsAvailable())
		{
			Reporting::Error(MPT_UFORMAT("Unable to open sound device '{}': Device not available.")(gpSoundDevice->GetDeviceInfo().GetDisplayName()));
		} else
		{
			Reporting::Error(MPT_UFORMAT("Unable to open sound device '{}'.")(gpSoundDevice->GetDeviceInfo().GetDisplayName()));
		}
		return false;
	}
	SampleFormat actualSampleFormat = gpSoundDevice->GetActualSampleFormat();
	deviceSettings.sampleFormat = actualSampleFormat;
	Dithers().SetMode(deviceSettings.DitherType, deviceSettings.Channels);
	TrackerSettings::Instance().MixerSamplerate = gpSoundDevice->GetSettings().Samplerate;
	TrackerSettings::Instance().SetSoundDeviceSettings(deviceIdentifier, deviceSettings);
	return true;
}


void CMainFrame::audioCloseDevice()
{
	MPT_TRACE_SCOPE();
	if(gpSoundDevice)
	{
		gpSoundDevice->Close();
	}
	if(m_NotifyTimer)
	{
		KillTimer(m_NotifyTimer);
		m_NotifyTimer = 0;
	}
	ResetNotificationBuffer();
}


void VUMeter::Process(Channel &c, MixSampleInt sample)
{
	c.peak = std::max(c.peak, std::abs(sample));
	if(sample < MixSampleIntTraits::mix_clip_min || MixSampleIntTraits::mix_clip_max < sample)
	{
		c.clipped = true;
	}
}


void VUMeter::Process(Channel &c, MixSampleFloat sample)
{
	Process(c, SC::ConvertToFixedPoint<MixSampleInt, MixSampleFloat, MixSampleIntTraits::mix_fractional_bits>()(sample));
}


void VUMeter::Process(mpt::audio_span_interleaved<const MixSampleInt> buffer)
{
	for(std::size_t frame = 0; frame < buffer.size_frames(); ++frame)
	{
		for(std::size_t channel = 0; channel < std::min(buffer.size_channels(), maxChannels); ++channel)
		{
			Process(channels[channel], buffer(channel, frame));
		}
	}
	for(std::size_t channel = std::min(buffer.size_channels(), maxChannels); channel < maxChannels; ++channel)
	{
		channels[channel] = Channel();
	}
}


void VUMeter::Process(mpt::audio_span_interleaved<const MixSampleFloat> buffer)
{
	for(std::size_t frame = 0; frame < buffer.size_frames(); ++frame)
	{
		for(std::size_t channel = 0; channel < std::min(buffer.size_channels(), maxChannels); ++channel)
		{
			Process(channels[channel], buffer(channel, frame));
		}
	}
	for(std::size_t channel = std::min(buffer.size_channels(), maxChannels); channel < maxChannels; ++channel)
	{
		channels[channel] = Channel();
	}
}


void VUMeter::Process(mpt::audio_span_planar<const MixSampleInt> buffer)
{
	for(std::size_t frame = 0; frame < buffer.size_frames(); ++frame)
	{
		for(std::size_t channel = 0; channel < std::min(buffer.size_channels(), maxChannels); ++channel)
		{
			Process(channels[channel], buffer(channel, frame));
		}
	}
	for(std::size_t channel = std::min(buffer.size_channels(), maxChannels); channel < maxChannels; ++channel)
	{
		channels[channel] = Channel();
	}
}


void VUMeter::Process(mpt::audio_span_planar<const MixSampleFloat> buffer)
{
	for(std::size_t frame = 0; frame < buffer.size_frames(); ++frame)
	{
		for(std::size_t channel = 0; channel < std::min(buffer.size_channels(), maxChannels); ++channel)
		{
			Process(channels[channel], buffer(channel, frame));
		}
	}
	for(std::size_t channel = std::min(buffer.size_channels(), maxChannels); channel < maxChannels; ++channel)
	{
		channels[channel] = Channel();
	}
}


const float VUMeter::dynamicRange = 48.0f; // corresponds to the current implementation of the UI widget displaying the result


void VUMeter::SetDecaySpeedDecibelPerSecond(float decibelPerSecond)
{
	float linearDecayRate = decibelPerSecond / dynamicRange;
	decayParam = mpt::saturate_round<int32>(linearDecayRate * static_cast<float>(MixSampleIntTraits::mix_clip_max));
}


void VUMeter::Decay(int32 secondsNum, int32 secondsDen)
{
	int32 decay = Util::muldivr(decayParam, secondsNum, secondsDen);
	for(std::size_t channel = 0; channel < maxChannels; ++channel)
	{
		channels[channel].peak = std::max(channels[channel].peak - decay, 0);
	}
}


void VUMeter::ResetClipped()
{
	for(std::size_t channel = 0; channel < maxChannels; ++channel)
	{
		channels[channel].clipped = false;
	}
}


static void SetVUMeter(std::array<uint32, 4> &masterVU, const VUMeter &vumeter)
{
	for(std::size_t channel = 0; channel < VUMeter::maxChannels; ++channel)
	{
		masterVU[channel] = Clamp(vumeter[channel].peak >> 11, 0, 0x10000);
		if(vumeter[channel].clipped)
		{
			masterVU[channel] |= Notification::ClipVU;
		}
	}
}


bool CMainFrame::DoNotification(uint32 dwSamplesRead, int64 streamPosition)
{
	MPT_TRACE_SCOPE();
	MPT_ASSERT(InAudioThread());
	if(!m_pSndFile) return false;

	FlagSet<Notification::Type> notifyType(Notification::Default);
	Notification::Item notifyItem = 0;

	if(CModDoc *modDoc = m_pSndFile->GetpModDoc())
	{
		notifyType = modDoc->GetNotificationType();
		notifyItem = modDoc->GetNotificationItem();
	}

	// Add an entry to the notification history

	Notification notification(notifyType, notifyItem, streamPosition, m_pSndFile->m_PlayState.m_nRow, m_pSndFile->m_PlayState.m_nTickCount, m_pSndFile->m_PlayState.TicksOnRow(), m_pSndFile->m_PlayState.m_nCurrentOrder, m_pSndFile->m_PlayState.m_nPattern, m_pSndFile->GetMixStat(), static_cast<uint8>(m_pSndFile->m_MixerSettings.gnChannels), static_cast<uint8>(m_pSndFile->m_MixerSettings.NumInputChannels));

	m_pSndFile->ResetMixStat();

	if(m_pSndFile->m_PlayState.m_flags[SONG_ENDREACHED]) notification.type.set(Notification::EOS);

	if(notifyType[Notification::Sample])
	{
		// Sample positions
		const SAMPLEINDEX smp = notifyItem;
		if(smp > 0 && smp <= m_pSndFile->GetNumSamples() && m_pSndFile->GetSample(smp).HasSampleData())
		{
			for(CHANNELINDEX k = 0; k < MAX_CHANNELS; k++)
			{
				const ModChannel &chn = m_pSndFile->m_PlayState.Chn[k];
				if(chn.pModSample == &m_pSndFile->GetSample(smp) && chn.nLength != 0	// Correct sample is set up on this channel
					&& (!chn.dwFlags[CHN_NOTEFADE] || chn.nFadeOutVol))					// And it hasn't completely faded out yet, so it's still playing
				{
					notification.pos[k] = chn.position.GetInt();
				} else
				{
					notification.pos[k] = Notification::PosInvalid;
				}
			}
		} else
		{
			// Can't generate a valid notification.
			notification.type.reset(Notification::Sample);
		}
	} else if(notifyType[Notification::VolEnv | Notification::PanEnv | Notification::PitchEnv])
	{
		// Instrument envelopes
		const INSTRUMENTINDEX ins = notifyItem;

		EnvelopeType notifyEnv = ENV_VOLUME;
		if(notifyType[Notification::PitchEnv])
			notifyEnv = ENV_PITCH;
		else if(notifyType[Notification::PanEnv])
			notifyEnv = ENV_PANNING;

		if(ins > 0 && ins <= m_pSndFile->GetNumInstruments() && m_pSndFile->Instruments[ins] != nullptr)
		{
			for(CHANNELINDEX k = 0; k < MAX_CHANNELS; k++)
			{
				const ModChannel &chn = m_pSndFile->m_PlayState.Chn[k];
				SmpLength pos = Notification::PosInvalid;

				if(chn.pModInstrument == m_pSndFile->Instruments[ins]				// Correct instrument is set up on this channel
					&& (chn.nLength || chn.pModInstrument->HasValidMIDIChannel())	// And it's playing something (sample or instrument plugin)
					&& (!chn.dwFlags[CHN_NOTEFADE] || chn.nFadeOutVol))				// And it hasn't completely faded out yet, so it's still playing
				{
					const ModChannel::EnvInfo &chnEnv = chn.GetEnvelope(notifyEnv);
					if(chnEnv.flags[ENV_ENABLED])
					{
						pos = chnEnv.nEnvPosition;
						if(m_pSndFile->m_playBehaviour[kITEnvelopePositionHandling])
						{
							// Impulse Tracker envelope handling (see e.g. CSoundFile::IncrementEnvelopePosition in SndMix.cpp for details)
							if(pos > 0)
								pos--;
							else
								pos = Notification::PosInvalid;	// Envelope isn't playing yet (e.g. when disabling it right when triggering a note)
						}
					}
				}
				notification.pos[k] = pos;
			}
		} else
		{
			// Can't generate a valid notification.
			notification.type.reset(Notification::VolEnv | Notification::PanEnv | Notification::PitchEnv);
		}
	} else if(notifyType[Notification::VUMeters])
	{
		// Pattern channel VU meters
		for(CHANNELINDEX k = 0; k < m_pSndFile->GetNumChannels(); k++)
		{
			uint32 vul = m_pSndFile->m_PlayState.Chn[k].nLeftVU;
			uint32 vur = m_pSndFile->m_PlayState.Chn[k].nRightVU;
			notification.pos[k] = (vul << 8) | (vur);
		}
	}

	{
		// Master VU meter
		SetVUMeter(notification.masterVUin, m_VUMeterInput);
		SetVUMeter(notification.masterVUout, m_VUMeterOutput);
		m_VUMeterInput.Decay(dwSamplesRead, m_pSndFile->m_MixerSettings.gdwMixingFreq);
		m_VUMeterOutput.Decay(dwSamplesRead, m_pSndFile->m_MixerSettings.gdwMixingFreq);

	}

	{
		mpt::lock_guard<mpt::mutex> lock(m_NotificationBufferMutex);
		if(m_NotifyBuffer.write_size() == 0)
		{
			MPT_ASSERT(0);
			return false; // drop notification
		}
		m_NotifyBuffer.push(notification);
	}

	return true;
}


void CMainFrame::UpdateDspEffects(CSoundFile &sndFile, bool reset)
{
	CriticalSection cs;
#ifndef NO_REVERB
	sndFile.m_Reverb.m_Settings = TrackerSettings::Instance().m_ReverbSettings;
#endif
#ifndef NO_DSP
	sndFile.m_Surround.m_Settings = TrackerSettings::Instance().m_SurroundSettings;
#endif
#ifndef NO_DSP
	sndFile.m_MegaBass.m_Settings = TrackerSettings::Instance().m_MegaBassSettings;
#endif
#ifndef NO_EQ
	sndFile.SetEQGains(TrackerSettings::Instance().m_EqSettings.Gains, TrackerSettings::Instance().m_EqSettings.Freqs, reset);
#endif
#ifndef NO_DSP
	sndFile.m_BitCrush.m_Settings = TrackerSettings::Instance().m_BitCrushSettings;
#endif
	sndFile.SetDspEffects(TrackerSettings::Instance().MixerDSPMask);
	sndFile.InitPlayer(reset);
}


void CMainFrame::UpdateAudioParameters(CSoundFile &sndFile, bool reset)
{
	CriticalSection cs;
	if (TrackerSettings::Instance().patternSetup & PatternSetup::IgnoreMutedChannels)
		TrackerSettings::Instance().MixerFlags |= SNDMIX_MUTECHNMODE;
	else
		TrackerSettings::Instance().MixerFlags &= ~SNDMIX_MUTECHNMODE;
	sndFile.SetMixerSettings(TrackerSettings::Instance().GetMixerSettings());
	sndFile.SetResamplerSettings(TrackerSettings::Instance().GetResamplerSettings());
	UpdateDspEffects(sndFile, false); // reset done in next line
	sndFile.InitPlayer(reset);
}


/////////////////////////////////////////////////////////////////////////////
// CMainFrame diagnostics

#ifdef _DEBUG
void CMainFrame::AssertValid() const
{
	mfcless::Window<QMainWindow>::AssertValid();
}

void CMainFrame::Dump(CDumpContext& dc) const
{
	mfcless::Window<QMainWindow>::Dump(dc);
}

#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CMainFrame static helpers


void CMainFrame::UpdateColors()
{
	const auto &colors = TrackerSettings::Instance().rgbCustomColors;
	// Generel tab VU meters
	for(uint32 i = 0; i < NUM_VUMETER_PENS * 2; i++)
	{
		int r0,g0,b0, r1,g1,b1;
		int r, g, b;
		int y;

		y = (i >= NUM_VUMETER_PENS) ? (i-NUM_VUMETER_PENS) : i;
		if (y < (NUM_VUMETER_PENS/2))
		{
			r0 = mfcless::RedOf(colors[MODCOLOR_VUMETER_LO]);
			g0 = mfcless::GreenOf(colors[MODCOLOR_VUMETER_LO]);
			b0 = mfcless::BlueOf(colors[MODCOLOR_VUMETER_LO]);
			r1 = mfcless::RedOf(colors[MODCOLOR_VUMETER_MED]);
			g1 = mfcless::GreenOf(colors[MODCOLOR_VUMETER_MED]);
			b1 = mfcless::BlueOf(colors[MODCOLOR_VUMETER_MED]);
		} else
		{
			y -= (NUM_VUMETER_PENS/2);
			r0 = mfcless::RedOf(colors[MODCOLOR_VUMETER_MED]);
			g0 = mfcless::GreenOf(colors[MODCOLOR_VUMETER_MED]);
			b0 = mfcless::BlueOf(colors[MODCOLOR_VUMETER_MED]);
			r1 = mfcless::RedOf(colors[MODCOLOR_VUMETER_HI]);
			g1 = mfcless::GreenOf(colors[MODCOLOR_VUMETER_HI]);
			b1 = mfcless::BlueOf(colors[MODCOLOR_VUMETER_HI]);
		}
		r = r0 + ((r1 - r0) * y) / (NUM_VUMETER_PENS/2);
		g = g0 + ((g1 - g0) * y) / (NUM_VUMETER_PENS/2);
		b = b0 + ((b1 - b0) * y) / (NUM_VUMETER_PENS/2);
		if (i >= NUM_VUMETER_PENS)
		{
			r = (r*2)/5;
			g = (g*2)/5;
			b = (b*2)/5;
		}
		gcolrefVuMeter[i] = mfcless::Rgb(r, g, b);
	}
	CMainFrame *mainFrm = GetMainFrame();
	if(mainFrm != nullptr)
	{
		mainFrm->m_wndToolBar.m_VuMeter->Invalidate();
	}
}


/////////////////////////////////////////////////////////////////////////////
// CMainFrame operations


uint32 CMainFrame::GetBaseOctave() const
{
	return m_wndToolBar.GetBaseOctave();
}


void CMainFrame::ResetNotificationBuffer()
{
	MPT_TRACE();
	mpt::lock_guard<mpt::mutex> lock(m_NotificationBufferMutex);
	m_NotifyBuffer.clear();
}


bool CMainFrame::PreparePlayback()
{
	MPT_TRACE_SCOPE();
	// open the audio device to update needed TrackerSettings mixer parameters
	if(!audioOpenDevice()) return false;
	return true;
}


bool CMainFrame::StartPlayback()
{
	MPT_TRACE_SCOPE();
	if(!m_pSndFile) return false; // nothing to play
	if(!IsAudioDeviceOpen()) return false;
	Dithers().Reset();
	if(!gpSoundDevice->Start()) return false;
	if(!m_NotifyTimer)
	{
		if(TrackerSettings::Instance().GUIUpdateInterval.Get() > 0)
		{
			m_NotifyTimer = SetTimer(TIMERID_NOTIFY, TrackerSettings::Instance().GUIUpdateInterval);
		} else
		{
			m_NotifyTimer = SetTimer(TIMERID_NOTIFY, std::max(int(1), mpt::saturate_round<int>(gpSoundDevice->GetEffectiveBufferAttributes().UpdateInterval * 1000.0)));
		}
	}
	return true;
}


void CMainFrame::StopPlayback()
{
	MPT_TRACE_SCOPE();
	if(!IsAudioDeviceOpen()) return;
	gpSoundDevice->Stop();
	if(m_NotifyTimer)
	{
		KillTimer(m_NotifyTimer);
		m_NotifyTimer = 0;
	}
	ResetNotificationBuffer();
	if(!gpSoundDevice->GetDeviceCaps().CanKeepDeviceRunning || TrackerSettings::Instance().m_SoundSettingsStopMode == SoundDeviceStopModeClosed)
	{
		audioCloseDevice();
	}
}


bool CMainFrame::RestartPlayback()
{
	MPT_TRACE_SCOPE();
	if(!m_pSndFile) return false; // nothing to play
	if(!IsAudioDeviceOpen()) return false;
	if(!gpSoundDevice->IsPlaying()) return false;
	gpSoundDevice->StopAndAvoidPlayingSilence();
	if(m_NotifyTimer)
	{
		KillTimer(m_NotifyTimer);
		m_NotifyTimer = 0;
	}
	ResetNotificationBuffer();
	return StartPlayback();
}


bool CMainFrame::PausePlayback()
{
	MPT_TRACE_SCOPE();
	if(!IsAudioDeviceOpen()) return false;
	gpSoundDevice->Stop();
	if(m_NotifyTimer)
	{
		KillTimer(m_NotifyTimer);
		m_NotifyTimer = 0;
	}
	ResetNotificationBuffer();
	return true;
}


void CMainFrame::GenerateStopNotification()
{
	Notification mn(Notification::Stop);
	SendMessage(WM_MOD_UPDATEPOSITION, 0, reinterpret_cast<intptr_t>(&mn));
}


void CMainFrame::UnsetPlaybackSoundFile()
{
	MPT_ASSERT_ALWAYS(!gpSoundDevice || !gpSoundDevice->IsPlaying());
	if(m_pSndFile)
	{
		CriticalSection cs;
		m_pSndFile->SuspendPlugins();
		m_pSndFile->m_PlayState.m_flags.reset(SONG_PAUSED);
		if(m_pSndFile == &m_WaveFile)
		{
			// Unload previewed instrument
			m_WaveFile.Destroy();
		} else
		{
			// Stop sample preview channels
			for(ModChannel &chn : m_pSndFile->m_PlayState.BackgroundChannels(*m_pSndFile))
			{
				if(chn.isPreviewNote)
				{
					chn.nLength = 0;
					chn.position.Set(0);
				}
			}
		}
		cs.Leave();
		if(m_pSndFile->GetpModDoc())
		{
			m_wndTree.UpdatePlayPos(m_pSndFile->GetpModDoc(), nullptr);
		}
	}
	m_pSndFile = nullptr;
	m_wndToolBar.SetCurrentSong(nullptr);
	ResetNotificationBuffer();
}


void CMainFrame::SetPlaybackSoundFile(CSoundFile *pSndFile)
{
	MPT_ASSERT_ALWAYS(pSndFile);
	m_pSndFile = pSndFile;
}


bool CMainFrame::PlayMod(CModDoc *pModDoc)
{
	MPT_ASSERT_ALWAYS(!theApp.GetGlobalMutexRef().IsLockedByCurrentThread());
	if(!pModDoc) return false;
	CSoundFile &sndFile = pModDoc->GetSoundFile();
	if(!IsValidSoundFile(sndFile)) return false;

	// if something is playing, pause it
	PausePlayback();
	GenerateStopNotification();

	UnsetPlaybackSoundFile();

	// open audio device if not already open
	if (!PreparePlayback()) return false;

	// set mixing parameters in CSoundFile
	UpdateAudioParameters(sndFile);

	SetPlaybackSoundFile(&sndFile);

	const bool bPaused = m_pSndFile->IsPaused();
	const bool bPatLoop = m_pSndFile->m_PlayState.m_flags[SONG_PATTERNLOOP];

	m_pSndFile->m_PlayState.m_flags.reset(SONG_FADINGSONG | SONG_ENDREACHED);

	if(!bPatLoop && bPaused) sndFile.m_PlayState.m_flags.set(SONG_PAUSED);
	sndFile.SetRepeatCount((TrackerSettings::Instance().gbLoopSong) ? -1 : 0);

	sndFile.InitPlayer(true);
	sndFile.ResumePlugins();

	m_wndToolBar.SetCurrentSong(m_pSndFile);

	m_VUMeterInput = VUMeter();
	m_VUMeterOutput = VUMeter();

	UpdateMetronomeSamples();

	if(!StartPlayback())
	{
		UnsetPlaybackSoundFile();
		return false;
	}

	return true;
}


bool CMainFrame::PauseMod(CModDoc *pModDoc)
{
	MPT_ASSERT_ALWAYS(!theApp.GetGlobalMutexRef().IsLockedByCurrentThread());
	if(pModDoc && (pModDoc != GetModPlaying())) return false;
	if(!IsPlaying()) return true;

	PausePlayback();
	GenerateStopNotification();

	UnsetPlaybackSoundFile();

	StopPlayback();

	return true;
}


bool CMainFrame::StopMod(CModDoc *pModDoc)
{
	MPT_ASSERT_ALWAYS(!theApp.GetGlobalMutexRef().IsLockedByCurrentThread());
	if(pModDoc && (pModDoc != GetModPlaying())) return false;
	if(!IsPlaying()) return true;

	PausePlayback();
	SetElapsedTime(0);
	GenerateStopNotification();

	m_pSndFile->ResetPlayPos();
	m_pSndFile->ResetChannels();
	UnsetPlaybackSoundFile();

	StopPlayback();

	return true;
}


bool CMainFrame::StopSoundFile(CSoundFile *pSndFile)
{
	MPT_ASSERT_ALWAYS(!theApp.GetGlobalMutexRef().IsLockedByCurrentThread());
	if(!IsValidSoundFile(pSndFile)) return false;
	if(pSndFile != m_pSndFile) return false;
	if(!IsPlaying()) return true;

	PausePlayback();
	GenerateStopNotification();

	UnsetPlaybackSoundFile();

	StopPlayback();

	return true;
}


bool CMainFrame::PlaySoundFile(CSoundFile *pSndFile)
{
	MPT_ASSERT_ALWAYS(!theApp.GetGlobalMutexRef().IsLockedByCurrentThread());
	if(!IsValidSoundFile(pSndFile)) return false;

	PausePlayback();
	GenerateStopNotification();

	if(m_pSndFile != pSndFile)
	{
		UnsetPlaybackSoundFile();
	}

	if(!PreparePlayback())
	{
		UnsetPlaybackSoundFile();
		return false;
	}

	UpdateAudioParameters(*pSndFile);

	SetPlaybackSoundFile(pSndFile);

	m_pSndFile->InitPlayer(true);

	if(!StartPlayback())
	{
		UnsetPlaybackSoundFile();
		return false;
	}

	return true;
}


bool CMainFrame::PlayDLSInstrument(const CDLSBank &bank, uint32 instr, uint32 region, ModCommand::NOTE note, int volume)
{
	bool ok = false;
	BeginWaitCursor();
	{
		CriticalSection cs;
		if(ModCommand::IsNote(note))
		{
			InitPreview();
			if(bank.ExtractInstrument(m_WaveFile, 1, instr, region))
			{
				PreparePreview(note, volume);
				ok = true;
			}
		} else
		{
			PreparePreview(NOTE_NOTECUT, volume);
			ok = true;
		}
	}
	EndWaitCursor();
	if(!ok)
	{
		PausePlayback();
		UnsetPlaybackSoundFile();
		StopPlayback();
		return false;
	}
	if(IsPlaying() && (m_pSndFile == &m_WaveFile))
	{
		return true;
	}
	return PlaySoundFile(&m_WaveFile);
}


bool CMainFrame::PlaySoundFile(const mpt::PathString &filename, ModCommand::NOTE note, int volume)
{
	bool ok = false;
	BeginWaitCursor();
	{
		CriticalSection cs;
		static mpt::PathString prevFile;
		// Did we already load this file for previewing? Don't load it again if the preview is still running.
		ok = (prevFile == filename && m_pSndFile == &m_WaveFile);

		if(!ok && !filename.empty())
		{
			mpt::IO::InputFile f(filename, TrackerSettings::Instance().MiscCacheCompleteFileBeforeLoading);
			if(f.IsValid())
			{
				FileReader file = GetFileReader(f);
				if(file.IsValid())
				{
					InitPreview();
					m_WaveFile.m_PlayState.m_flags.set(SONG_PAUSED);
					// Avoid hanging audio while reading file - we have removed all sample and instrument references before,
					// so it's safe to replace the sample / instrument now.
					cs.Leave();
					ok = m_WaveFile.ReadInstrumentFromFile(1, file, TrackerSettings::Instance().m_MayNormalizeSamplesOnLoad);
					cs.Enter();
					if(!ok)
					{
						// Try reading as sample if reading as instrument fails
						ok = m_WaveFile.ReadSampleFromFile(1, file, TrackerSettings::Instance().m_MayNormalizeSamplesOnLoad);
						m_WaveFile.AllocateInstrument(1, 1);
					}
				}
			}
		}
		if(ok)
		{
			// Write notes to pattern. Also done if we have previously loaded this file, since we might be previewing another note now.
			PreparePreview(note, volume);
			prevFile = filename;
		}
	}
	EndWaitCursor();
	if(!ok)
	{
		PausePlayback();
		UnsetPlaybackSoundFile();
		StopPlayback();
		return false;
	}
	if(IsPlaying() && (m_pSndFile == &m_WaveFile))
	{
		return true;
	}
	return PlaySoundFile(&m_WaveFile);
}


bool CMainFrame::PlaySoundFile(CSoundFile &sndFile, INSTRUMENTINDEX nInstrument, SAMPLEINDEX nSample, ModCommand::NOTE note, int volume)
{
	bool ok = false;
	BeginWaitCursor();
	{
		CriticalSection cs;
		InitPreview();
		m_WaveFile.ChangeModTypeTo(sndFile.GetType(), false);
		m_WaveFile.m_playBehaviour = sndFile.m_playBehaviour;
		if ((nInstrument) && (nInstrument <= sndFile.GetNumInstruments()))
		{
			m_WaveFile.m_nInstruments = 1;
			m_WaveFile.m_nSamples = 32;
		} else
		{
			m_WaveFile.m_nInstruments = 0;
			m_WaveFile.m_nSamples = 1;
		}
		if (nInstrument != INSTRUMENTINDEX_INVALID && nInstrument <= sndFile.GetNumInstruments())
		{
			m_WaveFile.ReadInstrumentFromSong(1, sndFile, nInstrument);
		} else if(nSample != SAMPLEINDEX_INVALID && nSample <= sndFile.GetNumSamples())
		{
			m_WaveFile.ReadSampleFromSong(1, sndFile, nSample);
		}
		PreparePreview(note, volume);
		ok = true;
	}
	EndWaitCursor();
	if(!ok)
	{
		PausePlayback();
		UnsetPlaybackSoundFile();
		StopPlayback();
		return false;
	}
	if(IsPlaying() && (m_pSndFile == &m_WaveFile))
	{
		return true;
	}
	return PlaySoundFile(&m_WaveFile);
}


void CMainFrame::InitPreview()
{
	m_WaveFile.Destroy();
	m_WaveFile.Create(MOD_TYPE_MPT, 2);
	m_WaveFile.Order().SetDefaultTempoInt(125);
	m_WaveFile.Order().SetDefaultSpeed(6);
	m_WaveFile.m_nInstruments = 1;
	m_WaveFile.m_nTempoMode = TempoMode::Classic;
	m_WaveFile.Order().assign(1, 0);
	m_WaveFile.Patterns.Insert(0, 2);
	m_WaveFile.m_SongFlags = SONG_LINEARSLIDES;
}


void CMainFrame::PreparePreview(ModCommand::NOTE note, int volume)
{
	if(!ModCommand::IsNote(note))
	{
		for(auto &chn : m_WaveFile.m_PlayState.Chn)
		{
			chn.nFadeOutVol = 0;
			chn.dwFlags.set(CHN_NOTEFADE | CHN_FASTVOLRAMP);
		}
		return;
	}
	m_WaveFile.m_PlayState.m_flags.reset(SONG_PAUSED);
	m_WaveFile.SetRepeatCount(-1);
	m_WaveFile.ResetPlayPos();

	const CModDoc *activeDoc = GetActiveDoc();
	if(activeDoc != nullptr && (TrackerSettings::Instance().patternSetup & PatternSetup::NoLoudSamplePreview))
	{
		m_WaveFile.SetMixLevels(activeDoc->GetSoundFile().GetMixLevels());
		m_WaveFile.m_nSamplePreAmp = activeDoc->GetSoundFile().m_nSamplePreAmp;
	} else
	{
		// Preview at 0dB
		m_WaveFile.SetMixLevels(MixLevels::v1_17RC3);
		m_WaveFile.m_nSamplePreAmp = static_cast<uint32>(m_WaveFile.GetPlayConfig().getNormalSamplePreAmp());
	}

	// Avoid global volume ramping when trying samples in the treeview.
	m_WaveFile.m_nDefaultGlobalVolume = m_WaveFile.m_PlayState.m_nGlobalVolume = (volume > 0) ? volume : MAX_GLOBAL_VOLUME;

	if(m_WaveFile.Patterns.IsValidPat(0))
	{
		auto m = m_WaveFile.Patterns[0].GetRow(0);
		if(m_WaveFile.GetNumSamples() > 0)
		{
			m[0].note = note;
			m[0].instr = 1;
		}
		// Infinite loop on second row
		m = m_WaveFile.Patterns[0].GetRow(1);
		m[0].command = CMD_POSITIONJUMP;
		m[1].command = CMD_PATTERNBREAK;
		m[1].param = 1;
	}
	m_WaveFile.InitPlayer(true);
}


QWidget * CMainFrame::GetFollowSong() const
{
	return GetModPlaying() ? GetModPlaying()->GetFollowWnd() : NULL;
}


void CMainFrame::IdleHandlerSounddevice()
{
	MPT_TRACE_SCOPE();
	if(gpSoundDevice)
	{
		const FlagSet<SoundDevice::RequestFlags> requestFlags = gpSoundDevice->GetRequestFlags();
		if(requestFlags[SoundDevice::RequestFlagClose])
		{
			StopPlayback();
			audioCloseDevice();
		} else if(requestFlags[SoundDevice::RequestFlagReset])
		{
			ResetSoundCard();
		} else if(requestFlags[SoundDevice::RequestFlagRestart])
		{
			RestartPlayback();
		} else
		{
			gpSoundDevice->OnIdle();
		}
	}
}


void CMainFrame::ResetSoundCard()
{
	MPT_TRACE_SCOPE();
	CMainFrame::SetupSoundCard(TrackerSettings::Instance().GetSoundDeviceSettings(TrackerSettings::Instance().GetSoundDeviceIdentifier()), TrackerSettings::Instance().GetSoundDeviceIdentifier(), TrackerSettings::Instance().m_SoundSettingsStopMode, true);
}


void CMainFrame::SetupSoundCard(SoundDevice::Settings deviceSettings, SoundDevice::Identifier deviceIdentifier, SoundDeviceStopMode stoppedMode, bool forceReset)
{
	MPT_TRACE_SCOPE();
	if(forceReset
		|| (TrackerSettings::Instance().GetSoundDeviceIdentifier() != deviceIdentifier)
		|| (TrackerSettings::Instance().GetSoundDeviceSettings(deviceIdentifier) != deviceSettings)
		|| (TrackerSettings::Instance().m_SoundSettingsStopMode != stoppedMode)
		)
	{
		CModDoc *pActiveMod = nullptr;
		if(IsPlaying())
		{
			if ((m_pSndFile) && (!m_pSndFile->IsPaused())) pActiveMod = GetModPlaying();
			PauseMod();
		}
		if(gpSoundDevice)
		{
			gpSoundDevice->Close();
		}
		TrackerSettings::Instance().m_SoundSettingsStopMode = stoppedMode;
		switch(stoppedMode)
		{
			case SoundDeviceStopModeClosed:
				deviceSettings.KeepDeviceRunning = true;
				break;
			case SoundDeviceStopModeStopped:
				deviceSettings.KeepDeviceRunning = false;
				break;
			case SoundDeviceStopModePlaying:
				deviceSettings.KeepDeviceRunning = true;
				break;
		}
		TrackerSettings::Instance().SetSoundDeviceIdentifier(deviceIdentifier);
		TrackerSettings::Instance().SetSoundDeviceSettings(deviceIdentifier, deviceSettings);
		TrackerSettings::Instance().MixerOutputChannels = deviceSettings.Channels;
		TrackerSettings::Instance().MixerNumInputChannels = deviceSettings.InputChannels;
		TrackerSettings::Instance().MixerSamplerate = deviceSettings.Samplerate;
		if(pActiveMod)
		{
			PlayMod(pActiveMod);
		}
		UpdateWindow();
	} else
	{
		// No need to restart playback
		CriticalSection cs;
		if(GetSoundFilePlaying()) UpdateAudioParameters(*GetSoundFilePlaying(), false);
	}
}


void CMainFrame::SetupPlayer()
{
	CriticalSection cs;
	if(GetSoundFilePlaying()) UpdateAudioParameters(*GetSoundFilePlaying(), false);
}


void CMainFrame::SetupMiscOptions()
{
	const FlagSet<PatternSetup> patternSetup = TrackerSettings::Instance().patternSetup;
	if(patternSetup[PatternSetup::IgnoreMutedChannels])
		TrackerSettings::Instance().MixerFlags |= SNDMIX_MUTECHNMODE;
	else
		TrackerSettings::Instance().MixerFlags &= ~SNDMIX_MUTECHNMODE;
	{
		CriticalSection cs;
		if(GetSoundFilePlaying()) UpdateAudioParameters(*GetSoundFilePlaying());
	}

	m_wndToolBar.EnableFlatButtons(patternSetup[PatternSetup::FlatToolbarButtons]);

	UpdateTree(nullptr, UpdateHint().MPTOptions());
	theApp.UpdateAllViews(UpdateHint().MPTOptions());
}


void CMainFrame::SetupMidi(FlagSet<MidiSetup> d, uint32 n)
{
	bool deviceChanged = (TrackerSettings::Instance().m_nMidiDevice != n);
	TrackerSettings::Instance().midiSetup = d;
	TrackerSettings::Instance().SetMIDIDevice(n);
	if(deviceChanged && midiInData.inHandle)
	{
		// Device has changed, close the old one.
		midiCloseDevice();
		midiOpenDevice();
	}
}


void CMainFrame::SetUserText(const mpt::uchar *text)
{
	if(text[0] || !m_userText.empty())
	{
		m_userText = text;
		{ mfcless::CommandState cmdUI; OnUpdateUser(cmdUI); }
	}
}


void CMainFrame::SetInfoText(const mpt::uchar *text)
{
	if(text[0] || !m_infoText.empty())
	{
		m_infoText = text;
		{ mfcless::CommandState cmdUI; OnUpdateInfo(cmdUI); }
	}
}


void CMainFrame::SetXInfoText(const mpt::uchar *text)
{
	if(text[0] || !m_xInfoText.empty())
	{
		m_xInfoText = text;
		{ mfcless::CommandState cmdUI; OnUpdateInfo(cmdUI); }
	}
}


void CMainFrame::SetHelpText(const mpt::uchar *text)
{
	m_wndStatusBar.SetPaneText(0, text);
}


mpt::ustring CMainFrame::GetHelpText() const
{
	return mfcless::FromQString(m_wndStatusBar.GetPaneText(0));
}


void CMainFrame::OnDocumentCreated(CModDoc *pModDoc)
{
	m_wndTree.OnDocumentCreated(pModDoc);
	UpdateMRUList();
	UpdateDocumentCount();
}


void CMainFrame::OnDocumentClosed(CModDoc *pModDoc)
{
	if (pModDoc == GetModPlaying()) PauseMod();

	m_wndTree.OnDocumentClosed(pModDoc);
	// We don't do UpdateDocumentCount() here because the document still exists at this point in time - the document template informs us instead.
}


void CMainFrame::UpdateTree(CModDoc *pModDoc, UpdateHint hint, QObject *pHint)
{
	m_wndTree.OnUpdate(pModDoc, hint, pHint);
}


void CMainFrame::RefreshDlsBanks()
{
	m_wndTree.RefreshDlsBanks();
}


/////////////////////////////////////////////////////////////////////////////
// CMainFrame message handlers


class CPropertySheetMPT : public PropertySheet
{
public:
	using PropertySheet::PropertySheet;

protected:
	bool PreTranslateMessage(mfcless::Message *pMsg) override
	{
		if(pMsg && DialogBase::HandleGlobalKeyMessage(*pMsg))
			return true;

		return WindowBase::PreTranslateMessage(pMsg);
	}

	bool OnInitDialog() override
	{
		// mfcless: the context help button (WS_EX_CONTEXTHELP) is a Help button.
		SetHelpButton(true);
		return PropertySheet::OnInitDialog();
	}

	void OnHelp() override
	{
		CMainFrame::GetMainFrame()->OnHelp();
	}
};


void CMainFrame::OnViewOptions()
{
	if (m_bOptionsLocked)
		return;

	CPropertySheetMPT dlg(U_("OpenMPT Setup"), this, m_nLastOptionsPage);
	struct Pages
	{
		COptionsGeneral general;
		COptionsSoundcard sounddlg{TrackerSettings::Instance().m_SoundDeviceIdentifier};
		COptionsSampleEditor smpeditor;
		COptionsKeyboard keyboard;
		COptionsColors colors;
		COptionsMixer mixerdlg;
#if !defined(NO_REVERB) || !defined(NO_DSP) || !defined(NO_EQ) || !defined(NO_AGC)
		COptionsPlayer dspdlg;
#endif
		CMidiSetupDlg mididlg{TrackerSettings::Instance().midiSetup, TrackerSettings::Instance().GetCurrentMIDIDevice()};
		PathConfigDlg pathsdlg;
#if defined(MPT_ENABLE_UPDATE)
		CUpdateSetupDlg updatedlg;
#endif  // MPT_ENABLE_UPDATE
		COptionsAdvanced advanced;
		// mfcless: no Wine page; this build runs natively.
	};
	mpt::heap_value<Pages> pages;
	dlg.AddPage(&pages->general);
	dlg.AddPage(&pages->sounddlg);
	dlg.AddPage(&pages->mixerdlg);
#if !defined(NO_REVERB) || !defined(NO_DSP) || !defined(NO_EQ) || !defined(NO_AGC)
	dlg.AddPage(&pages->dspdlg);
#endif
	dlg.AddPage(&pages->smpeditor);
	dlg.AddPage(&pages->keyboard);
	dlg.AddPage(&pages->colors);
	dlg.AddPage(&pages->mididlg);
	dlg.AddPage(&pages->pathsdlg);
#if defined(MPT_ENABLE_UPDATE)
	dlg.AddPage(&pages->updatedlg);
#endif // MPT_ENABLE_UPDATE
	dlg.AddPage(&pages->advanced);
	m_bOptionsLocked = true;
	m_SoundCardOptionsDialog = &pages->sounddlg;
#if defined(MPT_ENABLE_UPDATE)
	m_UpdateOptionsDialog = &pages->updatedlg;
#endif // MPT_ENABLE_UPDATE

	dlg.DoModal();

	m_SoundCardOptionsDialog = nullptr;
#if defined(MPT_ENABLE_UPDATE)
	m_UpdateOptionsDialog = nullptr;
#endif // MPT_ENABLE_UPDATE
	m_bOptionsLocked = false;
	m_wndTree.OnOptionsChanged();
}


void CMainFrame::OnPluginManager()
{
#ifndef NO_PLUGINS
	PLUGINDEX nPlugslot = PLUGINDEX_INVALID;
	CModDoc* pModDoc = GetActiveDoc();

	if (pModDoc)
	{
		CSoundFile &sndFile = pModDoc->GetSoundFile();
		//Find empty plugin slot
		for (PLUGINDEX nPlug = 0; nPlug < MAX_MIXPLUGINS; nPlug++)
		{
			if (sndFile.m_MixPlugins[nPlug].pMixPlugin == nullptr)
			{
				nPlugslot = nPlug;
				break;
			}
		}
	}

	CSelectPluginDlg dlg(GetActiveDoc(), nPlugslot, this);
	if(dlg.DoModal() == IDOK && pModDoc)
	{
		pModDoc->SetModified();
		//Refresh views
		pModDoc->UpdateAllViews(nullptr, PluginHint().Info().Names().ModType());
		//Refresh Controls
		CChildFrame *pActiveChild = (CChildFrame *)MDIGetActive();
		pActiveChild->ForceRefresh();
	}
#endif // NO_PLUGINS
}


void CMainFrame::OnClipboardManager()
{
	PatternClipboardDialog::Show();
}


void CMainFrame::OnAddDlsBank()
{
	FileDialog dlg = OpenFileDialog()
		.AllowMultiSelect()
		.ExtensionFilter("All Sound Banks|*.dls;*.sbk;*.sf2;*.sf3;*.sf4;*.mss|"
			"Downloadable Sounds Banks (*.dls)|*.dls;*.mss|"
			"SoundFont 2.0 Banks (*.sf2)|*.sbk;*.sf2;*.sf3;*.sf4|"
			"All Files (*.*)|*.*||");
	if(!dlg.Show()) return;

	BeginWaitCursor();
	bool ok = true;
	for(const auto &file : dlg.GetFilenames())
	{
		ok &= CTrackApp::AddDLSBank(file);
	}
	if(!ok)
	{
		Reporting::Error("At least one selected file was not a valid sound bank.");
	}
	m_wndTree.RefreshDlsBanks();
	EndWaitCursor();
}


void CMainFrame::OnImportMidiLib()
{
	FileDialog dlg = OpenFileDialog()
		.ExtensionFilter("Text and INI files (*.txt,*.ini)|*.txt;*.ini;*.dls;*.sf2;*.sf3;*.sf4;*.sbk|"
			"Downloadable Sound Banks (*.dls)|*.dls;*.mss|"
			"SoundFont 2.0 banks (*.sf2)|*.sbk;*.sf2;*.sf3;*.sf4|"
			"Gravis UltraSound (ultrasnd.ini)|ultrasnd.ini|"
			"All Files (*.*)|*.*||");
	if(!dlg.Show()) return;

	BeginWaitCursor();
	CTrackApp::ImportMidiConfig(dlg.GetFirstFile());
	m_wndTree.RefreshMidiLibrary();
	EndWaitCursor();
}


void CMainFrame::OnTimer(uintptr_t timerID)
{
	switch(timerID)
	{
		case TIMERID_GUI:
			OnTimerGUI();
			break;
		case TIMERID_NOTIFY:
			OnTimerNotify();
			break;
	}
}


void CMainFrame::OnTimerGUI()
{
	IdleHandlerSounddevice();

	// Display Time in status bar
	if(m_pSndFile != nullptr && m_pSndFile->GetSampleRate() != 0)
	{
		samplecount_t time = Util::muldivr(m_pSndFile->GetTotalSampleCount(), 10, m_pSndFile->GetSampleRate());
		if(time != m_dwTimeSec)
		{
			m_dwTimeSec = time;
			m_nAvgMixChn = m_nMixChn;
			{ mfcless::CommandState cmdUI; OnUpdateTime(cmdUI); }
		}
	}

	if(m_AutoSaver->IsEnabled())
	{
		bool success = m_AutoSaver->DoSave();
		if(!success)  // autosave failure; bring up options.
		{
			CMainFrame::m_nLastOptionsPage = OPTIONS_PAGE_PATHS;
			OnViewOptions();
		}
	}

	if(m_SoundCardOptionsDialog)
	{
		m_SoundCardOptionsDialog->UpdateStatistics();
	}

#ifdef USE_PROFILER
	{
		Profiler::Update();

		QWidget * cwnd = m_mdiArea->viewport();
		mfcless::PaintDC dc(cwnd);

		int height = 16;
		int width = 256;

		std::vector<std::string> catnames = Profiler::GetCategoryNames();
		std::vector<double> cats = Profiler::DumpCategories();

		for(int i=0; i<Profiler::CategoriesCount; i++)
		{
			dc.FillSolidRect(0, i * height, (int)(width * cats[i]), height, mfcless::Rgb(255,0,0));
			dc.FillSolidRect((int)(width * cats[i]), i * height, width - (int)(width * cats[i]), height, mfcless::Rgb(192,192,192));
			mfcless::Rect rect;
			cwnd->GetClientRect(&rect);
			rect.left += width;
			rect.top += i * height;
			auto s = mpt::ToUnicode(mpt::Charset::ASCII, mpt::afmt::right(6, mpt::afmt::fix(cats[i] * 100.0, 3)) + "% " + catnames[i]);
			dc.DrawText(s, s.GetLength(), &rect, mfcless::dt::Left);
		}

		mfcless::Rect rect;
		cwnd->GetClientRect(&rect);
		rect.top += Profiler::CategoriesCount * height;
		auto s = mpt::ToUnicode(mpt::Charset::ASCII, Profiler::DumpProfiles());
		dc.DrawText(s, s.GetLength(), &rect, mfcless::dt::Left);

		cwnd->Detach();
	}
#endif

}


CModDoc *CMainFrame::GetActiveDoc() const
{
	CChildFrame *pMDIActive = MDIGetActive();
	if (pMDIActive)
	{
		return static_cast<CModDoc *>(pMDIActive->GetActiveDocument());
	}
	return nullptr;
}


mfcless::DocumentView *CMainFrame::GetActiveView() const
{
	CChildFrame *pMDIActive = MDIGetActive();
	if (pMDIActive)
	{
		return pMDIActive->GetActiveView();
	}

	return nullptr;
}


void CMainFrame::SwitchToActiveView()
{
	mfcless::DocumentView *view = GetActiveView();
	if (view)
	{
		QWidget *wnd = view->ViewWidget();
		// Hack: If the upper view is active, we only get the "container" (the dialog view with the tabs), not the view itself.
		if(auto *ctrlView = dynamic_cast<CModControlView *>(view))
		{
			wnd = ctrlView->GetCurrentControlDlg();
		}
		if(wnd)
			wnd->setFocus();
	}
}


void CMainFrame::OnUpdateTime(mfcless::CommandState &)
{
	auto timeSec = m_dwTimeSec / 10u;
	mpt::ustring s = mfcless::PrintF(UL_("%u:%02u:%02u.%u"),
		timeSec / 3600u, (timeSec / 60u) % 60u, timeSec % 60u, m_dwTimeSec % 10u);

	if(m_pSndFile != nullptr && m_pSndFile != &m_WaveFile && !m_pSndFile->IsPaused())
	{
		PATTERNINDEX nPat = m_pSndFile->m_PlayState.m_nPattern;
		if(m_pSndFile->Patterns.IsValidIndex(nPat))
		{
			if(nPat < 10) s += UL_(" ");
			if(nPat < 100) s += UL_(" ");
			s += mfcless::PrintF(UL_(" [%u]"), nPat);
		}
		s += mfcless::PrintF(UL_(" %uch"), m_nAvgMixChn);
	}
	m_wndStatusBar.SetPaneText(m_wndStatusBar.CommandToIndex(ID_INDICATOR_TIME), s, true);
}


void CMainFrame::OnUpdateUser(mfcless::CommandState &)
{
	m_wndStatusBar.SetPaneText(m_wndStatusBar.CommandToIndex(ID_INDICATOR_USER), m_userText, true);
}


void CMainFrame::OnUpdateInfo(mfcless::CommandState &)
{
	m_wndStatusBar.SetPaneText(m_wndStatusBar.CommandToIndex(ID_INDICATOR_INFO), m_infoText, true);
}


void CMainFrame::OnUpdateXInfo(mfcless::CommandState &)
{
	m_wndStatusBar.SetPaneText(m_wndStatusBar.CommandToIndex(ID_INDICATOR_XINFO), m_xInfoText, true);
}

void CMainFrame::OnPlayerPause()
{
	if (GetModPlaying())
	{
		GetModPlaying()->OnPlayerPause();
	} else
	{
		PauseMod();
	}
}


void CMainFrame::OpenMenuItemFile(const uint32 nId, const bool isTemplateFile)
{
	const uint32 nIdBegin = (isTemplateFile) ? ID_FILE_OPENTEMPLATE : ID_EXAMPLE_MODULES;
	const std::vector<mpt::PathString> &vecFilePaths = (isTemplateFile) ? m_TemplateModulePaths : m_ExampleModulePaths;

	const uint32 nIndex = nId - nIdBegin;
	if (nIndex < vecFilePaths.size())
	{
		const mpt::PathString& sPath = vecFilePaths[nIndex];
		const bool bExists = mpt::native_fs{}.is_file(sPath);
		CModDoc *pDoc = nullptr;
		if(bExists)
		{
			pDoc = theApp.GetModDocTemplate()->OpenTemplateFile(sPath, !isTemplateFile);
		}
		if(!pDoc)
		{
			Reporting::Notification(U_("The file '") + sPath.ToUnicode() + U_("' ") + (bExists ? U_("exists but can't be read.") : U_("does not exist.")));
		}
	} else
	{
		MPT_ASSERT(nId == uint32(isTemplateFile ? ID_FILE_OPENTEMPLATE_LASTINRANGE : ID_EXAMPLE_MODULES_LASTINRANGE));
		FileDialog::PathList files;
		theApp.OpenModulesDialog(files, isTemplateFile ? theApp.GetUserTemplatesPath() : theApp.GetExampleSongsPath());
		for(const auto &file : files)
		{
			theApp.OpenDocumentFile(file);
		}
	}
}


void CMainFrame::OnOpenTemplateModule(uint32 nId)
{
	OpenMenuItemFile(nId, true/*open template menu file*/);
}


void CMainFrame::OnExampleSong(uint32 nId)
{
	OpenMenuItemFile(nId, false/*open example menu file*/);
}


void CMainFrame::OnOpenMRUItem(uint32 nId)
{
	theApp.OpenDocumentFile(TrackerSettings::Instance().mruFiles[nId - ID_MRU_LIST_FIRST]);
}


void CMainFrame::OnUpdateMRUItem(mfcless::CommandState &state)
{
	state.Enable(!TrackerSettings::Instance().mruFiles.empty());
}


intptr_t CMainFrame::OnInvalidatePatterns(uintptr_t, intptr_t)
{
	theApp.UpdateAllViews(UpdateHint().MPTOptions());
	return true;
}


intptr_t CMainFrame::OnUpdatePosition(uintptr_t, intptr_t lParam)
{
	OPENMPT_PROFILE_FUNCTION(Profiler::GUI);
	m_VUMeterOutput.SetDecaySpeedDecibelPerSecond(TrackerSettings::Instance().VuMeterDecaySpeedDecibelPerSecond); // update in notification update in order to avoid querying the settings framework from inside audio thread
	m_VUMeterInput.SetDecaySpeedDecibelPerSecond(TrackerSettings::Instance().VuMeterDecaySpeedDecibelPerSecond); // update in notification update in order to avoid querying the settings framework from inside audio thread
	Notification *pnotify = (Notification *)lParam;
	if (pnotify)
	{
		if(pnotify->type[Notification::EOS])
		{
			mfcless::PostCommand(this, ID_PLAYER_STOP);
			m_currentSpeed = 0;
		}
		//Log("OnUpdatePosition: row=%d time=%lu\n", pnotify->nRow, pnotify->TimestampSamples);
		if(CModDoc *modDoc = GetModPlaying(); modDoc != nullptr)
		{
			m_wndTree.UpdatePlayPos(modDoc, pnotify);
			if (GetFollowSong())
				mfcless::SendModMessage(GetFollowSong(), WM_MOD_UPDATEPOSITION, 0, lParam);
			if(m_pSndFile->m_pluginDryWetRatioChanged.any())
			{
				for(PLUGINDEX i = 0; i < MAX_MIXPLUGINS; i++)
				{
					if(m_pSndFile->m_pluginDryWetRatioChanged[i])
						modDoc->PostMessageToAllViews(WM_MOD_PLUGINDRYWETRATIOCHANGED, i);
				}
				m_pSndFile->m_pluginDryWetRatioChanged.reset();
			}
			// Update envelope views if speed has changed
			if(m_pSndFile->m_PlayState.m_nMusicSpeed != m_currentSpeed)
			{
				m_currentSpeed = m_pSndFile->m_PlayState.m_nMusicSpeed;
				modDoc->UpdateAllViews(InstrumentHint().Envelope());
			}
		}
		m_nMixChn = pnotify->mixedChannels;

		bool duplicateMono = false;
		if(pnotify->masterVUinChannels == 1 && pnotify->masterVUoutChannels > 1)
		{
			duplicateMono = true;
		} else if(pnotify->masterVUoutChannels == 1 && pnotify->masterVUinChannels > 1)
		{
			duplicateMono = true;
		}
		uint8 countChan = 0;
		uint32 vu[VUMeter::maxChannels * 2];
		MemsetZero(vu);
		std::copy(pnotify->masterVUin.begin(), pnotify->masterVUin.begin() + pnotify->masterVUinChannels, vu + countChan);

		countChan += pnotify->masterVUinChannels;
		if(pnotify->masterVUinChannels == 1 && duplicateMono)
		{
			std::copy(pnotify->masterVUin.begin(), pnotify->masterVUin.begin() + 1, vu + countChan);
			countChan += 1;
		}

		std::copy(pnotify->masterVUout.begin(), pnotify->masterVUout.begin() + pnotify->masterVUoutChannels, vu + countChan);
		countChan += pnotify->masterVUoutChannels;
		if(pnotify->masterVUoutChannels == 1 && duplicateMono)
		{
			std::copy(pnotify->masterVUout.begin(), pnotify->masterVUout.begin() + 1, vu + countChan);
			countChan += 1;
		}

		m_wndToolBar.m_VuMeter->SetVuMeter(countChan, vu, pnotify->type[Notification::Stop]);

		m_wndToolBar.SetCurrentSong(m_pSndFile);
	}
	return 0;
}


intptr_t CMainFrame::OnUpdateViews(uintptr_t modDoc, intptr_t hint)
{
	CModDoc *doc = reinterpret_cast<CModDoc *>(modDoc);
	CModDocTemplate *pDocTmpl = theApp.GetModDocTemplate();
	if(pDocTmpl && pDocTmpl->DocumentExists(doc))
	{
		// Since this message is potentially posted, we first need to verify if the document still exists
		doc->UpdateAllViews(nullptr, UpdateHint::FromLPARAM(hint));
	}
	return 0;
}


intptr_t CMainFrame::OnSetModified(uintptr_t modDoc, intptr_t)
{
	CModDoc *doc = reinterpret_cast<CModDoc *>(modDoc);
	CModDocTemplate *pDocTmpl = theApp.GetModDocTemplate();
	if(pDocTmpl && pDocTmpl->DocumentExists(doc))
	{
		// Since this message is potentially posted, we first need to verify if the document still exists
		doc->UpdateFrameCounts();
	}
	return 0;
}


void CMainFrame::OnPanic()
{
	// "Panic button." At the moment, it just resets all VSTi and sample notes.
	if(GetModPlaying())
		GetModPlaying()->OnPanic();
}


void CMainFrame::OnPrevOctave()
{
	uint32 n = GetBaseOctave();
	if (n > MIN_BASEOCTAVE) m_wndToolBar.SetBaseOctave(n-1);
}


void CMainFrame::OnNextOctave()
{
	uint32 n = GetBaseOctave();
	if (n < MAX_BASEOCTAVE) m_wndToolBar.SetBaseOctave(n+1);
}


void CMainFrame::OnReportBug()
{
	CTrackApp::OpenURL(Build::GetURL(Build::Url::Bugtracker));
}


bool CMainFrame::OnInternetLink(uint32 nID)
{
	mpt::ustring url;
	switch(nID)
	{
	case ID_NETLINK_MODPLUG:	url = Build::GetURL(Build::Url::Website); break;
	case ID_NETLINK_TOP_PICKS:	url = Build::GetURL(Build::Url::TopPicks); break;
	}
	if(!url.empty())
	{
		return CTrackApp::OpenURL(url) ? true : false;
	}
	return false;
}


void CMainFrame::OnRButtonDown(uint32, mfcless::Point pt)
{
	ClientToScreen(&pt);
	ShowToolbarMenu(pt);
}


void CMainFrame::ShowToolbarMenu(mfcless::Point screenPt)
{
	mfcless::Menu menu;
	if(!menu.CreatePopupMenu())
		return;
	AddToolBarMenuEntries(menu);
	menu.TrackPopupMenu(mfcless::menu::LeftAlign | mfcless::menu::RightButton, screenPt.x, screenPt.y, this);
}


void CMainFrame::AddToolBarMenuEntries(mfcless::Menu &menu) const
{
	menu.AppendMenu(mfcless::menu::String, ID_VIEW_TOOLBAR, m_InputHandler->GetMenuText(ID_VIEW_TOOLBAR));
	menu.AppendMenu(mfcless::menu::String, IDD_TREEVIEW, m_InputHandler->GetMenuText(IDD_TREEVIEW));
	menu.AppendMenu(mfcless::menu::String | (TrackerSettings::Instance().treeViewOnLeft ? mfcless::menu::Checked : 0), ID_TREEVIEW_ON_LEFT, U_("Tree View on &Left"));

	const FlagSet<MainToolBarItem> visibleItems = TrackerSettings::Instance().mainToolBarVisibleItems.Get();

	mfcless::Menu subMenu;
	subMenu.CreatePopupMenu();

	subMenu.AppendMenu(mfcless::menu::String | (visibleItems[MainToolBarItem::IconsFile] ? mfcless::menu::Checked : 0), ID_MAINBAR_SHOW_FILE_ICONS, U_("&File Icons"));
	subMenu.AppendMenu(mfcless::menu::String | (visibleItems[MainToolBarItem::IconsEdit] ? mfcless::menu::Checked : 0), ID_MAINBAR_SHOW_EDIT_ICONS, U_("&Edit Icons"));
	subMenu.AppendMenu(mfcless::menu::String | (visibleItems[MainToolBarItem::IconsPlayback] ? mfcless::menu::Checked : 0), ID_MAINBAR_SHOW_PLAY_ICONS, U_("&Play / Record Icons"));
	subMenu.AppendMenu(mfcless::menu::String | (visibleItems[MainToolBarItem::Octave] ? mfcless::menu::Checked : 0), ID_MAINBAR_SHOW_OCTAVE, U_("Base &Octave"));
	subMenu.AppendMenu(mfcless::menu::String | (visibleItems[MainToolBarItem::Tempo] ? mfcless::menu::Checked : 0), ID_MAINBAR_SHOW_TEMPO, U_("&Tempo"));
	subMenu.AppendMenu(mfcless::menu::String | (visibleItems[MainToolBarItem::Speed] ? mfcless::menu::Checked : 0), ID_MAINBAR_SHOW_SPEED, U_("Ticks/&Row"));
	subMenu.AppendMenu(mfcless::menu::String | (visibleItems[MainToolBarItem::RowsPerBeat] ? mfcless::menu::Checked : 0), ID_MAINBAR_SHOW_ROWSPERBEAT, U_("Rows Per &Beat"));
	subMenu.AppendMenu(mfcless::menu::String | (visibleItems[MainToolBarItem::GlobalVolume] ? mfcless::menu::Checked : 0), ID_MAINBAR_SHOW_GLOBALVOLUME, U_("&Global Volume"));
	subMenu.AppendMenu(mfcless::menu::String | (visibleItems[MainToolBarItem::IconsMisc] ? mfcless::menu::Checked : 0), ID_MAINBAR_SHOW_MISC_ICONS, U_("&Misc Icons"));
	subMenu.AppendMenu(mfcless::menu::String | (visibleItems[MainToolBarItem::VUMeter] ? mfcless::menu::Checked : 0), ID_MAINBAR_SHOW_VUMETER, U_("&VU Meters"));
	menu.AppendMenu(mfcless::menu::Popup, subMenu, U_("Main Toolbar &Items"));
}


void CMainFrame::OnToggleMainBarShowOctave() { OnToggleMainBarItem(MainToolBarItem::Octave, ID_MAINBAR_SHOW_OCTAVE); }
void CMainFrame::OnToggleMainBarShowTempo() { OnToggleMainBarItem(MainToolBarItem::Tempo, ID_MAINBAR_SHOW_TEMPO); }
void CMainFrame::OnToggleMainBarShowSpeed() { OnToggleMainBarItem(MainToolBarItem::Speed, ID_MAINBAR_SHOW_SPEED); }
void CMainFrame::OnToggleMainBarShowRowsPerBeat() { OnToggleMainBarItem(MainToolBarItem::RowsPerBeat, ID_MAINBAR_SHOW_ROWSPERBEAT); }
void CMainFrame::OnToggleMainBarShowGlobalVolume() { OnToggleMainBarItem(MainToolBarItem::GlobalVolume, ID_MAINBAR_SHOW_GLOBALVOLUME); }
void CMainFrame::OnToggleMainBarShowVUMeter() { OnToggleMainBarItem(MainToolBarItem::VUMeter, ID_MAINBAR_SHOW_VUMETER); }
void CMainFrame::OnToggleMainBarShowFileIcons() { OnToggleMainBarItem(MainToolBarItem::IconsFile, ID_MAINBAR_SHOW_FILE_ICONS); }
void CMainFrame::OnToggleMainBarShowEditIcons() { OnToggleMainBarItem(MainToolBarItem::IconsEdit, ID_MAINBAR_SHOW_EDIT_ICONS); }
void CMainFrame::OnToggleMainBarShowPlayIcons() { OnToggleMainBarItem(MainToolBarItem::IconsPlayback, ID_MAINBAR_SHOW_PLAY_ICONS); }
void CMainFrame::OnToggleMainBarShowMiscIcons() { OnToggleMainBarItem(MainToolBarItem::IconsMisc, ID_MAINBAR_SHOW_MISC_ICONS); }

void CMainFrame::OnToggleMainBarItem(MainToolBarItem item, uint32 menuID)
{
	const bool visible = m_wndToolBar.ToggleVisibility(item);
	GetMenu()->CheckMenuItem(menuID, mfcless::menu::ByCommand | (visible ? mfcless::menu::Checked : 0));
}


void CMainFrame::OnToggleTreeViewOnLeft()
{
	const bool left = !TrackerSettings::Instance().treeViewOnLeft;
	TrackerSettings::Instance().treeViewOnLeft = left;
	m_wndTree.SetBarOnLeft(left);
	RecalcLayout();
	GetMenu()->CheckMenuItem(ID_TREEVIEW_ON_LEFT, mfcless::menu::ByCommand | (left ? mfcless::menu::Checked : 0));
}


intptr_t CMainFrame::OnCustomKeyMsg(uintptr_t wParam, intptr_t lParam)
{
	CommandID cmd = static_cast<CommandID>(wParam);
	switch(cmd)
	{
		case kcViewTree: OnBarCheck(IDD_TREEVIEW); break;
		case kcViewOptions: OnViewOptions(); break;
		case kcViewMain: OnBarCheck(ID_VIEW_TOOLBAR); break;
		case kcFileImportMidiLib: OnImportMidiLib(); break;
		case kcFileAddSoundBank: OnAddDlsBank(); break;
		case kcPauseSong: OnPlayerPause(); break;
		case kcPrevOctave: OnPrevOctave(); break;
		case kcNextOctave: OnNextOctave(); break;
		case kcFileNew: theApp.OnFileNew(); break;
		case kcFileOpen: theApp.OnFileOpen(); break;
		case kcMidiRecord: OnMidiRecord(); break;
		case kcHelp: OnHelp(); break;
		case kcViewAddPlugin: OnPluginManager(); break;
		case kcNextDocument: MDINext(); break;
		case kcPrevDocument: MDIPrev(); break;
		case kcFileCloseAll:
			if(QApplication::activeWindow() != this)
				return kcNull;
			theApp.OnFileCloseAll();
			break;

		//D'oh!! moddoc isn't a QWidget so we have to handle its messages and pass them on.

		case kcFileAppend:
		case kcFileSaveAs:
		case kcFileSaveCopy:
		case kcFileSaveAsWave:
		case kcFileSaveMidi:
		case kcFileSaveOPL:
		case kcFileExportCompat:
		case kcFileClose:
		case kcFileSave:
		case kcViewGeneral:
		case kcViewPattern:
		case kcViewSamples:
		case kcViewInstruments:
		case kcViewComments:
		case kcViewGraph: //rewbs.graph
		case kcViewSongProperties:
		case kcViewTempoSwing:
		case kcViewMIDImapping:
		case kcViewEditHistory:
		case kcViewChannelManager:
		case kcPlayPatternFromCursor:
		case kcPlayPatternFromStart:
		case kcPlaySongFromCursor:
		case kcPlaySongFromStart:
		case kcPlayPauseSong:
		case kcPlayStopSong:
		case kcPlaySongFromPattern:
		case kcPlaySongFromCursorPause:
		case kcPlaySongFromPatternPause:
		case kcStopSong:
		case kcToggleLoopSong:
		case kcPanic:
		case kcEstimateSongLength:
		case kcApproxRealBPM:
		case kcTempoIncrease:
		case kcTempoDecrease:
		case kcTempoIncreaseFine:
		case kcTempoDecreaseFine:
		case kcSpeedIncrease:
		case kcSpeedDecrease:
		case kcViewToggle:
			if(CModDoc *modDoc = GetActiveDoc())
				return modDoc->OnCustomKeyMsg(wParam, lParam);
			else if(cmd == kcPlayPauseSong || cmd == kcPlayStopSong || cmd == kcStopSong)
				StopPreview();
			else
				return kcNull;
			break;

		case kcSwitchToInstrLibrary:
			if(!m_wndTree.isVisible())
				return kcNull;
			if(m_bModTreeHasFocus)
				SwitchToActiveView();
			else
				m_wndTree.setFocus();
			break;

		default:
			// If handled neither by MainFrame nor by ModDoc, send it to the active view
			// Note: MDIGetActive() will return a valid view even if we are currently in a modal dialog!
			CChildFrame *pMDIActive = MDIGetActive();
			QWidget *wnd = nullptr;
			bool isPatternView = false;
			if(pMDIActive && pMDIActive->GetActiveView())
			{
				mfcless::DocumentView *view = pMDIActive->GetActiveView();
				wnd = view->ViewWidget();
				isPatternView = dynamic_cast<CViewPattern *>(view) != nullptr;
				// Hack: If the upper view is active, we only get the "container" (the dialog view with the tabs), not the view itself.
				if(auto *ctrlView = dynamic_cast<CModControlView *>(view))
				{
					wnd = ctrlView->GetCurrentControlDlg();
				}
			}

			// Backup solution for order navigation if the currently active view is not a pattern view, but a module is playing
			// Note: This should also work if the currently active window is not a CMDIChildWnd, hence it happens before the GetActiveWindow() check.
			if(mpt::is_in_range(cmd, kcPrevNextOrderStart, kcPrevNextOrderEnd)
				&& m_pSndFile && m_pSndFile->GetpModDoc()
				&& wnd != nullptr
				&& !isPatternView)
			{
				CriticalSection cs;

				ORDERINDEX order = m_pSndFile->m_PlayState.m_nCurrentOrder;
				if(cmd == kcPrevOrder || cmd == kcPrevOrderAtMeasureEnd || cmd == kcPrevOrderAtBeatEnd || cmd == kcPrevOrderAtRowEnd)
					order = m_pSndFile->Order().GetPreviousOrderIgnoringSkips(order);
				else
					order = m_pSndFile->Order().GetNextOrderIgnoringSkips(order);
				
				if(order == m_pSndFile->m_PlayState.m_nCurrentOrder)
					return wParam;

				ResetNotificationBuffer();
				switch(wParam)
				{
				case kcPrevOrder:
				case kcNextOrder:
					m_pSndFile->GetpModDoc()->SetElapsedTime(order, 0, !m_pSndFile->m_PlayState.m_flags[SONG_PAUSED | SONG_STEP]);
					break;
				case kcPrevOrderAtMeasureEnd:
				case kcNextOrderAtMeasureEnd:
					m_pSndFile->m_PlayState.m_seqOverrideMode = OrderTransitionMode::AtMeasureEnd;
					m_pSndFile->m_PlayState.m_nSeqOverride = order;
					break;
				case kcPrevOrderAtBeatEnd:
				case kcNextOrderAtBeatEnd:
					m_pSndFile->m_PlayState.m_seqOverrideMode = OrderTransitionMode::AtBeatEnd;
					m_pSndFile->m_PlayState.m_nSeqOverride = order;
					break;
				case kcPrevOrderAtRowEnd:
				case kcNextOrderAtRowEnd:
					m_pSndFile->m_PlayState.m_seqOverrideMode = OrderTransitionMode::AtRowEnd;
					m_pSndFile->m_PlayState.m_nSeqOverride = order;
					break;
				}
				return wParam;
			}

			if(wnd && QApplication::activeWindow() == this)
				return mfcless::SendModMessage(wnd, WM_MOD_KEYCOMMAND, wParam, lParam);
			return kcNull;
	}

	return wParam;
}


void CMainFrame::InitRenderer(CSoundFile *pSndFile)
{
	CriticalSection cs;
	pSndFile->m_bIsRendering = true;
	pSndFile->SuspendPlugins();
	pSndFile->ResumePlugins();
}


void CMainFrame::StopRenderer(CSoundFile *pSndFile)
{
	CriticalSection cs;
	pSndFile->SuspendPlugins();
	pSndFile->m_bIsRendering = false;
}


CInputHandler *CMainFrame::GetInputHandler()
{
	if(CMainFrame *mainFrm = GetMainFrame())
		return mainFrm->m_InputHandler.get();
	return nullptr;
}


// We have switched focus to a new module - might need to update effect keys to reflect module type
bool CMainFrame::UpdateEffectKeys(const CModDoc *modDoc)
{
	if(modDoc != nullptr)
	{
		return m_InputHandler->SetEffectLetters(modDoc->GetSoundFile().GetModSpecifications());
	}
	return false;
}


void CMainFrame::OnShowWindow(bool bShow, uint32 /*nStatus*/)
{
	static bool firstShow = true;
	if(bShow && !IsWindowVisible() && firstShow)
	{
		firstShow = false;
		RestoreWindowGeometry(theApp.GetSettings());
	}
}


void CMainFrame::SaveWindowGeometry(SettingsContainer &conf)
{
	conf.Write<std::string>(U_("Display"), U_("WindowGeometry"), saveGeometry().toBase64().toStdString());
}


void CMainFrame::RestoreWindowGeometry(SettingsContainer &conf)
{
	const std::string geometry = conf.Read<std::string>(U_("Display"), U_("WindowGeometry"), std::string());
	if(!geometry.empty())
		restoreGeometry(QByteArray::fromBase64(QByteArray::fromStdString(geometry)));
}


void CMainFrame::OnInternetUpdate()
{
#if defined(MPT_ENABLE_UPDATE)
	CUpdateCheck::DoManualUpdateCheck();
#endif // MPT_ENABLE_UPDATE
}


void CMainFrame::OnUpdateAvailable()
{
#if defined(MPT_ENABLE_UPDATE)
	if(m_updateCheckResult)
		CUpdateCheck::ShowSuccessGUI(false, *m_updateCheckResult);
	else
		CUpdateCheck::DoManualUpdateCheck();
#endif  // MPT_ENABLE_UPDATE
}


void CMainFrame::OnShowSettingsFolder()
{
	theApp.OpenDirectory(theApp.GetConfigPath());
}



class CUpdateCheckProgressDialog
	: public CProgressDialog
{
public:
	CUpdateCheckProgressDialog(QWidget *parent)
		: CProgressDialog(parent)
	{
		return;
	}
	void Run() override
	{
	}
};

static std::unique_ptr<CUpdateCheckProgressDialog> g_UpdateCheckProgressDialog = nullptr;


#if defined(MPT_ENABLE_UPDATE)


bool CMainFrame::ShowUpdateIndicator(const UpdateCheckResult &result, const mpt::ustring &releaseVersion, const mpt::ustring &infoURL, bool showHighlight)
{
	m_updateCheckResult = std::make_unique<UpdateCheckResult>(result);
	if(m_wndToolBar.IsVisible() && (TrackerSettings::Instance().mainToolBarVisibleItems.Get() & MainToolBarItem::IconsMisc))
	{
		return m_wndToolBar.ShowUpdateInfo(releaseVersion, infoURL, showHighlight);
	} else
	{
		GetMenu()->RemoveMenu(ID_UPDATE_AVAILABLE, mfcless::menu::ByCommand);
		GetMenu()->AppendMenu(mfcless::menu::String, ID_UPDATE_AVAILABLE, U_("[Update Available]"));
		DrawMenuBar();
		return true;
	}
}


intptr_t CMainFrame::OnUpdateCheckStart(uintptr_t wparam, intptr_t lparam)
{
	GetMenu()->RemoveMenu(ID_UPDATE_AVAILABLE, mfcless::menu::ByCommand);
	m_wndToolBar.RemoveUpdateInfo();

	const bool isAutoUpdate = CUpdateCheck::IsAutoUpdateFromMessage(wparam, lparam);
	mpt::ustring updateText = U_("Checking for updates...");
	if(isAutoUpdate)
	{
		SetHelpText(updateText);
	} else if(m_UpdateOptionsDialog)
	{
		m_UpdateOptionsDialog->SetItemText(IDC_LASTUPDATE, updateText);
	} else
	{
		if(!g_UpdateCheckProgressDialog)
		{
			g_UpdateCheckProgressDialog = std::make_unique<CUpdateCheckProgressDialog>(CMainFrame::GetMainFrame());
			g_UpdateCheckProgressDialog->Create(IDD_PROGRESS, CMainFrame::GetMainFrame());
			g_UpdateCheckProgressDialog->SetTitle(U_("Checking for updates..."));
			g_UpdateCheckProgressDialog->SetText(U_("Checking for updates..."));
			g_UpdateCheckProgressDialog->SetAbortText(U_("&Cancel"));
			g_UpdateCheckProgressDialog->SetRange(0, 100);
			g_UpdateCheckProgressDialog->ShowWindow(SW_SHOWDEFAULT);
		}
		if(g_UpdateCheckProgressDialog)
		{
			g_UpdateCheckProgressDialog->SetProgress(0);
		}
	}
	return true;
}


intptr_t CMainFrame::OnUpdateCheckProgress(uintptr_t wparam, intptr_t lparam)
{
	if(m_cancelUpdateCheck)
	{
		return false;
	}
	bool isAutoUpdate = wparam != 0;
	mpt::ustring updateText = MPT_UFORMAT("Checking for updates... {}%")(lparam);
	if(isAutoUpdate)
	{
		SetHelpText(updateText);
	} else if(m_UpdateOptionsDialog)
	{
		m_UpdateOptionsDialog->SetItemText(IDC_LASTUPDATE, updateText);
	} else
	{
		if(g_UpdateCheckProgressDialog)
		{
			g_UpdateCheckProgressDialog->SetProgress(lparam);
			if(g_UpdateCheckProgressDialog->m_abort)
			{
				return false;
			}
		}
	}
	return true;
}


intptr_t CMainFrame::OnUpdateCheckCanceled(uintptr_t wparam, intptr_t lparam)
{
	m_updateCheckResult.reset();
	bool isAutoUpdate = CUpdateCheck::IsAutoUpdateFromMessage(wparam, lparam);
	mpt::ustring updateText = U_("Checking for updates... Canceled.");
	if(isAutoUpdate)
	{
		SetHelpText(updateText);
	} else if(m_UpdateOptionsDialog)
	{
		m_UpdateOptionsDialog->SetItemText(IDC_LASTUPDATE, updateText);
		m_UpdateOptionsDialog->SettingChanged(TrackerSettings::Instance().UpdateLastUpdateCheck.GetPath());
	} else
	{
		if(g_UpdateCheckProgressDialog)
		{
			g_UpdateCheckProgressDialog->DestroyWindow();
			g_UpdateCheckProgressDialog = nullptr;
		}
	}
	if(isAutoUpdate)
	{
		SetHelpText(U_(""));
	}
	return true;
}


intptr_t CMainFrame::OnUpdateCheckFailure(uintptr_t wparam, intptr_t lparam)
{
	m_updateCheckResult.reset();
	const bool isAutoUpdate = CUpdateCheck::IsAutoUpdateFromMessage(wparam, lparam);
	const CUpdateCheck::Error &error = CUpdateCheck::MessageAsError(wparam, lparam);
	mpt::ustring updateText = MPT_UFORMAT("Checking for updates failed: {}")(mpt::get_exception_text<mpt::ustring>(error));
	if(isAutoUpdate)
	{
		SetHelpText(updateText);
	} else if(m_UpdateOptionsDialog)
	{
		m_UpdateOptionsDialog->SetItemText(IDC_LASTUPDATE, updateText);
		m_UpdateOptionsDialog->SettingChanged(TrackerSettings::Instance().UpdateLastUpdateCheck.GetPath());
	} else
	{
		if(g_UpdateCheckProgressDialog)
		{
			g_UpdateCheckProgressDialog->DestroyWindow();
			g_UpdateCheckProgressDialog = nullptr;
		}
	}
	CUpdateCheck::ShowFailureGUI(isAutoUpdate, error);
	if(isAutoUpdate)
	{
		SetHelpText(U_(""));
	}
	return true;
}


intptr_t CMainFrame::OnUpdateCheckSuccess(uintptr_t wparam, intptr_t lparam)
{
	m_updateCheckResult.reset();
	const bool isAutoUpdate = CUpdateCheck::IsAutoUpdateFromMessage(wparam, lparam);
	const UpdateCheckResult &result = CUpdateCheck::MessageAsResult(wparam, lparam);
	CUpdateCheck::AcknowledgeSuccess(result);
	if(result.CheckTime != mpt::Date::Unix{})
	{
		TrackerSettings::Instance().UpdateLastUpdateCheck = result.CheckTime;
	}
	if(!isAutoUpdate)
	{
		if(m_UpdateOptionsDialog)
		{
			m_UpdateOptionsDialog->SetItemText(IDC_LASTUPDATE, U_("Checking for updates... Done."));
		} else
		{
			if(g_UpdateCheckProgressDialog)
			{
				g_UpdateCheckProgressDialog->DestroyWindow();
				g_UpdateCheckProgressDialog = nullptr;
			}
			SetHelpText(U_(""));
		}
	}
	CUpdateCheck::ShowSuccessGUI(isAutoUpdate, result);
	if(isAutoUpdate)
	{
		SetHelpText(U_(""));
	}
	return true;
}


intptr_t CMainFrame::OnToolbarUpdateIndicatorClick(uintptr_t action, intptr_t)
{
	const auto popAction = static_cast<UpdateToolTip::PopAction>(action);
	if(popAction != UpdateToolTip::PopAction::ClickBubble)
		return 0;

	if(m_updateCheckResult)
		CUpdateCheck::ShowSuccessGUI(false, *m_updateCheckResult);
	else
		CUpdateCheck::DoManualUpdateCheck();
	return 0;
}


#endif // MPT_ENABLE_UPDATE


void CMainFrame::OnHelp()
{
	mfcless::DocumentView *view = GetActiveView();
	const char *page = "";
	if(m_bOptionsLocked)
	{
		switch(m_nLastOptionsPage)
		{
			case OPTIONS_PAGE_GENERAL:		page = "::/Setup_General.html"; break;
			case OPTIONS_PAGE_SOUNDCARD:	page = "::/Setup_Soundcard.html"; break;
			case OPTIONS_PAGE_MIXER:		page = "::/Setup_Mixer.html"; break;
			case OPTIONS_PAGE_PLAYER:		page = "::/Setup_DSP.html"; break;
			case OPTIONS_PAGE_SAMPLEDITOR:	page = "::/Setup_Samples.html"; break;
			case OPTIONS_PAGE_KEYBOARD:		page = "::/Setup_Keyboard.html"; break;
			case OPTIONS_PAGE_COLORS:		page = "::/Setup_Display.html"; break;
			case OPTIONS_PAGE_MIDI:			page = "::/Setup_MIDI.html"; break;
			case OPTIONS_PAGE_PATHS:		page = "::/Setup_Paths_Auto_Save.html"; break;
			case OPTIONS_PAGE_UPDATE:		page = "::/Setup_Update.html"; break;
			case OPTIONS_PAGE_ADVANCED:		page = "::/Setup_Advanced.html"; break;
			case OPTIONS_PAGE_WINE:			page = "::/Setup_Wine.html"; break;
		}
	} else if(view != nullptr)
	{
		const char *className = view->GetRuntimeClass() ? view->GetRuntimeClass()->m_lpszClassName : "";
		if(!strcmp("CViewGlobals", className))
			page = "::/General.html";
		else if(!strcmp("CViewPattern", className))
			page = "::/Patterns.html";
		else if(!strcmp("CViewSample", className))
			page = "::/Samples.html";
		else if(!strcmp("CViewInstrument", className))
			page = "::/Instruments.html";
		else if(!strcmp("CViewComments", className))
			page = "::/Comments.html";
		else if(!strcmp(CModControlView::classCModControlView.m_lpszClassName, className))
		{
			switch(static_cast<CModControlView*>(view)->GetActivePage())
			{
				case CModControlView::Page::Globals: page = "::/General.html"; break;
				case CModControlView::Page::Patterns: page = "::/Patterns.html"; break;
				case CModControlView::Page::Samples: page = "::/Samples.html"; break;
				case CModControlView::Page::Instruments: page = "::/Instruments.html"; break;
				case CModControlView::Page::Comments: page = "::/Comments.html"; break;
				case CModControlView::Page::Unknown: /* nothing */ break;
				case CModControlView::Page::NumPages: /* nothing */ break;
			}
		}
	}

	// mfcless: the manual is the online one (the .chm file is made from it);
	// its pages have the same names, with sub pages in folders.
	QString wikiPage = QString::fromLatin1(page);
	wikiPage.remove(QStringLiteral("::/"));
	wikiPage.remove(QStringLiteral(".html"));
	if(wikiPage.startsWith(QStringLiteral("Setup_")))
	{
		wikiPage = QStringLiteral("Setup/") + wikiPage.mid(6);
		wikiPage.replace(QStringLiteral("Paths_"), QStringLiteral("Paths/"));
	}
	const QString url = QStringLiteral("https://wiki.openmpt.org/Manual") + (wikiPage.isEmpty() ? QString() : QStringLiteral(":_") + wikiPage);
	if(!QDesktopServices::openUrl(QUrl(url)))
	{
		Reporting::Error(U_("Could not find help file:\n") + mfcless::FromQString(url));
		return;
	}
	//::ShowWindow(hwndHelp, mfcless::show::ShowMaximized);
}


intptr_t CMainFrame::OnViewMIDIMapping(uintptr_t wParam, intptr_t lParam)
{
	static bool inMapper = false;
	if(!inMapper)
	{
		inMapper = true;
		CModDoc *doc = GetActiveDoc();
		if(doc != nullptr)
			doc->ViewMIDIMapping(static_cast<PLUGINDEX>(wParam), static_cast<PlugParamIndex>(lParam));
		inMapper = false;
	}
	return 0;
}


void CMainFrame::LoadMetronomeSamples()
{
	const std::tuple<mpt::PathString, ModSample &, const int, const int> metronomeSamples[] =
	{
		{TrackerSettings::Instance().metronomeSampleMeasure, m_metronomeMeasure, 4, 256},
		{TrackerSettings::Instance().metronomeSampleBeat, m_metronomeBeat, 2, 192},
	};
	CriticalSection cs;
	for(auto &[path, sample, speed, amp] : metronomeSamples)
	{
		sample.FreeSample();
		if(path.empty())
			continue;
		if(path != TrackerSettings::GetDefaultMetronomeSample())
		{
			mpt::IO::InputFile inputFile(path, TrackerSettings::Instance().MiscCacheCompleteFileBeforeLoading);
			if(inputFile.IsValid())
			{
				FileReader file = GetFileReader(inputFile);
				SAMPLEINDEX srcSmp = m_WaveFile.GetNextFreeSample();
				if(srcSmp == SAMPLEINDEX_INVALID)
					srcSmp = 1;
				if(m_WaveFile.ReadSampleFromFile(srcSmp, file))
				{
					std::swap(sample, m_WaveFile.GetSample(srcSmp));
					sample.Convert(m_WaveFile.GetType(), MOD_TYPE_MPT);
				}
			}
		}
		if(!sample.HasSampleData())
		{
			sample.Initialize(MOD_TYPE_MPT);
			sample.nC5Speed = 8363 * 16;
			sample.nLength = 4096;
			sample.uFlags.set(CHN_16BIT);
			if(sample.AllocateSample())
			{
				int16_t *sampleData = sample.sample16();
				for(SmpLength i = 0; i < sample.nLength; i++)
				{
					sampleData[i] = static_cast<int16>(Util::muldiv(ITSinusTable[(i * speed) % std::size(ITSinusTable)] * amp, 4096 * 4096 - i * i, 4096 * 4096));
				}
			}
		}
	}
	UpdateMetronomeVolume();
	UpdateMetronomeSamples();
}


void CMainFrame::UpdateMetronomeSamples()
{
	if(!m_pSndFile)
		return;
	ModSample *measure = nullptr, *beat = nullptr;
	if(TrackerSettings::Instance().metronomeEnabled)
	{
		measure = &m_metronomeMeasure;
		beat = &m_metronomeBeat;
	}
	CriticalSection cs;
	m_pSndFile->SetMetronomeSamples(measure, beat);
}


void CMainFrame::UpdateMetronomeVolume()
{
	const float linear = std::pow(10.0f, TrackerSettings::Instance().metronomeVolume / 20.0f);
	const uint16 volume = std::clamp(mpt::saturate_round<uint16>(linear * 256.0f), uint16(0), uint16(256));
	CriticalSection cs;
	m_metronomeBeat.nVolume = m_metronomeMeasure.nVolume = volume;
}


mfcless::Menu CMainFrame::CreateFileMenu(const size_t maxCount, std::vector<mpt::PathString>& paths, const mpt::PathString &folderName, const uint16 idRangeBegin)
{
	paths.clear();

	for(size_t i = 0; i < 2; i++)  // 0: app items, 1: user items
	{
		// To avoid duplicates, check whether app path and config path are the same.
		if(i == 1 && mpt::PathCompareNoCase(theApp.GetInstallPath(), theApp.GetConfigPath()) == 0)
			break;

		mpt::PathString basePath;
		basePath = (i == 0) ? theApp.GetInstallPath() : theApp.GetConfigPath();
		basePath += folderName;
		if(!mpt::native_fs{}.is_directory(basePath))
			continue;

		FolderScanner scanner(basePath, FolderScanner::kOnlyFiles);
		mpt::PathString fileName;
		while(scanner.Next(fileName))
		{
			paths.push_back(std::move(fileName));
		}
	}
	// mfcless: natural sorting, ignoring case (CompareString with SORT_DIGITSASNUMBERS).
	QCollator collator;
	collator.setCaseSensitivity(Qt::CaseInsensitive);
	collator.setNumericMode(true);
	std::sort(paths.begin(), paths.end(), [&collator](const mpt::PathString &left, const mpt::PathString &right)
	{
		return collator.compare(mfcless::ToQString(left.ToUnicode()), mfcless::ToQString(right.ToUnicode())) < 0;
	});

	mfcless::Menu hMenu = mfcless::Menu::CreatePopup();
	MPT_ASSERT(hMenu != nullptr);
	if(hMenu == nullptr)
		return hMenu;

	uintptr_t filesAdded = 0;
	for(const auto &fileName : paths)
	{
		const mpt::ustring file = mfcless::FromQString(mfcless::ToQString(fileName.GetFilename().ToUnicode()).replace(QLatin1Char('&'), QStringLiteral("&&")));
		hMenu.AppendMenu(mfcless::menu::String, idRangeBegin + filesAdded, file);
		filesAdded++;
		if(filesAdded >= maxCount)
			break;
	}

	if(filesAdded == 0)
	{
		hMenu.AppendMenu(mfcless::menu::String | mfcless::menu::Grayed | mfcless::menu::Disabled, 0, U_("No items found"));
	} else
	{
		hMenu.AppendMenu(mfcless::menu::Separator);
		hMenu.AppendMenu(mfcless::menu::String, idRangeBegin + maxCount, U_("&Browse..."));
	}

	return hMenu;
}


void CMainFrame::CreateExampleModulesMenu()
{
	static_assert(nMaxItemsInExampleModulesMenu == ID_EXAMPLE_MODULES_LASTINRANGE - ID_EXAMPLE_MODULES,
				  "Make sure that there's a proper range for menu commands in resources.");
	mfcless::Menu hMenu = CreateFileMenu(nMaxItemsInExampleModulesMenu, m_ExampleModulePaths, P_("ExampleSongs/"), ID_EXAMPLE_MODULES);
	mfcless::Menu *const pMainMenu = GetMenu();
	if (hMenu && pMainMenu)
		pMainMenu->ModifyMenu(ID_EXAMPLE_MODULES, mfcless::menu::ByCommand | mfcless::menu::Popup, hMenu, m_InputHandler->GetMenuText(ID_EXAMPLE_MODULES));
	else
		MPT_ASSERT(false);
}


std::pair<mfcless::Menu , int> CMainFrame::FindMenuItemByCommand(mfcless::Menu &menu, uint32 commandID)
{
	const int numItems = menu.GetMenuItemCount();
	for(int item = 0; item < numItems; item++)
	{
		if(menu.GetMenuItemID(item) == static_cast<int>(commandID))
			return {menu, item};
		mfcless::Menu subMenu = menu.GetSubMenu(item);
		if(subMenu != nullptr)
		{
			if(auto result = FindMenuItemByCommand(subMenu, commandID); result.first != nullptr)
				return result;
		}
	}
	return {};
}


void CMainFrame::CreateTemplateModulesMenu()
{
	static_assert(nMaxItemsInTemplateModulesMenu == ID_FILE_OPENTEMPLATE_LASTINRANGE - ID_FILE_OPENTEMPLATE,
				  "Make sure that there's a proper range for menu commands in resources.");
	mfcless::Menu hMenu = CreateFileMenu(nMaxItemsInTemplateModulesMenu, m_TemplateModulePaths, P_("TemplateModules/"), ID_FILE_OPENTEMPLATE);
	auto [fileMenu, position] = FindMenuItemByCommand(*GetMenu(), ID_FILE_OPEN);
	if(hMenu && fileMenu)
	{
		fileMenu->RemoveMenu(position + 1, mfcless::menu::ByPosition);
		fileMenu->InsertMenu(position + 1, mfcless::menu::ByPosition | mfcless::menu::Popup, hMenu, m_InputHandler->GetMenuText(ID_FILE_OPENTEMPLATE));
	}
	else
		MPT_ASSERT_NOTREACHED();
}


void CMainFrame::UpdateMRUList()
{
	const auto [pMenu, firstMenu] = FindMenuItemByCommand(*GetMenu(), ID_MRU_LIST_FIRST);
	MPT_ASSERT(pMenu);
	if(!pMenu)
		return;

	for(int i = ID_MRU_LIST_FIRST; i <= ID_MRU_LIST_LAST; i++)
	{
		pMenu->DeleteMenu(i, mfcless::menu::ByCommand);
	}

	if(TrackerSettings::Instance().mruFiles.empty())
	{
		// MFC will automatically ignore if we set mfcless::menu::Grayed here because of mfcless::Window<QMainWindow>::m_bAutoMenuEnable.
		// So we will have to install a ON_UPDATE_COMMAND_UI callback...
		pMenu->InsertMenu(firstMenu, mfcless::menu::String | mfcless::menu::ByPosition, ID_MRU_LIST_FIRST, U_("Recent File"));
	} else
	{
		const mpt::PathString workDir = TrackerSettings::Instance().PathSongs.GetWorkingDir();
		const int entries = mpt::saturate_cast<int>(TrackerSettings::Instance().mruFiles.size());
		for(int i = 0; i < entries; i++)
		{
			mpt::ustring s = mpt::ufmt::val(i + 1) + U_(" ");
			// Add mnemonics
			if(i < 9)
			{
				s = U_("&") + s;
			} else if(i == 9)
			{
				s = U_("1&0 ");
			}

			const mpt::PathString &pathMPT = TrackerSettings::Instance().mruFiles[i];
			mpt::ustring path = pathMPT.ToUnicode();
			if(!mpt::PathCompareNoCase(workDir, pathMPT.GetDirectoryWithDrive()))
			{
				// Only show filename
				path = path.substr(workDir.AsNative().length());
			} else if(path.length() > 30)	// Magic number experimentally determined to be equal to MFC's behaviour
			{
				// Shorten path ("C:\Foo\VeryLongString...\Bar.it" => "C:\Foo\...\Bar.it")
				size_t start = path.find_first_of(U_("\\/"), path.find_first_of(U_("\\/")) + 1);
				size_t end = path.find_last_of(U_("\\/"));
				if(start < end)
				{
					path = path.substr(0, start + 1) + U_("...") + path.substr(end);
				}
			}
			path = mpt::replace(path, mpt::ustring(U_("&")), mpt::ustring(U_("&&")));
			s += path;
			pMenu->InsertMenu(firstMenu + i, mfcless::menu::String | mfcless::menu::ByPosition, ID_MRU_LIST_FIRST + i, mpt::ToUnicode(s));
		}
	}
}


bool CMainFrame::OnQueryEndSession()
{
	int modifiedCount = 0;
	for(const auto &modDoc : theApp.GetOpenDocuments())
	{
		if(modDoc->IsModified())
			modifiedCount++;
	}
#if MPT_WINNT_AT_LEAST(MPT_WIN_VISTA)
	ShutdownBlockReasonDestroy(m_hWnd);
	if(modifiedCount > 0)
	{
		ShutdownBlockReasonCreate(m_hWnd,
			MPT_WFORMAT("There {} {} unsaved file{}.")(modifiedCount == 1 ? L"is" : L"are", modifiedCount, modifiedCount == 1 ? L"" : L"s").c_str());
	}
#endif
	return modifiedCount ? false : true;
}


void CMainFrame::UpdateDocumentCount()
{
	const bool isLoaded = m_quickStartDlg != nullptr;
	const bool shouldLoad = !m_unloading && !theApp.GetOpenDocumentCount();
	if(shouldLoad && !isLoaded)
	{
		m_quickStartDlg = std::make_unique<QuickStartDlg>(m_TemplateModulePaths, m_ExampleModulePaths, m_mdiArea);
		m_quickStartDlg->CenterWindow();
		m_quickStartDlg->ShowWindow(true);
	} else if(isLoaded && !shouldLoad)
	{
		m_quickStartDlg.reset();
	}
}


#ifdef MPT_ENABLE_PLAYBACK_TEST_MENU
void CMainFrame::OnCreateMixerDump()
{
	std::string exts;
	for(const auto &ext : CSoundFile::GetSupportedExtensions(true))
	{
		exts += std::string("*.") + ext + std::string(";");
	}

	FileDialog dlg = OpenFileDialog()
		.AllowMultiSelect()
		.ExtensionFilter("Module Files (*.it,*.xm,...)|" + exts + "|All files(*.*)|*.*||");
	if(!dlg.Show(this))
		return;
	for(const auto &fileName : dlg.GetFilenames())
	{
		MPT_LOG_GLOBAL(LogDebug, "info", U_("Loading ") + fileName.ToUnicode());
		auto sndFile = std::make_unique<CSoundFile>();
		mpt::IO::InputFile f(fileName);
		if(!f.IsValid())
			continue;
		if(!sndFile->Create(GetFileReader(f)))
			continue;
		auto playTest = sndFile->CreatePlaybackTest(PlaybackTestSettings{});
		mpt::IO::ofstream outFile(fileName + P_(".testdata.gz"), std::ios::binary | std::ios::trunc);
		if(outFile)
		{
			std::ostringstream outStream;
			playTest.Serialize(outStream);
			#ifdef MPT_WITH_ZLIB
				std::string outData = std::move(outStream).str();
				WriteGzip(outFile, outData, fileName.GetFilename().ToUnicode() + U_(".testdata"));
			#else
				// miniz doesn't have gzip convenience functions
				outFile << std::move(outStream).str();
			#endif
		}
	}
}


void CMainFrame::OnVerifyMixerDump()
{
	FileDialog dlg = OpenFileDialog()
		.AllowMultiSelect()
		.ExtensionFilter("Test Data|*.testdata;*.testdata.gz|All files(*.*)|*.*||");
	if(!dlg.Show(this))
		return;
	for(const auto &fileName : dlg.GetFilenames())
	{
		MPT_LOG_GLOBAL(LogDebug, "info", U_("Loading ") + fileName.ToUnicode());
		try
		{
			auto modFileName = fileName;
			if(!mpt::PathCompareNoCase(modFileName.GetFilenameExtension(), P_(".gz")))
				modFileName = modFileName.ReplaceExtension({});
			if(!mpt::PathCompareNoCase(modFileName.GetFilenameExtension(), P_(".testdata")))
				modFileName = modFileName.ReplaceExtension({});

			mpt::IO::InputFile testFile{fileName};
			if(!testFile.IsValid())
				throw std::runtime_error{"Cannot open test data file: " + fileName.ToUTF8()};

			mpt::IO::InputFile modFile{modFileName};
			if(!modFile.IsValid())
				throw std::runtime_error{"Cannot open module data file: " + modFileName.ToUTF8()};

			FileReader testFileReader = GetFileReader(testFile);
			CGzipArchive archive{testFileReader};
			if(archive.IsArchive())
			{
				if(!archive.ExtractFile(0))
					throw std::runtime_error{"Cannot extract test data file!"};
				testFileReader = archive.GetOutputFile();
			}

			PlaybackTest playTest{testFileReader};
			auto sndFile = std::make_unique<CSoundFile>();
			sndFile->Create(GetFileReader(modFile));

			const auto result = PlaybackTest::Compare(playTest, sndFile->CreatePlaybackTest(playTest.GetSettings()));
			if(!result.empty())
			{
				InfoDialog infoDlg{this};
				infoDlg.SetCaption(U_("Test results for ") + fileName.AsNative());
				mpt::ustring content;
				for(const auto &line : result)
				{
					content += mpt::ToUnicode(line) + U_("\r\n");
				}
				infoDlg.SetContent(content);
				infoDlg.DoModal();
			}
		} catch(const std::exception &e)
		{
			Reporting::Error(MPT_UFORMAT("Cannot convert {}: {}")(fileName.ToUnicode(), mpt::ToUnicode(mpt::Charset::UTF8, e.what())), this);
		}
	}
}


void CMainFrame::OnConvertMixerDumpToText()
{
	FileDialog dlg = OpenFileDialog()
		.AllowMultiSelect()
		.ExtensionFilter("Test Data|*.testdata;*.testdata.gz|All files(*.*)|*.*||");
	if(!dlg.Show(this))
		return;
	for(const auto &fileName : dlg.GetFilenames())
	{
		MPT_LOG_GLOBAL(LogDebug, "info", U_("Loading ") + fileName.ToUnicode());
		try
		{
			mpt::IO::InputFile f(fileName);
			if(!f.IsValid())
				throw std::runtime_error{"Cannot open test data file!"};
			PlaybackTest playTest{GetFileReader(f)};
			mpt::IO::ofstream output(fileName.ReplaceExtension(P_(".tsv")), std::ios::binary);
			playTest.ToTSV(output);
		} catch(const std::exception &e)
		{
			Reporting::Error(MPT_UFORMAT("Cannot convert {}: {}")(fileName.ToUnicode(), mpt::ToUnicode(mpt::Charset::UTF8, e.what())), this);
		}
	}
}

#endif  // ENABLE_PLAYBACK_TEST_MENU


void CMainFrame::NotifyAccessibilityUpdate(QWidget &source)
{
	if(!IsPlaying() || m_pSndFile->m_PlayState.m_flags[SONG_PAUSED])
	{
		QAccessibleEvent event(&source, QAccessible::NameChanged);
		QAccessible::updateAccessibility(&event);
	}
}

// mfcless: what upstream's ITfLanguageProfileNotifySink does, on Qt's input
// method: when the keyboard layout changes, key positions might have
// changed too.
void CMainFrame::OnInputLanguageChanged()
{
	UpdateEffectKeys(GetActiveDoc());
}

OPENMPT_NAMESPACE_END
