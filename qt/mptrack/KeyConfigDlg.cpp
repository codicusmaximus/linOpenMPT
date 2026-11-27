/*
 * KeyConfigDlg.cpp
 * ----------------
 * Purpose: Implementation of OpenMPT's keyboard configuration dialog.
 * Notes  : mfcless twin of upstream's mptrack/KeyConfigDlg.cpp.
 *          Draft from qt/tools/twin.py, not yet edited by hand.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "KeyConfigDlg.h"
#include "mfcless/Widgets.h"
#include "CListCtrl.h"
#include "FileDialog.h"
#include "Mainfrm.h"
#include "Mptrack.h"
#include "InputHandler.h"
#include "Reporting.h"
#include "resource.h"
#include "TrackerSettings.h"
#include "WindowMessages.h"
#include "../soundlib/mod_specifications.h"
#include "../soundlib/MIDIEvents.h"


OPENMPT_NAMESPACE_BEGIN


//***************************************************************************************//
// CCustEdit: customised QLineEdit control to catch keypresses.
// (does what CHotKeyCtrl does,but better)
//***************************************************************************************//

void CCustEdit::RegisterMessageMap()
{
	mfcless::Window<QLineEdit>::RegisterMessageMap();
	// ON_WM_SETFOCUS() (mfcless::Window)
	// ON_WM_KILLFOCUS() (mfcless::Window)
	AddMessage(WM_MOD_MIDIMSG, &CCustEdit::OnMidiMsg);
}


intptr_t CCustEdit::OnMidiMsg(uintptr_t dwMidiDataParam, intptr_t)
{
	if(!m_isFocussed)
		return 1;

	uint32 midiData = static_cast<uint32>(dwMidiDataParam);
	const auto byte1 = MIDIEvents::GetDataByte1FromEvent(midiData), byte2 = MIDIEvents::GetDataByte2FromEvent(midiData);
	switch(MIDIEvents::GetTypeFromEvent(midiData))
	{
	case MIDIEvents::evControllerChange:
		if(byte2 != 0)
		{
			SetKey(ModMidi, byte1);
			m_pOptKeyDlg->OnSetKeyChoice(this);
		}
		break;

	case MIDIEvents::evNoteOn:
	case MIDIEvents::evNoteOff:
		SetKey(ModMidi, byte1 | 0x80);
		m_pOptKeyDlg->OnSetKeyChoice(this);
		break;

	default:
		break;
	}

	return 1;
}


bool CCustEdit::PreTranslateMessage(mfcless::Message *pMsg)
{
	if(pMsg && !m_bypassed)
	{
		if(pMsg->message == mfcless::msg::KeyDown || pMsg->message == mfcless::msg::SysKeyDown)
		{
			SetKey(CInputHandler::GetModifierMask(), static_cast<uint32>(pMsg->wParam));
			return -1;  // Keypress handled, don't pass on message.
		} else if(pMsg->message == mfcless::msg::KeyUp || pMsg->message == mfcless::msg::SysKeyUp)
		{
			//if a key has been released but custom edit box is empty, we have probably just
			//navigated into the box with TAB or SHIFT-TAB. No need to set keychoice.
			if(code != 0)
				m_pOptKeyDlg->OnSetKeyChoice(this);
		}
	}
	return WindowBase::PreTranslateMessage(pMsg);
}


void CCustEdit::SetKey(FlagSet<Modifiers> inMod, uint32 inCode)
{
	mod = inMod;
	code = inCode;
	//Setup display
	SetWindowText(KeyCombination::GetKeyText(mod, code));
}


void CCustEdit::OnSetFocus(QWidget *pOldWnd)
{
	Window::OnSetFocus(pOldWnd);
	// Lock the input handler
	CMainFrame::GetInputHandler()->Bypass(true);
	// Accept MIDI input
	CMainFrame::GetMainFrame()->SetMidiRecordWnd(this);

	m_isFocussed = true;
}


void CCustEdit::OnKillFocus(QWidget *pNewWnd)
{
	Window::OnKillFocus(pNewWnd);
	//unlock the input handler
	CMainFrame::GetInputHandler()->Bypass(false);
	m_isFocussed = false;
	m_pOptKeyDlg->OnCancelKeyChoice(this);
}


//***************************************************************************************//
// COptionsKeyboard:
//
//***************************************************************************************//

static constexpr CListCtrlEx::Header KeyListHeaders[] =
{
	{UL_("Shortcut"),      274, mfcless::list::AlignLeft},
	{UL_("Assigned Keys"), 174, mfcless::list::AlignLeft},
};

void COptionsKeyboard::RegisterMessageMap()
{
	PropertyPage::RegisterMessageMap();
	// ON_WM_LBUTTONDBLCLK() (mfcless::Window)
	// ON_WM_DESTROY(): by hand (Qt event override)
	AddMessage(WM_DPICHANGED_AFTERPARENT, &COptionsKeyboard::OnDPIChangedAfterParent);
	AddNotify(IDC_CHOICECOMBO, mfcless::Notify::SelChange, &COptionsKeyboard::OnKeyChoiceSelect);
	AddNotify(IDC_KEYCATEGORY, mfcless::Notify::SelChange, &COptionsKeyboard::OnCategorySelChanged);
	AddNotify(IDC_CHORDDETECTWAITTIME, mfcless::Notify::EditUpdate, &COptionsKeyboard::OnChordWaitTimeChanged);
	AddCommand(IDC_BUTTON3, &COptionsKeyboard::OnClearSearch);
	AddCommand(IDC_BUTTON2, &COptionsKeyboard::OnEnableFindHotKey);
	AddCommand(IDC_BUTTON1, &COptionsKeyboard::OnListenForKeys);
	AddCommand(IDC_DELETE, &COptionsKeyboard::OnDeleteKeyChoice);
	AddCommand(IDC_RESTORE, &COptionsKeyboard::OnRestoreKeyChoice);
	AddCommand(IDC_LOAD, &COptionsKeyboard::OnLoad);
	AddCommand(IDC_SAVE, &COptionsKeyboard::OnSave);
	AddCommand(IDC_CHECKKEYDOWN, &COptionsKeyboard::OnCheck);
	AddCommand(IDC_CHECKKEYHOLD, &COptionsKeyboard::OnCheck);
	AddCommand(IDC_CHECKKEYUP, &COptionsKeyboard::OnCheck);
	AddCommand(IDC_NOTESREPEAT, &COptionsKeyboard::OnToggleNotesRepeat);
	AddCommand(IDC_RESTORE_KEYMAP, &COptionsKeyboard::OnRestoreDefaultKeymap);
	AddCommand(ID_KEYPRESET_MPT, &COptionsKeyboard::OnRestoreMPTKeymap);
	AddCommand(ID_KEYPRESET_IT, &COptionsKeyboard::OnRestoreITKeymap);
	AddCommand(ID_KEYPRESET_FT2, &COptionsKeyboard::OnRestoreFT2Keymap);
	AddNotify(IDC_FIND, mfcless::Notify::EditChange, &COptionsKeyboard::OnSearchTermChanged);
	AddNotify(IDC_FINDHOTKEY, mfcless::Notify::SetFocus, &COptionsKeyboard::OnClearHotKey);
	AddNotify(IDC_COMMAND_LIST, mfcless::Notify::ItemChanged, &COptionsKeyboard::OnCommandKeySelChanged);
	AddNotify(IDC_COMMAND_LIST, mfcless::Notify::DoubleClicked, &COptionsKeyboard::OnListenForKeysFromList);
	// #if MPT_WINNT_AT_LEAST(MPT_WIN_VISTA)
	// ON_NOTIFY(BCN_DROPDOWN, IDC_RESTORE_KEYMAP, &COptionsKeyboard::OnRestoreKeymapDropdown): by hand (Qt event override)
	// #endif
}


void COptionsKeyboard::DoDataExchange(DataExchange &dx)
{
	PropertyPage::DoDataExchange(dx);
	ExchangeControl(dx, IDC_KEYCATEGORY, m_cmbCategory);
	ExchangeControl(dx, IDC_COMMAND_LIST, m_lbnCommandKeys);
	ExchangeControl(dx, IDC_CHOICECOMBO, m_cmbKeyChoice);
	ExchangeControl(dx, IDC_CHORDDETECTWAITTIME, m_eChordWaitTime);
	ExchangeControl(dx, IDC_CUSTHOTKEY, m_eCustHotKey);
	ExchangeControl(dx, IDC_FINDHOTKEY, m_eFindHotKey);
	ExchangeControl(dx, IDC_CHECKKEYDOWN, m_bKeyDown);
	ExchangeControl(dx, IDC_CHECKKEYHOLD, m_bKeyHold);
	ExchangeControl(dx, IDC_CHECKKEYUP, m_bKeyUp);
	ExchangeControl(dx, IDC_FIND, m_eFind);
	ExchangeControl(dx, IDC_STATIC1, m_warnIconCtl);
	ExchangeControl(dx, IDC_KEYREPORT, m_warnText);
	ExchangeControl(dx, IDC_RESTORE_KEYMAP, m_restoreDefaultButton);
}


COptionsKeyboard::COptionsKeyboard() : PropertyPage{IDD_OPTIONS_KEYBOARD} { }


bool COptionsKeyboard::OnSetActive()
{
	CMainFrame::m_nLastOptionsPage = OPTIONS_PAGE_KEYBOARD;
	return PropertyPage::OnSetActive();
}


bool COptionsKeyboard::OnInitDialog()
{
	PropertyPage::OnInitDialog();
	m_fullPathName = TrackerSettings::Instance().m_szKbdFile;

	m_localCmdSet = std::make_unique<CCommandSet>();
	m_localCmdSet->Copy(*CMainFrame::GetInputHandler()->m_activeCommandSet);

	m_lbnCommandKeys->SetExtendedStyle(m_lbnCommandKeys->GetExtendedStyle() | mfcless::list::FullRowSelect);
	m_listGrouped = m_lbnCommandKeys->EnableGroupView();
	m_lbnCommandKeys->SetHeaders(KeyListHeaders);

	//Fill category combo and automatically selects first category
	DefineCommandCategories();
	for(size_t c = 0; c < commandCategories.size(); c++)
	{
		if(commandCategories[c].name && !commandCategories[c].commandRanges.empty())
			mfcless::SetItemData(m_cmbCategory, mfcless::AddItem(m_cmbCategory, commandCategories[c].name), c);
	}
	m_cmbCategory->setCurrentIndex(0);
	UpdateDialog();

#if MPT_WINNT_AT_LEAST(MPT_WIN_VISTA)
	m_restoreDefaultButton->ModifyStyle(0, BS_SPLITBUTTON);
#endif

	m_eCustHotKey->SetOwner(*this);
	m_eFindHotKey->SetOwner(*this);

	EnableKeyChoice(false);

	mfcless::SetWindowText(m_eChordWaitTime, mpt::ufmt::val(TrackerSettings::Instance().gnAutoChordWaitTime.Get()));
	return true;
}


intptr_t COptionsKeyboard::OnDPIChangedAfterParent(uintptr_t, intptr_t)
{
	auto result = Default();

	m_lbnCommandKeys->SetColumnWidths(KeyListHeaders);

	if(m_warnIcon)
	{
		DestroyIcon(m_warnIcon);
		m_warnIcon = nullptr;
	}
	if(m_infoIcon)
	{
		DestroyIcon(m_infoIcon);
		m_infoIcon = nullptr;
	}
	UpdateWarning(m_lastWarning);

	return result;
}


void COptionsKeyboard::DefineCommandCategories()
{
	{
		auto &commands = commandCategories.emplace_back(UL_("Global"), kCtxAllContexts).commandRanges;
		commands.emplace_back(kcStartFile, kcEndFile, UL_("File"));
		commands.emplace_back(kcStartPlayCommands, kcEndPlayCommands, UL_("Player"));
		commands.emplace_back(kcStartEditCommands, kcEndEditCommands, UL_("Edit"));
		commands.emplace_back(kcStartView, kcEndView, UL_("View"));
		commands.emplace_back(kcStartMisc, kcEndMisc, UL_("Miscellaneous"));
		commands.emplace_back(kcDummyShortcut, kcDummyShortcut, UL_(""));
	}

	commandCategories.emplace_back(UL_("  General [Top]"), kCtxCtrlGeneral);
	commandCategories.emplace_back(UL_("  General [Bottom]"), kCtxViewGeneral);
	commandCategories.emplace_back(UL_("  Pattern Editor [Top]"), kCtxCtrlPatterns);

	{
		auto &commands = commandCategories.emplace_back(UL_("  Pattern Editor - Order List"), kCtxCtrlOrderlist).commandRanges;
		commands.emplace_back(kcStartOrderlistEdit, kcEndOrderlistEdit, UL_("Edit"));
		commands.emplace_back(kcStartOrderlistNavigation, kcEndOrderlistNavigation, UL_("Navigation"));
		commands.emplace_back(kcStartOrderlistNum, kcEndOrderlistNum, UL_("Pattern Entry"));
		commands.emplace_back(kcStartOrderlistMisc, kcEndOrderlistMisc, UL_("Miscellaneous"));
	}

	{
		auto &commands = commandCategories.emplace_back(UL_("  Pattern Editor - Quick Channel Settings"), kCtxChannelSettings).commandRanges;
		commands.emplace_back(kcStartChnSettingsCommands, kcEndChnSettingsCommands, UL_(""));
	}

	{
		auto &commands = commandCategories.emplace_back(UL_("    Pattern Editor - General"), kCtxViewPatterns).commandRanges;
		commands.emplace_back(kcStartPlainNavigate, kcEndPlainNavigate, UL_("Navigation"));
		commands.emplace_back(kcStartJumpSnap, kcEndJumpSnap, UL_("Jump"));
		commands.emplace_back(kcStartHomeEnd, kcEndHomeEnd, UL_("Go To"));
		commands.emplace_back(kcStartGotoColumn, kcEndGotoColumn, UL_("Go To Column"));
		commands.emplace_back(kcPrevPattern, kcNextSequence, UL_("Order List Navigation"));
		commands.emplace_back(kcStartPatternScrolling, kcEndPatternScrolling, UL_("Scrolling"));
		commands.emplace_back(kcStartSelect, kcEndSelect, UL_("Selection"));
		commands.emplace_back(kcStartPatternClipboard, kcEndPatternClipboard, UL_("Clipboard"));
		commands.emplace_back(kcClearRow, kcInsertWholeRowGlobal, UL_("Clear / Insert"));
		commands.emplace_back(kcStartChannelKeys, kcEndChannelKeys, UL_("Channels"));
		commands.emplace_back(kcBeginTranspose, kcEndTranspose, UL_("Transpose"));
		commands.emplace_back(kcStartPatternEditMisc, kcEndPatternEditMisc, UL_("Edit"));
		commands.emplace_back(kcStartPatternMisc, kcEndPatternMisc, UL_("Miscellaneous"));
	}

	{
		auto &commands = commandCategories.emplace_back(UL_("        Pattern Editor - Note Column"), kCtxViewPatternsNote).commandRanges;
		commands.emplace_back(kcVPStartNotes, kcVPEndNotes, UL_("Note Entry"));
		commands.emplace_back(kcSetOctave0, kcSetOctave9, UL_("Octave Entry"));
		commands.emplace_back(kcStartNoteMisc, kcEndNoteMisc, UL_("Miscellaneous"));
	}

	{
		auto &commands = commandCategories.emplace_back(UL_("        Pattern Editor - Instrument Column"), kCtxViewPatternsIns).commandRanges;
		commands.emplace_back(kcSetIns0, kcSetIns9, UL_("Instrument Entry"));
	}

	{
		auto &commands = commandCategories.emplace_back(UL_("        Pattern Editor - Volume Column"), kCtxViewPatternsVol).commandRanges;
		commands.emplace_back(kcStartVolumeDigits, kcEndVolumeDigits, UL_("Volume Entry"));
		commands.emplace_back(kcStartVolumeCommands, kcEndVolumeCommands, UL_("Volume Command Entry"));
	}

	{
		auto &commands = commandCategories.emplace_back(UL_("        Pattern Editor - Effect Column"), kCtxViewPatternsFX).commandRanges;
		commands.emplace_back(kcSetFXStart, kcSetFXEnd, UL_("Effect Command Entry"));
	}

	{
		auto &commands = commandCategories.emplace_back(UL_("        Pattern Editor - Effect Parameter Column"), kCtxViewPatternsFXparam).commandRanges;
		commands.emplace_back(kcSetFXParam0, kcSetFXParamF, UL_("Parameter Digit Entry"));
	}

	commandCategories.emplace_back(UL_("  Sample [Top]"), kCtxCtrlSamples);

	{
		auto &commands = commandCategories.emplace_back(UL_("    Sample Editor"), kCtxViewSamples).commandRanges;
		commands.emplace_back(kcStartSampleEditing, kcEndSampleEditing, UL_("Edit"));
		commands.emplace_back(kcStartSampleMisc, kcEndSampleMisc, UL_("Miscellaneous"));
		commands.emplace_back(kcStartSampleCues, kcEndSampleCueGroup, UL_("Sample Cues"));
	}

	{
		auto &commands = commandCategories.emplace_back(UL_("  Instrument Editor"), kCtxCtrlInstruments).commandRanges;
		commands.emplace_back(kcStartInstrumentCtrl, kcEndInstrumentCtrl, UL_("Miscellaneous"));
		commands.emplace_back(kcStartInsNoteMap, kcEndInsNoteMap, UL_("Note Map"));
	}

	{
		auto &commands = commandCategories.emplace_back(UL_("    Envelope Editor"), kCtxViewInstruments).commandRanges;
		commands.emplace_back(kcStartInsEnvelopeEdit, kcEndInsEnvelopeEdit, UL_(""));
	}

	commandCategories.emplace_back(UL_("  Comments [Top]"), kCtxCtrlComments);

	{
		auto &commands = commandCategories.emplace_back(UL_("  Comments [Bottom]"), kCtxViewComments).commandRanges;
		commands.emplace_back(kcStartCommentsCommands, kcEndCommentsCommands, UL_(""));
	}

	{
		auto &commands = commandCategories.emplace_back(UL_("  Plugin Editor"), kCtxVSTGUI).commandRanges;
		commands.emplace_back(kcStartVSTGUICommands, kcEndVSTGUICommands, UL_(""));
	}

	{
		auto &commands = commandCategories.emplace_back(UL_("  Tree View"), kCtxViewTree).commandRanges;
		commands.emplace_back(kcStartTreeViewCommands, kcEndTreeViewCommands, UL_(""));
	}
}


// Pure GUI methods

void COptionsKeyboard::UpdateDialog()
{
	OnCategorySelChanged();    // Fills command list and automatically selects first command.
	OnCommandKeySelChanged();  // Fills command key choice list for that command and automatically selects first choice.
}


void COptionsKeyboard::OnKeyboardChanged()
{
	OnSettingsChanged();
	UpdateDialog();
}


void COptionsKeyboard::OnCategorySelChanged()
{
	LockControls();
	int cat = static_cast<int>(mfcless::GetItemData(m_cmbCategory, m_cmbCategory->currentIndex()));
	if(cat < 0)
		return;

	const bool refresh = cat != m_curCategory || mfcless::GetWindowTextLength(m_eFind) > 0 || mfcless::GetWindowTextLength(m_eFindHotKey) > 0;
	mfcless::SetWindowText(m_eFind, UL_(""));
	OnClearHotKey();
	if(refresh)
	{
		// Changed category
		UpdateShortcutList(cat);
	}
	UnlockControls();
}


// Force last active category to be selected in dropdown menu.
void COptionsKeyboard::UpdateCategory()
{
	for(int i = 0; i < m_cmbCategory->count(); i++)
	{
		if(static_cast<int>(mfcless::GetItemData(m_cmbCategory, i)) == m_curCategory)
		{
			m_cmbCategory->setCurrentIndex(i);
			break;
		}
	}
}

void COptionsKeyboard::OnSearchTermChanged()
{
	if(IsLocked())
		return;

	mpt::ustring findString;
	mfcless::GetWindowText(m_eFind, findString);

	if(findString.empty())
	{
		UpdateCategory();
	}
	UpdateShortcutList(findString.empty() ? m_curCategory : -1);
}


void COptionsKeyboard::OnClearSearch()
{
	m_eFindHotKey->SetKey(ModNone, 0);
	mfcless::SetWindowText(m_eFind, UL_(""));
}


void COptionsKeyboard::OnEnableFindHotKey()
{
	OnClearHotKey();
	ShowItem(IDC_BUTTON2, mfcless::show::Hide);
	ShowItem(IDC_FINDHOTKEY_LABEL, mfcless::show::Show);
	ShowItem(IDC_FINDHOTKEY, mfcless::show::Show);
	GetDlgItem(IDC_FINDHOTKEY)->SetFocus();
}


void COptionsKeyboard::OnFindHotKey()
{
	ShowItem(IDC_BUTTON2, mfcless::show::Show);
	ShowItem(IDC_FINDHOTKEY_LABEL, mfcless::show::Hide);
	ShowItem(IDC_FINDHOTKEY, mfcless::show::Hide);
	const bool hasKey = m_eFindHotKey->HasKey();
	if(!hasKey)
		UpdateCategory();
	UpdateShortcutList(hasKey ? -1 : m_curCategory);
	mfcless::GotoDlgCtrl(m_lbnCommandKeys);
}


void COptionsKeyboard::OnClearHotKey()
{
	// Focus key search: Clear input
	m_eFindHotKey->SetKey(ModNone, 0);
}


void COptionsKeyboard::InsertGroup(const mpt::uchar *title, int groupId)
{
	LVGROUP group{};
#if MPT_WINNT_AT_LEAST(MPT_WIN_VISTA)
	group.cbSize = LVGROUP_V5_SIZE;
#else
	group.cbSize = sizeof(group);
#endif
	group.mask = LVGF_HEADER | LVGF_GROUPID;
#if defined(UNICODE)
	group.pszHeader = const_cast<mpt::uchar *>(title);
#else
	std::wstring titlew = mpt::ToWide(mpt::ustring(title));
	group.pszHeader = const_cast<WCHAR *>(titlew.c_str());
#endif
	group.cchHeader = 0;
	group.pszFooter = nullptr;
	group.cchFooter = 0;
	group.iGroupId = groupId;
	group.stateMask = 0;
	group.state = 0;
	group.uAlign = LVGA_HEADER_LEFT;
	ListView_InsertGroup(m_lbnCommandKeys, -1, &group);
	ListView_SetGroupState(m_lbnCommandKeys, group.iGroupId, LVGS_COLLAPSIBLE, LVGS_COLLAPSIBLE);
}


// Fills command list and automatically selects first command.
void COptionsKeyboard::UpdateShortcutList(int category)
{
	mpt::ustring findString;
	mfcless::GetWindowText(m_eFind, findString);
	findString.MakeLower();

	const bool searchByName = !findString.empty(), searchByKey = m_eFindHotKey->HasKey();
	const bool doSearch = (searchByName || searchByKey);

	int firstCat = category, lastCat = category;
	if(category == -1)
	{
		// We will search in all categories
		firstCat = 0;
		lastCat = static_cast<int>(commandCategories.size()) - 1;
	}

	const auto curSelection = m_lbnCommandKeys->GetSelectionMark();
	const CommandID curCommand = (curSelection >= 0) ? static_cast<CommandID>(mfcless::GetItemData(m_lbnCommandKeys, curSelection)) : kcNull;
	m_lbnCommandKeys->setUpdatesEnabled(false);
	m_lbnCommandKeys->DeleteAllItems();
	if(m_listGrouped)
	{
		ListView_RemoveAllGroups(m_lbnCommandKeys);
		m_lbnCommandKeys->EnableGroupView();
	}

	int currentGroup = -1;
	int itemID = -1;
	int itemToSelect = -1;

	for(int cat = firstCat; cat <= lastCat; cat++)
	{
		// When searching, we also add the category names to the list.
		bool addCategoryName = (firstCat != lastCat);

		for(const auto &range : commandCategories[cat].commandRanges)
		{
			for(CommandID com = range.first; com <= range.last; com = static_cast<CommandID>(com + 1))
			{
				mpt::ustring cmdText = m_localCmdSet->GetCommandText(com);
				bool addKey = true;

				if(searchByKey)
				{
					addKey = false;
					for(const KeyCombination &kc : m_localCmdSet->GetKeyChoices(com))
					{
						if(kc.KeyCode() == m_eFindHotKey->code && kc.Modifier() == m_eFindHotKey->mod)
						{
							addKey = true;
							break;
						}
					}
				}
				if(searchByName && addKey)
				{
					addKey = (cmdText.MakeLower().Find(findString) >= 0);
				}

				if(!addKey)
					continue;

				m_curCategory = cat;

				LVITEM lvi;
				lvi.mask = LVIF_TEXT | LVIF_PARAM;
				if(m_listGrouped)
					lvi.mask |= LVIF_GROUPID;
				lvi.iSubItem = 0;
				lvi.state = 0;
				lvi.stateMask = 0;
				lvi.cchTextMax = 0;
				lvi.iImage = 0;
				lvi.iIndent = 0;
				lvi.iGroupId = 0;

				if(com == range.first && !doSearch)
				{
					if(m_listGrouped)
					{
						InsertGroup(range.name, ++currentGroup);
					} else
					{
						lvi.iItem = ++itemID;
						lvi.lParam = kcNull;
						mpt::ustring catName;
						if(range.name.empty())
							catName = const_cast<mpt::uchar *>(UL_("------------------------------------------------------"));
						else
							catName = UL_("------ ") + range.name + UL_(" ------");
						lvi.pszText = const_cast<mpt::uchar *>(catName.c_str());
						m_lbnCommandKeys->InsertItem(&lvi);
						if(itemToSelect == -1)
							itemToSelect = itemID;
					}
				}

				if(!m_localCmdSet->IsHidden(com))
				{
					if(doSearch && addCategoryName)
					{
						if(m_listGrouped)
						{
							InsertGroup(mpt::ustring{commandCategories[cat].name}.TrimLeft(), ++currentGroup);
						} else
						{
							const mpt::ustring catName = UL_("------ ") + mpt::ustring{commandCategories[cat].name}.TrimLeft() + UL_(" ------");
							lvi.iItem = ++itemID;
							lvi.lParam = kcNull;
							lvi.pszText = const_cast<mpt::uchar *>(catName.c_str());
							m_lbnCommandKeys->InsertItem(&lvi);
						}
						addCategoryName = false;
					} else if(currentGroup == -1 && m_listGrouped)
					{
						InsertGroup(UL_(""), ++currentGroup);
					}

					const mpt::ustring text = m_localCmdSet->GetCommandText(com);
					lvi.iItem = ++itemID;
					lvi.lParam = static_cast<intptr_t>(com);
					lvi.pszText = const_cast<mpt::uchar *>(text.c_str());
					lvi.iGroupId = currentGroup;
					m_lbnCommandKeys->InsertItem(&lvi);
					m_lbnCommandKeys->SetItemText(itemID, 1, m_localCmdSet->GetKeyTextFromCommand(com));
					if(itemToSelect == -1)
						itemToSelect = itemID;

					if(curCommand == com)
					{
						// Keep selection on previously selected string
						itemToSelect = itemID;
					}
				}
			}
		}
	}

	if(itemToSelect != -1)
	{
		m_lbnCommandKeys->SetSelectionMark(itemToSelect);
		m_lbnCommandKeys->SetItemState(itemToSelect, mfcless::list::Selected, mfcless::list::Selected);
	}
	m_lbnCommandKeys->setUpdatesEnabled(true);
	OnCommandKeySelChanged();
}


// Fills  key choice list and automatically selects first key choice
void COptionsKeyboard::OnCommandKeySelChanged()
{
	int selectedItem;
	if(pNMHDR)
	{
		auto hdr = reinterpret_cast<const NMLISTVIEW *>(pNMHDR);
		if(!(hdr->uNewState & mfcless::list::Selected))
			return;
		selectedItem = hdr->iItem;
	} else
	{
		selectedItem = m_lbnCommandKeys->GetSelectionMark();
	}

	const CommandID cmd = (selectedItem >= 0) ? static_cast<CommandID>(mfcless::GetItemData(m_lbnCommandKeys, selectedItem)) : kcNull;
	mpt::ustring str;

	EnableKeyChoice(false);

	bool enableButton = (cmd != kcNull) ? true : false;
	bool enableCheckBoxes = (cmd != kcNull && m_localCmdSet->GetKeyListSize(cmd) > 0) ? true : false;
	EnableItem(IDC_BUTTON1, enableButton);
	EnableItem(IDC_DELETE, enableButton);
	EnableItem(IDC_RESTORE, enableButton);
	m_cmbKeyChoice->setEnabled(enableButton);
	m_bKeyDown->setEnabled(enableCheckBoxes);
	m_bKeyHold->setEnabled(enableCheckBoxes);
	m_bKeyUp->setEnabled(enableCheckBoxes);

	//Separator
	if(cmd == kcNull)
	{
		mfcless::SetWindowText(m_cmbKeyChoice, UL_(""));
		mfcless::SetWindowText(m_eCustHotKey, UL_(""));
		m_bKeyDown->setChecked(0);
		m_bKeyHold->setChecked(0);
		m_bKeyUp->setChecked(0);
		m_curCommand = kcNull;

		GetDlgItem(IDC_GROUPBOX_KEYSETUP)->SetWindowText(UL_("&Key setup for selected command"));
		UpdateWarning();
	}

	//Fill "choice" list
	else if((cmd >= kcFirst && cmd != m_curCommand) || m_forceUpdate)  // Have we changed command?
	{
		GetDlgItem(IDC_GROUPBOX_KEYSETUP)->SetWindowText(UL_("&Key setup for ") + m_localCmdSet->GetCommandText(cmd));

		m_forceUpdate = false;

		m_curCommand = cmd;
		m_curCategory = GetCategoryFromCommandID(cmd);

		m_cmbKeyChoice->clear();
		int numChoices = m_localCmdSet->GetKeyListSize(cmd);
		if(cmd >= kcFirst && cmd < kcNumCommands && numChoices > 0)
		{
			for(int i = 0; i < numChoices; i++)
			{
				mpt::ustring s = MPT_UFORMAT("Choice {} (of {})")(i + 1, numChoices);
				mfcless::SetItemData(m_cmbKeyChoice, mfcless::AddItem(m_cmbKeyChoice, s), i);
			}
		}
		mfcless::SetItemData(m_cmbKeyChoice, mfcless::AddItem(m_cmbKeyChoice, UL_("<new>")), numChoices);
		m_cmbKeyChoice->setCurrentIndex(0);
		m_curKeyChoice = -1;
		OnKeyChoiceSelect();
	}
}


void COptionsKeyboard::OnListenForKeysFromList()
{
	auto hdr = reinterpret_cast<const NMLISTVIEW *>(pNMHDR);
	if(m_curCommand != kcNull && hdr->iSubItem == 1)
		OnListenForKeys();
}


//Fills or clears key choice info
void COptionsKeyboard::OnKeyChoiceSelect()
{
	EnableKeyChoice(false);

	int choice = static_cast<int>(mfcless::GetItemData(m_cmbKeyChoice, m_cmbKeyChoice->currentIndex()));
	CommandID cmd = m_curCommand;

	//If nothing there, clear
	if(cmd == kcNull || choice >= m_localCmdSet->GetKeyListSize(cmd) || choice < 0)
	{
		UpdateWarning();
		m_curKeyChoice = choice;
		m_forceUpdate = true;
		m_eCustHotKey->SetKey(ModNone, 0);
		m_bKeyDown->setChecked(0);
		m_bKeyHold->setChecked(0);
		m_bKeyUp->setChecked(0);
		return;
	}

	//else, if changed, Fill
	if(choice != m_curKeyChoice || m_forceUpdate)
	{
		m_curKeyChoice = choice;
		m_forceUpdate = false;
		KeyCombination kc = m_localCmdSet->GetKey(cmd, choice);
		m_eCustHotKey->SetKey(kc.Modifier(), kc.KeyCode());

		m_bKeyDown->setChecked((kc.EventType() & kKeyEventDown) ? 1 : 0);
		m_bKeyHold->setChecked((kc.EventType() & kKeyEventRepeat) ? 1 : 0);
		m_bKeyUp->setChecked((kc.EventType() & kKeyEventUp) ? 1 : 0);

		if(auto conflictCmd = m_localCmdSet->IsConflicting(kc, cmd, true, false); conflictCmd.first != kcNull
		   && conflictCmd.first != cmd)
		{
			UpdateWarning(m_localCmdSet->FormatConflict(kc, conflictCmd.first, conflictCmd.second));
		} else
		{
			UpdateWarning();
		}
	}
}

void COptionsKeyboard::OnChordWaitTimeChanged()
{
	mpt::ustring s;
	uint32 val;
	mfcless::GetWindowText(m_eChordWaitTime, s);
	val = _tstoi(s);
	if(val > 5000)
	{
		val = 5000;
		mfcless::SetWindowText(m_eChordWaitTime, UL_("5000"));
	}
	OnSettingsChanged();
}

// Change handling

void COptionsKeyboard::OnRestoreKeyChoice()
{
	CommandID cmd = m_curCommand;

	CInputHandler *ih = CMainFrame::GetInputHandler();

	// Do nothing if there's nothing to restore
	if(cmd == kcNull || ((m_curKeyChoice < 0 || m_curKeyChoice >= ih->GetKeyListSize(cmd)) && ih->GetKeyListSize(cmd) != 0))
	{
		mfcless::MessageBeep();
		return;
	}

	if(ih->GetKeyListSize(cmd) == 0)
	{
		// Restore the defaults for this key
		mpt::heap_value<CCommandSet> defaultSet;
		defaultSet->LoadDefaultKeymap();
		for(const KeyCombination &kc : defaultSet->GetKeyChoices(cmd))
		{
			m_localCmdSet->Add(kc, cmd, true, m_curKeyChoice);
		}
	} else
	{
		// Restore current key combination choice for currently selected command.
		KeyCombination kc = ih->m_activeCommandSet->GetKey(cmd, m_curKeyChoice);
		m_localCmdSet->Remove(m_curKeyChoice, cmd);
		UpdateWarning(m_localCmdSet->Add(kc, cmd, true, m_curKeyChoice));
	}

	ForceUpdateGUI();
	return;
}


void COptionsKeyboard::OnLButtonDblClk(uint32 flags, mfcless::Point point)
{
	ClientToScreen(&point);
	mfcless::Rect rect;
	m_eCustHotKey->GetWindowRect(rect);
	if(m_curCommand != kcNull && m_eCustHotKey->IsBypassed() && rect.PtInRect(point))
		EnableKeyChoice(true);
	else
		Window::OnLButtonDblClk(flags, point);
}


void COptionsKeyboard::OnListenForKeys()
{
	EnableKeyChoice(m_eCustHotKey->IsBypassed());
}


void COptionsKeyboard::EnableKeyChoice(bool enable)
{
	if(!enable && mfcless::GetFocus() == &m_eCustHotKey)
		mfcless::GotoDlgCtrl(GetDlgItem(IDC_BUTTON1));
	m_eCustHotKey->Bypass(!enable);
	GetDlgItem(IDC_BUTTON1)->SetWindowText(enable ? UL_("Cancel") : UL_("&Set"));
	if(enable)
		mfcless::GotoDlgCtrl(m_eCustHotKey);
}


void COptionsKeyboard::OnDeleteKeyChoice()
{
	CommandID cmd = m_curCommand;

	// Do nothing if there's no key defined for this slot.
	if(m_curCommand == kcNull || m_curKeyChoice < 0 || m_curKeyChoice >= m_localCmdSet->GetKeyListSize(cmd))
	{
		mfcless::MessageBeep();
		return;
	}

	// Delete current key combination choice for currently selected command.
	m_localCmdSet->Remove(m_curKeyChoice, cmd);

	ForceUpdateGUI();
	UpdateWarning();
}


void COptionsKeyboard::OnCancelKeyChoice(const QWidget *source)
{
	if(source == &m_eFindHotKey)
	{
		ShowItem(IDC_BUTTON2, mfcless::show::Show);
		ShowItem(IDC_FINDHOTKEY_LABEL, mfcless::show::Hide);
		ShowItem(IDC_FINDHOTKEY, mfcless::show::Hide);
	} else
	{
		EnableKeyChoice(false);
	}
}


void COptionsKeyboard::OnSetKeyChoice(const QWidget *source)
{
	if(source == &m_eFindHotKey)
	{
		OnFindHotKey();
		return;
	}

	EnableKeyChoice(false);

	CommandID cmd = m_curCommand;
	if(cmd == kcNull)
	{
		Reporting::Warning("Invalid slot.", "Invalid key data", this);
		return;
	}

	FlagSet<KeyEventType> event = kKeyEventNone;
	if(m_bKeyDown->isChecked() != 0)
		event |= kKeyEventDown;
	if(m_bKeyHold->isChecked() != 0)
		event |= kKeyEventRepeat;
	if(m_bKeyUp->isChecked() != 0)
		event |= kKeyEventUp;
	if(event == kKeyEventNone)
		event = kKeyEventDown;

	KeyCombination kc(CCommandSet::ContextFromCommand(cmd), m_eCustHotKey->mod, m_eCustHotKey->code, event);
	//detect invalid input
	if(!kc.KeyCode())
	{
		Reporting::Warning("You need to say to which key you'd like to map this command to.", "Invalid key data", this);
		return;
	}

	bool add = true, updateAll = false;
	std::pair<CommandID, KeyCombination> conflictCmd;
	if(CCommandSet::MustBeModifierKey(cmd) && !kc.IsModifierCombination())
	{
		KeyCombination origKc = m_localCmdSet->GetKey(cmd, m_curKeyChoice);
		m_eCustHotKey->SetKey(origKc.Modifier(), origKc.KeyCode());
		UpdateWarning(m_localCmdSet->GetCommandText(cmd) + UL_(" must be a modifier (Shift/Ctrl/Alt), but you chose ") + kc.GetKeyText(), true);
		add = false;
	} else if((conflictCmd = m_localCmdSet->IsConflicting(kc, cmd)).first != kcNull && conflictCmd.first != cmd && !m_localCmdSet->IsCrossContextConflict(kc, conflictCmd.second))
	{
		ConfirmAnswer delOld = Reporting::Confirm(UL_("New shortcut (") + kc.GetKeyText() + UL_(") has the same key combination as ") + m_localCmdSet->GetCommandText(conflictCmd.first) + UL_(" in ") + conflictCmd.second.GetContextText() + UL_(".\nDo you want to delete the other shortcut, only keeping the new one?"), UL_("Shortcut Conflict"), true, false, this);
		if(delOld == cnfYes)
		{
			m_localCmdSet->Remove(conflictCmd.second, conflictCmd.first);
			updateAll = true;
		} else if(delOld == cnfCancel)
		{
			// Cancel altogther; restore original choice
			add = false;
			if(m_curKeyChoice >= 0 && m_curKeyChoice < m_localCmdSet->GetKeyListSize(cmd))
			{
				KeyCombination origKc = m_localCmdSet->GetKey(cmd, m_curKeyChoice);
				m_eCustHotKey->SetKey(origKc.Modifier(), origKc.KeyCode());
			} else
			{
				mfcless::SetWindowText(m_eCustHotKey, UL_(""));
			}
		}
	}
	
	if(add)
	{
		//process valid input
		m_localCmdSet->Remove(m_curKeyChoice, cmd);
		UpdateWarning(m_localCmdSet->Add(kc, cmd, true, m_curKeyChoice), true);
		ForceUpdateGUI(updateAll);
	}
}


static QIcon LoadScaledIcon(const mpt::uchar *resourceName, int iconSize)
{
	mpt::Library comctl32(mpt::LibraryPath::System(P_("Comctl32")));
	if(comctl32.IsValid())
	{
		using PLOADICONWITHSCALEDOWN = HRESULT(WINAPI *)(HINSTANCE, PCWSTR, int, int, QIcon *);
		PLOADICONWITHSCALEDOWN LoadIconWithScaleDown = nullptr;
		QIcon icon;
		if(comctl32.Bind(LoadIconWithScaleDown, "LoadIconWithScaleDown"))
			LoadIconWithScaleDown(NULL, MAKEINTRESOURCEW(resourceName), iconSize, iconSize, &icon);
		if(icon)
			return icon;
	}
	return static_cast<QIcon>(::LoadImage(nullptr, resourceName, IMAGE_ICON, iconSize, iconSize, LR_SHARED));
}


void COptionsKeyboard::UpdateWarning(mpt::ustring text, bool notify)
{
	const int iconSize = HighDPISupport::ScalePixels(16, this);
	QIcon icon;
	if(text.empty())
	{
		mfcless::SetWindowText(m_warnText, UL_("No conflicts found."));
		if(!m_infoIcon)
			m_infoIcon = LoadScaledIcon(IDI_INFORMATION, iconSize);
		icon = m_infoIcon;
	} else
	{
		if(notify)
			mfcless::MessageBeep();

		mfcless::SetWindowText(m_warnText, text);
		if(!m_warnIcon)
			m_warnIcon = LoadScaledIcon(IDI_EXCLAMATION, iconSize);
		icon = m_warnIcon;
	}
	m_warnIconCtl->SetWindowPos(nullptr, 0, 0, iconSize, iconSize, mfcless::swp::ShowWindow | mfcless::swp::NoZOrder | mfcless::swp::NoRedraw | mfcless::swp::NoMove | mfcless::swp::NoActivate);
	m_warnIconCtl->SetIcon(icon);
	m_lastWarning = std::move(text);
}


void COptionsKeyboard::OnOK()
{
	CMainFrame::GetInputHandler()->SetNewCommandSet(*m_localCmdSet);

	mpt::ustring cs;
	mfcless::GetWindowText(m_eChordWaitTime, cs);
	TrackerSettings::Instance().gnAutoChordWaitTime = _tstoi(cs);

	PropertyPage::OnOK();
}


void COptionsKeyboard::OnDestroy()
{
	if(m_warnIcon)
	{
		DestroyIcon(m_warnIcon);
		m_warnIcon = nullptr;
	}
	if(m_infoIcon)
	{
		DestroyIcon(m_infoIcon);
		m_infoIcon = nullptr;
	}
	PropertyPage::OnDestroy();
	m_localCmdSet.reset();
}


void COptionsKeyboard::OnLoad()
{
	auto dlg = OpenFileDialog()
	               .DefaultExtension(U_("mkb"))
	               .DefaultFilename(m_fullPathName)
	               .ExtensionFilter(U_("OpenMPT Key Bindings (*.mkb)|*.mkb||"))
	               .AddPlace(theApp.GetInstallPkgPath() + P_("extraKeymaps"))
	               .WorkingDirectory(TrackerSettings::Instance().m_szKbdFile);
	if(!dlg.Show(this))
		return;

	m_fullPathName = dlg.GetFirstFile();
	m_localCmdSet->LoadFile(m_fullPathName);
	ForceUpdateGUI(true);
	UpdateWarning();
}


void COptionsKeyboard::OnSave()
{
	auto dlg = SaveFileDialog()
	               .DefaultExtension(U_("mkb"))
	               .DefaultFilename(m_fullPathName)
	               .ExtensionFilter(U_("OpenMPT Key Bindings (*.mkb)|*.mkb||"))
	               .WorkingDirectory(TrackerSettings::Instance().m_szKbdFile);
	if(!dlg.Show(this))
		return;

	m_fullPathName = dlg.GetFirstFile();
	m_localCmdSet->SaveFile(m_fullPathName);
}


void COptionsKeyboard::OnToggleNotesRepeat()
{
	m_localCmdSet->QuickChange_NotesRepeat(IsChecked(IDC_NOTESREPEAT) != 1);
	ForceUpdateGUI(true);
}


void COptionsKeyboard::ForceUpdateGUI(bool updateAllKeys)
{
	m_forceUpdate = true;                  // m_curKeyChoice and m_curCommand haven't changed, yet we still want to update.
	int curChoice = m_curKeyChoice;        // next call will overwrite m_curKeyChoice
	OnCommandKeySelChanged();              // update keychoice list
	m_cmbKeyChoice->setCurrentIndex(curChoice);   // select fresh keychoice (thus restoring m_curKeyChoice)
	OnKeyChoiceSelect();                   // update key data
	OnSettingsChanged();                   // Enable "apply" button

	if(updateAllKeys)
	{
		const int numItems = m_lbnCommandKeys->GetItemCount();
		for(int i = 0; i < numItems; i++)
		{
			if(const auto cmd = static_cast<CommandID>(mfcless::GetItemData(m_lbnCommandKeys, i)); cmd != kcNull)
				m_lbnCommandKeys->SetItemText(i, 1, m_localCmdSet->GetKeyTextFromCommand(cmd));
		}
		UpdateNoteRepeatCheckbox();
	} else if(m_curCommand != kcNull)
	{
		m_lbnCommandKeys->SetItemText(m_lbnCommandKeys->GetSelectionMark(), 1, m_localCmdSet->GetKeyTextFromCommand(m_curCommand));
		if(mpt::is_in_range(m_curCommand, kcVPStartNotes, kcVPEndNotes))
			UpdateNoteRepeatCheckbox();
	}
}


void COptionsKeyboard::UpdateNoteRepeatCheckbox()
{
	uint32 state = uint32_max;
	for(CommandID cmd = kcVPStartNotes; cmd <= kcVPEndNotes && state != 2; cmd = static_cast<CommandID>(cmd + 1))
	{
		for(auto &kc : m_localCmdSet->GetKeyChoices(cmd))
		{
			const bool repeat = (kc.EventType() & kKeyEventRepeat);
			if(repeat && (state == uint32_max || state == 1))
				state = 1;
			else if(!repeat && (state == uint32_max || state == 0))
				state = 0;
			else
				state = 2;
		}
	}
	SetChecked(IDC_NOTESREPEAT, state);
}


void COptionsKeyboard::OnRestoreKeymapDropdown()
{
	ShowRestoreKeymapMenu();
	*result = 0;
}

void COptionsKeyboard::OnRestoreDefaultKeymap()
{
#if MPT_WINNT_AT_LEAST(MPT_WIN_VISTA)
	if((m_restoreDefaultButton->GetStyle() & BS_SPLITBUTTON) == BS_SPLITBUTTON)
	{
		OnRestoreMPTKeymap();
		return;
	}
#endif
	ShowRestoreKeymapMenu();
}


void COptionsKeyboard::ShowRestoreKeymapMenu()
{
	mfcless::Rect rect;
	m_restoreDefaultButton->GetWindowRect(rect);

	TPMPARAMS tpmParams{};
	tpmParams.cbSize = sizeof(TPMPARAMS);
	tpmParams.rcExclude = rect;

	mfcless::Menu menu;
	if(!menu.CreatePopupMenu())
		return;
	menu.AppendMenu(mfcless::menu::String, ID_KEYPRESET_MPT, UL_("&OpenMPT style"));
	menu.AppendMenu(mfcless::menu::String, ID_KEYPRESET_IT, UL_("&Impulse Tracker style"));
	menu.AppendMenu(mfcless::menu::String, ID_KEYPRESET_FT2, UL_("&Fast Tracker style"));
	menu.TrackPopupMenuEx(mfcless::menu::LeftAlign | mfcless::menu::TopAlign | mfcless::menu::LeftButton, rect.left, rect.bottom, this, &tpmParams);
}


void COptionsKeyboard::RestoreKeymap(KeyboardPreset preset)
{
	if(Reporting::Confirm("Discard all custom changes and restore default key configuration?", false, true, this) == cnfYes)
	{
		m_localCmdSet->LoadDefaultKeymap(preset);
		ForceUpdateGUI(true);
	}
}


int COptionsKeyboard::GetCategoryFromCommandID(CommandID command) const
{
	const InputTargetContext context = CCommandSet::ContextFromCommand(command);
	for(size_t cat = 0; cat < commandCategories.size(); cat++)
	{
		if(commandCategories[cat].id == context)
			return static_cast<int>(cat);
	}
	return -1;
}


OPENMPT_NAMESPACE_END
