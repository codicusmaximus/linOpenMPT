/*
 * AbstractVstEditor.cpp
 * ---------------------
 * Purpose: Common plugin editor interface class. This code is shared between custom and default plugin user interfaces.
 * Notes  : mfcless twin of upstream's mptrack/AbstractVstEditor.cpp.
 *          Draft from qt/tools/twin.py, not yet edited by hand.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "AbstractVstEditor.h"
#include "mfcless/Widgets.h"
#include "Clipboard.h"
#include "dlg_misc.h"
#include "Globals.h"
#include "InputHandler.h"
#include "MIDIMacros.h"
#include "Mainfrm.h"
#include "Mptrack.h"
#include "Reporting.h"
#include "resource.h"
#include "VstPresets.h"
#include "Vstplug.h"
#include "WindowMessages.h"
#include "../common/FileReader.h"
#include "../common/mptStringBuffer.h"
#include "../soundlib/Sndfile.h"
#include "../soundlib/mod_specifications.h"
#include "../soundlib/plugins/PlugInterface.h"
#include "../soundlib/plugins/PluginManager.h"

#include <sstream>


OPENMPT_NAMESPACE_BEGIN


#ifndef NO_PLUGINS

CAbstractVstEditor::WindowSizeAdjuster::WindowSizeAdjuster(QWidget &wnd)
	: m_wnd(wnd)
{
	MENUBARINFO mbi{};
	mbi.cbSize = sizeof(mbi);
	if(GetMenuBarInfo(m_wnd, OBJID_MENU, 0, &mbi))
		m_menuHeight = (mbi.rcBar.bottom - mbi.rcBar.top);
}

CAbstractVstEditor::WindowSizeAdjuster::~WindowSizeAdjuster()
{
	// Extend window height by the menu size if it changed
	MENUBARINFO mbi{};
	mbi.cbSize = sizeof(mbi);
	if(GetMenuBarInfo(m_wnd, OBJID_MENU, 0, &mbi))
	{
		mfcless::Rect windowRect;
		m_wnd.GetWindowRect(&windowRect);
		windowRect.bottom += (mbi.rcBar.bottom - mbi.rcBar.top) - m_menuHeight;
		m_wnd.SetWindowPos(nullptr, 0, 0,
			windowRect.Width(), windowRect.Height(),
			mfcless::swp::NoZOrder | mfcless::swp::NoMove | mfcless::swp::NoActivate);
	}
}

#define PRESETS_PER_COLUMN 32
#define PRESETS_PER_GROUP 128

uint32 CAbstractVstEditor::m_clipboardFormat = mfcless::clip::RegisterFormat(QStringLiteral("VST Preset Data"));

void CAbstractVstEditor::RegisterMessageMap()
{
	ResizableDialog::RegisterMessageMap();
	// ON_WM_CLOSE() (mfcless::Window)
	// ON_WM_INITMENU(): by hand (Qt event override)
	// ON_WM_MENUSELECT(): by hand (Qt event override)
	// ON_WM_ACTIVATE(): by hand (Qt event override)
	// ON_WM_DROPFILES() (mfcless::Window)
	// ON_WM_MOVE() (mfcless::Window)
	// ON_WM_NCLBUTTONDBLCLK(): by hand (Qt event override)
	AddCommand(ID_EDIT_COPY, &CAbstractVstEditor::OnCopyParameters);
	AddCommand(ID_EDIT_PASTE, &CAbstractVstEditor::OnPasteParameters);
	AddCommand(ID_PRESET_LOAD, &CAbstractVstEditor::OnLoadPreset);
	AddCommand(ID_PLUG_BYPASS, &CAbstractVstEditor::OnBypassPlug);
	AddCommand(ID_PLUG_RECORDAUTOMATION, &CAbstractVstEditor::OnRecordAutomation);
	AddCommand(ID_PLUG_RECORD_MIDIOUT, &CAbstractVstEditor::OnRecordMIDIOut);
	AddCommand(ID_PLUG_PASSKEYS, &CAbstractVstEditor::OnPassKeypressesToPlug);
	AddCommand(ID_PRESET_SAVE, &CAbstractVstEditor::OnSavePreset);
	AddCommand(ID_PRESET_RANDOM, &CAbstractVstEditor::OnRandomizePreset);
	AddCommand(ID_RENAME_PLUGIN, &CAbstractVstEditor::OnRenamePlugin);
	AddCommand(ID_PREVIOUSVSTPRESET, &CAbstractVstEditor::OnSetPreviousVSTPreset);
	AddCommand(ID_NEXTVSTPRESET, &CAbstractVstEditor::OnSetNextVSTPreset);
	AddCommand(ID_VSTPRESETBACKWARDJUMP, &CAbstractVstEditor::OnVSTPresetBackwardJump);
	AddCommand(ID_VSTPRESETFORWARDJUMP, &CAbstractVstEditor::OnVSTPresetForwardJump);
	AddCommand(ID_VSTPRESETNAME, &CAbstractVstEditor::OnVSTPresetRename);
	AddCommand(ID_PLUGINTOINSTRUMENT, &CAbstractVstEditor::OnCreateInstrument);
	AddCommandRange(ID_PRESET_SET, ID_PRESET_SET + PRESETS_PER_GROUP, &CAbstractVstEditor::OnSetPreset);
	AddMessage(WM_MOD_MIDIMSG, &CAbstractVstEditor::OnMidiMsg);
	AddMessage(WM_MOD_KEYCOMMAND, &CAbstractVstEditor::OnCustomKeyMsg);  //rewbs.customKeys
	AddCommandRange(ID_PLUGSELECT, ID_PLUGSELECT + MAX_MIXPLUGINS, &CAbstractVstEditor::OnToggleEditor);  //rewbs.patPlugName
	AddCommandRange(ID_SELECTINST, ID_SELECTINST + MAX_INSTRUMENTS, &CAbstractVstEditor::OnSetInputInstrument);  //rewbs.patPlugName
	AddCommandRange(ID_LEARN_MACRO_FROM_PLUGGUI, ID_LEARN_MACRO_FROM_PLUGGUI + kSFxMacros, &CAbstractVstEditor::PrepareToLearnMacro);
}


CAbstractVstEditor::CAbstractVstEditor(IMixPlugin &plugin)
	: m_VstPlugin(plugin)
{
	m_Menu->LoadMenu(IDR_VSTMENU);
	m_nInstrument = GetBestInstrumentCandidate();
}


CAbstractVstEditor::~CAbstractVstEditor()
{
	m_VstPlugin.m_pEditor = nullptr;
}


void CAbstractVstEditor::PostNcDestroy()
{
	ResizableDialog::PostNcDestroy();
	delete this;
}


void CAbstractVstEditor::OnNcLButtonDblClk(uint32 nHitTest, mfcless::Point point)
{
	ResizableDialog::OnNcLButtonDblClk(nHitTest, point);
	// Double click on title bar = reduce plugin window to non-client area
	if(nHitTest == HTCAPTION)
	{
		mfcless::Rect rcWnd, rcClient;
		GetWindowRect(&rcWnd);
		if(!m_isMinimized)
		{
			// When minimizing, remove the client area
			GetClientRect(&rcClient);
			m_clientHeight = rcClient.Height();
		}
		m_isMinimized = !m_isMinimized;
		m_clientHeight = -m_clientHeight;
		int rcHeight = rcWnd.Height() + m_clientHeight;

		EnableAutoLayout(!m_isMinimized);
		SetWindowPos(nullptr, 0, 0,
			rcWnd.Width(), rcHeight,
			mfcless::swp::NoZOrder | mfcless::swp::NoMove);
	}
}


void CAbstractVstEditor::OnActivate(uint32 nState, QWidget *pWndOther, bool bMinimized)
{
	ResizableDialog::OnActivate(nState, pWndOther, bMinimized);
	if(nState != WA_INACTIVE)
	{
		auto callback = [&plugin = m_VstPlugin](mpt::const_byte_span sysex) { plugin.MidiSend(sysex); };
		CMainFrame::GetMainFrame()->SetMidiRecordWnd(GetSafeHwnd(), callback);
	}
}


intptr_t CAbstractVstEditor::OnMidiMsg(uintptr_t midiData, intptr_t sender)
{
	CModDoc *modDoc = m_VstPlugin.GetModDoc();
	if(modDoc != nullptr && sender != reinterpret_cast<intptr_t>(&m_VstPlugin))
	{
		if(!CheckInstrument(m_nInstrument))
			m_nInstrument = GetBestInstrumentCandidate();
		modDoc->ProcessMIDI((uint32)midiData, 0, m_nInstrument, &m_VstPlugin, kCtxVSTGUI);
		return 1;
	}
	return 0;
}


// Drop files from Windows
void CAbstractVstEditor::OnDropFiles(HDROP hDropInfo)
{
	const uint32 nFiles = ::DragQueryFileW(hDropInfo, (uint32)-1, NULL, 0);
	CMainFrame::GetMainFrame()->SetForegroundWindow();
	for(uint32 f = 0; f < nFiles; f++)
	{
		uint32 size = ::DragQueryFile(hDropInfo, f, nullptr, 0) + 1;
		std::vector<mpt::uchar> fileName(size, u8'\0');
		if(::DragQueryFile(hDropInfo, f, fileName.data(), size))
		{
			m_VstPlugin.LoadProgram(mpt::PathString::FromUnicode(fileName.data()));
		}
	}
	::DragFinish(hDropInfo);
}


void CAbstractVstEditor::OnLoadPreset()
{
	if(m_VstPlugin.LoadProgram())
	{
		UpdatePresetMenu(true);
		UpdatePresetField();
	}
}


void CAbstractVstEditor::OnSavePreset()
{
	m_VstPlugin.SaveProgram();
}


void CAbstractVstEditor::OnCopyParameters()
{
	if(CMainFrame::GetMainFrame() == nullptr) return;

	mfcless::BeginWaitCursor();
	std::ostringstream f(std::ios::out | std::ios::binary);
	if(VSTPresets::SaveFile(f, m_VstPlugin, false))
	{
		const std::string data = f.str();
		Clipboard clipboard(m_clipboardFormat, data.length());
		if(auto dst = clipboard.As<char>())
		{
			memcpy(dst, data.data(), data.length());
		}
	}
	mfcless::EndWaitCursor();
}


void CAbstractVstEditor::OnPasteParameters()
{
	if(CMainFrame::GetMainFrame() == nullptr) return;

	mfcless::BeginWaitCursor();
	Clipboard clipboard(m_clipboardFormat);
	if(auto data = clipboard.Get(); data.data())
	{
		FileReader file(data);
		VSTPresets::ErrorCode error = VSTPresets::LoadFile(file, m_VstPlugin);
		clipboard.Close();

		if(error == VSTPresets::noError)
		{
			const CSoundFile &sndFile = m_VstPlugin.GetSoundFile();
			CModDoc *pModDoc;
			if(sndFile.GetModSpecifications().supportsPlugins && (pModDoc = sndFile.GetpModDoc()) != nullptr)
			{
				pModDoc->SetModified();
			}
			WindowSizeAdjuster adjuster(*this);
			UpdatePresetField();
		} else
		{
			Reporting::Error(VSTPresets::GetErrorMessage(error));
		}
	}
	mfcless::EndWaitCursor();
}


void CAbstractVstEditor::OnRandomizePreset()
{
	static double randomFactor = 10.0;
	CInputDlg dlg(this, UL_("Input parameter randomization amount (0 = no change, 100 = completely random)"), 0.0, 100.0, randomFactor);
	if(dlg.DoModal() == IDOK)
	{
		randomFactor = dlg.resultAsDouble;
		PlugParamValue factor = PlugParamValue(randomFactor / 100.0);
		PlugParamIndex numParams = m_VstPlugin.GetNumVisibleParameters();
		for(PlugParamIndex p = 0; p < numParams; p++)
		{
			PlugParamValue val = m_VstPlugin.GetParameter(p);
			val += mpt::random(theApp.PRNG(), PlugParamValue(-1.0), PlugParamValue(1.0)) * factor;
			Limit(val, 0.0f, 1.0f);
			m_VstPlugin.SetParameter(p, val);
		}
		UpdateParamDisplays();
	}
}


void CAbstractVstEditor::OnRenamePlugin()
{
	auto &sndFile = m_VstPlugin.GetSoundFile();
	auto &plugin = sndFile.m_MixPlugins[m_VstPlugin.m_nSlot];

	CInputDlg dlg(this, UL_("New name for this plugin instance:"), mpt::ToUnicode(plugin.GetName()), static_cast<int32>(std::size(plugin.Info.szName.buf)));
	if(dlg.DoModal() == IDOK)
	{
		if(dlg.resultAsString != mpt::ToUnicode(plugin.GetName()))
		{
			plugin.Info.szName = mpt::ToCharset(mpt::Charset::Locale, dlg.resultAsString);
			if(auto *modDoc = sndFile.GetpModDoc(); modDoc != nullptr)
			{
				if(sndFile.GetModSpecifications().supportsPlugins)
					modDoc->SetModified();
				modDoc->UpdateAllViews(nullptr, PluginHint(m_VstPlugin.m_nSlot + 1).Info().Names(), this);
			}
			SetTitle();
		}
	}
}


bool CAbstractVstEditor::OpenEditor(QWidget *)
{
	ModifyStyleEx(0, WS_EX_ACCEPTFILES);
	RestoreWindowPos();
	SetTitle();
	SetupMenu();
	ShowWindow(true);
	return true;
}


void CAbstractVstEditor::DoClose()
{
	StoreWindowPos();
	m_presetMenuGroup.clear();
	DestroyWindow();
}


void CAbstractVstEditor::SetupMenu(bool force)
{
	WindowSizeAdjuster adjuster(*this);
	SetMenu(m_Menu);
	UpdatePresetMenu(force);
	UpdateInputMenu();
	UpdateOutputMenu();
	UpdateMacroMenu();
	UpdateOptionsMenu();
	UpdatePresetField();
}


void CAbstractVstEditor::UpdatePresetField()
{
	if(m_VstPlugin.GetNumPrograms() > 0)
	{
		if(m_Menu->GetMenuItemCount() < 5)
		{
			m_Menu->AppendMenu(mfcless::menu::ByPosition, ID_VSTPRESETBACKWARDJUMP, UL_("<<"));
			m_Menu->AppendMenu(mfcless::menu::ByPosition, ID_PREVIOUSVSTPRESET, UL_("<"));
			m_Menu->AppendMenu(mfcless::menu::ByPosition, ID_NEXTVSTPRESET, UL_(">"));
			m_Menu->AppendMenu(mfcless::menu::ByPosition, ID_VSTPRESETFORWARDJUMP, UL_(">>"));
			m_Menu->AppendMenu(mfcless::menu::ByPosition|mfcless::menu::Disabled, ID_VSTPRESETNAME, UL_(""));
		}

		mpt::ustring programName = m_VstPlugin.GetFormattedProgramName(m_VstPlugin.GetCurrentProgram());
		programName.Replace(UL_("&"), UL_("&&"));
		m_Menu->ModifyMenu(8, mfcless::menu::ByPosition, ID_VSTPRESETNAME, programName);
	}

	DrawMenuBar();
}


void CAbstractVstEditor::OnSetPreset(uint32 nID)
{
	SetPreset(nID - ID_PRESET_SET + m_currentPresetMenu * PRESETS_PER_GROUP);
}


void CAbstractVstEditor::OnSetPreviousVSTPreset()
{
	SetPreset(m_VstPlugin.GetCurrentProgram() - 1);
}


void CAbstractVstEditor::OnSetNextVSTPreset()
{
	SetPreset(m_VstPlugin.GetCurrentProgram() + 1);
}


void CAbstractVstEditor::OnVSTPresetBackwardJump()
{
	SetPreset(std::max(0, m_VstPlugin.GetCurrentProgram() - 10));
}


void CAbstractVstEditor::OnVSTPresetForwardJump()
{
	SetPreset(std::min(m_VstPlugin.GetCurrentProgram() + 10, m_VstPlugin.GetNumPrograms() - 1));
}


void CAbstractVstEditor::SetPreset(int32 preset)
{
	if(preset >= 0 && preset < m_VstPlugin.GetNumPrograms())
	{
		m_VstPlugin.SetCurrentProgram(preset);
		WindowSizeAdjuster adjuster(*this);
		UpdatePresetField();

		if(m_VstPlugin.GetSoundFile().GetModSpecifications().supportsPlugins)
		{
			m_VstPlugin.GetModDoc()->SetModified();
		}
	}
}


void CAbstractVstEditor::OnVSTPresetRename()
{
	auto currentName = m_VstPlugin.GetCurrentProgramName();
	CInputDlg dlg(this, UL_("New program name:"), currentName);
	if(dlg.DoModal() == IDOK)
	{
		m_VstPlugin.SetCurrentProgramName(dlg.resultAsString);
		if(m_VstPlugin.GetCurrentProgramName() != currentName)
		{
			m_VstPlugin.SetModified();
			WindowSizeAdjuster adjuster(*this);
			UpdatePresetField();
			UpdatePresetMenu(true);
		}
	}
}


void CAbstractVstEditor::OnBypassPlug()
{
	m_VstPlugin.ToggleBypass();
	if(m_VstPlugin.GetSoundFile().GetModSpecifications().supportsPlugins)
	{
		m_VstPlugin.GetModDoc()->SetModified();
	}
	SetTitle();
}


void CAbstractVstEditor::OnRecordAutomation()
{
	m_VstPlugin.m_recordAutomation = !m_VstPlugin.m_recordAutomation;
}


void CAbstractVstEditor::OnRecordMIDIOut()
{
	m_VstPlugin.m_recordMIDIOut = !m_VstPlugin.m_recordMIDIOut;
}


void CAbstractVstEditor::OnPassKeypressesToPlug()
{
	m_VstPlugin.m_passKeypressesToPlug  = !m_VstPlugin.m_passKeypressesToPlug;
}


bool CAbstractVstEditor::PreTranslateMessage(mfcless::Message *msg)
{
	if(msg && HandleKeyMessage(*msg))
		return true;

	return ResizableDialog::PreTranslateMessage(msg);
}


bool CAbstractVstEditor::HandleKeyMessage(MSG &msg, bool handleGlobal)
{
	if(m_VstPlugin.m_passKeypressesToPlug)
		return false;
	if(msg.message != mfcless::msg::SysKeyUp && msg.message != mfcless::msg::KeyUp && msg.message != mfcless::msg::SysKeyDown && msg.message != mfcless::msg::KeyDown)
		return false;

	CInputHandler *ih = CMainFrame::GetInputHandler();
	if(ih->IsKeyPressHandledByTextBox(static_cast<uint32>(msg.wParam), mfcless::GetFocus()))
		return false;

	const auto event = ih->Translate(msg);

	// If we successfully mapped to a command and plug does not listen for keypresses, no need to pass message on.
	if(ih->KeyEvent(kCtxVSTGUI, event, this) != kcNull)
		return true;

	if(handleGlobal && HandleGlobalKeyMessage(msg))
		return true;

	// Don't forward key repeats if plug does not listen for keypresses
	// (avoids system beeps on note hold)
	if(event.keyEventType == kKeyEventRepeat)
		return true;
	
	return false;
}


void CAbstractVstEditor::UpdateView(UpdateHint hint)
{
	if(!hint.GetType()[HINT_PLUGINNAMES | HINT_MIXPLUGINS])
		return;

	PLUGINDEX hintPlug = hint.ToType<PluginHint>().GetPlugin();
	if(hintPlug > 0 && (hintPlug - 1) != m_VstPlugin.GetSlot())
		return;

	SetTitle();
}


void CAbstractVstEditor::SetTitle()
{
	if(m_VstPlugin.m_pMixStruct)
	{
		mpt::ustring title = MPT_UFORMAT("FX {}: ")(mpt::ufmt::dec0<2>(m_VstPlugin.m_nSlot + 1));

		bool hasCustomName = (m_VstPlugin.m_pMixStruct->GetName() != U_("")) && (m_VstPlugin.m_pMixStruct->GetName() != m_VstPlugin.m_pMixStruct->GetLibraryName());
		if(hasCustomName)
			title += mpt::ToUnicode(m_VstPlugin.m_pMixStruct->GetName()) + UL_(" (");
		title += mpt::ToUnicode(m_VstPlugin.m_pMixStruct->GetLibraryName());
		if(hasCustomName)
			title += UL_(")");

#ifdef MPT_WITH_VST
		const CVstPlugin *vstPlugin = dynamic_cast<CVstPlugin *>(&m_VstPlugin);
		if(vstPlugin != nullptr && vstPlugin->isBridged)
			title += MPT_UFORMAT(" ({} Bridged)")(m_VstPlugin.GetPluginFactory().GetDllArchNameUser());
#endif // MPT_WITH_VST

		if(m_VstPlugin.IsBypassed())
			title += UL_(" - Bypass");

		SetWindowText(title);
	}
}


intptr_t CAbstractVstEditor::OnCustomKeyMsg(uintptr_t wParam, intptr_t /*lParam*/)
{
	switch(wParam)
	{
		case kcVSTGUIPrevPreset:			OnSetPreviousVSTPreset(); return wParam;
		case kcVSTGUIPrevPresetJump:		OnVSTPresetBackwardJump(); return wParam;
		case kcVSTGUINextPreset:			OnSetNextVSTPreset(); return wParam;
		case kcVSTGUINextPresetJump:		OnVSTPresetForwardJump(); return wParam;
		case kcVSTGUIRandParams:			OnRandomizePreset() ; return wParam;
		case kcVSTGUIToggleRecordParams:	OnRecordAutomation(); return wParam;
		case kcVSTGUIToggleSendKeysToPlug:	OnPassKeypressesToPlug(); return wParam;
		case kcVSTGUIBypassPlug:			OnBypassPlug(); return wParam;
	}
	if (wParam >= kcVSTGUIStartNotes && wParam <= kcVSTGUIEndNotes)
	{
		if(ValidateCurrentInstrument())
		{
			CModDoc *pModDoc = m_VstPlugin.GetModDoc();
			const ModCommand::NOTE note = pModDoc->GetNoteWithBaseOctave(static_cast<int>(wParam - kcVSTGUIStartNotes), m_nInstrument);
			if(ModCommand::IsNote(note))
			{
				pModDoc->PlayNote(PlayNoteParam(note).Instrument(m_nInstrument), &m_noteChannel);
			}
		}
		return wParam;
	}
	if (wParam >= kcVSTGUIStartNoteStops && wParam <= kcVSTGUIEndNoteStops)
	{
		if(ValidateCurrentInstrument())
		{
			CModDoc *pModDoc = m_VstPlugin.GetModDoc();
			const ModCommand::NOTE note = pModDoc->GetNoteWithBaseOctave(static_cast<int>(wParam - kcVSTGUIStartNoteStops), m_nInstrument);
			if(ModCommand::IsNote(note))
			{
				pModDoc->NoteOff(note, false, m_nInstrument, m_noteChannel[note - NOTE_MIN]);
			}
		}
		return wParam;
	}

	return kcNull;
}


// When trying to play a note using this plugin, but no instrument is assigned to it,
// the user is asked whether a new instrument should be added.
bool CAbstractVstEditor::ValidateCurrentInstrument()
{
	if(!CheckInstrument(m_nInstrument))
		m_nInstrument = GetBestInstrumentCandidate();

	//only show messagebox if plug is able to process notes.
	if(m_nInstrument == INSTRUMENTINDEX_INVALID)
	{
		if(m_VstPlugin.CanRecieveMidiEvents())
		{
			// We might need to steal the focus from the plugin bridge. This is going to work
			// as the plugin bridge will call AllowSetForegroundWindow on key messages.
			SetForegroundWindow();
			if(!m_VstPlugin.IsInstrument() || m_VstPlugin.GetSoundFile().GetModSpecifications().instrumentsMax == 0 ||
				Reporting::Confirm(UL_("You need to assign an instrument to this plugin before you can play notes from here.\nCreate a new instrument and assign this plugin to the instrument?"), false, false, this) == cnfNo)
			{
				return false;
			} else
			{
				OnCreateInstrument();
				// Return true since we don't want to trigger the note for which the instrument has been validated yet.
				// Otherwise, the note might hang forever because the key-up event will go missing.
				return false;
			}
		} else
		{
			// Can't process notes
			return false;
		}
	}
	return true;

}


static int GetNumSubMenus(int32 numProgs) { return (numProgs + (PRESETS_PER_GROUP - 1)) / PRESETS_PER_GROUP; }


void CAbstractVstEditor::OnMenuSelect(uint32 nItemID, uint32 nFlags, mfcless::Menu hSysMenu)
{
	if(!(nFlags & mfcless::menu::Popup))
	{
		return;
	}
	if(hSysMenu == m_Menu->m_hMenu)
	{
		// Main menu
		switch(nItemID)
		{
		case 0:
			// Grey out paste menu item.
			m_Menu->EnableMenuItem(ID_EDIT_PASTE, mfcless::menu::ByCommand | (mfcless::clip::IsFormatAvailable(m_clipboardFormat) ? 0 : mfcless::menu::Grayed));
			break;
		case 1:
			// If there would be only one sub menu, we add presets directly to the factory menu
			{
				const int32 numProgs = m_VstPlugin.GetNumPrograms();
				if(GetNumSubMenus(numProgs) <= 1)
				{
					GeneratePresetMenu(0, m_PresetMenu);
				}
			}
			break;
		}
	} else if(hSysMenu == m_Menu->GetSubMenu(1)->m_hMenu)
	{
		// Preset menu
		m_currentPresetMenu = nItemID;
		GeneratePresetMenu(nItemID * PRESETS_PER_GROUP, *m_presetMenuGroup[nItemID]);
	}
}


void CAbstractVstEditor::UpdatePresetMenu(bool force)
{
	const int32 numProgs = m_VstPlugin.GetNumPrograms();
	const int32 curProg  = m_VstPlugin.GetCurrentProgram();

	if(m_PresetMenu.m_hMenu) // We rebuild the menu from scratch, so remove any exiting menus...
	{
		if(curProg == m_nCurProg && !force) // ... unless menu exists and is accurate, in which case we are done.
			return;

		m_presetMenuGroup.clear();

		// If there were no preset groups, delete the remaining content so that it can be refilled dynamically
		while(m_PresetMenu.GetMenuItemCount() > 0)
			m_PresetMenu.RemoveMenu(0, mfcless::menu::ByPosition);
	} else
	{
		// Create Factory preset menu
		m_PresetMenu.CreatePopupMenu();
		m_Menu->InsertMenu(1, mfcless::menu::ByPosition | mfcless::menu::Popup, m_PresetMenu, UL_("&Presets"));
	}

	m_Menu->EnableMenuItem(1, mfcless::menu::ByPosition | (numProgs ? 0 : mfcless::menu::Grayed));

	const int numSubMenus = GetNumSubMenus(numProgs);

	if(numSubMenus > 1)
	{
		// Depending on the plugin and its number of presets, filling the sub menus can take quite a while (e.g. Synth1),
		// so we fill the menus only on demand (when they  are opened), so that the editor GUI creation doesn't take forever.
		m_presetMenuGroup.resize(numSubMenus);
		for(int bank = 0, prog = 0; bank < numSubMenus; bank++, prog += PRESETS_PER_GROUP)
		{
			m_presetMenuGroup[bank] = std::make_unique<mfcless::Menu>();
			m_presetMenuGroup[bank]->CreatePopupMenu();

			mpt::ustring label;
			label = mfcless::PrintF(UL_("Bank %d (%d-%d)"), bank + 1, prog + 1, std::min(prog + PRESETS_PER_GROUP, numProgs));
			m_PresetMenu.AppendMenu(mfcless::menu::Popup
				| (bank % 32 == 0 ? mfcless::menu::MenuBreak : 0)
				| (curProg >= prog && curProg < prog + PRESETS_PER_GROUP ? mfcless::menu::Checked : mfcless::menu::Unchecked),
				reinterpret_cast<uintptr_t>(m_presetMenuGroup[bank]->m_hMenu), label);
		}
	}

	m_currentPresetMenu = 0;
	m_nCurProg = curProg;
}


void CAbstractVstEditor::GeneratePresetMenu(int32 offset, mfcless::Menu &parent)
{
	const int32 numProgs = m_VstPlugin.GetNumPrograms();
	const int32 curProg  = m_VstPlugin.GetCurrentProgram();
	const int32 endProg = std::min(offset + PRESETS_PER_GROUP, numProgs);

	if(parent.GetMenuItemCount() != 0)
	{
		// Already generated.
		return;
	}

	m_VstPlugin.CacheProgramNames(offset, endProg);
	for(int32 p = offset, row = 0, id = 0; p < endProg; p++, row++, id++)
	{
		mpt::ustring programName = m_VstPlugin.GetFormattedProgramName(p);
		programName.Replace(UL_("&"), UL_("&&"));
		uint32 splitMenuFlag = 0;

		if(row == PRESETS_PER_COLUMN)
		{
			// Advance to next menu column
			row = 0;
			splitMenuFlag = mfcless::menu::MenuBarBreak;
		}

		parent.AppendMenu(mfcless::menu::String | (p == curProg ? mfcless::menu::Checked : mfcless::menu::Unchecked) | splitMenuFlag, ID_PRESET_SET + id, programName);
	}
}


void CAbstractVstEditor::UpdateInputMenu()
{
	mfcless::Menu pInfoMenu = m_Menu->GetSubMenu(2);
	pInfoMenu->DeleteMenu(0, mfcless::menu::ByPosition);

	const CSoundFile &sndFile = m_VstPlugin.GetSoundFile();

	if(m_InputMenu.m_hMenu)
	{
		m_InputMenu.DestroyMenu();
	}
	if(!m_InputMenu.m_hMenu)
	{
		m_InputMenu.CreatePopupMenu();
	}

	std::vector<IMixPlugin *> inputPlugs;
	m_VstPlugin.GetInputPlugList(inputPlugs);
	for(auto plug : inputPlugs)
	{
		mpt::ustring name = MPT_UFORMAT("FX{}: {}")(mpt::ufmt::dec0<2>(plug->m_nSlot + 1), mpt::ToUnicode(plug->m_pMixStruct->GetName()));
		m_InputMenu.AppendMenu(mfcless::menu::String, ID_PLUGSELECT + plug->m_nSlot, name);
	}

	std::vector<CHANNELINDEX> inputChannels;
	m_VstPlugin.GetInputChannelList(inputChannels);
	bool addSeparator = !inputPlugs.empty();
	for(auto chn : inputChannels)
	{
		if(addSeparator)
		{
			m_InputMenu.AppendMenu(mfcless::menu::Separator);
			addSeparator = false;
		}
		mpt::ustring name = MPT_UFORMAT("Chn{}: {}")(mpt::ufmt::dec0<2>(chn + 1), mpt::ToUnicode(sndFile.GetCharsetInternal(), sndFile.ChnSettings[chn].szName));
		m_InputMenu.AppendMenu(mfcless::menu::String, NULL, name);
	}

	std::vector<INSTRUMENTINDEX> inputInstruments;
	m_VstPlugin.GetInputInstrumentList(inputInstruments);
	addSeparator = !inputPlugs.empty() || !inputChannels.empty();
	for(auto ins : inputInstruments)
	{
		if(addSeparator)
		{
			m_InputMenu.AppendMenu(mfcless::menu::Separator);
			addSeparator = false;
		}
		mpt::ustring name = MPT_UFORMAT("Ins{}: {}")(mpt::ufmt::dec0<2>(ins), mpt::ToUnicode(sndFile.GetCharsetInternal(), sndFile.GetInstrumentName(ins)));
		m_InputMenu.AppendMenu(mfcless::menu::String | ((ins == m_nInstrument) ? mfcless::menu::Checked : 0), ID_SELECTINST + ins, name);
	}

	if(inputPlugs.empty() && inputChannels.empty() && inputInstruments.empty())
	{
		m_InputMenu.AppendMenu(mfcless::menu::String | mfcless::menu::Grayed, NULL, UL_("None"));
	}

	pInfoMenu->InsertMenu(0, mfcless::menu::ByPosition | mfcless::menu::Popup, m_InputMenu, UL_("I&nputs"));
}


void CAbstractVstEditor::UpdateOutputMenu()
{
	mfcless::Menu pInfoMenu = m_Menu->GetSubMenu(2);
	pInfoMenu->DeleteMenu(1, mfcless::menu::ByPosition);

	if(m_OutputMenu.m_hMenu)
	{
		m_OutputMenu.DestroyMenu();
	}
	if(!m_OutputMenu.m_hMenu)
	{
		m_OutputMenu.CreatePopupMenu();
	}

	std::vector<IMixPlugin *> outputPlugs;
	m_VstPlugin.GetOutputPlugList(outputPlugs);
	mpt::ustring name;

	for(auto plug : outputPlugs)
	{
		if(plug != nullptr)
		{
			name = mfcless::PrintF(UL_("FX%02d: "), plug->m_nSlot + 1);
			name += mpt::ToUnicode(plug->m_pMixStruct->GetName());
			m_OutputMenu.AppendMenu(mfcless::menu::String, ID_PLUGSELECT + plug->m_nSlot, name);
		} else
		{
			name = UL_("Master Output");
			m_OutputMenu.AppendMenu(mfcless::menu::String | mfcless::menu::Grayed, NULL, name);
		}
	}
	pInfoMenu->InsertMenu(1, mfcless::menu::ByPosition | mfcless::menu::Popup, m_OutputMenu, UL_("Ou&tputs"));
}


void CAbstractVstEditor::UpdateMacroMenu()
{
	mfcless::Menu pInfoMenu = m_Menu->GetSubMenu(2);
	pInfoMenu->DeleteMenu(2, mfcless::menu::ByPosition);

	if(m_MacroMenu.m_hMenu)
	{
		m_MacroMenu.DestroyMenu();
	}
	if(!m_MacroMenu.m_hMenu)
	{
		m_MacroMenu.CreatePopupMenu();
	}

	mpt::ustring label, macroName;
	for(int nMacro = 0; nMacro < kSFxMacros; nMacro++)
	{
		int action = 0;
		uint32 greyed = mfcless::menu::Grayed;

		const MIDIMacroConfig &midiCfg = m_VstPlugin.GetSoundFile().m_MidiCfg;

		const ParameteredMacro macroType = midiCfg.GetParameteredMacroType(nMacro);

		if(macroType == kSFxUnused)
		{
			macroName = UL_("Unused. Learn Param...");
			action= ID_LEARN_MACRO_FROM_PLUGGUI + nMacro;
			greyed = 0;
		} else
		{
			macroName = midiCfg.GetParameteredMacroName(nMacro, &m_VstPlugin);
			if(macroType != kSFxPlugParam || macroName.Left(3) != UL_("N/A"))
			{
				greyed = 0;
			}
		}

		label = mfcless::PrintF(UL_("SF%X: "), nMacro);
		label += macroName;
		m_MacroMenu.AppendMenu(mfcless::menu::String | greyed, action, label);
	}

	pInfoMenu->InsertMenu(2, mfcless::menu::ByPosition | mfcless::menu::Popup, m_MacroMenu, UL_("&Macros"));
}

void CAbstractVstEditor::UpdateOptionsMenu()
{

	if(m_OptionsMenu.m_hMenu)
		m_OptionsMenu.DestroyMenu();

	CInputHandler *ih = CMainFrame::GetInputHandler();

	m_OptionsMenu.CreatePopupMenu();

	//Bypass
	m_OptionsMenu.AppendMenu(mfcless::menu::String | (m_VstPlugin.IsBypassed() ? mfcless::menu::Checked : 0),
							   ID_PLUG_BYPASS, ih->GetKeyTextFromCommand(kcVSTGUIBypassPlug, UL_("&Bypass Plugin")));
	//Record Params
	m_OptionsMenu.AppendMenu(mfcless::menu::String | (m_VstPlugin.m_recordAutomation ? mfcless::menu::Checked : 0),
							   ID_PLUG_RECORDAUTOMATION, ih->GetKeyTextFromCommand(kcVSTGUIToggleRecordParams, UL_("Record &Parameter Changes")));
	//Record MIDI Out
	m_OptionsMenu.AppendMenu(mfcless::menu::String | (m_VstPlugin.m_recordMIDIOut ? mfcless::menu::Checked : 0),
							   ID_PLUG_RECORD_MIDIOUT, ih->GetKeyTextFromCommand(kcVSTGUIToggleRecordMIDIOut, UL_("Record &MIDI Out to Pattern Editor")));
	//Pass on keypresses
	m_OptionsMenu.AppendMenu(mfcless::menu::String | (m_VstPlugin.m_passKeypressesToPlug ? mfcless::menu::Checked : 0),
							   ID_PLUG_PASSKEYS, ih->GetKeyTextFromCommand(kcVSTGUIToggleSendKeysToPlug, UL_("Pass &Keys to Plugin")));


	m_Menu->DeleteMenu(3, mfcless::menu::ByPosition);
	m_Menu->InsertMenu(3, mfcless::menu::ByPosition | mfcless::menu::Popup, m_OptionsMenu, UL_("&Options"));

}


void CAbstractVstEditor::OnToggleEditor(uint32 nID)
{
	CModDoc *pModDoc = m_VstPlugin.GetModDoc();

	if(pModDoc)
	{
		pModDoc->TogglePluginEditor(nID - ID_PLUGSELECT);
	}
}


void CAbstractVstEditor::OnInitMenu(mfcless::Menu /*pMenu*/)
{
	SetupMenu();
}


bool CAbstractVstEditor::CheckInstrument(INSTRUMENTINDEX ins) const
{
	const CSoundFile &sndFile = m_VstPlugin.GetSoundFile();

	if(ins != INSTRUMENTINDEX_INVALID && ins < MAX_INSTRUMENTS && sndFile.Instruments[ins] != nullptr)
	{
		return (sndFile.Instruments[ins]->nMixPlug) == (m_VstPlugin.m_nSlot + 1);
	}
	return false;
}


INSTRUMENTINDEX CAbstractVstEditor::GetBestInstrumentCandidate() const
{
	// First try current instrument:
	const CModDoc *modDoc = m_VstPlugin.GetModDoc();
	POSITION pos = modDoc->GetFirstViewPosition();
	while(pos)
	{
		CModControlView *pView = dynamic_cast<CModControlView *>(modDoc->GetNextView(pos));
		if(pView != nullptr && pView->GetDocument() == modDoc)
		{
			INSTRUMENTINDEX ins = static_cast<INSTRUMENTINDEX>(pView->GetInstrumentChange());
			if(CheckInstrument(ins))
				return ins;
		}
	}

	// Just take the first instrument that points to this plug..
	return modDoc->HasInstrumentForPlugin(m_VstPlugin.m_nSlot);
}


void CAbstractVstEditor::OnSetInputInstrument(uint32 nID)
{
	m_nInstrument = static_cast<INSTRUMENTINDEX>(nID - ID_SELECTINST);
}


void CAbstractVstEditor::OnCreateInstrument()
{
	if(m_VstPlugin.GetModDoc() != nullptr)
	{
		INSTRUMENTINDEX instr = m_VstPlugin.GetModDoc()->InsertInstrumentForPlugin(m_VstPlugin.GetSlot());
		if(instr != INSTRUMENTINDEX_INVALID) m_nInstrument  = instr;
	}
}


void CAbstractVstEditor::PrepareToLearnMacro(uint32 nID)
{
	m_nLearnMacro = (nID-ID_LEARN_MACRO_FROM_PLUGGUI);
	//Now we wait for a param to be touched. We'll get the message from the VST Plug Manager.
	//Then pModDoc->LearnMacro(macro, param) is called
}

void CAbstractVstEditor::SetLearnMacro(int inMacro)
{
	if (inMacro < kSFxMacros)
	{
		m_nLearnMacro=inMacro;
	}
}

int CAbstractVstEditor::GetLearnMacro()
{
	return m_nLearnMacro;
}


void CAbstractVstEditor::OnMove(int, int)
{
	if(IsWindowVisible())
	{
		StoreWindowPos();
	}
}


void CAbstractVstEditor::StoreWindowPos()
{
	if(this)
	{
		mfcless::WindowPlacement wnd;
		wnd.length = sizeof(mfcless::WindowPlacement);
		GetWindowPlacement(&wnd);
		m_VstPlugin.SetEditorPos(wnd.rcNormalPosition.left, wnd.rcNormalPosition.top);
	}
}


void CAbstractVstEditor::RestoreWindowPos()
{
	// Restore previous editor position
	int32 editorX, editorY;
	m_VstPlugin.GetEditorPos(editorX, editorY);

	if(editorX != int32_min && editorY != int32_min)
	{
		mfcless::WindowPlacement wnd;
		wnd.length = sizeof(wnd);
		GetWindowPlacement(&wnd);
		wnd.showCmd = mfcless::show::ShowNoActivate;
		mfcless::Rect rect = wnd.rcNormalPosition;
		rect.MoveToXY(editorX, editorY);
		wnd.rcNormalPosition = rect;
		SetWindowPlacement(&wnd);
	}
}


#endif // NO_PLUGINS


OPENMPT_NAMESPACE_END
