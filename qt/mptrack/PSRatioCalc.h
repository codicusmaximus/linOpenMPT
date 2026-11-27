/*
 * PSRatioCalc.h
 * -------------
 * Purpose: Dialog for calculating time stretch shift ratios in the sample editor.
 * Notes  : mfcless twin of upstream's mptrack/PSRatioCalc.h.
 *          Draft from qt/tools/twin.py, not yet edited by hand.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"

#include "CDecimalSupport.h"
#include "DialogBase.h"

class QWidget;

OPENMPT_NAMESPACE_BEGIN

class CSoundFile;

class CPSRatioCalc : public DialogBase
{
	Q_OBJECT
public:
	CPSRatioCalc(const CSoundFile &sndFile, SAMPLEINDEX sample, double ratio, QWidget *parent = nullptr);

	double m_ratio = 100.0;

protected:
	bool OnInitDialog() override;

	void CalcSamples();
	void CalcMs();
	void CalcRows();

	void LockControls() { m_lockCount++; }
	void UnlockControls() { m_lockCount--; }
	bool IsLocked() const { return m_lockCount != 0; }

	void OnChangeSampleLength();
	void OnChangeDuration();
	void OnChangeSpeed();
	void OnChangeRows();
	void OnChangeRatio();

	CNumberEdit m_EditTempo;
	CNumberEdit m_EditRatio;
	CNumberEdit m_EditDurationOrig, m_EditDurationNew;
	CNumberEdit m_EditRowsOrig, m_EditRowsNew;

	const CSoundFile &m_sndFile;
	SAMPLEINDEX m_sampleIndex = 0;

	double m_durationOrig = 0, m_durationNew = 0;
	double m_rowsOrig = 0, m_rowsNew = 0;
	SmpLength m_sampleLengthNew = 0;
	uint32 m_speed = 0;
	TEMPO m_tempo;
	int m_lockCount = 0;

	void RegisterMessageMap() override;
};

OPENMPT_NAMESPACE_END
