/*
 * ctrl_gen.h
 * ----------
 * Purpose: General tab, upper panel.
 * Notes  : mfcless twin of upstream's mptrack/Ctrl_gen.h.
 *          Draft from qt/tools/twin.py, not yet edited by hand.
 * Authors: Olivier Lapicque
 *          OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "mfcless/System.h"
#include "mfcless/Controls.h"
#include "mfcless/Graphics.h"
#include "mfcless/Window.h"
#include "DocumentView.h"
#include <QtWidgets/QWidget>

#include "CDecimalSupport.h"
#include "Globals.h"
#include "../misc/mptClock.h"

class QAbstractButton;
class QComboBox;
class QLineEdit;
class QObject;
class QScrollBar;
class QSlider;

OPENMPT_NAMESPACE_BEGIN

namespace Util { class MultimediaClock; }

class CVuMeter final : public mfcless::Window<QWidget>
{
protected:
	int m_lastDisplayedLevel = -1, m_lastLevel = 0;
	uint32 m_lastVuUpdateTime;

public:
	CVuMeter() { m_lastVuUpdateTime = mfcless::TickCount(); }
	void SetVuMeter(int level, bool force = false);

protected:
	void DrawVuMeter(mfcless::DC &dc, bool redraw = false);

protected:
	void OnPaint();
	void RegisterMessageMap() override;;
};


class CCtrlGeneral final : public CModControlDlg
{
public:
	QLineEdit *m_EditTitle = nullptr, *m_EditArtist = nullptr;
	QLineEdit *m_EditSpeed = nullptr, *m_EditGlobalVol = nullptr, *m_EditRestartPos = nullptr, *m_EditSamplePA = nullptr, *m_EditVSTiVol = nullptr;
	CNumberEdit *m_EditTempo = nullptr;
	QAbstractButton *m_BtnModType = nullptr;
	mfcless::UpDown *m_SpinTempo = nullptr, *m_SpinSpeed = nullptr, *m_SpinGlobalVol = nullptr, *m_SpinRestartPos = nullptr, *m_SpinSamplePA = nullptr, *m_SpinVSTiVol = nullptr;
	QComboBox *m_CbnResampling = nullptr;

	QSlider *m_SliderTempo = nullptr, *m_SliderSamplePreAmp = nullptr, *m_SliderGlobalVol = nullptr, *m_SliderVSTiVol = nullptr;
	CVuMeter *m_VuMeterLeft = nullptr, *m_VuMeterRight = nullptr;
	std::unique_ptr<Util::MultimediaClock> m_tapTimer;
	bool m_editsLocked = false;

	TEMPO m_tempoMin, m_tempoMax;

public:
	CCtrlGeneral(CModControlView &parent, CModDoc &document);

private:
	// Determine how the global volume slider should be scaled to actual global volume.
	// Display range for XM / S3M should be 0...64, for other formats it's 0...256.
	uint32 GetGlobalVolumeFactor() const;
	Setting<int32> &GetSplitPosRef() override;
	bool OnInitDialog() override;
	void DoDataExchange(DataExchange &dx) override;  // DDX/DDV support
	void RecalcLayout() override;
	void UpdateView(UpdateHint hint, QObject *pObj = nullptr) override;
	const mfcless::ViewClass *GetAssociatedViewClass() override;
	void OnActivatePage(intptr_t) override;
	void OnDeactivatePage() override;
	mpt::ustring GetToolTipText(uint32 uId, QWidget * hwnd) const override;
protected:
	static constexpr int MAX_SLIDER_VSTI_VOL = 255;
	static constexpr int MAX_SLIDER_SAMPLE_VOL = 255;

	// At this point, the tempo slider moves in more coarse steps to provide detailed values in the regions where it matters
	static constexpr auto TEMPO_SPLIT_THRESHOLD = TEMPO(256, 0);
	static constexpr int TEMPO_SPLIT_PRECISION = 3;

	TEMPO TempoSliderRange() const;
	TEMPO SliderToTempo(int value) const;
	int TempoToSlider(TEMPO tempo) const;
	intptr_t OnUpdatePosition(uintptr_t, intptr_t);
	void OnVScroll(mfcless::ScrollCode nSBCode, int nPos, QWidget *pScrollBar);
	void OnTapTempo();
	void OnTitleChanged();
	void OnArtistChanged();
	void OnTempoChanged();
	void OnSpeedChanged();
	void OnGlobalVolChanged();
	void OnVSTiVolChanged();
	void OnSamplePAChanged();
	void OnRestartPosChanged();
	void OnRestartPosDone();
	void OnSongProperties();
	void OnLoopSongChanged();
	void OnEnSetfocusEditSongtitle();
	void OnResamplingChanged();
	void RegisterMessageMap() override;
};

OPENMPT_NAMESPACE_END
