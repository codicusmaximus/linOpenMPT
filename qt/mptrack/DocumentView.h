/*
 * DocumentView.h
 * --------------
 * Purpose: mfcless: the document/view framework upstream gets from MFC:
 *          a document (CModDoc's base) with its path, title, modified flag
 *          and views, and what the views have in common: they belong to a
 *          document, are told when it changes, and take commands.
 * Notes  : Not a twin. The functions have the names upstream calls.
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */

#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "mfcless/Graphics.h"
#include "mfcless/MessageMap.h"

#include "mpt/string/types.hpp"

#include <QtCore/QObject>

#include <atomic>
#include <vector>

class QWidget;

OPENMPT_NAMESPACE_BEGIN
class CModDoc;
class CModDocTemplate;
class CChildFrame;
OPENMPT_NAMESPACE_END

namespace mfcless
{

using CModDoc = OPENMPT_NAMESPACE::CModDoc;
using CModDocTemplate = OPENMPT_NAMESPACE::CModDocTemplate;

class DocumentView;
class Document;

// The class of a view, for creating one by class (upstream: CRuntimeClass).
struct ViewClass
{
	const char *m_lpszClassName;
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
	// Window messages to the view (upstream: CWnd's).
	void PostMessage(uint32_t message, uintptr_t wParam = 0, intptr_t lParam = 0);
	intptr_t SendMessage(uint32_t message, uintptr_t wParam = 0, intptr_t lParam = 0);
	void SendNotifyMessage(uint32_t message, uintptr_t wParam = 0, intptr_t lParam = 0);
	// The frame (MDI child window) the view is in, or nullptr.
	OPENMPT_NAMESPACE::CChildFrame *GetParentFrame();
	// The view's message map (its window's).
	MessageMap *GetMessageMap() { return dynamic_cast<MessageMap *>(this); }

	// The view's class (upstream: CObject::GetRuntimeClass).
	virtual const ViewClass *GetRuntimeClass() const { return nullptr; }

	// Called once the view has its document and frame.
	virtual void OnInitialUpdate() {}
	// Draws the view (from its OnPaint, on a PaintDC).
	virtual void OnDraw(mfcless::DC * /*pDC*/) {}

protected:
	friend class Document;
	CModDoc *m_document = nullptr;
};


// Where GetNextView is in a document's view list; false at the end.
struct ViewPosition
{
	std::size_t index = 0;
	bool valid = false;
	explicit operator bool() const { return valid; }
};

class Document : public QObject, public MessageMap
{
	Q_OBJECT
public:
	~Document() override;

	// A new, empty document: DeleteContents, not modified.
	virtual bool OnNewDocument();
	virtual bool OnOpenDocument(const ::mpt::uchar *pathName);
	virtual bool OnSaveDocument(const ::mpt::uchar *pathName);
	// Closes the document's frames and deletes the document.
	virtual void OnCloseDocument();
	// Asks whether to save a modified document. False if the user cancels.
	virtual bool SaveModified();
	// Numbers the document's frames (":1", ":2") when it has more than one,
	// and updates their titles.
	void UpdateFrameCounts();
	// The File > Close command: asks to save, then closes.
	void OnFileClose();
	// File > Save and File > Save As (CDocument's own commands).
	void OnFileSave() { DoFileSave(); }
	void OnFileSaveAs() { DoSave(nullptr); }
	// Saves to pathName, or asks for a name (saveAs, or no path yet).
	virtual bool DoSave(const ::mpt::uchar *pathName, bool saveAs = true);
	bool DoFileSave();
	virtual void DeleteContents() {}

protected:
	void RegisterMessageMap() override;
public:

	virtual void SetPathName(const ::mpt::uchar *pathName, bool addToMRU = true);
	const ::mpt::ustring &GetPathName() const { return m_strPathName; }
	virtual void SetTitle(const ::mpt::ustring &title);
	const ::mpt::ustring &GetTitle() const { return m_strTitle; }
	bool IsModified() const { return m_bModified; }
	void SetModifiedFlag(bool modified = true) { m_bModified = modified; }

	CModDocTemplate *GetDocTemplate() const { return m_pDocTemplate; }
	void SetDocTemplate(CModDocTemplate *docTemplate) { m_pDocTemplate = docTemplate; }

	// The views (upstream: GetFirstViewPosition and GetNextView).
	void AddView(DocumentView *view);
	void RemoveView(DocumentView *view);
	ViewPosition GetFirstViewPosition() const { return {0, !m_views.empty()}; }
	DocumentView *GetNextView(ViewPosition &pos) const;
	const std::vector<DocumentView *> &GetViews() const { return m_views; }
	// Calls OnUpdate of every view but sender.
	void UpdateAllViews(DocumentView *sender, intptr_t hint = 0, QObject *hintObject = nullptr);

	// The busy cursor while something takes long.
	void BeginWaitCursor();
	void EndWaitCursor();

protected:
	::mpt::ustring m_strPathName;
	::mpt::ustring m_strTitle;
	std::atomic<bool> m_bModified = false;  // set from the audio thread too
	std::vector<DocumentView *> m_views;
	CModDocTemplate *m_pDocTemplate = nullptr;
};

} // namespace mfcless
