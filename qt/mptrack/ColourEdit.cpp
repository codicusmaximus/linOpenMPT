/*
 * ColourEdit.cpp
 * --------------
 * Purpose: Implementation of a coloured edit UI item.
 * Notes  : mfcless twin of upstream's mptrack/ColourEdit.cpp.
 *          Hand-edited from the twin.py draft.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "ColourEdit.h"
#include "mfcless/Widgets.h"


OPENMPT_NAMESPACE_BEGIN


/////////////////////////////////////////////////////////////////////////////
// CColourEdit

CColourEdit::CColourEdit(QWidget *parent)
	: Window(parent)
{
	m_crText = mfcless::Rgb(0, 0, 0);  //default text color
	m_crBackGnd = mfcless::Rgb(255, 255, 255);
}


void CColourEdit::ApplyColors()
{
	QPalette pal = palette();
	pal.setColor(QPalette::Text, mfcless::ToQColor(m_crText));
	if(m_hasBackColor)
		pal.setColor(QPalette::Base, mfcless::ToQColor(m_crBackGnd));
	setPalette(pal);
	Invalidate(true);
}


void CColourEdit::SetBackColor(uint32 rgb)
{
	m_crBackGnd = rgb;
	m_hasBackColor = true;
	ApplyColors();
}


void CColourEdit::SetTextColor(uint32 rgb)
{
	m_crText = rgb;
	ApplyColors();
}


OPENMPT_NAMESPACE_END
