/*
 * OPLInstrDlg.h
 * -------------
 * Purpose: Editor for OPL-based synth instruments
 * Notes  : mfcless twin of upstream's mptrack/OPLInstrDlg.h.
 *          Draft from qt/tools/twin.py, not yet edited by hand.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */

#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "mfcless/Geometry.h"
#include "mfcless/Keyboard.h"
#include "DialogBase.h"
#include "../soundlib/Snd_defs.h"

class QAbstractButton;
class QComboBox;
class QScrollBar;
class QSlider;
class QWidget;

OPENMPT_NAMESPACE_BEGIN

class CSoundFile;

class OPLInstrDlg : public DialogBase
{
	Q_OBJECT
	QAbstractButton *m_additive = nullptr, *m_sustain[2] = {}, *m_scaleEnv[2] = {}, *m_vibrato[2] = {}, *m_tremolo[2] = {};
	QSlider *m_feedback = nullptr, *m_attackRate[2] = {}, *m_decayRate[2] = {}, *m_sustainLevel[2] = {}, *m_releaseRate[2] = {}, *m_volume[2] = {}, *m_levelScaling[2] = {}, *m_freqMultiplier[2] = {};
	QComboBox *m_waveform[2] = {};
	mfcless::Size m_windowSize;
	QWidget &m_parent;
	QWidget * m_lastFocusItem = nullptr;
	const CSoundFile &m_sndFile;
	OPLPatch *m_patch;

public:
	OPLInstrDlg(QWidget &parent, const CSoundFile &sndFile);
	~OPLInstrDlg();
	void SetEnabled(bool enabled);
	void SetPatch(OPLPatch &patch);
	mfcless::Size GetMinimumSize() const { return m_windowSize; }

protected:
	void ParamsChanged();

	void DoDataExchange(DataExchange &dx) override;
	bool OnInitDialog() override;
	bool PreTranslateMessage(mfcless::Message *pMsg) override;
	mpt::ustring GetToolTipText(uint32 id, QWidget * hwnd) const override;
	void OnOK() override { }
	void OnCancel() override { }
	void OnHScroll(mfcless::ScrollCode nSBCode, int nPos, QWidget *pScrollBar) { ParamsChanged(); }
	intptr_t OnDragonDropping(uintptr_t wParam, intptr_t lParam);
	void RegisterMessageMap() override;
};

OPENMPT_NAMESPACE_END
