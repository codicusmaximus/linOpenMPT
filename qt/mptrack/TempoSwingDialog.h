/*
 * TempoSwingDialog.h
 * ------------------
 * Purpose: Implementation of the tempo swing configuration dialog.
 * Notes  : mfcless twin of upstream's mptrack/TempoSwingDialog.h.
 *          Draft from qt/tools/twin.py, not yet edited by hand.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include <QtWidgets/QLabel>

#include "DialogBase.h"
#include "../soundlib/Snd_defs.h"

class QAbstractButton;
class QScrollBar;
class QSlider;
class QWidget;

OPENMPT_NAMESPACE_BEGIN

class CSoundFile;

class CTempoSwingDlg : public DialogBase
{
	Q_OBJECT
protected:
	// Scrollable container for the sliders
	class SliderContainer : public QLabel
	{
	public:
		CTempoSwingDlg &m_parent;

		SliderContainer(CTempoSwingDlg &parent) : m_parent(parent) { }

		void OnHScroll(uint32 nSBCode, uint32 nPos, QScrollBar *pScrollBar);
		bool OnToolTipNotify(uint32 );

		void RegisterMessageMap() override;
	};

	enum { SliderResolution = 1000, SliderUnity = SliderResolution / 2 };
	struct RowCtls
	{
		QLabel *rowLabel = nullptr, *valueLabel = nullptr;
		QSlider *valueSlider = nullptr;

		void SetValue(TempoSwing::value_type v);
		TempoSwing::value_type GetValue() const;
	};
	std::vector<std::unique_ptr<RowCtls>> m_controls;

	QAbstractButton *m_checkGroup = nullptr;
	QScrollBar *m_scrollBar = nullptr;
	SliderContainer m_container;

	int m_scrollPos;
	static int m_groupSize;

public:
	TempoSwing m_tempoSwing;
	const TempoSwing m_origTempoSwing;
	CSoundFile &m_sndFile;
	PATTERNINDEX m_pattern;

public:
	CTempoSwingDlg(QWidget *parent, const TempoSwing &currentTempoSwing, CSoundFile &sndFile, PATTERNINDEX pattern = PATTERNINDEX_INVALID);

protected:
	void DoDataExchange(DataExchange &dx) override;
	bool OnInitDialog() override;
	void OnDPIChanged() override;
	void OnOK() override;
	void OnCancel() override;
	void OnClose();
	void OnReset();
	void OnUseGlobal();
	void OnToggleGroup();
	void OnGroupChanged();
	void OnVScroll(uint32 nSBCode, uint32 nPos, QScrollBar *pScrollBar);

	void RegisterMessageMap() override;
};

OPENMPT_NAMESPACE_END
