/*
 * Ctrl_smp.cpp
 * ------------
 * Purpose: Sample tab, upper panel.
 * Notes  : mfcless twin of upstream's mptrack/Ctrl_smp.cpp.
 *          Draft from qt/tools/twin.py, not yet edited by hand.
 * Authors: Olivier Lapicque
 *          OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "Ctrl_smp.h"
#include "mfcless/Widgets.h"
#include "Autotune.h"
#include "Childfrm.h"
#include "dlg_misc.h"
#include "FileDialog.h"
#include "Globals.h"
#include "HighDPISupport.h"
#include "ImageLists.h"
#include "InputHandler.h"
#include "Mainfrm.h"
#include "Moddoc.h"
#include "Mptrack.h"
#include "ProgressDialog.h"
#include "PSRatioCalc.h"
#include "Reporting.h"
#include "resource.h"
#include "SampleEditorDialogs.h"
#include "TrackerSettings.h"
#include "View_smp.h"
#include "WindowMessages.h"

#include "../common/mptStringBuffer.h"
#include "../common/mptFileIO.h"
#include "../common/FileReader.h"
#include "../soundlib/mod_specifications.h"
#include "../soundlib/SampleCopy.h"
#include "../tracklib/SampleEdit.h"
#include "../tracklib/TimeStretchPitchShift.h"
#include "mpt/io_file/inputfile.hpp"
#include "mpt/io_file_read/inputfile_filecursor.hpp"
#include "mpt/io_file/outputfile.hpp"
#include "mpt/parse/parse.hpp"
#include "mpt/string/utility.hpp"
#include "openmpt/soundbase/Copy.hpp"


OPENMPT_NAMESPACE_BEGIN

inline constexpr int32 BASENOTE_MIN = (1 * 12);        // C-1
inline constexpr int32 BASENOTE_MAX = (10 * 12 + 11);  // B-10

static int32 GetSampleTransponse(const ModSample &sample)
{
	return mpt::saturate_round<int32>(std::log(sample.nC5Speed * (1.0 / 8363.0)) * (12.0 / mpt::numbers::ln2));
}


void CCtrlSamples::RegisterMessageMap()
{
	CModControlDlg::RegisterMessageMap();
	AddScroll(Qt::Vertical, &CCtrlSamples::OnVScroll);
	// ON_WM_XBUTTONUP() (mfcless::Window)
	AddNotify(IDC_TOOLBAR1, mfcless::Notify::DropDown, &CCtrlSamples::OnTbnDropDownToolBar);
	AddCommand(IDC_SAMPLE_NEW, &CCtrlSamples::OnSampleNew);
	AddCommand(IDC_SAMPLE_DUPLICATE, &CCtrlSamples::OnSampleDuplicate);
	AddCommand(IDC_SAMPLE_OPEN, &CCtrlSamples::OnSampleOpen);
	AddCommand(IDC_SAMPLE_OPENKNOWN, &CCtrlSamples::OnSampleOpenKnown);
	AddCommand(IDC_SAMPLE_OPENRAW, &CCtrlSamples::OnSampleOpenRaw);
	AddCommand(IDC_SAMPLE_SAVEAS, &CCtrlSamples::OnSampleSave);
	AddCommand(IDC_SAVE_ONE, &CCtrlSamples::OnSampleSaveOne);
	AddCommand(IDC_SAVE_ALL, &CCtrlSamples::OnSampleSaveAll);
	AddCommand(IDC_SAMPLE_PLAY, &CCtrlSamples::OnSamplePlay);
	AddCommand(IDC_SAMPLE_NORMALIZE, &CCtrlSamples::OnNormalize);
	AddCommand(IDC_SAMPLE_AMPLIFY, &CCtrlSamples::OnAmplify);
	AddCommand(IDC_SAMPLE_RESAMPLE, &CCtrlSamples::OnResample);
	AddCommand(IDC_SAMPLE_REVERSE, &CCtrlSamples::OnReverse);
	AddCommand(IDC_SAMPLE_SILENCE, &CCtrlSamples::OnSilence);
	AddCommand(IDC_SAMPLE_INVERT, &CCtrlSamples::OnInvert);
	AddCommand(IDC_SAMPLE_SIGN_UNSIGN, &CCtrlSamples::OnSignUnSign);
	AddCommand(IDC_SAMPLE_DCOFFSET, &CCtrlSamples::OnRemoveDCOffset);
	AddCommand(IDC_SAMPLE_XFADE, &CCtrlSamples::OnXFade);
	AddCommand(IDC_SAMPLE_STEREOSEPARATION, &CCtrlSamples::OnStereoSeparation);
	AddCommand(IDC_SAMPLE_AUTOTUNE, &CCtrlSamples::OnAutotune);
	AddCommand(IDC_CHECK1, &CCtrlSamples::OnSetPanningChanged);
	AddCommand(IDC_CHECK2, &CCtrlSamples::OnKeepSampleOnDisk);
	AddCommand(ID_PREVINSTRUMENT, &CCtrlSamples::OnPrevInstrument);
	AddCommand(ID_NEXTINSTRUMENT, &CCtrlSamples::OnNextInstrument);
	AddCommand(IDC_BUTTON1, &CCtrlSamples::OnPitchShiftTimeStretch);
	AddCommand(IDC_BUTTON2, &CCtrlSamples::OnEstimateSampleSize);
	AddCommand(IDC_CHECK3, &CCtrlSamples::OnToggleTimestretchQuality);
	AddCommand(IDC_SAMPLE_INITOPL, &CCtrlSamples::OnInitOPLInstrument);
	AddNotify(IDC_SAMPLE_NAME, mfcless::Notify::EditChange, &CCtrlSamples::OnNameChanged);
	AddNotify(IDC_SAMPLE_FILENAME, mfcless::Notify::EditChange, &CCtrlSamples::OnFileNameChanged);
	AddNotify(IDC_EDIT_SAMPLE, mfcless::Notify::EditChange, &CCtrlSamples::OnSampleChanged);
	AddNotify(IDC_EDIT1, mfcless::Notify::EditChange, &CCtrlSamples::OnLoopPointsChanged);
	AddNotify(IDC_EDIT2, mfcless::Notify::EditChange, &CCtrlSamples::OnLoopPointsChanged);
	AddNotify(IDC_EDIT3, mfcless::Notify::EditChange, &CCtrlSamples::OnSustainPointsChanged);
	AddNotify(IDC_EDIT4, mfcless::Notify::EditChange, &CCtrlSamples::OnSustainPointsChanged);
	AddNotify(IDC_EDIT5, mfcless::Notify::EditChange, &CCtrlSamples::OnFineTuneChanged);
	AddNotify(IDC_EDIT7, mfcless::Notify::EditChange, &CCtrlSamples::OnVolumeChanged);
	AddNotify(IDC_EDIT8, mfcless::Notify::EditChange, &CCtrlSamples::OnGlobalVolChanged);
	AddNotify(IDC_EDIT9, mfcless::Notify::EditChange, &CCtrlSamples::OnPanningChanged);
	AddNotify(IDC_EDIT14, mfcless::Notify::EditChange, &CCtrlSamples::OnVibSweepChanged);
	AddNotify(IDC_EDIT15, mfcless::Notify::EditChange, &CCtrlSamples::OnVibDepthChanged);
	AddNotify(IDC_EDIT16, mfcless::Notify::EditChange, &CCtrlSamples::OnVibRateChanged);
	AddNotify(IDC_SAMPLE_NAME, mfcless::Notify::SetFocus, &CCtrlSamples::OnEditFocus);
	AddNotify(IDC_SAMPLE_FILENAME, mfcless::Notify::SetFocus, &CCtrlSamples::OnEditFocus);
	AddNotify(IDC_EDIT1, mfcless::Notify::SetFocus, &CCtrlSamples::OnEditFocus);
	AddNotify(IDC_EDIT2, mfcless::Notify::SetFocus, &CCtrlSamples::OnEditFocus);
	AddNotify(IDC_EDIT3, mfcless::Notify::SetFocus, &CCtrlSamples::OnEditFocus);
	AddNotify(IDC_EDIT4, mfcless::Notify::SetFocus, &CCtrlSamples::OnEditFocus);
	AddNotify(IDC_EDIT5, mfcless::Notify::SetFocus, &CCtrlSamples::OnEditFocus);
	AddNotify(IDC_EDIT7, mfcless::Notify::SetFocus, &CCtrlSamples::OnEditFocus);
	AddNotify(IDC_EDIT8, mfcless::Notify::SetFocus, &CCtrlSamples::OnEditFocus);
	AddNotify(IDC_EDIT9, mfcless::Notify::SetFocus, &CCtrlSamples::OnEditFocus);
	AddNotify(IDC_EDIT14, mfcless::Notify::SetFocus, &CCtrlSamples::OnEditFocus);
	AddNotify(IDC_EDIT15, mfcless::Notify::SetFocus, &CCtrlSamples::OnEditFocus);
	AddNotify(IDC_EDIT16, mfcless::Notify::SetFocus, &CCtrlSamples::OnEditFocus);
	AddNotify(IDC_EDIT5, mfcless::Notify::KillFocus, &CCtrlSamples::OnFineTuneChangedDone);
	AddNotify(IDC_COMBO_BASENOTE, mfcless::Notify::SelChange, &CCtrlSamples::OnBaseNoteChanged);
	AddNotify(IDC_COMBO_ZOOM, mfcless::Notify::SelChange, &CCtrlSamples::OnZoomChanged);
	AddNotify(IDC_COMBO1, mfcless::Notify::SelChange, &CCtrlSamples::OnLoopTypeChanged);
	AddNotify(IDC_COMBO2, mfcless::Notify::SelChange, &CCtrlSamples::OnSustainTypeChanged);
	AddNotify(IDC_COMBO3, mfcless::Notify::SelChange, &CCtrlSamples::OnVibTypeChanged);
	AddMessage(WM_MOD_KEYCOMMAND, &CCtrlSamples::OnCustomKeyMsg);
}


void CCtrlSamples::DoDataExchange(DataExchange &dx)
{
	CModControlDlg::DoDataExchange(dx);
	//{{AFX_DATA_MAP(CCtrlSamples)
	ExchangeControl(dx, IDC_TOOLBAR1, m_ToolBar1);
	ExchangeControl(dx, IDC_TOOLBAR2, m_ToolBar2);
	ExchangeControl(dx, IDC_SAMPLE_NAME, m_EditName);
	ExchangeControl(dx, IDC_SAMPLE_FILENAME, m_EditFileName);
	ExchangeControl(dx, IDC_SAMPLE_NAME, m_EditName);
	ExchangeControl(dx, IDC_SAMPLE_FILENAME, m_EditFileName);
	ExchangeControl(dx, IDC_COMBO_ZOOM, m_ComboZoom);
	ExchangeControl(dx, IDC_COMBO_BASENOTE, m_CbnBaseNote);
	ExchangeControl(dx, IDC_SPIN_SAMPLE, m_SpinSample);
	ExchangeControl(dx, IDC_EDIT_SAMPLE, m_EditSample);
	ExchangeControl(dx, IDC_CHECK1, m_CheckPanning);
	ExchangeControl(dx, IDC_SPIN1, m_SpinLoopStart);
	ExchangeControl(dx, IDC_SPIN2, m_SpinLoopEnd);
	ExchangeControl(dx, IDC_SPIN3, m_SpinSustainStart);
	ExchangeControl(dx, IDC_SPIN4, m_SpinSustainEnd);
	ExchangeControl(dx, IDC_SPIN5, m_SpinFineTune);
	ExchangeControl(dx, IDC_SPIN7, m_SpinVolume);
	ExchangeControl(dx, IDC_SPIN8, m_SpinGlobalVol);
	ExchangeControl(dx, IDC_SPIN9, m_SpinPanning);
	ExchangeControl(dx, IDC_SPIN11, m_SpinVibSweep);
	ExchangeControl(dx, IDC_SPIN12, m_SpinVibDepth);
	ExchangeControl(dx, IDC_SPIN13, m_SpinVibRate);
	ExchangeControl(dx, IDC_SPIN16, m_SpinTimeStretchRatio);
	ExchangeControl(dx, IDC_COMBO1, m_ComboLoopType);
	ExchangeControl(dx, IDC_COMBO2, m_ComboSustainType);
	ExchangeControl(dx, IDC_COMBO3, m_ComboAutoVib);
	ExchangeControl(dx, IDC_COMBO5, m_ComboGrainSize);
	ExchangeControl(dx, IDC_EDIT1, m_EditLoopStart);
	ExchangeControl(dx, IDC_EDIT2, m_EditLoopEnd);
	ExchangeControl(dx, IDC_EDIT3, m_EditSustainStart);
	ExchangeControl(dx, IDC_EDIT4, m_EditSustainEnd);
	ExchangeControl(dx, IDC_EDIT5, m_EditFineTune);
	ExchangeControl(dx, IDC_EDIT7, m_EditVolume);
	ExchangeControl(dx, IDC_EDIT8, m_EditGlobalVol);
	ExchangeControl(dx, IDC_EDIT9, m_EditPanning);
	ExchangeControl(dx, IDC_EDIT14, m_EditVibSweep);
	ExchangeControl(dx, IDC_EDIT15, m_EditVibDepth);
	ExchangeControl(dx, IDC_EDIT16, m_EditVibRate);
	ExchangeControl(dx, IDC_COMBO4, m_ComboPitch);
	//}}AFX_DATA_MAP
}


CCtrlSamples::CCtrlSamples(CModControlView &parent, CModDoc &document)
	: CModControlDlg(parent, document)
{
	m_nLockCount = 1;
	m_EditTimeStretchRatio->SetAccessibleSuffix(UL_("%"));
}



CCtrlSamples::~CCtrlSamples()
{
}



const mfcless::ViewClass *CCtrlSamples::GetAssociatedViewClass()
{
	return (&CViewSample::classCViewSample);
}


void CCtrlSamples::OnEditFocus()
{
	m_startedEdit = false;
}


bool CCtrlSamples::OnInitDialog()
{
	CModControlDlg::OnInitDialog();
	m_initialized = false;
	SetRedraw(false);

	// Zoom Selection
	static constexpr std::pair<const mpt::uchar *, int> ZoomLevels[] =
	{
		{UL_("Auto"),  0},
		{UL_("1:1"),   1},
		{UL_("2:1"),  -2},
		{UL_("4:1"),  -3},
		{UL_("8:1"),  -4},
		{UL_("16:1"), -5},
		{UL_("32:1"), -6},
		{UL_("1:2"),   2},
		{UL_("1:4"),   3},
		{UL_("1:8"),   4},
		{UL_("1:16"),  5},
		{UL_("1:32"),  6},
		{UL_("1:64"),  7},
		{UL_("1:128"), 8},
		{UL_("1:256"), 9},
		{UL_("1:512"), 10},
	};
	m_ComboZoom->setUpdatesEnabled(false);
	for(const auto &[str, data] : ZoomLevels)
	{
		mfcless::SetItemData(m_ComboZoom, mfcless::AddItem(m_ComboZoom, str), static_cast<uintptr_t>(data));
	}
	m_ComboZoom->setUpdatesEnabled(true);
	m_ComboZoom->setCurrentIndex(0);

	// File ToolBar
	m_ToolBar1->Init(CMainFrame::GetMainFrame()->m_PatternIcons,CMainFrame::GetMainFrame()->m_PatternIconsDisabled);
	m_ToolBar1->AddButton(IDC_SAMPLE_NEW, TIMAGE_SAMPLE_NEW, CModControlBar::Button | CModControlBar::DropDown);
	m_ToolBar1->AddButton(IDC_SAMPLE_OPEN, TIMAGE_OPEN, CModControlBar::Button | CModControlBar::DropDown);
	m_ToolBar1->AddButton(IDC_SAMPLE_SAVEAS, TIMAGE_SAVE, CModControlBar::Button | CModControlBar::DropDown);
	// Edit ToolBar
	m_ToolBar2->Init(CMainFrame::GetMainFrame()->m_PatternIcons,CMainFrame::GetMainFrame()->m_PatternIconsDisabled);
	m_ToolBar2->AddButton(IDC_SAMPLE_PLAY, TIMAGE_PREVIEW);
	m_ToolBar2->AddButton(IDC_SAMPLE_NORMALIZE, TIMAGE_SAMPLE_NORMALIZE);
	m_ToolBar2->AddButton(IDC_SAMPLE_AMPLIFY, TIMAGE_SAMPLE_AMPLIFY);
	m_ToolBar2->AddButton(IDC_SAMPLE_DCOFFSET, TIMAGE_SAMPLE_DCOFFSET);
	m_ToolBar2->AddButton(IDC_SAMPLE_STEREOSEPARATION, TIMAGE_SAMPLE_STEREOSEP);
	m_ToolBar2->AddButton(IDC_SAMPLE_RESAMPLE, TIMAGE_SAMPLE_RESAMPLE);
	m_ToolBar2->AddButton(IDC_SAMPLE_REVERSE, TIMAGE_SAMPLE_REVERSE);
	m_ToolBar2->AddButton(IDC_SAMPLE_SILENCE, TIMAGE_SAMPLE_SILENCE);
	m_ToolBar2->AddButton(IDC_SAMPLE_INVERT, TIMAGE_SAMPLE_INVERT);
	m_ToolBar2->AddButton(IDC_SAMPLE_SIGN_UNSIGN, TIMAGE_SAMPLE_UNSIGN);
	m_ToolBar2->AddButton(IDC_SAMPLE_XFADE, TIMAGE_SAMPLE_FIXLOOP);
	m_ToolBar2->AddButton(IDC_SAMPLE_AUTOTUNE, TIMAGE_SAMPLE_AUTOTUNE);
	// Setup Controls
	m_SpinVolume->SetRange(0, 64);
	m_SpinGlobalVol->SetRange(0, 64);

	m_CbnBaseNote->setUpdatesEnabled(false);
	for(ModCommand::NOTE i = BASENOTE_MIN; i <= BASENOTE_MAX; i++)
	{
		mpt::ustring noteName = mpt::ToUnicode(CSoundFile::GetDefaultNoteName(i % 12)) + mpt::ufmt::val(i / 12);
		mfcless::SetItemData(m_CbnBaseNote, mfcless::AddItem(m_CbnBaseNote, noteName), i - (NOTE_MIDDLEC - NOTE_MIN));
	}
	m_CbnBaseNote->setUpdatesEnabled(true);

	// Pitch selection
	// Allow pitch from -12 (1 octave down) to +12 (1 octave up)
	m_ComboPitch->setUpdatesEnabled(false);
	for(int i = -12 ; i <= 12 ; i++)
	{
		mpt::ustring str;
		if(i == 0)
			str = UL_("unchanged");
		else if(i < 0)
			str = mpt::ufmt::dec(i);
		else
			str = UL_("+") + mpt::ufmt::dec(i);
		mfcless::SetItemData(m_ComboPitch, mfcless::AddItem(m_ComboPitch, str.c_str()), i + 12);
	}

	COMBOBOXINFO cbi{};
	cbi.cbSize = sizeof(cbi);
	GetComboBoxInfo(m_ComboPitch, &cbi);
	(m_EditPitch->SubclassWindow)(cbi.hwndItem);
	m_EditPitch->ModifyStyle(0, ES_NUMBER);
	m_EditPitch->AllowNegative(true);
	m_EditPitch->AllowFractions(true);

	m_ComboPitch->setUpdatesEnabled(true);
	// Set "unchanged" as default pitch
	m_ComboPitch->setCurrentIndex(12);
	m_ComboGrainSize->setCurrentIndex(3);
	OnToggleTimestretchQuality();
	SubclassDlgItem(IDC_EDIT6, m_EditTimeStretchRatio);
	m_EditTimeStretchRatio->AllowNegative(false);
	m_EditTimeStretchRatio->SetDecimalValue(100.0);
	m_SpinTimeStretchRatio->SetRange(1, 100000);

	SetRedraw(true);
	return true;
}


void CCtrlSamples::OnDPIChanged()
{
	m_ToolBar1->OnDPIChanged();
	m_ToolBar2->OnDPIChanged();
	CModControlDlg::OnDPIChanged();
}


Setting<int32> &CCtrlSamples::GetSplitPosRef() { return TrackerSettings::Instance().glSampleWindowHeight; }


void CCtrlSamples::RecalcLayout()
{
}


bool CCtrlSamples::SetCurrentSample(SAMPLEINDEX nSmp, int32 lZoom, bool bUpdNum)
{
	if(m_sndFile.GetNumSamples() < 1)
		m_sndFile.m_nSamples = 1;
	if((nSmp < 1) || (nSmp > m_sndFile.GetNumSamples()))
		return false;

	LockControls();
	if(m_nSample != nSmp)
	{
		m_nSample = nSmp;
		UpdateView(SampleHint(m_nSample).Info());
		m_parent.SampleChanged(m_nSample);
	}
	if(bUpdNum)
	{
		SetItemInt(IDC_EDIT_SAMPLE, m_nSample);
		m_SpinSample->SetRange(1, m_sndFile.GetNumSamples());
	}
	if(lZoom == -1)
	{
		lZoom = static_cast<int>(mfcless::GetItemData(m_ComboZoom, m_ComboZoom->currentIndex()));
	} else
	{
		for(int i = 0; i< m_ComboZoom->count(); i++)
		{
			if(static_cast<int>(mfcless::GetItemData(m_ComboZoom, i)) == lZoom)
			{
				m_ComboZoom->setCurrentIndex(i);
				break;
			}
		}
	}
	static_assert(MAX_SAMPLES < uint16_max);
	SendViewMessage(VIEWMSG_SETCURRENTSAMPLE, (lZoom << 16) | m_nSample);
	UnlockControls();
	return true;
}


void CCtrlSamples::OnActivatePage(intptr_t lParam)
{
	if (lParam < 0)
	{
		int nIns = m_parent.GetInstrumentChange();
		if (m_sndFile.GetNumInstruments())
		{
			if ((nIns > 0) && (!m_modDoc.IsChildSample((INSTRUMENTINDEX)nIns, m_nSample)))
			{
				SAMPLEINDEX k = m_modDoc.FindInstrumentChild((INSTRUMENTINDEX)nIns);
				if (k > 0) lParam = k;
			}
		} else
		{
			if (nIns > 0) lParam = nIns;
		}
	} else if (lParam > 0)
	{
		if (m_sndFile.GetNumInstruments())
		{
			INSTRUMENTINDEX k = (INSTRUMENTINDEX)m_parent.GetInstrumentChange();
			if (!m_modDoc.IsChildSample(k, (SAMPLEINDEX)lParam))
			{
				INSTRUMENTINDEX nins = m_modDoc.FindSampleParent((SAMPLEINDEX)lParam);
				if(nins != INSTRUMENTINDEX_INVALID)
				{
					m_parent.InstrumentChanged(nins);
				}
			}
		} else
		{
			m_parent.InstrumentChanged(static_cast<int>(lParam));
		}
	}

	CChildFrame *pFrame = (CChildFrame *)GetParentFrame();
	SampleViewState &sampleState = pFrame->GetSampleViewState();
	if(sampleState.initialSample != 0)
	{
		m_nSample = sampleState.initialSample;
		sampleState.initialSample = 0;
	}

	SetCurrentSample((lParam > 0) ? ((SAMPLEINDEX)lParam) : m_nSample);

	// Initial Update
	if(!m_initialized)
		UpdateView(SampleHint(m_nSample).Info().ModType(), nullptr);
	if(m_hWndView)
		PostViewMessage(VIEWMSG_LOADSTATE, (intptr_t)&sampleState);
	SwitchToView();

	// Combo boxes randomly disappear without this... why?
	Invalidate();
}


void CCtrlSamples::OnDeactivatePage()
{
	CChildFrame *pFrame = (CChildFrame *)GetParentFrame();
	if ((pFrame) && (m_hWndView)) SendViewMessage(VIEWMSG_SAVESTATE, (intptr_t)&pFrame->GetSampleViewState());
	m_modDoc.NoteOff(0, true);
}


intptr_t CCtrlSamples::OnModCtrlMsg(uintptr_t wParam, intptr_t lParam)
{
	switch(wParam)
	{
	case CTRLMSG_GETCURRENTINSTRUMENT:
		return m_nSample;
		break;

	case CTRLMSG_SMP_PREVINSTRUMENT:
		OnPrevInstrument();
		break;

	case CTRLMSG_SMP_NEXTINSTRUMENT:
		OnNextInstrument();
		break;

	case CTRLMSG_SMP_OPENFILE:
		if(lParam)
			return OpenSample(*reinterpret_cast<const mpt::PathString *>(lParam));
		break;

	case CTRLMSG_SMP_SONGDROP:
		if(lParam)
		{
			const auto &dropInfo = *reinterpret_cast<const DRAGONDROP *>(lParam);
			if(dropInfo.sndFile)
				return OpenSample(*dropInfo.sndFile, static_cast<SAMPLEINDEX>(dropInfo.dropItem)) ? true : false;
		}
		break;

	case CTRLMSG_SMP_SETZOOM:
		SetCurrentSample(m_nSample, static_cast<int>(lParam), false);
		break;

	case CTRLMSG_SETCURRENTINSTRUMENT:
		SetCurrentSample((SAMPLEINDEX)lParam, -1, true);
		break;

	case CTRLMSG_SMP_INITOPL:
		OnInitOPLInstrument();
		break;

	case CTRLMSG_SMP_NEWSAMPLE:
		return InsertSample(false) ? 1 : 0;

	case IDC_SAMPLE_REVERSE:
		OnReverse();
		break;

	case IDC_SAMPLE_SILENCE:
		OnSilence();
		break;

	case IDC_SAMPLE_INVERT:
		OnInvert();
		break;

	case IDC_SAMPLE_XFADE:
		OnXFade();
		break;

	case IDC_SAMPLE_STEREOSEPARATION:
		OnStereoSeparation();
		break;

	case IDC_SAMPLE_AUTOTUNE:
		OnAutotune();
		break;

	case IDC_SAMPLE_SIGN_UNSIGN:
		OnSignUnSign();
		break;

	case IDC_SAMPLE_DCOFFSET:
		RemoveDCOffset(false);
		break;

	case IDC_SAMPLE_NORMALIZE:
		Normalize(false);
		break;

	case IDC_SAMPLE_AMPLIFY:
		OnAmplify();
		break;

	case IDC_SAMPLE_QUICKFADE:
		OnQuickFade();
		break;

	case IDC_SAMPLE_OPEN:
		OnSampleOpen();
		break;

	case IDC_SAMPLE_SAVEAS:
		OnSampleSave();
		break;

	case IDC_SAMPLE_NEW:
		InsertSample(false);
		break;

	default:
		return CModControlDlg::OnModCtrlMsg(wParam, lParam);
	}
	return 0;
}


static mpt::ustring EffectiveSampleVolume(double value, double valueAtZeroDB, double effectiveFactor, const CSoundFile &sndFile)
{
	mpt::ustring s = CModDoc::LinearToDecibelsString(value, valueAtZeroDB);
	if(value == 0)
		return s;

	const auto &playConfig = sndFile.GetPlayConfig();
	if(!playConfig.getDisplayDBValues())
		return s;

	s += UL_(" (") + CModDoc::LinearToDecibelsString(value * effectiveFactor * sndFile.m_nSamplePreAmp, valueAtZeroDB * playConfig.getNormalSamplePreAmp()) + UL_(" effectively)");
	return s;
}


static mpt::ustring EffectiveOPLVolume(double value, double effectiveFactor, const CSoundFile &sndFile)
{
	const double dB = (value - 64.0) * 0.75;
	const double effectiveDB = (effectiveFactor - 64.0) * 0.75;
	mpt::ustring s = CModDoc::DecibelsToStrings(dB);

	const auto &playConfig = sndFile.GetPlayConfig();
	if(!playConfig.getDisplayDBValues())
		return s;

	s += UL_(" (") + CModDoc::DecibelsToStrings(dB + effectiveDB + CModDoc::LinearToDecibels(sndFile.m_nVSTiVolume, playConfig.getNormalVSTiVol())) + UL_(" effectively)");
	return s;
}


mpt::ustring CCtrlSamples::GetToolTipText(uint32 uId, QWidget *) const
{
	mpt::ustring s;
	if(uId)
	{
		uint32 val = GetItemInt(uId);
		CommandID cmd = kcNull;
		switch(uId)
		{
		case IDC_SAMPLE_NEW: s = UL_("Insert Sample"); cmd = kcSampleNew; break;
		case IDC_SAMPLE_OPEN: s = UL_("Import Sample"); cmd = kcSampleLoad; break;
		case IDC_SAMPLE_SAVEAS: s = UL_("Save Sample"); cmd = kcSampleSave; break;
		case IDC_SAMPLE_PLAY: s = UL_("Play Sample"); break;
		case IDC_SAMPLE_NORMALIZE: s = UL_("Normalize (hold shift to normalize all samples)"); cmd = kcSampleNormalize; break;
		case IDC_SAMPLE_AMPLIFY: s = UL_("Amplify"); cmd = kcSampleAmplify; break;
		case IDC_SAMPLE_DCOFFSET: s = UL_("Remove DC Offset and Normalize (hold shift to process all samples)"); cmd = kcSampleRemoveDCOffset; break;
		case IDC_SAMPLE_STEREOSEPARATION: s = UL_("Change Stereo Separation / Stereo Width of the sample"); cmd = kcSampleStereoSep; break;
		case IDC_SAMPLE_RESAMPLE: s = UL_("Resample (hold shift to resample all samples)"); cmd = kcSampleResample; break;
		case IDC_SAMPLE_REVERSE: s = UL_("Reverse"); cmd = kcSampleReverse; break;
		case IDC_SAMPLE_SILENCE: s = UL_("Silence"); cmd = kcSampleSilence; break;
		case IDC_SAMPLE_INVERT: s = UL_("Invert Phase"); cmd = kcSampleInvert; break;
		case IDC_SAMPLE_SIGN_UNSIGN: s = UL_("Signed/Unsigned Conversion"); cmd = kcSampleSignUnsign; break;
		case IDC_SAMPLE_XFADE: s = UL_("Crossfade Sample Loops"); cmd = kcSampleXFade; break;
		case IDC_SAMPLE_AUTOTUNE: s = UL_("Tune the sample to a given note"); cmd = kcSampleAutotune; break;

		case IDC_EDIT7:
		case IDC_EDIT8:
			// Volume to dB
			if(m_nSample)
			{
				const auto globalVol = m_sndFile.GetSample(m_nSample).nGlobalVol;
				if(IsOPLInstrument())
					s = EffectiveOPLVolume(val, (uId == IDC_EDIT7) ? globalVol : 64.0, m_sndFile);
				else
					s = EffectiveSampleVolume(val, 64.0, (uId == IDC_EDIT7) ? globalVol / 64.0 : 1.0, m_sndFile);
			}
			break;

		case IDC_EDIT9:
			// Panning
			if(m_nSample)
			{
				const ModSample &sample = m_sndFile.GetSample(m_nSample);
				s = CModDoc::PanningToString(sample.nPan, 128);
			}
			break;

		case IDC_EDIT5:
		case IDC_SPIN5:
		case IDC_COMBO_BASENOTE:
			if(m_nSample)
			{
				const ModSample &sample = m_sndFile.GetSample(m_nSample);
				const auto freqHz = sample.GetSampleRate(m_sndFile.GetType());
				if(sample.uFlags[CHN_ADLIB])
				{
					// Translate to actual note frequency
					s = MPT_UFORMAT("{}Hz")(mpt::ufmt::flt(freqHz * (261.625 / 8363.0), 6));
				}
				if(m_sndFile.UseFinetuneAndTranspose())
				{
					// Transpose + Finetune to Frequency
					s = MPT_UFORMAT("{}Hz")(freqHz);
				}
			}
			break;

		case IDC_EDIT14:
			// Vibrato Sweep
			if(!(m_sndFile.GetType() & (MOD_TYPE_IT | MOD_TYPE_MPT | MOD_TYPE_XM)))
			{
				s = UL_("Only available in IT / MPTM / XM format");
				break;
			} else if(m_nSample)
			{
				const ModSample &sample = m_sndFile.GetSample(m_nSample);
				int ticks = -1;
				if(m_sndFile.m_playBehaviour[kITVibratoTremoloPanbrello])
				{
					if(val > 0)
						ticks = Util::muldivr_unsigned(sample.nVibDepth, 256, val);
				} else if(m_sndFile.GetType() & (MOD_TYPE_IT | MOD_TYPE_MPT))
				{
					if(val > 0)
						ticks = Util::muldivr_unsigned(sample.nVibDepth, 128, val);
				} else
				{
					ticks = val;
				}
				if(ticks >= 0)
					s = MPT_UFORMAT("{} ticks")(ticks);
				else
					s = UL_("No Vibrato");
			}
			break;
		case IDC_EDIT15:
			// Vibrato Depth
			if(!(m_sndFile.GetType() & (MOD_TYPE_IT | MOD_TYPE_MPT | MOD_TYPE_XM)))
				s = UL_("Only available in IT / MPTM / XM format");
			else
				s = MPT_UFORMAT("{} cents")(Util::muldivr_unsigned(val, 100, 64));
			break;
		case IDC_EDIT16:
			// Vibrato Rate
			if(!(m_sndFile.GetType() & (MOD_TYPE_IT | MOD_TYPE_MPT | MOD_TYPE_XM)))
			{
				s = UL_("Only available in IT / MPTM / XM format");
			} else if(val == 0)
			{
				s = UL_("Stopped");
			} else
			{
				const double ticksPerCycle = 256.0 / val;
				const uint32 ticksPerBeat = std::max(1u, m_sndFile.m_PlayState.m_nCurrentRowsPerBeat * m_sndFile.m_PlayState.m_nMusicSpeed);
				s = MPT_UFORMAT("{} beats per cycle ({} ticks)")(mpt::ufmt::flt(ticksPerCycle / ticksPerBeat, 3), mpt::ufmt::flt(ticksPerCycle, 3));
			}
			break;

		case IDC_CHECK1:
		case IDC_EDIT3:
		case IDC_EDIT4:
		case IDC_COMBO2:
			if(!(m_sndFile.GetType() & (MOD_TYPE_IT | MOD_TYPE_MPT)))
				s = UL_("Only available in IT / MPTM format");
			break;

		case IDC_COMBO3:
			if(!(m_sndFile.GetType() & (MOD_TYPE_IT | MOD_TYPE_MPT | MOD_TYPE_XM)))
				s = UL_("Only available in IT / MPTM / XM format");
			break;

		case IDC_CHECK2:
			s = UL_("Keep a reference to the original waveform instead of saving it in the module.");
			break;
		}
		if(cmd != kcNull)
		{
			auto keyText = CMainFrame::GetInputHandler()->m_activeCommandSet->GetKeyTextFromCommand(cmd, 0);
			if (!keyText.empty())
				s += MPT_UFORMAT(" ({})")(keyText);
		}
	}
	return s;
}


void CCtrlSamples::UpdateView(UpdateHint hint, QObject *pObj)
{
	if(pObj == this) return;
	if (hint.GetType()[HINT_MPTOPTIONS])
	{
		m_ToolBar1->UpdateStyle();
		m_ToolBar2->UpdateStyle();
	}

	const SampleHint sampleHint = hint.ToType<SampleHint>();
	FlagSet<HintType> hintType = sampleHint.GetType();
	if(!m_initialized)
		hintType.set(HINT_MODTYPE);
	if(!hintType[HINT_SMPNAMES | HINT_SAMPLEINFO | HINT_MODTYPE]) return;

	const SAMPLEINDEX updateSmp = sampleHint.GetSample();
	if(updateSmp != m_nSample && updateSmp != 0 && !hintType[HINT_MODTYPE]) return;

	const CModSpecifications &specs = m_sndFile.GetModSpecifications();
	const bool isOPL = IsOPLInstrument();

	LockControls();
	// Updating Ranges
	if(hintType[HINT_MODTYPE])
	{

		// Limit text fields
		m_EditName->setMaxLength(specs.sampleNameLengthMax);
		m_EditFileName->setMaxLength(specs.sampleFilenameLengthMax);

		// Loop Type
		m_ComboLoopType->clear();
		mfcless::AddItem(m_ComboLoopType, UL_("Off"));
		mfcless::AddItem(m_ComboLoopType, UL_("On"));

		// Sustain Loop Type
		m_ComboSustainType->clear();
		mfcless::AddItem(m_ComboSustainType, UL_("Off"));
		mfcless::AddItem(m_ComboSustainType, UL_("On"));

		// Bidirectional Loops
		if (m_sndFile.GetType() & (MOD_TYPE_XM|MOD_TYPE_IT|MOD_TYPE_MPT))
		{
			mfcless::AddItem(m_ComboLoopType, UL_("Bidi"));
			mfcless::AddItem(m_ComboSustainType, UL_("Bidi"));
		}

		// Loop Start
		m_SpinLoopStart->SetRange(-1, 1);
		m_SpinLoopStart->SetPos(0);

		// Loop End
		m_SpinLoopEnd->SetRange(-1, 1);
		m_SpinLoopEnd->SetPos(0);

		// Sustain Loop Start
		m_SpinSustainStart->SetRange(-1, 1);
		m_SpinSustainStart->SetPos(0);

		// Sustain Loop End
		m_SpinSustainEnd->SetRange(-1, 1);
		m_SpinSustainEnd->SetPos(0);

		// Finetune / C-5 Speed / BaseNote
		bool b = m_sndFile.UseFinetuneAndTranspose() ? false : true;
		SetItemText(IDC_TEXT7, (b) ? UL_("Freq. (Hz)") : UL_("Finetune"));
		m_SpinFineTune->SetRange(-1, 1);
		m_EditFileName->setEnabled(b);

		// AutoVibrato
		b = (m_sndFile.GetType() & (MOD_TYPE_XM|MOD_TYPE_IT|MOD_TYPE_MPT)) ? true : false;
		m_ComboAutoVib->setEnabled(b);
		m_SpinVibSweep->setEnabled(b);
		m_SpinVibDepth->setEnabled(b);
		m_SpinVibRate->setEnabled(b);
		m_EditVibSweep->setEnabled(b);
		m_EditVibDepth->setEnabled(b);
		m_EditVibRate->setEnabled(b);
		m_SpinVibSweep->SetRange(0, 255);
		if(m_sndFile.GetType() & MOD_TYPE_XM)
		{
			m_SpinVibDepth->SetRange(0, 15);
			m_SpinVibRate->SetRange(0, 63);
		} else
		{
			m_SpinVibDepth->SetRange(0, 32);
			m_SpinVibRate->SetRange(0, 64);
		}

		// Global Volume
		b = (m_sndFile.GetType() & (MOD_TYPE_IT|MOD_TYPE_MPT)) ? true : false;
		m_EditGlobalVol->setEnabled(b);
		m_SpinGlobalVol->setEnabled(b);

		// Panning
		b = (m_sndFile.GetType() & (MOD_TYPE_XM|MOD_TYPE_IT|MOD_TYPE_MPT)) ? true : false;
		m_CheckPanning->setEnabled(b && !(m_sndFile.GetType() & MOD_TYPE_XM));
		m_EditPanning->setEnabled(b);
		m_SpinPanning->setEnabled(b);
		m_SpinPanning->SetRange(0, (m_sndFile.GetType() == MOD_TYPE_XM) ? 255 : 64);

		b = (m_sndFile.GetType() & MOD_TYPE_MOD) ? false : true;
		m_CbnBaseNote->setEnabled(b);
	}
	// Updating Values
	if (hintType[HINT_MODTYPE | HINT_SAMPLEINFO])
	{
		if(m_nSample > m_sndFile.GetNumSamples())
		{
			SetCurrentSample(m_sndFile.GetNumSamples());
		}
		const ModSample &sample = m_sndFile.GetSample(m_nSample);
		mpt::ustring s;
		uint32 d;

		m_SpinSample->SetRange(1, m_sndFile.GetNumSamples());
		m_SpinSample->update();	// In case the spin button was previously disabled

		// Length / Type
		if(isOPL)
			s = UL_("OPL instrument");
		else
			s = MPT_UFORMAT("{}-bit {}, len: {}")(sample.GetElementarySampleSize() * 8, mpt::ustring(sample.uFlags[CHN_STEREO] ? UL_("stereo") : UL_("mono")), mpt::ufmt::dec(3, UL_(","), sample.nLength));
		SetItemText(IDC_TEXT5, s);
		// File Name
		s = mpt::ToUnicode(m_sndFile.GetCharsetInternal(), sample.filename);
		if (specs.sampleFilenameLengthMax == 0) s.clear();
		SetItemText(IDC_SAMPLE_FILENAME, s);
		// Volume
		if(sample.uFlags[SMP_NODEFAULTVOLUME])
			SetItemText(IDC_EDIT7, UL_("none"));
		else
			SetItemInt(IDC_EDIT7, sample.nVolume / 4u);
		// Global Volume
		SetItemInt(IDC_EDIT8, sample.nGlobalVol);
		// Panning
		SetChecked(IDC_CHECK1, (sample.uFlags[CHN_PANNING]) ? 1 : 0);
		if (m_sndFile.GetType() == MOD_TYPE_XM)
			SetItemInt(IDC_EDIT9, sample.nPan);		//displayed panning with XM is 0-256, just like MPT's internal engine
		else
			SetItemInt(IDC_EDIT9, sample.nPan / 4u);	//displayed panning with anything but XM is 0-64 so we divide by 4
		// FineTune / C-4 Speed / BaseNote
		int transp = 0;
		m_EditFineTune->AllowNegative(m_sndFile.UseFinetuneAndTranspose());
		m_EditFineTune->AllowFractions(false);
		if (!m_sndFile.UseFinetuneAndTranspose())
		{
			s = mpt::ufmt::val(sample.nC5Speed);
			mfcless::SetWindowText(m_EditFineTune, s);
			if(sample.nC5Speed != 0)
				transp = GetSampleTransponse(sample);
		} else
		{
			int ftune = ((int)sample.nFineTune);
			// MOD finetune range -8 to 7 translates to -128 to 112
			if(m_sndFile.GetType() & MOD_TYPE_MOD) ftune >>= 4;
			SetItemInt(IDC_EDIT5, ftune);
			transp = (int)sample.RelativeTone;
		}
		int basenote = (NOTE_MIDDLEC - NOTE_MIN) + transp;
		Limit(basenote, BASENOTE_MIN, BASENOTE_MAX);
		basenote -= BASENOTE_MIN;
		if (basenote != m_CbnBaseNote->currentIndex()) m_CbnBaseNote->setCurrentIndex(basenote);

		// Auto vibrato
		// Ramp up and ramp down are swapped in XM - probably because they ramp up the *period* instead of *frequency*.
		const VibratoType rampUp = m_sndFile.GetType() == MOD_TYPE_XM ? VIB_RAMP_DOWN : VIB_RAMP_UP;
		const VibratoType rampDown = m_sndFile.GetType() == MOD_TYPE_XM ? VIB_RAMP_UP : VIB_RAMP_DOWN;
		m_ComboAutoVib->clear();
		mfcless::SetItemData(m_ComboAutoVib, mfcless::AddItem(m_ComboAutoVib, UL_("Sine")), VIB_SINE);
		mfcless::SetItemData(m_ComboAutoVib, mfcless::AddItem(m_ComboAutoVib, UL_("Square")), VIB_SQUARE);
		if(m_sndFile.GetType() != MOD_TYPE_IT || sample.nVibType == VIB_RAMP_UP)
			mfcless::SetItemData(m_ComboAutoVib, mfcless::AddItem(m_ComboAutoVib, UL_("Ramp Up")), rampUp);
		mfcless::SetItemData(m_ComboAutoVib, mfcless::AddItem(m_ComboAutoVib, UL_("Ramp Down")), rampDown);
		if(m_sndFile.GetType() != MOD_TYPE_XM || sample.nVibType == VIB_RANDOM)
			mfcless::SetItemData(m_ComboAutoVib, mfcless::AddItem(m_ComboAutoVib, UL_("Random")), VIB_RANDOM);

		for(int i = 0; i < m_ComboAutoVib->count(); i++)
		{
			if(mfcless::GetItemData(m_ComboAutoVib, i) == sample.nVibType)
			{
				m_ComboAutoVib->setCurrentIndex(i);
				break;
			}
		}

		SetItemInt(IDC_EDIT14, sample.nVibSweep);
		SetItemInt(IDC_EDIT15, sample.nVibDepth);
		SetItemInt(IDC_EDIT16, sample.nVibRate);
		// Loop
		d = 0;
		if (sample.uFlags[CHN_LOOP]) d = sample.uFlags[CHN_PINGPONGLOOP] ? 2 : 1;
		if (sample.uFlags[CHN_REVERSE]) d |= 4;
		m_ComboLoopType->setCurrentIndex(d);
		s = mpt::ufmt::val(sample.nLoopStart);
		mfcless::SetWindowText(m_EditLoopStart, s);
		s = mpt::ufmt::val(sample.nLoopEnd);
		mfcless::SetWindowText(m_EditLoopEnd, s);
		// Sustain Loop
		d = 0;
		if (sample.uFlags[CHN_SUSTAINLOOP]) d = sample.uFlags[CHN_PINGPONGSUSTAIN] ? 2 : 1;
		m_ComboSustainType->setCurrentIndex(d);
		s = mpt::ufmt::val(sample.nSustainStart);
		mfcless::SetWindowText(m_EditSustainStart, s);
		s = mpt::ufmt::val(sample.nSustainEnd);
		mfcless::SetWindowText(m_EditSustainEnd, s);

		// Disable certain buttons for OPL instruments
		bool b = isOPL ? false : true;
		static constexpr int sampleButtons[] =
		{
			IDC_SAMPLE_NORMALIZE,   IDC_SAMPLE_AMPLIFY,
			IDC_SAMPLE_DCOFFSET,    IDC_SAMPLE_STEREOSEPARATION,
			IDC_SAMPLE_RESAMPLE,    IDC_SAMPLE_REVERSE,
			IDC_SAMPLE_SILENCE,     IDC_SAMPLE_INVERT,
			IDC_SAMPLE_SIGN_UNSIGN, IDC_SAMPLE_XFADE,
		};
		for(auto btn : sampleButtons)
		{
			m_ToolBar2->EnableButton(btn, b);
		}
		m_ComboLoopType->setEnabled(b);
		m_SpinLoopStart->setEnabled(b);
		m_SpinLoopEnd->setEnabled(b);
		m_EditLoopStart->setEnabled(b);
		m_EditLoopEnd->setEnabled(b);

		const bool hasSustainLoop = !isOPL && ((m_sndFile.GetType() & (MOD_TYPE_IT | MOD_TYPE_MPT)) || (m_nSample <= m_sndFile.GetNumSamples() && m_sndFile.GetSample(m_nSample).uFlags[CHN_SUSTAINLOOP]));
		b = hasSustainLoop ? true : false;
		m_ComboSustainType->setEnabled(b);
		m_SpinSustainStart->setEnabled(b);
		m_SpinSustainEnd->setEnabled(b);
		m_EditSustainStart->setEnabled(b);
		m_EditSustainEnd->setEnabled(b);

	}
	if(hintType[HINT_MODTYPE | HINT_SAMPLEINFO | HINT_SMPNAMES])
	{
		// Name
		SetItemText(IDC_SAMPLE_NAME, mpt::ToUnicode(m_sndFile.GetCharsetInternal(), m_sndFile.m_szNames[m_nSample]).c_str());

		SetChecked(IDC_CHECK2, m_sndFile.GetSample(m_nSample).uFlags[SMP_KEEPONDISK] ? 1 : 0);
		EnableItem(IDC_CHECK2, (m_sndFile.SampleHasPath(m_nSample) && m_sndFile.GetType() == MOD_TYPE_MPT) ? true : false);
	}

	if(!m_initialized)
	{
		// First update
		m_initialized = true;
		UnlockControls();
	}

	m_ComboLoopType->update();
	m_ComboSustainType->update();
	m_ComboAutoVib->update();

	UnlockControls();
}


// updateAll: Update all views including this one. Otherwise, only update update other views.
void CCtrlSamples::SetModified(SAMPLEINDEX smp, SampleHint hint, bool updateAll, bool waveformModified)
{
	m_modDoc.SetModified();

	if(waveformModified)
	{
		// Update on-disk sample status in tree
		ModSample &sample = m_sndFile.GetSample(smp);
		if(sample.uFlags[SMP_KEEPONDISK] && !sample.uFlags[SMP_MODIFIED])
			hint.Names();
		sample.uFlags.set(SMP_MODIFIED);
	}
	m_modDoc.UpdateAllViews(nullptr, hint.SetData(smp), updateAll ? nullptr : this);
}


void CCtrlSamples::PrepareUndo(const char *description, sampleUndoTypes type, SmpLength start, SmpLength end)
{
	m_startedEdit = true;
	m_modDoc.GetSampleUndo().PrepareUndo(m_nSample, type, description, start, end);
}


bool CCtrlSamples::OpenSample(const mpt::PathString &fileName, FlagSet<OpenSampleTypes> types)
{
	mfcless::BeginWaitCursor();
	mpt::IO::InputFile f(fileName, TrackerSettings::Instance().MiscCacheCompleteFileBeforeLoading);
	if(!f.IsValid())
	{
		mfcless::EndWaitCursor();
		return false;
	}

	FileReader file = GetFileReader(f);
	if(!file.IsValid())
	{
		mfcless::EndWaitCursor();
		return false;
	}

	PrepareUndo("Replace", sundo_replace);
	const auto parentIns = m_modDoc.GetParentInstrumentWithSameName(m_nSample);
	bool bOk = false;
	if(types[OpenSampleKnown])
	{
		bOk = m_sndFile.ReadSampleFromFile(m_nSample, file, TrackerSettings::Instance().m_MayNormalizeSamplesOnLoad);

		if(!bOk)
		{
			// Try loading as module
			bOk = CMainFrame::GetMainFrame()->SetTreeSoundfile(file);
			if(bOk)
			{
				m_modDoc.GetSampleUndo().RemoveLastUndoStep(m_nSample);
				return true;
			}
		}
	}
	ModSample &sample = m_sndFile.GetSample(m_nSample);

	if(!bOk && types[OpenSampleRaw])
	{
		CRawSampleDlg dlg(file, this);
		mfcless::EndWaitCursor();
		if(m_rememberRawFormat || dlg.DoModal() == IDOK)
		{
			SampleIO sampleIO   = dlg.GetSampleFormat();
			m_rememberRawFormat = m_rememberRawFormat || dlg.GetRemeberFormat();

			mfcless::BeginWaitCursor();

			file.Seek(dlg.GetOffset());

			m_sndFile.DestroySampleThreadsafe(m_nSample);
			const auto bytesPerSample = sampleIO.GetNumChannels() * sampleIO.GetBitDepth() / 8u;
			sample.nLength            = mpt::saturate_cast<SmpLength>(file.BytesLeft() / bytesPerSample);

			if(TrackerSettings::Instance().m_MayNormalizeSamplesOnLoad)
			{
				sampleIO.MayNormalize();
			}

			if(sampleIO.ReadSample(sample, file))
			{
				bOk = true;

				sample.nGlobalVol = 64;
				sample.nVolume = 256;
				sample.nPan = 128;
				sample.uFlags.reset(CHN_LOOP | CHN_SUSTAINLOOP | SMP_MODIFIED);
				sample.filename = "";
				m_sndFile.m_szNames[m_nSample] = "";
				if(!sample.nC5Speed) sample.nC5Speed = 22050;
				sample.PrecomputeLoops(m_sndFile, false);
			} else
			{
				m_modDoc.GetSampleUndo().Undo(m_nSample);
			}

		} else
		{
			m_modDoc.GetSampleUndo().RemoveLastUndoStep(m_nSample);
		}
	} else
	{
		m_sndFile.SetSamplePath(m_nSample, fileName);
	}

	mfcless::EndWaitCursor();
	if (bOk)
	{
		TrackerSettings::Instance().PathSamples.SetWorkingDir(fileName, true);
		if(sample.filename.empty())
		{
			mpt::PathString name, ext;
			fileName.SplitPath(nullptr, nullptr, nullptr, &name, &ext);

			if(m_sndFile.m_szNames[m_nSample].empty()) m_sndFile.m_szNames[m_nSample] = name.ToLocale();

			if(name.AsNative().length() < 9) name += ext;
			sample.filename = name.ToLocale();
		}
		if ((m_sndFile.GetType() & MOD_TYPE_XM) && !sample.uFlags[CHN_PANNING])
		{
			sample.nPan = 128;
			sample.uFlags.set(CHN_PANNING);
		}
		SetModified(SampleHint().Info().Data().Names(), true, false);
		sample.uFlags.reset(SMP_KEEPONDISK);

		if(parentIns <= m_sndFile.GetNumInstruments())
		{
			if(auto instr = m_sndFile.Instruments[parentIns]; instr != nullptr)
			{
				m_modDoc.GetInstrumentUndo().PrepareUndo(parentIns, "Set Name");
				instr->name = m_sndFile.m_szNames[m_nSample];
				m_modDoc.UpdateAllViews(nullptr, InstrumentHint(parentIns).Names(), this);
			}
		}
	}
	return true;
}


bool CCtrlSamples::OpenSample(const CSoundFile &sndFile, SAMPLEINDEX nSample)
{
	if(!nSample || nSample > sndFile.GetNumSamples()) return false;

	mfcless::BeginWaitCursor();

	PrepareUndo("Replace", sundo_replace);
	const auto parentIns = m_modDoc.GetParentInstrumentWithSameName(m_nSample);
	if(m_sndFile.ReadSampleFromSong(m_nSample, sndFile, nSample))
	{
		SetModified(SampleHint().Info().Data().Names(), true, false);
		if(parentIns <= m_sndFile.GetNumInstruments())
		{
			if(auto instr = m_sndFile.Instruments[parentIns]; instr != nullptr)
			{
				m_modDoc.GetInstrumentUndo().PrepareUndo(parentIns, "Set Name");
				instr->name = m_sndFile.m_szNames[m_nSample];
				m_modDoc.UpdateAllViews(nullptr, InstrumentHint(parentIns).Names(), this);
			}
		}
	} else
	{
		m_modDoc.GetSampleUndo().RemoveLastUndoStep(m_nSample);
	}

	mfcless::EndWaitCursor();

	return true;
}


//////////////////////////////////////////////////////////////////////////////////
// CCtrlSamples messages

void CCtrlSamples::OnSampleChanged()
{
	if(!IsLocked())
	{
		SAMPLEINDEX n = (SAMPLEINDEX)GetItemInt(IDC_EDIT_SAMPLE);
		if ((n > 0) && (n <= m_sndFile.GetNumSamples()) && (n != m_nSample))
		{
			SetCurrentSample(n, -1, false);
			m_parent.SampleChanged(m_nSample);
		}
	}
}


void CCtrlSamples::OnZoomChanged()
{
	if (!IsLocked()) SetCurrentSample(m_nSample);
	SwitchToViewIfMouse();
}


void CCtrlSamples::OnTbnDropDownToolBar()
{
	CInputHandler *ih = CMainFrame::GetInputHandler();
	NMTOOLBAR *pToolBar = reinterpret_cast<NMTOOLBAR *>(pNMHDR);
	ClientToScreen(&(pToolBar->rcButton)); // TrackPopupMenu uses screen coords
	const int offset = HighDPISupport::ScalePixels(4, this);  // Compared to the main toolbar, the offset seems to be a bit wrong here...?
	int x = pToolBar->rcButton.left + offset, y = pToolBar->rcButton.bottom + offset;
	mfcless::Menu menu;
	switch(pToolBar->iItem)
	{
	case IDC_SAMPLE_NEW:
		menu.CreatePopupMenu();
		menu.AppendMenu(mfcless::menu::String, IDC_SAMPLE_DUPLICATE, ih->GetKeyTextFromCommand(kcSampleDuplicate, m_sndFile.GetSample(m_nSample).uFlags[CHN_ADLIB] ? UL_("&Duplicate Instrument") : UL_("&Duplicate Sample")));
		menu.AppendMenu(mfcless::menu::String | (m_sndFile.SupportsOPL() ? 0 : mfcless::menu::Disabled), IDC_SAMPLE_INITOPL, ih->GetKeyTextFromCommand(kcSampleInitializeOPL, UL_("Initialize &OPL Instrument")));
		menu.TrackPopupMenu(mfcless::menu::LeftAlign | mfcless::menu::RightButton, x, y, this);
		menu.DestroyMenu();
		break;
	case IDC_SAMPLE_OPEN:
		menu.CreatePopupMenu();
		menu.AppendMenu(mfcless::menu::String, IDC_SAMPLE_OPENKNOWN, ih->GetKeyTextFromCommand(kcSampleLoad, UL_("Import &Sample...")));
		menu.AppendMenu(mfcless::menu::String, IDC_SAMPLE_OPENRAW, ih->GetKeyTextFromCommand(kcSampleLoadRaw, UL_("Import &Raw Sample...")));
		menu.TrackPopupMenu(mfcless::menu::LeftAlign | mfcless::menu::RightButton, x, y, this);
		menu.DestroyMenu();
		break;
	case IDC_SAMPLE_SAVEAS:
		menu.CreatePopupMenu();
		menu.AppendMenu(mfcless::menu::String, IDC_SAVE_ALL, UL_("Save &All..."));
		menu.TrackPopupMenu(mfcless::menu::LeftAlign | mfcless::menu::RightButton, x, y, this);
		menu.DestroyMenu();
		break;
	}
	CurrentNotify().result = 0;
}


void CCtrlSamples::OnSampleNew()
{
	InsertSample(CInputHandler::ShiftPressed());
	SwitchToViewIfMouse();
}


bool CCtrlSamples::InsertSample(bool duplicate, int8 *confirm)
{
	const SAMPLEINDEX smp = m_modDoc.InsertSample();
	if(smp != SAMPLEINDEX_INVALID)
	{
		const SAMPLEINDEX oldSmp = m_nSample;
		CSoundFile &sndFile = m_modDoc.GetSoundFile();
		SetCurrentSample(smp);

		if(duplicate && oldSmp >= 1 && oldSmp <= sndFile.GetNumSamples())
		{
			m_modDoc.GetSampleUndo().PrepareUndo(smp, sundo_replace, "Duplicate");
			sndFile.ReadSampleFromSong(smp, sndFile, oldSmp);
		}

		m_modDoc.UpdateAllViews(nullptr, SampleHint(smp).Info().Data().Names());
		if(m_modDoc.GetNumInstruments() > 0 && m_modDoc.FindSampleParent(smp) == INSTRUMENTINDEX_INVALID)
		{
			bool insertInstrument;
			if(confirm == nullptr || *confirm == -1)
			{
				insertInstrument = Reporting::Confirm("This sample is not used by any instrument. Do you want to create a new instrument using this sample?") == cnfYes;
				if(confirm != nullptr) *confirm = insertInstrument;
			} else
			{
				insertInstrument = (*confirm) != 0;
			}
			if(insertInstrument)
			{
				INSTRUMENTINDEX nins = m_modDoc.InsertInstrument(smp);
				m_modDoc.UpdateAllViews(nullptr, InstrumentHint(nins).Info().Envelope().Names());
				m_parent.InstrumentChanged(nins);
			}
		}
	}
	return (smp != SAMPLEINDEX_INVALID);
}


void CCtrlSamples::OnSampleOpen()
{
	static int nLastIndex = 0;
	FileDialog dlg = OpenFileDialog()
		.AllowMultiSelect()
		.EnableAudioPreview()
		.ExtensionFilter(ConstructSampleFormatFileFilter(true))
		.WorkingDirectory(TrackerSettings::Instance().PathSamples.GetWorkingDir())
		.FilterIndex(&nLastIndex);
	if(!dlg.Show(this)) return;

	TrackerSettings::Instance().PathSamples.SetWorkingDir(dlg.GetWorkingDirectory());

	OpenSamples(dlg.GetFilenames(), OpenSampleKnown | OpenSampleRaw);
	SwitchToViewIfMouse();
}


void CCtrlSamples::OnSampleOpenKnown()
{
	static int nLastIndex = 0;
	FileDialog dlg = OpenFileDialog()
		.AllowMultiSelect()
		.EnableAudioPreview()
		.ExtensionFilter(ConstructSampleFormatFileFilter(false))
		.WorkingDirectory(TrackerSettings::Instance().PathSamples.GetWorkingDir())
		.FilterIndex(&nLastIndex);
	if(!dlg.Show(this)) return;

	TrackerSettings::Instance().PathSamples.SetWorkingDir(dlg.GetWorkingDirectory());

	OpenSamples(dlg.GetFilenames(), OpenSampleKnown);
}


void CCtrlSamples::OnSampleOpenRaw()
{
	static int nLastIndex = 0;
	FileDialog dlg = OpenFileDialog()
		.AllowMultiSelect()
		.EnableAudioPreview()
		.ExtensionFilter(U_("Raw Samples (*.raw,*.snd,*.pcm,*.sam)|*.raw;*.snd;*.pcm;*.sam|All Files (*.*)|*.*||"))
		.WorkingDirectory(TrackerSettings::Instance().PathSamples.GetWorkingDir())
		.FilterIndex(&nLastIndex);
	if(!dlg.Show(this)) return;

	TrackerSettings::Instance().PathSamples.SetWorkingDir(dlg.GetWorkingDirectory());

	OpenSamples(dlg.GetFilenames(), OpenSampleRaw);
}


void CCtrlSamples::OpenSamples(const std::vector<mpt::PathString> &files, FlagSet<OpenSampleTypes> types)
{
	int8 confirm = -1;
	bool first = true;
	for(const auto &file : files)
	{
		// If loading multiple samples, create new slots for them
		if(!first)
		{
			if(!InsertSample(false, &confirm))
				break;
		}

		if(OpenSample(file, types))
			first = false;
		else
			ErrorBox(IDS_ERR_FILEOPEN, this);
	}
	SwitchToViewIfMouse();
}


void CCtrlSamples::OnSampleSave()
{
	SaveSample(CInputHandler::ShiftPressed());
}


void CCtrlSamples::SaveSample(bool doBatchSave)
{
	mpt::PathString fileName, defaultPath = TrackerSettings::Instance().PathSamples.GetWorkingDir();
	SampleEditorDefaultFormat defaultFormat = TrackerSettings::Instance().m_defaultSampleFormat;
	bool hasAdlib = false;

	if(!doBatchSave)
	{
		// Save this sample
		const ModSample &sample = m_sndFile.GetSample(m_nSample);
		if((!m_nSample) || (!sample.HasSampleData()))
		{
			SwitchToViewIfMouse();
			return;
		}
		if(m_sndFile.SampleHasPath(m_nSample))
		{
			// For on-disk samples, propose their original filename and location
			auto path = m_sndFile.GetSamplePath(m_nSample);
			fileName = path.GetFilename();
			defaultPath = path.GetDirectoryWithDrive();
		}
		if(fileName.empty()) fileName = mpt::PathString::FromLocale(sample.filename);
		if(fileName.empty()) fileName = mpt::PathString::FromLocale(m_sndFile.m_szNames[m_nSample]);
		if(fileName.empty()) fileName = P_("untitled");

		const mpt::PathString ext = fileName.GetFilenameExtension();
		if(!mpt::PathCompareNoCase(ext, P_(".flac"))) defaultFormat = dfFLAC;
		else if(!mpt::PathCompareNoCase(ext, P_(".wav"))) defaultFormat = dfWAV;
		else if(!mpt::PathCompareNoCase(ext, P_(".s3i"))) defaultFormat = dfS3I;
		else if(!mpt::PathCompareNoCase(ext, P_(".iff"))) defaultFormat = dfIFF;
		else if(!mpt::PathCompareNoCase(ext, P_(".8sv"))) defaultFormat = dfIFF;
		else if(!mpt::PathCompareNoCase(ext, P_(".8svx"))) defaultFormat = dfIFF;
		else if(!mpt::PathCompareNoCase(ext, P_(".16sv"))) defaultFormat = dfIFF;
		else if(!mpt::PathCompareNoCase(ext, P_(".16svx"))) defaultFormat = dfIFF;

		hasAdlib = sample.uFlags[CHN_ADLIB];
	} else
	{
		// Save all samples
		fileName = m_sndFile.GetpModDoc()->GetPathNameMpt().GetFilenameBase();
		if(fileName.empty()) fileName = P_("untitled");

		fileName += P_(" - %sample_number% - ");
		if(m_sndFile.GetModSpecifications().sampleFilenameLengthMax == 0)
			fileName += P_("%sample_name%");
		else
			fileName += P_("%sample_filename%");
	}
	fileName = fileName.AsSanitizedComponent();

	int filter;
	switch(defaultFormat)
	{
	case dfWAV:
		filter = 1;
		break;
	case dfFLAC:
	default:
		filter = 2;
		break;
	case dfS3I:
		filter = 3;
		break;
	case dfIFF:
		filter = 4;
		break;
	case dfRAW:
		filter = 5;
		break;
	}
	// Do we have to use a format that can save OPL instruments?
	if(hasAdlib)
		filter = 3;

	FileDialog dlg = SaveFileDialog()
		.DefaultExtension(ToSettingValue(defaultFormat).as<mpt::ustring>())
		.DefaultFilename(fileName)
		.ExtensionFilter("Wave File (*.wav)|*.wav|"
			"FLAC File (*.flac)|*.flac|"
			"S3I Scream Tracker 3 Instrument (*.s3i)|*.s3i|"
			"IFF (8SVX / 16SVX) (*.iff)|*.iff;*.8sv;*.8svx;*.16sv;*.16svx|"
			"RAW Audio (*.raw)|*.raw||")
			.WorkingDirectory(defaultPath)
			.FilterIndex(&filter);
	if(!dlg.Show(this)) return;

	mfcless::BeginWaitCursor();

	const auto saveFormat = FromSettingValue<SampleEditorDefaultFormat>(dlg.GetExtension().ToUnicode());

	SAMPLEINDEX minSmp = m_nSample, maxSmp = m_nSample;
	if(doBatchSave)
	{
		minSmp = 1;
		maxSmp = m_sndFile.GetNumSamples();
	}
	const auto numberFmt = mpt::format_simple_spec<mpt::ustring>().Dec().FillNul().Width(1 + static_cast<int>(std::log10(maxSmp)));

	bool ok = false;

	for(SAMPLEINDEX smp = minSmp; smp <= maxSmp; smp++)
	{
		ModSample &sample = m_sndFile.GetSample(smp);
		if(sample.HasSampleData())
		{
			const bool isAdlib = sample.uFlags[CHN_ADLIB];

			fileName = dlg.GetFirstFile();
			if(doBatchSave)
			{
				mpt::ustring sampleName = mpt::ToUnicode(m_sndFile.GetCharsetInternal(), (!m_sndFile.m_szNames[smp].empty()) ? std::string(m_sndFile.m_szNames[smp]) : "untitled");
				mpt::ustring sampleFilename = mpt::ToUnicode(m_sndFile.GetCharsetInternal(), (!sample.filename.empty()) ? sample.GetFilename() : m_sndFile.m_szNames[smp]);
				sampleName = mpt::SanitizePathComponent(sampleName);
				sampleFilename = mpt::SanitizePathComponent(sampleFilename);

				mpt::ustring fileNameU = fileName.ToUnicode();
				fileNameU = mpt::replace(fileNameU, U_("%sample_number%"), mpt::ufmt::fmt(smp, numberFmt));
				fileNameU = mpt::replace(fileNameU, U_("%sample_filename%"), sampleFilename);
				fileNameU = mpt::replace(fileNameU, U_("%sample_name%"), sampleName);
				fileName = mpt::PathString::FromUnicode(fileNameU);

				// Need to enforce S3I for Adlib samples
				if(isAdlib && saveFormat != dfS3I)
					fileName = fileName.ReplaceExtension(P_(".s3i"));
			}

			try
			{
				mpt::IO::SafeOutputFile sf(fileName, std::ios::binary, mpt::IO::FlushModeFromBool(TrackerSettings::Instance().MiscFlushFileBuffersOnSave));
				mpt::IO::ofstream &f = sf;
				if(!f)
				{
					ok = false;
					continue;
				}
				f.exceptions(f.exceptions() | std::ios::badbit | std::ios::failbit);

				// Need to enforce S3I for Adlib samples
				const auto thisFormat = isAdlib ? dfS3I : saveFormat;
				if(thisFormat == dfRAW)
					ok = m_sndFile.SaveRAWSample(smp, f);
				else if(thisFormat == dfFLAC)
					ok = m_sndFile.SaveFLACSample(smp, f);
				else if(thisFormat == dfS3I)
					ok = m_sndFile.SaveS3ISample(smp, f);
				else if(thisFormat == dfIFF)
					ok = m_sndFile.SaveIFFSample(smp, f);
				else
					ok = m_sndFile.SaveWAVSample(smp, f);
			} catch(const std::exception &)
			{
				ok = false;
			}

			if(ok)
			{
				m_sndFile.SetSamplePath(smp, fileName);
				sample.uFlags.reset(SMP_MODIFIED);
				UpdateView(SampleHint().Info());

				// Check if any other samples refer to the same file - that would be dangerous.
				if(sample.uFlags[SMP_KEEPONDISK])
				{
					for(SAMPLEINDEX i = 1; i <= m_sndFile.GetNumSamples(); i++)
					{
						if(i != smp && m_sndFile.GetSample(i).uFlags[SMP_KEEPONDISK] && m_sndFile.GetSamplePath(i) == m_sndFile.GetSamplePath(smp))
						{
							m_sndFile.GetSample(i).uFlags.reset(SMP_KEEPONDISK);
							m_modDoc.UpdateAllViews(nullptr, SampleHint(i).Names().Info(), this);
						}
					}
				}
			}
		}
	}
	mfcless::EndWaitCursor();

	if (!ok)
	{
		ErrorBox(IDS_ERR_SAVESMP, this);
	} else
	{
		TrackerSettings::Instance().PathSamples.SetWorkingDir(dlg.GetWorkingDirectory());
	}
	SwitchToViewIfMouse();
}


void CCtrlSamples::OnSamplePlay()
{
	if (m_modDoc.IsNotePlaying(NOTE_NONE, m_nSample, 0))
	{
		m_modDoc.NoteOff(0, true);
	} else
	{
		m_modDoc.PlayNote(PlayNoteParam(NOTE_MIDDLEC).Sample(m_nSample));
	}
	SwitchToViewIfMouse();
}


void CCtrlSamples::Normalize(bool allSamples)
{
	//Default case: Normalize current sample
	SAMPLEINDEX minSample = m_nSample, maxSample = m_nSample;
	//If only one sample is selected, parts of it may be amplified
	SmpLength selStart = 0, selEnd = 0;

	if(allSamples)
	{
		if(Reporting::Confirm(UL_("This will normalize all samples independently. Continue?"), UL_("Normalize")) == cnfNo)
			return;
		minSample = 1;
		maxSample = m_sndFile.GetNumSamples();
	} else
	{
		SampleSelectionPoints selection = GetSelectionPoints();
		selStart = selection.nStart;
		selEnd = selection.nEnd;
	}


	mfcless::BeginWaitCursor();

	for(SAMPLEINDEX smp = minSample; smp <= maxSample; smp++)
	{
		if(m_sndFile.GetSample(smp).HasSampleData())
		{
			ModSample &sample = m_sndFile.GetSample(smp);

			if(minSample != maxSample)
			{
				// If more than one sample is selected, always amplify the whole sample.
				selStart = 0;
				selEnd = sample.nLength;
			} else
			{
				// One sample: correct the boundaries, if needed
				LimitMax(selEnd, sample.nLength);
				LimitMax(selStart, selEnd);
				if(selStart == selEnd)
				{
					selStart = 0;
					selEnd = sample.nLength;
				}
			}

			m_modDoc.GetSampleUndo().PrepareUndo(smp, sundo_update, "Normalize", selStart, selEnd);

			if(SampleEdit::NormalizeSample(sample, selStart, selEnd, m_sndFile))
			{
				SetModified(smp, SampleHint().Data(), smp == m_nSample, true);
			}
		}
	}

	mfcless::EndWaitCursor();
	SwitchToViewIfMouse();
}


void CCtrlSamples::OnNormalize()
{
	Normalize(CInputHandler::ShiftPressed());
}


void CCtrlSamples::RemoveDCOffset(bool allSamples)
{
	SAMPLEINDEX minSample = m_nSample, maxSample = m_nSample;

	//Shift -> Process all samples
	if(allSamples)
	{
		if(Reporting::Confirm(UL_("This will process all samples independently. Continue?"), UL_("DC Offset Removal")) == cnfNo)
			return;
		minSample = 1;
		maxSample = m_sndFile.GetNumSamples();
	}

	mfcless::BeginWaitCursor();

	// for report / SetModified
	SAMPLEINDEX numModified = 0;
	double reportOffset = 0;

	for(SAMPLEINDEX smp = minSample; smp <= maxSample; smp++)
	{
		SmpLength selStart, selEnd;

		if(!m_sndFile.GetSample(smp).HasSampleData())
			continue;

		if (minSample != maxSample)
		{
			selStart = 0;
			selEnd = m_sndFile.GetSample(smp).nLength;
		} else
		{
			SampleSelectionPoints selection = GetSelectionPoints();
			selStart = selection.nStart;
			selEnd = selection.nEnd;
		}

		m_modDoc.GetSampleUndo().PrepareUndo(smp, sundo_update, "Remove DC Offset", selStart, selEnd);

		const double offset = SampleEdit::RemoveDCOffset(m_sndFile.GetSample(smp), selStart, selEnd, m_sndFile);

		if(offset == 0.0f) // No offset removed.
			continue;

		reportOffset += offset;
		numModified++;
		SetModified(smp, SampleHint().Info().Data(), smp == m_nSample, true);
	}

	mfcless::EndWaitCursor();
	SwitchToViewIfMouse();

	// fill the statusbar with some nice information

	mpt::ustring dcInfo;
	if(numModified)
	{
		if(numModified == 1)
		{
			dcInfo = mfcless::PrintF(UL_("Removed DC offset (%.1f%%)"), reportOffset * 100);
		} else
		{
			dcInfo = mfcless::PrintF(UL_("Removed DC offset from %u samples (avg %0.1f%%)"), numModified, reportOffset / numModified * 100);
		}
	} else
	{
		dcInfo.SetString(UL_("No DC offset found"));
	}

	CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
	pMainFrm->SetXInfoText(dcInfo);

}


void CCtrlSamples::OnRemoveDCOffset()
{
	RemoveDCOffset(CInputHandler::ShiftPressed());
}


void CCtrlSamples::ApplyAmplify(const double amp, const double fadeInStart, const double fadeOutEnd, const bool fadeIn, const bool fadeOut, const Fade::Law fadeLaw)
{
	ModSample &sample = m_sndFile.GetSample(m_nSample);
	if(!sample.HasSampleData() || sample.uFlags[CHN_ADLIB]) return;

	mfcless::BeginWaitCursor();

	SampleSelectionPoints selection = GetSelectionPoints();
	const auto start = selection.nStart, end = selection.nEnd, mid = (start + end) / 2;

	PrepareUndo("Amplify", sundo_update, start, end);

	if(fadeIn && fadeOut)
	{
		SampleEdit::AmplifySample(sample, start, mid, fadeInStart, amp, true, fadeLaw, m_sndFile);
		SampleEdit::AmplifySample(sample, mid, end, amp, fadeOutEnd, false, fadeLaw, m_sndFile);
	} else if(fadeIn)
	{
		SampleEdit::AmplifySample(sample, start, end, fadeInStart, amp, true, fadeLaw, m_sndFile);
	} else if(fadeOut)
	{
		SampleEdit::AmplifySample(sample, start, end, amp, fadeOutEnd, false, fadeLaw, m_sndFile);
	} else
	{
		SampleEdit::AmplifySample(sample, start, end, amp, amp, true, Fade::kLinear, m_sndFile);
	}

	sample.PrecomputeLoops(m_sndFile, false);
	SetModified(SampleHint().Data(), false, true);
	mfcless::EndWaitCursor();
	SwitchToViewIfMouse();
}


void CCtrlSamples::OnAmplify()
{
	static CAmpDlg::AmpSettings settings { Fade::kLinear, 0, 0, 100, false, false };

	CAmpDlg dlg(this, settings);
	if (dlg.DoModal() != IDOK) return;

	ApplyAmplify(settings.factor / 100.0, settings.fadeInStart / 100.0, settings.fadeOutEnd / 100.0, settings.fadeIn, settings.fadeOut, settings.fadeLaw);
}


// Quickly fade the selection in/out without asking the user.
// Fade-In is applied if the selection starts at the beginning of the sample.
// Fade-Out is applied if the selection ends and the end of the sample.
void CCtrlSamples::OnQuickFade()
{
	ModSample &sample = m_sndFile.GetSample(m_nSample);
	if(!sample.HasSampleData() || sample.uFlags[CHN_ADLIB]) return;

	SampleSelectionPoints sel = GetSelectionPoints();
	if(sel.selectionActive && (sel.nStart == 0 || sel.nEnd == sample.nLength))
	{
		ApplyAmplify(1.0, (sel.nStart == 0) ? 0.0 : 1.0, (sel.nEnd == sample.nLength) ? 0.0 : 1.0, sel.nStart == 0, sel.nEnd == sample.nLength, Fade::kLinear);
	} else
	{
		// Can't apply quick fade as no appropriate selection has been made, so ask the user to amplify the whole sample instead.
		OnAmplify();
	}
}


void CCtrlSamples::OnResample()
{
	ModSample &sample = m_sndFile.GetSample(m_nSample);
	if(!sample.HasSampleData() || sample.uFlags[CHN_ADLIB])
		return;

	SAMPLEINDEX first = m_nSample, last = m_nSample;
	if(CInputHandler::ShiftPressed())
	{
		first = 1;
		last = m_sndFile.GetNumSamples();
	}
	
	const uint32 oldRate = sample.GetSampleRate(m_sndFile.GetType());
	CResamplingDlg dlg(this, oldRate, TrackerSettings::Instance().sampleEditorDefaultResampler, first != last, m_sndFile.GetBestSaveFormat() == MOD_TYPE_MOD);
	if(dlg.DoModal() != IDOK)
		return;
	
	TrackerSettings::Instance().sampleEditorDefaultResampler = dlg.GetFilter();
	for(SAMPLEINDEX smp = first; smp <= last; smp++)
	{
		const uint32 sampleFreq = m_sndFile.GetSample(smp).GetSampleRate(m_sndFile.GetType());
		uint32 newFreq = dlg.GetFrequency();
		if(dlg.GetResamplingOption() == CResamplingDlg::Upsample)
			newFreq = sampleFreq * 2;
		else if(dlg.GetResamplingOption() == CResamplingDlg::Downsample)
			newFreq = sampleFreq / 2;
		else if(newFreq == sampleFreq)
			continue;
		ApplyResample(smp, newFreq, dlg.GetFilter(), first != last, dlg.UpdatePatternCommands(), dlg.UpdatePatternNotes());
	}
}


void CCtrlSamples::ApplyResample(SAMPLEINDEX smp, uint32 newRate, ResamplingMode mode, bool ignoreSelection, bool updatePatternCommands, bool updatePatternNotes)
{
	mfcless::BeginWaitCursor();

	ModSample &sample = m_sndFile.GetSample(smp);
	if(!sample.HasSampleData() || sample.uFlags[CHN_ADLIB])
	{
		mfcless::EndWaitCursor();
		return;
	}

	SampleSelectionPoints selection = GetSelectionPoints();
	LimitMax(selection.nEnd, sample.nLength);
	if(selection.nStart >= selection.nEnd || ignoreSelection)
	{
		selection.nStart = 0;
		selection.nEnd = sample.nLength;
	}

	const uint32 oldRate = sample.GetSampleRate(m_sndFile.GetType());
	if(newRate < 1 || oldRate < 1)
	{
		mfcless::MessageBeep();
		mfcless::EndWaitCursor();
		return;
	}

	auto prepareSampleUndoFunc = [&]()
	{
		m_modDoc.GetSampleUndo().PrepareUndo(smp, sundo_replace, (newRate > oldRate) ? "Upsample" : "Downsample");
	};
	auto preparePatternUndoFunc = [&]()
	{
		m_modDoc.PrepareUndoForAllPatterns(false, "Resample (Adjust Offsets)");
	};
	SmpLength newSelEnd = SampleEdit::Resample(sample, selection.nStart, selection.nEnd, newRate, mode, m_sndFile, updatePatternCommands, updatePatternNotes, prepareSampleUndoFunc, preparePatternUndoFunc);
	if(!newSelEnd)
	{
		mfcless::MessageBeep();
		mfcless::EndWaitCursor();
		return;
	}

	SetModified(smp, SampleHint().Info().Data(), smp == m_nSample, true);
	if(updatePatternCommands || updatePatternNotes)
	{
		m_modDoc.UpdateAllViews(nullptr, PatternHint().Data(), this);
	}

	if(selection.selectionActive && !ignoreSelection)
	{
		SetSelectionPoints(selection.nStart, newSelEnd);
	}

	mfcless::EndWaitCursor();
	SwitchToViewIfMouse();
}


void CCtrlSamples::OnToggleTimestretchQuality()
{
	// Enable time-stretching / disable unused pitch-shifting UI elements
	bool isLoFi = (IsChecked(IDC_CHECK3) != 0) ? true : false;
	m_ComboGrainSize->setEnabled(isLoFi ? true : false);
}

void CCtrlSamples::OnEstimateSampleSize()
{
	if(!m_sndFile.GetSample(m_nSample).HasSampleData())
		return;

	double ratio = 100.0;
	m_EditTimeStretchRatio->GetDecimalValue(ratio);
	CPSRatioCalc dlg{m_sndFile, m_nSample, ratio, this};
	if (dlg.DoModal() != IDOK)
		return;

	m_EditTimeStretchRatio->SetDecimalValue(dlg.m_ratio);
}


class DoPitchShiftTimeStretch : public CProgressDialog
{
public:
	DoPitchShiftTimeStretch(CCtrlSamples &parent, CModDoc &modDoc, SAMPLEINDEX sample, SmpLength start, SmpLength end, float pitch, float stretchRatio, int grainSize, bool loFi)
		: CProgressDialog{&parent}
		, m_updateInterval{std::max(uint32(15), TrackerSettings::Instance().GUIUpdateInterval.Get())}
	{
		const auto updateFunc = [this](SmpLength current, SmpLength maximum) { return UpdateProgress(current, maximum); };
		const auto prepareUndo = [&parent]() { return parent.PrepareUndo("Pitch Shift / Time Stretch", sundo_replace); };

		CSoundFile &sndFile = modDoc.GetSoundFile();
		if(loFi)
			m_instance = std::make_unique<TimeStretchPitchShift::LoFi>(updateFunc, prepareUndo, sndFile, sample, pitch, stretchRatio, start, end, grainSize);
		else
			m_instance = std::make_unique<TimeStretchPitchShift::Signalsmith>(updateFunc, prepareUndo, sndFile, sample, pitch, stretchRatio, start, end);
	}

	SmpLength NewSelectionEnd() const { return m_instance->NewSelectionEnd(); }

private:
	void Run() override
	{
		SetTitle(UL_("Pitch Shift / Time Stretch"));
		SetRange(0, 100);

		mfcless::BeginWaitCursor();
		m_result = m_instance->Process();
		mfcless::EndWaitCursor();

		EndDialog((m_result == TimeStretchPitchShift::Result::OK) ? IDOK : IDCANCEL);
	}

	bool UpdateProgress(SmpLength current, SmpLength maximum)
	{
		uint32 timeNow = mfcless::TickCount();
		if(timeNow - m_lastUpdate < m_updateInterval)
			return m_abort;

		m_lastUpdate = timeNow;
		mpt::ustring progress;
		uint32 percent = Util::muldivr(current, 100, maximum);
		progress = mfcless::PrintF(UL_("Pitch Shift / Time Stretch... %u%%"), percent);
		SetText(progress);
		SetProgress(percent);
		ProcessMessages();
		return m_abort;
	}

private:
	std::unique_ptr<TimeStretchPitchShift::Base> m_instance;
	const uint32 m_updateInterval;
	uint32 m_lastUpdate = 0;
public:
	TimeStretchPitchShift::Result m_result = TimeStretchPitchShift::Result::Abort;
};


void CCtrlSamples::OnPitchShiftTimeStretch()
{
	auto errorCode = TimeStretchPitchShift::Result::Abort;
	ModSample &sample = m_sndFile.GetSample(m_nSample);
	if(!sample.HasSampleData())
		return;
	
	double semitones = 0.0f;
	m_EditPitch->GetDecimalValue(semitones);
	const double pitch = std::pow(2.0, semitones / 12.0);

	double ratio = 100.0;
	m_EditTimeStretchRatio->GetDecimalValue(ratio);

	const auto grainSize = static_cast<int>(GetItemInt(IDC_COMBO5));

	if(pitch != 1.0 || ratio != 100.0)
	{
		auto selection = GetSelectionPoints();
		DoPitchShiftTimeStretch timeStretch(*this, m_modDoc, m_nSample, selection.nStart, selection.nEnd, static_cast<float>(pitch), static_cast<float>(ratio / 100.0), grainSize, IsChecked(IDC_CHECK3) != 0);
		timeStretch.DoModal();
		errorCode = timeStretch.m_result;
		if(selection.selectionActive)
			SetSelectionPoints(selection.nStart, timeStretch.NewSelectionEnd());

	}

	if(errorCode == TimeStretchPitchShift::Result::OK)
	{
		// Update sample view
		SetModified(SampleHint().Info().Data(), true, true);
		SwitchToViewIfMouse();
		return;
	}

	// Error management
	if(errorCode != TimeStretchPitchShift::Result::Abort)
	{
		mpt::ustring str;
		switch(errorCode)
		{
		case TimeStretchPitchShift::Result::OK:
		case TimeStretchPitchShift::Result::Abort:
			break;
		case TimeStretchPitchShift::Result::StretchTooShort:
			str = UL_("Stretch ratio is too low, not leaving any sample data.");
			break;
		case TimeStretchPitchShift::Result::StretchTooLong:
			str = UL_("Stretch ratio is too high, exceeding maximum sample length.");
			break;
		case TimeStretchPitchShift::Result::InvalidGrainSize:
			str = UL_("Grain size too small.");
			break;
		case TimeStretchPitchShift::Result::SampleTooShort:
			str = UL_("Sample too short.");
			break;
		case TimeStretchPitchShift::Result::OutOfMemory:
			str = UL_("Out of memory.");
			break;
		}
		Reporting::Error(str);
		SwitchToViewIfMouse();
	}
}


void CCtrlSamples::OnReverse()
{
	ModSample &sample = m_sndFile.GetSample(m_nSample);

	SampleSelectionPoints selection = GetSelectionPoints();

	PrepareUndo("Reverse", sundo_reverse, selection.nStart, selection.nEnd);
	if(SampleEdit::ReverseSample(sample, selection.nStart, selection.nEnd, m_sndFile))
	{
		SetModified(SampleHint().Data(), false, true);
	} else
	{
		m_modDoc.GetSampleUndo().RemoveLastUndoStep(m_nSample);
	}
	mfcless::EndWaitCursor();
	SwitchToViewIfMouse();
}


void CCtrlSamples::OnInvert()
{
	ModSample &sample = m_sndFile.GetSample(m_nSample);

	SampleSelectionPoints selection = GetSelectionPoints();

	PrepareUndo("Invert", sundo_invert, selection.nStart, selection.nEnd);
	if(SampleEdit::InvertSample(sample, selection.nStart, selection.nEnd, m_sndFile) == true)
	{
		SetModified(SampleHint().Data(), false, true);
	} else
	{
		m_modDoc.GetSampleUndo().RemoveLastUndoStep(m_nSample);
	}
	mfcless::EndWaitCursor();
	SwitchToViewIfMouse();
}


void CCtrlSamples::OnSignUnSign()
{
	if(!m_sndFile.GetSample(m_nSample).HasSampleData()) return;

	mfcless::BeginWaitCursor();
	ModSample &sample = m_sndFile.GetSample(m_nSample);
	SampleSelectionPoints selection = GetSelectionPoints();

	PrepareUndo("Unsign", sundo_unsign, selection.nStart, selection.nEnd);
	if(SampleEdit::UnsignSample(sample, selection.nStart, selection.nEnd, m_sndFile) == true)
	{
		SetModified(SampleHint().Data(), false, true);
	} else
	{
		m_modDoc.GetSampleUndo().RemoveLastUndoStep(m_nSample);
	}
	mfcless::EndWaitCursor();
	SwitchToViewIfMouse();
}


void CCtrlSamples::OnSilence()
{
	if(!m_sndFile.GetSample(m_nSample).HasSampleData()) return;
	mfcless::BeginWaitCursor();
	SampleSelectionPoints selection = GetSelectionPoints();

	// never apply silence to a sample that has no selection
	const SmpLength len = selection.nEnd - selection.nStart;
	if(selection.selectionActive && len > 1)
	{
		ModSample &sample = m_sndFile.GetSample(m_nSample);
		PrepareUndo("Silence", sundo_update, selection.nStart, selection.nEnd);
		if(SampleEdit::SilenceSample(sample, selection.nStart, selection.nEnd, m_sndFile))
		{
			SetModified(SampleHint().Data(), false, true);
		}
	}

	mfcless::EndWaitCursor();
	SwitchToViewIfMouse();
}


void CCtrlSamples::OnPrevInstrument()
{
	if (m_nSample > 1)
		SetCurrentSample(m_nSample - 1);
	else
		SetCurrentSample(m_sndFile.GetNumSamples());
}


void CCtrlSamples::OnNextInstrument()
{
	if (m_nSample < m_sndFile.GetNumSamples())
		SetCurrentSample(m_nSample + 1);
	else
		SetCurrentSample(1);
}


void CCtrlSamples::OnNameChanged()
{
	if(IsLocked() || !m_nSample) return;
	mpt::ustring tmp;
	mfcless::GetWindowText(m_EditName, tmp);
	const std::string s = mpt::ToCharset(m_sndFile.GetCharsetInternal(), tmp);
	if(s != m_sndFile.m_szNames[m_nSample])
	{
		if(!m_startedEdit)
		{
			PrepareUndo("Set Name");
			m_editInstrumentName = m_modDoc.GetParentInstrumentWithSameName(m_nSample);
			if(m_editInstrumentName != INSTRUMENTINDEX_INVALID)
				m_modDoc.GetInstrumentUndo().PrepareUndo(m_editInstrumentName, "Set Name");
		}
		if(m_editInstrumentName <= m_sndFile.GetNumInstruments() && m_editInstrumentName < MAX_INSTRUMENTS)
		{
			if(auto instr = m_sndFile.Instruments[m_editInstrumentName]; instr != nullptr)
			{
				instr->name = s;
				m_modDoc.UpdateAllViews(nullptr, InstrumentHint(m_editInstrumentName).Names(), this);
			}
		}

		m_sndFile.m_szNames[m_nSample] = s;
		SetModified(SampleHint().Names(), false, false);
	}
}


void CCtrlSamples::OnFileNameChanged()
{
	if(IsLocked()) return;
	mpt::ustring tmp;
	mfcless::GetWindowText(m_EditFileName, tmp);
	const std::string s = mpt::ToCharset(m_sndFile.GetCharsetInternal(), tmp);
	if(s != m_sndFile.GetSample(m_nSample).filename)
	{
		if(!m_startedEdit) PrepareUndo("Set Filename");
		m_sndFile.GetSample(m_nSample).filename = s;
		SetModified(SampleHint().Info(), false, false);
	}
}


void CCtrlSamples::OnVolumeChanged()
{
	if (IsLocked()) return;
	int nVol = GetItemInt(IDC_EDIT7);
	Limit(nVol, 0, 64);
	nVol *= 4;
	ModSample &sample = m_sndFile.GetSample(m_nSample);
	if (nVol != sample.nVolume)
	{
		if(!m_startedEdit) PrepareUndo("Set Default Volume");
		sample.nVolume = static_cast<uint16>(nVol);
		sample.uFlags.reset(SMP_NODEFAULTVOLUME);
		SetModified(SampleHint().Info(), false, false);
	}
}


void CCtrlSamples::OnGlobalVolChanged()
{
	if (IsLocked()) return;
	int nVol = GetItemInt(IDC_EDIT8);
	Limit(nVol, 0, 64);
	ModSample &sample = m_sndFile.GetSample(m_nSample);
	if (nVol != sample.nGlobalVol)
	{
		if(!m_startedEdit) PrepareUndo("Set Global Volume");
		sample.nGlobalVol = static_cast<uint16>(nVol);
		// Live-adjust volume
		for(auto &chn : m_sndFile.m_PlayState.Chn)
		{
			if(chn.pModSample == &sample)
			{
				chn.UpdateInstrumentVolume(chn.pModSample, chn.pModInstrument);
			}
		}
		SetModified(SampleHint().Info(), false, false);
	}
}


void CCtrlSamples::OnSetPanningChanged()
{
	if (IsLocked()) return;
	bool b = false;
	if (m_sndFile.GetType() & (MOD_TYPE_IT|MOD_TYPE_MPT))
	{
		b = IsChecked(IDC_CHECK1) != false;
	}

	ModSample &sample = m_sndFile.GetSample(m_nSample);
	if(b != sample.uFlags[CHN_PANNING])
	{
		PrepareUndo("Toggle Panning");
		sample.uFlags.set(CHN_PANNING, b);
		SetModified(SampleHint().Info(), false, false);
	}
}


void CCtrlSamples::OnPanningChanged()
{
	if (IsLocked()) return;
	int nPan = GetItemInt(IDC_EDIT9);
	if (nPan < 0) nPan = 0;

	if (m_sndFile.GetType() == MOD_TYPE_XM)
	{
		if (nPan > 255) nPan = 255;	// displayed panning will be 0-255 with XM
	} else
	{
		if (nPan > 64) nPan = 64;	// displayed panning will be 0-64 with anything but XM.
		nPan = nPan * 4;			// so we x4 to get MPT's internal 0-256 range.
	}

	ModSample &sample = m_sndFile.GetSample(m_nSample);
	if (nPan != sample.nPan)
	{
		if(!m_startedEdit) PrepareUndo("Set Panning");
		sample.nPan = static_cast<uint16>(nPan);
		SetModified(SampleHint().Info(), false, false);
	}
}


void CCtrlSamples::OnFineTuneChanged()
{
	if (IsLocked()) return;
	int n = GetItemInt(IDC_EDIT5);
	if(!m_startedEdit)
		PrepareUndo("Finetune");
	ModSample &sample = m_sndFile.GetSample(m_nSample);
	if (!m_sndFile.UseFinetuneAndTranspose())
	{
		if ((n > 0) && (n <= (m_sndFile.GetType() == MOD_TYPE_S3M ? 65535 : 9999999)) && (n != (int)m_sndFile.GetSample(m_nSample).nC5Speed))
		{
			sample.nC5Speed = n;
			int transp = GetSampleTransponse(sample);
			int basenote = (NOTE_MIDDLEC - NOTE_MIN) + transp;
			Clamp(basenote, BASENOTE_MIN, BASENOTE_MAX);
			basenote -= BASENOTE_MIN;
			if (basenote != m_CbnBaseNote->currentIndex())
			{
				LockControls();
				m_CbnBaseNote->setCurrentIndex(basenote);
				UnlockControls();
			}
			SetModified(SampleHint().Info(), false, false);
		}
	} else
	{
		if(m_sndFile.GetType() & MOD_TYPE_MOD)
			n = MOD2XMFineTune(n);
		if((n >= -128) && (n <= 127))
		{
			sample.nFineTune = static_cast<int8>(n);
			SetModified(SampleHint().Info(), false, false);
		}
	}
}


void CCtrlSamples::OnFineTuneChangedDone()
{
	// Update all playing channels
	ModSample &sample = m_sndFile.GetSample(m_nSample);
	for(auto &chn : m_sndFile.m_PlayState.Chn)
	{
		if(chn.pModSample == &sample)
		{
			chn.nTranspose = sample.RelativeTone;
			chn.nFineTune = sample.nFineTune;
			if(chn.nC5Speed != 0 && sample.nC5Speed != 0)
			{
				if(m_sndFile.PeriodsAreFrequencies())
					chn.nPeriod = Util::muldivr(chn.nPeriod, sample.nC5Speed, chn.nC5Speed);
				else if(!m_sndFile.m_SongFlags[SONG_LINEARSLIDES])
					chn.nPeriod = Util::muldivr(chn.nPeriod, chn.nC5Speed, sample.nC5Speed);
			}
			chn.nC5Speed = sample.nC5Speed;
		}
	}
}


void CCtrlSamples::OnBaseNoteChanged()
{
	if (IsLocked()) return;
	int n = static_cast<int>(mfcless::GetItemData(m_CbnBaseNote, m_CbnBaseNote->currentIndex()));

	ModSample &sample = m_sndFile.GetSample(m_nSample);
	PrepareUndo("Transpose");

	if(!m_sndFile.UseFinetuneAndTranspose())
	{
		const int oldTransp = GetSampleTransponse(sample);
		const uint32 newFreq = mpt::saturate_round<uint32>(sample.nC5Speed * std::pow(2.0, (n - oldTransp) / 12.0));
		if(newFreq > 0 && newFreq <= (m_sndFile.GetType() == MOD_TYPE_S3M ? 65535u : 9999999u) && newFreq != sample.nC5Speed)
		{
			sample.nC5Speed = newFreq;
			LockControls();
			SetItemInt(IDC_EDIT5, newFreq);

			// Due to rounding imprecisions if the base note is below 0, we recalculate it here to make sure that the value stays consistent.
			int basenote = (NOTE_MIDDLEC - NOTE_MIN) + GetSampleTransponse(sample);
			Limit(basenote, BASENOTE_MIN, BASENOTE_MAX);
			basenote -= BASENOTE_MIN;
			if(basenote != m_CbnBaseNote->currentIndex())
				m_CbnBaseNote->setCurrentIndex(basenote);

			OnFineTuneChangedDone();
			UnlockControls();
			SetModified(SampleHint().Info(), false, false);
		}
	} else
	{
		if((n >= -128) && (n < 128))
		{
			sample.RelativeTone = (int8)n;
			OnFineTuneChangedDone();
			SetModified(SampleHint().Info(), false, false);
		}
	}
}


void CCtrlSamples::OnVibTypeChanged()
{
	if (IsLocked()) return;
	int n = m_ComboAutoVib->currentIndex();
	if (n >= 0)
	{
		PrepareUndo("Set Vibrato Type");
		m_sndFile.GetSample(m_nSample).nVibType = static_cast<VibratoType>(mfcless::GetItemData(m_ComboAutoVib, n));

		PropagateAutoVibratoChanges();
		SetModified(SampleHint().Info(), false, false);
	}
}


void CCtrlSamples::OnVibDepthChanged()
{
	if (IsLocked()) return;
	int lmin = 0, lmax = 0;
	m_SpinVibDepth->GetRange(lmin, lmax);
	int n = GetItemInt(IDC_EDIT15);
	if ((n >= lmin) && (n <= lmax))
	{
		if(!m_startedEdit) PrepareUndo("Set Vibrato Depth");
		m_sndFile.GetSample(m_nSample).nVibDepth = static_cast<uint8>(n);

		PropagateAutoVibratoChanges();
		SetModified(SampleHint().Info(), false, false);
	}
}


void CCtrlSamples::OnVibSweepChanged()
{
	if (IsLocked()) return;
	int lmin = 0, lmax = 0;
	m_SpinVibSweep->GetRange(lmin, lmax);
	int n = GetItemInt(IDC_EDIT14);
	if ((n >= lmin) && (n <= lmax))
	{
		if(!m_startedEdit) PrepareUndo("Set Vibrato Sweep");
		m_sndFile.GetSample(m_nSample).nVibSweep = static_cast<uint8>(n);

		PropagateAutoVibratoChanges();
		SetModified(SampleHint().Info(), false, false);
	}
}


void CCtrlSamples::OnVibRateChanged()
{
	if (IsLocked()) return;
	int lmin = 0, lmax = 0;
	m_SpinVibRate->GetRange(lmin, lmax);
	int n = GetItemInt(IDC_EDIT16);
	if ((n >= lmin) && (n <= lmax))
	{
		if(!m_startedEdit) PrepareUndo("Set Vibrato Rate");
		m_sndFile.GetSample(m_nSample).nVibRate = static_cast<uint8>(n);

		PropagateAutoVibratoChanges();
		SetModified(SampleHint().Info(), false, false);
	}
}


void CCtrlSamples::OnLoopTypeChanged()
{
	if(IsLocked()) return;
	const int n = m_ComboLoopType->currentIndex();
	ModSample &sample = m_sndFile.GetSample(m_nSample);
	bool wasDisabled = !sample.uFlags[CHN_LOOP];

	PrepareUndo("Set Loop Type");

	// Loop type index: 0: Off, 1: On, 2: PingPong
	sample.uFlags.set(CHN_LOOP, n > 0);
	sample.uFlags.set(CHN_PINGPONGLOOP, n == 2);

	// set loop points if theren't any
	if(wasDisabled && sample.uFlags[CHN_LOOP] && sample.nLoopStart == sample.nLoopEnd)
	{
		SampleSelectionPoints selection = GetSelectionPoints();
		if(selection.selectionActive)
		{
			sample.SetLoop(selection.nStart, selection.nEnd, true, n == 2, m_sndFile);
		} else
		{
			sample.SetLoop(0, sample.nLength, true, n == 2, m_sndFile);
		}
		m_modDoc.UpdateAllViews(nullptr, SampleHint(m_nSample).Info());
	} else
	{
		sample.PrecomputeLoops(m_sndFile, true);
	}
	SetModified(SampleHint().Info(), false, false);
}


void CCtrlSamples::OnLoopPointsChanged()
{
	if(IsLocked()) return;
	ModSample &sample = m_sndFile.GetSample(m_nSample);
	SmpLength start = GetItemInt(IDC_EDIT1), end = GetItemInt(IDC_EDIT2);
	if(start < end || !sample.uFlags[CHN_LOOP])
	{
		if(!m_startedEdit) PrepareUndo("Set Loop");
		const int n = m_ComboLoopType->currentIndex();
		sample.SetLoop(start, end, n > 0, n == 2, m_sndFile);
		SetModified(SampleHint().Info(), false, false);
	}
}


void CCtrlSamples::OnSustainTypeChanged()
{
	if(IsLocked()) return;
	const int n = m_ComboSustainType->currentIndex();
	ModSample &sample = m_sndFile.GetSample(m_nSample);
	bool wasDisabled = !sample.uFlags[CHN_SUSTAINLOOP];

	PrepareUndo("Set Sustain Loop Type");

	// Loop type index: 0: Off, 1: On, 2: PingPong
	sample.uFlags.set(CHN_SUSTAINLOOP, n > 0);
	sample.uFlags.set(CHN_PINGPONGSUSTAIN, n == 2);

	// set sustain loop points if theren't any
	if(wasDisabled && sample.uFlags[CHN_SUSTAINLOOP] && sample.nSustainStart == sample.nSustainEnd)
	{
		SampleSelectionPoints selection = GetSelectionPoints();
		if(selection.selectionActive)
		{
			sample.SetSustainLoop(selection.nStart, selection.nEnd, true, n == 2, m_sndFile);
		} else
		{
			sample.SetSustainLoop(0, sample.nLength, true, n == 2, m_sndFile);
		}
		m_modDoc.UpdateAllViews(nullptr, SampleHint(m_nSample).Info());
	} else
	{
		sample.PrecomputeLoops(m_sndFile, true);
	}
	SetModified(SampleHint().Info(), false, false);
}


void CCtrlSamples::OnSustainPointsChanged()
{
	if(IsLocked()) return;
	ModSample &sample = m_sndFile.GetSample(m_nSample);
	SmpLength start = GetItemInt(IDC_EDIT3), end = GetItemInt(IDC_EDIT4);
	if(start < end || !sample.uFlags[CHN_SUSTAINLOOP])
	{
		if(!m_startedEdit) PrepareUndo("Set Sustain Loop");
		const int n = m_ComboSustainType->currentIndex();
		sample.SetSustainLoop(start, end, n > 0, n == 2, m_sndFile);
		SetModified(SampleHint().Info(), false, false);
	}
}


void CCtrlSamples::OnVScroll(mfcless::ScrollCode nCode, int nPos, QWidget *scrollBar)
{
	mpt::ustring s;
	if(IsLocked()) return;
	ModSample &sample = m_sndFile.GetSample(m_nSample);
	const bool moveLoop = CMainFrame::GetInputHandler()->CtrlPressed();
	const bool fineLoop = CMainFrame::GetInputHandler()->ShiftPressed();
	bool redraw = false;
	static CScrollBar *lastScrollbar = nullptr;

	LockControls();
	// Loop Start
	if(int pos = m_SpinLoopStart->GetPos(); pos != 0 && sample.nLoopEnd > 0 && sample.HasSampleData())
	{
		if(SmpLength i = SampleEdit::FindLoopStart(sample, false, pos > 0, moveLoop, fineLoop); i < sample.nLength)
		{
			if(!m_startedEdit && lastScrollbar != scrollBar)
				PrepareUndo(moveLoop ? "Move Loop" : "Set Loop Start");
			if(moveLoop)
			{
				sample.nLoopEnd = i + (sample.nLoopEnd - sample.nLoopStart);
				s = mfcless::PrintF(UL_("%u"), sample.nLoopEnd);
				mfcless::SetWindowText(m_EditLoopEnd, s);
			}
			sample.nLoopStart = i;
			s = mfcless::PrintF(UL_("%u"), sample.nLoopStart);
			mfcless::SetWindowText(m_EditLoopStart, s);
			redraw = true;
			sample.PrecomputeLoops(m_sndFile);
		}
		m_SpinLoopStart->SetPos(0);
	}
	// Loop End
	if(int pos = m_SpinLoopEnd->GetPos(); pos != 0 && sample.HasSampleData())
	{
		if(SmpLength i = SampleEdit::FindLoopEnd(sample, false, pos > 0, moveLoop, fineLoop); i > 0)
		{
			if(!m_startedEdit && lastScrollbar != scrollBar)
				PrepareUndo(moveLoop ? "Move Loop" : "Set Loop End");
			if(moveLoop)
			{
				sample.nLoopStart = i - (sample.nLoopEnd - sample.nLoopStart);
				s = mfcless::PrintF(UL_("%u"), sample.nLoopStart);
				mfcless::SetWindowText(m_EditLoopStart, s);
			}
			sample.nLoopEnd = i;
			s = mfcless::PrintF(UL_("%u"), sample.nLoopEnd);
			mfcless::SetWindowText(m_EditLoopEnd, s);
			redraw = true;
			sample.PrecomputeLoops(m_sndFile);
		}
		m_SpinLoopEnd->SetPos(0);
	}
	// Sustain Loop Start
	if(int pos = m_SpinSustainStart->GetPos(); pos != 0 && sample.nSustainEnd > 0 && sample.HasSampleData())
	{
		if(SmpLength i = SampleEdit::FindLoopStart(sample, true, pos > 0, moveLoop, fineLoop); i < sample.nLength)
		{
			if(!m_startedEdit && lastScrollbar != scrollBar)
				PrepareUndo(moveLoop ? "Move Sustain Loop" : "Set Sustain Loop Start");
			if(moveLoop)
			{
				sample.nSustainEnd = i + (sample.nSustainEnd - sample.nSustainStart);
				s = mfcless::PrintF(UL_("%u"), sample.nSustainEnd);
				mfcless::SetWindowText(m_EditSustainEnd, s);
			}
			sample.nSustainStart = i;
			s = mfcless::PrintF(UL_("%u"), sample.nSustainStart);
			mfcless::SetWindowText(m_EditSustainStart, s);
			redraw = true;
			sample.PrecomputeLoops(m_sndFile);
		}
		m_SpinSustainStart->SetPos(0);
	}
	// Sustain Loop End
	if(int pos = m_SpinSustainEnd->GetPos(); pos != 0 && sample.HasSampleData())
	{
		if(SmpLength i = SampleEdit::FindLoopEnd(sample, true, pos > 0, moveLoop, fineLoop); i > 0)
		{
			if(!m_startedEdit && lastScrollbar != scrollBar)
				PrepareUndo(moveLoop ? "Move Sustain Loop" : "Set Sustain Loop End");
			if(moveLoop)
			{
				sample.nSustainStart = i - (sample.nSustainEnd - sample.nSustainStart);
				s = mfcless::PrintF(UL_("%u"), sample.nSustainStart);
				mfcless::SetWindowText(m_EditSustainStart, s);
			}
			sample.nSustainEnd = i;
			s = mfcless::PrintF(UL_("%u"), sample.nSustainEnd);
			mfcless::SetWindowText(m_EditSustainEnd, s);
			redraw = true;
			sample.PrecomputeLoops(m_sndFile);
		}
		m_SpinSustainEnd->SetPos(0);
	}
	// FineTune / C-5 Speed
	if(int pos = m_SpinFineTune->GetPos(); pos != 0)
	{
		if(!m_startedEdit && lastScrollbar != scrollBar)
			PrepareUndo("Finetune");
		SetFinetune(pos);
		m_SpinFineTune->SetPos(0);
	}
	if(nCode == mfcless::ScrollCode::EndScroll)
		SwitchToViewIfMouse();
	if(redraw)
	{
		SetModified(SampleHint().Info().Data(), false, false);
	}
	lastScrollbar = scrollBar;
	UnlockControls();
}


void CCtrlSamples::SetFinetune(int step)
{
	ModSample &sample = m_sndFile.GetSample(m_nSample);
	if(!m_sndFile.UseFinetuneAndTranspose())
	{
		if(sample.nC5Speed < 1)
			sample.nC5Speed = 8363;
		auto oldFreq = sample.nC5Speed;
		sample.Transpose((step * TrackerSettings::Instance().m_nFinetuneStep) / 1200.0);
		if(sample.nC5Speed == oldFreq)
			sample.nC5Speed += step;
		Limit(sample.nC5Speed, 1u, 9999999u);  // 9999999 is max. in Impulse Tracker
		int basenote = (NOTE_MIDDLEC - NOTE_MIN) + GetSampleTransponse(sample);
		Clamp(basenote, BASENOTE_MIN, BASENOTE_MAX);
		basenote -= BASENOTE_MIN;
		if(basenote != m_CbnBaseNote->currentIndex())
			m_CbnBaseNote->setCurrentIndex(basenote);
		SetItemInt(IDC_EDIT5, sample.nC5Speed);
	} else
	{
		int ftune = (int)sample.nFineTune;
		// MOD finetune range -8 to 7 translates to -128 to 112
		if(m_sndFile.GetType() & MOD_TYPE_MOD)
		{
			ftune = Clamp((ftune >> 4) + step, -8, 7);
			sample.nFineTune = MOD2XMFineTune((signed char)ftune);
		} else
		{
			ftune = Clamp(ftune + step, -128, 127);
			sample.nFineTune = (signed char)ftune;
		}
		SetItemInt(IDC_EDIT5, ftune);
	}
	OnFineTuneChangedDone();
	SetModified(SampleHint().Info().Data(), false, false);
}


bool CCtrlSamples::PreTranslateMessage(mfcless::Message *pMsg)
{
	if (pMsg)
	{
		//We handle keypresses before Windows has a chance to handle them (for alt etc..)
		if ((pMsg->message == mfcless::msg::SysKeyUp)   || (pMsg->message == mfcless::msg::KeyUp) ||
			(pMsg->message == mfcless::msg::SysKeyDown) || (pMsg->message == mfcless::msg::KeyDown))
		{
			CInputHandler* ih = CMainFrame::GetInputHandler();
			if (ih->KeyEvent(kCtxViewSamples, ih->Translate(*pMsg)) != kcNull)
				return true; // Mapped to a command, no need to pass message on.
		}

	}
	return CModControlDlg::PreTranslateMessage(pMsg);
}


intptr_t CCtrlSamples::OnCustomKeyMsg(uintptr_t wParam, intptr_t /*lParam*/)
{
	int transpose = 0;
	switch(wParam)
	{
	case kcSampleLoad:		OnSampleOpen(); return wParam;
	case kcSampleLoadRaw:	OnSampleOpenRaw(); return wParam;
	case kcSampleSave:		OnSampleSaveOne(); return wParam;
	case kcSampleNew:		InsertSample(false); return wParam;
	case kcSampleDuplicate:	InsertSample(true); return wParam;

	case kcSampleToggleNormalLoop:
		if(m_ComboLoopType->isEnabled())
		{
			m_ComboLoopType->setCurrentIndex((m_ComboLoopType->currentIndex() + 1) % m_ComboLoopType->count());
			OnLoopTypeChanged();
		}
		return wParam;
	case kcSampleToggleSustainLoop:
		if(m_ComboSustainType->isEnabled())
		{
			m_ComboSustainType->setCurrentIndex((m_ComboSustainType->currentIndex() + 1) % m_ComboSustainType->count());
			OnSustainTypeChanged();
		}
		return wParam;

	case kcSampleTransposeUp: transpose = 1; break;
	case kcSampleTransposeDown: transpose = -1; break;
	case kcSampleTransposeOctUp: transpose = 12; break;
	case kcSampleTransposeOctDown: transpose = -12; break;
	case kcSampleFinetuneUp: SetFinetune(1); break;
	case kcSampleFinetuneDown: SetFinetune(-1); break;

	case kcSampleUpsample:
	case kcSampleDownsample:
		{
			uint32 oldRate = m_sndFile.GetSample(m_nSample).GetSampleRate(m_sndFile.GetType());
			ApplyResample(m_nSample, wParam == kcSampleUpsample ? oldRate * 2 : oldRate / 2, TrackerSettings::Instance().sampleEditorDefaultResampler);
		}
		return wParam;
	case kcSampleResample:
		OnResample();
		return wParam;
	case kcSampleStereoSep:
		OnStereoSeparation();
		return wParam;
	case kcSampleInitializeOPL:
		OnInitOPLInstrument();
		return wParam;
	}

	if(transpose)
	{
		if(m_CbnBaseNote->isEnabled())
		{
			int sel = Clamp(m_CbnBaseNote->currentIndex() + transpose, 0, m_CbnBaseNote->count() - 1);
			if(sel != m_CbnBaseNote->currentIndex())
			{
				m_CbnBaseNote->setCurrentIndex(sel);
				OnBaseNoteChanged();
			}
		}
		return wParam;
	}

	return kcNull;
}


// Return currently selected part of the sample.
// The whole sample size will be returned if no part of the sample is selected.
// However, points.selectionActive indicates whether a sample selection exists or not.
CCtrlSamples::SampleSelectionPoints CCtrlSamples::GetSelectionPoints()
{
	SampleSelectionPoints points;
	SampleViewState viewstate{};
	const ModSample &sample = m_sndFile.GetSample(m_nSample);

	SendViewMessage(VIEWMSG_SAVESTATE, reinterpret_cast<intptr_t>(&viewstate));
	points.nStart = viewstate.dwBeginSel;
	points.nEnd = viewstate.dwEndSel;
	if(points.nEnd > sample.nLength) points.nEnd = sample.nLength;
	if(points.nStart > points.nEnd) points.nStart = points.nEnd;
	points.selectionActive = true;
	if(points.nStart >= points.nEnd)
	{
		points.nStart = 0;
		points.nEnd = sample.nLength;
		points.selectionActive = false;
	}
	return points;
}

// Set the currently selected part of the sample.
// To reset the selection, use nStart = nEnd = 0.
void CCtrlSamples::SetSelectionPoints(SmpLength nStart, SmpLength nEnd)
{
	const ModSample &sample = m_sndFile.GetSample(m_nSample);

	Limit(nStart, SmpLength(0), sample.nLength);
	Limit(nEnd, SmpLength(0), sample.nLength);

	SampleViewState viewstate{};
	SendViewMessage(VIEWMSG_SAVESTATE, reinterpret_cast<intptr_t>(&viewstate));

	viewstate.dwBeginSel = nStart;
	viewstate.dwEndSel = nEnd;
	SendViewMessage(VIEWMSG_LOADSTATE, reinterpret_cast<intptr_t>(&viewstate));
}


// Crossfade loop to create smooth loop transitions
void CCtrlSamples::OnXFade()
{
	ModSample &sample = m_sndFile.GetSample(m_nSample);

	if(!sample.HasSampleData())
	{
		mfcless::MessageBeep();
		SwitchToViewIfMouse();
		return;
	}
	bool resetLoopOnCancel = false;
	if((sample.nLoopEnd <= sample.nLoopStart || sample.nLoopEnd > sample.nLength)
		&& (sample.nSustainEnd <= sample.nSustainStart || sample.nSustainEnd > sample.nLength))
	{
		const auto selection = GetSelectionPoints();
		if(selection.nStart > 0 && selection.nEnd > selection.nStart)
		{
			sample.SetLoop(selection.nStart, selection.nEnd, true, false, m_sndFile);
			resetLoopOnCancel = true;
		} else
		{
			Reporting::Error("Crossfade requires a sample loop to work.", this);
			SwitchToViewIfMouse();
			return;
		}
	}
	if(sample.nLoopStart == 0 && sample.nSustainStart == 0)
	{
		Reporting::Error("Crossfade requires the sample to have data before the loop start.", this);
		SwitchToViewIfMouse();
		return;
	}

	CSampleXFadeDlg dlg(this, sample);
	if(dlg.DoModal() == IDOK)
	{
		const auto [loopStart, loopEnd] = dlg.m_useSustainLoop ? sample.GetSustainLoop() : sample.GetLoop();
		const SmpLength maxSamples = std::min({ sample.nLength, loopStart, loopEnd / 2 });
		SmpLength fadeSamples = dlg.PercentToSamples(dlg.m_fadeLength);
		LimitMax(fadeSamples, maxSamples);
		if(fadeSamples < 2) return;

		PrepareUndo("Crossfade", sundo_update,
			loopEnd - fadeSamples,
			loopEnd + (dlg.m_afterloopFade ? std::min(sample.nLength - loopEnd, fadeSamples) : 0));

		if(SampleEdit::XFadeSample(sample, fadeSamples, dlg.m_fadeLaw, dlg.m_afterloopFade, dlg.m_useSustainLoop, m_sndFile))
		{
			SetModified(SampleHint().Info().Data(), true, true);
		} else
		{
			m_modDoc.GetSampleUndo().RemoveLastUndoStep(m_nSample);
		}
	} else if(resetLoopOnCancel)
	{
		sample.SetLoop(0, 0, false, false, m_sndFile);
	}
	SwitchToViewIfMouse();
}


void CCtrlSamples::OnStereoSeparation()
{
	ModSample &sample = m_sndFile.GetSample(m_nSample);

	if(!sample.HasSampleData()
		|| sample.GetNumChannels() != 2
		|| sample.uFlags[CHN_ADLIB])
	{
		mfcless::MessageBeep();
		SwitchToViewIfMouse();
		return;
	}

	static double separation = 100.0;
	CInputDlg dlg(this, UL_("Stereo separation amount\n0% = mono, 100% = no change, 200% = double separation\nNegative values swap channels"), -200.0, 200.0, separation);
	if(dlg.DoModal() == IDOK)
	{
		separation = dlg.resultAsDouble;

		SampleSelectionPoints selection = GetSelectionPoints();
		PrepareUndo("Stereo Separation", sundo_update,
			selection.nStart, selection.nEnd);

		if(SampleEdit::StereoSepSample(sample, selection.nStart, selection.nEnd, separation, m_sndFile))
		{
			SetModified(SampleHint().Info().Data(), true, true);
		} else
		{
			m_modDoc.GetSampleUndo().RemoveLastUndoStep(m_nSample);
		}
	}
	SwitchToViewIfMouse();
}


void CCtrlSamples::OnAutotune()
{
	SampleSelectionPoints selection = GetSelectionPoints();
	if(!selection.selectionActive)
	{
		selection.nStart = selection.nEnd = 0;
	}

	ModSample &sample = m_sndFile.GetSample(m_nSample);
	Autotune at(sample, m_sndFile.GetType(), selection.nStart, selection.nEnd);
	if(at.CanApply())
	{
		CAutotuneDlg dlg(this);
		if(dlg.DoModal() == IDOK)
		{
			mfcless::BeginWaitCursor();
			PrepareUndo("Automatic Sample Tuning");
			bool modified = true;
			if(IsOPLInstrument())
			{
				const uint32 newFreq = mpt::saturate_round<uint32>(dlg.GetPitchReference() * (8363.0 / 440.0) * std::pow(2.0, dlg.GetTargetNote() / 12.0));
				modified = (newFreq != sample.nC5Speed);
				sample.nC5Speed = newFreq;
			} else
			{
				modified = at.Apply(static_cast<double>(dlg.GetPitchReference()), dlg.GetTargetNote());
			}
			OnFineTuneChangedDone();
			if(modified)
				SetModified(SampleHint().Info(), true, false);
			mfcless::EndWaitCursor();
		}
	}
	SwitchToViewIfMouse();
}


void CCtrlSamples::OnKeepSampleOnDisk()
{
	SAMPLEINDEX first = m_nSample, last = m_nSample;
	if(CInputHandler::ShiftPressed())
	{
		first = 1;
		last = m_sndFile.GetNumSamples();
	}

	const bool enable = IsChecked(IDC_CHECK2) != 0;
	for(SAMPLEINDEX i = first; i <= last; i++)
	{
		if(bool newState = enable && m_sndFile.SampleHasPath(i); newState != m_sndFile.GetSample(i).uFlags[SMP_KEEPONDISK])
		{
			m_sndFile.GetSample(i).uFlags.set(SMP_KEEPONDISK, newState);
			m_modDoc.UpdateAllViews(nullptr, SampleHint(i).Info().Names(), this);
		}
	}
	m_modDoc.SetModified();
}


// When changing auto vibrato properties, propagate them to other samples of the same instrument in XM edit mode.
void CCtrlSamples::PropagateAutoVibratoChanges()
{
	if(!(m_sndFile.GetType() & MOD_TYPE_XM))
	{
		return;
	}

	for(INSTRUMENTINDEX i = 1; i <= m_sndFile.GetNumInstruments(); i++)
	{
		if(m_sndFile.IsSampleReferencedByInstrument(m_nSample, i))
		{
			const auto referencedSamples = m_sndFile.Instruments[i]->GetSamples();

			// Propagate changes to all samples that belong to this instrument.
			const ModSample &it = m_sndFile.GetSample(m_nSample);
			m_sndFile.PropagateXMAutoVibrato(i, it.nVibType, it.nVibSweep, it.nVibDepth, it.nVibRate);
			for(auto smp : referencedSamples)
			{
				m_modDoc.UpdateAllViews(nullptr, SampleHint(smp).Info(), this);
			}
		}
	}
}


void CCtrlSamples::OnXButtonUp(uint32 nFlags, uint32 nButton, mfcless::Point point)
{
	if(nButton == mfcless::XButton::Back) OnPrevInstrument();
	else if(nButton == mfcless::XButton::Forward) OnNextInstrument();
	CModControlDlg::OnXButtonUp(nFlags, nButton, point);
	SwitchToView();
}


bool CCtrlSamples::IsOPLInstrument() const
{
	return m_nSample >= 1 && m_nSample <= m_sndFile.GetNumSamples() && m_sndFile.GetSample(m_nSample).uFlags[CHN_ADLIB];
}


void CCtrlSamples::OnInitOPLInstrument()
{
	if(m_sndFile.SupportsOPL())
	{
		CriticalSection cs;
		PrepareUndo("Initialize OPL Instrument", sundo_replace);
		m_sndFile.DestroySample(m_nSample);
		m_sndFile.InitOPL();
		ModSample &sample = m_sndFile.GetSample(m_nSample);
		sample.nC5Speed = 8363;
		// Initialize with instant attack, release and enabled sustain for carrier and instant attack for modulator
		sample.SetAdlib(true, { 0x00, 0x20, 0x00, 0x00, 0xF0, 0xF0, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x00 });
		SetModified(SampleHint().Info().Data().Names(), true, true);
		SwitchToViewIfMouse();
	}
}


OPENMPT_NAMESPACE_END
