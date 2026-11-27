/*
 * Globals.h
 * ---------
 * Purpose: Implementation of the base classes for the upper and lower half of the MDI child windows.
 * Notes  : mfcless twin of upstream's mptrack/Globals.h.
 *          Draft from qt/tools/twin.py, not yet edited by hand.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include <QtWidgets/QAbstractScrollArea>
#include <QtWidgets/QTabBar>
#include <QtWidgets/QToolBar>
#include "DialogBase.h"
#include "Settings.h"
#include "UpdateHints.h"

class QEvent;
class QObject;
class QPoint;
class QSize;
class QWidget;

OPENMPT_NAMESPACE_BEGIN

#define ID_EDIT_MIXPASTE ID_EDIT_PASTE_SPECIAL

class CModControlView;
class CModDoc;
class CSoundFile;
struct DRAGONDROP;
struct Notification;

class CModControlBar: public QToolBar
{
	Q_OBJECT
public:
	bool Init(CImageList &icons, CImageList &disabledIcons);
	void UpdateStyle();
	bool AddButton(uint32 nID, int iImage=0, uint32 nStyle=TBSTYLE_BUTTON, uint32 nState=TBSTATE_ENABLED);
	void OnDPIChanged();
};


class CModControlDlg : public DialogBase
{
	Q_OBJECT
protected:
	CModDoc &m_modDoc;
	CSoundFile &m_sndFile;
	CModControlView &m_parent;
	QWidget * m_hWndView = nullptr;
	QWidget * m_lastFocusItem = nullptr;
	int m_nLockCount = 0;
	bool m_initialized = false;

public:
	CModControlDlg(CModControlView &parent, CModDoc &document);
	~CModControlDlg() override;
	
public:
	void SetViewWnd(QWidget * hwndView) { m_hWndView = hwndView; }
	QWidget * GetViewWnd() const { return m_hWndView; }
	intptr_t SendViewMessage(uint32 uMsg, intptr_t lParam = 0) const;
	bool PostViewMessage(uint32 uMsg, intptr_t lParam = 0) const;
	void SwitchToView() const;
	// Switch focus to lower view, but only if last interaction with the upper view was through the mouse
	void SwitchToViewIfMouse() const;
	void LockControls() { m_nLockCount++; }
	void UnlockControls();
	bool IsLocked() const { return (m_nLockCount > 0); }
	virtual Setting<int32> &GetSplitPosRef() = 0;

	void SaveLastFocusItem(QWidget * hwnd);
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
	virtual CRuntimeClass *GetAssociatedViewClass() { return nullptr; }
	virtual intptr_t OnModCtrlMsg(uintptr_t wParam, intptr_t lParam);
	virtual void OnActivatePage(intptr_t) {}
	virtual void OnDeactivatePage() {}
	void OnSize(uint32 nType, int cx, int cy);
	intptr_t OnUnlockControls(uintptr_t, intptr_t) { if (m_nLockCount > 0) m_nLockCount--; return 0; }
};


class CModTabCtrl: public QTabBar
{
	Q_OBJECT
public:
	bool InsertItem(int nIndex, const mpt::uchar * pszText, intptr_t lParam=0, int iImage=-1);
	bool Create(uint32 dwStyle, const RECT& rect, QWidget* pParentWnd, uint32 nID);
	intptr_t GetItemData(int nIndex);
	void OnDPIChanged();
};


class CModControlView: public QAbstractScrollArea
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
	CModTabCtrl m_TabCtrl;
	std::array<CModControlDlg *, int(Page::NumPages)> m_Pages = {{}};
	Page m_nActiveDlg = Page::Unknown;
	int m_nInstrumentChanged = -1;
	QWidget * m_hWndView = nullptr, m_hWndMDI = nullptr;

protected: // create from serialization only
	CModControlView() = default;
	Q_OBJECT
public:
	virtual ~CModControlView() = default;
	CModDoc *GetDocument() const noexcept;
	void SampleChanged(SAMPLEINDEX smp);
	void InstrumentChanged(int nInstr = -1) { m_nInstrumentChanged = nInstr; }
	int GetInstrumentChange() const { return m_nInstrumentChanged; }
	void SetMDIParentFrame(QWidget * hwnd) { m_hWndMDI = hwnd; }
	void ForceRefresh();
	CModControlDlg *GetCurrentControlDlg() const;

protected:
	void RecalcLayout();
	void UpdateView(UpdateHint hint, QObject *pHint = nullptr);
	bool SetActivePage(Page page = Page::Unknown, intptr_t lParam = -1);
public:
	Page GetActivePage() const { return m_nActiveDlg; }

protected:
protected:
	void OnInitialUpdate() override;
	void OnDraw(CDC *) override {}
	void OnUpdate(CView* pSender, intptr_t lHint, QObject* pHint) override;
protected:
	void OnSetFocus(QWidget *pOldWnd);
	void OnSize(uint32 nType, int cx, int cy);
	intptr_t OnDPIChangedAfterParent(uintptr_t = 0, intptr_t = 0);
	void OnDestroy();
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
};

// Non-client button attributes
#define NCBTNS_MOUSEOVER		0x01
#define NCBTNS_CHECKED			0x02
#define NCBTNS_DISABLED			0x04
#define NCBTNS_PUSHED			0x08


class CModScrollView: public QAbstractScrollArea
{
	Q_OBJECT
protected:
	QWidget * m_hWndCtrl = nullptr;
	QWidget * m_lastFocusItem = nullptr;
	int m_nScrollPosX = 0, m_nScrollPosY = 0;
	int m_nScrollPosXfine = 0, m_nScrollPosYfine = 0;
	int m_dpi = 0;  // Cached DPI settings

public:
	Q_OBJECT
	CModScrollView() = default;
	virtual ~CModScrollView() = default;

public:
	CModDoc *GetDocument() const noexcept;
	void SendCtrlCommand(int id) const;
	intptr_t SendCtrlMessage(uint32 uMsg, intptr_t lParam = 0) const;
	bool PostCtrlMessage(uint32 uMsg, intptr_t lParam = 0) const;
	void UpdateIndicator(const mpt::uchar * lpszText = nullptr);

public:
	void OnInitialUpdate() override;
	bool eventFilter(QObject *watched, QEvent *e) override;
	void OnDraw(CDC *) override {}
	void OnPrepareDC(CDC *, CPrintInfo *) override {}
	void OnUpdate(CView *pSender, intptr_t lHint, QObject *pHint) override;
	virtual void OnDPIChanged() { Invalidate(); }
	virtual void UpdateView(UpdateHint, QObject *) {}
	virtual intptr_t OnModViewMsg(uintptr_t wParam, intptr_t lParam);
	virtual bool OnDragonDrop(bool, const DRAGONDROP *) { return false; }
	virtual intptr_t OnPlayerNotify(Notification *) { return 0; }
	CModControlDlg *GetControlDlg() { return static_cast<CModControlView *>(QWidget::FromHandle(m_hWndCtrl))->GetCurrentControlDlg(); }

	void SaveLastFocusItem(QWidget * hwnd);

protected:
	void OnDestroy();
	void OnSetFocus(QWidget *pOldWnd);
	intptr_t OnReceiveModViewMsg(uintptr_t wParam, intptr_t lParam);
	bool OnMouseWheel(uint32 fFlags, short zDelta, QPoint point);
	void OnMouseHWheel(uint32 fFlags, short zDelta, QPoint point);
	intptr_t OnDragonDropping(uintptr_t bDoDrop, intptr_t lParam) { return OnDragonDrop((bool)bDoDrop, (const DRAGONDROP *)lParam); }
	intptr_t OnUpdatePosition(uintptr_t, intptr_t);
	intptr_t OnDPIChangedAfterParent(uintptr_t, intptr_t);
	// Fixes for 16-bit limitation in MFC's CScrollView and support for fractional mouse wheel movements
	bool OnScroll(uint32 nScrollCode, uint32 nPos, bool bDoScroll = true) override;
	bool OnScrollBy(QSize sizeScroll, bool bDoScroll = true) override;
	int SetScrollPos(int nBar, int nPos, bool bRedraw = true);
	void SetScrollSizes(int nMapMode, SIZE sizeTotal, const SIZE& sizePage = CScrollView::sizeDefault, const SIZE& sizeLine = CScrollView::sizeDefault);

	bool OnGesturePan(QPoint ptFrom, QPoint ptTo) override;
};


OPENMPT_NAMESPACE_END
