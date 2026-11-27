/*
 * Childfrm.h
 * ----------
 * Purpose: Implementation of tab interface class.
 * Notes  : mfcless twin of upstream's mptrack/Childfrm.h. An MDI child
 *          window (QMdiSubWindow) with a splitter: the control view
 *          (tabs) on top and the editor view below.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "DocumentView.h"
#include "mfcless/Window.h"
#include "PatternCursor.h"
#include "../common/FileReaderFwd.h"
#include "../soundlib/plugins/PluginStructs.h"

#include "mfcless/MessageMap.h"

#include <QtWidgets/QMdiSubWindow>

#include <bitset>

class QSplitter;

OPENMPT_NAMESPACE_BEGIN

class CModControlView;
class CModControlDlg;
class CModDoc;

struct GeneralViewState
{
	PlugParamIndex nParam = 0;
	CHANNELINDEX nTab = 0;
	PLUGINDEX nPlugin = 0;
	bool initialized = false;

	std::string Serialize() const { return {}; }
	void Deserialize(FileReader &) { }
};


struct PatternViewState
{
	PATTERNINDEX nPattern = 0;
	PatternCursor cursor = 0;
	PatternRect selection;
	ORDERINDEX nOrder = 0;
	ORDERINDEX initialOrder = ORDERINDEX_INVALID;
	std::bitset<PatternCursor::numColumns> visibleColumns;
	bool initialized = false;

	std::string Serialize() const;
	void Deserialize(FileReader &f);
};

struct SampleViewState
{
	SmpLength dwScrollPos = 0;
	SmpLength dwBeginSel = 0;
	SmpLength dwEndSel = 0;
	SAMPLEINDEX nSample = 0;
	SAMPLEINDEX initialSample = 0;

	std::string Serialize() const;
	void Deserialize(FileReader &f);
};


struct InstrumentViewState
{
	float zoom = 4;
	EnvelopeType nEnv = ENV_VOLUME;
	INSTRUMENTINDEX initialInstrument = 0;
	INSTRUMENTINDEX instrument = 0;
	bool bGrid = false;
	bool initialized = false;

	std::string Serialize() const;
	void Deserialize(FileReader &f);
};

struct CommentsViewState
{
	uint32 nId = 0;
	bool initialized = false;

	std::string Serialize() const { return {}; }
	void Deserialize(FileReader &) {}
};



class CChildFrame: public mfcless::Window<QMdiSubWindow>
{
	Q_OBJECT
	friend class CModControlDlg;
public:
	CChildFrame();
	~CChildFrame() override;

protected:
	static CChildFrame *m_lastActiveFrame;
	static int glMdiOpenCount;

// Attributes
protected:
	QSplitter *m_wndSplitter = nullptr;
	QWidget *m_hWndCtrl = nullptr, *m_hWndView = nullptr;
	GeneralViewState m_ViewGeneral;
	PatternViewState m_ViewPatterns;
	SampleViewState m_ViewSamples;
	InstrumentViewState m_ViewInstruments;
	CommentsViewState m_ViewComments;
	std::string m_currentViewClassName;
	int m_dpi = 0;
	bool m_maxWhenClosed = false;
	bool m_initialActivation = true;
	// mfcless: what CFrameWnd kept.
	CModDoc *m_document = nullptr;
	mfcless::DocumentView *m_activeView = nullptr;
	int m_nWindow = 0;  // the number of this window of the document, if it has several

// Operations
public:
	CModControlView *GetModControlView() const;
	bool ChangeViewClass(const mfcless::ViewClass *pNewViewClass);
	void ForceRefresh();
	void SavePosition(bool exit = false);
	intptr_t SendCtrlMessage(uint32 uMsg, intptr_t lParam = 0) const;
	intptr_t SendViewMessage(uint32 uMsg, intptr_t lParam = 0) const;
	intptr_t ActivateView(uint32 nId, intptr_t lParam);
	QWidget *GetHwndCtrl() const { return m_hWndCtrl; }
	QWidget *GetHwndView() const { return m_hWndView; }
	GeneralViewState &GetGeneralViewState() { return m_ViewGeneral; }
	PatternViewState &GetPatternViewState() { return m_ViewPatterns; }
	SampleViewState &GetSampleViewState() { return m_ViewSamples; }
	InstrumentViewState &GetInstrumentViewState() { return m_ViewInstruments; }
	CommentsViewState &GetCommentViewState() { return m_ViewComments; }

	bool IsPatternView() const;

	void SetSplitterHeight(int x);
	int GetSplitterHeight();

	void SaveAllViewStates();
	std::string SerializeView();
	void DeserializeView(FileReader &file);

	void ToggleViews();

	static CChildFrame *LastActiveFrame() { return m_lastActiveFrame; }

	// mfcless: CFrameWnd's document and active view.
	CModDoc *GetActiveDocument() const { return m_document; }
	mfcless::DocumentView *GetActiveView() const { return m_activeView; }
	void SetActiveView(mfcless::DocumentView *view);
	void SetWindowNumber(int n) { m_nWindow = n; }

// Overrides
public:
	// Creates the views for the document (upstream: OnCreateClient).
	bool OnCreateClient(CModDoc *document, const mfcless::ViewClass *pNewViewClass);
	void ActivateFrame(int nCmdShow = -1);
	void OnUpdateFrameTitle(bool bAddToTitle);

// Generated message map functions
public:
	// Called by the main frame when the MDI area activates or deactivates this window.
	void OnMDIActivate(bool bActivate);
protected:
	intptr_t OnDPIChangedAfterParent(uintptr_t, intptr_t);
	void OnDestroy();
	intptr_t OnChangeViewClass(uintptr_t, intptr_t lParam);
	intptr_t OnInstrumentSelected(uintptr_t, intptr_t lParam);
	void RegisterMessageMap() override;

	void closeEvent(QCloseEvent *e) override;
};

/////////////////////////////////////////////////////////////////////////////


OPENMPT_NAMESPACE_END
