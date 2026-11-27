/*
 * tuningRatioMapWnd.cpp
 * ---------------------
 * Purpose: Alternative sample tuning configuration dialog - ratio map edit control.
 * Notes  : mfcless twin of upstream's mptrack/tuningRatioMapWnd.cpp.
 *          Hand-edited from the twin.py draft.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "tuningRatioMapWnd.h"
#include "mfcless/Widgets.h"
#include "HighDPISupport.h"
#include "Mainfrm.h"
#include "Mptrack.h"
#include "TuningDialog.h"
#include "../soundlib/tuning.h"


OPENMPT_NAMESPACE_BEGIN


void CTuningRatioMapWnd::RegisterMessageMap()
{
	mfcless::Window<QLabel>::RegisterMessageMap();
	// ON_WM_PAINT() (mfcless::Window)
	// ON_WM_SETFOCUS() (mfcless::Window)
	// ON_WM_KILLFOCUS() (mfcless::Window)
	// ON_WM_LBUTTONDOWN() (mfcless::Window)
	// ON_WM_MOUSEWHEEL() (mfcless::Window)
}


void CTuningRatioMapWnd::Init(CTuningDialog* const pParent, CTuning* const tuning)
{
	m_pParent = pParent;
	m_pTuning = tuning;
}

void CTuningRatioMapWnd::OnPaint()
{
	mfcless::PaintDC dc(this);

	if(!m_pTuning)
		return;

	mfcless::Rect rcClient;
	GetClientRect(&rcClient);
	
	const auto colorText = mfcless::SystemColor(mfcless::SysColor::WindowText);
	const auto colorTextSel = mfcless::SystemColor(mfcless::SysColor::HighlightText);
	const auto highlightBrush = mfcless::Brush(mfcless::SystemColor(mfcless::SysColor::Highlight)), windowBrush = mfcless::Brush(mfcless::SystemColor(mfcless::SysColor::Window));
	const int lineWidth = HighDPISupport::ScalePixels(1, *this);

	if(int dpi = HighDPISupport::GetDpiForWindow(this); m_dpi != dpi || !m_fontCreated)
	{
		m_dpi = dpi;
		HighDPISupport::CreateGUIFont(m_font, this);
		m_fontCreated = true;
		m_cxFont = m_cyFont = 0;
	}

	auto oldFont = dc.SelectObject(m_font);
	dc.SetBkMode(mfcless::bk::Transparent);
	if ((m_cxFont <= 0) || (m_cyFont <= 0))
	{
		mfcless::Size sz;
		sz = dc.GetTextExtent(UL_("123456789"));
		m_cyFont = sz.cy + 2;
		m_cxFont = rcClient.right / 4;
	}
	dc.IntersectClipRect(&rcClient);
	if ((m_cxFont > 0) && (m_cyFont > 0))
	{
		const bool focus = (mfcless::GetFocus() == this);
		mfcless::Rect rect;

		NOTEINDEXTYPE nNotes = static_cast<NOTEINDEXTYPE>((rcClient.bottom + m_cyFont - 1) / m_cyFont);
		//if(!m_nNote) m_nNote = m_nNoteCentre;
		NOTEINDEXTYPE nPos = m_nNote - (nNotes/2);
		int ypaint = 0;

		for (int ynote=0; ynote<nNotes; ynote++, ypaint+=m_cyFont, nPos++)
		{
			// Note
			NOTEINDEXTYPE noteToDraw = nPos - m_nNoteCentre;
			const bool isValidNote = m_pTuning->IsValidNote(noteToDraw);

			rect.SetRect(0, ypaint, m_cxFont, ypaint + m_cyFont);
			const auto noteStr = isValidNote ? mpt::ufmt::val(noteToDraw) : mpt::ustring(UL_("..."));
			DrawButtonRect(dc, lineWidth, m_font, rect, noteStr.c_str(), false, false);

			// Mapped Note
			const bool highLight = focus && (nPos == (int)m_nNote);
			rect.left = rect.right;
			rect.right = m_cxFont*4-1;
			mfcless::FillRect(dc, &rect, highLight ? highlightBrush : windowBrush);
			if(nPos == (int)m_nNote)
			{
				rect.InflateRect(-1, -1);
				dc.DrawFocusRect(&rect);
				rect.InflateRect(1, 1);
			}
			dc.SetTextColor(highLight ? colorTextSel : colorText);

			rect.SetRect(m_cxFont * 1, ypaint, m_cxFont * 2 - 1, ypaint + m_cyFont);
			dc.DrawText(mpt::ToUnicode(m_pTuning->GetNoteName(noteToDraw)), -1, &rect, mfcless::dt::SingleLine | mfcless::dt::Center | mfcless::dt::VCenter | mfcless::dt::NoPrefix);

			rect.SetRect(m_cxFont * 2, ypaint, m_cxFont * 3 - 1, ypaint + m_cyFont);
			dc.DrawText(mpt::ufmt::flt(m_pTuning->GetRatio(noteToDraw), 6), -1, &rect, mfcless::dt::SingleLine | mfcless::dt::Center | mfcless::dt::VCenter | mfcless::dt::NoPrefix);

			rect.SetRect(m_cxFont * 3, ypaint, m_cxFont * 4 - 1, ypaint + m_cyFont);
			dc.DrawText(mpt::ufmt::fix(std::log2(static_cast<double>(m_pTuning->GetRatio(noteToDraw))) * 1200.0, 1), -1, &rect, mfcless::dt::SingleLine | mfcless::dt::Center | mfcless::dt::VCenter | mfcless::dt::NoPrefix);

		}
		rect.SetRect(rcClient.left + m_cxFont * 4 - 1, rcClient.top, rcClient.left + m_cxFont * 4 + 3, ypaint);
		DrawButtonRect(dc, lineWidth, m_font, rect, UL_(""));
		if (ypaint < rcClient.bottom)
		{
			rect.SetRect(rcClient.left, ypaint, rcClient.right, rcClient.bottom);
			mfcless::FillRect(dc, &rect, mfcless::Brush(mfcless::SystemColor(mfcless::SysColor::ButtonFace)));
		}
	}
	dc.SelectObject(oldFont);
}

void CTuningRatioMapWnd::OnSetFocus(QWidget *pOldWnd)
{
	Window::OnSetFocus(pOldWnd);
	InvalidateRect(NULL, false);
}


void CTuningRatioMapWnd::OnKillFocus(QWidget *pNewWnd)
{
	Window::OnKillFocus(pNewWnd);
	InvalidateRect(NULL, false);
}


bool CTuningRatioMapWnd::OnMouseWheel(uint32 nFlags, short zDelta, mfcless::Point pt)
{
	NOTEINDEXTYPE note = static_cast<NOTEINDEXTYPE>(m_nNote - mpt::signum(zDelta));
	if(m_pTuning->IsValidNote(note - m_nNoteCentre))
	{
		m_nNote = note;
		InvalidateRect(NULL, false);
		if(m_pParent)
			m_pParent->UpdateRatioMapEdits(GetShownCentre());
	}

	return Window::OnMouseWheel(nFlags, zDelta, pt);
}


void CTuningRatioMapWnd::OnLButtonDown(uint32, mfcless::Point pt)
{
	if ((pt.x >= m_cxFont) && (pt.x < m_cxFont*2))
	{
		InvalidateRect(NULL, false);
	}
	if ((pt.x > m_cxFont*2) && (pt.x <= m_cxFont*3))
	{
		InvalidateRect(NULL, false);
	}
	if ((pt.x >= 0) && (m_cyFont))
	{
		mfcless::Rect rcClient;
		GetClientRect(&rcClient);
		int nNotes = (rcClient.bottom + m_cyFont - 1) / m_cyFont;
		const int n = (pt.y / m_cyFont) + m_nNote - (nNotes/2);
		const NOTEINDEXTYPE note = static_cast<NOTEINDEXTYPE>(n - m_nNoteCentre);
		if(m_pTuning->IsValidNote(note))
		{
			m_nNote = static_cast<NOTEINDEXTYPE>(n);
			InvalidateRect(NULL, false);
			if(m_pParent)
				m_pParent->UpdateRatioMapEdits(GetShownCentre());
		}

	}
	setFocus();
}



NOTEINDEXTYPE CTuningRatioMapWnd::GetShownCentre() const
{
	return m_nNote - m_nNoteCentre;
}


OPENMPT_NAMESPACE_END
