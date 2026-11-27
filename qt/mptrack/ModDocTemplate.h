/*
 * ModDocTemplate.h
 * ----------------
 * Purpose: CDocTemplate and CModDocManager specializations for CModDoc.
 * Notes  : mfcless twin of upstream's mptrack/ModDocTemplate.h. What MFC's
 *          CMultiDocTemplate and CDocManager did (make a document with its
 *          frame and view, give new documents their titles, keep the
 *          documents) is done here, with the same function names.
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
class CChildFrame;

class CModDocTemplate: public QObject
{
	Q_OBJECT
	std::unordered_set<CModDoc *> m_documents;	// Allow faster lookup of open documents than MFC's linear search allows for

public:
	// mfcless: the resource ID names the frame's menu and icon; the view
	// class is the frame's first view.
	CModDocTemplate(uint32 nIDResource, const mfcless::ViewClass *pViewClass)
		: m_nIDResource(nIDResource), m_pViewClass(pViewClass) {}

	CModDoc *OpenTemplateFile(const mpt::PathString &filename, bool isExampleTune = false);

	// An empty filename makes a new document.
	CModDoc *OpenDocumentFile(const mpt::PathString &filename, bool addToMru = true, bool makeVisible = true);

	void AddDocument(CModDoc *doc);
	void RemoveDocument(CModDoc *doc);
	bool DocumentExists(const CModDoc *doc) const;

	size_t size() const { return m_documents.size(); }
	bool empty() const { return m_documents.empty(); }
	auto begin() { return m_documents.begin(); }
	auto begin() const { return m_documents.begin(); }
	auto cbegin() const { return m_documents.cbegin(); }
	auto end() { return m_documents.end(); }
	auto end() const { return m_documents.end(); }
	auto cend() const { return m_documents.cend(); }

	// mfcless: CMultiDocTemplate's part.
	// Titles new documents "Untitled", "Untitled2", ... as CMultiDocTemplate does.
	void SetDefaultTitle(CModDoc *doc);
	// Makes the frame (with its view) for a document.
	CChildFrame *CreateNewFrame(CModDoc *doc);
	void InitialUpdateFrame(CChildFrame *frame, CModDoc *doc, bool makeVisible = true);
	// Closes all documents (without asking to save them).
	void CloseAllDocuments(bool endSession);
	bool SaveAllModified();
	uint32 GetResourceID() const { return m_nIDResource; }

protected:
	uint32 m_nIDResource = 0;
	const mfcless::ViewClass *m_pViewClass = nullptr;
	uint32 m_nUntitledCount = 0;
};


class CModDocManager : public QObject
{
	Q_OBJECT
public:
	// mfcless: CDocManager kept the templates; OpenMPT has one.
	void AddDocTemplate(CModDocTemplate *pTemplate) { m_template = pTemplate; }
	CModDocTemplate *GetBestTemplate() const { return m_template; }

	CModDoc *OpenDocumentFile(const mpt::PathString &filename, bool bAddToMRU = true);
	// A command of another instance of OpenMPT ("[Open(file)]", "[New]"),
	// as upstream gets them by DDE.
	bool OnDDECommand(const mpt::ustring &command);

	bool SaveAllModified() { return !m_template || m_template->SaveAllModified(); }
	void CloseAllDocuments(bool endSession) { if(m_template) m_template->CloseAllDocuments(endSession); }
	int GetOpenDocumentCount() const { return m_template ? static_cast<int>(m_template->size()) : 0; }

protected:
	CModDocTemplate *m_template = nullptr;
};


OPENMPT_NAMESPACE_END
