/*
 * ChannelManagerDlg.cpp
 * ---------------------
 * Purpose: Dialog class for moving, removing, managing channels
 * Notes  : mfcless twin of upstream's mptrack/ChannelManagerDlg.cpp.
 *          Draft from qt/tools/twin.py, not yet edited by hand.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "ChannelManagerDlg.h"
#include "mfcless/Widgets.h"
#include "HighDPISupport.h"
#include "Mainfrm.h"
#include "Moddoc.h"
#include "PatternEditorDialogs.h"
#include "resource.h"
#include "UpdateHints.h"
#include "../common/mptStringBuffer.h"

#include <functional>

OPENMPT_NAMESPACE_BEGIN

#define CM_NB_COLS		8
#define CM_BT_HEIGHT	22

///////////////////////////////////////////////////////////
// CChannelManagerDlg

void CChannelManagerDlg::RegisterMessageMap()
{
	DialogBase::RegisterMessageMap();
	// ON_WM_PAINT() (mfcless::Window)
	// ON_WM_MOUSEMOVE() (mfcless::Window)
	// ON_WM_LBUTTONUP() (mfcless::Window)
	// ON_WM_LBUTTONDOWN() (mfcless::Window)
	// ON_WM_LBUTTONDBLCLK() (mfcless::Window)
	// ON_WM_RBUTTONUP() (mfcless::Window)
	// ON_WM_RBUTTONDOWN() (mfcless::Window)
	// ON_WM_RBUTTONDBLCLK() (mfcless::Window)
	// ON_WM_MBUTTONDOWN() (mfcless::Window)
	// ON_WM_CLOSE() (mfcless::Window)
	AddCommand(IDC_BUTTON1, &CChannelManagerDlg::OnApply);
	AddCommand(IDC_BUTTON2, &CChannelManagerDlg::OnClose);
	AddCommand(IDC_BUTTON3, &CChannelManagerDlg::OnSelectAll);
	AddCommand(IDC_BUTTON4, &CChannelManagerDlg::OnInvert);
	AddCommand(IDC_BUTTON5, &CChannelManagerDlg::OnAction1);
	AddCommand(IDC_BUTTON6, &CChannelManagerDlg::OnAction2);
	AddCommand(IDC_BUTTON7, &CChannelManagerDlg::OnStore);
	AddCommand(IDC_BUTTON8, &CChannelManagerDlg::OnRestore);
	AddNotify(IDC_TAB1, mfcless::Notify::SelChange, &CChannelManagerDlg::OnTabSelchange);
}

CChannelManagerDlg * CChannelManagerDlg::sharedInstance_ = nullptr;

CChannelManagerDlg * CChannelManagerDlg::sharedInstanceCreate()
{
	try
	{
		if(sharedInstance_ == nullptr)
			sharedInstance_ = new CChannelManagerDlg();
	} catch(mpt::out_of_memory e)
	{
		mpt::delete_out_of_memory(e);
	}
	return sharedInstance_;
}

void CChannelManagerDlg::SetDocument(CModDoc *modDoc)
{
	if(modDoc == m_ModDoc)
		return;
	
	m_ModDoc = modDoc;
	ResetState(true, true, true);
	if(m_show)
	{
		if(m_ModDoc)
		{
			ResizeWindow();
			ShowWindow(mfcless::show::ShowNoActivate);  // In case the window was hidden because no module was loaded
			InvalidateRect(m_drawableArea, false);
		} else
		{
			ShowWindow(false);
		}
	}
}

bool CChannelManagerDlg::IsDisplayed() const
{
	return m_show;
}

void CChannelManagerDlg::Update(UpdateHint hint, QObject *pHint)
{
	if(!this || !m_show)
		return;
	if(!hint.ToType<GeneralHint>().GetType()[HINT_MODCHANNELS | HINT_MODGENERAL | HINT_MODTYPE | HINT_MPTOPTIONS])
		return;

	const size_t oldNumChannels = m_states.size();
	m_states.resize(m_ModDoc->GetNumChannels());
	for(size_t chn = oldNumChannels; chn < m_states.size(); chn++)
	{
		m_states[chn].sourceChn = static_cast<CHANNELINDEX>(chn);
	}

	ResizeWindow();
	InvalidateRect(nullptr, false);
	if(hint.ToType<GeneralHint>().GetType()[HINT_MODCHANNELS] && m_quickChannelProperties && pHint != m_quickChannelProperties.get())
		m_quickChannelProperties->UpdateDisplay();
}

void CChannelManagerDlg::Show()
{
	if(!this)
	{
		Create(IDD_CHANNELMANAGER, nullptr);
	}
	ResizeWindow();
	ShowWindow(true);
	m_show = true;
}

void CChannelManagerDlg::Hide()
{
	if(this != nullptr && m_show)
	{
		ResetState(true, true, true);
		ShowWindow(false);
		m_show = false;
	}
}


CChannelManagerDlg::CChannelManagerDlg()
	: m_quickChannelProperties{std::make_unique<QuickChannelProperties>()}
	, m_buttonHeight{CM_BT_HEIGHT}
{
}

CChannelManagerDlg::~CChannelManagerDlg()
{
	if(this == sharedInstance_)
		sharedInstance_ = nullptr;
	if(m_bkgnd)
		DeleteBitmap(m_bkgnd);
	DestroyWindow();
}

bool CChannelManagerDlg::OnInitDialog()
{
	DialogBase::OnInitDialog();

	QWidget * menu = ::GetDlgItem(this, IDC_TAB1);

	TCITEM tie;
	tie.mask = TCIF_TEXT | TCIF_IMAGE;
	tie.iImage = -1;
	tie.pszText = const_cast<LPTSTR>(UL_("Solo/Mute"));
	TabCtrl_InsertItem(menu, kSoloMute, &tie);
	tie.pszText = const_cast<LPTSTR>(UL_("Record Select"));
	TabCtrl_InsertItem(menu, kRecordSelect, &tie);
	tie.pszText = const_cast<LPTSTR>(UL_("Plugins"));
	TabCtrl_InsertItem(menu, kPluginState, &tie);
	tie.pszText = const_cast<LPTSTR>(UL_("Reorder/Remove"));
	TabCtrl_InsertItem(menu, kReorderRemove, &tie);
	m_currentTab = kSoloMute;

	m_buttonHeight = HighDPISupport::ScalePixels(CM_BT_HEIGHT, this);
	::ShowWindow(::GetDlgItem(this, IDC_BUTTON1), mfcless::show::Hide);

	ResetState(true, true, true, true);

	return true;
}


void CChannelManagerDlg::OnDPIChanged()
{
	m_font.DeleteObject();
	ResizeWindow();
	DialogBase::OnDPIChanged();
}


intptr_t CChannelManagerDlg::OnToolHitTest(mfcless::Point point, mfcless::ToolInfo *pTI) const
{
	mfcless::Rect rect;
	pTI->hwnd = this;
	pTI->uId = ButtonHit(point, &rect);
	pTI->rect = rect;
	pTI->lpszText = LPSTR_TEXTCALLBACK;
	if(pTI->uId == CHANNELINDEX_INVALID)
		return -1;
	return pTI->uId;
}


mpt::ustring CChannelManagerDlg::GetToolTipText(uint32 id, QWidget * hwnd) const
{
	mpt::ustring text;
	if(hwnd || !m_ModDoc || id >= m_states.size())
		return text;

	const CHANNELINDEX chn = m_states[id].sourceChn;
	const auto &chnSettings = m_ModDoc->GetSoundFile().ChnSettings[chn];
	if(!chnSettings.szName.empty())
		text = MPT_UFORMAT("{}: {}")(chn + 1, mpt::ToUnicode(m_ModDoc->GetSoundFile().GetCharsetInternal(), chnSettings.szName));
	else
		text = MPT_UFORMAT("Channel {}")(chn + 1);

	switch(m_currentTab)
	{
	case kSoloMute:
		text += chnSettings.dwFlags[CHN_MUTE] ? UL_(" (Muted)") : UL_(" (Unmuted)");
		break;
	case kRecordSelect:
		switch(m_ModDoc->GetChannelRecordGroup(chn))
		{
		case RecordGroup::NoGroup: text += UL_(" (No Record Group)"); break;
		case RecordGroup::Group1: text += UL_(" (Record Group 1)"); break;
		case RecordGroup::Group2: text += UL_(" (Record Group 2)"); break;
		}
		break;
	case kPluginState:
		text += chnSettings.dwFlags[CHN_NOFX] ? UL_(" (Plugins Bypassed)") : UL_(" (Plugins Enabled)");
		break;
	case kReorderRemove:
		if(m_states[id].removed)
			text += UL_(" (Marked for Removal)");
		break;
	case kNumTabs:
		MPT_ASSERT_NOTREACHED();
		break;
	}
	return text;
}


void CChannelManagerDlg::OnApply()
{
	if(!m_ModDoc) return;

	std::vector<State> newStates;
	std::vector<CHANNELINDEX> newChnOrder;
	newStates.reserve(m_ModDoc->GetNumChannels());
	newChnOrder.reserve(m_ModDoc->GetNumChannels());

	// Count new number of channels, copy pattern pointers & manager internal store memory
	for(const auto &state : m_states)
	{
		if(!state.removed)
		{
			newStates.push_back(state);
			newChnOrder.push_back(state.sourceChn);
		}
	}

	mfcless::BeginWaitCursor();

	CriticalSection cs;
	if(m_ModDoc->ReArrangeChannels(newChnOrder) != newChnOrder.size())
	{
		cs.Leave();
		mfcless::EndWaitCursor();
		return;
	}

	// Update manager internal store memory
	m_states = std::move(newStates);
	for(CHANNELINDEX chn = 0; chn < m_states.size(); chn++)
	{
		m_states[chn].sourceChn = chn;
	}

	cs.Leave();
	mfcless::EndWaitCursor();

	ResetState(true, true, true, true);

	// Update document & windows
	m_ModDoc->SetModified();
	m_ModDoc->UpdateAllViews(nullptr, GeneralHint().Channels().ModType(), this); //refresh channel headers

	// Redraw channel manager window
	ResizeWindow();
	InvalidateRect(nullptr, false);
}

void CChannelManagerDlg::OnClose()
{
	if(m_bkgnd) DeleteBitmap(m_bkgnd);
	ResetState(true, true, true, true);
	m_bkgnd = nullptr;
	m_show = false;

	DialogBase::OnCancel();
}

void CChannelManagerDlg::OnSelectAll()
{
	for(auto &state : m_states)
		state.select = true;
	InvalidateRect(m_drawableArea, false);
}

void CChannelManagerDlg::OnInvert()
{
	for(auto &state : m_states)
		state.select = !state.select;
	InvalidateRect(m_drawableArea, false);
}

void CChannelManagerDlg::OnAction(uint8 action)
{
	if(!m_show || m_ModDoc == nullptr)
		return;

	int numSelectedAndMatching = 0, numSelected = 0;
	const auto recordGroup = (action == 1 ? RecordGroup::Group1 : RecordGroup::Group2);

	switch(m_currentTab)
	{
		case kSoloMute:
			for(CHANNELINDEX chn = 0; chn < m_ModDoc->GetNumChannels(); chn++)
			{
				CHANNELINDEX sourceChn = m_states[chn].sourceChn;
				if(action == 1)
					m_ModDoc->MuteChannel(sourceChn, !m_states[chn].select);
				else if(m_states[chn].select)
					m_ModDoc->MuteChannel(sourceChn, !m_ModDoc->IsChannelMuted(sourceChn));
			}
			break;
		case kRecordSelect:
			for(CHANNELINDEX chn = 0; chn < m_ModDoc->GetNumChannels(); chn++)
			{
				CHANNELINDEX sourceChn = m_states[chn].sourceChn;
				if(m_states[chn].select)
					numSelected++;
				if(m_states[chn].select && m_ModDoc->GetChannelRecordGroup(sourceChn) == recordGroup)
					numSelectedAndMatching++;
			}
			for(CHANNELINDEX chn = 0; chn < m_ModDoc->GetNumChannels(); chn++)
			{
				if(m_states[chn].select)
				{
					CHANNELINDEX sourceChn = m_states[chn].sourceChn;
					if(numSelected != numSelectedAndMatching && m_ModDoc->GetChannelRecordGroup(sourceChn) != recordGroup)
						m_ModDoc->SetChannelRecordGroup(sourceChn, recordGroup);
					else if(numSelected == numSelectedAndMatching)
						m_ModDoc->SetChannelRecordGroup(sourceChn, RecordGroup::NoGroup);
				}
			}
			break;
		case kPluginState:
			for(CHANNELINDEX chn = 0; chn < m_ModDoc->GetNumChannels(); chn++)
			{
				CHANNELINDEX sourceChn = m_states[chn].sourceChn;
				if(m_states[chn].select)
					m_ModDoc->NoFxChannel(sourceChn, action == 2);
			}
			break;
		case kReorderRemove:
			if(action == 1)
			{
				for(CHANNELINDEX chn = 0; chn < m_ModDoc->GetNumChannels(); chn++)
				{
					if(m_states[chn].select)
						m_states[chn].removed = !m_states[chn].removed;
				}
			} else
			{
				ResetState(false, false, false, true);
			}
			break;
		case kNumTabs:
			MPT_ASSERT_NOTREACHED();
			break;
	}

	ResetState();

	m_ModDoc->UpdateAllViews(nullptr, GeneralHint().Channels(), this);
	InvalidateRect(m_drawableArea, false);
}


void CChannelManagerDlg::OnStore()
{
	if(!m_show || m_ModDoc == nullptr)
		return;

	switch(m_currentTab)
	{
		case kSoloMute:
			for(auto &state : m_states)
				state.memoryMute = m_ModDoc->IsChannelMuted(state.sourceChn);
			break;
		case kRecordSelect:
			for(auto &state : m_states)
				state.memoryRecordGroup = static_cast<uint8>(m_ModDoc->GetChannelRecordGroup(state.sourceChn));
			break;
		case kPluginState:
			for(auto &state : m_states)
				state.memoryNoFx = m_ModDoc->IsChannelNoFx(state.sourceChn);
			break;
		case kReorderRemove:
		default:
			break;
	}
}

void CChannelManagerDlg::OnRestore()
{
	if(!m_show || m_ModDoc == nullptr)
		return;

	switch(m_currentTab)
	{
		case kSoloMute:
			for(auto &state : m_states)
				m_ModDoc->MuteChannel(state.sourceChn, state.memoryMute);
			break;
		case kRecordSelect:
			m_ModDoc->ReinitRecordState();
			for(auto &state : m_states)
				m_ModDoc->SetChannelRecordGroup(state.sourceChn, static_cast<RecordGroup>(state.memoryRecordGroup));
			break;
		case kPluginState:
			for(auto &state : m_states)
				m_ModDoc->NoFxChannel(state.sourceChn, state.memoryNoFx);
			break;
		case kReorderRemove:
			for(CHANNELINDEX chn = 0; chn < m_ModDoc->GetNumChannels(); chn++)
				m_states[chn].sourceChn = chn;
			ResetState(false, false, false, true);
			break;
		default:
			break;
	}

	if(m_currentTab != kReorderRemove)
		ResetState();

	m_ModDoc->UpdateAllViews(nullptr, GeneralHint().Channels(), this);
	InvalidateRect(m_drawableArea, false);
}

void CChannelManagerDlg::OnTabSelchange()
{
	if(!m_show) return;

	m_currentTab = static_cast<Tab>(TabCtrl_GetCurFocus(::GetDlgItem(this, IDC_TAB1)));

	switch(m_currentTab)
	{
		case kSoloMute:
			SetItemText(IDC_BUTTON5, UL_("S&olo"));
			SetItemText(IDC_BUTTON6, UL_("M&ute"));
			::ShowWindow(::GetDlgItem(this, IDC_BUTTON5),mfcless::show::Show);
			::ShowWindow(::GetDlgItem(this, IDC_BUTTON6),mfcless::show::Show);
			::ShowWindow(::GetDlgItem(this, IDC_BUTTON1),mfcless::show::Hide);
			break;
		case kRecordSelect:
			SetItemText(IDC_BUTTON5, UL_("Instrument &1"));
			SetItemText(IDC_BUTTON6, UL_("Instrument &2"));
			::ShowWindow(::GetDlgItem(this, IDC_BUTTON5),mfcless::show::Show);
			::ShowWindow(::GetDlgItem(this, IDC_BUTTON6),mfcless::show::Show);
			::ShowWindow(::GetDlgItem(this, IDC_BUTTON1),mfcless::show::Hide);
			break;
		case kPluginState:
			SetItemText(IDC_BUTTON5, UL_("&Enable FX"));
			SetItemText(IDC_BUTTON6, UL_("&Disable FX"));
			::ShowWindow(::GetDlgItem(this, IDC_BUTTON5),mfcless::show::Show);
			::ShowWindow(::GetDlgItem(this, IDC_BUTTON6),mfcless::show::Show);
			::ShowWindow(::GetDlgItem(this, IDC_BUTTON1),mfcless::show::Hide);
			break;
		case kReorderRemove:
			SetItemText(IDC_BUTTON5, UL_("R&emove"));
			SetItemText(IDC_BUTTON6, UL_("Ca&ncel All"));
			::ShowWindow(::GetDlgItem(this, IDC_BUTTON5),mfcless::show::Show);
			::ShowWindow(::GetDlgItem(this, IDC_BUTTON6),mfcless::show::Show);
			::ShowWindow(::GetDlgItem(this, IDC_BUTTON1),mfcless::show::Show);
			break;
		default:
			break;
	}

	InvalidateRect(m_drawableArea, false);
}


void CChannelManagerDlg::ResizeWindow()
{
	if(!this || !m_ModDoc)
		return;

	const int dpi = HighDPISupport::GetDpiForWindow(this);

	const auto oldButtonHeight = m_buttonHeight;
	m_buttonHeight = mfcless::MulDiv(CM_BT_HEIGHT, dpi, 96);

	CHANNELINDEX channels = m_ModDoc->GetNumChannels();
	int lines = channels / CM_NB_COLS + (channels % CM_NB_COLS ? 1 : 0);

	mfcless::Rect window;
	GetWindowRect(window);

	mfcless::Rect client;
	GetClientRect(client);
	m_drawableArea = client;
	m_drawableArea.DeflateRect(mfcless::MulDiv(10, dpi, 96), mfcless::MulDiv(38, dpi, 96), mfcless::MulDiv(8, dpi, 96), mfcless::MulDiv(30, dpi, 96));

	int chnSizeY = m_drawableArea.Height() / lines;

	if(chnSizeY != m_buttonHeight || oldButtonHeight != m_buttonHeight)
	{
		SetWindowPos(nullptr, 0, 0, window.Width(), window.Height() + (m_buttonHeight - chnSizeY) * lines, mfcless::swp::NoMove | mfcless::swp::NoZOrder | mfcless::swp::NoRedraw);

		GetClientRect(client);

		// Move butttons to bottom of the window
		auto dwp = ::BeginDeferWindowPos(6);
		for(auto id : {IDC_BUTTON1, IDC_BUTTON2, IDC_BUTTON3, IDC_BUTTON4, IDC_BUTTON5, IDC_BUTTON6})
		{
			QWidget *button = GetDlgItem(id);
			if(button != nullptr)
			{
				mfcless::Rect btn;
				button->GetClientRect(btn);
				button->MapWindowPoints(this, btn);
				::DeferWindowPos(dwp, *button, nullptr, btn.left, client.Height() - btn.Height() - mfcless::MulDiv(3, dpi, 96), 0, 0, mfcless::swp::NoSize | mfcless::swp::NoZOrder | mfcless::swp::NoActivate);
			}
		}

		::EndDeferWindowPos(dwp);

		if(m_bkgnd)
		{
			DeleteObject(m_bkgnd);
			m_bkgnd = nullptr;
		}

		m_drawableArea = client;
		m_drawableArea.DeflateRect(mfcless::MulDiv(10, dpi, 96), mfcless::MulDiv(38, dpi, 96), mfcless::MulDiv(8, dpi, 96), mfcless::MulDiv(30, dpi, 96));
		Invalidate(false);
	}
}


void CChannelManagerDlg::OnPaint()
{
	if(!this || !m_show || m_ModDoc == nullptr)
	{
		DialogBase::OnPaint();
		ShowWindow(false);
		return;
	}
	if(IsIconic())
	{
		DialogBase::OnPaint();
		return;
	}

	if(!m_font.m_hObject)
	{
		HighDPISupport::CreateGUIFont(m_font, this);
	}

	PAINTSTRUCT pDC;
	::BeginPaint(this, &pDC);
	const mfcless::Rect &rcPaint = pDC.rcPaint;

	const int chnSizeX = m_drawableArea.Width() / CM_NB_COLS;
	const int chnSizeY = m_buttonHeight;

	if(m_currentTab == kReorderRemove && m_moveRect && m_bkgnd)
	{
		// Only draw channels to be moved around
		mfcless::DC * bdc = ::CreateCompatibleDC(pDC.hdc);
		::SelectObject(bdc, m_bkgnd);
		::BitBlt(pDC.hdc, rcPaint.left, rcPaint.top, rcPaint.Width(), rcPaint.Height(), bdc, rcPaint.left, rcPaint.top, mfcless::rop::SrcCopy);

		BLENDFUNCTION ftn;
		ftn.BlendOp = AC_SRC_OVER;
		ftn.BlendFlags = 0;
		ftn.SourceConstantAlpha = 192;
		ftn.AlphaFormat = 0;

		for(const auto &state : m_states)
		{
			if(state.select)
			{
				mfcless::Rect btn = state.move;
				btn.DeflateRect(3, 3, 0, 0);

				AlphaBlend(pDC.hdc, btn.left + m_moveX - m_downX, btn.top + m_moveY - m_downY, btn.Width(), btn.Height(), bdc,
					btn.left, btn.top, btn.Width(), btn.Height(), ftn);
			}
		}
		::SelectObject(bdc, nullptr);
		::DeleteDC(bdc);

		::EndPaint(this, &pDC);
		return;
	}

	mfcless::Rect client;
	GetClientRect(&client);

	mfcless::DC * dc = ::CreateCompatibleDC(pDC.hdc);
	if(!m_bkgnd)
		m_bkgnd = ::CreateCompatibleBitmap(pDC.hdc, client.Width(), client.Height());
	HGDIOBJ oldBmp = ::SelectObject(dc, m_bkgnd);
	HGDIOBJ oldFont = ::SelectObject(dc, m_font);

	const auto dcBrush = GetStockBrush(DC_BRUSH);

	const int dpi = HighDPISupport::GetDpiForWindow(this);
	client.SetRect(client.left + mfcless::MulDiv(2, dpi, 96), client.top + mfcless::MulDiv(32, dpi, 96), client.right - mfcless::MulDiv(2, dpi, 96), client.bottom - mfcless::MulDiv(24, dpi, 96));
	// Draw background
	{
		const auto bgIntersected = client & pDC.rcPaint;  // In case of partial redraws, FillRect may still draw into areas that are not part of the redraw area and thus make some buttons disappear
		::FillRect(dc, &pDC.rcPaint, GetSysColorBrush(COLOR_BTNFACE));
		::FillRect(dc, &bgIntersected, GetSysColorBrush(COLOR_HIGHLIGHT));
		::SetDCBrushColor(dc, mfcless::Rgb(20, 20, 20));
		::FrameRect(dc, &client, dcBrush);
	}

	client.SetRect(client.left + 8,client.top + 6,client.right - 6,client.bottom - 6);

	const uint32 highlight = mfcless::SystemColor(mfcless::SysColor::Highlight), red = mfcless::Rgb(192, 96, 96), green = mfcless::Rgb(96, 192, 96), redBright = mfcless::Rgb(218, 163, 163), greenBright = mfcless::Rgb(163, 218, 163);
	const uint32 brushColors[] = { highlight, green, red };
	const uint32 brushColorsBright[] = { highlight, greenBright, redBright };
	const auto buttonFaceColor = mfcless::SystemColor(mfcless::SysColor::ButtonFace), windowColor = mfcless::SystemColor(mfcless::SysColor::Window);

	uint32 col = 0, row = 0;
	const CSoundFile &sndFile = m_ModDoc->GetSoundFile();
	mpt::ustring s;
	for(const auto &state : m_states)
	{
		const CHANNELINDEX sourceChn = state.sourceChn;
		const auto &chnSettings = sndFile.ChnSettings[sourceChn];

		if(!chnSettings.szName.empty())
			s = MPT_UFORMAT("{}: {}")(sourceChn + 1, mpt::ToUnicode(sndFile.GetCharsetInternal(), sndFile.ChnSettings[sourceChn].szName));
		else
			s = MPT_UFORMAT("Channel {}")(sourceChn + 1);

		const int border = HighDPISupport::ScalePixels(3, this);
		mfcless::Rect btn;
		btn.left = client.left + col * chnSizeX + border;
		btn.right = btn.left + chnSizeX - border;
		btn.top = client.top + row * chnSizeY + border;
		btn.bottom = btn.top + chnSizeY - border;
		col++;
		if(col >= CM_NB_COLS)
		{
			col = 0;
			row++;
		}

		if(!mfcless::Rect{}.IntersectRect(&pDC.rcPaint, &btn))
			continue;

		// Button
		const bool activate = state.select;
		const bool enable = !state.removed;
		auto btnAdjusted = btn;  // Without border
		::DrawEdge(dc, btnAdjusted, enable ? mfcless::edge::Raised : mfcless::edge::Sunken, mfcless::edge::Rect | mfcless::edge::Middle | mfcless::edge::Adjust);
		if(activate)
			::FillRect(dc, btnAdjusted, GetSysColorBrush(COLOR_WINDOW));

		if(chnSettings.color != ModChannelSettings::INVALID_COLOR)
		{
			// Channel color
			const auto startColor = chnSettings.color;
			const auto endColor = activate ? windowColor : buttonFaceColor;
			const auto width = btnAdjusted.Width() / 2;
			auto rect = btnAdjusted;
			rect.left = rect.right - width;
			rect.right = rect.left + 1;
			for(int i = 0; i < width; i++)
			{
				auto blend = static_cast<double>(i) / width, blendInv = 1.0 - blend;
				auto blendColor = mfcless::Rgb(mpt::saturate_round<uint8>(mfcless::RedOf(startColor) * blend + mfcless::RedOf(endColor) * blendInv),
				                      mpt::saturate_round<uint8>(mfcless::GreenOf(startColor) * blend + mfcless::GreenOf(endColor) * blendInv),
				                      mpt::saturate_round<uint8>(mfcless::BlueOf(startColor) * blend + mfcless::BlueOf(endColor) * blendInv));
				::SetDCBrushColor(dc, blendColor);
				::FillRect(dc, &rect, dcBrush);
				rect.left++;
				rect.right++;
			}
		}

		// Text
		{
			auto rect = btnAdjusted;
			rect.left += HighDPISupport::ScalePixels(9, this);
			rect.right -= HighDPISupport::ScalePixels(3, this);

			::SetBkMode(dc, TRANSPARENT);
			::SetTextColor(dc, GetSysColor(enable || activate ? COLOR_BTNTEXT : COLOR_GRAYTEXT));
			::DrawText(dc, s, -1, &rect, mfcless::dt::Right | mfcless::dt::VCenter | mfcless::dt::SingleLine | mfcless::dt::NoPrefix);
		}

		// Draw red/green markers
		{
			const int margin = HighDPISupport::ScalePixels(1, this);
			auto rect = btnAdjusted;
			rect.DeflateRect(margin, margin);
			rect.right = rect.left + HighDPISupport::ScalePixels(7, this);
			const auto &brushes = activate ? brushColorsBright : brushColors;
			const auto redBrush = brushes[2], greenBrush = brushes[1];
			uint32 color = 0;
			switch(m_currentTab)
			{
			case kSoloMute:
				color = chnSettings.dwFlags[CHN_MUTE] ? redBrush : greenBrush;
				break;
			case kRecordSelect:
				color = brushColors[static_cast<size_t>(m_ModDoc->GetChannelRecordGroup(sourceChn)) % std::size(brushColors)];
				break;
			case kPluginState:
				color = chnSettings.dwFlags[CHN_NOFX] ? redBrush : greenBrush;
				break;
			case kReorderRemove:
				color = state.removed ? redBrush : greenBrush;
				break;
			case kNumTabs:
				MPT_ASSERT_NOTREACHED();
				break;
			}
			::SetDCBrushColor(dc, color);
			::FillRect(dc, rect, dcBrush);
			// Draw border around marker
			::SetDCBrushColor(dc, mfcless::Rgb(20, 20, 20));
			::FrameRect(dc, rect, dcBrush);
		}
	}

	::BitBlt(pDC.hdc, rcPaint.left, rcPaint.top, rcPaint.Width(), rcPaint.Height(), dc, rcPaint.left, rcPaint.top, mfcless::rop::SrcCopy);
	::SelectObject(dc, oldFont);
	::SelectObject(dc, oldBmp);
	::DeleteDC(dc);

	::EndPaint(this, &pDC);
}


CHANNELINDEX CChannelManagerDlg::ButtonHit(mfcless::Point point, mfcless::Rect *invalidate) const
{
	const mfcless::Rect &client = m_drawableArea;

	if(PtInRect(client, point) && m_ModDoc != nullptr)
	{
		uint32 nColns = CM_NB_COLS;

		int x = point.x - client.left;
		int y = point.y - client.top;

		int dx = client.Width() / (int)nColns;
		int dy = m_buttonHeight;

		x = x / dx;
		y = y / dy;
		CHANNELINDEX n = static_cast<CHANNELINDEX>(y * nColns + x);
		if(n < m_ModDoc->GetNumChannels())
		{
			if(invalidate)
			{
				invalidate->left = client.left + x * dx;
				invalidate->right = invalidate->left + dx;
				invalidate->top = client.top + y * dy;
				invalidate->bottom = invalidate->top + dy;
			}
			return n;
		}
	}
	return CHANNELINDEX_INVALID;
}


void CChannelManagerDlg::ResetState(bool bSelection, bool bMove, bool bInternal, bool bOrder)
{
	size_t oldSize = m_states.size();
	m_states.resize(m_ModDoc ? m_ModDoc->GetNumChannels() : 0);
	for(CHANNELINDEX chn = 0; chn < m_states.size(); chn++)
	{
		if(bSelection)
			m_states[chn].select = false;
		if(bOrder || chn > oldSize)
		{
			m_states[chn].sourceChn = chn;
			m_states[chn].removed = false;
		}
	}
	if(bMove || bInternal)
	{
		m_leftButton = false;
		m_rightButton = false;
	}
	if(bMove)
		m_moveRect = false;
}


void CChannelManagerDlg::OnMouseMove(uint32 nFlags,mfcless::Point point)
{
	if(!this || m_show == false) return;

	if(!m_leftButton && !m_rightButton)
	{
		m_moveX = point.x;
		m_moveY = point.y;
		return;
	}
	MouseEvent(nFlags, point, m_moveRect ? CM_BT_NONE : (m_leftButton ? CM_BT_LEFT : CM_BT_RIGHT));
}

void CChannelManagerDlg::OnLButtonUp(uint32 /*nFlags*/,mfcless::Point point)
{
	ReleaseCapture();
	if(!this || !m_show)
		return;

	if(m_moveRect && m_ModDoc)
	{
		mfcless::Rect dropRect;
		CHANNELINDEX dropChn = ButtonHit(point, &dropRect);
		if(dropChn != CHANNELINDEX_INVALID)
		{
			// Rearrange channels
			const auto numChannels = m_ModDoc->GetNumChannels();
			if(point.x > dropRect.left + dropRect.Width() / 2 && dropChn < numChannels)
				dropChn++;

			std::vector<State> states;
			CHANNELINDEX selectedBeforeDropChn = 0;
			for(CHANNELINDEX chn = 0; chn < numChannels; chn++)
			{
				if(m_states[chn].select)
				{
					states.push_back(m_states[chn]);
					states.back().select = false;
					if(chn < dropChn)
						selectedBeforeDropChn++;
				}
			}

			// Remove all selected channels from the order
			const auto IsSelected = [](const State &state) { return state.select; };
			m_states.erase(std::remove_if(m_states.begin(), m_states.end(), IsSelected), m_states.end());
			// Then insert them at the drop position
			m_states.insert(m_states.begin() + dropChn - selectedBeforeDropChn, states.begin(), states.end());
		} else
		{
			ResetState(true, false, false, false);
		}

		m_moveRect = false;
		InvalidateRect(m_drawableArea, false);
		if(m_ModDoc) m_ModDoc->UpdateAllViews(nullptr, GeneralHint().Channels(), this);
	}

	m_leftButton = false;
}

void CChannelManagerDlg::OnLButtonDown(uint32 nFlags,mfcless::Point point)
{
	if(!this || m_show == false) return;
	SetCapture();

	if(ButtonHit(point) == CHANNELINDEX_INVALID)
		ResetState(true, false, false);

	m_leftButton = true;
	m_buttonAction = kUndetermined;
	MouseEvent(nFlags,point,CM_BT_LEFT);
	m_downX = point.x;
	m_downY = point.y;
}

void CChannelManagerDlg::OnRButtonUp(uint32 /*nFlags*/,mfcless::Point /*point*/)
{
	ReleaseCapture();
	m_rightButton = false;
}

void CChannelManagerDlg::OnRButtonDown(uint32 nFlags,mfcless::Point point)
{
	if(!this || m_show == false) return;
	SetCapture();

	m_rightButton = true;
	m_buttonAction = kUndetermined;
	if(m_moveRect)
	{
		ResetState(true, true, false, false);
		InvalidateRect(m_drawableArea, false);
	} else
	{
		MouseEvent(nFlags, point, CM_BT_RIGHT);
		m_downX = point.x;
		m_downY = point.y;
	}
}

void CChannelManagerDlg::OnMButtonDown(uint32 /*nFlags*/, mfcless::Point point)
{
	mfcless::Rect rect;
	CHANNELINDEX chn = ButtonHit(point, &rect);
	if(m_ModDoc != nullptr && chn != CHANNELINDEX_INVALID)
	{
		ClientToScreen(&point);
		m_quickChannelProperties->Show(m_ModDoc, m_states[chn].sourceChn, point);
	}
}

void CChannelManagerDlg::MouseEvent(uint32 nFlags,mfcless::Point point, MouseButton button)
{
	if(!m_ModDoc)
		return;

	m_moveX = point.x;
	m_moveY = point.y;

	mfcless::Rect client, invalidate;
	const CHANNELINDEX n = ButtonHit(point, &invalidate);
	if(n != CHANNELINDEX_INVALID && button != CM_BT_NONE)
	{
		const CHANNELINDEX sourceChn = m_states[n].sourceChn;
		if(nFlags & mfcless::MouseKey::Control)
		{
			if(button == CM_BT_LEFT)
			{
				if(!m_states[n].select && !m_states[n].removed)
					m_states[n].move = invalidate;
				m_states[n].select = true;
			} else if(button == CM_BT_RIGHT)
			{
				m_states[n].select = false;
			}
		} else
		{
			switch(m_currentTab)
			{
			case kSoloMute:
				if(button == CM_BT_LEFT)
				{
					if(m_buttonAction == kUndetermined)
					{
						bool isAlreadySolo = true;
						for(CHANNELINDEX chn = 0; chn < m_ModDoc->GetNumChannels(); chn++)
						{
							if((chn == sourceChn) == m_ModDoc->IsChannelMuted(chn))
							{
								isAlreadySolo = false;
								break;
							}
						}
						m_buttonAction = isAlreadySolo ? kAction2 : kAction1;
						for(CHANNELINDEX chn = 0; chn < m_ModDoc->GetNumChannels(); chn++)
						{
							m_ModDoc->MuteChannel(chn, m_buttonAction == kAction1);
						}
					}
					if(m_buttonAction == kAction1)
						m_ModDoc->MuteChannel(sourceChn, false);
					invalidate = client = m_drawableArea;
				} else
				{
					if(m_buttonAction == kUndetermined)
						m_buttonAction = m_ModDoc->IsChannelMuted(sourceChn) ? kAction1 : kAction2;
					m_ModDoc->MuteChannel(sourceChn, m_buttonAction == kAction2);
				}
				m_ModDoc->SetModified();
				m_ModDoc->UpdateAllViews(nullptr, GeneralHint(sourceChn).Channels(), this);
				break;
			case kRecordSelect:
				if(m_buttonAction == kUndetermined)
				{
					auto rec = m_ModDoc->GetChannelRecordGroup(sourceChn);
					m_buttonAction = (rec == RecordGroup::NoGroup || rec != (button == CM_BT_LEFT ? RecordGroup::Group1 : RecordGroup::Group2)) ? kAction1 : kAction2;
				}

				if(m_buttonAction == kAction1 && button == CM_BT_LEFT)
					m_ModDoc->SetChannelRecordGroup(sourceChn, RecordGroup::Group1);
				else if(m_buttonAction == kAction1 && button == CM_BT_RIGHT)
					m_ModDoc->SetChannelRecordGroup(sourceChn, RecordGroup::Group2);
				else
					m_ModDoc->SetChannelRecordGroup(sourceChn, RecordGroup::NoGroup);
				m_ModDoc->UpdateAllViews(nullptr, GeneralHint(sourceChn).Channels(), this);
				break;
			case kPluginState:
				m_ModDoc->NoFxChannel(sourceChn, (button != CM_BT_LEFT));
				m_ModDoc->SetModified();
				m_ModDoc->UpdateAllViews(nullptr, GeneralHint(sourceChn).Channels(), this);
				break;
			case kReorderRemove:
				if(button == CM_BT_LEFT)
				{
					m_states[n].move = invalidate;
					m_states[n].select = true;
				} else if(button == CM_BT_RIGHT)
				{
					if(m_buttonAction == kUndetermined)
						m_buttonAction = m_states[n].removed ? kAction1 : kAction2;
					m_states[n].select = false;
					m_states[n].removed = (m_buttonAction == kAction2);
				}

				if(m_states[n].select || button == CM_BT_NONE)
				{
					m_moveRect = true;
				}
				break;
			case kNumTabs:
				MPT_ASSERT_NOTREACHED();
				break;
			}
		}

		InvalidateRect(invalidate, false);
	} else
	{
		InvalidateRect(m_drawableArea, false);
	}
}


void CChannelManagerDlg::OnLButtonDblClk(uint32 nFlags, mfcless::Point point)
{
	OnLButtonDown(nFlags, point);
	DialogBase::OnLButtonDblClk(nFlags, point);
}

void CChannelManagerDlg::OnRButtonDblClk(uint32 nFlags, mfcless::Point point)
{
	OnRButtonDown(nFlags, point);
	DialogBase::OnRButtonDblClk(nFlags, point);
}


OPENMPT_NAMESPACE_END
