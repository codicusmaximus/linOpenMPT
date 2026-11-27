/*
 * MIDIMacroDialog.cpp
 * -------------------
 * Purpose: MIDI Macro Configuration Dialog
 * Notes  : mfcless twin of upstream's mptrack/MIDIMacroDialog.cpp.
 *          Draft from qt/tools/twin.py, not yet edited by hand.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "MIDIMacroDialog.h"
#include "mfcless/Widgets.h"
#include "HighDPISupport.h"
#include "Mptrack.h"
#include "Reporting.h"
#include "resource.h"
#include "../common/mptStringBuffer.h"
#include "../soundlib/MIDIEvents.h"
#include "../soundlib/Sndfile.h"
#include "../soundlib/plugins/PlugInterface.h"


OPENMPT_NAMESPACE_BEGIN


void CMidiMacroSetup::RegisterMessageMap()
{
	DialogBase::RegisterMessageMap();
	AddCommand(IDC_BUTTON1, &CMidiMacroSetup::OnSetAsDefault);
	AddCommand(IDC_BUTTON2, &CMidiMacroSetup::OnResetCfg);
	AddCommand(IDC_BUTTON3, &CMidiMacroSetup::OnMacroHelp);
	AddNotify(IDC_COMBO1, mfcless::Notify::SelChange, &CMidiMacroSetup::OnSFxChanged);
	AddNotify(IDC_COMBO2, mfcless::Notify::SelChange, &CMidiMacroSetup::OnSFxPresetChanged);
	AddNotify(IDC_COMBO3, mfcless::Notify::SelChange, &CMidiMacroSetup::OnZxxPresetChanged);
	AddNotify(IDC_COMBO4, mfcless::Notify::SelChange, &CMidiMacroSetup::UpdateZxxSelection);
	AddNotify(IDC_MACROPLUG, mfcless::Notify::SelChange, &CMidiMacroSetup::OnPlugChanged);
	AddNotify(IDC_MACROPARAM, mfcless::Notify::SelChange, &CMidiMacroSetup::OnPlugParamChanged);
	AddNotify(IDC_MACROCC, mfcless::Notify::SelChange, &CMidiMacroSetup::OnCCChanged);
	AddNotify(IDC_EDIT1, mfcless::Notify::EditChange, &CMidiMacroSetup::OnSFxEditChanged);
	AddNotify(IDC_EDIT2, mfcless::Notify::EditChange, &CMidiMacroSetup::OnZxxEditChanged);
	AddCommandRange(ID_PLUGSELECT, ID_PLUGSELECT + kSFxMacros - 1, &CMidiMacroSetup::OnViewAllParams);
	AddCommandRange(ID_PLUGSELECT + kSFxMacros, ID_PLUGSELECT + kSFxMacros + kSFxMacros - 1, &CMidiMacroSetup::OnSetSFx);
}


void CMidiMacroSetup::DoDataExchange(DataExchange &dx)
{
	DialogBase::DoDataExchange(dx);
	ExchangeControl(dx, IDC_COMBO1, m_CbnSFx);
	ExchangeControl(dx, IDC_COMBO2, m_CbnSFxPreset);
	ExchangeControl(dx, IDC_COMBO3, m_CbnZxxPreset);
	ExchangeControl(dx, IDC_COMBO4, m_CbnZxx);
	ExchangeControl(dx, IDC_EDIT1, m_EditSFx);
	ExchangeControl(dx, IDC_EDIT2, m_EditZxx);
	ExchangeControl(dx, IDC_MACROPLUG, m_CbnMacroPlug);
	ExchangeControl(dx, IDC_MACROPARAM, m_CbnMacroParam);
	ExchangeControl(dx, IDC_MACROCC, m_CbnMacroCC);
}


CMidiMacroSetup::CMidiMacroSetup(CSoundFile &sndFile, QWidget *parent)
	: DialogBase{IDD_MIDIMACRO, parent}
	, m_SndFile{sndFile}
	, m_vMidiCfg{sndFile.m_MidiCfg}
	, m_MidiCfg{*m_vMidiCfg}
{

}

bool CMidiMacroSetup::OnInitDialog()
{
	mpt::ustring s;
	DialogBase::OnInitDialog();
	m_EditSFx->setMaxLength(kMacroLength - 1);
	m_EditZxx->setMaxLength(kMacroLength - 1);

	// Parametered macro selection
	m_CbnSFx->setUpdatesEnabled(false);
	for(int i = 0; i < 16; i++)
	{
		s = mfcless::PrintF(UL_("%d (SF%X)"), i, i);
		mfcless::AddItem(m_CbnSFx, s);
	}
	m_CbnSFx->setUpdatesEnabled(true);
	m_CbnSFx->setCurrentIndex(0);

	// Parametered macro presets
	m_CbnSFxPreset->setUpdatesEnabled(false);
	for(int i = 0; i < kSFxMax; i++)
	{
		mfcless::SetItemData(m_CbnSFxPreset, mfcless::AddItem(m_CbnSFxPreset, m_MidiCfg.GetParameteredMacroName(static_cast<ParameteredMacro>(i))), i);
	}
	m_CbnSFxPreset->setUpdatesEnabled(true);
	OnSFxChanged();

	// MIDI CC selection box
	m_CbnMacroCC->setUpdatesEnabled(false);
	for (int cc = MIDIEvents::MIDICC_start; cc <= MIDIEvents::MIDICC_end; cc++)
	{
		s = mfcless::PrintF(UL_("CC %02d "), cc);
		s += mpt::ToUnicode(mpt::Charset::UTF8, MIDIEvents::MidiCCNames[cc]);
		mfcless::SetItemData(m_CbnMacroCC, mfcless::AddItem(m_CbnMacroCC, s), cc);
	}
	m_CbnMacroCC->setUpdatesEnabled(true);

	// Z80...ZFF box
	m_CbnZxx->setUpdatesEnabled(false);
	for(int zxx = 0x80; zxx <= 0xFF; zxx++)
	{
		s = mfcless::PrintF(UL_("Z%02X"), zxx);
		mfcless::AddItem(m_CbnZxx, s);
	}
	m_CbnZxx->setUpdatesEnabled(true);
	m_CbnZxx->setCurrentIndex(0);

	// Fixed macro presets
	m_CbnZxxPreset->setUpdatesEnabled(false);
	for(int i = 0; i < kZxxMax; i++)
	{
		mfcless::SetItemData(m_CbnZxxPreset, mfcless::AddItem(m_CbnZxxPreset, m_MidiCfg.GetFixedMacroName(static_cast<FixedMacro>(i))), i);
	}
	m_CbnZxxPreset->setUpdatesEnabled(true);
	m_CbnZxxPreset->setCurrentIndex(m_MidiCfg.GetFixedMacroType());

	UpdateDialog();

	auto ScalePixels = [&](auto x) { return HighDPISupport::ScalePixels(x, this); };
	int offsetx = ScalePixels(19), offsety = ScalePixels(30), separatorx = ScalePixels(4), separatory = ScalePixels(2);
	int height = ScalePixels(18), widthMacro = ScalePixels(30), widthVal = ScalePixels(179), widthType = ScalePixels(135), widthBtn = ScalePixels(70);

	for(uint32 m = 0; m < kSFxMacros; m++)
	{
		m_EditMacro[m].Button.Create(UL_(""), WS_CHILD | WS_VISIBLE | WS_TABSTOP,
			mfcless::Rect(offsetx, offsety + m * (separatory + height), offsetx + widthMacro, offsety + m * (separatory + height) + height), this, ID_PLUGSELECT + kSFxMacros + m);
		m_EditMacro[m].Button.SetFont(GetFont());

		m_EditMacro[m].Type.Create(ES_READONLY | WS_CHILD| WS_VISIBLE | WS_TABSTOP | WS_BORDER, 
			mfcless::Rect(offsetx + separatorx + widthMacro, offsety + m * (separatory + height), offsetx + widthMacro + widthType, offsety + m * (separatory + height) + height), this, ID_PLUGSELECT + kSFxMacros + m);
		m_EditMacro[m].Type.SetFont(GetFont());

		m_EditMacro[m].Value.Create(ES_CENTER | ES_READONLY | WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER, 
			mfcless::Rect(offsetx + separatorx + widthType + widthMacro, offsety + m * (separatory + height), offsetx + widthMacro + widthType + widthVal, offsety + m * (separatory + height) + height), this, ID_PLUGSELECT + kSFxMacros + m);
		m_EditMacro[m].Value.SetFont(GetFont());

		m_EditMacro[m].ShowAll.Create(UL_("Show All..."), WS_CHILD | WS_TABSTOP | WS_VISIBLE,
			mfcless::Rect(offsetx + separatorx + widthType + widthMacro + widthVal, offsety + m * (separatory + height), offsetx + widthMacro + widthType + widthVal + widthBtn, offsety + m * (separatory + height) + height), this, ID_PLUGSELECT + m);
		m_EditMacro[m].ShowAll.SetFont(GetFont());
	}
	UpdateMacroList();
	m_CbnMacroPlug->Update(PluginComboBox::Config{PluginComboBox::Flags::ShowLibraryNames}, m_SndFile);
	m_CbnMacroPlug->SetRawSelection(0);
	OnPlugChanged();
	return false;
}


// macro == -1 for updating all macros at once
void CMidiMacroSetup::UpdateMacroList(int macro)
{
	if(!m_EditMacro[0].Button)
	{
		// GUI not yet initialized
		return;
	}

	int start, end;

	if(macro >= 0 && macro < kSFxMacros)
	{
		start = end = macro;
	} else
	{
		start = 0;
		end = kSFxMacros - 1;
	}

	mpt::ustring s;
	const int selectedMacro = m_CbnSFx->currentIndex();

	for(int m = start; m <= end; m++)
	{
		// SFx
		s = mfcless::PrintF(UL_("SF%X"), static_cast<unsigned int>(m));
		m_EditMacro[m].Button.SetWindowText(s);

		// Macro value:
		m_EditMacro[m].Value.SetWindowText(mpt::ToUnicode(mpt::Charset::ASCII, static_cast<std::string>(m_MidiCfg.SFx[m])));
		m_EditMacro[m].Value.SetBackColor(m == selectedMacro ? mfcless::Rgb(200, 200, 225) : mfcless::Rgb(245, 245, 245));

		// Macro Type:
		const ParameteredMacro macroType = m_MidiCfg.GetParameteredMacroType(m);
		switch(macroType)
		{
		case kSFxPlugParam:
			s = mfcless::PrintF(UL_("Control Plugin Param %u"), static_cast<unsigned int>(m_MidiCfg.MacroToPlugParam(m)));
			break;

		default:
			s = m_MidiCfg.GetParameteredMacroName(m);
			break;
		}
		m_EditMacro[m].Type.SetWindowText(s);
		m_EditMacro[m].Type.SetBackColor(m == selectedMacro ? mfcless::Rgb(200,200,225) : mfcless::Rgb(245,245,245));

		// Param details button:
		m_EditMacro[m].ShowAll.ShowWindow((macroType == kSFxPlugParam) ? mfcless::show::Show : mfcless::show::Hide);
	}
}


void CMidiMacroSetup::UpdateDialog()
{
	uint32 sfx = m_CbnSFx->currentIndex();
	uint32 sfx_preset = static_cast<uint32>(mfcless::GetItemData(m_CbnSFxPreset, m_CbnSFxPreset->currentIndex()));
	if(sfx < m_MidiCfg.SFx.size())
	{
		ToggleBoxes(sfx_preset, sfx);
		mfcless::SetWindowText(m_EditSFx, mpt::ToUnicode(mpt::Charset::ASCII, static_cast<std::string>(m_MidiCfg.SFx[sfx])));
	}

	UpdateZxxSelection();
	UpdateMacroList();
}


void CMidiMacroSetup::OnSetAsDefault()
{
	theApp.SetDefaultMidiMacro(m_MidiCfg);
}


void CMidiMacroSetup::OnResetCfg()
{
	theApp.GetDefaultMidiMacro(m_MidiCfg);
	m_CbnZxxPreset->setCurrentIndex(0);
	OnSFxChanged();
}


void CMidiMacroSetup::OnMacroHelp()
{
	Reporting::Information(_T("Valid characters in macros:\n\n"
		"0-9, A-F - Raw hex data (4-Bit value)\n"
		"c - MIDI channel (4-Bit value)\n"
		"n - Note value\n\n"
		"v - Note velocity\n"
		"u - Computed note volume (including envelopes)\n\n"
		"x - Note panning\n"
		"y - Computed panning (including envelopes)\n\n"
		"a - High byte of bank select\n"
		"b - Low byte of bank select\n"
		"p - Program select\n\n"
		"h - Pattern channel\n"
		"m - Sample loop direction\n"
		"o - Last sample offset (Oxx / 9xx)\n"
		"s - SysEx checksum (Roland)\n\n"
		"z - Zxx parameter (00-7F)\n\n"
		"Macros can be up to 31 characters long and contain multiple MIDI messages. SysEx messages are automatically terminated if not specified by the user."),
		UL_("OpenMPT MIDI Macro quick reference"));
}


void CMidiMacroSetup::OnSFxChanged()
{
	uint32 sfx = m_CbnSFx->currentIndex();
	if (sfx < 16)
	{
		int preset = m_MidiCfg.GetParameteredMacroType(sfx);
		m_CbnSFxPreset->setCurrentIndex(preset);
	}
	UpdateDialog();
}


void CMidiMacroSetup::OnSFxPresetChanged()
{
	uint32 sfx = m_CbnSFx->currentIndex();
	ParameteredMacro sfx_preset = static_cast<ParameteredMacro>(mfcless::GetItemData(m_CbnSFxPreset, m_CbnSFxPreset->currentIndex()));

	if (sfx < kSFxMacros)
	{
		if(sfx_preset != kSFxCustom)
		{
			m_MidiCfg.CreateParameteredMacro(sfx, sfx_preset);
		}
		UpdateDialog();
	}
}


void CMidiMacroSetup::OnZxxPresetChanged()
{
	FixedMacro zxxPreset = static_cast<FixedMacro>(mfcless::GetItemData(m_CbnZxxPreset, m_CbnZxxPreset->currentIndex()));

	if (zxxPreset != kZxxCustom)
	{
		m_MidiCfg.CreateFixedMacro(zxxPreset);
		UpdateDialog();
	}
}


void CMidiMacroSetup::UpdateZxxSelection()
{
	uint32 zxx = m_CbnZxx->currentIndex();
	if(zxx < m_MidiCfg.Zxx.size())
	{
		mfcless::SetWindowText(m_EditZxx, mpt::ToUnicode(mpt::Charset::ASCII, static_cast<std::string>(m_MidiCfg.Zxx[zxx])));
	}
}


void CMidiMacroSetup::OnSFxEditChanged()
{
	uint32 sfx = m_CbnSFx->currentIndex();
	if(sfx < m_MidiCfg.SFx.size())
	{
		if(ValidateMacroString(m_EditSFx, m_MidiCfg.SFx[sfx], true, true, false))
		{
			mpt::ustring s;
			mfcless::GetWindowText(m_EditSFx, s);
			m_MidiCfg.SFx[sfx] = mpt::ToCharset(mpt::Charset::ASCII, s);

			int sfx_preset = m_MidiCfg.GetParameteredMacroType(sfx);
			m_CbnSFxPreset->setCurrentIndex(sfx_preset);
			ToggleBoxes(sfx_preset, sfx);
			UpdateMacroList(sfx);
		}
	}
}


void CMidiMacroSetup::OnZxxEditChanged()
{
	uint32 zxx = m_CbnZxx->currentIndex();
	if(zxx < m_MidiCfg.Zxx.size())
	{
		if(ValidateMacroString(m_EditZxx, m_MidiCfg.Zxx[zxx], false, true, false))
		{
			mpt::ustring s;
			mfcless::GetWindowText(m_EditZxx, s);
			m_MidiCfg.Zxx[zxx] = mpt::ToCharset(mpt::Charset::ASCII, s);
			m_CbnZxxPreset->setCurrentIndex(m_MidiCfg.GetFixedMacroType());
		}
	}
}

void CMidiMacroSetup::OnSetSFx(uint32 id)
{
	m_CbnSFx->setCurrentIndex(id - (ID_PLUGSELECT + kSFxMacros));
	OnSFxChanged();
}

void CMidiMacroSetup::OnViewAllParams(uint32 id)
{
#ifndef NO_PLUGINS
	mpt::ustring message, plugName;
	int sfx = id - ID_PLUGSELECT;
	PlugParamIndex param = m_MidiCfg.MacroToPlugParam(sfx);
	message = mfcless::PrintF(UL_("These are the parameters that can be controlled by macro SF%X:\n\n"), sfx);

	for(PLUGINDEX plug = 0; plug < MAX_MIXPLUGINS; plug++)
	{
		IMixPlugin *pVstPlugin = m_SndFile.m_MixPlugins[plug].pMixPlugin;
		if(pVstPlugin && param < pVstPlugin->GetNumVisibleParameters())
		{
			plugName = mpt::ToUnicode(m_SndFile.m_MixPlugins[plug].GetName());
			message += mfcless::PrintF(UL_("FX%d: "), plug + 1);
			message += plugName + UL_("\t") + pVstPlugin->GetFormattedParamName(param) + UL_("\n");
		}
	}

	Reporting::Notification(message, UL_("Macro -> Parameters"));
#endif // NO_PLUGINS
}

void CMidiMacroSetup::OnPlugChanged()
{
#ifndef NO_PLUGINS
	PLUGINDEX plug = m_CbnMacroPlug->GetSelection().value_or(PLUGINDEX_INVALID);

	if(plug >= MAX_MIXPLUGINS)
		return;

	IMixPlugin *pVstPlugin = m_SndFile.m_MixPlugins[plug].pMixPlugin;
	if (pVstPlugin != nullptr)
	{
		m_CbnMacroParam->setUpdatesEnabled(false);
		m_CbnMacroParam->Clear();
		m_CbnMacroParam->clear();
		AddPluginParameternamesToCombobox(m_CbnMacroParam, *pVstPlugin);
		m_CbnMacroParam->setUpdatesEnabled(true);

		int param = m_MidiCfg.MacroToPlugParam(m_CbnSFx->currentIndex());
		m_CbnMacroParam->setCurrentIndex(param);
	}
#endif // NO_PLUGINS
}

void CMidiMacroSetup::OnPlugParamChanged()
{
	int param = static_cast<int>(mfcless::GetItemData(m_CbnMacroParam, m_CbnMacroParam->currentIndex()));

	if(param < 384)
	{
		const std::string macroText = m_MidiCfg.CreateParameteredMacro(kSFxPlugParam, param);
		mfcless::SetWindowText(m_EditSFx, mpt::ToUnicode(mpt::Charset::ASCII, macroText));
	} else
	{
		Reporting::Notification("Only parameters 0 to 383 can be controlled using MIDI Macros. Use Parameter Control Events to automate higher parameters.");
	}
}

void CMidiMacroSetup::OnCCChanged()
{
	int cc = static_cast<int>(mfcless::GetItemData(m_CbnMacroCC, m_CbnMacroCC->currentIndex()));
	const std::string macroText = m_MidiCfg.CreateParameteredMacro(kSFxCC, cc);
	mfcless::SetWindowText(m_EditSFx, mpt::ToUnicode(mpt::Charset::ASCII, macroText));
}

void CMidiMacroSetup::ToggleBoxes(uint32 sfxPreset, uint32 sfx)
{

	if (sfxPreset == kSFxPlugParam)
	{
		m_CbnMacroCC->setVisible(false);
		m_CbnMacroPlug->setVisible(true);
		m_CbnMacroParam->setVisible(true);
		m_CbnMacroPlug->setEnabled(true);
		m_CbnMacroParam->setEnabled(true);
		SetItemText(IDC_GENMACROLABEL, UL_("Plugin/Param"));
		m_CbnMacroParam->setCurrentIndex(m_MidiCfg.MacroToPlugParam(sfx));
	} else
	{
		m_CbnMacroPlug->setEnabled(false);
		m_CbnMacroParam->setEnabled(false);
	}

	if (sfxPreset == kSFxCC)
	{
		m_CbnMacroCC->setEnabled(true);
		m_CbnMacroCC->setVisible(true);
		m_CbnMacroPlug->setVisible(false);
		m_CbnMacroParam->setVisible(false);
		SetItemText(IDC_GENMACROLABEL, UL_("MIDI CC"));
		m_CbnMacroCC->setCurrentIndex(m_MidiCfg.MacroToMidiCC(sfx));
	} else
	{
		m_CbnMacroCC->setEnabled(false);
	}
}


OPENMPT_NAMESPACE_END
