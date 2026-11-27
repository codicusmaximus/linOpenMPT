/*
 * GeneralConfigDlg.cpp
 * --------------------
 * Purpose: Implementation of the general settings dialog.
 * Notes  : mfcless twin of upstream's mptrack/GeneralConfigDlg.cpp.
 *          Draft from qt/tools/twin.py, not yet edited by hand.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "GeneralConfigDlg.h"
#include "mfcless/Widgets.h"
#include "FileDialog.h"
#include "FolderScanner.h"
#include "Mainfrm.h"
#include "Mptrack.h"
#include "resource.h"
#include "Settings.h"
#include "TrackerSettings.h"
#include "../common/mptStringBuffer.h"


OPENMPT_NAMESPACE_BEGIN

void COptionsGeneral::RegisterMessageMap()
{
	PropertyPage::RegisterMessageMap();
	AddNotify(IDC_LIST1, mfcless::Notify::SelChange, &COptionsGeneral::OnOptionSelChanged);
	AddNotify(IDC_LIST1, mfcless::Notify::CheckChange, &COptionsGeneral::OnSettingsChanged);
	AddCommand(IDC_RADIO1, &COptionsGeneral::OnSettingsChanged);
	AddCommand(IDC_RADIO2, &COptionsGeneral::OnSettingsChanged);
	AddCommand(IDC_RADIO3, &COptionsGeneral::OnSettingsChanged);
	AddNotify(IDC_EDIT1, mfcless::Notify::EditChange, &COptionsGeneral::OnSettingsChanged);
	AddNotify(IDC_COMBO1, mfcless::Notify::SelChange, &COptionsGeneral::OnDefaultTypeChanged);
	AddNotify(IDC_COMBO2, mfcless::Notify::SelChange, &COptionsGeneral::OnTemplateChanged);
	AddNotify(IDC_COMBO2, mfcless::Notify::EditChange, &COptionsGeneral::OnTemplateChanged);
	AddCommand(IDC_BUTTON1, &COptionsGeneral::OnBrowseTemplate);
}


COptionsGeneral::COptionsGeneral() : CPropertyPage{IDD_OPTIONS_GENERAL}
{
	m_templateBrowseButton->SetAccessibleText(UL_("Browse for template file..."));
}


static constexpr struct GeneralOptionsDescriptions
{
	PatternSetup flag;
	const char *name, *description;
} generalOptionsList[] =
{
	{PatternSetup::PlayNewNotesWhileRecording, "Play new notes while recording", "When this option is enabled, notes entered in the pattern editor will always be played (If not checked, notes won't be played in record mode)."},
	{PatternSetup::PlayRowOnNoteEntry,         "Play whole row while recording", "When this option is enabled, all notes on the current row are played when entering notes in the pattern editor."},
	{PatternSetup::PlayRowOnNavigate,          "Play whole row when navigating", "When this option is enabled, all notes on the current row are played when navigating vertically in the pattern editor."},
	{PatternSetup::PreviewNoteTransposition,   "Play notes when transposing",    "When transposing a single note, the new note is previewed."},
	{PatternSetup::CenterActiveRow,            "Always center active row",       "Turn on this option to have the active row always centered in the pattern editor."},
	{PatternSetup::SmoothScrolling,            "Smooth pattern scrolling",       "Scroll patterns tick by tick rather than row by row at the expense of an increased CPU load."},
	{PatternSetup::RowAndOrderNumbersHex,      "Display rows / orders in hex",   "With this option enabled, row numbers and sequence numbers will be displayed in hexadecimal."},
	{PatternSetup::CursorWrap,                 "Cursor wrap in pattern editor",  "When this option is active, going past the end of a pattern row or channel will move the cursor to the beginning. When \"Continuous scroll\"-option is enabled, row wrap is disabled."},
	{PatternSetup::DragNDropEdit,              "Drag and Drop Editing",          "Enable moving a selection in the pattern editor (copying if pressing shift while dragging)"},
	{PatternSetup::FlatToolbarButtons,         "Flat Buttons",                   "Use flat buttons in toolbars"},
	{PatternSetup::SingleClickToExpand,        "Single click to expand tree",    "Single-clicking in the left tree view will expand a node."},
	{PatternSetup::IgnoreMutedChannels,        "Ignored muted channels",         "Notes will not be played on muted channels (unmuting will only start on a new note)."},
	{PatternSetup::NoLoudSamplePreview,        "No loud sample preview",         "Disable loud playback of samples in the sample/instrument editor. Sample volume depends on the sample volume slider on the general tab when activated (if disabled, samples are previewed at 0 dB)."},
	{PatternSetup::ShowPrevNextPattern,        "Show Prev/Next patterns",        "Displays grayed-out version of the previous/next patterns in the pattern editor. Does not work if \"always center active row\" is disabled."},
	{PatternSetup::ContinuousScrolling,        "Continuous scroll",              "Jumps to the next pattern when moving past the end of a pattern"},
	{PatternSetup::RecordNoteOff,              "Record note off",                "Record note off when a key is released on the PC keyboard."},
	{PatternSetup::FollowSongOffByDefault,     "Follow Song off by default",     "Ensure Follow Song is off when opening or starting a new song."},
	{PatternSetup::DisableFollowOnClick,       "Disable Follow Song on click",   "Follow Song is deactivated when clicking into the pattern."},
	{PatternSetup::HideUnavailableMenuEntries, "Old style pattern context menu", "Check this option to hide unavailable items in the pattern editor context menu. Uncheck to grey-out unavailable items instead."},
	{PatternSetup::SyncMute,                   "Maintain sample sync on mute",   "Samples continue to be processed when channels are muted (like in IT2 and FT2)"},
	{PatternSetup::SampleSyncOnSeek,           "Maintain sample sync on seek",   "Sample that are still active from previous patterns are continued to be played after seeking.\nNote: Some pattern commands may prevent samples from being synced. This feature may slow down seeking."},
	{PatternSetup::AutoDelayCommands,          "Automatic delay commands",       "Automatically insert appropriate note-delay commands when recording notes during live playback.\nThis setting is ignored when quantization is enabled in the pattern editor."},
	{PatternSetup::NoteFadeOnKeyUp,            "Note fade on key up",            "Enable to fade / stop notes on key up in pattern tab."},
	{PatternSetup::OverflowPaste,              "Overflow paste mode",            "Wrap pasted pattern data into next pattern. This is useful for creating echo channels."},
	{PatternSetup::ResetChannelsOnLoop,        "Reset channels on loop",         "If enabled, channels will be reset to their initial state when song looping is enabled.\nNote: This does not affect manual song loops (i.e. triggered by pattern commands) and is not recommended to be enabled."},
	{PatternSetup::LiveUpdateTreeView,         "Update sample status in tree",   "If enabled, active samples and instruments will be indicated by a different icon in the treeview."},
	{PatternSetup::NoCustomCloseDialog,        "Disable modern close dialog",    "When closing the main window, a confirmation window is shown for every unsaved document instead of one single window with a list of unsaved documents."},
	{PatternSetup::DblClickSelectsChannel,     "Double-click to select channel", "Instead of showing the note properties, double-clicking a pattern cell selects the whole channel."},
	{PatternSetup::ShowDefaultVolume,          "Show default volume commands",   "If there is no volume command next to a note + instrument combination, the sample's default volume is shown."},
};


void COptionsGeneral::DoDataExchange(DataExchange &dx)
{
	PropertyPage::DoDataExchange(dx);
	//{{AFX_DATA_MAP(COptionsGeneral)
	ExchangeControl(dx, IDC_LIST1, m_CheckList);
	ExchangeControl(dx, IDC_EDIT1, m_defaultArtist);
	ExchangeControl(dx, IDC_COMBO2, m_defaultTemplate);
	ExchangeControl(dx, IDC_COMBO1, m_defaultFormat);
	ExchangeControl(dx, IDC_BUTTON1, m_templateBrowseButton);
	//}}AFX_DATA_MAP
}


bool COptionsGeneral::OnInitDialog()
{
	PropertyPage::OnInitDialog();

	mfcless::SetWindowText(m_defaultArtist, mpt::ToUnicode(TrackerSettings::Instance().defaultArtist.Get()));

	static constexpr struct
	{
		MODTYPE type;
		const mpt::uchar *str;
	} formats[] =
	{
		{ MOD_TYPE_MOD, UL_("MOD (Amiga)") },
		{ MOD_TYPE_MOD_PC, UL_("MOD (PC)")},
		{ MOD_TYPE_XM, UL_("XM") },
		{ MOD_TYPE_S3M, UL_("S3M") },
		{ MOD_TYPE_IT, UL_("IT") },
		{ MOD_TYPE_MPT, UL_("MPTM") },
	};
	m_defaultFormat->setCurrentIndex(0);
	for(const auto &fmt : formats)
	{
		auto idx = mfcless::AddItem(m_defaultFormat, fmt.str);
		mfcless::SetItemData(m_defaultFormat, idx, fmt.type);
		if(fmt.type == TrackerSettings::Instance().defaultModType)
		{
			m_defaultFormat->setCurrentIndex(idx);
		}
	}

	const mpt::PathString basePath = theApp.GetUserTemplatesPath();
	FolderScanner scanner(basePath, FolderScanner::kOnlyFiles | FolderScanner::kFindInSubDirectories);
	mpt::PathString file;
	while(scanner.Next(file))
	{
		mpt::RawPathString fileW = file.AsNative();
		fileW = fileW.substr(basePath.Length());
		::SendMessage(m_defaultTemplate, CB_ADDSTRING, 0, (intptr_t)fileW.c_str());
	}
	file = TrackerSettings::Instance().defaultTemplateFile;
	if(file.GetDirectoryWithDrive() == basePath)
	{
		file = file.GetFilename();
	}
	mfcless::SetWindowText(m_defaultTemplate, file.ToUnicode());

	CheckRadio(IDC_RADIO1, IDC_RADIO3, IDC_RADIO1 + TrackerSettings::Instance().defaultNewFileAction);

	const FlagSet<PatternSetup> patternSetup = TrackerSettings::Instance().patternSetup;
	for(const auto &opt : generalOptionsList)
	{
		auto idx = mfcless::AddItem(m_CheckList, mpt::ToUnicode(mpt::Charset::ASCII, opt.name));
		const int check = patternSetup[opt.flag] ? 1 : 0;
		m_CheckList->SetCheck(idx, check);
	}
	m_CheckList->setCurrentRow(0);
	m_CheckList->SetItemHeight(0, 0);  // Workaround to force MFC to correctly compute the height of the first list item, in particular on high-DPI setups
	OnOptionSelChanged();

	return true;
}


void COptionsGeneral::OnOK()
{
	TrackerSettings::Instance().defaultArtist = GetWindowTextUnicode(m_defaultArtist);
	TrackerSettings::Instance().defaultModType = static_cast<MODTYPE>(mfcless::GetItemData(m_defaultFormat, m_defaultFormat->currentIndex()));
	TrackerSettings::Instance().defaultTemplateFile = mpt::PathString::FromUnicode(GetWindowTextString(m_defaultTemplate));

	NewFileAction action = nfDefaultFormat;
	int newActionRadio = GetCheckedRadio(IDC_RADIO1, IDC_RADIO3);
	if(newActionRadio == IDC_RADIO2) action = nfSameAsCurrent;
	if(newActionRadio == IDC_RADIO3) action = nfDefaultTemplate;
	if(action == nfDefaultTemplate && TrackerSettings::Instance().defaultTemplateFile.Get().empty())
	{
		action = nfDefaultFormat;
		mfcless::MessageBeep(nfIconWarning);
	}
	TrackerSettings::Instance().defaultNewFileAction = action;

	FlagSet<PatternSetup> patternSetup = TrackerSettings::Instance().patternSetup;
	for(int i = 0; i < mpt::saturate_cast<int>(std::size(generalOptionsList)); i++)
	{
		const bool check = (m_CheckList->GetCheck(i) != 0);

		patternSetup.set(generalOptionsList[i].flag, check);
	}
	TrackerSettings::Instance().patternSetup = patternSetup;

	CMainFrame::GetMainFrame()->SetupMiscOptions();

	PropertyPage::OnOK();
}


bool COptionsGeneral::OnSetActive()
{
	CMainFrame::m_nLastOptionsPage = OPTIONS_PAGE_GENERAL;
	return PropertyPage::OnSetActive();
}


void COptionsGeneral::OnOptionSelChanged()
{
	const char *desc = "";
	const int sel = m_CheckList->currentRow();
	if ((sel >= 0) && (sel < mpt::saturate_cast<int>(std::size(generalOptionsList))))
	{
		desc = generalOptionsList[sel].description;
	}
	SetItemText(IDC_TEXT1, mpt::ToUnicode(mpt::Charset::ASCII, desc));
}


void COptionsGeneral::OnBrowseTemplate()
{
	mpt::PathString basePath = theApp.GetUserTemplatesPath();
	mpt::PathString defaultFile = mpt::PathString::FromUnicode(GetWindowTextString(m_defaultTemplate));
	if(defaultFile.empty()) defaultFile = TrackerSettings::Instance().defaultTemplateFile;

	OpenFileDialog dlg;
	if(defaultFile.empty())
	{
		dlg.WorkingDirectory(basePath);
	} else
	{
		if(defaultFile.ToWide().find_first_of(UL_("/\\")) == std::wstring::npos)
		{
			// Relative path
			defaultFile = basePath + defaultFile;
		}
		dlg.DefaultFilename(defaultFile);
	}
	if(dlg.Show(this))
	{
		defaultFile = dlg.GetFirstFile();
		if(defaultFile.GetDirectoryWithDrive() == basePath)
		{
			defaultFile = defaultFile.GetFilename();
		}
		mfcless::SetWindowText(m_defaultTemplate, defaultFile.ToUnicode());
		OnTemplateChanged();
	}
}


void COptionsGeneral::OnDefaultTypeChanged() { CheckRadio(IDC_RADIO1, IDC_RADIO3, IDC_RADIO1); OnSettingsChanged(); }
void COptionsGeneral::OnTemplateChanged() { CheckRadio(IDC_RADIO1, IDC_RADIO3, IDC_RADIO3); OnSettingsChanged(); }


OPENMPT_NAMESPACE_END
