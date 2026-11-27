/*
 * Ctrl_pat.cpp
 * ------------
 * Purpose: Pattern tab, upper panel.
 * Notes  : mfcless twin of upstream's mptrack/Ctrl_pat.cpp.
 *          Draft from qt/tools/twin.py, not yet edited by hand.
 * Authors: Olivier Lapicque
 *          OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "Ctrl_pat.h"
#include "ChannelManagerDlg.h"
#include "Childfrm.h"
#include "Globals.h"
#include "HighDPISupport.h"
#include "ImageLists.h"
#include "InputHandler.h"
#include "Mainfrm.h"
#include "Moddoc.h"
#include "Mptrack.h"
#include "PatternCursor.h"
#include "PatternEditorDialogs.h"
#include "Reporting.h"
#include "resource.h"
#include "TrackerSettings.h"
#include "View_pat.h"
#include "WindowMessages.h"
#include "../common/mptStringBuffer.h"
#include "../soundlib/mod_specifications.h"


OPENMPT_NAMESPACE_BEGIN


//////////////////////////////////////////////////////////////
// CCtrlPatterns


void CCtrlPatterns::RegisterMessageMap()
{
	CModControlDlg::RegisterMessageMap();
	// ON_WM_KEYDOWN() (mfcless::Window)
	AddScroll(Qt::Vertical, &CCtrlPatterns::OnVScroll);
	// ON_WM_XBUTTONUP() (mfcless::Window)
	AddCommand(IDC_BUTTON1, &CCtrlPatterns::OnSequenceNext);
	AddCommand(IDC_BUTTON2, &CCtrlPatterns::OnSequencePrev);
	AddCommand(ID_PLAYER_PAUSE, &CCtrlPatterns::OnPlayerPause);
	AddCommand(IDC_PATTERN_NEW, &CCtrlPatterns::OnPatternNew);
	AddCommand(IDC_PATTERN_STOP, &CCtrlPatterns::OnPatternStop);
	AddCommand(IDC_PATTERN_PLAY, &CCtrlPatterns::OnPatternPlay);
	AddCommand(IDC_PATTERN_PLAYFROMSTART, &CCtrlPatterns::OnPatternPlayFromStart);
	AddCommand(IDC_PATTERN_RECORD, &CCtrlPatterns::OnPatternRecord);
	AddCommand(IDC_METRONOME, &CCtrlPatterns::OnToggleMetronome);
	AddCommand(ID_METRONOME_SETTINGS, &CCtrlPatterns::OnMetronomeSettings);
	AddCommand(IDC_PATTERN_LOOP, &CCtrlPatterns::OnChangeLoopStatus);
	AddCommand(ID_PATTERN_PLAYROW, &CCtrlPatterns::OnPatternPlayRow);
	AddCommand(ID_PATTERN_CHANNELMANAGER, &CCtrlPatterns::OnChannelManager);
	AddCommand(ID_PATTERN_VUMETERS, &CCtrlPatterns::OnPatternVUMeters);
	AddCommand(ID_VIEWPLUGNAMES, &CCtrlPatterns::OnPatternViewPlugNames);
	AddCommand(ID_NEXTINSTRUMENT, &CCtrlPatterns::OnNextInstrument);
	AddCommand(ID_PREVINSTRUMENT, &CCtrlPatterns::OnPrevInstrument);
	AddCommand(IDC_PATTERN_FOLLOWSONG, &CCtrlPatterns::OnFollowSong);
	AddCommand(ID_PATTERN_CHORDEDIT, &CCtrlPatterns::OnChordEditor);
	AddCommand(ID_PATTERN_PROPERTIES, &CCtrlPatterns::OnPatternProperties);
	AddCommand(ID_PATTERN_EXPAND, &CCtrlPatterns::OnPatternExpand);
	AddCommand(ID_PATTERN_SHRINK, &CCtrlPatterns::OnPatternShrink);
	AddCommand(ID_PATTERN_AMPLIFY, &CCtrlPatterns::OnPatternAmplify);
	AddCommand(ID_ORDERLIST_NEW, &CCtrlPatterns::OnPatternNew);
	AddCommand(ID_ORDERLIST_COPY, &CCtrlPatterns::OnPatternDuplicate);
	AddCommand(ID_ORDERLIST_MERGE, &CCtrlPatterns::OnPatternMerge);
	AddCommand(ID_PATTERNPASTE, &CCtrlPatterns::OnPatternPaste);
	AddCommand(ID_EDIT_UNDO, &CCtrlPatterns::OnEditUndo);
	AddCommand(ID_PATTERNDETAIL_DROPDOWN, &CCtrlPatterns::OnDetailSwitch);
	AddCommand(ID_PATTERNDETAIL_INSTR, &CCtrlPatterns::OnDetailInstr);
	AddCommand(ID_PATTERNDETAIL_VOLUME, &CCtrlPatterns::OnDetailVolume);
	AddCommand(ID_PATTERNDETAIL_EFFECT, &CCtrlPatterns::OnDetailEffect);
	AddCommand(ID_OVERFLOWPASTE, &CCtrlPatterns::OnToggleOverflowPaste);
	AddNotify(IDC_COMBO_INSTRUMENT, mfcless::Notify::DropDown, &CCtrlPatterns::OnOpenInstrumentDropdown);
	// ON_CBN_SELENDCANCEL(IDC_COMBO_INSTRUMENT, &CCtrlPatterns::OnCancelInstrumentDropdown): by hand (Qt event override)
	// ON_CBN_SELENDOK(IDC_COMBO_INSTRUMENT,     &CCtrlPatterns::OnInstrumentChanged): by hand (Qt event override)
	AddCommand(IDC_PATINSTROPLUGGUI, &CCtrlPatterns::TogglePluginEditor);
	AddNotify(IDC_EDIT_SPACING, mfcless::Notify::EditChange, &CCtrlPatterns::OnSpacingChanged);
	AddNotify(IDC_EDIT_PATTERNNAME, mfcless::Notify::EditChange, &CCtrlPatterns::OnPatternNameChanged);
	AddNotify(IDC_EDIT_SEQUENCE_NAME, mfcless::Notify::EditChange, &CCtrlPatterns::OnSequenceNameChanged);
	AddNotify(IDC_EDIT_SEQNUM, mfcless::Notify::EditChange, &CCtrlPatterns::OnSequenceNumChanged);
	// ON_NOTIFY(TBN_DROPDOWN, IDC_TOOLBAR1,     &CCtrlPatterns::OnTbnDropDownToolBar): by hand (Qt event override)
	AddCommandUpdate(IDC_PATTERN_RECORD, &CCtrlPatterns::OnUpdateRecord);
	// ON_WM_MOUSEWHEEL() (mfcless::Window)
}

void CCtrlPatterns::DoDataExchange(DataExchange &dx)
{
	CModControlDlg::DoDataExchange(dx);
	//{{AFX_DATA_MAP(CCtrlPatterns)
	ExchangeControl(dx, IDC_BUTTON1, m_BtnNext);
	ExchangeControl(dx, IDC_BUTTON2, m_BtnPrev);
	ExchangeControl(dx, IDC_COMBO_INSTRUMENT, m_CbnInstrument);
	ExchangeControl(dx, IDC_EDIT_SPACING, m_EditSpacing);
	ExchangeControl(dx, IDC_EDIT_PATTERNNAME, m_EditPatName);
	ExchangeControl(dx, IDC_EDIT_SEQNUM, m_EditSequence);
	ExchangeControl(dx, IDC_SPIN_SPACING, m_SpinSpacing);
	ExchangeControl(dx, IDC_SPIN_INSTRUMENT, m_SpinInstrument);
	ExchangeControl(dx, IDC_SPIN_SEQNUM, m_SpinSequence);
	ExchangeControl(dx, IDC_TOOLBAR1, m_ToolBar);
	//}}AFX_DATA_MAP
}


const ModSequence &CCtrlPatterns::Order() const { return m_sndFile.Order(); }
ModSequence &CCtrlPatterns::Order() { return m_sndFile.Order(); }


CCtrlPatterns::CCtrlPatterns(CModControlView &parent, CModDoc &document)
	: CModControlDlg{parent, document}
	, m_OrderList{*this, document}
{
	m_BtnPrev->SetAccessibleText(U_("Select Previous Order"));
	m_BtnNext->SetAccessibleText(U_("Select Next Order"));
	m_bVUMeters = TrackerSettings::Instance().gbPatternVUMeters;
	m_bPluginNames = TrackerSettings::Instance().gbPatternPluginNames;
	m_bRecord = TrackerSettings::Instance().gbPatternRecord;
}


bool CCtrlPatterns::OnInitDialog()
{
	mfcless::Rect rect, rcOrderList;
	CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
	CModControlDlg::OnInitDialog();

	if(!pMainFrm)
		return true;
	SetRedraw(false);
	LockControls();
	// Order List
	const int cyHScroll = HighDPISupport::GetSystemMetrics(SM_CYHSCROLL, GetDPI());
	m_BtnNext->GetWindowRect(&rect);
	ScreenToClient(&rect);
	auto margins = HighDPISupport::ScalePixels(4, m_hWnd);
	rcOrderList.left = rect.right + margins;
	rcOrderList.top = rect.top;
	rcOrderList.bottom = rect.bottom + cyHScroll;
	GetClientRect(&rect);
	rcOrderList.right = rect.right - margins;
	m_OrderList->Init(rcOrderList);
	// Toolbar buttons
	m_ToolBar->Init(CMainFrame::GetMainFrame()->m_PatternIcons, CMainFrame::GetMainFrame()->m_PatternIconsDisabled);
	m_ToolBar->SetExtendedStyle(m_ToolBar->GetExtendedStyle() | TBSTYLE_EX_DRAWDDARROWS);
	m_ToolBar->AddButton(IDC_PATTERN_NEW, TIMAGE_PATTERN_NEW, TBSTYLE_BUTTON | TBSTYLE_DROPDOWN);
	m_ToolBar->AddButton(IDC_PATTERN_PLAY, TIMAGE_PATTERN_PLAY);
	m_ToolBar->AddButton(IDC_PATTERN_PLAYFROMSTART, TIMAGE_PATTERN_RESTART);
	m_ToolBar->AddButton(IDC_PATTERN_STOP, TIMAGE_PATTERN_STOP);
	m_ToolBar->AddButton(ID_PATTERN_PLAYROW, TIMAGE_PATTERN_PLAYROW);
	m_ToolBar->AddButton(IDC_PATTERN_RECORD, TIMAGE_PATTERN_RECORD, TBSTYLE_CHECK, (m_bRecord ? TBSTATE_CHECKED : 0) | TBSTATE_ENABLED);
	m_ToolBar->AddButton(IDC_METRONOME, TIMAGE_METRONOME, TBSTYLE_CHECK | TBSTYLE_DROPDOWN, (TrackerSettings::Instance().metronomeEnabled ? TBSTATE_CHECKED : 0) | TBSTATE_ENABLED);
	m_ToolBar->AddButton(ID_SEPARATOR, 0, TBSTYLE_SEP);
	m_ToolBar->AddButton(ID_PATTERN_VUMETERS, TIMAGE_PATTERN_VUMETERS, TBSTYLE_CHECK, (m_bVUMeters ? TBSTATE_CHECKED : 0) | TBSTATE_ENABLED);
	m_ToolBar->AddButton(ID_VIEWPLUGNAMES, TIMAGE_PATTERN_PLUGINS, TBSTYLE_CHECK, (m_bPluginNames ? TBSTATE_CHECKED : 0) | TBSTATE_ENABLED);
	m_ToolBar->AddButton(ID_PATTERN_CHANNELMANAGER, TIMAGE_CHANNELMANAGER);
	m_ToolBar->AddButton(ID_SEPARATOR, 0, TBSTYLE_SEP);
	m_ToolBar->AddButton(ID_PATTERN_MIDIMACRO, TIMAGE_MACROEDITOR);
	m_ToolBar->AddButton(ID_PATTERN_CHORDEDIT, TIMAGE_CHORDEDITOR);
	m_ToolBar->AddButton(ID_SEPARATOR, 0, TBSTYLE_SEP);
	m_ToolBar->AddButton(ID_EDIT_UNDO, TIMAGE_UNDO);
	m_ToolBar->AddButton(ID_PATTERN_PROPERTIES, TIMAGE_PATTERN_PROPERTIES);
	m_ToolBar->AddButton(ID_PATTERN_EXPAND, TIMAGE_PATTERN_EXPAND);
	m_ToolBar->AddButton(ID_PATTERN_SHRINK, TIMAGE_PATTERN_SHRINK);
	//	m_ToolBar->AddButton(ID_PATTERN_AMPLIFY, TIMAGE_SAMPLE_AMPLIFY);
	m_ToolBar->AddButton(ID_SEPARATOR, 0, TBSTYLE_SEP);
	m_ToolBar->AddButton(ID_PATTERNDETAIL_DROPDOWN, TIMAGE_PATTERN_DETAIL, TBSTYLE_BUTTON | TBSTYLE_DROPDOWN);
	m_ToolBar->AddButton(ID_SEPARATOR, 0, TBSTYLE_SEP);
	m_ToolBar->AddButton(ID_OVERFLOWPASTE, TIMAGE_PATTERN_OVERFLOWPASTE, TBSTYLE_CHECK, ((TrackerSettings::Instance().patternSetup & PatternSetup::OverflowPaste) ? TBSTATE_CHECKED : 0) | TBSTATE_ENABLED);

	m_EditPatName->setMaxLength(MAX_PATTERNNAME - 1);
	// Spin controls
	m_SpinSpacing->SetRange(0, MAX_SPACING);
	m_SpinSpacing->SetPos(TrackerSettings::Instance().gnPatternSpacing);

	m_SpinInstrument->SetRange(-1, 1);
	m_SpinInstrument->SetPos(0);

	SetDlgItemInt(IDC_EDIT_SPACING, TrackerSettings::Instance().gnPatternSpacing);
	SetChecked(IDC_PATTERN_FOLLOWSONG, (TrackerSettings::Instance().patternSetup & PatternSetup::FollowSongOffByDefault) ? 0 : 1);

	m_SpinSequence->SetRange(1, m_sndFile.Order.GetNumSequences());
	m_SpinSequence->SetPos(m_sndFile.Order.GetCurrentSequenceIndex() + 1);
	SetItemText(IDC_EDIT_SEQUENCE_NAME, mpt::ToUnicode(Order().GetName()));

	m_OrderList->setFocus();

	UpdateView(PatternHint().Names().ModType(), nullptr);
	RecalcLayout();

	m_initialized = true;
	UnlockControls();

	SetRedraw(true);
	return false;
}


void CCtrlPatterns::OnDPIChanged()
{
	m_ToolBar->OnDPIChanged();
	CModControlDlg::OnDPIChanged();
}


Setting<int32> &CCtrlPatterns::GetSplitPosRef() { return TrackerSettings::Instance().glPatternWindowHeight; }


void CCtrlPatterns::RecalcLayout()
{
	// Update Order List Position
	if(m_OrderList->m_hWnd)
	{
		mfcless::Rect rect;
		int cx, cy, cellcx;
		int cyHScroll = HighDPISupport::GetSystemMetrics(SM_CYHSCROLL, GetDPI());
		m_BtnNext->GetWindowRect(&rect);
		ScreenToClient(&rect);
		cx = -(rect.right + 4);
		cy = rect.bottom - rect.top + cyHScroll;
		GetClientRect(&rect);
		cx += rect.right - 8;
		cellcx = m_OrderList->GetFontWidth();
		if(cellcx > 0)
			cx -= (cx % cellcx);
		cx += 2;
		if((cx > 0) && (cy > 0))
		{
			m_OrderList->SetWindowPos(nullptr, 0, 0, cx, cy, SWP_NOMOVE | SWP_NOZORDER | SWP_DRAWFRAME);
		}
	}
}


void CCtrlPatterns::UpdateView(UpdateHint hint, QObject *pObj)
{
	m_OrderList->UpdateView(hint, pObj);
	FlagSet<HintType> hintType = hint.GetType();

	const bool updateAll = hintType[HINT_MODTYPE];
	const bool updateSeq = hint.GetCategory() == HINTCAT_SEQUENCE;
	const bool updatePlug = hint.GetCategory() == HINTCAT_PLUGINS && hintType[HINT_MIXPLUGINS];
	const PatternHint patternHint = hint.ToType<PatternHint>();

	if(updateAll || (updateSeq && hintType[HINT_SEQNAMES]))
	{
		SetItemText(IDC_EDIT_SEQUENCE_NAME, mpt::ToUnicode(Order().GetName()));
	}

	if(updateAll || (updateSeq && hintType[HINT_MODSEQUENCE]))
	{
		m_SpinSequence->SetRange(1, m_sndFile.Order.GetNumSequences());
		m_SpinSequence->SetPos(m_sndFile.Order.GetCurrentSequenceIndex() + 1);

		// Enable/disable multisequence controls according the current modtype.
		const bool isMultiSeqAvail = (m_sndFile.GetModSpecifications().sequencesMax > 1 || m_sndFile.Order.GetNumSequences() > 1) ? true : false;
		EnableItem(IDC_STATIC_SEQUENCE_NAME, isMultiSeqAvail);
		EnableItem(IDC_EDIT_SEQUENCE_NAME, isMultiSeqAvail);
		EnableItem(IDC_EDIT_SEQNUM, isMultiSeqAvail);
		EnableItem(IDC_SPIN_SEQNUM, isMultiSeqAvail);
	}

	if(updateAll || updatePlug || (hint.GetCategory() == HINTCAT_INSTRUMENTS && hintType[HINT_INSTRUMENT]))
	{
		EnableItem(IDC_PATINSTROPLUGGUI, HasValidPlug(m_nInstrument) ? true : false);
	}

	if(updateAll)
	{
		// Enable/disable pattern names
		const bool isPatNameAvail = m_sndFile.GetModSpecifications().hasPatternNames ? true : false;
		EnableItem(IDC_STATIC_PATTERNNAME, isPatNameAvail);
		EnableItem(IDC_EDIT_PATTERNNAME, isPatNameAvail);
	}

	if(hintType[HINT_MPTOPTIONS])
	{
		m_ToolBar->UpdateStyle();
	}

	bool instrPluginsChanged = false;
	if(hint.GetCategory() == HINTCAT_PLUGINS && hintType[HINT_PLUGINNAMES])
	{
		const auto changedPlug = hint.ToType<PluginHint>().GetPlugin();
		for(INSTRUMENTINDEX i = 1; i <= m_sndFile.GetNumInstruments(); i++)
		{
			const auto ins = m_sndFile.Instruments[i];
			if(!ins)
				continue;
			if((!changedPlug && ins->nMixPlug != 0) || (changedPlug && ins->nMixPlug == changedPlug))
			{
				instrPluginsChanged = true;
				break;
			}
		}
	}

	const bool updatePatNames = patternHint.GetType()[HINT_PATNAMES];
	const bool updateSmpNames = hint.GetCategory() == HINTCAT_SAMPLES && hintType[HINT_SMPNAMES];
	const bool updateInsNames = (hint.GetCategory() == HINTCAT_INSTRUMENTS && hintType[HINT_INSNAMES]) || instrPluginsChanged;
	if(updateAll || updatePatNames || updateSmpNames || updateInsNames)
	{
		LockControls();
		mpt::ustring s;
		if(updateAll || updateSmpNames || updateInsNames)
		{
			constexpr mpt::uchar szSplitFormat[] = U_("%02u %s %02u: %s/%s");
			uint32 nPos = 0;
			m_CbnInstrument->setUpdatesEnabled(false);
			m_CbnInstrument->clear();
			mfcless::SetItemData(m_CbnInstrument, mfcless::AddItem(m_CbnInstrument, U_(" No Instrument")), 0);
			const INSTRUMENTINDEX nSplitIns = m_modDoc.GetSplitKeyboardSettings().splitInstrument;
			const ModCommand::NOTE noteSplit = 1 + m_modDoc.GetSplitKeyboardSettings().splitNote;
			const mpt::ustring sSplitInsName = m_modDoc.GetPatternViewInstrumentName(nSplitIns, true, false);
			if(m_sndFile.GetNumInstruments())
			{
				// Show instrument names
				for(INSTRUMENTINDEX i = 1; i <= m_sndFile.GetNumInstruments(); i++)
				{
					if(m_sndFile.Instruments[i] == nullptr)
						continue;

					mpt::ustring sDisplayName;
					if(m_modDoc.GetSplitKeyboardSettings().IsSplitActive())
					{
						s.Format(szSplitFormat,
							nSplitIns,
							mpt::ToUnicode(m_sndFile.GetNoteName(noteSplit, nSplitIns)).GetString(),
							i,
							sSplitInsName.GetString(),
							m_modDoc.GetPatternViewInstrumentName(i, true, false).GetString());
						sDisplayName = s;
					}
					else
						sDisplayName = m_modDoc.GetPatternViewInstrumentName(i);

					uint32 n = mfcless::AddItem(m_CbnInstrument, sDisplayName);
					if(n == m_nInstrument) nPos = n;
					mfcless::SetItemData(m_CbnInstrument, n, i);
				}
			} else
			{
				// Show sample names
				SAMPLEINDEX nmax = m_sndFile.GetNumSamples();
				for(SAMPLEINDEX i = 1; i <= nmax; i++) if (m_sndFile.GetSample(i).HasSampleData() || m_sndFile.GetSample(i).uFlags[CHN_ADLIB])
				{
					if (m_modDoc.GetSplitKeyboardSettings().IsSplitActive())
						s.Format(szSplitFormat,
							nSplitIns,
							mpt::ToUnicode(m_sndFile.GetNoteName(noteSplit, nSplitIns)).GetString(),
							i,
							mpt::ToUnicode(m_sndFile.GetCharsetInternal(), m_sndFile.m_szNames[nSplitIns]).GetString(),
							mpt::ToUnicode(m_sndFile.GetCharsetInternal(), m_sndFile.m_szNames[i]).GetString());
					else
						s.Format(U_("%02u: %s"),
							i,
							mpt::ToUnicode(m_sndFile.GetCharsetInternal(), m_sndFile.m_szNames[i]).GetString());

					uint32 n = mfcless::AddItem(m_CbnInstrument, s);
					if(n == m_nInstrument) nPos = n;
					mfcless::SetItemData(m_CbnInstrument, n, i);
				}
			}
			m_CbnInstrument->setCurrentIndex(nPos);
			if(nPos == 0)
				SetCurrentInstrument(0);
			m_CbnInstrument->setUpdatesEnabled(true);
			m_CbnInstrument->update(false);
		}
		if(updateAll || updatePatNames)
		{
			PATTERNINDEX nPat;
			if(patternHint.GetType()[HINT_PATNAMES])
				nPat = patternHint.GetPattern();
			else
				nPat = (PATTERNINDEX)SendViewMessage(VIEWMSG_GETCURRENTPATTERN);
			if(m_sndFile.Patterns.IsValidIndex(nPat))
			{
				m_EditPatName->SetWindowText(mpt::ToUnicode(m_sndFile.GetCharsetInternal(), m_sndFile.Patterns[nPat].GetName()));
			}

			bool bXMIT = (m_sndFile.GetType() & (MOD_TYPE_XM | MOD_TYPE_IT | MOD_TYPE_MPT)) ? true : false;
			m_ToolBar->EnableButton(ID_PATTERN_MIDIMACRO, bXMIT);
			m_ToolBar->EnableButton(ID_PATTERN_PROPERTIES, bXMIT);
			m_ToolBar->EnableButton(ID_PATTERN_EXPAND, bXMIT);
			m_ToolBar->EnableButton(ID_PATTERN_SHRINK, bXMIT);
		}
		UnlockControls();
	}
	if(hintType[HINT_MODTYPE | HINT_UNDO])
	{
		m_ToolBar->EnableButton(ID_EDIT_UNDO, m_modDoc.GetPatternUndo().CanUndo());
	}
}


const mfcless::ViewClass *CCtrlPatterns::GetAssociatedViewClass()
{
	return (&CViewPattern::classCViewPattern);
}


intptr_t CCtrlPatterns::OnModCtrlMsg(uintptr_t wParam, intptr_t lParam)
{
	switch(wParam)
	{
	case CTRLMSG_GETCURRENTINSTRUMENT:
		return m_nInstrument;

	case CTRLMSG_GETCURRENTPATTERN:
		return m_OrderList->GetCurrentPattern();

	case CTRLMSG_PATTERNCHANGED:
		UpdateView(PatternHint(static_cast<PATTERNINDEX>(lParam)).Names());
		break;

	case CTRLMSG_PAT_PREVINSTRUMENT:
		OnPrevInstrument();
		break;

	case CTRLMSG_PAT_NEXTINSTRUMENT:
		OnNextInstrument();
		break;

	case CTRLMSG_NOTIFYCURRENTORDER:
		if(m_OrderList->GetCurSel().GetSelCount() > 1 || m_OrderList->m_bDragging)
		{
			// Only update play cursor in case there's a selection
			m_OrderList->update(false);
			break;
		}
		// Otherwise, just act the same as a normal selection change
		[[fallthrough]];
	case CTRLMSG_SETCURRENTORDER:
		// Set order list selection and refresh GUI if change successful
		m_OrderList->SetCurSel(static_cast<ORDERINDEX>(lParam), false, false, true);
		break;

	case CTRLMSG_FORCEREFRESH:
		//refresh GUI
		m_OrderList->InvalidateRect(nullptr, false);
		break;

	case CTRLMSG_GETCURRENTORDER:
		return m_OrderList->GetCurSel(true).firstOrd;

	case CTRLMSG_SETCURRENTINSTRUMENT:
	case CTRLMSG_PAT_SETINSTRUMENT:
		return SetCurrentInstrument(static_cast<uint32>(lParam));

	case CTRLMSG_SETVIEWWND:
		{
			SendViewMessage(VIEWMSG_FOLLOWSONG, IsChecked(IDC_PATTERN_FOLLOWSONG));
			SendViewMessage(VIEWMSG_PATTERNLOOP, (m_sndFile.m_PlayState.m_flags[SONG_PATTERNLOOP]) ? true : false);
			OnSpacingChanged();
			SendViewMessage(VIEWMSG_SETRECORD, m_bRecord);
			SendViewMessage(VIEWMSG_SETVUMETERS, m_bVUMeters);
			SendViewMessage(VIEWMSG_SETPLUGINNAMES, m_bPluginNames);
		}
		break;

	case CTRLMSG_SETSPACING:
		SetItemInt(IDC_EDIT_SPACING, static_cast<uint32>(lParam));
		break;

	case CTRLMSG_PAT_SETORDERLISTFOCUS:
		GetParentFrame()->SetActiveView(&m_parent);
		m_OrderList->setFocus();
		break;

	case CTRLMSG_SETRECORD:
		if(lParam >= 0)
			m_bRecord = lParam != 0;
		else
			m_bRecord = !m_bRecord;
		m_ToolBar->CheckButton(IDC_PATTERN_RECORD, m_bRecord ? true : false);
		TrackerSettings::Instance().gbPatternRecord = m_bRecord;
		SendViewMessage(VIEWMSG_SETRECORD, m_bRecord);
		break;

	case CTRLMSG_TOGGLE_METRONOME:
		m_ToolBar->CheckButton(IDC_METRONOME, m_ToolBar->IsButtonChecked(IDC_METRONOME) ? false : true);
		OnToggleMetronome();
		break;

	case CTRLMSG_TOGGLE_OVERFLOW_PASTE:
		m_ToolBar->CheckButton(ID_OVERFLOWPASTE, m_ToolBar->IsButtonChecked(ID_OVERFLOWPASTE) ? false : true);
		OnToggleOverflowPaste();
		break;

	case CTRLMSG_PREVORDER:
		m_OrderList->SetCurSel(Order().GetPreviousOrderIgnoringSkips(m_OrderList->GetCurSel(true).firstOrd), true);
		break;

	case CTRLMSG_NEXTORDER:
		m_OrderList->SetCurSel(Order().GetNextOrderIgnoringSkips(m_OrderList->GetCurSel(true).firstOrd), true);
		break;

	case CTRLMSG_PAT_FOLLOWSONG:
		// parameters: 0 = turn off, 1 = toggle
		{
			uint32 state = 0;
			if(lParam == 1)	// toggle
			{
				state = (IsChecked(IDC_PATTERN_FOLLOWSONG) == 0) ? 1 : 0;
			}
			SetChecked(IDC_PATTERN_FOLLOWSONG, state);
			OnFollowSong();
		}
		break;

	case CTRLMSG_PAT_LOOP:
		{
			bool setLoop = false;
			if (lParam == -1)
			{
				//Toggle loop state
				setLoop = !m_sndFile.m_PlayState.m_flags[SONG_PATTERNLOOP];
			} else
			{
				setLoop = (lParam != 0);
			}

			m_sndFile.m_PlayState.m_flags.set(SONG_PATTERNLOOP, setLoop);
			SetChecked(IDC_PATTERN_LOOP, setLoop ? 1 : 0);
			break;
		}
	case CTRLMSG_PAT_NEWPATTERN:
		OnPatternNew();
		break;

	case CTRLMSG_PAT_DUPPATTERN:
		OnPatternDuplicate();
		break;

	case CTRLMSG_PAT_SETSEQUENCE:
		m_OrderList->SelectSequence(static_cast<SEQUENCEINDEX>(lParam));
		UpdateView(SequenceHint(static_cast<SEQUENCEINDEX>(lParam)).Names(), nullptr);
		break;

	case CTRLMSG_PAT_UPDATE_TOOLBAR:
		m_ToolBar->CheckButton(ID_OVERFLOWPASTE, (TrackerSettings::Instance().patternSetup & PatternSetup::OverflowPaste) ? true : false);
		m_ToolBar->CheckButton(IDC_METRONOME, TrackerSettings::Instance().metronomeEnabled ? true : false);
		break;

	default:
		return CModControlDlg::OnModCtrlMsg(wParam, lParam);
	}
	return 0;
}


void CCtrlPatterns::SetCurrentPattern(PATTERNINDEX nPat)
{
	SendViewMessage(VIEWMSG_SETCURRENTPATTERN, (intptr_t)nPat);
}


bool CCtrlPatterns::SetCurrentInstrument(uint32 nIns)
{
	if(nIns == m_nInstrument)
		return true;
	int n = m_CbnInstrument->count();
	for(int i = 0; i < n; i++)
	{
		if(mfcless::GetItemData(m_CbnInstrument, i) == nIns)
		{
			m_CbnInstrument->setCurrentIndex(i);
			m_nInstrument = static_cast<INSTRUMENTINDEX>(nIns);
			m_parent.InstrumentChanged(m_nInstrument);
			EnableItem(IDC_PATINSTROPLUGGUI, HasValidPlug(m_nInstrument) ? true : false);
			return true;
		}
	}
	return false;
}


bool CCtrlPatterns::GetFollowSong() const
{
	return IsChecked(IDC_PATTERN_FOLLOWSONG);
}


bool CCtrlPatterns::GetLoopPattern() const
{
	return IsChecked(IDC_PATTERN_LOOP);
}



////////////////////////////////////////////////////////////
// CCtrlPatterns messages

void CCtrlPatterns::OnActivatePage(intptr_t lParam)
{
	int nIns = m_parent.GetInstrumentChange();
	if(nIns > 0)
	{
		SetCurrentInstrument(nIns);
	}

	if(!(lParam & 0x80000000))
	{
		// Pattern item
		auto pat = static_cast<PATTERNINDEX>(lParam & 0xFFFF);
		if(m_sndFile.Patterns.IsValidIndex(pat))
		{
			for(SEQUENCEINDEX seq = 0; seq < m_sndFile.Order.GetNumSequences(); seq++)
			{
				if(ORDERINDEX ord = m_sndFile.Order(seq).FindOrder(pat); ord != ORDERINDEX_INVALID)
				{
					m_OrderList->SelectSequence(seq);
					m_OrderList->SetCurSel(ord, true);
					UpdateView(SequenceHint(seq).Names(), nullptr);
					break;
				}
			}
		}
		SetCurrentPattern(pat);
	} else if((lParam & 0x80000000))
	{
		// Order item
		auto ord = static_cast<ORDERINDEX>(lParam & 0xFFFF);
		auto seq = static_cast<SEQUENCEINDEX>((lParam >> 16) & 0x7FFF);
		if(seq < m_sndFile.Order.GetNumSequences())
		{
			m_OrderList->SelectSequence(seq);
			const auto &order = Order();
			if(ord < order.size())
			{
				m_OrderList->SetCurSel(ord);
				SetCurrentPattern(order[ord]);
			}
			UpdateView(SequenceHint(static_cast<SEQUENCEINDEX>(seq)).Names(), nullptr);
		}
	}
	if(m_hWndView)
	{
		OnSpacingChanged();
		if(m_bRecord)
			SendViewMessage(VIEWMSG_SETRECORD, m_bRecord);
		CChildFrame *pFrame = (CChildFrame *)GetParentFrame();

		// Restore all save pattern state, except pattern number which we might have just set.
		PatternViewState &patternViewState = pFrame->GetPatternViewState();
		if(patternViewState.initialOrder != ORDERINDEX_INVALID)
		{
			if(CMainFrame::GetMainFrame()->GetModPlaying() != &m_modDoc)
				m_OrderList->SetCurSel(patternViewState.initialOrder);
			patternViewState.initialOrder = ORDERINDEX_INVALID;
		}

		patternViewState.nPattern = static_cast<PATTERNINDEX>(SendViewMessage(VIEWMSG_GETCURRENTPATTERN));
		SendViewMessage(VIEWMSG_LOADSTATE, (intptr_t)&patternViewState);

		SwitchToView();
	}

	// Combo boxes randomly disappear without this... why?
	Invalidate();
}


void CCtrlPatterns::OnDeactivatePage()
{
	CChildFrame *pFrame = (CChildFrame *)GetParentFrame();
	if((pFrame) && (m_hWndView))
		SendViewMessage(VIEWMSG_SAVESTATE, (intptr_t)&pFrame->GetPatternViewState());
}


void CCtrlPatterns::OnVScroll(mfcless::ScrollCode code, int pos, QWidget *pScrollBar)
{
	CModControlDlg::OnVScroll(nSBCode, nPos, pScrollBar);
	short int pos = (short int)m_SpinInstrument->GetPos();
	if(pos)
	{
		m_SpinInstrument->SetPos(0);
		if(pos < 0)
			OnPrevInstrument();
		else
			OnNextInstrument();
	}
}


void CCtrlPatterns::OnSequencePrev()
{
	m_OrderList->SetCurSel(m_OrderList->GetCurSel(true).firstOrd - 1);
	m_OrderList->setFocus();
}


void CCtrlPatterns::OnSequenceNext()
{
	m_OrderList->SetCurSel(m_OrderList->GetCurSel(true).firstOrd + 1);
	m_OrderList->setFocus();
}


void CCtrlPatterns::OnChannelManager()
{
	m_modDoc.OnChannelManager();
}


void CCtrlPatterns::OnKeyDown(uint32 nChar, uint32 nRepCnt, uint32 nFlags)
{
	CModControlDlg::OnKeyDown(nChar, nRepCnt, nFlags);
}


void CCtrlPatterns::OnSpacingChanged()
{
	if((m_EditSpacing->m_hWnd) && (m_EditSpacing->GetWindowTextLength() > 0))
	{
		TrackerSettings::Instance().gnPatternSpacing = GetItemInt(IDC_EDIT_SPACING);
		if(TrackerSettings::Instance().gnPatternSpacing > MAX_SPACING)
		{
			TrackerSettings::Instance().gnPatternSpacing = MAX_SPACING;
			SetDlgItemInt(IDC_EDIT_SPACING, TrackerSettings::Instance().gnPatternSpacing, false);
		}
		SendViewMessage(VIEWMSG_SETSPACING, TrackerSettings::Instance().gnPatternSpacing);
	}
}


void CCtrlPatterns::OnInstrumentChanged()
{
	int n = m_CbnInstrument->currentIndex();
	if(n >= 0)
	{
		n = static_cast<int>(mfcless::GetItemData(m_CbnInstrument, n));
		int nmax = m_sndFile.GetNumInstruments() ? m_sndFile.GetNumInstruments() : m_sndFile.GetNumSamples();
		if((n >= 0) && (n <= nmax) && (n != (int)m_nInstrument))
		{
			m_nInstrument = static_cast<INSTRUMENTINDEX>(n);
			m_parent.InstrumentChanged(m_nInstrument);
		}
		if(m_instrDropdownOpen)
		{
			m_instrDropdownOpen = false;
			SwitchToView();
		}
		::EnableWindow(::GetDlgItem(m_hWnd, IDC_PATINSTROPLUGGUI), HasValidPlug(m_nInstrument));
	}
}


void CCtrlPatterns::OnPrevInstrument()
{
	int n = m_CbnInstrument->count();
	if(n > 0)
	{
		int pos = m_CbnInstrument->currentIndex();
		if(pos > 0)
			pos--;
		else
			pos = n - 1;
		m_CbnInstrument->setCurrentIndex(pos);
		OnInstrumentChanged();
		SwitchToViewIfMouse();
	}
}


void CCtrlPatterns::OnNextInstrument()
{
	int n = m_CbnInstrument->count();
	if(n > 0)
	{
		int pos = m_CbnInstrument->currentIndex() + 1;
		if(pos >= n)
			pos = 0;
		m_CbnInstrument->setCurrentIndex(pos);
		OnInstrumentChanged();
		SwitchToViewIfMouse();
	}
}


void CCtrlPatterns::OnPlayerPause()
{
	CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
	if(pMainFrm)
		pMainFrm->PauseMod();
}


void CCtrlPatterns::OnPatternNew()
{
	const auto &order = Order();
	ORDERINDEX curOrd = m_OrderList->GetCurSel(true).firstOrd;
	PATTERNINDEX curPat = (curOrd < order.size()) ? order[curOrd] : 0;
	ROWINDEX rows = 64;
	if(m_sndFile.Patterns.IsValidPat(curPat))
	{
		// Only if the current oder is already occupied, create a new pattern at the next position.
		curOrd++;
	} else
	{
		// Use currently edited pattern for new pattern length
		curPat = static_cast<PATTERNINDEX>(SendViewMessage(VIEWMSG_GETCURRENTPATTERN));
	}
	if(m_sndFile.Patterns.IsValidPat(curPat))
	{
		rows = m_sndFile.Patterns[curPat].GetNumRows();
	}
	rows = Clamp(rows, m_sndFile.GetModSpecifications().patternRowsMin, m_sndFile.GetModSpecifications().patternRowsMax);
	const PATTERNINDEX newPat = m_modDoc.InsertPattern(rows, curOrd);
	if(m_sndFile.Patterns.IsValidPat(newPat))
	{
		// update time signature
		if(m_sndFile.Patterns.IsValidIndex(curPat))
		{
			if(m_sndFile.Patterns[curPat].GetOverrideSignature())
				m_sndFile.Patterns[newPat].SetSignature(m_sndFile.Patterns[curPat].GetRowsPerBeat(), m_sndFile.Patterns[curPat].GetRowsPerMeasure());
			if(m_sndFile.Patterns[curPat].HasTempoSwing())
				m_sndFile.Patterns[newPat].SetTempoSwing(m_sndFile.Patterns[curPat].GetTempoSwing());
		}
		// move to new pattern
		m_OrderList->SetCurSel(curOrd);
		m_OrderList->update(false);
		SetCurrentPattern(newPat);
		m_modDoc.SetModified();
		m_modDoc.UpdateAllViews(nullptr, PatternHint(newPat).Names(), this);
		m_modDoc.UpdateAllViews(nullptr, SequenceHint().Data(), this);
		SwitchToViewIfMouse();
	}
}


// Duplicates one or more patterns.
void CCtrlPatterns::OnPatternDuplicate()
{
	OrdSelection selection = m_OrderList->GetCurSel();
	const ORDERINDEX insertFrom = selection.firstOrd;
	const ORDERINDEX insertWhere = selection.lastOrd + 1u;
	if(insertWhere >= m_sndFile.GetModSpecifications().ordersMax)
		return;
	const ORDERINDEX insertCount = std::min(selection.GetSelCount(), static_cast<ORDERINDEX>(m_sndFile.GetModSpecifications().ordersMax - insertWhere));
	if(!insertCount)
		return;

	bool success = false, outOfPatterns = false;
	// Has this pattern been duplicated already? (for multiselect)
	std::vector<PATTERNINDEX> patReplaceIndex(m_sndFile.Patterns.Size(), PATTERNINDEX_INVALID);

	ModSequence &order = Order();
	for(ORDERINDEX i = 0; i < insertCount; i++)
	{
		PATTERNINDEX curPat = order[insertFrom + i];
		if(curPat < patReplaceIndex.size() && patReplaceIndex[curPat] == PATTERNINDEX_INVALID)
		{
			PATTERNINDEX newPat = m_sndFile.Patterns.Duplicate(curPat, true);
			if(newPat != PATTERNINDEX_INVALID)
			{
				order.insert(insertWhere + i, 1, newPat);
				success = true;
				// Mark as duplicated, so if this pattern is to be duplicated again, the same new pattern number is inserted into the order list.
				patReplaceIndex[curPat] = newPat;
			} else
			{
				if(m_sndFile.Patterns.IsValidPat(curPat))
					outOfPatterns = true;
				continue;
			}
		} else
		{
			// Invalid pattern, or it has been duplicated before (multiselect)
			PATTERNINDEX newPat;
			if(curPat < patReplaceIndex.size() && patReplaceIndex[curPat] != PATTERNINDEX_INVALID)
			{
				// Take care of patterns that have been duplicated before
				newPat = patReplaceIndex[curPat];
			} else
			{
				newPat = order[insertFrom + i];
			}

			order.insert(insertWhere + i, 1, newPat);

			success = true;
		}
	}
	if(success)
	{
		m_OrderList->InsertUpdatePlaystate(selection.firstOrd, selection.lastOrd);

		m_OrderList->update(false);
		m_OrderList->SetCurSel(insertWhere, true, false, true);

		// If the first duplicated order is e.g. a +++ item, we need to move the pattern display on or else we'll still edit the previously shown pattern.
		ORDERINDEX showPattern = std::min(insertWhere, order.GetLastIndex());
		while(!order.IsValidPat(showPattern) && showPattern < order.GetLastIndex())
		{
			showPattern++;
		}
		SetCurrentPattern(order[showPattern]);

		m_modDoc.SetModified();
		m_modDoc.UpdateAllViews(nullptr, SequenceHint().Data(), this);
		m_modDoc.UpdateAllViews(nullptr, PatternHint(PATTERNINDEX_INVALID).Names(), this);
		if(selection.lastOrd != selection.firstOrd)
			m_OrderList->m_nScrollPos2nd = insertWhere + insertCount - 1u;
	}
	if(outOfPatterns)
	{
		const auto &specs = m_sndFile.GetModSpecifications();
		Reporting::Error(MPT_UFORMAT("Pattern limit of the {} format ({} patterns) has been reached.")(specs.GetFileExtensionUpper(), specs.patternsMax), U_("Duplicate Patterns"));
	}
	SwitchToViewIfMouse();
}


// Merges one or more patterns into a single pattern
void CCtrlPatterns::OnPatternMerge()
{
	const OrdSelection selection = m_OrderList->GetCurSel();
	const ORDERINDEX firstOrder = selection.firstOrd;
	const ORDERINDEX numOrders = selection.GetSelCount();

	// Get the total number of lines to be merged
	std::vector<ModCommand> originalData;
	ROWINDEX numRows = 0, minPatternSize = MAX_PATTERN_ROWS, maxPatternSize = 0;
	ModSequence &order = Order();
	for(ORDERINDEX ord = selection.firstOrd; ord <= selection.lastOrd; ord++)
	{
		const CPattern *pattern = order.PatternAt(ord);
		if(!pattern)
			continue;
		numRows += pattern->GetNumRows();
		minPatternSize = std::min(minPatternSize, pattern->GetNumRows());
		maxPatternSize = std::max(maxPatternSize, pattern->GetNumRows());
		originalData.insert(originalData.end(), pattern->cbegin(), pattern->cend());
	}
	if(!numRows || !numOrders)
	{
		MessageBeep(MB_ICONWARNING);
		SwitchToViewIfMouse();
		return;
	}

	const auto &specs = m_sndFile.GetModSpecifications();
	const mpt::ustring format = specs.GetFileExtensionUpper();

	const ORDERINDEX remainingOrders = order.GetRemainingCapacity(selection.lastOrd + 1) + numOrders;
	ROWINDEX minRows = (numRows + (remainingOrders - 1)) / remainingOrders;
	if(minRows > specs.patternRowsMax)
	{
		Reporting::Error(U_("There are not enough empty orders left to merge the selected patterns into."), U_("Merge Patterns"));
		SwitchToViewIfMouse();
		return;
	}

	const PATTERNINDEX remainingPatterns = m_sndFile.Patterns.GetRemainingCapacity();
	if(!remainingPatterns)
	{
		Reporting::Error(MPT_UFORMAT("Pattern limit of the {} format ({} patterns) has been reached.")(format, specs.patternsMax), U_("Merge Patterns"));
		SwitchToViewIfMouse();
		return;
	}
	minRows = std::max(minRows, (numRows + (remainingPatterns - 1)) / remainingPatterns);
	if(minRows > specs.patternRowsMax)
	{
		Reporting::Error(U_("There are not enough empty patterns left to merge the selected patterns into."), U_("Merge Patterns"));
		SwitchToViewIfMouse();
		return;
	}

	ROWINDEX patternSize = minRows;
	if(minRows != specs.patternRowsMax)
	{
		CInputDlg dlg(this, U_("New pattern length:"), static_cast<int32>(minRows), static_cast<int32>(specs.patternRowsMax), static_cast<int32>(std::clamp(numRows, minRows, specs.patternRowsMax)));
		if(dlg.DoModal() != IDOK)
		{
			SwitchToViewIfMouse();
			return;
		}
		patternSize = static_cast<ROWINDEX>(dlg.resultAsInt);
	}
	if(minPatternSize == maxPatternSize && minPatternSize == patternSize)
	{
		MessageBeep(MB_ICONWARNING);
		SwitchToViewIfMouse();
		return;
	}

	CriticalSection cs;

	const PATTERNINDEX patternsRequired = static_cast<PATTERNINDEX>((numRows + (patternSize - 1)) / patternSize);
	// Double-check if we *still* have enough patterns and orders (right now it should not be possible that this number changed, but in the future it might due to scripting / etc.)
	if(m_sndFile.Patterns.GetRemainingCapacity() < patternsRequired || (order.GetRemainingCapacity(selection.lastOrd + 1) + numOrders) < patternsRequired)
	{
		cs.Leave();
		Reporting::Error(U_("There are not enough empty patterns or order lists for this operation."), U_("Merge Patterns"));
		SwitchToViewIfMouse();
		return;
	}

	order.Remove(selection.firstOrd, selection.lastOrd);
	m_OrderList->DeleteUpdatePlaystate(selection.firstOrd, selection.lastOrd);

	order.insert(firstOrder, patternsRequired, PATTERNINDEX_INVALID);
	m_OrderList->InsertUpdatePlaystate(firstOrder, firstOrder + patternsRequired);

	PATTERNINDEX patternsInserted = 0;
	auto sourceData = originalData.cbegin();
	while(sourceData != originalData.end())
	{
		const ROWINDEX thisPatternSize = std::min(numRows, patternSize);

		const PATTERNINDEX newPat = m_sndFile.Patterns.InsertAny(std::max(thisPatternSize, specs.patternRowsMin), true);
		if(newPat == PATTERNINDEX_INVALID)
			break;

		auto &pattern = m_sndFile.Patterns[newPat];
		auto sourceEnd = sourceData + thisPatternSize * m_sndFile.GetNumChannels();
		std::copy(sourceData, sourceEnd, pattern.begin());
		sourceData = sourceEnd;

		if(pattern.GetNumRows() > thisPatternSize)
			pattern.WriteEffect(EffectWriter(CMD_PATTERNBREAK, 0).Row(thisPatternSize - 1).RetryNextRow());

		order[firstOrder + patternsInserted] = newPat;

		numRows -= thisPatternSize;
		patternsInserted++;
	}

	m_OrderList->update(false);
	m_OrderList->SetSelection(firstOrder, firstOrder + patternsInserted);
	SetCurrentPattern(order[firstOrder]);

	m_modDoc.SetModified();
	m_modDoc.UpdateAllViews(nullptr, SequenceHint().Data(), this);
	m_modDoc.UpdateAllViews(nullptr, PatternHint(PATTERNINDEX_INVALID).Names(), this);

	SwitchToViewIfMouse();
}


void CCtrlPatterns::OnPatternStop()
{
	CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
	if(pMainFrm)
		pMainFrm->PauseMod(&m_modDoc);
	m_sndFile.ResetChannels();
	SwitchToViewIfMouse();
}


void CCtrlPatterns::OnPatternPlay()
{
	if(CMainFrame::GetMainFrame()->GetInputHandler()->ShiftPressed())
		m_modDoc.OnPatternPlayNoLoop();
	else
		m_modDoc.OnPatternPlay();
	SwitchToViewIfMouse();
}


void CCtrlPatterns::OnPatternPlayFromStart()
{
	if(CMainFrame::GetMainFrame()->GetInputHandler()->ShiftPressed())
		m_modDoc.OnPatternRestart(false);
	else
		m_modDoc.OnPatternRestart();
	SwitchToViewIfMouse();
}


void CCtrlPatterns::OnPatternRecord()
{
	m_bRecord = !m_bRecord;
	TrackerSettings::Instance().gbPatternRecord = m_bRecord;
	m_ToolBar->CheckButton(IDC_PATTERN_RECORD, m_bRecord ? true : false);
	SendViewMessage(VIEWMSG_SETRECORD, m_bRecord);
	SwitchToViewIfMouse();
}


void CCtrlPatterns::OnPatternVUMeters()
{
	m_bVUMeters = !m_bVUMeters;
	TrackerSettings::Instance().gbPatternVUMeters = m_bVUMeters;
	m_ToolBar->CheckButton(ID_PATTERN_VUMETERS, m_bVUMeters ? true : false);
	SendViewMessage(VIEWMSG_SETVUMETERS, m_bVUMeters);
	SwitchToViewIfMouse();
}


void CCtrlPatterns::OnPatternViewPlugNames()
{
	m_bPluginNames = !m_bPluginNames;
	TrackerSettings::Instance().gbPatternPluginNames = m_bPluginNames;
	m_ToolBar->CheckButton(ID_VIEWPLUGNAMES, m_bPluginNames ? true : false);
	SendViewMessage(VIEWMSG_SETPLUGINNAMES, m_bPluginNames);
	SwitchToViewIfMouse();
}


void CCtrlPatterns::OnToggleOverflowPaste()
{
	TrackerSettings::Instance().patternSetup ^= PatternSetup::OverflowPaste;
	theApp.PostMessageToAllViews(WM_MOD_CTRLMSG, CTRLMSG_PAT_UPDATE_TOOLBAR);
	SwitchToViewIfMouse();
}


void CCtrlPatterns::OnToggleMetronome()
{
	TrackerSettings::Instance().metronomeEnabled = !TrackerSettings::Instance().metronomeEnabled;
	CMainFrame::GetMainFrame()->UpdateMetronomeSamples();
	theApp.PostMessageToAllViews(WM_MOD_CTRLMSG, CTRLMSG_PAT_UPDATE_TOOLBAR);
	SwitchToViewIfMouse();
}


void CCtrlPatterns::OnMetronomeSettings()
{
	MetronomeSettingsDlg dlg{this};
	dlg.DoModal();
	SwitchToViewIfMouse();
}


void CCtrlPatterns::OnPatternProperties()
{
	SendViewMessage(VIEWMSG_PATTERNPROPERTIES, PATTERNINDEX_INVALID);
	SwitchToViewIfMouse();
}


void CCtrlPatterns::OnPatternExpand()
{
	SendViewMessage(VIEWMSG_EXPANDPATTERN);
	SwitchToViewIfMouse();
}


void CCtrlPatterns::OnPatternPaste()
{
	SendViewMessage(VIEWMSG_PASTEPATTERN);
	SwitchToViewIfMouse();
}


void CCtrlPatterns::OnPatternShrink()
{
	SendViewMessage(VIEWMSG_SHRINKPATTERN);
	SwitchToViewIfMouse();
}


void CCtrlPatterns::OnPatternAmplify()
{
	SendViewMessage(VIEWMSG_AMPLIFYPATTERN);
	SwitchToViewIfMouse();
}


void CCtrlPatterns::OnPatternPlayRow()
{
	::SendMessage(m_hWndView, WM_COMMAND, ID_PATTERN_PLAYROW, 0);
	SwitchToViewIfMouse();
}


void CCtrlPatterns::OnUpdateRecord(mfcless::CommandState &state)
{
	if(pCmdUI)
		state.SetCheck((m_bRecord) ? true : false);
}


void CCtrlPatterns::OnFollowSong()
{
	SendViewMessage(VIEWMSG_FOLLOWSONG, IsChecked(IDC_PATTERN_FOLLOWSONG));
	SwitchToViewIfMouse();
}


void CCtrlPatterns::OnChangeLoopStatus()
{
	OnModCtrlMsg(CTRLMSG_PAT_LOOP, IsChecked(IDC_PATTERN_LOOP));
	SwitchToViewIfMouse();
}


void CCtrlPatterns::OnEditUndo()
{
	if(m_hWndView)
		::SendMessage(m_hWndView, WM_COMMAND, ID_EDIT_UNDO, 0);
	SwitchToViewIfMouse();
}


// cppcheck-suppress duplInheritedMember
void CCtrlPatterns::OnSwitchToView()
{
	PostViewMessage(VIEWMSG_SETFOCUS);
}


void CCtrlPatterns::OnPatternNameChanged()
{
	if(!IsLocked())
	{
		const PATTERNINDEX nPat = (PATTERNINDEX)SendViewMessage(VIEWMSG_GETCURRENTPATTERN);

		mpt::ustring tmp;
		m_EditPatName->GetWindowText(tmp);
		const std::string s = mpt::ToCharset(m_sndFile.GetCharsetInternal(), tmp);

		if(m_sndFile.Patterns[nPat].GetName() != s)
		{
			if(m_sndFile.Patterns[nPat].SetName(s))
			{
				if(m_sndFile.GetType() & (MOD_TYPE_XM | MOD_TYPE_IT | MOD_TYPE_MPT))
					m_modDoc.SetModified();
				m_modDoc.UpdateAllViews(nullptr, PatternHint(nPat).Names(), this);
			}
		}
	}
}


void CCtrlPatterns::OnSequenceNameChanged()
{
	mpt::ustring tmp;
	tmp = GetItemText(IDC_EDIT_SEQUENCE_NAME);
	const mpt::ustring str = mpt::ToUnicode(tmp);
	auto &order = Order();
	if(str != order.GetName())
	{
		order.SetName(str);
		m_modDoc.SetModified();
		m_modDoc.UpdateAllViews(nullptr, SequenceHint(m_sndFile.Order.GetCurrentSequenceIndex()).Names(), this);
	}
}


void CCtrlPatterns::OnChordEditor()
{
	CChordEditor dlg(this);
	dlg.DoModal();
	SwitchToViewIfMouse();
}


void CCtrlPatterns::OnTbnDropDownToolBar()
{
	CInputHandler *ih = CMainFrame::GetInputHandler();
	NMTOOLBAR *pToolBar = reinterpret_cast<NMTOOLBAR *>(pNMHDR);
	ClientToScreen(&(pToolBar->rcButton));
	const int offset = HighDPISupport::ScalePixels(4, m_hWnd);  // Compared to the main toolbar, the offset seems to be a bit wrong here...?
	int x = pToolBar->rcButton.left + offset, y = pToolBar->rcButton.bottom + offset;
	const auto visibleColumns = std::bitset<PatternCursor::numColumns>{static_cast<unsigned long>(SendViewMessage(VIEWMSG_GETDETAIL))};
	mfcless::Menu menu;
	switch(pToolBar->iItem)
	{
	case IDC_PATTERN_NEW:
		menu.CreatePopupMenu();
		menu.AppendMenu(mfcless::menu::String, ID_ORDERLIST_COPY, ih->GetKeyTextFromCommand(kcDuplicatePattern, U_("&Duplicate Pattern")));
		menu.TrackPopupMenu(mfcless::menu::LeftAlign | mfcless::menu::RightButton, x, y, this);
		menu.DestroyMenu();
		break;

	case IDC_METRONOME:
		menu.CreatePopupMenu();
		menu.AppendMenu(mfcless::menu::String, ID_METRONOME_SETTINGS, U_("&Metronome Settings"));
		menu.TrackPopupMenu(mfcless::menu::LeftAlign | mfcless::menu::RightButton, x, y, this);
		menu.DestroyMenu();
		break;

	case ID_PATTERNDETAIL_DROPDOWN:
		menu.CreatePopupMenu();
		menu.AppendMenu(mfcless::menu::String | (visibleColumns[PatternCursor::instrColumn] ? mfcless::menu::Checked : 0), ID_PATTERNDETAIL_INSTR, ih->GetKeyTextFromCommand(kcToggleVisibilityInstrColumn, U_("Show &Instrument Column")));
		menu.AppendMenu(mfcless::menu::String | (visibleColumns[PatternCursor::volumeColumn] ? mfcless::menu::Checked : 0), ID_PATTERNDETAIL_VOLUME, ih->GetKeyTextFromCommand(kcToggleVisibilityVolumeColumn, U_("Show &Volume Column")));
		menu.AppendMenu(mfcless::menu::String | (visibleColumns[PatternCursor::effectColumn] ? mfcless::menu::Checked : 0), ID_PATTERNDETAIL_EFFECT, ih->GetKeyTextFromCommand(kcToggleVisibilityEffectColumn, U_("Show &Effect Column")));
		menu.TrackPopupMenu(mfcless::menu::LeftAlign | mfcless::menu::RightButton, x, y, this);
		menu.DestroyMenu();
		break;
	}
	CurrentNotify().result = 0;
}


void CCtrlPatterns::OnDetailSwitch()
{
	// Cycle through all bit combinations
	auto visibleColumns = std::bitset<PatternCursor::numColumns>{static_cast<unsigned long>(SendViewMessage(VIEWMSG_GETDETAIL) + 1)};
	visibleColumns.set(PatternCursor::noteColumn);
	visibleColumns.set(PatternCursor::paramColumn, visibleColumns[PatternCursor::effectColumn]);
	SendViewMessage(VIEWMSG_SETDETAIL, visibleColumns.to_ulong());
	SwitchToViewIfMouse();
}


void CCtrlPatterns::OnDetailInstr()
{
	auto visibleColumns = std::bitset<PatternCursor::numColumns>{static_cast<unsigned long>(SendViewMessage(VIEWMSG_GETDETAIL))};
	visibleColumns.flip(PatternCursor::instrColumn);
	SendViewMessage(VIEWMSG_SETDETAIL, visibleColumns.to_ulong());
	SwitchToViewIfMouse();
}


void CCtrlPatterns::OnDetailVolume()
{
	auto visibleColumns = std::bitset<PatternCursor::numColumns>{static_cast<unsigned long>(SendViewMessage(VIEWMSG_GETDETAIL))};
	visibleColumns.flip(PatternCursor::volumeColumn);
	SendViewMessage(VIEWMSG_SETDETAIL, visibleColumns.to_ulong());
	SwitchToViewIfMouse();
}


void CCtrlPatterns::OnDetailEffect()
{
	auto visibleColumns = std::bitset<PatternCursor::numColumns>{static_cast<unsigned long>(SendViewMessage(VIEWMSG_GETDETAIL))};
	visibleColumns.flip(PatternCursor::effectColumn);
	visibleColumns.flip(PatternCursor::paramColumn);
	SendViewMessage(VIEWMSG_SETDETAIL, visibleColumns.to_ulong());
	SwitchToViewIfMouse();
}


void CCtrlPatterns::TogglePluginEditor()
{
	if(m_sndFile.GetInstrumentPlugin(m_nInstrument) != nullptr)
	{
		m_modDoc.TogglePluginEditor(m_sndFile.Instruments[m_nInstrument]->nMixPlug - 1, CInputHandler::ShiftPressed());
	}
}


bool CCtrlPatterns::HasValidPlug(INSTRUMENTINDEX instr) const
{
	return m_sndFile.GetInstrumentPlugin(instr) != nullptr;
}


bool CCtrlPatterns::OnMouseWheel(uint32 nFlags, short zDelta, mfcless::Point pt)
{
	if(nFlags == 0)
	{
		PostViewMessage(VIEWMSG_DOSCROLL, zDelta);
	}
	return CModControlDlg::OnMouseWheel(nFlags, zDelta, pt);
}


void CCtrlPatterns::OnXButtonUp(uint32 nFlags, uint32 nButton, mfcless::Point point)
{
	if(nButton == XBUTTON1)
		OnModCtrlMsg(CTRLMSG_PREVORDER, 0);
	else if(nButton == XBUTTON2)
		OnModCtrlMsg(CTRLMSG_NEXTORDER, 0);
	CModControlDlg::OnXButtonUp(nFlags, nButton, point);
}


mpt::ustring CCtrlPatterns::GetToolTipText(uint32 id, QWidget *) const
{
	mpt::ustring s;
	CommandID cmd = kcNull;
	switch(id)
	{
	case IDC_PATTERN_NEW: s = U_("Insert Pattern"); cmd = kcNewPattern; break;
	case IDC_PATTERN_PLAY: s = U_("Play Pattern (Shift-click to play song from cursor)"); cmd = kcPlayPatternFromCursor; break;
	case IDC_PATTERN_PLAYFROMSTART: s = U_("Replay Pattern (Shift-click to play song from start of pattern)"); cmd = kcPlayPatternFromStart; break;
	case IDC_PATTERN_STOP: s = U_("Stop"); cmd = kcPauseSong; break;
	case ID_PATTERN_PLAYROW: s = U_("Play Row"); cmd = kcPatternPlayRow; break;
	case IDC_PATTERN_RECORD: s = U_("Record"); cmd = kcPatternRecord; break;
	case IDC_METRONOME: s = U_("Metronome"); cmd = kcToggleMetronome; break;
	case ID_PATTERN_VUMETERS: s = U_("VU-Meters"); break;
	case ID_VIEWPLUGNAMES: s = U_("Show Plugins"); break;
	case ID_PATTERN_CHANNELMANAGER: s = U_("Channel Manager"); cmd = kcViewChannelManager; break;
	case ID_PATTERN_MIDIMACRO: s = U_("Zxx Macro Configuration"); cmd = kcShowMacroConfig; break;
	case ID_PATTERN_CHORDEDIT: s = U_("Chord Editor"); cmd = kcChordEditor; break;
	case ID_EDIT_UNDO:
		s = U_("Undo");
		if(m_modDoc.GetPatternUndo().CanUndo())
			s += U_(" ") + m_modDoc.GetPatternUndo().GetUndoName();
		cmd = kcEditUndo;
		break;
	case ID_PATTERN_PROPERTIES: s = U_("Pattern Properties"); cmd = kcShowPatternProperties; break;
	case ID_PATTERN_EXPAND: s = U_("Expand Pattern"); cmd = kcPatternExpand; break;
	case ID_PATTERN_SHRINK: s = U_("Shrink Pattern"); cmd = kcPatternShrink; break;
	case ID_PATTERNDETAIL_DROPDOWN: s = U_("Change Pattern Detail Level"); break;
	case ID_OVERFLOWPASTE: s = U_("Toggle Overflow Paste"); cmd = kcToggleOverflowPaste; break;
	case IDC_PATTERN_LOOP: s = U_("Toggle Loop Pattern"); cmd = kcChangeLoopStatus; break;
	case IDC_PATTERN_FOLLOWSONG: s = U_("Toggle Follow Song"); cmd = kcToggleFollowSong; break;

	case IDC_SPIN_SEQNUM:
	case IDC_EDIT_SEQNUM:
	case IDC_EDIT_SEQUENCE_NAME:
		if(!GetDlgItem(id)->IsWindowEnabled())
			s = U_("Multiple sequences are only supported in the MPTM format.");
		break;

	case IDC_BUTTON1: s = U_("Next Pattern"); cmd = kcNextOrder; break;
	case IDC_BUTTON2: s = U_("Previous Pattern"); cmd = kcPrevOrder; break;
	}

	if(cmd != kcNull)
	{
		auto keyText = CMainFrame::GetInputHandler()->m_activeCommandSet->GetKeyTextFromCommand(cmd, 0);
		if(!keyText.IsEmpty())
			s += MPT_UFORMAT(" ({})")(keyText);
	}
	return s;
}


void CCtrlPatterns::OnSequenceNumChanged()
{
	if((m_EditSequence->m_hWnd) && (m_EditSequence->GetWindowTextLength() > 0))
	{
		SEQUENCEINDEX newSeq = static_cast<SEQUENCEINDEX>(GetItemInt(IDC_EDIT_SEQNUM) - 1);

		if(newSeq == m_sndFile.Order.GetCurrentSequenceIndex())
			return;

		if(newSeq >= m_sndFile.Order.GetNumSequences())
		{
			newSeq = m_sndFile.Order.GetNumSequences() - 1;
			SetItemInt(IDC_EDIT_SEQNUM, newSeq + 1);
		}
		m_OrderList->SelectSequence(newSeq);
		UpdateView(SequenceHint(newSeq).Names(), nullptr);
	}
}

OPENMPT_NAMESPACE_END
