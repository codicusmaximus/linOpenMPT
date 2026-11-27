/*
 * tuningRatioMapWnd.h
 * -------------------
 * Purpose: Alternative sample tuning configuration dialog - ratio map edit control.
 * Notes  : mfcless twin of upstream's mptrack/tuningRatioMapWnd.h.
 *          Draft from qt/tools/twin.py, not yet edited by hand.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include <QtWidgets/QLabel>

#include "../soundlib/tuning.h"
#include "../soundlib/modcommand.h"

class QFont;
class QPoint;
class QWidget;

OPENMPT_NAMESPACE_BEGIN

class CTuningDialog;

using NOTEINDEXTYPE = Tuning::NOTEINDEXTYPE;

//Copied from CNoteMapWnd.
class CTuningRatioMapWnd: public QLabel
{
	Q_OBJECT
	friend class CTuningDialog;
protected:
	const CTuning* m_pTuning = nullptr;
	CTuningDialog* m_pParent = nullptr;

	QFont m_font;
	int m_cxFont = 0, m_cyFont = 0;
	int m_dpi = 0;
	NOTEINDEXTYPE m_nNote = NOTE_MIDDLEC;

	NOTEINDEXTYPE m_nNoteCentre = NOTE_MIDDLEC;


public:
	void Init(CTuningDialog* const pParent, CTuning* const tuning);

	NOTEINDEXTYPE GetShownCentre() const;

protected:

	bool OnMouseWheel(uint32 nFlags, short zDelta, QPoint pt);
	void OnLButtonDown(uint32, QPoint);
	void OnSetFocus(QWidget *pOldWnd);
	void OnKillFocus(QWidget *pNewWnd);
	void OnPaint();
};

OPENMPT_NAMESPACE_END
