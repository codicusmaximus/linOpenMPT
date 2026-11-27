/*
 * Mainbar.h
 * ---------
 * Purpose: Implementation of OpenMPT's window toolbar and parent container of the tree view.
 * Notes  : mfcless twin of upstream's mptrack/Mainbar.h.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "mfcless/System.h"
#include "mfcless/Controls.h"
#include "mfcless/Graphics.h"
#include "mfcless/Window.h"
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtCore/QTimer>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QWidget>
#include "CDecimalSupport.h"
#include "CImageListEx.h"
#include "UpdateHints.h"
#include "UpdateToolTip.h"
#include "../soundlib/Snd_defs.h"

class QFont;
class QObject;
class QScrollBar;
class QSplitter;

OPENMPT_NAMESPACE_BEGIN

enum class MainToolBarItem : uint32;
class CMainToolBar;

class CStereoVU: public mfcless::Window<QLabel>
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
	explicit CStereoVU(QWidget *parent = nullptr) : Window(parent) { lastVuUpdateTime = mfcless::TickCount(); }
	void SetVuMeter(uint8 validChannels, const uint32 channels[4], bool force = false);
	void SetOrientation(bool h) { horizontal = h; }

protected:
	void DrawVuMeters(mfcless::DC &dc, bool redraw = false);
	void DrawVuMeter(mfcless::DC &dc, const mfcless::Rect &rect, int index, bool redraw = false);

protected:
	void OnPaint();
	void OnLButtonDown(uint32, mfcless::Point);
	void RegisterMessageMap() override;;
};

class COctaveEdit : public mfcless::Window<QLineEdit>
{
	Q_OBJECT
public:
	COctaveEdit(CMainToolBar &owner, QWidget *parent = nullptr) : Window(parent), m_owner{owner} { }

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

// mfcless: CToolBar as a QToolBar. Its items (buttons, separators and
// controls) are kept by index, as upstream addresses them.
class CToolBarEx: public mfcless::Window<QToolBar>
{
	Q_OBJECT
protected:
	struct Item
	{
		QAction *action = nullptr;
		QWidget *control = nullptr;
		uint32 id = 0;
		int image = -1;
		bool separator = false;
	};
	std::vector<Item> m_items;
	CImageListEx *m_images = nullptr, *m_disabledImages = nullptr;
	QTimer m_updateTimer;
	bool m_bVertical = false;
	bool m_flat = true;

public:
	explicit CToolBarEx(QWidget *parent = nullptr);
	~CToolBarEx() override {}

public:
	// The buttons, by command ID (ID_SEPARATOR for a separator); the images
	// are taken in order, as CToolBar::SetButtons does.
	void SetButtons(const uint32 *ids, int count);
	void SetImageLists(CImageListEx *images, CImageListEx *disabledImages);
	// Shows the control in the place of the separator at index.
	void SetControl(int index, uint32 id, QWidget *control);
	// Another command and image for the button at index.
	void SetButtonCommand(int index, uint32 id, int image);
	// An arrow beside the button opens OnButtonDropDown (TBSTYLE_DROPDOWN).
	void SetDropDown(uint32 id);
	int CommandToIndex(uint32 id) const;
	QAction *GetButton(uint32 id) const;
	void SetButtonHidden(uint32 id, bool hidden);

	void UpdateControl(bool show, QWidget &wnd, int index, int id, int height = 0);
	void EnableFlatButtons(bool flat);
	void SetButtonVisibility(int index, bool visible);

public:
	virtual void SetHorizontal();
	virtual void SetVertical();

protected:
	void UpdateButtonImages();
	void UpdateButtonStates();
	virtual void OnButtonCommand(uint32 id);
	virtual void OnButtonDropDown(uint32 /*id*/, QPoint /*pos*/) {}
	virtual QString GetButtonToolTip(uint32 /*id*/) { return {}; }
};


class CMainToolBar: public CToolBarEx
{
	Q_OBJECT
protected:
	UpdateToolTip m_tooltip;
	CImageListEx m_ImageList, m_ImageListDisabled;
	QFont m_font;
	bool m_fontValid = false;
	COctaveEdit m_EditOctave;
	CNumberEdit *m_EditTempo = nullptr;
	QLineEdit *m_EditSpeed = nullptr, *m_EditRowsPerBeat = nullptr, *m_EditGlobalVolume = nullptr;
	QLabel *m_StaticTempo = nullptr, *m_StaticSpeed = nullptr, *m_StaticRowsPerBeat = nullptr, *m_StaticGlobalVolume = nullptr;
	mfcless::UpDown *m_SpinTempo = nullptr, *m_SpinSpeed = nullptr, *m_SpinOctave = nullptr, *m_SpinRowsPerBeat = nullptr, *m_SpinGlobalVolume = nullptr;
	int m_currentSpeed = 0, m_currentOctave = -1, m_currentRowsPerBeat = 0, m_currentGlobalVolume = 0;
	TEMPO m_currentTempo{1, 0};
	bool m_updating = false;
public:
	CStereoVU *m_VuMeter = nullptr;

public:
	CMainToolBar() : m_EditOctave{*this} { }

protected:
	void SetRowsPerBeat(ROWINDEX nNewRPB);

public:
	void SetHorizontal() override;
	void SetVertical() override;
public:
	bool Create(QWidget *parent);
	void Init(CMainFrame *);
	uint32 GetBaseOctave() const;
	void SetBaseOctave(uint32 nOctave);
	void SetCurrentSong(CSoundFile *pModDoc);

	bool ShowUpdateInfo(const mpt::ustring &newVersion, const mpt::ustring &infoURL, bool showHighLight);
	void RemoveUpdateInfo();

	bool ToggleVisibility(MainToolBarItem item);
	void OnDPIChanged();

protected:
	void RefreshToolbar();
	void UpdateSizes();
	void UpdateControls();
	void OnVScroll(mfcless::ScrollCode nSBCode, int nPos, QWidget *pScrollBar) override;
	void OnButtonDropDown(uint32 id, QPoint pos) override;
	QString GetButtonToolTip(uint32 id) override;
	void OnSelectMIDIDevice(uint32 id);

	void OnSpeedChanged();
	void OnTempoChanged();
	void OnRPBChanged();
	void OnGlobalVolChanged();
	void RegisterMessageMap() override;
};


class CModTreeBar: public mfcless::Window<QWidget>
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
	mfcless::Point ptDragging;
	uint32 m_cxOriginal = 0, m_cyOriginal = 0, m_nTrackPos = 0;
	uint32 m_nTreeSplitRatio = 0;
	bool m_barOnLeft = true;  // mfcless: CBRS_ALIGN_LEFT of the bar style

	QLineEdit *m_filterEdit = nullptr;
	CModTree *m_filterSource = nullptr;
	uintptr_t m_filterTimer = 0;
	bool m_splitterConnected = false;

public:
	CModTree *m_pModTree = nullptr, *m_pModTreeData = nullptr;

	CModTreeBar();
	~CModTreeBar() override;

	void DelayShow(bool show);

public:
	void Init();
	void RecalcLayout();
	void DoMouseMove(mfcless::Point point);
	void DoLButtonDown(mfcless::Point point);
	void DoLButtonUp();
	void CancelTracking();
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
	bool BarOnLeft() { return m_barOnLeft; }

protected:
	bool PreTranslateMessage(mfcless::Message *pMsg) override;
	void CloseTreeFilter();
	void CancelTimer();

	int Padding() const;
	mfcless::Rect SplitterRect() const;
	QSplitter *ParentSplitter() const;
	// Sizes the bar in the main window's splitter from glTreeWindowWidth.
	void ApplyWidth();

protected:
	void OnSize(uint32 nType, int cx, int cy) override;
	void OnMouseMove(uint32 nFlags, mfcless::Point point) override;
	void OnLButtonDown(uint32, mfcless::Point) override;
	void OnLButtonUp(uint32, mfcless::Point) override;
	void OnRButtonDown(uint32, mfcless::Point) override { CancelTracking(); }
	void OnFilterChanged();
	void OnFilterLostFocus();
	void OnTimer(uintptr_t id) override;
	void RegisterMessageMap() override;
};

DECLARE_FLAGSET(CModTreeBar::Status)


OPENMPT_NAMESPACE_END
