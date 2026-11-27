/*
 * dlg_misc.cpp
 * ------------
 * Purpose: Implementation of various OpenMPT dialogs.
 * Notes  : mfcless twin of upstream's mptrack/dlg_misc.cpp.
 *          Draft from qt/tools/twin.py, not yet edited by hand.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "dlg_misc.h"
#include "Childfrm.h"
#include "Dlsbank.h"
#include "HighDPISupport.h"
#include "Moddoc.h"
#include "Mptrack.h"
#include "Reporting.h"
#include "resource.h"
#include "TempoSwingDialog.h"
#include "TrackerSettings.h"
#include "WindowMessages.h"
#include "../common/mptStringBuffer.h"
#include "../common/version.h"
#include "../soundlib/mod_specifications.h"
#include "../soundlib/plugins/PlugInterface.h"

#if MPT_WINNT_AT_LEAST(MPT_WIN_VISTA) && defined(UNICODE)
#include <afxtaskdialog.h>
#endif


OPENMPT_NAMESPACE_BEGIN


///////////////////////////////////////////////////////////////////////
// CModTypeDlg


void CModTypeDlg::RegisterMessageMap()
{
	DialogBase::RegisterMessageMap();
	AddNotify(IDC_COMBO1, mfcless::Notify::SelChange, &CModTypeDlg::UpdateDialog);
	AddNotify(IDC_COMBO_TEMPOMODE, mfcless::Notify::SelChange, &CModTypeDlg::OnTempoModeChanged);
	AddCommand(IDC_CHECK_PT1X, &CModTypeDlg::OnPTModeChanged);
	AddCommand(IDC_BUTTON1, &CModTypeDlg::OnTempoSwing);
	AddCommand(IDC_BUTTON2, &CModTypeDlg::OnLegacyPlaybackSettings);
	AddCommand(IDC_BUTTON3, &CModTypeDlg::OnDefaultBehaviour);
}


void CModTypeDlg::DoDataExchange(DataExchange &dx)
{
	DialogBase::DoDataExchange(dx);
	//{{AFX_DATA_MAP(CModTypeDlg)
	ExchangeControl(dx, IDC_COMBO1, m_TypeBox);
	ExchangeControl(dx, IDC_COMBO2, m_ChannelsBox);
	ExchangeControl(dx, IDC_COMBO_TEMPOMODE, m_TempoModeBox);
	ExchangeControl(dx, IDC_COMBO_MIXLEVELS, m_PlugMixBox);

	ExchangeControl(dx, IDC_CHECK1, m_CheckBox1);
	ExchangeControl(dx, IDC_CHECK2, m_CheckBox2);
	ExchangeControl(dx, IDC_CHECK3, m_CheckBox3);
	ExchangeControl(dx, IDC_CHECK4, m_CheckBox4);
	ExchangeControl(dx, IDC_CHECK5, m_CheckBox5);
	ExchangeControl(dx, IDC_CHECK_PT1X, m_CheckBoxPT1x);
	ExchangeControl(dx, IDC_CHECK_AMIGALIMITS, m_CheckBoxAmigaLimits);

	//}}AFX_DATA_MAP
}


CModTypeDlg::CModTypeDlg(CSoundFile &sf, QWidget *parent)
	: DialogBase{IDD_MODDOC_MODTYPE, parent}
	, sndFile{sf}
{
}


bool CModTypeDlg::OnInitDialog()
{
	DialogBase::OnInitDialog();
	m_nType = sndFile.GetBestSaveFormat();
	m_nChannels = sndFile.GetNumChannels();
	m_tempoSwing = sndFile.m_tempoSwing;
	m_playBehaviour = sndFile.m_playBehaviour;
	m_initialized = false;

	// Mod types

	mfcless::SetItemData(m_TypeBox, mfcless::AddItem(m_TypeBox, U_("ProTracker MOD")), MOD_TYPE_MOD);
	mfcless::SetItemData(m_TypeBox, mfcless::AddItem(m_TypeBox, U_("Scream Tracker S3M")), MOD_TYPE_S3M);
	mfcless::SetItemData(m_TypeBox, mfcless::AddItem(m_TypeBox, U_("FastTracker XM")), MOD_TYPE_XM);
	mfcless::SetItemData(m_TypeBox, mfcless::AddItem(m_TypeBox, U_("Impulse Tracker IT")), MOD_TYPE_IT);
	mfcless::SetItemData(m_TypeBox, mfcless::AddItem(m_TypeBox, U_("OpenMPT MPTM")), MOD_TYPE_MPT);
	switch(m_nType)
	{
	case MOD_TYPE_S3M:	m_TypeBox->setCurrentIndex(1); break;
	case MOD_TYPE_XM:	m_TypeBox->setCurrentIndex(2); break;
	case MOD_TYPE_IT:	m_TypeBox->setCurrentIndex(3); break;
	case MOD_TYPE_MPT:	m_TypeBox->setCurrentIndex(4); break;
	default:			m_TypeBox->setCurrentIndex(0); break;
	}

	// Time signature information

	SetItemInt(IDC_ROWSPERBEAT, sndFile.m_nDefaultRowsPerBeat);
	SetItemInt(IDC_ROWSPERMEASURE, sndFile.m_nDefaultRowsPerMeasure);

	// Version information

	if(sndFile.m_dwCreatedWithVersion) SetItemText(IDC_EDIT_CREATEDWITH, U_("OpenMPT ") + FormatVersionNumber(sndFile.m_dwCreatedWithVersion));
	SetItemText(IDC_EDIT_SAVEDWITH, mpt::ToUnicode(sndFile.m_modFormat.madeWithTracker.empty() ? sndFile.m_modFormat.formatName : sndFile.m_modFormat.madeWithTracker));

	OnDPIChanged();
	UpdateDialog();

	m_initialized = true;
	return true;
}


void CModTypeDlg::OnDPIChanged()
{
	DialogBase::OnDPIChanged();

	const int iconSize = HighDPISupport::ScalePixels(32, m_hWnd);
	DestroyIcon(m_warnIcon);
	m_warnIcon = nullptr;

	mpt::Library comctl32(mpt::LibraryPath::System(P_("Comctl32")));
	if(comctl32.IsValid())
	{
		using PLOADICONWITHSCALEDOWN = HRESULT(WINAPI *)(HINSTANCE, PCWSTR, int, int, QIcon *);
		PLOADICONWITHSCALEDOWN LoadIconWithScaleDown = nullptr;
		if(comctl32.Bind(LoadIconWithScaleDown, "LoadIconWithScaleDown"))
			LoadIconWithScaleDown(NULL, MAKEINTRESOURCEW(IDI_EXCLAMATION), iconSize, iconSize, &m_warnIcon);
	}
	if(!m_warnIcon)
		m_warnIcon = reinterpret_cast<QIcon>(::LoadImage(NULL, IDI_EXCLAMATION, IMAGE_ICON, iconSize, iconSize, LR_SHARED));

	if(m_showWarning)
		static_cast<CStatic *>(GetDlgItem(IDC_STATIC1))->SetIcon(m_warnIcon);
}


mpt::ustring CModTypeDlg::FormatVersionNumber(Version version)
{
	return mpt::ToUnicode(version.ToUString() + (version.IsTestVersion() ? U_(" (test build)") : U_("")));
}


void CModTypeDlg::UpdateChannelCBox()
{
	const MODTYPE type = static_cast<MODTYPE>(mfcless::GetItemData(m_TypeBox, m_TypeBox->currentIndex()));
	CHANNELINDEX currChanSel = static_cast<CHANNELINDEX>(mfcless::GetItemData(m_ChannelsBox, m_ChannelsBox->currentIndex()));
	const CHANNELINDEX minChans = CSoundFile::GetModSpecifications(type).channelsMin;
	const CHANNELINDEX maxChans = CSoundFile::GetModSpecifications(type).channelsMax;
	
	if(m_ChannelsBox->count() < 1 
		|| mfcless::GetItemData(m_ChannelsBox, 0) != minChans
		|| mfcless::GetItemData(m_ChannelsBox, m_ChannelsBox->count() - 1) != maxChans)
	{
		// Update channel list if number of supported channels has changed.
		if(m_ChannelsBox->count() < 1) currChanSel = m_nChannels;
		m_ChannelsBox->clear();

		mpt::ustring s;
		for(CHANNELINDEX i = minChans; i <= maxChans; i++)
		{
			s.Format(U_("%u Channel%s"), i, (i != 1) ? U_("s") : U_(""));
			mfcless::SetItemData(m_ChannelsBox, mfcless::AddItem(m_ChannelsBox, s), i);
		}

		Limit(currChanSel, minChans, maxChans);
		m_ChannelsBox->setCurrentIndex(currChanSel - minChans);
	}
}


void CModTypeDlg::UpdateDialog()
{
	m_nType = static_cast<MODTYPE>(mfcless::GetItemData(m_TypeBox, m_TypeBox->currentIndex()));

	UpdateChannelCBox();

	m_CheckBox1->setChecked(sndFile.m_SongFlags[SONG_LINEARSLIDES] ? 1 : 0);
	m_CheckBox2->setChecked(sndFile.m_SongFlags[SONG_FASTVOLSLIDES] ? 1 : 0);
	m_CheckBox3->setChecked(sndFile.m_SongFlags[SONG_ITOLDEFFECTS] ? 1 : 0);
	m_CheckBox4->setChecked(sndFile.m_SongFlags[SONG_ITCOMPATGXX] ? 1 : 0);
	m_CheckBox5->setChecked(sndFile.m_SongFlags[SONG_EXFILTERRANGE] ? 1 : 0);
	m_CheckBoxPT1x->setChecked(sndFile.m_SongFlags[SONG_PT_MODE] ? 1 : 0);
	m_CheckBoxAmigaLimits->setChecked(sndFile.m_SongFlags[SONG_AMIGALIMITS] ? 1 : 0);

	const FlagSet<SongFlags> allowedFlags(sndFile.GetModSpecifications(m_nType).songFlags);
	m_CheckBox1->setEnabled(allowedFlags[SONG_LINEARSLIDES]);
	m_CheckBox2->setEnabled(allowedFlags[SONG_FASTVOLSLIDES]);
	m_CheckBox3->setEnabled(allowedFlags[SONG_ITOLDEFFECTS]);
	m_CheckBox4->setEnabled(allowedFlags[SONG_ITCOMPATGXX]);
	m_CheckBox5->setEnabled(allowedFlags[SONG_EXFILTERRANGE]);
	m_CheckBoxPT1x->setEnabled(allowedFlags[SONG_PT_MODE]);
	m_CheckBoxAmigaLimits->setEnabled(allowedFlags[SONG_AMIGALIMITS]);

	// These two checkboxes are mutually exclusive and share the same screen space
	m_CheckBoxPT1x->setVisible(m_nType == MOD_TYPE_MOD ? true : false);
	m_CheckBox5->setVisible(m_nType != MOD_TYPE_MOD ? true : false);
	if(allowedFlags[SONG_PT_MODE]) OnPTModeChanged();

	// Tempo modes
	const TempoMode oldTempoMode = m_initialized ? static_cast<TempoMode>(mfcless::GetItemData(m_TempoModeBox, m_TempoModeBox->currentIndex())) : sndFile.m_nTempoMode;
	m_TempoModeBox->clear();

	mfcless::SetItemData(m_TempoModeBox, mfcless::AddItem(m_TempoModeBox, U_("Classic")), static_cast<uintptr_t>(TempoMode::Classic));
	if(m_nType == MOD_TYPE_MPT || (sndFile.GetType() != MOD_TYPE_MPT && sndFile.m_nTempoMode == TempoMode::Alternative))
		mfcless::SetItemData(m_TempoModeBox, mfcless::AddItem(m_TempoModeBox, U_("Alternative")), static_cast<uintptr_t>(TempoMode::Alternative));
	if(m_nType == MOD_TYPE_MPT || (sndFile.GetType() != MOD_TYPE_MPT && sndFile.m_nTempoMode == TempoMode::Modern))
		mfcless::SetItemData(m_TempoModeBox, mfcless::AddItem(m_TempoModeBox, U_("Modern (accurate)")), static_cast<uintptr_t>(TempoMode::Modern));
	m_TempoModeBox->setCurrentIndex(0);
	for(int i = m_TempoModeBox->count(); i > 0; i--)
	{
		if(static_cast<TempoMode>(mfcless::GetItemData(m_TempoModeBox, i)) == oldTempoMode)
		{
			m_TempoModeBox->setCurrentIndex(i);
			break;
		}
	}
	OnTempoModeChanged();

	// Mix levels
	const MixLevels oldMixLevels = m_initialized ? static_cast<MixLevels>(mfcless::GetItemData(m_PlugMixBox, m_PlugMixBox->currentIndex())) : sndFile.GetMixLevels();
	m_PlugMixBox->clear();
	if(m_nType == MOD_TYPE_MPT || sndFile.GetMixLevels() == MixLevels::v1_17RC3) // In XM/IT, this is only shown for backwards compatibility with existing tunes
		mfcless::SetItemData(m_PlugMixBox, mfcless::AddItem(m_PlugMixBox, U_("OpenMPT 1.17RC3")), static_cast<uintptr_t>(MixLevels::v1_17RC3));
	if(sndFile.GetMixLevels() == MixLevels::v1_17RC2) // Only shown for backwards compatibility with existing tunes
		mfcless::SetItemData(m_PlugMixBox, mfcless::AddItem(m_PlugMixBox, U_("OpenMPT 1.17RC2")), static_cast<uintptr_t>(MixLevels::v1_17RC2));
	if(sndFile.GetMixLevels() == MixLevels::v1_17RC1) // Ditto
		mfcless::SetItemData(m_PlugMixBox, mfcless::AddItem(m_PlugMixBox, U_("OpenMPT 1.17RC1")), static_cast<uintptr_t>(MixLevels::v1_17RC1));
	if(sndFile.GetMixLevels() == MixLevels::Original) // Ditto
		mfcless::SetItemData(m_PlugMixBox, mfcless::AddItem(m_PlugMixBox, U_("Original (MPT 1.16)")), static_cast<uintptr_t>(MixLevels::Original));
	int compatMixMode = mfcless::AddItem(m_PlugMixBox, U_("Compatible"));
	mfcless::SetItemData(m_PlugMixBox, compatMixMode, static_cast<uintptr_t>(MixLevels::Compatible));
	if(m_nType == MOD_TYPE_XM)
		mfcless::SetItemData(m_PlugMixBox, mfcless::AddItem(m_PlugMixBox, U_("Compatible (FT2 Pan Law)")), static_cast<uintptr_t>(MixLevels::CompatibleFT2));

	// Default to compatible mix mode
	m_PlugMixBox->setCurrentIndex(compatMixMode);
	int mixCount = m_PlugMixBox->count();
	for(int i = 0; i < mixCount; i++)
	{
		if(static_cast<MixLevels>(mfcless::GetItemData(m_PlugMixBox, i)) == oldMixLevels)
		{
			m_PlugMixBox->setCurrentIndex(i);
			break;
		}
	}

	const bool XMorITorMPT = (m_nType & (MOD_TYPE_XM | MOD_TYPE_IT | MOD_TYPE_MPT));
	const bool isMPTM = (m_nType == MOD_TYPE_MPT);

	// Mixmode Box
	EnableItem(IDC_TEXT_MIXMODE, XMorITorMPT);
	m_PlugMixBox->setEnabled(XMorITorMPT);
	
	// Tempo mode box
	m_TempoModeBox->setEnabled(XMorITorMPT);
	EnableItem(IDC_ROWSPERBEAT, XMorITorMPT);
	EnableItem(IDC_ROWSPERMEASURE, XMorITorMPT);
	EnableItem(IDC_TEXT_ROWSPERBEAT, XMorITorMPT);
	EnableItem(IDC_TEXT_ROWSPERMEASURE, XMorITorMPT);
	EnableItem(IDC_TEXT_TEMPOMODE, XMorITorMPT);
	EnableItem(IDC_FRAME_TEMPOMODE, XMorITorMPT);

	// Compatibility settings
	const PlayBehaviourSet defaultBehaviour = CSoundFile::GetDefaultPlaybackBehaviour(m_nType);
	const PlayBehaviourSet supportedBehaviour = CSoundFile::GetSupportedPlaybackBehaviour(m_nType);
	bool enableSetDefaults = false;
	m_showWarning = false;
	if(m_nType & (MOD_TYPE_MPT | MOD_TYPE_IT | MOD_TYPE_XM))
	{
		for(size_t i = 0; i < m_playBehaviour.size(); i++)
		{
			// Some flags are not really important for "default" behaviour.
			if(defaultBehaviour[i] != m_playBehaviour[i]
				&& i != MSF_COMPATIBLE_PLAY
				&& i != kFT2VolumeRamping)
			{
				enableSetDefaults = true;
				if(!isMPTM)
				{
					m_showWarning = true;
					break;
				}
			}
			if(isMPTM && m_playBehaviour[i] && !supportedBehaviour[i])
			{

				enableSetDefaults = true;
				m_showWarning = true;
				break;
			}
		}
	}
	static_cast<CStatic *>(GetDlgItem(IDC_STATIC1))->SetIcon(m_showWarning ? m_warnIcon : nullptr);
	GetDlgItem(IDC_STATIC2)->SetWindowText(m_showWarning
		? U_("Playback settings have been set to legacy compatibility mode. Click \"Set Defaults\" to use the recommended settings instead.")
		: U_("Compatibility settings are currently optimal. It is advised to not edit them."));
	EnableItem(IDC_BUTTON3, enableSetDefaults ? true : false);
}


void CModTypeDlg::OnPTModeChanged()
{
	// PT1/2 mode enforces Amiga limits
	const bool ptMode = IsChecked(IDC_CHECK_PT1X) != 0;
	m_CheckBoxAmigaLimits->setEnabled(!ptMode);
	if(ptMode) m_CheckBoxAmigaLimits->setChecked(1);
}


void CModTypeDlg::OnTempoModeChanged()
{
	EnableItem(IDC_BUTTON1, static_cast<TempoMode>(mfcless::GetItemData(m_TempoModeBox, m_TempoModeBox->currentIndex())) == TempoMode::Modern);
}


void CModTypeDlg::OnTempoSwing()
{
	const ROWINDEX oldRPB = sndFile.m_nDefaultRowsPerBeat;
	const ROWINDEX oldRPM = sndFile.m_nDefaultRowsPerMeasure;
	const TempoMode oldMode = sndFile.m_nTempoMode;

	// Temporarily apply new tempo signature for preview
	const ROWINDEX newRPB = std::clamp(static_cast<ROWINDEX>(GetItemInt(IDC_ROWSPERBEAT)), ROWINDEX(1), MAX_ROWS_PER_BEAT);
	const ROWINDEX newRPM = std::clamp(static_cast<ROWINDEX>(GetItemInt(IDC_ROWSPERMEASURE)), newRPB, MAX_ROWS_PER_BEAT);
	sndFile.m_nDefaultRowsPerBeat = newRPB;
	sndFile.m_nDefaultRowsPerMeasure = newRPM;
	sndFile.m_nTempoMode = TempoMode::Modern;

	m_tempoSwing.resize(newRPB, TempoSwing::Unity);
	CTempoSwingDlg dlg(this, m_tempoSwing, sndFile);
	if(dlg.DoModal() == IDOK)
	{
		m_tempoSwing = dlg.m_tempoSwing;
	}
	sndFile.m_nDefaultRowsPerBeat = oldRPB;
	sndFile.m_nDefaultRowsPerMeasure = oldRPM;
	sndFile.m_nTempoMode = oldMode;
}


void CModTypeDlg::OnLegacyPlaybackSettings()
{
	CLegacyPlaybackSettingsDlg dlg(this, m_playBehaviour, m_nType);
	if(dlg.DoModal() == IDOK)
	{
		m_playBehaviour = dlg.GetPlayBehaviour();
	}
	UpdateDialog();
}


void CModTypeDlg::OnDefaultBehaviour()
{
	m_playBehaviour = CSoundFile::GetDefaultPlaybackBehaviour(m_nType);
	UpdateDialog();
}


bool CModTypeDlg::VerifyData()
{
	const int newRPB = GetItemInt(IDC_ROWSPERBEAT);
	const int newRPM = GetItemInt(IDC_ROWSPERMEASURE);
	if(newRPB > newRPM)
	{
		Reporting::Warning("Error: Rows per measure must be greater than or equal to rows per beat.");
		GotoDlgCtrl(GetDlgItem(IDC_ROWSPERMEASURE));
		return false;
	}
	if(newRPB == 0 && static_cast<TempoMode>(mfcless::GetItemData(m_TempoModeBox, m_TempoModeBox->currentIndex())) == TempoMode::Modern)
	{
		Reporting::Warning("Error: Rows per beat must be greater than 0 in modern tempo mode.");
		GotoDlgCtrl(GetDlgItem(IDC_ROWSPERBEAT));
		return false;
	}

	int sel = static_cast<int>(mfcless::GetItemData(m_ChannelsBox, m_ChannelsBox->currentIndex()));
	MODTYPE type = static_cast<MODTYPE>(mfcless::GetItemData(m_TypeBox, m_TypeBox->currentIndex()));

	CHANNELINDEX maxChans = CSoundFile::GetModSpecifications(type).channelsMax;

	if(sel > maxChans)
	{
		mpt::ustring error;
		error.Format(U_("Error: Maximum number of channels for this module type is %u."), maxChans);
		Reporting::Warning(error);
		return false;
	}

	if(maxChans < sndFile.GetNumChannels())
	{
		if(Reporting::Confirm("New module type supports less channels than currently used, and reducing channel number is required. Continue?") != cnfYes)
			return false;
	}

	return true;
}


void CModTypeDlg::OnOK()
{
	if (!VerifyData())
		return;

	int sel = m_TypeBox->currentIndex();
	if (sel >= 0)
	{
		m_nType = static_cast<MODTYPE>(mfcless::GetItemData(m_TypeBox, sel));
	}
	const auto &newModSpecs = sndFile.GetModSpecifications(m_nType);

	sndFile.m_SongFlags.set(SONG_LINEARSLIDES, m_CheckBox1->isChecked() != 0);
	sndFile.m_SongFlags.set(SONG_FASTVOLSLIDES, m_CheckBox2->isChecked() != 0);
	sndFile.m_SongFlags.set(SONG_ITOLDEFFECTS, m_CheckBox3->isChecked() != 0);
	sndFile.m_SongFlags.set(SONG_ITCOMPATGXX, m_CheckBox4->isChecked() != 0);
	sndFile.m_SongFlags.set(SONG_EXFILTERRANGE, m_CheckBox5->isChecked() != 0);
	sndFile.m_SongFlags.set(SONG_PT_MODE, m_CheckBoxPT1x->isChecked() != 0);
	sndFile.m_SongFlags.set(SONG_AMIGALIMITS, m_CheckBoxAmigaLimits->isChecked() != 0);

	sel = m_ChannelsBox->currentIndex();
	if (sel >= 0)
	{
		m_nChannels = static_cast<CHANNELINDEX>(mfcless::GetItemData(m_ChannelsBox, sel));
	}
	
	sndFile.m_nDefaultRowsPerBeat    = std::min(static_cast<ROWINDEX>(GetItemInt(IDC_ROWSPERBEAT)), MAX_ROWS_PER_BEAT);
	sndFile.m_nDefaultRowsPerMeasure = std::min(static_cast<ROWINDEX>(GetItemInt(IDC_ROWSPERMEASURE)), MAX_ROWS_PER_BEAT);
	sndFile.m_PlayState.UpdateTimeSignature(sndFile);

	sel = m_TempoModeBox->currentIndex();
	if(sel >= 0)
	{
		const auto oldMode = sndFile.m_nTempoMode;
		sndFile.m_nTempoMode = static_cast<TempoMode>(mfcless::GetItemData(m_TempoModeBox, sel));
		if(oldMode == TempoMode::Modern && sndFile.m_nTempoMode != TempoMode::Modern)
		{
			for(auto &order : sndFile.Order)
			{
				double newTempo = order.GetDefaultTempo().ToDouble() * (order.GetDefaultSpeed() * sndFile.m_nDefaultRowsPerBeat) / ((sndFile.m_nTempoMode == TempoMode::Classic) ? 24 : 60);
				if(!newModSpecs.hasFractionalTempo)
					newTempo = std::round(newTempo);
				order.SetDefaultTempo(Clamp(TEMPO(newTempo), newModSpecs.GetTempoMin(), newModSpecs.GetTempoMax()));
			}
		}
	}
	if(sndFile.m_nTempoMode == TempoMode::Modern)
	{
		sndFile.m_tempoSwing = m_tempoSwing;
		if(!sndFile.m_tempoSwing.empty())
			sndFile.m_tempoSwing.resize(sndFile.m_nDefaultRowsPerBeat);
	} else
	{
		sndFile.m_tempoSwing.clear();
	}

	sel = m_PlugMixBox->currentIndex();
	if(sel >= 0)
	{
		sndFile.SetMixLevels(static_cast<MixLevels>(mfcless::GetItemData(m_PlugMixBox, sel)));
	}

	PlayBehaviourSet allowedFlags = CSoundFile::GetSupportedPlaybackBehaviour(m_nType);
	for(size_t i = 0; i < kMaxPlayBehaviours; i++)
	{
		// Only set those flags which are supported by the new format or were already enabled previously
		sndFile.m_playBehaviour.set(i, m_playBehaviour[i] && (allowedFlags[i] || (sndFile.m_playBehaviour[i] && sndFile.GetType() == m_nType)));
	}

	DestroyIcon(m_warnIcon);
	DialogBase::OnOK();
}


mpt::ustring CModTypeDlg::GetToolTipText(uint32 id, QWidget *) const
{
	mpt::ustring text;
	switch(id)
	{
	case IDC_CHECK1:
		text = U_("Note slides always slide the same amount, not depending on the sample frequency.");
		break;
	case IDC_CHECK2:
		text = U_("Old Scream Tracker 3 volume slide behaviour (not recommended).");
		break;
	case IDC_CHECK3:
		text = U_("Play some effects like in early versions of Impulse Tracker (not recommended).");
		break;
	case IDC_CHECK4:
		text = U_("Gxx and Exx/Fxx won't share effect memory. Gxx resets instrument envelopes.");
		break;
	case IDC_CHECK5:
		text = U_("The resonant filter's frequency range is increased from about 5kHz to 10kHz.");
		break;
	case IDC_CHECK_PT1X:
		text = U_("Enforce Amiga frequency limits, ProTracker offset bug emulation.");
		break;
	case IDC_COMBO_MIXLEVELS:
		text = U_("Mixing method of sample and instrument plugin levels.");
		break;
	case IDC_BUTTON1:
		if(!GetDlgItem(IDC_BUTTON1)->IsWindowEnabled())
		{
			text = U_("Tempo swing is only available in modern tempo mode.");
		} else
		{
			text = U_("Swing setting: ");
			if(m_tempoSwing.empty())
			{
				text += U_("Default");
			} else
			{
				for(size_t i = 0; i < m_tempoSwing.size(); i++)
				{
					if(i > 0)
						text += U_(" / ");
					text += MPT_UFORMAT("{}%")(Util::muldivr(m_tempoSwing[i], 100, TempoSwing::Unity));
				}
			}
		}
	}

	return text;
}


//////////////////////////////////////////////////////////////////////////////
// Legacy Playback Settings dialog

void CLegacyPlaybackSettingsDlg::RegisterMessageMap()
{
	ResizableDialog::RegisterMessageMap();
	AddCommand(IDC_BUTTON1, &CLegacyPlaybackSettingsDlg::OnSelectDefaults);
	AddNotify(IDC_EDIT1, mfcless::Notify::EditUpdate, &CLegacyPlaybackSettingsDlg::OnFilterStringChanged);
	AddNotify(IDC_LIST1, mfcless::Notify::CheckChange, &CLegacyPlaybackSettingsDlg::UpdateSelectDefaults);
}


void CLegacyPlaybackSettingsDlg::DoDataExchange(DataExchange &dx)
{
	ResizableDialog::DoDataExchange(dx);
	ExchangeControl(dx, IDC_LIST1, m_CheckList);
}


CLegacyPlaybackSettingsDlg::CLegacyPlaybackSettingsDlg(QWidget *parent, PlayBehaviourSet &playBehaviour, MODTYPE modType)
	: ResizableDialog{IDD_LEGACY_PLAYBACK, parent}
	, m_playBehaviour{playBehaviour}
	, m_modType{modType}
{
}


bool CLegacyPlaybackSettingsDlg::OnInitDialog()
{
	ResizableDialog::OnInitDialog();
	m_CheckList->SetItemHeight(0, 0);  // Workaround to force MFC to correctly compute the height of the first list item, in particular on high-DPI setups
	OnFilterStringChanged();
	UpdateSelectDefaults();
	return true;
}


void CLegacyPlaybackSettingsDlg::OnSelectDefaults()
{
	const int count = m_CheckList->count();
	m_playBehaviour = CSoundFile::GetDefaultPlaybackBehaviour(m_modType);
	for(int i = 0; i < count; i++)
	{
		m_CheckList->SetCheck(i, m_playBehaviour[mfcless::GetItemData(m_CheckList, i)] ? 1 : 0);
	}
}


void CLegacyPlaybackSettingsDlg::UpdateSelectDefaults()
{
	const int count = m_CheckList->count();
	for(int i = 0; i < count; i++)
	{
		m_playBehaviour.set(mfcless::GetItemData(m_CheckList, i),  m_CheckList->GetCheck(i) != 0);
	}
	const auto defaults = CSoundFile::GetDefaultPlaybackBehaviour(m_modType);
	EnableItem(IDC_BUTTON1, m_playBehaviour != defaults ? true : false);
}


void CLegacyPlaybackSettingsDlg::OnFilterStringChanged()
{
	mpt::ustring s;
	s = GetItemText(IDC_EDIT1);
	const bool filterActive = !s.IsEmpty();
	s.MakeLower();

	m_CheckList->setUpdatesEnabled(false);
	m_CheckList->clear();

	const auto allowedFlags = CSoundFile::GetSupportedPlaybackBehaviour(m_modType);
	for(size_t i = 0; i < kMaxPlayBehaviours; i++)
	{
		const mpt::uchar *desc = U_("");
		switch(i)
		{
		case MSF_COMPATIBLE_PLAY: continue;

		case kMPTOldSwingBehaviour: desc = U_("OpenMPT 1.17 compatible random variation behaviour for instruments"); break;
		case kMIDICCBugEmulation: desc = U_("Plugin volume MIDI CC bug emulation"); break;
		case kOldMIDIPitchBends: desc = U_("Old Pitch Wheel behaviour for instrument plugins"); break;
		case kFT2VolumeRamping: desc = U_("Use smooth Fasttracker 2 volume ramping"); break;
		case kMODVBlankTiming: desc = U_("VBlank timing: F20 and above sets speed instead of tempo"); break;

		case kSlidesAtSpeed1: desc = U_("Execute regular portamento slides at speed 1"); break;
		case kPeriodsAreHertz: desc = U_("Compute note frequency in Hertz rather than periods"); break;
		case kTempoClamp: desc = U_("Clamp tempo to 32-255 range"); break;
		case kPerChannelGlobalVolSlide: desc = U_("Global volume slide memory is per-channel"); break;
		case kPanOverride: desc = U_("Panning commands override surround and random pan variation"); break;

		case kITInstrWithoutNote: desc = U_("Retrigger instrument envelopes on instrument change"); break;
		case kITVolColFinePortamento: desc = U_("Volume column portamento never does fine portamento"); break;
		case kITArpeggio: desc = U_("IT arpeggio algorithm"); break;
		case kITOutOfRangeDelay: desc = U_("Out-of-range delay commands queue new instrument"); break;
		case kITPortaMemoryShare: desc = U_("Gxx shares memory with Exx and Fxx"); break;
		case kITPatternLoopTargetReset: desc = U_("After finishing a pattern loop, set the pattern loop target to the next row"); break;
		case kITFT2PatternLoop: desc = U_("Nested pattern loop behaviour"); break;
		case kITPingPongNoReset: desc = U_("Do not reset ping pong direction with instrument numbers"); break;
		case kITEnvelopeReset: desc = U_("IT envelope reset behaviour"); break;
		case kITClearOldNoteAfterCut: desc = U_("Forget the previous note after cutting it"); break;
		case kITVibratoTremoloPanbrello: desc = U_("More IT-like Vibrato, Tremolo and Panbrello handling"); break;
		case kITTremor: desc = U_("Ixx behaves like in IT"); break;
		case kITRetrigger: desc = U_("Qxx behaves like in IT"); break;
		case kITMultiSampleBehaviour: desc = U_("Properly update C-5 frequency when changing note in multisampled instrument"); break;
		case kITPortaTargetReached: desc = U_("Clear portamento target after it has been reached"); break;
		case kITPatternLoopBreak: desc = U_("Do not reset loop count on pattern break"); break;
		case kITOffset: desc = U_("Offset after sample end is treated like in IT"); break;
		case kITSwingBehaviour: desc = U_("Volume and panning random variation work more like in IT"); break;
		case kITNNAReset: desc = U_("NNA is reset on every note change, not every instrument change"); break;
		case kITSCxStopsSample: desc = U_("SCx really stops the sample and does not just mute it"); break;
		case kITEnvelopePositionHandling: desc = U_("IT-style envelope position advance + enable/disable behaviour"); break;
		case kITPortamentoInstrument: desc = U_("More compatible instrument change + portamento"); break;
		case kITPingPongMode: desc = U_("Do not repeat last sample point in ping pong loop, like IT's software mixer"); break;
		case kITRealNoteMapping: desc = U_("Use triggered note rather than translated note for PPS and DNA note check"); break;
		case kITHighOffsetNoRetrig: desc = U_("SAx does not apply an offset effect to a note next to it"); break;
		case kITFilterBehaviour: desc = U_("User IT's filter coefficients (unless extended filter range is used) and behaviour"); break;
		case kITNoSurroundPan: desc = U_("Panning modulation is disabled on surround channels"); break;
		case kITShortSampleRetrig: desc = U_("Do not retrigger already stopped channels"); break;
		case kITPortaNoNote: desc = U_("Do not apply any portamento if no previous note is playing"); break;
		case kITFT2DontResetNoteOffOnPorta:
			if(m_modType == MOD_TYPE_XM)
				desc = U_("Reset note-off on portamento if there is an instrument number");
			else
				desc = U_("Reset note-off on portamento if there is an instrument number in Compatible Gxx mode");
			break;
		case kITVolColMemory: desc = U_("Volume column effects share their memory with the effect column"); break;
		case kITPortamentoSwapResetsPos: desc = U_("Portamento with sample swap plays the new sample from the beginning"); break;
		case kITEmptyNoteMapSlot: desc = U_("Ignore instrument note map entries with no note completely"); break;
		case kITFirstTickHandling: desc = U_("IT-style first tick handling"); break;
		case kITSampleAndHoldPanbrello: desc = U_("IT-style sample&hold panbrello waveform"); break;
		case kITClearPortaTarget: desc = U_("New notes reset portamento target in IT"); break;
		case kITPanbrelloHold: desc = U_("Do not reset panbrello effect until next note or panning effect"); break;
		case kITPanningReset: desc = U_("Sample and instrument panning is only applied on note change, not instrument change"); break;
		case kITPatternLoopWithJumpsOld: desc = U_("Bxx on the same row as SBx terminates the loop in IT"); break;
		case kITInstrWithNoteOff: desc = U_("Instrument number with note-off recalls default volume"); break;
		case kFT2Arpeggio: desc = U_("FT2 arpeggio algorithm"); break;
		case kFT2Retrigger: desc = U_("Rxx behaves like in FT2"); break;
		case kFT2VolColVibrato: desc = U_("Vibrato speed in volume column does not actually execute the vibrato effect"); break;
		case kFT2PortaNoNote: desc = U_("Do not play portamento-ed note if no previous note is playing"); break;
		case kFT2KeyOff: desc = U_("FT2-style Kxx handling"); break;
		case kFT2PanSlide: desc = U_("Volume-column pan slides are finer"); break;
		case kFT2ST3OffsetOutOfRange: desc = U_("Offset past sample end stops the note"); break;
		case kFT2RestrictXCommand: desc = U_("Do not allow ModPlug extensions to X command"); break;
		case kFT2RetrigWithNoteDelay: desc = U_("Retrigger envelopes if there is a note delay with no note"); break;
		case kFT2SetPanEnvPos: desc = U_("Lxx only sets the pan envelope position if the volume envelope's sustain flag is set"); break;
		case kFT2PortaIgnoreInstr: desc = U_("Portamento with instrument number applies volume settings of new sample, but not the new sample itself"); break;
		case kFT2VolColMemory: desc = U_("No volume column memory"); break;
		case kFT2LoopE60Restart: desc = U_("Next pattern starts on the same row as the last E60 command"); break;
		case kFT2ProcessSilentChannels: desc = U_("Keep processing faded channels for later portamento pickup"); break;
		case kFT2ReloadSampleSettings: desc = U_("Reload sample settings even if a note-off is placed next to an instrument number"); break;
		case kFT2PortaDelay: desc = U_("Portamento with note delay next to it is ignored"); break;
		case kFT2Transpose: desc = U_("Ignore out-of-range transposed notes"); break;
		case kFT2PatternLoopWithJumps: desc = U_("Bxx or Dxx on the same row as E6x terminates the loop"); break;
		case kFT2PortaTargetNoReset: desc = U_("Portamento target is not reset with new notes"); break;
		case kFT2EnvelopeEscape: desc = U_("Sustain point at end of envelope loop stops the loop after release"); break;
		case kFT2Tremor: desc = U_("Txx behaves like in FT2"); break;
		case kFT2OutOfRangeDelay: desc = U_("Do not trigger notes with out-of-range note delay"); break;
		case kFT2Periods: desc = U_("Use FT2's broken period handling"); break;
		case kFT2PanWithDelayedNoteOff: desc = U_("Panning command with delayed note-off is ignored"); break;
		case kFT2VolColDelay: desc = U_("FT2-style volume column handling if there is a note delay"); break;
		case kFT2FinetunePrecision: desc = U_("Round sample finetune to multiples of 8"); break;
		case kFT2NoteOffFlags: desc = U_("Fade instrument on note-off when there is no volume envelope; instrument numbers reset note-off status"); break;
		case kITMultiSampleInstrumentNumber: desc = U_("Lone instrument number after portamento within multi-sampled instrument sets the target sample's settings"); break;
		case kRowDelayWithNoteDelay: desc = U_("Note delays next to a row delay are repeated on every row repetition"); break;
		case kFT2MODTremoloRampWaveform: desc = U_("Emulate FT2/ProTracker tremolo ramp down / triangle waveform"); break;
		case kFT2PortaUpDownMemory: desc = U_("Portamento Up and Down have separate effect memory"); break;
		case kST3NoMutedChannels: desc = U_("Do not process any effects on muted S3M channels"); break;
		case kST3EffectMemory: desc = U_("Most effects share the same memory"); break;
		case kST3PortaSampleChange: desc = U_("Portamento with instrument number applies volume settings of new sample, but not the new sample itself (GUS)"); break;
		case kST3VibratoMemory: desc = U_("Do not remember vibrato type in effect memory"); break;
		case kST3LimitPeriod: desc = U_("Stop note when reaching the format's maximum note frequency"); break;
		case KST3PortaAfterArpeggio: desc = U_("Portamento immediately following an arpeggio effect continues at the last arpeggiated note"); break;
		case kMODOneShotLoops: desc = U_("ProTracker one-shot loops"); break;
		case kMODIgnorePanning: desc = U_("Ignore panning commands"); break;
		case kMODSampleSwap: desc = U_("Enable on-the-fly sample swapping"); break;
		case kMODOutOfRangeNoteDelay: desc = U_("Out-of-range note delay is played on next row"); break;
		case kMODTempoOnSecondTick: desc = U_("Tempo changes are handled on second tick instead of first"); break;
		case kFT2PanSustainRelease: desc = U_("If the sustain point of the panning envelope is reached before key-off, it is never released"); break;
		case kLegacyReleaseNode: desc = U_("Old volume envelope release node scaling behaviour"); break;
		case kOPLBeatingOscillators: desc = U_("Beating OPL oscillators"); break;
		case kST3OffsetWithoutInstrument: desc = U_("Notes without instrument use the previous note's sample offset"); break;
		case kReleaseNodePastSustainBug: desc = U_("Broken release node after sustain end behaviour"); break;
		case kFT2NoteDelayWithoutInstr: desc = U_("Delayed instrument-less notes should not recall volume and panning"); break;
		case kOPLFlexibleNoteOff: desc = U_("Full control over OPL notes after note-off"); break;
		case kITInstrWithNoteOffOldEffects: desc = U_("Instrument number with note-off retriggers envelopes with Old Effects enabled"); break;
		case kMIDIVolumeOnNoteOffBug: desc = U_("Reset VST volume on note-off"); break;
		case kITDoNotOverrideChannelPan: desc = U_("Instruments / samples with forced panning do not override channel panning for following instruments / samples"); break;
		case kITPatternLoopWithJumps: desc = U_("Bxx right of SBx terminates the loop in IT"); break;
		case kITDCTBehaviour: desc = U_("Duplicate Sample Check requires same instrument, Duplicate Note Check uses pattern notes for comparison"); break;
		case kOPLwithNNA: desc = U_("New Note Action / Duplicate Note Action set to Note Off and Note Fade affect OPL notes like samples"); break;
		case kST3RetrigAfterNoteCut: desc = U_("Notes cannot be retriggered after they have been cut"); break;
		case kST3SampleSwap: desc = U_("Enable on-the-fly sample swapping (SoundBlaster driver)"); break;
		case kOPLRealRetrig: desc = U_("Retrigger (Qxy) affects OPL notes"); break;
		case kOPLNoResetAtEnvelopeEnd: desc = U_("Do not reset OPL channel status at end of envelopes"); break;
		case kOPLNoteStopWith0Hz: desc = U_("OPL key-off sets note frequency to 0 Hz"); break;
		case kOPLNoteOffOnNoteChange: desc = U_("Send OPL key-off when triggering notes"); break;
		case kFT2PortaResetDirection: desc = U_("Tone Portamento direction resets after reaching portamento target from below"); break;
		case kApplyUpperPeriodLimit: desc = U_("Apply lower frequency limit"); break;
		case kApplyOffsetWithoutNote: desc = U_("Offset commands work without a note next to them"); break;
		case kITPitchPanSeparation: desc = U_("Pitch / Pan Separation can be overridden by panning commands"); break;
		case kImprecisePingPongLoops: desc = U_("Use old imprecise ping-pong loop end calculation"); break;
		case kPluginIgnoreTonePortamento:
			if(m_modType == MOD_TYPE_XM)
				desc = U_("Ignore tone portamento and fine pitch slides for instrument plugins");
			else
				desc = U_("Ignore tone portamento for instrument plugins");
			break;
		case kST3TonePortaWithAdlibNote: desc = U_("OPL notes with Tone Portamento are delayed until the next row"); break;
		case kITResetFilterOnPortaSmpChange: desc = U_("Reset filter on portamento if new note plays a different sample"); break;
		case kITInitialNoteMemory: desc = U_("Initial Last Note Memory of each channel is C-0 instead of No Note"); break;
		case kPluginDefaultProgramAndBank1: desc = U_("Assume initial plugin MIDI program and bank number is 1"); break;
		case kITNoSustainOnPortamento: desc = U_("Portamento after note-off does not re-enable sample sustain loop"); break;
		case kITEmptyNoteMapSlotIgnoreCell: desc = U_("Ignore pattern cell completely when trying to play unmapped instrument note"); break;
		case kITOffsetWithInstrNumber: desc = U_("Offset command with instrument number recalls offset with last note"); break;
		case kContinueSampleWithoutInstr: desc = U_("New note without instrument number does not play looped samples from the start"); break;
		case kMIDINotesFromChannelPlugin: desc = U_("MIDI notes can be sent to channel plugins"); break;
		case kITDoublePortamentoSlides: desc = U_("Parameters of conflicting volume and effect column portamento commands may overwrite each other"); break;
		case kS3MIgnoreCombinedFineSlides: desc =U_("Ignore combined fine slides (Kxy / Lxy)"); break;
		case kFT2AutoVibratoAbortSweep: desc = U_("Key-off before auto-vibrato sweep-in is complete resets auto-vibrato depth"); break;
		case kLegacyPPQpos: desc = U_("Report inaccurate PPQ position to VST plugins (like OpenMPT 1.31 and older)"); break;
		case kLegacyPluginNNABehaviour: desc = U_("Plugin notes with New Note Action set to Continue are affected by note-offs (like OpenMPT 1.31 and older)"); break;
		case kITCarryAfterNoteOff: desc = U_("Note-Off status does not influence Envelope Carry behaviour"); break;
		case kFT2OffsetMemoryRequiresNote: desc = U_("Offset effect memory is only updated when the command is next to a note"); break;
		case kITNoteCutWithPorta: desc = U_("Note Cut (SCx) resets note pitch and interacts with tone portamento + row delay"); break;
		case kITVolColNoSlidePropagation: desc = U_("Do not propagate volume column volume slide memory to regular effect column"); break;
		case kITStoppedFilterEnvAtStart: desc = U_("Stopped filter envelope is still applied even if its first tick has not been processed yet"); break;
		case kITCompatGxxCarryPortaWithIns: desc = U_("Envelope Carry quirk in Compatible Gxx mode with portamento and instrument number"); break;

		default: MPT_ASSERT_NOTREACHED();
		}

		if(filterActive && mpt::ustring{desc}.MakeLower().Find(s) < 0)
			continue;

		if(m_playBehaviour[i] || allowedFlags[i])
		{
			int item = mfcless::AddItem(m_CheckList, desc);
			mfcless::SetItemData(m_CheckList, item, i);
			int check = m_playBehaviour[i] ? 1 : 0;
			if(!allowedFlags[i])
				check = 2;  // Is checked but not supported by format -> grey out
			m_CheckList->SetCheck(item, check);
		}
	}
	m_CheckList->setUpdatesEnabled(true);
}


///////////////////////////////////////////////////////////
// CRemoveChannelsDlg

void CRemoveChannelsDlg::DoDataExchange(DataExchange &dx)
{
	DialogBase::DoDataExchange(dx);
	ExchangeControl(dx, IDC_REMCHANSLIST, m_RemChansList);
}


void CRemoveChannelsDlg::RegisterMessageMap()
{
	DialogBase::RegisterMessageMap();
	AddNotify(IDC_REMCHANSLIST, mfcless::Notify::SelChange, &CRemoveChannelsDlg::OnChannelChanged);
}


CRemoveChannelsDlg::CRemoveChannelsDlg(CSoundFile &sf, CHANNELINDEX toRemove, bool showCancel, QWidget *parent)
	: DialogBase{IDD_REMOVECHANNELS, parent}
	, sndFile{sf}
	, m_bKeepMask(sf.GetNumChannels(), true)
	, m_nRemove{toRemove}
	, m_ShowCancel{showCancel}
{
}


bool CRemoveChannelsDlg::OnInitDialog()
{
	mpt::ustring s;
	DialogBase::OnInitDialog();
	const CHANNELINDEX numChannels = sndFile.GetNumChannels();
	for(CHANNELINDEX n = 0; n < numChannels; n++)
	{
		s = MPT_UFORMAT("Channel {}")(n + 1);
		if(sndFile.ChnSettings[n].szName[0] >= 0x20)
		{
			s += U_(": ");
			s += mpt::ToUnicode(sndFile.GetCharsetInternal(), sndFile.ChnSettings[n].szName);
		}
		mfcless::SetItemData(m_RemChansList, mfcless::AddItem(m_RemChansList, s), n);
		if (!m_bKeepMask[n]) m_RemChansList->SetSel(n);
	}

	if (m_nRemove > 0)
		s = MPT_UFORMAT("Select {} channel{} to remove:")(m_nRemove, (m_nRemove != 1) ? mpt::ustring(U_("s")) : mpt::ustring(U_("")));
	else
		s = MPT_UFORMAT("Select channels to remove (the minimum number of remaining channels is {})")(sndFile.GetModSpecifications().channelsMin);
	
	SetItemText(IDC_QUESTION1, s);
	if(GetDlgItem(IDCANCEL)) ShowItem(IDCANCEL, m_ShowCancel);

	OnChannelChanged();
	return true;
}


void CRemoveChannelsDlg::OnOK()
{
	int selCount = m_RemChansList->GetSelCount();
	std::vector<int> selected(selCount);
	m_RemChansList->GetSelItems(selCount, selected.data());

	m_bKeepMask.assign(sndFile.GetNumChannels(), true);
	for (const auto sel : selected)
	{
		m_bKeepMask[sel] = false;
	}
	if ((static_cast<CHANNELINDEX>(selCount) == m_nRemove && selCount > 0)
		|| (m_nRemove == 0 && (sndFile.GetNumChannels() >= selCount + sndFile.GetModSpecifications().channelsMin)))
		DialogBase::OnOK();
	else
		DialogBase::OnCancel();
}


void CRemoveChannelsDlg::OnChannelChanged()
{
	const uint32 selCount = m_RemChansList->GetSelCount();
	EnableItem(IDOK, ((selCount == m_nRemove && selCount > 0)  || (m_nRemove == 0 && (sndFile.GetNumChannels() >= selCount + sndFile.GetModSpecifications().channelsMin) && selCount > 0)) ? true : false);
}


InfoDialog::InfoDialog(QWidget *parent)
	: ResizableDialog(IDD_INFO_BOX, parent)
{ }

bool InfoDialog::OnInitDialog()
{
	ResizableDialog::OnInitDialog();
	SetWindowText(m_caption.c_str());
	SetItemText(IDC_EDIT1, m_content.c_str());
	return true;
}

void InfoDialog::SetContent(mpt::ustring content)
{
	m_content = std::move(content);
}

void InfoDialog::SetCaption(mpt::ustring caption)
{
	m_caption = std::move(caption);
}

////////////////////////////////////////////////////////////////////////////////
// Sound Bank Information

CSoundBankProperties::CSoundBankProperties(const CDLSBank &bank, QWidget *parent)
	: InfoDialog(parent)
{
	const SOUNDBANKINFO &bi = bank.GetBankInfo();
	std::string info;
	info.reserve(128 + bi.szBankName.size() + bi.szDescription.size() + bi.szCopyRight.size() + bi.szEngineer.size() + bi.szSoftware.size() + bi.szComments.size());
	info = "Type:\t" + std::string((bank.GetBankType() & SOUNDBANK_TYPE_SF2) ? "Sound Font (SF2)" : "Downloadable Sound (DLS)");
	if (bi.szBankName.size())
		info += "\r\nName:\t" + bi.szBankName;
	if (bi.szDescription.size())
		info += "\r\n\t" + bi.szDescription;
	if (bi.szCopyRight.size())
		info += "\r\nCopyright:\t" + bi.szCopyRight;
	if (bi.szEngineer.size())
		info += "\r\nAuthor:\t" + bi.szEngineer;
	if (bi.szSoftware.size())
		info += "\r\nSoftware:\t" + bi.szSoftware;
	if (bi.szComments.size())
		info += "\r\n\r\nComments:\r\n" + bi.szComments;
	SetCaption((bank.GetFileName().ToWide() + U_(" - Sound Bank Information")));
	SetContent(mpt::ToUnicode(mpt::Charset::Locale, info));
}


////////////////////////////////////////////////////////////////////////////////////////////
// Keyboard Control

static constexpr uint8 whitetab[7] = {0,2,4,5,7,9,11};
static constexpr uint8 blacktab[7] = {0xff,1,3,0xff,6,8,10};

void CKeyboardControl::RegisterMessageMap()
{
	mfcless::Window<QWidget>::RegisterMessageMap();
	AddMessage(WM_DPICHANGED, &CKeyboardControl::OnDPIChanged);
	// ON_WM_DESTROY(): by hand (Qt event override)
	// ON_WM_PAINT() (mfcless::Window)
	// ON_WM_MOUSEMOVE() (mfcless::Window)
	// ON_WM_LBUTTONDOWN() (mfcless::Window)
	// ON_WM_LBUTTONUP() (mfcless::Window)
}


void CKeyboardControl::Init(QWidget *parent, int octaves, bool cursorNotify)
{
	m_parent = parent;
	m_nOctaves = std::max(1, octaves);
	m_cursorNotify = cursorNotify;
	MemsetZero(KeyFlags);
	MemsetZero(m_sampleNum);
	OnDPIChanged(0, 0);
}


intptr_t CKeyboardControl::OnDPIChanged(uintptr_t, intptr_t)
{
	// Point size to pixels
	int fontSize = -MulDiv(60, HighDPISupport::GetDpiForWindow(m_hWnd), 720);
	m_font.DeleteObject();
	m_font.CreateFont(fontSize, 0, 0, 0, FW_NORMAL, false, false, false, DEFAULT_CHARSET, OUT_RASTER_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH | FF_DONTCARE, U_("MS Shell Dlg"));
	return 0;
}


void CKeyboardControl::OnDestroy()
{
	m_font.DeleteObject();
}


void CKeyboardControl::DrawKey(mfcless::PaintDC &dc, const mfcless::Rect rect, int key, bool black) const
{
	const bool selected = (key == m_nSelection);
	uint32 color = black ? mfcless::Rgb(20, 20, 20) : mfcless::Rgb(255, 255, 255);
	if(m_mouseDown && selected)
		color = black ? mfcless::Rgb(104, 104, 104) : mfcless::Rgb(212, 212, 212);
	else if(selected)
		color = black ? mfcless::Rgb(130, 130, 130) : mfcless::Rgb(228, 228, 228);
	dc.SetDCBrushColor(color);
	dc.Rectangle(&rect);

	if(static_cast<size_t>(key) < std::size(KeyFlags) && KeyFlags[key] != KEYFLAG_NORMAL)
	{
		const int margin = black ? 0 : 2;
		mfcless::Rect ellipseRect(rect.left + margin, rect.bottom - rect.Width() + margin, rect.right - margin, rect.bottom - margin);
		dc.SetDCBrushColor((KeyFlags[key] & KEYFLAG_BRIGHTDOT) ? mfcless::Rgb(255, 192, 192) : mfcless::Rgb(255, 0, 0));
		dc.Ellipse(ellipseRect);
		if(m_sampleNum[key] != 0)
		{
			dc.SetTextColor((KeyFlags[key] & KEYFLAG_BRIGHTDOT) ? mfcless::Rgb(0, 0, 0) : mfcless::Rgb(255, 255, 255));
			mpt::uchar s[16];
			wsprintf(s, U_("%u"), m_sampleNum[key]);
			dc.DrawText(s, -1, ellipseRect, mfcless::dt::Center | mfcless::dt::SingleLine | mfcless::dt::VCenter);
		}

		if(KeyFlags[key] == (KEYFLAG_REDDOT | KEYFLAG_BRIGHTDOT))
		{
			// Both flags set: Draw second dot
			ellipseRect.MoveToY(ellipseRect.top - ellipseRect.Height() - 2);
			dc.SetDCBrushColor(mfcless::Rgb(255, 0, 0));
			dc.Ellipse(ellipseRect);
		}
	}
}


void CKeyboardControl::OnPaint()
{
	mfcless::Rect rcClient, rect;
	mfcless::PaintDC dc(this);

	dc.SetBkMode(mfcless::bk::Transparent);
	GetClientRect(&rcClient);
	rect = rcClient;
	auto oldBrush = dc.SelectObject(GetStockObject(DC_BRUSH));
	auto oldPen = dc.SelectObject(GetStockObject(DC_PEN));
	auto oldFont = dc.SelectObject(&m_font);

	// Rectangle outline
	dc.SetDCPenColor(mfcless::Rgb(50, 50, 50));

	// White notes
	for(int note = 0; note < m_nOctaves * 7; note++)
	{
		rect.right = ((note + 1) * rcClient.Width()) / (m_nOctaves * 7);
		int val = (note / 7) * 12 + whitetab[note % 7];

		DrawKey(dc, rect, val, false);
		
		rect.left = rect.right - 1;
	}

	// Black notes
	rect = rcClient;
	rect.bottom -= rcClient.Height() / 3;
	for(int note = 0; note < m_nOctaves * 7; note++)
	{
		switch(note % 7)
		{
		case 1:
		case 2:
		case 4:
		case 5:
		case 6:
		{
			rect.left = (note * rcClient.Width()) / (m_nOctaves * 7);
			rect.right = rect.left;
			int delta = rcClient.Width() / (m_nOctaves * 7 * 3);
			rect.left -= delta;
			rect.right += delta;
			int val = (note / 7) * 12 + blacktab[note % 7];

			DrawKey(dc, rect, val, true);
			break;
		}
		}
	}

	dc.SelectObject(oldBrush);
	dc.SelectObject(oldPen);
	dc.SelectObject(oldFont);
}


void CKeyboardControl::OnMouseMove(uint32 flags, mfcless::Point point)
{
	mfcless::Rect rcClient, rect;
	GetClientRect(&rcClient);
	rect = rcClient;
	int xmin = rcClient.right;
	int xmax = rcClient.left;
	int sel = -1;
	// White notes
	for(int note = 0; note < m_nOctaves * 7; note++)
	{
		int val = (note / 7) * 12 + whitetab[note % 7];
		rect.right = ((note + 1) * rcClient.Width()) / (m_nOctaves * 7);
		if (val == m_nSelection)
		{
			if (rect.left < xmin) xmin = rect.left;
			if (rect.right > xmax) xmax = rect.right;
		}
		if (rect.PtInRect(point))
		{
			sel = val;
			if (rect.left < xmin) xmin = rect.left;
			if (rect.right > xmax) xmax = rect.right;
		}
		rect.left = rect.right - 1;
	}
	// Black notes
	rect = rcClient;
	rect.bottom -= rcClient.Height() / 3;
	for(int note = 0; note < m_nOctaves * 7; note++)
	{
		switch(note % 7)
		{
		case 1:
		case 2:
		case 4:
		case 5:
		case 6:
		{
			int val = (note / 7) * 12 + blacktab[note % 7];
			rect.left = (note * rcClient.Width()) / (m_nOctaves * 7);
			rect.right = rect.left;
			int delta = rcClient.Width() / (m_nOctaves * 7 * 3);
			rect.left -= delta;
			rect.right += delta;
			if(val == m_nSelection)
			{
				if(rect.left < xmin)
					xmin = rect.left;
				if(rect.right > xmax)
					xmax = rect.right;
			}
			if(rect.PtInRect(point))
			{
				sel = val;
				if(rect.left < xmin)
					xmin = rect.left;
				if(rect.right > xmax)
					xmax = rect.right;
			}
			break;
		}
		}
	}
	// Check for selection change
	if(sel != m_nSelection)
	{
		m_nSelection = sel;
		rcClient.left = xmin;
		rcClient.right = xmax;
		InvalidateRect(&rcClient, false);
		if(m_cursorNotify && m_parent)
		{
			m_parent->PostMessage(WM_MOD_KBDNOTIFY, KBDNOTIFY_MOUSEMOVE, m_nSelection);
			if(flags & mfcless::MouseKey::Left)
				m_parent->SendMessage(WM_MOD_KBDNOTIFY, KBDNOTIFY_LBUTTONDOWN, m_nSelection);
		}
	}
	if(sel >= 0)
	{
		if(!m_mouseCapture)
		{
			m_mouseCapture = true;
			SetCapture();
		}
	} else
	{
		if(m_mouseCapture)
		{
			m_mouseCapture = false;
			ReleaseCapture();
		}
	}
}


void CKeyboardControl::OnLButtonDown(uint32, mfcless::Point)
{
	m_mouseDown = true;
	InvalidateRect(nullptr, false);
	if(m_parent)
		m_parent->SendMessage(WM_MOD_KBDNOTIFY, KBDNOTIFY_LBUTTONDOWN, m_nSelection);
}


void CKeyboardControl::OnLButtonUp(uint32, mfcless::Point)
{
	m_mouseDown = false;
	InvalidateRect(nullptr, false);
	if(m_parent)
		m_parent->SendMessage(WM_MOD_KBDNOTIFY, KBDNOTIFY_LBUTTONUP, m_nSelection);
}


////////////////////////////////////////////////////////////////////////////////
//
// Sample Map
//

void CSampleMapDlg::RegisterMessageMap()
{
	DialogBase::RegisterMessageMap();
	AddMessage(WM_MOD_KBDNOTIFY, &CSampleMapDlg::OnKeyboardNotify);
	AddScroll(Qt::Horizontal, &CSampleMapDlg::OnHScroll);
	AddCommand(IDC_CHECK1, &CSampleMapDlg::OnUpdateSamples);
	AddNotify(IDC_COMBO1, mfcless::Notify::SelChange, &CSampleMapDlg::OnUpdateKeyboard);
}

void CSampleMapDlg::DoDataExchange(DataExchange &dx)
{
	DialogBase::DoDataExchange(dx);
	//{{AFX_DATA_MAP(CSampleMapDlg)
	ExchangeControl(dx, IDC_KEYBOARD1, m_Keyboard);
	ExchangeControl(dx, IDC_COMBO1, m_CbnSample);
	ExchangeControl(dx, IDC_SLIDER1, m_SbOctave);
	//}}AFX_DATA_MAP
}


CSampleMapDlg::CSampleMapDlg(CSoundFile &sf, INSTRUMENTINDEX nInstr, QWidget *parent)
	: DialogBase{IDD_EDITSAMPLEMAP, parent}
	, sndFile{sf}
	, m_nInstrument{nInstr}
{
}


bool CSampleMapDlg::OnInitDialog()
{
	DialogBase::OnInitDialog();
	ModInstrument *pIns = sndFile.Instruments[m_nInstrument];
	if(pIns)
		KeyboardMap = pIns->Keyboard;
	m_Keyboard->Init(this, 3, true);
	m_SbOctave->setRange(0, 7);
	m_SbOctave->setValue(4);
	OnUpdateSamples();
	OnUpdateOctave();
	return true;
}


void CSampleMapDlg::OnDPIChanged()
{
	DialogBase::OnDPIChanged();
	m_Keyboard->SendMessage(WM_DPICHANGED);
}


void CSampleMapDlg::OnHScroll(mfcless::ScrollCode code, int pos, QWidget *pBar)
{
	DialogBase::OnHScroll(nCode, nPos, pBar);
	OnUpdateKeyboard();
	OnUpdateOctave();
}


void CSampleMapDlg::OnUpdateSamples()
{
	uint32 oldPos = 0;
	uint32 newPos = 0;

	if(m_nInstrument >= MAX_INSTRUMENTS)
		return;
	if(m_CbnSample->count() > 0)
		oldPos = static_cast<uint32>(mfcless::GetItemData(m_CbnSample, m_CbnSample->currentIndex()));
	m_CbnSample->setUpdatesEnabled(false);
	m_CbnSample->clear();
	const bool showAll = (IsChecked(IDC_CHECK1) != false) || (*std::max_element(std::begin(KeyboardMap), std::end(KeyboardMap)) == 0);

	uint32 insertPos = mfcless::AddItem(m_CbnSample, U_("0: No sample"));
	mfcless::SetItemData(m_CbnSample, insertPos, 0);

	for(SAMPLEINDEX i = 1; i <= sndFile.GetNumSamples(); i++)
	{
		bool isUsed = showAll || mpt::contains(KeyboardMap, i);
		if(isUsed)
		{
			mpt::ustring sampleName;
			sampleName.Format(U_("%d: %s"), i, mpt::ToUnicode(sndFile.GetCharsetInternal(), sndFile.GetSampleName(i)).GetString());
			insertPos = mfcless::AddItem(m_CbnSample, sampleName);

			mfcless::SetItemData(m_CbnSample, insertPos, i);
			if(i == oldPos)
				newPos = insertPos;
		}
	}
	m_CbnSample->setUpdatesEnabled(true);
	m_CbnSample->setCurrentIndex(newPos);
	OnUpdateKeyboard();
}


void CSampleMapDlg::OnUpdateOctave()
{
	mpt::uchar s[64];
	const uint32 baseOctave = m_SbOctave->value() & 7;
	wsprintf(s, U_("Octaves %u-%u"), baseOctave, baseOctave + 2);
	SetItemText(IDC_TEXT1, s);
}



void CSampleMapDlg::OnUpdateKeyboard()
{
	SAMPLEINDEX nSample = static_cast<SAMPLEINDEX>(mfcless::GetItemData(m_CbnSample, m_CbnSample->currentIndex()));
	const uint32 baseOctave = m_SbOctave->value() & 7;
	bool redraw = false;
	for(uint32 iNote = 0; iNote < 3 * 12; iNote++)
	{
		uint8 oldFlags = m_Keyboard->GetFlags(iNote);
		SAMPLEINDEX oldSmp = m_Keyboard->GetSample(iNote);
		uint32 ndx = baseOctave * 12 + iNote;
		uint8 newFlags = CKeyboardControl::KEYFLAG_NORMAL;
		if(KeyboardMap[ndx] == nSample)
			newFlags = CKeyboardControl::KEYFLAG_REDDOT;
		else if(KeyboardMap[ndx] != 0)
			newFlags = CKeyboardControl::KEYFLAG_BRIGHTDOT;
		if(newFlags != oldFlags || oldSmp != KeyboardMap[ndx])
		{
			m_Keyboard->SetFlags(iNote, newFlags);
			m_Keyboard->SetSample(iNote, KeyboardMap[ndx]);
			redraw = true;
		}
	}
	if(redraw)
		m_Keyboard->InvalidateRect(NULL, false);
}


intptr_t CSampleMapDlg::OnKeyboardNotify(uintptr_t wParam, intptr_t lParam)
{
	mpt::uchar s[32] = U_("--");

	if((lParam >= 0) && (lParam < 3 * 12))
	{
		const SAMPLEINDEX sample = static_cast<SAMPLEINDEX>(mfcless::GetItemData(m_CbnSample, m_CbnSample->currentIndex()));
		const uint32 baseOctave = m_SbOctave->value() & 7;

		const mpt::ustring temp = mpt::ToUnicode(sndFile.GetNoteName(static_cast<ModCommand::NOTE>(lParam + 1 + 12 * baseOctave), m_nInstrument));
		if(temp.GetLength() >= mpt::saturate_cast<int>(std::size(s)))
			wsprintf(s, U_("%s"), U_("..."));
		else
			wsprintf(s, U_("%s"), temp.GetString());

		ModInstrument *pIns = sndFile.Instruments[m_nInstrument];
		if((wParam == KBDNOTIFY_LBUTTONDOWN) && (sample < MAX_SAMPLES) && (pIns))
		{
			const uint32 note = static_cast<uint32>(baseOctave * 12 + lParam);

			if(mouseAction == MouseAction::Unknown)
			{
				// Mouse down -> decide if we are going to set or remove notes
				mouseAction = MouseAction::Set;
				if(KeyboardMap[note] == sample)
				{
					mouseAction = (KeyboardMap[note] == pIns->Keyboard[note]) ? MouseAction::Zero : MouseAction::Unset;
				}
			}

			switch(mouseAction)
			{
			case MouseAction::Unknown:
			case MouseAction::Set:
				KeyboardMap[note] = sample;
				break;
			case MouseAction::Unset:
				KeyboardMap[note] = pIns->Keyboard[note];
				break;
			case MouseAction::Zero:
				if(KeyboardMap[note] == sample)
					KeyboardMap[note] = 0;
				break;
			}
			OnUpdateKeyboard();
		}
	}
	
	if(wParam == KBDNOTIFY_LBUTTONUP)
		mouseAction = MouseAction::Unknown;
	
	SetItemText(IDC_TEXT2, s);
	return 0;
}


void CSampleMapDlg::OnOK()
{
	ModInstrument *pIns = sndFile.Instruments[m_nInstrument];
	if(pIns)
	{
		bool modified = false;
		for(uint32 i = 0; i < NOTE_MAX; i++)
		{
			if(KeyboardMap[i] != pIns->Keyboard[i])
			{
				pIns->Keyboard[i] = KeyboardMap[i];
				modified = true;
			}
		}
		if(modified)
		{
			DialogBase::OnOK();
			return;
		}
	}
	DialogBase::OnCancel();
}


////////////////////////////////////////////////////////////////////////////////////////////
// Edit history dialog

void CEditHistoryDlg::RegisterMessageMap()
{
	ResizableDialog::RegisterMessageMap();
	AddCommand(IDC_BTN_CLEAR, &CEditHistoryDlg::OnClearHistory);
}


CEditHistoryDlg::CEditHistoryDlg(QWidget *parent, CModDoc &modDoc)
	: ResizableDialog{IDD_EDITHISTORY, parent}
	, m_modDoc{modDoc}
{
}


bool CEditHistoryDlg::OnInitDialog()
{
	ResizableDialog::OnInitDialog();

	mpt::ustring s;
	uint64 totalTime = 0;
	const auto &editHistory = m_modDoc.GetSoundFile().GetFileHistory();
	const bool isEmpty = editHistory.empty();

	for(const auto &entry : editHistory)
	{
		totalTime += entry.openTime;
		// Date
		mpt::ustring sDate = mpt::ustring(U_("<unknown date>"));
		if(entry.HasValidDate())
		{
			const mpt::Date::Unix unixdate = ((m_modDoc.GetSoundFile().GetTimezoneInternal() == mpt::Date::LogicalTimezone::Local) || (m_modDoc.GetSoundFile().GetTimezoneInternal() == mpt::Date::LogicalTimezone::Unspecified))
				? mpt::Date::UnixFromLocal(mpt::Date::interpret_as_timezone<mpt::Date::LogicalTimezone::Local>(entry.loadDate))
				: mpt::Date::UnixFromUTC(mpt::Date::interpret_as_timezone<mpt::Date::LogicalTimezone::UTC>(entry.loadDate));
				;
			sDate = CTime(mpt::Date::UnixAsSeconds(unixdate)).Format(U_("%d %b %Y, %H:%M:%S"));
		}
		// Time + stuff
		uint32 duration = mpt::saturate_round<uint32>(entry.openTime / HISTORY_TIMER_PRECISION);
		s += MPT_UFORMAT("Loaded {}, open for {}h {}m {}s\r\n")(
			sDate, mpt::ufmt::dec(duration / 3600), mpt::ufmt::dec0<2>((duration / 60) % 60), mpt::ufmt::dec0<2>(duration % 60));
	}
	if(isEmpty)
	{
		s = U_("No information available about the previous edit history of this module.");
	}
	SetItemText(IDC_EDIT_HISTORY, s);

	// Total edit time
	s.Empty();
	if(totalTime)
	{
		totalTime = mpt::saturate_round<uint64>(totalTime / HISTORY_TIMER_PRECISION);

		s.Format(U_("Total edit time: %lluh %02llum %02llus (%zu session%s)"), totalTime / 3600, (totalTime / 60) % 60, totalTime % 60, editHistory.size(), (editHistory.size() != 1) ? U_("s") : U_(""));
		SetItemText(IDC_TOTAL_EDIT_TIME, s);
		// Window title
		s.Format(U_("Edit History for %s"), m_modDoc.GetTitle().GetString());
		SetWindowText(s);
	}
	// Enable or disable Clear button
	EnableItem(IDC_BTN_CLEAR, isEmpty ? false : true);

	return true;
}


void CEditHistoryDlg::OnClearHistory()
{
	if(!m_modDoc.GetSoundFile().GetFileHistory().empty())
	{
		m_modDoc.GetSoundFile().GetFileHistory().clear();
		m_modDoc.SetModified();
		OnInitDialog();
	}
}


/////////////////////////////////////////////////////////////////////////
// Generic input dialog

void CInputDlg::DoDataExchange(DataExchange &dx)
{
	DialogBase::DoDataExchange(dx);
	if(m_minValueInt == m_maxValueInt && m_minValueDbl == m_maxValueDbl)
	{
		// Only need this for freeform text
		ExchangeControl(dx, IDC_EDIT1, m_edit);
	}
	ExchangeControl(dx, IDC_SPIN1, m_spin);
}


CInputDlg::CInputDlg(QWidget *parent, const mpt::uchar *desc, const mpt::uchar *defaultString, int32 maxLength, double minValDbl, double maxValDbl, double defaultDbl, int32 minValInt, int32 maxValInt, int32 defaultInt)
	: DialogBase{IDD_INPUT, parent}
	, m_description{desc}
	, m_minValueDbl{minValDbl}
	, m_maxValueDbl{maxValDbl}
	, m_minValueInt{minValInt}
	, m_maxValueInt{maxValInt}
	, m_maxLength{maxLength}
	, resultAsInt{defaultInt}
	, resultAsDouble{defaultDbl}
	, resultAsString{defaultString}
{
}


bool CInputDlg::OnInitDialog()
{
	DialogBase::OnInitDialog();
	SetItemText(IDC_PROMPT, m_description);

	// Get all current control sizes and positions
	mfcless::Rect windowRect, labelRect, inputRect, okRect, cancelRect;
	GetWindowRect(windowRect);
	GetDlgItem(IDC_PROMPT)->GetWindowRect(labelRect);
	GetDlgItem(IDC_EDIT1)->GetWindowRect(inputRect);
	GetDlgItem(IDOK)->GetWindowRect(okRect);
	GetDlgItem(IDCANCEL)->GetWindowRect(cancelRect);
	ScreenToClient(labelRect);
	ScreenToClient(inputRect);
	ScreenToClient(okRect);
	ScreenToClient(cancelRect);

	// Find out how big our label shall be
	mfcless::DC * dc = ::GetDC(m_hWnd);
	mfcless::Rect textRect(0,0,0,0);
	DrawText(dc, m_description, m_description.GetLength(), textRect, mfcless::dt::CalcRect);
	LPtoDP(dc, &textRect.BottomRight(), 1);
	::ReleaseDC(m_hWnd, dc);
	if(textRect.right < 320) textRect.right = 320;
	const int windowWidth = windowRect.Width() - labelRect.Width() + textRect.right;
	const int windowHeight = windowRect.Height() - labelRect.Height() + textRect.bottom;

	// Resize and move all controls
	GetDlgItem(IDC_PROMPT)->SetWindowPos(nullptr, 0, 0, textRect.right, textRect.bottom, SWP_NOMOVE | SWP_NOZORDER);
	GetDlgItem(IDC_EDIT1)->SetWindowPos(nullptr, inputRect.left, labelRect.top + textRect.bottom + (inputRect.top - labelRect.bottom), textRect.right, inputRect.Height(), SWP_NOZORDER);
	GetDlgItem(IDOK)->SetWindowPos(nullptr, windowWidth - (windowRect.Width() - okRect.left), windowHeight - (windowRect.Height() - okRect.top), 0, 0, SWP_NOSIZE | SWP_NOZORDER);
	GetDlgItem(IDCANCEL)->SetWindowPos(nullptr, windowWidth - (windowRect.Width() - cancelRect.left), windowHeight - (windowRect.Height() - cancelRect.top), 0, 0, SWP_NOSIZE | SWP_NOZORDER);
	SetWindowPos(nullptr, 0, 0, windowWidth, windowHeight, SWP_NOMOVE | SWP_NOZORDER);

	if(m_minValueInt != m_maxValueInt)
	{
		// Numeric (int)
		m_spin->SetRange(m_minValueInt, m_maxValueInt);
		m_edit->SubclassDlgItem(IDC_EDIT1, this);
		m_edit->ModifyStyle(0, ES_NUMBER);
		m_edit->AllowNegative(m_minValueInt < 0);
		m_edit->AllowFractions(false);
		SetItemInt(IDC_EDIT1, resultAsInt);
		m_spin->SetBuddy(m_edit);
	} else if(m_minValueDbl != m_maxValueDbl)
	{
		// Numeric (double)
		m_spin->SetRange(static_cast<int32>(m_minValueDbl), static_cast<int32>(m_maxValueDbl));
		m_edit->SubclassDlgItem(IDC_EDIT1, this);
		m_edit->ModifyStyle(0, ES_NUMBER);
		m_edit->AllowNegative(m_minValueDbl < 0);
		m_edit->AllowFractions(true);
		m_edit->SetDecimalValue(resultAsDouble);
		m_spin->SetBuddy(m_edit);
	} else
	{
		// Text
		m_spin->setVisible(false);
		if(m_maxLength > 0)
			Edit_LimitText(m_edit, m_maxLength);
		SetItemText(IDC_EDIT1, resultAsString);
	}

	return true;
}


void CInputDlg::OnOK()
{
	DialogBase::OnOK();
	resultAsString = GetItemText(IDC_EDIT1);
	resultAsInt = static_cast<int32>(GetItemInt(IDC_EDIT1));
	Limit(resultAsInt, m_minValueInt, m_maxValueInt);
	m_edit->GetDecimalValue(resultAsDouble);
	Limit(resultAsDouble, m_minValueDbl, m_maxValueDbl);
}


///////////////////////////////////////////////////////////////////////////////////////
// Messagebox with 'don't show again'-option.

struct MsgBoxHidableMessage
{
	const mpt::uchar *mainTitle;
	const mpt::uchar *message;
	const uint32 mask;
	const bool defaultDontShowAgainStatus; // true for don't show again, false for show again.
};

static constexpr MsgBoxHidableMessage HidableMessages[] =
{
	{ U_("Compatibility Notice"), U_("The first two bytes of oneshot samples are silenced for ProTracker compatibility."), 1, true },
	{ U_("Compatibility Hint"), U_("To create IT files without OpenMPT-specific extensions included, try compatibility export from File menu."), 1 << 1, true },
	{ U_("Compatibility Hint"), U_("To create XM files without OpenMPT-specific extensions included, try compatibility export from File menu."), 1 << 3, true },
	{ U_("Compatibility Notice"), U_("The exported file will not contain any of OpenMPT's file format hacks."), 1 << 4, true },
};

static_assert(mpt::array_size<decltype(HidableMessages)>::size == enMsgBoxHidableMessage_count);

// Messagebox with 'don't show this again'-checkbox. Uses parameter 'enMsg'
// to get the needed information from message array, and updates the variable that
// controls the show/don't show-flags.
void MsgBoxHidable(enMsgBoxHidableMessage enMsg)
{
	const auto &msg = HidableMessages[enMsg];
	if((TrackerSettings::Instance().gnMsgBoxVisiblityFlags & msg.mask) == 0)
		return;

#if MPT_WINNT_AT_LEAST(MPT_WIN_VISTA) && defined(UNICODE)
	if(CTaskDialog::IsSupported()
	   && !(mpt::OS::Windows::IsWine() && theApp.GetWineVersion()->Version().IsBefore(mpt::OS::Wine::Version(3, 13, 0))))
	{
		CTaskDialog taskDialog(msg.message, msg.mainTitle ? mpt::ustring{msg.mainTitle} : mpt::ustring{}, AfxGetAppName(), TDCBF_OK_BUTTON);
		taskDialog.SetVerificationCheckboxText(U_("Do not show this message again"));
		taskDialog.SetVerificationCheckbox(msg.defaultDontShowAgainStatus);
		taskDialog.DoModal();

		if(taskDialog.GetVerificationCheckboxState())
			TrackerSettings::Instance().gnMsgBoxVisiblityFlags &= ~msg.mask;
		else
			TrackerSettings::Instance().gnMsgBoxVisiblityFlags |= msg.mask;
	} else
#endif
	{
		if(Reporting::Confirm(msg.message + mpt::ustring(U_("\n\nShow this message again?")), msg.mainTitle ? mpt::ustring{msg.mainTitle} : mpt::ustring{}, msg.defaultDontShowAgainStatus) == cnfNo)
			TrackerSettings::Instance().gnMsgBoxVisiblityFlags &= ~msg.mask;
		else
			TrackerSettings::Instance().gnMsgBoxVisiblityFlags |= msg.mask;
	}
}


/////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////


void AppendNotesToControl(CComboBox& combobox, ModCommand::NOTE noteStart, ModCommand::NOTE noteEnd)
{
	const ModCommand::NOTE upperLimit = std::min(ModCommand::NOTE(NOTE_MAX), noteEnd);
	for(ModCommand::NOTE note = noteStart; note <= upperLimit; note++)
		combobox.SetItemData(combobox.AddString(mpt::ToUnicode(CSoundFile::GetNoteName(note, CSoundFile::GetDefaultNoteNames()))), note);
}


void AppendNotesToControlEx(CComboBox& combobox, const CSoundFile &sndFile, INSTRUMENTINDEX nInstr, ModCommand::NOTE noteStart, ModCommand::NOTE noteEnd)
{
	bool addSpecial = noteStart == noteEnd;
	if(noteStart == noteEnd)
	{
		noteStart = sndFile.GetModSpecifications().noteMin;
		noteEnd = sndFile.GetModSpecifications().noteMax;
	}
	for(ModCommand::NOTE note = noteStart; note <= noteEnd; note++)
	{
		combobox.SetItemData(combobox.AddString(mpt::ToUnicode(sndFile.GetNoteName(note, nInstr))), note);
	}
	if(addSpecial)
	{
		for(ModCommand::NOTE note = NOTE_MIN_SPECIAL - 1; note++ < NOTE_MAX_SPECIAL;)
		{
			if(sndFile.GetModSpecifications().HasNote(note))
				combobox.SetItemData(combobox.AddString(szSpecialNoteNamesMPT[note - NOTE_MIN_SPECIAL]), note);
		}
	}
}


OPENMPT_NAMESPACE_END
