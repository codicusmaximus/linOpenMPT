/*
 * view_com.cpp
 * ------------
 * Purpose: Song comments tab, lower panel.
 * Notes  : mfcless twin of upstream's mptrack/view_com.cpp.
 *          Hand-edited from the twin.py draft.
 * Authors: Olivier Lapicque
 *          OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "view_com.h"
#include "mfcless/Widgets.h"
#include "Childfrm.h"
#include "Clipboard.h"
#include "Ctrl_com.h"
#include "Globals.h"
#include "ImageLists.h"
#include "InputHandler.h"
#include "Mainfrm.h"
#include "Moddoc.h"
#include "Mptrack.h"
#include "Reporting.h"
#include "resource.h"
#include "TrackerSettings.h"
#include "WindowMessages.h"
#include "../common/mptStringBuffer.h"
#include "../soundlib/mod_specifications.h"

OPENMPT_NAMESPACE_BEGIN


#define DETAILS_TOOLBAR_CY HighDPISupport::ScalePixels(28, this)

enum
{
	SMPLIST_SAMPLENAME = 0,
	SMPLIST_SAMPLENO,
	SMPLIST_SIZE,
	SMPLIST_TYPE,
	SMPLIST_MIDDLEC,
	SMPLIST_INSTR,
	SMPLIST_FILENAME,
	SMPLIST_PATH,
	SMPLIST_COLUMNS
};


enum
{
	INSLIST_INSTRUMENTNAME = 0,
	INSLIST_INSTRUMENTNO,
	INSLIST_SAMPLES,
	INSLIST_ENVELOPES,
	INSLIST_FILENAME,
	INSLIST_PLUGIN,
	INSLIST_COLUMNS
};


static constexpr CListCtrlEx::Header SampleHeaders[SMPLIST_COLUMNS] =
{
	{ UL_("Sample Name"), 212, mfcless::list::AlignLeft },
	{ UL_("Num"),         45,  mfcless::list::AlignRight },
	{ UL_("Size"),        72,  mfcless::list::AlignRight },
	{ UL_("Type"),        80,  mfcless::list::AlignRight },
	{ UL_("C-5 Freq"),    80,  mfcless::list::AlignRight },
	{ UL_("Instr"),       64,  mfcless::list::AlignRight },
	{ UL_("File Name"),   160, mfcless::list::AlignRight },
	{ UL_("Path"),        256, mfcless::list::AlignLeft },
};

static constexpr CListCtrlEx::Header InstrumentHeaders[INSLIST_COLUMNS] =
{
	{ UL_("Instrument Name"), 212, mfcless::list::AlignLeft },
	{ UL_("Num"),             45,  mfcless::list::AlignRight },
	{ UL_("Samples"),         64,  mfcless::list::AlignRight },
	{ UL_("Envelopes"),       128, mfcless::list::AlignRight },
	{ UL_("File Name"),       160, mfcless::list::AlignRight },
	{ UL_("Plugin"),          128, mfcless::list::AlignRight },
};


const mfcless::ViewClass CViewComments::classCViewComments{"CViewComments", [](QWidget *parent) -> mfcless::DocumentView * { auto *view = new CViewComments(); if(parent) view->setParent(parent); return view; }};
void CViewComments::RegisterMessageMap()
{
	CModScrollView::RegisterMessageMap();
	// ON_WM_SIZE() (mfcless::Window)
	// ON_WM_DESTROY(): by hand (Qt event override)
	AddMessage(WM_MOD_KEYCOMMAND, &CViewComments::OnCustomKeyMsg);
	AddMessage(WM_MOD_MIDIMSG, &CViewComments::OnMidiMsg);
	AddCommand(IDC_LIST_SAMPLES, &CViewComments::OnShowSamples);
	AddCommand(IDC_LIST_INSTRUMENTS, &CViewComments::OnShowInstruments);
	AddCommand(IDC_LIST_PATTERNS, &CViewComments::OnShowPatterns);
	AddCommand(ID_EDIT_COPY, &CViewComments::OnCopyNames);
	AddCommand(ID_EDIT_PASTE, &CViewComments::OnPasteNames);
	AddNotify(IDC_LIST_DETAILS, mfcless::Notify::EndLabelEdit, &CViewComments::OnEndLabelEdit);
	AddNotify(IDC_LIST_DETAILS, mfcless::Notify::BeginLabelEdit, &CViewComments::OnBeginLabelEdit);
	AddNotify(IDC_LIST_DETAILS, mfcless::Notify::DoubleClicked, &CViewComments::OnDblClickListItem);
	AddNotify(IDC_LIST_DETAILS, mfcless::Notify::RightClick, &CViewComments::OnRClickListItem);
	// ON_NOTIFY(NM_CUSTOMDRAW,      IDC_LIST_DETAILS, &CViewComments::OnCustomDrawList): by hand (Qt event override)
}


void CViewComments::OnInitialUpdate()
{
	CModScrollView::OnInitialUpdate();
	if(m_nListId == 0)
	{
		m_nListId = IDC_LIST_SAMPLES;

		// For XM, set the instrument list as the default list
		const CModDoc *pModDoc = GetDocument();
		if(pModDoc && pModDoc->GetSoundFile().GetMessageHeuristic() == ModMessageHeuristicOrder::InstrumentsSamples && pModDoc->GetNumInstruments() > 0)
		{
			m_nListId = IDC_LIST_INSTRUMENTS;
		}
	}

	CChildFrame *pFrame = (CChildFrame *)GetParentFrame();
	mfcless::Rect rect;

	if (pFrame)
	{
		CommentsViewState &commentState = pFrame->GetCommentViewState();
		if (commentState.initialized)
		{
			m_nListId = commentState.nId;
		}
	}
	GetClientRect(&rect);
	m_ToolBar = new CModControlBar(ClientWidget());
	m_ToolBar->setGeometry(QRect(rect));
	m_ToolBar->SetCommandTarget(this);
	m_ToolBar->Init(CMainFrame::GetMainFrame()->m_MiscIcons, CMainFrame::GetMainFrame()->m_MiscIconsDisabled);
	m_ToolBar->show();
	m_ItemList = new CListCtrlEx(ClientWidget());
	m_ItemList->setGeometry(QRect(rect));
	m_ItemList->setSelectionMode(QAbstractItemView::SingleSelection);
	m_ItemList->header()->setSectionsClickable(false);
	m_ItemList->EnableLabelEdit(true);
	mfcless::ConnectNotifications(m_ItemList, IDC_LIST_DETAILS, this);
	m_ItemList->show();
	m_ItemList->SetExtendedStyle(m_ItemList->GetExtendedStyle() | mfcless::list::FullRowSelect);

	// Add ToolBar Buttons
	m_ToolBar->AddButton(IDC_LIST_SAMPLES, IMAGE_SAMPLES);
	m_ToolBar->AddButton(IDC_LIST_INSTRUMENTS, IMAGE_INSTRUMENTS);
	//m_ToolBar->AddButton(IDC_LIST_PATTERNS, TIMAGE_TAB_PATTERNS);
	UpdateButtonState();
	OnDPIChanged();
}


void CViewComments::OnDPIChanged()
{
	UpdateView(GeneralHint().MPTOptions().ModType());
	m_ToolBar->OnDPIChanged();
	RecalcLayout();
	CModScrollView::OnDPIChanged();
}


// cppcheck-suppress duplInheritedMember
void CViewComments::OnDestroy()
{
	if(m_lastNote != NOTE_NONE)
		GetDocument()->NoteOff(m_lastNote, true, m_noteInstr, m_noteChannel);

	CChildFrame *pFrame = (CChildFrame *)GetParentFrame();
	if (pFrame)
	{
		CommentsViewState &commentState = pFrame->GetCommentViewState();
		commentState.initialized = true;
		commentState.nId = m_nListId;
	}
	CModScrollView::OnDestroy();
}


intptr_t CViewComments::OnModViewMsg(uintptr_t wParam, intptr_t lParam)
{
	switch(wParam)
	{
		case VIEWMSG_SETFOCUS:
		case VIEWMSG_SETACTIVE:
			GetParentFrame()->SetActiveView(this);
			m_ItemList->setFocus();
			return 0;
		default:
			return CModScrollView::OnModViewMsg(wParam, lParam);
	}
}


intptr_t CViewComments::OnMidiMsg(uintptr_t midiData_, intptr_t)
{
	uint32 midiData = static_cast<uint32>(midiData_);
	INSTRUMENTINDEX ins = 0;
	SAMPLEINDEX smp = 0;

	const int item = m_ItemList->GetSelectionMark() + 1;
	if(item > 0 && m_nListId == IDC_LIST_SAMPLES)
		smp = static_cast<SAMPLEINDEX>(item);
	else if(item > 0 && m_nListId == IDC_LIST_INSTRUMENTS)
		ins = static_cast<INSTRUMENTINDEX>(item);

	GetDocument()->ProcessMIDI(midiData, smp, ins, GetDocument()->GetSoundFile().GetInstrumentPlugin(ins), kCtxViewComments);
	return 1;
}


intptr_t CViewComments::OnCustomKeyMsg(uintptr_t wParam, intptr_t)
{
	const int item = m_ItemList->GetSelectionMark() + 1;
	if(item == 0)
		return kcNull;

	auto modDoc = GetDocument();
	if(wParam >= kcCommentsStartNotes && wParam <= kcCommentsEndNotes)
	{
		const auto lastInstr = m_noteInstr;
		m_noteInstr = (m_nListId == IDC_LIST_SAMPLES) ? INSTRUMENTINDEX_INVALID : static_cast<INSTRUMENTINDEX>(item);
		const auto note = modDoc->GetNoteWithBaseOctave(static_cast<int>(wParam - kcCommentsStartNotes), m_noteInstr);
		PlayNoteParam params(note);
		if(m_nListId == IDC_LIST_SAMPLES)
			params.Sample(static_cast<SAMPLEINDEX>(item));
		else if(m_nListId == IDC_LIST_INSTRUMENTS)
			params.Instrument(m_noteInstr);
		else
			return kcNull;
		if(m_lastNote != NOTE_NONE)
			modDoc->NoteOff(m_lastNote, true, lastInstr, m_noteChannel);
		m_noteChannel = modDoc->PlayNote(params);
		m_lastNote = note;
		return wParam;
	} else if(wParam >= kcCommentsStartNoteStops && wParam <= kcCommentsEndNoteStops)
	{
		const auto note = modDoc->GetNoteWithBaseOctave(static_cast<int>(wParam - kcCommentsStartNoteStops), m_noteInstr);
		modDoc->NoteOff(note, false, m_noteInstr, m_noteChannel);
		return wParam;
	} else if(wParam == kcToggleSmpInsList)
	{
		bool ok = SwitchToList(m_nListId == IDC_LIST_SAMPLES ? IDC_LIST_INSTRUMENTS : IDC_LIST_SAMPLES);
		if(ok)
		{
			int newItem = 0;
			switch(m_nListId)
			{
			case IDC_LIST_SAMPLES:
				// Switch to a sample belonging to previously selected instrument
				if(SAMPLEINDEX smp = modDoc->FindInstrumentChild(static_cast<INSTRUMENTINDEX>(item)); smp != 0 && smp != SAMPLEINDEX_INVALID)
					newItem = smp - 1;
				break;

			case IDC_LIST_INSTRUMENTS:
				// Switch to parent instrument of previously selected sample
				if(INSTRUMENTINDEX ins = modDoc->FindSampleParent(static_cast<SAMPLEINDEX>(item)); ins != 0 && ins != INSTRUMENTINDEX_INVALID)
					newItem = ins - 1;
				break;
			}
			m_ItemList->SetItemState(newItem, mfcless::list::Selected | mfcless::list::Focused, mfcless::list::Selected | mfcless::list::Focused);
			m_ItemList->SetSelectionMark(newItem);
			m_ItemList->EnsureVisible(newItem, false);
			m_ItemList->setFocus();
		}
		return wParam;
	} else if(wParam == kcExecuteSmpInsListItem)
	{
		OnDblClickListItem();
		return wParam;
	} else if(wParam == kcRenameSmpInsListItem)
	{
		m_ItemList->EditLabel(item - 1);
		return wParam;
	} else if(wParam == kcEditCopy)
	{
		OnCopyNames();
		return wParam;
	} else if(wParam == kcEditPaste)
	{
		OnPasteNames();
		return wParam;
	}
	return kcNull;
}


bool CViewComments::PreTranslateMessage(mfcless::Message *pMsg)
{
	if(pMsg)
	{
		if(m_editLabel && pMsg->message == mfcless::msg::KeyDown && pMsg->wParam == mfcless::vk::ESCAPE)
		{
			m_editLabel = false;
			m_ItemList->setFocus();
			return true;
		}

		if((pMsg->message == mfcless::msg::SysKeyUp) || (pMsg->message == mfcless::msg::KeyUp)
			|| (pMsg->message == mfcless::msg::SysKeyDown) || (pMsg->message == mfcless::msg::KeyDown))
		{
			CInputHandler *ih = CMainFrame::GetInputHandler();
			if(!ih->IsBypassed() && ih->KeyEvent(kCtxViewComments, ih->Translate(*pMsg)) != kcNull)
			{
				return true;  // Mapped to a command, no need to pass message on.
			}
		} else if(pMsg->message == mfcless::msg::Char)
		{
			// Avoid Windows warning sound when holding note key
			if(!CMainFrame::GetInputHandler()->IsBypassed() && CInputHandler::GetKeyEventType(*pMsg) == kKeyEventRepeat)
				return true;
		}
	}

	return CModScrollView::PreTranslateMessage(pMsg);
}


///////////////////////////////////////////////////////////////
// CViewComments drawing

void CViewComments::UpdateView(UpdateHint hint, QObject *)
{
	const CModDoc *pModDoc = GetDocument();

	if ((!pModDoc) || (!(m_ItemList))) return;
	const FlagSet<HintType> hintType = hint.GetType();
	if (hintType[HINT_MPTOPTIONS])
	{
		m_ToolBar->UpdateStyle();
		
		// Font for sample / instrument names
		m_fixedFont = m_ItemList->font();
		m_fixedFont.setFamily(mfcless::ToQString(TrackerSettings::Instance().commentsFont.Get().name));

		m_ItemList->update();
	}
	const SampleHint sampleHint = hint.ToType<SampleHint>();
	const InstrumentHint instrHint = hint.ToType<InstrumentHint>();
	const bool updateSamples = sampleHint.GetType()[HINT_SMPNAMES | HINT_SAMPLEINFO];
	const bool updateInstr = instrHint.GetType()[HINT_INSNAMES|HINT_INSTRUMENT];
	bool updateAll = hintType[HINT_MODTYPE];

	if(!updateSamples && !updateInstr && !updateAll) return;

	const CSoundFile &sndFile = pModDoc->GetSoundFile();

	m_ToolBar->ChangeBitmap(IDC_LIST_INSTRUMENTS, sndFile.GetNumInstruments() ? IMAGE_INSTRUMENTS : IMAGE_INSTRMUTE);

	mpt::ustring s;

	m_ItemList->setUpdatesEnabled(false);
	// Add sample headers
	if (m_nListId != m_nCurrentListId || updateAll)
	{
		uint32 ichk = 0;
		m_ItemList->DeleteAllItems();
		while ((m_ItemList->DeleteColumn(0)) && (ichk < 25)) ichk++;
		m_nCurrentListId = m_nListId;
		if (m_nCurrentListId == IDC_LIST_SAMPLES)
		{
			// Add Sample Headers
			m_ItemList->SetHeaders(SampleHeaders);
		} else if (m_nCurrentListId == IDC_LIST_INSTRUMENTS)
		{
			// Add Instrument Headers
			m_ItemList->SetHeaders(InstrumentHeaders);
		} else
		updateAll = true;
	}
	// Add Items
	uint32 nCount = m_ItemList->GetItemCount();
	// Add Samples
	if (m_nCurrentListId == IDC_LIST_SAMPLES && (updateAll || updateSamples))
	{
		SAMPLEINDEX nMax = static_cast<SAMPLEINDEX>(nCount);
		if (nMax < sndFile.GetNumSamples()) nMax = sndFile.GetNumSamples();
		for (SAMPLEINDEX iSmp = 0; iSmp < nMax; iSmp++)
		{
			if (iSmp < sndFile.GetNumSamples())
			{
				uint32 nCol = 0;
				for (uint32 iCol=0; iCol<SMPLIST_COLUMNS; iCol++)
				{
					const ModSample &sample = sndFile.GetSample(iSmp + 1);
					s.clear();
					switch(iCol)
					{
					case SMPLIST_SAMPLENAME:
						s = mpt::ToUnicode(sndFile.GetCharsetInternal(), sndFile.m_szNames[iSmp + 1]);
						break;
					case SMPLIST_SAMPLENO:
						s = mpt::ufmt::dec0<2>(iSmp + 1);
						break;
					case SMPLIST_SIZE:
						if(sample.nLength && !sample.uFlags[CHN_ADLIB])
							s = FormatFileSize(sample.GetSampleSizeInBytes());
						break;
					case SMPLIST_TYPE:
						if(sample.uFlags[CHN_ADLIB])
							s = UL_("OPL");
						else if(sample.HasSampleData())
							s = MPT_UFORMAT("{}-bit {}")(sample.GetElementarySampleSize() * 8, (sample.GetNumChannels() == 2) ? mpt::ustring(UL_("stereo")) : mpt::ustring(UL_("mono")));
						break;
					case SMPLIST_INSTR:
						if (sndFile.GetNumInstruments())
						{
							bool first = true;
							for (INSTRUMENTINDEX i = 1; i <= sndFile.GetNumInstruments(); i++)
							{
								if (sndFile.IsSampleReferencedByInstrument(iSmp + 1, i))
								{
									if (!first) s.push_back(u8',');
									first = false;

									s += mfcless::PrintF(UL_("%u"), i);
								}
							}
						}
						break;
					case SMPLIST_MIDDLEC:
						if (sample.nLength)
						{
							s = mfcless::PrintF(UL_("%u Hz"), sample.GetSampleRate(sndFile.GetType()));
						}
						break;
					case SMPLIST_FILENAME:
						s = mpt::ToUnicode(sndFile.GetCharsetInternal(), sample.filename);
						break;
					case SMPLIST_PATH:
						s = sndFile.GetSamplePath(iSmp + 1).ToUnicode();
						break;
					}
					if ((iCol) || (iSmp < nCount))
					{
						bool update = true;
						if (iSmp < nCount)
						{
							mpt::ustring stmp = mfcless::FromQString(m_ItemList->GetItemText(iSmp, nCol));
							if (s == stmp) update = false;
						}
						if (update) m_ItemList->SetItemText(iSmp, nCol, s);
					} else
					{
						m_ItemList->InsertItem(iSmp, s);
					}
					nCol++;
				}
			} else
			{
				m_ItemList->DeleteItem(iSmp);
			}
		}
	} else
	// Add Instruments
	if ((m_nCurrentListId == IDC_LIST_INSTRUMENTS) && (updateAll || updateInstr))
	{
		INSTRUMENTINDEX nMax = static_cast<INSTRUMENTINDEX>(nCount);
		if (nMax < sndFile.GetNumInstruments()) nMax = sndFile.GetNumInstruments();
		for (INSTRUMENTINDEX iIns = 0; iIns < nMax; iIns++)
		{
			if (iIns < sndFile.GetNumInstruments())
			{
				uint32 nCol = 0;
				for (uint32 iCol=0; iCol<INSLIST_COLUMNS; iCol++)
				{
					ModInstrument *pIns = sndFile.Instruments[iIns+1];
					s.clear();
					switch(iCol)
					{
					case INSLIST_INSTRUMENTNAME:
						if (pIns) s = mpt::ToUnicode(sndFile.GetCharsetInternal(), pIns->name);
						break;
					case INSLIST_INSTRUMENTNO:
						s = mpt::ufmt::dec0<2>(iIns + 1);
						break;
					case INSLIST_SAMPLES:
						if (pIns)
						{
							bool first = true;
							for(auto sample : pIns->GetSamples())
							{
								if(!first) s.push_back(u8',');
								first = false;
								s += mfcless::PrintF(UL_("%u"), sample);
							}
						}
						break;
					case INSLIST_ENVELOPES:
						if (pIns)
						{
							if (pIns->VolEnv.dwFlags[ENV_ENABLED]) s += UL_("Vol");
							if (pIns->PanEnv.dwFlags[ENV_ENABLED]) { if (!s.empty()) s += UL_(", "); s += UL_("Pan"); }
							if (pIns->PitchEnv.dwFlags[ENV_ENABLED]) { if (!s.empty()) s += UL_(", "); s += (pIns->PitchEnv.dwFlags[ENV_FILTER] ? UL_("Filter") : UL_("Pitch")); }
						}
						break;
					case INSLIST_FILENAME:
						if (pIns)
						{
							s = mpt::ToUnicode(sndFile.GetCharsetInternal(), pIns->filename);
						}
						break;
					case INSLIST_PLUGIN:
						if (pIns != nullptr && pIns->nMixPlug > 0 && sndFile.m_MixPlugins[pIns->nMixPlug - 1].IsValidPlugin())
						{
							s = mfcless::PrintF(UL_("FX%02u: "), pIns->nMixPlug);
							s += mpt::ToUnicode(sndFile.m_MixPlugins[pIns->nMixPlug - 1].GetLibraryName());
						}
						break;
					}
					if ((iCol) || (iIns < nCount))
					{
						bool update = true;
						if (iIns < nCount)
						{
							mpt::ustring stmp = mfcless::FromQString(m_ItemList->GetItemText(iIns, nCol));
							if (s == stmp) update = false;
						}
						if (update) m_ItemList->SetItemText(iIns, nCol, s);
					} else
					{
						m_ItemList->InsertItem(iIns, s);
					}
					nCol++;
				}
			} else
			{
				m_ItemList->DeleteItem(iIns);
			}
		}
	} else
	// Add Patterns
	//if ((m_nCurrentListId == IDC_LIST_PATTERNS) && (hintType & (HINT_MODTYPE|HINT_PATNAMES|HINT_PATTERNROW)))
	{
	}
	OnCustomDrawList();
	m_ItemList->setUpdatesEnabled(true);
}


void CViewComments::RecalcLayout()
{
	mfcless::Rect rect;

	if (!this) return;
	GetClientRect(&rect);
	const int toolbarHeight = std::max(DETAILS_TOOLBAR_CY, m_ToolBar->sizeHint().height());
	m_ToolBar->setGeometry(0, 0, rect.Width(), toolbarHeight);
	m_ItemList->setGeometry(-1, toolbarHeight, rect.Width()+2, rect.Height() - toolbarHeight + 1);
}


void CViewComments::UpdateButtonState()
{
	const CModDoc *pModDoc = GetDocument();
	if (pModDoc)
	{
		m_ToolBar->SetState(IDC_LIST_SAMPLES, ((m_nListId == IDC_LIST_SAMPLES) ? CModControlBar::StateChecked : 0)|CModControlBar::StateEnabled);
		m_ToolBar->SetState(IDC_LIST_INSTRUMENTS, ((m_nListId == IDC_LIST_INSTRUMENTS) ? CModControlBar::StateChecked : 0)|CModControlBar::StateEnabled);
		m_ToolBar->SetState(IDC_LIST_PATTERNS, ((m_nListId == IDC_LIST_PATTERNS) ? CModControlBar::StateChecked : 0)|CModControlBar::StateEnabled);
		m_ToolBar->EnableButton(IDC_LIST_INSTRUMENTS, (pModDoc->GetNumInstruments()) ? true : false);
	}
}


void CViewComments::OnBeginLabelEdit()
{
	QLineEdit *editCtrl = m_ItemList->GetEditControl();
	if(editCtrl)
	{
		m_editLabel = true;
		const CModSpecifications &specs = GetDocument()->GetSoundFile().GetModSpecifications();
		const auto maxStrLen = (m_nListId == IDC_LIST_SAMPLES) ? specs.sampleNameLengthMax : specs.instrNameLengthMax;
		editCtrl->setMaxLength(maxStrLen);
		CMainFrame::GetInputHandler()->Bypass(true);
	}
}


void CViewComments::OnEndLabelEdit()
{
	CMainFrame::GetInputHandler()->Bypass(false);
	if(!m_editLabel)
	{
		return;
	}
	m_editLabel = false;

	const mfcless::NotifyInfo &info = CurrentNotify();
	const mpt::ustring text = mfcless::FromQString(info.text);
	CModDoc *pModDoc = GetDocument();

	if(info.textValid && !info.subItem && pModDoc && info.item >= 0)
	{
		uint32 iItem = static_cast<uint32>(info.item);
		CSoundFile &sndFile = pModDoc->GetSoundFile();

		if(m_nListId == IDC_LIST_SAMPLES)
		{
			if(iItem < sndFile.GetNumSamples())
			{
				sndFile.m_szNames[iItem + 1] = mpt::ToCharset(sndFile.GetCharsetInternal(), text);
				pModDoc->UpdateAllViews(this, SampleHint(static_cast<SAMPLEINDEX>(iItem + 1)).Info().Names(), this);
				pModDoc->SetModified();
			}
		} else if(m_nListId == IDC_LIST_INSTRUMENTS)
		{
			if((iItem < sndFile.GetNumInstruments()) && (sndFile.Instruments[iItem + 1]))
			{
				ModInstrument *pIns = sndFile.Instruments[iItem + 1];
				pIns->name = mpt::ToCharset(sndFile.GetCharsetInternal(), text);
				pModDoc->UpdateAllViews(this, InstrumentHint(static_cast<INSTRUMENTINDEX>(iItem + 1)).Info().Names(), this);
				pModDoc->SetModified();
			}
		} else
		{
			return;
		}
		m_ItemList->SetItemText(iItem, 0, text);
	}
}


///////////////////////////////////////////////////////////////
// CViewComments messages


void CViewComments::OnSize(uint32 nType, int cx, int cy)
{
	CModScrollView::OnSize(nType, cx, cy);
	if (((nType == mfcless::size::Restored) || (nType == mfcless::size::Maximized)) && (cx > 0) && (cy > 0) && (this))
	{
		RecalcLayout();
	}
}


void CViewComments::OnShowSamples() { SwitchToList(IDC_LIST_SAMPLES); }
void CViewComments::OnShowInstruments() { SwitchToList(IDC_LIST_INSTRUMENTS); }
void CViewComments::OnShowPatterns() { SwitchToList(IDC_LIST_PATTERNS); }


bool CViewComments::SwitchToList(int list)
{
	if(list == m_nListId)
		return false;

	if(list == IDC_LIST_SAMPLES)
	{
		m_nListId = IDC_LIST_SAMPLES;
		UpdateButtonState();
		UpdateView(UpdateHint().ModType());
	} else if(list == IDC_LIST_INSTRUMENTS)
	{
		const CModDoc *modDoc = GetDocument();
		if(!modDoc || !modDoc->GetNumInstruments())
			return false;

		m_nListId = IDC_LIST_INSTRUMENTS;
		UpdateButtonState();
		UpdateView(UpdateHint().ModType());
	/*} else if(list == IDC_LIST_PATTERNS)
	{
		m_nListId = IDC_LIST_PATTERNS;
		UpdateButtonState();
		UpdateView(UpdateHint().ModType());*/
	} else
	{
		return false;
	}
	return true;
}


void CViewComments::OnDblClickListItem()
{
	// Double click -> switch to instrument or sample tab
	int nItem = m_ItemList->GetSelectionMark();
	if(nItem == -1) return;
	CModDoc *pModDoc = GetDocument();
	if(!pModDoc) return;
	nItem++;

	switch(m_nListId)
	{
	case IDC_LIST_SAMPLES:
		pModDoc->ViewSample(nItem);
		break;
	case IDC_LIST_INSTRUMENTS:
		pModDoc->ViewInstrument(nItem);
		break;
	case IDC_LIST_PATTERNS:
		pModDoc->ViewPattern(nItem, 0);
		break;
	}
}


void CViewComments::OnRClickListItem()
{
	const auto ih = CMainFrame::GetMainFrame()->GetInputHandler();
	mfcless::Menu menu = mfcless::Menu::CreatePopup();
	menu.AppendMenu(mfcless::menu::String, ID_EDIT_COPY, ih->GetKeyTextFromCommand(kcEditCopy, UL_("&Copy Names")));
	menu.AppendMenu(mfcless::menu::String | (mfcless::clip::IsFormatAvailable(mfcless::clip::UnicodeText) ? 0 : mfcless::menu::Disabled), ID_EDIT_PASTE, ih->GetKeyTextFromCommand(kcEditPaste, UL_("&Paste Names")));
	const QPoint cursor = QCursor::pos();
	mfcless::Point pt{cursor.x(), cursor.y()};
	menu.TrackPopupMenu(mfcless::menu::LeftAlign | mfcless::menu::RightButton, pt.x, pt.y, this);
	menu.DestroyMenu();

}


// mfcless: upstream draws the name and file name columns in the comments
// font (NM_CUSTOMDRAW); here those cells get the font when the list is filled.
void CViewComments::OnCustomDrawList()
{
	for(int i = 0; i < m_ItemList->GetItemCount(); i++)
	{
		QTreeWidgetItem *item = m_ItemList->ItemAt(i);
		if(!item)
			continue;
		for(int col = 0; col < m_ItemList->GetColumnCount(); col++)
		{
			const bool useFont = col == 0 || (m_nListId == IDC_LIST_SAMPLES && col == SMPLIST_FILENAME) || (m_nListId == IDC_LIST_INSTRUMENTS && col == INSLIST_FILENAME);
			if(useFont)
				item->setFont(col, m_fixedFont);
		}
	}
}


void CViewComments::OnCopyNames()
{
	std::wstring names;
	const CSoundFile &sndFile = GetDocument()->GetSoundFile();
	if(m_nListId == IDC_LIST_SAMPLES)
	{
		for(SAMPLEINDEX i = 1; i <= sndFile.GetNumSamples(); i++)
			names += mpt::ToWide(sndFile.GetCharsetInternal(), sndFile.GetSampleName(i)) + L"\r\n";
	} else if(m_nListId == IDC_LIST_INSTRUMENTS)
	{
		for(INSTRUMENTINDEX i = 1; i <= sndFile.GetNumInstruments(); i++)
			names += mpt::ToWide(sndFile.GetCharsetInternal(), sndFile.GetInstrumentName(i)) + L"\r\n";
	}
	const size_t sizeBytes = (names.length() + 1) * sizeof(wchar_t);
	Clipboard clipboard(mfcless::clip::UnicodeText, sizeBytes);
	if(auto dst = clipboard.Get(); dst.data())
	{
		std::memcpy(dst.data(), names.c_str(), sizeBytes);
	}
}


void CViewComments::OnPasteNames()
{
	Clipboard clipboard(mfcless::clip::UnicodeText);
	if(!clipboard.IsValid())
		return;

	if(Reporting::Confirm(MPT_UFORMAT("Replace all {} names?")(m_nListId == IDC_LIST_INSTRUMENTS ? U_("instrument") : U_("sample"))) != cnfYes)
		return;

	auto whitespace = mpt::default_whitespace<std::wstring>();
	whitespace.push_back(L'\0');
	const auto names = mpt::split(mpt::trim_right(std::wstring{clipboard.GetWideString()}, whitespace), std::wstring{L"\n"});

	CSoundFile &sndFile = GetDocument()->GetSoundFile();
	const auto FormatName = [&](size_t index, size_t maxLength)
	{
		if(index >= names.size())
			return std::string{};
		return mpt::replace(mpt::ToCharset(sndFile.GetCharsetInternal(), names[index]), "\t", " ").substr(0, maxLength);
	};

	CriticalSection cs;
	if(m_nListId == IDC_LIST_SAMPLES)
	{
		if(sndFile.GetNumSamples() < names.size())
			sndFile.m_nSamples = std::min(sndFile.GetModSpecifications().samplesMax, mpt::saturate_cast<SAMPLEINDEX>(names.size()));

		for(SAMPLEINDEX i = 1; i <= sndFile.GetNumSamples(); i++)
		{
			sndFile.m_szNames[i] = FormatName(i - 1, sndFile.GetModSpecifications().sampleNameLengthMax);
		}
		cs.Leave();
		GetDocument()->UpdateAllViews(SampleHint().Names());
	} else if(m_nListId == IDC_LIST_INSTRUMENTS)
	{
		if(sndFile.GetNumInstruments() < names.size())
			sndFile.m_nInstruments = std::min(sndFile.GetModSpecifications().instrumentsMax, mpt::saturate_cast<INSTRUMENTINDEX>(names.size()));

		for(INSTRUMENTINDEX i = 1; i <= sndFile.GetNumInstruments(); i++)
		{
			if(sndFile.Instruments[i] || sndFile.AllocateInstrument(i))
				sndFile.Instruments[i]->name = FormatName(i - 1, sndFile.GetModSpecifications().instrNameLengthMax);
		}
		cs.Leave();
		GetDocument()->UpdateAllViews(InstrumentHint().Names());
	}

	GetDocument()->SetModified();
}


OPENMPT_NAMESPACE_END
