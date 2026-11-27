/*
 * AutoSaver.h
 * -----------
 * Purpose: Class for automatically saving open modules at a specified interval.
 * Notes  : mfcless twin of upstream's mptrack/AutoSaver.h.
 *          Draft from qt/tools/twin.py, not yet edited by hand.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"

#include "../common/mptTime.h"

OPENMPT_NAMESPACE_BEGIN

class CModDoc;

class CAutoSaver
{
public:
	CAutoSaver();
	
	bool DoSave();

	bool IsEnabled() const;
	bool GetUseOriginalPath() const;
	mpt::PathString GetPath() const;
	uint32 GetHistoryDepth() const;
	std::chrono::minutes GetSaveInterval() const;
	mpt::chrono::days GetRetentionTime() const;

private:
	bool SaveSingleFile(CModDoc &modDoc);
	mpt::PathString GetBasePath(const CModDoc &modDoc, bool createPath) const;
	mpt::PathString GetBaseName(const CModDoc &modDoc) const;
	mpt::PathString BuildFileName(const CModDoc &modDoc) const;
	void CleanUpAutosaves(const CModDoc &modDoc) const;
	void CleanUpAutosaves() const;
	bool CheckTimer(uint32 curTime) const;
	static void DeleteAutosave(const mpt::PathString &fileName, bool deletePermanently);
	
	uint32 m_lastSave = 0;
	// Flag to prevent autosave from starting new saving if previous is still in progress.
	bool m_saveInProgress = false;

};


OPENMPT_NAMESPACE_END
