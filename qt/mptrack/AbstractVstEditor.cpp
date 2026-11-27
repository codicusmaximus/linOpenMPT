/*
 * AbstractVstEditor.cpp
 * ---------------------
 * Purpose: Common plugin editor interface class. This code is shared between custom and default plugin user interfaces.
 * Notes  : mfcless twin of upstream's mptrack/AbstractVstEditor.cpp.
 *          Hand-edited from the twin.py draft. The menu bar is a QMenuBar
 *          built from IDR_VSTMENU; what upstream does when a menu is opened
 *          (WM_INITMENU, WM_MENUSELECT) happens when a QMenu is about to
 *          show. Upstream's double click on the title bar (which shrinks the
 *          window to its title) is left to the window manager, which shades
 *          windows that way on Linux desktops.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "AbstractVstEditor.h"
#include "mfcless/Clipboard.h"
#include "mfcless/DialogBuilder.h"
#include "mfcless/Menus.h"
#include "mfcless/QtString.h"
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

#include <QtWidgets/QMenuBar>

#include <sstream>


OPENMPT_NAMESPACE_BEGIN


#ifndef NO_PLUGINS

#define PRESETS_PER_COLUMN 32
#define PRESETS_PER_GROUP 128

uint32 CAbstractVstEditor::m_clipboardFormat = mfcless::clip::RegisterFormat(QStringLiteral("VST Preset Data"));


namespace
{
// Menu item texts show a single & as is.
QString EscapeAmpersands(const mpt::ustring &s)
{
	return mfcless::ToQString(s).replace(QLatin1Char('&'), QLatin1String("&&"));
}
}


CAbstractVstEditor::WindowSizeAdjuster::WindowSizeAdjuster(QWidget &wnd)
	: m_wnd(wnd)
{
}

CAbstractVstEditor::WindowSizeAdjuster::~WindowSizeAdjuster()
{
	// Extend window height by the menu size if it changed
	if(auto *editor = dynamic_cast<CAbstractVstEditor *>(&m_wnd))
		editor->FitMenuBar();
}


int CAbstractVstEditor::MenuBarHeight() const
{
	if(!m_Menu || m_Menu->isHidden())
		return 0;
	const int h = m_Menu->heightForWidth(std::max(1, width()));
	return h > 0 ? h : m_Menu->sizeHint().height();
}


void CAbstractVstEditor::FitMenuBar()
{
	const int newHeight = MenuBarHeight();
	if(m_Menu && !m_Menu->isHidden())
		m_Menu->setGeometry(0, 0, width(), newHeight);
	const int delta = newHeight - m_menuHeight;
	if(delta == 0)
		return;
	m_menuHeight = newHeight;
	// The controls move down (or up) with the client area, and the window
	// grows by as much.
	for(QWidget *child : findChildren<QWidget *>(Qt::FindDirectChildrenOnly))
	{
		if(child != m_Menu && !child->isWindow())
			child->move(child->x(), child->y() + delta);
	}
	for(auto *layout : findChildren<mfcless::DialogLayout *>(Qt::FindDirectChildrenOnly))
		layout->ShiftContent(delta);
	const int h = height() + delta;
	if(minimumHeight() == maximumHeight())
		setFixedHeight(h);
	else
		resize(width(), h);
}


void CAbstractVstEditor::RegisterMessageMap()
{
	ResizableDialog::RegisterMessageMap();
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
	// The menu bar: File, Presets, Info (Inputs, Outputs, Macros), Options,
	// then the preset buttons and name (UpdatePresetField).
	m_Menu = new QMenuBar(this);
	m_Menu->setNativeMenuBar(false);
	m_Menu->hide();
	mfcless::BuildMenuBar(m_Menu, IDR_VSTMENU, [this](uint32 id) { RouteCommand(id); },
		[this](uint32 id, mfcless::CommandState &state) { return RouteCommandUpdate(id, state); });
	const QList<QAction *> top = m_Menu->actions();
	QAction *fileAction = top.value(0), *optionsAction = top.value(2);
	m_infoAction = top.value(1);

	m_PresetMenu.setTitle(QStringLiteral("&Presets"));
	m_Menu->insertMenu(m_infoAction, &m_PresetMenu);

	if(m_infoAction && m_infoAction->menu())
	{
		QMenu *info = m_infoAction->menu();
		info->clear();
		m_InputMenu.setTitle(QStringLiteral("I&nputs"));
		m_OutputMenu.setTitle(QStringLiteral("Ou&tputs"));
		m_MacroMenu.setTitle(QStringLiteral("&Macros"));
		info->addMenu(&m_InputMenu);
		info->addMenu(&m_OutputMenu);
		info->addMenu(&m_MacroMenu);
	}
	m_OptionsMenu.setTitle(QStringLiteral("&Options"));
	m_Menu->insertMenu(optionsAction, &m_OptionsMenu);
	if(optionsAction)
		m_Menu->removeAction(optionsAction);

	// Opening a menu (WM_INITMENU, WM_MENUSELECT).
	for(QAction *a : {fileAction, m_PresetMenu.menuAction(), m_infoAction, m_OptionsMenu.menuAction()})
	{
		if(a && a->menu())
			connect(a->menu(), &QMenu::aboutToShow, this, [this] { OnInitMenu(); });
	}
	if(fileAction && fileAction->menu())
	{
		connect(fileAction->menu(), &QMenu::aboutToShow, this, [this, menu = fileAction->menu()] {
			// Grey out paste menu item.
			if(QAction *paste = mfcless::FindMenuAction(menu, ID_EDIT_PASTE))
				paste->setEnabled(mfcless::clip::IsFormatAvailable(m_clipboardFormat));
		});
	}
	connect(&m_PresetMenu, &QMenu::aboutToShow, this, [this] {
		// If there would be only one sub menu, we add presets directly to the factory menu
		const int32 numProgs = m_VstPlugin.GetNumPrograms();
		if((numProgs + (PRESETS_PER_GROUP - 1)) / PRESETS_PER_GROUP <= 1)
			GeneratePresetMenu(0, m_PresetMenu);
	});

	m_nInstrument = GetBestInstrumentCandidate();
}


CAbstractVstEditor::~CAbstractVstEditor()
{
	m_presetMenuGroup.clear();
	m_VstPlugin.m_pEditor = nullptr;
}


void CAbstractVstEditor::PostNcDestroy()
{
	ResizableDialog::PostNcDestroy();
	delete this;
}


bool CAbstractVstEditor::event(QEvent *e)
{
	if(e->type() == QEvent::WindowActivate)
	{
		auto callback = [&plugin = m_VstPlugin](mpt::const_byte_span sysex) { plugin.MidiSend(sysex); };
		CMainFrame::GetMainFrame()->SetMidiRecordWnd(this, callback);
	} else if(e->type() == QEvent::Resize && m_Menu && !m_Menu->isHidden())
	{
		// A narrower window can wrap the menu bar into more lines.
		const bool result = ResizableDialog::event(e);
		FitMenuBar();
		return result;
	}
	return ResizableDialog::event(e);
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


// Drop files from the desktop
void CAbstractVstEditor::OnDropFiles(const QStringList &files)
{
	if(CMainFrame *mainFrm = CMainFrame::GetMainFrame())
		mainFrm->activateWindow();
	for(const QString &fileName : files)
	{
		m_VstPlugin.LoadProgram(mpt::PathString::FromUnicode(mfcless::FromQString(fileName)));
	}
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

	BeginWaitCursor();
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
	EndWaitCursor();
}


void CAbstractVstEditor::OnPasteParameters()
{
	if(CMainFrame::GetMainFrame() == nullptr) return;

	BeginWaitCursor();
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
	EndWaitCursor();
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

	CInputDlg dlg(this, UL_("New name for this plugin instance:"), plugin.GetName(), static_cast<int32>(std::size(plugin.Info.szName.buf)));
	if(dlg.DoModal() == IDOK)
	{
		if(dlg.resultAsString != plugin.GetName())
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
	m_hasWindow = true;
	setAcceptDrops(true);
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
	m_hasWindow = false;
	DestroyWindow();
}


void CAbstractVstEditor::SetupMenu(bool force)
{
	WindowSizeAdjuster adjuster(*this);
	m_Menu->show();
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
		if(m_presetField.empty())
		{
			const std::pair<uint32, const char *> buttons[] =
			{
				{ID_VSTPRESETBACKWARDJUMP, "<<"},
				{ID_PREVIOUSVSTPRESET, "<"},
				{ID_NEXTVSTPRESET, ">"},
				{ID_VSTPRESETFORWARDJUMP, ">>"},
				{ID_VSTPRESETNAME, ""},
			};
			for(const auto &[id, text] : buttons)
			{
				QAction *a = m_Menu->addAction(QString::fromLatin1(text));
				connect(a, &QAction::triggered, this, [this, id = id] { RouteCommand(id); });
				m_presetField.push_back(a);
			}
			// The program name is shown, not chosen.
			m_presetField.back()->setEnabled(false);
		}

		m_presetField.back()->setText(EscapeAmpersands(m_VstPlugin.GetFormattedProgramName(m_VstPlugin.GetCurrentProgram())));
	}
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


bool CAbstractVstEditor::HandleKeyMessage(const mfcless::Message &msg, bool handleGlobal)
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
			title += m_VstPlugin.m_pMixStruct->GetName() + UL_(" (");
		title += m_VstPlugin.m_pMixStruct->GetLibraryName();
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
			// We might need to steal the focus from the plugin bridge.
			raise();
			activateWindow();
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


void CAbstractVstEditor::UpdatePresetMenu(bool force)
{
	const int32 numProgs = m_VstPlugin.GetNumPrograms();
	const int32 curProg  = m_VstPlugin.GetCurrentProgram();

	if(m_nCurProg != -1 || !m_PresetMenu.isEmpty()) // We rebuild the menu from scratch, so remove any exiting menus...
	{
		if(curProg == m_nCurProg && !force) // ... unless menu exists and is accurate, in which case we are done.
			return;

		// If there were no preset groups, delete the remaining content so that it can be refilled dynamically
		m_PresetMenu.clear();
		m_presetMenuGroup.clear();
	}

	m_PresetMenu.menuAction()->setEnabled(numProgs != 0);

	const int numSubMenus = GetNumSubMenus(numProgs);

	if(numSubMenus > 1)
	{
		// Depending on the plugin and its number of presets, filling the sub menus can take quite a while (e.g. Synth1),
		// so we fill the menus only on demand (when they  are opened), so that the editor GUI creation doesn't take forever.
		m_presetMenuGroup.resize(numSubMenus);
		for(int bank = 0, prog = 0; bank < numSubMenus; bank++, prog += PRESETS_PER_GROUP)
		{
			m_presetMenuGroup[bank] = std::make_unique<QMenu>();
			QMenu *sub = m_presetMenuGroup[bank].get();
			sub->setTitle(QStringLiteral("Bank %1 (%2-%3)").arg(bank + 1).arg(prog + 1).arg(std::min(prog + PRESETS_PER_GROUP, numProgs)));
			QAction *a = m_PresetMenu.addMenu(sub);
			if(curProg >= prog && curProg < prog + PRESETS_PER_GROUP)
			{
				a->setCheckable(true);
				a->setChecked(true);
			}
			connect(sub, &QMenu::aboutToShow, this, [this, bank, sub] {
				m_currentPresetMenu = bank;
				GeneratePresetMenu(bank * PRESETS_PER_GROUP, *sub);
			});
		}
	}

	m_currentPresetMenu = 0;
	m_nCurProg = curProg;
}


void CAbstractVstEditor::GeneratePresetMenu(int32 offset, QMenu &parent)
{
	const int32 numProgs = m_VstPlugin.GetNumPrograms();
	const int32 curProg  = m_VstPlugin.GetCurrentProgram();
	const int32 endProg = std::min(offset + PRESETS_PER_GROUP, numProgs);

	if(!parent.isEmpty())
	{
		// Already generated.
		return;
	}

	// Qt menus scroll instead of breaking into columns (PRESETS_PER_COLUMN).
	m_VstPlugin.CacheProgramNames(offset, endProg);
	mfcless::Menu menu = mfcless::Menu::Wrap(&parent);
	for(int32 p = offset, id = 0; p < endProg; p++, id++)
	{
		menu.Append(mfcless::menu::String | (p == curProg ? mfcless::menu::Checked : mfcless::menu::Unchecked), ID_PRESET_SET + id, EscapeAmpersands(m_VstPlugin.GetFormattedProgramName(p)));
	}
}


void CAbstractVstEditor::UpdateInputMenu()
{
	const CSoundFile &sndFile = m_VstPlugin.GetSoundFile();

	m_InputMenu.clear();
	mfcless::Menu menu = mfcless::Menu::Wrap(&m_InputMenu);

	std::vector<IMixPlugin *> inputPlugs;
	m_VstPlugin.GetInputPlugList(inputPlugs);
	for(auto plug : inputPlugs)
	{
		mpt::ustring name = MPT_UFORMAT("FX{}: {}")(mpt::ufmt::dec0<2>(plug->m_nSlot + 1), plug->m_pMixStruct->GetName());
		menu.Append(mfcless::menu::String, ID_PLUGSELECT + plug->m_nSlot, name);
	}

	std::vector<CHANNELINDEX> inputChannels;
	m_VstPlugin.GetInputChannelList(inputChannels);
	bool addSeparator = !inputPlugs.empty();
	for(auto chn : inputChannels)
	{
		if(addSeparator)
		{
			menu.AppendSeparator();
			addSeparator = false;
		}
		mpt::ustring name = MPT_UFORMAT("Chn{}: {}")(mpt::ufmt::dec0<2>(chn + 1), mpt::ToUnicode(sndFile.GetCharsetInternal(), sndFile.ChnSettings[chn].szName));
		menu.Append(mfcless::menu::String, 0, name);
	}

	std::vector<INSTRUMENTINDEX> inputInstruments;
	m_VstPlugin.GetInputInstrumentList(inputInstruments);
	addSeparator = !inputPlugs.empty() || !inputChannels.empty();
	for(auto ins : inputInstruments)
	{
		if(addSeparator)
		{
			menu.AppendSeparator();
			addSeparator = false;
		}
		mpt::ustring name = MPT_UFORMAT("Ins{}: {}")(mpt::ufmt::dec0<2>(ins), mpt::ToUnicode(sndFile.GetCharsetInternal(), sndFile.GetInstrumentName(ins)));
		menu.Append(mfcless::menu::String | ((ins == m_nInstrument) ? mfcless::menu::Checked : 0), ID_SELECTINST + ins, name);
	}

	if(inputPlugs.empty() && inputChannels.empty() && inputInstruments.empty())
	{
		menu.Append(mfcless::menu::String | mfcless::menu::Grayed, 0, UL_("None"));
	}
}


void CAbstractVstEditor::UpdateOutputMenu()
{
	m_OutputMenu.clear();
	mfcless::Menu menu = mfcless::Menu::Wrap(&m_OutputMenu);

	std::vector<IMixPlugin *> outputPlugs;
	m_VstPlugin.GetOutputPlugList(outputPlugs);

	for(auto plug : outputPlugs)
	{
		if(plug != nullptr)
		{
			mpt::ustring name = MPT_UFORMAT("FX{}: ")(mpt::ufmt::dec0<2>(plug->m_nSlot + 1)) + plug->m_pMixStruct->GetName();
			menu.Append(mfcless::menu::String, ID_PLUGSELECT + plug->m_nSlot, name);
		} else
		{
			menu.Append(mfcless::menu::String | mfcless::menu::Grayed, 0, UL_("Master Output"));
		}
	}
}


void CAbstractVstEditor::UpdateMacroMenu()
{
	m_MacroMenu.clear();
	mfcless::Menu menu = mfcless::Menu::Wrap(&m_MacroMenu);

	mpt::ustring macroName;
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
			if(macroType != kSFxPlugParam || macroName.substr(0, 3) != UL_("N/A"))
			{
				greyed = 0;
			}
		}

		menu.Append(mfcless::menu::String | greyed, action, MPT_UFORMAT("SF{}: ")(mpt::ufmt::HEX(nMacro)) + macroName);
	}
}

void CAbstractVstEditor::UpdateOptionsMenu()
{
	m_OptionsMenu.clear();
	mfcless::Menu menu = mfcless::Menu::Wrap(&m_OptionsMenu);

	CInputHandler *ih = CMainFrame::GetInputHandler();

	//Bypass
	menu.Append(mfcless::menu::String | (m_VstPlugin.IsBypassed() ? mfcless::menu::Checked : 0),
		ID_PLUG_BYPASS, ih->GetKeyTextFromCommand(kcVSTGUIBypassPlug, UL_("&Bypass Plugin")));
	//Record Params
	menu.Append(mfcless::menu::String | (m_VstPlugin.m_recordAutomation ? mfcless::menu::Checked : 0),
		ID_PLUG_RECORDAUTOMATION, ih->GetKeyTextFromCommand(kcVSTGUIToggleRecordParams, UL_("Record &Parameter Changes")));
	//Record MIDI Out
	menu.Append(mfcless::menu::String | (m_VstPlugin.m_recordMIDIOut ? mfcless::menu::Checked : 0),
		ID_PLUG_RECORD_MIDIOUT, ih->GetKeyTextFromCommand(kcVSTGUIToggleRecordMIDIOut, UL_("Record &MIDI Out to Pattern Editor")));
	//Pass on keypresses
	menu.Append(mfcless::menu::String | (m_VstPlugin.m_passKeypressesToPlug ? mfcless::menu::Checked : 0),
		ID_PLUG_PASSKEYS, ih->GetKeyTextFromCommand(kcVSTGUIToggleSendKeysToPlug, UL_("Pass &Keys to Plugin")));
}


void CAbstractVstEditor::OnToggleEditor(uint32 nID)
{
	CModDoc *pModDoc = m_VstPlugin.GetModDoc();

	if(pModDoc)
	{
		pModDoc->TogglePluginEditor(nID - ID_PLUGSELECT);
	}
}


void CAbstractVstEditor::OnInitMenu()
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
	if(modDoc == nullptr)
		return INSTRUMENTINDEX_INVALID;
	for(mfcless::DocumentView *view : modDoc->GetViews())
	{
		CModControlView *pView = dynamic_cast<CModControlView *>(view);
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
	if(isVisible())
	{
		StoreWindowPos();
	}
}


void CAbstractVstEditor::StoreWindowPos()
{
	if(m_hasWindow)
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
