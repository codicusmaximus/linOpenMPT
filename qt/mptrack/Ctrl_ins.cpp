/*
 * Ctrl_ins.cpp
 * ------------
 * Purpose: Instrument tab, upper panel.
 * Notes  : mfcless twin of upstream's mptrack/Ctrl_ins.cpp.
 *          Draft from qt/tools/twin.py, not yet edited by hand.
 * Authors: Olivier Lapicque
 *          OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "Ctrl_ins.h"
#include "mfcless/Widgets.h"
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
#include "Reporting.h"
#include "resource.h"
#include "SelectPluginDialog.h"
#include "TrackerSettings.h"
#include "TuningDialog.h"
#include "View_ins.h"
#include "WindowMessages.h"
#include "../common/FileReader.h"
#include "../common/misc_util.h"
#include "../common/mptFileIO.h"
#include "../common/mptStringBuffer.h"
#include "../soundlib/mod_specifications.h"
#include "../soundlib/plugins/PlugInterface.h"
#include "mpt/base/saturate_round.hpp"
#include "mpt/io_file/inputfile.hpp"
#include "mpt/io_file_read/inputfile_filecursor.hpp"
#include "mpt/io_file/outputfile.hpp"
#include "mpt/string/utility.hpp"


OPENMPT_NAMESPACE_BEGIN


/////////////////////////////////////////////////////////////////////////
// CNoteMapWnd

void CNoteMapWnd::RegisterMessageMap()
{
	mfcless::Window<QLabel>::RegisterMessageMap();
	// ON_WM_ERASEBKGND(): by hand (Qt event override)
	// ON_WM_PAINT() (mfcless::Window)
	// ON_WM_SETFOCUS() (mfcless::Window)
	// ON_WM_KILLFOCUS() (mfcless::Window)
	// ON_WM_LBUTTONDOWN() (mfcless::Window)
	// ON_WM_MBUTTONDOWN() (mfcless::Window)
	// ON_WM_RBUTTONUP() (mfcless::Window)
	// ON_WM_LBUTTONDBLCLK() (mfcless::Window)
	// ON_WM_MOUSEWHEEL() (mfcless::Window)
	AddCommand(ID_NOTEMAP_TRANS_UP, &CNoteMapWnd::OnMapTransposeUp);
	AddCommand(ID_NOTEMAP_TRANS_DOWN, &CNoteMapWnd::OnMapTransposeDown);
	AddCommand(ID_NOTEMAP_COPY_NOTE, &CNoteMapWnd::OnMapCopyNote);
	AddCommand(ID_NOTEMAP_COPY_SMP, &CNoteMapWnd::OnMapCopySample);
	AddCommand(ID_NOTEMAP_RESET, &CNoteMapWnd::OnMapReset);
	AddCommand(ID_NOTEMAP_TRANSPOSE_SAMPLES, &CNoteMapWnd::OnTransposeSamples);
	AddCommand(ID_NOTEMAP_REMOVE, &CNoteMapWnd::OnMapRemove);
	AddCommand(ID_INSTRUMENT_SAMPLEMAP, &CNoteMapWnd::OnEditSampleMap);
	AddCommand(ID_INSTRUMENT_DUPLICATE, &CNoteMapWnd::OnInstrumentDuplicate);
	AddMessage(WM_MOD_KEYCOMMAND, &CNoteMapWnd::OnCustomKeyMsg);
	AddMessage(WM_DPICHANGED_AFTERPARENT, &CNoteMapWnd::OnDPIChangedAfterParent);
	AddCommandRange(ID_NOTEMAP_EDITSAMPLE, ID_NOTEMAP_EDITSAMPLE + MAX_SAMPLES, &CNoteMapWnd::OnEditSample);
}


bool CNoteMapWnd::PreTranslateMessage(mfcless::Message *pMsg)
{
	//We handle keypresses before Windows has a chance to handle them (for alt etc..)
	if ((pMsg->message == mfcless::msg::SysKeyUp)   || (pMsg->message == mfcless::msg::KeyUp) ||
		(pMsg->message == mfcless::msg::SysKeyDown) || (pMsg->message == mfcless::msg::KeyDown))
	{
		CInputHandler *ih = CMainFrame::GetInputHandler();
		const auto event = ih->Translate(*pMsg);

		if (ih->KeyEvent(kCtxInsNoteMap, event, this) != kcNull)
			return true; // Mapped to a command, no need to pass message on.

		// a bit of a hack...
		if (ih->KeyEvent(kCtxCtrlInstruments, event, this) != kcNull)
			return true; // Mapped to a command, no need to pass message on.

		// For context menu shortcut
		if(ih->KeyEvent(kCtxAllContexts, event, this) != kcNull)
			return true;  // Mapped to a command, no need to pass message on.
	}

	//The key was not handled by a command, but it might still be useful
	uint32 wParam = static_cast<uint32>(pMsg->wParam);
	if(pMsg->message == mfcless::msg::Char && CInputHandler::GetKeyEventType(*pMsg) == kKeyEventDown)  // Key is a character
	{
		if(HandleChar(wParam))
			return true;
	} else if(pMsg->message == mfcless::msg::KeyDown)  // Key is not a character
	{
		if(HandleNav(wParam))
			return true;
	} else if(pMsg->message == mfcless::msg::KeyUp)  // Stop notes on key release
	{
		if(((pMsg->wParam >= '0') && (pMsg->wParam <= '9')) || (pMsg->wParam == ' ') ||
			((pMsg->wParam >= mfcless::vk::NUMPAD0) && (pMsg->wParam <= mfcless::vk::NUMPAD9)))
		{
			StopNote();
			return true;
		}
	}

	return WindowBase::PreTranslateMessage(pMsg);
}


intptr_t CNoteMapWnd::OnDPIChangedAfterParent(uintptr_t, intptr_t)
{
	auto result = Default();
	m_cxFont = m_cyFont = 0;
	return result;
}


void CNoteMapWnd::PrepareUndo(const char *description)
{
	m_modDoc.GetInstrumentUndo().PrepareUndo(m_nInstrument, description);
}


void CNoteMapWnd::SetCurrentInstrument(INSTRUMENTINDEX nIns)
{
	if (nIns != m_nInstrument)
	{
		if (nIns < MAX_INSTRUMENTS) m_nInstrument = nIns;

		// create missing instrument if needed
		CSoundFile &sndFile = m_modDoc.GetSoundFile();
		if(m_nInstrument > 0 && m_nInstrument <= sndFile.GetNumInstruments() && sndFile.Instruments[m_nInstrument] == nullptr)
		{
			ModInstrument *instrument = sndFile.AllocateInstrument(m_nInstrument);
			if(instrument == nullptr)
				return;
			m_modDoc.InitializeInstrument(instrument);
		}

		Invalidate(false);
		UpdateAccessibleTitle();
	}
}


void CNoteMapWnd::SetCurrentNote(uint32 nNote)
{
	if(nNote != m_nNote && ModCommand::IsNote(static_cast<ModCommand::NOTE>(nNote + NOTE_MIN)))
	{
		m_nNote = nNote;
		Invalidate(false);
		UpdateAccessibleTitle();
	}
}


void CNoteMapWnd::OnPaint()
{
	mfcless::PaintDC dc(this);

	mfcless::Rect rcClient;
	GetClientRect(&rcClient);
	const auto highlightBrush = GetSysColorBrush(COLOR_HIGHLIGHT), windowBrush = GetSysColorBrush(COLOR_WINDOW);
	const auto colorText = mfcless::SystemColor(mfcless::SysColor::WindowText);
	const auto colorTextSel = mfcless::SystemColor(mfcless::SysColor::HighlightText);
	const int lineWidth = HighDPISupport::ScalePixels(1, *this);
	auto oldFont = dc.SelectObject(CMainFrame::GetGUIFont());
	dc.SetBkMode(mfcless::bk::Transparent);
	if ((m_cxFont <= 0) || (m_cyFont <= 0))
	{
		mfcless::Size sz;
		sz = dc.GetTextExtent(UL_("C#0."), 4);
		m_cyFont = sz.cy + 2;
		m_cxFont = rcClient.right / 3;
	}
	dc.IntersectClipRect(&rcClient);

	const CSoundFile &sndFile = m_modDoc.GetSoundFile();
	const auto &modSpecs = sndFile.GetModSpecifications();
	int noteMin = 0, noteMax = NOTE_MAX - NOTE_MIN;
	if(modSpecs.instrumentsMax)
	{
		noteMin = modSpecs.noteMin - NOTE_MIN;
		noteMax = modSpecs.noteMax - NOTE_MIN;
	}
	if (m_cxFont > 0 && m_cyFont > 0)
	{
		const bool focus = (mfcless::GetFocus() == this);
		const ModInstrument *pIns = sndFile.Instruments[m_nInstrument];
		mfcless::Rect rect;

		int nNotes = (rcClient.bottom + m_cyFont - 1) / m_cyFont;
		int nPos = m_nNote - (nNotes/2);
		int ypaint = 0;
		mpt::ustring s;
		for(int ynote = 0; ynote < nNotes; ynote++, ypaint += m_cyFont, nPos++)
		{
			// Note
			const bool isValidPos = mpt::is_in_range(nPos, noteMin, noteMax);
			if(isValidPos)
			{
				s = mpt::ToUnicode(sndFile.GetNoteName(static_cast<ModCommand::NOTE>(nPos + 1), m_nInstrument));
				s.resize(4);
			} else
			{
				s.clear();
			}
			rect.SetRect(0, ypaint, m_cxFont, ypaint+m_cyFont);
			DrawButtonRect(dc, lineWidth, rect, s.c_str(), false, false);
			// Mapped Note
			bool highlight = ((focus) && (nPos == (int)m_nNote));
			rect.left = rect.right;
			rect.right = m_cxFont*2-1;
			s = UL_("...");
			if(pIns != nullptr && isValidPos && (pIns->NoteMap[nPos] != NOTE_NONE))
			{
				ModCommand::NOTE n = pIns->NoteMap[nPos];
				if(ModCommand::IsNote(n))
				{
					s = mpt::ToUnicode(sndFile.GetNoteName(n, m_nInstrument));
					s.resize(4);
				} else
				{
					s = UL_("???");
				}
			}
			FillRect(dc, &rect, highlight ? highlightBrush : windowBrush);
			if(nPos == (int)m_nNote && !m_bIns)
			{
				rect.InflateRect(-1, -1);
				dc.DrawFocusRect(&rect);
				rect.InflateRect(1, 1);
			}
			dc.SetTextColor(highlight ? colorTextSel : colorText);
			dc.DrawText(s.c_str(), -1, &rect, mfcless::dt::SingleLine | mfcless::dt::Center | mfcless::dt::VCenter | mfcless::dt::NoPrefix);
			// Sample
			highlight = (focus && nPos == (int)m_nNote);
			rect.left = rcClient.left + m_cxFont * 2 + 3;
			rect.right = rcClient.right;
			s = UL_(" ..");
			if(pIns && isValidPos && pIns->Keyboard[nPos])
			{
				s = mpt::ufmt::right(3, mpt::ufmt::dec(pIns->Keyboard[nPos]));
			}
			FillRect(dc, &rect, highlight ? highlightBrush : windowBrush);
			if((nPos == (int)m_nNote) && (m_bIns))
			{
				rect.InflateRect(-1, -1);
				dc.DrawFocusRect(&rect);
				rect.InflateRect(1, 1);
			}
			dc.SetTextColor((highlight) ? colorTextSel : colorText);
			dc.DrawText(s.c_str(), -1, &rect, mfcless::dt::SingleLine | mfcless::dt::Center | mfcless::dt::VCenter | mfcless::dt::NoPrefix);
		}
		rect.SetRect(rcClient.left + m_cxFont * 2 - 1, rcClient.top, rcClient.left + m_cxFont * 2 + 3, ypaint);
		DrawButtonRect(dc, lineWidth, rect, UL_(""), false, false);
		if (ypaint < rcClient.bottom)
		{
			rect.SetRect(rcClient.left, ypaint, rcClient.right, rcClient.bottom);
			FillRect(dc, &rect, GetSysColorBrush(COLOR_BTNFACE));
		}
	}
	dc.SelectObject(oldFont);
}


void CNoteMapWnd::OnSetFocus(QWidget *pOldWnd)
{
	Window::OnSetFocus(pOldWnd);
	Invalidate(false);
	m_undo = true;
}


void CNoteMapWnd::OnKillFocus(QWidget *pNewWnd)
{
	Window::OnKillFocus(pNewWnd);
	Invalidate(false);
}


void CNoteMapWnd::OnLButtonDown(uint32, mfcless::Point pt)
{
	if ((pt.x >= m_cxFont) && (pt.x < m_cxFont*2) && (m_bIns))
	{
		m_bIns = false;
		Invalidate(false);
	}
	if ((pt.x > m_cxFont*2) && (pt.x <= m_cxFont*3) && (!m_bIns))
	{
		m_bIns = true;
		Invalidate(false);
	}
	if ((pt.x >= 0) && (m_cyFont))
	{
		mfcless::Rect rcClient;
		GetClientRect(&rcClient);
		int nNotes = (rcClient.bottom + m_cyFont - 1) / m_cyFont;
		int n = (pt.y / m_cyFont) + m_nNote - (nNotes/2);
		if(n >= 0)
		{
			SetCurrentNote(n);
		}
	}
	setFocus();
}


void CNoteMapWnd::OnLButtonDblClk(uint32, mfcless::Point)
{
	// Double-click edits sample map
	OnEditSampleMap();
}


void CNoteMapWnd::OnRButtonUp(uint32, mfcless::Point pt)
{
	CInputHandler* ih = CMainFrame::GetInputHandler();

	CSoundFile &sndFile = m_modDoc.GetSoundFile();
	ModInstrument *pIns = sndFile.Instruments[m_nInstrument];
	if (pIns)
	{
		mfcless::Menu hMenu = mfcless::Menu::CreatePopup();
		mfcless::Menu hSubMenu = mfcless::Menu::CreatePopup();

		if (hMenu)
		{
			hMenu.AppendMenu(mfcless::menu::String, ID_INSTRUMENT_SAMPLEMAP, ih->GetKeyTextFromCommand(kcInsNoteMapEditSampleMap, UL_("Edit Sample &Map")));
			if (hSubMenu)
			{
				// Create sub menu with a list of all samples that are referenced by this instrument.
				for(auto sample : pIns->GetSamples())
				{
					if(sample <= sndFile.GetNumSamples())
					{
						hSubMenu.AppendMenu(mfcless::menu::String, ID_NOTEMAP_EDITSAMPLE + sample, MPT_UFORMAT("{}: {}")(sample, mpt::ToUnicode(sndFile.GetCharsetInternal(), sndFile.m_szNames[sample])));
					}
				}

				hMenu.AppendMenu(mfcless::menu::Popup, hSubMenu, ih->GetKeyTextFromCommand(kcInsNoteMapEditSample, UL_("&Edit Sample")));
				hMenu.AppendMenu(mfcless::menu::Separator, 0, NULL);
			}
			hMenu.AppendMenu(mfcless::menu::String, ID_NOTEMAP_COPY_SMP, ih->GetKeyTextFromCommand(kcInsNoteMapCopyCurrentSample, MPT_UFORMAT("Map All Notes to &Sample {}")(pIns->Keyboard[m_nNote])));

			if(sndFile.GetType() != MOD_TYPE_XM)
			{
				if(ModCommand::IsNote(pIns->NoteMap[m_nNote]))
				{
					hMenu.AppendMenu(mfcless::menu::String, ID_NOTEMAP_COPY_NOTE, ih->GetKeyTextFromCommand(kcInsNoteMapCopyCurrentNote, MPT_UFORMAT("Map All &Notes to {}")(mpt::ToUnicode(sndFile.GetNoteName(pIns->NoteMap[m_nNote], m_nInstrument)))));
				}
				hMenu.AppendMenu(mfcless::menu::String, ID_NOTEMAP_TRANS_UP, ih->GetKeyTextFromCommand(kcInsNoteMapTransposeUp, UL_("Transpose Map &Up")));
				hMenu.AppendMenu(mfcless::menu::String, ID_NOTEMAP_TRANS_DOWN, ih->GetKeyTextFromCommand(kcInsNoteMapTransposeDown, UL_("Transpose Map &Down")));
			}
			hMenu.AppendMenu(mfcless::menu::String, ID_NOTEMAP_RESET, ih->GetKeyTextFromCommand(kcInsNoteMapReset, UL_("&Reset Note Mapping")));
			hMenu.AppendMenu(mfcless::menu::String | (pIns->CanConvertToDefaultNoteMap().empty() ? mfcless::menu::Grayed : 0), ID_NOTEMAP_TRANSPOSE_SAMPLES, ih->GetKeyTextFromCommand(kcInsNoteMapTransposeSamples, UL_("&Transpose Samples / Reset Map")));
			hMenu.AppendMenu(mfcless::menu::String, ID_NOTEMAP_REMOVE, ih->GetKeyTextFromCommand(kcInsNoteMapRemove, UL_("Remo&ve All Samples")));
			hMenu.AppendMenu(mfcless::menu::String, ID_INSTRUMENT_DUPLICATE, ih->GetKeyTextFromCommand(kcInstrumentCtrlDuplicate, UL_("Duplicate &Instrument")));
			SetMenuDefaultItem(hMenu, ID_INSTRUMENT_SAMPLEMAP, false);
			ClientToScreen(&pt);
			hMenu.TrackPopupMenu(mfcless::menu::LeftAlign|mfcless::menu::RightButton, pt.x, pt.y, this);
			hMenu.DestroyMenu();
			if (hSubMenu) hSubMenu.DestroyMenu();
		}
	}
}


bool CNoteMapWnd::OnMouseWheel(uint32 nFlags, short zDelta, mfcless::Point pt)
{
	SetCurrentNote(m_nNote - mpt::signum(zDelta));
	return Window::OnMouseWheel(nFlags, zDelta, pt);
}


void CNoteMapWnd::OnMapCopyNote()
{
	ModInstrument *pIns = m_modDoc.GetSoundFile().Instruments[m_nInstrument];
	if (pIns)
	{
		m_undo = true;
		bool bModified = false;
		auto n = pIns->NoteMap[m_nNote];
		for (auto &key : pIns->NoteMap) if (key != n)
		{
			if(!bModified)
			{
				PrepareUndo("Map Notes");
			}
			key = n;
			bModified = true;
		}
		if (bModified)
		{
			m_pParent.SetModified(InstrumentHint().Info(), false);
			Invalidate(false);
		}
	}
}

void CNoteMapWnd::OnMapCopySample()
{
	ModInstrument *pIns = m_modDoc.GetSoundFile().Instruments[m_nInstrument];
	if (pIns)
	{
		m_undo = true;
		bool bModified = false;
		auto n = pIns->Keyboard[m_nNote];
		for (auto &sample : pIns->Keyboard) if (sample != n)
		{
			if(!bModified)
			{
				PrepareUndo("Map Samples");
			}
			sample = n;
			bModified = true;
		}
		if (bModified)
		{
			m_pParent.SetModified(InstrumentHint().Info(), false);
			Invalidate(false);
			UpdateAccessibleTitle();
		}
	}
}


void CNoteMapWnd::OnMapReset()
{
	ModInstrument *pIns = m_modDoc.GetSoundFile().Instruments[m_nInstrument];
	if (pIns)
	{
		m_undo = true;
		bool modified = false;
		for (size_t i = 0; i < std::size(pIns->NoteMap); i++) if (pIns->NoteMap[i] != i + 1)
		{
			if(!modified)
			{
				PrepareUndo("Reset Note Map");
			}
			pIns->NoteMap[i] = static_cast<ModCommand::NOTE>(i + 1);
			modified = true;
		}
		if(modified)
		{
			m_pParent.SetModified(InstrumentHint().Info(), false);
			Invalidate(false);
			UpdateAccessibleTitle();
		}
	}
}


void CNoteMapWnd::OnTransposeSamples()
{
	auto &sndFile = m_modDoc.GetSoundFile();
	ModInstrument *pIns = sndFile.Instruments[m_nInstrument];
	if(!pIns)
		return;
	const auto samples = pIns->CanConvertToDefaultNoteMap();
	if(samples.empty())
		return;

	PrepareUndo("Transpose Samples");
	for(const auto &[smp, transpose] : samples)
	{
		if(smp > sndFile.GetNumSamples())
			continue;
		m_modDoc.GetSampleUndo().PrepareUndo(smp, sundo_none, "Transpose");
		auto &sample = sndFile.GetSample(smp);
		if(sndFile.UseFinetuneAndTranspose())
			sample.RelativeTone += transpose;
		else
			sample.Transpose(transpose / 12.0);
		m_modDoc.UpdateAllViews(nullptr, SampleHint(smp).Info(), &m_pParent);
	}
	pIns->ResetNoteMap();
	m_pParent.SetModified(InstrumentHint().Info(), false);
	Invalidate(false);
}


void CNoteMapWnd::OnMapRemove()
{
	ModInstrument *pIns = m_modDoc.GetSoundFile().Instruments[m_nInstrument];
	if (pIns)
	{
		m_undo = true;
		bool modified = false;
		for (auto &sample: pIns->Keyboard) if (sample != 0)
		{
			if(!modified)
			{
				PrepareUndo("Remove Sample Assocations");
			}
			sample = 0;
			modified = true;
		}
		if(modified)
		{
			m_pParent.SetModified(InstrumentHint().Info(), false);
			Invalidate(false);
			UpdateAccessibleTitle();
		}
	}
}


void CNoteMapWnd::OnMapTransposeUp()
{
	MapTranspose(1);
}


void CNoteMapWnd::OnMapTransposeDown()
{
	MapTranspose(-1);
}


void CNoteMapWnd::MapTranspose(int nAmount)
{
	if(nAmount == 0 || m_modDoc.GetModType() == MOD_TYPE_XM) return;

	ModInstrument *pIns = m_modDoc.GetSoundFile().Instruments[m_nInstrument];
	if((nAmount == 12 || nAmount == -12))
	{
		// Special case for instrument-specific tunings
		nAmount = m_modDoc.GetInstrumentGroupSize(m_nInstrument) * mpt::signum(nAmount);
	}

	m_undo = true;
	if (pIns)
	{
		bool modified = false;
		for(NOTEINDEXTYPE i = 0; i < NOTE_MAX; i++)
		{
			int n = pIns->NoteMap[i];
			if ((n > NOTE_MIN && nAmount < 0) || (n < NOTE_MAX && nAmount > 0))
			{
				n = Clamp(n + nAmount, NOTE_MIN, NOTE_MAX);
				if(n != pIns->NoteMap[i])
				{
					if(!modified)
					{
						PrepareUndo("Transpose Map");
					}
					pIns->NoteMap[i] = static_cast<uint8>(n);
					modified = true;
				}
			}
		}
		if(modified)
		{
			m_pParent.SetModified(InstrumentHint().Info(), false);
			Invalidate(false);
			UpdateAccessibleTitle();
		}
	}
}


void CNoteMapWnd::OnEditSample(uint32 nID)
{
	uint32 nSample = nID - ID_NOTEMAP_EDITSAMPLE;
	m_pParent.EditSample(nSample);
}


void CNoteMapWnd::OnEditSampleMap()
{
	m_undo = true;
	m_pParent.mfcless::PostCommand(this, ID_INSTRUMENT_SAMPLEMAP);
}


void CNoteMapWnd::OnInstrumentDuplicate()
{
	m_undo = true;
	m_pParent.mfcless::PostCommand(this, ID_INSTRUMENT_DUPLICATE);
}


intptr_t CNoteMapWnd::OnCustomKeyMsg(uintptr_t wParam, intptr_t lParam)
{
	ModInstrument *pIns = m_modDoc.GetSoundFile().Instruments[m_nInstrument];

	// Handle notes

	if (wParam >= kcInsNoteMapStartNotes && wParam <= kcInsNoteMapEndNotes)
	{
		// Special case: number keys override notes if we're in the sample # column.
		const auto key = KeyCombination::FromLPARAM(lParam).KeyCode();
		if(m_bIns && ((key >= '0' && key <= '9') || (key == ' ')))
			HandleChar(key);
		else
			EnterNote(m_modDoc.GetNoteWithBaseOctave(static_cast<int>(wParam - kcInsNoteMapStartNotes), m_nInstrument));

		return wParam;
	}

	if (wParam >= kcInsNoteMapStartNoteStops && wParam <= kcInsNoteMapEndNoteStops)
	{
		StopNote();
		return wParam;
	}

	// Other shortcuts

	switch(wParam)
	{
	case kcContextMenu:
		{
			mfcless::Rect clientRect;
			GetClientRect(clientRect);
			clientRect.bottom = clientRect.top + mpt::align_up(clientRect.Height(), m_cyFont);
			OnRButtonUp(0, clientRect.CenterPoint());
		}
		return wParam;
	case kcInsNoteMapTransposeDown:		MapTranspose(-1); return wParam;
	case kcInsNoteMapTransposeUp:		MapTranspose(1); return wParam;
	case kcInsNoteMapTransposeOctDown:	MapTranspose(-12); return wParam;
	case kcInsNoteMapTransposeOctUp:	MapTranspose(12); return wParam;

	case kcInsNoteMapCopyCurrentSample:	OnMapCopySample(); return wParam;
	case kcInsNoteMapCopyCurrentNote:	OnMapCopyNote(); return wParam;
	case kcInsNoteMapReset:				OnMapReset(); return wParam;
	case kcInsNoteMapTransposeSamples:	OnTransposeSamples(); return wParam;
	case kcInsNoteMapRemove:			OnMapRemove(); return wParam;

	case kcInsNoteMapEditSample:		if(pIns) OnEditSample(pIns->Keyboard[m_nNote] + ID_NOTEMAP_EDITSAMPLE); return wParam;
	case kcInsNoteMapEditSampleMap:		OnEditSampleMap(); return wParam;

	// Parent shortcuts (also displayed in context menu of this control)
	case kcInstrumentCtrlDuplicate:		OnInstrumentDuplicate(); return wParam;
	case kcNextInstrument:				m_pParent.mfcless::PostCommand(this, ID_NEXTINSTRUMENT); return wParam;
	case kcPrevInstrument:				m_pParent.mfcless::PostCommand(this, ID_PREVINSTRUMENT); return wParam;
	}

	return kcNull;
}

void CNoteMapWnd::EnterNote(uint32 note)
{
	CSoundFile &sndFile = m_modDoc.GetSoundFile();
	ModInstrument *pIns = sndFile.Instruments[m_nInstrument];
	if ((pIns) && (m_nNote < NOTE_MAX))
	{
		if (!m_bIns && (sndFile.GetType() & (MOD_TYPE_IT|MOD_TYPE_MPT)))
		{
			uint32 n = pIns->NoteMap[m_nNote];
			bool ok = false;
			if ((note >= sndFile.GetModSpecifications().noteMin) && (note <= sndFile.GetModSpecifications().noteMax))
			{
				n = note;
				ok = true;
			}
			if (n != pIns->NoteMap[m_nNote])
			{
				StopNote(); // Stop old note according to current instrument settings
				pIns->NoteMap[m_nNote] = static_cast<ModCommand::NOTE>(n);
				m_pParent.SetModified(InstrumentHint().Info(), false);
				Invalidate(false);
				UpdateAccessibleTitle();
			}
			if(ok)
			{
				PlayNote(m_nNote);
			}
		}
	}
}

bool CNoteMapWnd::HandleChar(uintptr_t c)
{
	CSoundFile &sndFile = m_modDoc.GetSoundFile();
	ModInstrument *pIns = sndFile.Instruments[m_nInstrument];
	if ((pIns) && (m_nNote < NOTE_MAX))
	{

		if ((m_bIns) && (((c >= '0') && (c <= '9')) || (c == ' ')))	//in sample # column
		{
			uint32 n = m_nOldIns;
			if (c != ' ')
			{
				n = (10 * pIns->Keyboard[m_nNote] + (c - '0')) % 10000;
				if ((n >= MAX_SAMPLES) || ((sndFile.GetNumSamples() < 1000) && (n >= 1000)))
					n = (n % 1000);
				if ((n >= MAX_SAMPLES) || ((sndFile.GetNumSamples() < 100) && (n >= 100)))
					n = (n % 100);
				else if ((n > 31) && (sndFile.GetNumSamples() < 32) && (n % 10))
					n = (n % 10);
			}

			if (n != pIns->Keyboard[m_nNote])
			{
				if(m_undo)
				{
					PrepareUndo("Enter Instrument");
					m_undo = false;
				}
				StopNote(); // Stop old note according to current instrument settings
				pIns->Keyboard[m_nNote] = static_cast<SAMPLEINDEX>(n);
				m_pParent.SetModified(InstrumentHint().Info(), false);
				Invalidate(false);
				UpdateAccessibleTitle();
				PlayNote(m_nNote);
			}

			if (c == ' ')
			{
				SetCurrentNote(m_nNote + 1);
				PlayNote(m_nNote);
			}
			return true;
		}

		else if(!m_bIns && !(sndFile.GetType() & MOD_TYPE_XM))
		{
			uint32 n = pIns->NoteMap[m_nNote];

			if ((c >= '0') && (c <= '9'))
			{
				if (n)
					n = static_cast<uint32>(((n - 1) % 12) + (c - '0') * 12 + 1);
				else
					n = static_cast<uint32>((m_nNote % 12) + (c - '0') * 12 + 1);
			} else if (c == ' ')
			{
				n = (m_nOldNote) ? m_nOldNote : m_nNote+1;
			}

			if (n != pIns->NoteMap[m_nNote])
			{
				if(m_undo)
				{
					PrepareUndo("Enter Note");
					m_undo = false;
				}

				StopNote(); // Stop old note according to current instrument settings
				pIns->NoteMap[m_nNote] = static_cast<ModCommand::NOTE>(n);
				m_pParent.SetModified(InstrumentHint().Info(), false);
				Invalidate(false);
				UpdateAccessibleTitle();
			}

			if(c == ' ')
			{
				SetCurrentNote(m_nNote + 1);
			}

			PlayNote(m_nNote);

			return true;
		}
	}
	return false;
}


bool CNoteMapWnd::HandleNav(uintptr_t k)
{
	bool redraw = false;

	//HACK: handle numpad (convert numpad number key to normal number key)
	if ((k >= mfcless::vk::NUMPAD0) && (k <= mfcless::vk::NUMPAD9))
		return HandleChar(k-mfcless::vk::NUMPAD0+'0');

	const CSoundFile &sndFile = m_modDoc.GetSoundFile();
	const auto &modSpecs = sndFile.GetModSpecifications();
	uint32 noteMin = 0, noteMax = NOTE_MAX - NOTE_MIN;
	if(modSpecs.instrumentsMax)
	{
		noteMin = modSpecs.noteMin - NOTE_MIN;
		noteMax = modSpecs.noteMax - NOTE_MIN;
	}

	switch(k)
	{
	case mfcless::vk::RIGHT:
		if (!m_bIns) { m_bIns = true; redraw = true; }
		else if (m_nNote < noteMax) { m_nNote++; m_bIns = false; redraw = true; }
		break;
	case mfcless::vk::LEFT:
		if (m_bIns) { m_bIns = false; redraw = true; }
		else if (m_nNote > noteMin) { m_nNote--; m_bIns = true; redraw = true; }
		break;
	case mfcless::vk::UP:
		if (m_nNote > noteMin) { m_nNote--; redraw = true; }
		break;
	case mfcless::vk::DOWN:
		if (m_nNote < noteMax) { m_nNote++; redraw = true; }
		break;
	case mfcless::vk::PRIOR:
		if (m_nNote > noteMin + 3) { m_nNote -= 3; redraw = true; }
		else if (m_nNote > noteMin) { m_nNote = noteMin; redraw = true; }
		break;
	case mfcless::vk::NEXT:
		if (m_nNote + 3 < noteMax) { m_nNote += 3; redraw = true; }
		else if (m_nNote < noteMax) { m_nNote = noteMax; redraw = true; }
		break;
	case mfcless::vk::HOME:
		if(m_nNote > noteMin) { m_nNote = noteMin; redraw = true; }
		break;
	case mfcless::vk::END:
		if(m_nNote < noteMax) { m_nNote = noteMax; redraw = true; }
		break;
	case mfcless::vk::RETURN:
		{
			ModInstrument *pIns = m_modDoc.GetSoundFile().Instruments[m_nInstrument];
			if(pIns)
			{
				if (m_bIns)
					m_nOldIns = pIns->Keyboard[m_nNote];
				else
					m_nOldNote = pIns->NoteMap[m_nNote];
			}
		}

		return true;
	default:
		return false;
	}
	if(redraw)
	{
		m_undo = true;
		Invalidate(false);
		UpdateAccessibleTitle();
	}

	return true;
}


void CNoteMapWnd::PlayNote(uint32 note)
{
	if(m_nPlayingNote != NOTE_NONE)
	{
		// No polyphony in notemap window
		StopNote();
	}
	m_nPlayingNote = static_cast<ModCommand::NOTE>(note + NOTE_MIN);
	m_noteChannel = m_modDoc.PlayNote(PlayNoteParam(m_nPlayingNote).Instrument(m_nInstrument));
}


void CNoteMapWnd::StopNote()
{
	if(!ModCommand::IsNote(m_nPlayingNote)) return;

	m_modDoc.NoteOff(m_nPlayingNote, true, m_nInstrument, m_noteChannel);
	m_nPlayingNote = NOTE_NONE;
}


void CNoteMapWnd::UpdateAccessibleTitle()
{
	CMainFrame::GetMainFrame()->NotifyAccessibilityUpdate(*this);
}


// Accessible description for screen readers
HRESULT CNoteMapWnd::get_accName(VARIANT varChild, BSTR *pszName)
{
	const auto *ins = m_modDoc.GetSoundFile().Instruments[m_nInstrument];
	if(!ins || m_nNote >= std::size(ins->NoteMap))
		return mfcless::Window<QLabel>::get_accName(varChild, pszName);

	const auto &sndFile = m_modDoc.GetSoundFile();
	mpt::ustring str = mpt::ToUnicode(sndFile.GetNoteName(static_cast<ModCommand::NOTE>(m_nNote + NOTE_MIN), m_nInstrument)) + UL_(": ");
	if(ins->Keyboard[m_nNote] == 0)
	{
		str += UL_("no sample");
	} else
	{
		auto mappedNote = ins->NoteMap[m_nNote];
		str += MPT_UFORMAT("sample {} at {}")(ins->Keyboard[m_nNote], mpt::ToUnicode(sndFile.GetNoteName(mappedNote, m_nInstrument)));
	}

	*pszName = str.AllocSysString();
	return S_OK;
}


/////////////////////////////////////////////////////////////////////////
// CCtrlInstruments

#define MAX_ATTACK_LENGTH	2001
#define MAX_ATTACK_VALUE	(MAX_ATTACK_LENGTH - 1)  // 16 bit unsigned max

void CCtrlInstruments::RegisterMessageMap()
{
	CModControlDlg::RegisterMessageMap();
	AddScroll(Qt::Vertical, &CCtrlInstruments::OnVScroll);
	AddScroll(Qt::Horizontal, &CCtrlInstruments::OnHScroll);
	// ON_WM_XBUTTONUP() (mfcless::Window)
	// ON_NOTIFY(TBN_DROPDOWN, IDC_TOOLBAR1, &CCtrlInstruments::OnTbnDropDownToolBar): by hand (Qt event override)
	AddCommand(IDC_INSTRUMENT_NEW, &CCtrlInstruments::OnInstrumentNew);
	AddCommand(IDC_INSTRUMENT_OPEN, &CCtrlInstruments::OnInstrumentOpen);
	AddCommand(IDC_INSTRUMENT_SAVEAS, &CCtrlInstruments::OnInstrumentSave);
	AddCommand(IDC_SAVE_ONE, &CCtrlInstruments::OnInstrumentSaveOne);
	AddCommand(IDC_SAVE_ALL, &CCtrlInstruments::OnInstrumentSaveAll);
	AddCommand(IDC_INSTRUMENT_PLAY, &CCtrlInstruments::OnInstrumentPlay);
	AddCommand(ID_PREVINSTRUMENT, &CCtrlInstruments::OnPrevInstrument);
	AddCommand(ID_NEXTINSTRUMENT, &CCtrlInstruments::OnNextInstrument);
	AddCommand(ID_INSTRUMENT_DUPLICATE, &CCtrlInstruments::OnInstrumentDuplicate);
	AddCommand(IDC_CHECK1, &CCtrlInstruments::OnSetPanningChanged);
	AddCommand(IDC_CHECK2, &CCtrlInstruments::OnEnableCutOff);
	AddCommand(IDC_CHECK3, &CCtrlInstruments::OnEnableResonance);
	AddCommand(IDC_INSVIEWPLG, &CCtrlInstruments::TogglePluginEditor);
	AddNotify(IDC_EDIT_INSTRUMENT, mfcless::Notify::EditChange, &CCtrlInstruments::OnInstrumentChanged);
	AddNotify(IDC_SAMPLE_NAME, mfcless::Notify::EditChange, &CCtrlInstruments::OnNameChanged);
	AddNotify(IDC_SAMPLE_FILENAME, mfcless::Notify::EditChange, &CCtrlInstruments::OnFileNameChanged);
	AddNotify(IDC_EDIT7, mfcless::Notify::EditChange, &CCtrlInstruments::OnFadeOutVolChanged);
	AddNotify(IDC_EDIT8, mfcless::Notify::EditChange, &CCtrlInstruments::OnGlobalVolChanged);
	AddNotify(IDC_EDIT9, mfcless::Notify::EditChange, &CCtrlInstruments::OnPanningChanged);
	AddNotify(IDC_EDIT10, mfcless::Notify::EditChange, &CCtrlInstruments::OnMPRChanged);
	AddNotify(IDC_EDIT10, mfcless::Notify::KillFocus, &CCtrlInstruments::OnMPRKillFocus);
	AddNotify(IDC_EDIT11, mfcless::Notify::EditChange, &CCtrlInstruments::OnMBKChanged);
	AddNotify(IDC_EDIT15, mfcless::Notify::EditChange, &CCtrlInstruments::OnPPSChanged);
	AddNotify(IDC_PITCHWHEELDEPTH, mfcless::Notify::EditChange, &CCtrlInstruments::OnPitchWheelDepthChanged);
	AddNotify(IDC_EDIT2, mfcless::Notify::EditChange, &CCtrlInstruments::OnAttackChanged);
	AddNotify(IDC_SAMPLE_NAME, mfcless::Notify::SetFocus, &CCtrlInstruments::OnEditFocus);
	AddNotify(IDC_SAMPLE_FILENAME, mfcless::Notify::SetFocus, &CCtrlInstruments::OnEditFocus);
	AddNotify(IDC_EDIT7, mfcless::Notify::SetFocus, &CCtrlInstruments::OnEditFocus);
	AddNotify(IDC_EDIT8, mfcless::Notify::SetFocus, &CCtrlInstruments::OnEditFocus);
	AddNotify(IDC_EDIT9, mfcless::Notify::SetFocus, &CCtrlInstruments::OnEditFocus);
	AddNotify(IDC_EDIT10, mfcless::Notify::SetFocus, &CCtrlInstruments::OnEditFocus);
	AddNotify(IDC_EDIT11, mfcless::Notify::SetFocus, &CCtrlInstruments::OnEditFocus);
	AddNotify(IDC_EDIT15, mfcless::Notify::SetFocus, &CCtrlInstruments::OnEditFocus);
	AddNotify(IDC_PITCHWHEELDEPTH, mfcless::Notify::SetFocus, &CCtrlInstruments::OnEditFocus);
	AddNotify(IDC_EDIT2, mfcless::Notify::SetFocus, &CCtrlInstruments::OnEditFocus);
	AddNotify(IDC_EDIT_PITCHTEMPOLOCK, mfcless::Notify::SetFocus, &CCtrlInstruments::OnEditFocus);
	AddNotify(IDC_COMBO1, mfcless::Notify::SelChange, &CCtrlInstruments::OnNNAChanged);
	AddNotify(IDC_COMBO2, mfcless::Notify::SelChange, &CCtrlInstruments::OnDCTChanged);
	AddNotify(IDC_COMBO3, mfcless::Notify::SelChange, &CCtrlInstruments::OnDCAChanged);
	AddNotify(IDC_COMBO4, mfcless::Notify::SelChange, &CCtrlInstruments::OnPPCChanged);
	AddNotify(IDC_COMBO5, mfcless::Notify::SelChange, &CCtrlInstruments::OnMCHChanged);
	AddNotify(IDC_COMBO6, mfcless::Notify::SelChange, &CCtrlInstruments::OnMixPlugChanged);
	AddNotify(IDC_COMBO6, mfcless::Notify::DropDown, &CCtrlInstruments::OnOpenPluginList);
	AddNotify(IDC_COMBO9, mfcless::Notify::SelChange, &CCtrlInstruments::OnResamplingChanged);
	AddNotify(IDC_FILTERMODE, mfcless::Notify::SelChange, &CCtrlInstruments::OnFilterModeChanged);
	AddNotify(IDC_PLUGIN_VOLUMESTYLE, mfcless::Notify::SelChange, &CCtrlInstruments::OnPluginVolumeHandlingChanged);
	AddCommand(IDC_PLUGIN_VELOCITYSTYLE, &CCtrlInstruments::OnPluginVelocityHandlingChanged);
	AddCommand(ID_INSTRUMENT_SAMPLEMAP, &CCtrlInstruments::OnEditSampleMap);
	AddNotify(IDC_COMBOTUNING, mfcless::Notify::SelChange, &CCtrlInstruments::OnCbnSelchangeCombotuning);
	AddNotify(IDC_EDIT_PITCHTEMPOLOCK, mfcless::Notify::EditChange, &CCtrlInstruments::OnEnChangeEditPitchTempoLock);
	AddCommand(IDC_CHECK_PITCHTEMPOLOCK, &CCtrlInstruments::OnBnClickedCheckPitchtempolock);
	AddNotify(IDC_EDIT_PITCHTEMPOLOCK, mfcless::Notify::KillFocus, &CCtrlInstruments::OnEnKillFocusEditPitchTempoLock);
	AddNotify(IDC_EDIT7, mfcless::Notify::KillFocus, &CCtrlInstruments::OnEnKillFocusEditFadeOut);
}

void CCtrlInstruments::DoDataExchange(DataExchange &dx)
{
	CModControlDlg::DoDataExchange(dx);
	//{{AFX_DATA_MAP(CCtrlInstruments)
	ExchangeControl(dx, IDC_TOOLBAR1, m_ToolBar);
	ExchangeControl(dx, IDC_NOTEMAP, m_NoteMap);
	ExchangeControl(dx, IDC_SAMPLE_NAME, m_EditName);
	ExchangeControl(dx, IDC_SAMPLE_FILENAME, m_EditFileName);
	ExchangeControl(dx, IDC_SPIN_INSTRUMENT, m_SpinInstrument);
	ExchangeControl(dx, IDC_COMBO1, m_ComboNNA);
	ExchangeControl(dx, IDC_COMBO2, m_ComboDCT);
	ExchangeControl(dx, IDC_COMBO3, m_ComboDCA);
	ExchangeControl(dx, IDC_COMBO4, m_ComboPPC);
	ExchangeControl(dx, IDC_COMBO5, m_CbnMidiCh);
	ExchangeControl(dx, IDC_COMBO6, m_CbnMixPlug);
	ExchangeControl(dx, IDC_COMBO9, m_CbnResampling);
	ExchangeControl(dx, IDC_FILTERMODE, m_CbnFilterMode);
	ExchangeControl(dx, IDC_EDIT7, m_EditFadeOut);
	ExchangeControl(dx, IDC_SPIN7, m_SpinFadeOut);
	ExchangeControl(dx, IDC_SPIN8, m_SpinGlobalVol);
	ExchangeControl(dx, IDC_SPIN9, m_SpinPanning);
	ExchangeControl(dx, IDC_SPIN10, m_SpinMidiPR);
	ExchangeControl(dx, IDC_SPIN11, m_SpinMidiBK);
	ExchangeControl(dx, IDC_SPIN12, m_SpinPPS);
	ExchangeControl(dx, IDC_EDIT8, m_EditGlobalVol);
	ExchangeControl(dx, IDC_EDIT9, m_EditPanning);
	ExchangeControl(dx, IDC_CHECK1, m_CheckPanning);
	ExchangeControl(dx, IDC_CHECK2, m_CheckCutOff);
	ExchangeControl(dx, IDC_CHECK3, m_CheckResonance);
	ExchangeControl(dx, IDC_SLIDER1, m_SliderVolSwing);
	ExchangeControl(dx, IDC_SLIDER2, m_SliderPanSwing);
	ExchangeControl(dx, IDC_SLIDER3, m_SliderCutOff);
	ExchangeControl(dx, IDC_SLIDER4, m_SliderResonance);
	ExchangeControl(dx, IDC_SLIDER6, m_SliderCutSwing);
	ExchangeControl(dx, IDC_SLIDER7, m_SliderResSwing);
	ExchangeControl(dx, IDC_SLIDER5, m_SliderAttack);
	ExchangeControl(dx, IDC_SPIN1, m_SpinAttack);
	ExchangeControl(dx, IDC_COMBOTUNING, m_ComboTuning);
	ExchangeControl(dx, IDC_CHECK_PITCHTEMPOLOCK, m_CheckPitchTempoLock);
	ExchangeControl(dx, IDC_PLUGIN_VOLUMESTYLE, m_CbnPluginVolumeHandling);
	ExchangeControl(dx, IDC_PLUGIN_VELOCITYSTYLE, velocityStyle);
	ExchangeControl(dx, IDC_SPIN2, m_SpinPWD);
	//}}AFX_DATA_MAP
}


CCtrlInstruments::CCtrlInstruments(CModControlView &parent, CModDoc &document)
	: CModControlDlg(parent, document)
	, m_NoteMap(*this, document)
{
	m_nLockCount = 1;
}


const mfcless::ViewClass *CCtrlInstruments::GetAssociatedViewClass()
{
	return (&CViewInstrument::classCViewInstrument);
}


void CCtrlInstruments::OnEditFocus()
{
	m_startedEdit = false;
}


bool CCtrlInstruments::OnInitDialog()
{
	CModControlDlg::OnInitDialog();
	m_initialized = false;
	SetRedraw(false);

	m_ToolBar->SetExtendedStyle(m_ToolBar->GetExtendedStyle() | TBSTYLE_EX_DRAWDDARROWS);
	m_ToolBar->Init(CMainFrame::GetMainFrame()->m_PatternIcons,CMainFrame::GetMainFrame()->m_PatternIconsDisabled);
	m_ToolBar->AddButton(IDC_INSTRUMENT_NEW, TIMAGE_INSTR_NEW, TBSTYLE_BUTTON | TBSTYLE_DROPDOWN);
	m_ToolBar->AddButton(IDC_INSTRUMENT_OPEN, TIMAGE_OPEN);
	m_ToolBar->AddButton(IDC_INSTRUMENT_SAVEAS, TIMAGE_SAVE, TBSTYLE_BUTTON | TBSTYLE_DROPDOWN);
	m_ToolBar->AddButton(IDC_INSTRUMENT_PLAY, TIMAGE_PREVIEW);
	m_SpinInstrument->SetRange(0, 0);
	m_SpinInstrument->setEnabled(false);
	// NNA
	mfcless::AddItem(m_ComboNNA, UL_("Note Cut"));
	mfcless::AddItem(m_ComboNNA, UL_("Continue"));
	mfcless::AddItem(m_ComboNNA, UL_("Note Off"));
	mfcless::AddItem(m_ComboNNA, UL_("Note Fade"));
	// DCT
	mfcless::AddItem(m_ComboDCT, UL_("Disabled"));
	mfcless::AddItem(m_ComboDCT, UL_("Note"));
	mfcless::AddItem(m_ComboDCT, UL_("Sample"));
	mfcless::AddItem(m_ComboDCT, UL_("Instrument"));
	mfcless::AddItem(m_ComboDCT, UL_("Plugin"));
	// DCA
	mfcless::AddItem(m_ComboDCA, UL_("Note Cut"));
	mfcless::AddItem(m_ComboDCA, UL_("Note Off"));
	mfcless::AddItem(m_ComboDCA, UL_("Note Fade"));
	// FadeOut Volume
	m_SpinFadeOut->SetRange(0, 8192);
	// Global Volume
	m_SpinGlobalVol->SetRange(0, 64);
	// Panning
	m_SpinPanning->SetRange(0, (m_modDoc.GetModType() & MOD_TYPE_IT) ? 64 : 256);
	// Midi Program
	m_SpinMidiPR->SetRange(0, 128);
	// Midi Bank
	m_SpinMidiBK->SetRange(0, 16384);
	// MIDI Pitch Wheel Depth
	m_EditPWD->SubclassDlgItem(IDC_PITCHWHEELDEPTH, this);
	m_EditPWD->AllowFractions(false);

	const auto resamplingModes = Resampling::AllModes();
	mfcless::SetItemData(m_CbnResampling, mfcless::AddItem(m_CbnResampling, UL_("Default")), SRCMODE_DEFAULT);
	for(auto mode : resamplingModes)
	{
		mfcless::SetItemData(m_CbnResampling, mfcless::AddItem(m_CbnResampling, CTrackApp::GetResamplingModeName(mode, 1, false)), mode);
	}

	mfcless::SetItemData(m_CbnFilterMode, mfcless::AddItem(m_CbnFilterMode, UL_("Channel default")), static_cast<uintptr_t>(FilterMode::Unchanged));
	mfcless::SetItemData(m_CbnFilterMode, mfcless::AddItem(m_CbnFilterMode, UL_("Force lowpass")), static_cast<uintptr_t>(FilterMode::LowPass));
	mfcless::SetItemData(m_CbnFilterMode, mfcless::AddItem(m_CbnFilterMode, UL_("Force highpass")), static_cast<uintptr_t>(FilterMode::HighPass));

	//VST velocity/volume handling
	mfcless::AddItem(m_CbnPluginVolumeHandling, UL_("MIDI volume"));
	mfcless::AddItem(m_CbnPluginVolumeHandling, UL_("Dry/Wet ratio"));
	mfcless::AddItem(m_CbnPluginVolumeHandling, UL_("None"));

	// Vol/Pan Swing
	m_SliderVolSwing->setRange(0, 100);
	m_SliderPanSwing->setRange(0, 64);
	m_SliderCutSwing->setRange(0, 64);
	m_SliderResSwing->setRange(0, 64);
	// Filter
	m_SliderCutOff->setRange(0x00, 0x7F);
	m_SliderResonance->setRange(0x00, 0x7F);
	// Pitch/Pan Separation
	m_EditPPS->SubclassDlgItem(IDC_EDIT15, this);
	m_EditPPS->AllowFractions(false);
	m_SpinPPS->SetRange(-32, +32);
	// Pitch/Pan Center
	SetWindowLongPtr(m_ComboPPC, GWLP_USERDATA, 0);

	// Volume ramping (attack)
	m_SliderAttack->setRange(0,MAX_ATTACK_VALUE);
	m_SpinAttack->SetRange(0,MAX_ATTACK_VALUE);

	m_SpinInstrument->setFocus();

	m_EditPWD->setEnabled(false);

	BuildTuningComboBox();

	SetChecked(IDC_CHECK_PITCHTEMPOLOCK, 0);
	m_EditPitchTempoLock->SubclassDlgItem(IDC_EDIT_PITCHTEMPOLOCK, this);
	m_EditPitchTempoLock->AllowNegative(false);
	m_EditPitchTempoLock->SetLimitText(9);

	SetRedraw(true);
	return false;
}


void CCtrlInstruments::OnDPIChanged()
{
	m_ToolBar->OnDPIChanged();
	CModControlDlg::OnDPIChanged();
}


Setting<int32> &CCtrlInstruments::GetSplitPosRef() { return TrackerSettings::Instance().glInstrumentWindowHeight; }


void CCtrlInstruments::RecalcLayout()
{
}


void CCtrlInstruments::OnTbnDropDownToolBar()
{
	CInputHandler *ih = CMainFrame::GetInputHandler();
	NMTOOLBAR *pToolBar = reinterpret_cast<NMTOOLBAR *>(pNMHDR);
	ClientToScreen(&(pToolBar->rcButton)); // TrackPopupMenu uses screen coords
	const int offset = HighDPISupport::ScalePixels(4, this);  // Compared to the main toolbar, the offset seems to be a bit wrong here...?
	int x = pToolBar->rcButton.left + offset, y = pToolBar->rcButton.bottom + offset;
	mfcless::Menu menu;
	switch(pToolBar->iItem)
	{
	case IDC_INSTRUMENT_NEW:
		{
			menu.CreatePopupMenu();
			menu.AppendMenu(mfcless::menu::String, ID_INSTRUMENT_DUPLICATE, ih->GetKeyTextFromCommand(kcInstrumentCtrlDuplicate, UL_("Duplicate &Instrument")));
			menu.TrackPopupMenu(mfcless::menu::LeftAlign | mfcless::menu::RightButton, x, y, this);
			menu.DestroyMenu();
		}
		break;
	case IDC_INSTRUMENT_SAVEAS:
		{
			menu.CreatePopupMenu();
			menu.AppendMenu(mfcless::menu::String, IDC_SAVE_ALL, UL_("Save &All..."));
			menu.TrackPopupMenu(mfcless::menu::LeftAlign | mfcless::menu::RightButton, x, y, this);
			menu.DestroyMenu();
		}
		break;
	}
	CurrentNotify().result = 0;
}


void CCtrlInstruments::PrepareUndo(const char *description)
{
	m_startedEdit = true;
	m_modDoc.GetInstrumentUndo().PrepareUndo(m_nInstrument, description);
}


// Set document as modified and update other views.
// updateAll: Update all views including this one. Otherwise, only update update other views.
void CCtrlInstruments::SetModified(InstrumentHint hint, bool updateAll)
{
	m_modDoc.SetModified();
	m_modDoc.UpdateAllViews(nullptr, hint.SetData(m_nInstrument), updateAll ? nullptr : this);
}


bool CCtrlInstruments::SetCurrentInstrument(uint32 nIns, bool bUpdNum)
{
	if (m_sndFile.m_nInstruments < 1) return false;
	if ((nIns < 1) || (nIns > m_sndFile.m_nInstruments)) return false;
	LockControls();
	if (m_nInstrument != nIns || !m_initialized)
	{
		m_nInstrument = static_cast<INSTRUMENTINDEX>(nIns);
		m_NoteMap->SetCurrentInstrument(m_nInstrument);
		UpdateView(InstrumentHint(m_nInstrument).Info().Envelope(), NULL);
	} else
	{
		// Just in case
		m_NoteMap->SetCurrentInstrument(m_nInstrument);
	}
	if (bUpdNum)
	{
		SetItemInt(IDC_EDIT_INSTRUMENT, m_nInstrument);
		m_SpinInstrument->SetRange(1, m_sndFile.GetNumInstruments());
		m_SpinInstrument->setEnabled((m_sndFile.GetNumInstruments()) ? true : false);
		// Is this a bug ?
		m_SliderCutOff->update(false);
		m_SliderResonance->update(false);
		// Volume ramping (attack)
		m_SliderAttack->update(false);
	}
	SendViewMessage(VIEWMSG_SETCURRENTINSTRUMENT, m_nInstrument);
	UnlockControls();

	return true;
}


void CCtrlInstruments::OnActivatePage(intptr_t lParam)
{
	CModControlDlg::OnActivatePage(lParam);
	if (lParam < 0)
	{
		int nIns = m_parent.GetInstrumentChange();
		if (nIns > 0) lParam = nIns;
	} else if(lParam > 0)
	{
		m_parent.InstrumentChanged(static_cast<INSTRUMENTINDEX>(lParam));
	}

	m_CbnMixPlug->Update(PluginComboBox::Config{PluginComboBox::ShowNoPlugin | PluginComboBox::ShowEmptySlots}, m_sndFile);

	CChildFrame *pFrame = (CChildFrame *)GetParentFrame();
	InstrumentViewState &instrumentState = pFrame->GetInstrumentViewState();
	if(instrumentState.initialInstrument != 0)
	{
		m_nInstrument = instrumentState.initialInstrument;
		instrumentState.initialInstrument = 0;
	}

	SetCurrentInstrument(static_cast<INSTRUMENTINDEX>((lParam > 0) ? lParam : m_nInstrument));

	// Initial Update
	if(!m_initialized)
		UpdateView(InstrumentHint(m_nInstrument).Info().Envelope().ModType(), NULL);

	PostViewMessage(VIEWMSG_LOADSTATE, (intptr_t)&instrumentState);
	SwitchToView();

	// Combo boxes randomly disappear without this... why?
	Invalidate();
}


void CCtrlInstruments::OnDeactivatePage()
{
	m_modDoc.NoteOff(0, true);
	CChildFrame *pFrame = (CChildFrame *)GetParentFrame();
	if ((pFrame) && (m_hWndView)) SendViewMessage(VIEWMSG_SAVESTATE, (intptr_t)&pFrame->GetInstrumentViewState());
	CModControlDlg::OnDeactivatePage();
}


intptr_t CCtrlInstruments::OnModCtrlMsg(uintptr_t wParam, intptr_t lParam)
{
	switch(wParam)
	{
	case CTRLMSG_GETCURRENTINSTRUMENT:
		return m_nInstrument;
		break;

	case CTRLMSG_INS_PREVINSTRUMENT:
		OnPrevInstrument();
		break;

	case CTRLMSG_INS_NEXTINSTRUMENT:
		OnNextInstrument();
		break;

	case CTRLMSG_INS_OPENFILE:
		if(lParam)
			return OpenInstrument(*reinterpret_cast<const mpt::PathString *>(lParam));
		break;

	case CTRLMSG_INS_SONGDROP:
		if(lParam)
		{
			const auto &dropInfo = *reinterpret_cast<const DRAGONDROP *>(lParam);
			if(dropInfo.sndFile)
				return OpenInstrument(*dropInfo.sndFile, static_cast<INSTRUMENTINDEX>(dropInfo.dropItem));
		}
		break;

	case CTRLMSG_INS_NEWINSTRUMENT:
		return InsertInstrument(false) ? 1 : 0;

	case CTRLMSG_SETCURRENTINSTRUMENT:
		SetCurrentInstrument(static_cast<INSTRUMENTINDEX>(lParam));
		break;

	case CTRLMSG_INS_SAMPLEMAP:
		OnEditSampleMap();
		break;

	case IDC_INSTRUMENT_NEW:
		OnInstrumentNew();
		break;
	case IDC_INSTRUMENT_OPEN:
		OnInstrumentOpen();
		break;
	case IDC_INSTRUMENT_SAVEAS:
		OnInstrumentSave();
		break;

	default:
		return CModControlDlg::OnModCtrlMsg(wParam, lParam);
	}
	return 0;
}


void CCtrlInstruments::UpdateView(UpdateHint hint, QObject *pObj)
{
	if(pObj == this)
		return;
	if (hint.GetType()[HINT_MPTOPTIONS])
	{
		m_ToolBar->UpdateStyle();
		hint.ModType(); // For possibly updating note names in Pitch/Pan Separation dropdown
	}
	LockControls();
	if(hint.ToType<PluginHint>().GetType()[HINT_PLUGINNAMES | HINT_MODTYPE])
	{
		m_CbnMixPlug->Update(PluginComboBox::Config{hint, pObj}, m_sndFile);
	}
	if(hint.ToType<GeneralHint>().GetType()[HINT_TUNINGS | HINT_MODTYPE])
	{
		BuildTuningComboBox();
	}
	UnlockControls();

	const InstrumentHint instrHint = hint.ToType<InstrumentHint>();
	FlagSet<HintType> hintType = instrHint.GetType();
	if(!m_initialized)
		hintType.set(HINT_MODTYPE);
	if(!hintType[HINT_MODTYPE | HINT_INSTRUMENT | HINT_ENVELOPE | HINT_INSNAMES])
		return;

	const INSTRUMENTINDEX updateIns = instrHint.GetInstrument();
	if(updateIns != m_nInstrument && updateIns != 0 && !hintType[HINT_MODTYPE])
		return;

	LockControls();
	const ModInstrument *pIns = m_sndFile.Instruments[m_nInstrument];

	if(hintType[HINT_MODTYPE])
	{
		auto &specs = m_sndFile.GetModSpecifications();

		// Limit text fields
		m_EditName->setMaxLength(specs.instrNameLengthMax);
		m_EditFileName->setMaxLength(specs.instrFilenameLengthMax);

		const bool onlyITandMPT = ((m_sndFile.GetType() & (MOD_TYPE_IT | MOD_TYPE_MPT)) && (m_sndFile.GetNumInstruments())) ? true : false;
		const bool anyFormat = m_sndFile.GetNumInstruments() ? true : false;
		const bool onlyMPT = ((m_sndFile.GetType() == MOD_TYPE_MPT) && (m_sndFile.GetNumInstruments())) ? true : false;
		::EnableWindow(::GetDlgItem(this, IDC_EDIT10), anyFormat);
		::EnableWindow(::GetDlgItem(this, IDC_EDIT11), anyFormat);
		::EnableWindow(::GetDlgItem(this, IDC_EDIT7), anyFormat);
		m_EditName->setEnabled(anyFormat);
		m_EditFileName->setEnabled(onlyITandMPT);
		m_CbnMidiCh->setEnabled(anyFormat);
		m_CbnMixPlug->setEnabled(anyFormat);
		m_SpinMidiPR->setEnabled(anyFormat);
		m_SpinMidiBK->setEnabled(anyFormat);

		const bool extendedFadeoutRange = !(m_sndFile.GetType() & MOD_TYPE_IT);
		m_SpinFadeOut->setEnabled(anyFormat);
		m_SpinFadeOut->SetRange(0, extendedFadeoutRange ? 32767 : 8192);
		m_EditFadeOut->setMaxLength(extendedFadeoutRange ? 5 : 4);
		// XM-style fade-out is 32 times more precise than IT
		UDACCEL accell[2];
		accell[0].nSec = 0;
		accell[0].nInc = (m_sndFile.GetType() == MOD_TYPE_IT ? 32 : 1);
		accell[1].nSec = 2;
		accell[1].nInc = 5 * accell[0].nInc;
		m_SpinFadeOut->SetAccel(mpt::saturate_cast<int>(std::size(accell)), accell);

		// Panning ranges (0...64 for IT, 0...256 for MPTM)
		m_SpinPanning->SetRange(0, (m_sndFile.GetType() & MOD_TYPE_IT) ? 64 : 256);

		// Pitch Wheel Depth
		if(m_sndFile.GetType() == MOD_TYPE_XM)
			m_SpinPWD->SetRange(0, 36);
		else
			m_SpinPWD->SetRange(-128, 127);
		m_EditPWD->setEnabled(anyFormat);
		m_SpinPWD->setEnabled(anyFormat);

		m_NoteMap->setEnabled(anyFormat);

		m_ComboNNA->setEnabled(onlyITandMPT);
		m_SliderVolSwing->setEnabled(onlyITandMPT);
		m_SliderPanSwing->setEnabled(onlyITandMPT);
		m_ComboDCT->setEnabled(onlyITandMPT);
		m_ComboDCA->setEnabled(onlyITandMPT);
		m_ComboPPC->setEnabled(onlyITandMPT);
		m_SpinPPS->setEnabled(onlyITandMPT);
		m_EditGlobalVol->setEnabled(onlyITandMPT);
		m_SpinGlobalVol->setEnabled(onlyITandMPT);
		m_EditPanning->setEnabled(onlyITandMPT);
		m_SpinPanning->setEnabled(onlyITandMPT);
		m_CheckPanning->setEnabled(onlyITandMPT);
		m_EditPPS->setEnabled(onlyITandMPT);
		m_CheckCutOff->setEnabled(onlyITandMPT);
		m_CheckResonance->setEnabled(onlyITandMPT);
		m_SliderCutOff->setEnabled(onlyITandMPT);
		m_SliderResonance->setEnabled(onlyITandMPT);
		m_ComboTuning->setEnabled(onlyMPT);
		m_EditPitchTempoLock->setEnabled(onlyMPT);
		m_CheckPitchTempoLock->setEnabled(onlyMPT);

		// MIDI Channel
		// XM has no "mapped" MIDI channels.
		m_CbnMidiCh->clear();
		for(int ich = MidiNoChannel; ich <= (onlyITandMPT ? MidiMappedChannel : MidiLastChannel); ich++)
		{
			mpt::ustring s;
			if (ich == MidiNoChannel)
				s = UL_("None");
			else if (ich == MidiMappedChannel)
				s = UL_("Mapped");
			else
				s = mfcless::PrintF(UL_("%i"), ich);
			mfcless::SetItemData(m_CbnMidiCh, mfcless::AddItem(m_CbnMidiCh, s), ich);
		}
	}
	if(hintType[HINT_MODTYPE | HINT_INSTRUMENT | HINT_INSNAMES])
	{
		if(pIns)
			mfcless::SetWindowText(m_EditName, mpt::ToUnicode(m_sndFile.GetCharsetInternal(), pIns->name));
		else
			mfcless::SetWindowText(m_EditName, UL_(""));
	}
	if(hintType[HINT_MODTYPE | HINT_INSTRUMENT])
	{
		m_SpinInstrument->SetRange(1, m_sndFile.m_nInstruments);
		m_SpinInstrument->setEnabled((m_sndFile.m_nInstruments) ? true : false);

		// Backwards compatibility with legacy IT/XM modules that use now deprecated hack features.
		m_SliderCutSwing->setEnabled(pIns != nullptr && (m_sndFile.GetType() == MOD_TYPE_MPT || pIns->nCutSwing != 0));
		m_SliderResSwing->setEnabled(pIns != nullptr && (m_sndFile.GetType() == MOD_TYPE_MPT || pIns->nResSwing != 0));
		m_CbnFilterMode->EnableWindow (pIns != nullptr && (m_sndFile.GetType() == MOD_TYPE_MPT || pIns->filterMode != FilterMode::Unchanged));
		m_CbnResampling->EnableWindow (pIns != nullptr && (m_sndFile.GetType() == MOD_TYPE_MPT || pIns->resampling != SRCMODE_DEFAULT));
		m_SliderAttack->EnableWindow  (pIns != nullptr && (m_sndFile.GetType() == MOD_TYPE_MPT || pIns->nVolRampUp));
		::EnableWindow(::GetDlgItem(this, IDC_EDIT2), pIns != nullptr && (m_sndFile.GetType() == MOD_TYPE_MPT || pIns->nVolRampUp));

		if (pIns)
		{
			mfcless::SetWindowText(m_EditFileName, mpt::ToUnicode(m_sndFile.GetCharsetInternal(), pIns->filename));
			// Fade Out Volume
			SetItemInt(IDC_EDIT7, pIns->nFadeOut);
			// Global Volume
			SetItemInt(IDC_EDIT8, pIns->nGlobalVol);
			// Panning
			SetDlgItemInt(IDC_EDIT9, (m_modDoc.GetModType() & MOD_TYPE_IT) ? (pIns->nPan / 4) : pIns->nPan);
			m_CheckPanning->setChecked(pIns->dwFlags[INS_SETPANNING] ? true : false);
			// Midi
			if (pIns->nMidiProgram>0 && pIns->nMidiProgram<=128)
				SetItemInt(IDC_EDIT10, pIns->nMidiProgram);
			else
				SetItemText(IDC_EDIT10, UL_("---"));
			if (pIns->wMidiBank && pIns->wMidiBank <= 16384)
				SetItemInt(IDC_EDIT11, pIns->wMidiBank);
			else
				SetItemText(IDC_EDIT11, UL_("---"));

			if (pIns->nMidiChannel < 18)
			{
				m_CbnMidiCh->setCurrentIndex(pIns->nMidiChannel);
			} else
			{
				m_CbnMidiCh->setCurrentIndex(0);
			}
			if (pIns->nMixPlug > 0)
			{
				m_CbnMixPlug->SetSelection(pIns->nMixPlug - 1);
			} else
			{
				m_CbnMixPlug->SetSelection(PLUGINDEX_INVALID);
			}
			OnMixPlugChanged();
			for(int resMode = 0; resMode<m_CbnResampling->count(); resMode++)
			{
				if(pIns->resampling == mfcless::GetItemData(m_CbnResampling, resMode))
				{
					m_CbnResampling->setCurrentIndex(resMode);
					break;
				}
			}
			for(int fltMode = 0; fltMode<m_CbnFilterMode->count(); fltMode++)
			{
				if(pIns->filterMode == static_cast<FilterMode>(mfcless::GetItemData(m_CbnFilterMode, fltMode)))
				{
					m_CbnFilterMode->setCurrentIndex(fltMode);
					break;
				}
			}

			// NNA, DCT, DCA
			m_ComboNNA->setCurrentIndex(static_cast<int>(pIns->nNNA));
			m_ComboDCT->setCurrentIndex(static_cast<int>(pIns->nDCT));
			m_ComboDCA->setCurrentIndex(static_cast<int>(pIns->nDNA));
			// Pitch/Pan Separation
			if(hintType[HINT_MODTYPE] || pIns->pTuning != (CTuning *)GetWindowLongPtr(m_ComboPPC, GWLP_USERDATA))
			{
				// Tuning may have changed, and thus the note names need to be updated
				m_ComboPPC->setUpdatesEnabled(false);
				m_ComboPPC->clear();
				AppendNotesToControlEx(m_ComboPPC, m_sndFile, m_nInstrument, NOTE_MIN, NOTE_MAX);
				SetWindowLongPtr(m_ComboPPC, GWLP_USERDATA, (LONG_PTR)pIns->pTuning);
				m_ComboPPC->setUpdatesEnabled(true);
			}
			m_ComboPPC->setCurrentIndex(pIns->nPPC);
			MPT_ASSERT((uint8)mfcless::GetItemData(m_ComboPPC, m_ComboPPC->currentIndex()) == pIns->nPPC + NOTE_MIN);
			SetItemInt(IDC_EDIT15, pIns->nPPS);
			// Filter
			if (m_sndFile.GetType() & (MOD_TYPE_IT | MOD_TYPE_MPT))
			{
				m_CheckCutOff->setChecked((pIns->IsCutoffEnabled()) ? true : false);
				m_CheckResonance->setChecked((pIns->IsResonanceEnabled()) ? true : false);
				m_SliderVolSwing->setValue(pIns->nVolSwing);
				m_SliderPanSwing->setValue(pIns->nPanSwing);
				m_SliderResSwing->setValue(pIns->nResSwing);
				m_SliderCutSwing->setValue(pIns->nCutSwing);
				m_SliderCutOff->setValue(pIns->GetCutoff());
				m_SliderResonance->setValue(pIns->GetResonance());
				UpdateFilterText();
			}
			// Volume ramping (attack)
			int n = pIns->nVolRampUp; //? MAX_ATTACK_LENGTH - pIns->nVolRampUp : 0;
			m_SliderAttack->setValue(n);
			if(n == 0) SetItemText(IDC_EDIT2, UL_("default"));
			else SetItemInt(IDC_EDIT2, n);

			UpdateTuningComboBox();

			// Only enable Pitch/Tempo Lock for MPTM files or legacy files that have this property enabled.
			m_CheckPitchTempoLock->setEnabled((m_sndFile.GetType() == MOD_TYPE_MPT || pIns->pitchToTempoLock.GetRaw() > 0) ? true : false);
			SetChecked(IDC_CHECK_PITCHTEMPOLOCK, pIns->pitchToTempoLock.GetRaw() > 0 ? 1 : 0);
			m_EditPitchTempoLock->setEnabled(pIns->pitchToTempoLock.GetRaw() > 0 ? true : false);
			if(pIns->pitchToTempoLock.GetRaw() > 0)
			{
				m_EditPitchTempoLock->SetTempoValue(pIns->pitchToTempoLock);
			}

			// Pitch Wheel Depth
			SetItemInt(IDC_PITCHWHEELDEPTH, pIns->midiPWD);

			if(m_sndFile.GetType() & (MOD_TYPE_XM|MOD_TYPE_IT|MOD_TYPE_MPT))
			{
				bool enableVol = (m_CbnMixPlug->GetSelection() != PLUGINDEX_INVALID && !m_sndFile.m_playBehaviour[kMIDICCBugEmulation]) ? true : false;
				velocityStyle.EnableWindow(enableVol);
				m_CbnPluginVolumeHandling->setEnabled(enableVol);
			}
		} else
		{
			mfcless::SetWindowText(m_EditFileName, UL_(""));
			velocityStyle.EnableWindow(false);
			m_CbnPluginVolumeHandling->setEnabled(false);
			if(m_nInstrument > m_sndFile.GetNumInstruments())
				SetCurrentInstrument(m_sndFile.GetNumInstruments());

		}
		m_NoteMap->update(false);

		m_ComboNNA->update(false);
		m_ComboDCT->update(false);
		m_ComboDCA->update(false);
		m_ComboPPC->update(false);
		m_CbnMidiCh->update(false);
		m_CbnMixPlug->update(false);
		m_CbnResampling->update(false);
		m_CbnFilterMode->update(false);
		m_CbnPluginVolumeHandling->update(false);
		m_ComboTuning->update(false);
	}

	if(!m_initialized)
	{
		// First update
		m_initialized = true;
		UnlockControls();
	}

	UnlockControls();
}


void CCtrlInstruments::UpdateFilterText()
{
	if(m_nInstrument)
	{
		ModInstrument *pIns = m_sndFile.Instruments[m_nInstrument];
		if(pIns)
		{
			mpt::uchar s[32];
			// In IT Compatible mode, it is enough to just have resonance enabled to turn on the filter.
			const bool resEnabled = (pIns->IsResonanceEnabled() && pIns->GetResonance() > 0 && m_sndFile.m_playBehaviour[kITFilterBehaviour]);

			if((pIns->IsCutoffEnabled() && pIns->GetCutoff() < 0x7F) || resEnabled)
			{
				const auto cutoff = (resEnabled && !pIns->IsCutoffEnabled()) ? 0x7F : pIns->GetCutoff();
				wsprintf(s, UL_("Z%02X (%u Hz)"), cutoff, mpt::saturate_round<int32>(m_sndFile.CutOffToFrequency(cutoff)));
			} else if(pIns->IsCutoffEnabled())
			{
				_tcscpy(s, UL_("Z7F (Off)"));
			} else
			{
				_tcscpy(s, UL_("No Change"));
			}

			SetItemText(IDC_FILTERTEXT, s);
		}
	}
}


bool CCtrlInstruments::OpenInstrument(const mpt::PathString &fileName)
{
	mfcless::BeginWaitCursor();
	mpt::IO::InputFile f(fileName, TrackerSettings::Instance().MiscCacheCompleteFileBeforeLoading);
	if(!f.IsValid())
	{
		mfcless::EndWaitCursor();
		return false;
	}

	FileReader file = GetFileReader(f);

	bool first = false, ok = false;
	if (file.IsValid())
	{
		if (!m_sndFile.GetNumInstruments())
		{
			first = true;
			m_sndFile.m_nInstruments = 1;
			m_modDoc.SetModified();
		}
		if (!m_nInstrument) m_nInstrument = 1;
		ScopedLogCapturer log(m_modDoc, UL_("Instrument Import"), this);
		PrepareUndo("Replace Instrument");
		if (m_sndFile.ReadInstrumentFromFile(m_nInstrument, file, TrackerSettings::Instance().m_MayNormalizeSamplesOnLoad))
		{
			ok = true;
		} else
		{
			m_modDoc.GetInstrumentUndo().RemoveLastUndoStep(m_nInstrument);
		}
	}

	if(!ok && first)
	{
		// Undo adding the instrument
		delete m_sndFile.Instruments[1];
		m_sndFile.m_nInstruments = 0;
	} else if(ok && first)
	{
		m_NoteMap->SetCurrentInstrument(1);
	}

	mfcless::EndWaitCursor();
	if(ok)
	{
		TrackerSettings::Instance().PathInstruments.SetWorkingDir(fileName, true);
		ModInstrument *pIns = m_sndFile.Instruments[m_nInstrument];
		if (pIns)
		{
			mpt::PathString name, ext;
			fileName.SplitPath(nullptr, nullptr, nullptr, &name, &ext);

			if (!pIns->name[0] && m_sndFile.GetModSpecifications().instrNameLengthMax > 0)
			{
				pIns->name = mpt::truncate(name.ToLocale(), m_sndFile.GetModSpecifications().instrNameLengthMax);
			}
			if (!pIns->filename[0] && m_sndFile.GetModSpecifications().instrFilenameLengthMax > 0)
			{
				name += ext;
				pIns->filename = mpt::truncate(name.ToLocale(), m_sndFile.GetModSpecifications().instrFilenameLengthMax);
			}

			SetCurrentInstrument(m_nInstrument);
			InstrumentHint hint = InstrumentHint().Info().Envelope().Names();
			if(first) hint.ModType();
			SetModified(hint, true);
		} else ok = false;
	} else
	{
		// Try loading as module
		ok = CMainFrame::GetMainFrame()->SetTreeSoundfile(file);
		if(ok) return true;
	}
	SampleHint hint = SampleHint().Info().Data().Names();
	if (first) hint.ModType();
	m_modDoc.UpdateAllViews(nullptr, hint);
	if (!ok) ErrorBox(IDS_ERR_FILETYPE, this);
	return ok;
}


bool CCtrlInstruments::OpenInstrument(const CSoundFile &sndFile, INSTRUMENTINDEX nInstr)
{
	if((!nInstr) || (nInstr > sndFile.GetNumInstruments())) return false;
	mfcless::BeginWaitCursor();

	CriticalSection cs;

	bool first = false;
	if (!m_sndFile.GetNumInstruments())
	{
		first = true;
		m_sndFile.m_nInstruments = 1;
		SetCurrentInstrument(1);
		first = true;
	}
	PrepareUndo("Replace Instrument");
	m_sndFile.ReadInstrumentFromSong(m_nInstrument, sndFile, nInstr);

	cs.Leave();

	if (m_sndFile.Instruments[m_nInstrument] && m_sndFile.Instruments[m_nInstrument]->pTuning)
		BuildTuningComboBox();

	{
		InstrumentHint hint = InstrumentHint().Info().Envelope().Names();
		if (first) hint.ModType();
		SetModified(hint, true);
	}
	{
		SampleHint hint = SampleHint().Info().Data().Names();
		if (first) hint.ModType();
		m_modDoc.UpdateAllViews(nullptr, hint, this);
	}
	mfcless::EndWaitCursor();
	return true;
}


bool CCtrlInstruments::EditSample(uint32 nSample)
{
	if ((nSample > 0) && (nSample < MAX_SAMPLES))
	{
		m_parent.PostMessage(WM_MOD_ACTIVATEVIEW, IDD_CONTROL_SAMPLES, nSample);
		return true;
	}
	return false;
}


mpt::ustring CCtrlInstruments::GetToolTipText(uint32 uId, QWidget *) const
{
	mpt::ustring s;
	if(uId)
	{
		QWidget *wnd = GetDlgItem(uId);
		bool isEnabled = wnd == nullptr || wnd->IsWindowEnabled() != false;
		if(!isEnabled && !m_sndFile.GetNumInstruments())
		{
			s = UL_("Create a new instrument to enable instrument mode.");
			return s;
		}

		ModInstrument *pIns = m_sndFile.Instruments[m_nInstrument];
		if(pIns == nullptr)
			return s;

		const auto plusMinus = mpt::ToUnicode(mpt::Charset::UTF8, "\xC2\xB1");
		CommandID cmd = kcNull;
		switch(uId)
		{
		case IDC_INSTRUMENT_NEW: s = UL_("Insert Instrument (Hold Shift to duplicate)"); cmd = kcInstrumentNew; break;
		case IDC_INSTRUMENT_OPEN: s = UL_("Import Instrument"); cmd = kcInstrumentLoad; break;
		case IDC_INSTRUMENT_SAVEAS: s = UL_("Save Instrument"); cmd = kcInstrumentSave; break;
		case IDC_INSTRUMENT_PLAY: s = UL_("Play Instrument"); break;

		case IDC_EDIT_PITCHTEMPOLOCK:
		case IDC_CHECK_PITCHTEMPOLOCK:
			// Pitch/Tempo lock
			if(isEnabled)
			{
				const CModSpecifications &specs = m_sndFile.GetModSpecifications();
				s = MPT_UFORMAT("Tempo Range: {} - {}")(specs.GetTempoMin().GetInt(), specs.GetTempoMax().GetInt());
			} else
			{
				s = UL_("Only available in MPTM format");
			}
			break;
		case IDC_EDIT7:
			// Fade Out
			if(!pIns->nFadeOut)
				s =UL_("Fade disabled");
			else
				s = MPT_UFORMAT("{} ticks (Higher value <-> Faster fade out)")(0x8000 / pIns->nFadeOut);
			break;
		case IDC_EDIT8:
			// Global volume
			if(isEnabled)
				s = CModDoc::LinearToDecibelsString(GetItemInt(IDC_EDIT8), 64.0);
			else
				s = UL_("Only available in IT / MPTM format");
			break;
		case IDC_EDIT9:
			// Panning
			if(isEnabled)
				s = CModDoc::PanningToString(pIns->nPan, 128);
			else
				s = UL_("Only available in IT / MPTM format");
			break;

#ifndef NO_PLUGINS
		case IDC_EDIT10:
		case IDC_EDIT11:
			// Show plugin program name when hovering program or bank edits
			if(pIns->nMixPlug > 0 && pIns->nMidiProgram != 0)
			{
				const SNDMIXPLUGIN &plugin = m_sndFile.m_MixPlugins[pIns->nMixPlug - 1];
				if(plugin.pMixPlugin != nullptr)
				{
					int32 prog = pIns->nMidiProgram - 1;
					if(pIns->wMidiBank > 1) prog += 128 * (pIns->wMidiBank - 1);
					s = plugin.pMixPlugin->GetFormattedProgramName(prog);
				}
			}
			break;
#endif // NO_PLUGINS

		case IDC_PLUGIN_VELOCITYSTYLE:
		case IDC_PLUGIN_VOLUMESTYLE:
			// Plugin volume handling
			if(pIns->nMixPlug < 1)
				return s;
			if(m_sndFile.m_playBehaviour[kMIDICCBugEmulation])
			{
				s = UL_("To enable, clear Plugin volume command bug emulation flag from Song Properties");
			} else
			{
				if(uId == IDC_PLUGIN_VELOCITYSTYLE)
					s = UL_("Volume commands (vxx) next to a note are sent as note velocity instead.");
			}
			break;
		case IDC_COMBO5:
			// MIDI Channel
			s = UL_("Mapped: MIDI channel corresponds to pattern channel modulo 16");
			break;
		case IDC_SLIDER1:
			if(isEnabled)
				s = MPT_UFORMAT("{}{}% volume variation")(plusMinus, pIns->nVolSwing);
			else
				s = UL_("Only available in IT / MPTM format");
			break;
		case IDC_SLIDER2:
			if(isEnabled)
				s = MPT_UFORMAT("{}{} panning variation")(plusMinus, pIns->nPanSwing);
			else
				s = UL_("Only available in IT / MPTM format");
			break;
		case IDC_SLIDER3:
			if(isEnabled)
				s = mpt::ufmt::val(pIns->GetCutoff());
			else
				s = UL_("Only available in IT / MPTM format");
			break;
		case IDC_SLIDER4:
			if(isEnabled)
				s = MPT_UFORMAT("{} ({} dB)")(pIns->GetResonance(), Util::muldivr(pIns->GetResonance(), 24, 128));
			else
				s = UL_("Only available in IT / MPTM format");
			break;
		case IDC_SLIDER6:
			if(isEnabled)
				s = MPT_UFORMAT("{}{} cutoff variation")(plusMinus, pIns->nCutSwing);
			else
				s = UL_("Only available in MPTM format");
			break;
		case IDC_SLIDER7:
			if(isEnabled)
				s = MPT_UFORMAT("{}{} resonance variation")(plusMinus, pIns->nResSwing);
			else
				s = UL_("Only available in MPTM format");
			break;
		case IDC_PITCHWHEELDEPTH:
			s = UL_("Set this to the actual Pitch Wheel Depth used in your plugin on this channel.");
			break;

		case IDC_INSVIEWPLG:	// Open Editor
			if(!isEnabled)
				s = UL_("No Plugin Loaded");
			break;

		case IDC_SPIN9:		// Pan
		case IDC_CHECK1:	// Pan
		case IDC_COMBO1:	// NNA
		case IDC_COMBO2:	// DCT
		case IDC_COMBO3:	// DNA
		case IDC_COMBO4:	// PPC
		case IDC_SPIN12:	// PPS
		case IDC_EDIT15:	// PPS
			if(!isEnabled)
				s = UL_("Only available in IT / MPTM format");
			break;

		case IDC_COMBOTUNING:	// Tuning
		case IDC_COMBO9:		// Resampling:
		case IDC_SLIDER5:		// Ramping
		case IDC_SPIN1:			// Ramping
		case IDC_EDIT2:			// Ramping
			if(!isEnabled)
				s = UL_("Only available in MPTM format");
			break;

		}

		if(cmd != kcNull)
		{
			auto keyText = CMainFrame::GetInputHandler()->m_activeCommandSet->GetKeyTextFromCommand(cmd, 0);
			if (!keyText.IsEmpty())
				s += MPT_UFORMAT(" ({})")(keyText);
		}
	}
	return s;
}


////////////////////////////////////////////////////////////////////////////
// CCtrlInstruments Messages

void CCtrlInstruments::OnInstrumentChanged()
{
	if(!IsLocked())
	{
		uint32 n = GetItemInt(IDC_EDIT_INSTRUMENT);
		if ((n > 0) && (n <= m_sndFile.GetNumInstruments()) && (n != m_nInstrument))
		{
			SetCurrentInstrument(n, false);
			m_parent.InstrumentChanged(n);
		}
	}
}


void CCtrlInstruments::OnPrevInstrument()
{
	if(m_nInstrument > 1)
		SetCurrentInstrument(m_nInstrument - 1);
	else
		SetCurrentInstrument(m_sndFile.GetNumInstruments());
	m_parent.InstrumentChanged(m_nInstrument);
}


void CCtrlInstruments::OnNextInstrument()
{
	if(m_nInstrument < m_sndFile.GetNumInstruments())
		SetCurrentInstrument(m_nInstrument + 1);
	else
		SetCurrentInstrument(1);
	m_parent.InstrumentChanged(m_nInstrument);
}


void CCtrlInstruments::OnInstrumentNew()
{
	InsertInstrument(m_sndFile.GetNumInstruments() > 0 && CInputHandler::ShiftPressed());
	SwitchToViewIfMouse();
}


bool CCtrlInstruments::InsertInstrument(bool duplicate)
{
	const bool hasInstruments = m_sndFile.GetNumInstruments() > 0;

	INSTRUMENTINDEX ins = m_modDoc.InsertInstrument(SAMPLEINDEX_INVALID, (duplicate && hasInstruments) ? m_nInstrument : INSTRUMENTINDEX_INVALID);
	if (ins == INSTRUMENTINDEX_INVALID)
		return false;

	if (!hasInstruments) m_modDoc.UpdateAllViews(nullptr, InstrumentHint().Info().Names().ModType());

	SetCurrentInstrument(ins);
	m_modDoc.UpdateAllViews(nullptr, InstrumentHint(ins).Info().Envelope().Names());
	m_parent.InstrumentChanged(m_nInstrument);
	return true;
}


void CCtrlInstruments::OnInstrumentOpen()
{
	static int nLastIndex = 0;

	std::vector<FileType> mediaFoundationTypes = CSoundFile::GetMediaFoundationFileTypes();
	FileDialog dlg = OpenFileDialog()
		.AllowMultiSelect()
		.EnableAudioPreview()
		.ExtensionFilter(
			"All Instruments (*.xi,*.pat,*.iti,*.sfz,...)|*.xi;*.pat;*.iti;*.sfz;*.flac;*.wav;*.w64;*.caf;*.aif;*.aiff;*.au;*.snd;*.sbk;*.sf2;*.sf3;*.sf4;*.dls;*.oga;*.ogg;*.opus;*.s3i;*.sb0;*.sb2;*.sbi;*.brr" + ToFilterOnlyString(mediaFoundationTypes, true).ToLocale() + "|"
			"FastTracker II Instruments (*.xi)|*.xi|"
			"GF1 Patches (*.pat)|*.pat|"
			"Impulse Tracker Instruments (*.iti)|*.iti|"
			"SFZ Instruments (*.sfz)|*.sfz|"
			"SoundFont 2.0 Banks (*.sf2)|*.sbk;*.sf2;*.sf3;*.sf4|"
			"DLS Sound Banks (*.dls)|*.dls|"
			"All Files (*.*)|*.*||")
		.WorkingDirectory(TrackerSettings::Instance().PathInstruments.GetWorkingDir())
		.FilterIndex(&nLastIndex);
	if(!dlg.Show(this)) return;

	TrackerSettings::Instance().PathInstruments.SetWorkingDir(dlg.GetWorkingDirectory());

	const FileDialog::PathList &files = dlg.GetFilenames();
	for(size_t counter = 0; counter < files.size(); counter++)
	{
		//If loading multiple instruments, advancing to next instrument and creating
		//new instrument if necessary.
		if(counter > 0)
		{
			if(m_nInstrument >= MAX_INSTRUMENTS - 1)
				break;
			else
				m_nInstrument++;

			if(m_nInstrument > m_sndFile.GetNumInstruments())
				OnInstrumentNew();
		}

		if(!OpenInstrument(files[counter]))
			ErrorBox(IDS_ERR_FILEOPEN, this);
	}

	m_parent.InstrumentChanged(m_nInstrument);
	SwitchToViewIfMouse();
}


void CCtrlInstruments::OnInstrumentSave()
{
	SaveInstrument(CInputHandler::ShiftPressed());
}


void CCtrlInstruments::SaveInstrument(bool doBatchSave)
{
	if(!doBatchSave && m_sndFile.Instruments[m_nInstrument] == nullptr)
	{
		SwitchToViewIfMouse();
		return;
	}

	mpt::PathString fileName;
	if(!doBatchSave)
	{
		const ModInstrument *pIns = m_sndFile.Instruments[m_nInstrument];
		if(pIns->filename[0])
			fileName = mpt::PathString::FromLocale(pIns->filename);
		else
			fileName = mpt::PathString::FromLocale(pIns->name);
	} else
	{
		// Save all samples
		fileName = m_sndFile.GetpModDoc()->GetPathNameMpt().GetFilenameBase();
		if(fileName.empty()) fileName = P_("untitled");

		fileName += P_(" - %instrument_number% - ");
		if(m_sndFile.GetModSpecifications().sampleFilenameLengthMax == 0)
			fileName += P_("%instrument_name%");
		else
			fileName += P_("%instrument_filename%");

	}
	fileName = fileName.AsSanitizedComponent();

	int index;
	if(TrackerSettings::Instance().compressITI)
		index = 2;
	else if(m_sndFile.GetType() == MOD_TYPE_XM)
		index = 4;
	else
		index = 1;

	FileDialog dlg = SaveFileDialog()
		.DefaultExtension(m_sndFile.GetType() == MOD_TYPE_XM ? U_("xi") : U_("iti"))
		.DefaultFilename(fileName)
		.ExtensionFilter(
			"Impulse Tracker Instruments (*.iti)|*.iti|"
			"Compressed Impulse Tracker Instruments (*.iti)|*.iti|"
			"Impulse Tracker Instruments with external Samples (*.iti)|*.iti|"
			"FastTracker II Instruments (*.xi)|*.xi|"
			"SFZ Instruments with WAV (*.sfz)|*.sfz|"
			"SFZ Instruments with FLAC (*.sfz)|*.sfz||")
		.WorkingDirectory(TrackerSettings::Instance().PathInstruments.GetWorkingDir())
		.FilterIndex(&index);
	if(!dlg.Show(this)) return;

	mfcless::BeginWaitCursor();

	INSTRUMENTINDEX minIns = m_nInstrument, maxIns = m_nInstrument;
	if(doBatchSave)
	{
		minIns = 1;
		maxIns = m_sndFile.GetNumInstruments();
	}
	auto numberFmt = mpt::format_simple_spec<mpt::ustring>().Dec().FillNul().Width(1 + static_cast<int>(std::log10(maxIns)));

	bool ok = true;
	const bool saveXI = !mpt::PathCompareNoCase(dlg.GetExtension(), P_("xi"));
	const bool saveSFZ = !mpt::PathCompareNoCase(dlg.GetExtension(), P_("sfz"));
	const bool doCompress = index == 2 || index == 6;
	const bool allowExternal = index == 3;

	for(INSTRUMENTINDEX ins = minIns; ins <= maxIns; ins++)
	{
		const ModInstrument *pIns = m_sndFile.Instruments[ins];
		if(pIns != nullptr)
		{
			fileName = dlg.GetFirstFile();
			if(doBatchSave)
			{
				mpt::ustring instrName = mpt::ToUnicode(m_sndFile.GetCharsetInternal(), pIns->name[0] ? pIns->GetName() : "untitled");
				mpt::ustring instrFilename = mpt::ToUnicode(m_sndFile.GetCharsetInternal(), pIns->filename[0] ? pIns->GetFilename() : pIns->GetName());
				instrName = mpt::SanitizePathComponent(instrName);
				instrFilename = mpt::SanitizePathComponent(instrFilename);

				mpt::ustring fileNameW = fileName.ToUnicode();
				fileNameW = mpt::replace(fileNameW, U_("%instrument_number%"), mpt::ufmt::fmt(ins, numberFmt));
				fileNameW = mpt::replace(fileNameW, U_("%instrument_filename%"), instrFilename);
				fileNameW = mpt::replace(fileNameW, U_("%instrument_name%"), instrName);
				fileName = mpt::PathString::FromUnicode(fileNameW);
			}

			try
			{
				ScopedLogCapturer logcapturer(m_modDoc);
				mpt::IO::SafeOutputFile sf(fileName, std::ios::binary, mpt::IO::FlushModeFromBool(TrackerSettings::Instance().MiscFlushFileBuffersOnSave));
				mpt::IO::ofstream &f = sf;
				if(!f)
				{
					ok = false;
					continue;
				}
				f.exceptions(f.exceptions() | std::ios::badbit | std::ios::failbit);

				if (saveXI)
					ok &= m_sndFile.SaveXIInstrument(ins, f);
				else if (saveSFZ)
					ok &= m_sndFile.SaveSFZInstrument(ins, f, fileName, doCompress);
				else
					ok &= m_sndFile.SaveITIInstrument(ins, f, fileName, doCompress, allowExternal);
			} catch(const std::exception &)
			{
				ok = false;
			}
		}
	}

	mfcless::EndWaitCursor();
	if (!ok)
		ErrorBox(IDS_ERR_SAVEINS, this);
	else
		TrackerSettings::Instance().PathInstruments.SetWorkingDir(dlg.GetWorkingDirectory());
	SwitchToViewIfMouse();
}


void CCtrlInstruments::OnInstrumentPlay()
{
	if (m_modDoc.IsNotePlaying(NOTE_MIDDLEC, 0, m_nInstrument))
	{
		m_modDoc.NoteOff(NOTE_MIDDLEC, true, m_nInstrument);
	} else
	{
		m_modDoc.PlayNote(PlayNoteParam(NOTE_MIDDLEC).Instrument(m_nInstrument));
	}
	SwitchToViewIfMouse();
}


void CCtrlInstruments::OnNameChanged()
{
	if (!IsLocked())
	{
		mpt::ustring tmp;
		mfcless::GetWindowText(m_EditName, tmp);
		const std::string s = mpt::ToCharset(m_sndFile.GetCharsetInternal(), tmp);
		ModInstrument *pIns = m_sndFile.Instruments[m_nInstrument];
		if ((pIns) && (s != pIns->name))
		{
			if(!m_startedEdit) PrepareUndo("Set Name");
			pIns->name = s;
			SetModified(InstrumentHint().Names(), false);
		}
	}
}


void CCtrlInstruments::OnFileNameChanged()
{
	if (!IsLocked())
	{
		mpt::ustring tmp;
		mfcless::GetWindowText(m_EditFileName, tmp);
		const std::string s = mpt::ToCharset(m_sndFile.GetCharsetInternal(), tmp);
		ModInstrument *pIns = m_sndFile.Instruments[m_nInstrument];
		if ((pIns) && (s != pIns->filename))
		{
			if(!m_startedEdit) PrepareUndo("Set Filename");
			pIns->filename = s;
			SetModified(InstrumentHint().Names(), false);
		}
	}
}


void CCtrlInstruments::OnFadeOutVolChanged()
{
	ModInstrument *pIns = m_sndFile.Instruments[m_nInstrument];
	if ((!IsLocked()) && (pIns))
	{
		int minval = 0, maxval = 32767;
		m_SpinFadeOut->GetRange(minval, maxval);
		int nVol = GetItemInt(IDC_EDIT7);
		Limit(nVol, minval, maxval);

		if(nVol != (int)pIns->nFadeOut)
		{
			if(!m_startedEdit) PrepareUndo("Set Fade Out");
			pIns->nFadeOut = nVol;
			SetModified(InstrumentHint().Info(), false);
		}
	}
}


void CCtrlInstruments::OnGlobalVolChanged()
{
	ModInstrument *pIns = m_sndFile.Instruments[m_nInstrument];
	if ((!IsLocked()) && (pIns))
	{
		int nVol = GetItemInt(IDC_EDIT8);
		Limit(nVol, 0, 64);
		if (nVol != (int)pIns->nGlobalVol)
		{
			if(!m_startedEdit) PrepareUndo("Set Global Volume");
			// Live-adjust volume
			pIns->nGlobalVol = nVol;
			for(auto &chn : m_sndFile.m_PlayState.Chn)
			{
				if(chn.pModInstrument == pIns)
				{
					chn.UpdateInstrumentVolume(chn.pModSample, pIns);
				}
			}
			SetModified(InstrumentHint().Info(), false);
		}
	}
}


void CCtrlInstruments::OnSetPanningChanged()
{
	ModInstrument *pIns = m_sndFile.Instruments[m_nInstrument];
	if ((!IsLocked()) && (pIns))
	{
		const bool b = m_CheckPanning->isChecked() != 0;

		PrepareUndo("Toggle Panning");
		pIns->dwFlags.set(INS_SETPANNING, b);

		if(b && m_sndFile.GetType() & (MOD_TYPE_IT|MOD_TYPE_MPT))
		{
			bool smpPanningInUse = false;

			const std::set<SAMPLEINDEX> referencedSamples = pIns->GetSamples();

			for(auto sample : referencedSamples)
			{
				if(sample <= m_sndFile.GetNumSamples() && m_sndFile.GetSample(sample).uFlags[CHN_PANNING])
				{
					smpPanningInUse = true;
					break;
				}
			}

			if(smpPanningInUse)
			{
				if(Reporting::Confirm(_T("Some of the samples used in the instrument have \"Set Pan\" enabled. "
						"Sample panning overrides instrument panning for the notes associated with such samples. "
						"Do you wish to disable panning from those samples so that the instrument pan setting is effective "
						"for the whole instrument?")) == cnfYes)
				{
					for (auto sample : referencedSamples)
					{
						if(sample <= m_sndFile.GetNumSamples())
							m_sndFile.GetSample(sample).uFlags.reset(CHN_PANNING);
					}
					m_modDoc.UpdateAllViews(nullptr, SampleHint().Info().ModType(), this);
				}
			}
		}
		SetModified(InstrumentHint().Info(), false);
	}
}


void CCtrlInstruments::OnPanningChanged()
{
	ModInstrument *pIns = m_sndFile.Instruments[m_nInstrument];
	if ((!IsLocked()) && (pIns))
	{
		int nPan = GetItemInt(IDC_EDIT9);
		if(m_modDoc.GetModType() & MOD_TYPE_IT)	// IT panning ranges from 0 to 64
			nPan *= 4;
		if (nPan < 0) nPan = 0;
		if (nPan > 256) nPan = 256;
		if (nPan != (int)pIns->nPan)
		{
			if(!m_startedEdit) PrepareUndo("Set Panning");
			pIns->nPan = nPan;
			SetModified(InstrumentHint().Info(), false);
		}
	}
}


void CCtrlInstruments::OnNNAChanged()
{
	ModInstrument *pIns = m_sndFile.Instruments[m_nInstrument];
	if ((!IsLocked()) && (pIns))
	{
		const auto nna = static_cast<NewNoteAction>(m_ComboNNA->currentIndex());
		if(pIns->nNNA != nna)
		{
			PrepareUndo("Set New Note Action");
			pIns->nNNA = nna;
			SetModified(InstrumentHint().Info(), false);
		}
	}
}


void CCtrlInstruments::OnDCTChanged()
{
	ModInstrument *pIns = m_sndFile.Instruments[m_nInstrument];
	if ((!IsLocked()) && (pIns))
	{
		const auto dct = static_cast<DuplicateCheckType>(m_ComboDCT->currentIndex());
		if(pIns->nDCT != dct)
		{
			PrepareUndo("Set Duplicate Check Type");
			pIns->nDCT = dct;
			SetModified(InstrumentHint().Info(), false);
		}
	}
}


void CCtrlInstruments::OnDCAChanged()
{
	ModInstrument *pIns = m_sndFile.Instruments[m_nInstrument];
	if ((!IsLocked()) && (pIns))
	{
		const auto dna = static_cast<DuplicateNoteAction>(m_ComboDCA->currentIndex());
		if (pIns->nDNA != dna)
		{
			PrepareUndo("Set Duplicate Check Action");
			pIns->nDNA = dna;
			SetModified(InstrumentHint().Info(), false);
		}
	}
}


void CCtrlInstruments::OnMPRChanged()
{
	ModInstrument *pIns = m_sndFile.Instruments[m_nInstrument];
	if ((!IsLocked()) && (pIns))
	{
		int n = GetItemInt(IDC_EDIT10);
		if ((n >= 0) && (n <= 128))
		{
			if (pIns->nMidiProgram != n)
			{
				if(!m_startedEdit) PrepareUndo("Set MIDI Program");
				pIns->nMidiProgram = static_cast<uint8>(n);
				SetModified(InstrumentHint().Info(), false);
			}
		}
		// we will not set the midi bank/program if it is 0
		if(n == 0)
		{
			LockControls();
			SetItemText(IDC_EDIT10, UL_("---"));
			UnlockControls();
		}
	}
}


void CCtrlInstruments::OnMPRKillFocus()
{
	ModInstrument *pIns = m_sndFile.Instruments[m_nInstrument];
	if ((!IsLocked()) && (pIns))
	{
		int n = GetItemInt(IDC_EDIT10);
		if (n > 128)
		{
			n--;
			pIns->nMidiProgram = static_cast<uint8>(n % 128 + 1);
			pIns->wMidiBank = static_cast<uint16>(n / 128 + 1);
			SetModified(InstrumentHint().Info(), false);

			LockControls();
			SetItemInt(IDC_EDIT10, pIns->nMidiProgram);
			SetItemInt(IDC_EDIT11, pIns->wMidiBank);
			UnlockControls();
		}
	}
}


void CCtrlInstruments::OnMBKChanged()
{
	ModInstrument *pIns = m_sndFile.Instruments[m_nInstrument];
	if ((!IsLocked()) && (pIns))
	{
		uint16 w = static_cast<uint16>(GetItemInt(IDC_EDIT11));
		if(w >= 0 && w <= 16384 && pIns->wMidiBank != w)
		{
			if(!m_startedEdit) PrepareUndo("Set MIDI Bank");
			pIns->wMidiBank = w;
			SetModified(InstrumentHint().Info(), false);
		}
		// we will not set the midi bank/program if it is 0
		if(w == 0)
		{
			LockControls();
			SetItemText(IDC_EDIT11, UL_("---"));
			UnlockControls();
		}
	}
}


void CCtrlInstruments::OnMCHChanged()
{
	ModInstrument *pIns = m_sndFile.Instruments[m_nInstrument];
	if(!IsLocked() && pIns)
	{
		uint8 ch = static_cast<uint8>(mfcless::GetItemData(m_CbnMidiCh, m_CbnMidiCh->currentIndex()));
		if(pIns->nMidiChannel != ch)
		{
			PrepareUndo("Set MIDI Channel");
			pIns->nMidiChannel = ch;
			SetModified(InstrumentHint().Info(), false);
		}
	}
}

void CCtrlInstruments::OnResamplingChanged()
{
	ModInstrument *pIns = m_sndFile.Instruments[m_nInstrument];
	if ((!IsLocked()) && (pIns))
	{
		ResamplingMode n = static_cast<ResamplingMode>(mfcless::GetItemData(m_CbnResampling, m_CbnResampling->currentIndex()));
		if (pIns->resampling != n)
		{
			PrepareUndo("Set Resampling");
			pIns->resampling = n;
			SetModified(InstrumentHint().Info(), false);
		}
	}
}


void CCtrlInstruments::OnMixPlugChanged()
{
	ModInstrument *pIns = m_sndFile.Instruments[m_nInstrument];
	const PLUGINDEX nPlug = m_CbnMixPlug->GetSelection().value_or(PLUGINDEX_INVALID);

	bool wasOpenedWithMouse = m_openendPluginListWithMouse;
	m_openendPluginListWithMouse = false;

	if(pIns)
	{
		bool enableVol = (nPlug == PLUGINDEX_INVALID || m_sndFile.m_playBehaviour[kMIDICCBugEmulation]) ? false : true;
		velocityStyle.EnableWindow(enableVol);
		m_CbnPluginVolumeHandling->setEnabled(enableVol);

		const PLUGINDEX mixPlug = (nPlug != PLUGINDEX_INVALID) ? nPlug + 1 : 0;
		if(mixPlug <= MAX_MIXPLUGINS)
		{
			bool active = !IsLocked();
			if(active && pIns->nMixPlug != mixPlug)
			{
				PrepareUndo("Set Plugin");
				pIns->nMixPlug = mixPlug;
				SetModified(InstrumentHint().Info(), false);
			}

			velocityStyle.SetCheck(pIns->pluginVelocityHandling == PLUGIN_VELOCITYHANDLING_CHANNEL ? 1 : 0);
			m_CbnPluginVolumeHandling->setCurrentIndex(pIns->pluginVolumeHandling);

#ifndef NO_PLUGINS
			if(pIns->nMixPlug)
			{
				// we have selected a plugin that's not "no plugin"
				const SNDMIXPLUGIN &plugin = m_sndFile.m_MixPlugins[pIns->nMixPlug - 1];

				if(!plugin.IsValidPlugin() && active && wasOpenedWithMouse)
				{
					// No plugin in this slot yet: Ask user to add one.
					CSelectPluginDlg dlg(&m_modDoc, nPlug, this);
					if (dlg.DoModal() == IDOK)
					{
						if(m_sndFile.GetModSpecifications().supportsPlugins)
						{
							m_modDoc.SetModified();
						}
						m_modDoc.UpdateAllViews(nullptr, PluginHint(mixPlug).Info().Names());
					}
				}

				if(plugin.pMixPlugin != nullptr)
				{
					EnableItem(IDC_INSVIEWPLG, true);

					if(active && plugin.pMixPlugin->IsInstrument())
					{
						if(pIns->nMidiChannel == MidiNoChannel)
						{
							// If this plugin can recieve MIDI events and we have no MIDI channel
							// selected for this instrument, automatically select MIDI channel 1.
							pIns->nMidiChannel = MidiFirstChannel;
							UpdateView(InstrumentHint(m_nInstrument).Info());
						}
						if(pIns->midiPWD == 0)
						{
							pIns->midiPWD = 2;
						}

						// If we just dialled up an instrument plugin, zap the sample assignments.
						const std::set<SAMPLEINDEX> referencedSamples = pIns->GetSamples();
						bool hasSamples = false;
						for(auto sample : referencedSamples)
						{
							if(sample > 0 && sample <= m_sndFile.GetNumSamples() && m_sndFile.GetSample(sample).HasSampleData())
							{
								hasSamples = true;
								break;
							}
						}

						if(!hasSamples || Reporting::Confirm("Remove sample associations of this instrument?") == cnfYes)
						{
							pIns->AssignSample(0);
							m_NoteMap->update();
							UpdateView(InstrumentHint(m_nInstrument).Info());
						}
					}
					return;
				}
			}
#endif // NO_PLUGINS
		}

	}
	::EnableWindow(::GetDlgItem(this, IDC_INSVIEWPLG), false);
}


void CCtrlInstruments::OnPPSChanged()
{
	ModInstrument *pIns = m_sndFile.Instruments[m_nInstrument];
	if ((!IsLocked()) && (pIns))
	{
		int n = GetItemInt(IDC_EDIT15);
		if ((n >= -32) && (n <= 32))
		{
			if (pIns->nPPS != (signed char)n)
			{
				if(!m_startedEdit) PrepareUndo("Set Pitch/Pan Separation");
				pIns->nPPS = (signed char)n;
				SetModified(InstrumentHint().Info(), false);
			}
		}
	}
}


void CCtrlInstruments::OnPPCChanged()
{
	ModInstrument *pIns = m_sndFile.Instruments[m_nInstrument];
	if ((!IsLocked()) && (pIns))
	{
		int n = m_ComboPPC->currentIndex();
		if(n >= 0 && n <= NOTE_MAX - NOTE_MIN)
		{
			if (pIns->nPPC != n)
			{
				PrepareUndo("Set Pitch/Pan Center");
				pIns->nPPC = static_cast<decltype(pIns->nPPC)>(n);
				SetModified(InstrumentHint().Info(), false);
			}
		}
	}
}


void CCtrlInstruments::OnAttackChanged()
{
	ModInstrument *pIns = m_sndFile.Instruments[m_nInstrument];
	if(!IsLocked() && pIns)
	{
		int n = Clamp(static_cast<int>(GetItemInt(IDC_EDIT2)), 0, MAX_ATTACK_VALUE);
		auto newRamp = static_cast<decltype(pIns->nVolRampUp)>(n);
		if(pIns->nVolRampUp != newRamp)
		{
			if(!m_startedEdit)
				PrepareUndo("Set Ramping");
			pIns->nVolRampUp = newRamp;
			SetModified(InstrumentHint().Info(), false);
		}

		m_SliderAttack->setValue(n);
		if(CSpinButtonCtrl *spin = (CSpinButtonCtrl *)GetDlgItem(IDC_SPIN1))
			spin->SetPos(n);
		LockControls();
		if (n == 0) SetItemText(IDC_EDIT2, UL_("default"));
		UnlockControls();
	}
}


void CCtrlInstruments::OnEnableCutOff()
{
	const bool enableCutOff = IsChecked(IDC_CHECK2) != 0;

	ModInstrument *pIns = m_sndFile.Instruments[m_nInstrument];
	if (pIns)
	{
		PrepareUndo("Toggle Cutoff");
		pIns->SetCutoff(pIns->GetCutoff(), enableCutOff);
		m_sndFile.UpdateInstrumentFilter(*pIns, false, true, false);
	}
	UpdateFilterText();
	SetModified(InstrumentHint().Info(), false);
	SwitchToViewIfMouse();
}


void CCtrlInstruments::OnEnableResonance()
{
	const bool enableReso = IsChecked(IDC_CHECK3) != 0;

	ModInstrument *pIns = m_sndFile.Instruments[m_nInstrument];
	if (pIns)
	{
		PrepareUndo("Toggle Resonance");
		pIns->SetResonance(pIns->GetResonance(), enableReso);
		m_sndFile.UpdateInstrumentFilter(*pIns, false, false, true);
	}
	UpdateFilterText();
	SetModified(InstrumentHint().Info(), false);
	SwitchToViewIfMouse();
}

void CCtrlInstruments::OnFilterModeChanged()
{
	ModInstrument *pIns = m_sndFile.Instruments[m_nInstrument];
	if ((!IsLocked()) && (pIns))
	{
		FilterMode instFiltermode = static_cast<FilterMode>(mfcless::GetItemData(m_CbnFilterMode, m_CbnFilterMode->currentIndex()));

		if(pIns->filterMode != instFiltermode)
		{
			PrepareUndo("Set Filter Mode");
			pIns->filterMode = instFiltermode;
			SetModified(InstrumentHint().Info(), false);

			//Update channel settings where this instrument is active, if required.
			if(instFiltermode != FilterMode::Unchanged)
				m_sndFile.UpdateInstrumentFilter(*pIns, true, false, false);
		}
	}
}


void CCtrlInstruments::OnVScroll(mfcless::ScrollCode code, int pos, QWidget *pSB)
{
	// Give focus back to envelope editor when stopping to scroll spin buttons (for instrument preview keyboard focus)
	CModControlDlg::OnVScroll(nCode, nPos, pSB);
	if (nCode == SB_ENDSCROLL) SwitchToViewIfMouse();
}


void CCtrlInstruments::OnHScroll(mfcless::ScrollCode code, int pos, QWidget *pSB)
{
	CModControlDlg::OnHScroll(nCode, nPos, pSB);
	if ((m_nInstrument) && (!IsLocked()) && (nCode != SB_ENDSCROLL))
	{
		ModInstrument *pIns = m_sndFile.Instruments[m_nInstrument];
		if(!pIns)
			return;
			
		auto *pSlider = reinterpret_cast<const CSliderCtrl *>(pSB);
		int32 n = pSlider->GetPos();

		if(pSlider == &m_SliderAttack)
		{
			// Volume ramping (attack)
			if(pIns->nVolRampUp != n)
			{
				if(!m_startedHScroll)
				{
					PrepareUndo("Set Ramping");
					m_startedHScroll = true;
				}
				pIns->nVolRampUp = static_cast<decltype(pIns->nVolRampUp)>(n);
				SetItemInt(IDC_EDIT2, n);
				SetModified(InstrumentHint().Info(), false);
			}
		} else if(pSlider == &m_SliderVolSwing)
		{
			// Volume Swing
			if((n >= 0) && (n <= 100) && (n != pIns->nVolSwing))
			{
				if(!m_startedHScroll)
				{
					PrepareUndo("Set Volume Random Variation");
					m_startedHScroll = true;
				}
				pIns->nVolSwing = static_cast<uint8>(n);
				SetModified(InstrumentHint().Info(), false);
			}
		} else if(pSlider == &m_SliderPanSwing)
		{
			// Pan Swing
			if((n >= 0) && (n <= 64) && (n != pIns->nPanSwing))
			{
				if(!m_startedHScroll)
				{
					PrepareUndo("Set Panning Random Variation");
					m_startedHScroll = true;
				}
				pIns->nPanSwing = static_cast<uint8>(n);
				SetModified(InstrumentHint().Info(), false);
			}
		} else if(pSlider == &m_SliderCutSwing)
		{
			// Cutoff swing
			if((n >= 0) && (n <= 64) && (n != pIns->nCutSwing))
			{
				if(!m_startedHScroll)
				{
					PrepareUndo("Set Cutoff Random Variation");
					m_startedHScroll = true;
				}
				pIns->nCutSwing = static_cast<uint8>(n);
				SetModified(InstrumentHint().Info(), false);
			}
		} else if(pSlider == &m_SliderResSwing)
		{
			// Resonance swing
			if((n >= 0) && (n <= 64) && (n != pIns->nResSwing))
			{
				if(!m_startedHScroll)
				{
					PrepareUndo("Set Resonance Random Variation");
					m_startedHScroll = true;
				}
				pIns->nResSwing = static_cast<uint8>(n);
				SetModified(InstrumentHint().Info(), false);
			}
		} else if(pSlider == &m_SliderCutOff)
		{
			// Filter Cutoff
			if((n >= 0) && (n < 0x80) && (n != (int)(pIns->GetCutoff())))
			{
				if(!m_startedHScroll)
				{
					PrepareUndo("Set Cutoff");
					m_startedHScroll = true;
				}
				pIns->SetCutoff(static_cast<uint8>(n), pIns->IsCutoffEnabled());
				SetModified(InstrumentHint().Info(), false);
				UpdateFilterText();
				CriticalSection cs;
				m_sndFile.UpdateInstrumentFilter(*pIns, false, true, false);
			}
		} else if(pSlider == &m_SliderResonance)
		{
			// Filter Resonance
			if((n >= 0) && (n < 0x80) && (n != (int)(pIns->GetResonance())))
			{
				if(!m_startedHScroll)
				{
					PrepareUndo("Set Resonance");
					m_startedHScroll = true;
				}
				pIns->SetResonance(static_cast<uint8>(n), pIns->IsResonanceEnabled());
				SetModified(InstrumentHint().Info(), false);
				UpdateFilterText();
				CriticalSection cs;
				m_sndFile.UpdateInstrumentFilter(*pIns, false, false, true);
			}
		}
	} else if(nCode == SB_ENDSCROLL)
	{
		m_startedHScroll = false;
	}
	if ((nCode == SB_ENDSCROLL) || (nCode == SB_THUMBPOSITION))
	{
		SwitchToViewIfMouse();
	}

}


void CCtrlInstruments::OnEditSampleMap()
{
	if(m_nInstrument)
	{
		ModInstrument *pIns = m_sndFile.Instruments[m_nInstrument];
		if (pIns)
		{
			PrepareUndo("Edit Sample Map");
			CSampleMapDlg dlg(m_sndFile, m_nInstrument, this);
			if (dlg.DoModal() == IDOK)
			{
				SetModified(InstrumentHint().Info(), true);
				m_NoteMap->update(false);
			} else
			{
				m_modDoc.GetInstrumentUndo().RemoveLastUndoStep(m_nInstrument);
			}
		}
	}
}


void CCtrlInstruments::TogglePluginEditor()
{
	if(m_nInstrument)
	{
		m_modDoc.TogglePluginEditor(m_CbnMixPlug->GetSelection().value_or(PLUGINDEX_INVALID), CInputHandler::ShiftPressed());
	}
}


bool CCtrlInstruments::PreTranslateMessage(mfcless::Message *pMsg)
{
	if(pMsg)
	{
		//We handle keypresses before Windows has a chance to handle them (for alt etc..)
		if ((pMsg->message == mfcless::msg::SysKeyUp)   || (pMsg->message == mfcless::msg::KeyUp) ||
			(pMsg->message == mfcless::msg::SysKeyDown) || (pMsg->message == mfcless::msg::KeyDown))
		{
			CInputHandler *ih = CMainFrame::GetInputHandler();
			if (ih->KeyEvent(kCtxCtrlInstruments, ih->Translate(*pMsg)) != kcNull)
				return true; // Mapped to a command, no need to pass message on.
		}

	}

	return CModControlDlg::PreTranslateMessage(pMsg);
}

intptr_t CCtrlInstruments::OnCustomKeyMsg(uintptr_t wParam, intptr_t /*lParam*/)
{
	switch(wParam)
	{
		case kcInstrumentCtrlLoad: OnInstrumentOpen(); return wParam;
		case kcInstrumentCtrlSave: OnInstrumentSaveOne(); return wParam;
		case kcInstrumentCtrlNew:  InsertInstrument(false); return wParam;
		case kcInstrumentCtrlDuplicate:	InsertInstrument(true); return wParam;
	}

	return kcNull;
}


void CCtrlInstruments::OnCbnSelchangeCombotuning()
{
	if (IsLocked()) return;

	ModInstrument *instr = m_sndFile.Instruments[m_nInstrument];
	if(instr == nullptr)
		return;

	size_t sel = m_ComboTuning->currentIndex();
	if(sel == 0) //Setting IT behavior
	{
		CriticalSection cs;
		PrepareUndo("Reset Tuning");
		instr->SetTuning(nullptr);
		cs.Leave();

		SetModified(InstrumentHint().Info(), true);
		return;
	}

	sel -= 1;

	if(sel < m_sndFile.GetTuneSpecificTunings().GetNumTunings())
	{
		CriticalSection cs;
		PrepareUndo("Set Tuning");
		instr->SetTuning(m_sndFile.GetTuneSpecificTunings().GetTuning(sel));
		cs.Leave();

		SetModified(InstrumentHint().Info(), true);
		return;
	}

	//Case: Chosen tuning editor to be displayed.
	//Creating vector for the CTuningDialog.
	CTuningDialog td(this, m_nInstrument, m_sndFile);
	td.DoModal();
	if(td.GetModifiedStatus(&m_sndFile.GetTuneSpecificTunings()))
	{
		m_modDoc.SetModified();
	}

	//Recreating tuning combobox so that possible
	//new tuning(s) come visible.
	BuildTuningComboBox();

	m_modDoc.UpdateAllViews(nullptr, GeneralHint().Tunings());
	m_modDoc.UpdateAllViews(nullptr, InstrumentHint().Info());
}


void CCtrlInstruments::UpdateTuningComboBox()
{
	if(m_nInstrument > m_sndFile.GetNumInstruments()
		|| m_sndFile.Instruments[m_nInstrument] == nullptr) return;

	ModInstrument* const pIns = m_sndFile.Instruments[m_nInstrument];
	if(pIns->pTuning == nullptr)
	{
		m_ComboTuning->setCurrentIndex(0);
		return;
	}

	for(size_t i = 0; i < m_sndFile.GetTuneSpecificTunings().GetNumTunings(); i++)
	{
		if(pIns->pTuning == m_sndFile.GetTuneSpecificTunings().GetTuning(i))
		{
			m_ComboTuning->setCurrentIndex((int)(i + 1));
			return;
		}
	}

	Reporting::Notification(MPT_UFORMAT("Tuning {} was not found. Setting to default tuning.")(mpt::ToUnicode(m_sndFile.Instruments[m_nInstrument]->pTuning->GetName())));

	CriticalSection cs;
	pIns->SetTuning(nullptr);

	m_modDoc.SetModified();
}


void CCtrlInstruments::OnPluginVelocityHandlingChanged()
{
	ModInstrument *pIns = m_sndFile.Instruments[m_nInstrument];
	if(!IsLocked() && pIns != nullptr)
	{
		PlugVelocityHandling n = velocityStyle.GetCheck() != 0 ? PLUGIN_VELOCITYHANDLING_CHANNEL : PLUGIN_VELOCITYHANDLING_VOLUME;
		if(n != pIns->pluginVelocityHandling)
		{
			PrepareUndo("Set Velocity Handling");
			if(n == PLUGIN_VELOCITYHANDLING_VOLUME && m_CbnPluginVolumeHandling->currentIndex() == PLUGIN_VOLUMEHANDLING_IGNORE)
			{
				// This combination doesn't make sense.
				m_CbnPluginVolumeHandling->setCurrentIndex(PLUGIN_VOLUMEHANDLING_MIDI);
			}

			pIns->pluginVelocityHandling = n;
			SetModified(InstrumentHint().Info(), false);
		}
	}
}


void CCtrlInstruments::OnPluginVolumeHandlingChanged()
{
	ModInstrument *pIns = m_sndFile.Instruments[m_nInstrument];
	if(!IsLocked() && pIns != nullptr)
	{
		PlugVolumeHandling n = static_cast<PlugVolumeHandling>(m_CbnPluginVolumeHandling->currentIndex());
		if(n != pIns->pluginVolumeHandling)
		{
			PrepareUndo("Set Volume Handling");

			if(velocityStyle.GetCheck() == 0 && n == PLUGIN_VOLUMEHANDLING_IGNORE)
			{
				// This combination doesn't make sense.
				velocityStyle.SetCheck(1);
			}

			pIns->pluginVolumeHandling = n;
			SetModified(InstrumentHint().Info(), false);
		}
	}
}


void CCtrlInstruments::OnPitchWheelDepthChanged()
{
	ModInstrument *pIns = m_sndFile.Instruments[m_nInstrument];
	if(!IsLocked() && pIns != nullptr)
	{
		int pwd = GetItemInt(IDC_PITCHWHEELDEPTH);
		int lower = -128, upper = 127;
		m_SpinPWD->GetRange32(lower, upper);
		Limit(pwd, lower, upper);
		if(pwd != pIns->midiPWD)
		{
			if(!m_startedEdit) PrepareUndo("Set Pitch Wheel Depth");
			pIns->midiPWD = static_cast<int8>(pwd);
			SetModified(InstrumentHint().Info(), false);
		}
	}
}


void CCtrlInstruments::OnBnClickedCheckPitchtempolock()
{
	if(IsLocked() || !m_nInstrument) return;

	INSTRUMENTINDEX firstIns = m_nInstrument, lastIns = m_nInstrument;
	if(CInputHandler::ShiftPressed())
	{
		firstIns = 1;
		lastIns = m_sndFile.GetNumInstruments();
	}

	m_EditPitchTempoLock->setEnabled(IsChecked(IDC_CHECK_PITCHTEMPOLOCK));
	TEMPO ptl(0, 0);
	bool isZero = false;
	if(IsChecked(IDC_CHECK_PITCHTEMPOLOCK))
	{
		//Checking what value to put for the wPitchToTempoLock.
		if(mfcless::GetWindowTextLength(m_EditPitchTempoLock) > 0)
		{
			ptl = m_EditPitchTempoLock->GetTempoValue();
		}
		if(!ptl.GetRaw())
		{
			ptl = m_sndFile.Order().GetDefaultTempo();
		}
		m_EditPitchTempoLock->SetTempoValue(ptl);
		isZero = true;
	}

	for(INSTRUMENTINDEX i = firstIns; i <= lastIns; i++)
	{
		if(m_sndFile.Instruments[i] != nullptr && (m_sndFile.Instruments[i]->pitchToTempoLock.GetRaw() == 0) == isZero)
		{
			m_modDoc.GetInstrumentUndo().PrepareUndo(i, "Set Pitch/Tempo Lock");
			m_sndFile.Instruments[i]->pitchToTempoLock = ptl;
			m_modDoc.SetModified();
		}
	}

	m_modDoc.UpdateAllViews(nullptr, InstrumentHint().Info(), this);
}


void CCtrlInstruments::OnEnChangeEditPitchTempoLock()
{
	if(IsLocked() || !m_nInstrument || !m_sndFile.Instruments[m_nInstrument]) return;

	TEMPO ptlTempo = m_EditPitchTempoLock->GetTempoValue();
	Limit(ptlTempo, m_sndFile.GetModSpecifications().GetTempoMin(), m_sndFile.GetModSpecifications().GetTempoMax());

	if(m_sndFile.Instruments[m_nInstrument]->pitchToTempoLock != ptlTempo)
	{
		if(!m_startedEdit) PrepareUndo("Set Pitch/Tempo Lock");
		m_sndFile.Instruments[m_nInstrument]->pitchToTempoLock = ptlTempo;
		m_modDoc.SetModified();	// Only update other views after killing focus
	}
}


void CCtrlInstruments::OnEnKillFocusEditPitchTempoLock()
{
	//Checking that tempo value is in correct range.
	if(IsLocked()) return;

	TEMPO ptlTempo = m_EditPitchTempoLock->GetTempoValue();
	bool changed = false;
	const CModSpecifications& specs = m_sndFile.GetModSpecifications();

	if(ptlTempo < specs.GetTempoMin())
	{
		ptlTempo = specs.GetTempoMin();
		changed = true;
	} else if(ptlTempo > specs.GetTempoMax())
	{
		ptlTempo = specs.GetTempoMax();
		changed = true;
	}
	if(changed)
	{
		m_EditPitchTempoLock->SetTempoValue(ptlTempo);
		m_modDoc.SetModified();
	}
	m_modDoc.UpdateAllViews(nullptr, InstrumentHint().Info(), this);
}


void CCtrlInstruments::OnEnKillFocusEditFadeOut()
{
	if(IsLocked() || !m_nInstrument || !m_sndFile.Instruments[m_nInstrument]) return;

	if(m_modDoc.GetModType() == MOD_TYPE_IT)
	{
		// Coarse fade-out in IT files
		bool success;
		uint32 fadeout = (GetItemInt(IDC_EDIT7) + 16) & ~31;
		if(success && fadeout != m_sndFile.Instruments[m_nInstrument]->nFadeOut)
		{
			SetItemInt(IDC_EDIT7, fadeout);
		}
	}
}


void CCtrlInstruments::BuildTuningComboBox()
{
	m_ComboTuning->setUpdatesEnabled(false);
	m_ComboTuning->clear();

	mfcless::AddItem(m_ComboTuning, UL_("OpenMPT IT behaviour")); //<-> Instrument pTuning pointer == NULL
	for(const auto &tuning : m_sndFile.GetTuneSpecificTunings())
	{
		mfcless::AddItem(m_ComboTuning, mpt::ToUnicode(tuning->GetName()));
	}
	mfcless::AddItem(m_ComboTuning, UL_("Control Tunings..."));
	UpdateTuningComboBox();
	m_ComboTuning->setUpdatesEnabled(true);
}


void CCtrlInstruments::OnXButtonUp(uint32 nFlags, uint32 nButton, mfcless::Point point)
{
	if(nButton == XBUTTON1) OnPrevInstrument();
	else if(nButton == XBUTTON2) OnNextInstrument();
	CModControlDlg::OnXButtonUp(nFlags, nButton, point);
}


OPENMPT_NAMESPACE_END
