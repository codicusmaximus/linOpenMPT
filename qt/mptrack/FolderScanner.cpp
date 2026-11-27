/*
 * FolderScanner.cpp
 * -----------------
 * Purpose: Class for finding files in folders.
 * Notes  : mfcless twin of upstream's mptrack/FolderScanner.cpp, on
 *          QDirIterator. As on Windows, "*.*" matches every name and the
 *          filter ignores case.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "stdafx.h"
#include "FolderScanner.h"

#include "mfcless/QtString.h"

#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QDirIterator>
#include <QtCore/QFileInfo>

OPENMPT_NAMESPACE_BEGIN

FolderScanner::FolderScanner(mpt::PathString path, FlagSet<ScanType> type, mpt::PathString filter)
	: m_paths{1, std::move(path)}
	, m_filter{std::move(filter)}
	, m_type{type}
{
}


FolderScanner::~FolderScanner()
{
}


bool FolderScanner::Next(mpt::PathString &file, FileInfo *fileInfo)
{
	bool found = false;
	do
	{
		if(!m_hFind)
		{
			if(m_paths.empty())
				return false;

			m_currentPath = m_paths.back().WithTrailingSlash();
			m_paths.pop_back();
			QString filter = mfcless::ToQString(m_filter.ToUnicode());
			if(filter == QLatin1String("*.*"))
				filter = QStringLiteral("*");
			QDir dir(mfcless::ToQString(m_currentPath.ToUnicode()));
			dir.setNameFilters({filter});
			dir.setFilter(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot);
			m_hFind = std::make_unique<QDirIterator>(dir);
		}

		while(m_hFind->hasNext() && !found)
		{
			m_hFind->next();
			const QFileInfo info = m_hFind->fileInfo();
			file = m_currentPath + mpt::PathString::FromUnicode(mfcless::FromQString(info.fileName()));
			if(info.isDir())
			{
				// Add sub directory
				if(m_type[kFindInSubDirectories])
					m_paths.push_back(file);
				if(m_type[kOnlyDirectories])
					found = true;
			} else if(m_type[kOnlyFiles])
			{
				found = true;
			}
			if(found && fileInfo)
			{
				fileInfo->isDirectory = info.isDir();
				fileInfo->size = static_cast<uint64>(info.size());
				// FILETIME: 100 ns since 1601-01-01
				fileInfo->lastWriteTime = (info.lastModified().toMSecsSinceEpoch() + int64(11644473600) * 1000) * 10000;
			}
		}
		if(!found)
		{
			// Done with this directory, advance to next
			m_hFind.reset();
		}
	} while(!found);
	return true;
}

OPENMPT_NAMESPACE_END
