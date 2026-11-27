/*
 * PSRatioCalc.cpp
 * ---------------
 * Purpose: Dialog for calculating sample time stretch ratios in the sample editor.
 * Notes  : mfcless twin of upstream's mptrack/PSRatioCalc.cpp.
 *          Draft from qt/tools/twin.py, not yet edited by hand.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "PSRatioCalc.h"
#include "mfcless/Widgets.h"
#include "resource.h"
#include "../soundlib/Sndfile.h"


OPENMPT_NAMESPACE_BEGIN


CPSRatioCalc::CPSRatioCalc(const CSoundFile &sndFile, SAMPLEINDEX sample, double ratio, QWidget *parent)
	: DialogBase{IDD_PITCHSHIFT, parent}
	, m_ratio{ratio}
	, m_sndFile{sndFile}
	, m_sampleIndex{sample}
	, m_speed{sndFile.m_PlayState.m_nMusicSpeed}
	, m_tempo{sndFile.m_PlayState.m_nMusicTempo}
{
	const ModSample &smp = m_sndFile.GetSample(m_sampleIndex);
	uint32 sampleRate = smp.GetSampleRate(m_sndFile.GetType());
	if(sampleRate <= 0)
		sampleRate = 8363;

	// Sample rate will not change. We can calculate original duration once and disgard sampleRate.
	m_durationOrig = 1000.0 * static_cast<double>(smp.nLength) / sampleRate;
}


void CPSRatioCalc::RegisterMessageMap()
{
	DialogBase::RegisterMessageMap();
	AddNotify(IDC_SAMPLE_LENGTH_NEW, mfcless::Notify::EditUpdate, &CPSRatioCalc::OnChangeSampleLength);
	AddNotify(IDC_MS_LENGTH_NEW, mfcless::Notify::EditUpdate, &CPSRatioCalc::OnChangeDuration);
	AddNotify(IDC_SPEED, mfcless::Notify::EditUpdate, &CPSRatioCalc::OnChangeSpeed);
	AddNotify(IDC_TEMPO, mfcless::Notify::EditUpdate, &CPSRatioCalc::OnChangeSpeed);
	AddNotify(IDC_ROW_LENGTH_NEW2, mfcless::Notify::EditUpdate, &CPSRatioCalc::OnChangeRows);
	AddNotify(IDC_PSRATIO, mfcless::Notify::EditUpdate, &CPSRatioCalc::OnChangeRatio);
}


bool CPSRatioCalc::OnInitDialog()
{
	DialogBase::OnInitDialog();

	LockControls();

	const std::pair<CNumberEdit &, int> NumberControls[] =
	{
		{m_EditTempo,        IDC_TEMPO},
		{m_EditRatio,        IDC_PSRATIO},
		{m_EditRowsOrig,     IDC_ROW_LENGTH_ORIGINAL},
		{m_EditRowsNew,      IDC_ROW_LENGTH_NEW2},
		{m_EditDurationOrig, IDC_MS_LENGTH_ORIGINAL2},
		{m_EditDurationNew,  IDC_MS_LENGTH_NEW},
	};
	for(auto [editCtrl, dlgID] : NumberControls)
	{
		editCtrl.SubclassDlgItem(dlgID, this);
		editCtrl.AllowNegative(false);
	}

	SetDlgItemInt(IDC_SAMPLE_LENGTH_ORIGINAL, m_sndFile.GetSample(m_sampleIndex).nLength, false);
	SetItemInt(IDC_SAMPLE_LENGTH_NEW, m_sampleLengthNew);
	SetItemInt(IDC_SPEED, m_speed);
	m_EditTempo->SetTempoValue(m_tempo);
	m_EditRatio->SetDecimalValue(m_ratio);
	m_EditDurationOrig->SetDecimalValue(m_durationOrig);
	CalcSamples();
	CalcMs();
	CalcRows();

	UnlockControls();

	return true;
}

void CPSRatioCalc::OnChangeSampleLength()
{
	if(IsLocked())
		return;
	m_sampleLengthNew = GetItemInt(IDC_SAMPLE_LENGTH_NEW);
	if(m_sampleLengthNew && m_sndFile.GetSample(m_sampleIndex).nLength)
	{
		LockControls();
		m_ratio = static_cast<double>(m_sampleLengthNew) * 100.0 / static_cast<double>(m_sndFile.GetSample(m_sampleIndex).nLength);
		CalcMs();
		CalcRows();
		m_EditRatio->SetDecimalValue(m_ratio);
		UnlockControls();
	}
}

void CPSRatioCalc::OnChangeDuration()
{
	if(IsLocked())
		return;
	if(m_durationOrig > 0.0 && m_EditDurationNew->GetDecimalValue(m_durationNew))
	{
		LockControls();
		m_ratio = m_durationNew * 100.0 / m_durationOrig;
		CalcSamples();
		CalcRows();
		m_EditRatio->SetDecimalValue(m_ratio);
		UnlockControls();
	}
}

void CPSRatioCalc::OnChangeRows()
{
	if(IsLocked())
		return;
	if(m_rowsOrig && m_EditRowsNew->GetDecimalValue(m_rowsNew))
	{
		LockControls();
		m_ratio = m_rowsNew * 100.0 / m_rowsOrig;
		CalcSamples();
		CalcMs();
		m_EditRatio->SetDecimalValue(m_ratio);
		UnlockControls();
	}
}

void CPSRatioCalc::OnChangeSpeed()
{
	if(IsLocked())
		return;
	LockControls();
	m_tempo = std::max(TEMPO(1, 0), m_EditTempo->GetTempoValue());
	m_speed = std::max(uint32(1), GetItemInt(IDC_SPEED));
	CalcRows();
	UnlockControls();
}

void CPSRatioCalc::OnChangeRatio()
{
	if(IsLocked())
		return;
	if(m_EditRatio->GetDecimalValue(m_ratio))
	{
		LockControls();
		CalcSamples();
		CalcMs();
		CalcRows();
		UnlockControls();
	}
}

// CPSRatioCalc Internal Calculations:

void CPSRatioCalc::CalcSamples()
{
	m_sampleLengthNew = mpt::saturate_round<SmpLength>(m_sndFile.GetSample(m_sampleIndex).nLength * m_ratio / 100.0);
	SetItemInt(IDC_SAMPLE_LENGTH_NEW, m_sampleLengthNew);
}

void CPSRatioCalc::CalcMs()
{
	m_durationNew = m_durationOrig * m_ratio / 100.0;
	m_EditDurationNew->SetDecimalValue(m_durationNew);
}

void CPSRatioCalc::CalcRows()
{
	m_rowsOrig = m_durationOrig / m_sndFile.GetRowDuration(m_tempo, m_speed);
	m_rowsNew = m_rowsOrig * m_ratio / 100.0;
	m_EditRowsOrig->SetDecimalValue(m_rowsOrig);
	m_EditRowsNew->SetDecimalValue(m_rowsNew);
}

OPENMPT_NAMESPACE_END
