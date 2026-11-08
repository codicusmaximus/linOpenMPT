// linOpenMPT platform layer: CModDoc members the engine calls.
//
// TEMPORARY. CModDoc (mptrack/Moddoc.cpp) is upstream's document class and is
// logic, not UI, so the plan is to compile upstream's Moddoc.cpp unchanged
// once compat/ covers what it needs. Until then the engine runs without a
// document (CSoundFile::GetpModDoc() is nullptr) and these are never reached
// in practice; they exist so the engine links.

#include "stdafx.h"
#include "Moddoc.h"
#include "UpdateHints.h"

#include <iostream>

OPENMPT_NAMESPACE_BEGIN

void CModDoc::SetModified(bool modified) { m_bModified = modified; }
void CModDoc::UpdateAllViews(CView *, UpdateHint, CObject *) { }
void CModDoc::UpdateAllViews(UpdateHint) { }
void CModDoc::SendNotifyMessageToAllViews(UINT, WPARAM, LPARAM) { }
void CModDoc::RecordParamChange(PLUGINDEX, PlugParamIndex) { }
void CModDoc::LearnMacro(int, PlugParamIndex) { }
bool CModDoc::MuteChannel(CHANNELINDEX, bool) { return false; }
bool CModDoc::UpdateChannelMuteStatus(CHANNELINDEX) { return false; }
bool CModDoc::IsChannelMuted(CHANNELINDEX) const { return false; }
bool CModDoc::IsChannelUnused(CHANNELINDEX) const { return false; }
CHANNELINDEX CModDoc::ReArrangeChannels(const std::vector<CHANNELINDEX> &, const bool) { return 0; }
void CModDoc::AddToLog(LogLevel, const mpt::ustring &text) const
{
	std::cerr << "[log] " << mpt::ToCharset(mpt::Charset::UTF8, text) << std::endl;
}

OPENMPT_NAMESPACE_END
