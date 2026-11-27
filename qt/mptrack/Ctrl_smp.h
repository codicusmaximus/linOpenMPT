/*
 * Ctrl_smp.h
 * ----------
 * Purpose: Sample tab, upper panel.
 * Notes  : mfcless twin of upstream's mptrack/Ctrl_smp.h.
 *          Draft from qt/tools/twin.py, not yet edited by hand.
 * Authors: Olivier Lapicque
 *          OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "mfcless/Controls.h"
#include "mfcless/Keyboard.h"

#include "AccessibleControls.h"
#include "CDecimalSupport.h"
#include "Globals.h"
#include "Undo.h"
#include "UpdateHints.h"
#include "../soundlib/SampleIO.h"
#include "../tracklib/FadeLaws.h"

class QAbstractButton;
class QComboBox;
class QLineEdit;
class QObject;
class QPoint;
class QScrollBar;
class QWidget;

OPENMPT_NAMESPACE_BEGIN

enum OpenSampleTypes
{
	OpenSampleKnown = (1<<0),
	OpenSampleRaw   = (1<<1),
};
MPT_DECLARE_ENUM(OpenSampleTypes)

class CCtrlSamples: public CModControlDlg
{
	Q_OBJECT
protected:
	friend class DoPitchShiftTimeStretch;

	struct SampleSelectionPoints
	{
		SmpLength nStart;
		SmpLength nEnd;
		bool selectionActive;	// does sample selection exist or not?
	};

	CModControlBar *m_ToolBar1 = nullptr, *m_ToolBar2 = nullptr;
	QLineEdit *m_EditSample = nullptr, *m_EditName = nullptr, *m_EditFileName = nullptr;
	QLineEdit *m_EditLoopStart = nullptr, *m_EditLoopEnd = nullptr, *m_EditSustainStart = nullptr, *m_EditSustainEnd = nullptr;
	QLineEdit *m_EditVibSweep = nullptr, *m_EditVibDepth = nullptr, *m_EditVibRate = nullptr;
	QLineEdit *m_EditVolume = nullptr, *m_EditGlobalVol = nullptr, *m_EditPanning = nullptr;
	CNumberEdit *m_EditFineTune = nullptr, *m_EditTimeStretchRatio = nullptr, *m_EditPitch = nullptr;
	mfcless::UpDown *m_SpinVolume = nullptr, *m_SpinGlobalVol = nullptr, *m_SpinPanning = nullptr, *m_SpinVibSweep = nullptr, *m_SpinVibDepth = nullptr, *m_SpinVibRate = nullptr;
	mfcless::UpDown *m_SpinLoopStart = nullptr, *m_SpinLoopEnd = nullptr, *m_SpinSustainStart = nullptr, *m_SpinSustainEnd = nullptr;
	mfcless::UpDown *m_SpinFineTune = nullptr, *m_SpinSample = nullptr, *m_SpinTimeStretchRatio = nullptr;
	QComboBox *m_ComboAutoVib = nullptr, *m_ComboLoopType = nullptr, *m_ComboSustainType = nullptr, *m_ComboZoom = nullptr, *m_CbnBaseNote = nullptr, *m_ComboGrainSize = nullptr;
	QAbstractButton *m_CheckPanning = nullptr;
	SAMPLEINDEX m_nSample = 1;
	INSTRUMENTINDEX m_editInstrumentName = INSTRUMENTINDEX_INVALID;
	bool m_rememberRawFormat = false;
	bool m_startedEdit = false;

	QComboBox *m_ComboPitch = nullptr;

public:
	CCtrlSamples(CModControlView &parent, CModDoc &document);
	~CCtrlSamples();

protected:
	bool IsOPLInstrument() const;
	
	bool SetCurrentSample(SAMPLEINDEX nSmp, int32 lZoom = -1, bool bUpdNum = true);
	bool InsertSample(bool duplicate, int8 *confirm = nullptr);
	bool OpenSample(const mpt::PathString &fileName, FlagSet<OpenSampleTypes> types = OpenSampleKnown | OpenSampleRaw);
	bool OpenSample(const CSoundFile &sndFile, SAMPLEINDEX nSample);
	void OpenSamples(const std::vector<mpt::PathString> &files, FlagSet<OpenSampleTypes> types);
	void SaveSample(bool doBatchSave);

	void Normalize(bool allSamples);
	void RemoveDCOffset(bool allSamples);

	void ApplyAmplify(const double amp, const double fadeInStart, const double fadeOutEnd, const bool fadeIn, const bool fadeOut, const Fade::Law fadeLaw);
	void ApplyResample(SAMPLEINDEX smp, uint32 newRate, ResamplingMode mode, bool ignoreSelection = false, bool updatePatternCommands = false, bool updatePatternNotes = false);

	SampleSelectionPoints GetSelectionPoints();
	void SetSelectionPoints(SmpLength nStart, SmpLength nEnd);

	void PropagateAutoVibratoChanges();
	void SetFinetune(int step);

public:
	Setting<int32> &GetSplitPosRef() override;
	bool OnInitDialog() override;
	void DoDataExchange(DataExchange &dx) override;	// DDX/DDV support
	CRuntimeClass *GetAssociatedViewClass() override;
	void RecalcLayout() override;
	void OnActivatePage(intptr_t) override;
	void OnDeactivatePage() override;
	void UpdateView(UpdateHint hint, QObject *pObj = nullptr) override;
	intptr_t OnModCtrlMsg(uintptr_t wParam, intptr_t lParam) override;
	mpt::ustring GetToolTipText(uint32 uId, QWidget * hwnd) const override;
	bool PreTranslateMessage(mfcless::Message *pMsg) override;
	void OnDPIChanged() override;
protected:
	void OnEditFocus();
	void OnSampleChanged();
	void OnZoomChanged();
	void OnPrevInstrument();
	void OnNextInstrument();
	void OnTbnDropDownToolBar();
	void OnSampleNew();
	void OnSampleDuplicate() { InsertSample(true); }
	void OnSampleOpen();
	void OnSampleOpenKnown();
	void OnSampleOpenRaw();
	void OnSampleSave();
	void OnSampleSaveOne() { SaveSample(false); }
	void OnSampleSaveAll() { SaveSample(true); }
	void OnSamplePlay();
	void OnNormalize();
	void OnAmplify();
	void OnQuickFade();
	void OnRemoveDCOffset();
	void OnResample();
	void OnReverse();
	void OnSilence();
	void OnInvert();
	void OnSignUnSign();
	void OnAutotune();
	void OnNameChanged();
	void OnFileNameChanged();
	void OnVolumeChanged();
	void OnGlobalVolChanged();
	void OnSetPanningChanged();
	void OnPanningChanged();
	void OnFineTuneChanged();
	void OnFineTuneChangedDone();
	void OnBaseNoteChanged();
	void OnLoopTypeChanged();
	void OnLoopPointsChanged();
	void OnSustainTypeChanged();
	void OnSustainPointsChanged();
	void OnVibTypeChanged();
	void OnVibDepthChanged();
	void OnVibSweepChanged();
	void OnVibRateChanged();
	void OnXFade();
	void OnStereoSeparation();
	void OnKeepSampleOnDisk();
	void OnVScroll(uint32, uint32, QScrollBar *);
	intptr_t OnCustomKeyMsg(uintptr_t, intptr_t);
	void OnXButtonUp(uint32 nFlags, uint32 nButton, QPoint point);

	void OnPitchShiftTimeStretch();
	void OnToggleTimestretchQuality();
	void OnEstimateSampleSize();

	void OnInitOPLInstrument();

	MPT_NOINLINE void SetModified(SAMPLEINDEX smp, SampleHint hint, bool updateAll, bool waveformModified);
	void SetModified(SampleHint hint, bool updateAll, bool waveformModified) { SetModified(m_nSample, hint, updateAll, waveformModified); }
	void PrepareUndo(const char *description, sampleUndoTypes type = sundo_none, SmpLength start = 0, SmpLength end = 0);
	void RegisterMessageMap() override;
};

OPENMPT_NAMESPACE_END
