/*
 * PatternEditorDialogs.cpp
 * ------------------------
 * Purpose: Code for various dialogs that are used in the pattern editor.
 * Notes  : mfcless twin of upstream's mptrack/PatternEditorDialogs.cpp.
 *          Draft from qt/tools/twin.py, not yet edited by hand.
 * Authors: Olivier Lapicque
 *          OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "PatternEditorDialogs.h"
#include "FileDialog.h"
#include "HighDPISupport.h"
#include "InputHandler.h"
#include "Mainfrm.h"
#include "Moddoc.h"
#include "Mptrack.h"
#include "Reporting.h"
#include "resource.h"
#include "TempoSwingDialog.h"
#include "View_pat.h"
#include "WindowMessages.h"
#include "../common/mptStringBuffer.h"
#include "../soundlib/mod_specifications.h"


OPENMPT_NAMESPACE_BEGIN


static constexpr EffectCommand ExtendedCommands[] = {CMD_OFFSET, CMD_PATTERNBREAK, CMD_POSITIONJUMP, CMD_TEMPO, CMD_FINETUNE, CMD_FINETUNE_SMOOTH};

// For a given pattern cell, check if it contains a command supported by the X-Param mechanism.
// If so, calculate the multipler for this cell and the value of all the other cells belonging to this param.
void getXParam(ModCommand::COMMAND command, PATTERNINDEX nPat, ROWINDEX nRow, CHANNELINDEX nChannel, const CSoundFile &sndFile, uint32 &xparam, uint32 &multiplier)
{
	uint32 xp = 0, mult = 1;
	int cmdRow = static_cast<int>(nRow);
	const auto &pattern = sndFile.Patterns[nPat];

	if(command == CMD_XPARAM)
	{
		// current command is a parameter extension command
		cmdRow--;

		// Try to find previous command parameter to be extended
		while(cmdRow >= 0)
		{
			const ModCommand &m = *pattern.GetpModCommand(cmdRow, nChannel);
			if(mpt::contains(ExtendedCommands, m.command))
				break;
			if(m.command != CMD_XPARAM)
			{
				cmdRow = -1;
				break;
			}
			cmdRow--;
		}
	} else if(!mpt::contains(ExtendedCommands, command))
	{
		// If current row do not own any satisfying command parameter to extend, set return state
		cmdRow = -1;
	}

	if(cmdRow >= 0)
	{
		// An 'extendable' command parameter has been found
		const ModCommand &m = *pattern.GetpModCommand(cmdRow, nChannel);

		// Find extension resolution (8 to 24 bits)
		uint32 n = 1;
		while(n < 4 && cmdRow + n < pattern.GetNumRows())
		{
			if(pattern.GetpModCommand(cmdRow + n, nChannel)->command != CMD_XPARAM)
				break;
			n++;
		}

		// Parameter extension found (above 8 bits non-standard parameters)
		if(n > 1)
		{
			// Limit offset command to 24 bits, other commands to 16 bits
			n = m.command == CMD_OFFSET ? n : (n > 2 ? 2 : n);

			// Compute extended value WITHOUT current row parameter value : this parameter
			// is being currently edited (this is why this function is being called) so we
			// only need to compute a multiplier so that we can add its contribution while
			// its value is changed by user
			for(uint32 j = 0; j < n; j++)
			{
				const ModCommand &mx = *pattern.GetpModCommand(cmdRow + j, nChannel);

				uint32 k = 8 * (n - j - 1);
				if(cmdRow + j == nRow)
					mult = 1 << k;
				else
					xp += (mx.param << k);
			}
		} else if(m.command == CMD_OFFSET || m.command == CMD_FINETUNE || m.command == CMD_FINETUNE_SMOOTH)
		{
			// Even when no parameter extension is found, extend the offset / finetune parameters to full resolution.
			mult <<= 8;
		}

		const auto modDoc = sndFile.GetpModDoc();
		if(m.command == CMD_OFFSET && m.volcmd == VOLCMD_OFFSET && modDoc != nullptr)
		{
			SAMPLEINDEX smp = modDoc->GetSampleIndex(m);
			if(m.vol == 0 && smp != 0)
			{
				xp = Util::muldivr_unsigned(sndFile.GetSample(smp).nLength, pattern.GetpModCommand(nRow, nChannel)->param  * mult + xp, 256u << (8u * (std::max(uint32(2), n) - 1u)));
				mult = 0;
			} else if(m.vol > 0 && smp != 0)
			{
				xp += sndFile.GetSample(smp).cues[m.vol - 1];
			}
		}
	}

	// Return x-parameter
	multiplier = mult;
	xparam = xp;
}


/////////////////////////////////////////////////////////////////////////////////////////////
// CPatternPropertiesDlg

void CPatternPropertiesDlg::RegisterMessageMap()
{
	DialogBase::RegisterMessageMap();
	AddScroll(Qt::Vertical, &CPatternPropertiesDlg::OnVScroll);
	AddCommand(IDC_BUTTON_HALF, &CPatternPropertiesDlg::OnHalfRowNumber);
	AddCommand(IDC_BUTTON_DOUBLE, &CPatternPropertiesDlg::OnDoubleRowNumber);
	AddCommand(IDC_CHECK1, &CPatternPropertiesDlg::OnOverrideSignature);
	AddCommand(IDC_BUTTON1, &CPatternPropertiesDlg::OnTempoSwing);
	AddNotify(IDC_EDIT1, mfcless::Notify::EditChange, &CPatternPropertiesDlg::OnPatternChanged);
}


void CPatternPropertiesDlg::DoDataExchange(DataExchange &dx)
{
	DialogBase::DoDataExchange(dx);
	//{{AFX_DATA_MAP(CPatternPropertiesDlg)
	ExchangeControl(dx, IDC_COMBO1, m_numRows);
	ExchangeControl(dx, IDC_SPIN1, m_spinPattern);
	ExchangeControl(dx, IDC_SPIN2, m_spinRPB);
	ExchangeControl(dx, IDC_SPIN3, m_spinRPM);
	//}}AFX_DATA_MAP
}

CPatternPropertiesDlg::CPatternPropertiesDlg(CModDoc &modParent, PATTERNINDEX nPat, QWidget *parent)
	: DialogBase{IDD_PATTERN_PROPERTIES, parent}
	, m_modDoc{modParent}
	, m_nPattern{nPat}
{
}


bool CPatternPropertiesDlg::OnInitDialog()
{
	DialogBase::OnInitDialog();

	COMBOBOXINFO cbi{};
	cbi.cbSize = sizeof(cbi);
	GetComboBoxInfo(m_numRows, &cbi);
	mfcless::Window<QWidget>::FromHandle(cbi.hwndItem)->ModifyStyle(0, ES_NUMBER);

	const CSoundFile &sndFile = m_modDoc.GetSoundFile();
	const CModSpecifications &specs = sndFile.GetModSpecifications();
	m_numRows->setUpdatesEnabled(false);
	for(ROWINDEX irow = specs.patternRowsMin; irow <= specs.patternRowsMax; irow++)
	{
		mfcless::AddItem(m_numRows, mpt::ufmt::dec(irow));
	}
	m_numRows->setUpdatesEnabled(true);
	GotoDlgCtrl(m_numRows);
	m_spinRPB->SetRange(1, MAX_ROWS_PER_BEAT);
	m_spinRPM->SetRange(1, MAX_ROWS_PER_MEASURE);
	m_spinPattern->SetRange(-1, 1);
	SetItemInt(IDC_EDIT1, m_nPattern);
	CheckRadio(IDC_RADIO1, IDC_RADIO2, IDC_RADIO2);
	static_cast<CEdit *>(GetDlgItem(IDC_EDIT2))->SetLimitText(MAX_PATTERNNAME - 1);

	m_locked = false;
	SetCurrentPattern(m_nPattern);

	return false;
}


void CPatternPropertiesDlg::SetCurrentPattern(PATTERNINDEX pat)
{
	if(m_locked)
		return;
	const CSoundFile &sndFile = m_modDoc.GetSoundFile();
	const bool validPat = sndFile.Patterns.IsValidPat(pat);

	for(auto id : { IDC_EDIT2, IDC_COMBO1, IDC_BUTTON_HALF, IDC_BUTTON_DOUBLE, IDC_RADIO1, IDC_RADIO2, IDC_CHECK2, IDC_BUTTON1, IDC_ROWSPERBEAT, IDC_ROWSPERMEASURE, IDC_SPIN2, IDC_SPIN3 })
	{
		EnableItem(id, validPat ? true : false);
	}
	EnableItem(IDC_CHECK1, (sndFile.GetModSpecifications().hasPatternSignatures && validPat) ? true : false);
	if(validPat)
	{
		m_nPattern = pat;
		PatternProperties &prop = GetPatternProperties();
		m_numRows->setCurrentIndex(prop.numRows - sndFile.GetModSpecifications().patternRowsMin);
		CheckRadio(IDC_RADIO1, IDC_RADIO2, prop.resizeAtEnd ? IDC_RADIO2 : IDC_RADIO1);

		// Window title
		const mpt::ustring patternName = mpt::ToUnicode(sndFile.GetCharsetInternal(), prop.name);
		SetItemText(IDC_EDIT2, patternName);
		mpt::ustring s = MPT_UFORMAT("Pattern Properties for Pattern #{}")(m_nPattern);
		if(!patternName.IsEmpty())
		{
			s += U_(" (");
			s += patternName;
			s += U_(")");
		}
		SetWindowText(s);

		// Pattern time signature
		const bool overrideSignature = prop.rowsPerBeat != 0 || prop.rowsPerMeasure != 0;
		ROWINDEX rpb = prop.rowsPerBeat, rpm = prop.rowsPerMeasure;
		if(rpb == 0 || !overrideSignature)
			rpb = sndFile.m_nDefaultRowsPerBeat;
		if(rpm == 0 || !overrideSignature)
			rpm = sndFile.m_nDefaultRowsPerMeasure;

		SetChecked(IDC_CHECK1, overrideSignature ? 1 : 0);
		SetChecked(IDC_CHECK2, prop.repeatContents ? 1 : 0);
		SetItemInt(IDC_ROWSPERBEAT, rpb);
		SetItemInt(IDC_ROWSPERMEASURE, rpm);
		OnOverrideSignature();
	} else
	{
		MessageBeep(MB_ICONWARNING);
	}
}


void CPatternPropertiesDlg::StorePatternProperties()
{
	if(m_locked)
		return;
	auto &prop = GetPatternProperties();
	mpt::ustring str;
	str = GetItemText(IDC_EDIT2);
	prop.name = mpt::ToCharset(m_modDoc.GetSoundFile().GetCharsetInternal(), str);
	prop.numRows = GetItemInt(IDC_COMBO1);
	prop.resizeAtEnd = IsChecked(IDC_RADIO2) != 0;
	prop.repeatContents = IsChecked(IDC_CHECK2) != 0;
	if(IsChecked(IDC_CHECK1) != 0)
	{
		prop.rowsPerBeat = GetItemInt(IDC_ROWSPERBEAT);
		prop.rowsPerMeasure = GetItemInt(IDC_ROWSPERMEASURE);
	} else
	{
		prop.rowsPerBeat = prop.rowsPerMeasure = 0;
	}
}


bool CPatternPropertiesDlg::ValidatePatternProperties()
{
	StorePatternProperties();

	CSoundFile &sndFile = m_modDoc.GetSoundFile();
	auto &prop = GetPatternProperties();
	// Check for valid time signatures
	if((prop.rowsPerBeat != 0 || prop.rowsPerMeasure != 0) && !CPattern::IsValidSignature(prop.rowsPerBeat, prop.rowsPerMeasure))
	{
		Reporting::Error("Invalid time signature!", "Pattern Properties");
		SetCurrentPattern(m_nPattern);
		SetItemInt(IDC_EDIT1, m_nPattern);
		GotoDlgCtrl(GetDlgItem(IDC_ROWSPERBEAT));
		return false;
	}

	// Check if any pattern data would be removed.
	CPattern &pattern = sndFile.Patterns[m_nPattern];
	const ROWINDEX newSize = prop.numRows;
	const ROWINDEX oldSize = pattern.GetNumRows();
	if(pattern.IsValid() && newSize < oldSize
		&& (prop.resizeWarningShown != newSize || prop.resizeWarningAtEnd != prop.resizeAtEnd))
	{
		ROWINDEX firstRow = prop.resizeAtEnd ? newSize : 0;
		ROWINDEX lastRow = prop.resizeAtEnd ? oldSize : oldSize - newSize;
		for(ROWINDEX row = firstRow; row < lastRow; row++)
		{
			if(!pattern.IsEmptyRow(row))
			{
				bool resize = (Reporting::Confirm(MPT_AFORMAT("Data at the {} of pattern {} will be lost.\nDo you want to continue?")(prop.resizeAtEnd ? "end" : "start", m_nPattern), "Shrink Pattern") == cnfYes);
				if(!resize)
				{
					SetCurrentPattern(m_nPattern);
					SetItemInt(IDC_EDIT1, m_nPattern);
					GotoDlgCtrl(m_numRows);
					return false;
				}
				break;
			}
		}
		prop.resizeWarningShown = prop.numRows;
		prop.resizeWarningAtEnd = prop.resizeAtEnd;
	}
	return true;
}


void CPatternPropertiesDlg::OnPatternChanged()
{
	auto pat = GetItemInt(IDC_EDIT1);
	if(m_locked || pat == m_nPattern)
		return;
	if(!ValidatePatternProperties())
		return;
	SetCurrentPattern(mpt::saturate_cast<PATTERNINDEX>(pat));
}


void CPatternPropertiesDlg::OnVScroll(mfcless::ScrollCode code, int pos, QWidget *pScrollBar)
{
	DialogBase::OnVScroll(nSBCode, nPos, pScrollBar);

	const int direction = m_spinPattern->GetPos();
	const auto &patterns = m_modDoc.GetSoundFile().Patterns;
	if(!direction || !patterns.IsValidPat(m_nPattern))
		return;
	int pattern = std::clamp(static_cast<int>(GetItemInt(IDC_EDIT1)), 0, patterns.Size() - 1);
	const int startPattern = pattern;
	do
	{
		pattern += direction;
		if(pattern < 0)
			pattern = patterns.Size() - 1;
		else if(pattern >= patterns.Size())
			pattern = 0;
		else if(pattern == startPattern)
			break;  // Couldn't find any other pattern, abort
	} while(!patterns.IsValidPat(static_cast<PATTERNINDEX>(pattern)));
	m_spinPattern->SetPos(0);
	SetItemInt(IDC_EDIT1, static_cast<PATTERNINDEX>(pattern));
}


CPatternPropertiesDlg::PatternProperties& CPatternPropertiesDlg::GetPatternProperties(PATTERNINDEX pat)
{
	if(auto p = m_properties.find(pat); p != m_properties.end())
		return p->second;

	const CSoundFile &sndFile = m_modDoc.GetSoundFile();
	PatternProperties prop;
	if(sndFile.Patterns.IsValidPat(pat))
	{
		const CPattern &pattern = sndFile.Patterns[pat];
		prop.name = pattern.GetName();
		if(pattern.HasTempoSwing())
			prop.tempoSwing = pattern.GetTempoSwing();
		else
			prop.tempoSwing = sndFile.m_tempoSwing;
		prop.numRows = pattern.GetNumRows();
		prop.rowsPerBeat = pattern.GetRowsPerBeat();
		prop.rowsPerMeasure = pattern.GetRowsPerMeasure();
	}
	// Take whatever was selected for the previous pattern to be the default for this newly-edited pattern as well
	prop.resizeAtEnd = IsChecked(IDC_RADIO2) != 0;
	prop.repeatContents = IsChecked(IDC_CHECK2) != 0;
	return m_properties.insert(std::make_pair(pat, std::move(prop))).first->second;
}


void CPatternPropertiesDlg::OnHalfRowNumber()
{
	const CSoundFile &sndFile = m_modDoc.GetSoundFile();

	uint32 nRows = GetItemInt(IDC_COMBO1);
	nRows /= 2;
	if(nRows < sndFile.GetModSpecifications().patternRowsMin)
		nRows = sndFile.GetModSpecifications().patternRowsMin;
	SetItemInt(IDC_COMBO1, nRows);
}


void CPatternPropertiesDlg::OnDoubleRowNumber()
{
	const CSoundFile &sndFile = m_modDoc.GetSoundFile();

	uint32 nRows = GetItemInt(IDC_COMBO1);
	nRows *= 2;
	if(nRows > sndFile.GetModSpecifications().patternRowsMax)
		nRows = sndFile.GetModSpecifications().patternRowsMax;
	SetItemInt(IDC_COMBO1, nRows);
}


void CPatternPropertiesDlg::OnOverrideSignature()
{
	const bool enableTimeSignature = IsChecked(IDC_CHECK1);
	EnableItem(IDC_ROWSPERBEAT, enableTimeSignature);
	EnableItem(IDC_ROWSPERMEASURE, enableTimeSignature);
	EnableItem(IDC_BUTTON1, enableTimeSignature && m_modDoc.GetSoundFile().m_nTempoMode == TempoMode::Modern);
}


void CPatternPropertiesDlg::OnTempoSwing()
{
	CPattern &pat = m_modDoc.GetSoundFile().Patterns[m_nPattern];
	const ROWINDEX oldRPB = pat.GetRowsPerBeat();
	const ROWINDEX oldRPM = pat.GetRowsPerMeasure();

	// Temporarily apply new tempo signature for preview
	const ROWINDEX newRPB = std::clamp(static_cast<ROWINDEX>(GetItemInt(IDC_ROWSPERBEAT)), ROWINDEX(1), MAX_ROWS_PER_BEAT);
	const ROWINDEX newRPM = std::clamp(static_cast<ROWINDEX>(GetItemInt(IDC_ROWSPERMEASURE)), newRPB, MAX_ROWS_PER_BEAT);
	pat.SetSignature(newRPB, newRPM);

	TempoSwing &tempoSwing = GetPatternProperties().tempoSwing;
	tempoSwing.resize(newRPB, TempoSwing::Unity);
	CTempoSwingDlg dlg(this, tempoSwing, m_modDoc.GetSoundFile(), m_nPattern);
	if(dlg.DoModal() == IDOK)
	{
		tempoSwing = dlg.m_tempoSwing;
	}
	pat.SetSignature(oldRPB, oldRPM);
}


void CPatternPropertiesDlg::OnOK()
{
	if(!ValidatePatternProperties())
		return;

	CSoundFile &sndFile = m_modDoc.GetSoundFile();
	bool modified = false;
	for(const auto &[pat, prop] : m_properties)
	{
		PatternHint updateHint = PatternHint{pat};

		CPattern &pattern = sndFile.Patterns[pat];
		// Update pattern signature if necessary
		if(sndFile.GetModSpecifications().hasPatternSignatures)
		{
			if(prop.rowsPerBeat != 0 || prop.rowsPerMeasure != 0)
			{
				// Enable signature
				const ROWINDEX newRPB = prop.rowsPerBeat;
				const ROWINDEX newRPM = prop.rowsPerMeasure;
				TempoSwing tempoSwing = prop.tempoSwing;
				if(newRPB != pattern.GetRowsPerBeat() || newRPM != pattern.GetRowsPerMeasure() || tempoSwing != pattern.GetTempoSwing())
				{
					pattern.SetSignature(newRPB, newRPM);
					tempoSwing.resize(newRPB, TempoSwing::Unity);
					pattern.SetTempoSwing(tempoSwing);
					updateHint.Data();
				}
			} else
			{
				// Disable signature
				if(pattern.GetOverrideSignature() || pattern.HasTempoSwing())
				{
					pattern.RemoveSignature();
					pattern.RemoveTempoSwing();
					updateHint.Data();
				}
			}
		}

		const ROWINDEX newSize = prop.numRows;

		// Check if any pattern data would be removed.
		const ROWINDEX oldSize = pattern.GetNumRows();
		bool resize = newSize != oldSize;
		const bool resizeAtEnd = prop.resizeAtEnd;
		if(resize)
		{
			const bool copyContents = (newSize > oldSize) && prop.repeatContents;
			m_modDoc.BeginWaitCursor();
			m_modDoc.GetPatternUndo().PrepareUndo(m_nPattern, 0, 0, sndFile.Patterns[m_nPattern].GetNumChannels(), oldSize, "Resize");
			if(sndFile.Patterns[m_nPattern].Resize(newSize, true, resizeAtEnd))
			{
				if(copyContents)
				{
					const ROWINDEX copyRows = newSize - oldSize;
					const ROWINDEX sourceBaseRow = resizeAtEnd ? 0 : copyRows;
					const ROWINDEX baseOffset = resizeAtEnd ? 0 : (oldSize - copyRows % oldSize);
					ROWINDEX destRow = resizeAtEnd ? oldSize : 0;
					for(ROWINDEX row = 0; row < copyRows; row++, destRow++)
					{
						ROWINDEX sourceRow = sourceBaseRow + (baseOffset + row) % oldSize;
						const auto sourceRowData = pattern.GetRow(sourceRow);
						auto destRowData = pattern.GetRow(destRow);
						std::copy(sourceRowData.begin(), sourceRowData.end(), destRowData.begin());
					}
				}
				updateHint.Data();
			}
			m_modDoc.EndWaitCursor();
		}
		if(pattern.GetName() != prop.name)
		{
			pattern.SetName(prop.name);
			updateHint.Names();
		}

		if(updateHint.GetType() != HINT_NONE)
		{
			m_modDoc.UpdateAllViews(nullptr, updateHint, this);
			modified = true;
		}
	}

	if(modified)
		m_modDoc.SetModified();

	DialogBase::OnOK();
}


////////////////////////////////////////////////////////////////////////////////////////////
// CEditCommand

void CEditCommand::RegisterMessageMap()
{
	DialogBase::RegisterMessageMap();
	// ON_WM_ACTIVATE(): by hand (Qt event override)
	// ON_WM_CLOSE() (mfcless::Window)
	AddNotify(IDC_COMBO1, mfcless::Notify::SelChange, &CEditCommand::OnNoteChanged);
	AddNotify(IDC_COMBO2, mfcless::Notify::SelChange, &CEditCommand::OnNoteChanged);
	AddNotify(IDC_COMBO3, mfcless::Notify::SelChange, &CEditCommand::OnVolCmdChanged);
	AddNotify(IDC_COMBO4, mfcless::Notify::SelChange, &CEditCommand::OnCommandChanged);
	AddNotify(IDC_COMBO5, mfcless::Notify::SelChange, &CEditCommand::OnPlugParamChanged);
	AddScroll(Qt::Horizontal, &CEditCommand::OnHScroll);
}


void CEditCommand::DoDataExchange(DataExchange &dx)
{
	DialogBase::DoDataExchange(dx);
	//{{AFX_DATA_MAP(CEditCommand)
	ExchangeControl(dx, IDC_COMBO1, cbnNote);
	ExchangeControl(dx, IDC_COMBO2, cbnInstr);
	ExchangeControl(dx, IDC_COMBO3, cbnVolCmd);
	ExchangeControl(dx, IDC_COMBO4, cbnCommand);
	ExchangeControl(dx, IDC_COMBO5, cbnPlugParam);
	ExchangeControl(dx, IDC_SLIDER1, sldVolParam);
	ExchangeControl(dx, IDC_SLIDER2, sldParam);
	//}}AFX_DATA_MAP
}


CEditCommand::CEditCommand(CSoundFile &sndFile)
    : sndFile(sndFile), effectInfo(sndFile)
{
	DialogBase::Create(IDD_PATTERN_EDITCOMMAND);
}


bool CEditCommand::PreTranslateMessage(mfcless::Message *pMsg)
{
	if((pMsg) && (pMsg->message == mfcless::msg::KeyDown))
	{
		if((pMsg->wParam == mfcless::vk::ESCAPE) || (pMsg->wParam == mfcless::vk::RETURN) || (pMsg->wParam == mfcless::vk::APPS))
		{
			OnClose();
			return true;
		}
	}
	return DialogBase::PreTranslateMessage(pMsg);
}


bool CEditCommand::ShowEditWindow(PATTERNINDEX pat, const PatternCursor &cursor, QWidget *parent)
{
	editPattern = pat;
	const ROWINDEX row = editRow = cursor.GetRow();
	const CHANNELINDEX chn = editChannel = cursor.GetChannel();

	if(!sndFile.Patterns.IsValidPat(pat)
	   || !sndFile.Patterns[pat].IsValidRow(row)
	   || chn >= sndFile.GetNumChannels())
	{
		ShowWindow(false);
		return false;
	}

	m = sndFile.Patterns[pat].GetpModCommand(row, chn);
	modified = false;

	InitAll();

	switch(cursor.GetColumnType())
	{
	case PatternCursor::noteColumn:
		cbnNote.SetFocus();
		break;
	case PatternCursor::instrColumn:
		cbnInstr.SetFocus();
		break;
	case PatternCursor::volumeColumn:
		if(m->IsPcNote())
			cbnPlugParam.SetFocus();
		else
			cbnVolCmd.SetFocus();
		break;
	case PatternCursor::effectColumn:
		if(m->IsPcNote())
			sldParam.SetFocus();
		else
			cbnCommand.SetFocus();
		break;
	case PatternCursor::paramColumn:
		sldParam.SetFocus();
		break;
	case PatternCursor::numColumns:
		MPT_ASSERT_NOTREACHED();
		break;
	}

	// Update Window Title
	SetWindowText(MPT_UFORMAT("Note Properties - Row {}, Channel {}")(row, chn + 1));

	mfcless::Rect rectParent, rectWnd;
	parent->GetWindowRect(&rectParent);
	GetClientRect(&rectWnd);
	SetWindowPos(CMainFrame::GetMainFrame(),
		rectParent.left + (rectParent.Width() - rectWnd.right) / 2,
		rectParent.top + (rectParent.Height() - rectWnd.bottom) / 2,
		-1, -1, SWP_NOSIZE | SWP_NOACTIVATE);
	ShowWindow(SW_RESTORE);
	return true;
}


void CEditCommand::InitNote()
{
	// Note
	cbnNote.SetRedraw(false);
	if(oldSpecs != &sndFile.GetModSpecifications())
	{
		cbnNote.ResetContent();
		cbnNote.SetItemData(cbnNote.AddString(U_("No Note")), 0);
		AppendNotesToControlEx(cbnNote, sndFile, m->instr);
		oldSpecs = &sndFile.GetModSpecifications();
	}

	if(m->IsNote())
	{
		// Normal note / no note
		const ModCommand::NOTE noteStart = sndFile.GetModSpecifications().noteMin;
		cbnNote.SetCurSel(m->note - (noteStart - 1));
	} else if(m->note == NOTE_NONE)
	{
		cbnNote.SetCurSel(0);
	} else
	{
		// Special notes
		for(int i = cbnNote.GetCount() - 1; i >= 0; --i)
		{
			if(cbnNote.GetItemData(i) == m->note)
			{
				cbnNote.SetCurSel(i);
				break;
			}
		}
	}
	cbnNote.SetRedraw(true);

	// Instrument
	cbnInstr.SetRedraw(false);
	cbnInstr.ResetContent();

	if(m->IsPcNote())
	{
		// control plugin param note
		cbnInstr.Update(PluginComboBox::Config{PluginComboBox::ShowNoPlugin | PluginComboBox::ShowEmptySlots}.CurrentSelection(m->instr ? (m->instr - 1) : PLUGINDEX_INVALID), sndFile);
	} else
	{
		// instrument / sample
		cbnInstr.SetItemData(cbnInstr.AddString(U_("No Instrument")), 0);
		const uint32 nmax = sndFile.GetNumInstruments() ? sndFile.GetNumInstruments() : sndFile.GetNumSamples();
		for(uint32 i = 1; i <= nmax; i++)
		{
			mpt::ustring s = mpt::ufmt::val(i) + U_(": ");
			// instrument / sample
			if(sndFile.GetNumInstruments())
			{
				if(sndFile.Instruments[i])
					s += mpt::ToUnicode(sndFile.GetCharsetInternal(), sndFile.Instruments[i]->name);
			} else
				s += mpt::ToUnicode(sndFile.GetCharsetInternal(), sndFile.m_szNames[i]);
			cbnInstr.SetItemData(cbnInstr.AddString(s), i);
		}
		cbnInstr.SetRawSelection(m->instr);
	}
	cbnInstr.SetRedraw(true);
}


void CEditCommand::InitVolume()
{
	cbnVolCmd.SetRedraw(false);
	cbnVolCmd.ResetContent();
	if(sndFile.GetType() == MOD_TYPE_MOD || m->IsPcNote())
	{
		cbnVolCmd.EnableWindow(false);
		sldVolParam.EnableWindow(false);
	} else
	{
		// Normal volume column effect
		cbnVolCmd.EnableWindow(true);
		sldVolParam.EnableWindow(true);
		uint32 count = effectInfo.GetNumVolCmds();
		cbnVolCmd.SetItemData(cbnVolCmd.AddString(U_(" None")), (uintptr_t)-1);
		cbnVolCmd.SetCurSel(0);
		uint32 fxndx = effectInfo.GetIndexFromVolCmd(m->volcmd);
		for(uint32 i = 0; i < count; i++)
		{
			mpt::ustring s;
			if(effectInfo.GetVolCmdInfo(i, &s))
			{
				int k = cbnVolCmd.AddString(s);
				cbnVolCmd.SetItemData(k, i);
				if(i == fxndx)
					cbnVolCmd.SetCurSel(k);
			}
		}
		UpdateVolCmdRange();
	}
	cbnVolCmd.SetRedraw(true);
}


void CEditCommand::InitEffect()
{
	if(m->IsPcNote())
	{
		cbnCommand.ShowWindow(false);
		return;
	}
	cbnCommand.ShowWindow(true);
	xParam = 0;
	xMultiplier = 1;
	getXParam(m->command, editPattern, editRow, editChannel, sndFile, xParam, xMultiplier);

	cbnCommand.SetRedraw(false);
	cbnCommand.ResetContent();
	uint32 numfx = effectInfo.GetNumEffects();
	uint32 fxndx = effectInfo.GetIndexFromEffect(m->command, m->param);
	cbnCommand.SetItemData(cbnCommand.AddString(U_(" None")), (uintptr_t)-1);
	if(m->command == CMD_NONE)
		cbnCommand.SetCurSel(0);

	mpt::ustring s;
	for(uint32 i = 0; i < numfx; i++)
	{
		if(effectInfo.GetEffectInfo(i, &s, true))
		{
			int k = cbnCommand.AddString(s);
			cbnCommand.SetItemData(k, i);
			if(i == fxndx)
				cbnCommand.SetCurSel(k);
		}
	}
	UpdateEffectRange(false);
	cbnCommand.SetRedraw(true);
	cbnCommand.Invalidate();
}


void CEditCommand::InitPlugParam()
{
	if(!m->IsPcNote())
	{
		cbnPlugParam.ShowWindow(false);
		return;
	}
	cbnPlugParam.ShowWindow(true);

	cbnPlugParam.SetRedraw(false);
	cbnPlugParam.ResetContent();

	if(m->instr > 0 && m->instr <= MAX_MIXPLUGINS)
	{
		AddPluginParameternamesToCombobox(cbnPlugParam, sndFile.m_MixPlugins[m->instr - 1]);
		cbnPlugParam.SetCurSel(m->GetValueVolCol());
	}
	UpdateEffectRange(false);

	cbnPlugParam.SetRedraw(true);
	cbnPlugParam.Invalidate();
}


void CEditCommand::UpdateVolCmdRange()
{
	ModCommand::VOL rangeMin = 0, rangeMax = 0;
	int32 fxndx = effectInfo.GetIndexFromVolCmd(m->volcmd);
	bool ok = effectInfo.GetVolCmdInfo(fxndx, NULL, &rangeMin, &rangeMax);
	if(ok && rangeMax > rangeMin)
	{
		sldVolParam.EnableWindow(true);
		sldVolParam.SetRange(rangeMin, rangeMax);
		Limit(m->vol, rangeMin, rangeMax);
		sldVolParam.SetPos(effectInfo.MapVolumeToPos(m->volcmd, m->vol));
	} else
	{
		// Why does this not update the display at all?
		sldVolParam.SetRange(0, 0);
		sldVolParam.SetPos(0);
		sldVolParam.EnableWindow(false);
	}
	UpdateVolCmdValue();
}


void CEditCommand::UpdateEffectRange(bool set)
{
	uint32 pos;
	bool enable = true;

	if(m->IsPcNote())
	{
		// plugin param control note
		sldParam.SetRange(0, ModCommand::maxColumnValue);
		pos = m->GetValueEffectCol();
	} else
	{
		// process as effect
		ModCommand::PARAM rangeMin = 0, rangeMax = 0;
		int32 fxndx = effectInfo.GetIndexFromEffect(m->command, m->param);
		enable = ((fxndx >= 0) && (effectInfo.GetEffectInfo(fxndx, NULL, false, &rangeMin, &rangeMax)));

		pos = effectInfo.MapValueToPos(fxndx, m->param);
		if(pos > rangeMax)
			pos = rangeMin | (pos & 0x0F);
		Limit(pos, rangeMin, rangeMax);

		sldParam.SetRange(rangeMin, rangeMax);
	}

	if(enable)
	{
		sldParam.EnableWindow(true);
		sldParam.SetPageSize(1);
		sldParam.SetPos(pos);
	} else
	{
		// Why does this not update the display at all?
		sldParam.SetRange(0, 0);
		sldParam.SetPos(0);
		sldParam.EnableWindow(false);
	}
	UpdateEffectValue(set);
}


void CEditCommand::OnNoteChanged()
{
	const bool wasParamControl = m->IsPcNote();
	ModCommand::NOTE newNote = m->note;
	ModCommand::INSTR newInstr = m->instr;

	int n = cbnNote.GetCurSel();
	if(n >= 0)
		newNote = static_cast<ModCommand::NOTE>(cbnNote.GetItemData(n));

	if(wasParamControl)
	{
		if(auto sel = cbnInstr.GetSelection(); sel)
			newInstr = *sel + 1;
		else
			newInstr = 0;
	} else
	{
		if(n = cbnInstr.GetRawSelection(); n >= 0)
			newInstr = static_cast<ModCommand::INSTR>(cbnInstr.GetItemData(n));
	}

	if(m->note != newNote || m->instr != newInstr)
	{
		PrepareUndo("Note Entry");
		CModDoc *modDoc = sndFile.GetpModDoc();
		m->note = newNote;
		m->instr = newInstr;

		modDoc->UpdateAllViews(nullptr, RowHint(editRow), nullptr);

		if(wasParamControl != m->IsPcNote())
		{
			InitAll();
		} else if(!m->IsPcNote()
		          && m->instr <= sndFile.GetNumInstruments()
		          && newInstr <= sndFile.GetNumInstruments()
		          && sndFile.Instruments[m->instr] != nullptr
		          && sndFile.Instruments[newInstr] != nullptr
		          && sndFile.Instruments[newInstr]->pTuning != sndFile.Instruments[m->instr]->pTuning)
		{
			//Checking whether note names should be recreated.
			InitNote();
		} else if(m->IsPcNote())
		{
			// Update parameter list
			InitPlugParam();
		}
	}
}


void CEditCommand::OnVolCmdChanged()
{
	ModCommand::VOLCMD newVolCmd = m->volcmd;
	ModCommand::VOL newVol = m->vol;

	int n = cbnVolCmd.GetCurSel();
	if(n >= 0)
	{
		newVolCmd = effectInfo.GetVolCmdFromIndex(static_cast<uint32>(cbnVolCmd.GetItemData(n)));
	}

	newVol = effectInfo.MapPosToVolume(newVolCmd, sldVolParam.GetPos());

	const bool volCmdChanged = m->volcmd != newVolCmd;
	if(volCmdChanged || m->vol != newVol)
	{
		PrepareUndo("Volume Entry");
		CModDoc *modDoc = sndFile.GetpModDoc();
		m->volcmd = newVolCmd;
		m->vol = newVol;

		modDoc->UpdateAllViews(nullptr, RowHint(editRow), nullptr);

		if(volCmdChanged)
			UpdateVolCmdRange();
		else
			UpdateVolCmdValue();
	}
}


void CEditCommand::OnCommandChanged()
{
	ModCommand::COMMAND newCommand = m->command;
	ModCommand::PARAM newParam = m->param;

	int n = cbnCommand.GetCurSel();
	if(n >= 0)
	{
		int ndx = static_cast<int>(cbnCommand.GetItemData(n));
		newCommand = static_cast<ModCommand::COMMAND>((ndx >= 0) ? effectInfo.GetEffectFromIndex(ndx, newParam) : CMD_NONE);
	}

	if(m->command != newCommand || m->param != newParam)
	{
		PrepareUndo("Effect Entry");

		m->command = newCommand;
		if(newCommand != CMD_NONE)
		{
			m->param = newParam;
		}

		xParam = 0;
		xMultiplier = 1;
		if(newCommand == CMD_XPARAM || mpt::contains(ExtendedCommands, newCommand))
		{
			getXParam(newCommand, editPattern, editRow, editChannel, sndFile, xParam, xMultiplier);
		}

		UpdateEffectRange(true);

		sndFile.GetpModDoc()->UpdateAllViews(nullptr, RowHint(editRow), nullptr);
	}
}


void CEditCommand::OnPlugParamChanged()
{
	uint16 newPlugParam = m->GetValueVolCol();

	int n = cbnPlugParam.GetCurSel();
	if(n >= 0)
	{
		newPlugParam = static_cast<uint16>(cbnPlugParam.GetItemData(n));
	}

	if(m->GetValueVolCol() != newPlugParam)
	{
		PrepareUndo("Effect Entry");
		m->SetValueVolCol(newPlugParam);
		sndFile.GetpModDoc()->UpdateAllViews(nullptr, RowHint(editRow), nullptr);
	}
}


void CEditCommand::UpdateVolCmdValue()
{
	mpt::ustring s;
	if(m->IsPcNote())
	{
		// plugin param control note
		uint16 plugParam = static_cast<uint16>(sldVolParam.GetPos());
		s.Format(U_("Value: %u"), plugParam);
	} else
	{
		// process as effect
		effectInfo.GetVolCmdParamInfo(*m, &s, TrackerSettings::Instance().patternVolColHex);
	}
	SetItemText(IDC_TEXT2, s);
}


void CEditCommand::UpdateEffectValue(bool set)
{
	mpt::ustring s;

	uint16 newPlugParam = 0;
	ModCommand::PARAM newParam = 0;

	if(m->IsPcNote())
	{
		// plugin param control note
		newPlugParam = static_cast<uint16>(sldParam.GetPos());
		s.Format(U_("Value: %u"), newPlugParam);
	} else
	{
		// process as effect
		int32 fxndx = effectInfo.GetIndexFromEffect(m->command, m->param);
		if(fxndx >= 0)
		{
			newParam = static_cast<ModCommand::PARAM>(effectInfo.MapPosToValue(fxndx, sldParam.GetPos()));
			effectInfo.GetEffectNameEx(s, *m, newParam * xMultiplier + xParam, editChannel);
		}
	}
	SetItemText(IDC_TEXT1, s);

	if(set)
	{
		if((!m->IsPcNote() && m->param != newParam)
		   || (m->IsPcNote() && m->GetValueVolCol() != newPlugParam))
		{
			PrepareUndo("Effect Entry");
			CModDoc *modDoc = sndFile.GetpModDoc();
			if(m->IsPcNote())
			{
				m->SetValueEffectCol(newPlugParam);
			} else
			{
				m->param = newParam;
			}

			modDoc->UpdateAllViews(nullptr, RowHint(editRow), nullptr);
		}
	}
}


void CEditCommand::PrepareUndo(const char *description)
{
	CModDoc *modDoc = sndFile.GetpModDoc();
	if(!modified)
	{
		// Let's create just one undo step.
		modDoc->GetPatternUndo().PrepareUndo(editPattern, editChannel, editRow, 1, 1, description);
		modified = true;
	}
	modDoc->SetModified();
}


void CEditCommand::OnHScroll(mfcless::ScrollCode code, int pos, QWidget *bar)
{
	if(bar == static_cast<QWidget *>(&sldVolParam))
	{
		OnVolCmdChanged();
	} else if(bar == static_cast<QWidget *>(&sldParam))
	{
		UpdateEffectValue(true);
	}
}


void CEditCommand::OnActivate(uint32 nState, QWidget *pWndOther, bool bMinimized)
{
	DialogBase::OnActivate(nState, pWndOther, bMinimized);
	if(nState == WA_INACTIVE)
		ShowWindow(false);
}


////////////////////////////////////////////////////////////////////////////////////////////
// Chord Editor

void CChordEditor::RegisterMessageMap()
{
	ResizableDialog::RegisterMessageMap();
	AddMessage(WM_MOD_KBDNOTIFY, &CChordEditor::OnKeyboardNotify);
	AddNotify(IDC_COMBO1, mfcless::Notify::SelChange, &CChordEditor::OnChordChanged);
	AddNotify(IDC_COMBO2, mfcless::Notify::SelChange, &CChordEditor::OnBaseNoteChanged);
	AddNotify(IDC_COMBO3, mfcless::Notify::SelChange, &CChordEditor::OnNote1Changed);
	AddNotify(IDC_COMBO4, mfcless::Notify::SelChange, &CChordEditor::OnNote2Changed);
	AddNotify(IDC_COMBO5, mfcless::Notify::SelChange, &CChordEditor::OnNote3Changed);
}


void CChordEditor::DoDataExchange(DataExchange &dx)
{
	ResizableDialog::DoDataExchange(dx);
	//{{AFX_DATA_MAP(CChordEditor)
	ExchangeControl(dx, IDC_KEYBOARD1, m_Keyboard);
	ExchangeControl(dx, IDC_COMBO1, m_CbnShortcut);
	ExchangeControl(dx, IDC_COMBO2, m_CbnBaseNote);
	ExchangeControl(dx, IDC_COMBO3, m_CbnNote[0]);
	ExchangeControl(dx, IDC_COMBO4, m_CbnNote[1]);
	ExchangeControl(dx, IDC_COMBO5, m_CbnNote[2]);
	static_assert(mpt::array_size<decltype(m_CbnNote)>::size == 3);
	//}}AFX_DATA_MAP
}


CChordEditor::CChordEditor(QWidget *parent)
    : ResizableDialog(IDD_CHORDEDIT, parent)
{
	m_chords = TrackerSettings::GetChords();
}

bool CChordEditor::OnInitDialog()
{
	ResizableDialog::OnInitDialog();
	m_Keyboard->Init(this, (CHORD_MAX - CHORD_MIN) / 12, true);

	m_CbnShortcut->setUpdatesEnabled(false);
	m_CbnBaseNote->setUpdatesEnabled(false);
	for(auto &combo : m_CbnNote)
		combo.SetRedraw(false);

	// Shortcut key combo box
	AppendNotesToControl(m_CbnShortcut, NOTE_MIN, NOTE_MIN + static_cast<int>(kcVPEndChords) - static_cast<int>(kcVPStartChords));

	m_CbnShortcut->setCurrentIndex(0);
	// Base Note combo box
	mfcless::SetItemData(m_CbnBaseNote, mfcless::AddItem(m_CbnBaseNote, U_("Relative")), MPTChord::relativeMode);
	AppendNotesToControl(m_CbnBaseNote, NOTE_MIN, NOTE_MIN + 3 * 12 - 1);

	// Chord Note combo boxes
	mpt::ustring s;
	for(int note = CHORD_MIN - 1; note < CHORD_MAX; note++)
	{
		int noteVal = note;
		if(note == CHORD_MIN - 1)
		{
			s = U_("--");
			noteVal = MPTChord::noNote;
		} else
		{
			s = mpt::ToUnicode(CSoundFile::GetDefaultNoteName(mpt::wrapping_modulo(note, 12)));
			const int octave = mpt::wrapping_divide(note, 12);
			if(octave > 0)
				s.AppendFormat(U_(" (+%d)"), octave);
			else if(octave < 0)
				s.AppendFormat(U_(" (%d)"), octave);
		}
		for(auto &combo : m_CbnNote)
			combo.SetItemData(combo.AddString(s), noteVal);
	}

	m_CbnShortcut->setUpdatesEnabled(true);
	m_CbnBaseNote->setUpdatesEnabled(true);
	for(auto &combo : m_CbnNote)
		combo.SetRedraw(true);

	// Update Dialog
	OnChordChanged();
	return true;
}


void CChordEditor::OnDPIChanged()
{
	ResizableDialog::OnDPIChanged();
	m_Keyboard->SendMessage(WM_DPICHANGED);
}


void CChordEditor::OnOK()
{
	TrackerSettings::GetChords() = m_chords;
	ResizableDialog::OnOK();
}


MPTChord &CChordEditor::GetChord()
{
	int chord = m_CbnShortcut->currentIndex();
	if(chord >= 0)
		chord = static_cast<int>(mfcless::GetItemData(m_CbnShortcut, chord)) - NOTE_MIN;
	if(chord < 0 || chord >= static_cast<int>(m_chords.size()))
		chord = 0;
	return m_chords[chord];
}


intptr_t CChordEditor::OnKeyboardNotify(uintptr_t cmd, intptr_t nKey)
{
	const bool outside = static_cast<int>(nKey) == -1;
	if(cmd == KBDNOTIFY_LBUTTONUP && outside)
	{
		// Stopped dragging ouside of keyboard area
		m_mouseDownKey = m_dragKey = MPTChord::noNote;
		return 0;
	} else if (cmd == KBDNOTIFY_MOUSEMOVE || outside)
	{
		return 0;
	}

	MPTChord &chord = GetChord();
	const MPTChord::NoteType key = static_cast<MPTChord::NoteType>(nKey) + CHORD_MIN;
	bool update = false;

	if(cmd == KBDNOTIFY_LBUTTONDOWN && m_mouseDownKey == MPTChord::noNote)
	{
		// Initial mouse down
		m_mouseDownKey = key;
		m_dragKey = MPTChord::noNote;
		return 0;
	}
	if(cmd == KBDNOTIFY_LBUTTONDOWN && m_dragKey == MPTChord::noNote && key != m_mouseDownKey)
	{
		// Start dragging
		m_dragKey = m_mouseDownKey;
	}

	// Remove dragged note or toggle
	bool noteIsSet = false;
	for(auto &note : chord.notes)
	{
		if((m_dragKey != MPTChord::noNote && note == m_dragKey)
		   || (m_dragKey == MPTChord::noNote && note == m_mouseDownKey))
		{
			note = MPTChord::noNote;
			noteIsSet = update = true;
			break;
		}
	}

	// Move or toggle note
	if(cmd != KBDNOTIFY_LBUTTONUP || m_dragKey != MPTChord::noNote || !noteIsSet)
	{
		for(auto &note : chord.notes)
		{
			if(note == MPTChord::noNote)
			{
				note = key;
				update = true;
				break;
			}
		}
	}

	if(cmd == KBDNOTIFY_LBUTTONUP)
		m_mouseDownKey = m_dragKey = MPTChord::noNote;
	else
		m_dragKey = key;

	if(update)
	{
		std::sort(chord.notes.begin(), chord.notes.end(), [](MPTChord::NoteType left, MPTChord::NoteType right)
		{
			return (left == MPTChord::noNote)  ? false : (left < right);
		});
		OnChordChanged();
	}
	return 0;
}


void CChordEditor::OnChordChanged()
{
	const MPTChord &chord = GetChord();
	if(chord.key != MPTChord::relativeMode)
		m_CbnBaseNote->setCurrentIndex(chord.key + 1);
	else
		m_CbnBaseNote->setCurrentIndex(0);
	for(int i = 0; i < MPTChord::notesPerChord - 1; i++)
	{
		int note = chord.notes[i];
		if(note == MPTChord::noNote)
			note = 0;
		else
			note += 1 - CHORD_MIN;
		m_CbnNote[i].SetCurSel(note);
	}
	UpdateKeyboard();
}


void CChordEditor::UpdateKeyboard()
{
	MPTChord &chord = GetChord();
	const int baseNote = (chord.key == MPTChord::relativeMode) ? 0 : (chord.key % 12);
	for(int i = CHORD_MIN; i < CHORD_MAX; i++)
	{
		uint8 b = CKeyboardControl::KEYFLAG_NORMAL;
		for(const auto note : chord.notes)
		{
			if(i == note)
				b |= CKeyboardControl::KEYFLAG_REDDOT;
		}
		if(i == baseNote)
			b |= CKeyboardControl::KEYFLAG_BRIGHTDOT;
		m_Keyboard->SetFlags(i - CHORD_MIN, b);
	}
	m_Keyboard->InvalidateRect(nullptr, false);
}


void CChordEditor::OnBaseNoteChanged()
{
	MPTChord &chord = GetChord();
	int basenote = static_cast<int>(mfcless::GetItemData(m_CbnBaseNote, m_CbnBaseNote->currentIndex()));
	if(basenote != MPTChord::relativeMode)
		basenote -= NOTE_MIN;
	chord.key = (uint8)basenote;
	UpdateKeyboard();
}


void CChordEditor::OnNoteChanged(int noteIndex)
{
	MPTChord &chord = GetChord();
	int note = m_CbnNote[noteIndex].GetCurSel();
	if(note < 0)
		return;
	chord.notes[noteIndex] = static_cast<int8>(m_CbnNote[noteIndex].GetItemData(note));
	UpdateKeyboard();
}


////////////////////////////////////////////////////////////////////////////////////////////
// Keyboard Split Settings (pattern editor)

void CSplitKeyboardSettings::RegisterMessageMap()
{
	DialogBase::RegisterMessageMap();
	AddNotify(IDC_COMBO_OCTAVEMODIFIER, mfcless::Notify::SelChange, &CSplitKeyboardSettings::OnOctaveModifierChanged);
}


void CSplitKeyboardSettings::DoDataExchange(DataExchange &dx)
{
	DialogBase::DoDataExchange(dx);
	//{{AFX_DATA_MAP(CSplitKeyboadSettings)
	ExchangeControl(dx, IDC_COMBO_SPLITINSTRUMENT, m_CbnSplitInstrument);
	ExchangeControl(dx, IDC_COMBO_SPLITNOTE, m_CbnSplitNote);
	ExchangeControl(dx, IDC_COMBO_OCTAVEMODIFIER, m_CbnOctaveModifier);
	ExchangeControl(dx, IDC_COMBO_SPLITVOLUME, m_CbnSplitVolume);
	//}}AFX_DATA_MAP
}


CSplitKeyboardSettings::CSplitKeyboardSettings(QWidget *parent, CSoundFile &sf, SplitKeyboardSettings &settings)
	: DialogBase{IDD_KEYBOARD_SPLIT, parent}
	, sndFile{sf}
	, m_Settings{settings}
{
}


bool CSplitKeyboardSettings::OnInitDialog()
{
	if(sndFile.GetpModDoc() == nullptr)
		return true;

	DialogBase::OnInitDialog();

	mpt::ustring s;

	// Split Notes
	AppendNotesToControl(m_CbnSplitNote, sndFile.GetModSpecifications().noteMin, sndFile.GetModSpecifications().noteMax);
	m_CbnSplitNote->setCurrentIndex(m_Settings.splitNote - (sndFile.GetModSpecifications().noteMin - NOTE_MIN));

	// Octave modifier
	m_CbnOctaveModifier->setUpdatesEnabled(false);
	int numItems = SplitKeyboardSettings::splitOctaveRange * 2 + 1;
	m_CbnSplitVolume->InitStorage(numItems, static_cast<uint32>(numItems * 9 * sizeof(mpt::uchar)));
	for(int i = -SplitKeyboardSettings::splitOctaveRange; i < SplitKeyboardSettings::splitOctaveRange + 1; i++)
	{
		s.Format(i < 0 ? U_("Octave -%d") : i > 0 ? U_("Octave +%d") : U_("No Change"), std::abs(i));
		int n = mfcless::AddItem(m_CbnOctaveModifier, s);
		mfcless::SetItemData(m_CbnOctaveModifier, n, i);
	}
	m_CbnOctaveModifier->setUpdatesEnabled(true);
	m_CbnOctaveModifier->setCurrentIndex(m_Settings.octaveModifier + SplitKeyboardSettings::splitOctaveRange);
	SetChecked(IDC_PATTERN_OCTAVELINK, (m_Settings.octaveLink && m_Settings.octaveModifier != 0) ? 1 : 0);

	// Volume
	m_CbnSplitVolume->setUpdatesEnabled(false);
	m_CbnSplitVolume->InitStorage(65, 65 * 4 * sizeof(mpt::uchar));
	mfcless::AddItem(m_CbnSplitVolume, U_("No Change"));
	mfcless::SetItemData(m_CbnSplitVolume, 0, 0);
	for(int i = 1; i <= 64; i++)
	{
		s.Format(U_("%d"), i);
		int n = mfcless::AddItem(m_CbnSplitVolume, s);
		mfcless::SetItemData(m_CbnSplitVolume, n, i);
	}
	m_CbnSplitVolume->setUpdatesEnabled(true);
	m_CbnSplitVolume->setCurrentIndex(m_Settings.splitVolume);

	// Instruments
	m_CbnSplitInstrument->setUpdatesEnabled(false);
	numItems = 1 + (sndFile.GetNumInstruments() ? sndFile.GetNumInstruments() : sndFile.GetNumSamples());
	m_CbnSplitInstrument->InitStorage(numItems, static_cast<uint32>(numItems * 16 * sizeof(mpt::uchar)));
	mfcless::SetItemData(m_CbnSplitInstrument, mfcless::AddItem(m_CbnSplitInstrument, U_("No Change")), 0);

	if(sndFile.GetNumInstruments())
	{
		for(INSTRUMENTINDEX nIns = 1; nIns <= sndFile.GetNumInstruments(); nIns++)
		{
			if(sndFile.Instruments[nIns] == nullptr)
				continue;

			mpt::ustring displayName = sndFile.GetpModDoc()->GetPatternViewInstrumentName(nIns);
			int n = mfcless::AddItem(m_CbnSplitInstrument, displayName);
			mfcless::SetItemData(m_CbnSplitInstrument, n, nIns);
		}
	} else
	{
		for(SAMPLEINDEX nSmp = 1; nSmp <= sndFile.GetNumSamples(); nSmp++)
		{
			if(sndFile.GetSample(nSmp).HasSampleData())
			{
				s.Format(U_("%02d: "), nSmp);
				s += mpt::ToUnicode(sndFile.GetCharsetInternal(), sndFile.m_szNames[nSmp]);
				int n = mfcless::AddItem(m_CbnSplitInstrument, s);
				mfcless::SetItemData(m_CbnSplitInstrument, n, nSmp);
			}
		}
	}
	m_CbnSplitInstrument->setUpdatesEnabled(true);
	m_CbnSplitInstrument->setCurrentIndex(m_Settings.splitInstrument);

	return true;
}


void CSplitKeyboardSettings::OnOK()
{
	DialogBase::OnOK();

	m_Settings.splitNote = static_cast<ModCommand::NOTE>(mfcless::GetItemData(m_CbnSplitNote, m_CbnSplitNote->currentIndex()) - 1);
	m_Settings.octaveModifier = m_CbnOctaveModifier->currentIndex() - SplitKeyboardSettings::splitOctaveRange;
	m_Settings.octaveLink = (IsChecked(IDC_PATTERN_OCTAVELINK) != 0);
	m_Settings.splitVolume = static_cast<ModCommand::VOL>(m_CbnSplitVolume->currentIndex());
	m_Settings.splitInstrument = static_cast<ModCommand::INSTR>(mfcless::GetItemData(m_CbnSplitInstrument, m_CbnSplitInstrument->currentIndex()));
}


void CSplitKeyboardSettings::OnCancel()
{
	DialogBase::OnCancel();
}


void CSplitKeyboardSettings::OnOctaveModifierChanged()
{
	SetChecked(IDC_PATTERN_OCTAVELINK, (m_CbnOctaveModifier->currentIndex() != 9) ? 1 : 0);
}


/////////////////////////////////////////////////////////////////////////
// Show channel properties from pattern editor

void QuickChannelProperties::RegisterMessageMap()
{
	DialogBase::RegisterMessageMap();
	AddScroll(Qt::Horizontal, &QuickChannelProperties::OnHScroll);  // Sliders
	// ON_WM_ACTIVATE()	// Catch Window focus change: by hand (Qt event override)  // Catch Window focus change
	AddNotify(IDC_EDIT1, mfcless::Notify::EditUpdate, &QuickChannelProperties::OnVolChanged);
	AddNotify(IDC_EDIT2, mfcless::Notify::EditUpdate, &QuickChannelProperties::OnPanChanged);
	AddNotify(IDC_EDIT3, mfcless::Notify::EditUpdate, &QuickChannelProperties::OnNameChanged);
	AddCommand(IDC_CHECK1, &QuickChannelProperties::OnMuteChanged);
	AddCommand(IDC_CHECK2, &QuickChannelProperties::OnSurroundChanged);
	AddCommand(IDC_BUTTON1, &QuickChannelProperties::OnPrevChannel);
	AddCommand(IDC_BUTTON2, &QuickChannelProperties::OnNextChannel);
	AddCommand(IDC_BUTTON3, &QuickChannelProperties::OnChangeColor);
	AddCommand(IDC_BUTTON4, &QuickChannelProperties::OnChangeColor);
	AddCommand(IDC_BUTTON5, &QuickChannelProperties::OnPickPrevColor);
	AddCommand(IDC_BUTTON6, &QuickChannelProperties::OnPickNextColor);
	AddMessage(WM_MOD_KEYCOMMAND, &QuickChannelProperties::OnCustomKeyMsg);
}


void QuickChannelProperties::DoDataExchange(DataExchange &dx)
{
	ExchangeControl(dx, IDC_SLIDER1, m_volSlider);
	ExchangeControl(dx, IDC_SLIDER2, m_panSlider);
	ExchangeControl(dx, IDC_SPIN1, m_volSpin);
	ExchangeControl(dx, IDC_SPIN2, m_panSpin);
	ExchangeControl(dx, IDC_EDIT3, m_nameEdit);
}


QuickChannelProperties::~QuickChannelProperties()
{
	DestroyWindow();
}


void QuickChannelProperties::OnActivate(uint32 nState, QWidget *, bool)
{
	if(nState == WA_INACTIVE && !m_settingColor)
	{
		// Hide window when changing focus to another window.
		m_visible = false;
		ShowWindow(false);
	}
}


// Show channel properties for a given channel at a given screen position.
void QuickChannelProperties::Show(CModDoc *modDoc, CHANNELINDEX chn, mfcless::Point position)
{
	if(!m_hWnd)
	{
		Create(IDD_CHANNELSETTINGS, nullptr);
		m_colorBtn->SubclassDlgItem(IDC_BUTTON4, this);
		m_colorBtnPrev->SubclassDlgItem(IDC_BUTTON5, this);
		m_colorBtnNext->SubclassDlgItem(IDC_BUTTON6, this);

		m_volSlider->setRange(0, 64);
		m_volSlider->setTickInterval(8);
		m_volSpin->SetRange(0, 64);

		m_panSlider->setRange(0, 64);
		m_panSlider->setTickInterval(8);
		m_panSpin->SetRange(0, 256);
	}
	m_document = modDoc;
	m_channel = chn;

	SetParent(nullptr);

	// Center window around point where user clicked.
	mfcless::Rect rect, screenRect;
	GetWindowRect(rect);
	::GetWindowRect(::GetDesktopWindow(), &screenRect);
	rect.MoveToXY(
	    Clamp(static_cast<int>(position.x) - rect.Width() / 2, 0, static_cast<int>(screenRect.right) - rect.Width()),
	    Clamp(static_cast<int>(position.y) - rect.Height() / 2, 0, static_cast<int>(screenRect.bottom) - rect.Height()));
	MoveWindow(rect);

	UpdateDisplay();

	const bool enablePan = (m_document->GetModType() & (MOD_TYPE_XM | MOD_TYPE_MOD)) ? false : true;
	const bool itOnly = (m_document->GetModType() & (MOD_TYPE_IT | MOD_TYPE_MPT)) ? true : false;

	// Volume controls
	m_volSlider->setEnabled(itOnly);
	m_volSpin->setEnabled(itOnly);
	::EnableWindow(::GetDlgItem(m_hWnd, IDC_EDIT1), itOnly);

	// Pan controls
	m_panSlider->setEnabled(enablePan);
	m_panSpin->setEnabled(enablePan);
	::EnableWindow(::GetDlgItem(m_hWnd, IDC_EDIT2), enablePan);
	::EnableWindow(::GetDlgItem(m_hWnd, IDC_CHECK2), itOnly);

	// Channel name
	m_nameEdit->setEnabled((m_document->GetModType() & (MOD_TYPE_IT | MOD_TYPE_MPT | MOD_TYPE_XM)) ? true : false);

	SetFocusToFirstControl();
	ShowWindow(true);
	m_visible = true;
}


void QuickChannelProperties::UpdateDisplay()
{
	const QWidget *oldFocusWnd = GetFocus();
	SetWindowText(MPT_UFORMAT("Settings for Channel {}")(m_channel + 1).c_str());

	// Set up channel properties
	m_visible = false;
	const ModChannelSettings &settings = m_document->GetSoundFile().ChnSettings[m_channel];
	SetItemInt(IDC_EDIT1, settings.nVolume);
	SetItemInt(IDC_EDIT2, settings.nPan);
	m_volSlider->setValue(settings.nVolume);
	m_panSlider->setValue(settings.nPan / 4u);
	SetChecked(IDC_CHECK1, (settings.dwFlags[CHN_MUTE]) ? true : false);
	SetChecked(IDC_CHECK2, (settings.dwFlags[CHN_SURROUND]) ? true : false);

	mpt::uchar description[16];
	wsprintf(description, U_("Channel %d:"), m_channel + 1);
	SetItemText(IDC_STATIC_CHANNEL_NAME, description);
	m_nameEdit->setMaxLength(MAX_CHANNELNAME - 1);
	m_nameEdit->SetWindowText(mpt::ToUnicode(m_document->GetSoundFile().GetCharsetInternal(), settings.szName));

	const bool isFirst = (m_channel <= 0), isLast = (m_channel >= m_document->GetNumChannels() - 1);

	m_colorBtn->SetColor(settings.color);
	m_colorBtnPrev->setEnabled(isFirst ? false : true);
	if(!isFirst)
		m_colorBtnPrev->SetColor(m_document->GetSoundFile().ChnSettings[m_channel - 1].color);
	m_colorBtnNext->setEnabled(isLast ? false : true);
	if(!isLast)
		m_colorBtnNext->SetColor(m_document->GetSoundFile().ChnSettings[m_channel + 1].color);

	m_settingsChanged = false;
	m_visible = true;

	::EnableWindow(::GetDlgItem(m_hWnd, IDC_BUTTON1), isFirst ? false : true);
	::EnableWindow(::GetDlgItem(m_hWnd, IDC_BUTTON2), isLast ? false : true);

	// Avoid focus trap if we just navigated to the first or last channel
	const int PrevNextButtons[] = {IDC_BUTTON1, IDC_BUTTON2, IDC_BUTTON5, IDC_BUTTON6};
	for(int button : PrevNextButtons)
	{
		const QWidget *wnd = GetDlgItem(button);
		if (oldFocusWnd == wnd && !wnd->IsWindowEnabled())
		{
			SetFocusToFirstControl();
			break;
		}
	}
}

void QuickChannelProperties::PrepareUndo()
{
	if(!m_settingsChanged)
	{
		// Backup old channel settings through pattern undo.
		m_settingsChanged = true;
		m_document->GetPatternUndo().PrepareChannelUndo(m_channel, 1, "Channel Settings");
	}
}


void QuickChannelProperties::OnVolChanged()
{
	if(!m_visible)
	{
		return;
	}

	uint16 volume = static_cast<uint16>(GetItemInt(IDC_EDIT1));
	if(volume >= 0 && volume <= 64)
	{
		PrepareUndo();
		m_document->SetChannelGlobalVolume(m_channel, volume);
		m_volSlider->setValue(volume);
		m_document->UpdateAllViews(nullptr, GeneralHint(m_channel).Channels(), this);
	}
}


void QuickChannelProperties::OnPanChanged()
{
	if(!m_visible)
	{
		return;
	}

	uint16 panning = static_cast<uint16>(GetItemInt(IDC_EDIT2));
	if(panning >= 0 && panning <= 256)
	{
		PrepareUndo();
		m_document->SetChannelDefaultPan(m_channel, panning);
		m_panSlider->setValue(panning / 4u);
		m_document->UpdateAllViews(nullptr, GeneralHint(m_channel).Channels(), this);
		// Surround is forced off when changing pan, so uncheck the checkbox.
		SetChecked(IDC_CHECK2, 0);
	}
}


void QuickChannelProperties::OnHScroll(mfcless::ScrollCode code, int pos, QWidget *bar)
{
	if(!m_visible)
	{
		return;
	}

	bool update = false;

	// Volume slider
	if(bar->m_hWnd == m_volSlider->m_hWnd)
	{
		uint16 pos = static_cast<uint16>(m_volSlider->value());
		PrepareUndo();
		if(m_document->SetChannelGlobalVolume(m_channel, pos))
		{
			SetItemInt(IDC_EDIT1, pos);
			update = true;
		}
	}
	// Pan slider
	if(bar->m_hWnd == m_panSlider->m_hWnd)
	{
		uint16 pos = static_cast<uint16>(m_panSlider->value());
		PrepareUndo();
		if(m_document->SetChannelDefaultPan(m_channel, pos * 4u))
		{
			SetItemInt(IDC_EDIT2, pos * 4u);
			SetChecked(IDC_CHECK2, 0);
			update = true;
		}
	}

	if(update)
	{
		m_document->UpdateAllViews(nullptr, GeneralHint(m_channel).Channels(), this);
	}
}


void QuickChannelProperties::OnMuteChanged()
{
	if(!m_visible)
	{
		return;
	}

	m_document->MuteChannel(m_channel, IsChecked(IDC_CHECK1) != 0);
	m_document->UpdateAllViews(nullptr, GeneralHint(m_channel).Channels(), this);
}


void QuickChannelProperties::OnSurroundChanged()
{
	if(!m_visible)
	{
		return;
	}

	PrepareUndo();
	m_document->SurroundChannel(m_channel, IsChecked(IDC_CHECK2) != 0);
	m_document->UpdateAllViews(nullptr, GeneralHint(m_channel).Channels(), this);
	UpdateDisplay();
}


void QuickChannelProperties::OnNameChanged()
{
	if(!m_visible)
	{
		return;
	}

	ModChannelSettings &settings = m_document->GetSoundFile().ChnSettings[m_channel];
	mpt::ustring newNameTmp;
	m_nameEdit->GetWindowText(newNameTmp);
	std::string newName = mpt::ToCharset(m_document->GetSoundFile().GetCharsetInternal(), newNameTmp);

	if(newName != settings.szName)
	{
		PrepareUndo();
		settings.szName = newName;
		m_document->SetModified();
		m_document->UpdateAllViews(nullptr, GeneralHint(m_channel).Channels(), this);
	}
}


void QuickChannelProperties::OnChangeColor()
{
	m_settingColor = true;
	if(auto color = m_colorBtn->PickColor(m_document->GetSoundFile(), m_channel); color.has_value())
	{
		PrepareUndo();
		m_document->GetSoundFile().ChnSettings[m_channel].color = *color;
		if(m_document->SupportsChannelColors())
			m_document->SetModified();
		m_document->UpdateAllViews(nullptr, GeneralHint(m_channel).Channels(), this);
	}
	m_settingColor = false;
}


void QuickChannelProperties::OnPickPrevColor()
{
	if(m_channel > 0)
		PickColorFromChannel(m_channel - 1);
}


void QuickChannelProperties::OnPickNextColor()
{
	if(m_channel < m_document->GetNumChannels() - 1)
		PickColorFromChannel(m_channel + 1);
}


void QuickChannelProperties::PickColorFromChannel(CHANNELINDEX channel)
{
	auto &channels = m_document->GetSoundFile().ChnSettings;
	if(channels[channel].color != channels[m_channel].color)
	{
		PrepareUndo();
		channels[m_channel].color = channels[channel].color;
		m_colorBtn->SetColor(channels[m_channel].color);
		if(m_document->SupportsChannelColors())
			m_document->SetModified();
		m_document->UpdateAllViews(nullptr, GeneralHint(m_channel).Channels(), this);
	}
}


void QuickChannelProperties::OnPrevChannel()
{
	if(m_channel > 0)
	{
		m_channel--;
		UpdateDisplay();
	}
}


void QuickChannelProperties::OnNextChannel()
{
	if(m_channel < m_document->GetNumChannels() - 1)
	{
		m_channel++;
		UpdateDisplay();
	}
}


bool QuickChannelProperties::PreTranslateMessage(mfcless::Message *pMsg)
{
	if(pMsg)
	{
		//We handle keypresses before Windows has a chance to handle them (for alt etc..)
		if((pMsg->message == mfcless::msg::SysKeyUp) || (pMsg->message == mfcless::msg::KeyUp) ||
			(pMsg->message == mfcless::msg::SysKeyDown) || (pMsg->message == mfcless::msg::KeyDown))
		{
			CInputHandler *ih = CMainFrame::GetInputHandler();
			if(ih->KeyEvent(kCtxChannelSettings, ih->Translate(*pMsg), this) != kcNull)
			{
				return true;  // Mapped to a command, no need to pass message on.
			}
		}
	}

	return DialogBase::PreTranslateMessage(pMsg);
}


intptr_t QuickChannelProperties::OnCustomKeyMsg(uintptr_t wParam, intptr_t)
{
	switch(wParam)
	{
	case kcChnSettingsPrev:
		OnPrevChannel();
		return wParam;
	case kcChnSettingsNext:
		OnNextChannel();
		return wParam;
	case kcChnColorFromPrev:
		OnPickPrevColor();
		return wParam;
	case kcChnColorFromNext:
		OnPickNextColor();
		return wParam;
	case kcChnSettingsClose:
		OnActivate(WA_INACTIVE, nullptr, false);
		return wParam;
	}

	return kcNull;
}


mpt::ustring QuickChannelProperties::GetToolTipText(uint32 id, QWidget *) const
{
	mpt::ustring text;
	CommandID cmd = kcNull;
	switch(id)
	{
	case IDC_EDIT1:
	case IDC_SLIDER1:
		text = CModDoc::LinearToDecibelsString(m_document->GetSoundFile().ChnSettings[m_channel].nVolume, 64.0);
		break;
	case IDC_EDIT2:
	case IDC_SLIDER2:
		text = CModDoc::PanningToString(m_document->GetSoundFile().ChnSettings[m_channel].nPan, 128);
		break;
	case IDC_BUTTON1:
		text = U_("Previous Channel");
		cmd = kcChnSettingsPrev;
		break;
	case IDC_BUTTON2:
		text = U_("Next Channel");
		cmd = kcChnSettingsNext;
		break;
	case IDC_BUTTON5:
		text = U_("Take color from previous channel");
		cmd = kcChnColorFromPrev;
		break;
	case IDC_BUTTON6:
		text = U_("Take color from next channel");
		cmd = kcChnColorFromNext;
		break;
	}

	if(cmd != kcNull)
	{
		auto keyText = CMainFrame::GetInputHandler()->m_activeCommandSet->GetKeyTextFromCommand(cmd, 0);
		if(!keyText.IsEmpty())
			text += MPT_UFORMAT(" ({})")(keyText);
	}

	return text;
}


/////////////////////////////////////////////////////////////////////////
// Metronome settings

static constexpr float METRONOME_VOLUME_SCALE = 0.2f;

void MetronomeSettingsDlg::RegisterMessageMap()
{
	DialogBase::RegisterMessageMap();
	AddScroll(Qt::Horizontal, &MetronomeSettingsDlg::OnHScroll);
	AddCommand(IDC_CHECK1, &MetronomeSettingsDlg::OnToggleMetronome);
	AddCommand(IDC_BUTTON1, &MetronomeSettingsDlg::OnBrowseMeasure);
	AddCommand(IDC_BUTTON2, &MetronomeSettingsDlg::OnBrowseBeat);
	AddNotify(IDC_COMBO1, mfcless::Notify::SelChange, &MetronomeSettingsDlg::OnSampleChanged);
	AddNotify(IDC_COMBO2, mfcless::Notify::SelChange, &MetronomeSettingsDlg::OnSampleChanged);
	AddNotify(IDC_EDIT1, mfcless::Notify::KillFocus, &MetronomeSettingsDlg::OnSampleChanged);
	AddNotify(IDC_EDIT2, mfcless::Notify::KillFocus, &MetronomeSettingsDlg::OnSampleChanged);
}


void MetronomeSettingsDlg::DoDataExchange(DataExchange &dx)
{
	ExchangeControl(dx, IDC_SLIDER1, m_volumeSlider);
	ExchangeControl(dx, IDC_COMBO1, m_measureCombo);
	ExchangeControl(dx, IDC_COMBO2, m_beatCombo);
	ExchangeControl(dx, IDC_EDIT1, m_measureEdit);
	ExchangeControl(dx, IDC_EDIT2, m_beatEdit);
	ExchangeControl(dx, IDC_BUTTON1, m_measureButton);
	ExchangeControl(dx, IDC_BUTTON2, m_beatButton);
}


MetronomeSettingsDlg::MetronomeSettingsDlg(QWidget *parent)
	: DialogBase{IDD_METRONOME_SETTINGS, parent}
{ }


void MetronomeSettingsDlg::SetSampleInfo(const mpt::PathString &path, CComboBox &combo, CEdit &edit, CButton &browseButton)
{
	bool enable = false;
	if(path.empty())
	{
		combo.SetCurSel(0);
	} else if(path == TrackerSettings::GetDefaultMetronomeSample())
	{
		combo.SetCurSel(1);
	} else
	{
		combo.SetCurSel(2);
		edit.SetWindowText(path.ToCString());
		enable = true;
	}
	edit.EnableWindow(enable);
	browseButton.EnableWindow(enable);
}


bool MetronomeSettingsDlg::OnInitDialog()
{
	DialogBase::OnInitDialog();
	SetChecked(IDC_CHECK1, TrackerSettings::Instance().metronomeEnabled ? 1 : 0);
	m_volumeSlider->setRange(static_cast<int>(-48 / METRONOME_VOLUME_SCALE), 0, true);
	m_volumeSlider->setValue(mpt::saturate_round<int>(TrackerSettings::Instance().metronomeVolume / METRONOME_VOLUME_SCALE));
	m_volumeSlider->setTickInterval(static_cast<int>(3 / METRONOME_VOLUME_SCALE));
	static const mpt::uchar *Options[] = {U_("Off"), U_("Default (Sine)"), U_("Custom Sample")};
	for(const mpt::uchar *option : Options)
	{
		mfcless::AddItem(m_measureCombo, option);
		mfcless::AddItem(m_beatCombo, option);
	}
	SetSampleInfo(TrackerSettings::Instance().metronomeSampleMeasure, m_measureCombo, m_measureEdit, m_measureButton);
	SetSampleInfo(TrackerSettings::Instance().metronomeSampleBeat, m_beatCombo, m_beatEdit, m_beatButton);
	GetDlgItem(IDC_VOLUME)->SetWindowText(GetVolumeString());
	return true;
}


mpt::ustring MetronomeSettingsDlg::GetVolumeString() const
{
	mpt::ustring s = (m_volumeSlider->value() >= 0) ? U_("+") : U_("");
	s.AppendFormat(U_("%.2f dB"), m_volumeSlider->value() * METRONOME_VOLUME_SCALE);
	return s;
}


void MetronomeSettingsDlg::OnHScroll(mfcless::ScrollCode code, int pos, QWidget *bar)
{
	if(bar == static_cast<QWidget *>(m_volumeSlider))
	{
		TrackerSettings::Instance().metronomeVolume = m_volumeSlider->value() * METRONOME_VOLUME_SCALE;
		CMainFrame::GetMainFrame()->UpdateMetronomeVolume();
		GetDlgItem(IDC_VOLUME)->SetWindowText(GetVolumeString());
	}
}


void MetronomeSettingsDlg::OnToggleMetronome()
{
	TrackerSettings::Instance().metronomeEnabled = IsChecked(IDC_CHECK1) != 0;
	CMainFrame::GetMainFrame()->UpdateMetronomeSamples();
}


bool MetronomeSettingsDlg::GetSampleInfo(Setting<mpt::PathString> &path, CComboBox &combo, CEdit &edit, CButton &browseButton)
{
	mpt::ustring s;
	bool modified = false;
	bool enable = false;
	switch(combo.GetCurSel())
	{
	case 0:
		if(path != mpt::PathString{})
		{
			modified = true;
			path = {};
		}
		break;
	case 1:
		if(path != TrackerSettings::GetDefaultMetronomeSample())
		{
			modified = true;
			path = TrackerSettings::GetDefaultMetronomeSample();
		}
		break;
	case 2:
		edit.GetWindowText(s);
		if(auto newPath = mpt::PathString::FromCString(s); path != newPath)
		{
			path = newPath;
			modified = true;
		}
		enable = true;
		break;
	}
	edit.EnableWindow(enable);
	browseButton.EnableWindow(enable);
	return modified;
}


void MetronomeSettingsDlg::OnSampleChanged()
{
	bool modified = GetSampleInfo(TrackerSettings::Instance().metronomeSampleMeasure, m_measureCombo, m_measureEdit, m_measureButton);
	modified |= GetSampleInfo(TrackerSettings::Instance().metronomeSampleBeat, m_beatCombo, m_beatEdit, m_beatButton);
	if(modified)
		CMainFrame::GetMainFrame()->LoadMetronomeSamples();
}


mpt::PathString MetronomeSettingsDlg::BrowseForSample(const mpt::PathString &path)
{
	static int lastIndex = 0;
	FileDialog dlg = OpenFileDialog()
		.EnableAudioPreview()
		.ExtensionFilter(ConstructSampleFormatFileFilter(false))
		.WorkingDirectory(path.empty() ? TrackerSettings::Instance().PathSamples.GetWorkingDir() : path.GetDirectoryWithDrive())
		.DefaultFilename(path.GetFilename())
		.FilterIndex(&lastIndex);
	if(!dlg.Show(this))
		return {};
	TrackerSettings::Instance().PathSamples.SetWorkingDir(dlg.GetWorkingDirectory());
	return dlg.GetFirstFile();
}


void MetronomeSettingsDlg::OnBrowseMeasure()
{
	auto newPath = BrowseForSample(TrackerSettings::Instance().metronomeSampleMeasure);
	if(newPath.empty())
		return;
	m_measureEdit->SetWindowText(newPath.ToCString());
	OnSampleChanged();
}


void MetronomeSettingsDlg::OnBrowseBeat()
{
	auto newPath = BrowseForSample(TrackerSettings::Instance().metronomeSampleBeat);
	if(newPath.empty())
		return;
	m_beatEdit->SetWindowText(newPath.ToCString());
	OnSampleChanged();
}


mpt::ustring MetronomeSettingsDlg::GetToolTipText(uint32 id, QWidget *) const
{
	mpt::ustring s;
	switch(id)
	{
	case IDC_SLIDER1:
		s = GetVolumeString();
		break;
	}

	return s;
}


OPENMPT_NAMESPACE_END
