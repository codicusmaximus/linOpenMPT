/*
 * PatternEditorDialogs.h
 * ----------------------
 * Purpose: Code for various dialogs that are used in the pattern editor.
 * Notes  : mfcless twin of upstream's mptrack/PatternEditorDialogs.h.
 *          Draft from qt/tools/twin.py, not yet edited by hand.
 * Authors: Olivier Lapicque
 *          OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */

#pragma once

#include "openmpt/all/BuildSettings.hpp"

#include "DialogBase.h"
#include "dlg_misc.h"  // for keyboard control
#include "ColorPickerButton.h"
#include "EffectInfo.h"
#include "PatternCursor.h"
#include "PluginComboBox.h"
#include "ResizableDialog.h"
#include "TrackerSettings.h"

class QAbstractButton;
class QComboBox;
class QEvent;
class QLineEdit;
class QObject;
class QPoint;
class QScrollBar;
class QSlider;
class QSpinBox;
class QWidget;

OPENMPT_NAMESPACE_BEGIN

class CModDoc;
struct SplitKeyboardSettings;

class CPatternPropertiesDlg : public DialogBase
{
	Q_OBJECT
public:
	CPatternPropertiesDlg(CModDoc &modParent, PATTERNINDEX nPat, QWidget *parent = nullptr);

protected:
	void DoDataExchange();
	bool OnInitDialog() override;
	void OnOK() override;

	void OnHalfRowNumber();
	void OnDoubleRowNumber();
	void OnOverrideSignature();
	void OnTempoSwing();
	void OnPatternChanged();
	void OnVScroll(uint32 nSBCode, uint32 nPos, QScrollBar *pScrollBar);
	struct PatternProperties
	{
		std::string name;
		TempoSwing tempoSwing;
		ROWINDEX numRows = 0, rowsPerBeat = 0, rowsPerMeasure = 0;
		ROWINDEX resizeWarningShown = 0;
		bool resizeWarningAtEnd = false;
		bool resizeAtEnd = true, repeatContents = false;
	};

	PatternProperties& GetPatternProperties(PATTERNINDEX pat);
	PatternProperties& GetPatternProperties() { return GetPatternProperties(m_nPattern); }
	void StorePatternProperties();
	void SetCurrentPattern(PATTERNINDEX pat);
	bool ValidatePatternProperties();

	CModDoc &m_modDoc;
	std::map<PATTERNINDEX, PatternProperties> m_properties;
	PATTERNINDEX m_nPattern;
	QSpinBox *m_spinPattern = nullptr, *m_spinRPB = nullptr, *m_spinRPM = nullptr;
	QComboBox *m_numRows = nullptr;
	bool m_locked = true;
};


//////////////////////////////////////////////////////////////////////////
// Command Editing


class CEditCommand : public DialogBase
{
	Q_OBJECT
protected:
	QComboBox *cbnNote = nullptr, *cbnVolCmd = nullptr, *cbnCommand = nullptr, *cbnPlugParam = nullptr;
	PluginComboBox cbnInstr;
	QSlider *sldVolParam = nullptr, *sldParam = nullptr;
	CSoundFile &sndFile;
	const CModSpecifications *oldSpecs = nullptr;
	ModCommand *m = nullptr;
	EffectInfo effectInfo;
	PATTERNINDEX editPattern = PATTERNINDEX_INVALID;
	CHANNELINDEX editChannel = CHANNELINDEX_INVALID;
	ROWINDEX editRow = ROWINDEX_INVALID;
	uint32 xParam, xMultiplier;
	bool modified = false;

public:
	CEditCommand(CSoundFile &sndFile);

public:
	bool ShowEditWindow(PATTERNINDEX pat, const PatternCursor &cursor, QWidget *parent);

protected:
	void InitAll() { InitNote(); InitVolume(); InitEffect(); InitPlugParam(); }
	void InitNote();
	void InitVolume();
	void InitEffect();
	void InitPlugParam();

	void UpdateVolCmdRange();
	void UpdateVolCmdValue();
	void UpdateEffectRange(bool set);
	void UpdateEffectValue(bool set);

	void PrepareUndo(const char *description);
	void DoDataExchange();
	void OnOK() override { ShowWindow(SW_HIDE); }
	void OnCancel() override { ShowWindow(SW_HIDE); }
	bool eventFilter(QObject *watched, QEvent *e) override;
	void OnActivate(uint32 nState, QWidget *pWndOther, bool bMinimized);
	void OnClose()	{ ShowWindow(SW_HIDE); }

	void OnNoteChanged();
	void OnVolCmdChanged();
	void OnCommandChanged();
	void OnPlugParamChanged();
	void OnHScroll(uint32, uint32, QScrollBar *);
};


/////////////////////////////////////////////////////////////////////////
// Chord Editor

class CChordEditor : public ResizableDialog
{
	Q_OBJECT
protected:
	CKeyboardControl m_Keyboard;
	CComboBox m_CbnShortcut, m_CbnBaseNote, m_CbnNote[MPTChord::notesPerChord - 1];
	MPTChords m_chords;
	MPTChord::NoteType m_mouseDownKey = MPTChord::noNote, m_dragKey = MPTChord::noNote;

	static constexpr MPTChord::NoteType CHORD_MIN = -24;
	static constexpr MPTChord::NoteType CHORD_MAX = 24;

public:
	CChordEditor(QWidget *parent = nullptr);

protected:
	MPTChord &GetChord();

	void DoDataExchange();
	bool OnInitDialog() override;
	void OnOK() override;
	void OnDPIChanged() override;

	void UpdateKeyboard();
	intptr_t OnKeyboardNotify(uintptr_t, intptr_t);
	void OnChordChanged();
	void OnBaseNoteChanged();
	void OnNote1Changed() { OnNoteChanged(0); }
	void OnNote2Changed() { OnNoteChanged(1); }
	void OnNote3Changed() { OnNoteChanged(2); }
	void OnNoteChanged(int noteIndex);
};


/////////////////////////////////////////////////////////////////////////
// Keyboard Split Settings (pattern editor)

class CSplitKeyboardSettings : public DialogBase
{
	Q_OBJECT
protected:
	QComboBox *m_CbnSplitInstrument = nullptr, *m_CbnSplitNote = nullptr, *m_CbnOctaveModifier = nullptr, *m_CbnSplitVolume = nullptr;
	CSoundFile &sndFile;

public:
	SplitKeyboardSettings &m_Settings;

	CSplitKeyboardSettings(QWidget *parent, CSoundFile &sf, SplitKeyboardSettings &settings);

protected:
	void DoDataExchange();
	bool OnInitDialog() override;
	void OnOK() override;
	void OnCancel() override;

	void OnOctaveModifierChanged();
};


/////////////////////////////////////////////////////////////////////////
// Show channel properties from pattern editor

class QuickChannelProperties : public DialogBase
{
	Q_OBJECT
protected:
	CModDoc *m_document = nullptr;
	CHANNELINDEX m_channel = 0;
	bool m_visible = false;
	bool m_settingsChanged = false;
	bool m_settingColor = false;

	ColorPickerButton m_colorBtn, m_colorBtnPrev, m_colorBtnNext;
	QSlider *m_volSlider = nullptr, *m_panSlider = nullptr;
	QSpinBox *m_volSpin = nullptr, *m_panSpin = nullptr;
	QLineEdit *m_nameEdit = nullptr;

public:
	QuickChannelProperties() = default;
	~QuickChannelProperties();

	void Show(CModDoc *modDoc, CHANNELINDEX chn, QPoint position);
	void UpdateDisplay();
	CHANNELINDEX GetChannel() const { return m_channel; }

protected:
	void DoDataExchange();

	void PrepareUndo();
	void PickColorFromChannel(CHANNELINDEX channel);

	void OnActivate(uint32 nState, QWidget *, bool);
	void OnVolChanged();
	void OnPanChanged();
	void OnHScroll(uint32, uint32, QScrollBar *);
	void OnMuteChanged();
	void OnSurroundChanged();
	void OnNameChanged();
	void OnPrevChannel();
	void OnNextChannel();
	void OnChangeColor();
	void OnPickPrevColor();
	void OnPickNextColor();
	intptr_t OnCustomKeyMsg(uintptr_t, intptr_t);

	bool eventFilter(QObject *watched, QEvent *e) override;
	mpt::ustring GetToolTipText(uint32 id, QWidget * hwnd) const override;

	DECLARE_MESSAGE_MAP();
};


class MetronomeSettingsDlg : public DialogBase
{
	Q_OBJECT
public:
	MetronomeSettingsDlg(QWidget *parent = nullptr);

protected:
	void DoDataExchange();
	bool OnInitDialog() override;
	mpt::ustring GetToolTipText(uint32 id, QWidget * hwnd) const override;

	mpt::ustring GetVolumeString() const;
	void SetSampleInfo(const mpt::PathString &path, QComboBox &combo, QLineEdit &edit, QAbstractButton &browseButton);
	bool GetSampleInfo(Setting<mpt::PathString> &path, QComboBox &combo, QLineEdit &edit, QAbstractButton &browseButton);
	mpt::PathString BrowseForSample(const mpt::PathString &path);

	void OnHScroll(uint32, uint32, QScrollBar *);
	void OnToggleMetronome();
	void OnSampleChanged();
	void OnBrowseMeasure();
	void OnBrowseBeat();

	DECLARE_MESSAGE_MAP();

protected:
	QSlider *m_volumeSlider = nullptr;
	QComboBox *m_measureCombo = nullptr, *m_beatCombo = nullptr;
	QLineEdit *m_measureEdit = nullptr, *m_beatEdit = nullptr;
	QAbstractButton *m_measureButton = nullptr, *m_beatButton = nullptr;
};


OPENMPT_NAMESPACE_END
