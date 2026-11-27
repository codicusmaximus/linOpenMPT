/*
 * DocumentView.h
 * --------------
 * Purpose: mfcless: what the tracker's views have in common: they belong
 *          to a document, are told when it changes, and take commands.
 * Notes  : Not a twin; it takes the place of the document/view framework
 *          upstream's views get from their base classes. CModDoc keeps a
 *          list of its views and calls OnUpdate on them from UpdateAllViews.
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */

#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "mfcless/MessageMap.h"

class QObject;
class QWidget;

OPENMPT_NAMESPACE_BEGIN
class CModDoc;
OPENMPT_NAMESPACE_END

namespace mfcless
{

using CModDoc = OPENMPT_NAMESPACE::CModDoc;

class DocumentView;

// The class of a view, for creating one by class (upstream: CRuntimeClass).
struct ViewClass
{
	const char *name;  // upstream's m_lpszClassName
	DocumentView *(*create)(QWidget *parent);
};

class DocumentView
{
public:
	virtual ~DocumentView();

	CModDoc *GetDocument() const { return m_document; }
	// Joins the document's view list (and leaves the previous one's).
	void SetDocument(CModDoc *doc);

	// The document changed. hint is an UpdateHint as an integer; sender is the
	// view that made the change, or nullptr.
	virtual void OnUpdate(DocumentView * /*sender*/, intptr_t /*hint*/, QObject * /*hintObject*/) {}

	// The widget of the view.
	virtual QWidget *ViewWidget() = 0;
	// The view's message map (its window's).
	MessageMap *GetMessageMap() { return dynamic_cast<MessageMap *>(this); }

	// Called once the view has its document and frame.
	virtual void OnInitialUpdate() {}

protected:
	CModDoc *m_document = nullptr;
};

} // namespace mfcless
