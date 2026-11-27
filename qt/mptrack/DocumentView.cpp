/*
 * DocumentView.cpp
 * ----------------
 * Purpose: mfcless: the document/view framework upstream gets from MFC.
 * Notes  : Not a twin. What CDocument and CView did, as far as OpenMPT uses
 *          it.
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */

#include "DocumentView.h"
#include "ModDocTemplate.h"
#include "Moddoc.h"
#include "Mptrack.h"
#include "Childfrm.h"

#include "mfcless/ModMessage.h"

#include <QtCore/QThread>

#include "mfcless/QtString.h"

#include <QtCore/QFileInfo>
#include <QtGui/QGuiApplication>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMdiSubWindow>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QWidget>

#include <algorithm>

namespace mfcless
{


DocumentView::~DocumentView()
{
	SetDocument(nullptr);
}


void DocumentView::SetDocument(CModDoc *doc)
{
	if(doc == m_document)
		return;
	if(m_document)
		m_document->RemoveView(this);
	m_document = nullptr;
	if(doc)
		doc->AddView(this);
}


void DocumentView::PostMessage(uint32_t message, uintptr_t wParam, intptr_t lParam)
{
	PostModMessage(ViewWidget(), message, wParam, lParam);
}


intptr_t DocumentView::SendMessage(uint32_t message, uintptr_t wParam, intptr_t lParam)
{
	return SendModMessage(ViewWidget(), message, wParam, lParam);
}


void DocumentView::SendNotifyMessage(uint32_t message, uintptr_t wParam, intptr_t lParam)
{
	if(QThread::currentThread() == ViewWidget()->thread())
		SendModMessage(ViewWidget(), message, wParam, lParam);
	else
		PostModMessage(ViewWidget(), message, wParam, lParam);
}


OPENMPT_NAMESPACE::CChildFrame *DocumentView::GetParentFrame()
{
	for(QWidget *w = ViewWidget(); w; w = w->parentWidget())
	{
		if(auto *frame = dynamic_cast<OPENMPT_NAMESPACE::CChildFrame *>(w))
			return frame;
	}
	return nullptr;
}


Document::~Document()
{
	for(DocumentView *view : m_views)
		view->m_document = nullptr;
	m_views.clear();
	if(m_pDocTemplate)
		m_pDocTemplate->RemoveDocument(static_cast<CModDoc *>(this));
}


bool Document::OnNewDocument()
{
	DeleteContents();
	m_strPathName.clear();
	SetModifiedFlag(false);
	return true;
}


bool Document::OnOpenDocument(const ::mpt::uchar *)
{
	DeleteContents();
	SetModifiedFlag(false);
	return true;
}


bool Document::OnSaveDocument(const ::mpt::uchar *)
{
	SetModifiedFlag(false);
	return true;
}


void Document::OnCloseDocument()
{
	// Close the frames of the views first; their views leave the list as
	// they go.
	std::vector<QWidget *> frames;
	for(DocumentView *view : m_views)
	{
		QWidget *w = view->ViewWidget();
		QWidget *frame = w;
		while(frame && !qobject_cast<QMdiSubWindow *>(frame))
			frame = frame->parentWidget();
		if(!frame)
			frame = w;
		if(frame && std::find(frames.begin(), frames.end(), frame) == frames.end())
			frames.push_back(frame);
	}
	for(QWidget *frame : frames)
		delete frame;
	for(DocumentView *view : m_views)
		view->m_document = nullptr;
	m_views.clear();
	DeleteContents();
	delete this;
}


void Document::UpdateFrameCounts()
{
	std::vector<OPENMPT_NAMESPACE::CChildFrame *> frames;
	for(DocumentView *view : m_views)
	{
		if(auto *frame = view->GetParentFrame(); frame && std::find(frames.begin(), frames.end(), frame) == frames.end())
			frames.push_back(frame);
	}
	int number = 1;
	for(auto *frame : frames)
	{
		frame->SetWindowNumber(frames.size() > 1 ? number++ : 0);
		frame->OnUpdateFrameTitle(true);
	}
}


void Document::OnFileClose()
{
	if(!SaveModified())
		return;
	OnCloseDocument();
}


bool Document::SaveModified()
{
	if(!IsModified())
		return true;
	::mpt::ustring name = m_strPathName.empty() ? m_strTitle : FromQString(QFileInfo(ToQString(m_strPathName)).fileName());
	if(name.empty())
		name = U_("Untitled");
	const auto answer = QMessageBox::question(QApplication::activeWindow(), QGuiApplication::applicationDisplayName(),
		QStringLiteral("Save changes to %1?").arg(ToQString(name)),
		QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::Yes);
	switch(answer)
	{
	case QMessageBox::Cancel:
		return false;
	case QMessageBox::Yes:
		return DoFileSave();
	default:
		return true;
	}
}


bool Document::DoSave(const ::mpt::uchar *pathName, bool)
{
	if(!pathName || !*pathName)
		return false;
	if(!OnSaveDocument(pathName))
		return false;
	SetPathName(pathName);
	return true;
}


bool Document::DoFileSave()
{
	return DoSave(m_strPathName.empty() ? nullptr : m_strPathName.c_str(), m_strPathName.empty());
}


void Document::SetPathName(const ::mpt::uchar *pathName, bool addToMRU)
{
	m_strPathName = pathName ? ::mpt::ustring(pathName) : ::mpt::ustring();
	SetTitle(FromQString(QFileInfo(ToQString(m_strPathName)).fileName()));
	if(addToMRU && !m_strPathName.empty())
		OPENMPT_NAMESPACE::theApp.AddToRecentFileList(OPENMPT_NAMESPACE::mpt::PathString::FromUnicode(m_strPathName));
}


void Document::SetTitle(const ::mpt::ustring &title)
{
	m_strTitle = title;
}


void Document::AddView(DocumentView *view)
{
	if(std::find(m_views.begin(), m_views.end(), view) == m_views.end())
		m_views.push_back(view);
	view->m_document = static_cast<CModDoc *>(this);
}


void Document::RemoveView(DocumentView *view)
{
	m_views.erase(std::remove(m_views.begin(), m_views.end(), view), m_views.end());
	if(view->m_document == static_cast<CModDoc *>(this))
		view->m_document = nullptr;
}


DocumentView *Document::GetNextView(ViewPosition &pos) const
{
	if(!pos.valid || pos.index >= m_views.size())
	{
		pos.valid = false;
		return nullptr;
	}
	DocumentView *view = m_views[pos.index++];
	pos.valid = pos.index < m_views.size();
	return view;
}


void Document::UpdateAllViews(DocumentView *sender, intptr_t hint, QObject *hintObject)
{
	// A copy: a view may close while it updates.
	const std::vector<DocumentView *> views = m_views;
	for(DocumentView *view : views)
	{
		if(view != sender && std::find(m_views.begin(), m_views.end(), view) != m_views.end())
			view->OnUpdate(sender, hint, hintObject);
	}
}


void Document::BeginWaitCursor()
{
	QApplication::setOverrideCursor(Qt::WaitCursor);
}


void Document::EndWaitCursor()
{
	QApplication::restoreOverrideCursor();
}

} // namespace mfcless
