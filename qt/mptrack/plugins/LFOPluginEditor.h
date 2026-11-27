/*
 * LFOPluginEditor.h
 * -----------------
 * Purpose: Editor interface for the LFO plugin.
 * Notes  : mfcless twin of upstream's mptrack/plugins/LFOPluginEditor.h.
 * Authors: Johannes Schultz (OpenMPT Devs)
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"

#include "../AbstractVstEditor.h"
#include "../PluginComboBox.h"
#include "../../soundlib/plugins/LFOPlugin.h"

class QComboBox;
class QLineEdit;
class QSlider;
class QSpinBox;
class QWidget;

OPENMPT_NAMESPACE_BEGIN

struct UpdateHint;

class LFOPluginEditor : public CAbstractVstEditor
{
	Q_OBJECT
protected:
	QComboBox *m_plugParam = nullptr, *m_midiCC = nullptr;
	PluginComboBox *m_outPlug = nullptr;
	QSlider *m_amplitudeSlider = nullptr, *m_offsetSlider = nullptr, *m_frequencySlider = nullptr;
	QLineEdit *m_midiChnEdit = nullptr;
	QSpinBox *m_midiChnSpin = nullptr;
	LFOPlugin &m_lfoPlugin;
	bool m_locked : 1;
	static constexpr int SLIDER_GRANULARITY = 1000;

public:

	LFOPluginEditor(LFOPlugin &plugin);

	bool OpenEditor(QWidget *parent) override;
	bool IsResizable() const override { return false; }
	bool SetSize(int, int) override { return false; }

	void UpdateParamDisplays() override;
	void UpdateParam(int32 param) override;
	void UpdateView(UpdateHint hint) override;

protected:
	void DoDataExchange();
	void OnHScroll(QSlider *slider);

	void InitSlider(QSlider &slider, LFOPlugin::Parameters param);
	void SetSliderText(LFOPlugin::Parameters param);
	void SetSliderValue(QSlider &slider, float value);
	float GetSliderValue(QSlider &slider);

	void OnPolarityChanged();
	void OnTempoSyncChanged();
	void OnBypassChanged();
	void OnLoopModeChanged();
	void OnWaveformChanged(uint32 nID);
	void OnPlugParameterChanged();
	void OnMidiCCChanged();
	void OnParameterChanged();
	void OnOutputPlugChanged();
	void OnPluginEditor();
	intptr_t OnUpdateParam(uintptr_t wParam, intptr_t lParam);
};

OPENMPT_NAMESPACE_END
