/*
 * LFOPluginEditor.cpp
 * -------------------
 * Purpose: Editor interface for the LFO plugin.
 * Notes  : mfcless twin of upstream's mptrack/plugins/LFOPluginEditor.cpp.
 *          Hand-edited from the twin.py draft.
 * Authors: Johannes Schultz (OpenMPT Devs)
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "LFOPluginEditor.h"
#include "mfcless/Widgets.h"
#include "../UpdateHints.h"
#include "../../soundlib/Sndfile.h"
#include "../../soundlib/MIDIEvents.h"
#include "../../mptrack/resource.h"

OPENMPT_NAMESPACE_BEGIN

void LFOPluginEditor::RegisterMessageMap()
{
	CAbstractVstEditor::RegisterMessageMap();
	AddScroll(Qt::Horizontal, &LFOPluginEditor::OnHScroll);
	AddCommand(IDC_BUTTON1, &LFOPluginEditor::OnPluginEditor);
	AddCommand(IDC_CHECK1, &LFOPluginEditor::OnPolarityChanged);
	AddCommand(IDC_CHECK2, &LFOPluginEditor::OnTempoSyncChanged);
	AddCommand(IDC_CHECK3, &LFOPluginEditor::OnBypassChanged);
	AddCommand(IDC_CHECK4, &LFOPluginEditor::OnLoopModeChanged);
	AddCommandRange(IDC_RADIO1, IDC_RADIO1 + LFOPlugin::kNumWaveforms - 1, &LFOPluginEditor::OnWaveformChanged);
	AddNotify(IDC_COMBO1, mfcless::Notify::SelChange, &LFOPluginEditor::OnPlugParameterChanged);
	AddNotify(IDC_COMBO2, mfcless::Notify::SelChange, &LFOPluginEditor::OnOutputPlugChanged);
	AddNotify(IDC_COMBO3, mfcless::Notify::SelChange, &LFOPluginEditor::OnMidiCCChanged);
	AddCommand(IDC_RADIO7, &LFOPluginEditor::OnParameterChanged);
	AddCommand(IDC_RADIO8, &LFOPluginEditor::OnParameterChanged);
	AddNotify(IDC_EDIT1, mfcless::Notify::EditUpdate, &LFOPluginEditor::OnParameterChanged);
	AddMessage(LFOPlugin::WM_PARAM_UDPATE, &LFOPluginEditor::OnUpdateParam);
}


void LFOPluginEditor::DoDataExchange(DataExchange &dx)
{
	CAbstractVstEditor::DoDataExchange(dx);
	//{{AFX_DATA_MAP(LFOPluginEditor)
	ExchangeControl(dx, IDC_COMBO1, m_plugParam);
	ExchangeControl(dx, IDC_COMBO2, m_outPlug);
	ExchangeControl(dx, IDC_COMBO3, m_midiCC);
	ExchangeControl(dx, IDC_SLIDER1, m_amplitudeSlider);
	ExchangeControl(dx, IDC_SLIDER2, m_offsetSlider);
	ExchangeControl(dx, IDC_SLIDER3, m_frequencySlider);
	ExchangeControl(dx, IDC_EDIT1, m_midiChnEdit);
	ExchangeControl(dx, IDC_SPIN1, m_midiChnSpin);
	//}}AFX_DATA_MAP
}


LFOPluginEditor::LFOPluginEditor(LFOPlugin &plugin)
	: CAbstractVstEditor(plugin)
	, m_lfoPlugin(plugin)
	, m_locked(true)
{
}


bool LFOPluginEditor::OpenEditor(QWidget *parent)
{
	m_locked = true;
	Create(IDD_LFOPLUGIN, parent);

	m_midiChnSpin->SetRange(1, 16);
	m_midiCC->setUpdatesEnabled(false);
	mpt::ustring s;
	for(unsigned int i = 0; i < 128; i++)
	{
		s = mfcless::PrintF(UL_("%3u: "), i);
		s += mpt::ToUnicode(mpt::Charset::UTF8, MIDIEvents::MidiCCNames[i]);
		mfcless::AddItem(m_midiCC, s);
	}
	if(m_lfoPlugin.m_outputToCC && m_lfoPlugin.m_outputParam != LFOPlugin::INVALID_OUTPUT_PARAM)
	{
		m_midiCC->setCurrentIndex(m_lfoPlugin.m_outputParam & 0x7F);
		SetItemInt(IDC_EDIT1, 1 + ((m_lfoPlugin.m_outputParam & 0xF00) >> 8));
	} else
	{
		SetItemInt(IDC_EDIT1, 1);
	}
	m_midiCC->setUpdatesEnabled(true);

	UpdateView(PluginHint().Info().Names());

	for(int32 i = 0; i < LFOPlugin::kLFONumParameters; i++)
	{
		UpdateParam(i);
	}
	UpdateParamDisplays();

	m_locked = false;
	// Avoid weird WM_COMMAND message (following a WM_ACTIVATE message) activating a wrong waveform when closing the plugin editor while the pattern editor is open
	mfcless::GotoDlgCtrl(GetDlgItem(IDC_RADIO1 + m_lfoPlugin.m_waveForm));
	return CAbstractVstEditor::OpenEditor(parent);
}


void LFOPluginEditor::UpdateParamDisplays()
{
	CAbstractVstEditor::UpdateParamDisplays();
	m_locked = true;
	CheckRadio(IDC_RADIO7, IDC_RADIO8, m_lfoPlugin.m_outputToCC ? IDC_RADIO8 : IDC_RADIO7);
	m_plugParam->setUpdatesEnabled(false);
	IMixPlugin *outPlug = m_lfoPlugin.GetOutputPlugin();
	if(outPlug != nullptr)
	{
		if(reinterpret_cast<qlonglong>(outPlug) != m_plugParam->property("mfclessUserData").toLongLong())
		{
			m_plugParam->clear();
			AddPluginParameternamesToCombobox(*m_plugParam, *outPlug);
			if(!m_lfoPlugin.m_outputToCC)
				m_plugParam->setCurrentIndex(m_lfoPlugin.m_outputParam);
			m_plugParam->setProperty("mfclessUserData", QVariant::fromValue<qlonglong>(reinterpret_cast<qlonglong>(outPlug)));
		}
		EnableItem(IDC_BUTTON1, outPlug ? true :false);
	} else
	{
		m_plugParam->clear();
		m_plugParam->setProperty("mfclessUserData", QVariant::fromValue<qlonglong>(0));
	}
	const bool enableWindow = outPlug ? true : false;
	m_midiCC->setEnabled(enableWindow);
	m_midiChnEdit->setEnabled(enableWindow);
	m_midiChnSpin->setEnabled(enableWindow);
	EnableItem(IDC_RADIO7, enableWindow);
	EnableItem(IDC_RADIO8, enableWindow);

	m_plugParam->setUpdatesEnabled(true);
	m_locked = false;
}


void LFOPluginEditor::UpdateParam(int32 p)
{
	LFOPlugin::Parameters param = static_cast<LFOPlugin::Parameters>(p);
	CAbstractVstEditor::UpdateParam(p);
	m_locked = true;
	switch(param)
	{
	case LFOPlugin::kAmplitude:
		InitSlider(m_amplitudeSlider, LFOPlugin::kAmplitude);
		break;
	case LFOPlugin::kOffset:
		InitSlider(m_offsetSlider, LFOPlugin::kOffset);
		break;
	case LFOPlugin::kFrequency:
		InitSlider(m_frequencySlider, LFOPlugin::kFrequency);
		break;
	case LFOPlugin::kTempoSync:
		SetChecked(IDC_CHECK2, m_lfoPlugin.m_tempoSync ? 1 : 0);
		InitSlider(m_frequencySlider, LFOPlugin::kFrequency);
		break;
	case LFOPlugin::kWaveform:
		CheckRadio(IDC_RADIO1, IDC_RADIO1 + LFOPlugin::kNumWaveforms - 1, IDC_RADIO1 + m_lfoPlugin.m_waveForm);
		break;
	case LFOPlugin::kPolarity:
		SetChecked(IDC_CHECK1, m_lfoPlugin.m_polarity ? 1 : 0);
		break;
	case LFOPlugin::kBypassed:
		SetChecked(IDC_CHECK3, m_lfoPlugin.m_bypassed ? 1 : 0);
		break;
	case LFOPlugin::kLoopMode:
		SetChecked(IDC_CHECK4, m_lfoPlugin.m_oneshot ? 1 : 0);
		break;
	default:
		break;
	}
	m_locked = false;
}


void LFOPluginEditor::UpdateView(UpdateHint hint)
{
	CAbstractVstEditor::UpdateView(hint);
	m_outPlug->Update(PluginComboBox::Config{PluginComboBox::ShowLibraryNames | PluginComboBox::ShowNoPlugin}
		.Hint(hint)
		.CurrentSelection(m_lfoPlugin.m_pMixStruct->GetOutputPlugin())
		.FirstPlugin(m_lfoPlugin.GetSlot() + 1), m_lfoPlugin.GetSoundFile());
}


void LFOPluginEditor::InitSlider(QSlider *slider, LFOPlugin::Parameters param)
{
	slider->setRange(0, SLIDER_GRANULARITY);
	slider->setTickInterval(SLIDER_GRANULARITY / 10);
	SetSliderValue(slider, m_lfoPlugin.GetParameter(param));
	SetSliderText(param);
}


void LFOPluginEditor::SetSliderText(LFOPlugin::Parameters param)
{
	mpt::ustring s = m_lfoPlugin.GetParamName(param) + UL_(": ") + m_lfoPlugin.GetFormattedParamValue(param);
	SetItemText(IDC_STATIC1 + param, s);
}


void LFOPluginEditor::SetSliderValue(QSlider *slider, float value)
{
	slider->setValue(mpt::saturate_round<int>(value * SLIDER_GRANULARITY));
}


float LFOPluginEditor::GetSliderValue(QSlider *slider)
{
	float value = slider->value() / static_cast<float>(SLIDER_GRANULARITY);
	return value;
}


void LFOPluginEditor::OnHScroll(mfcless::ScrollCode nCode, int nPos, QWidget *pSB)
{
	CAbstractVstEditor::OnHScroll(nCode, nPos, pSB);
	if(!m_locked && nCode != mfcless::ScrollCode::EndScroll && pSB != nullptr)
	{
		auto slider = qobject_cast<QSlider *>(pSB);
		LFOPlugin::Parameters param;
		if(slider == m_amplitudeSlider)
			param = LFOPlugin::kAmplitude;
		else if(slider == m_offsetSlider)
			param = LFOPlugin::kOffset;
		else if(slider == m_frequencySlider)
			param = LFOPlugin::kFrequency;
		else
			return;

		float value = GetSliderValue(slider);
		m_lfoPlugin.SetParameter(param, value);
		m_lfoPlugin.AutomateParameter(param);
		SetSliderText(param);
	}
}


void LFOPluginEditor::OnPolarityChanged()
{
	if(!m_locked)
	{
		m_lfoPlugin.m_polarity = IsChecked(IDC_CHECK1) != 0;
		m_lfoPlugin.AutomateParameter(LFOPlugin::kPolarity);
	}
}


void LFOPluginEditor::OnTempoSyncChanged()
{
	if(!m_locked)
	{
		m_lfoPlugin.m_tempoSync = IsChecked(IDC_CHECK2) != 0;
		m_lfoPlugin.RecalculateFrequency();
		m_lfoPlugin.AutomateParameter(LFOPlugin::kTempoSync);
		InitSlider(m_frequencySlider, LFOPlugin::kFrequency);
	}
}


void LFOPluginEditor::OnBypassChanged()
{
	if(!m_locked)
	{
		m_lfoPlugin.m_bypassed = IsChecked(IDC_CHECK3) != 0;
		m_lfoPlugin.AutomateParameter(LFOPlugin::kBypassed);
	}
}


void LFOPluginEditor::OnLoopModeChanged()
{
	if(!m_locked)
	{
		m_lfoPlugin.m_oneshot = IsChecked(IDC_CHECK4) != 0;
		m_lfoPlugin.AutomateParameter(LFOPlugin::kLoopMode);
	}
}


void LFOPluginEditor::OnWaveformChanged(uint32 nID)
{
	if(!m_locked)
	{
		m_lfoPlugin.m_waveForm = static_cast<LFOPlugin::LFOWaveform>(nID - IDC_RADIO1);
		m_lfoPlugin.AutomateParameter(LFOPlugin::kWaveform);
	}
}


void LFOPluginEditor::OnPlugParameterChanged()
{
	if(!m_locked)
	{
		CheckRadio(IDC_RADIO7, IDC_RADIO8, IDC_RADIO7);
		OnParameterChanged();
	}
}


void LFOPluginEditor::OnMidiCCChanged()
{
	if(!m_locked)
	{
		CheckRadio(IDC_RADIO7, IDC_RADIO8, IDC_RADIO8);
		OnParameterChanged();
	}
}


void LFOPluginEditor::OnParameterChanged()
{
	if(!m_locked)
	{
		m_locked = true;
		PlugParamIndex param = 0;
		bool outputToCC = IsChecked(IDC_RADIO8) != 0;
		if(outputToCC)
		{
			param = ((Clamp(GetItemInt(IDC_EDIT1), 1u, 16u) - 1) << 8);
			if(m_lfoPlugin.m_outputToCC || m_midiCC->currentIndex() >= 0)
				param |= (m_midiCC->currentIndex() & 0x7F);
		} else
		{
			if(!m_lfoPlugin.m_outputToCC || m_plugParam->currentIndex() >= 0)
				param = static_cast<PlugParamIndex>(mfcless::GetItemData(m_plugParam, m_plugParam->currentIndex()));
		}
		m_lfoPlugin.m_outputToCC = outputToCC;
		m_lfoPlugin.m_outputParam = param;
		m_lfoPlugin.SetModified();
		m_locked = false;
	}
}


void LFOPluginEditor::OnOutputPlugChanged()
{
	if(m_locked)
		return;

	PLUGINDEX plug = m_outPlug->GetSelection().value_or(PLUGINDEX_INVALID);
	m_lfoPlugin.GetSoundFile().m_MixPlugins[m_lfoPlugin.GetSlot()].SetOutputPlugin(plug);
	m_lfoPlugin.SetModified();
	UpdateParamDisplays();
	if(CModDoc *modDoc = m_lfoPlugin.GetSoundFile().GetpModDoc(); modDoc != nullptr)
		modDoc->UpdateAllViews(nullptr, PluginHint(m_lfoPlugin.GetSlot() + 1).Info(), this);
}


void LFOPluginEditor::OnPluginEditor()
{
	std::vector<IMixPlugin *> plug;
	if(m_lfoPlugin.GetOutputPlugList(plug) && plug.front() != nullptr)
	{
		plug.front()->ToggleEditor();
	}
}


intptr_t LFOPluginEditor::OnUpdateParam(uintptr_t wParam, intptr_t lParam)
{
	if(wParam == m_lfoPlugin.GetSlot())
	{
		UpdateParam(static_cast<int32>(lParam));
	}
	return 0;
}


OPENMPT_NAMESPACE_END
