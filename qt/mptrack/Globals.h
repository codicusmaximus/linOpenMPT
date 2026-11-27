/*
 * Globals.h
 * ---------
 * Purpose: Implementation of the base classes for the upper and lower half of the MDI child windows.
 * Notes  : mfcless twin of upstream's mptrack/Globals.h. The views are Qt
 *          widgets that are also mfcless::DocumentView (document, updates,
 *          commands). CModScrollView keeps the scrolling model of the
 *          Windows version (a total size in pixels, a page and a line size,
 *          a scroll position) on a QAbstractScrollArea.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "DialogBase.h"
#include "mfcless/Window.h"
#include "DocumentView.h"
#include "Settings.h"
#include "UpdateHints.h"

#include <QtWidgets/QAbstractScrollArea>
#include <QtWidgets/QTabBar>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QWidget>

#include <array>

class QAction;
class QTabBar;

OPENMPT_NAMESPACE_BEGIN

#define ID_EDIT_MIXPASTE ID_EDIT_PASTE_SPECIAL

class CImageListEx;
class CModControlView;
class CModDoc;
class CSoundFile;
struct DRAGONDROP;
struct Notification;

class CModControlBar: public QToolBar
{
	Q_OBJECT
public:
	// mfcless: button styles and states (the values of Windows' toolbars).
	static constexpr uint32 Button = 0x00, Separator = 0x01, Check = 0x02, DropDown = 0x08;
	static constexpr uint32 StateChecked = 0x01, StateEnabled = 0x04;

	explicit CModControlBar(QWidget *parent = nullptr);

	bool Init(CImageListEx &icons, CImageListEx &disabledIcons);
	void UpdateStyle();
	bool AddButton(uint32 nID, int iImage=0, uint32 nStyle=Button, uint32 nState=StateEnabled);
	void OnDPIChanged();

	// mfcless: the toolbar's commands go to target, by ID (dropdown arrows
	// as the Notify::DropDown notification).
	void SetCommandTarget(mfcless::MessageMap *target) { m_target = target; }
	QAction *GetButton(uint32 nID) const;
	void CheckButton(uint32 nID, bool check = true);
	bool IsButtonChecked(uint32 nID) const;
	void EnableButton(uint32 nID, bool enable = true);
	// Where the drop-down menu of a button opens, in global coordinates.
	QPoint GetDropDownPos(uint32 nID) const;

private:
	CImageListEx *m_icons = nullptr, *m_disabledIcons = nullptr;
	mfcless::MessageMap *m_target = nullptr;
};


class CModControlDlg : public DialogBase
{
	Q_OBJECT
protected:
	CModDoc &m_modDoc;
	CSoundFile &m_sndFile;
	CModControlView &m_parent;
	QWidget *m_hWndView = nullptr;
	QWidget *m_lastFocusItem = nullptr;
	int m_nLockCount = 0;
	bool m_initialized = false;

public:
	CModControlDlg(CModControlView &parent, CModDoc &document);
	~CModControlDlg() override;
	
public:
	void SetViewWnd(QWidget *hwndView) { m_hWndView = hwndView; }
	QWidget *GetViewWnd() const { return m_hWndView; }
	intptr_t SendViewMessage(uint32 uMsg, intptr_t lParam = 0) const;
	bool PostViewMessage(uint32 uMsg, intptr_t lParam = 0) const;
	void SwitchToView() const;
	// Switch focus to lower view, but only if last interaction with the upper view was through the mouse
	void SwitchToViewIfMouse() const;
	void LockControls() { m_nLockCount++; }
	void UnlockControls();
	bool IsLocked() const { return (m_nLockCount > 0); }
	virtual Setting<int32> &GetSplitPosRef() = 0;

	void SaveLastFocusItem(QWidget *hwnd);
	void ForgetLastFocusItem() { m_lastFocusItem = nullptr; }
	void RestoreLastFocusItem();

	void OnEditCut();
	void OnEditCopy();
	void OnEditPaste();
	void OnEditMixPaste();
	void OnEditMixPasteITStyle();
	void OnEditPasteFlood();
	void OnEditPushForwardPaste();
	void OnEditFind();
	void OnEditFindNext();
	void OnSwitchToView();

	void OnOK() override {}
	void OnCancel() override {}
	void OnDPIChanged() override { RecalcLayout(); }
	virtual void RecalcLayout() = 0;
	virtual void UpdateView(UpdateHint, QObject *) = 0;
	virtual const mfcless::ViewClass *GetAssociatedViewClass() { return nullptr; }
	virtual intptr_t OnModCtrlMsg(uintptr_t wParam, intptr_t lParam);
	virtual void OnActivatePage(intptr_t) {}
	virtual void OnDeactivatePage() {}

	void OnSize(uint32 nType, int cx, int cy) override;
	intptr_t OnUnlockControls(uintptr_t, intptr_t) { if (m_nLockCount > 0) m_nLockCount--; return 0; }
	void RegisterMessageMap() override;
};


class CModTabCtrl: public QTabBar
{
	Q_OBJECT
public:
	explicit CModTabCtrl(QWidget *parent = nullptr);
	bool InsertItem(int nIndex, const mpt::ustring &text, intptr_t lParam=0, int iImage=-1);
	intptr_t GetItemData(int nIndex);
	void OnDPIChanged();
};


class CModControlView: public mfcless::Window<QWidget>, public mfcless::DocumentView
{
	Q_OBJECT
public:
	// Note: Page IDs are serialized to module window settings!
	enum class Page
	{
		Unknown = -1,
		First = 0,
		Globals = First,
		Patterns,
		Samples,
		Instruments,
		Comments,
		NumPages
	};

protected:
	CModTabCtrl *m_TabCtrl = nullptr;
	std::array<CModControlDlg *, int(Page::NumPages)> m_Pages = {{}};
	Page m_nActiveDlg = Page::Unknown;
	int m_nInstrumentChanged = -1;
	QWidget *m_hWndView = nullptr, *m_hWndMDI = nullptr;

public: // mfcless: created by CChildFrame
	explicit CModControlView(QWidget *parent = nullptr);
	static const mfcless::ViewClass classCModControlView;
	const mfcless::ViewClass *GetRuntimeClass() const override { return &classCModControlView; }

public:
	~CModControlView() override;
	CModDoc *GetDocument() const noexcept { return m_document; }
	void SampleChanged(SAMPLEINDEX smp);
	void InstrumentChanged(int nInstr = -1) { m_nInstrumentChanged = nInstr; }
	int GetInstrumentChange() const { return m_nInstrumentChanged; }
	void SetMDIParentFrame(QWidget *hwnd) { m_hWndMDI = hwnd; }
	void ForceRefresh();
	CModControlDlg *GetCurrentControlDlg() const;

protected:
	void RecalcLayout();
	void UpdateView(UpdateHint hint, QObject *pHint = nullptr);
	bool SetActivePage(Page page = Page::Unknown, intptr_t lParam = -1);
public:
	Page GetActivePage() const { return m_nActiveDlg; }

public:
	void OnInitialUpdate() override;
	void OnUpdate(mfcless::DocumentView *pSender, intptr_t lHint, QObject *pHint) override;
	QWidget *ViewWidget() override { return this; }

protected:
	void OnSize(uint32 nType, int cx, int cy) override;
	void OnDestroy();
	void OnSetFocus(QWidget *pOldWnd) override;
	// mfcless: CView's, drawing with OnDraw.
	void OnPaint() override;
	void OnTabSelchange();
	void OnEditCut();
	void OnEditCopy();
	void OnEditPaste();
	void OnEditMixPaste();
	void OnEditMixPasteITStyle();
	void OnEditFind();
	void OnEditFindNext();
	void OnSwitchToView();
	intptr_t OnActivateModView(uintptr_t, intptr_t);
	intptr_t OnModCtrlMsg(uintptr_t wParam, intptr_t lParam);
	void RegisterMessageMap() override;
};

// Non-client button attributes
#define NCBTNS_MOUSEOVER		0x01
#define NCBTNS_CHECKED			0x02
#define NCBTNS_DISABLED			0x04
#define NCBTNS_PUSHED			0x08


class CModScrollView: public mfcless::Window<QAbstractScrollArea>, public mfcless::DocumentView
{
	Q_OBJECT
protected:
	QWidget *m_hWndCtrl = nullptr;
	QWidget *m_lastFocusItem = nullptr;
	int m_nScrollPosX = 0, m_nScrollPosY = 0;
	int m_nScrollPosXfine = 0, m_nScrollPosYfine = 0;
	int m_dpi = 0;  // Cached DPI settings

	// mfcless: the scroll model of the Windows version (CScrollView), in pixels.
	QSize m_totalDev, m_pageDev, m_lineDev;

public:
	explicit CModScrollView(QWidget *parent = nullptr);
	~CModScrollView() override;

public:
	CModDoc *GetDocument() const noexcept { return m_document; }
	void SendCtrlCommand(int id) const;
	intptr_t SendCtrlMessage(uint32 uMsg, intptr_t lParam = 0) const;
	bool PostCtrlMessage(uint32 uMsg, intptr_t lParam = 0) const;
	void UpdateIndicator(const mpt::ustring &text = {});

public:
	void OnInitialUpdate() override;
	void OnUpdate(mfcless::DocumentView *pSender, intptr_t lHint, QObject *pHint) override;
	QWidget *ViewWidget() override { return this; }
	virtual void OnDPIChanged() { viewport()->update(); }
	virtual void UpdateView(UpdateHint, QObject *) {}
	virtual intptr_t OnModViewMsg(uintptr_t wParam, intptr_t lParam);
	virtual bool OnDragonDrop(bool, const DRAGONDROP *) { return false; }
	virtual intptr_t OnPlayerNotify(Notification *) { return 0; }

	CModControlDlg *GetControlDlg() { return static_cast<CModControlView *>(m_hWndCtrl)->GetCurrentControlDlg(); }

	void SaveLastFocusItem(QWidget *hwnd);

	// mfcless: the scroll position and sizes (CScrollView's).
	QPoint GetScrollPosition() const;
	QPoint GetDeviceScrollPosition() const { return GetScrollPosition(); }
	void ScrollToPosition(QPoint pt);
	QSize GetTotalSize() const { return m_totalDev; }
	int GetScrollPos(Qt::Orientation bar) const;

	bool PreTranslateMessage(mfcless::Message *pMsg) override;

protected:
	void scrollContentsBy(int dx, int dy) override;
	// mfcless: CScrollView's (the scroll bars follow the size).
	void OnSize(uint32 nType, int cx, int cy) override;
	// mfcless: CView's, drawing with OnDraw (in client coordinates).
	void OnPaint() override;
	void OnDestroy();
	intptr_t OnReceiveModViewMsg(uintptr_t wParam, intptr_t lParam);
	bool OnMouseWheel(uint32 fFlags, short zDelta, mfcless::Point point) override;
	void OnMouseHWheel(uint32 fFlags, short zDelta, mfcless::Point point) override;
	void OnSetFocus(QWidget *pOldWnd) override;
	intptr_t OnDragonDropping(uintptr_t bDoDrop, intptr_t lParam) { return OnDragonDrop(bDoDrop != 0, reinterpret_cast<const DRAGONDROP *>(lParam)); }
	intptr_t OnUpdatePosition(uintptr_t, intptr_t);

	// Fixes for 16-bit limitation in MFC's CScrollView and support for fractional mouse wheel movements
	// (mfcless: code is a mfcless::ScrollCode, for the horizontal bar in the
	// low byte and the vertical one in the high byte, as on Windows)
	virtual bool OnScroll(uint32 nScrollCode, uint32 nPos, bool bDoScroll = true);
	virtual bool OnScrollBy(mfcless::Size sizeScroll, bool bDoScroll = true);
	int SetScrollPos(Qt::Orientation bar, int nPos, bool bRedraw = true);
	void SetScrollSizes(mfcless::Size sizeTotal, const mfcless::Size &sizePage = {}, const mfcless::Size &sizeLine = {});
	void RegisterMessageMap() override;

private:
	void UpdateBars();
	bool m_inScroll = false;
};


OPENMPT_NAMESPACE_END
