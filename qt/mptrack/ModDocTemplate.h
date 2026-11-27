/*
 * ModDocTemplate.h
 * ----------------
 * Purpose: CDocTemplate and CModDocManager specializations for CModDoc.
 * Notes  : mfcless twin of upstream's mptrack/ModDocTemplate.h.
 *          Draft from qt/tools/twin.py, not yet edited by hand.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "DocumentView.h"
#include <QtCore/QObject>

#include "mpt/base/namespace.hpp"

#include "../common/mptPathString.h"

#include <unordered_set>

OPENMPT_NAMESPACE_BEGIN

class CModDoc;

class CModDocTemplate: public QObject
{
	Q_OBJECT
	std::unordered_set<CModDoc *> m_documents;	// Allow faster lookup of open documents than MFC's linear search allows for

public:
	CModDocTemplate(uint32 nIDResource, const mfcless::ViewClass * pDocClass, const mfcless::ViewClass * pFrameClass, const mfcless::ViewClass * pViewClass):
		CMultiDocTemplate(nIDResource, pDocClass, pFrameClass, pViewClass) {}

	CDocument* OpenTemplateFile(const mpt::PathString &filename, bool isExampleTune = false);

	CDocument* OpenDocumentFile(const mpt::uchar * lpszPathName, bool addToMru = true, bool makeVisible = true) override;

	void AddDocument(CDocument *doc) override;
	void RemoveDocument(CDocument *doc) override;
	bool DocumentExists(const CModDoc *doc) const;

	size_t size() const { return m_documents.size(); }
	bool empty() const { return m_documents.empty(); }
	auto begin() { return m_documents.begin(); }
	auto begin() const { return m_documents.begin(); }
	auto cbegin() const { return m_documents.cbegin(); }
	auto end() { return m_documents.end(); }
	auto end() const { return m_documents.end(); }
	auto cend() const { return m_documents.cend(); }
};


class CModDocManager : public QObject
{
	Q_OBJECT
public:
	CDocument *OpenDocumentFile(const mpt::uchar * lpszFileName, bool bAddToMRU = true) override;
	bool OnDDECommand(LPTSTR lpszCommand) override;
};


OPENMPT_NAMESPACE_END
