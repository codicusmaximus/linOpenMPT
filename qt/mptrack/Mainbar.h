/*
 * Mainbar.h
 * ---------
 * Purpose: Implementation of OpenMPT's window toolbar and parent container of the tree view.
 * Notes  : mfcless twin of upstream's mptrack/Mainbar.h.
 *          Draft from qt/tools/twin.py, not yet edited by hand.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "mfcless/Controls.h"
#include "mfcless/MessageMap.h"
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QWidget>
#include "CDecimalSupport.h"
#include "CImageListEx.h"
#include "UpdateHints.h"
#include "UpdateToolTip.h"
#include "../soundlib/Snd_defs.h"

class QEvent;
class QFont;
class QObject;
class QPoint;
class QRect;
class QScrollBar;
class QSize;

OPENMPT_NAMESPACE_BEGIN

enum class MainToolBarItem : uint32;
class CMainToolBar;

class CStereoVU: public QLabel, public mfcless::MessageMap
{
	Q_OBJECT
protected:
	uint8 numChannels = 2;
	uint32 vuMeter[4] = {{}};
	uint32 lastVuUpdateTime;
	int lastV[4] = {{}};
	bool lastClip[4] = {{}};
	bool horizontal = true;
	bool allowRightToLeft = false;

public:
	CStereoVU() { lastVuUpdateTime = timeGetTime(); }
	void SetVuMeter(uint8 validChannels, const uint32 channels[4], bool force = false);
	void SetOrientation(bool h) { horizontal = h; }

protected:
	void DrawVuMeters(CDC &dc, bool redraw = false);
	void DrawVuMeter(CDC &dc, const QRect &rect, int index, bool redraw = false);

protected:
	void OnPaint();
	void OnLButtonDown(uint32, QPoint);
	void RegisterMessageMap() override;;
};

class COctaveEdit : public QLineEdit, public mfcless::MessageMap
{
	Q_OBJECT
public:
	COctaveEdit(CMainToolBar &owner) : m_owner{owner} { }

protected:
	void OnChar(uint32 nChar, uint32 nRepCnt, uint32 nFlags);

	void RegisterMessageMap() override;

	CMainToolBar &m_owner;
};

#define MIN_BASEOCTAVE		0
#define MAX_BASEOCTAVE		8

class CSoundFile;
class CModDoc;
class CModTree;
class CMainFrame;
struct Notification;

class CToolBarEx: public QToolBar
{
	Q_OBJECT
protected:
	bool m_bVertical = false;

public:
	CToolBarEx() {}
	~CToolBarEx() override {}

public:
	void UpdateControl(bool show, QWidget &wnd, int index, int id, int height = 0);
	void EnableFlatButtons(bool flat);
	void SetButtonVisibility(int index, bool visible);

public:
	QSize CalcDynamicLayout(int nLength, uint32 dwMode) override;
	virtual void SetHorizontal();
	virtual void SetVertical();
};


class CMainToolBar: public CToolBarEx
{
protected:
	UpdateToolTip m_tooltip;
	CImageListEx m_ImageList, m_ImageListDisabled;
	QFont m_font;
	COctaveEdit m_EditOctave;
	CNumberEdit m_EditTempo;
	QLineEdit *m_EditSpeed = nullptr, *m_EditRowsPerBeat = nullptr, *m_EditGlobalVolume = nullptr;
	QLabel *m_StaticTempo = nullptr, *m_StaticSpeed = nullptr, *m_StaticRowsPerBeat = nullptr, *m_StaticGlobalVolume = nullptr;
	mfcless::UpDown *m_SpinTempo = nullptr, *m_SpinSpeed = nullptr, *m_SpinOctave = nullptr, *m_SpinRowsPerBeat = nullptr, *m_SpinGlobalVolume = nullptr;
	int m_currentSpeed = 0, m_currentOctave = -1, m_currentRowsPerBeat = 0, m_currentGlobalVolume = 0;
	TEMPO m_currentTempo{1, 0};
	bool m_updating = false;
public:
	CStereoVU m_VuMeter;

public:
	CMainToolBar() : m_EditOctave{*this} { }

protected:
	void SetRowsPerBeat(ROWINDEX nNewRPB);

public:
	void SetHorizontal() override;
	void SetVertical() override;
public:
#if MPT_COMPILER_CLANG
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Woverloaded-virtual"
#endif // MPT_COMPILER_CLANG
	bool Create(QWidget *parent);
#if MPT_COMPILER_CLANG
#pragma clang diagnostic pop
#endif // MPT_COMPILER_CLANG
	void Init(CMainFrame *);
	uint32 GetBaseOctave() const;
	void SetBaseOctave(uint32 nOctave);
	void SetCurrentSong(CSoundFile *pModDoc);

	bool ShowUpdateInfo(const mpt::ustring &newVersion, const mpt::ustring &infoURL, bool showHighLight);
	void RemoveUpdateInfo();

	bool ToggleVisibility(MainToolBarItem item);

protected:
	void RefreshToolbar();
	void UpdateSizes();
	void UpdateControls();
	intptr_t OnDPIChangedAfterParent(uintptr_t, intptr_t);
	intptr_t OnSettingChange(uintptr_t, intptr_t);
	intptr_t OnRefreshToolbar(uintptr_t, intptr_t);
	void OnVScroll(uint32, uint32, QScrollBar *);
	void OnTbnDropDownToolBar();
	bool OnToolTipText(uint32 );
	void OnSelectMIDIDevice(uint32 id);

	void OnSpeedChanged();
	void OnTempoChanged();
	void OnRPBChanged();
	void OnGlobalVolChanged();
	void RegisterMessageMap() override;
};


class CModTreeBar: public QWidget, public mfcless::MessageMap
{
	Q_OBJECT
public:
	enum Status
	{
		MTB_VERTICAL = 0x01,
		MTB_CAPTURE = 0x02,
		MTB_DRAGGING = 0x04,
		MTB_TRACKER = 0x08,
	};

protected:
	FlagSet<Status> m_status;
	uint32 m_nCursorDrag = 0;
	QPoint ptDragging;
	uint32 m_cxOriginal = 0, m_cyOriginal = 0, m_nTrackPos = 0;
	uint32 m_nTreeSplitRatio = 0;

	QLineEdit *m_filterEdit = nullptr;
	CModTree *m_filterSource = nullptr;
	uintptr_t m_filterTimer = 0;

public:
	CModTree *m_pModTree = nullptr, *m_pModTreeData = nullptr;

	CModTreeBar();
	~CModTreeBar() override;

	void DelayShow(bool show) override;

public:
	void Init();
	void RecalcLayout();
	void DoMouseMove(QPoint point);
	void DoLButtonDown(QPoint point);
	void DoLButtonUp();
	void CancelTracking();
	void OnInvertTracker(uint32 x);
	void RefreshDlsBanks();
	void RefreshMidiLibrary();
	void OnOptionsChanged();
	void OnDocumentCreated(CModDoc *pModDoc);
	void OnDocumentClosed(CModDoc *pModDoc);
	void OnUpdate(CModDoc *pModDoc, UpdateHint hint, QObject *pHint = nullptr);
	void UpdatePlayPos(CModDoc *pModDoc, Notification *pNotify);
	bool SetTreeSoundfile(FileReader &file);

	void StartTreeFilter(CModTree &source);

	void SetBarOnLeft(const bool left);
	bool BarOnLeft() { return (GetBarStyle() & CBRS_ALIGN_LEFT); }

protected:
	QSize CalcFixedLayout(bool bStretch, bool bHorz) override;
	bool eventFilter(QObject *watched, QEvent *e) override;
	void CloseTreeFilter();
	void CancelTimer();

	int Padding() const;

protected:
	void OnNcPaint();
	intptr_t OnNcHitTest(QPoint point);
	void OnNcCalcSize(bool bCalcValidRects, NCCALCSIZE_PARAMS* lpncsp);
	void OnSize(uint32 nType, int cx, int cy);
	void OnNcMouseMove(uint32 nHitTest, QPoint point);
	void OnMouseMove(uint32 nFlags, QPoint point);
	void OnNcLButtonDown(uint32, QPoint);
	void OnLButtonDown(uint32, QPoint);
	void OnNcLButtonUp(uint32, QPoint);
	void OnLButtonUp(uint32, QPoint);
	void OnNcRButtonDown(uint32, QPoint) { CancelTracking(); }
	void OnRButtonDown(uint32, QPoint) { CancelTracking(); }
	intptr_t OnInitDialog(uintptr_t, intptr_t);
	void OnFilterChanged();
	void OnFilterLostFocus();
	void OnTimer(uintptr_t id);
	void RegisterMessageMap() override;
};

DECLARE_FLAGSET(CModTreeBar::Status)


OPENMPT_NAMESPACE_END
