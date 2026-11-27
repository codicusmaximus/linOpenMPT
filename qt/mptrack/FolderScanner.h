/*
 * FolderScanner.h
 * ---------------
 * Purpose: Class for finding files in folders.
 * Notes  : mfcless twin of upstream's mptrack/FolderScanner.h: the files'
 *          details are a FileInfo instead of WIN32_FIND_DATA.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"

#include "../common/mptPathString.h"

#include <memory>

class QDirIterator;

OPENMPT_NAMESPACE_BEGIN

class FolderScanner
{
public:
	enum ScanType
	{
		kOnlyFiles = 0x01,
		kOnlyDirectories = 0x02,
		kFilesAndDirectories = kOnlyFiles | kOnlyDirectories,
		kFindInSubDirectories = 0x04,
	};

	// mfcless: what upstream reads from WIN32_FIND_DATA.
	struct FileInfo
	{
		bool isDirectory = false;
		uint64 size = 0;
		int64 lastWriteTime = 0;  // in 100 ns since 1601 (as FILETIME)
	};

	FolderScanner(mpt::PathString path, FlagSet<ScanType> type, mpt::PathString filter = P_("*.*"));
	~FolderScanner();

	FolderScanner(const FolderScanner&) = delete;
	FolderScanner &operator=(const FolderScanner&) = delete;

	// Return one file or directory at a time in parameter file. Returns true if a file was found (file parameter is valid), false if no more files can be found (file parameter is not touched).
	bool Next(mpt::PathString &file, FileInfo *fileInfo = nullptr);

protected:
	std::vector<mpt::PathString> m_paths;
	mpt::PathString m_currentPath;
	const mpt::PathString m_filter;
	std::unique_ptr<QDirIterator> m_hFind;
	const FlagSet<ScanType> m_type;
};

MPT_DECLARE_ENUM(FolderScanner::ScanType)

OPENMPT_NAMESPACE_END
