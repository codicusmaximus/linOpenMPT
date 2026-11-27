/*
 * AutoSaver.cpp
 * -------------
 * Purpose: Class for automatically saving open modules at a specified interval.
 * Notes  : mfcless twin of upstream's mptrack/AutoSaver.cpp.
 *          Hand-edited from the twin.py draft.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "AutoSaver.h"
#include "mfcless/Widgets.h"
#include "FileDialog.h"
#include "FolderScanner.h"
#include "Mainfrm.h"
#include "Moddoc.h"
#include "Mptrack.h"
#include "Reporting.h"
#include "resource.h"
#include "TrackerSettings.h"
#include "../soundlib/mod_specifications.h"
#include "mpt/fs/fs.hpp"

#include <QtCore/QDateTime>
#include <QtCore/QFile>

#include <algorithm>
#include <filesystem>


OPENMPT_NAMESPACE_BEGIN


CAutoSaver::CAutoSaver()
	: m_lastSave{mfcless::TickCount()}
{
}


bool CAutoSaver::IsEnabled() const
{
	return TrackerSettings::Instance().AutosaveEnabled;
}

bool CAutoSaver::GetUseOriginalPath() const
{
	return TrackerSettings::Instance().AutosaveUseOriginalPath;
}

mpt::PathString CAutoSaver::GetPath() const
{
	return TrackerSettings::Instance().AutosavePath.GetDefaultDir();
}

uint32 CAutoSaver::GetHistoryDepth() const
{
	return TrackerSettings::Instance().AutosaveHistoryDepth;
}

std::chrono::minutes CAutoSaver::GetSaveInterval() const
{
	return std::chrono::minutes{TrackerSettings::Instance().AutosaveIntervalMinutes.Get()};
}


mpt::chrono::days CAutoSaver::GetRetentionTime() const
{
	return mpt::chrono::days{TrackerSettings::Instance().AutosaveRetentionTimeDays.Get()};
}


bool CAutoSaver::DoSave()
{
	// Do nothing if we are already saving, or if time to save has not been reached yet.
	if(m_saveInProgress || !CheckTimer(mfcless::TickCount()))
		return true;
		
	bool success = true, clearStatus = false;
	m_saveInProgress = true;

	mfcless::BeginWaitCursor();  // Display hour glass

	for(auto &modDoc : theApp.GetOpenDocuments())
	{
		if(modDoc->ModifiedSinceLastAutosave())
		{
			clearStatus = true;
			CMainFrame::GetMainFrame()->SetHelpText(MPT_UFORMAT("Auto-saving {}...")(modDoc->GetPathNameMpt().GetFilename()));
			if(SaveSingleFile(*modDoc))
			{
				CleanUpAutosaves(*modDoc);
			} else
			{
				TrackerSettings::Instance().AutosaveEnabled = false;
				Reporting::Warning("Warning: Auto Save failed and has been disabled. Please:\n- Review your Auto Save paths\n- Check available disk space and filesystem access rights");
				success = false;
			}
		}
	}
	CleanUpAutosaves();

	m_lastSave = mfcless::TickCount();
	mfcless::EndWaitCursor();  // End display hour glass
	m_saveInProgress = false;

	if(clearStatus)
		CMainFrame::GetMainFrame()->SetHelpText(UL_(""));
	
	return success;
}


bool CAutoSaver::CheckTimer(uint32 curTime) const
{
	return std::chrono::milliseconds{curTime - m_lastSave} >= GetSaveInterval();
}


mpt::PathString CAutoSaver::GetBasePath(const CModDoc &modDoc, bool createPath) const
{
	mpt::PathString path;
	if(GetUseOriginalPath())
	{
		if(modDoc.m_bHasValidPath && !(path = modDoc.GetPathNameMpt()).empty())
		{
			// File has a user-chosen path - remove filename
			path = path.GetDirectoryWithDrive();
		} else
		{
			// If it doesn't, fall back to default
			path = TrackerSettings::GetDefaultAutosavePath();
		}
	} else
	{
		path = GetPath();
	}
	std::error_code ec;
	if(createPath)
		std::filesystem::create_directories(mpt::support_long_path(path.AsNative()).c_str(), ec);
	if(!mpt::native_fs{}.is_directory(path))
		path = theApp.GetConfigPath();

	return path.WithTrailingSlash();
}


mpt::PathString CAutoSaver::GetBaseName(const CModDoc &modDoc) const
{
	return mpt::PathString::FromUnicode(modDoc.GetTitle()).AsSanitizedComponent();
}


mpt::PathString CAutoSaver::BuildFileName(const CModDoc &modDoc) const
{
	mpt::PathString name = GetBasePath(modDoc, true) + GetBaseName(modDoc);
	const mpt::ustring timeStamp = mfcless::FromQString(QDateTime::currentDateTime().toString(QStringLiteral(".'AutoSave'.yyyyMMdd.HHmmss.")));
	name += mpt::PathString::FromUnicode(timeStamp);  // Append backtup tag + timestamp
	name += mpt::PathString::FromUnicode(modDoc.GetSoundFile().GetModSpecifications().GetFileExtension());
	return name;
}


bool CAutoSaver::SaveSingleFile(CModDoc &modDoc)
{
	mpt::PathString fileName = BuildFileName(modDoc);

	// We are actually not going to show the log for autosaved files.
	ScopedLogCapturer logcapturer(modDoc, UL_(""), nullptr, false);
	return modDoc.SaveFile(fileName, GetUseOriginalPath());
}


void CAutoSaver::CleanUpAutosaves(const CModDoc &modDoc) const
{
	// Find all autosave files for this document, and delete the oldest ones if there are more than the user wants.
	std::vector<mpt::PathString> foundfiles;
	FolderScanner scanner(GetBasePath(modDoc, false), FolderScanner::kOnlyFiles, GetBaseName(modDoc) + P_(".AutoSave.*.*.*"));
	mpt::PathString fileName;
	while(scanner.Next(fileName))
	{
		foundfiles.push_back(std::move(fileName));
	}
	std::sort(foundfiles.begin(), foundfiles.end());
	size_t filesToDelete = std::max(static_cast<size_t>(GetHistoryDepth()), foundfiles.size()) - GetHistoryDepth();
	const bool deletePermanently = TrackerSettings::Instance().AutosaveDeletePermanently;
	for(size_t i = 0; i < filesToDelete; i++)
	{
		DeleteAutosave(foundfiles[i], deletePermanently);
	}
}


// Find all autosave files in the autosave folder and delete all of those older than X days
void CAutoSaver::CleanUpAutosaves() const
{
	if(GetUseOriginalPath() || !GetRetentionTime().count())
		return;
	auto path = GetPath();
	if(!mpt::native_fs{}.is_directory(path))
		return;
	const std::chrono::seconds maxAge = GetRetentionTime();
	const bool deletePermanently = TrackerSettings::Instance().AutosaveDeletePermanently;
	// In 100 ns since 1601, as FolderScanner's lastWriteTime (FILETIME)
	const int64 currentTime = static_cast<int64>(QDateTime::currentMSecsSinceEpoch()) * 10000 + int64(116444736000000000);
	FolderScanner scanner(std::move(path), FolderScanner::kOnlyFiles, P_("*.AutoSave.*.*.*"));
	mpt::PathString fileName;
	FolderScanner::FileInfo fileInfo;
	while(scanner.Next(fileName, &fileInfo))
	{
		const auto fileTime = fileInfo.lastWriteTime;
		const auto timeDiff = std::chrono::seconds{(currentTime - fileTime) / 10000000};
		if(timeDiff >= maxAge)
			DeleteAutosave(fileName, deletePermanently);
	}
}


void CAutoSaver::DeleteAutosave(const mpt::PathString &fileName, bool deletePermanently)
{
	// mfcless: to the trash (upstream: the recycle bin), or deleted when there is no trash
	const QString path = mfcless::ToQString(fileName.ToUnicode());
	if(deletePermanently || !QFile::moveToTrash(path))
		QFile::remove(path);
}

OPENMPT_NAMESPACE_END
