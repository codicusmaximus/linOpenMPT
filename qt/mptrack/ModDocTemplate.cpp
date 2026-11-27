/*
 * ModDocTemplate.cpp
 * ------------------
 * Purpose: CDocTemplate and CModDocManager specialization for CModDoc.
 * Notes  : mfcless twin of upstream's mptrack/ModDocTemplate.cpp.
 *          Draft from qt/tools/twin.py, not yet edited by hand.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "ModDocTemplate.h"
#include "FolderScanner.h"
#include "Mainfrm.h"
#include "Moddoc.h"
#include "Mptrack.h"
#include "Reporting.h"
#include "SelectPluginDialog.h"
#include "TrackerSettings.h"
#include "../soundlib/plugins/PluginManager.h"

OPENMPT_NAMESPACE_BEGIN


#ifdef MPT_ALL_LOGGING
#define DDEDEBUG
#endif


CDocument *CModDocTemplate::OpenDocumentFile(const mpt::uchar * lpszPathName, bool addToMru, bool makeVisible)
{
	const mpt::PathString filename = (lpszPathName ? mpt::PathString::FromCString(lpszPathName) : mpt::PathString());

	// First, remove document from MRU list.
	if(addToMru)
	{
		theApp.RemoveMruItem(filename);
	}

	CDocument *pDoc = QObject::OpenDocumentFile(filename.empty() ? nullptr : filename.ToCString().GetString(), addToMru, makeVisible);
	if(pDoc)
	{
		CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
		if (pMainFrm) pMainFrm->OnDocumentCreated(static_cast<CModDoc *>(pDoc));
	} else if(!filename.empty() && CMainFrame::GetMainFrame() && addToMru)
	{
		// Opening the document failed
		CMainFrame::GetMainFrame()->UpdateMRUList();
	}
	return pDoc;
}


CDocument *CModDocTemplate::OpenTemplateFile(const mpt::PathString &filename, bool isExampleTune)
{
	CDocument *doc = OpenDocumentFile(filename.ToCString(), isExampleTune ? true : false, true);
	if(doc)
	{
		CModDoc *modDoc = static_cast<CModDoc *>(doc);
		// Clear path so that saving will not take place in templates/examples folder.
		modDoc->ClearFilePath();
		if(!isExampleTune)
		{
			QObject::SetDefaultTitle(modDoc);
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
			doc->UpdateAllViews(nullptr, UpdateHint().ModType().AsLPARAM());
		} else
		{
			// Remove extension from title, so that saving the file will not suggest a filename like e.g. "example.it.it".
			const mpt::ustring title = modDoc->GetTitle();
			const int dotPos = title.ReverseFind(_T('.'));
			if(dotPos >= 0)
			{
				modDoc->SetTitle(title.Left(dotPos));
			}
		}
	}
	return doc;
}


void CModDocTemplate::AddDocument(CDocument *doc)
{
	QObject::AddDocument(doc);
	m_documents.insert(static_cast<CModDoc *>(doc));
	// At this point we don't know yet if opening the document is going to be successful, so don't call UpdateDocumentCount()
}


void CModDocTemplate::RemoveDocument(CDocument *doc)
{
	QObject::RemoveDocument(doc);
	m_documents.erase(static_cast<CModDoc *>(doc));
	CMainFrame::GetMainFrame()->UpdateDocumentCount();
}


bool CModDocTemplate::DocumentExists(const CModDoc *doc) const
{
	return m_documents.count(const_cast<CModDoc *>(doc)) != 0;
}


CDocument *CModDocManager::OpenDocumentFile(const mpt::uchar * lpszFileName, bool bAddToMRU)
{
	const mpt::PathString filename = (lpszFileName ? mpt::PathString::FromCString(lpszFileName) : mpt::PathString());

	if(mpt::native_fs{}.is_directory(filename))
	{
		FolderScanner scanner(filename, FolderScanner::kOnlyFiles | FolderScanner::kFindInSubDirectories);
		mpt::PathString file;
		CDocument *pDoc = nullptr;
		while(scanner.Next(file))
		{
			pDoc = OpenDocumentFile(file.ToCString(), bAddToMRU);
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

	CDocument *pDoc = QObject::OpenDocumentFile(lpszFileName, bAddToMRU);
	if(pDoc == nullptr && !filename.empty())
	{
		if(!mpt::native_fs{}.is_file(filename))
		{
			Reporting::Error(MPT_UFORMAT("Unable to open \"{}\": file does not exist.")(filename.ToCString()));
			theApp.RemoveMruItem(filename);
			CMainFrame::GetMainFrame()->UpdateMRUList();
		} else
		{
			// Case: Valid path but opening failed.
			const int numDocs = theApp.GetOpenDocumentCount();
			Reporting::Notification(MPT_UFORMAT("Opening \"{}\" failed. This can happen if "
				"no more modules can be opened or if the file type was not "
				"recognised (currently there {} {} document{} open).")(
					filename.ToCString(), (numDocs == 1) ? mpt::ustring(U_("is")) : mpt::ustring(U_("are")), numDocs, (numDocs == 1) ? mpt::ustring(U_("")) : mpt::ustring(U_("s"))));
		}
	}
	return pDoc;
}


bool CModDocManager::OnDDECommand(LPTSTR lpszCommand)
{
	bool bResult, bActivate;
#ifdef DDEDEBUG
	MPT_LOG_GLOBAL(LogDebug, "DDE", U_("OnDDECommand: ") + mpt::ToUnicode(mpt::ustring(lpszCommand)));
#endif
	// Handle any DDE commands recognized by your application
	// and return true.  See implementation of CWinApp::OnDDEComand
	// for example of parsing the DDE command string.
	bResult = false;
	bActivate = false;
	if ((lpszCommand) && lpszCommand[0] && (theApp.m_pMainWnd))
	{
		std::size_t len = _tcslen(lpszCommand);
		std::vector<mpt::uchar> s(lpszCommand, lpszCommand + len + 1);

		len--;
		while((len > 0) && _tcschr(U_("(){}[]\'\" "), s[len]))
		{
			s[len--] = 0;
		}
		mpt::uchar *pszCmd = s.data();
		while (pszCmd[0] == _T('[')) pszCmd++;
		mpt::uchar *pszData = pszCmd;
		while ((pszData[0] != _T('(')) && (pszData[0]))
		{
			if (((uint8)pszData[0]) <= (uint8)' ') *pszData = 0;
			pszData++;
		}
		while ((*pszData) && (_tcschr(U_("(){}[]\'\" "), *pszData)))
		{
			*pszData = 0;
			pszData++;
		}
		// Edit/Open
		if ((!lstrcmpi(pszCmd, U_("Edit")))
		 || (!lstrcmpi(pszCmd, U_("Open"))))
		{
			if (pszData[0])
			{
				bResult = true;
				bActivate = true;
				OpenDocumentFile(pszData);
			}
		} else
		// New
		if (!lstrcmpi(pszCmd, U_("New")))
		{
			OpenDocumentFile(U_(""));
			bResult = true;
			bActivate = true;
		}
	#ifdef DDEDEBUG
		MPT_LOG_GLOBAL(LogDebug, "DDE", MPT_UFORMAT("{}({})")(mpt::ustring(pszCmd), mpt::ustring(pszData)));
	#endif
		if ((bActivate) && (theApp.m_pMainWnd->m_hWnd))
		{
			if (theApp.m_pMainWnd->IsIconic()) theApp.m_pMainWnd->ShowWindow(SW_RESTORE);
			theApp.m_pMainWnd->SetActiveWindow();
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
