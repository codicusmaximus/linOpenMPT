/*
 * Mainfrm.h
 * ---------
 * Purpose: Implementation of OpenMPT's main window code.
 * Notes  : mfcless twin of upstream's mptrack/Mainfrm.h. Same declarations in
 *          the same order; CMDIFrameWnd becomes QMainWindow with a QMdiArea,
 *          message handlers become plain member functions that the generated
 *          menus, timers and mfcless::ModMessage events call.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "Notification.h"
#include "UpdateHints.h"  // mfcless: upstream gets it through Mainbar.h
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

#include <QtWidgets/QMainWindow>
#include <QtGui/QFont>
#include <QtGui/QIcon>

#include <functional>
#include <thread>

class QMdiArea;
class QMenu;
class QStatusBar;
class QTimer;
class QWidget;
class QCloseEvent;
class QDragEnterEvent;
class QDropEvent;
class QEvent;

OPENMPT_NAMESPACE_BEGIN

class CAutoSaver;
class CDLSBank;
class CInputHandler;
class CModDoc;
class CModTree;
class CModTreeBar;
class CMainToolBar;
class CModView;
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
	: public QMainWindow
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
	static uint32 gcolrefVuMeter[NUM_VUMETER_PENS * 2];	// General tab VU meters

public:

	// Low-Level Audio
	CriticalSection m_SoundDeviceFillBufferCriticalSection;
	Util::MultimediaClock m_SoundDeviceClock;
	SoundDevice::IBase *gpSoundDevice = nullptr;
	QTimer *m_NotifyTimer = nullptr;
	VUMeter m_VUMeterInput;
	VUMeter m_VUMeterOutput;

	std::thread::id m_AudioThreadId;
	bool m_InNotifyHandler = false;

	// Midi Input
public:
	// mfcless: MIDI input runs on RtMidi (qt/mptrack/MidiInput.cpp).
	struct MidiInData
	{
		std::vector<std::byte> sysexData;
		mpt::mutex dataMutex;
		void *inHandle = nullptr;  // RtMidiIn
	};

	MidiInData midiInData;

public:
	// mfcless: the image lists are vectors of icons cut from the same bitmaps.
	std::vector<QIcon> m_MiscIcons, m_MiscIconsDisabled;			// Misc Icons
	std::vector<QIcon> m_PatternIcons, m_PatternIconsDisabled;	// Pattern icons (includes some from sample editor as well...)
	std::vector<QIcon> m_EnvelopeIcons;							// Instrument editor icons
	std::vector<QIcon> m_SampleIcons;								// Sample editor icons

protected:
	CModTreeBar *m_wndTree = nullptr;
	QStatusBar *m_wndStatusBar = nullptr;
	CMainToolBar *m_wndToolBar = nullptr;
	QMdiArea *m_mdiArea = nullptr;
	CSoundFile *m_pSndFile = nullptr; // != NULL only when currently playing or rendering
	QObject *m_hWndMidi = nullptr;
	std::function<void(mpt::const_byte_span)> m_midiSysExCallback;
	samplecount_t m_dwTimeSec = 0;
	QTimer *m_nTimer = nullptr;
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
	bool InAudioThread() const noexcept { return std::this_thread::get_id() == m_AudioThreadId; }
	bool InNotifyHandler() const noexcept { return m_InNotifyHandler; }

	bool audioOpenDevice();
	void audioCloseDevice();
	bool IsAudioDeviceOpen() const;
	bool DoNotification(uint32 dwSamplesRead, int64 streamPosition);

// Midi Input Functions
public:
	bool midiOpenDevice(bool showSettings = true);
	void midiCloseDevice();
	void SetMidiRecordWnd(QObject *hwnd, std::function<void(mpt::const_byte_span)> sysExCallback = {})
	{
		m_hWndMidi = hwnd;
		m_midiSysExCallback = std::move(sysExCallback);
	}
	QObject *GetMidiRecordWnd() const { return m_hWndMidi; }
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

	void SetUserText(const mpt::ustring &text);
	void SetInfoText(const mpt::ustring &text);
	void SetXInfoText(const mpt::ustring &text);
	void SetHelpText(const mpt::ustring &text);
	mpt::ustring GetHelpText() const;
	uint32 GetBaseOctave() const;
	CModDoc *GetActiveDoc() const;
	CModView *GetActiveView() const;
	void OnDocumentCreated(CModDoc *pModDoc);
	void OnDocumentClosed(CModDoc *pModDoc);
	void UpdateTree(CModDoc *pModDoc, UpdateHint hint, QObject *pHint = nullptr);
	void RefreshDlsBanks();
	static CInputHandler *GetInputHandler();
	void SetElapsedTime(double t) { m_dwTimeSec = mpt::saturate_trunc<samplecount_t>(t * 10.0); }

#if defined(MPT_ENABLE_UPDATE)
	bool ShowUpdateIndicator(const UpdateCheckResult &result, const mpt::ustring &releaseVersion, const mpt::ustring &infoURL, bool showHighlight);
#endif // MPT_ENABLE_UPDATE

	CModTree *GetUpperTreeview();
	CModTree *GetLowerTreeview();
	bool SetTreeSoundfile(FileReader &file);

	void CreateExampleModulesMenu();
	void CreateTemplateModulesMenu();
	static std::pair<QMenu *, int> FindMenuItemByCommand(QMenu &menu, uint32 commandID);

	// Creates submenu whose items are filenames of files in both
	// AppDirectory\folderName\ (usually C:\Program Files\OpenMPT\folderName\)
	// and
	// ConfigDirectory\folderName (usually %appdata%\OpenMPT\folderName\)
	// [in] maxCount: Maximum number of items allowed in the menu
	// [out] paths: Receives the full paths of the files added to the menu.
	// [in] folderName: Name of the folder
	// [in] idRangeBegin: First ID for the menu item.
	static QMenu *CreateFileMenu(const size_t maxCount, std::vector<mpt::PathString> &paths, const mpt::PathString &folderName, const uint16 idRangeBegin);

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
	QObject *GetFollowSong() const;
	QObject *GetFollowSong(const CModDoc *pDoc) const { return (pDoc == GetModPlaying()) ? GetFollowSong() : nullptr; }
	void ResetNotificationBuffer();

	// Notify accessbility software that it should read out updated UI elements
	void NotifyAccessibilityUpdate(QWidget &source);

	// mfcless: PostMessage to the main frame, as a queued mfcless::ModMessage.
	void PostModMessage(uint32 msg, uintptr_t wParam = 0, intptr_t lParam = 0);

	void UpdateDocumentCount();

	// mfcless: the window geometry, as Qt's saveGeometry() in Display/WindowGeometry
	// (upstream keeps a WINDOWPLACEMENT in Display/WindowPlacement).
	void SaveWindowGeometry(SettingsContainer &conf);
	void RestoreWindowGeometry(SettingsContainer &conf);

// Overrides
protected:
	// mfcless: CFrameWnd's overrides, as Qt event handlers.
	bool event(QEvent *e) override;
	bool eventFilter(QObject *watched, QEvent *e) override;
	void closeEvent(QCloseEvent *e) override;
	void changeEvent(QEvent *e) override;
	void dragEnterEvent(QDragEnterEvent *e) override;
	void dropEvent(QDropEvent *e) override;
	void OnUpdateFrameTitle(bool bAddToTitle);

	/// Opens either template or example menu item.
	void OpenMenuItemFile(const uint32 nId, const bool isTemplateFile);

	void ShowToolbarMenu(QPoint screenPt);
	void AddToolBarMenuEntries(QMenu &menu) const;

	void RecreateImageLists();
	void SetupStatusBarSizes();

public:
	void UpdateMRUList();

// Implementation
public:
	~CMainFrame() override;
	void RecalcLayout(bool notify = true);

	void OnTimerGUI();
	void OnTimerNotify();

// Message map functions
	// mfcless: upstream's message map is the table in Mainfrm.cpp that
	// connects menu commands, timers and mfcless::ModMessage events to these.
public:
	void OnAddDlsBank();
	void OnImportMidiLib();
	void OnViewOptions();
	void OnHelp();
protected:
	int OnCreate();
	void OnRButtonDown(uint32, QPoint);
	void OnClose();
	void OnTimer();

	void OnPluginManager();
	void OnClipboardManager();

	intptr_t OnViewMIDIMapping(uintptr_t wParam, intptr_t lParam);
	bool OnUpdateTime() const;  // update handler
	bool OnUpdateUser() const;  // update handler
	bool OnUpdateInfo() const;  // update handler
	bool OnUpdateXInfo() const;  // update handler
	bool OnUpdateMidiRecord() const;  // update handler
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
	bool OnUpdateMRUItem() const;  // update handler
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
	bool OnQueryEndSession();
	void OnActivateApp(bool active);

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

public:
	bool UpdateEffectKeys(const CModDoc *modDoc);
	void OnShowWindow(bool bShow, uint32 nStatus);

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

OPENMPT_NAMESPACE_END
