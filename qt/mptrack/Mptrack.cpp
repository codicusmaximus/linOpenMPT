/*
 * MPTrack.cpp
 * -----------
 * Purpose: OpenMPT core application class.
 * Notes  : mfcless twin of upstream's mptrack/Mptrack.cpp.
 *          Hand-edited from the twin.py draft.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "Mptrack.h"
#include "AboutDialog.h"
#include "AutoSaver.h"
#include "Childfrm.h"
#include "CloseMainDialog.h"
#include "DialogBase.h"
#include "ExceptionHandler.h"
#include "FileDialog.h"
#include "FolderScanner.h"
#include "Globals.h"
#include "Image.h"
#include "InputHandler.h"
#include "IPCWindow.h"
#include "Mainfrm.h"
#include "Moddoc.h"
#include "ModDocTemplate.h"
#include "MPTrackUtil.h"
#include "PlugNotFoundDlg.h"
#include "Reporting.h"
#include "resource.h"
#include "TrackerSettings.h"
#include "UpdateCheck.h"
#include "WelcomeDialog.h"
#include "../common/ComponentManager.h"
#include "../common/mptStringBuffer.h"
#include "../common/version.h"
#include "../misc/mptOS.h"
#include "../soundlib/Dlsbank.h"
#include "../soundlib/plugins/PluginManager.h"
#include "../test/test.h"
#include "mpt/arch/arch.hpp"
#include "mpt/fs/common_directories.hpp"
#include "mpt/fs/fs.hpp"
#include "mpt/io_file/outputfile.hpp"
#include "mpt/random/seed.hpp"
#include "mpt/string/utility.hpp"
#include "openmpt/sounddevice/SoundDeviceManager.hpp"

#include <thread>

#include "mfcless/ModMessage.h"
#include "mfcless/QtString.h"
#include "mfcless/Resources.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QTimer>
#include <QtCore/QStandardPaths>
#include <QtGui/QDesktopServices>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMdiArea>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QTextEdit>
#include <QtGui/QPainter>
#include <QtGui/QPaintEvent>
#include <QtCore/QUrl>
#include "mfcless/System.h"
#include "mfcless/Graphics.h"


OPENMPT_NAMESPACE_BEGIN

/////////////////////////////////////////////////////////////////////////////
// The one and only CTrackApp object

CTrackApp theApp;

const mpt::uchar *szSpecialNoteNamesMPT[] = {UL_("PCs"), UL_("PC"), UL_("~~ (Note Fade)"), UL_("^^ (Note Cut)"), UL_("== (Note Off)")};
const mpt::uchar *szSpecialNoteShortDesc[] = {UL_("Param Control (Smooth)"), UL_("Param Control"), UL_("Note Fade"), UL_("Note Cut"), UL_("Note Off")};

// Make sure that special note arrays include string for every note.
static_assert(NOTE_MAX_SPECIAL - NOTE_MIN_SPECIAL + 1 == mpt::array_size<decltype(szSpecialNoteNamesMPT)>::size);
static_assert(mpt::array_size<decltype(szSpecialNoteShortDesc)>::size == mpt::array_size<decltype(szSpecialNoteNamesMPT)>::size);

const char *szHexChar = "0123456789ABCDEF";


#ifdef MPT_WITH_ASIO
class ComponentASIO
	: public ComponentBuiltin
{
	MPT_DECLARE_COMPONENT_MEMBERS(ComponentASIO, "ASIO")
public:
	ComponentASIO() = default;
	virtual ~ComponentASIO() = default;
};
#endif // MPT_WITH_ASIO

#if defined(MPT_WITH_DIRECTSOUND)
class ComponentDirectSound 
	: public ComponentBuiltin
{
	MPT_DECLARE_COMPONENT_MEMBERS(ComponentDirectSound, "DirectSound")
public:
	ComponentDirectSound() = default;
	virtual ~ComponentDirectSound() = default;
};
#endif // MPT_WITH_DIRECTSOUND

#if defined(MPT_WITH_PORTAUDIO)
class ComponentPortAudio
	: public ComponentBuiltin
{
	MPT_DECLARE_COMPONENT_MEMBERS(ComponentPortAudio, "PortAudio")
public:
	ComponentPortAudio() = default;
	virtual ~ComponentPortAudio() = default;
};
#endif // MPT_WITH_PORTAUDIO

#if defined(MPT_WITH_PULSEAUDIO)
class ComponentPulseaudio
	: public ComponentBuiltin
{
	MPT_DECLARE_COMPONENT_MEMBERS(ComponentPulseaudio, "Pulseaudio")
public:
	ComponentPulseaudio() = default;
	virtual ~ComponentPulseaudio() = default;
};
#endif // MPT_WITH_PULSEAUDIO

#if defined(MPT_WITH_PULSEAUDIO) && defined(MPT_WITH_PULSEAUDIOSIMPLE)
class ComponentPulseaudioSimple
	: public ComponentBuiltin
{
	MPT_DECLARE_COMPONENT_MEMBERS(ComponentPulseaudioSimple, "PulseaudioSimple")
public:
	ComponentPulseaudioSimple() = default;
	virtual ~ComponentPulseaudioSimple() = default;
};
#endif // MPT_WITH_PULSEAUDIO && MPT_WITH_PULSEAUDIOSIMPLE

#if defined(MPT_WITH_RTAUDIO)
class ComponentRtAudio
	: public ComponentBuiltin
{
	MPT_DECLARE_COMPONENT_MEMBERS(ComponentRtAudio, "RtAudio")
public:
	ComponentRtAudio() = default;
	virtual ~ComponentRtAudio() = default;
};
#endif // MPT_WITH_RTAUDIO

#if MPT_OS_WINDOWS
class ComponentWaveOut
	: public ComponentBuiltin
{
	MPT_DECLARE_COMPONENT_MEMBERS(ComponentWaveOut, "WaveOut")
public:
	ComponentWaveOut() = default;
	virtual ~ComponentWaveOut() = default;
};
#endif // MPT_OS_WINDOWS

struct AllSoundDeviceComponents
{
#if defined(MPT_WITH_PULSEAUDIO) && defined(MPT_ENABLE_PULSEAUDIO_FULL)
	ComponentHandle<ComponentPulseaudio> m_Pulseaudio;
#endif // MPT_WITH_PULSEAUDIO && MPT_ENABLE_PULSEAUDIO_FULL
#if defined(MPT_WITH_PULSEAUDIO) && defined(MPT_WITH_PULSEAUDIOSIMPLE)
	ComponentHandle<ComponentPulseaudioSimple> m_PulseaudioSimple;
#endif // MPT_WITH_PULSEAUDIO && MPT_WITH_PULSEAUDIOSIMPLE
#if MPT_OS_WINDOWS
	ComponentHandle<ComponentWaveOut> m_WaveOut;
#endif // MPT_OS_WINDOWS
#if defined(MPT_WITH_DIRECTSOUND)
	ComponentHandle<ComponentDirectSound> m_DirectSound;
#endif // MPT_WITH_DIRECTSOUND
#ifdef MPT_WITH_ASIO
	ComponentHandle<ComponentASIO> m_ASIO;
#endif // MPT_WITH_ASIO
#ifdef MPT_WITH_PORTAUDIO
	ComponentHandle<ComponentPortAudio> m_PortAudio;
#endif // MPT_WITH_PORTAUDIO
#ifdef MPT_WITH_RTAUDIO
	ComponentHandle<ComponentRtAudio> m_RtAudio;
#endif // MPT_WITH_RTAUDIO
	operator SoundDevice::EnabledBackends() const
	{
		SoundDevice::EnabledBackends result;
#if defined(MPT_WITH_PULSEAUDIO) && defined(MPT_ENABLE_PULSEAUDIO_FULL)
		result.Pulseaudio = IsComponentAvailable(m_PulseAudio);
#endif // MPT_WITH_PULSEAUDIO && MPT_ENABLE_PULSEAUDIO_FULL
#if defined(MPT_WITH_PULSEAUDIO) && defined(MPT_WITH_PULSEAUDIOSIMPLE)
		result.PulseaudioSimple = IsComponentAvailable(m_PulseaudioSimple);
#endif // MPT_WITH_PULSEAUDIO && MPT_WITH_PULSEAUDIOSIMPLE
#if MPT_OS_WINDOWS
		result.WaveOut = IsComponentAvailable(m_WaveOut);
#endif // MPT_OS_WINDOWS
#if defined(MPT_WITH_DIRECTSOUND)
		result.DirectSound = IsComponentAvailable(m_DirectSound);
#endif // MPT_WITH_DIRECTSOUND
#ifdef MPT_WITH_ASIO
		result.ASIO = IsComponentAvailable(m_ASIO);
#endif // MPT_WITH_ASIO
#ifdef MPT_WITH_PORTAUDIO
		result.PortAudio = IsComponentAvailable(m_PortAudio);
#endif // MPT_WITH_PORTAUDIO
#ifdef MPT_WITH_RTAUDIO
		result.RtAudio = IsComponentAvailable(m_RtAudio);
#endif // MPT_WITH_RTAUDIO
		return result;
	}
};


void CTrackApp::OnFileCloseAll()
{
	if(!(TrackerSettings::Instance().patternSetup & PatternSetup::NoCustomCloseDialog))
	{
		// Show modified documents window
		CloseMainDialog dlg;
		if(dlg.DoModal() != IDOK)
		{
			return;
		}
	}

	for(auto &doc : GetOpenDocuments())
	{
		doc->SafeFileClose();
	}
}


void CTrackApp::OnUpdateAnyDocsOpen(mfcless::CommandState &state)
{
	state.Enable(!GetModDocTemplate()->empty());
}


int CTrackApp::GetOpenDocumentCount() const
{
	return static_cast<int>(GetModDocTemplate()->size());
}


// Retrieve a list of all open modules.
std::vector<CModDoc *> CTrackApp::GetOpenDocuments() const
{
	std::vector<CModDoc *> documents;
	if(auto *pDocTmpl = GetModDocTemplate())
	{
		documents = pDocTmpl->GetDocumentList();
	}

	return documents;
}


void CTrackApp::UpdateAllViews(UpdateHint hint, QObject *pHint)
{
	if(auto *pDocTmpl = GetModDocTemplate())
	{
		for(auto &doc : *pDocTmpl)
		{
			doc->UpdateAllViews(nullptr, hint, pHint);
		}
	}
}


void CTrackApp::PostMessageToAllViews(uint32 uMsg, uintptr_t wParam, intptr_t lParam)
{
	if(auto *pDocTmpl = GetModDocTemplate())
	{
		for(auto &doc : *pDocTmpl)
		{
			doc->PostMessageToAllViews(uMsg, wParam, lParam);
		}
	}
}


/////////////////////////////////////////////////////////////////////////////
// Command Line options

// mfcless: what CCommandLineInfo and CWinApp::ParseCommandLine did: flags
// start with '-' or '/', anything else is a file to open.
class CMPTCommandLineInfo
{
public:
	enum ShellCommand { FileNew, FileOpen, FileNothing };
	ShellCommand m_nShellCommand = FileNew;
	bool m_bShowSplash = true;
	mpt::ustring m_strFileName;

	std::vector<mpt::PathString> m_fileNames;
	bool m_noDls = false, m_noPlugins = false, m_noAssembly = false, m_noSysCheck = false, m_noWine = false,
		m_portable = false, m_noCrashHandler = false, m_debugCrashHandler = false, m_sharedInstance = false,
		m_autoPlay = false;
#ifdef ENABLE_TESTS
	bool m_noTests = false;
#endif

public:
	CMPTCommandLineInfo()
	{
		m_workingDir = mpt::PathString::FromUnicode(mfcless::FromQString(QDir::currentPath())).WithTrailingSlash();
	}

	void Parse(const QStringList &args)
	{
		for(qsizetype i = 1; i < args.size(); i++)
		{
			QString param = args[i];
			const bool isFlag = param.size() > 1 && (param[0] == QLatin1Char('-') || param[0] == QLatin1Char('/')) && !QFileInfo::exists(param);
			if(isFlag)
				param = param.mid(param.startsWith(QLatin1String("--")) ? 2 : 1);
			ParseParam(param, isFlag, i == args.size() - 1);
		}
	}

	void ParseParam(const QString &param, bool isFlag, bool /*isLast*/)
	{
		const auto is = [&param](const char *name) { return !param.compare(QLatin1String(name), Qt::CaseInsensitive); };
		if(isFlag)
		{
			if(is("nologo")) { m_bShowSplash = false; return; }
			if(is("nodls")) { m_noDls = true; return; }
			if(is("noplugs")) { m_noPlugins = true; return; }
			if(is("portable")) { m_portable = true; return; }
			if(is("fullMemDump")) { ExceptionHandler::fullMemDump = true; return; }
			if(is("noAssembly")) { m_noAssembly = true; return; }
			if(is("noSysCheck")) { m_noSysCheck = true; return; }
			if(is("noWine")) { m_noWine = true; return; }
			if(is("noCrashHandler")) { m_noCrashHandler = true; return; }
			if(is("DebugCrashHandler")) { m_debugCrashHandler = true; return; }
			if(is("shared")) { m_sharedInstance = true; return; }
			if(is("play")) { m_autoPlay = true; return; }
#ifdef ENABLE_TESTS
			if(is("noTests")) { m_noTests = true; return; }
#endif
		} else
		{
			m_fileNames.push_back(mpt::RelativePathToAbsolute(mpt::PathString::FromUnicode(mfcless::FromQString(param)), m_workingDir));
			if(m_nShellCommand == FileNew) m_nShellCommand = FileOpen;
			if(m_strFileName.empty())
				m_strFileName = mfcless::FromQString(param);
		}
	}

protected:
	mpt::PathString m_workingDir;
};


// Splash Screen

static void StartSplashScreen();
static void StopSplashScreen();
static void TimeoutSplashScreen();


/////////////////////////////////////////////////////////////////////////////
// Midi Library

MidiLibrary CTrackApp::midiLibrary;

void CTrackApp::ImportMidiConfig(const mpt::PathString &filename, bool hideWarning)
{
	if(filename.empty()) return;

	if(CDLSBank::IsDLSBank(filename))
	{
		ConfirmAnswer result = cnfYes;
		if(!hideWarning)
		{
			result = Reporting::Confirm("You are about to replace the current MIDI library:\n"
				"Do you want to replace only the missing instruments? (recommended)",
				"Warning", true);
		}
		if(result == cnfCancel) return;
		const bool replaceAll = (result == cnfNo);
		CDLSBank dlsbank;
		if (dlsbank.Open(filename))
		{
			for(uint32 ins = 0; ins < 256; ins++)
			{
				if(replaceAll || !midiLibrary[ins] || midiLibrary[ins]->empty())
				{
					uint32 prog = (ins < 128) ? ins : 0xFF;
					uint32 key = (ins < 128) ? 0xFF : ins & 0x7F;
					if(dlsbank.FindInstrument(ins >= 128, 0xFFFF, prog, key))
					{
						midiLibrary[ins] = filename;
					}
				}
			}
		}
		return;
	}

	IniFileSettingsContainer file(filename);
	ImportMidiConfig(file, filename.GetDirectoryWithDrive());
}


static mpt::PathString GetUltraSoundPatchDir(SettingsContainer &file, const mpt::ustring &iniSection, const mpt::PathString &path, bool forgetSettings)
{
	mpt::PathString patchDir = file.Read<mpt::PathString>(iniSection, U_("PatchDir"), {});
	if(forgetSettings)
		file.Forget(U_("Ultrasound"), U_("PatchDir"));
	if(patchDir.empty() || patchDir == P_("./"))
		patchDir = path;
	if(!patchDir.empty())
		patchDir = patchDir.WithTrailingSlash();
	return patchDir;
}

void CTrackApp::ImportMidiConfig(SettingsContainer &file, const mpt::PathString &path, bool forgetSettings)
{
	const mpt::PathString patchDir = GetUltraSoundPatchDir(file, U_("Ultrasound"), path, forgetSettings);
	for(uint32 prog = 0; prog < 256; prog++)
	{
		mpt::ustring key = MPT_UFORMAT("{}{}")((prog < 128) ? U_("Midi") : U_("Perc"), prog & 0x7F);
		mpt::PathString filename = file.Read<mpt::PathString>(U_("Midi Library"), key, mpt::PathString());
		// Check for ULTRASND.INI
		if(filename.empty())
		{
			mpt::ustring section = (prog < 128) ? UL_("Melodic Patches") : UL_("Drum Patches");
			key = mpt::ufmt::val(prog & 0x7f);
			filename = file.Read<mpt::PathString>(section, key, mpt::PathString());
			if(forgetSettings) file.Forget(section, key);
			if(filename.empty())
			{
				section = (prog < 128) ? UL_("Melodic Bank 0") : UL_("Drum Bank 0");
				filename = file.Read<mpt::PathString>(section, key, mpt::PathString());
				if(forgetSettings) file.Forget(section, key);
			}
			const mpt::PathString localPatchDir = GetUltraSoundPatchDir(file, section, patchDir, forgetSettings);
			if(!filename.empty())
			{
				filename = localPatchDir + filename + P_(".pat");
			}
		}

		if(filename == P_("*empty"))
			midiLibrary[prog] = P_("");
		else if(!filename.empty())
			midiLibrary[prog] = theApp.PathInstallRelativeToAbsolute(filename);
	}
}


void CTrackApp::ExportMidiConfig(const mpt::PathString &filename)
{
	if(filename.empty())
		return;
	IniFileSettingsContainer file(filename);
	ExportMidiConfig(file);
}

void CTrackApp::ExportMidiConfig(SettingsContainer &file)
{
	for(uint32 prog = 0; prog < 256; prog++)
	{
		const mpt::ustring key = MPT_UFORMAT("{}{}")((prog < 128) ? U_("Midi") : U_("Perc"), prog & 0x7F);
		if(!midiLibrary[prog])
		{
			file.Forget(U_("Midi Library"), key);
			continue;
		}

		mpt::PathString fileName = *midiLibrary[prog];
		if(midiLibrary[prog]->empty())
			fileName = P_("*empty");
		else if(theApp.IsPortableMode())
			fileName = theApp.PathAbsoluteToInstallRelative(fileName);
		file.Write<mpt::PathString>(U_("Midi Library"), key, fileName);
	}
}


/////////////////////////////////////////////////////////////////////////////
// DLS Banks support

std::vector<std::unique_ptr<CDLSBank>> CTrackApp::gpDLSBanks;


struct CompareLessPathStringNoCase
{
	inline bool operator()(const mpt::PathString &l, const mpt::PathString &r) const
	{
		return mpt::PathCompareNoCase(l, r) < 0;
	}
};

std::future<std::vector<std::unique_ptr<CDLSBank>>> CTrackApp::LoadDefaultDLSBanks()
{
	std::set<mpt::PathString, CompareLessPathStringNoCase> paths;

	uint32 numBanks = theApp.GetSettings().Read<uint32>(U_("DLS Banks"), U_("NumBanks"), 0);
	for(uint32 i = 0; i < numBanks; i++)
	{
		mpt::PathString path = theApp.GetSettings().Read<mpt::PathString>(U_("DLS Banks"), MPT_UFORMAT("Bank{}")(i + 1), mpt::PathString());
		paths.insert(theApp.PathInstallRelativeToAbsolute(path));
	}

	// mfcless: Windows' DirectMusic GM bank (from the registry) does not exist here.

	if(paths.empty())
		return {};

	return std::async(std::launch::async, [paths = std::move(paths)]()
	{
		std::vector<std::unique_ptr<CDLSBank>> banks;
		banks.reserve(paths.size());
		for(const auto &filename : paths)
		{
			if(filename.empty() || !CDLSBank::IsDLSBank(filename))
				continue;
			try
			{
				auto bank = std::make_unique<CDLSBank>();
				if(bank->Open(filename))
				{
					banks.push_back(std::move(bank));
					continue;
				}
			} catch(mpt::out_of_memory e)
			{
				mpt::delete_out_of_memory(e);
			} catch(const std::exception &)
			{
			}
		}
		// Avoid the overhead of future::wait_for(0) until future::is_ready is finally non-experimental
		theApp.m_scannedDlsBanksAvailable = true;
		return banks;
	});
}


void CTrackApp::SaveDefaultDLSBanks()
{
	uint32 nBanks = 0;
	for(const auto &bank : gpDLSBanks)
	{
		if(!bank || bank->GetFileName().empty())
			continue;

		mpt::PathString path = bank->GetFileName();
		if(theApp.IsPortableMode())
		{
			path = theApp.PathAbsoluteToInstallRelative(path);
		}

		mpt::ustring key = MPT_UFORMAT("Bank{}")(nBanks + 1);
		theApp.GetSettings().Write<mpt::PathString>(U_("DLS Banks"), key, path);
		nBanks++;

	}
	theApp.GetSettings().Write<uint32>(U_("DLS Banks"), U_("NumBanks"), nBanks);
}


void CTrackApp::RemoveDLSBank(uint32 nBank)
{
	if(nBank < gpDLSBanks.size())
		gpDLSBanks[nBank] = nullptr;
}


bool CTrackApp::AddDLSBank(const mpt::PathString &filename)
{
	if(filename.empty() || !CDLSBank::IsDLSBank(filename)) return false;
	// Check for dupes
	for(const auto &bank : gpDLSBanks)
	{
		if(bank && !mpt::PathCompareNoCase(filename, bank->GetFileName()))
			return true;
	}
	try
	{
		auto bank = std::make_unique<CDLSBank>();
		if(bank->Open(filename))
		{
			gpDLSBanks.push_back(std::move(bank));
			return true;
		}
	} catch(mpt::out_of_memory e)
	{
		mpt::delete_out_of_memory(e);
	} catch(const std::exception &)
	{
	}
	return false;
}


size_t CTrackApp::AddScannedDLSBanks()
{
	if(!m_scannedDlsBanks.valid())
		return 0;

	size_t numAdded = 0;
	auto scannedBanks = m_scannedDlsBanks.get();
	gpDLSBanks.reserve(gpDLSBanks.size() + scannedBanks.size());
	const size_t existingBanks = gpDLSBanks.size();
	for(auto &bank : scannedBanks)
	{
		if(std::find_if(gpDLSBanks.begin(), gpDLSBanks.begin() + existingBanks, [&bank](const auto &other) { return other && *bank == *other; }) == gpDLSBanks.begin() + existingBanks)
		{
			gpDLSBanks.push_back(std::move(bank));
			numAdded++;
		}
	}
	return numAdded;
}


/////////////////////////////////////////////////////////////////////////////
// CTrackApp

MODTYPE CTrackApp::m_nDefaultDocType = MOD_TYPE_IT;

void CTrackApp::RegisterMessageMap()
{
	AddCommand(ID_FILE_NEW, &CTrackApp::OnFileNew);
	AddCommand(ID_FILE_NEWMOD, &CTrackApp::OnFileNewMOD_Amiga);
	AddCommand(ID_FILE_NEWMOD_PC, &CTrackApp::OnFileNewMOD_PC);
	AddCommand(ID_FILE_NEWS3M, &CTrackApp::OnFileNewS3M);
	AddCommand(ID_FILE_NEWXM, &CTrackApp::OnFileNewXM);
	AddCommand(ID_FILE_NEWIT, &CTrackApp::OnFileNewIT);
	AddCommand(ID_NEW_MPT, &CTrackApp::OnFileNewMPT);
	AddCommand(ID_FILE_OPEN, &CTrackApp::OnFileOpen);
	AddCommand(ID_FILE_CLOSEALL, &CTrackApp::OnFileCloseAll);
	AddCommand(ID_APP_ABOUT, &CTrackApp::OnAppAbout);
	AddCommandUpdate(ID_FILE_CLOSEALL, &CTrackApp::OnUpdateAnyDocsOpen);
}

/////////////////////////////////////////////////////////////////////////////
// CTrackApp construction

CTrackApp::CTrackApp()
{
	// mfcless: Windows' Restart Manager has no equivalent here; a crash
	// rescue saves modified modules instead (ExceptionHandler).
}


CTrackApp::~CTrackApp()
{
#if !defined(MPT_LIBCXX_QUIRK_NO_CHRONO) && !defined(MPT_LIBCXX_QUIRK_NO_CHRONO_DATE) && defined(MPT_LIBCXX_QUIRK_CHRONO_TZ_MEMLEAK)
	// Work-around memleak (see <https://github.com/microsoft/STL/issues/2504#issuecomment-1068008937>)
	try
	{
#if defined(MODPLUG_TRACKER)
		if(mpt::OS::Windows::IsWine())
		{
			// <https://bugs.openmpt.org/view.php?id=1933>
			throw std::runtime_error("zoned_time is blacklisted on Wine");
		}
#endif
		std::chrono::get_tzdb_list().~tzdb_list();
	} catch(const std::exception &)
	{
		// nothing
	} catch(...)
	{
		// nothing
	}
#endif
}


CModDoc *CTrackApp::OpenTemplateFile(const mpt::PathString &file) const
{
	return GetModDocTemplate()->OpenTemplateFile(file);
}


void CTrackApp::AddToRecentFileList(const mpt::PathString &path)
{
	RemoveMruItem(path);
	TrackerSettings::Instance().mruFiles.insert(TrackerSettings::Instance().mruFiles.begin(), path);
	if(TrackerSettings::Instance().mruFiles.size() > TrackerSettings::Instance().mruListLength)
	{
		TrackerSettings::Instance().mruFiles.resize(TrackerSettings::Instance().mruListLength);
	}
	CMainFrame::GetMainFrame()->UpdateMRUList();
}


void CTrackApp::RemoveMruItem(const size_t item)
{
	if(item < TrackerSettings::Instance().mruFiles.size())
	{
		TrackerSettings::Instance().mruFiles.erase(TrackerSettings::Instance().mruFiles.begin() + item);
		CMainFrame::GetMainFrame()->UpdateMRUList();
	}
}


void CTrackApp::RemoveMruItem(const mpt::PathString &path)
{
	auto &mruFiles = TrackerSettings::Instance().mruFiles;
	for(auto i = mruFiles.begin(); i != mruFiles.end(); i++)
	{
		if(!mpt::PathCompareNoCase(*i, path))
		{
			mruFiles.erase(i);
			break;
		}
	}
}


/////////////////////////////////////////////////////////////////////////////
// CTrackApp initialization


namespace Tracker
{
mpt::recursive_mutex_with_lock_count & GetGlobalMutexRef()
{
	return theApp.GetGlobalMutexRef();
}
} // namespace Tracker


class ComponentManagerSettings
	: public IComponentManagerSettings
{
private:
	TrackerSettings &conf;
	mpt::PathString configPath;
public:
	ComponentManagerSettings(TrackerSettings &conf, const mpt::PathString &configPath)
		: conf(conf)
		, configPath(configPath)
	{
		return;
	}
	bool LoadOnStartup() const override
	{
		return conf.ComponentsLoadOnStartup;
	}
	bool KeepLoaded() const override
	{
		return conf.ComponentsKeepLoaded;
	}
	bool IsBlocked(const std::string &key) const override
	{
		return conf.IsComponentBlocked(key);
	}
};


SettingsContainer &CTrackApp::GetPluginCache()
{
	MPT_ASSERT(m_pPluginCache);
	return *m_pPluginCache;
}


SettingsContainer &CTrackApp::GetSongSettings()
{
	MPT_ASSERT(m_pSongSettings);
	return *m_pSongSettings;
}


const mpt::PathString &CTrackApp::GetSongSettingsFilename() const
{
	return m_pSongSettingsIniFile->GetFilename();
}


// Move a config file called fileName from the App's directory (or one of its sub directories specified by subDir) to
// %APPDATA%. If specified, it will be renamed to newFileName. Existing files are never overwritten.
// Returns true on success.
bool CTrackApp::MoveConfigFile(const mpt::PathString &fileName, mpt::PathString subDir, mpt::PathString newFileName)
{
	const mpt::PathString oldPath = GetInstallPath() + subDir + fileName;
	mpt::PathString newPath = GetConfigPath() + subDir;
	if(!newFileName.empty())
		newPath += newFileName;
	else
		newPath += fileName;

	if(!mpt::native_fs{}.is_file(newPath) && mpt::native_fs{}.is_file(oldPath))
	{
		return QFile::rename(mfcless::ToQString(oldPath.ToUnicode()), mfcless::ToQString(newPath.ToUnicode()));
	}
	return false;
}


// Set up paths were configuration data is written to. Set overridePortable to true if application's own directory should always be used.
void CTrackApp::SetupPaths(bool overridePortable)
{

	// First, determine if the executable is installed in multi-arch mode or in the old standard mode.
	bool modeMultiArch = false;
	bool modeSourceProject = false;
	const mpt::PathString exePath = mpt::common_directories::get_application_directory();
	auto exePathComponents = mpt::split(exePath.GetDirectory().WithoutTrailingSlash().ToUnicode(), P_("/").ToUnicode());
	if(exePathComponents.size() >= 2)
	{
		if(exePathComponents[exePathComponents.size()-1] == mpt::OS::Windows::Name(mpt::OS::Windows::GetProcessArchitecture()))
		{
			if(exePathComponents[exePathComponents.size()-2] == U_("bin"))
			{
				modeMultiArch = true;
			}
		}
	}
	// Check if we are running from the source tree.
	if(!modeMultiArch && exePathComponents.size() >= 4)
	{
		if(exePathComponents[exePathComponents.size()-1] == mpt::OS::Windows::Name(mpt::OS::Windows::GetProcessArchitecture()))
		{
			if(exePathComponents[exePathComponents.size()-4] == U_("bin"))
			{
				modeSourceProject = true;
			}
		}
	}
	if(modeSourceProject)
	{
		mpt::native_fs fs;
		m_InstallPath = fs.absolute(exePath + P_("../") + P_("../") + P_("../") + P_("../"));
		m_InstallBinPath = fs.absolute(exePath + P_("../"));
		m_InstallBinArchPath = exePath;
		m_InstallPkgPath = fs.absolute(exePath + P_("../") + P_("../") + P_("../") + P_("../packageTemplate/"));
	} else if(modeMultiArch)
	{
		mpt::native_fs fs;
		m_InstallPath = fs.absolute(exePath + P_("../") + P_("../"));
		m_InstallBinPath = fs.absolute(exePath + P_("../"));
		m_InstallBinArchPath = exePath;
		m_InstallPkgPath = fs.absolute(exePath + P_("../") + P_("../"));
	} else
	{
		m_InstallPath = exePath;
		m_InstallBinPath = exePath;
		m_InstallBinArchPath = exePath;
		m_InstallPkgPath = exePath;
	}

	// Determine paths, portable mode, first run. Do not yet update any state.
	mpt::PathString configPathPortable = (modeSourceProject ? exePath : m_InstallPath); // config path in portable mode
	mpt::PathString configPathUser; // config path in default non-portable mode
	{
		// Try to find a nice directory where we should store our settings (default: %APPDATA%)
		// mfcless: $XDG_CONFIG_HOME (~/.config), as the MFC build on Linux has it.
		QString dir = qEnvironmentVariable("XDG_CONFIG_HOME");
		if(dir.isEmpty() && !QDir::homePath().isEmpty())
			dir = QDir::homePath() + QStringLiteral("/.config");
		if(!dir.isEmpty())
		{
			QDir().mkpath(dir);
			// Store our app settings in %APPDATA% or "My Documents"
			configPathUser = mpt::PathString::FromUnicode(mfcless::FromQString(dir)) + P_("/OpenMPT/");
		}
	}

	// Check if the user has configured portable mode.
	bool configInstallPortable = false;
	mpt::PathString portableFlagFilename = (configPathPortable + P_("OpenMPT.portable"));
	bool configPortableFlag = mpt::native_fs{}.is_file(portableFlagFilename);
	configInstallPortable = configInstallPortable || configPortableFlag;
	// before 1.29.00.13:
	if(mpt::native_fs{}.is_file(configPathPortable + P_("mptrack.ini")))
	{
		IniFileSettingsBackend oldIni(configPathPortable + P_("mptrack.ini"));
		configInstallPortable = configInstallPortable || (oldIni.ReadSetting({U_("Paths"), U_("UseAppDataDirectory")}, SettingValue(int32(1))).as<int32>() == 0);
	}
	// convert to new style
	if(configInstallPortable && !configPortableFlag)
	{
		mpt::IO::SafeOutputFile f(portableFlagFilename);
	}

	// Determine portable mode.
	bool portableMode = overridePortable || configInstallPortable || configPathUser.empty();

	// Update config dir
	m_ConfigPath = portableMode ? configPathPortable : configPathUser;

	// Set up default file locations
	m_szConfigFileName = m_ConfigPath + P_("mptrack.ini"); // config file
	m_szPluginCacheFileName = m_ConfigPath + P_("plugin.cache"); // plugin cache

	m_bInstallerMode = !modeSourceProject && !portableMode;
	m_bPortableMode = portableMode;
	m_bSourceTreeMode = modeSourceProject;

}


void CTrackApp::CreatePaths()
{
	// Create missing directories
	if(!IsPortableMode())
	{
		if(!mpt::native_fs{}.is_directory(m_ConfigPath))
		{
			QDir().mkpath(mfcless::ToQString(m_ConfigPath.ToUnicode()));
		}
	}

	// Handle updates from old versions.
	if(!IsPortableMode())
	{
		// Move the config files if they're still in the old place.
		MoveConfigFile(P_("mptrack.ini"));
		MoveConfigFile(P_("plugin.cache"));
	
		// Import old tunings
		const mpt::PathString oldTunings = GetInstallPath() + P_("tunings/");

		if(mpt::native_fs{}.is_directory(oldTunings))
		{
			FolderScanner scanner{oldTunings, FolderScanner::kOnlyFiles};
			mpt::PathString fileName;
			while(scanner.Next(fileName))
			{
				MoveConfigFile(fileName.GetFilename(), P_("tunings/"));
			}
			QDir().rmdir(mfcless::ToQString(oldTunings.ToUnicode()));
		}
	}
}


mpt::PathString CTrackApp::GetUserTemplatesPath() const
{
	return GetConfigPath() + P_("TemplateModules/");
}


mpt::PathString CTrackApp::GetExampleSongsPath() const
{
	return GetInstallPath() + P_("ExampleSongs/");
}


#if !defined(MPT_BUILD_RETRO)

static bool ProcessorCanRunCurrentBuild()
{
#ifdef MPT_ENABLE_ARCH_INTRINSICS
	if(!mpt::arch::get_cpu_info().has_features(mpt::arch::current::assumed_features()))
	{
		return false;
	}
#endif // MPT_ENABLE_ARCH_INTRINSICS
	return true;
}

static bool SystemCanRunCurrentBuild()
{
	if(mpt::OS::Windows::IsOriginal())
	{
		if(mpt::osinfo::windows::Version::Current().IsBefore(mpt::OS::Windows::Version::GetMinimumKernelLevel()))
		{
			return false;
		}
		if(mpt::osinfo::windows::Version::Current().IsBefore(mpt::OS::Windows::Version::GetMinimumAPILevel()))
		{
			return false;
		}
	} else if(mpt::OS::Windows::IsWine() && theApp.GetWineVersion()->Version().IsValid())
	{
		if(theApp.GetWineVersion()->Version().IsBefore(mpt::OS::Wine::GetMinimumWineVersion()))
		{
			return false;
		}
	}
	return true;
}

bool CTrackApp::CheckSystemSupport()
{
	const mpt::ustring lf = U_("\n");
	const mpt::ustring url = Build::GetURL(Build::Url::Download);
	if(!ProcessorCanRunCurrentBuild())
	{
		mpt::ustring text;
		text += U_("Your CPU is too old to run this variant of OpenMPT.") + lf;
		text += U_("OpenMPT will exit now.") + lf;
		Reporting::Error(text, "OpenMPT");
		return false;
	}
	if(!SystemCanRunCurrentBuild())
	{
		if(mpt::OS::Windows::IsOriginal())
		{
			mpt::ustring text;
			text += U_("Your system does not meet the minimum requirements for this variant of OpenMPT.") + lf;
			text += U_("OpenMPT will exit now.") + lf;
			Reporting::Error(text, "OpenMPT");
			return false;
		} else
		{
			mpt::ustring text;
			text += U_("Your system does not meet the minimum requirements for this variant of OpenMPT.") + lf;
			Reporting::Error(text, "OpenMPT");
			// may work though
		}
	}
	return true;
}

#endif // !MPT_BUILD_RETRO


// mfcless: Windows' IsDebuggerPresent (a tracer is attached).
static bool IsDebuggerPresent()
{
	QFile status(QStringLiteral("/proc/self/status"));
	if(!status.open(QIODevice::ReadOnly))
		return false;
	for(const QByteArray &line : status.readAll().split('\n'))
	{
		if(line.startsWith("TracerPid:"))
			return line.mid(10).trimmed().toInt() != 0;
	}
	return false;
}


bool CTrackApp::InitInstanceEarly(CMPTCommandLineInfo &cmdInfo)
{
	// The first step of InitInstance, always executed without any crash handler.

	// Avoid e.g. audio APIs trying to load wdmaud.drv from arbitrary working directory
	// mfcless: as upstream (and the MFC build), after the command line's
	// working directory is taken.
	CMPTCommandLineInfo parsed;
	cmdInfo = std::move(parsed);
	QDir::setCurrent(mfcless::ToQString(mpt::common_directories::get_application_directory().ToUnicode()));

	// Parse command line for standard shell commands, DDE, file open
	cmdInfo.Parse(QCoreApplication::arguments());

	// Set up paths to store configuration in
	SetupPaths(cmdInfo.m_portable);

	if(cmdInfo.m_sharedInstance && IPCWindow::SendToIPC(cmdInfo.m_fileNames, cmdInfo.m_autoPlay))
	{
		std::exit(0);
	}

	// Initialize DocManager (for DDE)
	// requires mpt::PathString
	MPT_ASSERT(nullptr == m_pDocManager); // no MPT_ASSERT here!
	m_pDocManager = new CModDocManager();

	if(IsDebuggerPresent() && cmdInfo.m_debugCrashHandler)
	{
		ExceptionHandler::useAnyCrashHandler = true;
		ExceptionHandler::useImplicitFallbackSEH = false;
		ExceptionHandler::useExplicitSEH = true;
		ExceptionHandler::handleStdTerminate = true;
		ExceptionHandler::handleMfcExceptions = true;
		ExceptionHandler::debugExceptionHandler = true;
	} else if(IsDebuggerPresent() || cmdInfo.m_noCrashHandler)
	{
		ExceptionHandler::useAnyCrashHandler = false;
		ExceptionHandler::useImplicitFallbackSEH = false;
		ExceptionHandler::useExplicitSEH = false;
		ExceptionHandler::handleStdTerminate = false;
		ExceptionHandler::handleMfcExceptions = false;
		ExceptionHandler::debugExceptionHandler = false;
	} else
	{
		ExceptionHandler::useAnyCrashHandler = true;
		ExceptionHandler::useImplicitFallbackSEH = true;
		ExceptionHandler::useExplicitSEH = true;
		ExceptionHandler::handleStdTerminate = true;
		ExceptionHandler::handleMfcExceptions = true;
		ExceptionHandler::debugExceptionHandler = false;
	}

	return true;
}


bool CTrackApp::InitInstanceImpl(CMPTCommandLineInfo &cmdInfo)
{

	m_GuiThreadId = std::this_thread::get_id();

	mpt::log::Trace::SetThreadId(mpt::log::Trace::ThreadKindGUI, mfcless::CurrentThreadId());

	if(ExceptionHandler::useAnyCrashHandler)
	{
		ExceptionHandler::Register();
	}

	// Start loading
	QApplication::setOverrideCursor(Qt::WaitCursor);

	MPT_LOG_GLOBAL(LogInformation, "", U_("OpenMPT Start"));

	// create the tracker-global random device
	m_RD = std::make_unique<mpt::random_device>();
	// make the device available to non-tracker-only code
	mpt::set_global_random_device(m_RD.get());
	// create and seed the traker-global best PRNG with the random device
	m_PRNG = std::make_unique<mpt::thread_safe_prng<mpt::default_prng> >(mpt::make_prng<mpt::default_prng>(RandomDevice()));
	// make the best PRNG available to non-tracker-only code
	mpt::set_global_prng(m_PRNG.get());
	// additionally, seed the C rand() PRNG, just in case any third party library calls rand()
	mpt::crand::reseed(RandomDevice());

	#ifdef MPT_ENABLE_ARCH_INTRINSICS
		if(!cmdInfo.m_noAssembly)
		{
			CPU::EnableAvailableFeatures();
		}
	#endif // MPT_ENABLE_ARCH_INTRINSICS

	// Create paths to store configuration in
	CreatePaths();

	m_pSettingsIniFile = new IniFileSettingsBackend(m_szConfigFileName);
	m_pSettings = new SettingsContainer(m_pSettingsIniFile);

	m_pDebugSettings = new DebugSettings(*m_pSettings);

	m_pTrackerSettings = new TrackerSettings(*m_pSettings);

	MPT_LOG_GLOBAL(LogInformation, "", U_("OpenMPT settings initialized."));

	if(ExceptionHandler::useAnyCrashHandler)
	{
		ExceptionHandler::ConfigureSystemHandler();
	}

	if(TrackerSettings::Instance().MiscUseSingleInstance && IPCWindow::SendToIPC(cmdInfo.m_fileNames, cmdInfo.m_autoPlay))
	{
		std::exit(0);
	}

	IPCWindow::Open();

	m_pSongSettingsIniFile = new IniFileSettingsBackend(GetConfigPath() + P_("SongSettings.ini"));
	m_pSongSettings = new SettingsContainer(m_pSongSettingsIniFile);

	m_pComponentManagerSettings = new ComponentManagerSettings(TrackerSettings::Instance(), GetConfigPath());

	m_pPluginCache = new IniFileSettingsContainer(m_szPluginCacheFileName);

	// Load standard INI file options (without MRU)
	// requires SetupPaths+CreatePaths called
	// mfcless: MFC's own settings (the MRU list it kept) are not used.

// Set process priority class
	// mfcless: the class is a Windows one; the sound thread's priority is set by the sound device.

	// Dynamic DPI-awareness. Some users might want to disable DPI-awareness because of their DPI-unaware VST plugins.
	// mfcless: Qt scales the windows itself; the setting is kept for the MFC build.
	switch(TrackerSettings::Instance().dpiAwareness)
	{
	case DPIAwarenessMode::NoDPIAwareness:
		HighDPISupport::SetDPIAwareness(HighDPISupport::Mode::LowDpi);
		break;
	case DPIAwarenessMode::NoDPIAwarenessGDIUpscaled:
		HighDPISupport::SetDPIAwareness(HighDPISupport::Mode::LowDpiUpscaled);
		break;
	case DPIAwarenessMode::SystemDPIAware:
		HighDPISupport::SetDPIAwareness(HighDPISupport::Mode::HighDpiSystem);
		break;
	case DPIAwarenessMode::PerMonitorDPIAware:
		HighDPISupport::SetDPIAwareness(HighDPISupport::Mode::HighDpiPerMonitor);
		break;
	default:
		// Malformed entry in INI file
		TrackerSettings::Instance().dpiAwareness = DPIAwarenessMode::PerMonitorDPIAware;
		HighDPISupport::SetDPIAwareness(HighDPISupport::Mode::HighDpiPerMonitor);
		break;
	}

	// create main MDI Frame window
	CMainFrame *pMainFrame = new CMainFrame();
	m_pMainWnd = pMainFrame;

	// Show splash screen
	if(cmdInfo.m_bShowSplash && TrackerSettings::Instance().m_ShowSplashScreen)
	{
		StartSplashScreen();
	}

	// create component manager
	ComponentManager::Init(*m_pComponentManagerSettings);

	// load components
	ComponentManager::Instance()->Startup();

	// Register document templates
	m_pModTemplate = new CModDocTemplate(
		IDR_MODULETYPE,
		(&CModControlView::classCModControlView));
	m_pDocManager->AddDocTemplate(m_pModTemplate);

	// Enable DDE Execute open
	// requires m_pDocManager
	// mfcless: other instances send their files through IPCWindow.

	// mfcless: the main frame's window (CFrameWnd::LoadFrame): its menu, bars and views.
	pMainFrame->OnCreate();

	// Enable drag/drop open
	pMainFrame->DragAcceptFiles();

	// Load sound APIs
	// requires TrackerSettings
	m_pAllSoundDeviceComponents = std::make_unique<AllSoundDeviceComponents>();
	SoundDevice::SysInfo sysInfo(mpt::osinfo::get_class());
	SoundDevice::AppInfo appInfo;
	appInfo.SetName(U_("OpenMPT"));
	appInfo.BoostedThreadPriorityXP = TrackerSettings::Instance().SoundBoostedThreadPriority;
	appInfo.BoostedThreadMMCSSClassVista = TrackerSettings::Instance().SoundBoostedThreadMMCSSClass;
	appInfo.BoostedThreadRealtimePosix = TrackerSettings::Instance().SoundBoostedThreadRealtimePosix;
	appInfo.BoostedThreadNicenessPosix = TrackerSettings::Instance().SoundBoostedThreadNicenessPosix;
	appInfo.BoostedThreadRtprioPosix = TrackerSettings::Instance().SoundBoostedThreadRtprioPosix;
	appInfo.MaskDriverCrashes = TrackerSettings::Instance().SoundMaskDriverCrashes;
	appInfo.AllowDeferredProcessing = TrackerSettings::Instance().SoundAllowDeferredProcessing;
	std::vector<std::shared_ptr<SoundDevice::IDevicesEnumerator>> deviceEnumerators = SoundDevice::Manager::GetEnabledEnumerators(*m_pAllSoundDeviceComponents);
	m_pSoundDevicesManager = std::make_unique<SoundDevice::Manager>(m_GlobalLogger, sysInfo, appInfo, std::move(deviceEnumerators));
	m_pTrackerSettings->MigrateOldSoundDeviceSettings(*m_pSoundDevicesManager);

	// Set default note names
	CSoundFile::SetDefaultNoteNames();

	// Load Soundfonts and default MIDI Library
	if (!cmdInfo.m_noDls)
		m_scannedDlsBanks = LoadDefaultDLSBanks();
	// Load user-defined MIDI Library
	ImportMidiConfig(theApp.GetSettings(), {}, true);

	// Initialize Plugins
	if (!cmdInfo.m_noPlugins) InitializeDXPlugins();

	// Initialize CMainFrame
	pMainFrame->Initialize();

	// Dispatch commands specified on the command line
	// mfcless: CWinApp::ProcessShellCommand: open the files given; a new
	// document is not made when none is.
	bool shellSuccess = true;
	if(!cmdInfo.m_fileNames.empty())
	{
		shellSuccess = false;
		for(const auto &filename : cmdInfo.m_fileNames)
		{
			cmdInfo.m_strFileName = filename.ToUnicode();
			shellSuccess |= (OpenDocumentFile(filename) != nullptr);
		}
	}
	if(!shellSuccess)
	{
		QApplication::restoreOverrideCursor();
		StopSplashScreen();
		pMainFrame->DestroyWindow();
		delete pMainFrame;
		m_pMainWnd = nullptr;
		return false;
	}

	pMainFrame->UpdateDocumentCount();
	pMainFrame->show();
	pMainFrame->UpdateWindow();

	QApplication::restoreOverrideCursor();


	// Perform startup tasks.

#if !defined(MPT_BUILD_RETRO)
	// Check whether we are running the best build for the given system.
	if(!cmdInfo.m_noSysCheck)
	{
		if(!CheckSystemSupport())
		{
			StopSplashScreen();
			return false;
		}
	}
#endif // !MPT_BUILD_RETRO

	if(TrackerSettings::Instance().FirstRun)
	{
		new WelcomeDlg(m_pMainWnd);
	}

#ifdef ENABLE_TESTS
	if(!cmdInfo.m_noTests)
		Test::DoTests();
#endif

	if(TrackerSettings::Instance().m_SoundSettingsOpenDeviceAtStartup)
	{
		pMainFrame->InitPreview();
		pMainFrame->PreparePreview(NOTE_NOTECUT, 0);
		pMainFrame->PlayPreview();
	}

	if(cmdInfo.m_autoPlay)
	{
		pMainFrame->PlayMod(pMainFrame->GetActiveDoc());
	}

	if(!TrackerSettings::Instance().FirstRun)
	{
#if defined(MPT_ENABLE_UPDATE)
		if(CUpdateCheck::IsSuitableUpdateMoment())
		{
			CUpdateCheck::DoAutoUpdateCheck();
		}
#endif // MPT_ENABLE_UPDATE
	}

	return true;
}


bool CTrackApp::InitInstance()
{
	CMPTCommandLineInfo cmdInfo;
	if(!InitInstanceEarly(cmdInfo))
	{
		return false;
	}
	return InitInstanceLate(cmdInfo);
}


bool CTrackApp::InitInstanceLate(CMPTCommandLineInfo &cmdInfo)
{
	// mfcless: no structured exception handling; the crash handler catches signals.
	return InitInstanceImpl(cmdInfo);
}


int CTrackApp::Run()
{
	// mfcless: CWinApp::Run's message loop, with OnIdle when it is idle.
	QTimer idleTimer;
	int32 idleCount = 0;
	QObject::connect(&idleTimer, &QTimer::timeout, [this, &idleCount]()
	{
		if(!OnIdle(idleCount++))
			idleCount = 0;
	});
	idleTimer.start(10);
	const int result = QApplication::exec();
	idleTimer.stop();
	return result;
}


int CTrackApp::ExitInstance()
{
	return ExitInstanceImpl();
}


int CTrackApp::ExitInstanceImpl()
{
	IPCWindow::Close();

	m_pSoundDevicesManager = nullptr;
	m_pAllSoundDeviceComponents = nullptr;
	if(m_pSettings)
	{
		ExportMidiConfig(theApp.GetSettings());
	}
	AddScannedDLSBanks();
	if(m_pSettings)
	{
		SaveDefaultDLSBanks();
	}
	gpDLSBanks.clear();

	// Uninitialize Plugins
	UninitializeDXPlugins();

	ComponentManager::Release();

	delete m_pMainWnd;
	m_pMainWnd = nullptr;
	delete m_pModTemplate;
	m_pModTemplate = nullptr;
	delete m_pDocManager;
	m_pDocManager = nullptr;

	delete m_pPluginCache;
	m_pPluginCache = nullptr;
	delete m_pComponentManagerSettings;
	m_pComponentManagerSettings = nullptr;
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
	m_PRNG.reset();
	mpt::set_global_random_device(nullptr);
	m_RD.reset();

	if(ExceptionHandler::useAnyCrashHandler)
	{
		ExceptionHandler::UnconfigureSystemHandler();
		ExceptionHandler::Unregister();
	}

	return 0;
}


CModDoc *CTrackApp::OpenDocumentFile(const mpt::PathString &file, bool addToMru)
{
	// mfcless: CWinApp::OpenDocumentFile passes it to the document manager.
	return m_pDocManager ? m_pDocManager->OpenDocumentFile(file, addToMru) : nullptr;
}


////////////////////////////////////////////////////////////////////////////////
// App Messages


CModDoc *CTrackApp::NewDocument(MODTYPE newType)
{
	// Build from template
	if(newType == MOD_TYPE_NONE)
	{
		const mpt::PathString templateFile = TrackerSettings::Instance().defaultTemplateFile;
		if(TrackerSettings::Instance().defaultNewFileAction == nfDefaultTemplate && !templateFile.empty())
		{
			// Template file can be either a filename inside one of the preset and user TemplateModules folders, or a full path.
			const mpt::PathString dirs[] = { GetUserTemplatesPath(), GetInstallPath() + P_("TemplateModules/"), mpt::PathString() };
			for(const auto &dir : dirs)
			{
				if(mpt::native_fs{}.is_file(dir + templateFile))
				{
					if(CModDoc *modDoc = static_cast<CModDoc *>(m_pModTemplate->OpenTemplateFile(dir + templateFile)))
					{
						return modDoc;
					}
				}
			}
		}


		// Default module type
		newType = TrackerSettings::Instance().defaultModType;

		// Get active document to make the new module of the same type
		CModDoc *pModDoc = CMainFrame::GetMainFrame()->GetActiveDoc();
		if(pModDoc != nullptr && TrackerSettings::Instance().defaultNewFileAction == nfSameAsCurrent)
		{
			newType = pModDoc->GetSoundFile().GetBestSaveFormat();
		}
	}

	SetDefaultDocType(newType);
	return static_cast<CModDoc *>(m_pModTemplate->OpenDocumentFile(mpt::PathString()));
}


void CTrackApp::OpenModulesDialog(std::vector<mpt::PathString> &files, const mpt::PathString &overridePath)
{
	files.clear();

	static constexpr std::string_view commonExts[] = {"mod", "s3m", "xm", "it", "mptm", "mo3", "oxm", "nst", "stk", "m15", "pt36", "mid", "rmi", "smf", "wav", "mdz", "s3z", "xmz", "itz", "mdr"};
	std::string exts, extsWithoutCommon;
	for(const auto &ext : CSoundFile::GetSupportedExtensions(true))
	{
		const auto filter = std::string("*.") + ext + std::string(";");
		exts += filter;
		if(!mpt::contains(commonExts, ext))
			extsWithoutCommon += filter;
	}

	static int nFilterIndex = 0;
	FileDialog dlg = OpenFileDialog()
		.AllowMultiSelect()
		.ExtensionFilter("All Modules (*.mptm,*.mod,*.xm,*.s3m,*.it,...)|" + exts + ";mod.*"
		"|"
		"Compressed Modules (*.mdz,*.s3z,*.xmz,*.itz,*.mo3,*.oxm,...)|*.mdz;*.s3z;*.xmz;*.itz;*.mdr;*.zip;*.rar;*.lha;*.pma;*.lzs;*.gz;*.mo3;*.oxm"
		"|"
		"ProTracker Modules (*.mod,*.nst)|*.mod;mod.*;*.mdz;*.nst;*.m15;*.stk;*.pt36|"
		"Scream Tracker Modules (*.s3m,*.stm)|*.s3m;*.stm;*.s3z;*.stx|"
		"FastTracker Modules (*.xm)|*.xm;*.xmz|"
		"Impulse Tracker Modules (*.it)|*.it;*.itz|"
		"OpenMPT Modules (*.mptm)|*.mptm;*.mptmz|"
		"Other Modules (*.mtm,*.okt,*.mdl,*.669,*.far,...)|" + extsWithoutCommon + "|"
		"Wave Files (*.wav)|*.wav|"
		"MIDI Files (*.mid,*.rmi)|*.mid;*.rmi;*.smf|"
		"All Files (*.*)|*.*||")
		.WorkingDirectory(overridePath.empty() ? TrackerSettings::Instance().PathSongs.GetWorkingDir() : overridePath)
		.FilterIndex(&nFilterIndex);
	if(!dlg.Show()) return;

	if(overridePath.empty())
		TrackerSettings::Instance().PathSongs.SetWorkingDir(dlg.GetWorkingDirectory());

	files = dlg.GetFilenames();
}


void CTrackApp::OnFileOpen()
{
	FileDialog::PathList files;
	OpenModulesDialog(files);
	for(const auto &file : files)
	{
		OpenDocumentFile(file);
	}
}


// App command to run the dialog
void CTrackApp::OnAppAbout()
{
	if (CAboutDlg::instance) return;
	CAboutDlg::instance = new CAboutDlg();
	CAboutDlg::instance->Create(IDD_ABOUTBOX, m_pMainWnd);
}


/////////////////////////////////////////////////////////////////////////////
// Splash Screen

class CSplashScreen : public DialogBase
{
protected:
	QImage m_Image;

public:
	CSplashScreen() : DialogBase(IDD_SPLASHSCREEN) {}
	~CSplashScreen();
	bool OnInitDialog() override;
	void OnOK() override;
	void OnCancel() override { OnOK(); }

protected:
	void paintEvent(QPaintEvent *) override;
	void mousePressEvent(QMouseEvent *) override { OnOK(); }
};

static CSplashScreen *gpSplashScreen = nullptr;

static uint64 gSplashScreenStartTime = 0;


CSplashScreen::~CSplashScreen()
{
	gpSplashScreen = nullptr;
}


void CSplashScreen::paintEvent(QPaintEvent *)
{
	QPainter painter(this);
	painter.setRenderHint(QPainter::SmoothPixmapTransform);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.drawImage(rect(), m_Image);
}


bool CSplashScreen::OnInitDialog()
{
	DialogBase::OnInitDialog();

	m_Image = mfcless::res::Image(u"PNG", IDB_SPLASHNOFOLDFIN);
	if(m_Image.isNull())
		return false;

	// mfcless: a frameless window of half the image's size (it is drawn at
	// twice the resolution), centred where the dialog was.
	setWindowFlags(Qt::SplashScreen | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
	const QRect old = geometry();
	const int width = m_Image.width() / 2;
	const int height = m_Image.height() / 2;
	setMinimumSize(0, 0);
	setFixedSize(width, height);
	move(old.center() - QPoint(width / 2, height / 2));

	return true;
}


void CSplashScreen::OnOK()
{
	StopSplashScreen();
}


static void StartSplashScreen()
{
	if(!gpSplashScreen)
	{
		gpSplashScreen = new CSplashScreen();
		gpSplashScreen->Create(theApp.m_pMainWnd);
		gpSplashScreen->CenterWindow();
		gpSplashScreen->show();
		gpSplashScreen->repaint();
		QApplication::setOverrideCursor(Qt::WaitCursor);
		QCoreApplication::processEvents();
		gSplashScreenStartTime = Util::GetTickCount64();
	}
}


static void StopSplashScreen()
{
	if(gpSplashScreen)
	{
		QApplication::restoreOverrideCursor();
		gpSplashScreen->hide();
		delete gpSplashScreen;
		gpSplashScreen = nullptr;
	}
}


static void TimeoutSplashScreen()
{
	if(gpSplashScreen)
	{
		if(Util::GetTickCount64() - gSplashScreenStartTime > 100)
		{
			StopSplashScreen();
		}
	}
}


/////////////////////////////////////////////////////////////////////////////
// Idle-time processing

bool CTrackApp::OnIdle(int32 lCount)
{
	MPT_UNUSED(lCount);
	bool b = false;

	TimeoutSplashScreen();

	if(CMainFrame::GetMainFrame())
	{
		CMainFrame::GetMainFrame()->IdleHandlerSounddevice();

		if(m_scannedDlsBanksAvailable)
		{
			if(AddScannedDLSBanks())
				CMainFrame::GetMainFrame()->RefreshDlsBanks();
		}
	}

	// Call plugins idle routine for open editor
	if (m_pPluginManager)
	{
		uint32 curTime = mfcless::TickCount();
		//rewbs.vstCompliance: call @ 50Hz
		if (curTime - m_dwLastPluginIdleCall > 20 || curTime < m_dwLastPluginIdleCall)
		{
			m_pPluginManager->OnIdle();
			m_dwLastPluginIdleCall = curTime;
		}
	}

	return b;
}


/////////////////////////////////////////////////////////////////////////////
// DIB


mfcless::DibColor rgb2quad(uint32 c)
{
	mfcless::DibColor r;
	r.rgbBlue = mfcless::BlueOf(c);
	r.rgbGreen = mfcless::GreenOf(c);
	r.rgbRed = mfcless::RedOf(c);
	r.rgbReserved = 0;
	return r;
}


void DibBlt(mfcless::DC * hdc, int x, int y, int sizex, int sizey, int srcx, int srcy, MODPLUGDIB *lpdib)
{
	if (!lpdib) return;
	hdc->DrawDib(x,
		y,
		sizex,
		sizey,
		srcx,
		lpdib->bmiHeader.biHeight - srcy - sizey,
		0,
		lpdib->bmiHeader.biHeight,
		lpdib->lpDibBits,
		lpdib->bmiHeader,
		lpdib->bmiColors);
}


MODPLUGDIB *LoadDib(uint32 resourceID)
{
	const mfcless::res::ResourceData data = mfcless::res::FindData(mfcless::res::ResourceType::Bitmap, static_cast<uint16>(resourceID));
	if(!data || data.size < sizeof(mfcless::DibHeader) + 16 * sizeof(mfcless::DibColor))
	{
		return nullptr;
	}
	MODPLUGDIB *pmd = new MODPLUGDIB;
	std::memcpy(&pmd->bmiHeader, data.data, sizeof(mfcless::DibHeader));
	std::memcpy(pmd->bmiColors, data.data + pmd->bmiHeader.biSize, 16 * sizeof(mfcless::DibColor));
	pmd->lpDibBits = data.data + pmd->bmiHeader.biSize + 16 * sizeof(mfcless::DibColor);
	return pmd;
}

template<typename Tchar>
static void DrawButtonRectImpl(mfcless::DC * hdc, int lineWidth, QFont font, mfcless::Rect rect, const Tchar *text, bool disabled, bool pushed, uint32 textFlags, uint32 topMargin)
{
	const auto colorHighlight = mfcless::SystemColor(mfcless::SysColor::ButtonHighlight), colorShadow = mfcless::SystemColor(mfcless::SysColor::ButtonShadow);
	// mfcless: Hi-DPI buttons (DrawFrameControl) are drawn as a raised or sunken edge.
	hdc->FillSolidRect(rect, mfcless::SystemColor(mfcless::SysColor::ButtonFace));
	if(lineWidth != 1)
	{
		hdc->DrawEdge(rect, pushed ? mfcless::edge::Sunken : mfcless::edge::Raised, mfcless::edge::Rect);
	} else
	{
		hdc->Draw3dRect(rect, pushed ? colorShadow : colorHighlight, pushed ? colorHighlight : colorShadow);
	}

	if(text && text[0])
	{
		rect.DeflateRect(lineWidth, lineWidth);
		if(pushed)
		{
			rect.top += lineWidth;
			rect.left += lineWidth;
		}
		hdc->SetTextColor(mfcless::SystemColor(disabled ? mfcless::SysColor::GrayText : mfcless::SysColor::ButtonText));
		hdc->SetBkMode(mfcless::bk::Transparent);
		rect.top += topMargin;
		auto oldFont = hdc->SelectObject(font);
		hdc->DrawText(mfcless::Text(text), &rect, textFlags | mfcless::dt::SingleLine | mfcless::dt::NoPrefix);
		hdc->SelectObject(oldFont);
	}
}


void DrawButtonRect(mfcless::DC * hdc, int lineWidth, const mfcless::Rect &rect, const char *text, bool disabled, bool pushed, uint32 dwFlags, uint32 topMargin)
{
	DrawButtonRectImpl(hdc, lineWidth, CMainFrame::GetGUIFont(), rect, text, disabled, pushed, dwFlags, topMargin);
}


void DrawButtonRect(mfcless::DC * hdc, int lineWidth, const mfcless::Rect &rect, const wchar_t *text, bool disabled, bool pushed, uint32 dwFlags, uint32 topMargin)
{
	DrawButtonRectImpl(hdc, lineWidth, CMainFrame::GetGUIFont(), rect, text, disabled, pushed, dwFlags, topMargin);
}


void DrawButtonRect(mfcless::DC * hdc, int lineWidth, const QFont &font, const mfcless::Rect &rect, const char *text, bool disabled, bool pushed, uint32 dwFlags, uint32 topMargin)
{
	DrawButtonRectImpl(hdc, lineWidth, font, rect, text, disabled, pushed, dwFlags, topMargin);
}


void DrawButtonRect(mfcless::DC * hdc, int lineWidth, const QFont &font, const mfcless::Rect &rect, const wchar_t *text, bool disabled, bool pushed, uint32 dwFlags, uint32 topMargin)
{
	DrawButtonRectImpl(hdc, lineWidth, font, rect, text, disabled, pushed, dwFlags, topMargin);
}


//////////////////////////////////////////////////////////////////////////////////
// Misc functions


void ErrorBox(uint32 nStringID, QWidget *parent)
{
	mpt::ustring str = mpt::ToUnicode(mfcless::res::String(nStringID));
	const bool resourceLoaded = !str.empty();
	if(!resourceLoaded)
	{
		str = MPT_UFORMAT("Resource string {} not found.")(nStringID);
	}
	MPT_ASSERT(resourceLoaded);
	Reporting::CustomNotification(str, U_("Error!"), nfOk | nfIconError, parent);
}


mpt::ustring GetWindowTextUnicode(const QWidget &wnd)
{
	// mfcless: the text a control shows, or a window's title.
	QString text;
	if(auto *edit = qobject_cast<const QLineEdit *>(&wnd))
		text = edit->text();
	else if(auto *combo = qobject_cast<const QComboBox *>(&wnd))
		text = combo->currentText();
	else if(auto *button = qobject_cast<const QAbstractButton *>(&wnd))
		text = button->text();
	else if(auto *label = qobject_cast<const QLabel *>(&wnd))
		text = label->text();
	else if(auto *plain = qobject_cast<const QPlainTextEdit *>(&wnd))
		text = plain->toPlainText();
	else if(auto *rich = qobject_cast<const QTextEdit *>(&wnd))
		text = rich->toPlainText();
	else
		text = wnd.windowTitle();
	return mfcless::FromQString(text);
}


mpt::ustring FormatFileSize(uint64 fileSize)
{
	static constexpr std::array<const mpt::uchar *, 4> Unit = {UL_(" B"), UL_(" KB"), UL_(" MB"), UL_(" GB")};
	double size = static_cast<double>(fileSize);
	for(int i = 0; i < 4; i++)
	{
		if(size < 1024.0 || i == 3)
		{
			// Variable-length formatting may decide on a whim to switch to scientific formatting, so used a fixed width and trim manually...
			mpt::ustring s = mpt::ufmt::fix(size, 2);
			if(i == 0)
			{
				if(auto pos = s.rfind(u8'.'); pos != mpt::ustring::npos)
					s.erase(pos);
			}
			return s + Unit[i];
		}
		size /= 1024.0;
	}
	return U_("");
}


mpt::ustring FormatOrderRow(uint32 value)
{
	const bool rowAndOrderNumbersHex = TrackerSettings::Instance().patternSetup & PatternSetup::RowAndOrderNumbersHex;
	if(rowAndOrderNumbersHex)
		return mpt::ufmt::HEX0<2>(value);
	else
		return mpt::ufmt::val(value);
}


////////////////////////////////////////////////////////////////////////////////
// CFastBitmap 8-bit output / 4-bit input
// useful for lots of small blits with color mapping
// combined in one big blit

void CFastBitmap::Init(MODPLUGDIB *lpTextDib)
{
	m_nBlendOffset = 0;
	m_pTextDib = lpTextDib;
	m_Dib.bmiHeader = {};
	m_nTextColor = 0;
	m_nBkColor = 1;
	m_Dib.bmiHeader.biSize = sizeof(mfcless::DibHeader);
	m_Dib.bmiHeader.biWidth = 0;	// Set later
	m_Dib.bmiHeader.biHeight = 0;	// Ditto
	m_Dib.bmiHeader.biPlanes = 1;
	m_Dib.bmiHeader.biBitCount = 8;
	m_Dib.bmiHeader.biCompression = 0;  // BI_RGB
	m_Dib.bmiHeader.biSizeImage = 0;
	m_Dib.bmiHeader.biXPelsPerMeter = 96;
	m_Dib.bmiHeader.biYPelsPerMeter = 96;
	m_Dib.bmiHeader.biClrUsed = 0;
	m_Dib.bmiHeader.biClrImportant = 256; // MAX_MODPALETTECOLORS;
	m_n4BitPalette[0] = (uint8)m_nTextColor;
	m_n4BitPalette[4] = MODCOLOR_SEPSHADOW;
	m_n4BitPalette[12] = MODCOLOR_SEPFACE;
	m_n4BitPalette[14] = MODCOLOR_SEPHILITE;
	m_n4BitPalette[15] = (uint8)m_nBkColor;
}


void CFastBitmap::Blit(mfcless::DC * hdc, int x, int y, int cx, int cy)
{
	hdc->DrawDib(x,
		y,
		cx,
		cy,
		0,
		m_Dib.bmiHeader.biHeight - cy,
		0,
		m_Dib.bmiHeader.biHeight,
		m_Dib.DibBits.data(),
		m_Dib.bmiHeader,
		m_Dib.bmiColors);
}


void CFastBitmap::SetColor(uint32 nIndex, uint32 cr)
{
	if (nIndex < 256)
	{
		m_Dib.bmiColors[nIndex].rgbRed = mfcless::RedOf(cr);
		m_Dib.bmiColors[nIndex].rgbGreen = mfcless::GreenOf(cr);
		m_Dib.bmiColors[nIndex].rgbBlue = mfcless::BlueOf(cr);
	}
}


void CFastBitmap::SetAllColors(uint32 nBaseIndex, uint32 nColors, uint32 *pcr)
{
	for (uint32 i=0; i<nColors; i++)
	{
		SetColor(nBaseIndex+i, pcr[i]);
	}
}


void CFastBitmap::SetBlendColor(uint32 cr)
{
	uint32 r = mfcless::RedOf(cr);
	uint32 g = mfcless::GreenOf(cr);
	uint32 b = mfcless::BlueOf(cr);
	for (uint32 i=0; i<BLEND_OFFSET; i++)
	{
		uint32 m = (m_Dib.bmiColors[i].rgbRed >> 2)
				+ (m_Dib.bmiColors[i].rgbGreen >> 1)
				+ (m_Dib.bmiColors[i].rgbBlue >> 2);
		m_Dib.bmiColors[i|BLEND_OFFSET].rgbRed = static_cast<uint8>((m + r)>>1);
		m_Dib.bmiColors[i|BLEND_OFFSET].rgbGreen = static_cast<uint8>((m + g)>>1);
		m_Dib.bmiColors[i|BLEND_OFFSET].rgbBlue = static_cast<uint8>((m + b)>>1);
	}
}


// Monochrome 4-bit bitmap (0=text, !0 = back)
void CFastBitmap::TextBlt(int x, int y, int cx, int cy, int srcx, int srcy, MODPLUGDIB *lpdib)
{
	const uint8 *psrc;
	uint8 *pdest;
	uint32 x1, x2;
	int srcwidth, srcinc;

	m_n4BitPalette[0] = (uint8)m_nTextColor;
	m_n4BitPalette[15] = (uint8)m_nBkColor;
	if (x < 0)
	{
		cx += x;
		x = 0;
	}
	if (y < 0)
	{
		cy += y;
		y = 0;
	}
	if ((x >= m_Dib.bmiHeader.biWidth) || (y >= m_Dib.bmiHeader.biHeight)) return;
	if (x+cx >= m_Dib.bmiHeader.biWidth) cx = m_Dib.bmiHeader.biWidth - x;
	if (y+cy >= m_Dib.bmiHeader.biHeight) cy = m_Dib.bmiHeader.biHeight - y;
	if (!lpdib) lpdib = m_pTextDib;
	if ((cx <= 0) || (cy <= 0) || (!lpdib)) return;
	srcwidth = (lpdib->bmiHeader.biWidth+1) >> 1;
	srcinc = srcwidth;
	if (((int)lpdib->bmiHeader.biHeight) > 0)
	{
		srcy = lpdib->bmiHeader.biHeight - 1 - srcy;
		srcinc = -srcinc;
	}
	x1 = srcx & 1;
	x2 = x1 + cx;
	pdest = &m_Dib.DibBits[((m_Dib.bmiHeader.biHeight - 1 - y) << m_nXShiftFactor) + x];
	psrc = lpdib->lpDibBits + (srcx >> 1) + (srcy * srcwidth);
	for (int iy=0; iy<cy; iy++)
	{
		uint8 *p = pdest;
		uint32 ix = x1;
		if (ix&1)
		{
			uint32 b = psrc[ix >> 1];
			*p++ = m_n4BitPalette[b & 0x0F]+m_nBlendOffset;
			ix++;
		}
		while (ix+1 < x2)
		{
			uint32 b = psrc[ix >> 1];
			p[0] = m_n4BitPalette[b >> 4]+m_nBlendOffset;
			p[1] = m_n4BitPalette[b & 0x0F]+m_nBlendOffset;
			ix+=2;
			p+=2;
		}
		if (x2&1)
		{
			uint32 b = psrc[ix >> 1];
			*p++ = m_n4BitPalette[b >> 4]+m_nBlendOffset;
		}
		pdest -= m_Dib.bmiHeader.biWidth;
		psrc += srcinc;
	}
}


void CFastBitmap::SetSize(int x, int y)
{
	if(x > 4)
	{
		// Compute the required shift factor for obtaining a power-of-two bitmap width
		m_nXShiftFactor = 1;
		x--;
		while(x >>= 1)
		{
			m_nXShiftFactor++;
		}
	} else
	{
		// Bitmaps rows are aligned to 4 bytes, so let this bitmap be exactly 4 pixels wide.
		m_nXShiftFactor = 2;
	}

	x = (1 << m_nXShiftFactor);
	if(m_Dib.DibBits.size() != static_cast<size_t>(y << m_nXShiftFactor)) m_Dib.DibBits.resize(y << m_nXShiftFactor);
	m_Dib.bmiHeader.biWidth = x;
	m_Dib.bmiHeader.biHeight = y;
}


///////////////////////////////////////////////////////////////////////////////////
//
// Restore / save plugin list
//

static const auto PLUGFORMAT_FILENAME = MPT_UFORMAT("Plugin{}");
static const auto PLUGFORMAT_TAGS = MPT_UFORMAT("Plugin{}.Tags");
static const auto PLUGFORMAT_TAGS_BUILTIN = MPT_UFORMAT("Plugin{}{}.Tags");
static const auto PLUGFORMAT_LIBNAME = MPT_UFORMAT("Plugin{}.LibraryName");
static const auto PLUGFORMAT_SHELLID = MPT_UFORMAT("Plugin{}.ShellPluginID");

void CTrackApp::InitializeDXPlugins()
{
	m_pPluginManager = new CVstPluginManager;
	const size_t numPlugins = GetSettings().Read<int32>(U_("VST Plugins"), U_("NumPlugins"), 0);

	bool maskCrashes = TrackerSettings::Instance().BrokenPluginsWorkaroundVSTMaskAllCrashes;

	std::vector<VSTPluginLib *> nonFoundPlugs;
	const mpt::PathString failedPlugin = GetSettings().Read<mpt::PathString>(U_("VST Plugins"), U_("FailedPlugin"));
	ConfirmAnswer skipFailed = cnfCancel;

	DialogBase pluginScanDlg(IDD_SCANPLUGINS);
	bool scanDlgShown = false;
	QWidget *textWnd = nullptr;
	uint64 scanStart = Util::GetTickCount64();

	// Read tags for built-in plugins
	for(auto &plug : *m_pPluginManager)
	{
		plug->tags = GetSettings().Read<mpt::ustring>(U_("VST Plugins"), PLUGFORMAT_TAGS_BUILTIN(mpt::ufmt::HEX0<8>(plug->pluginId1), mpt::ufmt::HEX0<8>(plug->pluginId2)));
	}

	// Restructured plugin cache
	if(TrackerSettings::Instance().PreviousSettingsVersion < MPT_V("1.27.00.15"))
	{
		QFile::remove(mfcless::ToQString(m_szPluginCacheFileName.ToUnicode()));
		GetPluginCache().ForgetAll();
	}

	m_pPluginManager->reserve(numPlugins);
	auto scanFormat = MPT_UFORMAT("Scanning Plugin {} / {}...\n{}");
	for(size_t plug = 0; plug < numPlugins; plug++)
	{
		const mpt::PathString plugPath = PathInstallRelativeToAbsolute(GetSettings().Read<mpt::PathString>(U_("VST Plugins"), PLUGFORMAT_FILENAME(plug)));
		if(plugPath.empty())
			continue;

		if(!scanDlgShown && Util::GetTickCount64() >= scanStart + 2000)
		{
			// If this is taking too long, show the user what they're waiting for.
			scanDlgShown = true;
			pluginScanDlg.Create(IDD_SCANPLUGINS, gpSplashScreen);
			pluginScanDlg.ShowWindow(true);
			pluginScanDlg.CenterWindow(gpSplashScreen);
			textWnd = pluginScanDlg.GetDlgItem(IDC_SCANTEXT);
		} else if(scanDlgShown && Util::GetTickCount64() >= scanStart + 30)
		{
			if(auto *label = qobject_cast<QLabel *>(textWnd))
				label->setText(mfcless::ToQString(scanFormat(plug + 1, numPlugins + 1, plugPath.ToUnicode())));
			QCoreApplication::processEvents();
			scanStart = Util::GetTickCount64();
		}

		if(plugPath == failedPlugin)
		{
			GetSettings().Remove(U_("VST Plugins"), U_("FailedPlugin"));
			if(skipFailed == cnfCancel)
			{
				const mpt::ustring text = MPT_UFORMAT("The following plugin has previously crashed OpenMPT during initialisation:\n\n{}\n\nDo you still want to load it?")
					(failedPlugin.ToUnicode());
				skipFailed = Reporting::Confirm(text, false, true, &pluginScanDlg);
			}

			if(skipFailed == cnfNo)
				continue;
		}

		const uint32 shellPluginID = mpt::parse_hex<uint32>(GetSettings().Read<mpt::ustring>(U_("VST Plugins"), PLUGFORMAT_SHELLID(plug)));
		bool plugFound = true;
		for(VSTPluginLib *lib : m_pPluginManager->AddPlugin(plugPath, maskCrashes, true, &plugFound, shellPluginID))
		{
			if(lib->libraryName == P_("MIDI Input Output") && lib->pluginId1 == PLUGMAGIC('V', 's', 't', 'P') && lib->pluginId2 == PLUGMAGIC('M', 'M', 'I', 'D') && !lib->shellPluginID)
			{
				// This appears to be an old version of our MIDI I/O plugin, which is now built right into the main executable.
				m_pPluginManager->RemovePlugin(lib);
				continue;
			}
			if(!plugFound)
				nonFoundPlugs.push_back(lib);
			if(shellPluginID && lib->shellPluginID != shellPluginID)
				continue;

			lib->tags = GetSettings().Read<mpt::ustring>(U_("VST Plugins"), PLUGFORMAT_TAGS(plug));
			if(shellPluginID != 0)
			{
				if(mpt::PathString libName = GetSettings().Read<mpt::PathString>(U_("VST Plugins"), PLUGFORMAT_LIBNAME(plug)); !libName.empty())
					lib->libraryName = std::move(libName);
			}

		}
	}
	GetPluginCache().Flush();
	if(scanDlgShown)
	{
		pluginScanDlg.DestroyWindow();
	}
	if(!nonFoundPlugs.empty())
	{
		PlugNotFoundDialog(nonFoundPlugs, nullptr).DoModal();
	}
}


void CTrackApp::UninitializeDXPlugins()
{
	if(!m_pPluginManager) return;

#ifndef NO_PLUGINS

	size_t plugIndex = 0;
	for(auto &plug : *m_pPluginManager)
	{
		if(!plug->isBuiltIn)
		{
			mpt::PathString plugPath;
			if(theApp.IsPortableMode())
				plugPath = PathAbsoluteToInstallRelative(plug->dllPath);
			else
				plugPath = plug->dllPath;
			
			const auto libName = PLUGFORMAT_LIBNAME(plugIndex), shellID = PLUGFORMAT_SHELLID(plugIndex);
			if(plug->shellPluginID != 0)
			{
				theApp.GetSettings().Write(U_("VST Plugins"), libName, plug->libraryName);
				theApp.GetSettings().Write(U_("VST Plugins"), shellID, mpt::ufmt::HEX0<8>(plug->shellPluginID));
			} else
			{
				theApp.GetSettings().Remove(U_("VST Plugins"), libName);
				theApp.GetSettings().Remove(U_("VST Plugins"), shellID);
			}

			theApp.GetSettings().Write<mpt::PathString>(U_("VST Plugins"), PLUGFORMAT_FILENAME(plugIndex), plugPath);
			theApp.GetSettings().Write(U_("VST Plugins"), PLUGFORMAT_TAGS(plugIndex), plug->tags);

			plugIndex++;
		} else
		{
			theApp.GetSettings().Write(U_("VST Plugins"), PLUGFORMAT_TAGS_BUILTIN(mpt::ufmt::HEX0<8>(plug->pluginId1), mpt::ufmt::HEX0<8>(plug->pluginId2)), plug->tags);
		}
	}
	theApp.GetSettings().Write(U_("VST Plugins"), U_("NumPlugins"), static_cast<uint32>(plugIndex));
#endif // NO_PLUGINS

	delete m_pPluginManager;
	m_pPluginManager = nullptr;
}


///////////////////////////////////////////////////////////////////////////////////
// Internet-related functions

bool CTrackApp::OpenURL(const char *url)
{
	if(!url) return false;
	return OpenURL(mpt::PathString::FromUTF8(url));
}

bool CTrackApp::OpenURL(const std::string &url)
{
	return OpenURL(mpt::PathString::FromUTF8(url));
}

bool CTrackApp::OpenURL(const mpt::ustring &url)
{
	return OpenURL(mpt::PathString::FromUnicode(url));
}

bool CTrackApp::OpenURL(const mpt::PathString &lpszURL, const mpt::ustring &param)
{
	// mfcless: the desktop's handler for the URL or file (ShellExecute "open").
	MPT_UNUSED(param);
	if(lpszURL.empty())
		return false;
	const QString str = mfcless::ToQString(lpszURL.ToUnicode());
	const QUrl url = str.contains(QStringLiteral("://")) || str.startsWith(QStringLiteral("mailto:")) ? QUrl(str) : QUrl::fromLocalFile(str);
	return QDesktopServices::openUrl(url);
}

bool CTrackApp::OpenDirectory(const mpt::PathString &directory)
{
	// mfcless: for a file, its folder (there is no portable way to select it there).
	if(mpt::native_fs{}.is_file(directory))
		return OpenURL(directory.GetDirectoryWithDrive());
	else
		return OpenURL(directory);
}


mpt::ustring CTrackApp::GetResamplingModeName(ResamplingMode mode, int length, bool addTaps)
{
	mpt::ustring result;
	switch(mode)
	{
	case SRCMODE_NEAREST:
		result = (length > 1) ? U_("No Interpolation") : U_("None") ;
		break;
	case SRCMODE_LINEAR:
		result = U_("Linear");
		break;
	case SRCMODE_CUBIC:
		result = U_("Cubic");
		break;
	case SRCMODE_SINC8:
		result = U_("Sinc");
		break;
	case SRCMODE_SINC8LP:
		result = U_("Sinc");
		break;
	default:
		MPT_ASSERT_NOTREACHED();
		break;
	}
	if(Resampling::HasAA(mode))
	{
		result += (length > 1) ? U_(" + Low-Pass") : U_(" + LP");
	}
	if(addTaps)
	{
		result += MPT_UFORMAT(" ({} tap{})")(Resampling::Length(mode), (Resampling::Length(mode) != 1) ? mpt::ustring(U_("s")) : mpt::ustring(U_("")));
	}
	return result;
}


mpt::ustring CTrackApp::GetFriendlyMIDIPortName(const mpt::ustring &deviceName, bool isInputPort, bool addDeviceName)
{
	auto friendlyName = GetSettings().Read<mpt::ustring>(isInputPort ? U_("MIDI Input Ports") : U_("MIDI Output Ports"), deviceName, deviceName);
	if(friendlyName.empty())
		return deviceName;
	else if(addDeviceName && friendlyName != deviceName)
		return friendlyName + UL_(" (") + deviceName + UL_(")");
	else
		return friendlyName;
}


bool ValidateMacroString(QLineEdit &wnd, const std::string_view prevMacro, bool isParametric, bool allowVariables, bool allowMultiline)
{
	const mpt::ustring macroStrT = mfcless::FromQString(wnd.text());
	std::string macroStr = mpt::ToCharset(mpt::Charset::ASCII, macroStrT);

	bool allowed = true, caseChange = false;
	for(char &c : macroStr)
	{
		if(c >= 'G' && c <= 'Z')  // Potentially an allowed variable; lowercase it
		{
			caseChange = true;
			c = c - 'A' + 'a';
		}

		if(c == 'k')  // Previously, 'K' was used for MIDI channel
		{
			caseChange = true;
			c = 'c';
		} else if(c >= 'a' && c <= 'c' && !allowVariables)
		{
			caseChange = true;
			c = c - 'a' + 'A';
		} else if(c >= 'd' && c <= 'f')  // abc can be variables, but def can be fixed
		{
			caseChange = true;
			c = c - 'a' + 'A';
		} else if((c >= 'a' && c <= 'c') || c == 'h' || c == 'm' || c == 'n' || c == 'o' || c == 'p' || c == 's' || c == 'u' || c == 'v' || c == 'x' || c == 'y')
		{
			if(!allowVariables)
			{
				allowed = false;
				break;
			}
		} else if(c == 'z')
		{
			if(!isParametric || !allowVariables)
			{
				allowed = false;
				break;
			}
		} else if(c == '\r' || c == '\n')
		{
			if(!allowMultiline)
			{
				allowed = false;
				break;
			}
		} else if(!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || c == ' '))
		{
			allowed = false;
			break;
		}
	}

	if(!allowed)
	{
		// Replace text and keep cursor position if we just typed in an invalid character
		if(prevMacro != std::string_view{macroStr})
		{
			const int start = wnd.hasSelectedText() ? wnd.selectionStart() : wnd.cursorPosition();
			wnd.setText(QString::fromLatin1(prevMacro.data(), static_cast<qsizetype>(prevMacro.size())));
			wnd.setCursorPosition(std::max(start - 1, 0));
			QApplication::beep();
		}
		return false;
	} else
	{
		if(caseChange)
		{
			// Replace text and keep cursor position if there was a case conversion
			const int start = wnd.hasSelectedText() ? wnd.selectionStart() : wnd.cursorPosition();
			const int length = wnd.selectedText().length();
			wnd.setText(QString::fromLatin1(macroStr.data(), static_cast<qsizetype>(macroStr.size())));
			if(length)
				wnd.setSelection(start, length);
			else
				wnd.setCursorPosition(start);
		}
		return true;
	}
}


static constexpr std::pair<const mpt::uchar*, const mpt::uchar*> SampleFormats[]
{
	{ UL_("Wave Files (*.wav)"), UL_("*.wav") },
#ifdef MPT_WITH_FLAC
	{ UL_("FLAC Files (*.flac,*.oga)"), UL_("*.flac;*.oga") },
#endif  // MPT_WITH_FLAC
#if defined(MPT_WITH_OPUSFILE)
	{ UL_("Opus Files (*.opus,*.oga)"), UL_("*.opus;*.oga") },
#endif  // MPT_WITH_OPUSFILE
#if defined(MPT_WITH_VORBISFILE) || defined(MPT_WITH_STBVORBIS)
	{ UL_("Ogg Vorbis Files (*.ogg,*.oga)"), UL_("*.ogg;*.oga") },
#endif  // VORBIS
#if defined(MPT_ENABLE_MP3_SAMPLES)
	{ UL_("MPEG Files (*.mp1,*.mp2,*.mp3)"), UL_("*.mp1;*.mp2;*.mp3") },
#endif  // MPT_ENABLE_MP3_SAMPLES
	{ UL_("XI Samples (*.xi)"), UL_("*.xi") },
	{ UL_("Impulse Tracker Samples (*.its)"), UL_("*.its") },
	{ UL_("Scream Tracker Samples (*.s3i,*.smp)"), UL_("*.s3i;*.smp") },
	{ UL_("OPL Instruments (*.sb0,*.sb2,*.sbi)"), UL_("*.sb0;*.sb2;*.sbi") },
	{ UL_("GF1 Patches (*.pat)"), UL_("*.pat") },
	{ UL_("Wave64 Files (*.w64)"), UL_("*.w64") },
	{ UL_("CAF Files (*.wav)"), UL_("*.caf") },
	{ UL_("AIFF Files (*.aiff,*.8svx)"), UL_("*.aif;*.aiff;*.iff;*.8sv;*.8svx;*.svx") },
	{ UL_("Sun Audio (*.au,*.snd)"), UL_("*.au;*.snd") },
	{ UL_("SNES BRR Files (*.brr)"), UL_("*.brr") },
};


mpt::ustring ConstructSampleFormatFileFilter(bool includeRaw)
{
	mpt::ustring s = U_("All Samples (*.wav,*.flac,*.xi,*.its,*.s3i,*.sbi,...)|");
	bool first = true;
	for (const auto& [name, exts] : SampleFormats)
	{
		if (!first)
			s += U_(";");
		else
			first = false;
		s += exts;
	}
#if defined(MPT_WITH_MEDIAFOUNDATION)
	std::vector<FileType> mediaFoundationTypes = CSoundFile::GetMediaFoundationFileTypes();
	s += ToFilterOnlyString(mediaFoundationTypes, true).ToUnicode();
#endif
	if (includeRaw)
	{
		s += U_(";*.raw;*.snd;*.pcm;*.sam");
	}
	s += U_("|");
	for (const auto& [name, exts] : SampleFormats)
	{
		s += name + U_("|");
		s += exts + U_("|");
	}
#if defined(MPT_WITH_MEDIAFOUNDATION)
	s += ToFilterString(mediaFoundationTypes, FileTypeFormatShowExtensions).ToUnicode();
#endif
	if (includeRaw)
	{
		s += U_("Raw Samples (*.raw,*.snd,*.pcm,*.sam)|*.raw;*.snd;*.pcm;*.sam|");
	}
	s += U_("All Files (*.*)|*.*||");
	return s;
}



OPENMPT_NAMESPACE_END
