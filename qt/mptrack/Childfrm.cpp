/*
 * Childfrm.cpp
 * ------------
 * Purpose: Implementation of the MDI document child windows.
 * Notes  : mfcless twin of upstream's mptrack/Childfrm.cpp.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "Childfrm.h"
#include "ChannelManagerDlg.h"
#include "Ctrl_ins.h"
#include "Ctrl_pat.h"
#include "Ctrl_smp.h"
#include "Globals.h"
#include "HighDPISupport.h"
#include "Mainfrm.h"
#include "Moddoc.h"
#include "Mptrack.h"
#include "resource.h"
#include "view_com.h"
#include "View_gen.h"
#include "View_ins.h"
#include "View_pat.h"
#include "View_smp.h"
#include "WindowMessages.h"
#include "../common/FileReader.h"
#include "mpt/io/io.hpp"
#include "mpt/io/io_stdstream.hpp"

#include "mfcless/ModMessage.h"
#include "mfcless/QtString.h"

#include <QtGui/QCloseEvent>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMdiArea>
#include <QtWidgets/QSplitter>

#include <sstream>


OPENMPT_NAMESPACE_BEGIN


/////////////////////////////////////////////////////////////////////////////
// CChildFrame

void CChildFrame::RegisterMessageMap()
{
	// ON_WM_DESTROY, ON_WM_NCACTIVATE, ON_WM_MDIACTIVATE: OnDestroy, OnMDIActivate
	AddMessage(WM_MOD_CHANGEVIEWCLASS,    &CChildFrame::OnChangeViewClass);
	AddMessage(WM_MOD_INSTRSELECTED,      &CChildFrame::OnInstrumentSelected);
}

CChildFrame *CChildFrame::m_lastActiveFrame = nullptr;
int CChildFrame::glMdiOpenCount = 0;

/////////////////////////////////////////////////////////////////////////////
// CChildFrame construction/destruction

CChildFrame::CChildFrame()
{
	glMdiOpenCount++;
	setAttribute(Qt::WA_DeleteOnClose);
	RegisterMessageMap();
}


CChildFrame::~CChildFrame()
{
	OnDestroy();
	if ((--glMdiOpenCount) == 0)
	{
		TrackerSettings::Instance().gbMdiMaximize = m_maxWhenClosed;
	}
}


bool CChildFrame::event(QEvent *e)
{
	if(HandleModMessage(e))
		return true;
	return QMdiSubWindow::event(e);
}


CModControlView *CChildFrame::GetModControlView() const
{
	return m_wndSplitter ? qobject_cast<CModControlView *>(m_wndSplitter->widget(0)) : nullptr;
}


bool CChildFrame::OnCreateClient(CModDoc *document, const mfcless::ViewClass *)
{
	m_document = document;
	// create a splitter with 2 rows, 1 column
	m_wndSplitter = new QSplitter(Qt::Vertical);
	m_wndSplitter->setChildrenCollapsible(false);
	m_wndSplitter->setHandleWidth(4);
	setWidget(m_wndSplitter);

	// add the first splitter pane - the default view in row 0
	int cy = HighDPISupport::ScalePixels(TrackerSettings::Instance().glGeneralWindowHeight, this);  //rewbs.varWindowSize - default to general tab.
	auto *pModView = new CModControlView;
	m_wndSplitter->addWidget(pModView);
	pModView->SetDocument(document);

	// Get 2nd window handle
	m_hWndCtrl = pModView;
	pModView->SetMDIParentFrame(this);

	m_dpi = HighDPISupport::GetDpiForWindow(this);

	const bool bStatus = ChangeViewClass(&CViewGlobals::classCViewGlobals);
	if(cy <= 1) cy = 188;
	m_wndSplitter->setSizes({cy, std::max(1, height() - cy)});
	pModView->OnInitialUpdate();

	// If it all worked, we now have a splitter window which contain two different views
	return bStatus;
}


void CChildFrame::SetSplitterHeight(int cy)
{
	if (cy <= 1) cy = 188;	//default to 188? why not..
	cy = HighDPISupport::ScalePixels(cy, this);
	if(m_wndSplitter)
	{
		const int total = m_wndSplitter->height();
		m_wndSplitter->setSizes({cy, std::max(15, total - cy - m_wndSplitter->handleWidth())});
	}
}


intptr_t CChildFrame::OnDPIChangedAfterParent(uintptr_t, intptr_t)
{
	// Qt scales the whole window; nothing to do.
	return 0;
}


void CChildFrame::OnMDIActivate(bool bActivate)
{
	if(bActivate)
	{
		CMainFrame::GetMainFrame()->UpdateEffectKeys(GetActiveDocument());
		CMainFrame::GetMainFrame()->SetMidiRecordWnd(m_hWndView);
		m_lastActiveFrame = this;
	}
	if(m_hWndCtrl)
		mfcless::SendModMessage(m_hWndCtrl, bActivate ? WM_MOD_MDIACTIVATE : WM_MOD_MDIDEACTIVATE, 0, 0);
	if(m_hWndView)
		mfcless::SendModMessage(m_hWndView, bActivate ? WM_MOD_MDIACTIVATE : WM_MOD_MDIDEACTIVATE, 0, 0);

	// Update channel manager according to active document
	auto instance = CChannelManagerDlg::sharedInstance();
	if(instance != nullptr)
	{
		if(bActivate)
			instance->SetDocument(GetActiveDocument());
		else if(mdiArea() && mdiArea()->activeSubWindow() == nullptr)
			instance->SetDocument(nullptr);
	}
}


void CChildFrame::ActivateFrame(int nCmdShow)
{
	if ((glMdiOpenCount == 1) && (TrackerSettings::Instance().gbMdiMaximize) && (nCmdShow == -1))
	{
		showMaximized();
	} else
	{
		show();
	}
	if(mdiArea())
		mdiArea()->setActiveSubWindow(this);

	// When song first loads, initialise patternViewState to point to start of song.
	CModDoc *pModDoc = GetActiveDocument();
	if ((m_hWndCtrl) && (pModDoc))
	{
		if (m_initialActivation && m_ViewPatterns.nPattern == 0)
		{
			if(!pModDoc->GetSoundFile().Order().empty())
				m_ViewPatterns.nPattern = pModDoc->GetSoundFile().Order()[0];
			m_initialActivation = false;
		}
	}
}


void CChildFrame::OnUpdateFrameTitle(bool bAddToTitle)
{
	// update our parent window first
	if(CMainFrame *mainFrm = CMainFrame::GetMainFrame())
		mainFrm->OnUpdateFrameTitle(bAddToTitle);

	CModDoc *pDocument = GetActiveDocument();
	if (bAddToTitle && pDocument)
	{
		mpt::ustring szText = pDocument->GetTitle();
		if (m_nWindow > 0)
			szText += MPT_UFORMAT(":{}")(m_nWindow);
		// The modified mark, as Qt shows it.
		setWindowTitle(mfcless::ToQString(szText) + QStringLiteral("[*]"));
		setWindowModified(pDocument->IsModified());
	}
}


bool CChildFrame::ChangeViewClass(const mfcless::ViewClass *pViewClass)
{
	CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
	if (pViewClass->name == m_currentViewClassName) return true;
	if(!m_currentViewClassName.empty() && m_wndSplitter && m_wndSplitter->count() > 1)
	{
		m_currentViewClassName.clear();
		QWidget *old = m_wndSplitter->widget(1);
		if(m_activeView && m_activeView->ViewWidget() == old)
			m_activeView = nullptr;
		old->hide();
		old->deleteLater();
		old->setParent(nullptr);
	}
	if ((m_hWndView) && (pMainFrm))
	{
		if (pMainFrm->GetMidiRecordWnd() == m_hWndView)
		{
			pMainFrm->SetMidiRecordWnd(nullptr);
		}
	}
	m_hWndView = nullptr;
	const QList<int> sizes = m_wndSplitter->sizes();
	mfcless::DocumentView *view = pViewClass->create(nullptr);
	if(!view)
		return false;
	// Get 2nd window handle
	m_hWndView = view->ViewWidget();
	m_wndSplitter->insertWidget(1, m_hWndView);
	if(sizes.size() >= 2)
		m_wndSplitter->setSizes(sizes);
	view->SetDocument(m_document);
	m_currentViewClassName = pViewClass->name;
	if(auto *scrollView = dynamic_cast<CModScrollView *>(view))
		scrollView->OnInitialUpdate();
	if ((m_hWndView) && (m_hWndCtrl))
	{
		mfcless::PostModMessage(m_hWndView, WM_MOD_VIEWMSG, VIEWMSG_SETCTRLWND, reinterpret_cast<intptr_t>(m_hWndCtrl));
		mfcless::PostModMessage(m_hWndCtrl, WM_MOD_CTRLMSG, CTRLMSG_SETVIEWWND, reinterpret_cast<intptr_t>(m_hWndView));
		pMainFrm->SetMidiRecordWnd(m_hWndView);
	}
	return true;
}

void CChildFrame::ForceRefresh()
{
	CModControlView *pModView;
	if ((pModView = GetModControlView()) != nullptr)
	{
		pModView->ForceRefresh();
	}

	return;
}

void CChildFrame::SavePosition(bool force)
{
	m_maxWhenClosed = isMaximized();
	if (force)
		TrackerSettings::Instance().gbMdiMaximize = m_maxWhenClosed;
	if (!isMinimized() && m_wndSplitter)
	{
		QWidget *pWnd = m_wndSplitter->widget(0);
		if (pWnd)
		{
			const QRect rect = pWnd->geometry();
			if(rect.width() == 0)
				return;
			int l = HighDPISupport::ScalePixelsInv(rect.height(), this);
			//rewbs.varWindowSize - not the nicest piece of code, but we need to distinguish between the views:
			if(CViewGlobals::classCViewGlobals.name == m_currentViewClassName)
				TrackerSettings::Instance().glGeneralWindowHeight = l;
			else if(CViewPattern::classCViewPattern.name == m_currentViewClassName)
				TrackerSettings::Instance().glPatternWindowHeight = l;
			else if(CViewSample::classCViewSample.name == m_currentViewClassName)
				TrackerSettings::Instance().glSampleWindowHeight = l;
			else if(CViewInstrument::classCViewInstrument.name == m_currentViewClassName)
				TrackerSettings::Instance().glInstrumentWindowHeight = l;
			else if(CViewComments::classCViewComments.name == m_currentViewClassName)
				TrackerSettings::Instance().glCommentsWindowHeight = l;
		}
	}
}


int CChildFrame::GetSplitterHeight()
{
	if (m_wndSplitter)
	{
		QWidget *pWnd = m_wndSplitter->widget(0);
		if (pWnd)
		{
			return HighDPISupport::ScalePixelsInv(pWnd->height(), this);
		}
	}
	return 15;	// tidy default
};


intptr_t CChildFrame::SendCtrlMessage(uint32 uMsg, intptr_t lParam) const
{
	if(m_hWndCtrl)
		return mfcless::SendModMessage(m_hWndCtrl, WM_MOD_CTRLMSG, uMsg, lParam);
	return 0;
}


intptr_t CChildFrame::SendViewMessage(uint32 uMsg, intptr_t lParam) const
{
	if(m_hWndView)
		return mfcless::SendModMessage(m_hWndView, WM_MOD_VIEWMSG, uMsg, lParam);
	return 0;
}


intptr_t CChildFrame::ActivateView(uint32 nId, intptr_t lParam) { return mfcless::SendModMessage(m_hWndCtrl, WM_MOD_ACTIVATEVIEW, nId, lParam); }


intptr_t CChildFrame::OnInstrumentSelected(uintptr_t wParam, intptr_t lParam)
{
	CModDoc *pModDoc = GetActiveDocument();
	if ((m_hWndCtrl) && (pModDoc))
	{
		auto nIns = lParam;

		if ((!wParam) && (pModDoc->GetNumInstruments() > 0))
		{
			nIns = pModDoc->FindSampleParent(static_cast<SAMPLEINDEX>(nIns));
			if(nIns == INSTRUMENTINDEX_INVALID)
			{
				nIns = 0;
			}
		}
		mfcless::SendModMessage(m_hWndCtrl, WM_MOD_CTRLMSG, CTRLMSG_PAT_SETINSTRUMENT, nIns);
	}
	return 0;
}


void CChildFrame::SetActiveView(mfcless::DocumentView *view)
{
	m_activeView = view;
}


/////////////////////////////////////////////////////////////////////////////
// CChildFrame message handlers

void CChildFrame::OnDestroy()
{
	if(m_lastActiveFrame == this)
		m_lastActiveFrame = nullptr;
}


void CChildFrame::closeEvent(QCloseEvent *e)
{
	// The document closes with its last window, once it is saved or the
	// user lets it go (MFC: CDocument::CanCloseFrame, OnCloseDocument).
	CModDoc *doc = GetActiveDocument();
	if(doc)
	{
		int otherFrames = 0;
		if(mdiArea())
		{
			for(QMdiSubWindow *w : mdiArea()->subWindowList())
			{
				if(auto *frame = qobject_cast<CChildFrame *>(w); frame && frame != this && frame->GetActiveDocument() == doc)
					otherFrames++;
			}
		}
		if(otherFrames == 0 && !doc->SaveModified())
		{
			e->ignore();
			return;
		}
	}
	SavePosition();
	if(m_lastActiveFrame == this)
		m_lastActiveFrame = nullptr;
	// Detach the views from the document before they go.
	if(m_wndSplitter)
	{
		for(int i = 0; i < m_wndSplitter->count(); i++)
		{
			if(auto *view = dynamic_cast<mfcless::DocumentView *>(m_wndSplitter->widget(i)))
				view->SetDocument(nullptr);
		}
	}
	QMdiSubWindow::closeEvent(e);
	if(doc && doc->GetViews().empty())
		QMetaObject::invokeMethod(doc, [doc] { doc->OnCloseDocument(); }, Qt::QueuedConnection);
}


intptr_t CChildFrame::OnChangeViewClass(uintptr_t wParam, intptr_t lParam)
{
	CModControlDlg *pDlg = reinterpret_cast<CModControlDlg *>(lParam);
	if (pDlg)
	{
		const mfcless::ViewClass *pNewViewClass = pDlg->GetAssociatedViewClass();
		if (pNewViewClass) ChangeViewClass(pNewViewClass);
		mfcless::PostModMessage(m_hWndCtrl, WM_MOD_CTRLMSG, CTRLMSG_ACTIVATEPAGE, static_cast<intptr_t>(wParam));
	}
	return 0;
}


bool CChildFrame::IsPatternView() const
{
	return CViewPattern::classCViewPattern.name == m_currentViewClassName;
}


void CChildFrame::SaveAllViewStates()
{
	void *ptr = nullptr;
	if(CViewPattern::classCViewPattern.name == m_currentViewClassName)
		ptr = &m_ViewPatterns;
	else if(CViewSample::classCViewSample.name == m_currentViewClassName)
		ptr = &m_ViewSamples;
	else if(CViewInstrument::classCViewInstrument.name == m_currentViewClassName)
		ptr = &m_ViewInstruments;
	else if(CViewGlobals::classCViewGlobals.name == m_currentViewClassName)
		ptr = &m_ViewGeneral;
	else if(CViewComments::classCViewComments.name == m_currentViewClassName)
		ptr = &m_ViewComments;
	mfcless::SendModMessage(m_hWndView, WM_MOD_VIEWMSG, VIEWMSG_SAVESTATE, reinterpret_cast<intptr_t>(ptr));
}


std::string CChildFrame::SerializeView()
{
	SaveAllViewStates();

	std::ostringstream f(std::ios::out | std::ios::binary);
	// Version
	mpt::IO::WriteVarInt(f, 1u);
	// Current page
	mpt::IO::WriteVarInt(f, static_cast<uint8>(GetModControlView()->GetActivePage()));

	const auto Serialize = [](std::ostringstream &f, CModControlView::Page page, const std::string &s)
	{
		mpt::IO::WriteVarInt(f, static_cast<uint8>(page));
		mpt::IO::WriteVarInt(f, s.size());
		mpt::IO::WriteRaw(f, s.data(), s.size());
	};
	Serialize(f, CModControlView::Page::Patterns, m_ViewPatterns.Serialize());
	Serialize(f, CModControlView::Page::Samples, m_ViewSamples.Serialize());
	Serialize(f, CModControlView::Page::Instruments, m_ViewInstruments.Serialize());

	return std::move(f).str();
}


void CChildFrame::DeserializeView(FileReader &file)
{
	uint32 version, page;
	if(!file.ReadVarInt(version) || version > 1)
		return;
	if(!file.ReadVarInt(page) || page >= static_cast<uint32>(CModControlView::Page::NumPages))
		return;

	uint32 pageDlg = 0;
	switch(static_cast<CModControlView::Page>(page))
	{
	case CModControlView::Page::Globals:
		pageDlg = IDD_CONTROL_GLOBALS;
		break;
	case CModControlView::Page::Patterns:
		pageDlg = IDD_CONTROL_PATTERNS;
		if(version == 0)
			m_ViewPatterns.Deserialize(file);
		break;
	case CModControlView::Page::Samples:
		pageDlg = IDD_CONTROL_SAMPLES;
		if(version == 0)
			m_ViewSamples.Deserialize(file);
		break;
	case CModControlView::Page::Instruments:
		pageDlg = IDD_CONTROL_INSTRUMENTS;
		if(version == 0)
			m_ViewInstruments.Deserialize(file);
		break;
	case CModControlView::Page::Comments:
		pageDlg = IDD_CONTROL_COMMENTS;
		break;
	case CModControlView::Page::Unknown:
	case CModControlView::Page::NumPages:
		break;
	}

	// Version 1 extensions: Deserialize all views, not just current view
	while(file.CanRead(3))
	{
		uint32 size = 0;
		file.ReadVarInt(page);
		file.ReadVarInt(size);
		FileReader chunk = file.ReadChunk(size);
		if(page >= static_cast<uint32>(CModControlView::Page::NumPages) || !chunk.IsValid())
			continue;

		switch(static_cast<CModControlView::Page>(page))
		{
		case CModControlView::Page::Globals:     m_ViewGeneral.Deserialize(chunk); break;
		case CModControlView::Page::Patterns:    m_ViewPatterns.Deserialize(chunk); break;
		case CModControlView::Page::Samples:     m_ViewSamples.Deserialize(chunk); break;
		case CModControlView::Page::Instruments: m_ViewInstruments.Deserialize(chunk); break;
		case CModControlView::Page::Comments:    m_ViewComments.Deserialize(chunk); break;
		case CModControlView::Page::Unknown:
		case CModControlView::Page::NumPages:
			break;
		}
	}

	mfcless::PostModMessage(GetModControlView(), WM_MOD_ACTIVATEVIEW, pageDlg, intptr_t(-1));
}


void CChildFrame::ToggleViews()
{
	QWidget *focus = QApplication::focusWidget();
	if(focus == GetHwndView() || (GetHwndView() && GetHwndView()->isAncestorOf(focus)))
		SendCtrlMessage(CTRLMSG_SETFOCUS);
	else if(focus == GetHwndCtrl() || (GetHwndCtrl() && GetHwndCtrl()->isAncestorOf(focus)))
		SendViewMessage(VIEWMSG_SETFOCUS);
}


std::string PatternViewState::Serialize() const
{
	std::ostringstream f(std::ios::out | std::ios::binary);
	mpt::IO::WriteVarInt(f, initialized ? nOrder : initialOrder);
	mpt::IO::WriteVarInt(f, visibleColumns.to_ulong());
	return std::move(f).str();
}


void PatternViewState::Deserialize(FileReader &f)
{
	f.ReadVarInt(initialOrder);
	unsigned long columns = 0;
	if(f.ReadVarInt(columns))
		visibleColumns = columns;
}


std::string SampleViewState::Serialize() const
{
	std::ostringstream f(std::ios::out | std::ios::binary);
	mpt::IO::WriteVarInt(f, nSample ? nSample : initialSample);
	return std::move(f).str();
}


void SampleViewState::Deserialize(FileReader &f)
{
	f.ReadVarInt(initialSample);
}


std::string InstrumentViewState::Serialize() const
{
	std::ostringstream f(std::ios::out | std::ios::binary);
	mpt::IO::WriteVarInt(f, instrument ? instrument : initialInstrument);
	return std::move(f).str();
}


void InstrumentViewState::Deserialize(FileReader &f)
{
	f.ReadVarInt(initialInstrument);
}


OPENMPT_NAMESPACE_END
