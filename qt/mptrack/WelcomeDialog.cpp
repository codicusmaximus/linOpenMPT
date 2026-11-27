/*
 * WelcomeDialog.cpp
 * -----------------
 * Purpose: "First run" OpenMPT welcome dialog
 * Notes  : mfcless twin of upstream's mptrack/WelcomeDialog.cpp.
 *          Draft from qt/tools/twin.py, not yet edited by hand.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "WelcomeDialog.h"
#include "mfcless/Widgets.h"
#include "CommandSet.h"
#include "InputHandler.h"
#include "Mainfrm.h"
#include "Mptrack.h"
#include "resource.h"
#include "SelectPluginDialog.h"
#include "TrackerSettings.h"
#include "UpdateCheck.h"
#include "WindowMessages.h"
#include "../common/mptStringBuffer.h"
#include "mpt/fs/fs.hpp"
#include "mpt/string/utility.hpp"


OPENMPT_NAMESPACE_BEGIN

void WelcomeDlg::RegisterMessageMap()
{
	DialogBase::RegisterMessageMap();
	AddCommand(IDC_BUTTON1, &WelcomeDlg::OnOptions);
	AddCommand(IDC_BUTTON2, &WelcomeDlg::OnScanPlugins);
}


WelcomeDlg::WelcomeDlg(QWidget *parent)
{
	Create(IDD_WECLOME, parent);
	CenterWindow(parent);
}


static mpt::PathString GetFullKeyPath(const char *keyFile)
{
	return theApp.GetInstallPkgPath() + P_("extraKeymaps/") + mpt::PathString::FromUTF8(keyFile) + P_(".mkb");
}


bool WelcomeDlg::OnInitDialog()
{
	DialogBase::OnInitDialog();

#if defined(MPT_WITH_VST) && !MPT_OS_WINDOWS
	// The usual Linux VST2 folders: $VST_PATH (a colon-separated list), then
	// the per-user and system folders, then Wine's usual Windows plugin
	// folders. The first that exists is offered.
	{
		std::vector<mpt::PathString> candidates;
		if(const char *vstPath = std::getenv("VST_PATH"); vstPath && *vstPath)
		{
			for(const auto &dir : mpt::split(std::string(vstPath), std::string(":")))
			{
				if(!dir.empty())
					candidates.push_back(mpt::PathString::FromLocale(dir));
			}
		}
		if(const char *home = std::getenv("HOME"); home && *home)
		{
			candidates.push_back(mpt::PathString::FromLocale(home) + P_("/.vst"));
			candidates.push_back(mpt::PathString::FromLocale(home) + P_("/.lxvst"));
		}
		candidates.push_back(P_("/usr/local/lib/vst"));
		candidates.push_back(P_("/usr/lib/vst"));
		candidates.push_back(P_("/usr/local/lib/lxvst"));
		candidates.push_back(P_("/usr/lib/lxvst"));
		// Then Windows plugins installed in Wine, which the Wine bridge loads.
		{
			const char *winePrefix = std::getenv("WINEPREFIX");
			const char *home = std::getenv("HOME");
			const mpt::PathString drive = (winePrefix && *winePrefix) ? mpt::PathString::FromLocale(winePrefix) + P_("/drive_c")
				: (home && *home) ? mpt::PathString::FromLocale(home) + P_("/.wine/drive_c") : mpt::PathString();
			if(!drive.empty())
			{
				candidates.push_back(drive + P_("/Program Files/Common Files/VST2"));
				candidates.push_back(drive + P_("/Program Files/VstPlugins"));
				candidates.push_back(drive + P_("/Program Files/Steinberg/VstPlugins"));
			}
		}
		for(const auto &dir : candidates)
		{
			if(mpt::native_fs{}.is_directory(dir))
			{
				m_vstPath = dir.WithTrailingSlash();
				break;
			}
		}
	}
#endif
#if defined(MPT_WITH_VST) && MPT_OS_WINDOWS
	HKEY hkEnum = NULL;
	mpt::uchar str[MAX_PATH];
	uint32 datasize = sizeof(str);
	uint32 datatype = REG_SZ;
	if(RegOpenKey(HKEY_LOCAL_MACHINE, UL_("Software\\VST"), &hkEnum) == ERROR_SUCCESS
		&& RegQueryValueEx(hkEnum, UL_("VSTPluginsPath"), 0, &datatype, reinterpret_cast<LPBYTE>(str), &datasize) == ERROR_SUCCESS)
	{
		m_vstPath = mpt::PathString::FromNative(ParseMaybeNullTerminatedStringFromBufferWithSizeInBytes<mpt::ustring>(str, datasize));
	} else if(SHGetSpecialFolderPath(0, str, CSIDL_PROGRAM_FILES, false))
	{
		m_vstPath = mpt::PathString::FromNative(ParseMaybeNullTerminatedStringFromBufferWithSizeInBytes<mpt::ustring>(str, datasize)) + P_("/Steinberg/VstPlugins/");
		if(!mpt::native_fs{}.is_directory(m_vstPath))
		{
			m_vstPath = mpt::PathString();
		}
	}
#endif
#ifdef MPT_WITH_VST
	SetItemText(IDC_EDIT2, mpt::ToUnicode(TrackerSettings::Instance().defaultArtist.Get()));
	if(!m_vstPath.empty())
	{
		SetItemText(IDC_EDIT1, m_vstPath.ToUnicode());
		if(TrackerSettings::Instance().PathPlugins.GetDefaultDir().empty())
		{
			TrackerSettings::Instance().PathPlugins.SetDefaultDir(m_vstPath);
		}
	} else
#endif // MPT_WITH_VST
	{
		SetItemText(IDC_EDIT1, UL_("No plugin path found!"));
		EnableItem(IDC_BUTTON2, false);
	}

	const char *keyFile = nullptr;
	const mpt::uchar *keyFileName = nullptr;
	const uint16 language = LOWORD(GetKeyboardLayout(0)), primaryLang = language & 0x3FF;
	CComboBox *combo = (CComboBox *)GetDlgItem(IDC_COMBO1);
	combo->AddString(UL_("OpenMPT / Chromatic (Default)"));
	combo->SetCurSel(0);
	switch(primaryLang)
	{
		case LANG_GERMAN:
			keyFile = "DE_jojo";
			keyFileName = UL_("German");
			break;
		case LANG_SPANISH:
			// Spanish latin-american keymap, so we ignore Spain.
			if(language != SUBLANG_SPANISH_MODERN && language != SUBLANG_SPANISH)
			{
				keyFile = "es-LA_mpt_(jmkz)";
				keyFileName = UL_("Spanish");
			}
			break;
		case LANG_FRENCH:
			keyFile = "FR_mpt_(legovitch)";
			keyFileName = UL_("French");
			break;
		case LANG_NORWEGIAN:
			keyFile = "NO_mpt_classic_(rakib)";
			keyFileName = UL_("Norwegian");
			break;
	}
	if(keyFile != nullptr)
	{
		if(mpt::native_fs{}.is_file(GetFullKeyPath(keyFile)))
		{
			int i = combo->AddString(UL_("OpenMPT / Chromatic (") + mpt::ustring(keyFileName) + UL_(")"));
			combo->SetItemDataPtr(i, (void *)keyFile);
			combo->SetCurSel(i);

			// As this is presented as the default, load it right now, even if the user closes the dialog through the close button
			auto cmdSet = std::make_unique<CCommandSet>();
			cmdSet->LoadFile(GetFullKeyPath(keyFile));
			CMainFrame::GetInputHandler()->SetNewCommandSet(*cmdSet);
		}
	}
	combo->SetItemDataPtr(combo->AddString(UL_("Impulse Tracker")), (void*)("US_mpt-it2_classic"));
	combo->SetItemDataPtr(combo->AddString(UL_("FastTracker 2")), (void*)("US_mpt-ft2_classic"));

	SetChecked(IDC_CHECK1, 1);
	SetChecked(IDC_CHECK3, 1);
#if defined(MPT_ENABLE_UPDATE)
	GetDlgItem(IDC_STATIC_WELCOME_STATISTICS)->SetWindowText(mpt::ToUnicode(mpt::replace(CUpdateCheck::GetStatisticsUserInformation(false), U_("\n"), U_(" "))));
#endif // MPT_ENABLE_UPDATE
	SetChecked(IDC_CHECK2, (TrackerSettings::Instance().patternFont.Get().name == PATTERNFONT_LARGE) ? 1 : 0);

	ShowWindow(true);

	return true;
}


void WelcomeDlg::OnOptions()
{
	OnOK();
	CMainFrame::GetMainFrame()->mfcless::PostCommand(this, ID_VIEW_OPTIONS);
}


void WelcomeDlg::OnScanPlugins()
{
#ifdef MPT_WITH_VST
	CSelectPluginDlg::ScanPlugins(m_vstPath, this);
#endif // MPT_WITH_VST
}


void WelcomeDlg::OnOK()
{
	DialogBase::OnOK();

#if defined(MPT_ENABLE_UPDATE)
	bool runUpdates = IsChecked(IDC_CHECK1) != 0;
	TrackerSettings::Instance().UpdateIntervalDays = (runUpdates ? 7 : -1);
	TrackerSettings::Instance().UpdateStatistics = (IsChecked(IDC_CHECK3) != 0);
	TrackerSettings::Instance().UpdateShowUpdateHint = false;
	TrackerSettings::Instance().UpdateStatisticsConsentAsked = true;

	mpt::ustring artistName;
	artistName = GetItemText(IDC_EDIT2);
	TrackerSettings::Instance().defaultArtist = mpt::ToUnicode(artistName);

#endif // MPT_ENABLE_UPDATE
	if(IsChecked(IDC_CHECK2) != 0)
	{
		FontSetting font = TrackerSettings::Instance().patternFont;
		font.name = PATTERNFONT_LARGE;
		TrackerSettings::Instance().patternFont = font;
	}

	CComboBox *combo = (CComboBox *)GetDlgItem(IDC_COMBO1);
	const char *keyFile = static_cast<char *>(combo->GetItemDataPtr(combo->GetCurSel()));
	auto cmdSet = std::make_unique<CCommandSet>();
	if(keyFile != nullptr)
		cmdSet->LoadFile(GetFullKeyPath(keyFile));
	else
		cmdSet->LoadDefaultKeymap();
	CMainFrame::GetInputHandler()->SetNewCommandSet(*cmdSet);

#if defined(MPT_ENABLE_UPDATE)
	if(runUpdates)
	{
		CUpdateCheck::DoAutoUpdateCheck();
	}
#endif // MPT_ENABLE_UPDATE
	CMainFrame::GetMainFrame()->PostMessage(WM_MOD_INVALIDATEPATTERNS, HINT_MPTOPTIONS);

	DestroyWindow();
}

void WelcomeDlg::OnCancel()
{
	DialogBase::OnCancel();
	DestroyWindow();
}

OPENMPT_NAMESPACE_END
