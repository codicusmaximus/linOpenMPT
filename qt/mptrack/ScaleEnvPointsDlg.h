/*
 * ScaleEnvPointsDlg.h
 * -------------------
 * Purpose: Dialog for scaling instrument envelope points on x and y axis.
 * Notes  : mfcless twin of upstream's mptrack/ScaleEnvPointsDlg.h.
 *          Hand-edited from the twin.py draft.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"

#include "CDecimalSupport.h"
#include "DialogBase.h"

class QWidget;

OPENMPT_NAMESPACE_BEGIN

struct InstrumentEnvelope;

class CScaleEnvPointsDlg : public DialogBase
{
	Q_OBJECT
protected:
	CNumberEdit *m_EditX = nullptr, *m_EditY = nullptr, *m_EditOffset = nullptr;
	InstrumentEnvelope &m_Env;
	static double m_factorX, m_factorY, m_offsetY;
	const int m_nCenter;

public:
	CScaleEnvPointsDlg(QWidget* pParent, InstrumentEnvelope &env, int nCenter);

	void Apply();

protected:
	void OnOK() override;
	bool OnInitDialog() override;
};

OPENMPT_NAMESPACE_END
