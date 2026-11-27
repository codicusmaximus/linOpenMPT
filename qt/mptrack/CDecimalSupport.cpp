/*
 * CDecimalSupport.cpp
 * -------------------
 * Purpose: Various extensions of the CDecimalSupport implementation.
 * Notes  : mfcless twin of upstream's mptrack/CDecimalSupport.cpp.
 *          Draft from qt/tools/twin.py, not yet edited by hand.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "CDecimalSupport.h"

OPENMPT_NAMESPACE_BEGIN

void CNumberEdit::RegisterMessageMap()
{
	AccessibleEdit::RegisterMessageMap();
	// ON_WM_CHAR() (mfcless::Window)
	AddMessage(WM_PASTE, &CNumberEdit::OnPaste);
}


void CNumberEdit::SetTempoValue(const TEMPO &t)
{
	SetFixedValue(t.ToDouble(), 4);
}


TEMPO CNumberEdit::GetTempoValue()
{
	double d;
	GetDecimalValue(d);
	return TEMPO(d);
}


void CNumberEdit::OnChar(uint32 nChar, uint32 nRepCnt, uint32 nFlags)
{
	bool handled = false;
	if(GetStyle() & ES_NUMBER)
		CDecimalSupport<CNumberEdit>::OnChar(0, nChar, 0, handled);
	if(!handled)
		AccessibleEdit::OnChar(nChar, nRepCnt, nFlags);
}


intptr_t CNumberEdit::OnPaste(uintptr_t wParam, intptr_t lParam)
{
	bool bHandled = false;
	CDecimalSupport<CNumberEdit>::OnPaste(0, wParam, lParam, bHandled);
	if(!bHandled)
		return AccessibleEdit::DefWindowProc(WM_PASTE, wParam, lParam);
	else
		return 0;
}

OPENMPT_NAMESPACE_END