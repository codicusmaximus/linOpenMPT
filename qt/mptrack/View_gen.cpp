/*
 * View_gen.cpp
 * ------------
 * Purpose: General tab, lower panel.
 * Notes  : mfcless twin of upstream's mptrack/View_gen.cpp.
 *          Draft from qt/tools/twin.py, not yet edited by hand.
 * Authors: Olivier Lapicque
 *          OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "View_gen.h"
#include "mfcless/Widgets.h"
#include "AbstractVstEditor.h"
#include "Childfrm.h"
#include "Ctrl_gen.h"
#include "EffectVis.h"
#include "Globals.h"
#include "InputHandler.h"
#include "Mainfrm.h"
#include "Moddoc.h"
#include "MoveFXSlotDialog.h"
#include "Reporting.h"
#include "resource.h"
#include "SelectPluginDialog.h"
#include "WindowMessages.h"
#include "../common/mptStringBuffer.h"
#include "../soundlib/mod_specifications.h"
#include "../soundlib/plugins/PlugInterface.h"

// This is used for retrieving the correct background colour for the
// frames on the general tab when using WinXP Luna or Vista/Win7 Aero.


OPENMPT_NAMESPACE_BEGIN


void CViewGlobals::RegisterMessageMap()
{
	DialogBase::RegisterMessageMap();
	// ON_WM_SIZE() (mfcless::Window)
	AddScroll(Qt::Horizontal, &CViewGlobals::OnHScroll);
	AddScroll(Qt::Vertical, &CViewGlobals::OnVScroll);
	// ON_WM_DESTROY(): by hand (Qt event override)
	AddMessage(WM_MOD_MDIACTIVATE, &CViewGlobals::OnMDIDeactivate);
	AddMessage(WM_MOD_MDIDEACTIVATE, &CViewGlobals::OnMDIDeactivate);
	AddCommand(IDC_CHECK1, &CViewGlobals::OnMute1);
	AddCommand(IDC_CHECK3, &CViewGlobals::OnMute2);
	AddCommand(IDC_CHECK5, &CViewGlobals::OnMute3);
	AddCommand(IDC_CHECK7, &CViewGlobals::OnMute4);
	AddCommand(IDC_CHECK2, &CViewGlobals::OnSurround1);
	AddCommand(IDC_CHECK4, &CViewGlobals::OnSurround2);
	AddCommand(IDC_CHECK6, &CViewGlobals::OnSurround3);
	AddCommand(IDC_CHECK8, &CViewGlobals::OnSurround4);
	AddCommand(IDC_BUTTON9, &CViewGlobals::OnEditColor1);
	AddCommand(IDC_BUTTON10, &CViewGlobals::OnEditColor2);
	AddCommand(IDC_BUTTON11, &CViewGlobals::OnEditColor3);
	AddCommand(IDC_BUTTON12, &CViewGlobals::OnEditColor4);
	AddNotify(IDC_EDIT1, mfcless::Notify::EditUpdate, &CViewGlobals::OnEditVol1);
	AddNotify(IDC_EDIT3, mfcless::Notify::EditUpdate, &CViewGlobals::OnEditVol2);
	AddNotify(IDC_EDIT5, mfcless::Notify::EditUpdate, &CViewGlobals::OnEditVol3);
	AddNotify(IDC_EDIT7, mfcless::Notify::EditUpdate, &CViewGlobals::OnEditVol4);
	AddNotify(IDC_EDIT2, mfcless::Notify::EditUpdate, &CViewGlobals::OnEditPan1);
	AddNotify(IDC_EDIT4, mfcless::Notify::EditUpdate, &CViewGlobals::OnEditPan2);
	AddNotify(IDC_EDIT6, mfcless::Notify::EditUpdate, &CViewGlobals::OnEditPan3);
	AddNotify(IDC_EDIT8, mfcless::Notify::EditUpdate, &CViewGlobals::OnEditPan4);
	AddNotify(IDC_EDIT9, mfcless::Notify::EditUpdate, &CViewGlobals::OnEditName1);
	AddNotify(IDC_EDIT10, mfcless::Notify::EditUpdate, &CViewGlobals::OnEditName2);
	AddNotify(IDC_EDIT11, mfcless::Notify::EditUpdate, &CViewGlobals::OnEditName3);
	AddNotify(IDC_EDIT12, mfcless::Notify::EditUpdate, &CViewGlobals::OnEditName4);
	AddNotify(IDC_COMBO1, mfcless::Notify::SelChange, &CViewGlobals::OnFx1Changed);
	AddNotify(IDC_COMBO2, mfcless::Notify::SelChange, &CViewGlobals::OnFx2Changed);
	AddNotify(IDC_COMBO3, mfcless::Notify::SelChange, &CViewGlobals::OnFx3Changed);
	AddNotify(IDC_COMBO4, mfcless::Notify::SelChange, &CViewGlobals::OnFx4Changed);
	// Plugins
	AddCommand(IDC_CHECK9, &CViewGlobals::OnMixModeChanged);
	AddCommand(IDC_CHECK10, &CViewGlobals::OnBypassChanged);
	AddCommand(IDC_CHECK11, &CViewGlobals::OnDryMixChanged);
	AddCommand(IDC_BUTTON1, &CViewGlobals::OnSelectPlugin);
	AddCommand(IDC_DELPLUGIN, &CViewGlobals::OnRemovePlugin);
	AddCommand(IDC_BUTTON2, &CViewGlobals::OnEditPlugin);
	AddCommand(IDC_BUTTON4, &CViewGlobals::OnNextPlugin);
	AddCommand(IDC_BUTTON5, &CViewGlobals::OnPrevPlugin);
	AddCommand(IDC_MOVEFXSLOT, &CViewGlobals::OnMovePlugToSlot);
	AddCommand(IDC_INSERTFXSLOT, &CViewGlobals::OnInsertSlot);
	AddCommand(IDC_CLONEPLUG, &CViewGlobals::OnClonePlug);
	AddCommand(IDC_BUTTON6, &CViewGlobals::OnLoadParam);
	AddCommand(IDC_BUTTON8, &CViewGlobals::OnSaveParam);
	AddNotify(IDC_EDIT13, mfcless::Notify::EditUpdate, &CViewGlobals::OnPluginNameChanged);
	AddNotify(IDC_EDIT14, mfcless::Notify::EditUpdate, &CViewGlobals::OnSetParameter);
	AddNotify(IDC_EDIT14, mfcless::Notify::SetFocus, &CViewGlobals::OnFocusParam);
	AddNotify(IDC_EDIT14, mfcless::Notify::KillFocus, &CViewGlobals::OnParamChanged);
	AddNotify(IDC_COMBO5, mfcless::Notify::SelChange, &CViewGlobals::OnPluginChanged);
	AddNotify(IDC_COMBO6, mfcless::Notify::SelChange, &CViewGlobals::OnParamChanged);
	AddNotify(IDC_COMBO6, mfcless::Notify::SetFocus, &CViewGlobals::OnFillParamCombo);
	AddNotify(IDC_COMBO7, mfcless::Notify::SelChange, &CViewGlobals::OnOutputRoutingChanged);
	AddNotify(IDC_COMBO8, mfcless::Notify::SelChange, &CViewGlobals::OnProgramChanged);
	AddNotify(IDC_COMBO8, mfcless::Notify::SetFocus, &CViewGlobals::OnFillProgramCombo);
	AddCommand(IDC_CHECK12, &CViewGlobals::OnWetDryExpandChanged);
	AddCommand(IDC_CHECK13, &CViewGlobals::OnAutoSuspendChanged);
	AddNotify(IDC_COMBO9, mfcless::Notify::SelChange, &CViewGlobals::OnSpecialMixProcessingChanged);
	AddNotify(IDC_TABCTRL1, mfcless::Notify::SelChange, &CViewGlobals::OnTabSelchange);
	AddMessage(WM_MOD_UNLOCKCONTROLS, &CViewGlobals::OnUnlockControls);
	AddMessage(WM_MOD_VIEWMSG, &CViewGlobals::OnModViewMsg);
	AddMessage(WM_MOD_MIDIMSG, &CViewGlobals::OnMidiMsg);
	AddMessage(WM_MOD_PLUGPARAMAUTOMATE, &CViewGlobals::OnParamAutomated);
	AddMessage(WM_MOD_PLUGINDRYWETRATIOCHANGED, &CViewGlobals::OnDryWetRatioChangedFromPlayer);
	AddCommand(ID_EDIT_UNDO, &CViewGlobals::OnEditUndo);
	AddCommand(ID_EDIT_REDO, &CViewGlobals::OnEditRedo);
	AddCommandUpdate(ID_EDIT_UNDO, &CViewGlobals::OnUpdateUndo);
	AddCommandUpdate(ID_EDIT_REDO, &CViewGlobals::OnUpdateRedo);
	// ON_NOTIFY_EX_RANGE(TTN_NEEDTEXT, 0, 0xFFFF, &CViewGlobals::OnToolTipText): by hand (Qt event override)
}


void CViewGlobals::DoDataExchange(DataExchange &dx)
{
	DialogBase::DoDataExchange(dx);
	//{{AFX_DATA_MAP(CViewGlobals)
	ExchangeControl(dx, IDC_TABCTRL1, m_TabCtrl);
	ExchangeControl(dx, IDC_COMBO1, m_CbnEffects[0]);
	ExchangeControl(dx, IDC_COMBO2, m_CbnEffects[1]);
	ExchangeControl(dx, IDC_COMBO3, m_CbnEffects[2]);
	ExchangeControl(dx, IDC_COMBO4, m_CbnEffects[3]);
	ExchangeControl(dx, IDC_COMBO5, m_CbnPlugin);
	ExchangeControl(dx, IDC_COMBO6, m_CbnParam);
	ExchangeControl(dx, IDC_COMBO7, m_CbnOutput);

	ExchangeControl(dx, IDC_COMBO8, m_CbnPreset);
	ExchangeControl(dx, IDC_COMBO9, m_CbnSpecialMixProcessing);
	ExchangeControl(dx, IDC_SPIN10, m_SpinMixGain);

	ExchangeControl(dx, IDC_SLIDER1, m_sbVolume[0]);
	ExchangeControl(dx, IDC_SLIDER2, m_sbPan[0]);
	ExchangeControl(dx, IDC_SLIDER3, m_sbVolume[1]);
	ExchangeControl(dx, IDC_SLIDER4, m_sbPan[1]);
	ExchangeControl(dx, IDC_SLIDER5, m_sbVolume[2]);
	ExchangeControl(dx, IDC_SLIDER6, m_sbPan[2]);
	ExchangeControl(dx, IDC_SLIDER7, m_sbVolume[3]);
	ExchangeControl(dx, IDC_SLIDER8, m_sbPan[3]);
	ExchangeControl(dx, IDC_SLIDER9, m_sbValue);
	ExchangeControl(dx, IDC_SLIDER10, m_sbDryRatio);
	ExchangeControl(dx, IDC_SPIN1, m_spinVolume[0]);
	ExchangeControl(dx, IDC_SPIN2, m_spinPan[0]);
	ExchangeControl(dx, IDC_SPIN3, m_spinVolume[1]);
	ExchangeControl(dx, IDC_SPIN4, m_spinPan[1]);
	ExchangeControl(dx, IDC_SPIN5, m_spinVolume[2]);
	ExchangeControl(dx, IDC_SPIN6, m_spinPan[2]);
	ExchangeControl(dx, IDC_SPIN7, m_spinVolume[3]);
	ExchangeControl(dx, IDC_SPIN8, m_spinPan[3]);
	ExchangeControl(dx, IDC_BUTTON1, m_BtnSelect);
	ExchangeControl(dx, IDC_BUTTON2, m_BtnEdit);
	ExchangeControl(dx, IDC_BUTTON4, m_nextPluginButton);
	ExchangeControl(dx, IDC_BUTTON5, m_prevPluginButton);
	//}}AFX_DATA_MAP
}


CViewGlobals::CViewGlobals() : CFormView{IDD_VIEW_GLOBALS}
{
	m_prevPluginButton->SetAccessibleText(UL_("Previous Plugin"));
	m_nextPluginButton->SetAccessibleText(UL_("Next Plugin"));
}


CModDoc* CViewGlobals::GetDocument() const { return static_cast<CModDoc *>(m_pDocument); }

void CViewGlobals::OnInitialUpdate()
{
	CChildFrame *pFrame = (CChildFrame *)GetParentFrame();
	int nMapMode = MM_TEXT;
	mfcless::Size sizeTotal, sizePage, sizeLine;

	m_nActiveTab = CHANNELINDEX(-1);
	m_nCurrentPlugin = 0;
	m_nCurrentParam = 0;
	DialogBase::OnInitialUpdate();
	EnableToolTips();

	if (pFrame)
	{
		GeneralViewState &generalState = pFrame->GetGeneralViewState();
		if (generalState.initialized)
		{
			m_TabCtrl->SetCurSel(generalState.nTab);
			m_nActiveTab = generalState.nTab;
			m_nCurrentPlugin = generalState.nPlugin;
			m_nCurrentParam = generalState.nParam;
		}
	}
	GetDeviceScrollSizes(nMapMode, sizeTotal, sizePage, sizeLine);
	m_rcClient.SetRect(0, 0, sizeTotal.cx, sizeTotal.cy);
	RecalcLayout();

	// Initializing scroll ranges
	for(int ichn = 0; ichn < CHANNELS_IN_TAB; ichn++)
	{
		// Color select
		SubclassDlgItem(IDC_BUTTON9 + ichn, m_channelColor[ichn]);
		// Volume Slider
		m_sbVolume[ichn].SetRange(0, 64);
		m_sbVolume[ichn].SetTicFreq(8);
		// Pan Slider
		m_sbPan[ichn].SetRange(0, 64);
		m_sbPan[ichn].SetTicFreq(8);
		// Volume Spin
		m_spinVolume[ichn].SetRange(0, 64);
		// Pan Spin
		m_spinPan[ichn].SetRange(0, 256);
	}
	m_sbValue->setValue(0);
	m_sbValue->setRange(0, 100);

	m_sbValue->setValue(0);
	m_sbValue->setRange(0, 100);

	mfcless::AddItem(m_CbnSpecialMixProcessing, UL_("Default"));
	mfcless::AddItem(m_CbnSpecialMixProcessing, UL_("Wet Subtract"));
	mfcless::AddItem(m_CbnSpecialMixProcessing, UL_("Dry Subtract"));
	mfcless::AddItem(m_CbnSpecialMixProcessing, UL_("Mix Subtract"));
	mfcless::AddItem(m_CbnSpecialMixProcessing, UL_("Middle Subtract"));
	mfcless::AddItem(m_CbnSpecialMixProcessing, UL_("LR Balance"));
	mfcless::AddItem(m_CbnSpecialMixProcessing, UL_("Instrument"));
	m_SpinMixGain->SetRange(0, 80);
	m_SpinMixGain->SetPos(10);
	SetItemText(IDC_EDIT16, UL_("Gain: x1.0"));

	UpdateView(UpdateHint().ModType());
	OnParamChanged();
	m_nLockCount = 0;

	// Use tab background color rather than regular dialog background color (required for Aero/etc. where they are not the same color)
	EnableThemeDialogTexture(this, ETDT_ENABLETAB);
}


void CViewGlobals::OnDestroy()
{
	CChildFrame *pFrame = (CChildFrame *)GetParentFrame();
	if (pFrame)
	{
		GeneralViewState &generalState = pFrame->GetGeneralViewState();
		generalState.initialized = true;
		generalState.nTab = m_nActiveTab;
		generalState.nPlugin = m_nCurrentPlugin;
		generalState.nParam = m_nCurrentParam;
	}
	DialogBase::OnDestroy();
}


intptr_t CViewGlobals::OnMDIDeactivate(uintptr_t, intptr_t)
{
	// Create new undo point if we switch to / from other window
	m_lastEdit = CHANNELINDEX_INVALID;
	return 0;
}


intptr_t CViewGlobals::OnMidiMsg(uintptr_t midiData_, intptr_t)
{
	uint32 midiData = static_cast<uint32>(midiData_);
	// Handle MIDI messages assigned to shortcuts
	CInputHandler *ih = CMainFrame::GetInputHandler();
	if(ih->HandleMIDIMessage(kCtxViewGeneral, midiData) == kcNull)
		ih->HandleMIDIMessage(kCtxAllContexts, midiData);
	return 1;
}


void CViewGlobals::RecalcLayout()
{
	if (m_TabCtrl != NULL)
	{
		mfcless::Rect rect;
		GetClientRect(&rect);
		if (rect.right < m_rcClient.right) rect.right = m_rcClient.right;
		if (rect.bottom < m_rcClient.bottom) rect.bottom = m_rcClient.bottom;
		m_TabCtrl->SetWindowPos(&mfcless::Window<QWidget>::wndBottom, 0,0, rect.right, rect.bottom, mfcless::swp::NoMove);
	}
}


void CViewGlobals::UnlockControls() { PostMessage(WM_MOD_UNLOCKCONTROLS); }


int CViewGlobals::GetDlgItemIntEx(uint32 nID)
{
	mpt::ustring s;
	s = GetItemText(nID);
	if(s.GetLength() < 1 || s[0] < u8'0' || s[0] > u8'9') return -1;
	return _ttoi(s);
}


void CViewGlobals::OnSize(uint32 nType, int cx, int cy)
{
	Window::OnSize(nType, cx, cy);
	if (((nType == mfcless::size::Restored) || (nType == mfcless::size::Maximized)) && (cx > 0) && (cy > 0) && (this))
	{
		RecalcLayout();
	}
}


void CViewGlobals::OnUpdate(mfcless::DocumentView *pView, intptr_t lHint, QObject *pHint)
{
	if (pView != this) UpdateView(UpdateHint::FromLPARAM(lHint), pHint);
}


void CViewGlobals::UpdateView(UpdateHint hint, QObject *pObject)
{
	const CModDoc *pModDoc = GetDocument();
	int nTabCount, nTabIndex;

	if(!pModDoc || pObject == this)
		return;
	const CSoundFile &sndFile = pModDoc->GetSoundFile();
	const GeneralHint genHint = hint.ToType<GeneralHint>();
	const PluginHint plugHint = hint.ToType<PluginHint>();
	if(!genHint.GetType()[HINT_MODTYPE | HINT_MODCHANNELS]
		&& !plugHint.GetType()[HINT_MIXPLUGINS | HINT_PLUGINNAMES | HINT_PLUGINPARAM])
	{
		return;
	}
	FlagSet<HintType> hintType = hint.GetType();
	const bool updateAll = hintType[HINT_MODTYPE];
	const auto updateChannel = genHint.GetChannel();
	const int updateTab = (updateChannel < sndFile.GetNumChannels()) ? (updateChannel / CHANNELS_IN_TAB) : m_nActiveTab;
	if(genHint.GetType()[HINT_MODCHANNELS] && updateTab != m_nActiveTab)
		return;

	mpt::ustring s;
	nTabCount = (sndFile.GetNumChannels() + (CHANNELS_IN_TAB - 1)) / CHANNELS_IN_TAB;
	if (nTabCount != m_TabCtrl->GetItemCount())
	{
		uint32 nOldSel = m_TabCtrl->GetCurSel();
		if (!m_TabCtrl->GetItemCount()) nOldSel = m_nActiveTab;
		m_TabCtrl->setUpdatesEnabled(false);
		m_TabCtrl->DeleteAllItems();
		for (int iItem=0; iItem<nTabCount; iItem++)
		{
			const int lastItem = std::min(iItem * CHANNELS_IN_TAB + CHANNELS_IN_TAB, static_cast<int>(MAX_BASECHANNELS));
			s = MPT_UFORMAT("{} - {}")(iItem * CHANNELS_IN_TAB + 1, lastItem);
			TC_ITEM tci;
			tci.mask = TCIF_TEXT | TCIF_PARAM;
			tci.pszText = const_cast<mpt::uchar *>(s.c_str());
			tci.lParam = iItem * CHANNELS_IN_TAB;
			m_TabCtrl->InsertItem(iItem, &tci);
		}
		if (nOldSel >= (uint32)nTabCount) nOldSel = 0;

		m_TabCtrl->setUpdatesEnabled(true);
		m_TabCtrl->SetCurSel(nOldSel);

		InvalidateRect(NULL, false);
	}
	nTabIndex = m_TabCtrl->GetCurSel();
	if ((nTabIndex < 0) || (nTabIndex >= nTabCount)) return; // ???

	if (m_nActiveTab != nTabIndex || genHint.GetType()[HINT_MODTYPE | HINT_MODCHANNELS])
	{
		LockControls();
		m_nActiveTab = static_cast<CHANNELINDEX>(nTabIndex);
		for (CHANNELINDEX ichn = 0; ichn < CHANNELS_IN_TAB; ichn++)
		{
			const CHANNELINDEX nChn = m_nActiveTab * CHANNELS_IN_TAB + ichn;
			const bool bEnable = (nChn < sndFile.GetNumChannels()) ? true : false;
			if(nChn < sndFile.GetNumChannels())
			{
				const auto &chnSettings = sndFile.ChnSettings[nChn];
				// Text
				if(bEnable)
					s = MPT_UFORMAT("Channel {}")(nChn + 1);
				else
					s = UL_("");
				SetItemText(IDC_TEXT1 + ichn, s);
				// Channel color
				m_channelColor[ichn].SetColor(chnSettings.color);
				m_channelColor[ichn].EnableWindow(bEnable);
				// Mute
				SetChecked(IDC_CHECK1 + ichn * 2, chnSettings.dwFlags[CHN_MUTE] ? true : false);
				// Surround
				SetChecked(IDC_CHECK2 + ichn * 2, chnSettings.dwFlags[CHN_SURROUND] ? true : false);
				// Volume
				int vol = chnSettings.nVolume;
				m_sbVolume[ichn].SetPos(vol);
				m_sbVolume[ichn].Invalidate(false);
				SetItemInt(IDC_EDIT1+ichn*2, vol);
				// Pan
				int pan = chnSettings.nPan;
				m_sbPan[ichn].SetPos(pan/4);
				m_sbPan[ichn].Invalidate(false);
				SetItemInt(IDC_EDIT2+ichn*2, pan);

				// Channel name
				s = mpt::ToUnicode(sndFile.GetCharsetInternal(), chnSettings.szName);
				SetItemText(IDC_EDIT9 + ichn, s);
				((CEdit*)(GetDlgItem(IDC_EDIT9 + ichn)))->LimitText(MAX_CHANNELNAME - 1);
			} else
			{
				SetItemText(IDC_TEXT1 + ichn, UL_(""));
				SetItemText(IDC_EDIT9 + ichn, UL_(""));
				m_channelColor[ichn].EnableWindow(false);
			}

			// Enable/Disable controls for this channel
			bool bIT = ((bEnable) && (sndFile.GetType() & (MOD_TYPE_IT|MOD_TYPE_MPT)));
			EnableItem(IDC_CHECK1 + ichn * 2, bEnable);
			EnableItem(IDC_CHECK2 + ichn * 2, bIT);

			m_sbVolume[ichn].EnableWindow(bIT);
			m_spinVolume[ichn].EnableWindow(bIT);

			m_sbPan[ichn].EnableWindow(bEnable && !(sndFile.GetType() & (MOD_TYPE_XM|MOD_TYPE_MOD)));
			m_spinPan[ichn].EnableWindow(bEnable && !(sndFile.GetType() & (MOD_TYPE_XM|MOD_TYPE_MOD)));
			EnableItem(IDC_EDIT1 + ichn * 2, bIT);	// channel vol
			EnableItem(IDC_EDIT2 + ichn * 2, bEnable && !(sndFile.GetType() & (MOD_TYPE_XM|MOD_TYPE_MOD)));	// channel pan
			EnableItem(IDC_EDIT9 + ichn, ((bEnable) && (sndFile.GetType() & (MOD_TYPE_XM|MOD_TYPE_IT|MOD_TYPE_MPT))));	// channel name
			m_CbnEffects[ichn].EnableWindow(bEnable & (sndFile.GetModSpecifications().supportsPlugins ? true : false));
		}
		UnlockControls();
	}

	// Update plugin names
	if(genHint.GetType()[HINT_MODTYPE | HINT_MODCHANNELS] || plugHint.GetType()[HINT_PLUGINNAMES])
	{
		PopulateChannelPlugins(hint, pObject);
		if(plugHint.GetType()[HINT_MODTYPE | HINT_PLUGINNAMES])
		{
			m_CbnPlugin->Update(PluginComboBox::Config{PluginComboBox::ShowEmptySlots | PluginComboBox::ShowLibraryNames}.Hint(plugHint, pObject).CurrentSelection(m_nCurrentPlugin), sndFile);
			SetItemText(IDC_EDIT13, mpt::ToUnicode(sndFile.m_MixPlugins[m_nCurrentPlugin].GetName()));
		}
	}
	// Update plugin info
	const SNDMIXPLUGIN &plugin = sndFile.m_MixPlugins[m_nCurrentPlugin];
	const bool updatePlug = (plugHint.GetPlugin() == 0 || plugHint.GetPlugin() == m_nCurrentPlugin + 1);
	const bool updateWholePluginView = updateAll || (plugHint.GetType()[HINT_MIXPLUGINS] && updatePlug);
	if(updateWholePluginView)
	{
		if(m_nCurrentPlugin >= MAX_MIXPLUGINS)
			m_nCurrentPlugin = 0;
		m_CbnPlugin->SetSelection(m_nCurrentPlugin);
		SetItemText(IDC_EDIT13, mpt::ToUnicode(plugin.GetName()));
		SetChecked(IDC_CHECK9, plugin.IsMasterEffect() ? 1 : 0);
		SetChecked(IDC_CHECK10, plugin.IsBypassed() ? 1 : 0);
		SetChecked(IDC_CHECK11, plugin.IsDryMix() ? 1 : 0);
		SetChecked(IDC_CHECK13, plugin.IsAutoSuspendable() ? 1 : 0);
		IMixPlugin *pPlugin = plugin.pMixPlugin;
		m_BtnEdit->setEnabled((pPlugin != nullptr && (pPlugin->HasEditor() || pPlugin->GetNumVisibleParameters())) ? true : false);
		EnableItem(IDC_MOVEFXSLOT, (pPlugin) ? true : false);
		EnableItem(IDC_INSERTFXSLOT, (pPlugin) ? true : false);
		EnableItem(IDC_CLONEPLUG, (pPlugin) ? true : false);
		EnableItem(IDC_DELPLUGIN, (plugin.IsValidPlugin() || !plugin.Info.szLibraryName.empty() || !plugin.Info.szName.empty()) ? true : false);
		UpdateDryWetDisplay();

		m_CbnSpecialMixProcessing->setCurrentIndex(static_cast<int>(plugin.GetMixMode()));
		SetChecked(IDC_CHECK12, plugin.IsExpandedMix() ? 1 : 0);
		int gain = plugin.GetGain();
		if(gain == 0) gain = 10;
		float value = 0.1f * (float)gain;
		s = mfcless::PrintF(UL_("Gain: x%1.1f"), value);
		SetItemText(IDC_EDIT16, s);
		m_SpinMixGain->SetPos(gain);

		if (pPlugin)
		{
			const PlugParamIndex nParams = pPlugin->GetNumVisibleParameters();
			m_CbnParam->setUpdatesEnabled(false);
			m_CbnParam->clear();
			if (m_nCurrentParam >= nParams) m_nCurrentParam = 0;

			if(nParams)
			{
				mfcless::SetItemData(m_CbnParam, mfcless::AddItem(m_CbnParam, pPlugin->GetFormattedParamName(m_nCurrentParam)), m_nCurrentParam);
			}

			m_CbnParam->setCurrentIndex(0);
			m_CbnParam->setUpdatesEnabled(true);
			OnParamChanged();

			// Input / Output type
			int in = pPlugin->GetNumInputChannels(), out = pPlugin->GetNumOutputChannels();
			if (in < 1) s = UL_("No input");
			else if (in == 1) s = UL_("Mono-In");
			else s = UL_("Stereo-In");
			s += UL_(", ");
			if (out < 1) s += UL_("No output");
			else if (out == 1) s += UL_("Mono-Out");
			else s += UL_("Stereo-Out");

			// For now, only display the "current" preset.
			// This prevents the program from hanging when switching between plugin slots or
			// switching to the general tab and the first plugin in the list has a lot of presets.
			// Some plugins like Synth1 have so many presets that this *does* indeed make a difference,
			// even on fairly modern CPUs. The rest of the presets are just added when the combo box
			// gets the focus, i.e. just when they're needed.
			int32 currentProg = pPlugin->GetCurrentProgram();
			FillPluginProgramBox(currentProg, currentProg);
			m_CbnPreset->setCurrentIndex(0);

			m_sbValue->setEnabled(true);
			m_sbDryRatio->setEnabled(true);
			EnableItem(IDC_EDIT14, true);
		} else
		{
			s.Empty();
			if (m_CbnParam->count() > 0) m_CbnParam->clear();
			m_nCurrentParam = 0;

			m_CbnPreset->setUpdatesEnabled(false);
			m_CbnPreset->clear();
			mfcless::SetItemData(m_CbnPreset, mfcless::AddItem(m_CbnPreset, UL_("none")), 0);
			m_CbnPreset->setUpdatesEnabled(true);
			m_CbnPreset->setCurrentIndex(0);
			m_sbValue->setEnabled(false);
			m_sbDryRatio->setEnabled(false);
			EnableItem(IDC_EDIT14, false);
		}
		SetItemText(IDC_TEXT6, s);
	}
	
	if(updateWholePluginView || plugHint.GetPlugin() > m_nCurrentPlugin)
	{
		int insertAt = 1;
		m_CbnOutput->setUpdatesEnabled(false);
		if(updateWholePluginView)
		{
			m_CbnOutput->clear();
			mfcless::SetItemData(m_CbnOutput, mfcless::AddItem(m_CbnOutput, UL_("Default")), 0);

			for(PLUGINDEX i = m_nCurrentPlugin + 1; i < MAX_MIXPLUGINS; i++)
			{
				if(!sndFile.m_MixPlugins[i].IsValidPlugin())
				{
					mfcless::SetItemData(m_CbnOutput, mfcless::AddItem(m_CbnOutput, UL_("New Plugin...")), 1);
					insertAt = 2;
					break;
				}
			}
		} else
		{
			const uintptr_t changedPlugin = 0x80 + (plugHint.GetPlugin() - 1);
			const int items = m_CbnOutput->count();
			for(insertAt = 1; insertAt < items; insertAt++)
			{
				uintptr_t thisPlugin = mfcless::GetItemData(m_CbnOutput, insertAt);
				if(thisPlugin == changedPlugin)
					m_CbnOutput->removeItem(insertAt);
				if(thisPlugin >= changedPlugin)
					break;
			}
		}

		int outputSel = plugin.IsOutputToMaster() ? 0 : -1;
		for(PLUGINDEX iOut = m_nCurrentPlugin + 1; iOut < MAX_MIXPLUGINS; iOut++)
		{
			if(!updateWholePluginView && (iOut + 1) != plugHint.GetPlugin())
				continue;
			const SNDMIXPLUGIN &outPlug = sndFile.m_MixPlugins[iOut];
			if(outPlug.IsValidPlugin())
			{
				const auto name = outPlug.GetName(), libName = outPlug.GetLibraryName();
				s = mfcless::PrintF(UL_("FX%d: "), iOut + 1);
				s += mpt::ToUnicode(name.empty() ? libName : name);
				if(!name.empty() && libName != name)
				{
					s += UL_(" (");
					s += mpt::ToUnicode(libName);
					s += UL_(")");
				}

				insertAt = mfcless::InsertItem(m_CbnOutput, insertAt, s);
				mfcless::SetItemData(m_CbnOutput, insertAt, 0x80 + iOut);
				if(!plugin.IsOutputToMaster() && (plugin.GetOutputPlugin() == iOut))
				{
					outputSel = insertAt;
				}
				insertAt++;
			}
		}
		m_CbnOutput->setUpdatesEnabled(true);
		if(outputSel >= 0)
			m_CbnOutput->setCurrentIndex(outputSel);
	}
	if(plugHint.GetType()[HINT_PLUGINPARAM] && updatePlug)
	{
		OnParamChanged();
	}

	m_CbnPlugin->update(false);
	m_CbnParam->update(false);
	m_CbnPreset->update(false);
	m_CbnSpecialMixProcessing->update(false);
	m_CbnOutput->update(false);
}


void CViewGlobals::PopulateChannelPlugins(UpdateHint hint, const QObject *pObj)
{
	const CSoundFile &sndFile = GetDocument()->GetSoundFile();
	for(CHANNELINDEX ichn = 0; ichn < CHANNELS_IN_TAB; ichn++)
	{
		if(const CHANNELINDEX nChn = m_nActiveTab * CHANNELS_IN_TAB + ichn; nChn < sndFile.GetNumChannels())
		{
			PLUGINDEX sel = sndFile.ChnSettings[nChn].nMixPlugin ? sndFile.ChnSettings[nChn].nMixPlugin - 1 : PLUGINDEX_INVALID;
			m_CbnEffects[ichn].Update(PluginComboBox::Config{PluginComboBox::ShowNoPlugin}.Hint(hint, pObj).CurrentSelection(sel), sndFile);
		}
	}
}


IMixPlugin *CViewGlobals::GetCurrentPlugin() const
{
	if(GetDocument() == nullptr || m_nCurrentPlugin >= MAX_MIXPLUGINS)
	{
		return nullptr;
	}

	return GetDocument()->GetSoundFile().m_MixPlugins[m_nCurrentPlugin].pMixPlugin;
}


void CViewGlobals::OnTabSelchange()
{
	UpdateView(GeneralHint().Channels());
	if (pResult) CurrentNotify().result = 0;
}


void CViewGlobals::OnUpdateUndo(mfcless::CommandState &state)
{
	CModDoc *pModDoc = GetDocument();
	if((pCmdUI) && (pModDoc))
	{
		state.Enable(pModDoc->GetPatternUndo().CanUndoChannelSettings());
		state.SetText(CMainFrame::GetInputHandler()->GetKeyTextFromCommand(kcEditUndo, UL_("Undo ") + pModDoc->GetPatternUndo().GetUndoName()));
	}
}


void CViewGlobals::OnUpdateRedo(mfcless::CommandState &state)
{
	CModDoc *pModDoc = GetDocument();
	if((pCmdUI) && (pModDoc))
	{
		state.Enable(pModDoc->GetPatternUndo().CanRedoChannelSettings());
		state.SetText(CMainFrame::GetInputHandler()->GetKeyTextFromCommand(kcEditRedo, UL_("Redo ") + pModDoc->GetPatternUndo().GetRedoName()));
	}
}


void CViewGlobals::OnEditUndo()
{
	UndoRedo(true);
}


void CViewGlobals::OnEditRedo()
{
	UndoRedo(false);
}


void CViewGlobals::UndoRedo(bool undo)
{
	CModDoc *pModDoc = GetDocument();
	if(!pModDoc)
		return;
	if(undo && pModDoc->GetPatternUndo().CanUndoChannelSettings())
		pModDoc->GetPatternUndo().Undo();
	else if(!undo && pModDoc->GetPatternUndo().CanRedoChannelSettings())
		pModDoc->GetPatternUndo().Redo();
}


void CViewGlobals::PrepareUndo(CHANNELINDEX chnMod4)
{
	if(m_lastEdit != chnMod4)
	{
		// Backup old channel settings through pattern undo.
		m_lastEdit = chnMod4;
		const CHANNELINDEX chn = static_cast<CHANNELINDEX>(m_nActiveTab * CHANNELS_IN_TAB) + chnMod4;
		GetDocument()->GetPatternUndo().PrepareChannelUndo(chn, 1, "Channel Settings");
	}
}


void CViewGlobals::OnEditColor(const CHANNELINDEX chnMod4)
{
	auto *modDoc = GetDocument();
	auto &sndFile = modDoc->GetSoundFile();
	const CHANNELINDEX chn = static_cast<CHANNELINDEX>(m_nActiveTab * CHANNELS_IN_TAB) + chnMod4;
	if(auto color = m_channelColor[chnMod4].PickColor(sndFile, chn); color.has_value())
	{
		PrepareUndo(chnMod4);
		sndFile.ChnSettings[chn].color = *color;
		if(modDoc->SupportsChannelColors())
			modDoc->SetModified();
		modDoc->UpdateAllViews(nullptr, GeneralHint(chn).Channels());
	}
}


void CViewGlobals::OnEditColor1() { OnEditColor(0); }
void CViewGlobals::OnEditColor2() { OnEditColor(1); }
void CViewGlobals::OnEditColor3() { OnEditColor(2); }
void CViewGlobals::OnEditColor4() { OnEditColor(3); }


void CViewGlobals::OnMute(const CHANNELINDEX chnMod4, const uint32 itemID)
{
	CModDoc *pModDoc = GetDocument();

	if (pModDoc)
	{
		const bool b = (IsChecked(itemID) != false);
		const CHANNELINDEX nChn = (CHANNELINDEX)(m_nActiveTab * CHANNELS_IN_TAB) + chnMod4;
		pModDoc->MuteChannel(nChn, b);
		pModDoc->UpdateAllViews(this, GeneralHint(nChn).Channels());
	}
}

void CViewGlobals::OnMute1() {OnMute(0, IDC_CHECK1);}
void CViewGlobals::OnMute2() {OnMute(1, IDC_CHECK3);}
void CViewGlobals::OnMute3() {OnMute(2, IDC_CHECK5);}
void CViewGlobals::OnMute4() {OnMute(3, IDC_CHECK7);}


void CViewGlobals::OnSurround(const CHANNELINDEX chnMod4, const uint32 itemID)
{
	CModDoc *pModDoc = GetDocument();

	if (pModDoc)
	{
		PrepareUndo(chnMod4);
		const bool b = (IsChecked(itemID) != false);
		const CHANNELINDEX nChn = (CHANNELINDEX)(m_nActiveTab * CHANNELS_IN_TAB) + chnMod4;
		pModDoc->SurroundChannel(nChn, b);
		pModDoc->UpdateAllViews(nullptr, GeneralHint(nChn).Channels());
	}
}

void CViewGlobals::OnSurround1() {OnSurround(0, IDC_CHECK2);}
void CViewGlobals::OnSurround2() {OnSurround(1, IDC_CHECK4);}
void CViewGlobals::OnSurround3() {OnSurround(2, IDC_CHECK6);}
void CViewGlobals::OnSurround4() {OnSurround(3, IDC_CHECK8);}

void CViewGlobals::OnEditVol(const CHANNELINDEX chnMod4, const uint32 itemID)
{
	CModDoc *pModDoc = GetDocument();
	const CHANNELINDEX nChn = (CHANNELINDEX)(m_nActiveTab * CHANNELS_IN_TAB) + chnMod4;
	const int vol = GetDlgItemIntEx(itemID);
	if ((pModDoc) && (vol >= 0) && (vol <= 64) && (!m_nLockCount))
	{
		PrepareUndo(chnMod4);
		if (pModDoc->SetChannelGlobalVolume(nChn, static_cast<uint16>(vol)))
		{
			m_sbVolume[chnMod4].SetPos(vol);
			pModDoc->UpdateAllViews(this, GeneralHint(nChn).Channels());
		}
	}
}

void CViewGlobals::OnEditVol1() {OnEditVol(0, IDC_EDIT1);}
void CViewGlobals::OnEditVol2() {OnEditVol(1, IDC_EDIT3);}
void CViewGlobals::OnEditVol3() {OnEditVol(2, IDC_EDIT5);}
void CViewGlobals::OnEditVol4() {OnEditVol(3, IDC_EDIT7);}


void CViewGlobals::OnEditPan(const CHANNELINDEX chnMod4, const uint32 itemID)
{
	CModDoc *pModDoc = GetDocument();
	const CHANNELINDEX nChn = (CHANNELINDEX)(m_nActiveTab * CHANNELS_IN_TAB) + chnMod4;
	const int pan = GetDlgItemIntEx(itemID);
	if ((pModDoc) && (pan >= 0) && (pan <= 256) && (!m_nLockCount))
	{
		PrepareUndo(chnMod4);
		if (pModDoc->SetChannelDefaultPan(nChn, static_cast<uint16>(pan)))
		{
			m_sbPan[chnMod4].SetPos(pan / 4);
			pModDoc->UpdateAllViews(this, GeneralHint(nChn).Channels());
			// Surround is forced off when changing pan, so uncheck the checkbox.
			SetChecked(IDC_CHECK2 + chnMod4 * 2, 0);
		}
	}
}


void CViewGlobals::OnEditPan1() {OnEditPan(0, IDC_EDIT2);}
void CViewGlobals::OnEditPan2() {OnEditPan(1, IDC_EDIT4);}
void CViewGlobals::OnEditPan3() {OnEditPan(2, IDC_EDIT6);}
void CViewGlobals::OnEditPan4() {OnEditPan(3, IDC_EDIT8);}


void CViewGlobals::OnHScroll(mfcless::ScrollCode code, int pos, QWidget *pScrollBar)
{
	DialogBase::OnHScroll(nSBCode, nPos, pScrollBar);

	CModDoc *pModDoc = GetDocument();
	const CHANNELINDEX nChn = (CHANNELINDEX)(m_nActiveTab * CHANNELS_IN_TAB);
	if(pModDoc && !IsLocked() && nChn < pModDoc->GetNumChannels())
	{
		bool bUpdate = false;
		short int pos;

		LockControls();
		const CHANNELINDEX nLoopLimit = std::min(static_cast<CHANNELINDEX>(CHANNELS_IN_TAB), static_cast<CHANNELINDEX>(pModDoc->GetSoundFile().GetNumChannels() - nChn));
		for (CHANNELINDEX iCh = 0; iCh < nLoopLimit; iCh++)
		{
			if(static_cast<QWidget*>(pScrollBar) == &m_sbVolume[iCh])
			{
				// Volume sliders
				pos = (short int)m_sbVolume[iCh].GetPos();
				if ((pos >= 0) && (pos <= 64))
				{
					PrepareUndo(iCh);
					if (pModDoc->SetChannelGlobalVolume(nChn + iCh, pos))
					{
						SetItemInt(IDC_EDIT1 + iCh * 2, pos);
						bUpdate = true;
					}
				}
			} else if(static_cast<QWidget*>(pScrollBar) == &m_sbPan[iCh])
			{
				// Pan sliders
				pos = (short int)m_sbPan[iCh].GetPos();
				if(pos >= 0 && pos <= 64 && (static_cast<uint16>(pos) != pModDoc->GetSoundFile().ChnSettings[nChn+iCh].nPan / 4u))
				{
					PrepareUndo(iCh);
					if (pModDoc->SetChannelDefaultPan(nChn + iCh, pos * 4))
					{
						SetItemInt(IDC_EDIT2 + iCh * 2, pos * 4);
						SetChecked(IDC_CHECK2 + iCh * 2, 0);
						bUpdate = true;
					}
				}
			}
		}

		if ((pScrollBar) && (pScrollBar == m_sbDryRatio))
		{
			int n = 100 - m_sbDryRatio->value();
			if ((n >= 0) && (n <= 100) && (m_nCurrentPlugin < MAX_MIXPLUGINS))
			{
				SNDMIXPLUGIN &plugin = pModDoc->GetSoundFile().m_MixPlugins[m_nCurrentPlugin];

				if(plugin.pMixPlugin)
				{
					plugin.fDryRatio = static_cast<float>(n) / 100.0f;
					SetPluginModified();
				}
				UpdateDryWetDisplay();
			}
		}

		if (bUpdate) pModDoc->UpdateAllViews(this, GeneralHint(nChn).Channels());
		UnlockControls();

		if ((pScrollBar) && (pScrollBar == m_sbValue))
		{
			int n = (short int)m_sbValue->value();
			if ((n >= 0) && (n <= 100) && (m_nCurrentPlugin < MAX_MIXPLUGINS))
			{
				IMixPlugin *pPlugin = GetCurrentPlugin();
				if(pPlugin != nullptr)
				{
					const PlugParamIndex nParams = pPlugin->GetNumVisibleParameters();
					if(m_nCurrentParam < nParams)
					{
						if (nSBCode == SB_THUMBPOSITION || nSBCode == SB_THUMBTRACK || nSBCode == SB_ENDSCROLL)
						{
							pPlugin->SetScaledUIParam(m_nCurrentParam, 0.01f * n);
							OnParamChanged();
							SetPluginModified();
						}
					}
				}
			}
		}
	}
}


void CViewGlobals::OnVScroll(mfcless::ScrollCode code, int pos, QWidget *pScrollBar)
{
	CModDoc *pModDoc = GetDocument();

	if((m_nCurrentPlugin >= MAX_MIXPLUGINS) || (!pModDoc)) return;
	CSoundFile &sndFile = pModDoc->GetSoundFile();
	mpt::uchar s[32];

	if(nSBCode != SB_ENDSCROLL && pScrollBar && pScrollBar == (CScrollBar*)&m_SpinMixGain)
	{

		SNDMIXPLUGIN &plugin = sndFile.m_MixPlugins[m_nCurrentPlugin];

		if(plugin.pMixPlugin)
		{
			uint8 gain = (uint8)nPos;
			if(gain == 0) gain = 1;

			plugin.SetGain(gain);

			float fValue = 0.1f * (float)gain;
			_stprintf(s, UL_("Gain: x%1.1f"), fValue);
			CEdit *gainEdit = (CEdit *)GetDlgItem(IDC_EDIT16);
			mfcless::SetWindowText(gainEdit, s);

			SetPluginModified();
		}
	}

	DialogBase::OnVScroll(nSBCode, nPos, pScrollBar);
}


void CViewGlobals::OnEditName(const CHANNELINDEX chnMod4, const uint32 itemID)
{
	CModDoc *pModDoc = GetDocument();

	if ((pModDoc) && (!m_nLockCount))
	{
		CSoundFile &sndFile = pModDoc->GetSoundFile();
		const CHANNELINDEX nChn = m_nActiveTab * CHANNELS_IN_TAB + chnMod4;
		mpt::ustring tmp;
		tmp = GetItemText(itemID);
		const std::string s = mpt::ToCharset(sndFile.GetCharsetInternal(), tmp);
		if ((sndFile.GetType() & (MOD_TYPE_XM|MOD_TYPE_IT|MOD_TYPE_MPT)) && (nChn < sndFile.GetNumChannels()) && (s != sndFile.ChnSettings[nChn].szName))
		{
			PrepareUndo(chnMod4);
			sndFile.ChnSettings[nChn].szName = s;
			pModDoc->SetModified();
			pModDoc->UpdateAllViews(this, GeneralHint(nChn).Channels());
		}
	}
}
void CViewGlobals::OnEditName1() {OnEditName(0, IDC_EDIT9);}
void CViewGlobals::OnEditName2() {OnEditName(1, IDC_EDIT10);}
void CViewGlobals::OnEditName3() {OnEditName(2, IDC_EDIT11);}
void CViewGlobals::OnEditName4() {OnEditName(3, IDC_EDIT12);}


void CViewGlobals::OnFxChanged(const CHANNELINDEX chnMod4)
{
	CModDoc *pModDoc = GetDocument();

	if (pModDoc)
	{
		CSoundFile &sndFile = pModDoc->GetSoundFile();
		CHANNELINDEX nChn = m_nActiveTab * CHANNELS_IN_TAB + chnMod4;
		PLUGINDEX nfx = m_CbnEffects[chnMod4].GetSelection().value_or(PLUGINDEX_INVALID);
		if(nfx == PLUGINDEX_INVALID)
			nfx = 0;
		else
			nfx++;
		if(nChn < sndFile.GetNumChannels() && sndFile.ChnSettings[nChn].nMixPlugin != nfx)
		{
			PrepareUndo(chnMod4);
			sndFile.ChnSettings[nChn].nMixPlugin = nfx;
			if(sndFile.GetModSpecifications().supportsPlugins)
				pModDoc->SetModified();
			pModDoc->UpdateAllViews(this, GeneralHint(nChn).Channels());
		}
	}
}


void CViewGlobals::OnFx1Changed() {OnFxChanged(0);}
void CViewGlobals::OnFx2Changed() {OnFxChanged(1);}
void CViewGlobals::OnFx3Changed() {OnFxChanged(2);}
void CViewGlobals::OnFx4Changed() {OnFxChanged(3);}


void CViewGlobals::OnPluginNameChanged()
{
	CModDoc *pModDoc = GetDocument();

	if ((pModDoc) && (m_nCurrentPlugin < MAX_MIXPLUGINS))
	{
		CSoundFile &sndFile = pModDoc->GetSoundFile();
		SNDMIXPLUGIN &plugin = sndFile.m_MixPlugins[m_nCurrentPlugin];

		mpt::ustring s;
		s = GetItemText(IDC_EDIT13);
		if (s != mpt::ToUnicode(plugin.GetName()))
		{
			plugin.Info.szName = mpt::ToCharset(mpt::Charset::Locale, s);
			if(sndFile.GetModSpecifications().supportsPlugins)
				pModDoc->SetModified();
			const auto updateHint = PluginHint(m_nCurrentPlugin + 1).Names();
			pModDoc->UpdateAllViews(this, updateHint, this);

			IMixPlugin *pPlugin = plugin.pMixPlugin;
			if(pPlugin != nullptr && pPlugin->GetEditor() != nullptr)
			{
				pPlugin->GetEditor()->SetTitle();
			}
			m_CbnPlugin->Update(PluginComboBox::Config{updateHint}, sndFile);
			PopulateChannelPlugins(updateHint);
		}
	}
}


void CViewGlobals::OnPrevPlugin()
{
	CModDoc *pModDoc = GetDocument();
	if ((m_nCurrentPlugin > 0) && (pModDoc))
	{
		m_nCurrentPlugin--;
		UpdateView(PluginHint(m_nCurrentPlugin + 1).Info());
	}
}


void CViewGlobals::OnNextPlugin()
{
	CModDoc *pModDoc = GetDocument();
	if ((m_nCurrentPlugin < MAX_MIXPLUGINS-1) && (pModDoc))
	{
		m_nCurrentPlugin++;
		UpdateView(PluginHint(m_nCurrentPlugin + 1).Info());

	}
}


void CViewGlobals::OnPluginChanged()
{
	const PLUGINDEX plugin = m_CbnPlugin->GetSelection().value_or(PLUGINDEX_INVALID);
	if(plugin != PLUGINDEX_INVALID)
	{
		m_nCurrentPlugin = plugin;
		UpdateView(PluginHint(m_nCurrentPlugin + 1).Info());
	}
	m_CbnPreset->setCurrentIndex(0);
}


void CViewGlobals::OnSelectPlugin()
{
#ifndef NO_PLUGINS
	CModDoc *pModDoc = GetDocument();

	if ((pModDoc) && (m_nCurrentPlugin < MAX_MIXPLUGINS))
	{
		CSelectPluginDlg dlg(pModDoc, m_nCurrentPlugin, this);
		if (dlg.DoModal() == IDOK)
		{
			if(pModDoc->GetSoundFile().GetModSpecifications().supportsPlugins)
				pModDoc->SetModified();
		}
		OnPluginChanged();
		OnParamChanged();
	}
#endif // NO_PLUGINS
}


void CViewGlobals::OnRemovePlugin()
{
#ifndef NO_PLUGINS
	CModDoc *pModDoc = GetDocument();

	if(pModDoc && m_nCurrentPlugin < MAX_MIXPLUGINS && Reporting::Confirm(MPT_UFORMAT("Remove plugin FX{}: {}?")(m_nCurrentPlugin + 1, pModDoc->GetSoundFile().m_MixPlugins[m_nCurrentPlugin].GetName()), false, true) == cnfYes)
	{
		if(pModDoc->RemovePlugin(m_nCurrentPlugin))
		{
			OnPluginChanged();
			OnParamChanged();
		}
	}
#endif  // NO_PLUGINS
}


intptr_t CViewGlobals::OnParamAutomated(uintptr_t plugin, intptr_t param)
{
	if(plugin == m_nCurrentPlugin && static_cast<PlugParamIndex>(param) == m_nCurrentParam)
	{
		OnParamChanged();
	}
	return 0;
}


intptr_t CViewGlobals::OnDryWetRatioChangedFromPlayer(uintptr_t plugin, intptr_t)
{
	if(plugin == m_nCurrentPlugin)
	{
		UpdateDryWetDisplay();
	}
	return 0;
}


void CViewGlobals::OnParamChanged()
{
	PlugParamIndex cursel = static_cast<PlugParamIndex>(mfcless::GetItemData(m_CbnParam, m_CbnParam->currentIndex()));

	IMixPlugin *pPlugin = GetCurrentPlugin();

	if(pPlugin != nullptr && cursel != static_cast<PlugParamIndex>(CB_ERR))
	{
		const PlugParamIndex nParams = pPlugin->GetNumVisibleParameters();
		if(cursel < nParams) m_nCurrentParam = cursel;
		if(m_nCurrentParam < nParams)
		{
			const auto value = pPlugin->GetScaledUIParam(m_nCurrentParam);
			int intValue = mpt::saturate_round<int>(value * 100.0f);
			LockControls();
			if(mfcless::GetFocus() != GetDlgItem(IDC_EDIT14))
			{
				mpt::ustring s = pPlugin->GetFormattedParamValue(m_nCurrentParam).Trim();
				if(s.IsEmpty())
				{
					s = mfcless::PrintF(UL_("%f"), value);
				}
				SetItemText(IDC_EDIT14, s);
			}
			m_sbValue->setValue(intValue);
			UnlockControls();
			return;
		}
	}
	SetItemText(IDC_EDIT14, UL_(""));
	m_sbValue->setValue(0);
}


// When focussing the parameter value, show its real value to edit
void CViewGlobals::OnFocusParam()
{
	IMixPlugin *pPlugin = GetCurrentPlugin();
	if(pPlugin != nullptr)
	{
		const PlugParamIndex nParams = pPlugin->GetNumVisibleParameters();
		if(m_nCurrentParam < nParams)
		{
			mpt::uchar s[32];
			float fValue = pPlugin->GetScaledUIParam(m_nCurrentParam);
			_stprintf(s, UL_("%f"), fValue);
			LockControls();
			SetItemText(IDC_EDIT14, s);
			UnlockControls();
		}
	}
}


void CViewGlobals::SetPluginModified()
{
	CModDoc *pModDoc = GetDocument();
	if(pModDoc->GetSoundFile().GetModSpecifications().supportsPlugins)
		pModDoc->SetModified();
	pModDoc->UpdateAllViews(this, PluginHint(m_nCurrentPlugin + 1).Info());
}


void CViewGlobals::OnProgramChanged()
{
	int32 curProg = static_cast<int32>(mfcless::GetItemData(m_CbnPreset, m_CbnPreset->currentIndex()));

	IMixPlugin *pPlugin = GetCurrentPlugin();
	if(pPlugin != nullptr)
	{
		const int32 numProgs = pPlugin->GetNumPrograms();
		if(curProg <= numProgs)
		{
			pPlugin->SetCurrentProgram(curProg);
			// Update parameter display
			OnParamChanged();

			SetPluginModified();
		}
	}
}


void CViewGlobals::OnLoadParam()
{
	IMixPlugin *pPlugin = GetCurrentPlugin();
	if(pPlugin != nullptr && pPlugin->LoadProgram())
	{
		int32 currentProg = pPlugin->GetCurrentProgram();
		FillPluginProgramBox(currentProg, currentProg);
		m_CbnPreset->setCurrentIndex(0);
		SetPluginModified();
	}
}


void CViewGlobals::OnSaveParam()
{
	IMixPlugin *pPlugin = GetCurrentPlugin();
	if(pPlugin != nullptr)
	{
		pPlugin->SaveProgram();
	}
}


void CViewGlobals::OnSetParameter()
{
	if(m_nCurrentPlugin >= MAX_MIXPLUGINS || IsLocked()) return;
	IMixPlugin *pPlugin = GetCurrentPlugin();

	if(pPlugin != nullptr)
	{
		const PlugParamIndex nParams = pPlugin->GetNumVisibleParameters();
		mpt::uchar s[32];
		s, mpt::saturate_cast<int>(std::size(s = GetItemText(IDC_EDIT14)));
		if ((m_nCurrentParam < nParams) && (s[0]))
		{
			float fValue = (float)_tstof(s);
			pPlugin->SetScaledUIParam(m_nCurrentParam, fValue);
			OnParamChanged();
			SetPluginModified();
		}
	}
}


void CViewGlobals::UpdateDryWetDisplay()
{
	SNDMIXPLUGIN &plugin = GetDocument()->GetSoundFile().m_MixPlugins[m_nCurrentPlugin];
	float wetRatio = 1.0f - plugin.fDryRatio, dryRatio = plugin.fDryRatio;
	m_sbDryRatio->setValue(mpt::saturate_round<int>(wetRatio * 100));
	if(plugin.IsExpandedMix())
	{
		wetRatio = 2.0f * wetRatio - 1.0f;
		dryRatio = -wetRatio;
	}
	int wetInt = mpt::saturate_round<int>(wetRatio * 100), dryInt = mpt::saturate_round<int>(dryRatio * 100);
	SetItemText(IDC_STATIC8, MPT_UFORMAT("{}% wet, {}% dry")(wetInt, dryInt).c_str());
}


void CViewGlobals::OnMixModeChanged()
{
	CModDoc *pModDoc = GetDocument();
	if ((m_nCurrentPlugin >= MAX_MIXPLUGINS) || (!pModDoc)) return;

	pModDoc->GetSoundFile().m_MixPlugins[m_nCurrentPlugin].SetMasterEffect(IsChecked(IDC_CHECK9) != 0);
	SetPluginModified();
}


void CViewGlobals::OnBypassChanged()
{
	CModDoc *pModDoc = GetDocument();
	if ((m_nCurrentPlugin >= MAX_MIXPLUGINS) || (!pModDoc)) return;

	auto &mixPlugs = pModDoc->GetSoundFile().m_MixPlugins;
	auto &currentPlug = pModDoc->GetSoundFile().m_MixPlugins[m_nCurrentPlugin];
	bool bypass = IsChecked(IDC_CHECK10) != 0;
	const bool bypassOthers = CInputHandler::ShiftPressed();
	const bool bypassOnlyMasterPlugs = CInputHandler::CtrlPressed();
	if(bypassOthers || bypassOnlyMasterPlugs)
	{
		SetChecked(IDC_CHECK10, currentPlug.IsBypassed() ? 1 : 0);
		uint8 state = 0;
		for(const auto &plug : mixPlugs)
		{
			if(!plug.pMixPlugin || &plug == &currentPlug)
				continue;
			if(!plug.IsMasterEffect() && bypassOnlyMasterPlugs)
				continue;
			if(plug.IsBypassed())
				state |= 1;
			else
				state |= 2;
			if(state == 3)
				break;
		}
		if(!state)
			return;

		bypass = (state != 1);
		for(auto &plug : mixPlugs)
		{
			if(!plug.pMixPlugin || &plug == &currentPlug)
				continue;
			if(!plug.IsMasterEffect() && bypassOnlyMasterPlugs)
				continue;
			if(plug.IsBypassed() == bypass)
				continue;
			plug.SetBypass(bypass);
			pModDoc->UpdateAllViews(this, PluginHint(plug.pMixPlugin->GetSlot() + 1).Info());
		}
		if(pModDoc->GetSoundFile().GetModSpecifications().supportsPlugins)
			pModDoc->SetModified();
	} else
	{
		currentPlug.SetBypass(bypass);
		SetPluginModified();
	}
}


void CViewGlobals::OnWetDryExpandChanged()
{
	CModDoc *pModDoc = GetDocument();
	if ((m_nCurrentPlugin >= MAX_MIXPLUGINS) || (!pModDoc)) return;

	pModDoc->GetSoundFile().m_MixPlugins[m_nCurrentPlugin].SetExpandedMix(IsChecked(IDC_CHECK12) != 0);
	UpdateDryWetDisplay();
	SetPluginModified();
}


void CViewGlobals::OnAutoSuspendChanged()
{
	CModDoc *pModDoc = GetDocument();
	if(m_nCurrentPlugin >= MAX_MIXPLUGINS || !pModDoc)
		return;

	pModDoc->GetSoundFile().m_MixPlugins[m_nCurrentPlugin].SetAutoSuspend(IsChecked(IDC_CHECK13) != 0);
	SetPluginModified();
}


void CViewGlobals::OnSpecialMixProcessingChanged()
{
	CModDoc *pModDoc = GetDocument();
	if ((m_nCurrentPlugin >= MAX_MIXPLUGINS) || (!pModDoc)) return;

	pModDoc->GetSoundFile().m_MixPlugins[m_nCurrentPlugin].SetMixMode(static_cast<PluginMixMode>(m_CbnSpecialMixProcessing->currentIndex()));
	SetPluginModified();
}


void CViewGlobals::OnDryMixChanged()
{
	CModDoc *pModDoc = GetDocument();
	if ((m_nCurrentPlugin >= MAX_MIXPLUGINS) || (!pModDoc)) return;

	pModDoc->GetSoundFile().m_MixPlugins[m_nCurrentPlugin].SetDryMix(IsChecked(IDC_CHECK11) != 0);
	SetPluginModified();
}


void CViewGlobals::OnEditPlugin()
{
	CModDoc *pModDoc = GetDocument();
	if ((m_nCurrentPlugin >= MAX_MIXPLUGINS) || (!pModDoc)) return;
	pModDoc->TogglePluginEditor(m_nCurrentPlugin, CInputHandler::ShiftPressed());
	return;
}


void CViewGlobals::OnOutputRoutingChanged()
{
	CModDoc *pModDoc = GetDocument();
	int nroute;

	if ((m_nCurrentPlugin >= MAX_MIXPLUGINS) || (!pModDoc)) return;
	CSoundFile &sndFile = pModDoc->GetSoundFile();
	SNDMIXPLUGIN &plugin = sndFile.m_MixPlugins[m_nCurrentPlugin];
	nroute = static_cast<int>(mfcless::GetItemData(m_CbnOutput, m_CbnOutput->currentIndex()));

	if(nroute == 1)
	{
		// Add new plugin
		for(PLUGINDEX i = m_nCurrentPlugin + 1; i < MAX_MIXPLUGINS; i++)
		{
			if(!sndFile.m_MixPlugins[i].IsValidPlugin())
			{
				CSelectPluginDlg dlg(pModDoc, i, this);
				if(dlg.DoModal() != IDOK)
					return;
				
				plugin.SetOutputPlugin(i);
				SetPluginModified();
				nroute = 0x80 + i;
				m_nCurrentPlugin = i;
				m_CbnPlugin->SetSelection(i);
				OnPluginChanged();
				break;
			}
		}
		if(nroute == 1)
		{
			Reporting::Error("All following plugin slots are used.", "Unable to add new plugin");
			return;
		}
	}

	if(!nroute)
		plugin.SetOutputToMaster();
	else
		plugin.SetOutputPlugin(static_cast<PLUGINDEX>(nroute - 0x80));

	SetPluginModified();
}



intptr_t CViewGlobals::OnModViewMsg(uintptr_t wParam, intptr_t /*lParam*/)
{
	switch(wParam)
	{
		case VIEWMSG_SETFOCUS:
		case VIEWMSG_SETACTIVE:
			GetParentFrame()->SetActiveView(this);
			setFocus();
			return 0;
		default:
			return 0;
	}
}

void CViewGlobals::OnMovePlugToSlot()
{
	if(GetCurrentPlugin() == nullptr)
	{
		return;
	}

	// If any plugin routes its output to the current plugin, we shouldn't try to move it before that plugin...
	PLUGINDEX defaultIndex = 0;
	CSoundFile &sndFile = GetDocument()->GetSoundFile();
	for(PLUGINDEX i = 0; i < m_nCurrentPlugin; i++)
	{
		if(sndFile.m_MixPlugins[i].GetOutputPlugin() == m_nCurrentPlugin)
		{
			defaultIndex = i + 1;
		}
	}

	std::vector<PLUGINDEX> emptySlots;
	BuildEmptySlotList(emptySlots);

	CMoveFXSlotDialog dlg(this, m_nCurrentPlugin, emptySlots, defaultIndex, false, !sndFile.m_MixPlugins[m_nCurrentPlugin].IsOutputToMaster());

	if(dlg.DoModal() == IDOK)
	{
		size_t toIndex = dlg.GetSlotIndex();
		do
		{
			const SNDMIXPLUGIN &curPlugin = sndFile.m_MixPlugins[m_nCurrentPlugin];
			SNDMIXPLUGIN &newPlugin = sndFile.m_MixPlugins[emptySlots[toIndex]];
			const PLUGINDEX nextPlugin = curPlugin.GetOutputPlugin();

			MovePlug(m_nCurrentPlugin, emptySlots[toIndex]);

			if(nextPlugin == PLUGINDEX_INVALID || toIndex == emptySlots.size() - 1)
			{
				break;
			}

			m_nCurrentPlugin = nextPlugin;

			if(dlg.DoMoveChain())
			{
				toIndex++;
				newPlugin.SetOutputPlugin(emptySlots[toIndex]);
			}
		} while(dlg.DoMoveChain());

		m_CbnPlugin->SetSelection(dlg.GetSlot());
		OnPluginChanged();
		GetDocument()->UpdateAllViews(nullptr, PluginHint().Names().Info());
	}
}


// Functor for adjusting plug indexes in modcommands. Adjusts all instrument column values in
// range [m_nInstrMin, m_nInstrMax] by m_nDiff.
struct PlugIndexModifier
{
	PlugIndexModifier(PLUGINDEX nMin, PLUGINDEX nMax, int nDiff) :
		m_nDiff(nDiff), m_nInstrMin(nMin), m_nInstrMax(nMax) {}
	void operator()(ModCommand& m)
	{
		if (m.IsInstrPlug() && m.instr >= m_nInstrMin && m.instr <= m_nInstrMax)
			m.instr = (ModCommand::INSTR)((int)m.instr + m_nDiff);
	}
	int m_nDiff;
	ModCommand::INSTR m_nInstrMin;
	ModCommand::INSTR m_nInstrMax;
};


bool CViewGlobals::MovePlug(PLUGINDEX src, PLUGINDEX dest, bool bAdjustPat)
{
	if (src == dest)
		return false;
	CModDoc *pModDoc = GetDocument();
	CSoundFile &sndFile = pModDoc->GetSoundFile();

	mfcless::BeginWaitCursor();

	CriticalSection cs;

	// Move plug data
	sndFile.m_MixPlugins[dest] = std::move(sndFile.m_MixPlugins[src]);
	mpt::reconstruct(sndFile.m_MixPlugins[src]);

	sndFile.GetMIDIMapper().MovePlugin(src, dest);

	// Prevent plug from pointing backwards.
	if(!sndFile.m_MixPlugins[dest].IsOutputToMaster())
	{
		PLUGINDEX nOutput = sndFile.m_MixPlugins[dest].GetOutputPlugin();
		if (nOutput <= dest && nOutput != PLUGINDEX_INVALID)
		{
			sndFile.m_MixPlugins[dest].SetOutputToMaster();
		}
	}

	// Update current plug
	IMixPlugin *pPlugin = sndFile.m_MixPlugins[dest].pMixPlugin;
	if(pPlugin != nullptr)
	{
		pPlugin->SetSlot(dest);
		if(pPlugin->GetEditor() != nullptr)
		{
			pPlugin->GetEditor()->SetTitle();
		}
	}

	// Update all other plugs' outputs
	for (PLUGINDEX nPlug = 0; nPlug < src; nPlug++)
	{
		if(!sndFile.m_MixPlugins[nPlug].IsOutputToMaster())
		{
			if(sndFile.m_MixPlugins[nPlug].GetOutputPlugin() == src)
			{
				sndFile.m_MixPlugins[nPlug].SetOutputPlugin(dest);
			}
		}
	}
	// Update channels
	for (CHANNELINDEX nChn = 0; nChn < sndFile.GetNumChannels(); nChn++)
	{
		if (sndFile.ChnSettings[nChn].nMixPlugin == src + 1u)
		{
			sndFile.ChnSettings[nChn].nMixPlugin = dest + 1u;
		}
	}

	// Update instruments
	for (INSTRUMENTINDEX nIns = 1; nIns <= sndFile.GetNumInstruments(); nIns++)
	{
		if (sndFile.Instruments[nIns] && (sndFile.Instruments[nIns]->nMixPlug == src + 1))
		{
			sndFile.Instruments[nIns]->nMixPlug = dest + 1u;
		}
	}

	// Update MODCOMMANDs so that they won't be referring to old indexes (e.g. with NOTE_PC).
	if (bAdjustPat && sndFile.GetModSpecifications().HasNote(NOTE_PC))
		sndFile.Patterns.ForEachModCommand(PlugIndexModifier(src + 1, src + 1, int(dest) - int(src)));

	cs.Leave();

	SetPluginModified();

	mfcless::EndWaitCursor();

	return true;
}


void CViewGlobals::BuildEmptySlotList(std::vector<PLUGINDEX> &emptySlots)
{
	const CSoundFile &sndFile = GetDocument()->GetSoundFile();

	emptySlots.clear();

	for(PLUGINDEX nSlot = 0; nSlot < MAX_MIXPLUGINS; nSlot++)
	{
		if(sndFile.m_MixPlugins[nSlot].pMixPlugin == nullptr)
		{
			emptySlots.push_back(nSlot);
		}
	}
	return;
}

void CViewGlobals::OnInsertSlot()
{
	mpt::ustring prompt;
	CSoundFile &sndFile = GetDocument()->GetSoundFile();
	prompt = mfcless::PrintF(UL_("Insert empty slot before slot FX%d?"), m_nCurrentPlugin + 1);

	// If last plugin slot is occupied, move it so that the plugin is not lost.
	// This could certainly be improved...
	bool moveLastPlug = false;

	if(sndFile.m_MixPlugins[MAX_MIXPLUGINS - 1].pMixPlugin)
	{
		if(sndFile.m_MixPlugins[MAX_MIXPLUGINS - 2].pMixPlugin == nullptr)
		{
			moveLastPlug = true;
		} else
		{
			prompt += UL_("\nWarning: plugin data in last slot will be lost.");
		}
	}
	if(Reporting::Confirm(prompt) == cnfYes)
	{

		// Delete last plug...
		if(sndFile.m_MixPlugins[MAX_MIXPLUGINS - 1].pMixPlugin)
		{
			if(moveLastPlug)
			{
				MovePlug(MAX_MIXPLUGINS - 1, MAX_MIXPLUGINS - 2, true);
			} else
			{
				sndFile.m_MixPlugins[MAX_MIXPLUGINS - 1].Destroy();
				MemsetZero(sndFile.m_MixPlugins[MAX_MIXPLUGINS - 1].Info);
			}
		}

		// Update MODCOMMANDs so that they won't be referring to old indexes (e.g. with NOTE_PC).
		if(sndFile.GetModSpecifications().HasNote(NOTE_PC))
			sndFile.Patterns.ForEachModCommand(PlugIndexModifier(m_nCurrentPlugin + 1, MAX_MIXPLUGINS - 1, 1));


		for(PLUGINDEX nSlot = MAX_MIXPLUGINS - 1; nSlot > m_nCurrentPlugin; nSlot--)
		{
			if(sndFile.m_MixPlugins[nSlot-1].pMixPlugin)
			{
				MovePlug(nSlot - 1, nSlot, NoPatternAdjust);
			}
		}

		m_CbnPlugin->SetSelection(m_nCurrentPlugin);
		OnPluginChanged();
		GetDocument()->UpdateAllViews(nullptr, PluginHint().Names().Info());

		SetPluginModified();
	}

}


void CViewGlobals::OnClonePlug()
{
	if(GetCurrentPlugin() == nullptr)
	{
		return;
	}

	CSoundFile &sndFile = GetDocument()->GetSoundFile();

	std::vector<PLUGINDEX> emptySlots;
	BuildEmptySlotList(emptySlots);

	CMoveFXSlotDialog dlg(this, m_nCurrentPlugin, emptySlots, 0, true, !sndFile.m_MixPlugins[m_nCurrentPlugin].IsOutputToMaster());

	if(dlg.DoModal() == IDOK)
	{
		size_t toIndex = dlg.GetSlotIndex();
		do
		{
			const SNDMIXPLUGIN &curPlugin = sndFile.m_MixPlugins[m_nCurrentPlugin];
			SNDMIXPLUGIN &newPlugin = sndFile.m_MixPlugins[emptySlots[toIndex]];

			GetDocument()->ClonePlugin(newPlugin, curPlugin);
			IMixPlugin *mixPlug = newPlugin.pMixPlugin;
			if(mixPlug != nullptr && mixPlug->IsInstrument() && GetDocument()->HasInstrumentForPlugin(emptySlots[toIndex]) == INSTRUMENTINDEX_INVALID)
			{
				GetDocument()->InsertInstrumentForPlugin(emptySlots[toIndex]);
			}

			if(curPlugin.IsOutputToMaster() || toIndex == emptySlots.size() - 1)
			{
				break;
			}

			m_nCurrentPlugin = curPlugin.GetOutputPlugin();

			if(dlg.DoMoveChain())
			{
				toIndex++;
				newPlugin.SetOutputPlugin(emptySlots[toIndex]);
			}
		} while(dlg.DoMoveChain());

		m_CbnPlugin->SetSelection(dlg.GetSlot());
		OnPluginChanged();
		//PopulateChannelPlugins(PluginHint().Names());
		GetDocument()->UpdateAllViews(nullptr, PluginHint().Names(), nullptr);

		SetPluginModified();
	}
}


// The plugin param box is only filled when it gets the focus (done here).
void CViewGlobals::OnFillParamCombo()
{
	// no need to fill it again.
	if(m_CbnParam->count() > 1)
		return;

	IMixPlugin *pPlugin = GetCurrentPlugin();
	if(pPlugin == nullptr) return;

	const PlugParamIndex nParams = pPlugin->GetNumVisibleParameters();
	m_CbnParam->setUpdatesEnabled(false);
	m_CbnParam->clear();

	AddPluginParameternamesToCombobox(m_CbnParam, *pPlugin);

	if (m_nCurrentParam >= nParams) m_nCurrentParam = 0;
	m_CbnParam->setCurrentIndex(m_nCurrentParam);
	m_CbnParam->setUpdatesEnabled(true);
	m_CbnParam->update(false);
}


// The preset box is only filled when it gets the focus (done here).
void CViewGlobals::OnFillProgramCombo()
{
	// no need to fill it again.
	if(m_CbnPreset->count() > 1)
		return;

	IMixPlugin *pPlugin = GetCurrentPlugin();
	if(pPlugin == nullptr) return;

	FillPluginProgramBox(0, pPlugin->GetNumPrograms() - 1);
	m_CbnPreset->setCurrentIndex(pPlugin->GetCurrentProgram());
}


void CViewGlobals::FillPluginProgramBox(int32 firstProg, int32 lastProg)
{
	IMixPlugin *pPlugin = GetCurrentPlugin();

	m_CbnPreset->setUpdatesEnabled(false);
	m_CbnPreset->clear();

	pPlugin->CacheProgramNames(firstProg, lastProg + 1);
	for (int32 i = firstProg; i <= lastProg; i++)
	{
		mfcless::SetItemData(m_CbnPreset, mfcless::AddItem(m_CbnPreset, pPlugin->GetFormattedProgramName(i)), i);
	}

	m_CbnPreset->setUpdatesEnabled(true);
	m_CbnPreset->update(false);
}


intptr_t CViewGlobals::OnToolHitTest(mfcless::Point point, mfcless::ToolInfo *pTI) const
{
	intptr_t nHit = DialogBase::OnToolHitTest(point, pTI);
	if(nHit >= 0 && pTI && (pTI->uFlags & TTF_IDISHWND))
	{
		// Workaround to get tooltips even for disabled controls inside group boxes that are positioned in the "correct" tab order position.
		// For some reason doesn't work for enabled controls (probably because pTI->hwnd then doesn't point at the active control under the cursor),
		// so we use the default code path there, which works just fine.
		QWidget * child = reinterpret_cast<QWidget *>(pTI->uId);
		if(!::IsWindowEnabled(child))
		{
			pTI->uId = nHit;
			pTI->uFlags &= ~TTF_IDISHWND;
			::GetWindowRect(child, &pTI->rect);
			ScreenToClient(&pTI->rect);
		}
	}
	return nHit;
}


bool CViewGlobals::OnToolTipText(uint32 )
{
	auto pTTT = reinterpret_cast<TOOLTIPTEXT *>(pNMHDR);
	uintptr_t id = pNMHDR->idFrom;
	if(pTTT->uFlags & TTF_IDISHWND)
	{
		// idFrom is actually the QWidget * of the tool
		id = static_cast<uintptr_t>(::GetDlgCtrlID(reinterpret_cast<QWidget *>(id)));
	}

	mpt::ustring text;
	const auto &chnSettings = GetDocument()->GetSoundFile().ChnSettings;
	switch(id)
	{
		case IDC_EDIT1:
		case IDC_EDIT3:
		case IDC_EDIT5:
		case IDC_EDIT7:
			if(size_t chn = m_nActiveTab * CHANNELS_IN_TAB + (id - IDC_EDIT1) / 2; chn < chnSettings.size())
			text = CModDoc::LinearToDecibelsString(chnSettings[chn].nVolume, 64.0);
			break;
		case IDC_SLIDER1:
		case IDC_SLIDER3:
		case IDC_SLIDER5:
		case IDC_SLIDER7:
			if(size_t chn = m_nActiveTab * CHANNELS_IN_TAB + (id - IDC_SLIDER1) / 2; chn < chnSettings.size())
				text = CModDoc::LinearToDecibelsString(chnSettings[chn].nVolume, 64.0);
			break;
		case IDC_EDIT2:
		case IDC_EDIT4:
		case IDC_EDIT6:
		case IDC_EDIT8:
			if(size_t chn = m_nActiveTab * CHANNELS_IN_TAB + (id - IDC_EDIT2) / 2; chn < chnSettings.size())
				text = CModDoc::PanningToString(chnSettings[chn].nPan, 128);
			break;
		case IDC_SLIDER2:
		case IDC_SLIDER4:
		case IDC_SLIDER6:
		case IDC_SLIDER8:
			if(size_t chn = m_nActiveTab * CHANNELS_IN_TAB + (id - IDC_SLIDER2) / 2; chn < chnSettings.size())
				text = CModDoc::PanningToString(chnSettings[chn].nPan, 128);
			break;
		case IDC_EDIT16:
			{
				const auto gain = GetDocument()->GetSoundFile().m_MixPlugins[m_nCurrentPlugin].GetGain();
				text = CModDoc::LinearToDecibelsString(gain ? gain : 10, 10.0);
			}
			break;
		case IDC_BUTTON5:
			text = UL_("Previous Plugin");
			break;
		case IDC_BUTTON4:
			text = UL_("Next Plugin");
			break;
		default:
			return false;
	}

	mpt::String::WriteWinBuf(pTTT->szText) = text;
	CurrentNotify().result = 0;

	// bring the tooltip window above other popup windows
	::SetWindowPos(pNMHDR->hwndFrom, HWND_TOP, 0, 0, 0, 0, mfcless::swp::NoActivate | mfcless::swp::NoSize | mfcless::swp::NoMove | mfcless::swp::NoOwnerZOrder);

	return true;  // message was handled
}


OPENMPT_NAMESPACE_END
