/*
 * PatternGotoDialog.cpp
 * ---------------------
 * Purpose: Implementation of pattern "go to" dialog.
 * Notes  : mfcless twin of upstream's mptrack/PatternGotoDialog.cpp.
 *          Draft from qt/tools/twin.py, not yet edited by hand.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "PatternGotoDialog.h"
#include "mfcless/Widgets.h"
#include "resource.h"
#include "Sndfile.h"


OPENMPT_NAMESPACE_BEGIN


// CPatternGotoDialog dialog

CPatternGotoDialog::CPatternGotoDialog(QWidget *pParent, ROWINDEX row, CHANNELINDEX chan, PATTERNINDEX pat, ORDERINDEX ord, CSoundFile &sndFile)
	: DialogBase(IDD_EDIT_GOTO, pParent)
	, m_SndFile(sndFile)
	, m_nRow(row)
	, m_nChannel(chan)
	, m_nPattern(pat)
	, m_nOrder(ord)
	, m_nActiveOrder(ord)
{ }


void CPatternGotoDialog::DoDataExchange(DataExchange &dx)
{
	DialogBase::DoDataExchange(dx);
	ExchangeControl(dx, IDC_SPIN1, m_SpinRow);
	ExchangeControl(dx, IDC_SPIN2, m_SpinChannel);
	ExchangeControl(dx, IDC_SPIN3, m_SpinPattern);
	ExchangeControl(dx, IDC_SPIN4, m_SpinOrder);
}


void CPatternGotoDialog::RegisterMessageMap()
{
	DialogBase::RegisterMessageMap();
	AddNotify(IDC_GOTO_PAT, mfcless::Notify::EditChange, &CPatternGotoDialog::OnPatternChanged);
	AddNotify(IDC_GOTO_ORD, mfcless::Notify::EditChange, &CPatternGotoDialog::OnOrderChanged);
	AddNotify(IDC_GOTO_ROW, mfcless::Notify::EditChange, &CPatternGotoDialog::OnRowChanged);
	AddNotify(IDC_EDIT5, mfcless::Notify::EditChange, &CPatternGotoDialog::OnTimeChanged);
	AddNotify(IDC_EDIT6, mfcless::Notify::EditChange, &CPatternGotoDialog::OnTimeChanged);
}


bool CPatternGotoDialog::OnInitDialog()
{
	DialogBase::OnInitDialog();
	QIcon icon = ::LoadIcon(AfxGetResourceHandle(), MAKEINTRESOURCE(IDR_MODULETYPE));
	SetIcon(icon, false);
	SetIcon(icon, true);

	UpdateNumRows();
	m_SpinChannel->SetRange(1, m_SndFile.GetNumChannels());
	m_SpinPattern->SetRange(0, std::max(m_SndFile.Patterns.GetNumPatterns(), PATTERNINDEX(1)) - 1);
	m_SpinOrder->SetRange(0, std::max(m_SndFile.Order().GetLengthTailTrimmed(), ORDERINDEX(1)) - 1);
	SetItemInt(IDC_GOTO_ROW, m_nRow);
	SetItemInt(IDC_GOTO_CHAN, m_nChannel);
	SetItemInt(IDC_GOTO_PAT, m_nPattern);
	SetItemInt(IDC_GOTO_ORD, m_nOrder);
	UpdateTime();
	UnlockControls();
	return true;
}

void CPatternGotoDialog::OnOK()
{
	const auto &Order = m_SndFile.Order();
	m_nRow = mpt::saturate_cast<ROWINDEX>(GetItemInt(IDC_GOTO_ROW));
	m_nChannel = mpt::saturate_cast<CHANNELINDEX>(GetItemInt(IDC_GOTO_CHAN));

	// Valid order item?
	if(m_nOrder >= Order.size())
	{
		mfcless::MessageBeep(nfIconWarning);
		mfcless::GotoDlgCtrl(GetDlgItem(IDC_GOTO_ORD));
		return;
	}
	// Does order match pattern? Does pattern exist?
	if(Order[m_nOrder] != m_nPattern || !Order.IsValidPat(m_nOrder))
	{
		mfcless::MessageBeep(nfIconWarning);
		mfcless::GotoDlgCtrl(GetDlgItem(IDC_GOTO_PAT));
		return;
	}

	LimitMax(m_nRow, m_SndFile.Patterns[m_nPattern].GetNumRows() - ROWINDEX(1));
	Limit(m_nChannel, CHANNELINDEX(1), m_SndFile.GetNumChannels());

	DialogBase::OnOK();
}

void CPatternGotoDialog::OnPatternChanged()
{
	if(ControlsLocked())
		return;  // The change to textbox did not come from user.

	m_nPattern = mpt::saturate_cast<PATTERNINDEX>(GetItemInt(IDC_GOTO_PAT));
	m_nOrder = m_SndFile.Order().FindOrder(m_nPattern, m_nActiveOrder);

	if(m_nOrder == ORDERINDEX_INVALID)
	{
		m_nOrder = 0;
	}

	LockControls();
	SetItemInt(IDC_GOTO_ORD, m_nOrder);
	UpdateNumRows();
	UpdateTime();
	UnlockControls();
}


void CPatternGotoDialog::OnOrderChanged()
{
	if(ControlsLocked())
		return;  // The change to textbox did not come from user.

	m_nOrder = mpt::saturate_cast<ORDERINDEX>(GetItemInt(IDC_GOTO_ORD));
	if(m_SndFile.Order().IsValidPat(m_nOrder))
	{
		m_nPattern = m_SndFile.Order()[m_nOrder];
	}

	LockControls();
	SetItemInt(IDC_GOTO_PAT, m_nPattern);
	UpdateNumRows();
	UpdateTime();
	UnlockControls();
}


void CPatternGotoDialog::OnRowChanged()
{
	if(ControlsLocked())
		return;  // The change to textbox did not come from user.

	m_nRow = mpt::saturate_cast<ROWINDEX>(GetItemInt(IDC_GOTO_ROW));
	UpdateTime();
}


void CPatternGotoDialog::OnTimeChanged()
{
	if(ControlsLocked())
		return;  // The change to textbox did not come from user.

	bool success = true;
	auto minutes = GetItemInt(IDC_EDIT5);
	if(!success)
		return;
	auto seconds = GetItemInt(IDC_EDIT6);
	if(!success)
		return;

	auto result = m_SndFile.GetLength(eNoAdjust, GetLengthTarget(minutes * 60.0 + seconds)).back();
	if(!result.targetReached)
		return;

	m_nOrder = result.restartOrder;
	m_nRow = result.restartRow;
	if(m_SndFile.Order().IsValidPat(m_nOrder))
		m_nPattern = m_SndFile.Order()[m_nOrder];

	LockControls();
	SetItemInt(IDC_GOTO_ORD, m_nOrder);
	SetItemInt(IDC_GOTO_ROW, m_nRow);
	SetItemInt(IDC_GOTO_PAT, m_nPattern);
	UnlockControls();
}


void CPatternGotoDialog::UpdateNumRows()
{
	const ROWINDEX maxRow = (m_SndFile.Patterns.IsValidPat(m_nPattern) ? m_SndFile.Patterns[m_nPattern].GetNumRows() : MAX_PATTERN_ROWS) - 1;
	m_SpinRow->SetRange(0, maxRow);
	if(m_nRow > maxRow)
	{
		m_nRow = maxRow;
		SetItemInt(IDC_GOTO_ROW, m_nRow);
	}
}


void CPatternGotoDialog::UpdateTime()
{
	const double length = m_SndFile.GetPlaybackTimeAt(m_nOrder, m_nRow, false, false);
	if(length < 0.0)
		return;
	const double minutes = std::floor(length / 60.0), seconds = std::fmod(length, 60.0);
	LockControls();
	SetItemInt(IDC_EDIT5, static_cast<int>(minutes));
	SetItemInt(IDC_EDIT6, static_cast<int>(seconds));
	UnlockControls();
}


OPENMPT_NAMESPACE_END
