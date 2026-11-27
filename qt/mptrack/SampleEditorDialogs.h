/*
 * SampleEditorDialogs.h
 * ---------------------
 * Purpose: Code for various dialogs that are used in the sample editor.
 * Notes  : mfcless twin of upstream's mptrack/SampleEditorDialogs.h.
 *          Draft from qt/tools/twin.py, not yet edited by hand.
 * Authors: Olivier Lapicque
 *          OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "mfcless/Controls.h"

#include "CDecimalSupport.h"
#include "DialogBase.h"
#include "../common/FileReaderFwd.h"
#include "../soundlib/SampleIO.h"
#include "../tracklib/FadeLaws.h"

class QAbstractButton;
class QComboBox;
class QLineEdit;
class QScrollBar;
class QSlider;
class QWidget;

OPENMPT_NAMESPACE_BEGIN

//////////////////////////////////////////////////////////////////////////
// Sample amplification dialog

class CAmpDlg : public DialogBase
{
	Q_OBJECT
public:
	struct AmpSettings
	{
		Fade::Law fadeLaw;
		int fadeInStart, fadeOutEnd;
		int16 factor;
		bool fadeIn, fadeOut;
	};

	AmpSettings &m_settings;
	int16 m_nFactorMin, m_nFactorMax;

protected:
	QComboBox *m_fadeBox = nullptr;
	CImageList m_list;
	CNumberEdit *m_edit = nullptr, *m_editFadeIn = nullptr, *m_editFadeOut = nullptr;
	bool m_locked = true;

public:
	CAmpDlg(QWidget *parent, AmpSettings &settings, int16 factorMin = int16_min, int16 factorMax = int16_max);

protected:
	void DoDataExchange(DataExchange &dx) override;
	bool OnInitDialog() override;
	void OnDPIChanged() override;
	void OnOK() override;
	void OnDestroy();

	void EnableFadeIn();
	void EnableFadeOut();

	void RegisterMessageMap() override;
};


//////////////////////////////////////////////////////////////////////////
// Sample import dialog

class CRawSampleDlg : public DialogBase
{
	Q_OBJECT
	friend class AutodetectFormatDlg;

protected:
	static SampleIO m_format;
	static SmpLength m_offset;

	mfcless::UpDown *m_SpinOffset = nullptr;
	FileReader &m_file;
	bool m_rememberFormat = false;

public:
	SampleIO GetSampleFormat() const { return m_format; }
	void SetSampleFormat(SampleIO nFormat) { m_format = nFormat; }

	bool GetRemeberFormat() const { return m_rememberFormat; };
	void SetRememberFormat(bool remember) { m_rememberFormat = remember; };

	SmpLength GetOffset() const { return m_offset; }
	void SetOffset(SmpLength offset) { m_offset = offset; }

public:
	CRawSampleDlg(FileReader &file, QWidget *parent = nullptr);

protected:
	void DoDataExchange(DataExchange &dx) override;
	bool OnInitDialog() override;
	void OnOK() override;
	void UpdateDialog();

	void OnBitDepthChanged(uint32 id);
	void OnEncodingChanged(uint32 id);
	void OnAutodetectFormat();

	void RegisterMessageMap() override;
};


/////////////////////////////////////////////////////////////////////////
// Add silence dialog - add silence to a sample

class AddSilenceDlg : public DialogBase
{
	Q_OBJECT
public:
	enum AddSilenceOptions
	{
		kSilenceAtBeginning, // Add at beginning of sample
		kSilenceAtEnd,       // Add at end of sample
		kResize,             // Resize sample
		kOPLInstrument,      // Initialize as OPL instrument
	};

	enum Unit
	{
		kSamples = 0,
		kMilliseconds,
	};

	SmpLength m_numSamples; // Add x samples (also containes the return value in all cases)
	SmpLength m_length;  // Set size to x samples (init value: current sample size)
	AddSilenceOptions m_editOption; // See above

protected:
	AccessibleComboBox *m_ComboUnit = nullptr;
	CNumberEdit *m_EditAmount = nullptr;
	static SmpLength m_addSamples;
	static SmpLength m_createSamples;
	uint32 m_sampleRate;
	Unit m_unit = kSamples;
	bool m_allowOPL;

public:
	AddSilenceDlg(QWidget *parent, SmpLength origLength, uint32 sampleRate, bool allowOPL);

	void DoDataExchange(DataExchange &dx) override;
	bool OnInitDialog() override;
	void OnOK() override;
	
protected:
	SmpLength GetEditLength() const;
	AddSilenceOptions GetEditMode() const;
	void OnEditModeChanged();
	void OnUnitChanged();
	void RegisterMessageMap() override;
};


/////////////////////////////////////////////////////////////////////////
// Sample grid dialog

class CSampleGridDlg : public DialogBase
{
	Q_OBJECT
public:
	SmpLength m_nSegments, m_nMaxSegments;

protected:
	QLineEdit *m_EditSegments = nullptr;
	mfcless::UpDown *m_SpinSegments = nullptr;

public:
	CSampleGridDlg(QWidget *parent, SmpLength nSegments, SmpLength nMaxSegments);

protected:
	void DoDataExchange(DataExchange &dx) override;
	bool OnInitDialog() override;
	void OnOK() override;
};


/////////////////////////////////////////////////////////////////////////
// Sample cross-fade dialog

class CSampleXFadeDlg : public DialogBase
{
	Q_OBJECT
public:
	static uint32 m_fadeLength;
	static uint32 m_fadeLaw;
	static bool m_afterloopFade;
	static bool m_useSustainLoop;
	SmpLength m_loopLength = 0, m_maxLength = 0;

protected:
	QSlider *m_SliderLength = nullptr, *m_SliderFadeLaw = nullptr;
	QLineEdit *m_EditSamples = nullptr;
	mfcless::UpDown *m_SpinSamples = nullptr;
	QAbstractButton *m_RadioNormalLoop = nullptr, *m_RadioSustainLoop = nullptr;
	ModSample &m_sample;
	bool m_editLocked = true;

public:
	CSampleXFadeDlg(QWidget *parent, ModSample &sample);

	SmpLength PercentToSamples(uint32 percent) const { return Util::muldivr_unsigned(percent, m_loopLength, 100000); }
	uint32 SamplesToPercent(SmpLength samples) const { return Util::muldivr_unsigned(samples, 100000, m_loopLength); }

protected:
	void DoDataExchange(DataExchange &dx) override;
	bool OnInitDialog() override;
	void OnOK() override;
	mpt::ustring GetToolTipText(uint32 id, QWidget * hwnd) const override;

	void OnLoopTypeChanged();
	void OnFadeLengthChanged();
	void OnHScroll(uint32, uint32, QScrollBar *);

	void RegisterMessageMap() override;
};


/////////////////////////////////////////////////////////////////////////
// Resampling dialog

class CResamplingDlg : public DialogBase
{
	Q_OBJECT
public:
	enum ResamplingOption
	{
		Upsample,
		Downsample,
		Custom
	};

protected:
	ResamplingMode m_srcMode;
	uint32 m_frequency;
	const bool m_resampleAll;
	const bool m_allowAdjustNotes;
	static uint32 m_lastFrequency;
	static ResamplingOption m_lastChoice;
	static bool m_updatePatternCommands;
	static bool m_updatePatternNotes;

public:
	CResamplingDlg(QWidget *parent, uint32 frequency, ResamplingMode srcMode, bool resampleAll, bool allowAdjustNotes);
	uint32 GetFrequency() const { return m_frequency; }
	ResamplingMode GetFilter() const { return m_srcMode; }
	static ResamplingOption GetResamplingOption() { return m_lastChoice; }
	static bool UpdatePatternCommands() { return m_updatePatternCommands; }
	static bool UpdatePatternNotes() { return m_updatePatternNotes; }

protected:
	bool OnInitDialog() override;
	void OnOK() override;

	void OnFocusEdit();
	void OnEditChanged();

	void RegisterMessageMap() override;
};


/////////////////////////////////////////////////////////////////////////
// Sample mix dialog

class CMixSampleDlg : public DialogBase
{
	Q_OBJECT
protected:
	// Dialog controls
	QLineEdit *m_EditOffset = nullptr;
	CNumberEdit *m_EditVolOriginal = nullptr, *m_EditVolMix = nullptr;
	mfcless::UpDown *m_SpinOffset = nullptr, *m_SpinVolOriginal = nullptr, *m_SpinVolMix = nullptr;

public:
	static SmpLength sampleOffset;
	static int amplifyOriginal;
	static int amplifyMix;

public:
	CMixSampleDlg(QWidget *parent);

protected:
	void DoDataExchange(DataExchange &dx) override;
	bool OnInitDialog() override;
	void OnOK() override;
};


OPENMPT_NAMESPACE_END
