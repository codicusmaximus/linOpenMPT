/*
 * Mainfrm.h
 * ---------
 * Purpose: Implementation of OpenMPT's main window code.
 * Notes  : mfcless twin of upstream's mptrack/Mainfrm.h. The MDI frame is a
 *          QMainWindow with a QMdiArea in the middle; the tree bar, toolbar
 *          and status bar are its children as upstream's control bars were.
 *          Windows' text services (keyboard layout notifications), the focus
 *          hook and HTML help are left out; MIDI input runs on RtMidi.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "mfcless/System.h"
#include "mfcless/StatusBar.h"
#include "mfcless/Menus.h"
#include "mfcless/Graphics.h"
#include "mfcless/Window.h"
#include "DocumentView.h"
#include <QtWidgets/QMainWindow>
#include "CImageListEx.h"
#include "Mainbar.h"
#include "Notification.h"
#include "openmpt/soundbase/Dither.hpp"
#include "Settings.h"
#include "../soundlib/AudioCriticalSection.h"
#include "../soundlib/AudioReadTarget.h"
#include "../soundlib/Sndfile.h"
#include "mpt/audio/span.hpp"
#include "mpt/mutex/mutex.hpp"
#include "openmpt/sounddevice/SoundDevice.hpp"
#include "openmpt/sounddevice/SoundDeviceBuffer.hpp"
#include "../misc/mptClock.h"

#include <functional>
#include <thread>

class QCursor;
class QMdiArea;
class QMdiSubWindow;
class QFont;
class QIcon;
class QObject;
class QWidget;

OPENMPT_NAMESPACE_BEGIN

class CAutoSaver;
class CDLSBank;
class CChildFrame;
class CInputHandler;
class CModDoc;
class QuickStartDlg;
struct UpdateCheckResult;
struct UpdateHint;
struct MODPLUGDIB;
enum class MidiSetup: int32;
enum class MainToolBarItem : uint32;
enum SoundDeviceStopMode : int;
namespace SoundDevice {
class Base;
class ICallback;
} // namerspace SoundDevice

#define MAINFRAME_TITLE U_("Open ModPlug Tracker")


#define NUM_VUMETER_PENS		32


// Tab Order
enum OptionsPage
{
	OPTIONS_PAGE_DEFAULT = 0,
	OPTIONS_PAGE_GENERAL = OPTIONS_PAGE_DEFAULT,
	OPTIONS_PAGE_SOUNDCARD,
	OPTIONS_PAGE_MIXER,
	OPTIONS_PAGE_PLAYER,
	OPTIONS_PAGE_SAMPLEDITOR,
	OPTIONS_PAGE_KEYBOARD,
	OPTIONS_PAGE_COLORS,
	OPTIONS_PAGE_MIDI,
	OPTIONS_PAGE_PATHS,
	OPTIONS_PAGE_UPDATE,
	OPTIONS_PAGE_ADVANCED,
	OPTIONS_PAGE_WINE,
};


/////////////////////////////////////////////////////////////////////////
// Player position notification

#define MAX_UPDATE_HISTORY		2000 // 2 seconds with 1 ms updates

template<> inline SettingValue ToSettingValue(const mfcless::WindowPlacement &val)
{
	return SettingValue(EncodeBinarySetting<mfcless::WindowPlacement>(val), "mfcless::WindowPlacement");
}
template<> inline mfcless::WindowPlacement FromSettingValue(const SettingValue &val)
{
	MPT_ASSERT(val.GetTypeTag() == "mfcless::WindowPlacement");
	return DecodeBinarySetting<mfcless::WindowPlacement>(val.as<std::vector<std::byte> >());
}


class VUMeter
	: public IMonitorInput
	, public IMonitorOutput
{
public:
	static constexpr std::size_t maxChannels = 4;
	static const float dynamicRange; // corresponds to the current implementation of the UI widget diplaying the result
	struct Channel
	{
		int32 peak = 0;
		bool clipped = false;
	};
private:
	Channel channels[maxChannels];
	int32 decayParam;
	void Process(Channel &channel, MixSampleInt sample);
	void Process(Channel &channel, MixSampleFloat sample);
public:
	VUMeter() : decayParam(0) { SetDecaySpeedDecibelPerSecond(88.0f); }
	void SetDecaySpeedDecibelPerSecond(float decibelPerSecond);
public:
	const Channel & operator [] (std::size_t channel) const { return channels[channel]; }
	void Process(mpt::audio_span_interleaved<const MixSampleInt> buffer);
	void Process(mpt::audio_span_planar<const MixSampleInt> buffer);
	void Process(mpt::audio_span_interleaved<const MixSampleFloat> buffer);
	void Process(mpt::audio_span_planar<const MixSampleFloat> buffer);
	void Decay(int32 secondsNum, int32 secondsDen);
	void ResetClipped();
};


class CMainFrame
	: public mfcless::Window<QMainWindow>
	, public SoundDevice::CallbackBufferHandler<DithersOpenMPT>
	, public SoundDevice::IMessageReceiver
{
	Q_OBJECT
	// static data
public:

	// Globals
	static OptionsPage m_nLastOptionsPage;

	// GDI
	QFont m_hCommentsFont;
	static QFont m_hGUIFont;
	static QIcon m_hIcon;
	static mfcless::Pen penDarkGray, penHalfDarkGray, penGray99;
	static QCursor curDragging, curNoDrop, curArrow, curNoDrop2, curVSplit;
	static MODPLUGDIB *bmpNotes;
	static uint32 gcolrefVuMeter[NUM_VUMETER_PENS * 2];	// General tab VU meters

public:

	// Low-Level Audio
	CriticalSection m_SoundDeviceFillBufferCriticalSection;
	Util::MultimediaClock m_SoundDeviceClock;
	SoundDevice::IBase *gpSoundDevice = nullptr;
	uintptr_t m_NotifyTimer = 0;
	VUMeter m_VUMeterInput;
	VUMeter m_VUMeterOutput;

	uint32 m_AudioThreadId = 0;  // mfcless: as mfcless::CurrentThreadId gives it
	bool m_InNotifyHandler = false;

	// Midi Input
public:
	struct MidiInData
	{
		// mfcless: MIDI input runs on RtMidi (qt/mptrack/Mpt_midi.cpp),
		// which collects SysEx messages itself.
		std::vector<std::byte> sysexData;
		mpt::mutex dataMutex;
		void *inHandle = nullptr;  // RtMidiIn
	};

	MidiInData midiInData;

public:
	CImageListEx m_MiscIcons, m_MiscIconsDisabled;			// Misc Icons
	CImageListEx m_PatternIcons, m_PatternIconsDisabled;	// Pattern icons (includes some from sample editor as well...)
	CImageListEx m_EnvelopeIcons;							// Instrument editor icons
	CImageListEx m_SampleIcons;								// Sample editor icons

protected:
	CModTreeBar m_wndTree;
	mfcless::StatusBar m_wndStatusBar;
	CMainToolBar m_wndToolBar;
	// mfcless: the MDI client window, in the middle of the main window, and
	// the splitter that holds it and the tree bar.
	QMdiArea *m_mdiArea = nullptr;
	class QSplitter *m_centralSplitter = nullptr;
	// mfcless: the main menu (IDR_MAINFRAME); the menu bar shows its popups.
	mfcless::Menu m_menu;
	// mfcless: the window's title without the document's (CFrameWnd::m_strTitle).
	mpt::ustring m_strTitle;
	CSoundFile *m_pSndFile = nullptr; // != NULL only when currently playing or rendering
	QWidget * m_hWndMidi = nullptr;
	std::function<void(mpt::const_byte_span)> m_midiSysExCallback;
	samplecount_t m_dwTimeSec = 0;
	uintptr_t m_nTimer = 0;
	uint32 m_nAvgMixChn = 0, m_nMixChn = 0;
	uint32 m_currentSpeed = 0;
	// Misc
	class COptionsSoundcard *m_SoundCardOptionsDialog = nullptr;
#if defined(MPT_ENABLE_UPDATE)
	class CUpdateSetupDlg *m_UpdateOptionsDialog = nullptr;
	std::unique_ptr<UpdateCheckResult> m_updateCheckResult;
	bool m_cancelUpdateCheck = false;
#endif // MPT_ENABLE_UPDATE
	bool m_bOptionsLocked = false;
	bool m_unloading = false;

	// Notification Buffer
	mpt::mutex m_NotificationBufferMutex; // to avoid deadlocks, this mutex should only be taken as a innermost lock, i.e. do not block on anything while holding this mutex
	Util::fixed_size_queue<Notification, MAX_UPDATE_HISTORY> m_NotifyBuffer;

	// Instrument preview in tree view
	CSoundFile m_WaveFile;
	ModSample m_metronomeMeasure{}, m_metronomeBeat{};

	mpt::ustring m_userText, m_infoText, m_xInfoText;

	mpt::heap_value<CAutoSaver> m_AutoSaver;
	mpt::heap_value<CInputHandler> m_InputHandler;

public:
	bool m_bModTreeHasFocus = false;

public:
	CMainFrame();
	void Initialize();


// Low-Level Audio
public:
	static void UpdateDspEffects(CSoundFile &sndFile, bool reset=false);
	static void UpdateAudioParameters(CSoundFile &sndFile, bool reset=false);

	// from SoundDevice::IBufferHandler
	uint64 SoundCallbackGetReferenceClockNowNanoseconds() const override;
	void SoundCallbackPreStart() override;
	void SoundCallbackPostStop() override;
	bool SoundCallbackIsLockedByCurrentThread() const override;
	void SoundCallbackLock() override;
	uint64 SoundCallbackLockedGetReferenceClockNowNanoseconds() const override;
	void SoundCallbackLockedProcessPrepare(SoundDevice::TimeInfo timeInfo) override;
	void SoundCallbackLockedCallback(SoundDevice::CallbackBuffer<DithersOpenMPT> &buffer) override;
	void SoundCallbackLockedProcessDone(SoundDevice::TimeInfo timeInfo) override;
	void SoundCallbackUnlock() override;

	// from SoundDevice::IMessageReceiver
	void SoundDeviceMessage(LogLevel level, const mpt::ustring &str) override;

	bool InGuiThread() const noexcept;
	bool InAudioThread() const noexcept { return mfcless::CurrentThreadId() == m_AudioThreadId; }
	bool InNotifyHandler() const noexcept { return m_InNotifyHandler; }

	bool audioOpenDevice();
	void audioCloseDevice();
	bool IsAudioDeviceOpen() const;
	bool DoNotification(uint32 dwSamplesRead, int64 streamPosition);

// Midi Input Functions
public:
	bool midiOpenDevice(bool showSettings = true);
	void midiCloseDevice();
	void SetMidiRecordWnd(QWidget * hwnd, std::function<void(mpt::const_byte_span)> sysExCallback = {})
	{
		m_hWndMidi = hwnd;
		m_midiSysExCallback = std::move(sysExCallback);
	}
	QWidget * GetMidiRecordWnd() const { return m_hWndMidi; }
	auto GetMidiSysexCallback() const { return m_midiSysExCallback; }
	void LoadMetronomeSamples();
	void UpdateMetronomeSamples();
	void UpdateMetronomeVolume();

	static int ApplyVolumeRelatedSettings(const uint32 &dwParam1, const uint8 midivolume);

// static functions
public:
	static CMainFrame *GetMainFrame() noexcept;
	static void UpdateColors();
	static QIcon GetModIcon() { return m_hIcon; }
	static QFont GetGUIFont() { return m_hGUIFont; }

	// Misc functions
public:
	QFont &GetCommentsFont() { return m_hCommentsFont; }

	void SetUserText(const mpt::uchar *text);
	void SetInfoText(const mpt::uchar *text);
	void SetXInfoText(const mpt::uchar *text);
	void SetUserText(const mpt::ustring &text) { SetUserText(text.c_str()); }
	void SetInfoText(const mpt::ustring &text) { SetInfoText(text.c_str()); }
	void SetXInfoText(const mpt::ustring &text) { SetXInfoText(text.c_str()); }
	void SetHelpText(const mpt::uchar *text);
	void SetHelpText(const mpt::ustring &text) { SetHelpText(text.c_str()); }
	mpt::ustring GetHelpText() const;
	uint32 GetBaseOctave() const;
	CModDoc *GetActiveDoc() const;
	mfcless::DocumentView *GetActiveView() const;
	void OnDocumentCreated(CModDoc *pModDoc);
	void OnDocumentClosed(CModDoc *pModDoc);
	void UpdateTree(CModDoc *pModDoc, UpdateHint hint, QObject *pHint = nullptr);
	void RefreshDlsBanks();
	static CInputHandler *GetInputHandler();
	void SetElapsedTime(double t) { m_dwTimeSec = mpt::saturate_trunc<samplecount_t>(t * 10.0); }

#if defined(MPT_ENABLE_UPDATE)
	bool ShowUpdateIndicator(const UpdateCheckResult &result, const mpt::ustring &releaseVersion, const mpt::ustring &infoURL, bool showHighlight);
#endif // MPT_ENABLE_UPDATE

	CModTree *GetUpperTreeview() { return m_wndTree.m_pModTree; }
	CModTree *GetLowerTreeview() { return m_wndTree.m_pModTreeData; }
	bool SetTreeSoundfile(FileReader &file) { return m_wndTree.SetTreeSoundfile(file); }

	void CreateExampleModulesMenu();
	void CreateTemplateModulesMenu();
	static std::pair<mfcless::Menu , int> FindMenuItemByCommand(mfcless::Menu &menu, uint32 commandID);

	// Creates submenu whose items are filenames of files in both
	// AppDirectory\folderName\ (usually C:\Program Files\OpenMPT\folderName\)
	// and
	// ConfigDirectory\folderName (usually %appdata%\OpenMPT\folderName\)
	// [in] maxCount: Maximum number of items allowed in the menu
	// [out] paths: Receives the full paths of the files added to the menu.
	// [in] folderName: Name of the folder
	// [in] idRangeBegin: First ID for the menu item.
	static mfcless::Menu CreateFileMenu(const size_t maxCount, std::vector<mpt::PathString> &paths, const mpt::PathString &folderName, const uint16 idRangeBegin);

// Player functions
public:

	// High-level synchronous playback functions, do not hold AudioCriticalSection while calling these
	bool PreparePlayback();
	bool StartPlayback();
	void StopPlayback();
	bool RestartPlayback();
	bool PausePlayback();
	static bool IsValidSoundFile(CSoundFile &sndFile) { return sndFile.GetType() != MOD_TYPE_NONE; }
	static bool IsValidSoundFile(CSoundFile *pSndFile) { return pSndFile && pSndFile->GetType(); }
	void SetPlaybackSoundFile(CSoundFile *pSndFile);
	void UnsetPlaybackSoundFile();
	void GenerateStopNotification();

	bool PlayMod(CModDoc *);
	bool StopMod(CModDoc *pDoc = nullptr);
	bool PauseMod(CModDoc *pDoc = nullptr);

	bool StopSoundFile(CSoundFile *);
	bool PlaySoundFile(CSoundFile *);
	bool PlaySoundFile(const mpt::PathString &filename, ModCommand::NOTE note, int volume = -1);
	bool PlaySoundFile(CSoundFile &sndFile, INSTRUMENTINDEX nInstrument, SAMPLEINDEX nSample, ModCommand::NOTE note, int volume = -1);
	bool PlayDLSInstrument(const CDLSBank &bank, uint32 instr, uint32 region, ModCommand::NOTE note, int volume = -1);

	void InitPreview();
	void PreparePreview(ModCommand::NOTE note, int volume);
	void StopPreview() { StopSoundFile(&m_WaveFile); }
	void PlayPreview() { PlaySoundFile(&m_WaveFile); }

	inline bool IsPlaying() const { return m_pSndFile != nullptr; }
	// Return currently playing module (nullptr if none is playing)
	inline CModDoc *GetModPlaying() const { return m_pSndFile ? m_pSndFile->GetpModDoc() : nullptr; }
	// Return currently playing module (nullptr if none is playing)
	inline CSoundFile *GetSoundFilePlaying() const { return m_pSndFile; }
	void InitRenderer(CSoundFile *);
	void StopRenderer(CSoundFile *);
	void SwitchToActiveView();

	void IdleHandlerSounddevice();

	void ResetSoundCard();
	void SetupSoundCard(SoundDevice::Settings deviceSettings, SoundDevice::Identifier deviceIdentifier, SoundDeviceStopMode stoppedMode, bool forceReset = false);
	void SetupMiscOptions();
	void SetupPlayer();

	void SetupMidi(FlagSet<MidiSetup> d, uint32 n);
	QWidget * GetFollowSong() const;
	QWidget * GetFollowSong(const CModDoc *pDoc) const { return (pDoc == GetModPlaying()) ? GetFollowSong() : nullptr; }
	void ResetNotificationBuffer();

	// Notify accessbility software that it should read out updated UI elements
	void NotifyAccessibilityUpdate(QWidget &source);

	void UpdateDocumentCount();

	// mfcless: what CFrameWnd did with its menu and title.
	mfcless::Menu *GetMenu() { return &m_menu; }
	// Shows the main menu's changes in the menu bar.
	void DrawMenuBar();
	mpt::ustring GetTitle() const { return m_strTitle; }
	void SetTitle(const mpt::ustring &title) { m_strTitle = title; }
	void UpdateFrameTitleForDocument(const mpt::ustring &docTitle);
	// Commands go to the active MDI child (its view and document), then this
	// window, then the application (CMDIFrameWnd::OnCmdMsg). Items without
	// any handler are disabled, as MFC does.
	bool RouteCommand(uint32 id) override;
	bool RouteCommandUpdate(uint32 id, mfcless::CommandState &state) override;
	bool UpdateCommandState(uint32 id, mfcless::CommandState &state);
	// The control bars' shown state (View > Tree View): CFrameWnd::OnBarCheck.
	bool OnBarCheck(uint32 id);
	void OnUpdateControlBarMenu(mfcless::CommandState &state);
	// The window's activation (WM_ACTIVATE and WM_ACTIVATEAPP).
	void OnApplicationStateChanged(bool active);
	// Keeps the last focussed item of the views (upstream: a CBT hook).
	void OnFocusChanged(QWidget *lostFocus, QWidget *gainFocus);
	void BeginWaitCursor();
	void EndWaitCursor();
	void OnInputLanguageChanged();

	// mfcless: what CMDIFrameWnd did with its MDI client.
	QMdiArea *GetMDIClient() const { return m_mdiArea; }
	CChildFrame *MDIGetActive() const;
	void MDIActivate(CChildFrame *frame);
	void MDINext();
	void MDIPrev();

// Overrides
protected:
	// ClassWizard generated virtual function overrides
	bool PreTranslateMessage(mfcless::Message *pMsg) override;
public:
	bool DestroyWindow() override;
	void OnUpdateFrameTitle(bool bAddToTitle);
protected:
	/// Opens either template or example menu item.
	void OpenMenuItemFile(const uint32 nId, const bool isTemplateFile);

	void ShowToolbarMenu(mfcless::Point screenPt);
	void AddToolBarMenuEntries(mfcless::Menu &menu) const;

	void RecreateImageLists();
	void SetupStatusBarSizes();

public:
	void UpdateMRUList();

// Implementation
public:
	~CMainFrame() override;
	void RecalcLayout(bool notify = true);

	// mfcless: the window geometry, as Qt's saveGeometry() in Display/WindowGeometry
	// (upstream keeps a WINDOWPLACEMENT in Display/WindowPlacement).
	void SaveWindowGeometry(SettingsContainer &conf);
	void RestoreWindowGeometry(SettingsContainer &conf);

	void OnTimerGUI();
	void OnTimerNotify();

// Message map functions
public:
	void OnAddDlsBank();
	void OnImportMidiLib();
	void OnViewOptions();
	void OnHelp();
public:
	int OnCreate();
protected:
	void closeEvent(class QCloseEvent *e) override;
	void OnRButtonDown(uint32, mfcless::Point);
	void OnClose();
	void OnTimer(uintptr_t);
	intptr_t OnDPIChanged(uintptr_t, intptr_t);

	void OnPluginManager();
	void OnClipboardManager();

	intptr_t OnViewMIDIMapping(uintptr_t wParam, intptr_t lParam);
	void OnUpdateTime(mfcless::CommandState &state);
	void OnUpdateUser(mfcless::CommandState &state);
	void OnUpdateInfo(mfcless::CommandState &state);
	void OnUpdateXInfo(mfcless::CommandState &state);
	void OnUpdateMidiRecord(mfcless::CommandState &state);
	void OnPlayerPause();
	void OnMidiRecord();
	void OnPrevOctave();
	void OnNextOctave();
	void OnPanic();
	void OnReportBug();
	bool OnInternetLink(uint32 nID);
	intptr_t OnUpdatePosition(uintptr_t, intptr_t lParam);
	intptr_t OnUpdateViews(uintptr_t modDoc, intptr_t hint);
	intptr_t OnSetModified(uintptr_t modDoc, intptr_t);
	void OnOpenTemplateModule(uint32 nId);
	void OnExampleSong(uint32 nId);
	void OnOpenMRUItem(uint32 nId);
	void OnUpdateMRUItem(mfcless::CommandState &state);
	intptr_t OnInvalidatePatterns(uintptr_t, intptr_t);
	intptr_t OnCustomKeyMsg(uintptr_t, intptr_t);
	void OnInternetUpdate();
	void OnUpdateAvailable();
	void OnShowSettingsFolder();
#if defined(MPT_ENABLE_UPDATE)
	intptr_t OnUpdateCheckStart(uintptr_t wparam, intptr_t lparam);
	intptr_t OnUpdateCheckProgress(uintptr_t wparam, intptr_t lparam);
	intptr_t OnUpdateCheckCanceled(uintptr_t wparam, intptr_t lparam);
	intptr_t OnUpdateCheckFailure(uintptr_t wparam, intptr_t lparam);
	intptr_t OnUpdateCheckSuccess(uintptr_t wparam, intptr_t lparam);
	intptr_t OnToolbarUpdateIndicatorClick(uintptr_t wparam, intptr_t lparam);
#endif // MPT_ENABLE_UPDATE
	void OnDropFiles(const QStringList &files);
	bool OnQueryEndSession();



	void OnToggleMainBarShowOctave();
	void OnToggleMainBarShowTempo();
	void OnToggleMainBarShowSpeed();
	void OnToggleMainBarShowRowsPerBeat();
	void OnToggleMainBarShowGlobalVolume();
	void OnToggleMainBarShowVUMeter();
	void OnToggleMainBarShowFileIcons();
	void OnToggleMainBarShowEditIcons();
	void OnToggleMainBarShowPlayIcons();
	void OnToggleMainBarShowMiscIcons();
	void OnToggleMainBarItem(MainToolBarItem item, uint32 menuID);
	void OnToggleTreeViewOnLeft();

	void OnCreateMixerDump();
	void OnVerifyMixerDump();
	void OnConvertMixerDumpToText();
	void RegisterMessageMap() override;
public:
	bool UpdateEffectKeys(const CModDoc *modDoc);
	void OnShowWindow(bool bShow, uint32 nStatus) override;

	// Defines maximum number of items in example modules menu.
	static constexpr size_t nMaxItemsInExampleModulesMenu = 50;
	static constexpr size_t nMaxItemsInTemplateModulesMenu = 50;

private:
	/// Array of paths of example modules that are available from help menu.
	std::vector<mpt::PathString> m_ExampleModulePaths;
	/// Array of paths of template modules that are available from file menu.
	std::vector<mpt::PathString> m_TemplateModulePaths;

	std::unique_ptr<QuickStartDlg> m_quickStartDlg;
};


/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.


OPENMPT_NAMESPACE_END
