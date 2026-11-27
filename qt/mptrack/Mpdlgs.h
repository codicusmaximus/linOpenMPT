/*
 * MPDlgs.h
 * --------
 * Purpose: Implementation of various player setup dialogs.
 * Notes  : mfcless twin of upstream's mptrack/Mpdlgs.h.
 *          Draft from qt/tools/twin.py, not yet edited by hand.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "DialogBase.h"
#include "mfcless/Controls.h"
#include "mfcless/Window.h"
#include <QtWidgets/QSlider>

#include "AccessibleControls.h"
#include "TrackerSettings.h"
#include "../sounddsp/EQ.h"
#include "openmpt/sounddevice/SoundDevice.hpp"

class QAbstractButton;
class QComboBox;
class QLineEdit;
class QScrollBar;
class QWidget;

OPENMPT_NAMESPACE_BEGIN

class CSoundFile;
class CMainFrame;

#define NUM_CHANNELCOMBOBOXES	4

class COptionsSoundcard: public PropertyPage
{
	Q_OBJECT
protected:
	QComboBox *m_CbnDevice = nullptr;
	QComboBox *m_CbnLatencyMS = nullptr, *m_CbnUpdateIntervalMS = nullptr, *m_CbnMixingFreq = nullptr, *m_CbnChannels = nullptr, *m_CbnSampleFormat = nullptr, *m_CbnDither = nullptr, *m_CbnRecordingChannels = nullptr, *m_CbnRecordingSource = nullptr;
	QLineEdit *m_EditStatistics = nullptr;
	QAbstractButton *m_BtnDriverPanel = nullptr;

	QComboBox *m_CbnStoppedMode = nullptr;

	QComboBox *m_CbnChannelMapping[NUM_CHANNELCOMBOBOXES] = {};

	SoundDevice::Identifier m_InitialDeviceIdentifier;

	void SetInitialDevice();

	void SetDevice(SoundDevice::Identifier dev, bool forceReload=false);
	SoundDevice::Info m_CurrentDeviceInfo;
	SoundDevice::Caps m_CurrentDeviceCaps;
	SoundDevice::DynamicCaps m_CurrentDeviceDynamicCaps;
	SoundDevice::Settings m_Settings;

public:
	COptionsSoundcard(SoundDevice::Identifier deviceIdentifier);

	void UpdateStatistics();

private:
	void UpdateEverything();
	void UpdateDevice();
	void UpdateGeneral();
	void UpdateLatency();
	void UpdateUpdateInterval();
	void UpdateSampleRates();
	void UpdateChannels();
	void UpdateSampleFormat();
	void UpdateDither();
	void UpdateChannelMapping();
	void UpdateRecording();
	void UpdateControls();

protected:
	bool OnInitDialog() override;
	void OnOK() override;
	bool OnSetActive() override;
	void DoDataExchange(DataExchange &dx) override;
	void UpdateStereoSep();

	void OnDeviceChanged();
	void OnSettingsChanged() { SetModified(true); }
	void OnExclusiveModeChanged();
	void OnChannelsChanged();
	void OnSampleFormatChanged();
	void OnRecordingChanged();
	void OnSoundCardShowAll();
	void OnSoundCardRescan();
	void OnSoundCardDriverPanel();

	void OnChannelChanged(int channel);
	void OnChannel1Changed() { OnChannelChanged(0); };
	void OnChannel2Changed() { OnChannelChanged(1); };
	void OnChannel3Changed() { OnChannelChanged(2); };
	void OnChannel4Changed() { OnChannelChanged(3); };

	void RegisterMessageMap() override;
};


class COptionsMixer: public PropertyPage
{
	Q_OBJECT
protected:

	QComboBox *m_CbnResampling = nullptr, *m_CbnAmigaType = nullptr;

	AccessibleEdit *m_CEditRampUp = nullptr;
	AccessibleEdit *m_CEditRampDown = nullptr;
	QLineEdit *m_CInfoRampUp = nullptr;
	QLineEdit *m_CInfoRampDown = nullptr;

	QSlider *m_SliderStereoSep = nullptr;

	// check box soft pan

	QSlider *m_SliderPreAmp = nullptr;

	bool m_initialized = false;

public:
	COptionsMixer();

protected:
	void UpdateRamping();
	void UpdateStereoSep();

	bool OnInitDialog() override;
	void OnOK() override;
	bool OnSetActive() override;
	void DoDataExchange(DataExchange &dx) override;

	void OnSettingsChanged() { SetModified(true); }
	void OnAmigaChanged();
	void OnRampingChanged();
	void OnDefaultRampSettings();

	void OnHScroll(mfcless::ScrollCode nSBCode, int nPos, QWidget *pScrollBar);

	void RegisterMessageMap() override;

};


#ifndef NO_EQ

class CEQSlider: public mfcless::Window<QSlider>
{
	Q_OBJECT
public:
	QWidget *m_pParent;
	uint32 m_nSliderNo;
	short int m_x, m_y;
public:
	CEQSlider() = default;
	void Init(uint32 nID, uint32 n, QWidget *parent);
	bool PreTranslateMessage(mfcless::Message *pMsg);
};

#endif // !NO_EQ


class COptionsPlayer: public PropertyPage
{
	Q_OBJECT
protected:
	QComboBox *m_CbnReverbPreset = nullptr;
	QSlider *m_SbXBassDepth = nullptr, *m_SbXBassRange = nullptr;
	QSlider *m_SbSurroundDepth = nullptr, *m_SbSurroundDelay = nullptr;
	QSlider *m_SbReverbDepth = nullptr;
	QSlider *m_SbBitCrushBits = nullptr;

#ifndef NO_EQ
	CEQSlider *m_Sliders[MAX_EQ_BANDS] = {};
	EQPreset &m_EQPreset;
	uint32 m_nSliderMenu;
#endif // !NO_EQ

public:
	COptionsPlayer();

protected:
	bool OnInitDialog() override;
	void OnOK() override;
	bool OnSetActive() override;
	void DoDataExchange(DataExchange &dx) override;
	void OnHScroll(mfcless::ScrollCode nSBCode, int nPos, QWidget *pScrollBar);
	void OnSettingsChanged() { SetModified(true); }

#ifndef NO_EQ
	void OnVScroll(mfcless::ScrollCode nSBCode, int nPos, QWidget *pScrollBar);
	void OnEqUser1() { LoadEQPreset(TrackerSettings::Instance().m_EqUserPresets[0]); };
	void OnEqUser2() { LoadEQPreset(TrackerSettings::Instance().m_EqUserPresets[1]); };
	void OnEqUser3() { LoadEQPreset(TrackerSettings::Instance().m_EqUserPresets[2]); };
	void OnEqUser4() { LoadEQPreset(TrackerSettings::Instance().m_EqUserPresets[3]); };
	void OnSavePreset();
	void OnSliderMenu(uint32);
	void OnSliderFreq(uint32);

	void UpdateDialog();
	void UpdateEQ(bool bReset);
	void LoadEQPreset(const EQPreset &preset);
#endif // !NO_EQ

	void RegisterMessageMap() override;
};


class CMidiSetupDlg: public PropertyPage
{
	Q_OBJECT
public:
	FlagSet<MidiSetup> m_midiSetup;
	uint32 m_nMidiDevice;

public:
	CMidiSetupDlg(FlagSet<MidiSetup> flags, uint32 device);

protected:
	bool OnInitDialog() override;
	void OnOK() override;
	bool OnSetActive() override;
	void DoDataExchange(DataExchange &dx) override;
	void RefreshDeviceList(uint32 currentDevice);
	void OnRenameDevice();
	void OnSettingsChanged() { SetModified(true); }
	void RegisterMessageMap() override;

protected:
	mfcless::UpDown *m_SpinSpd = nullptr, *m_SpinPat = nullptr, *m_SpinAmp = nullptr;
	QComboBox *m_InputDevice = nullptr, *m_ATBehaviour = nullptr, *m_Quantize = nullptr, *m_ContinueMode = nullptr, *m_RecordPitchBend = nullptr;
	AccessibleEdit *m_editAmp = nullptr;
};



class COptionsWine: public PropertyPage
{
	Q_OBJECT

protected:
	QComboBox *m_CbnPulseAudio = nullptr;
	QComboBox *m_CbnPortAudio = nullptr;
	QComboBox *m_CbnRtAudio = nullptr;

public:
	COptionsWine();

protected:
	bool OnInitDialog() override;
	void OnOK() override;
	bool OnSetActive() override;
	void DoDataExchange(DataExchange &dx) override;

	void OnSettingsChanged();
	
	void RegisterMessageMap() override;
};



OPENMPT_NAMESPACE_END
