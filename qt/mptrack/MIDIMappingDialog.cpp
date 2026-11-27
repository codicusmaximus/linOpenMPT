/*
 * MIDIMappingDialog.cpp
 * ---------------------
 * Purpose: Implementation of OpenMPT's MIDI mapping dialog, for mapping incoming MIDI messages to plugin parameters.
 * Notes  : mfcless twin of upstream's mptrack/MIDIMappingDialog.cpp.
 *          Draft from qt/tools/twin.py, not yet edited by hand.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "MIDIMappingDialog.h"
#include "InputHandler.h"
#include "Mainfrm.h"
#include "Moddoc.h"
#include "Mptrack.h"
#include "Reporting.h"
#include "resource.h"
#include "WindowMessages.h"
#include "../common/mptStringBuffer.h"
#include "../soundlib/MIDIEvents.h"
#include "../soundlib/mod_specifications.h"
#include "../soundlib/plugins/PlugInterface.h"

#ifndef NO_PLUGINS

OPENMPT_NAMESPACE_BEGIN


CMIDIMappingDialog::CMIDIMappingDialog(QWidget *pParent, CSoundFile &sndfile)
	: ResizableDialog{IDD_MIDIPARAMCONTROL, pParent}
	, m_sndFile{sndfile}
	, m_rMIDIMapper{m_sndFile.GetMIDIMapper()}
{
	CMainFrame::GetInputHandler()->Bypass(true);
	oldMIDIRecondWnd = CMainFrame::GetMainFrame()->GetMidiRecordWnd();
}


CMIDIMappingDialog::~CMIDIMappingDialog()
{
	CMainFrame::GetMainFrame()->SetMidiRecordWnd(oldMIDIRecondWnd);
	CMainFrame::GetInputHandler()->Bypass(false);
}


void CMIDIMappingDialog::DoDataExchange(DataExchange &dx)
{
	ResizableDialog::DoDataExchange(dx);
	ExchangeControl(dx, IDC_COMBO_CONTROLLER, m_ControllerCBox);
	ExchangeControl(dx, IDC_COMBO_PLUGIN, m_PluginCBox);
	ExchangeControl(dx, IDC_COMBO_PARAM, m_PlugParamCBox);
	ExchangeControl(dx, IDC_LIST1, m_List);
	ExchangeControl(dx, IDC_COMBO_CHANNEL, m_ChannelCBox);
	ExchangeControl(dx, IDC_COMBO_EVENT, m_EventCBox);
	ExchangeControl(dx, IDC_SPINMOVEMAPPING, m_SpinMoveMapping);
}


void CMIDIMappingDialog::RegisterMessageMap()
{
	ResizableDialog::RegisterMessageMap();
	AddNotify(IDC_LIST1, mfcless::Notify::ItemChanged, &CMIDIMappingDialog::OnSelectionChanged);
	AddCommand(IDC_CHECKACTIVE, &CMIDIMappingDialog::OnBnClickedCheckactive);
	AddCommand(IDC_CHECKCAPTURE, &CMIDIMappingDialog::OnBnClickedCheckCapture);
	AddNotify(IDC_COMBO_CONTROLLER, mfcless::Notify::SelChange, &CMIDIMappingDialog::OnCbnSelchangeComboController);
	AddNotify(IDC_COMBO_CHANNEL, mfcless::Notify::SelChange, &CMIDIMappingDialog::OnCbnSelchangeComboChannel);
	AddNotify(IDC_COMBO_PLUGIN, mfcless::Notify::SelChange, &CMIDIMappingDialog::OnCbnSelchangeComboPlugin);
	AddNotify(IDC_COMBO_PARAM, mfcless::Notify::SelChange, &CMIDIMappingDialog::OnCbnSelchangeComboParam);
	AddNotify(IDC_COMBO_EVENT, mfcless::Notify::SelChange, &CMIDIMappingDialog::OnCbnSelchangeComboEvent);
	AddCommand(IDC_BUTTON_ADD, &CMIDIMappingDialog::OnBnClickedButtonAdd);
	AddCommand(IDC_BUTTON_REPLACE, &CMIDIMappingDialog::OnBnClickedButtonReplace);
	AddCommand(IDC_BUTTON_REMOVE, &CMIDIMappingDialog::OnBnClickedButtonRemove);
	AddMessage(WM_MOD_MIDIMSG, &CMIDIMappingDialog::OnMidiMsg);
	AddNotify(IDC_SPINMOVEMAPPING, mfcless::Notify::DeltaPos, &CMIDIMappingDialog::OnDeltaposSpinmovemapping);
	AddCommand(IDC_CHECK_PATRECORD, &CMIDIMappingDialog::OnBnClickedCheckPatRecord);
}


intptr_t CMIDIMappingDialog::OnMidiMsg(uintptr_t dwMidiDataParam, intptr_t)
{
	uint32 midiData = static_cast<uint32>(dwMidiDataParam);
	if(IsChecked(IDC_CHECK_MIDILEARN))
	{
		for(int i = 0; i < m_EventCBox->count(); i++)
		{
			if(static_cast<MIDIEvents::EventType>(mfcless::GetItemData(m_EventCBox, i)) == MIDIEvents::GetTypeFromEvent(midiData))
			{
				m_ChannelCBox->setCurrentIndex(1 + MIDIEvents::GetChannelFromEvent(midiData));
				m_EventCBox->setCurrentIndex(i);
				if(MIDIEvents::GetTypeFromEvent(midiData) == MIDIEvents::evControllerChange)
				{
					uint8 cc = MIDIEvents::GetDataByte1FromEvent(midiData);
					if(m_lastCC >= 32 || cc != m_lastCC + 32)
					{
						// Ignore second CC message of 14-bit CC.
						m_ControllerCBox->setCurrentIndex(cc);
					}
					m_lastCC = cc;
				}
				OnCbnSelchangeComboChannel();
				OnCbnSelchangeComboEvent();
				OnCbnSelchangeComboController();
				break;
			}
		}
	}
	return 1;
}


bool CMIDIMappingDialog::OnInitDialog()
{
	ResizableDialog::OnInitDialog();

	// Add events
	mfcless::SetItemData(m_EventCBox, mfcless::AddItem(m_EventCBox, U_("Controller Change")), MIDIEvents::evControllerChange);
	mfcless::SetItemData(m_EventCBox, mfcless::AddItem(m_EventCBox, U_("Polyphonic Aftertouch")), MIDIEvents::evPolyAftertouch);
	mfcless::SetItemData(m_EventCBox, mfcless::AddItem(m_EventCBox, U_("Channel Aftertouch")), MIDIEvents::evChannelAftertouch);
	
	// Add controller names
	mpt::ustring s;
	for(uint8 i = MIDIEvents::MIDICC_start; i <= MIDIEvents::MIDICC_end; i++)
	{
		s.Format(U_("%3u "), i);
		s += mpt::ToUnicode(mpt::Charset::UTF8, MIDIEvents::MidiCCNames[i]);
		mfcless::AddItem(m_ControllerCBox, s);
	}

	// Add plugin names
	m_PluginCBox->Update(PluginComboBox::Config{PluginComboBox::ShowEmptySlots}, m_sndFile);

	// Initialize mapping table
	static constexpr CListCtrlEx::Header headers[] =
	{
		{ U_("Channel"),            58,  LVCFMT_LEFT },
		{ U_("Event / Controller"), 176, LVCFMT_LEFT },
		{ U_("Plugin"),             120, LVCFMT_LEFT },
		{ U_("Parameter"),          120, LVCFMT_LEFT },
		{ U_("Capture"),            40,  LVCFMT_LEFT },
		{ U_("Pattern Record"),     40,  LVCFMT_LEFT }
	};
	m_List->SetHeaders(headers);
	m_List->SetExtendedStyle(m_List->GetExtendedStyle() | LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT);

	// Add directives to list
	for(size_t i = 0; i < m_rMIDIMapper.GetCount(); i++)
	{
		InsertItem(m_rMIDIMapper.GetDirective(i), int(i));
	}

	if(m_rMIDIMapper.GetCount() > 0 && m_Setting.IsDefault())
	{
		SelectItem(0);
		OnSelectionChanged();
	} else
	{
		UpdateDialog();
	}

	EnableItem(IDC_CHECK_PATRECORD, (m_sndFile.GetType() == MOD_TYPE_MPT) ? true : false);

	CMainFrame::GetMainFrame()->SetMidiRecordWnd(GetSafeHwnd());

	SetChecked(IDC_CHECK_MIDILEARN, 1);

	return true;  // return true unless you set the focus to a control
}


int CMIDIMappingDialog::InsertItem(const CMIDIMappingDirective &m, int insertAt)
{
	mpt::ustring s;
	if(m.GetAnyChannel())
		s = U_("Any");
	else
		s.Format(U_("Ch %u"), m.GetChannel());

	insertAt = m_List->InsertItem(insertAt, s);
	if(insertAt == -1)
		return -1;
	m_List->SetCheck(insertAt, m.IsActive() ? true : false);

	switch(m.GetEvent())
	{
	case MIDIEvents::evControllerChange:
		s.Format(U_("CC %u: "), m.GetController());
		if(m.GetController() <= MIDIEvents::MIDICC_end) s += mpt::ToUnicode(mpt::Charset::UTF8, MIDIEvents::MidiCCNames[m.GetController()]);
		break;
	case MIDIEvents::evPolyAftertouch:
		s = U_("Polyphonic Aftertouch"); break;
	case MIDIEvents::evChannelAftertouch:
		s = U_("Channel Aftertouch"); break;
	default:
		s.Format(U_("0x%02X"), m.GetEvent()); break;
	}
	m_List->SetItemText(insertAt, 1, s);

	const PLUGINDEX plugindex = m.GetPlugIndex();
	if(plugindex > 0 && plugindex < MAX_MIXPLUGINS)
	{
		const SNDMIXPLUGIN &plug = m_sndFile.m_MixPlugins[plugindex - 1];
		s.Format(U_("FX%u: "), plugindex);
		s += mpt::ToUnicode(plug.GetName());
		m_List->SetItemText(insertAt, 2, s);
		if(plug.pMixPlugin != nullptr)
			s = plug.pMixPlugin->GetFormattedParamName(m.GetParamIndex());
		else
			s.Empty();
		m_List->SetItemText(insertAt, 3, s);
	}
	m_List->SetItemText(insertAt, 4, m.GetCaptureMIDI() ? U_("Capt") : U_(""));
	m_List->SetItemText(insertAt, 5, m.GetAllowPatternEdit() ? U_("Rec") : U_(""));

	return insertAt;
}


void CMIDIMappingDialog::SelectItem(int i)
{
	m_List->SetItemState(i, LVIS_SELECTED, LVIS_SELECTED);
	m_List->SetSelectionMark(i);
}


void CMIDIMappingDialog::UpdateDialog(int selItem)
{
	SetChecked(IDC_CHECKACTIVE, m_Setting.IsActive() ? 1 : 0);
	SetChecked(IDC_CHECKCAPTURE, m_Setting.GetCaptureMIDI() ? 1 : 0);
	SetChecked(IDC_CHECK_PATRECORD, m_Setting.GetAllowPatternEdit() ? 1 : 0);

	m_ChannelCBox->setCurrentIndex(m_Setting.GetChannel());

	m_EventCBox->setCurrentIndex(-1);
	for(int i = 0; i < m_EventCBox->count(); i++)
	{
		if(mfcless::GetItemData(m_EventCBox, i) == m_Setting.GetEvent())
		{
			m_EventCBox->setCurrentIndex(i);
			break;
		}
	}

	m_ControllerCBox->setCurrentIndex(m_Setting.GetController());
	if(PLUGINDEX plug = m_Setting.GetPlugIndex(); plug > 0)
		m_PluginCBox->SetSelection(plug - 1);
	else
		m_PluginCBox->SetSelection(PLUGINDEX_INVALID);
	m_PlugParamCBox->setCurrentIndex(m_Setting.GetParamIndex());

	UpdateEvent();
	UpdateParameters();

	bool enableMover = selItem >= 0;
	if(enableMover)
	{
		const bool previousEqual = (selItem > 0 && m_rMIDIMapper.AreOrderEqual(selItem - 1, selItem));
		const bool nextEqual = (selItem + 1 < m_List->GetItemCount() && m_rMIDIMapper.AreOrderEqual(selItem, selItem + 1));
		enableMover = previousEqual || nextEqual;
	}
	m_SpinMoveMapping->setEnabled(enableMover);
}


void CMIDIMappingDialog::UpdateEvent()
{
	m_ControllerCBox->setEnabled(m_Setting.GetEvent() == MIDIEvents::evControllerChange ? true : false);
	if(m_Setting.GetEvent() != MIDIEvents::evControllerChange)
		m_ControllerCBox->setCurrentIndex(0);
}


void CMIDIMappingDialog::UpdateParameters()
{
	m_PlugParamCBox->setUpdatesEnabled(false);
	m_PlugParamCBox->clear();
	AddPluginParameternamesToCombobox(m_PlugParamCBox, m_sndFile.m_MixPlugins[m_Setting.GetPlugIndex() - 1]);
	m_PlugParamCBox->setCurrentIndex(m_Setting.GetParamIndex());
	m_PlugParamCBox->setUpdatesEnabled(true);
	m_PlugParamCBox->update();
}


void CMIDIMappingDialog::OnSelectionChanged()
{
	int i;
	if(pNMHDR != nullptr)
	{
		// cppcheck-suppress dangerousTypeCast
		NMLISTVIEW *nmlv = (NMLISTVIEW *)pNMHDR;

		if(((nmlv->uOldState ^ nmlv->uNewState) & INDEXTOSTATEIMAGEMASK(3)) != 0 && nmlv->uOldState != 0)
		{
			// Check box status changed
			CMIDIMappingDirective m = m_rMIDIMapper.GetDirective(nmlv->iItem);
			m.SetActive(nmlv->uNewState == INDEXTOSTATEIMAGEMASK(2));
			m_rMIDIMapper.SetDirective(nmlv->iItem, m);
			SetModified();
			if(nmlv->iItem == m_List->GetSelectionMark())
				SetChecked(IDC_CHECKACTIVE, nmlv->uNewState == INDEXTOSTATEIMAGEMASK(2) ? 1 : 0);
		}

		if(nmlv->uNewState & LVIS_SELECTED)
			i = nmlv->iItem;
		else
			return;
	} else
	{
		i = m_List->GetSelectionMark();
	}
	if(i < 0 || (size_t)i >= m_rMIDIMapper.GetCount()) return;
	m_Setting = m_rMIDIMapper.GetDirective(i);
	UpdateDialog(i);
}


void CMIDIMappingDialog::OnBnClickedCheckactive()
{
	m_Setting.SetActive(IsChecked(IDC_CHECKACTIVE) == 1);
}


void CMIDIMappingDialog::OnBnClickedCheckCapture()
{
	m_Setting.SetCaptureMIDI(IsChecked(IDC_CHECKCAPTURE) == 1);
}


void CMIDIMappingDialog::OnBnClickedCheckPatRecord()
{
	m_Setting.SetAllowPatternEdit(IsChecked(IDC_CHECK_PATRECORD) == 1);
}


void CMIDIMappingDialog::OnCbnSelchangeComboController()
{
	m_Setting.SetController(m_ControllerCBox->currentIndex());
}


void CMIDIMappingDialog::OnCbnSelchangeComboChannel()
{
	m_Setting.SetChannel(m_ChannelCBox->currentIndex());
}


void CMIDIMappingDialog::OnCbnSelchangeComboPlugin()
{
	PLUGINDEX i = m_PluginCBox->GetSelection().value_or(PLUGINDEX_INVALID);
	if(i >= MAX_MIXPLUGINS)
		return;
	m_Setting.SetPlugIndex(i + 1);
	UpdateParameters();
}


void CMIDIMappingDialog::OnCbnSelchangeComboParam()
{
	m_Setting.SetParamIndex(m_PlugParamCBox->currentIndex());
}


void CMIDIMappingDialog::OnCbnSelchangeComboEvent()
{
	uint8 eventType = static_cast<uint8>(mfcless::GetItemData(m_EventCBox, m_EventCBox->currentIndex()));
	m_Setting.SetEvent(eventType);
	UpdateEvent();
}


void CMIDIMappingDialog::OnBnClickedButtonAdd()
{
	if(m_sndFile.GetModSpecifications().MIDIMappingDirectivesMax <= m_rMIDIMapper.GetCount())
	{
		Reporting::Information("Maximum amount of MIDI Mapping directives reached.");
	} else
	{
		const size_t i = m_rMIDIMapper.AddDirective(m_Setting);
		SetModified();

		SelectItem(InsertItem(m_Setting, static_cast<int>(i)));
		OnSelectionChanged();
	}
}


void CMIDIMappingDialog::OnBnClickedButtonReplace()
{
	const int i = m_List->GetSelectionMark();
	if(i >= 0 && (size_t)i < m_rMIDIMapper.GetCount())
	{
		const size_t newIndex = m_rMIDIMapper.SetDirective(i, m_Setting);
		SetModified();

		m_List->DeleteItem(i);
		SelectItem(InsertItem(m_Setting, static_cast<int>(newIndex)));
		OnSelectionChanged();
	}
}


void CMIDIMappingDialog::OnBnClickedButtonRemove()
{
	int i = m_List->GetSelectionMark();
	if(i >= 0 && (size_t)i < m_rMIDIMapper.GetCount())
	{
		m_rMIDIMapper.RemoveDirective(i);
		SetModified();

		m_List->DeleteItem(i);
		if(m_List->GetItemCount() > 0)
		{
			if(i < m_List->GetItemCount())
				SelectItem(i);
			else
				SelectItem(i - 1);
		}
		i = m_List->GetSelectionMark();
		if(i >= 0 && (size_t)i < m_rMIDIMapper.GetCount())
			m_Setting = m_rMIDIMapper.GetDirective(i);

		OnSelectionChanged();
	}
}


void CMIDIMappingDialog::OnDeltaposSpinmovemapping()
{
	const int index = m_List->GetSelectionMark();
	if(index < 0 || index >= m_List->GetItemCount()) return;

	LPNMUPDOWN pNMUpDown = reinterpret_cast<LPNMUPDOWN>(pNMHDR);

	int newIndex = -1;
	if(CurrentNotify().delta < 0) //Up
	{
		if(index - 1 >= 0 && m_rMIDIMapper.AreOrderEqual(index-1, index))
		{
			newIndex = index - 1;
		}
	} else //Down
	{
		if(index + 1 < m_List->GetItemCount() && m_rMIDIMapper.AreOrderEqual(index, index+1))
		{
			newIndex = index + 1;
		}
	}

	if(newIndex != -1)
	{
		m_rMIDIMapper.Swap(size_t(newIndex), size_t(index));
		m_List->DeleteItem(index);
		InsertItem(m_rMIDIMapper.GetDirective(newIndex), newIndex);
		SelectItem(newIndex);
	}
	
	CurrentNotify().result = 0;
}


mpt::ustring CMIDIMappingDialog::GetToolTipText(uint32 id, QWidget *) const
{
	const mpt::uchar *text = U_("");
	switch(id)
	{
	case IDC_CHECKCAPTURE:
		text = U_("The event is not passed to any further MIDI mappings or recording facilities.");
		break;
	case IDC_CHECKACTIVE:
		text = U_("The MIDI mapping is enabled and can be processed.");
		break;
	case IDC_CHECK_PATRECORD:
		text = U_("Parameter changes are recorded into patterns as Parameter Control events.");
		break;
	case IDC_CHECK_MIDILEARN:
		text = U_("Listens to incoming MIDI data to automatically fill in the appropriate data.");
		break;
	case IDC_SPINMOVEMAPPING:
		text = U_("Change the processing order of the current selected MIDI mapping.");
		break;
	case IDC_COMBO_CHANNEL:
		text = U_("The MIDI channel to listen on for this event.");
		break;
	case IDC_COMBO_EVENT:
		text = U_("The MIDI event to listen for.");
		break;
	case IDC_COMBO_CONTROLLER:
		text = U_("The MIDI controler to listen for.");
		break;
	}

	return text;
}


void CMIDIMappingDialog::SetModified()
{
	if(m_sndFile.GetpModDoc() != nullptr)
		m_sndFile.GetpModDoc()->SetModified();
}


OPENMPT_NAMESPACE_END

#endif // NO_PLUGINS
