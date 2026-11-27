/*
 * PathConfigDlg.cpp
 * -----------------
 * Purpose: Default paths and auto save setup dialog.
 * Notes  : mfcless twin of upstream's mptrack/PathConfigDlg.cpp.
 *          Draft from qt/tools/twin.py, not yet edited by hand.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "PathConfigDlg.h"
#include "FileDialog.h"
#include "Mainfrm.h"
#include "Mptrack.h"
#include "Reporting.h"
#include "resource.h"
#include "TrackerSettings.h"
#include "mpt/fs/fs.hpp"

OPENMPT_NAMESPACE_BEGIN

static constexpr std::pair<ConfigurableDirectory TrackerSettings::*, int> PathSettings[] =
{
	{ &TrackerSettings::PathSongs, IDC_OPTIONS_DIR_MODS },
	{ &TrackerSettings::PathSamples, IDC_OPTIONS_DIR_SAMPS },
	{ &TrackerSettings::PathInstruments, IDC_OPTIONS_DIR_INSTS },
	{ &TrackerSettings::PathPlugins, IDC_OPTIONS_DIR_VSTS },
	{ &TrackerSettings::PathPluginPresets, IDC_OPTIONS_DIR_VSTPRESETS },
	{ &TrackerSettings::AutosavePath, IDC_AUTOSAVE_PATH },
};

PathConfigDlg::PathConfigDlg()
	: CPropertyPage{IDD_OPTIONS_AUTOSAVE}
{
	m_accessibleEdits[0].SetAccessibleSuffix(U_("minutes"));
	m_accessibleEdits[1].SetAccessibleSuffix(U_("backups"));
	m_accessibleEdits[2].SetAccessibleSuffix(U_("days"));
	m_browseButtons[0].SetAccessibleText(U_("Browse for song folder..."));
	m_browseButtons[1].SetAccessibleText(U_("Browse for sample folder..."));
	m_browseButtons[2].SetAccessibleText(U_("Browse for instrument folder..."));
	m_browseButtons[3].SetAccessibleText(U_("Browse for VST plugin folder..."));
	m_browseButtons[4].SetAccessibleText(U_("Browse for VST preset folder..."));
	m_browseButtons[5].SetAccessibleText(U_("Browse for auto save folder..."));
}


void PathConfigDlg::DoDataExchange(DataExchange &dx)
{
	PropertyPage::DoDataExchange(dx);
	ExchangeControl(dx, IDC_AUTOSAVE_INTERVAL, m_accessibleEdits[0]);
	ExchangeControl(dx, IDC_AUTOSAVE_HISTORY, m_accessibleEdits[1]);
	ExchangeControl(dx, IDC_EDIT1, m_accessibleEdits[2]);
	ExchangeControl(dx, IDC_BUTTON_CHANGE_MODDIR, m_browseButtons[0]);
	ExchangeControl(dx, IDC_BUTTON_CHANGE_SAMPDIR, m_browseButtons[1]);
	ExchangeControl(dx, IDC_BUTTON_CHANGE_INSTRDIR, m_browseButtons[2]);
	ExchangeControl(dx, IDC_BUTTON_CHANGE_VSTDIR, m_browseButtons[3]);
	ExchangeControl(dx, IDC_BUTTON_CHANGE_VSTPRESETSDIR, m_browseButtons[4]);
	ExchangeControl(dx, IDC_AUTOSAVE_BROWSE, m_browseButtons[5]);
}

void PathConfigDlg::RegisterMessageMap()
{
	PropertyPage::RegisterMessageMap();
	// Paths
	AddNotify(IDC_OPTIONS_DIR_MODS, mfcless::Notify::EditChange, &PathConfigDlg::OnSettingsChanged);
	AddNotify(IDC_OPTIONS_DIR_SAMPS, mfcless::Notify::EditChange, &PathConfigDlg::OnSettingsChanged);
	AddNotify(IDC_OPTIONS_DIR_INSTS, mfcless::Notify::EditChange, &PathConfigDlg::OnSettingsChanged);
	AddNotify(IDC_OPTIONS_DIR_VSTPRESETS, mfcless::Notify::EditChange, &PathConfigDlg::OnSettingsChanged);
	AddCommand(IDC_BUTTON_CHANGE_MODDIR, &PathConfigDlg::OnBrowseSongs);
	AddCommand(IDC_BUTTON_CHANGE_SAMPDIR, &PathConfigDlg::OnBrowseSamples);
	AddCommand(IDC_BUTTON_CHANGE_INSTRDIR, &PathConfigDlg::OnBrowseInstruments);
	AddCommand(IDC_BUTTON_CHANGE_VSTDIR, &PathConfigDlg::OnBrowsePlugins);
	AddCommand(IDC_BUTTON_CHANGE_VSTPRESETSDIR, &PathConfigDlg::OnBrowsePresets);
	// Autosave
	AddCommand(IDC_CHECK1, &PathConfigDlg::OnSettingsChanged);
	AddCommand(IDC_CHECK2, &PathConfigDlg::OnAutosaveRetention);
	AddCommand(IDC_CHECK3, &PathConfigDlg::OnSettingsChanged);
	AddCommand(IDC_AUTOSAVE_BROWSE, &PathConfigDlg::OnBrowseAutosavePath);
	AddCommand(IDC_AUTOSAVE_ENABLE, &PathConfigDlg::OnAutosaveEnable);
	AddCommand(IDC_AUTOSAVE_USEORIGDIR, &PathConfigDlg::OnAutosaveUseOrigDir);
	AddCommand(IDC_AUTOSAVE_USECUSTOMDIR, &PathConfigDlg::OnAutosaveUseOrigDir);
	AddNotify(IDC_AUTOSAVE_PATH, mfcless::Notify::EditUpdate, &PathConfigDlg::OnSettingsChanged);
	AddNotify(IDC_AUTOSAVE_HISTORY, mfcless::Notify::EditUpdate, &PathConfigDlg::OnSettingsChanged);
	AddNotify(IDC_AUTOSAVE_INTERVAL, mfcless::Notify::EditUpdate, &PathConfigDlg::OnSettingsChanged);
	AddNotify(IDC_EDIT1, mfcless::Notify::EditUpdate, &PathConfigDlg::OnSettingsChanged);
}


bool PathConfigDlg::OnInitDialog()
{
	PropertyPage::OnInitDialog();

	const auto &settings = TrackerSettings::Instance();
	// Default paths
	for(const auto & [path, id] : PathSettings)
	{
		SetItemText(id, (settings.*path).GetDefaultDir().ToCString());
	}

	// Autosave
	SetChecked(IDC_CHECK1, settings.CreateBackupFiles ? 1 : 0);

	static_cast<CSpinButtonCtrl *>(GetDlgItem(IDC_SPIN1))->SetRange32(1, int32_max);
	static_cast<CSpinButtonCtrl *>(GetDlgItem(IDC_SPIN2))->SetRange32(1, int32_max);
	static_cast<CSpinButtonCtrl *>(GetDlgItem(IDC_SPIN3))->SetRange32(1, int32_max);
	SetChecked(IDC_AUTOSAVE_ENABLE, settings.AutosaveEnabled ? 1 : 0);
	SetItemInt(IDC_AUTOSAVE_HISTORY, settings.AutosaveHistoryDepth);
	SetItemInt(IDC_AUTOSAVE_INTERVAL, settings.AutosaveIntervalMinutes);
	SetItemInt(IDC_EDIT1, settings.AutosaveRetentionTimeDays ? settings.AutosaveRetentionTimeDays : 30);
	SetChecked(IDC_CHECK2, settings.AutosaveRetentionTimeDays > 0);
	SetChecked(IDC_AUTOSAVE_USEORIGDIR, settings.AutosaveUseOriginalPath ? 1 : 0);
	SetChecked(IDC_AUTOSAVE_USECUSTOMDIR, settings.AutosaveUseOriginalPath ? 0 : 1);
	SetChecked(IDC_CHECK3, settings.AutosaveDeletePermanently ? 1 : 0);
	//enable/disable stuff as appropriate
	OnAutosaveEnable();
	OnAutosaveUseOrigDir();
	OnAutosaveRetention();

	return true;
}


mpt::PathString PathConfigDlg::GetPath(int id)
{
	return mpt::PathString::FromCString(GetWindowTextString(*GetDlgItem(id))).WithTrailingSlash();
}


void PathConfigDlg::OnOK()
{
	auto &settings = TrackerSettings::Instance();
	// Default paths
	for(const auto &path : PathSettings)
	{
		(settings.*path.first).SetDefaultDir(GetPath(path.second));
	}

	// Autosave
	settings.CreateBackupFiles = IsChecked(IDC_CHECK1) != 0;

	settings.AutosaveEnabled = IsChecked(IDC_AUTOSAVE_ENABLE) != 0;
	settings.AutosaveHistoryDepth = GetItemInt(IDC_AUTOSAVE_HISTORY);
	settings.AutosaveIntervalMinutes = GetItemInt(IDC_AUTOSAVE_INTERVAL);
	if(IsChecked(IDC_CHECK2))
		settings.AutosaveRetentionTimeDays = std::max(GetItemInt(IDC_EDIT1), uint32(1));
	else
		settings.AutosaveRetentionTimeDays = 0;
	settings.AutosaveUseOriginalPath = IsChecked(IDC_AUTOSAVE_USEORIGDIR) != 0;
	settings.AutosaveDeletePermanently = IsChecked(IDC_CHECK3) != 0;

	PropertyPage::OnOK();
}


void PathConfigDlg::BrowseFolder(uint32 nID)
{
	const mpt::uchar *prompt = (nID == IDC_AUTOSAVE_PATH)
		? U_("Select a folder to store autosaved files in...")
		: U_("Select a default folder...");
	BrowseForFolder dlg(GetPath(nID), prompt);
	if(dlg.Show(this))
	{
		SetItemText(nID, dlg.GetDirectory().ToCString());
		OnSettingsChanged();
	}
}


void PathConfigDlg::OnAutosaveEnable()
{
	const bool enabled = IsChecked(IDC_AUTOSAVE_ENABLE);
	static constexpr uint32 AutoSaveDlgItems[] =
	{
		IDC_AUTOSAVE_INTERVAL, IDC_AUTOSAVE_HISTORY, IDC_AUTOSAVE_USEORIGDIR, IDC_AUTOSAVE_USECUSTOMDIR,
		IDC_AUTOSAVE_PATH, IDC_AUTOSAVE_BROWSE, IDC_CHECK2, IDC_CHECK3, IDC_EDIT1, IDC_SPIN1, IDC_SPIN2, IDC_SPIN3
	};
	for(uint32 id : AutoSaveDlgItems)
	{
		EnableItem(id, enabled);
	}
	OnSettingsChanged();
	return;
}


void PathConfigDlg::OnAutosaveUseOrigDir()
{
	if(IsChecked(IDC_AUTOSAVE_ENABLE))
	{
		const bool enabled = IsChecked(IDC_AUTOSAVE_USEORIGDIR) ? false : true;
		static constexpr uint32 AutoSaveDirDlgItems[] = {IDC_AUTOSAVE_PATH, IDC_AUTOSAVE_BROWSE, IDC_CHECK2, IDC_EDIT1, IDC_SPIN3};
		for(uint32 id : AutoSaveDirDlgItems)
		{
			EnableItem(id, enabled);
		}
		OnSettingsChanged();
	}
}


void PathConfigDlg::OnAutosaveRetention()
{
	const bool enabled = (IsChecked(IDC_AUTOSAVE_ENABLE) && IsChecked(IDC_CHECK2)) ? true : false;
	EnableItem(IDC_EDIT1, enabled);
	EnableItem(IDC_SPIN3, enabled);
}


void PathConfigDlg::OnSettingsChanged()
{
	SetModified(true);
}


bool PathConfigDlg::OnSetActive()
{
	CMainFrame::m_nLastOptionsPage = OPTIONS_PAGE_PATHS;
	return PropertyPage::OnSetActive();
}


void PathConfigDlg::OnBrowseAutosavePath() { BrowseFolder(IDC_AUTOSAVE_PATH); }
void PathConfigDlg::OnBrowseSongs() { BrowseFolder(IDC_OPTIONS_DIR_MODS); }
void PathConfigDlg::OnBrowseSamples() { BrowseFolder(IDC_OPTIONS_DIR_SAMPS); }
void PathConfigDlg::OnBrowseInstruments() { BrowseFolder(IDC_OPTIONS_DIR_INSTS); }
void PathConfigDlg::OnBrowsePlugins() { BrowseFolder(IDC_OPTIONS_DIR_VSTS); }
void PathConfigDlg::OnBrowsePresets() { BrowseFolder(IDC_OPTIONS_DIR_VSTPRESETS); }


OPENMPT_NAMESPACE_END
