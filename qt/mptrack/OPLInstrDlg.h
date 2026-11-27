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
#include "DialogBase.h"
#include "../soundlib/Snd_defs.h"

class QComboBox;
class QEvent;
class QObject;
class QScrollBar;
class QSize;
class QWidget;

OPENMPT_NAMESPACE_BEGIN

class CSoundFile;

class OPLInstrDlg : public DialogBase
{
	Q_OBJECT
	CButton m_additive, m_sustain[2], m_scaleEnv[2], m_vibrato[2], m_tremolo[2];
	CSliderCtrl m_feedback, m_attackRate[2], m_decayRate[2], m_sustainLevel[2], m_releaseRate[2], m_volume[2], m_levelScaling[2], m_freqMultiplier[2];
	QComboBox *m_waveform[2] = {};
	QSize m_windowSize;
	QWidget &m_parent;
	QWidget * m_lastFocusItem = nullptr;
	const CSoundFile &m_sndFile;
	OPLPatch *m_patch;

public:
	OPLInstrDlg(QWidget &parent, const CSoundFile &sndFile);
	~OPLInstrDlg();
	void SetEnabled(bool enabled);
	void SetPatch(OPLPatch &patch);
	QSize GetMinimumSize() const { return m_windowSize; }

protected:
	void ParamsChanged();

	void DoDataExchange();
	bool OnInitDialog() override;
	bool eventFilter(QObject *watched, QEvent *e) override;
	mpt::ustring GetToolTipText(uint32 id, QWidget * hwnd) const override;
	void OnOK() override { }
	void OnCancel() override { }
	void OnHScroll(uint32, uint32, QScrollBar *) { ParamsChanged(); }
	intptr_t OnDragonDropping(uintptr_t wParam, intptr_t lParam);
};

OPENMPT_NAMESPACE_END
