/*
 * PatternFindReplaceDlg.cpp
 * -------------------------
 * Purpose: The find/replace dialog for pattern data.
 * Notes  : mfcless twin of upstream's mptrack/PatternFindReplaceDlg.cpp.
 *          Draft from qt/tools/twin.py, not yet edited by hand.
 * Authors: Olivier Lapicque
 *          OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "PatternFindReplaceDlg.h"
#include "mfcless/Widgets.h"
#include "DialogBase.h"
#include "Mptrack.h"
#include "PatternFindReplace.h"
#include "resource.h"
#include "View_pat.h"
#include "mpt/parse/parse.hpp"


OPENMPT_NAMESPACE_BEGIN

// CFindRangeDlg: Find a range of values.

class CFindRangeDlg : public DialogBase
{
public:
	enum DisplayMode
	{
		kDecimal,
		kHex,
		kNotes,
	};
protected:
	CComboBox m_cbnMin, m_cbnMax;
	int m_minVal, m_minDefault;
	int m_maxVal, m_maxDefault;
	DisplayMode m_displayMode;

public:
	CFindRangeDlg(QWidget *parent, int minVal, int minDefault, int maxVal, int maxDefault, DisplayMode displayMode) : DialogBase(IDD_FIND_RANGE, parent)
		, m_minVal(minVal)
		, m_minDefault(minDefault)
		, m_maxVal(maxVal)
		, m_maxDefault(maxDefault)
		, m_displayMode(displayMode)
	{ }

	int GetMinVal() const { return m_minVal; }
	int GetMaxVal() const { return m_maxVal; }

protected:
	void DoDataExchange(CDataExchange* pDX) override
	{
		DialogBase::DoDataExchange(dx);
		ExchangeControl(dx, IDC_COMBO1, m_cbnMin);
		ExchangeControl(dx, IDC_COMBO2, m_cbnMax);
	}

	bool OnInitDialog() override
	{
		DialogBase::OnInitDialog();

		if(m_displayMode == kNotes)
		{
			AppendNotesToControl(m_cbnMin, static_cast<ModCommand::NOTE>(m_minVal), static_cast<ModCommand::NOTE>(m_maxVal));
			AppendNotesToControl(m_cbnMax, static_cast<ModCommand::NOTE>(m_minVal), static_cast<ModCommand::NOTE>(m_maxVal));
		} else
		{
			const int numItems = m_minVal - m_maxVal + 1;
			const uint32 strSize = static_cast<uint32>(numItems * 4 * sizeof(mpt::uchar));
			m_cbnMin.InitStorage(numItems, strSize);
			m_cbnMax.InitStorage(numItems, strSize);
			const mpt::uchar *formatString;
			if(m_displayMode == kHex && m_maxVal <= 0x0F)
				formatString = UL_("%01X");
			else if(m_displayMode == kHex)
				formatString = UL_("%02X");
			else
				formatString = UL_("%d");
			for(int i = m_minVal; i <= m_maxVal; i++)
			{
				mpt::uchar s[16];
				wsprintf(s, formatString, i);
				m_cbnMin.SetItemData(m_cbnMin.AddString(s), i);
				m_cbnMax.SetItemData(m_cbnMax.AddString(s), i);
			}
		}
		if(m_minDefault < m_minVal || m_minDefault > m_maxDefault)
		{
			m_minDefault = m_minVal;
			m_maxDefault = m_maxVal;
		}
		m_cbnMin.SetCurSel(m_minDefault - m_minVal);
		m_cbnMax.SetCurSel(m_maxDefault - m_minVal);

		return true;
	}

	void OnOK() override
	{
		DialogBase::OnOK();
		m_minVal = static_cast<int>(m_cbnMin.GetItemData(m_cbnMin.GetCurSel()));
		m_maxVal = static_cast<int>(m_cbnMax.GetItemData(m_cbnMax.GetCurSel()));
		if(m_maxVal < m_minVal)
			std::swap(m_minVal, m_maxVal);
	}
};


void CFindReplaceTab::RegisterMessageMap()
{
	PropertyPage::RegisterMessageMap();
	AddNotify(IDC_COMBO1, mfcless::Notify::SelChange, &CFindReplaceTab::OnNoteChanged);
	AddNotify(IDC_COMBO2, mfcless::Notify::SelChange, &CFindReplaceTab::OnInstrChanged);
	AddNotify(IDC_COMBO3, mfcless::Notify::SelChange, &CFindReplaceTab::OnVolCmdChanged);
	AddNotify(IDC_COMBO4, mfcless::Notify::SelChange, &CFindReplaceTab::OnVolumeChanged);
	AddNotify(IDC_COMBO5, mfcless::Notify::SelChange, &CFindReplaceTab::OnEffectChanged);
	AddNotify(IDC_COMBO6, mfcless::Notify::SelChange, &CFindReplaceTab::OnParamChanged);
	AddNotify(IDC_COMBO7, mfcless::Notify::SelChange, &CFindReplaceTab::OnPCParamChanged);
	AddNotify(IDC_COMBO4, mfcless::Notify::EditChange, &CFindReplaceTab::OnVolumeChanged);
	AddNotify(IDC_COMBO6, mfcless::Notify::EditChange, &CFindReplaceTab::OnParamChanged);
	AddCommand(IDC_CHECK1, &CFindReplaceTab::OnCheckNote);
	AddCommand(IDC_CHECK2, &CFindReplaceTab::OnCheckInstr);
	AddCommand(IDC_CHECK3, &CFindReplaceTab::OnCheckVolCmd);
	AddCommand(IDC_CHECK4, &CFindReplaceTab::OnCheckVolume);
	AddCommand(IDC_CHECK5, &CFindReplaceTab::OnCheckEffect);
	AddCommand(IDC_CHECK6, &CFindReplaceTab::OnCheckParam);
	AddCommand(IDC_CHECK7, &CFindReplaceTab::OnCheckChannelSearch);
}


void CFindReplaceTab::DoDataExchange(DataExchange &dx)
{
	PropertyPage::DoDataExchange(dx);
	ExchangeControl(dx, IDC_COMBO1, m_cbnNote);
	ExchangeControl(dx, IDC_COMBO2, m_cbnInstr);
	ExchangeControl(dx, IDC_COMBO3, m_cbnVolCmd);
	ExchangeControl(dx, IDC_COMBO4, m_cbnVolume);
	ExchangeControl(dx, IDC_COMBO5, m_cbnCommand);
	ExchangeControl(dx, IDC_COMBO6, m_cbnParam);
	ExchangeControl(dx, IDC_COMBO7, m_cbnPCParam);
}


bool CFindReplaceTab::OnInitDialog()
{
	mpt::ustring s;

	PropertyPage::OnInitDialog();
	// Search flags
	FlagSet<FindReplace::Flags> flags = m_isReplaceTab ? m_settings.replaceFlags : m_settings.findFlags;

	COMBOBOXINFO info;
	info.cbSize = sizeof(info);
	if(m_cbnVolume->GetComboBoxInfo(&info))
	{
		::SetWindowLong(info.hwndItem, GWL_STYLE, ::GetWindowLong(info.hwndItem, GWL_STYLE) | ES_NUMBER);
		::SendMessage(info.hwndItem, EM_SETLIMITTEXT, 4, 0);
	}
	if(m_cbnParam->GetComboBoxInfo(&info))
	{
		// Might need to enter hex values
		//::SetWindowLong(info.hwndItem, GWL_STYLE, ::GetWindowLong(info.hwndItem, GWL_STYLE) | ES_NUMBER);
		::SendMessage(info.hwndItem, EM_SETLIMITTEXT, 4, 0);
	}

	SetChecked(IDC_CHECK1, flags[FindReplace::Note]                           ? 1 : 0);
	SetChecked(IDC_CHECK2, flags[FindReplace::Instr]                          ? 1 : 0);
	SetChecked(IDC_CHECK3, flags[FindReplace::VolCmd | FindReplace::PCParam]  ? 1 : 0);
	SetChecked(IDC_CHECK4, flags[FindReplace::Volume | FindReplace::PCValue]  ? 1 : 0);
	SetChecked(IDC_CHECK5, flags[FindReplace::Command]                        ? 1 : 0);
	SetChecked(IDC_CHECK6, flags[FindReplace::Param]                          ? 1 : 0);
	if(m_isReplaceTab)
	{
		SetChecked(IDC_CHECK7, flags[FindReplace::Replace]    ? 1 : 0);
		SetChecked(IDC_CHECK8, flags[FindReplace::ReplaceAll] ? 1 : 0);
	} else
	{
		SetChecked(IDC_CHECK7, flags[FindReplace::InChannels] ? 1 : 0);
		int nButton = IDC_RADIO1;
		if(flags[FindReplace::FullSearch])
			nButton = IDC_RADIO2;
		else if(flags[FindReplace::InPatSelection])
			nButton = IDC_RADIO3;
		
		CheckRadio(IDC_RADIO1, IDC_RADIO3, nButton);
		EnableItem(IDC_RADIO3, flags[FindReplace::InPatSelection] ? true : false);
		SetItemInt(IDC_EDIT1, m_settings.findChnMin + 1);
		SetItemInt(IDC_EDIT2, m_settings.findChnMax + 1);
		static_cast<CSpinButtonCtrl *>(GetDlgItem(IDC_SPIN1))->SetRange32(1, m_sndFile.GetNumChannels());
		static_cast<CSpinButtonCtrl *>(GetDlgItem(IDC_SPIN2))->SetRange32(1, m_sndFile.GetNumChannels());

		// Pre-fill with selected pattern data
		if(!flags[FindReplace::Note] && m_initialValues.note != NOTE_NONE)
		{
			m_settings.findNoteMin = m_settings.findNoteMax = m_initialValues.note;
		}
		if(!flags[FindReplace::Instr] && m_initialValues.instr != 0)
		{
			m_settings.findInstrMin = m_settings.findInstrMax = m_initialValues.instr;
		}
		if(IsPCEvent())
		{
			if(!flags[FindReplace::PCParam] && m_initialValues.GetValueVolCol() != 0)
				m_settings.findParamMin = m_settings.findParamMax = m_initialValues.GetValueVolCol();
			if(!flags[FindReplace::PCValue] && m_initialValues.GetValueEffectCol() != 0)
				m_settings.findVolumeMin = m_settings.findVolumeMax = m_initialValues.GetValueEffectCol();
		} else
		{
			if(!flags[FindReplace::VolCmd] && m_initialValues.volcmd != VOLCMD_NONE)
				m_settings.findVolCmd = m_initialValues.volcmd;
			if(!flags[FindReplace::Volume] && m_initialValues.volcmd != VOLCMD_NONE)
				m_settings.findVolumeMin = m_settings.findVolumeMax = m_initialValues.vol;
			if(!flags[FindReplace::Command] && m_initialValues.command != CMD_NONE)
				m_settings.findCommand = m_initialValues.command;
			if(!flags[FindReplace::Param] && m_initialValues.command != CMD_NONE)
				m_settings.findParamMin = m_settings.findParamMax = m_initialValues.param;
		}
	}

	// Note
	{
		int sel = -1;
		m_cbnNote->setUpdatesEnabled(false);
		m_cbnNote->InitStorage(150, 150 * 6 * sizeof(mpt::uchar));
		mfcless::SetItemData(m_cbnNote, mfcless::AddItem(m_cbnNote, UL_("...")), NOTE_NONE);
		if (m_isReplaceTab)
		{
			mfcless::SetItemData(m_cbnNote, mfcless::AddItem(m_cbnNote, UL_("note -1")), kReplaceNoteMinusOne);
			mfcless::SetItemData(m_cbnNote, mfcless::AddItem(m_cbnNote, UL_("note +1")), kReplaceNotePlusOne);
			mfcless::SetItemData(m_cbnNote, mfcless::AddItem(m_cbnNote, UL_("-1 oct")), kReplaceNoteMinusOctave);
			mfcless::SetItemData(m_cbnNote, mfcless::AddItem(m_cbnNote, UL_("+1 oct")), kReplaceNotePlusOctave);
			mfcless::SetItemData(m_cbnNote, mfcless::AddItem(m_cbnNote, UL_("Transpose...")), kReplaceRelative);
			if(m_settings.replaceNoteAction == FindReplace::ReplaceRelative)
			{
				switch(m_settings.replaceNote)
				{
				case -1: sel = 1; break;
				case  1: sel = 2; break;
				case FindReplace::ReplaceOctaveDown: sel = 3; break;
				case FindReplace::ReplaceOctaveUp  : sel = 4; break;
				default: sel = 5; break;
				}
			}
		} else
		{
			mfcless::SetItemData(m_cbnNote, mfcless::AddItem(m_cbnNote, UL_("any")), kFindAny);
			mfcless::SetItemData(m_cbnNote, mfcless::AddItem(m_cbnNote, UL_("Range...")), kFindRange);
			if(m_settings.findNoteMin == NOTE_MIN && m_settings.findNoteMax == NOTE_MAX)
				sel = 1;
			else if(m_settings.findNoteMin < m_settings.findNoteMax)
				sel = 2;
		}
		AppendNotesToControlEx(m_cbnNote, m_sndFile);

		if(sel == -1)
		{
			uintptr_t searchNote = m_isReplaceTab ? m_settings.replaceNote : m_settings.findNoteMin;
			int ncount = m_cbnNote->count();
			for(int i = 0; i < ncount; i++) if(searchNote == mfcless::GetItemData(m_cbnNote, i))
			{
				sel = i;
				break;
			}
		}
		m_cbnNote->setCurrentIndex(sel);
		m_cbnNote->setUpdatesEnabled(true);
	}

	// Volume Command
	m_cbnVolCmd->setUpdatesEnabled(false);
	uint32 count = m_effectInfo.GetNumVolCmds();
	m_cbnVolCmd->InitStorage(count, static_cast<uint32>(count * 15 * sizeof(mpt::uchar)));
	mfcless::SetItemData(m_cbnVolCmd, mfcless::AddItem(m_cbnVolCmd, UL_(" None")), (uintptr_t)-1);
	for (uint32 n = 0; n < count; n++)
	{
		if(m_effectInfo.GetVolCmdInfo(n, &s) && !s.IsEmpty())
		{
			mfcless::SetItemData(m_cbnVolCmd, mfcless::AddItem(m_cbnVolCmd, s), n);
		}
	}
	m_cbnVolCmd->setCurrentIndex(0);
	uint32 fxndx = m_effectInfo.GetIndexFromVolCmd(m_isReplaceTab ? m_settings.replaceVolCmd : m_settings.findVolCmd);
	for (uint32 i=0; i<=count; i++) if (fxndx == mfcless::GetItemData(m_cbnVolCmd, i))
	{
		m_cbnVolCmd->setCurrentIndex(i);
		break;
	}
	m_cbnVolCmd->setUpdatesEnabled(true);
	m_cbnVolCmd->setVisible(true);
	m_cbnPCParam->setVisible(false);

	// Command
	{
		m_cbnCommand->setUpdatesEnabled(false);
		count = m_effectInfo.GetNumEffects();
		m_cbnCommand->InitStorage(count, static_cast<uint32>(count * 20 * sizeof(mpt::uchar)));
		mfcless::SetItemData(m_cbnCommand, mfcless::AddItem(m_cbnCommand, UL_(" None")), (uintptr_t)-1);
		for (uint32 n = 0; n < count; n++)
		{
			if(m_effectInfo.GetEffectInfo(n, &s, true) && !s.IsEmpty())
			{
				mfcless::SetItemData(m_cbnCommand, mfcless::AddItem(m_cbnCommand, s), n);
			}
		}
		m_cbnCommand->setCurrentIndex(0);
		fxndx = m_effectInfo.GetIndexFromEffect(m_isReplaceTab ? m_settings.replaceCommand : m_settings.findCommand, static_cast<ModCommand::PARAM>(m_isReplaceTab ? m_settings.replaceParam : m_settings.findParamMin));
		for (uint32 i=0; i<=count; i++) if (fxndx == mfcless::GetItemData(m_cbnCommand, i))
		{
			m_cbnCommand->setCurrentIndex(i);
			break;
		}
		m_cbnCommand->setUpdatesEnabled(true);
	}
	UpdateInstrumentList();
	UpdateVolumeList();
	UpdateParamList();
	OnCheckChannelSearch();
	return true;
}


bool CFindReplaceTab::IsPCEvent() const
{
	if(m_isReplaceTab)
	{
		if(m_settings.replaceFlags[FindReplace::Note] && m_settings.replaceNoteAction == FindReplace::ReplaceValue)
			return ModCommand::IsPcNote(static_cast<ModCommand::NOTE>(m_settings.replaceNote));
	}
	return ModCommand::IsPcNote(m_settings.findNoteMin);
}


void CFindReplaceTab::UpdateInstrumentList()
{
	const bool isPCEvent = IsPCEvent();
	if(m_cbnInstr->GetCount() != 0 && !!GetWindowLongPtr(m_cbnInstr, GWLP_USERDATA) == isPCEvent)
		return;
	SetWindowLongPtr(m_cbnInstr, GWLP_USERDATA, isPCEvent);

	int oldSelection = (m_isReplaceTab ? m_settings.replaceInstr : m_settings.findInstrMin);
	int sel = (oldSelection == 0) ? 0 : -1;
	m_cbnInstr->setUpdatesEnabled(false);
	m_cbnInstr->ResetContent();
	const int numItems = (isPCEvent ? MAX_MIXPLUGINS : MAX_INSTRUMENTS) + 3;
	m_cbnInstr->InitStorage(numItems, static_cast<uint32>(numItems * 32 * sizeof(mpt::uchar)));
	m_cbnInstr->SetItemData(m_cbnInstr->AddString(UL_("..")), 0);
	if (m_isReplaceTab)
	{
		m_cbnInstr->SetItemData(m_cbnInstr->AddString(isPCEvent ? UL_("Plugin -1") : UL_("Instrument -1")), kReplaceInstrumentMinusOne);
		m_cbnInstr->SetItemData(m_cbnInstr->AddString(isPCEvent ? UL_("Plugin +1") : UL_("Instrument +1")), kReplaceInstrumentPlusOne);
		m_cbnInstr->SetItemData(m_cbnInstr->AddString(UL_("Other...")), kReplaceRelative);
		if(m_settings.replaceInstrAction == FindReplace::ReplaceRelative)
		{
			switch(m_settings.replaceInstr)
			{
			case -1: sel = 1; break;
			case  1: sel = 2; break;
			default: sel = 3; break;
			}
		}
	} else
	{
		m_cbnInstr->SetItemData(m_cbnInstr->AddString(UL_("Range...")), kFindRange);
		if(m_settings.findInstrMin < m_settings.findInstrMax)
			sel = 1;
	}
	// Did we switch from an instrument index > MAX_MIXPLUGINS to plugin list?
	const bool updateInstr = (sel == -1 && isPCEvent && oldSelection > MAX_MIXPLUGINS);
	if(sel == -1)
		sel = m_cbnInstr->GetCount() + oldSelection - 1;

	if(isPCEvent)
	{
		m_cbnInstr->Update(PluginComboBox::Config{PluginComboBox::ShowEmptySlots | PluginComboBox::DoNotResetContent}, m_sndFile);
	} else
	{
		mpt::ustring s;
		for(INSTRUMENTINDEX n = 1; n < MAX_INSTRUMENTS; n++)
		{
			s = mfcless::PrintF(UL_("%03d:"), n);
			if(m_sndFile.GetNumInstruments())
				s += mpt::ToUnicode(m_sndFile.GetCharsetInternal(), m_sndFile.GetInstrumentName(n));
			else
				s += mpt::ToUnicode(m_sndFile.GetCharsetInternal(), m_sndFile.m_szNames[n]);
			m_cbnInstr->SetItemData(m_cbnInstr->AddString(s), n);
		}
	}
	m_cbnInstr->SetRawSelection(sel);
	m_cbnInstr->setUpdatesEnabled(true);
	m_cbnInstr->update(false);
	if(updateInstr)
		OnInstrChanged();
}


void CFindReplaceTab::UpdateParamList()
{
	const bool isPCEvent = IsPCEvent();
	if(m_cbnInstr->GetCount() == 0 || !!GetWindowLongPtr(m_cbnInstr, GWLP_USERDATA) != isPCEvent)
	{
		SetWindowLongPtr(m_cbnInstr, GWLP_USERDATA, isPCEvent);
	}

	int effectIndex = static_cast<int>(mfcless::GetItemData(m_cbnCommand, m_cbnCommand->currentIndex()));
	ModCommand::PARAM n = 0; // unused parameter adjustment
	ModCommand::COMMAND cmd = m_effectInfo.GetEffectFromIndex(effectIndex, n);
	const uint32 mask = m_effectInfo.GetEffectMaskFromIndex(effectIndex);
	if(m_isReplaceTab)
		m_settings.replaceCommand = cmd;
	else
		m_settings.findCommand = cmd;

	// Update Param range
	const bool isExtended = m_effectInfo.IsExtendedEffect(effectIndex);
	int sel = -1;
	int oldcount = m_cbnParam->count();
	int newcount = isExtended ? 16 : 256;
	if(oldcount)
		oldcount -= m_isReplaceTab ? 2 : 1;

	auto findParam = m_isReplaceTab ? m_settings.replaceParam : m_settings.findParamMin;
	if(isExtended)
	{
		findParam &= 0x0F;
		if(!m_isReplaceTab && !IsChecked(IDC_CHECK6))
		{
			m_settings.findParamMin = (m_settings.findParamMin & 0x0F) | mask;
			m_settings.findParamMax = (m_settings.findParamMax & 0x0F) | mask;
		} else if(m_isReplaceTab)
		{
			m_settings.replaceParam |= mask;
		}
	}

	if(oldcount != newcount)
	{
		mpt::uchar s[16];
		int newpos;
		if(oldcount && m_cbnParam->currentIndex() != CB_ERR)
			newpos = static_cast<int>(mfcless::GetItemData(m_cbnParam, m_cbnParam->currentIndex()));
		else
			newpos = findParam;
		Limit(newpos, 0, newcount - 1);
		m_cbnParam->setUpdatesEnabled(false);
		m_cbnParam->clear();
		m_cbnParam->InitStorage(newcount + 2, static_cast<uint32>((newcount + 2) * 4 * sizeof(mpt::uchar)));

		if(m_isReplaceTab)
		{
			wsprintf(s, UL_("+ %d"), m_settings.replaceParam);
			mfcless::SetItemData(m_cbnParam, mfcless::AddItem(m_cbnParam, s), kReplaceRelative);
			wsprintf(s, UL_("* %d%%"), m_settings.replaceParam);
			mfcless::SetItemData(m_cbnParam, mfcless::AddItem(m_cbnParam, s), kReplaceMultiply);
			if(m_settings.replaceParamAction == FindReplace::ReplaceRelative)
				sel = 0;
			else if(m_settings.replaceParamAction == FindReplace::ReplaceMultiply)
				sel = 1;

			m_settings.replaceParam = newpos;
			if(isExtended)
			{
				m_settings.replaceParam = (m_settings.replaceParam & 0x0F) | mask;
			}
		} else
		{
			mfcless::SetItemData(m_cbnParam, mfcless::AddItem(m_cbnParam, UL_("Range")), kFindRange);
			if(m_settings.findParamMin < m_settings.findParamMax)
				sel = 0;
		}

		if(sel == -1)
			sel = m_cbnParam->count() + newpos;
		for(int param = 0; param < newcount; param++)
		{
			wsprintf(s, (newcount == 256) ? UL_("%02X") : UL_("%X"), param);
			int i = mfcless::AddItem(m_cbnParam, s);
			mfcless::SetItemData(m_cbnParam, i, param);
		}
		m_cbnParam->setCurrentIndex(sel);
		m_cbnParam->setUpdatesEnabled(true);
		m_cbnParam->update(false);
	}
}


void CFindReplaceTab::UpdateVolumeList()
{
	mpt::uchar s[256];
	const bool isPCEvent = IsPCEvent();

	bool enable = isPCEvent ? false : true;
	EnableItem(IDC_CHECK5, enable);
	EnableItem(IDC_CHECK6, enable);
	m_cbnCommand->setEnabled(enable);
	m_cbnParam->setEnabled(enable);

	// Update plugin parameter list
	PLUGINDEX plug = PLUGINDEX_INVALID;
	int plugData = 0;
	if(int sel = m_cbnInstr->GetRawSelection(); sel >= 0 && m_cbnInstr->GetItemData(sel) < kBeginSpecial)
	{
		plug = m_cbnInstr->GetSelection().value_or(PLUGINDEX_INVALID);
		plugData = (plug != PLUGINDEX_INVALID) ? plug + 1 : 0;
	}
	if(isPCEvent && (m_cbnPCParam->count() == 0 || GetWindowLongPtr(m_cbnPCParam, GWLP_USERDATA) != plugData))
	{
		SetWindowLongPtr(m_cbnPCParam, GWLP_USERDATA, plugData);

		SetChecked(IDC_CHECK5, 0);
		SetChecked(IDC_CHECK6, 0);

		int sel = m_isReplaceTab ? m_settings.replaceParam : m_settings.findParamMin;
		m_cbnPCParam->setUpdatesEnabled(false);
		m_cbnPCParam->clear();
		if(plug < MAX_MIXPLUGINS && m_sndFile.m_MixPlugins[plug].pMixPlugin != nullptr)
		{
			AddPluginParameternamesToCombobox(m_cbnPCParam, *m_sndFile.m_MixPlugins[plug].pMixPlugin);
		} else
		{
			const int numItems = ModCommand::maxColumnValue + 1;
			m_cbnPCParam->InitStorage(numItems, static_cast<uint32>(numItems * 20 * sizeof(mpt::uchar)));
			for(int i = 0; i < numItems; i++)
			{
				wsprintf(s, UL_("%02u: Parameter %02u"), static_cast<unsigned int>(i), static_cast<unsigned int>(i));
				mfcless::SetItemData(m_cbnPCParam, mfcless::AddItem(m_cbnPCParam, s), i);
			}
		}
		m_cbnPCParam->setCurrentIndex(sel);
		m_cbnPCParam->setUpdatesEnabled(true);
		m_cbnPCParam->update(false);
	}

	m_cbnVolCmd->setVisible(isPCEvent ? mfcless::show::Hide : mfcless::show::Show);
	m_cbnPCParam->setVisible(isPCEvent ? mfcless::show::Show : mfcless::show::Hide);

	int rangeMin, rangeMax, curVal;

	if(isPCEvent)
	{
		rangeMin = 0;
		rangeMax = ModCommand::maxColumnValue;
		curVal = (m_isReplaceTab ? m_settings.replaceVolume : m_settings.findVolumeMin);
	} else
	{
		int effectIndex = static_cast<int>(mfcless::GetItemData(m_cbnVolCmd, m_cbnVolCmd->currentIndex()));
		ModCommand::VOLCMD cmd = m_effectInfo.GetVolCmdFromIndex(effectIndex);
		if(m_isReplaceTab)
			m_settings.replaceVolCmd = cmd;
		else
			m_settings.findVolCmd = cmd;

		// Update Param range
		ModCommand::VOL volMin, volMax;
		if(!m_effectInfo.GetVolCmdInfo(effectIndex, nullptr, &volMin, &volMax))
		{
			volMin = 0;
			volMax = 64;
		}
		rangeMin = volMin;
		rangeMax = volMax;
		curVal = (m_isReplaceTab ? m_settings.replaceVolume : m_settings.findVolumeMin);
	}

	int oldcount = m_cbnVolume->count();
	int newcount = rangeMax - rangeMin + 1;
	if (oldcount != newcount)
	{
		int sel = -1;
		int newpos;
		if (oldcount)
			newpos = static_cast<int>(mfcless::GetItemData(m_cbnVolume, m_cbnVolume->currentIndex()));
		else
			newpos = curVal;
		Limit(newpos, 0, newcount - 1);
		m_cbnVolume->setUpdatesEnabled(false);
		m_cbnVolume->clear();
		m_cbnVolume->InitStorage(newcount + 2, static_cast<uint32>((newcount + 2) * 4 * sizeof(mpt::uchar)));
		if(m_isReplaceTab)
		{
			wsprintf(s, UL_("+ %d"), m_settings.replaceVolume);
			mfcless::SetItemData(m_cbnVolume, mfcless::AddItem(m_cbnVolume, s), kReplaceRelative);
			wsprintf(s, UL_("* %d%%"), m_settings.replaceVolume);
			mfcless::SetItemData(m_cbnVolume, mfcless::AddItem(m_cbnVolume, s), kReplaceMultiply);
			if(m_settings.replaceVolumeAction == FindReplace::ReplaceRelative)
				sel = 0;
			else if(m_settings.replaceVolumeAction == FindReplace::ReplaceMultiply)
				sel = 1;
		} else
		{
			mfcless::SetItemData(m_cbnVolume, mfcless::AddItem(m_cbnVolume, UL_("Range...")), kFindRange);
			if(m_settings.findVolumeMin < m_settings.findVolumeMax)
				sel = 0;
		}

		if(sel == -1)
			sel = m_cbnVolume->count() + newpos - rangeMin;
		for (int vol = rangeMin; vol <= rangeMax; vol++)
		{
			wsprintf(s, (rangeMax < 10 || rangeMax > 99) ? UL_("%d") : UL_("%02d"), vol);
			int i = mfcless::AddItem(m_cbnVolume, s);
			mfcless::SetItemData(m_cbnVolume, i, vol);
		}
		m_cbnVolume->setCurrentIndex(sel);
		m_cbnVolume->setUpdatesEnabled(true);
		m_cbnVolume->update(false);
	}
}


void CFindReplaceTab::OnNoteChanged()
{
	CheckOnChange(IDC_CHECK1);
	int item = static_cast<int>(mfcless::GetItemData(m_cbnNote, m_cbnNote->currentIndex()));
	if(m_isReplaceTab)
	{
		m_settings.replaceNoteAction = FindReplace::ReplaceRelative;
		switch(item)
		{
		case kReplaceNoteMinusOne:    m_settings.replaceNote = -1; break;
		case kReplaceNotePlusOne:     m_settings.replaceNote =  1; break;
		case kReplaceNoteMinusOctave: m_settings.replaceNote = FindReplace::ReplaceOctaveDown; break;
		case kReplaceNotePlusOctave:  m_settings.replaceNote = FindReplace::ReplaceOctaveUp; break;

		case kReplaceRelative:
			{
				CInputDlg dlg(this, UL_("Custom Transpose Amount:"), -120, 120, m_settings.replaceNote);
				if(dlg.DoModal() == IDOK)
				{
					m_settings.replaceNote = dlg.resultAsInt;
				} else
				{
					// TODO undo selection
				}
			}
			break;

		default:
			m_settings.replaceNote = item;
			m_settings.replaceNoteAction = FindReplace::ReplaceValue;
		}
	} else
	{
		if(item == kFindRange)
		{
			CFindRangeDlg dlg(this, NOTE_MIN, m_settings.findNoteMin, NOTE_MAX, m_settings.findNoteMax, CFindRangeDlg::kNotes);
			if(dlg.DoModal() == IDOK)
			{
				m_settings.findNoteMin = static_cast<ModCommand::NOTE>(dlg.GetMinVal());
				m_settings.findNoteMax = static_cast<ModCommand::NOTE>(dlg.GetMaxVal());
			}
		} else if(item == kFindAny)
		{
			m_settings.findNoteMin = NOTE_MIN;
			m_settings.findNoteMax = NOTE_MAX;
		} else
		{
			m_settings.findNoteMin = m_settings.findNoteMax = static_cast<ModCommand::NOTE>(item);
		}
	}
	UpdateInstrumentList();
	UpdateVolumeList();
}


void CFindReplaceTab::OnInstrChanged()
{
	CheckOnChange(IDC_CHECK2);
	int item = static_cast<int>(m_cbnInstr->GetItemData(m_cbnInstr->GetRawSelection()));
	if(item < kBeginSpecial && IsPCEvent())
	{
		if(auto sel = m_cbnInstr->GetSelection(); sel)
			item = *sel + 1;
		else
			item = 0;
	}

	if(m_isReplaceTab)
	{
		m_settings.replaceInstrAction = FindReplace::ReplaceRelative;
		switch(item)
		{
		case kReplaceInstrumentMinusOne: m_settings.replaceInstr = -1; break;
		case kReplaceInstrumentPlusOne:  m_settings.replaceInstr =  1; break;

		case kReplaceRelative:
			{
				CInputDlg dlg(this, UL_("Custom Replacement Amount:"), -255, 255, m_settings.replaceInstr);
				if(dlg.DoModal() == IDOK)
				{
					m_settings.replaceInstrAction = FindReplace::ReplaceRelative;
					m_settings.replaceInstr = dlg.resultAsInt;
				} else
				{
					// TODO undo selection
				}
			}
			break;

		default:
			m_settings.replaceInstrAction = FindReplace::ReplaceValue;
			m_settings.replaceInstr = item;
			break;
		}
	} else
	{
		if(item == kFindRange)
		{
			CFindRangeDlg dlg(this, 1, m_settings.findInstrMin, MAX_INSTRUMENTS - 1, m_settings.findInstrMax, CFindRangeDlg::kDecimal);
			if(dlg.DoModal() == IDOK)
			{
				m_settings.findInstrMin = static_cast<ModCommand::INSTR>(dlg.GetMinVal());
				m_settings.findInstrMax = static_cast<ModCommand::INSTR>(dlg.GetMaxVal());
			}
		} else
		{
			m_settings.findInstrMin = m_settings.findInstrMax = static_cast<ModCommand::INSTR>(item);
		}
	}
	if(IsPCEvent())
		UpdateVolumeList();
}


void CFindReplaceTab::OnVolCmdChanged()
{
	CheckOnChange(IDC_CHECK3);
	UpdateVolumeList();
};


void CFindReplaceTab::RelativeOrMultiplyPrompt(CComboBox &comboBox, FindReplace::ReplaceMode &action, int &value, int range, bool isHex)
{
	int sel = comboBox.GetCurSel();
	int item = static_cast<int>(comboBox.GetItemData(sel));

	if(sel == CB_ERR)
	{
		item = 0;
		mpt::ustring s;
		mfcless::GetWindowText(comboBox, s);
		s.TrimLeft();
		if(s.GetLength() >= 1)
		{
			mpt::uchar first = s[0];
			if(first == u8'+')
			{
				item = kReplaceRelative;
				sel = 0;
			} else if(first == u8'*')
			{
				item = kReplaceMultiply;
				sel = 1;
			}
		}
		if(!item)
		{
			if(isHex)
			{
				int len = ::GetWindowTextLengthA(m_cbnParam);
				std::string sHex(len, 0);
				::GetWindowTextA(m_cbnParam, &sHex[0], len + 1);
				item = mpt::parse_hex<unsigned int>(sHex);
			} else
			{
				item = mpt::parse<int>(s);
			}
		}
	}

	if(item == kReplaceRelative || item == kReplaceMultiply)
	{
		const mpt::uchar *prompt, *format;
		FindReplace::ReplaceMode act;
		if(item == kReplaceRelative)
		{
			act = FindReplace::ReplaceRelative;
			prompt = UL_("Amount to add or subtract:");
			format = UL_("+ %d");
		} else
		{
			act = FindReplace::ReplaceMultiply;
			prompt = UL_("Multiply by percentage:");
			format = UL_("* %d%%");
		}

		range *= 100;
		CInputDlg dlg(this, prompt, -range, range, value);
		if(dlg.DoModal() == IDOK)
		{
			value = dlg.resultAsInt;
			action = act;

			mpt::uchar s[32];
			wsprintf(s, format, value);
			comboBox.DeleteString(sel);
			comboBox.InsertString(sel, s);
			comboBox.SetItemData(sel, item);
			comboBox.SetCurSel(sel);
		} else
		{
			// TODO undo selection
		}
	} else
	{
		action = FindReplace::ReplaceValue;
		value = item;
	}
}


void CFindReplaceTab::OnVolumeChanged()
{
	CheckOnChange(IDC_CHECK4);
	int item = m_cbnVolume->currentIndex();
	if(item != CB_ERR)
		item = static_cast<int>(mfcless::GetItemData(m_cbnVolume, item));
	else
		item = GetItemInt(IDC_COMBO4);

	int rangeMax = IsPCEvent() ? ModCommand::maxColumnValue : 64;
	if(m_isReplaceTab)
	{
		RelativeOrMultiplyPrompt(m_cbnVolume, m_settings.replaceVolumeAction, m_settings.replaceVolume, rangeMax, false);
	} else
	{
		if(item == kFindRange)
		{
			CFindRangeDlg dlg(this, 0, m_settings.findVolumeMin, rangeMax, m_settings.findVolumeMax, CFindRangeDlg::kDecimal);
			if(dlg.DoModal() == IDOK)
			{
				m_settings.findVolumeMin = dlg.GetMinVal();
				m_settings.findVolumeMax = dlg.GetMaxVal();
			} else
			{
				// TODO undo selection
			}
		} else
		{
			m_settings.findVolumeMin = m_settings.findVolumeMax = item;
		}
	}
}


void CFindReplaceTab::OnEffectChanged()
{
	CheckOnChange(IDC_CHECK5);
	UpdateParamList();
};


void CFindReplaceTab::OnParamChanged()
{
	CheckOnChange(IDC_CHECK6);
	int item = m_cbnParam->currentIndex();
	if(item != CB_ERR)
	{
		item = static_cast<int>(mfcless::GetItemData(m_cbnParam, item));
	} else
	{
		int len = ::GetWindowTextLengthA(m_cbnParam);
		std::string s(len, 0);
		::GetWindowTextA(m_cbnParam, &s[0], len + 1);
		item = mpt::parse_hex<unsigned int>(s);
	}

	// Apply parameter value mask if required (e.g. SDx has mask D0).
	int effectIndex = static_cast<int>(mfcless::GetItemData(m_cbnCommand, m_cbnCommand->currentIndex()));
	uint32 mask = (effectIndex > -1) ? m_effectInfo.GetEffectMaskFromIndex(effectIndex) : 0;

	if(m_isReplaceTab)
	{
		RelativeOrMultiplyPrompt(m_cbnParam, m_settings.replaceParamAction, m_settings.replaceParam, 256, true);
		if(m_settings.replaceParamAction == FindReplace::ReplaceValue && effectIndex > -1)
		{
			m_settings.replaceParam |= mask;
		}
	} else
	{
		if(item == kFindRange)
		{
			CFindRangeDlg dlg(this, 0, m_settings.findParamMin & ~mask, m_cbnParam->count() - 2, m_settings.findParamMax & ~mask, CFindRangeDlg::kHex);
			if(dlg.DoModal() == IDOK)
			{
				m_settings.findParamMin = dlg.GetMinVal() | mask;
				m_settings.findParamMax = dlg.GetMaxVal() | mask;
			} else
			{
				// TODO undo selection
			}
		} else
		{
			m_settings.findParamMin = m_settings.findParamMax = (item | mask);
		}
	}
}


void CFindReplaceTab::OnPCParamChanged()
{
	CheckOnChange(IDC_CHECK3);
	int item = static_cast<int>(mfcless::GetItemData(m_cbnPCParam, m_cbnPCParam->currentIndex()));

	if(m_isReplaceTab)
	{
		RelativeOrMultiplyPrompt(m_cbnPCParam, m_settings.replaceParamAction, m_settings.replaceParam, 256, false);
	} else
	{
		if(item == kFindRange)
		{
			CFindRangeDlg dlg(this, 0, m_settings.findParamMin, ModCommand::maxColumnValue, m_settings.findParamMax, CFindRangeDlg::kDecimal);
			if(dlg.DoModal() == IDOK)
			{
				m_settings.findParamMin = dlg.GetMinVal();
				m_settings.findParamMax = dlg.GetMaxVal();
			} else
			{
				// TODO undo selection
			}
		} else
		{
			m_settings.findParamMin = m_settings.findParamMax = item;
		}
	}
}


void CFindReplaceTab::OnCheckNote()
{
	CheckReplace(IDC_CHECK1);
	if(m_isReplaceTab && IsPCEvent() != !!GetWindowLongPtr(m_cbnInstr, GWLP_USERDATA))
	{
		UpdateInstrumentList();
		UpdateVolumeList();
	}
}

void CFindReplaceTab::OnCheckInstr() { CheckReplace(IDC_CHECK2); }
void CFindReplaceTab::OnCheckVolCmd() { CheckReplace(IDC_CHECK3); }
void CFindReplaceTab::OnCheckVolume() { CheckReplace(IDC_CHECK4); }
void CFindReplaceTab::OnCheckEffect() { CheckReplace(IDC_CHECK5); }
void CFindReplaceTab::OnCheckParam() { CheckReplace(IDC_CHECK6); }


void CFindReplaceTab::CheckReplace(int buttonID)
{
	if(m_isReplaceTab && IsChecked(buttonID))
		SetChecked(IDC_CHECK7, 1);

	if(buttonID == IDC_CHECK1)
	{
		FlagSet<FindReplace::Flags> &flags = m_isReplaceTab ? m_settings.replaceFlags : m_settings.findFlags;
		flags.set(FindReplace::Note, !!IsChecked(IDC_CHECK1));
	}
};


void CFindReplaceTab::OnCheckChannelSearch()
{
	if (!m_isReplaceTab)
	{
		bool b = IsChecked(IDC_CHECK7);
		EnableItem(IDC_EDIT1, b);
		EnableItem(IDC_SPIN1, b);
		EnableItem(IDC_EDIT2, b);
		EnableItem(IDC_SPIN2, b);
	}
}


void CFindReplaceTab::OnOK()
{
	// Search flags
	FlagSet<FindReplace::Flags> &flags = m_isReplaceTab ? m_settings.replaceFlags : m_settings.findFlags;
	flags.reset();
	flags.set(FindReplace::Note, !!IsChecked(IDC_CHECK1));
	flags.set(FindReplace::Instr, !!IsChecked(IDC_CHECK2));
	if(IsPCEvent())
	{
		flags.set(FindReplace::PCParam, !!IsChecked(IDC_CHECK3));
		flags.set(FindReplace::PCValue, !!IsChecked(IDC_CHECK4));
	} else
	{
		flags.set(FindReplace::VolCmd, !!IsChecked(IDC_CHECK3));
		flags.set(FindReplace::Volume, !!IsChecked(IDC_CHECK4));
		flags.set(FindReplace::Command, !!IsChecked(IDC_CHECK5));
		flags.set(FindReplace::Param, !!IsChecked(IDC_CHECK6));
	}
	if(m_isReplaceTab)
	{
		flags.set(FindReplace::Replace, !!IsChecked(IDC_CHECK7));
		flags.set(FindReplace::ReplaceAll, !!IsChecked(IDC_CHECK8));
	} else
	{
		flags.set(FindReplace::InChannels, !!IsChecked(IDC_CHECK7));
		flags.set(FindReplace::FullSearch, !!IsChecked(IDC_RADIO2));
		flags.set(FindReplace::InPatSelection, !!IsChecked(IDC_RADIO3));
	}

	// Min/Max channels
	if (!m_isReplaceTab)
	{
		m_settings.findChnMin = static_cast<CHANNELINDEX>(GetItemInt(IDC_EDIT1) - 1);
		m_settings.findChnMax = static_cast<CHANNELINDEX>(GetItemInt(IDC_EDIT2) - 1);
		if (m_settings.findChnMax < m_settings.findChnMin)
		{
			std::swap(m_settings.findChnMin, m_settings.findChnMax);
		}
	}
	PropertyPage::OnOK();
}

OPENMPT_NAMESPACE_END
