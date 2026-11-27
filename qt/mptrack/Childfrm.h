/*
 * Childfrm.h
 * ----------
 * Purpose: Implementation of the MDI document child windows.
 * Notes  : mfcless twin of upstream's mptrack/Childfrm.h.
 *          Draft from qt/tools/twin.py, not yet edited by hand.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include <QtWidgets/QMdiSubWindow>

#include "PatternCursor.h"

#include "../common/FileReaderFwd.h"

class QWidget;

OPENMPT_NAMESPACE_BEGIN

class CModControlView;
class CModControlDlg;

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



class CChildFrame: public QMdiSubWindow
{
	Q_OBJECT
	friend class CModControlDlg;
	Q_OBJECT
public:
	CChildFrame();
	~CChildFrame() override;

protected:
	static CChildFrame *m_lastActiveFrame;
	static int glMdiOpenCount;

// Attributes
protected:
	CSplitterWnd m_wndSplitter;
	QWidget * m_hWndCtrl = nullptr, m_hWndView = nullptr;
	GeneralViewState m_ViewGeneral;
	PatternViewState m_ViewPatterns;
	SampleViewState m_ViewSamples;
	InstrumentViewState m_ViewInstruments;
	CommentsViewState m_ViewComments;
	std::string m_currentViewClassName;
	int m_dpi = 0;
	bool m_maxWhenClosed = false;
	bool m_initialActivation = true;

// Operations
public:
	CModControlView *GetModControlView() const { return reinterpret_cast<CModControlView *>(m_wndSplitter.GetPane(0, 0)); }
	bool ChangeViewClass(CRuntimeClass *pNewViewClass, CCreateContext *pContext = nullptr);
	void ForceRefresh();
	void SavePosition(bool exit = false);
	intptr_t SendCtrlMessage(uint32 uMsg, intptr_t lParam = 0) const;
	intptr_t SendViewMessage(uint32 uMsg, intptr_t lParam = 0) const;
	intptr_t ActivateView(uint32 nId, intptr_t lParam);
	QWidget * GetHwndCtrl() const { return m_hWndCtrl; }
	QWidget * GetHwndView() const { return m_hWndView; }
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

// Overrides
	// ClassWizard generated virtual function overrides
public:
	bool OnCreateClient(LPCREATESTRUCT lpcs, CCreateContext* pContext) override;
	bool PreCreateWindow(CREATESTRUCT& cs) override;
	void ActivateFrame(int nCmdShow) override;
	void OnUpdateFrameTitle(bool bAddToTitle) override;
// Generated message map functions
protected:
	intptr_t OnDPIChangedAfterParent(uintptr_t, intptr_t);
	void OnDestroy();
	bool OnNcActivate(bool bActivate);
	void OnMDIActivate(bool bActivate, QWidget *pActivateWnd, QWidget *pDeactivateWnd);
	intptr_t OnChangeViewClass(uintptr_t, intptr_t lParam);
	intptr_t OnInstrumentSelected(uintptr_t, intptr_t lParam);
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.


OPENMPT_NAMESPACE_END
