/*
 * view_ins.h
 * ----------
 * Purpose: Instrument tab, lower panel.
 * Notes  : mfcless twin of upstream's mptrack/View_ins.h.
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
#include "../soundlib/Snd_defs.h"

class QObject;
class QString;
class QWidget;

OPENMPT_NAMESPACE_BEGIN

#define INSSTATUS_DRAGGING		0x01
#define INSSTATUS_NCLBTNDOWN	0x02
#define INSSTATUS_SPLITCURSOR	0x04

// Non-Client toolbar buttons
#define ENV_LEFTBAR_BUTTONS		22

enum DragPoints
{
	ENV_DRAGLOOPSTART		= (MAX_ENVPOINTS + 1),
	ENV_DRAGLOOPEND			= (MAX_ENVPOINTS + 2),
	ENV_DRAGSUSTAINSTART	= (MAX_ENVPOINTS + 3),
	ENV_DRAGSUSTAINEND		= (MAX_ENVPOINTS + 4),
	ENV_DRAGPREVIOUS		= (MAX_ENVPOINTS + 5),
	ENV_DRAGNEXT			= (MAX_ENVPOINTS + 6),
};

class CViewInstrument: public CModScrollView
{
	Q_OBJECT
protected:
	mfcless::Point m_ptMenu;
	mfcless::Rect m_rcClient, m_rcOldClient;

	mfcless::Bitmap m_bmpGrid;
	mfcless::Bitmap m_bmpMemMain;
	mfcless::Bitmap * m_pbmpOldGrid = nullptr;
	mfcless::Bitmap * oldBitmap = nullptr;

	EnvelopeType m_nEnv = ENV_VOLUME;
	uint32 m_nDragItem = 1, m_nBtnMouseOver = 0xFFFF;
	uint32 m_dwStatus = 0;
	uint32 m_NcButtonState[ENV_LEFTBAR_BUTTONS];

	INSTRUMENTINDEX m_nInstrument = 1;

	mfcless::DC m_dcMemMain;
	mfcless::DC m_dcGrid;
	int m_GridScrollPos = -1;
	int m_GridSpeed = -1;

	float m_zoom = 4;
	int m_envPointSize = 4;
	uint32 m_maxTickDrag = 0;  // Maximum tick for scroll size during envelope node dragging

	bool m_bGrid = true;
	bool m_bGridForceRedraw = false;
	bool m_mouseMoveModified = false;

	std::bitset<128> m_baPlayingNote;
	CModDoc::NoteToChannelMap m_noteChannel;	// Note -> Preview channel assignment
	std::array<uint32, MAX_CHANNELS> m_dwNotifyPos;

public:
	CViewInstrument();
public:
	static const mfcless::ViewClass classCViewInstrument;
protected:
	void PrepareUndo(const char *description);

	////////////////////////
	// Envelope get stuff

	// Flags
	bool EnvGetFlag(const EnvelopeFlags dwFlag) const;
	bool EnvGetLoop() const { return EnvGetFlag(ENV_LOOP); };
	bool EnvGetSustain() const { return EnvGetFlag(ENV_SUSTAIN); };
	bool EnvGetCarry() const { return EnvGetFlag(ENV_CARRY); };

	// Misc.
	uint32 EnvGetTick(int nPoint) const;
	uint32 EnvGetValue(int nPoint) const;
	uint32 EnvGetLastPoint() const;
	uint32 EnvGetNumPoints() const;

	// Get loop points
	uint32 EnvGetLoopStart() const;
	uint32 EnvGetLoopEnd() const;
	uint32 EnvGetSustainStart() const;
	uint32 EnvGetSustainEnd() const;

	// Get envelope status
	bool EnvGetVolEnv() const;
	bool EnvGetPanEnv() const;
	bool EnvGetPitchEnv() const;
	bool EnvGetFilterEnv() const;

	////////////////////////
	// Envelope set stuff

	// Flags
	bool EnvSetFlag(EnvelopeFlags flag, bool enable);
	bool EnvSetLoop(bool enable) {return EnvSetFlag(ENV_LOOP, enable);};
	bool EnvSetSustain(bool enable) {return EnvSetFlag(ENV_SUSTAIN, enable);};
	bool EnvSetCarry(bool enable) {return EnvSetFlag(ENV_CARRY, enable);};

	// Misc.
	bool EnvSetValue(int nPoint, int32 nTick = int32_min, int32 nValue = int32_min, bool moveTail = false);
	bool CanMovePoint(uint32 envPoint, int step);

	// Set loop points
	bool EnvSetLoopStart(int nPoint);
	bool EnvSetLoopEnd(int nPoint);
	bool EnvSetSustainStart(int nPoint);
	bool EnvSetSustainEnd(int nPoint);
	bool EnvToggleReleaseNode(int nPoint);

	// Set envelope status
	bool EnvToggleEnv(EnvelopeType envelope, CSoundFile &sndFile, ModInstrument &ins, bool enable, EnvelopeNode::value_t defaultValue, EnvelopeFlags extraFlags = EnvelopeFlags(0));
	bool EnvSetVolEnv(bool enable);
	bool EnvSetPanEnv(bool enable);
	bool EnvSetPitchEnv(bool enable);
	bool EnvSetFilterEnv(bool enable);

	// Keyboard envelope control
	void EnvKbdSelectPoint(DragPoints point);
	void EnvKbdMovePointLeft(int stepsize);
	void EnvKbdMovePointRight(int stepsize);
	void EnvKbdMovePointVertical(int stepsize);
	void EnvKbdInsertPoint();
	void EnvKbdRemovePoint();
	void EnvKbdSetLoopStart();
	void EnvKbdSetLoopEnd();
	void EnvKbdSetSustainStart();
	void EnvKbdSetSustainEnd();
	void EnvKbdToggleReleaseNode();

	bool IsDragItemEnvPoint() const { return m_nDragItem >= 1 && m_nDragItem <= EnvGetNumPoints(); }

	////////////////////////
	// Misc stuff
	void UpdateScrollSize();
	void SetModified(InstrumentHint hint, bool updateAll);
	bool SetCurrentInstrument(INSTRUMENTINDEX nIns, EnvelopeType m_nEnv = ENV_VOLUME);
	ModInstrument *GetInstrumentPtr() const;
	InstrumentEnvelope *GetEnvelopePtr() const;
	bool InsertAtPoint(mfcless::Point pt);
	uint32 EnvInsertPoint(int nTick, int nValue);
	bool EnvRemovePoint(uint32 nPoint);
	uint32 DragItemToEnvPoint() const;
	int TickToScreen(int nTick) const;
	int PointToScreen(int nPoint) const;
	int ScreenToTick(int x) const;
	int ScreenToPoint(int x, int y) const;
	int ValueToScreen(int val) const { return m_rcClient.bottom - 1 - (val * (m_rcClient.bottom - 1)) / 64; }
	int ScreenToValue(int y) const;
	void InvalidateEnvelope() { InvalidateRect(NULL, false); }
	void DrawPositionMarks();
	void DrawNcButton(mfcless::DC *pDC, uint32 nBtn);
	bool GetNcButtonRect(uint32 button, mfcless::Rect &rect) const;
	uint32 GetNcButtonAtPoint(mfcless::Point point, mfcless::Rect *outRect = nullptr) const;
	void UpdateNcButtonState();
	void PlayNote(ModCommand::NOTE note);
	void DrawGrid(mfcless::DC *memDC, uint32 speed);
	void UpdateIndicator();
	void UpdateIndicator(int tick, int val);
	mpt::ustring EnvValueToString(int tick, int val) const;

	void OnEnvZoomIn() { EnvSetZoom(m_zoom + 1); };
	void OnEnvZoomOut() { EnvSetZoom(m_zoom - 1); };
	void EnvSetZoom(float fNewZoom);

public:
	void OnDraw(mfcless::DC *) override;
	void OnInitialUpdate() override;
	void OnDPIChanged() override;
	void UpdateView(UpdateHint hint, QObject *pObj = nullptr) override;
	bool PreTranslateMessage(mfcless::Message *pMsg) override;
	bool OnDragonDrop(bool, const DRAGONDROP *) override;
	intptr_t OnModViewMsg(uintptr_t, intptr_t) override;
	intptr_t OnPlayerNotify(Notification *) override;
	QString GetAccessibleName() override;
	intptr_t OnToolHitTest(mfcless::Point point, mfcless::ToolInfo *pTI) const override;
protected:
	bool OnEraseBkgnd(mfcless::DC *) { return true; }
	// cppcheck-suppress duplInheritedMember
	void OnSetFocus(QWidget *pOldWnd);
	void OnSize(uint32 nType, int cx, int cy);
	// cppcheck-suppress duplInheritedMember
	void OnNcCalcSize(bool bCalcValidRects, mfcless::Rect *lpncsp);
	intptr_t OnNcHitTest(mfcless::Point point);
	void OnNcPaint();
	void OnPrevInstrument();
	void OnNextInstrument();
	void OnMouseMove(uint32, mfcless::Point);
	void OnLButtonDown(uint32, mfcless::Point);
	void OnLButtonUp(uint32, mfcless::Point);
	void OnLButtonDblClk(uint32 /*nFlags*/, mfcless::Point point) { InsertAtPoint(point); }
	void OnRButtonUp(uint32, mfcless::Point);
	void OnMButtonUp(uint32, mfcless::Point);
	void OnNcMouseMove(uint32 nHitTest, mfcless::Point point);
	void OnNcLButtonDown(uint32, mfcless::Point);
	void OnNcLButtonUp(uint32, mfcless::Point);
	void OnNcLButtonDblClk(uint32, mfcless::Point);
	void OnEnvLoopChanged();
	void OnEnvSustainChanged();
	void OnEnvCarryChanged();
	void OnEnvToggleReleasNode();
	void OnEnvInsertPoint();
	void OnEnvRemovePoint();
	void OnSelectVolumeEnv();
	void OnSelectPanningEnv();
	void OnSelectPitchEnv();
	void OnEnvVolChanged();
	void OnEnvPanChanged();
	void OnEnvPitchChanged();
	void OnEnvFilterChanged();
	void OnEnvToggleGrid();
	void OnEnvLoad();
	void OnEnvSave();
	void OnEditCopy();
	void OnEditPaste();
	void OnEditSampleMap();
	void OnEnvelopeScalePoints();
	void OnDropFiles(const QStringList &files);
	intptr_t OnCustomKeyMsg(uintptr_t, intptr_t);
	intptr_t OnMidiMsg(uintptr_t, intptr_t);
	// cppcheck-suppress duplInheritedMember
	bool OnMouseWheel(uint32 nFlags, short zDelta, mfcless::Point pt);
	void OnXButtonUp(uint32 nFlags, uint32 nButton, mfcless::Point point);
	void OnEditUndo();
	void OnEditRedo();
	void OnUpdateUndo(mfcless::CommandState &state);
	void OnUpdateRedo(mfcless::CommandState &state);
	void RegisterMessageMap() override;

private:
	uint8 EnvGetReleaseNode();
};


OPENMPT_NAMESPACE_END
