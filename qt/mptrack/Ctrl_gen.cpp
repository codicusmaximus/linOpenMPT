/*
 * Ctrl_gen.cpp
 * ------------
 * Purpose: General tab, upper panel.
 * Notes  : mfcless twin of upstream's mptrack/Ctrl_gen.cpp.
 *          Draft from qt/tools/twin.py, not yet edited by hand.
 * Authors: Olivier Lapicque
 *          OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "Ctrl_gen.h"
#include "dlg_misc.h"
#include "Globals.h"
#include "InputHandler.h"
#include "Mainfrm.h"
#include "Moddoc.h"
#include "Mptrack.h"
#include "resource.h"
#include "TrackerSettings.h"
#include "View_gen.h"
#include "WindowMessages.h"
#include "../common/misc_util.h"
#include "../misc/mptClock.h"
#include "../soundlib/mod_specifications.h"
#include "mpt/parse/parse.hpp"


OPENMPT_NAMESPACE_BEGIN


void CCtrlGeneral::RegisterMessageMap()
{
	CModControlDlg::RegisterMessageMap();
	AddScroll(Qt::Vertical, &CCtrlGeneral::OnVScroll);
	AddCommand(IDC_BUTTON1, &CCtrlGeneral::OnTapTempo);
	AddCommand(IDC_BUTTON_MODTYPE, &CCtrlGeneral::OnSongProperties);
	AddCommand(IDC_CHECK_LOOPSONG, &CCtrlGeneral::OnLoopSongChanged);
	AddNotify(IDC_EDIT_SONGTITLE, mfcless::Notify::EditChange, &CCtrlGeneral::OnTitleChanged);
	AddNotify(IDC_EDIT_ARTIST, mfcless::Notify::EditChange, &CCtrlGeneral::OnArtistChanged);
	AddNotify(IDC_EDIT_TEMPO, mfcless::Notify::EditChange, &CCtrlGeneral::OnTempoChanged);
	AddNotify(IDC_EDIT_SPEED, mfcless::Notify::EditChange, &CCtrlGeneral::OnSpeedChanged);
	AddNotify(IDC_EDIT_GLOBALVOL, mfcless::Notify::EditChange, &CCtrlGeneral::OnGlobalVolChanged);
	AddNotify(IDC_EDIT_RESTARTPOS, mfcless::Notify::EditChange, &CCtrlGeneral::OnRestartPosChanged);
	AddNotify(IDC_EDIT_VSTIVOL, mfcless::Notify::EditChange, &CCtrlGeneral::OnVSTiVolChanged);
	AddNotify(IDC_EDIT_SAMPLEPA, mfcless::Notify::EditChange, &CCtrlGeneral::OnSamplePAChanged);
	AddMessage(WM_MOD_UPDATEPOSITION, &CCtrlGeneral::OnUpdatePosition);
	AddNotify(IDC_EDIT_SONGTITLE, mfcless::Notify::SetFocus, &CCtrlGeneral::OnEnSetfocusEditSongtitle);
	AddNotify(IDC_EDIT_RESTARTPOS, mfcless::Notify::KillFocus, &CCtrlGeneral::OnRestartPosDone);
	AddNotify(IDC_COMBO1, mfcless::Notify::SelChange, &CCtrlGeneral::OnResamplingChanged);
}

void CCtrlGeneral::DoDataExchange(DataExchange &dx)
{
	CModControlDlg::DoDataExchange(dx);
	//{{AFX_DATA_MAP(CCtrlGeneral)
	ExchangeControl(dx, IDC_EDIT_SONGTITLE, m_EditTitle);
	ExchangeControl(dx, IDC_EDIT_ARTIST, m_EditArtist);
	//ExchangeControl(dx, IDC_EDIT_TEMPO, m_EditTempo);
	ExchangeControl(dx, IDC_SPIN_TEMPO, m_SpinTempo);
	ExchangeControl(dx, IDC_EDIT_SPEED, m_EditSpeed);
	ExchangeControl(dx, IDC_SPIN_SPEED, m_SpinSpeed);
	ExchangeControl(dx, IDC_EDIT_GLOBALVOL, m_EditGlobalVol);
	ExchangeControl(dx, IDC_SPIN_GLOBALVOL, m_SpinGlobalVol);
	ExchangeControl(dx, IDC_EDIT_VSTIVOL, m_EditVSTiVol);
	ExchangeControl(dx, IDC_SPIN_VSTIVOL, m_SpinVSTiVol);
	ExchangeControl(dx, IDC_EDIT_SAMPLEPA, m_EditSamplePA);
	ExchangeControl(dx, IDC_SPIN_SAMPLEPA, m_SpinSamplePA);
	ExchangeControl(dx, IDC_EDIT_RESTARTPOS, m_EditRestartPos);
	ExchangeControl(dx, IDC_SPIN_RESTARTPOS, m_SpinRestartPos);

	ExchangeControl(dx, IDC_SLIDER_SONGTEMPO, m_SliderTempo);
	ExchangeControl(dx, IDC_SLIDER_VSTIVOL, m_SliderVSTiVol);
	ExchangeControl(dx, IDC_SLIDER_GLOBALVOL, m_SliderGlobalVol);
	ExchangeControl(dx, IDC_SLIDER_SAMPLEPREAMP, m_SliderSamplePreAmp);

	ExchangeControl(dx, IDC_BUTTON_MODTYPE, m_BtnModType);
	ExchangeControl(dx, IDC_VUMETER_LEFT, m_VuMeterLeft);
	ExchangeControl(dx, IDC_VUMETER_RIGHT, m_VuMeterRight);

	ExchangeControl(dx, IDC_COMBO1, m_CbnResampling);
	//}}AFX_DATA_MAP
}


CCtrlGeneral::CCtrlGeneral(CModControlView &parent, CModDoc &document) : CModControlDlg(parent, document)
{
}


// Determine how the global volume slider should be scaled to actual global volume.
// Display range for XM / S3M should be 0...64, for other formats it's 0...256.
uint32 CCtrlGeneral::GetGlobalVolumeFactor() const
{
	return MAX_GLOBAL_VOLUME / m_sndFile.GlobalVolumeRange();
}


bool CCtrlGeneral::OnInitDialog()
{
	const auto &specs = m_sndFile.GetModSpecifications();
	CModControlDlg::OnInitDialog();
	// Song Title
	m_EditTitle->setMaxLength(specs.modNameLengthMax);

	m_SpinGlobalVol->SetRange(0, (short)(256 / GetGlobalVolumeFactor()));
	m_SpinSamplePA->SetRange(0, 2000);
	m_SpinVSTiVol->SetRange(0, 2000);
	m_SpinRestartPos->SetRange(0, ORDERINDEX_MAX);
	
	m_SliderGlobalVol->setRange(0, MAX_GLOBAL_VOLUME);
	m_SliderVSTiVol->setRange(0, MAX_SLIDER_VSTI_VOL);
	m_SliderSamplePreAmp->setRange(0, MAX_SLIDER_SAMPLE_VOL);

	m_SpinTempo->SetRange(-10, 10);
	m_SliderTempo->setSingleStep(1);
	m_SliderTempo->setPageStep(10);
	m_EditTempo->SubclassDlgItem(IDC_EDIT_TEMPO, this);
	m_EditTempo->AllowNegative(false);
	
	m_editsLocked = false;
	UpdateView(GeneralHint().ModType());
	OnActivatePage(0);
	m_initialized = true;
	
	return false;
}


const mfcless::ViewClass *CCtrlGeneral::GetAssociatedViewClass()
{
	return (&CViewGlobals::classCViewGlobals);
}


Setting<int32> &CCtrlGeneral::GetSplitPosRef() { return TrackerSettings::Instance().glGeneralWindowHeight; }


void CCtrlGeneral::RecalcLayout()
{
}


void CCtrlGeneral::OnActivatePage(intptr_t)
{
	m_modDoc.SetNotifications(Notification::Default);
	m_modDoc.SetFollowWnd(m_hWnd);
	PostViewMessage(VIEWMSG_SETACTIVE, NULL);

	// Combo boxes randomly disappear without this... why?
	Invalidate();
}


void CCtrlGeneral::OnDeactivatePage()
{
	m_modDoc.SetFollowWnd(NULL);
	m_VuMeterLeft->SetVuMeter(0, true);
	m_VuMeterRight->SetVuMeter(0, true);
	m_tapTimer = nullptr;  // Reset high-precision clock if required
}


TEMPO CCtrlGeneral::TempoSliderRange() const
{
	return (TEMPO_SPLIT_THRESHOLD - m_tempoMin) + TEMPO((m_tempoMax - TEMPO_SPLIT_THRESHOLD).GetInt() / TEMPO_SPLIT_PRECISION, 0);
}


TEMPO CCtrlGeneral::SliderToTempo(int value) const
{
	if(m_tempoMax < TEMPO_SPLIT_THRESHOLD)
	{
		return m_tempoMax - TEMPO(value, 0);
	} else
	{
		const auto tempoSliderSplit = TempoToSlider(TEMPO_SPLIT_THRESHOLD);
		if(value <= tempoSliderSplit)
			return m_tempoMax - TEMPO(value * TEMPO_SPLIT_PRECISION, 0);
		else
			return m_tempoMin + TempoSliderRange() - TEMPO(value, 0);
	}
}


int CCtrlGeneral::TempoToSlider(TEMPO tempo) const
{
	if(m_tempoMax < TEMPO_SPLIT_THRESHOLD)
	{
		return (m_tempoMax - tempo).GetInt();
	} else
	{
		if(tempo < TEMPO_SPLIT_THRESHOLD)
			return (TempoSliderRange() - (std::max(m_tempoMin, tempo) - m_tempoMin)).GetInt();
		else
			return (m_tempoMax - std::min(m_tempoMax, tempo)).GetInt() / TEMPO_SPLIT_PRECISION;
	}
}


void CCtrlGeneral::OnTapTempo()
{
	using TapType = decltype(m_tapTimer->Now());
	static std::array<TapType, 32> tapTime;
	static TapType lastTap = 0;
	static uint32 numTaps = 0;

	if(m_tapTimer == nullptr)
		m_tapTimer = std::make_unique<Util::MultimediaClock>(1);

	const uint32 now = m_tapTimer->Now();
	if(now - lastTap >= 2000)
		numTaps = 0;
	lastTap = now;

	if(static_cast<size_t>(numTaps) >= tapTime.size())
	{
		// Shift back the previously recorded tap history
		// cppcheck false-positive
		// cppcheck-suppress mismatchingContainers
		std::copy(tapTime.begin() + 1, tapTime.end(), tapTime.begin());
		numTaps = static_cast<uint32>(tapTime.size() - 1);
	}
	
	tapTime[numTaps++] = now;

	if(numTaps <= 1)
		return;

	// Now apply least squares to tap history
	double sum = 0.0, weightedSum = 0.0;
	for(uint32 i = 0; i < numTaps; i++)
	{
		const double tapMs = tapTime[i] / 1000.0;
		sum += tapMs;
		weightedSum += i * tapMs;
	}

	const double lengthSum = numTaps * (numTaps - 1) / 2;
	const double lengthSumSum = lengthSum * (2 * numTaps - 1) / 3.0;
	const double secondsPerBeat = (numTaps * weightedSum - lengthSum * sum) / (lengthSumSum * numTaps - lengthSum * lengthSum);

	double newTempo = 60.0 / secondsPerBeat;
	if(m_sndFile.m_nTempoMode != TempoMode::Modern)
		newTempo *= (m_sndFile.Order().GetDefaultSpeed() * m_sndFile.m_nDefaultRowsPerBeat) / 24.0;
	if(!m_sndFile.GetModSpecifications().hasFractionalTempo)
		newTempo = std::round(newTempo);
	TEMPO t(newTempo);
	Limit(t, m_tempoMin, m_tempoMax);
	m_EditTempo->SetTempoValue(t);
}


void CCtrlGeneral::UpdateView(UpdateHint hint, QObject *pHint)
{
	if (pHint == this) return;
	FlagSet<HintType> hintType = hint.GetType();
	const bool updateAll = hintType[HINT_MODTYPE];

	const auto resamplingModes = Resampling::AllModes();

	LockControls();
	if (hintType == HINT_MPTOPTIONS || updateAll)
	{
		mpt::ustring defaultResampler;
		if(m_sndFile.m_SongFlags[SONG_ISAMIGA] && TrackerSettings::Instance().ResamplerEmulateAmiga != Resampling::AmigaFilter::Off)
			defaultResampler = U_("Amiga Resampler");
		else
			defaultResampler = CTrackApp::GetResamplingModeName(TrackerSettings::Instance().ResamplerMode, 1, false);

		m_CbnResampling->clear();
		mfcless::SetItemData(m_CbnResampling, mfcless::AddItem(m_CbnResampling, U_("Default (") + defaultResampler + U_(")")), SRCMODE_DEFAULT);
		for(auto mode : resamplingModes)
		{
			mfcless::SetItemData(m_CbnResampling, mfcless::AddItem(m_CbnResampling, CTrackApp::GetResamplingModeName(mode, 2, true)), mode);
		}
		m_CbnResampling->update(false);
	}

	if(updateAll)
	{
		const auto &specs = m_sndFile.GetModSpecifications();

		// S3M HACK: ST3 will ignore speed 255, even though it can be used with Axx.
		if(m_sndFile.GetType() == MOD_TYPE_S3M)
			m_SpinSpeed->SetRange(1, 254);
		else
			m_SpinSpeed->SetRange(specs.speedMin, specs.speedMax);

		m_tempoMin = specs.GetTempoMin();
		m_tempoMax = specs.GetTempoMax();
		// IT Hack: There are legacy OpenMPT-made ITs out there which use a higher default speed than 255.
		// Changing the upper tempo limit in the mod specs would break them, so do it here instead.
		if(m_sndFile.GetType() == MOD_TYPE_IT && m_sndFile.Order().GetDefaultTempo() <= TEMPO(255, 0))
			m_tempoMax.Set(255);
		// Lower resolution for BPM above 256
		if(m_tempoMax >= TEMPO_SPLIT_THRESHOLD)
			m_SliderTempo->setRange(0, TempoSliderRange().GetInt());
		else
			m_SliderTempo->setRange(0, m_tempoMax.GetInt() - m_tempoMin.GetInt());
		m_EditTempo->AllowFractions(specs.hasFractionalTempo);

		const bool bIsNotMOD = (m_sndFile.GetType() != MOD_TYPE_MOD);
		const bool bIsNotMOD_XM = ((bIsNotMOD) && (m_sndFile.GetType() != MOD_TYPE_XM));
		m_EditArtist->setEnabled(specs.hasArtistName);
		m_EditTempo->setEnabled(bIsNotMOD);
		m_SpinTempo->setEnabled(bIsNotMOD);
		EnableItem(IDC_BUTTON1, bIsNotMOD);
		m_SliderTempo->setEnabled(bIsNotMOD);
		m_EditSpeed->setEnabled(bIsNotMOD);
		m_SpinSpeed->setEnabled(bIsNotMOD);
		const bool globalVol = bIsNotMOD_XM || m_sndFile.m_nDefaultGlobalVolume != MAX_GLOBAL_VOLUME;
		m_SliderGlobalVol->setEnabled(globalVol);
		m_EditGlobalVol->setEnabled(globalVol);
		m_SpinGlobalVol->setEnabled(globalVol);
		m_EditSamplePA->setEnabled(bIsNotMOD);
		m_SpinSamplePA->setEnabled(bIsNotMOD);
		m_SliderVSTiVol->setEnabled(bIsNotMOD);
		m_EditVSTiVol->setEnabled(bIsNotMOD);
		m_SpinVSTiVol->setEnabled(bIsNotMOD);
		m_EditRestartPos->setEnabled((specs.hasRestartPos || m_sndFile.Order().GetRestartPos() != 0));
		m_SpinRestartPos->setEnabled(m_EditRestartPos->isEnabled());

		//Note: Sample volume slider is not disabled for MOD
		//on purpose (can be used to control play volume)
	}

	if(updateAll || (hint.GetCategory() == HINTCAT_GLOBAL && hintType[HINT_MODCHANNELS]))
	{
		// MOD Type
		mpt::ustring modType;
		switch(m_sndFile.GetType())
		{
		case MOD_TYPE_MOD:	modType = U_("MOD (ProTracker)"); break;
		case MOD_TYPE_S3M:	modType = U_("S3M (Scream Tracker)"); break;
		case MOD_TYPE_XM:	modType = U_("XM (FastTracker 2)"); break;
		case MOD_TYPE_IT:	modType = U_("IT (Impulse Tracker)"); break;
		case MOD_TYPE_MPT:	modType = U_("MPTM (OpenMPT)"); break;
		default:			modType = MPT_UFORMAT("{} ({})")(mpt::ToUpperCase(m_sndFile.m_modFormat.type), m_sndFile.m_modFormat.formatName); break;
		}
		mpt::ustring s;
		s.Format(U_("%s, %u channel%s"), mpt::ToUnicode(modType).GetString(), m_sndFile.GetNumChannels(), (m_sndFile.GetNumChannels() != 1) ? U_("s") : U_(""));
		m_BtnModType->SetWindowText(s);
	}

	if (updateAll || (hint.GetCategory() == HINTCAT_SEQUENCE && hintType[HINT_MODSEQUENCE | HINT_RESTARTPOS]))
	{
		// Set max valid restart position
		m_SpinRestartPos->SetRange(0, std::max(m_sndFile.Order().GetRestartPos(), static_cast<ORDERINDEX>(m_sndFile.Order().GetLengthTailTrimmed() - 1)));
		SetDlgItemInt(IDC_EDIT_RESTARTPOS, m_sndFile.Order().GetRestartPos(), false);
	}

	if(updateAll || (hint.GetCategory() == HINTCAT_GENERAL && hintType[HINT_MODGENERAL]) || (hint.GetCategory() == HINTCAT_SEQUENCE && hint.ToType<SequenceHint>().GetSequence() == SEQUENCEINDEX_INVALID))
	{
		if(!m_editsLocked)
		{
			m_EditTempo->SetTempoValue(m_sndFile.Order().GetDefaultTempo());
			SetDlgItemInt(IDC_EDIT_SPEED, m_sndFile.Order().GetDefaultSpeed(), false);
		}
		m_SliderTempo->setValue(TempoToSlider(m_sndFile.Order().GetDefaultTempo()));
	}

	if (updateAll || (hint.GetCategory() == HINTCAT_GENERAL && hintType[HINT_MODGENERAL]))
	{
		if (!m_editsLocked)
		{
			m_EditTitle->SetWindowText(mpt::ToUnicode(m_sndFile.GetCharsetInternal(), m_sndFile.GetTitle()));
			m_EditArtist->SetWindowText(mpt::ToUnicode(m_sndFile.m_songArtist));
			SetItemInt(IDC_EDIT_GLOBALVOL, m_sndFile.m_nDefaultGlobalVolume / GetGlobalVolumeFactor());
			SetItemInt(IDC_EDIT_VSTIVOL, m_sndFile.m_nVSTiVolume);
			SetItemInt(IDC_EDIT_SAMPLEPA, m_sndFile.m_nSamplePreAmp);
		}

		m_SliderGlobalVol->setValue(MAX_GLOBAL_VOLUME - m_sndFile.m_nDefaultGlobalVolume);
		m_SliderVSTiVol->setValue(MAX_SLIDER_VSTI_VOL - m_sndFile.m_nVSTiVolume);
		m_SliderSamplePreAmp->setValue(MAX_SLIDER_SAMPLE_VOL - m_sndFile.m_nSamplePreAmp);
	}

	if(updateAll || hintType == HINT_MPTOPTIONS || (hint.GetCategory() == HINTCAT_GENERAL && hintType[HINT_MODGENERAL]))
	{
		for(int i = 0; i < m_CbnResampling->count(); ++i)
		{
			if(m_sndFile.m_nResampling == static_cast<ResamplingMode>(mfcless::GetItemData(m_CbnResampling, i)))
			{
				m_CbnResampling->setCurrentIndex(i);
				break;
			}
		}
	}

	SetChecked(IDC_CHECK_LOOPSONG, (TrackerSettings::Instance().gbLoopSong) ? true : false);
	if (hintType[HINT_MPTOPTIONS])
	{
		m_VuMeterLeft->InvalidateRect(NULL, false);
		m_VuMeterRight->InvalidateRect(NULL, false);
	}
	UnlockControls();
}


void CCtrlGeneral::OnVScroll(mfcless::ScrollCode code, int pos, QWidget *pscroll)
{
	CModControlDlg::OnVScroll(code, pos, pscroll);

	if (m_initialized)
	{
		QWidget* pSlider = static_cast<QWidget*>(pscroll);

		if (pSlider == &m_SliderTempo)
		{
			const TEMPO tempo = SliderToTempo(m_SliderTempo->value());
			if ((tempo >= m_sndFile.GetModSpecifications().GetTempoMin()) && (tempo <= m_sndFile.GetModSpecifications().GetTempoMax()) && (tempo != m_sndFile.Order().GetDefaultTempo()))
			{
				m_sndFile.Order().SetDefaultTempo(m_sndFile.m_PlayState.m_nMusicTempo = tempo);
				m_modDoc.SetModified();
				m_modDoc.UpdateAllViews(nullptr, GeneralHint().General(), this);
				m_EditTempo->SetTempoValue(tempo);
			}
		}

		else if (pSlider == &m_SliderGlobalVol)
		{
			const uint32 gv = MAX_GLOBAL_VOLUME - m_SliderGlobalVol->value();
			if ((gv >= 0) && (gv <= MAX_GLOBAL_VOLUME) && (gv != m_sndFile.m_nDefaultGlobalVolume))
			{
				m_sndFile.m_PlayState.m_nGlobalVolume = gv;
				m_sndFile.m_nDefaultGlobalVolume = gv;
				m_modDoc.SetModified();
				m_modDoc.UpdateAllViews(nullptr, GeneralHint().General(), this);
				SetItemInt(IDC_EDIT_GLOBALVOL, m_sndFile.m_nDefaultGlobalVolume / GetGlobalVolumeFactor());
			}
		}

		else if (pSlider == &m_SliderSamplePreAmp)
		{
			const uint32 spa = MAX_SLIDER_SAMPLE_VOL - m_SliderSamplePreAmp->value();
			if ((spa >= 0) && (spa <= MAX_SLIDER_SAMPLE_VOL) && (spa != m_sndFile.m_nSamplePreAmp))
			{
				m_sndFile.m_nSamplePreAmp = spa;
				if(m_sndFile.GetType() != MOD_TYPE_MOD)
					m_modDoc.SetModified();
				m_modDoc.UpdateAllViews(nullptr, GeneralHint().General(), this);
				SetItemInt(IDC_EDIT_SAMPLEPA, m_sndFile.m_nSamplePreAmp);
			}
		}

		else if (pSlider == &m_SliderVSTiVol)
		{
			const uint32 vv = MAX_SLIDER_VSTI_VOL - m_SliderVSTiVol->value();
			if ((vv >= 0) && (vv <= MAX_SLIDER_VSTI_VOL) && (vv != m_sndFile.m_nVSTiVolume))
			{
				m_sndFile.m_nVSTiVolume = vv;
				m_sndFile.RecalculateGainForAllPlugs();
				m_modDoc.SetModified();
				m_modDoc.UpdateAllViews(nullptr, GeneralHint().General(), this);
				SetItemInt(IDC_EDIT_VSTIVOL, m_sndFile.m_nVSTiVolume);
			}
		}

		else if(pSlider == (CSliderCtrl*)&m_SpinTempo)
		{
			int pos32 = m_SpinTempo->GetPos();
			if(pos32 != 0)
			{
				pos32 *= TEMPO::fractFact;
				if(m_sndFile.GetModSpecifications().hasFractionalTempo)
				{
					if(CInputHandler::CtrlPressed())
						pos32 /= 100;
					else if(CInputHandler::ShiftPressed())
						pos32 /= 10;
				}
				TEMPO newTempo;
				newTempo.SetRaw(pos32);
				newTempo += m_sndFile.Order().GetDefaultTempo();
				Limit(newTempo, m_tempoMin, m_tempoMax);
				m_sndFile.Order().SetDefaultTempo(m_sndFile.m_PlayState.m_nMusicTempo = newTempo);
				m_modDoc.SetModified();
				LockControls();
				m_modDoc.UpdateAllViews(nullptr, GeneralHint().General(), this);
				UnlockControls();
				m_SliderTempo->setValue(TempoToSlider(newTempo));
				m_EditTempo->SetTempoValue(newTempo);
			}
			m_SpinTempo->SetPos(0);
		}
	}
}


void CCtrlGeneral::OnTitleChanged()
{
	if (!m_EditTitle->m_hWnd || !m_EditTitle->GetModify()) return;

	mpt::ustring title;
	m_EditTitle->GetWindowText(title);
	if(m_sndFile.SetTitle(mpt::ToCharset(m_sndFile.GetCharsetInternal(), title)))
	{
		m_EditTitle->SetModify(false);
		m_modDoc.SetModified();
		m_modDoc.UpdateAllViews(nullptr, GeneralHint().General(), this);
	}
}


void CCtrlGeneral::OnArtistChanged()
{
	if (!m_EditArtist->m_hWnd || !m_EditArtist->GetModify()) return;

	mpt::ustring artist = GetWindowTextUnicode(m_EditArtist);
	if(artist != m_sndFile.m_songArtist)
	{
		m_EditArtist->SetModify(false);
		m_sndFile.m_songArtist = artist;
		m_modDoc.SetModified();
		m_modDoc.UpdateAllViews(NULL, GeneralHint().General(), this);
	}
}


void CCtrlGeneral::OnTempoChanged()
{
	if (m_initialized && m_EditTempo->GetWindowTextLength() > 0 && !IsLocked())
	{
		TEMPO tempo = m_EditTempo->GetTempoValue();
		Limit(tempo, m_tempoMin, m_tempoMax);
		if(!m_sndFile.GetModSpecifications().hasFractionalTempo) tempo.Set(tempo.GetInt());
		if (tempo != m_sndFile.Order().GetDefaultTempo())
		{
			m_editsLocked = true;
			m_EditTempo->SetModify(false);
			m_sndFile.Order().SetDefaultTempo(tempo);
			m_sndFile.m_PlayState.m_nMusicTempo = tempo;
			m_modDoc.SetModified();
			m_modDoc.UpdateAllViews(nullptr, GeneralHint().General());
			m_editsLocked = false;
		}
	}
}


void CCtrlGeneral::OnSpeedChanged()
{
	mpt::uchar s[16];
	if(m_initialized)
	{
		m_EditSpeed->GetWindowText(s, mpt::saturate_cast<int>(std::size(s)));
		if (s[0])
		{
			uint32 n = mpt::parse<uint32>(s);
			n = Clamp(n, m_sndFile.GetModSpecifications().speedMin, m_sndFile.GetModSpecifications().speedMax);
			if (n != m_sndFile.Order().GetDefaultSpeed())
			{
				m_editsLocked = true;
				m_EditSpeed->SetModify(false);
				m_sndFile.Order().SetDefaultSpeed(n);
				m_sndFile.m_PlayState.m_nMusicSpeed = n;
				m_modDoc.SetModified();
				m_modDoc.UpdateAllViews(nullptr, GeneralHint().General(), this);
				// Update envelope grid view
				m_modDoc.UpdateAllViews(nullptr, InstrumentHint().Envelope(), this);
				m_editsLocked = false;
			}
		}
	}
}


void CCtrlGeneral::OnVSTiVolChanged()
{
	mpt::uchar s[16];
	if (m_initialized)
	{
		m_EditVSTiVol->GetWindowText(s, mpt::saturate_cast<int>(std::size(s)));
		if (s[0])
		{
			uint32 n = mpt::parse<uint32>(s);
			Limit(n, uint32(0), MAX_PREAMP);
			if (n != m_sndFile.m_nVSTiVolume)
			{
				m_editsLocked = true;
				m_sndFile.m_nVSTiVolume = n;
				m_sndFile.RecalculateGainForAllPlugs();
				m_modDoc.SetModified();
				m_modDoc.UpdateAllViews(nullptr, GeneralHint().General(), this);
				UpdateView(GeneralHint().General());
				m_editsLocked = false;
			}
		}
	}
}

void CCtrlGeneral::OnSamplePAChanged()
{
	mpt::uchar s[16];
	if(m_initialized)
	{
		m_EditSamplePA->GetWindowText(s, mpt::saturate_cast<int>(std::size(s)));
		if (s[0])
		{
			uint32 n = mpt::parse<uint32>(s);
			Limit(n, uint32(0), MAX_PREAMP);
			if (n != m_sndFile.m_nSamplePreAmp)
			{
				m_editsLocked = true;
				m_sndFile.m_nSamplePreAmp = n;
				m_modDoc.SetModified();
				m_modDoc.UpdateAllViews(nullptr, GeneralHint().General(), this);
				UpdateView(GeneralHint().General());
				m_editsLocked = false;
			}
		}
	}
}

void CCtrlGeneral::OnGlobalVolChanged()
{
	mpt::uchar s[16];
	if(m_initialized)
	{
		m_EditGlobalVol->GetWindowText(s, mpt::saturate_cast<int>(std::size(s)));
		if (s[0])
		{
			uint32 n = mpt::parse<ORDERINDEX>(s) * GetGlobalVolumeFactor();
			Limit(n, 0u, 256u);
			if (n != m_sndFile.m_nDefaultGlobalVolume)
			{ 
				m_editsLocked = true;
				m_EditGlobalVol->SetModify(false);
				m_sndFile.m_nDefaultGlobalVolume = n;
				m_sndFile.m_PlayState.m_nGlobalVolume = n;
				m_modDoc.SetModified();
				m_modDoc.UpdateAllViews(nullptr, GeneralHint().General(), this);
				UpdateView(GeneralHint().General());
				m_editsLocked = false;
			}
		}
	}
}


void CCtrlGeneral::OnRestartPosChanged()
{
	if(!m_initialized)
		return;
	mpt::uchar s[32];
	m_EditRestartPos->GetWindowText(s, mpt::saturate_cast<int>(std::size(s)));
	if(!s[0])
		return;

	ORDERINDEX n = mpt::parse<ORDERINDEX>(s);
	LimitMax(n, m_sndFile.Order().GetLastIndex());
	while(n > 0 && n < m_sndFile.Order().GetLastIndex() && !m_sndFile.Order().IsValidPat(n))
		n++;

	if(n == m_sndFile.Order().GetRestartPos())
		return;
	
	m_EditRestartPos->SetModify(false);
	m_sndFile.Order().SetRestartPos(n);
	m_modDoc.SetModified();
	m_modDoc.UpdateAllViews(nullptr, SequenceHint(m_sndFile.Order.GetCurrentSequenceIndex()).RestartPos(), this);
}


void CCtrlGeneral::OnRestartPosDone()
{
	if(m_initialized)
		SetDlgItemInt(IDC_EDIT_RESTARTPOS, m_sndFile.Order().GetRestartPos());
}


void CCtrlGeneral::OnSongProperties()
{
	m_modDoc.OnSongProperties();
}


void CCtrlGeneral::OnLoopSongChanged()
{
	m_modDoc.SetLoopSong(IsChecked(IDC_CHECK_LOOPSONG) != 0);
}


intptr_t CCtrlGeneral::OnUpdatePosition(uintptr_t, intptr_t lParam)
{
	Notification *pnotify = (Notification *)lParam;
	if (pnotify)
	{
		m_VuMeterLeft->SetVuMeter(pnotify->masterVUout[0] & (~Notification::ClipVU), pnotify->type[Notification::Stop]);
		m_VuMeterRight->SetVuMeter(pnotify->masterVUout[1] & (~Notification::ClipVU), pnotify->type[Notification::Stop]);
	}
	return 0;
}


mpt::ustring CCtrlGeneral::GetToolTipText(uint32 uId, QWidget *) const
{
	mpt::ustring s;
	if(uId)
	{
		const mpt::uchar moreRecentMixModeNote[] = U_("Use a more recent mixmode to see dB offsets.");
		const bool displayDBValues = m_sndFile.GetPlayConfig().getDisplayDBValues();
		const QWidget *wnd = GetDlgItem(uId);
		const bool isEnabled = wnd ? (wnd->IsWindowEnabled() != false) : true;  // nullptr check is for a Wine bug workaround (https://bugs.openmpt.org/view.php?id=1553)
		mpt::ustring notAvailable;
		if(!isEnabled)
			notAvailable = MPT_UFORMAT("Feature is not available in the {} format.")(mpt::ToUnicode(m_sndFile.GetModSpecifications().GetFileExtensionUpper()));

		switch(uId)
		{
		case IDC_BUTTON_MODTYPE:
			s = U_("Song Properties");
			{
				const auto keyText = CMainFrame::GetInputHandler()->m_activeCommandSet->GetKeyTextFromCommand(kcViewSongProperties, 0);
				if (!keyText.IsEmpty())
					s +=  MPT_UFORMAT(" ({})")(keyText);
			}
			break;
		case IDC_BUTTON1:
			if(isEnabled)
				s = U_("Click button multiple times to tap in the desired tempo.");
			else
				s = notAvailable;
			break;
		case IDC_SLIDER_SAMPLEPREAMP:
			s = displayDBValues ? CModDoc::LinearToDecibelsString(m_sndFile.m_nSamplePreAmp, m_sndFile.GetPlayConfig().getNormalSamplePreAmp()).GetString() : moreRecentMixModeNote;
			break;
		case IDC_SLIDER_VSTIVOL:
			if(isEnabled)
				s = displayDBValues ? CModDoc::LinearToDecibelsString(m_sndFile.m_nVSTiVolume, m_sndFile.GetPlayConfig().getNormalVSTiVol()).GetString() : moreRecentMixModeNote;
			else
				s = notAvailable;
			break;
		case IDC_SLIDER_GLOBALVOL:
			if(isEnabled)
				s = displayDBValues ? CModDoc::LinearToDecibelsString(m_sndFile.m_PlayState.m_nGlobalVolume, m_sndFile.GetPlayConfig().getNormalGlobalVol()).GetString() : moreRecentMixModeNote;
			else
				s = notAvailable;
			break;
		case IDC_SLIDER_SONGTEMPO:
		case IDC_EDIT_ARTIST:
		case IDC_EDIT_TEMPO:
		case IDC_EDIT_SPEED:
		case IDC_EDIT_RESTARTPOS:
		case IDC_EDIT_GLOBALVOL:
		case IDC_EDIT_VSTIVOL:
			if(isEnabled)
				break;
			s = notAvailable;
			break;
		}
	}
	return s;
	
}


void CCtrlGeneral::OnEnSetfocusEditSongtitle()
{
	m_EditTitle->setMaxLength(m_sndFile.GetModSpecifications().modNameLengthMax);
}


void CCtrlGeneral::OnResamplingChanged()
{
	int sel = m_CbnResampling->currentIndex();
	if(sel >= 0)
	{
		m_sndFile.m_nResampling = static_cast<ResamplingMode>(mfcless::GetItemData(m_CbnResampling, sel));
		if(m_sndFile.GetModSpecifications().hasDefaultResampling)
		{
			m_modDoc.SetModified();
			m_modDoc.UpdateAllViews(nullptr, GeneralHint().General(), this);
		}
	}
}


////////////////////////////////////////////////////////////////////////////////
//
// CVuMeter
//

void CVuMeter::RegisterMessageMap()
{
	mfcless::Window<QWidget>::RegisterMessageMap();
	// ON_WM_PAINT() (mfcless::Window)
}


void CVuMeter::OnPaint()
{
	mfcless::Rect rect;
	mfcless::PaintDC dc(this);
	GetClientRect(&rect);
	dc.FillSolidRect(rect.left, rect.top, rect.Width(), rect.Height(), mfcless::Rgb(0,0,0));
	m_lastDisplayedLevel = -1;
	DrawVuMeter(dc, true);
}


void CVuMeter::SetVuMeter(int level, bool force)
{
	level >>= 8;
	if (level != m_lastLevel)
	{
		uint32 curTime = mfcless::TickCount();
		if(curTime - m_lastVuUpdateTime >= TrackerSettings::Instance().VuMeterUpdateInterval || force)
		{
			m_lastLevel = level;
			mfcless::PaintDC dc(this);
			DrawVuMeter(dc);
			m_lastVuUpdateTime = curTime;
		}
	}
}


void CVuMeter::DrawVuMeter(mfcless::DC &dc, bool /*redraw*/)
{
	mfcless::Rect rect;
	GetClientRect(&rect);
	int vu = (m_lastLevel * (rect.bottom-rect.top)) >> 8;
	int cy = rect.bottom - rect.top;
	if (cy < 1) cy = 1;
	for (int ry=rect.bottom-1; ry>rect.top; ry-=2)
	{
		int y0 = rect.bottom - ry;
		int n = Clamp((y0 * NUM_VUMETER_PENS) / cy, 0, NUM_VUMETER_PENS - 1);
		if (vu < y0)
			n += NUM_VUMETER_PENS;
		dc.FillSolidRect(rect.left, ry, rect.Width(), 1, CMainFrame::gcolrefVuMeter[n]);
	}
	m_lastDisplayedLevel = m_lastLevel;
}


OPENMPT_NAMESPACE_END
