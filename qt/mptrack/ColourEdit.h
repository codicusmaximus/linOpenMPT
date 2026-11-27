/*
 * ColourEdit.h
 * ------------
 * Purpose: Implementation of a coloured edit UI item.
 * Notes  : mfcless twin of upstream's mptrack/ColourEdit.h.
 *          Draft from qt/tools/twin.py, not yet edited by hand.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "mfcless/Window.h"
#include <QtWidgets/QLineEdit>

OPENMPT_NAMESPACE_BEGIN

class CColourEdit : public mfcless::Window<QLineEdit>
{
	Q_OBJECT
public:
	CColourEdit();
	~CColourEdit();

public:
	void SetTextColor(uint32 rgb);
	void SetBackColor(uint32 rgb);

private:
	uint32 m_crText;
	uint32 m_crBackGnd;
	CBrush m_brBackGnd;

protected:
	HBRUSH CtlColor(CDC* pDC, uint32 nCtlColor);
	void RegisterMessageMap() override;

};

OPENMPT_NAMESPACE_END
