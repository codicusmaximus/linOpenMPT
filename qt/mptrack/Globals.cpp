/*
 * Globals.cpp
 * -----------
 * Purpose: Implementation of the base classes for the upper and lower half of the MDI child windows.
 * Notes  : CModControlDlg = Upper half (Ctrl_*.cpp/h), which is contained inside a CModControlView together with the tab switcher (CModTabCtrl).
 *          CModScrollView = Lower half (View_*.cpp/h).
 *          mfcless twin of upstream's mptrack/Globals.cpp.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "Globals.h"
#include "CImageListEx.h"
#include "Childfrm.h"
#include "Ctrl_com.h"
#include "Ctrl_gen.h"
#include "Ctrl_ins.h"
#include "Ctrl_pat.h"
#include "Ctrl_smp.h"
#include "HighDPISupport.h"
#include "ImageLists.h"
#include "InputHandler.h"
#include "Mainfrm.h"
#include "Moddoc.h"
#include "Mptrack.h"
#include "resource.h"
#include "TrackerSettings.h"
#include "WindowMessages.h"
#include "../soundlib/mod_specifications.h"

#include "mfcless/ModMessage.h"
#include "mfcless/QtString.h"

#include <QtGui/QKeyEvent>
#include <QtGui/QWheelEvent>
#include <QtWidgets/QApplication>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QTabBar>
#include <QtWidgets/QMenu>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QWidgetAction>

#include "mfcless/Resources.h"


OPENMPT_NAMESPACE_BEGIN


static void RestoreLastFocusItem(QWidget *parent, QWidget *&lastFocusItem)
{
	if(lastFocusItem && parent->isAncestorOf(lastFocusItem) && lastFocusItem->isEnabled() && lastFocusItem->isVisible())
	{
		lastFocusItem->setFocus();
		return;
	}
	// The first control that takes focus with the tab key.
	for(QWidget *w = parent->nextInFocusChain(); w && w != parent; w = w->nextInFocusChain())
	{
		if(parent->isAncestorOf(w) && w->isEnabled() && w->isVisible() && (w->focusPolicy() & Qt::TabFocus))
		{
			(lastFocusItem = w)->setFocus();
			return;
		}
	}
	parent->setFocus();
}


// mfcless: the MDI child frame a view or dialog is in (MFC's GetParentFrame).
static CChildFrame *GetParentFrame(const QWidget *w)
{
	for(QWidget *p = w ? w->parentWidget() : nullptr; p != nullptr; p = p->parentWidget())
	{
		if(auto *frame = qobject_cast<CChildFrame *>(p))
			return frame;
	}
	return nullptr;
}


/////////////////////////////////////////////////////////////////////////////
// CModControlDlg

void CModControlDlg::RegisterMessageMap()
{
	DialogBase::RegisterMessageMap();
	// ON_WM_SIZE() (mfcless::Window)
	AddMessage(WM_MOD_UNLOCKCONTROLS, &CModControlDlg::OnUnlockControls);
}


CModControlDlg::CModControlDlg(CModControlView &parent, CModDoc &document) : DialogBase(&parent), m_modDoc(document), m_sndFile(document.GetSoundFile()), m_parent(parent)
{
}


CModControlDlg::~CModControlDlg()
{
}


void CModControlDlg::OnSize(uint32 nType, int cx, int cy)
{
	DialogBase::OnSize(nType, cx, cy);
	if (((nType == mfcless::size::Restored) || (nType == mfcless::size::Maximized)) && (cx > 0) && (cy > 0))
	{
		RecalcLayout();
	}
}


void CModControlDlg::SaveLastFocusItem(QWidget *hwnd)
{
	MPT_ASSERT(!hwnd || isAncestorOf(hwnd));
	if(hwnd)
		m_lastFocusItem = hwnd;
}


void CModControlDlg::RestoreLastFocusItem()
{
	OPENMPT_NAMESPACE::RestoreLastFocusItem(this, m_lastFocusItem);
}


void CModControlDlg::OnEditCut() { if(m_hWndView) mfcless::SendCommand(m_hWndView, ID_EDIT_CUT); }
void CModControlDlg::OnEditCopy() { if(m_hWndView) mfcless::SendCommand(m_hWndView, ID_EDIT_COPY); }
void CModControlDlg::OnEditPaste() { if(m_hWndView) mfcless::SendCommand(m_hWndView, ID_EDIT_PASTE); }
void CModControlDlg::OnEditMixPaste() { if(m_hWndView) mfcless::SendCommand(m_hWndView, ID_EDIT_MIXPASTE); }
void CModControlDlg::OnEditMixPasteITStyle() { if(m_hWndView) mfcless::SendCommand(m_hWndView, ID_EDIT_MIXPASTE_ITSTYLE); }
void CModControlDlg::OnEditPasteFlood() { if(m_hWndView) mfcless::SendCommand(m_hWndView, ID_EDIT_PASTEFLOOD); }
void CModControlDlg::OnEditPushForwardPaste() { if(m_hWndView) mfcless::SendCommand(m_hWndView, ID_EDIT_PUSHFORWARDPASTE); }
void CModControlDlg::OnEditFind() { if(m_hWndView) mfcless::SendCommand(m_hWndView, ID_EDIT_FIND); }
void CModControlDlg::OnEditFindNext() { if(m_hWndView) mfcless::SendCommand(m_hWndView, ID_EDIT_FINDNEXT); }
void CModControlDlg::OnSwitchToView() { if(m_hWndView) mfcless::PostModMessage(m_hWndView, WM_MOD_VIEWMSG, VIEWMSG_SETFOCUS, 0); }


intptr_t CModControlDlg::OnModCtrlMsg(uintptr_t wParam, intptr_t lParam)
{
	switch(wParam)
	{
	case CTRLMSG_SETVIEWWND:
		m_hWndView = reinterpret_cast<QWidget *>(lParam);
		break;

	case CTRLMSG_ACTIVATEPAGE:
		OnActivatePage(lParam);
		break;

	case CTRLMSG_DEACTIVATEPAGE:
		OnDeactivatePage();
		break;

	case CTRLMSG_SETFOCUS:
		if(CChildFrame *frame = GetParentFrame(this))
			frame->SetActiveView(&m_parent);
		RestoreLastFocusItem();
		break;
	}
	return 0;
}


intptr_t CModControlDlg::SendViewMessage(uint32 uMsg, intptr_t lParam) const
{
	if(m_hWndView)
		return mfcless::SendModMessage(m_hWndView, WM_MOD_VIEWMSG, uMsg, lParam);
	return 0;
}


bool CModControlDlg::PostViewMessage(uint32 uMsg, intptr_t lParam) const
{
	if(m_hWndView)
	{
		mfcless::PostModMessage(m_hWndView, WM_MOD_VIEWMSG, uMsg, lParam);
		return true;
	}
	return false;
}

void CModControlDlg::SwitchToView() const
{
	SendViewMessage(VIEWMSG_SETACTIVE);
}

void CModControlDlg::SwitchToViewIfMouse() const
{
	if(m_lastInputDevice == InputDevice::Mouse)
		SendViewMessage(VIEWMSG_SETACTIVE);
}

void CModControlDlg::UnlockControls()
{
	mfcless::PostModMessage(const_cast<CModControlDlg *>(this), WM_MOD_UNLOCKCONTROLS);
}


/////////////////////////////////////////////////////////////////////////////
// CModControlView

CModTabCtrl::CModTabCtrl(QWidget *parent)
	: QTabBar(parent)
{
	setDrawBase(true);
	setExpanding(false);
	setFocusPolicy(Qt::NoFocus);  // TCS_FOCUSNEVER
	OnDPIChanged();
}


bool CModTabCtrl::InsertItem(int nIndex, const mpt::ustring &text, intptr_t lParam, int iImage)
{
	const int index = insertTab(nIndex, mfcless::ToQString(text));
	setTabData(index, QVariant::fromValue<qlonglong>(lParam));
	if(iImage >= 0)
	{
		if(CMainFrame *pMainFrm = CMainFrame::GetMainFrame())
			setTabIcon(index, pMainFrm->m_MiscIcons.GetIcon(iImage));
	}
	return index >= 0;
}


intptr_t CModTabCtrl::GetItemData(int nIndex)
{
	if(nIndex < 0 || nIndex >= count())
		return 0;
	return static_cast<intptr_t>(tabData(nIndex).toLongLong());
}


void CModTabCtrl::OnDPIChanged()
{
	setFont(CMainFrame::GetGUIFont());
	setIconSize(QSize(16, 16));
}


/////////////////////////////////////////////////////////////////////////////////
// CModControlView

void CModControlView::RegisterMessageMap()
{
	// ON_WM_SIZE() (mfcless::Window)
	// ON_WM_DESTROY(): in the destructor
	// ON_WM_SETFOCUS() (mfcless::Window)
	AddMessage(WM_MOD_ACTIVATEVIEW,        &CModControlView::OnActivateModView);
	AddMessage(WM_MOD_CTRLMSG,             &CModControlView::OnModCtrlMsg);
	AddCommand(ID_EDIT_CUT,                &CModControlView::OnEditCut);
	AddCommand(ID_EDIT_COPY,               &CModControlView::OnEditCopy);
	AddCommand(ID_EDIT_PASTE,              &CModControlView::OnEditPaste);
	AddCommand(ID_EDIT_MIXPASTE,           &CModControlView::OnEditMixPaste);
	AddCommand(ID_EDIT_MIXPASTE_ITSTYLE,   &CModControlView::OnEditMixPasteITStyle);
	AddCommand(ID_EDIT_FIND,               &CModControlView::OnEditFind);
	AddCommand(ID_EDIT_FINDNEXT,           &CModControlView::OnEditFindNext);
}


const mfcless::ViewClass CModControlView::classCModControlView{"CModControlView", [](QWidget *parent) -> mfcless::DocumentView * { return new CModControlView(parent); }};


CModControlView::CModControlView(QWidget *parent)
	: Window(parent)
{
	setFocusPolicy(Qt::StrongFocus);
	RegisterMessageMap();
}


CModControlView::~CModControlView()
{
	OnDestroy();
	SetDocument(nullptr);
}


void CModControlView::OnInitialUpdate() // called first time after construct
{
	CChildFrame *pParentFrame = GetParentFrame();
	if (pParentFrame) m_hWndView = pParentFrame->GetHwndView();
	m_TabCtrl = new CModTabCtrl(this);
	m_TabCtrl->show();
	connect(m_TabCtrl, &QTabBar::currentChanged, this, [this](int) { OnTabSelchange(); });
	UpdateView(UpdateHint().ModType());
	SetActivePage(Page::First);
}


void CModControlView::OnSetFocus(QWidget *pOldWnd)
{
	if(CModControlDlg *activeDlg = GetCurrentControlDlg())
		activeDlg->RestoreLastFocusItem();
	Window::OnSetFocus(pOldWnd);
}


void CModControlView::OnPaint()
{
	mfcless::PaintDC dc(this);
	OnDraw(&dc);
}


void CModControlView::OnSize(uint32 nType, int cx, int cy)
{
	Window::OnSize(nType, cx, cy);
	if (((nType == mfcless::size::Restored) || (nType == mfcless::size::Maximized)) && (cx > 0) && (cy > 0))
	{
		RecalcLayout();
	}
}


void CModControlView::RecalcLayout()
{
	if (m_TabCtrl == nullptr) return;
	const QRect rcClient = rect();
	const int tabHeight = m_TabCtrl->sizeHint().height();
	m_TabCtrl->setGeometry(rcClient.left(), rcClient.top(), rcClient.width(), tabHeight);
	if(QWidget *pDlg = GetCurrentControlDlg())
	{
		pDlg->setGeometry(rcClient.left(), rcClient.top() + tabHeight, rcClient.width(), std::max(0, rcClient.height() - tabHeight));
	}
}


void CModControlView::OnUpdate(mfcless::DocumentView *, intptr_t lHint, QObject *pHint)
{
	UpdateView(UpdateHint::FromLPARAM(lHint), pHint);
}


void CModControlView::ForceRefresh()
{
	SetActivePage(GetActivePage());
}


CModControlDlg *CModControlView::GetCurrentControlDlg() const
{
	if(m_nActiveDlg >= Page::First && m_nActiveDlg < Page::NumPages)
		return m_Pages[static_cast<size_t>(m_nActiveDlg)];
	else
		return nullptr;
}


bool CModControlView::SetActivePage(Page page, intptr_t lParam)
{
	CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
	CModControlDlg *pDlg = nullptr;

	if(page == Page::Unknown)
		page = static_cast<Page>(m_TabCtrl->currentIndex());

	const uint32 nID = static_cast<uint32>(m_TabCtrl->GetItemData(static_cast<int>(page)));
	if(nID == 0)
		return false;

	switch(nID)
	{
		case IDD_CONTROL_COMMENTS:
			page = Page::Comments;
			break;
		case IDD_CONTROL_GLOBALS:
			page = Page::Globals;
			break;
		case IDD_CONTROL_PATTERNS:
			page = Page::Patterns;
			break;
		case IDD_CONTROL_SAMPLES:
			page = Page::Samples;
			break;
		case IDD_CONTROL_INSTRUMENTS:
			page = Page::Instruments;
			break;
		default:
			return false;
	}

	if(page < Page::First || page >= Page::NumPages || !pMainFrm)
		return false;

	CChildFrame *parentFrame = GetParentFrame();
	CModControlDlg *oldActiveDlg = GetCurrentControlDlg();
	if(oldActiveDlg && parentFrame)
		oldActiveDlg->GetSplitPosRef() = parentFrame->GetSplitterHeight();

	if(page == m_nActiveDlg)
	{
		pDlg = oldActiveDlg;
		mfcless::PostModMessage(this, WM_MOD_CTRLMSG, CTRLMSG_ACTIVATEPAGE, lParam);
		return true;
	}
	if(oldActiveDlg)
	{
		OnModCtrlMsg(CTRLMSG_DEACTIVATEPAGE, 0);
		oldActiveDlg->hide();
	}
	if(m_Pages[static_cast<size_t>(page)]) // Ctrl window already created?
	{
		m_nActiveDlg = page;
		pDlg = m_Pages[static_cast<size_t>(page)];
		pDlg->ForgetLastFocusItem();
	} else // Ctrl window is not created yet - creating one.
	{
		m_nActiveDlg = Page::Unknown;
		MPT_ASSERT_ALWAYS(GetDocument() != nullptr);
		switch(nID)
		{
		case IDD_CONTROL_COMMENTS:
			pDlg = new CCtrlComments(*this, *GetDocument());
			break;
		case IDD_CONTROL_GLOBALS:
			pDlg = new CCtrlGeneral(*this, *GetDocument());
			break;
		case IDD_CONTROL_PATTERNS:
			pDlg = new CCtrlPatterns(*this, *GetDocument());
			break;
		case IDD_CONTROL_SAMPLES:
			pDlg = new CCtrlSamples(*this, *GetDocument());
			break;
		case IDD_CONTROL_INSTRUMENTS:
			pDlg = new CCtrlInstruments(*this, *GetDocument());
			break;
		default:
			return false;
		}
		pDlg->SetViewWnd(m_hWndView);
		if(!pDlg->Create(this)) // Creation failed.
		{
			delete pDlg;
			return false;
		}
		m_nActiveDlg = page;
		m_Pages[static_cast<size_t>(page)] = pDlg;
	}
	RecalcLayout();
	pMainFrm->SetUserText(mpt::ustring());
	pMainFrm->SetInfoText(mpt::ustring());
	pMainFrm->SetXInfoText(mpt::ustring());
	pDlg->show();
	if(parentFrame)
		parentFrame->SetSplitterHeight(pDlg->GetSplitPosRef());
	if(m_hWndMDI)
		mfcless::PostModMessage(m_hWndMDI, WM_MOD_CHANGEVIEWCLASS, static_cast<uintptr_t>(lParam), reinterpret_cast<intptr_t>(pDlg));
	return true;
}


void CModControlView::OnDestroy()
{
	m_nActiveDlg = Page::Unknown;
	for(auto &pDlg : m_Pages)
	{
		if(pDlg)
		{
			delete pDlg;
			pDlg = nullptr;
		}
	}
}


void CModControlView::UpdateView(UpdateHint lHint, QObject *pObject)
{
	QWidget *pActiveDlg = nullptr;
	CModDoc *pDoc = GetDocument();
	if(!pDoc)
		return;
	// Module type changed: update tabs
	if (lHint.GetType()[HINT_MODTYPE])
	{
		uint32 nCount = 4;
		uint32 mask = 1 | 2 | 4 | 16;

		if(pDoc->GetSoundFile().GetModSpecifications().instrumentsMax > 0 || pDoc->GetNumInstruments() > 0)
		{
			mask |= 8;
			//mask |= 32; //rewbs.graph
			nCount++;
		}
		if (nCount != static_cast<uint32>(m_TabCtrl->count()))
		{
			uint32 count = 0;
			pActiveDlg = GetCurrentControlDlg();
			if(pActiveDlg)
				pActiveDlg->hide();
			const QSignalBlocker block(m_TabCtrl);
			while(m_TabCtrl->count())
				m_TabCtrl->removeTab(0);
			if (mask & 1) m_TabCtrl->InsertItem(count++, U_("General"), IDD_CONTROL_GLOBALS, IMAGE_GENERAL);
			if (mask & 2) m_TabCtrl->InsertItem(count++, U_("Patterns"), IDD_CONTROL_PATTERNS, IMAGE_PATTERNS);
			if (mask & 4) m_TabCtrl->InsertItem(count++, U_("Samples"), IDD_CONTROL_SAMPLES, IMAGE_SAMPLES);
			if (mask & 8) m_TabCtrl->InsertItem(count++, U_("Instruments"), IDD_CONTROL_INSTRUMENTS, IMAGE_INSTRUMENTS);
			//if (mask & 32) m_TabCtrl->InsertItem(count++, U_("Graph"), IDD_CONTROL_GRAPH, IMAGE_GRAPH); //rewbs.graph
			if (mask & 16) m_TabCtrl->InsertItem(count++, U_("Comments"), IDD_CONTROL_COMMENTS, IMAGE_COMMENTS);
			if(m_nActiveDlg >= Page::First)
				m_TabCtrl->setCurrentIndex(static_cast<int>(m_nActiveDlg));
		}
	}
	// Update child dialogs
	for (uint32 nIndex=0; nIndex<int(Page::NumPages); nIndex++)
	{
		CModControlDlg *pDlg = m_Pages[nIndex];
		if ((pDlg) && (pObject != pDlg)) pDlg->UpdateView(UpdateHint(lHint), pObject);
	}
	// Restore the displayed child dialog
	if (pActiveDlg) pActiveDlg->show();
}


void CModControlView::OnTabSelchange()
{
	SetActivePage(static_cast<Page>(m_TabCtrl->currentIndex()));
}


intptr_t CModControlView::OnActivateModView(uintptr_t nIndex, intptr_t lParam)
{
	if(QApplication::activeModalWidget() != nullptr)
	{
		// If we are in a dialog (e.g. Amplify Sample), do not allow to switch to a different tab. Otherwise, watch the tracker crash!
		return 0;
	}

	if (m_TabCtrl)
	{
		if (static_cast<Page>(nIndex) < Page::NumPages)
		{
			const QSignalBlocker block(m_TabCtrl);
			m_TabCtrl->setCurrentIndex(static_cast<int>(nIndex));
			SetActivePage(static_cast<Page>(nIndex), lParam);
		} else
		// Might be a dialog id IDD_XXXX
		{
			int nItems = m_TabCtrl->count();
			for (int i = 0; i < nItems; i++)
			{
				if (static_cast<uintptr_t>(m_TabCtrl->GetItemData(i)) == nIndex)
				{
					const QSignalBlocker block(m_TabCtrl);
					m_TabCtrl->setCurrentIndex(i);
					SetActivePage(static_cast<Page>(i), lParam);
					break;
				}
			}
		}
	}
	return 0;
}

void CModControlView::OnEditCut() { if(m_hWndView) mfcless::SendCommand(m_hWndView, ID_EDIT_CUT); }
void CModControlView::OnEditCopy() { if(m_hWndView) mfcless::SendCommand(m_hWndView, ID_EDIT_COPY); }
void CModControlView::OnEditPaste() { if(m_hWndView) mfcless::SendCommand(m_hWndView, ID_EDIT_PASTE); }
void CModControlView::OnEditMixPaste() { if(m_hWndView) mfcless::SendCommand(m_hWndView, ID_EDIT_MIXPASTE); }
void CModControlView::OnEditMixPasteITStyle() { if(m_hWndView) mfcless::SendCommand(m_hWndView, ID_EDIT_MIXPASTE_ITSTYLE); }
void CModControlView::OnEditFind() { if(m_hWndView) mfcless::SendCommand(m_hWndView, ID_EDIT_FIND); }
void CModControlView::OnEditFindNext() { if(m_hWndView) mfcless::SendCommand(m_hWndView, ID_EDIT_FINDNEXT); }
void CModControlView::OnSwitchToView() { if(m_hWndView) mfcless::PostModMessage(m_hWndView, WM_MOD_VIEWMSG, VIEWMSG_SETFOCUS, 0); }


intptr_t CModControlView::OnModCtrlMsg(uintptr_t wParam, intptr_t lParam)
{
	CModControlDlg *pActiveDlg = GetCurrentControlDlg();
	if(!pActiveDlg)
		return 0;
	switch(wParam)
	{
	case CTRLMSG_SETVIEWWND:
		m_hWndView = reinterpret_cast<QWidget *>(lParam);
		for(CModControlDlg *dlg : m_Pages)
		{
			if(dlg)
				dlg->SetViewWnd(m_hWndView);
		}
		break;
	}
	return pActiveDlg->OnModCtrlMsg(wParam, lParam);
}


void CModControlView::SampleChanged(SAMPLEINDEX smp)
{
	const CModDoc *modDoc = GetDocument();
	if(modDoc && modDoc->GetNumInstruments())
	{
		INSTRUMENTINDEX k = static_cast<INSTRUMENTINDEX>(GetInstrumentChange());
		if(!modDoc->IsChildSample(k, smp))
		{
			INSTRUMENTINDEX nins = modDoc->FindSampleParent(smp);
			if(nins != INSTRUMENTINDEX_INVALID)
			{
				InstrumentChanged(nins);
			}
		}
	} else
	{
		InstrumentChanged(smp);
	}
}


//////////////////////////////////////////////////////////////////
// CModScrollView

void CModScrollView::RegisterMessageMap()
{
	// ON_WM_DESTROY(): in the destructor
	// ON_WM_MOUSEWHEEL() (mfcless::Window)
	// ON_WM_MOUSEHWHEEL() (mfcless::Window)
	// ON_WM_SETFOCUS() (mfcless::Window)
	AddMessage(WM_MOD_VIEWMSG,            &CModScrollView::OnReceiveModViewMsg);
	AddMessage(WM_MOD_DRAGONDROPPING,     &CModScrollView::OnDragonDropping);
	AddMessage(WM_MOD_UPDATEPOSITION,     &CModScrollView::OnUpdatePosition);
}


CModScrollView::CModScrollView(QWidget *parent)
	: Window(parent)
{
	setFocusPolicy(Qt::StrongFocus);
	viewport()->setFocusProxy(this);
	setFrameShape(QFrame::NoFrame);
	RegisterMessageMap();
}


CModScrollView::~CModScrollView()
{
	OnDestroy();
	SetDocument(nullptr);
}


intptr_t CModScrollView::SendCtrlMessage(uint32 uMsg, intptr_t lParam) const
{
	if (m_hWndCtrl)	return mfcless::SendModMessage(m_hWndCtrl, WM_MOD_CTRLMSG, uMsg, lParam);
	return 0;
}


void CModScrollView::SendCtrlCommand(int id) const
{
	mfcless::SendCommand(m_hWndCtrl, id);
}


bool CModScrollView::PostCtrlMessage(uint32 uMsg, intptr_t lParam) const
{
	if (m_hWndCtrl)
	{
		mfcless::PostModMessage(m_hWndCtrl, WM_MOD_CTRLMSG, uMsg, lParam);
		return true;
	}
	return false;
}


intptr_t CModScrollView::OnReceiveModViewMsg(uintptr_t wParam, intptr_t lParam)
{
	return OnModViewMsg(wParam, lParam);
}


void CModScrollView::SaveLastFocusItem(QWidget *hwnd)
{
	MPT_ASSERT(!hwnd || isAncestorOf(hwnd));
	if(hwnd)
		m_lastFocusItem = hwnd;
}


void CModScrollView::OnSetFocus(QWidget *pOldWnd)
{
	if(m_lastFocusItem && m_lastFocusItem != this)
		RestoreLastFocusItem(this, m_lastFocusItem);
	Window::OnSetFocus(pOldWnd);
}


void CModScrollView::OnUpdate(mfcless::DocumentView *pView, intptr_t lHint, QObject *pHint)
{
	if (pView != this) UpdateView(UpdateHint::FromLPARAM(lHint), pHint);
}


intptr_t CModScrollView::OnModViewMsg(uintptr_t wParam, intptr_t lParam)
{
	switch(wParam)
	{
	case VIEWMSG_SETCTRLWND:
		m_hWndCtrl = reinterpret_cast<QWidget *>(lParam);
		break;

	case VIEWMSG_SETFOCUS:
	case VIEWMSG_SETACTIVE:
		if(CChildFrame *frame = GetParentFrame())
			frame->SetActiveView(this);
		if(m_lastFocusItem)
			RestoreLastFocusItem(this, m_lastFocusItem);
		else
			setFocus();
		break;
	}
	return 0;
}


void CModScrollView::OnInitialUpdate()
{
	m_dpi = HighDPISupport::GetDpiForWindow(this);
}


bool CModScrollView::PreTranslateMessage(mfcless::Message *pMsg)
{
	// We handle keypresses before Windows has a chance to handle them (for alt etc..)
	if(pMsg->message == mfcless::msg::KeyDown || pMsg->message == mfcless::msg::KeyUp || pMsg->message == mfcless::msg::SysKeyUp || pMsg->message == mfcless::msg::SysKeyDown)
	{
		CInputHandler *ih = CMainFrame::GetInputHandler();
		const auto event = ih->Translate(*pMsg);
		if(ih->KeyEvent(kCtxAllContexts, event) != kcNull)
			return true;  // Mapped to a command, no need to pass message on.
	}

	return Window::PreTranslateMessage(pMsg);
}


void CModScrollView::UpdateIndicator(const mpt::ustring &text)
{
	CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
	if (pMainFrm) pMainFrm->SetUserText(text);
}


// Accumulate mouse wheel steps for laptop precision touchpads that emit wheel events < WHEEL_DELTA
static short RoundMouseWheelToWholeStep(int value, int &accum)
{
	accum += value;
	value = mpt::align_down(accum, mfcless::WheelDelta);
	accum -= value;
	return mpt::saturate_cast<short>(value);
}


bool CModScrollView::OnMouseWheel(uint32 fFlags, short zDelta, mfcless::Point point)
{
	// we don't handle anything but scrolling just now
	if(fFlags & (mfcless::MouseKey::Shift | mfcless::MouseKey::Control))
		return false;

	// we can't get out of it--perform the scroll ourselves
	// (mfcless: CScrollView::DoMouseWheel: a wheel step scrolls by the
	// system's number of lines, 3 by default, at most a page)
	const short steps = RoundMouseWheelToWholeStep(zDelta, m_nScrollPosYfine);
	if(!steps)
		return true;
	MPT_UNUSED(point);
	const int lines = 3;
	int scroll = -steps * lines * std::max(1, m_lineDev.height()) / mfcless::WheelDelta;
	if(m_pageDev.height() > 0)
		scroll = std::clamp(scroll, -m_pageDev.height(), m_pageDev.height());
	return OnScrollBy(mfcless::Size(0, scroll), true);
}


void CModScrollView::OnMouseHWheel(uint32 fFlags, short zDelta, mfcless::Point point)
{
	zDelta = RoundMouseWheelToWholeStep(zDelta, m_nScrollPosXfine);

	// we don't handle anything but scrolling just now
	if(fFlags & (mfcless::MouseKey::Shift | mfcless::MouseKey::Control))
	{
		Window::OnMouseHWheel(fFlags, zDelta, point);
		return;
	}

	if(OnScrollBy(mfcless::Size(zDelta * m_lineDev.width() / mfcless::WheelDelta, 0), true))
		viewport()->update();
}


void CModScrollView::OnDestroy()
{
	CModDoc *pModDoc = GetDocument();
	CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
	if ((pMainFrm) && (pModDoc))
	{
		if (pMainFrm->GetFollowSong(pModDoc) == this)
		{
			pModDoc->SetNotifications(Notification::Default);
			pModDoc->SetFollowWnd(nullptr);
		}
		if (pMainFrm->GetMidiRecordWnd() == this)
		{
			pMainFrm->SetMidiRecordWnd(nullptr);
		}
	}
}


intptr_t CModScrollView::OnUpdatePosition(uintptr_t, intptr_t lParam)
{
	Notification *pnotify = reinterpret_cast<Notification *>(lParam);
	if (pnotify) return OnPlayerNotify(pnotify);
	return 0;
}


bool CModScrollView::OnScroll(uint32 nScrollCode, uint32 nPos, bool bDoScroll)
{
	// mfcless: what CScrollView::OnScroll does, on Qt's scroll bars.
	const auto codeX = static_cast<mfcless::ScrollCode>(nScrollCode & 0xFF), codeY = static_cast<mfcless::ScrollCode>((nScrollCode >> 8) & 0xFF);
	if(codeX == mfcless::ScrollCode::ThumbTrack)
		m_nScrollPosX = nPos;
	else if(codeY == mfcless::ScrollCode::ThumbTrack)
		m_nScrollPosY = nPos;
	if(bDoScroll)
		m_nScrollPosXfine = m_nScrollPosYfine = 0;

	auto delta = [](mfcless::ScrollCode code, int pos, int cur, int line, int page, int max) {
		switch(code)
		{
		case mfcless::ScrollCode::Top: return -cur;
		case mfcless::ScrollCode::Bottom: return max - cur;
		case mfcless::ScrollCode::LineUp: return -line;
		case mfcless::ScrollCode::LineDown: return line;
		case mfcless::ScrollCode::PageUp: return -page;
		case mfcless::ScrollCode::PageDown: return page;
		case mfcless::ScrollCode::ThumbTrack:
		case mfcless::ScrollCode::ThumbPosition: return pos - cur;
		default: return 0;
		}
	};
	const QPoint cur = GetScrollPosition();
	const mfcless::Size scroll(
		(nScrollCode & 0xFF) != 0xFF ? delta(codeX, static_cast<int>(nPos), cur.x(), m_lineDev.width(), m_pageDev.width(), horizontalScrollBar()->maximum()) : 0,
		(nScrollCode >> 8) != 0xFF ? delta(codeY, static_cast<int>(nPos), cur.y(), m_lineDev.height(), m_pageDev.height(), verticalScrollBar()->maximum()) : 0);
	return OnScrollBy(scroll, bDoScroll);
}


bool CModScrollView::OnScrollBy(mfcless::Size sizeScroll, bool bDoScroll)
{
	// mfcless: CScrollView::OnScrollBy
	const QPoint cur = GetScrollPosition();
	const int x = std::clamp(cur.x() + sizeScroll.cx, 0, horizontalScrollBar()->maximum());
	const int y = std::clamp(cur.y() + sizeScroll.cy, 0, verticalScrollBar()->maximum());
	bool ret = (x != cur.x() || y != cur.y());
	if(ret && bDoScroll)
	{
		m_inScroll = true;
		horizontalScrollBar()->setValue(x);
		verticalScrollBar()->setValue(y);
		m_inScroll = false;
		viewport()->update();
	}
	if(ret)
	{
		if(sizeScroll.cx)
			m_nScrollPosX = x;
		if(sizeScroll.cy)
			m_nScrollPosY = y;
		if(bDoScroll)
			m_nScrollPosXfine = m_nScrollPosYfine = 0;
	}
	return ret;
}


int CModScrollView::SetScrollPos(Qt::Orientation bar, int nPos, bool bRedraw)
{
	QScrollBar *sb = (bar == Qt::Horizontal) ? horizontalScrollBar() : verticalScrollBar();
	const int old = sb->value();
	if(bar == Qt::Horizontal)
		m_nScrollPosX = nPos;
	else
		m_nScrollPosY = nPos;
	m_inScroll = true;
	sb->setValue(nPos);
	m_inScroll = false;
	if(bRedraw)
		viewport()->update();
	return old;
}


int CModScrollView::GetScrollPos(Qt::Orientation bar) const
{
	return (bar == Qt::Horizontal) ? m_nScrollPosX : m_nScrollPosY;
}


QPoint CModScrollView::GetScrollPosition() const
{
	return QPoint(horizontalScrollBar()->value(), verticalScrollBar()->value());
}


void CModScrollView::ScrollToPosition(QPoint pt)
{
	OnScrollBy(mfcless::Size(pt.x() - GetScrollPosition().x(), pt.y() - GetScrollPosition().y()), true);
}


void CModScrollView::SetScrollSizes(mfcless::Size sizeTotal, const mfcless::Size &sizePageIn, const mfcless::Size &sizeLineIn)
{
	const QSize sizePage = sizePageIn, sizeLine = sizeLineIn;
	m_totalDev = sizeTotal;
	// CScrollView's defaults: a tenth of the total size per page and a tenth
	// of the page per line.
	m_pageDev = sizePage.isEmpty() ? QSize(sizeTotal.cx / 10, sizeTotal.cy / 10) : sizePage;
	m_lineDev = sizeLine.isEmpty() ? QSize(m_pageDev.width() / 10, m_pageDev.height() / 10) : sizeLine;
	UpdateBars();
	// Fix scroll positions
	m_nScrollPosX = horizontalScrollBar()->value();
	m_nScrollPosY = verticalScrollBar()->value();
}


void CModScrollView::UpdateBars()
{
	const QSize view = viewport()->size();
	m_inScroll = true;
	horizontalScrollBar()->setRange(0, std::max(0, m_totalDev.width() - view.width()));
	horizontalScrollBar()->setPageStep(std::max(1, view.width()));
	horizontalScrollBar()->setSingleStep(std::max(1, m_lineDev.width()));
	verticalScrollBar()->setRange(0, std::max(0, m_totalDev.height() - view.height()));
	verticalScrollBar()->setPageStep(std::max(1, view.height()));
	verticalScrollBar()->setSingleStep(std::max(1, m_lineDev.height()));
	m_inScroll = false;
}


void CModScrollView::OnPaint()
{
	mfcless::PaintDC dc(this);
	OnDraw(&dc);
}


void CModScrollView::OnSize(uint32 nType, int cx, int cy)
{
	Window::OnSize(nType, cx, cy);
	UpdateBars();
}


void CModScrollView::scrollContentsBy(int dx, int dy)
{
	// The user moved a scroll bar: tell the view as Windows' scroll
	// messages do.
	if(m_inScroll)
		return;
	if(dx)
	{
		m_nScrollPosX = horizontalScrollBar()->value();
		OnScroll(static_cast<uint32>(mfcless::ScrollCode::ThumbPosition) | 0xFF00, m_nScrollPosX, false);
	}
	if(dy)
	{
		m_nScrollPosY = verticalScrollBar()->value();
		OnScroll(0xFF | (static_cast<uint32>(mfcless::ScrollCode::ThumbPosition) << 8), m_nScrollPosY, false);
	}
	viewport()->update();
}


////////////////////////////////////////////////////////////////////////////
// 	CModControlBar


CModControlBar::CModControlBar(QWidget *parent)
	: QToolBar(parent)
{
	setFloatable(false);
	setMovable(false);
}


bool CModControlBar::Init(CImageListEx &icons, CImageListEx &disabledIcons)
{
	OnDPIChanged();

	// Add bitmaps
	m_icons = &icons;
	m_disabledIcons = &disabledIcons;
	UpdateStyle();
	return true;
}


bool CModControlBar::AddButton(uint32 nID, int iImage, uint32 nStyle, uint32 nState)
{
	if(nStyle & Separator)
	{
		addSeparator();
		return true;
	}
	QIcon icon;
	if(m_icons)
	{
		icon.addPixmap(m_icons->GetImage(iImage), QIcon::Normal);
		if(m_disabledIcons)
			icon.addPixmap(m_disabledIcons->GetImage(iImage), QIcon::Disabled);
	}
	QAction *action = new QAction(icon, QString(), this);
	action->setData(nID);
	action->setCheckable(nStyle & Check);
	action->setChecked(nState & StateChecked);
	action->setEnabled(nState & StateEnabled);
	// The tooltip text is the command's, from the string table.
	const QString tip = QString::fromStdU16String(mfcless::res::String(nID));
	action->setToolTip(tip.section(u'\n', -1));
	connect(action, &QAction::triggered, this, [this, nID] {
		if(m_target)
			m_target->DispatchCommand(nID);
	});
	if(nStyle & DropDown)
	{
		// A button with an arrow beside it: the arrow sends DropDown.
		auto *button = new QToolButton(this);
		button->setDefaultAction(action);
		button->setPopupMode(QToolButton::MenuButtonPopup);
		auto *menu = new QMenu(button);
		button->setMenu(menu);
		connect(menu, &QMenu::aboutToShow, this, [this, nID, menu] {
			// The owner shows its own menu; the empty one only makes the arrow.
			QMetaObject::invokeMethod(menu, &QMenu::close, Qt::QueuedConnection);
			if(m_target)
			{
				mfcless::NotifyInfo info;
				info.id = nID;
				info.code = mfcless::Notify::DropDown;
				info.control = this;
				m_target->DispatchNotify(info);
			}
		});
		addWidget(button);
	} else
	{
		addAction(action);
	}
	return true;
}


QAction *CModControlBar::GetButton(uint32 nID) const
{
	for(QAction *action : actions())
	{
		if(action->data().toUInt() == nID && !action->isSeparator())
			return action;
		if(auto *widgetAction = qobject_cast<QWidgetAction *>(action))
		{
			if(auto *button = qobject_cast<QToolButton *>(widgetAction->defaultWidget()); button && button->defaultAction() && button->defaultAction()->data().toUInt() == nID)
				return button->defaultAction();
		}
	}
	for(QToolButton *button : findChildren<QToolButton *>())
	{
		if(button->defaultAction() && button->defaultAction()->data().toUInt() == nID)
			return button->defaultAction();
	}
	return nullptr;
}


void CModControlBar::CheckButton(uint32 nID, bool check)
{
	if(QAction *action = GetButton(nID))
		action->setChecked(check);
}


bool CModControlBar::IsButtonChecked(uint32 nID) const
{
	QAction *action = GetButton(nID);
	return action && action->isChecked();
}


void CModControlBar::EnableButton(uint32 nID, bool enable)
{
	if(QAction *action = GetButton(nID))
		action->setEnabled(enable);
}


QPoint CModControlBar::GetDropDownPos(uint32 nID) const
{
	if(QAction *action = GetButton(nID))
	{
		for(QToolButton *button : findChildren<QToolButton *>())
		{
			if(button->defaultAction() == action)
				return button->mapToGlobal(QPoint(0, button->height()));
		}
		const QRect r = actionGeometry(action);
		return mapToGlobal(r.bottomLeft());
	}
	return mapToGlobal(QPoint(0, height()));
}


void CModControlBar::UpdateStyle()
{
	// Flat buttons are what Qt's toolbars have; raised ones get a frame.
	const bool flat = (TrackerSettings::Instance().patternSetup & PatternSetup::FlatToolbarButtons);
	setStyleSheet(flat ? QString() : QStringLiteral("QToolButton { border: 1px solid palette(mid); border-radius: 2px; margin: 1px; }"));
	update();
}


void CModControlBar::OnDPIChanged()
{
	const int imgSize = HighDPISupport::ScalePixels(16, this);
	setIconSize(QSize(imgSize, imgSize));
}


OPENMPT_NAMESPACE_END
