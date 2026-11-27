/*
 * ModDocTemplate.cpp
 * ------------------
 * Purpose: CDocTemplate and CModDocManager specialization for CModDoc.
 * Notes  : mfcless twin of upstream's mptrack/ModDocTemplate.cpp.
 *          What CMultiDocTemplate and CDocManager did for OpenMPT is here too.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "ModDocTemplate.h"
#include <QtCore/QTimer>
#include <QtWidgets/QMdiArea>
#include <QtWidgets/QStyle>
#include "FolderScanner.h"
#include "Mainfrm.h"
#include "Moddoc.h"
#include "Mptrack.h"
#include "Reporting.h"
#include "SelectPluginDialog.h"
#include "TrackerSettings.h"
#include "../soundlib/plugins/PluginManager.h"
#include "Childfrm.h"

#include "mfcless/QtString.h"

#include <QtWidgets/QMdiArea>

OPENMPT_NAMESPACE_BEGIN


#ifdef MPT_ALL_LOGGING
#define DDEDEBUG
#endif


CModDoc *CModDocTemplate::OpenDocumentFile(const mpt::PathString &filename, bool addToMru, bool makeVisible)
{
	// First, remove document from MRU list.
	if(addToMru)
	{
		theApp.RemoveMruItem(filename);
	}

	// mfcless: CMultiDocTemplate::OpenDocumentFile
	CModDoc *pDoc = new CModDoc();
	AddDocument(pDoc);
	CChildFrame *pFrame = CreateNewFrame(pDoc);
	if(pFrame == nullptr)
	{
		delete pDoc;
		pDoc = nullptr;
	} else if(filename.empty())
	{
		SetDefaultTitle(pDoc);
		if(!pDoc->OnNewDocument())
		{
			pDoc->OnCloseDocument();
			pDoc = nullptr;
		} else
		{
			m_nUntitledCount++;
		}
	} else
	{
		pDoc->BeginWaitCursor();
		const bool opened = pDoc->OnOpenDocument(filename.ToUnicode().c_str());
		pDoc->EndWaitCursor();
		if(!opened)
		{
			pDoc->OnCloseDocument();
			pDoc = nullptr;
		} else
		{
			pDoc->SetPathName(filename.ToUnicode().c_str(), addToMru);
		}
	}
	if(pDoc)
		InitialUpdateFrame(pFrame, pDoc, makeVisible);

	if(pDoc)
	{
		CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
		if (pMainFrm) pMainFrm->OnDocumentCreated(pDoc);
	} else if(!filename.empty() && CMainFrame::GetMainFrame() && addToMru)
	{
		// Opening the document failed
		CMainFrame::GetMainFrame()->UpdateMRUList();
	}
	return pDoc;
}


CModDoc *CModDocTemplate::OpenTemplateFile(const mpt::PathString &filename, bool isExampleTune)
{
	CModDoc *doc = OpenDocumentFile(filename, isExampleTune ? true : false, true);
	if(doc)
	{
		CModDoc *modDoc = doc;
		// Clear path so that saving will not take place in templates/examples folder.
		modDoc->ClearFilePath();
		if(!isExampleTune)
		{
			SetDefaultTitle(modDoc);
			m_nUntitledCount++;
			// Name has changed...
			CMainFrame::GetMainFrame()->UpdateTree(modDoc, GeneralHint().General());

			// Reset edit history for template files
			CSoundFile &sndFile = modDoc->GetSoundFile();
			sndFile.GetFileHistory().clear();
			sndFile.m_dwCreatedWithVersion = Version::Current();
			sndFile.m_dwLastSavedWithVersion = Version();
			sndFile.m_modFormat = ModFormatDetails();
			sndFile.m_songArtist = TrackerSettings::Instance().defaultArtist;
			if(sndFile.GetType() != MOD_TYPE_MPT)
			{
				// Always enforce most compatible playback for legacy module types
				sndFile.m_playBehaviour = sndFile.GetDefaultPlaybackBehaviour(sndFile.GetType());
			}
			doc->UpdateAllViews(nullptr, UpdateHint().ModType());
		} else
		{
			// Remove extension from title, so that saving the file will not suggest a filename like e.g. "example.it.it".
			const mpt::ustring title = modDoc->GetTitle();
			const auto dotPos = title.rfind(u8'.');
			if(dotPos != mpt::ustring::npos)
			{
				modDoc->SetTitle(title.substr(0, dotPos));
			}
		}
	}
	return doc;
}


void CModDocTemplate::AddDocument(CModDoc *doc)
{
	doc->SetDocTemplate(this);
	m_documents.insert(doc);
	m_documentList.push_back(doc);
	// At this point we don't know yet if opening the document is going to be successful, so don't call UpdateDocumentCount()
}


void CModDocTemplate::RemoveDocument(CModDoc *doc)
{
	if(doc->GetDocTemplate() == this)
		doc->SetDocTemplate(nullptr);
	m_documents.erase(doc);
	m_documentList.erase(std::remove(m_documentList.begin(), m_documentList.end(), doc), m_documentList.end());
	if(CMainFrame::GetMainFrame())
		CMainFrame::GetMainFrame()->UpdateDocumentCount();
}


bool CModDocTemplate::DocumentExists(const CModDoc *doc) const
{
	return m_documents.count(const_cast<CModDoc *>(doc)) != 0;
}


// mfcless: CMultiDocTemplate's part

void CModDocTemplate::SetDefaultTitle(CModDoc *doc)
{
	mpt::ustring title = U_("Untitled");
	if(m_nUntitledCount != 0)
		title += mpt::ufmt::val(m_nUntitledCount + 1);
	doc->SetTitle(title);
}


CChildFrame *CModDocTemplate::CreateNewFrame(CModDoc *doc)
{
	CMainFrame *mainFrm = CMainFrame::GetMainFrame();
	if(!mainFrm || !mainFrm->GetMDIClient())
		return nullptr;
	auto *frame = new CChildFrame();
	QMdiArea *area = mainFrm->GetMDIClient();
	// Windows gives a new MDI child (CW_USEDEFAULT) about three quarters of
	// the client area, each one a title bar below and right of the last.
	// While the program starts, the client area gets its size only once the
	// main window is laid out.
	const int count = static_cast<int>(area->subWindowList().size());
	area->addSubWindow(frame);
	// As with MFC, the frame stays hidden while the document loads (dialogs
	// such as the missing samples list come up then), until InitialUpdateFrame.
	frame->hide();
	auto placeFrame = [area, frame, count]
	{
		if(frame->isMaximized() || frame->isMinimized())
			return;
		const QRect client = area->viewport()->rect();
		const int step = std::max(16, frame->style()->pixelMetric(QStyle::PM_TitleBarHeight, nullptr, frame) + 4);
		const QSize size(std::max(client.width() * 3 / 4, 200), std::max(client.height() * 3 / 4, 150));
		const int room = std::max(1, std::min(client.width() - size.width(), client.height() - size.height()) / step + 1);
		frame->setGeometry((count % room) * step, (count % room) * step, size.width(), size.height());
	};
	placeFrame();
	if(!area->isVisible())
		QTimer::singleShot(0, frame, placeFrame);
	if(!frame->OnCreateClient(doc, m_pViewClass))
	{
		delete frame;
		return nullptr;
	}
	return frame;
}


void CModDocTemplate::InitialUpdateFrame(CChildFrame *frame, CModDoc *doc, bool makeVisible)
{
	if(!frame)
		return;
	if(makeVisible)
		frame->ActivateFrame();
	frame->OnUpdateFrameTitle(true);
	if(doc)
		doc->UpdateAllViews(nullptr, UpdateHint().ModType());
}


void CModDocTemplate::CloseAllDocuments(bool)
{
	// A copy: closing a document removes it from the set.
	const std::vector<CModDoc *> docs = m_documentList;
	for(CModDoc *doc : docs)
	{
		if(DocumentExists(doc))
			doc->OnCloseDocument();
	}
}


bool CModDocTemplate::SaveAllModified()
{
	const std::vector<CModDoc *> docs = m_documentList;
	for(CModDoc *doc : docs)
	{
		if(DocumentExists(doc) && !doc->SaveModified())
			return false;
	}
	return true;
}


CModDoc *CModDocManager::OpenDocumentFile(const mpt::PathString &filename, bool bAddToMRU)
{
	if(mpt::native_fs{}.is_directory(filename))
	{
		FolderScanner scanner(filename, FolderScanner::kOnlyFiles | FolderScanner::kFindInSubDirectories);
		mpt::PathString file;
		CModDoc *pDoc = nullptr;
		while(scanner.Next(file))
		{
			pDoc = OpenDocumentFile(file, bAddToMRU);
		}
		return pDoc;
	}

#if MPT_OS_WINDOWS
	if(const auto fileExt = filename.GetFilenameExtension(); !mpt::PathCompareNoCase(fileExt, P_(".dll")) || !mpt::PathCompareNoCase(fileExt, P_(".vst3")))
#else
	if(filename.GetFilenameExtension() == P_(".so") || !mpt::PathCompareNoCase(filename.GetFilenameExtension(), P_(".dll")))
#endif
	{
		if(auto plugManager = theApp.GetPluginManager(); plugManager != nullptr)
		{
			bool isPlugin = false;
			for(VSTPluginLib *plugLib : plugManager->AddPlugin(filename, TrackerSettings::Instance().BrokenPluginsWorkaroundVSTMaskAllCrashes))
			{
				isPlugin = true;
				if(!CSelectPluginDlg::VerifyPlugin(plugLib, nullptr))
					plugManager->RemovePlugin(plugLib);
			}
			if(isPlugin)
				return nullptr;
		}
	}

	// mfcless: CDocManager::OpenDocumentFile; an open document is activated
	// instead of opened again.
	CModDoc *pDoc = nullptr;
	if(m_template)
	{
		if(!filename.empty())
		{
			for(CModDoc *doc : m_template->GetDocumentList())
			{
				if(!doc->GetPathNameMpt().empty() && !mpt::PathCompareNoCase(doc->GetPathNameMpt(), filename))
				{
					mfcless::ViewPosition pos = doc->GetFirstViewPosition();
					if(mfcless::DocumentView *view = doc->GetNextView(pos); view && view->GetParentFrame())
						view->GetParentFrame()->MDIActivate();
					return doc;
				}
			}
		}
		pDoc = m_template->OpenDocumentFile(filename, bAddToMRU);
	}
	if(pDoc == nullptr && !filename.empty())
	{
		if(!mpt::native_fs{}.is_file(filename))
		{
			Reporting::Error(MPT_UFORMAT("Unable to open \"{}\": file does not exist.")(filename.ToUnicode()));
			theApp.RemoveMruItem(filename);
			CMainFrame::GetMainFrame()->UpdateMRUList();
		} else
		{
			// Case: Valid path but opening failed.
			const int numDocs = theApp.GetOpenDocumentCount();
			Reporting::Notification(MPT_UFORMAT("Opening \"{}\" failed. This can happen if "
				"no more modules can be opened or if the file type was not "
				"recognised (currently there {} {} document{} open).")(
					filename.ToUnicode(), (numDocs == 1) ? mpt::ustring(U_("is")) : mpt::ustring(U_("are")), numDocs, (numDocs == 1) ? mpt::ustring(U_("")) : mpt::ustring(U_("s"))));
		}
	}
	return pDoc;
}


bool CModDocManager::OnDDECommand(const mpt::ustring &command)
{
	bool bResult, bActivate;
#ifdef DDEDEBUG
	MPT_LOG_GLOBAL(LogDebug, "DDE", U_("OnDDECommand: ") + command);
#endif
	// Handle any DDE commands recognized by your application
	// and return true.  See implementation of CWinApp::OnDDEComand
	// for example of parsing the DDE command string.
	bResult = false;
	bActivate = false;
	CMainFrame *mainFrm = CMainFrame::GetMainFrame();
	if(!command.empty() && mainFrm)
	{
		// mfcless: the same parsing as upstream's, on a string:
		// [Cmd(data)] with brackets, quotes and spaces trimmed.
		const auto isTrim = [](char8_t c) { return mpt::ustring(U_("(){}[]\'\" ")).find(c) != mpt::ustring::npos; };
		mpt::ustring s = command;
		while(s.size() > 1 && isTrim(s.back()))
			s.pop_back();
		std::size_t start = 0;
		while(start < s.size() && s[start] == u8'[')
			start++;
		std::size_t dataPos = start;
		while(dataPos < s.size() && s[dataPos] != u8'(')
			dataPos++;
		mpt::ustring cmd;
		for(std::size_t i = start; i < dataPos && static_cast<uint8>(s[i]) > static_cast<uint8>(' '); i++)
			cmd.push_back(s[i]);
		while(dataPos < s.size() && isTrim(s[dataPos]))
			dataPos++;
		const mpt::ustring data = s.substr(dataPos);
		const QString qcmd = mfcless::ToQString(cmd);
		// Edit/Open
		if(!qcmd.compare(QStringLiteral("Edit"), Qt::CaseInsensitive)
		 || !qcmd.compare(QStringLiteral("Open"), Qt::CaseInsensitive))
		{
			if(!data.empty())
			{
				bResult = true;
				bActivate = true;
				OpenDocumentFile(mpt::PathString::FromUnicode(data));
			}
		} else
		// New
		if(!qcmd.compare(QStringLiteral("New"), Qt::CaseInsensitive))
		{
			OpenDocumentFile(mpt::PathString());
			bResult = true;
			bActivate = true;
		}
	#ifdef DDEDEBUG
		MPT_LOG_GLOBAL(LogDebug, "DDE", MPT_UFORMAT("{}({})")(cmd, data));
	#endif
		if(bActivate)
		{
			if(mainFrm->IsIconic()) mainFrm->ShowWindow(mfcless::show::Restore);
			mainFrm->raise();
			mainFrm->activateWindow();
		}
	}
	// Return false for any DDE commands you do not handle.
#ifdef DDEDEBUG
	if (!bResult)
	{
		MPT_LOG_GLOBAL(LogDebug, "DDE", U_("WARNING: failure in CModDocManager::OnDDECommand()"));
	}
#endif
	return bResult;
}


OPENMPT_NAMESPACE_END
