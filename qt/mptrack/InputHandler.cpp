/*
 * InputHandler.cpp
 * ----------------
 * Purpose: Implementation of keyboard input handling, keymap loading, ...
 * Notes  : mfcless twin of upstream's mptrack/InputHandler.cpp. Key events
 *          come from Qt (Translate), with Windows' key codes (mfcless/Keyboard.h).
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "InputHandler.h"
#include "CommandSet.h"
#include "Mainfrm.h"
#include "Mptrack.h"
#include "resource.h"
#include "TrackerSettings.h"
#include "WindowMessages.h"
#include "../soundlib/MIDIEvents.h"
#include "mpt/fs/fs.hpp"
#include "mfcless/Keyboard.h"
#include "mfcless/Menus.h"
#include "mfcless/ModMessage.h"

#include <QtWidgets/QApplication>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QTextEdit>


OPENMPT_NAMESPACE_BEGIN


#define TRANSITIONBIT 0x8000
#define REPEATBIT 0x4000

CInputHandler::CInputHandler(QWidget *mainframe)
{
	m_pMainFrm = mainframe;

	//Init CommandSet and Load defaults
	m_activeCommandSet = std::make_unique<CCommandSet>();
	m_lastCommands.fill(kcNull);

	mpt::PathString defaultPath = theApp.GetConfigPath() + P_("Keybindings.mkb");

	const bool noExistingKbdFileSetting = TrackerSettings::Instance().m_szKbdFile.empty();

	// 1. Try to load keybindings from the path saved in the settings.
	// 2. If the setting doesn't exist or the loading fails, try to load from default location.
	// 3. If neither one of these worked, load default keybindings from resources.
	// 4. If there were no keybinding setting already, create a keybinding file to default location
	//    and set its path to settings.

	if(noExistingKbdFileSetting || !m_activeCommandSet->LoadFile(TrackerSettings::Instance().m_szKbdFile))
	{
		if(!mpt::native_fs{}.is_file(defaultPath) || !m_activeCommandSet->LoadFile(defaultPath))
		{
			// Load keybindings from resources.
			MPT_LOG_GLOBAL(LogDebug, "InputHandler", U_("Loading keybindings from resources\n"));
			m_activeCommandSet->LoadDefaultKeymap();
			if (noExistingKbdFileSetting)
				m_activeCommandSet->SaveFile(defaultPath);
		}
	}
	// We will only overwrite the default Keybindings.mkb file from now on.
	TrackerSettings::Instance().m_szKbdFile = defaultPath;

	//Get Keymap
	m_activeCommandSet->GenKeyMap(m_keyMap);
	SetupSpecialKeyInterception(); // Feature: use Windows keys as modifier keys, intercept special keys
}


CommandID CInputHandler::SendCommands(QWidget *wnd, const KeyMapRange &cmd)
{
	CommandID executeCommand = kcNull;
	if(wnd != nullptr)
	{
		// Some commands (e.g. open/close/document switching) may invalidate the key map and thus its iterators.
		// To avoid this problem, copy over the elements we are interested in before sending commands.
		std::vector<KeyMap::value_type> commands;
		commands.reserve(std::distance(cmd.first, cmd.second));
		for(auto i = cmd.first; i != cmd.second; i++)
		{
			commands.push_back(*i);
		}
		for(const auto &i : commands)
		{
			m_lastCommands[m_lastCommandPos] = i.second;
			m_lastCommandPos = (m_lastCommandPos + 1) % m_lastCommands.size();
			if(mfcless::SendModMessage(wnd, WM_MOD_KEYCOMMAND, i.second, i.first.AsLPARAM()) != kcNull)
			{
				// Command was handled, no need to let the OS handle the key
				executeCommand = i.second;
			}
		}
	}
	return executeCommand;
}


CommandID CInputHandler::KeyEvent(const InputTargetContext context, const KeyboardEvent &event, QWidget *pSourceWnd)
{
	if(IsKeyPressHandledByTextBox(event.key, QApplication::focusWidget()))
		return kcNull;

	KeyMapRange cmd = m_keyMap.equal_range(KeyCombination(context, GetModifierMask(), event.key, event.keyEventType));
	if(cmd.first != cmd.second && InterceptSpecialKeys(event))
		return kcDummyShortcut;

	if(pSourceWnd == nullptr)
		pSourceWnd = m_pMainFrm;	// By default, send command message to main frame.
	return SendCommands(pSourceWnd, cmd);
}


// Feature: use Windows keys as modifier keys, intercept special keys
bool CInputHandler::InterceptSpecialKeys(const KeyboardEvent &event)
{
	enum { VK_NonExistentKey = mfcless::vk::F24+1 };

	// mfcless: upstream sends fake key presses here, so that the Windows keys
	// do not open the Start menu and the lock keys do not light up. Desktops
	// on Linux leave both to the window manager.
	if(event.key == VK_NonExistentKey)
	{
		return true;
	}
	return false;
};

void CInputHandler::SetupSpecialKeyInterception()
{
	m_bInterceptWindowsKeys = m_bInterceptNumLock = m_bInterceptCapsLock = m_bInterceptScrollLock = false;
	for(const auto &i : m_keyMap)
	{
		MPT_ASSERT(i.second != kcNull);
		if(i.first.Modifier() == ModWin)
			m_bInterceptWindowsKeys = true;
		if(i.first.KeyCode() == mfcless::vk::NUMLOCK)
			m_bInterceptNumLock = true;
		if(i.first.KeyCode() == mfcless::vk::CAPITAL)
			m_bInterceptCapsLock = true;
		if(i.first.KeyCode() == mfcless::vk::SCROLL)
			m_bInterceptScrollLock = true;
	}
};


// Translate MIDI messages to shortcut commands
CommandID CInputHandler::HandleMIDIMessage(InputTargetContext context, uint32 message)
{
	KeyMapRange cmd = { m_keyMap.end(), m_keyMap.end() };
	auto byte1 = MIDIEvents::GetDataByte1FromEvent(message), byte2 = MIDIEvents::GetDataByte2FromEvent(message);
	switch(MIDIEvents::GetTypeFromEvent(message))
	{
	case MIDIEvents::evControllerChange:
		if(byte2 != 0)
		{
			// Only capture MIDI CCs for now. Some controllers constantly send some MIDI CCs with value 0
			// (e.g. the Roland D-50 sends CC123 whenenver all notes have been released), so we will ignore those.
			cmd = m_keyMap.equal_range(KeyCombination(context, ModMidi, byte1, kKeyEventDown));
			return SendCommands(m_pMainFrm, cmd);
		}
		break;

	case MIDIEvents::evNoteOff:
		byte2 = 0;
		[[fallthrough]];
	case MIDIEvents::evNoteOn:
		if(byte2 != 0)
		{
			cmd = m_keyMap.equal_range(KeyCombination(context, ModMidi, byte1 | 0x80, kKeyEventDown));
			return SendCommands(m_pMainFrm, cmd);
		} else
		{
			cmd = m_keyMap.equal_range(KeyCombination(context, ModMidi, byte1 | 0x80, kKeyEventUp));
			// If the key-down triggered a note, we still want that note to be stopped. So we always pretend that no key was assigned to this event
			SendCommands(m_pMainFrm, cmd);
		}
		break;

	default:
		break;
	}

	return kcNull;
}


int CInputHandler::GetKeyListSize(CommandID cmd) const
{
	return m_activeCommandSet->GetKeyListSize(cmd);
}


CInputHandler::KeyboardEvent CInputHandler::Translate(const mfcless::Message &msg)
{
	MPT_ASSERT(msg.message == mfcless::msg::KeyDown || msg.message == mfcless::msg::SysKeyDown || msg.message == mfcless::msg::KeyUp || msg.message == mfcless::msg::SysKeyUp);

	// mfcless: keys without a key code (VK_PACKET on Windows) already have the
	// code of the key typing their character (mfcless::KeyMessage).
	uint32 key = static_cast<uint32>(msg.wParam);

	const auto repCnt = static_cast<uint16>(msg.lParam & 0xFFFF);
	const auto flags = static_cast<uint16>((msg.lParam >> 16) & 0xFFFF);
	return {key, repCnt, flags, GetKeyEventType(flags)};
}


KeyEventType CInputHandler::GetKeyEventType(const mfcless::Message &msg)
{
	return GetKeyEventType(static_cast<uint32>((msg.lParam >> 16) & 0xFFFF));
}


KeyEventType CInputHandler::GetKeyEventType(uint32 nFlags)
{
	if (nFlags & TRANSITIONBIT)
	{
		// Key released
		return kKeyEventUp;
	} else if (nFlags & REPEATBIT)
	{
		// Key repeated
		return kKeyEventRepeat;
	} else
	{
		// New key down
		return kKeyEventDown;
	}
}


bool CInputHandler::SelectionPressed() const
{
	int nSelectionKeys = m_activeCommandSet->GetKeyListSize(kcSelect);
	KeyCombination key;

	const auto modifierMask = GetModifierMask();
	for (int k=0; k<nSelectionKeys; k++)
	{
		key = m_activeCommandSet->GetKey(kcSelect, k);
		if(modifierMask & key.Modifier())
			return true;
	}
	return false;
}


bool CInputHandler::ShiftPressed()
{
	return mfcless::IsKeyDownNow(mfcless::vk::SHIFT);
}


bool CInputHandler::CtrlPressed()
{
	return mfcless::IsKeyDownNow(mfcless::vk::CONTROL);
}


bool CInputHandler::AltPressed()
{
	return mfcless::IsKeyDownNow(mfcless::vk::MENU);
}


OrderTransitionMode CInputHandler::ModifierKeysToTransitionMode()
{
	const bool shift = ShiftPressed();
	const bool alt = AltPressed();
	if(shift && alt)
		return OrderTransitionMode::AtRowEnd;
	else if(!shift && alt)
		return OrderTransitionMode::AtBeatEnd;
	else if(shift && !alt)
		return OrderTransitionMode::AtMeasureEnd;
	else
		return OrderTransitionMode::AtPatternEnd;
}


void CInputHandler::Bypass(bool b)
{
	if(b)
		m_bypassCount++;
	else
		m_bypassCount--;
	MPT_ASSERT(m_bypassCount >= 0);
}


bool CInputHandler::IsBypassed() const
{
	return m_bypassCount > 0;
}


FlagSet<Modifiers> CInputHandler::GetModifierMask()
{
	uint8 keyStates[256];
	FlagSet<Modifiers> modifierMask = ModNone;
	if(QApplication::activeWindow() == nullptr)
		return modifierMask;
	mfcless::GetKeyboardState(keyStates);

	const bool distinguishModifiers = TrackerSettings::Instance().MiscDistinguishModifiers;
	if(keyStates[mfcless::vk::LCONTROL] & 0x80)
		modifierMask.set(ModCtrl);
	if(keyStates[mfcless::vk::RCONTROL] & 0x80)
		modifierMask.set(distinguishModifiers ? ModRCtrl : ModCtrl);
	if(keyStates[mfcless::vk::LSHIFT] & 0x80)
		modifierMask.set(ModShift);
	if(keyStates[mfcless::vk::RSHIFT] & 0x80)
		modifierMask.set(distinguishModifiers ? ModRShift : ModShift);
	if(keyStates[mfcless::vk::LMENU] & 0x80)
		modifierMask.set(ModAlt);
	if(keyStates[mfcless::vk::RMENU] & 0x80)
		modifierMask.set(distinguishModifiers ? ModRAlt : ModAlt);
	if(keyStates[mfcless::vk::LWIN] & 0x80)
		modifierMask.set(ModWin);
	if(keyStates[mfcless::vk::RWIN] & 0x80)
		modifierMask.set(ModWin);
	return modifierMask;
}


mpt::ustring CInputHandler::GetKeyTextFromCommand(CommandID c, const mpt::uchar *prependText) const
{
	mpt::ustring s;
	if(prependText != nullptr)
	{
		s = prependText;
		s += UC_('\t');
	}
	s += m_activeCommandSet->GetKeyTextFromCommand(c, 0);
	return s;
}


mpt::ustring CInputHandler::GetMenuText(uint32 id) const
{
	static constexpr std::tuple<uint32, CommandID, const mpt::uchar *> MenuItems[] =
	{
		{ ID_FILE_NEW,                      kcFileNew,           UL_("&New") },
		{ ID_FILE_OPEN,                     kcFileOpen,          UL_("&Open...") },
		{ ID_FILE_OPENTEMPLATE,             kcNull,              UL_("Open &Template") },
		{ ID_FILE_OPENTEMPLATE_LASTINRANGE, kcFileOpenTemplate,  UL_("&Browse...") },
		{ ID_FILE_CLOSE,                    kcFileClose,         UL_("&Close") },
		{ ID_FILE_CLOSEALL,                 kcFileCloseAll,      UL_("C&lose All") },
		{ ID_FILE_APPENDMODULE,             kcFileAppend,        UL_("Appen&d Module...") },
		{ ID_FILE_SAVE,                     kcFileSave,          UL_("&Save") },
		{ ID_FILE_SAVE_AS,                  kcFileSaveAs,        UL_("Save &As...") },
		{ ID_FILE_SAVE_COPY,                kcFileSaveCopy,      UL_("Save Cop&y...") },
		{ ID_FILE_SAVEASTEMPLATE,           kcFileSaveTemplate,  UL_("Sa&ve as Template") },
		{ ID_FILE_SAVEASWAVE,               kcFileSaveAsWave,    UL_("Stream Export (&WAV, FLAC, MP3, etc.)...") },
		{ ID_FILE_SAVEMIDI,                 kcFileSaveMidi,      UL_("Export as M&IDI...") },
		{ ID_FILE_SAVEOPL,                  kcFileSaveOPL,       UL_("Export O&PL Register Dump...") },
		{ ID_FILE_SAVECOMPAT,               kcFileExportCompat,  UL_("Compatibility &Export...") },
		{ ID_IMPORT_MIDILIB,                kcFileImportMidiLib, UL_("Import &MIDI Library...") },
		{ ID_ADD_SOUNDBANK,                 kcFileAddSoundBank,  UL_("Add Sound &Bank...") },

		{ ID_PLAYER_PLAY,          kcPlayPauseSong,      UL_("Pause / &Resume") },
		{ ID_PLAYER_PLAYFROMSTART, kcPlaySongFromStart,  UL_("&Play from Start") },
		{ ID_PLAYER_STOP,          kcStopSong,           UL_("&Stop") },
		{ ID_PLAYER_PAUSE,         kcPauseSong,          UL_("P&ause") },
		{ ID_MIDI_RECORD,          kcMidiRecord,         UL_("&MIDI Record") },
		{ ID_ESTIMATESONGLENGTH,   kcEstimateSongLength, UL_("&Estimate Song Length") },
		{ ID_APPROX_BPM,           kcApproxRealBPM,      UL_("Approximate Real &BPM") },

		{ ID_EDIT_UNDO,                  kcEditUndo,      UL_("&Undo") },
		{ ID_EDIT_REDO,                  kcEditRedo,      UL_("&Redo") },
		{ ID_EDIT_CUT,                   kcEditCut,       UL_("Cu&t") },
		{ ID_EDIT_COPY,                  kcEditCopy,      UL_("&Copy") },
		{ ID_EDIT_PASTE,                 kcEditPaste,     UL_("&Paste") },
		{ ID_EDIT_SELECT_ALL,            kcEditSelectAll, UL_("Select &All") },
		{ ID_EDIT_CLEANUP,               kcNull,          UL_("C&leanup") },
		{ ID_EDIT_FIND,                  kcEditFind,      UL_("&Find / Replace") },
		{ ID_EDIT_FINDNEXT,              kcEditFindNext,  UL_("Find &Next") },
		{ ID_EDIT_GOTO_MENU,             kcPatternGoto,   UL_("&Goto") },
		{ ID_EDIT_SPLITKEYBOARDSETTINGS, kcShowSplitKeyboardSettings, UL_("Split &Keyboard Settings") },
		// "Paste Special" sub menu
		{ ID_EDIT_PASTE_SPECIAL,    kcEditMixPaste,         UL_("&Mix Paste") },
		{ ID_EDIT_MIXPASTE_ITSTYLE, kcEditMixPasteITStyle,  UL_("M&ix Paste (IT Style)") },
		{ ID_EDIT_PASTEFLOOD,       kcEditPasteFlood,       UL_("Paste Fl&ood") },
		{ ID_EDIT_PUSHFORWARDPASTE, kcEditPushForwardPaste, UL_("&Push Forward Paste (Insert)") },

		{ ID_VIEW_GLOBALS,        kcViewGeneral,            UL_("&General") },
		{ ID_VIEW_SAMPLES,        kcViewSamples,            UL_("&Samples") },
		{ ID_VIEW_PATTERNS,       kcViewPattern,            UL_("&Patterns") },
		{ ID_VIEW_INSTRUMENTS,    kcViewInstruments,        UL_("&Instruments") },
		{ ID_VIEW_COMMENTS,       kcViewComments,           UL_("&Comments") },
		{ ID_VIEW_OPTIONS,        kcViewOptions,            UL_("S&etup") },
		{ ID_VIEW_TOOLBAR,        kcViewMain,               UL_("Show &Main Toolbar") },
		{ IDD_TREEVIEW,           kcViewTree,               UL_("Show &Tree View") },
		{ ID_PLUGIN_SETUP,        kcViewAddPlugin,          UL_("Pl&ugin Manager") },
		{ ID_CHANNEL_MANAGER,     kcViewChannelManager,     UL_("Ch&annel Manager") },
		{ ID_CLIPBOARD_MANAGER,   kcToggleClipboardManager, UL_("C&lipboard Manager") },
		{ ID_VIEW_SONGPROPERTIES, kcViewSongProperties,     UL_("Song P&roperties") },
		{ ID_PATTERN_MIDIMACRO,   kcShowMacroConfig,        UL_("&Zxx Macro Configuration") },
		{ ID_VIEW_MIDIMAPPING,    kcViewMIDImapping,        UL_("&MIDI Mapping") },
		{ ID_VIEW_EDITHISTORY,    kcViewEditHistory,        UL_("Edit &History") },
		// Help submenu
		{ ID_HELPSHOW,        kcHelp, UL_("&Help") },
		{ ID_EXAMPLE_MODULES, kcNull, UL_("&Example Modules") },
	};

	for(const auto & [cmdID, command, text] : MenuItems)
	{
		if(id == cmdID)
		{
			if(command != kcNull)
				return GetKeyTextFromCommand(command, text);
			else
				return text;
		}
	}
	MPT_ASSERT_NOTREACHED();
	return UL_("Unknown Item");
}


void CInputHandler::UpdateMainMenu()
{
	auto *frame = qobject_cast<QMainWindow *>(m_pMainFrm);
	QMenuBar *pMenu = frame ? frame->menuBar() : nullptr;
	if (!pMenu || pMenu->actions().isEmpty() || !pMenu->actions()[0]->menu()) return;

	// The "New" sub menu
	if(QMenu *fileMenu = pMenu->actions()[0]->menu(); !fileMenu->actions().isEmpty())
		fileMenu->actions()[0]->setText(mfcless::Text(GetMenuText(ID_FILE_NEW)));
	static constexpr int MenuItems[] =
	{
		ID_FILE_OPEN,
		ID_FILE_OPENTEMPLATE_LASTINRANGE,
		ID_FILE_APPENDMODULE,
		ID_FILE_CLOSE,
		ID_FILE_CLOSEALL,
		ID_FILE_SAVE,
		ID_FILE_SAVE_AS,
		ID_FILE_SAVEASWAVE,
		ID_FILE_SAVEMIDI,
		ID_FILE_SAVECOMPAT,
		ID_IMPORT_MIDILIB,
		ID_ADD_SOUNDBANK,

		ID_PLAYER_PLAY,
		ID_PLAYER_PLAYFROMSTART,
		ID_PLAYER_STOP,
		ID_PLAYER_PAUSE,
		ID_MIDI_RECORD,
		ID_ESTIMATESONGLENGTH,
		ID_APPROX_BPM,

		ID_EDIT_UNDO,
		ID_EDIT_REDO,
		ID_EDIT_CUT,
		ID_EDIT_COPY,
		ID_EDIT_PASTE,
		ID_EDIT_PASTE_SPECIAL,
		ID_EDIT_MIXPASTE_ITSTYLE,
		ID_EDIT_PASTEFLOOD,
		ID_EDIT_PUSHFORWARDPASTE,
		ID_EDIT_SELECT_ALL,
		ID_EDIT_FIND,
		ID_EDIT_FINDNEXT,
		ID_EDIT_GOTO_MENU,
		ID_EDIT_SPLITKEYBOARDSETTINGS,

		ID_VIEW_GLOBALS,
		ID_VIEW_SAMPLES,
		ID_VIEW_PATTERNS,
		ID_VIEW_INSTRUMENTS,
		ID_VIEW_COMMENTS,
		ID_VIEW_TOOLBAR,
		IDD_TREEVIEW,
		ID_VIEW_OPTIONS,
		ID_PLUGIN_SETUP,
		ID_CHANNEL_MANAGER,
		ID_CLIPBOARD_MANAGER,
		ID_VIEW_SONGPROPERTIES,
		ID_VIEW_SONGPROPERTIES,
		ID_PATTERN_MIDIMACRO,
		ID_VIEW_EDITHISTORY,
		ID_HELPSHOW,
	};
	for(const auto id : MenuItems)
	{
		if(QAction *action = mfcless::FindMenuAction(pMenu, id))
			action->setText(mfcless::Text(GetMenuText(id)));
	}
}


void CInputHandler::SetNewCommandSet(const CCommandSet &newSet)
{
	m_activeCommandSet->Copy(newSet);
	m_activeCommandSet->GenKeyMap(m_keyMap);
	SetupSpecialKeyInterception(); // Feature: use Windows keys as modifier keys, intercept special keys
	UpdateMainMenu();
}


bool CInputHandler::SetEffectLetters(const CModSpecifications &modSpecs)
{
	MPT_LOG_GLOBAL(LogDebug, "InputHandler", U_("Changing command set."));
	bool retval = m_activeCommandSet->QuickChange_SetEffects(modSpecs);
	if(retval) m_activeCommandSet->GenKeyMap(m_keyMap);
	return retval;
}


bool CInputHandler::IsKeyPressHandledByTextBox(uint32 key, QWidget *hWnd) const
{
	if(hWnd == nullptr)
		return false;
	
	// mfcless: Windows' "Edit" class: line edits (also those of combo boxes
	// and spin boxes) and multi-line text fields.
	const bool multiLine = qobject_cast<QPlainTextEdit *>(hWnd) || qobject_cast<QTextEdit *>(hWnd);
	const bool textboxHasFocus = multiLine || qobject_cast<QLineEdit *>(hWnd);
	if(!textboxHasFocus)
		return false;

	// Alpha-numerics (only shift or no modifier):
	const auto modifierMask = GetModifierMask();
	if(!modifierMask.test_any_except(ModShift))
	{
		if((key >= 'A' && key <= 'Z') || (key >= '0' && key <= '9')
		   || (key >= mfcless::vk::MULTIPLY && key <= mfcless::vk::DIVIDE) || key == mfcless::vk::SPACE || key == mfcless::vk::CAPITAL
		   || (key >= mfcless::vk::OEM_1 && key <= mfcless::vk::OEM_3) || (key >= mfcless::vk::OEM_4 && key <= mfcless::vk::OEM_8))
			return true;
		if((key >= mfcless::vk::NUMPAD0 && key <= mfcless::vk::NUMPAD9) && modifierMask == ModNone)
			return true;
		if(key == mfcless::vk::RETURN && multiLine)
			return true;
	}

	// Navigation (any modifier except Alt without any other modifiers):
	if(modifierMask != ModAlt)
	{
		if(key == mfcless::vk::LEFT || key == mfcless::vk::RIGHT || key == mfcless::vk::UP || key == mfcless::vk::DOWN
		   || key == mfcless::vk::HOME || key == mfcless::vk::END || key == mfcless::vk::DELETE || key == mfcless::vk::INSERT || key == mfcless::vk::BACK)
			return true;
	}

	// Copy paste etc..
	if(modifierMask == ModCtrl)
	{
		if(key == 'Y' || key == 'Z' || key == 'X' ||  key == 'C' || key == 'V' || key == 'A')
			return true;
	}

	return false;
}


BypassInputHandler::BypassInputHandler()
{
	if(CMainFrame::GetInputHandler())
	{
		bypassed = true;
		CMainFrame::GetInputHandler()->Bypass(true);
	}
}


BypassInputHandler::~BypassInputHandler()
{
	if(bypassed)
	{
		CMainFrame::GetInputHandler()->Bypass(false);
		bypassed = false;
	}
}

OPENMPT_NAMESPACE_END
