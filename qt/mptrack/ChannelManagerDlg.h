/*
 * ChannelManagerDlg.h
 * -------------------
 * Purpose: Dialog class for moving, removing, managing channels
 * Notes  : mfcless twin of upstream's mptrack/ChannelManagerDlg.h.
 *          Hand-edited from the twin.py draft.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "mfcless/Graphics.h"
#include "DialogBase.h"

class QFont;
class QObject;
class QWidget;

OPENMPT_NAMESPACE_BEGIN

class CModDoc;
class QuickChannelProperties;
struct UpdateHint;

class CChannelManagerDlg : public DialogBase
{
	Q_OBJECT
	enum Tab
	{
		kSoloMute      = 0,
		kRecordSelect  = 1,
		kPluginState   = 2,
		kReorderRemove = 3,
		kNumTabs
	};

public:

	static CChannelManagerDlg *sharedInstance() { return sharedInstance_; }
	static CChannelManagerDlg *sharedInstanceCreate();
	static void DestroySharedInstance() { delete sharedInstance_; sharedInstance_ = nullptr; }
	void SetDocument(CModDoc *modDoc);
	CModDoc *GetDocument() const { return m_ModDoc; }
	bool IsDisplayed() const;
	void Update(UpdateHint hint, QObject* pHint);
	void Show();
	void Hide();

private:
	static CChannelManagerDlg *sharedInstance_;
	std::unique_ptr<QuickChannelProperties> m_quickChannelProperties;

protected:

	enum ButtonAction : uint8
	{
		kUndetermined,
		kAction1,
		kAction2,
	};

	enum MouseButton : uint8
	{
		CM_BT_NONE,
		CM_BT_LEFT,
		CM_BT_RIGHT,
	};

	CChannelManagerDlg();
	~CChannelManagerDlg();

	struct State
	{
		mfcless::Rect move;
		CHANNELINDEX sourceChn = 0;
		bool removed = false;
		bool select = false;
		bool memoryMute = false;
		uint8 memoryRecordGroup = 0;
		bool memoryNoFx = false;
	};

	std::vector<State> m_states;
	CModDoc *m_ModDoc = nullptr;
	mfcless::Bitmap m_bkgnd;  // the buttons as last drawn
	mfcless::Rect m_drawableArea;
	Tab m_currentTab = kSoloMute;
	int m_downX = 0, m_downY = 0;
	int m_moveX = 0, m_moveY = 0;
	int m_buttonHeight = 0;
	ButtonAction m_buttonAction = kUndetermined;
	bool m_leftButton = false;
	bool m_rightButton = false;
	bool m_moveRect = false;
	bool m_show = false;

	CHANNELINDEX ButtonHit(mfcless::Point point, mfcless::Rect *invalidate = nullptr) const;
	void MouseEvent(uint32 nFlags, mfcless::Point point, MouseButton button);
	void ResetState(bool bSelection = true, bool bMove = true, bool bInternal = true, bool bOrder = false);
	void ResizeWindow();
	bool OnInitDialog() override;
	void OnDPIChanged() override;
	intptr_t OnToolHitTest(mfcless::Point point, mfcless::ToolInfo *pTI) const override;
	QString GetToolTipText(uintptr_t id) override;
	void OnApply();
	void OnClose();
	void OnSelectAll();
	void OnInvert();
	void OnAction1() { OnAction(1); }
	void OnAction2() { OnAction(2); }
	void OnAction(uint8 action);
	void OnStore();
	void OnRestore();
	void OnTabSelchange();
	void OnPaint();
	void OnMouseMove(uint32 nFlags,mfcless::Point point);
	void OnLButtonUp(uint32 nFlags,mfcless::Point point);
	void OnLButtonDown(uint32 nFlags,mfcless::Point point);
	void OnRButtonUp(uint32 nFlags,mfcless::Point point);
	void OnRButtonDown(uint32 nFlags,mfcless::Point point);
	void OnMButtonDown(uint32 nFlags,mfcless::Point point);
	void OnLButtonDblClk(uint32 nFlags, mfcless::Point point);
	void OnRButtonDblClk(uint32 nFlags, mfcless::Point point);
	void RegisterMessageMap() override;;
};

OPENMPT_NAMESPACE_END
