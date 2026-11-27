/*
 * ColourEdit.cpp
 * --------------
 * Purpose: Implementation of a coloured edit UI item.
 * Notes  : mfcless twin of upstream's mptrack/ColourEdit.cpp.
 *          Draft from qt/tools/twin.py, not yet edited by hand.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "ColourEdit.h"
#include "mfcless/Widgets.h"


OPENMPT_NAMESPACE_BEGIN


/////////////////////////////////////////////////////////////////////////////
// CColourEdit

CColourEdit::CColourEdit()
{
	m_crText = mfcless::Rgb(0, 0, 0);  //default text color
}

CColourEdit::~CColourEdit()
{
	if(m_brBackGnd.GetSafeHandle())  //delete brush
		m_brBackGnd.DeleteObject();
}


void CColourEdit::RegisterMessageMap()
{
	mfcless::Window<QLineEdit>::RegisterMessageMap();
	// ON_WM_CTLCOLOR_REFLECT(): by hand (Qt event override)
}

/////////////////////////////////////////////////////////////////////////////
// CColourEdit message handlers

mfcless::Brush CColourEdit::CtlColor(mfcless::DC *pDC, uint32 nCtlColor)
{
	MPT_UNREFERENCED_PARAMETER(nCtlColor);
	pDC->SetTextColor(m_crText);		//set text color
	pDC->SetBkColor(m_crBackGnd);		//set the text's background color
	return m_brBackGnd;	//return the brush used for background - this sets control background
}

/////////////////////////////////////////////////////////////////////////////
// Implementation

void CColourEdit::SetBackColor(uint32 rgb)
{
	m_crBackGnd = rgb;					//set background color ref (used for text's background)
	if(m_brBackGnd.GetSafeHandle())  //free brush
		m_brBackGnd.DeleteObject();
	m_brBackGnd.CreateSolidBrush(rgb);  //set brush to new color
	Invalidate(true);					//redraw
}


void CColourEdit::SetTextColor(uint32 rgb)
{
	m_crText = rgb;    // set text color ref
	Invalidate(true);  // redraw
}


OPENMPT_NAMESPACE_END
