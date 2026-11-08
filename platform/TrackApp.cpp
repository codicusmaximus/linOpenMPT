// linOpenMPT platform layer: the Linux implementation of upstream's
// application object (CTrackApp, mptrack/Mptrack.h).
//
// Upstream's Mptrack.cpp mixes app logic with Windows UI start-up, so it is
// not compiled. This file implements the CTrackApp members the shared code
// needs, following upstream's InitInstanceImpl() step by step but without
// windows: random devices, config paths, settings files, tracker settings,
// sound device manager. The Qt application (plan step 3) builds on this.

#include "stdafx.h"
#include "Mptrack.h"
#include "Settings.h"
#include "TrackerSettings.h"
#include "ExceptionHandler.h"
#include "UpdateCheck.h"
#include "TuningDialog.h"
#include "../soundlib/Dlsbank.h"
#include "../soundlib/plugins/PluginManager.h"
#include "openmpt/sounddevice/SoundDeviceManager.hpp"
#include "mpt/random/seed.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <system_error>
#include "MptrackResources.h"

OPENMPT_NAMESPACE_BEGIN

// Upstream keeps these in classes defined only in Windows .cpp files.
class GdiplusRAII { };
// Upstream holds sound-API component handles here (Mptrack.cpp). On Linux
// the device manager uses its default enumerators directly.
struct AllSoundDeviceComponents { };
class CModDocTemplate;

// ---- statics upstream defines in Mptrack.cpp / other UI files -------------
CTrackApp theApp;
MODTYPE CTrackApp::m_nDefaultDocType = MOD_TYPE_IT;
MidiLibrary CTrackApp::midiLibrary;
std::vector<std::unique_ptr<CDLSBank>> CTrackApp::gpDLSBanks;
const mpt::Charset TuningCharsetFallback = mpt::Charset::Locale;

bool ExceptionHandler::fullMemDump = false;
bool ExceptionHandler::stopSoundDeviceOnCrash = true;
bool ExceptionHandler::stopSoundDeviceBeforeDump = false;
bool ExceptionHandler::delegateToWindowsHandler = false;
bool ExceptionHandler::debugExceptionHandler = false;
bool ExceptionHandler::useAnyCrashHandler = false;
bool ExceptionHandler::useImplicitFallbackSEH = false;
bool ExceptionHandler::useExplicitSEH = false;
bool ExceptionHandler::handleStdTerminate = false;
bool ExceptionHandler::handleMfcExceptions = false;

namespace Tracker
{
mpt::recursive_mutex_with_lock_count &GetGlobalMutexRef()
{
	return theApp.GetGlobalMutexRef();
}
} // namespace Tracker

MPT_NOINLINE void AssertHandler(const mpt::source_location &loc, const char *expr, const char *msg)
{
	std::cerr << "linOpenMPT: assertion failed: " << (expr ? expr : "") << " at "
	          << (loc.file_name() ? loc.file_name() : "?") << ":" << loc.line();
	if(msg)
		std::cerr << " (" << msg << ")";
	std::cerr << std::endl;
	if(!msg)
		std::abort();
}

void DebugTraceDump()
{
}

std::vector<mpt::ustring> CUpdateCheck::GetDefaultUpdateSigningKeysRootAnchors()
{
	return {
		U_("https://update.openmpt.de/update/"),
		U_("https://demo.openmpt.org/update/"),
	};
}

mpt::ustring CUpdateCheck::GetDefaultAPIURL()
{
	return U_("https://update.openmpt.org/api/v3/");
}

// ---- CTrackApp ------------------------------------------------------------

CTrackApp::CTrackApp()
{
	// mptrack.rc, linked into the binary as on Windows.
	LinOpenMPTRegisterMptrackResources();
	m_dwRestartManagerSupportFlags = AFX_RESTART_MANAGER_SUPPORT_RESTART | AFX_RESTART_MANAGER_REOPEN_PREVIOUS_FILES;
}

CTrackApp::~CTrackApp()
{
}

// XDG config directory for linOpenMPT, e.g. ~/.config/linOpenMPT/.
// LINOPENMPT_CONFIG_DIR overrides it (used by tests).
static mpt::PathString LinuxConfigPath()
{
	std::string dir;
	if(const char *env = std::getenv("LINOPENMPT_CONFIG_DIR"); env && *env)
		dir = env;
	else if(const char *xdg = std::getenv("XDG_CONFIG_HOME"); xdg && *xdg)
		dir = std::string(xdg) + "/linOpenMPT";
	else if(const char *home = std::getenv("HOME"); home && *home)
		dir = std::string(home) + "/.config/linOpenMPT";
	else
		dir = "/tmp/linOpenMPT";
	std::error_code ec;
	std::filesystem::create_directories(dir, ec);
	return mpt::PathString::FromUTF8(dir).WithTrailingSlash();
}

BOOL CTrackApp::InitInstance()
{
	m_GuiThreadId = GetCurrentThreadId();

	m_RD = std::make_unique<mpt::random_device>();
	mpt::set_global_random_device(m_RD.get());
	m_PRNG = std::make_unique<mpt::thread_safe_prng<mpt::default_prng>>(mpt::make_prng<mpt::default_prng>(RandomDevice()));
	mpt::set_global_prng(m_PRNG.get());
	mpt::crand::reseed(RandomDevice());

	// Paths. Linux keeps the install read-only and the config per user.
	m_InstallPath = mpt::PathString::FromUTF8(std::filesystem::current_path().string()).WithTrailingSlash();
	m_InstallBinPath = m_InstallPath;
	m_InstallBinArchPath = m_InstallPath;
	m_InstallPkgPath = m_InstallPath;
	m_ConfigPath = LinuxConfigPath();
	m_szConfigFileName = m_ConfigPath + P_("mptrack.ini");
	m_szPluginCacheFileName = m_ConfigPath + P_("plugin.cache");

	m_pSettingsIniFile = new IniFileSettingsBackend(m_szConfigFileName);
	m_pSettings = new SettingsContainer(m_pSettingsIniFile);
	m_pDebugSettings = new DebugSettings(*m_pSettings);
	m_pTrackerSettings = new TrackerSettings(*m_pSettings);

	m_pSongSettingsIniFile = new IniFileSettingsBackend(GetConfigPath() + P_("SongSettings.ini"));
	m_pSongSettings = new SettingsContainer(m_pSongSettingsIniFile);
	m_pPluginCache = new IniFileSettingsContainer(m_szPluginCacheFileName);

	// Sound devices, through OpenMPT's own sound device layer.
	SoundDevice::SysInfo sysInfo(mpt::osinfo::get_class());
	SoundDevice::AppInfo appInfo;
	appInfo.SetName(U_("linOpenMPT"));
	appInfo.BoostedThreadRealtimePosix = TrackerSettings::Instance().SoundBoostedThreadRealtimePosix;
	appInfo.BoostedThreadNicenessPosix = TrackerSettings::Instance().SoundBoostedThreadNicenessPosix;
	appInfo.BoostedThreadRtprioPosix = TrackerSettings::Instance().SoundBoostedThreadRtprioPosix;
	appInfo.MaskDriverCrashes = TrackerSettings::Instance().SoundMaskDriverCrashes;
	appInfo.AllowDeferredProcessing = TrackerSettings::Instance().SoundAllowDeferredProcessing;
	m_pSoundDevicesManager = std::make_unique<SoundDevice::Manager>(m_GlobalLogger, sysInfo, appInfo, SoundDevice::Manager::GetDefaultEnumerators());
	m_pTrackerSettings->MigrateOldSoundDeviceSettings(*m_pSoundDevicesManager);

	GetDefaultMidiMacro(m_MidiCfg);
	return TRUE;
}

int CTrackApp::ExitInstance()
{
	m_pSoundDevicesManager = nullptr;
	gpDLSBanks.clear();
	if(m_pSettings)
		m_pSettings->Flush();
	if(m_pSongSettings)
		m_pSongSettings->Flush();
	delete m_pPluginCache;
	m_pPluginCache = nullptr;
	delete m_pTrackerSettings;
	m_pTrackerSettings = nullptr;
	delete m_pDebugSettings;
	m_pDebugSettings = nullptr;
	delete m_pSettings;
	m_pSettings = nullptr;
	delete m_pSettingsIniFile;
	m_pSettingsIniFile = nullptr;
	delete m_pSongSettings;
	m_pSongSettings = nullptr;
	delete m_pSongSettingsIniFile;
	m_pSongSettingsIniFile = nullptr;
	mpt::set_global_prng(nullptr);
	mpt::set_global_random_device(nullptr);
	m_PRNG = nullptr;
	m_RD = nullptr;
	return 0;
}

// MFC framework overrides. There is no MFC message loop on Linux; the Qt
// application drives the event loop instead.
int CTrackApp::Run() { return 0; }
BOOL CTrackApp::OnIdle(LONG) { return FALSE; }
LRESULT CTrackApp::ProcessWndProcException(CException *, const MSG *) { return 0; }
CDataRecoveryHandler *CTrackApp::GetDataRecoveryHandler() { return nullptr; }
void CTrackApp::AddToRecentFileList(LPCTSTR) { }

SettingsContainer &CTrackApp::GetPluginCache()
{
	ASSERT(m_pPluginCache);
	return *m_pPluginCache;
}

SettingsContainer &CTrackApp::GetSongSettings()
{
	ASSERT(m_pSongSettings);
	return *m_pSongSettings;
}

bool CTrackApp::OpenURL(const mpt::PathString &url, const mpt::tstring &param)
{
	if(url.empty())
		return false;
	std::string cmd = "xdg-open '" + url.ToUTF8() + "' >/dev/null 2>&1 &";
	(void)param;
	return std::system(cmd.c_str()) == 0;
}

OPENMPT_NAMESPACE_END

namespace linopenmpt
{
// CTrackApp::InitInstance/ExitInstance are protected upstream; MFC calls them
// through CWinThread, and so do we.
bool StartApp()
{
	return static_cast<CWinThread &>(OPENMPT_NAMESPACE::theApp).InitInstance() != FALSE;
}
void StopApp()
{
	static_cast<CWinThread &>(OPENMPT_NAMESPACE::theApp).ExitInstance();
}
} // namespace linopenmpt

BEGIN_MESSAGE_MAP(CTrackApp, CWinApp)
END_MESSAGE_MAP()
