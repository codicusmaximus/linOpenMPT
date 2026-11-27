/*
 * View_smp.h
 * ----------
 * Purpose: Sample tab, lower panel.
 * Notes  : mfcless twin of upstream's mptrack/View_smp.h.
 *          Draft from qt/tools/twin.py, not yet edited by hand.
 * Authors: Olivier Lapicque
 *          OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "mfcless/Graphics.h"
#include "mfcless/Keyboard.h"
#include "DocumentView.h"
#include "Globals.h"
#include "Moddoc.h"
#include "TrackerSettings.h"
#include "../soundlib/modsmp_ctrl.h"


class QFont;
class QObject;
class QWidget;

OPENMPT_NAMESPACE_BEGIN

#define SMP_LEFTBAR_BUTTONS		8
class OPLInstrDlg;

class CViewSample: public CModScrollView
{
	Q_OBJECT
public:
	enum Flags
	{
		SMPSTATUS_MOUSEDRAG  = 0x01,
		SMPSTATUS_KEYDOWN    = 0x02,
		SMPSTATUS_NCLBTNDOWN = 0x04,
		SMPSTATUS_DRAWING    = 0x08,
		SMPSTATUS_MOUSEMOVED = 0x10,
	};

protected:
	enum class PasteMode
	{
		Replace,
		MixPaste,
		Insert
	};

	enum class HitTestItem
	{
		Nothing,
		SampleData,
		SelectionStart,
		SelectionEnd,
		LoopStart,
		LoopEnd,
		SustainStart,
		SustainEnd,
		CuePointFirst,
		CuePointLast = CuePointFirst + mpt::array_size<decltype(ModSample::cues)>::size - 1,
	};

	enum class ScrollTarget
	{
		Left,
		Right,
		Center,
	};

	std::unique_ptr<OPLInstrDlg> m_oplEditor;
	mfcless::Rect m_rcClient;
	mfcless::DC m_offScreenDC, m_waveformDC;
	mfcless::Bitmap m_offScreenBitmap, m_waveformBitmap;
	QFont m_timelineFont;
	mfcless::Size m_sizeTotal;
	uint32 m_nBtnMouseOver = 0xFFFF;
	int m_nZoom = 0;	// < 0: Zoom into sample (2^x:1 ratio), 0: Auto zoom, > 0: Zoom out (1:2^x ratio)
	int m_timelineHeight = 0;
	int m_timelineUnit = 0;
	int m_timelineInterval = 0;
	decltype(ModSample::nC5Speed) m_cachedSampleRate = 8363;
	FlagSet<Flags> m_dwStatus;
	SmpLength m_dwBeginSel, m_dwEndSel, m_dwBeginDrag, m_dwEndDrag;
	SmpLength m_dwMenuParam;
	SmpLength m_nGridSegments = 0;
	SAMPLEINDEX m_nSample = 1;
	HitTestItem m_dragItem = HitTestItem::Nothing;
	mfcless::Point m_startDragPoint;
	SmpLength m_startDragValue = MAX_SAMPLE_LENGTH;
	bool m_dragPreparedUndo = false, m_fineDrag = false, m_forceRedrawWaveform = true, m_scrolledSinceLastMouseMove = false;

	// Sample drawing
	mfcless::Point m_lastDrawPoint;		// For drawing horizontal lines
	int m_drawChannel;			// Which sample channel are we drawing on?

	// Note-off event buffer for MIDI sustain pedal
	std::array<std::vector<uint32>, 16> m_midiSustainBuffer;
	std::bitset<16> m_midiSustainActive;

	uint32 m_NcButtonState[SMP_LEFTBAR_BUTTONS];
	std::array<SmpLength, MAX_CHANNELS> m_dwNotifyPos;
	CModDoc::NoteToChannelMap m_noteChannel;	// Note -> Preview channel assignment

public:
	CViewSample();
public:
	static const mfcless::ViewClass classCViewSample;
protected:
	MPT_NOINLINE void SetModified(SampleHint hint, bool updateAll, bool waveformModified);
	void UpdateScrollSize() { UpdateScrollSize(m_nZoom, true); }
	void UpdateScrollSize(int newZoom, bool forceRefresh, SmpLength centeredSample = SmpLength(-1));
	void UpdateOPLEditor();
	void SetCurrentSample(SAMPLEINDEX nSmp);
	bool IsOPLInstrument() const;
	void SetZoom(int nZoom, SmpLength centeredSample = SmpLength(-1));
	int32 SampleToScreen(SmpLength pos, bool ignoreScrollPos = false) const;
	SmpLength ScreenToSample(int32 x, bool ignoreSampleLength = false) const;
	int32 SecondsToScreen(double x) const;
	double ScreenToSeconds(int32 x, bool ignoreSampleLength = false) const;
	std::pair<HitTestItem, SmpLength> PointToItem(mfcless::Point point, mfcless::Rect *rect = nullptr) const;
	void PlayNote(ModCommand::NOTE note, const SmpLength nStartPos = 0, int volume = -1);
	void NoteOff(ModCommand::NOTE note);
	void InvalidateSample(bool invalidateWaveform = true);
	void InvalidateTimeline();
	void SetCurSel(SmpLength nBegin, SmpLength nEnd);
	void ScrollToPosition(int x);
	void DrawPositionMarks();
	void DrawSampleData1(mfcless::DC * hdc, int ymed, int cx, int cy, SmpLength len, SampleFlags uFlags, const void *pSampleData);
	void DrawSampleData2(mfcless::DC * hdc, int ymed, int cx, int cy, SmpLength len, SampleFlags uFlags, const void *pSampleData);
	void DrawNcButton(mfcless::DC *pDC, uint32 nBtn);
	bool GetNcButtonRect(uint32 button, mfcless::Rect &rect) const;
	uint32 GetNcButtonAtPoint(mfcless::Point point, mfcless::Rect *outRect = nullptr) const;
	void UpdateNcButtonState();
	void DoPaste(PasteMode pasteMode);

	// Sets sample data on sample draw.
	template<class T>
	void SetSampleData(ModSample &smp, const mfcless::Point &point, const SmpLength old);

	// Sets initial draw point on sample draw.
	template<class T>
	void SetInitialDrawPoint(ModSample &smp, const mfcless::Point &point);

	// Returns sample value corresponding given point in the sample view.
	template<class T>
	T GetSampleValueFromPoint(const ModSample &smp, const mfcless::Point &point) const;

	int GetZoomLevel(SmpLength length) const;
	void DoZoom(int direction, const mfcless::Point &zoomPoint = mfcless::Point(-1, -1));
	bool CanZoomSelection() const;
	void ScrollToSample(SmpLength sample, bool refresh = true, ScrollTarget target = ScrollTarget::Center);

	SmpLength ScrollPosToSamplePos() const {return ScrollPosToSamplePos(m_nZoom);}
	SmpLength ScrollPosToSamplePos(int nZoom) const;

	void OnMonoConvert(ctrlSmp::StereoToMonoMode convert);
	void TrimSample(bool trimToLoopEnd);

	int CalcScroll(int &currentPos, int amount, int style, int bar);

	SmpLength SnapToGrid(const SmpLength pos) const;

	// Returns index of preview channel if exactly one one is being previewed, CHANNELINDEX_INVALID otherwise.
	CHANNELINDEX GetPreviewChannel() const;
	
	void PlayOrSetCuePoint(size_t cue);

public:
	void OnDraw(mfcless::DC *) override;
	void OnInitialUpdate() override;
	void UpdateView(UpdateHint hint, QObject *pObj = nullptr) override;
	intptr_t OnModViewMsg(uintptr_t, intptr_t) override;
	bool OnDragonDrop(bool, const DRAGONDROP *) override;
	intptr_t OnPlayerNotify(Notification *) override;
	bool PreTranslateMessage(mfcless::Message *pMsg) override;
	bool OnScrollBy(mfcless::Size sizeScroll, bool bDoScroll = true) override;
	intptr_t OnToolHitTest(mfcless::Point point, mfcless::ToolInfo *pTI) const override;
protected:
	bool OnEraseBkgnd(mfcless::DC *) { return true; }
	// cppcheck-suppress duplInheritedMember
	void OnSetFocus(QWidget *pOldWnd);
	void OnSize(uint32 nType, int cx, int cy);
	void OnNcCalcSize(bool bCalcValidRects, mfcless::Rect *lpncsp);
	intptr_t OnNcHitTest(mfcless::Point point);
	void OnNcPaint();
	void OnChar(uint32 nChar, uint32 nRepCnt, uint32 nFlags);
	void OnNcMouseMove(uint32 nHitTest, mfcless::Point point);
	void OnNcLButtonDown(uint32, mfcless::Point);
	void OnNcLButtonUp(uint32, mfcless::Point);
	void OnNcLButtonDblClk(uint32, mfcless::Point);
	void OnLButtonDblClk(uint32 nFlags, mfcless::Point point);
	void OnLButtonDown(uint32 nFlags, mfcless::Point point);
	void OnLButtonUp(uint32 nFlags, mfcless::Point point);
	void OnRButtonUp(uint32, mfcless::Point);
	void OnMouseMove(uint32, mfcless::Point);
	bool OnSetCursor(QWidget *pWnd, uint32 nHitTest, uint32 message);
	void OnEditSelectAll();
	void OnEditDelete();
	void OnEditCut();
	void OnEditCopy();
	void OnEditPaste();
	void OnEditMixPaste();
	void OnEditInsertPaste();
	void OnEditUndo();
	void OnEditRedo();
	void OnSetLoop();
	void OnSetSustainLoop();
	void On8BitConvert();
	void On16BitConvert();
	void OnMonoConvertMix() { OnMonoConvert(ctrlSmp::mixChannels); }
	void OnMonoConvertLeft() { OnMonoConvert(ctrlSmp::onlyLeft); }
	void OnMonoConvertRight() { OnMonoConvert(ctrlSmp::onlyRight); }
	void OnMonoConvertSplit() { OnMonoConvert(ctrlSmp::splitSample); }
	void OnSampleTrim() { TrimSample(false); }
	void OnPrevInstrument();
	void OnNextInstrument();
	void OnZoomOnSel();
	void OnDropFiles(const QStringList &files);
	void OnSetLoopStart();
	void OnSetLoopEnd();
	void OnConvertPingPongLoop();
	void OnConvertNormalLoopToSustain();
	void OnSetSustainStart();
	void OnSetSustainEnd();
	void OnConvertPingPongSustain();
	void OnConvertSustainLoopToNormal();
	void OnSetCuePoint(uint32 nID);
	void OnZoomUp();
	void OnZoomDown();
	void OnDrawingToggle();
	void OnAddSilence();
	void OnChangeGridSize();
	void OnQuickFade();
	intptr_t OnMidiMsg(uintptr_t, intptr_t);
	intptr_t OnCustomKeyMsg(uintptr_t, intptr_t); //rewbs.customKeys
	// cppcheck-suppress duplInheritedMember
	bool OnMouseWheel(uint32 nFlags, short zDelta, mfcless::Point pt);
	void OnXButtonUp(uint32 nFlags, uint32 nButton, mfcless::Point point);
	void OnUpdateUndo(mfcless::CommandState &state);
	void OnUpdateRedo(mfcless::CommandState &state);
	void OnSampleSliceCuePoints();
	void OnSampleSliceGrid();
	void OnSampleSlice(mpt::span<SmpLength> slicePoints);
	void OnSampleInsertCuePoint();
	void OnSampleDeleteCuePoint();
	void OnTimelineFormatSeconds() { SetTimelineFormat(TimelineFormat::Seconds); }
	void OnTimelineFormatSamples() { SetTimelineFormat(TimelineFormat::Samples); }
	void OnTimelineFormatSamplesPow2() { SetTimelineFormat(TimelineFormat::SamplesPow2); }
	void SetTimelineFormat(TimelineFormat fmt);
	void RegisterMessageMap() override;

	bool OnGestureZoom(mfcless::Point ptCenter, long lDelta) override
	{
		DoZoom(lDelta / 10, ptCenter);
		return true;
	}

	static bool IsCuePoint(HitTestItem item) { return item >= HitTestItem::CuePointFirst && item <= HitTestItem::CuePointLast; }
	static int CuePointFromItem(HitTestItem item) { return static_cast<int>(item) - static_cast<int>(HitTestItem::CuePointFirst); }
};

DECLARE_FLAGSET(CViewSample::Flags)


OPENMPT_NAMESPACE_END
