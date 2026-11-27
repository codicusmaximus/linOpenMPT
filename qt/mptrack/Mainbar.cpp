/*
 * Mainbar.cpp
 * -----------
 * Purpose: Implementation of OpenMPT's window toolbar and parent container of the tree view.
 * Notes  : mfcless twin of upstream's mptrack/Mainbar.cpp.
 *          Draft from qt/tools/twin.py, not yet edited by hand.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "Mainbar.h"
#include "HighDPISupport.h"
#include "ImageLists.h"
#include "InputHandler.h"
#include "Mainfrm.h"
#include "Moddoc.h"
#include "Mptrack.h"
#include "resource.h"
#include "TrackerSettings.h"
#include "View_tre.h"
#include "WindowMessages.h"
#include "../common/mptStringBuffer.h"
#include "../soundlib/mod_specifications.h"


OPENMPT_NAMESPACE_BEGIN


void COctaveEdit::RegisterMessageMap()
{
	mfcless::Window<QLineEdit>::RegisterMessageMap();
	// ON_WM_CHAR() (mfcless::Window)
}

void COctaveEdit::OnChar(uint32 nChar, uint32 /*nRepCnt*/, uint32 /*nFlags*/)
{
	if(nChar >= _T('0') + MIN_BASEOCTAVE && nChar <= _T('0') + MAX_BASEOCTAVE)
		m_owner.SetBaseOctave(nChar - _T('0'));
	else if(nChar == _T('+') && m_owner.GetBaseOctave() < MAX_BASEOCTAVE)
		m_owner.SetBaseOctave(m_owner.GetBaseOctave() + 1);
	else if(nChar == _T('-') && m_owner.GetBaseOctave() > MIN_BASEOCTAVE)
		m_owner.SetBaseOctave(m_owner.GetBaseOctave() - 1);
}

/////////////////////////////////////////////////////////////////////
// CToolBarEx: custom toolbar base class

void CToolBarEx::SetHorizontal()
{
	m_bVertical = false;
	SetBarStyle(GetBarStyle() | CBRS_ALIGN_TOP);
}


void CToolBarEx::SetVertical()
{
	m_bVertical = true;
}


mfcless::Size CToolBarEx::CalcDynamicLayout(int nLength, uint32 dwMode)
{
	// Make the toolbar go into multiline mode when it's docked
	if(dwMode & LM_HORZDOCK)
	{
		if(auto mainFrm = CMainFrame::GetMainFrame())
		{
			mfcless::Rect rect;
			mainFrm->GetClientRect(rect);
			nLength = rect.right;
			dwMode &= ~LM_HORZDOCK;
			dwMode |= LM_COMMIT;
		}
	}

	mfcless::Size sizeResult;
	// if we're committing set the buttons appropriately
	if(dwMode & LM_COMMIT)
	{
		if(dwMode & LM_VERTDOCK)
		{
			if(!m_bVertical)
				SetVertical();
		} else
		{
			if(m_bVertical)
				SetHorizontal();
		}
		sizeResult = mfcless::Window<QToolBar>::CalcDynamicLayout(nLength, dwMode);
	} else
	{
		const bool wasVertical = m_bVertical;
		const bool doSwitch = (dwMode & LM_HORZ) ? wasVertical : !wasVertical;

		if(doSwitch)
		{
			if(wasVertical)
				SetHorizontal();
			else
				SetVertical();
		}

		sizeResult = mfcless::Window<QToolBar>::CalcDynamicLayout(nLength, dwMode);

		if(doSwitch)
		{
			if(wasVertical)
				SetHorizontal();
			else
				SetVertical();
		}
	}

	return sizeResult;
}


void CToolBarEx::UpdateControl(bool show, QWidget &wnd, int index, int id, int height)
{
	if(show)
	{
		mfcless::Rect rect;
		wnd.GetWindowRect(rect);
		SetButtonInfo(index, id, TBBS_SEPARATOR, rect.Width());

		if(wnd.m_hWnd)
		{
			GetItemRect(index, rect);
			if(height)
			{
				int n = (rect.bottom + rect.top - height) / 2;
				if(n > rect.top)
					rect.top = n;
			}
			wnd.SetWindowPos(nullptr, rect.left, rect.top, 0, 0, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOSIZE);
		}
	}
	if(wnd.m_hWnd)
		wnd.ShowWindow(show ? true : false);
	SetButtonVisibility(index, show);
}


void CToolBarEx::EnableFlatButtons(bool flat)
{
	ModifyStyle(flat ? 0 : TBSTYLE_FLAT, flat ? TBSTYLE_FLAT : 0);
	Invalidate();
}


void CToolBarEx::SetButtonVisibility(int index, bool visible)
{
	auto style = GetButtonStyle(index);
	if(visible)
		style &= ~TBBS_HIDDEN;
	else
		style |= TBBS_HIDDEN;
	SetButtonStyle(index, style);
}


/////////////////////////////////////////////////////////////////////
// CMainToolBar

static constexpr uint32 WM_TOOLBAR_REFRESH = WM_USER + 1;

enum ToolbarItemIndex
{
	FILE_NEW_INDEX = 0,
	FILE_OPEN_INDEX,
	FILE_SAVE_INDEX,
	FILE_DIVIDER_INDEX,

	EDIT_CUT_INDEX,
	EDIT_COPY_INDEX,
	EDIT_PASTE_INDEX,
	EDIT_DIVIDER_INDEX,

	PLAY_MIDIRECORD_INDEX,
	PLAY_STOP_INDEX,
	PLAY_STARTPAUSE_INDEX,
	PLAY_RESTART_INDEX,
	PLAY_DIVIDER_INDEX,

	EDITOCTAVE_INDEX,
	SPINOCTAVE_INDEX,
	OCTAVE_DIVIDER_INDEX,

	TEMPOTEXT_INDEX,
	EDITTEMPO_INDEX,
	SPINTEMPO_INDEX,
	TEMPO_DIVIDER_INDEX,

	SPEEDTEXT_INDEX,
	EDITSPEED_INDEX,
	SPINSPEED_INDEX,
	SPEED_DIVIDER_INDEX,

	RPBTEXT_INDEX,
	EDITRPB_INDEX,
	SPINRPB_INDEX,
	RPB_DIVIDER_INDEX,

	GLOBALVOLTEXT_INDEX,
	EDITGLOBALVOL_INDEX,
	SPINGLOBALVOL_INDEX,
	GLOBALVOL_DIVIDER_INDEX,

	MISC_OPTIONS_INDEX,
	MISC_PANIC_INDEX,
	MISC_UPDATE_INDEX,
	MISC_DIVIDER_INDEX,
	
	VUMETER_INDEX,

	NUM_TOOLBAR_INDEX
};

enum
{
	TOOLBAR_IMAGE_PAUSE = 8,
	TOOLBAR_IMAGE_PLAY = 13,
};

#define SCALEPIXELS(x)      (HighDPISupport::ScalePixels(x, m_hWnd))
#define SPINNER_WIDTH       SCALEPIXELS(16)
#define SPINNER_HEIGHT      SCALEPIXELS(20)
#define VUMETER_WIDTH       SCALEPIXELS(255)
#define VUMETER_HEIGHT      SCALEPIXELS(19)


static uint32 MainButtons[] =
{
	// same order as in the bitmap 'main_toolbar.png'
	ID_FILE_NEW,
	ID_FILE_OPEN,
	ID_FILE_SAVE,
		ID_SEPARATOR,
	ID_EDIT_CUT,
	ID_EDIT_COPY,
	ID_EDIT_PASTE,
		ID_SEPARATOR,
	ID_MIDI_RECORD,
	ID_PLAYER_STOP,
	ID_PLAYER_PAUSE,
	ID_PLAYER_PLAYFROMSTART,
		ID_SEPARATOR,
	ID_SEPARATOR,  // Octave
	ID_SEPARATOR,
		ID_SEPARATOR,  // Divider for vertical mode
	ID_SEPARATOR,  // Tempo
	ID_SEPARATOR,
	ID_SEPARATOR,
		ID_SEPARATOR,  // Divider for vertical mode
	ID_SEPARATOR,  // Speed
	ID_SEPARATOR,
	ID_SEPARATOR,
		ID_SEPARATOR,  // Divider for vertical mode
	ID_SEPARATOR,  // Rows Per Beat
	ID_SEPARATOR,
	ID_SEPARATOR,
		ID_SEPARATOR,  // Divider for vertical mode
	ID_SEPARATOR,  // Global Volume
	ID_SEPARATOR,
	ID_SEPARATOR,
		ID_SEPARATOR,
	ID_VIEW_OPTIONS,
	ID_PANIC,
	ID_UPDATE_AVAILABLE,
		ID_SEPARATOR,
	ID_SEPARATOR,  // VU Meter
};

static_assert(std::size(MainButtons) == NUM_TOOLBAR_INDEX);


enum { MAX_MIDI_DEVICES = 256 };

void CMainToolBar::RegisterMessageMap()
{
	CToolBarEx::RegisterMessageMap();
	AddScroll(Qt::Vertical, &CMainToolBar::OnVScroll);
	// ON_NOTIFY_EX_RANGE(TTN_NEEDTEXT, 0, 0xFFFF, &CMainToolBar::OnToolTipText): by hand (Qt event override)
	// ON_NOTIFY_REFLECT(TBN_DROPDOWN, &CMainToolBar::OnTbnDropDownToolBar): by hand (Qt event override)
	AddCommandRange(ID_SELECT_MIDI_DEVICE, ID_SELECT_MIDI_DEVICE + MAX_MIDI_DEVICES, &CMainToolBar::OnSelectMIDIDevice);
	AddMessage(WM_DPICHANGED_AFTERPARENT, &CMainToolBar::OnDPIChangedAfterParent);
	AddMessage(WM_SETTINGCHANGE, &CMainToolBar::OnSettingChange);
	AddMessage(WM_TOOLBAR_REFRESH, &CMainToolBar::OnRefreshToolbar);
	AddNotify(IDC_EDIT_CURRENTSPEED, mfcless::Notify::EditChange, &CMainToolBar::OnSpeedChanged);
	AddNotify(IDC_EDIT_CURRENTTEMPO, mfcless::Notify::EditChange, &CMainToolBar::OnTempoChanged);
	AddNotify(IDC_EDIT_RPB, mfcless::Notify::EditChange, &CMainToolBar::OnRPBChanged);
	AddNotify(IDC_EDIT_GLOBALVOL, mfcless::Notify::EditChange, &CMainToolBar::OnGlobalVolChanged);
}


bool CMainToolBar::Create(QWidget *parent)
{
	if(!mfcless::Window<QToolBar>::Create(parent, WS_CHILD | WS_VISIBLE | CBRS_TOP | CBRS_SIZE_DYNAMIC | CBRS_TOOLTIPS | CBRS_FLYBY))
		return false;

	SetButtons(MainButtons, static_cast<int>(std::size(MainButtons)));

	// Placeholder rect, text width will be determined later
	mfcless::Rect rect{0, 0, SPINNER_WIDTH, SPINNER_HEIGHT};

	// Octave
	m_EditOctave.Create(WS_CHILD | WS_BORDER | WS_TABSTOP | ES_READONLY | ES_AUTOHSCROLL | ES_CENTER, rect, this, IDC_EDIT_BASEOCTAVE);
	m_SpinOctave->Create(WS_CHILD | UDS_ALIGNRIGHT | UDS_AUTOBUDDY, rect, this, IDC_SPIN_BASEOCTAVE);
	// Tempo
	m_StaticTempo->Create(U_("Tempo:"), WS_CHILD | SS_CENTER | SS_CENTERIMAGE, rect, this, IDC_TEXT_CURRENTTEMPO);
	m_EditTempo->Create(WS_CHILD | WS_BORDER | WS_TABSTOP | ES_READONLY | ES_AUTOHSCROLL | ES_NUMBER, rect, this, IDC_EDIT_CURRENTTEMPO);
	m_SpinTempo->Create(WS_CHILD | UDS_ALIGNRIGHT | UDS_AUTOBUDDY | UDS_ARROWKEYS, rect, this, IDC_SPIN_CURRENTTEMPO);
	// Speed
	m_StaticSpeed->Create(U_("Ticks/Row:"), WS_CHILD | SS_CENTER | SS_CENTERIMAGE, rect, this, IDC_TEXT_CURRENTSPEED);
	m_EditSpeed->Create(WS_CHILD | WS_BORDER | WS_TABSTOP | ES_READONLY | ES_AUTOHSCROLL | ES_NUMBER, rect, this, IDC_EDIT_CURRENTSPEED);
	m_SpinSpeed->Create(WS_CHILD | UDS_ALIGNRIGHT | UDS_AUTOBUDDY | UDS_ARROWKEYS, rect, this, IDC_SPIN_CURRENTSPEED);
	// Rows per Beat
	m_StaticRowsPerBeat->Create(U_("Rows/Beat:"), WS_CHILD | SS_CENTER | SS_CENTERIMAGE, rect, this, IDC_TEXT_RPB);
	m_EditRowsPerBeat->Create(WS_CHILD | WS_BORDER | WS_TABSTOP | ES_READONLY | ES_AUTOHSCROLL | ES_NUMBER, rect, this, IDC_EDIT_RPB);
	m_SpinRowsPerBeat->Create(WS_CHILD | UDS_ALIGNRIGHT | UDS_AUTOBUDDY | UDS_ARROWKEYS, rect, this, IDC_SPIN_RPB);
	// Global Volume
	m_StaticGlobalVolume->Create(U_("Global Volume:"), WS_CHILD | SS_CENTER | SS_CENTERIMAGE, rect, this, IDC_TEXT_GLOBALVOL);
	m_EditGlobalVolume->Create(WS_CHILD | WS_BORDER | WS_TABSTOP | ES_READONLY | ES_AUTOHSCROLL | ES_NUMBER, rect, this, IDC_EDIT_GLOBALVOL);
	m_SpinGlobalVolume->Create(WS_CHILD | UDS_ALIGNRIGHT | UDS_AUTOBUDDY | UDS_ARROWKEYS, rect, this, IDC_SPIN_GLOBALVOL);

	// VU Meter
	rect.SetRect(0, 0, VUMETER_WIDTH, VUMETER_HEIGHT);
	m_VuMeter->Create(U_(""), WS_CHILD | WS_BORDER | SS_NOTIFY, rect, this, IDC_VUMETER);

	UpdateSizes();

	m_SpinOctave->SetRange(MIN_BASEOCTAVE, MAX_BASEOCTAVE);
	m_SpinOctave->SetPos(4);
	m_SpinTempo->SetRange(-1, 1);
	m_SpinTempo->SetPos(0);
	m_SpinSpeed->SetRange(-1, 1);
	m_SpinSpeed->SetPos(0);
	m_SpinRowsPerBeat->SetRange(-1, 1);
	m_SpinRowsPerBeat->SetPos(0);
	m_SpinGlobalVolume->SetRange(-1, 1);
	m_SpinGlobalVolume->SetPos(0);

	m_EditTempo->AllowNegative(false);
	m_EditTempo->SetLimitText(9);
	static_assert(MAX_GLOBAL_VOLUME <= 999);
	m_EditGlobalVolume->setMaxLength(3);

	// Display everything
	SetWindowText(U_("Main"));
	SetBaseOctave(4);
	SetCurrentSong(nullptr);
	EnableDocking(CBRS_ALIGN_ANY);

	GetToolBarCtrl().SetState(ID_UPDATE_AVAILABLE, TBSTATE_HIDDEN);

	return true;
}


intptr_t CMainToolBar::OnDPIChangedAfterParent(uintptr_t, intptr_t)
{
	auto result = Default();
	m_font.DeleteObject();
	UpdateSizes();
	RefreshToolbar();
	return result;
}


intptr_t CMainToolBar::OnSettingChange(uintptr_t, intptr_t)
{
	// Workaround: When locking the screen or invoking a UAC prompt, spin buttons with auto-buddy property become invisible.
	// One common thing between those two events is that they result in a WM_SETTINGCHANGE message being sent to the toolbar.
	// If we try to refresh the toolbar right in this event, nothing happens, but if we defer it via PostMessage, the spin buttons reappear.
	PostMessage(WM_TOOLBAR_REFRESH);
	return Default();
}


intptr_t CMainToolBar::OnRefreshToolbar(uintptr_t, intptr_t)
{
	RefreshToolbar();
	return 0;
}


void CMainToolBar::RefreshToolbar()
{
	if(m_bVertical)
		SetVertical();
	else
		SetHorizontal();
	CMainFrame::GetMainFrame()->RecalcLayout();  // Update bar height (in case of DPI change)
}


void CMainToolBar::UpdateSizes()
{
	mfcless::DC *dc = GetDC();
	if(!m_font.m_hObject)
		HighDPISupport::CreateGUIFont(m_font, m_hWnd);
	const double scaling = HighDPISupport::GetDpiForWindow(m_hWnd) / 96.0;
	const int imgSize = HighDPISupport::ScalePixels(16, m_hWnd), btnSizeX = HighDPISupport::ScalePixels(23, m_hWnd), btnSizeY = HighDPISupport::ScalePixels(22, m_hWnd);
	m_ImageList.Create(IDB_MAINBAR, 16, 16, IMGLIST_NUMIMAGES, 1, dc, scaling, false);
	m_ImageListDisabled.Create(IDB_MAINBAR, 16, 16, IMGLIST_NUMIMAGES, 1, dc, scaling, true);
	
	struct TextWndInfo
	{
		QWidget &wnd;
		const mpt::uchar *measureText;
		int toolbarIndex, id;
	};
	const TextWndInfo TextWnds[] =
	{
		{m_EditOctave,         U_("Octave 9"),       EDITOCTAVE_INDEX,    IDC_EDIT_BASEOCTAVE  },
		{m_StaticTempo,        U_("Tempo"),          TEMPOTEXT_INDEX,     IDC_TEXT_CURRENTTEMPO},
		{m_EditTempo,          U_("999.9999"),       EDITTEMPO_INDEX,     IDC_EDIT_CURRENTTEMPO},
		{m_StaticSpeed,        U_("Ticks/Row:"),     SPEEDTEXT_INDEX,     IDC_TEXT_CURRENTSPEED},
		{m_EditSpeed,          U_("999"),            EDITSPEED_INDEX,     IDC_EDIT_CURRENTSPEED},
		{m_StaticRowsPerBeat,  U_("Rows/Beat:"),     RPBTEXT_INDEX,       IDC_TEXT_RPB         },
		{m_EditRowsPerBeat,    U_("9999"),           EDITRPB_INDEX,       IDC_EDIT_RPB         },
		{m_StaticGlobalVolume, U_("Global Volume:"), GLOBALVOLTEXT_INDEX, IDC_TEXT_GLOBALVOL   },
		{m_EditGlobalVolume,   U_("999"),            EDITGLOBALVOL_INDEX, IDC_EDIT_GLOBALVOL   },
	};
	
	auto oldFont = dc->SelectObject(m_font);
	const int textPaddingX = HighDPISupport::ScalePixels(10, m_hWnd), textPaddingY = HighDPISupport::ScalePixels(4, m_hWnd), textMinHeight = HighDPISupport::ScalePixels(20, m_hWnd);
	for(auto &info : TextWnds)
	{
		info.wnd.SetFont(&m_font, false);
		const auto size = dc->GetTextExtent(info.measureText);
		const int width = size.cx + textPaddingX;
		const int height = std::max(static_cast<int>(size.cy) + textPaddingY, textMinHeight);
		// For some reason, DeferWindowPos doesn't work here
		info.wnd.SetWindowPos(nullptr, 0, 0, width, height, SWP_NOMOVE | SWP_NOZORDER | SWP_NOREPOSITION | SWP_NOACTIVATE | SWP_NOCOPYBITS);
	}
	dc->SelectObject(oldFont);
	ReleaseDC(dc);

	GetToolBarCtrl().SetImageList(&m_ImageList);
	GetToolBarCtrl().SetDisabledImageList(&m_ImageListDisabled);
	SetFont(&m_font);

	SetSizes(mfcless::Size(btnSizeX, btnSizeY), mfcless::Size(imgSize, imgSize));

	// Dropdown menus for New and MIDI buttons
	GetToolBarCtrl().SetExtendedStyle(GetToolBarCtrl().GetExtendedStyle() | TBSTYLE_EX_DRAWDDARROWS);
	SetButtonStyle(CommandToIndex(ID_FILE_NEW), GetButtonStyle(CommandToIndex(ID_FILE_NEW)) | TBSTYLE_DROPDOWN);
	SetButtonStyle(CommandToIndex(ID_MIDI_RECORD), GetButtonStyle(CommandToIndex(ID_MIDI_RECORD)) | TBSTYLE_DROPDOWN);

	const int spinnerWidth = SPINNER_WIDTH, spinnerHeight = SPINNER_HEIGHT;
	m_SpinOctave->SetWindowPos(nullptr, 0, 0, spinnerWidth, spinnerHeight, SWP_NOMOVE | SWP_NOZORDER | SWP_NOREPOSITION | SWP_NOACTIVATE);
	m_SpinTempo->SetWindowPos(nullptr, 0, 0, spinnerWidth, spinnerHeight, SWP_NOMOVE | SWP_NOZORDER | SWP_NOREPOSITION | SWP_NOACTIVATE);
	m_SpinSpeed->SetWindowPos(nullptr, 0, 0, spinnerWidth, spinnerHeight, SWP_NOMOVE | SWP_NOZORDER | SWP_NOREPOSITION | SWP_NOACTIVATE);
	m_SpinRowsPerBeat->SetWindowPos(nullptr, 0, 0, spinnerWidth, spinnerHeight, SWP_NOMOVE | SWP_NOZORDER | SWP_NOREPOSITION | SWP_NOACTIVATE);
	m_SpinGlobalVolume->SetWindowPos(nullptr, 0, 0, spinnerWidth, spinnerHeight, SWP_NOMOVE | SWP_NOZORDER | SWP_NOREPOSITION | SWP_NOACTIVATE);
}


void CMainToolBar::Init(CMainFrame *pMainFrm)
{
	EnableFlatButtons(TrackerSettings::Instance().patternSetup & PatternSetup::FlatToolbarButtons);
	SetHorizontal();
	pMainFrm->DockControlBar(this);
}


void CMainToolBar::SetHorizontal()
{
	CToolBarEx::SetHorizontal();
	UpdateControls();
}


void CMainToolBar::SetVertical()
{
	CToolBarEx::SetVertical();
	UpdateControls();
}


void CMainToolBar::UpdateControls()
{
	const FlagSet<MainToolBarItem> visibleItems = TrackerSettings::Instance().mainToolBarVisibleItems.Get();

	SetButtonVisibility(FILE_NEW_INDEX, visibleItems[MainToolBarItem::IconsFile]);
	SetButtonVisibility(FILE_OPEN_INDEX, visibleItems[MainToolBarItem::IconsFile]);
	SetButtonVisibility(FILE_SAVE_INDEX, visibleItems[MainToolBarItem::IconsFile]);
	SetButtonVisibility(FILE_DIVIDER_INDEX, visibleItems[MainToolBarItem::IconsFile]);

	SetButtonVisibility(EDIT_CUT_INDEX, visibleItems[MainToolBarItem::IconsEdit]);
	SetButtonVisibility(EDIT_COPY_INDEX, visibleItems[MainToolBarItem::IconsEdit]);
	SetButtonVisibility(EDIT_PASTE_INDEX, visibleItems[MainToolBarItem::IconsEdit]);
	SetButtonVisibility(EDIT_DIVIDER_INDEX, visibleItems[MainToolBarItem::IconsEdit]);

	SetButtonVisibility(PLAY_MIDIRECORD_INDEX, visibleItems[MainToolBarItem::IconsPlayback]);
	SetButtonVisibility(PLAY_STOP_INDEX, visibleItems[MainToolBarItem::IconsPlayback]);
	SetButtonVisibility(PLAY_STARTPAUSE_INDEX, visibleItems[MainToolBarItem::IconsPlayback]);
	SetButtonVisibility(PLAY_RESTART_INDEX, visibleItems[MainToolBarItem::IconsPlayback]);
	SetButtonVisibility(PLAY_DIVIDER_INDEX, visibleItems[MainToolBarItem::IconsPlayback]);

	SetButtonVisibility(MISC_OPTIONS_INDEX, visibleItems[MainToolBarItem::IconsMisc]);
	SetButtonVisibility(MISC_PANIC_INDEX, visibleItems[MainToolBarItem::IconsMisc]);
	SetButtonVisibility(MISC_DIVIDER_INDEX, visibleItems.test_all(MainToolBarItem::IconsMisc | MainToolBarItem::VUMeter));

	UpdateControl(visibleItems[MainToolBarItem::Octave], m_EditOctave, EDITOCTAVE_INDEX, IDC_EDIT_BASEOCTAVE);
	UpdateControl(visibleItems[MainToolBarItem::Octave], m_SpinOctave, SPINOCTAVE_INDEX, IDC_SPIN_BASEOCTAVE);
	SetButtonVisibility(OCTAVE_DIVIDER_INDEX, m_bVertical && visibleItems[MainToolBarItem::Octave]);

	UpdateControl(visibleItems[MainToolBarItem::Tempo], m_StaticTempo, TEMPOTEXT_INDEX, IDC_TEXT_CURRENTTEMPO);
	UpdateControl(visibleItems[MainToolBarItem::Tempo], m_EditTempo, EDITTEMPO_INDEX, IDC_EDIT_CURRENTTEMPO);
	UpdateControl(visibleItems[MainToolBarItem::Tempo], m_SpinTempo, SPINTEMPO_INDEX, IDC_SPIN_CURRENTTEMPO);
	SetButtonVisibility(TEMPO_DIVIDER_INDEX, m_bVertical && visibleItems[MainToolBarItem::Tempo]);

	UpdateControl(visibleItems[MainToolBarItem::Speed], m_StaticSpeed, SPEEDTEXT_INDEX, IDC_TEXT_CURRENTSPEED);
	UpdateControl(visibleItems[MainToolBarItem::Speed], m_EditSpeed, EDITSPEED_INDEX, IDC_EDIT_CURRENTSPEED);
	UpdateControl(visibleItems[MainToolBarItem::Speed], m_SpinSpeed, SPINSPEED_INDEX, IDC_SPIN_CURRENTSPEED);
	SetButtonVisibility(SPEED_DIVIDER_INDEX, m_bVertical && visibleItems[MainToolBarItem::Speed]);

	UpdateControl(visibleItems[MainToolBarItem::RowsPerBeat], m_StaticRowsPerBeat, RPBTEXT_INDEX, IDC_TEXT_RPB);
	UpdateControl(visibleItems[MainToolBarItem::RowsPerBeat], m_EditRowsPerBeat, EDITRPB_INDEX, IDC_EDIT_RPB);
	UpdateControl(visibleItems[MainToolBarItem::RowsPerBeat], m_SpinRowsPerBeat, SPINRPB_INDEX, IDC_SPIN_RPB);
	SetButtonVisibility(RPB_DIVIDER_INDEX, m_bVertical && visibleItems[MainToolBarItem::RowsPerBeat]);

	UpdateControl(visibleItems[MainToolBarItem::GlobalVolume], m_StaticGlobalVolume, GLOBALVOLTEXT_INDEX, IDC_TEXT_GLOBALVOL);
	UpdateControl(visibleItems[MainToolBarItem::GlobalVolume], m_EditGlobalVolume, EDITGLOBALVOL_INDEX, IDC_EDIT_GLOBALVOL);
	UpdateControl(visibleItems[MainToolBarItem::GlobalVolume], m_SpinGlobalVolume, SPINGLOBALVOL_INDEX, IDC_SPIN_GLOBALVOL);
	SetButtonVisibility(GLOBALVOL_DIVIDER_INDEX, visibleItems.test_any_except(MainToolBarItem::VUMeter));

	m_VuMeter->SetOrientation(!m_bVertical);
	if(m_bVertical)
		m_VuMeter->SetWindowPos(nullptr, 0, 0, VUMETER_HEIGHT, VUMETER_HEIGHT, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
	else
		m_VuMeter->SetWindowPos(nullptr, 0, 0, VUMETER_WIDTH, VUMETER_HEIGHT, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
	UpdateControl(visibleItems[MainToolBarItem::VUMeter], m_VuMeter, VUMETER_INDEX, IDC_VUMETER, VUMETER_HEIGHT);
}


bool CMainToolBar::ToggleVisibility(MainToolBarItem item)
{
	FlagSet<MainToolBarItem> visibleItems = TrackerSettings::Instance().mainToolBarVisibleItems.Get();
	visibleItems.flip(item);
	// At least one icon group must be visible, otherwise the toolbar collapses
	if(!visibleItems[MainToolBarItem::AllIcons])
		visibleItems.flip(item);
	TrackerSettings::Instance().mainToolBarVisibleItems = visibleItems.value().as_enum();
	RefreshToolbar();
	return visibleItems[item];
}


uint32 CMainToolBar::GetBaseOctave() const
{
	if(m_currentOctave >= MIN_BASEOCTAVE) return (uint32)m_currentOctave;
	return 4;
}


void CMainToolBar::SetBaseOctave(uint32 octave)
{
	if(octave == static_cast<uint32>(m_currentOctave) || octave < MIN_BASEOCTAVE || octave > MAX_BASEOCTAVE)
		return;

	m_currentOctave = octave;
	mpt::uchar s[32];
	wsprintf(s, U_("Octave %d"), octave);
	m_EditOctave.SetWindowText(s);
	m_SpinOctave->SetPos(octave);
}


bool CMainToolBar::ShowUpdateInfo(const mpt::ustring &newVersion, const mpt::ustring &infoURL, bool showHighLight)
{
	GetToolBarCtrl().SetState(ID_UPDATE_AVAILABLE, TBSTATE_ENABLED);
	RefreshToolbar();

	// Trying to show the tooltip while the window is minimized hangs the application during TTM_TRACKACTIVATE.
	if(!showHighLight || CMainFrame::GetMainFrame()->IsIconic())
		return true;

	mfcless::Rect rect;
	GetToolBarCtrl().GetRect(ID_UPDATE_AVAILABLE, &rect);
	mfcless::Point pt = rect.CenterPoint();
	ClientToScreen(&pt);
	if(!IsFloating())
	{
		mfcless::Rect windowRect;
		CMainFrame::GetMainFrame()->GetWindowRect(windowRect);
		LimitMax(pt.x, windowRect.right);
	}

	return m_tooltip.ShowUpdate(*this, newVersion, infoURL, rect, pt, ID_UPDATE_AVAILABLE);
}


void CMainToolBar::RemoveUpdateInfo()
{
	if(m_tooltip)
		m_tooltip.Pop();
	GetToolBarCtrl().SetState(ID_UPDATE_AVAILABLE, TBSTATE_HIDDEN);
}


static void EnableEdit(CEdit &edit, CSpinButtonCtrl &spin, bool enable)
{
	// Avoid flicker when enabling / disabling controls
	edit.SetRedraw(false);
	if(!enable)
		edit.SetWindowText(U_("---"));
	edit.EnableWindow(enable ? true : false);
	edit.SetReadOnly(enable ? false : true);
	edit.SetRedraw(true);
	edit.Invalidate(false);
	spin.EnableWindow(enable ? true : false);
}


void CMainToolBar::SetCurrentSong(CSoundFile *pSndFile)
{
	// Update Info
	m_updating = true;
	const QWidget *focus = GetFocus();
	const FlagSet<MainToolBarItem> visibleItems = TrackerSettings::Instance().mainToolBarVisibleItems.Get();
	if(pSndFile)
	{
		// Update play/pause button
		if(m_currentTempo == TEMPO(0, 0))
			SetButtonInfo(PLAY_STARTPAUSE_INDEX, ID_PLAYER_PAUSE, TBBS_BUTTON, TOOLBAR_IMAGE_PAUSE);
		// Update Speed
		int nSpeed = pSndFile->m_PlayState.m_nMusicSpeed;
		if(nSpeed != m_currentSpeed && focus != &m_EditSpeed && visibleItems[MainToolBarItem::Speed])
		{
			if(m_currentSpeed < 0)
				EnableEdit(m_EditSpeed, m_SpinSpeed, true);

			m_currentSpeed = nSpeed;
			SetItemInt(IDC_EDIT_CURRENTSPEED, m_currentSpeed);
		}
		TEMPO nTempo = pSndFile->m_PlayState.m_nMusicTempo;
		if(nTempo != m_currentTempo && focus != &m_EditTempo && visibleItems[MainToolBarItem::Tempo])
		{
			if(m_currentTempo <= TEMPO(0, 0))
				EnableEdit(m_EditTempo, m_SpinTempo, true);

			m_currentTempo = nTempo;
			m_EditTempo->SetTempoValue(m_currentTempo);
		}
		int nRowsPerBeat = pSndFile->m_PlayState.m_nCurrentRowsPerBeat;
		if(nRowsPerBeat != m_currentRowsPerBeat && focus != &m_EditRowsPerBeat && visibleItems[MainToolBarItem::RowsPerBeat])
		{
			if(m_currentRowsPerBeat < 0)
				EnableEdit(m_EditRowsPerBeat, m_SpinRowsPerBeat, true);

			m_currentRowsPerBeat = nRowsPerBeat;
			SetItemInt(IDC_EDIT_RPB, m_currentRowsPerBeat);
		}
		int globalVol = pSndFile->m_PlayState.m_nGlobalVolume;
		if(globalVol != m_currentGlobalVolume && focus != &m_EditGlobalVolume && visibleItems[MainToolBarItem::GlobalVolume])
		{
			if(m_currentGlobalVolume < 0)
				EnableEdit(m_EditGlobalVolume, m_SpinGlobalVolume, true);

			m_currentGlobalVolume = globalVol;
			uint32 displayVolume = Util::muldivr_unsigned(m_currentGlobalVolume, pSndFile->GlobalVolumeRange(), MAX_GLOBAL_VOLUME);
			SetItemInt(IDC_EDIT_GLOBALVOL, displayVolume);
		}
	} else
	{
		if(m_currentTempo > TEMPO(0, 0))
		{
			EnableEdit(m_EditTempo, m_SpinTempo, false);
			SetButtonInfo(PLAY_STARTPAUSE_INDEX, ID_PLAYER_PLAY, TBBS_BUTTON, TOOLBAR_IMAGE_PLAY);
		}
		if(m_currentSpeed != -1)
			EnableEdit(m_EditSpeed, m_SpinSpeed, false);
		if(m_currentRowsPerBeat != -1)
			EnableEdit(m_EditRowsPerBeat, m_SpinRowsPerBeat, false);
		if(m_currentGlobalVolume != -1)
			EnableEdit(m_EditGlobalVolume, m_SpinGlobalVolume, false);

		m_currentTempo.Set(0);
		m_currentSpeed = -1;
		m_currentRowsPerBeat = -1;
		m_currentGlobalVolume = -1;
	}
	// If focus was on a now-disabled input field, move it somewhere else
	if(focus && !focus->IsWindowEnabled() && focus->GetParent() == this)
		CMainFrame::GetMainFrame()->SetFocus();

	m_updating = false;
}


void CMainToolBar::OnVScroll(mfcless::ScrollCode code, int pos, QWidget *pScrollBar)
{
	CMainFrame *pMainFrm;

	CToolBarEx::OnVScroll(nCode, nPos, pScrollBar);
	// Avoid auto-setting input focus to edit control
	if(auto *activeView = CMainFrame::GetMainFrame()->GetActiveView(); activeView != nullptr)
		activeView->SetFocus();
	else
		pScrollBar->SetFocus();

	short int oct = (short int)m_SpinOctave->GetPos();
	if((oct >= MIN_BASEOCTAVE) && ((int)oct != m_currentOctave))
	{
		SetBaseOctave(oct);
	}
	if((m_currentSpeed < 0) || (m_currentTempo <= TEMPO(0, 0))) return;
	if((pMainFrm = CMainFrame::GetMainFrame()) != nullptr)
	{
		CSoundFile *pSndFile = pMainFrm->GetSoundFilePlaying();
		if(pSndFile)
		{
			const auto &specs = pSndFile->GetModSpecifications();
			int n;
			if((n = mpt::signum(m_SpinTempo->GetPos())) != 0)
			{
				TEMPO newTempo;
				if(specs.hasFractionalTempo)
				{
					n *= TEMPO::fractFact;
					if(CMainFrame::GetMainFrame()->GetInputHandler()->CtrlPressed())
						n /= 100;
					else
						n /= 10;
					newTempo.SetRaw(n);
				} else
				{
					newTempo = TEMPO(n, 0);
				}
				newTempo += m_currentTempo;
				pSndFile->SetTempo(Clamp(newTempo, specs.GetTempoMin(), specs.GetTempoMax()), true);
				m_SpinTempo->SetPos(0);
			}
			if((n = mpt::signum(m_SpinSpeed->GetPos())) != 0)
			{
				pSndFile->m_PlayState.m_nMusicSpeed = Clamp(uint32(m_currentSpeed + n), specs.speedMin, specs.speedMax);
				m_SpinSpeed->SetPos(0);
			}
			if((n = m_SpinRowsPerBeat->GetPos()) != 0)
			{
				if(n < 0)
				{
					if(m_currentRowsPerBeat > 1)
						SetRowsPerBeat(m_currentRowsPerBeat - 1);
				} else if(static_cast<ROWINDEX>(m_currentRowsPerBeat) < pSndFile->m_PlayState.m_nCurrentRowsPerMeasure)
				{
						SetRowsPerBeat(m_currentRowsPerBeat + 1);
				}
				m_SpinRowsPerBeat->SetPos(0);
			}
			if((n = m_SpinGlobalVolume->GetPos()) != 0)
			{
				n = Util::muldiv(n, MAX_GLOBAL_VOLUME, pSndFile->GlobalVolumeRange());
				pSndFile->m_PlayState.m_nGlobalVolume = Clamp(pSndFile->m_PlayState.m_nGlobalVolume + n, 0, int(MAX_GLOBAL_VOLUME));
				m_SpinGlobalVolume->SetPos(0);
			}

			SetCurrentSong(pSndFile);
		}
	}
}


void CMainToolBar::OnSpeedChanged()
{
	if(CMainFrame *mainFrm = CMainFrame::GetMainFrame(); mainFrm && !m_updating)
	{
		bool ok = false;
		uint32 newSpeed = GetItemInt(IDC_EDIT_CURRENTSPEED);
		CSoundFile *sndFile = mainFrm->GetSoundFilePlaying();
		if(sndFile && ok)
		{
			const auto &specs = sndFile->GetModSpecifications();
			sndFile->m_PlayState.m_nMusicSpeed = Clamp(newSpeed, specs.speedMin, specs.speedMax);
		}
		m_currentSpeed = 0;  // Force display update once focus moves away from this input field
	}
}


void CMainToolBar::OnTempoChanged()
{
	if(CMainFrame *mainFrm = CMainFrame::GetMainFrame(); mainFrm && !m_updating)
	{
		TEMPO newTempo = m_EditTempo->GetTempoValue();
		CSoundFile *sndFile = mainFrm->GetSoundFilePlaying();
		if(sndFile && m_EditTempo->GetWindowTextLength() > 0)
		{
			const auto &specs = sndFile->GetModSpecifications();
			sndFile->m_PlayState.m_nMusicTempo = Clamp(newTempo, specs.GetTempoMin(), specs.GetTempoMax());
		}
		m_currentTempo.Set(0, 1);  // Force display update once focus moves away from this input field
	}
}


void CMainToolBar::OnRPBChanged()
{
	if(CMainFrame *mainFrm = CMainFrame::GetMainFrame(); mainFrm && !m_updating)
	{
		bool ok = false;
		uint32 newRPB = GetItemInt(IDC_EDIT_RPB);
		CSoundFile *sndFile = mainFrm->GetSoundFilePlaying();
		if(sndFile && ok && newRPB > 0)
		{
			SetRowsPerBeat(newRPB);
		}
		m_currentRowsPerBeat = -2;  // Force display update once focus moves away from this input field
	}
}


void CMainToolBar::OnGlobalVolChanged()
{
	if(CMainFrame *mainFrm = CMainFrame::GetMainFrame(); mainFrm && !m_updating)
	{
		bool ok = false;
		uint32 newGlobalVol = GetItemInt(IDC_EDIT_GLOBALVOL);
		CSoundFile *sndFile = mainFrm->GetSoundFilePlaying();
		if(sndFile && ok)
		{
			sndFile->m_PlayState.m_nGlobalVolume = Clamp(Util::muldivr_unsigned(newGlobalVol, MAX_GLOBAL_VOLUME, sndFile->GlobalVolumeRange()), uint32(0), MAX_GLOBAL_VOLUME);
		}
		m_currentGlobalVolume = -2;  // Force display update once focus moves away from this input field
	}
}


void CMainToolBar::OnTbnDropDownToolBar()
{
	NMTOOLBAR *pToolBar = reinterpret_cast<NMTOOLBAR *>(pNMHDR);
	ClientToScreen(&(pToolBar->rcButton));

	switch(pToolBar->iItem)
	{
	case ID_FILE_NEW:
		{
			auto *mainFrm = CMainFrame::GetMainFrame();
			auto [newMenu, newPos] = CMainFrame::FindMenuItemByCommand(*mainFrm->GetMenu(), ID_FILE_NEWIT);
			auto [templateMenu, templatePos] = CMainFrame::FindMenuItemByCommand(*mainFrm->GetMenu(), ID_FILE_OPENTEMPLATE);
			if(templateMenu)
				newMenu->AppendMenu(mfcless::menu::Popup, templateMenu, U_("&Templates"));
			newMenu->TrackPopupMenu(mfcless::menu::LeftAlign | mfcless::menu::RightButton, pToolBar->rcButton.left, pToolBar->rcButton.bottom, this);
			if(templateMenu)
				newMenu->RemoveMenu(newMenu->GetMenuItemCount() - 1, mfcless::menu::ByPosition);
		}
		break;
	case ID_MIDI_RECORD:
		// Show a list of MIDI devices
		{
			mfcless::Menu hMenu = mfcless::Menu::CreatePopup();
			MIDIINCAPS mic;
			uint32 numDevs = midiInGetNumDevs();
			if(numDevs > MAX_MIDI_DEVICES) numDevs = MAX_MIDI_DEVICES;
			uint32 current = TrackerSettings::Instance().GetCurrentMIDIDevice();
			for(uint32 i = 0; i < numDevs; i++)
			{
				mic.szPname[0] = 0;
				if(midiInGetDevCaps(i, &mic, sizeof(mic)) == MMSYSERR_NOERROR)
				{
					hMenu.AppendMenu(mfcless::menu::String | (i == current ? mfcless::menu::Checked : 0), ID_SELECT_MIDI_DEVICE + i, theApp.GetFriendlyMIDIPortName(mpt::String::ReadCStringBuf(mic.szPname), true));
				}
			}
			if(!numDevs)
			{
				hMenu.AppendMenu(mfcless::menu::String | mfcless::menu::Grayed, 0, U_("No MIDI input devices found"));
			}
			hMenu.TrackPopupMenu(mfcless::menu::LeftAlign | mfcless::menu::RightButton, pToolBar->rcButton.left, pToolBar->rcButton.bottom, this);
			hMenu.DestroyMenu();
		}
		break;
	}

	CurrentNotify().result = 0;
}


void CMainToolBar::OnSelectMIDIDevice(uint32 id)
{
	CMainFrame::GetMainFrame()->midiCloseDevice();
	TrackerSettings::Instance().SetMIDIDevice(id - ID_SELECT_MIDI_DEVICE);
	CMainFrame::GetMainFrame()->midiOpenDevice();
}


void CMainToolBar::SetRowsPerBeat(ROWINDEX newRPB)
{
	CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
	if(pMainFrm == nullptr)
		return;
	CModDoc *pModDoc = pMainFrm->GetModPlaying();
	CSoundFile *pSndFile = pMainFrm->GetSoundFilePlaying();
	if(pModDoc == nullptr || pSndFile == nullptr)
		return;

	CriticalSection cs;
	PATTERNINDEX pat = pSndFile->GetCurrentPattern();
	bool modified = false;
	if(pSndFile->Patterns.IsValidPat(pat) && pSndFile->Patterns[pat].GetOverrideSignature())
	{
		CPattern &pattern = pSndFile->Patterns[pat];
		if(newRPB <= pattern.GetRowsPerMeasure())
		{
			pattern.SetSignature(newRPB, pattern.GetRowsPerMeasure());
			TempoSwing swing = pattern.GetTempoSwing();
			if(!swing.empty())
			{
				swing.resize(newRPB);
				pattern.SetTempoSwing(swing);
			}
			modified = true;
		}
	} else
	{
		if(newRPB <= pSndFile->m_nDefaultRowsPerMeasure)
		{
			pSndFile->m_nDefaultRowsPerBeat = newRPB;
			if(!pSndFile->m_tempoSwing.empty())
				pSndFile->m_tempoSwing.resize(newRPB);
			modified = true;
		}
	}

	// Update pattern editor
	if(modified)
	{
		pSndFile->m_PlayState.m_nCurrentRowsPerBeat = newRPB;
		cs.Leave();
		pModDoc->SetModified();
		pModDoc->UpdateAllViews(PatternHint().Data());
	}
}


bool CMainToolBar::OnToolTipText(uint32 )
{
	auto pTTT = reinterpret_cast<TOOLTIPTEXT *>(pNMHDR);
	uintptr_t id = pNMHDR->idFrom;
	if(pTTT->uFlags & TTF_IDISHWND)
	{
		// idFrom is actually the QWidget * of the tool
		id = (uintptr_t)::GetDlgCtrlID((QWidget *)id);
	}

	const mpt::uchar *s = nullptr;
	CommandID cmd = kcNull;
	switch(id)
	{
	case ID_FILE_NEW: s = U_("New"); cmd = kcFileNew; break;
	case ID_FILE_OPEN: s = U_("Open"); cmd = kcFileOpen; break;
	case ID_FILE_SAVE: s = U_("Save"); cmd = kcFileSave; break;
	case ID_EDIT_CUT: s = U_("Cut"); cmd = kcEditCut; break;
	case ID_EDIT_COPY: s = U_("Copy"); cmd = kcEditCopy; break;
	case ID_EDIT_PASTE: s = U_("Paste"); cmd = kcEditPaste; break;
	case ID_MIDI_RECORD: s = U_("MIDI Record"); cmd = kcMidiRecord; break;
	case ID_PLAYER_STOP: s = U_("Stop"); cmd = kcStopSong; break;
	case ID_PLAYER_PLAY: s = U_("Play"); cmd = kcPlayPauseSong; break;
	case ID_PLAYER_PAUSE: s = U_("Pause"); cmd = kcPlayPauseSong; break;
	case ID_PLAYER_PLAYFROMSTART: s = U_("Play From Start"); cmd = kcPlaySongFromStart; break;
	case ID_VIEW_OPTIONS: s = U_("Setup"); cmd = kcViewOptions; break;
	case ID_PANIC: s = U_("Stop all hanging plugin and sample voices"); cmd = kcPanic; break;
	case ID_UPDATE_AVAILABLE: s = U_("A new update is available."); break;
	}

	if(s == nullptr)
		return false;
	
	mpt::ustring fmt = s;
	if(cmd != kcNull)
	{
		auto keyText = CMainFrame::GetInputHandler()->m_activeCommandSet->GetKeyTextFromCommand(cmd, 0);
		if(!keyText.IsEmpty())
			fmt += MPT_UFORMAT(" ({})")(keyText);
	}
	mpt::String::WriteWinBuf(pTTT->szText) = fmt;
	CurrentNotify().result = 0;

	// bring the tooltip window above other popup windows
	::SetWindowPos(pNMHDR->hwndFrom, HWND_TOP, 0, 0, 0, 0,
		SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOMOVE | SWP_NOOWNERZORDER);

	return true;    // message was handled
}


///////////////////////////////////////////////////////////////////////////////////////////////////
// CModTreeBar

void CModTreeBar::RegisterMessageMap()
{
	mfcless::Window<QWidget>::RegisterMessageMap();
	// ON_WM_NCCALCSIZE(): by hand (Qt event override)
	// ON_WM_NCPAINT(): by hand (Qt event override)
	// ON_WM_NCHITTEST(): by hand (Qt event override)
	// ON_WM_SIZE() (mfcless::Window)
	// ON_WM_NCMOUSEMOVE(): by hand (Qt event override)
	// ON_WM_MOUSEMOVE() (mfcless::Window)
	// ON_WM_LBUTTONDOWN() (mfcless::Window)
	// ON_WM_NCLBUTTONDOWN(): by hand (Qt event override)
	// ON_WM_LBUTTONUP() (mfcless::Window)
	// ON_WM_NCLBUTTONUP(): by hand (Qt event override)
	// ON_WM_TIMER() (mfcless::Window)
	AddMessage(WM_INITDIALOG, &CModTreeBar::OnInitDialog);
	AddNotify(IDC_EDIT1, mfcless::Notify::EditChange, &CModTreeBar::OnFilterChanged);
	AddNotify(IDC_EDIT1, mfcless::Notify::KillFocus, &CModTreeBar::OnFilterLostFocus);
	AddCommand(ID_CLOSE_LIBRARY_FILTER, &CModTreeBar::CloseTreeFilter);
}


CModTreeBar::CModTreeBar()
{
	m_nTreeSplitRatio = TrackerSettings::Instance().glTreeSplitRatio;
}


intptr_t CModTreeBar::OnInitDialog(uintptr_t wParam, intptr_t lParam)
{
	intptr_t l = mfcless::Window<QWidget>::HandleInitDialog(wParam, lParam);
	m_pModTreeData = new CModTree(nullptr);
	m_pModTreeData->SubclassDlgItem(IDC_TREEDATA, this);
	m_pModTree = new CModTree(m_pModTreeData);
	m_pModTree->SubclassDlgItem(IDC_TREEVIEW, this);
	m_status.reset();
	m_sizeDefault.cx = HighDPISupport::ScalePixels(TrackerSettings::Instance().glTreeWindowWidth, m_hWnd) + Padding();
	m_sizeDefault.cy = 32767;
	return l;
}


CModTreeBar::~CModTreeBar()
{
	delete m_pModTree;
	m_pModTree = nullptr;
	delete m_pModTreeData;
	m_pModTreeData = nullptr;
}


void CModTreeBar::Init()
{
	m_nTreeSplitRatio = TrackerSettings::Instance().glTreeSplitRatio;
	if(m_pModTree)
	{
		m_pModTreeData->Init();
		m_pModTree->Init();
	}
}


bool CModTreeBar::PreTranslateMessage(mfcless::Message *pMsg)
{
	if(m_filterEdit && pMsg->hwnd == m_filterEdit->m_hWnd && pMsg->message == mfcless::msg::KeyDown && m_filterSource != nullptr)
	{
		switch(pMsg->wParam)
		{
		case mfcless::vk::RETURN:
			if(const auto modItem = m_filterSource->GetModItem(m_filterSource->GetSelectedItem()); (modItem.type == CModTree::MODITEM_INSLIB_FOLDER || modItem.type == CModTree::MODITEM_INSLIB_SONG))
			{
				m_filterSource->PostMessage(WM_COMMAND, ID_MODTREE_EXECUTE);
			}
			[[fallthrough]];
		case mfcless::vk::ESCAPE:
			CloseTreeFilter();
			return true;

		case mfcless::vk::TAB:
			if(m_filterSource)
			{
				m_filterSource->SetFocus();
				return true;
			}
			break;

		case mfcless::vk::UP:
		case mfcless::vk::DOWN:
			if(const auto selectedItem = m_filterSource->GetSelectedItem(); selectedItem != nullptr)
			{
				const auto item = m_filterSource->GetNextItem(selectedItem, (pMsg->wParam == mfcless::vk::UP) ? TVGN_PREVIOUS : TVGN_NEXT);
				if(item)
					m_filterSource->SelectItem(item);
				return true;
			}
			break;
		}
	}
	return WindowBase::PreTranslateMessage(pMsg);
}


void CModTreeBar::DelayShow(bool show)
{
	if(!show && IsChild(GetFocus()))
		CMainFrame::GetMainFrame()->SetFocus();
	mfcless::Window<QWidget>::DelayShow(show);
}


void CModTreeBar::RefreshDlsBanks()
{
	if(m_pModTree) m_pModTree->RefreshDlsBanks();
}


void CModTreeBar::RefreshMidiLibrary()
{
	if(m_pModTree) m_pModTree->RefreshMidiLibrary();
}


void CModTreeBar::OnOptionsChanged()
{
	if(m_pModTree) m_pModTree->OnOptionsChanged();
}


int CModTreeBar::Padding() const
{
	return HighDPISupport::ScalePixels(3, m_hWnd);
}


void CModTreeBar::RecalcLayout()
{
	mfcless::Rect rect;

	if((m_pModTree) && (m_pModTreeData))
	{
		int cytree, cydata, cyavail;

		GetClientRect(&rect);
		const int padding = Padding();
		cyavail = rect.Height() - padding;
		if(cyavail < 0) cyavail = 0;
		cytree = (cyavail * m_nTreeSplitRatio) >> 8;
		cydata = cyavail - cytree;
		auto dwp = ::BeginDeferWindowPos(3);
		if(m_filterSource == m_pModTree)
		{
			int editHeight = HighDPISupport::ScalePixels(20, m_hWnd);
			::DeferWindowPos(dwp, *m_pModTree, nullptr, 0, 0, rect.Width(), cytree - editHeight, SWP_NOZORDER | SWP_NOACTIVATE);
			::DeferWindowPos(dwp, *m_pModTreeData, nullptr, 0, cytree + padding, rect.Width(), cydata, SWP_NOZORDER | SWP_NOACTIVATE);
			::DeferWindowPos(dwp, m_filterEdit, *m_pModTree, 0, cytree - editHeight, rect.Width(), editHeight, SWP_NOACTIVATE);
		} else if(m_filterSource == m_pModTreeData)
		{
			int editHeight = HighDPISupport::ScalePixels(20, m_hWnd);
			::DeferWindowPos(dwp, *m_pModTree, nullptr, 0, 0, rect.Width(), cytree, SWP_NOZORDER | SWP_NOACTIVATE);
			::DeferWindowPos(dwp, *m_pModTreeData, nullptr, 0, cytree + padding, rect.Width(), cydata - editHeight, SWP_NOZORDER | SWP_NOACTIVATE);
			::DeferWindowPos(dwp, m_filterEdit, *m_pModTreeData, 0, cytree + padding + cydata - editHeight, rect.Width(), editHeight, SWP_NOACTIVATE);
		} else
		{
			::DeferWindowPos(dwp, *m_pModTree, nullptr, 0, 0, rect.Width(), cytree, SWP_NOZORDER | SWP_NOACTIVATE);
			::DeferWindowPos(dwp, *m_pModTreeData, nullptr, 0, cytree + padding, rect.Width(), cydata, SWP_NOZORDER | SWP_NOACTIVATE);
		}
		::EndDeferWindowPos(dwp);
	}
}


mfcless::Size CModTreeBar::CalcFixedLayout(bool, bool)
{
	int width = HighDPISupport::ScalePixels(TrackerSettings::Instance().glTreeWindowWidth, m_hWnd);
	mfcless::Size sz;
	m_sizeDefault.cx = width;
	m_sizeDefault.cy = 32767;
	const int padding = Padding();
	sz.cx = width + padding;
	if(sz.cx < padding + 1)
		sz.cx = padding + 1;
	sz.cy = 32767;
	return sz;
}


void CModTreeBar::DoMouseMove(mfcless::Point pt)
{
	mfcless::Rect rect;
	GetClientRect(&rect);

	if(m_status[MTB_CAPTURE | MTB_DRAGGING] && (::GetCapture() != m_hWnd))
	{
		CancelTracking();
	}
	if(m_status[MTB_DRAGGING])
	{
		if(m_status[MTB_VERTICAL])
		{
			if(m_pModTree)
			{
				mfcless::Rect windowRect;
				m_pModTree->GetWindowRect(&windowRect);
				pt.y += windowRect.Height();
			}
			pt.y -= ptDragging.y;
			pt.y = std::clamp(static_cast<int>(pt.y), 0, rect.Height());
			if(!m_status[MTB_TRACKER] || (pt.y != static_cast<int>(m_nTrackPos)))
			{
				if(m_status[MTB_TRACKER])
					OnInvertTracker(m_nTrackPos);
				m_nTrackPos = pt.y;
				OnInvertTracker(m_nTrackPos);
				m_status.set(MTB_TRACKER);
			}
		} else
		{
			pt.x -= ptDragging.x;
			if(BarOnLeft())
				pt.x += (m_cxOriginal - Padding());
			else
				pt.x = m_cxOriginal - pt.x;
			pt.x = std::max(pt.x, int32(0));
			if(!m_status[MTB_TRACKER] || (pt.x != static_cast<int>(m_nTrackPos)))
			{
				if(m_status[MTB_TRACKER])
					OnInvertTracker(m_nTrackPos);
				m_nTrackPos = pt.x;
				OnInvertTracker(m_nTrackPos);
				m_status.set(MTB_TRACKER);
			}
		}
	} else
	{
		uint32 nCursor = 0;

		const int padding = Padding();
		const int extraPadding = HighDPISupport::ScalePixels(2, m_hWnd);
		if(BarOnLeft())
		{
			rect.left = rect.right - extraPadding;
			rect.right = rect.left + padding + extraPadding;
		} else
		{
			rect.left -= extraPadding;
			rect.right = rect.left + padding + extraPadding;
		}
		if(rect.PtInRect(pt))
		{
			nCursor = AFX_IDC_HSPLITBAR;
		} else
		if(m_pModTree)
		{
			m_pModTree->GetWindowRect(&rect);
			rect.right = rect.Width();
			rect.left = 0;
			rect.top = rect.Height() - extraPadding;
			rect.bottom = rect.top + padding + extraPadding;
			if(rect.PtInRect(pt))
			{
				nCursor = AFX_IDC_VSPLITBAR;
			}
		}
		if(nCursor)
		{
			const bool wasVertical = m_status[MTB_VERTICAL];
			bool vertical = (nCursor == AFX_IDC_VSPLITBAR);
			bool setCursor = false;
			if(!m_status[MTB_CAPTURE])
			{
				m_status.set(MTB_CAPTURE);
				SetCapture();
				setCursor = true;
			} else
			{
				if(vertical != wasVertical)
					setCursor = true;
			}
			m_status.set(MTB_VERTICAL, vertical);
			if(setCursor)
				SetCursor(theApp.LoadCursor(nCursor));
		} else
		{
			if(m_status[MTB_CAPTURE])
			{
				m_status.reset(MTB_CAPTURE);
				ReleaseCapture();
				SetCursor(LoadCursor(NULL, IDC_ARROW));
			}
		}
	}
}


void CModTreeBar::DoLButtonDown(mfcless::Point pt)
{
	if(m_status[MTB_CAPTURE] && !m_status[MTB_DRAGGING])
	{
		mfcless::Rect rect;
		GetWindowRect(&rect);
		m_cxOriginal = rect.Width();
		m_cyOriginal = rect.Height();
		ptDragging = pt;
		m_status.set(MTB_DRAGGING);
		DoMouseMove(pt);
	}
}


void CModTreeBar::DoLButtonUp()
{
	if(m_status[MTB_DRAGGING])
	{
		mfcless::Rect rect;

		m_status.reset(MTB_DRAGGING);
		if(m_status[MTB_TRACKER])
		{
			OnInvertTracker(m_nTrackPos);
			m_status.reset(MTB_TRACKER);
		}
		const int padding = Padding();
		if(m_status[MTB_VERTICAL])
		{
			GetClientRect(&rect);
			int cyavail = rect.Height() - padding;
			if(cyavail < padding + 1)
				cyavail = padding + 1;
			int ratio = std::clamp(static_cast<int>(m_nTrackPos * 256) / cyavail, 0, 256);
			m_nTreeSplitRatio = ratio;
			TrackerSettings::Instance().glTreeSplitRatio = ratio;
			RecalcLayout();
		} else
		{
			GetWindowRect(&rect);
			m_nTrackPos += padding;
			if(m_nTrackPos < static_cast<uint32>(padding + 1))
				m_nTrackPos = padding + 1;
			CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
			if((m_nTrackPos != (uint32)rect.Width()) && (pMainFrm))
			{
				TrackerSettings::Instance().glTreeWindowWidth = HighDPISupport::ScalePixelsInv(m_nTrackPos - padding, m_hWnd);
				m_sizeDefault.cx = m_nTrackPos;
				m_sizeDefault.cy = 32767;
				pMainFrm->RecalcLayout();
			}
		}
	}
}


void CModTreeBar::CancelTracking()
{
	if(m_status[MTB_TRACKER])
	{
		OnInvertTracker(m_nTrackPos);
		m_status.reset(MTB_TRACKER);
	}
	m_status.reset(MTB_DRAGGING);
	if(m_status[MTB_CAPTURE])
	{
		m_status.reset(MTB_CAPTURE);
		ReleaseCapture();
	}
}


void CModTreeBar::OnInvertTracker(uint32 x)
{
	CMainFrame *pMainFrm = CMainFrame::GetMainFrame();

	if(pMainFrm)
	{
		mfcless::Rect rect;

		GetClientRect(&rect);
		const int padding = Padding() + 1;
		if(m_status[MTB_VERTICAL])
		{
			rect.top = x;
			rect.bottom = rect.top + padding;
		} else
		{
			if(BarOnLeft())
				rect.left = x;
			else
				rect.left = rect.right - x;
			rect.right = rect.left + padding;
		}
		ClientToScreen(&rect);
		pMainFrm->ScreenToClient(&rect);

		// pat-blt without clip children on
		mfcless::DC* pDC = pMainFrm->GetDC();
		// invert the brush pattern (looks just like frame window sizing)
		mfcless::Brush* pBrush = mfcless::DC::GetHalftoneBrush();
		mfcless::Brush hOldBrush;
		if(pBrush != nullptr)
			hOldBrush = (mfcless::Brush)SelectObject(pDC->m_hDC, pBrush->m_hObject);
		pDC->PatBlt(rect.left, rect.top, rect.Width(), rect.Height(), mfcless::rop::PatInvert);
		if(hOldBrush != nullptr)
			SelectObject(pDC->m_hDC, hOldBrush);
		ReleaseDC(pDC);
	}
}


void CModTreeBar::OnDocumentCreated(CModDoc *pModDoc)
{
	if(m_pModTree && pModDoc) m_pModTree->AddDocument(*pModDoc);
}


void CModTreeBar::OnDocumentClosed(CModDoc *pModDoc)
{
	if(m_pModTree && pModDoc) m_pModTree->RemoveDocument(*pModDoc);
}


void CModTreeBar::OnUpdate(CModDoc *pModDoc, UpdateHint hint, QObject *pHint)
{
	if(m_pModTree) m_pModTree->OnUpdate(pModDoc, hint, pHint);
}


void CModTreeBar::UpdatePlayPos(CModDoc *pModDoc, Notification *pNotify)
{
	if(m_pModTree && pModDoc) m_pModTree->UpdatePlayPos(*pModDoc, pNotify);
}


void CModTreeBar::SetBarOnLeft(const bool left)
{
	SetBarStyle(left ? CBRS_LEFT : CBRS_RIGHT);
	SetWindowPos(nullptr, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOZORDER | SWP_NOSIZE | SWP_FRAMECHANGED | SWP_DRAWFRAME);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// CModTreeBar message handlers

void CModTreeBar::OnNcCalcSize(bool bCalcValidRects, mfcless::Rect *lpncsp)
{
	mfcless::Window<QWidget>::OnNcCalcSize(bCalcValidRects, lpncsp);
	if(lpncsp)
	{
		if(BarOnLeft())
			lpncsp->rgrc[0].right -= Padding();
		else
			lpncsp->rgrc[0].left += Padding();
		lpncsp->rgrc[0].right = std::max(lpncsp->rgrc[0].left, lpncsp->rgrc[0].right);
	}
}


intptr_t CModTreeBar::OnNcHitTest(mfcless::Point point)
{
	mfcless::Rect rect;

	GetWindowRect(&rect);
	rect.DeflateRect(1, 1);
	if(BarOnLeft())
		rect.right -= Padding();
	else
		rect.left += Padding();
	if(!rect.PtInRect(point))
		return HTBORDER;
	return mfcless::Window<QWidget>::OnNcHitTest(point);
}


void CModTreeBar::OnNcPaint()
{
	mfcless::Rect rect;
	mfcless::Window<QWidget>::OnNcPaint();

	GetWindowRect(&rect);
	// Assumes there is no other non-client items
	rect.right -= rect.left;
	rect.bottom -= rect.top;
	rect.left = 0;
	rect.top = 0;
	if(BarOnLeft())
		rect.left = rect.right - Padding();
	else
		rect.right = rect.left + Padding();
	if((rect.left < rect.right) && (rect.top < rect.bottom))
	{
		mfcless::DC *pDC = GetWindowDC();
		mfcless::DC * hdc = pDC->m_hDC;
		FillRect(hdc, &rect, GetSysColorBrush(COLOR_BTNFACE));
		ReleaseDC(pDC);
	}
}


void CModTreeBar::OnSize(uint32 nType, int cx, int cy)
{
	Window::OnSize(nType, cx, cy);
	RecalcLayout();
}


void CModTreeBar::OnNcMouseMove(uint32, mfcless::Point point)
{
	mfcless::Rect rect;
	mfcless::Point pt = point;

	GetWindowRect(&rect);
	pt.x -= rect.left;
	pt.y -= rect.top;
	DoMouseMove(pt);
}


void CModTreeBar::OnMouseMove(uint32, mfcless::Point point)
{
	DoMouseMove(point);
}


void CModTreeBar::OnNcLButtonDown(uint32, mfcless::Point point)
{
	mfcless::Rect rect;
	mfcless::Point pt = point;

	GetWindowRect(&rect);
	pt.x -= rect.left;
	pt.y -= rect.top;
	DoLButtonDown(pt);
}


void CModTreeBar::OnLButtonDown(uint32, mfcless::Point point)
{
	DoLButtonDown(point);
}


void CModTreeBar::OnNcLButtonUp(uint32, mfcless::Point)
{
	DoLButtonUp();
}


void CModTreeBar::OnLButtonUp(uint32, mfcless::Point)
{
	DoLButtonUp();
}


bool CModTreeBar::SetTreeSoundfile(FileReader &file)
{
	return m_pModTree->SetSoundFile(file);
}


void CModTreeBar::StartTreeFilter(CModTree &source)
{
	if(!m_filterEdit)
	{
		mfcless::Rect rect;
		GetClientRect(rect);
		rect.bottom = HighDPISupport::ScalePixels(20, m_hWnd);

		m_pModTree->GetItemRect(m_pModTree->GetFirstVisibleItem(), rect, false);
		m_filterEdit->Create(WS_VISIBLE | WS_CHILD | WS_BORDER | WS_TABSTOP | ES_LEFT | ES_AUTOHSCROLL, rect, this, IDC_EDIT1);
		m_filterEdit->SetFont(GetFont());
	} else if(m_filterSource != &source)
	{
		m_filterEdit->SetWindowText(U_(""));
	}
	m_filterEdit->setFocus();
	m_filterSource = &source;
	RecalcLayout();
}


void CModTreeBar::OnFilterChanged()
{
	if(!m_filterSource)
		return;

	mpt::ustring filter;
	m_filterEdit->GetWindowText(filter);
	if(filter.GetLength() < 1 || filter.GetLength() > 2)
	{
		CancelTimer();
		m_filterSource->SetInstrumentLibraryFilter(mpt::ToUnicode(filter));
	} else
	{
		if(!m_filterTimer)
			m_filterTimer = SetTimer(1, 360 - filter.GetLength() * 120, nullptr);
	}
}


void CModTreeBar::OnTimer(uintptr_t id)
{
	if(id != m_filterTimer)
		return;

	if(m_filterSource)
	{
		mpt::ustring filter;
		m_filterEdit->GetWindowText(filter);
		m_filterSource->SetInstrumentLibraryFilter(mpt::ToUnicode(filter));
	}
	CancelTimer();
}


void CModTreeBar::OnFilterLostFocus()
{
	if(m_filterEdit && !m_filterEdit->GetWindowTextLength())
		CloseTreeFilter();
}


void CModTreeBar::CloseTreeFilter()
{
	CancelTimer();
	if(m_filterSource)
	{
		m_filterSource->SetInstrumentLibraryFilter({});
		if(GetFocus() == &m_filterEdit)
			m_filterSource->SetFocus();
		m_filterSource = nullptr;
	}
	if(m_filterEdit)
	{
		m_filterEdit->DestroyWindow();
		RecalcLayout();
	}
}


void CModTreeBar::CancelTimer()
{
	if(m_filterTimer)
	{
		KillTimer(m_filterTimer);
		m_filterTimer = 0;
	}
}


////////////////////////////////////////////////////////////////////////////////
//
// Stereo VU Meter for toolbar
//

void CStereoVU::RegisterMessageMap()
{
	mfcless::Window<QLabel>::RegisterMessageMap();
	// ON_WM_PAINT() (mfcless::Window)
	// ON_WM_LBUTTONDOWN() (mfcless::Window)
}


void CStereoVU::OnPaint()
{
	mfcless::Rect rect;
	mfcless::PaintDC dc(this);
	DrawVuMeters(dc, true);
}


void CStereoVU::SetVuMeter(uint8 validChannels, const uint32 channels[4], bool force)
{
	bool changed = false;
	if(validChannels == 0)
	{
		// reset
		validChannels = numChannels;
	} else if(validChannels != numChannels)
	{
		changed = true;
		force = true;
		numChannels = validChannels;
		allowRightToLeft = (numChannels > 2);
	}
	for(uint8 c = 0; c < validChannels; ++c)
	{
		if(vuMeter[c] != channels[c])
		{
			changed = true;
		}
	}
	if(changed)
	{
		uint32 curTime = mfcless::TickCount();
		if(curTime - lastVuUpdateTime >= TrackerSettings::Instance().VuMeterUpdateInterval || force)
		{
			for(uint8 c = 0; c < validChannels; ++c)
			{
				vuMeter[c] = channels[c];
			}
			mfcless::PaintDC dc(this);
			DrawVuMeters(dc, force);
			lastVuUpdateTime = curTime;
		}
	}
}


// Draw stereo VU
void CStereoVU::DrawVuMeters(mfcless::DC &dc, bool redraw)
{
	mfcless::Rect rect;
	GetClientRect(&rect);

	if(redraw)
	{
		dc.FillSolidRect(rect.left, rect.top, rect.Width(), rect.Height(), mfcless::Rgb(0,0,0));
	}

	for(uint8 channel = 0; channel < numChannels; ++channel)
	{
		mfcless::Rect chanrect = rect;
		if(horizontal)
		{
			if(allowRightToLeft)
			{
				const int col = channel % 2;
				const int row = channel / 2;

				float width = (rect.Width() - 2.0f) / 2.0f;
				float height = rect.Height() / float(numChannels/2);

				chanrect.top = mpt::saturate_round<int32>(rect.top + height * row);
				chanrect.bottom = mpt::saturate_round<int32>(chanrect.top + height) - 1;
				
				chanrect.left = mpt::saturate_round<int32>(rect.left + width * col) + ((col == 1) ? 2 : 0);
				chanrect.right = mpt::saturate_round<int32>(chanrect.left + width) - 1;

			} else
			{
				float height = rect.Height() / float(numChannels);
				chanrect.top = mpt::saturate_round<int32>(rect.top + height * channel);
				chanrect.bottom = mpt::saturate_round<int32>(chanrect.top + height) - 1;
			}
		} else
		{
			float width = rect.Width() / float(numChannels);
			chanrect.left = mpt::saturate_round<int32>(rect.left + width * channel);
			chanrect.right = mpt::saturate_round<int32>(chanrect.left + width) - 1;
		}
		DrawVuMeter(dc, chanrect, channel, redraw);
	}

}


// Draw a single VU Meter
void CStereoVU::DrawVuMeter(mfcless::DC &dc, const mfcless::Rect &rect, int index, bool redraw)
{
	uint32 vu = vuMeter[index];

	if(CMainFrame::GetMainFrame()->GetSoundFilePlaying() == nullptr)
	{
		vu = 0;
	}

	const bool clip = (vu & Notification::ClipVU) != 0;
	vu = (vu & (~Notification::ClipVU)) >> 8;

	if(horizontal)
	{
		const bool rtl = allowRightToLeft && ((index % 2) == 0);

		const int cx = std::max(1, rect.Width());
		int v = (vu * cx) >> 8;

		for(int x = 0; x <= cx; x += 2)
		{
			int pen = Clamp((x * NUM_VUMETER_PENS) / cx, 0, NUM_VUMETER_PENS - 1);
			const bool last = (x == (cx & ~0x1));

			// Darken everything above volume, unless it's the clip indicator
			if(v <= x && (!last || !clip))
				pen += NUM_VUMETER_PENS;

			bool draw = redraw || (v < lastV[index] && v<=x && x<=lastV[index]) || (lastV[index] < v && lastV[index]<=x && x<=v);
			draw = draw || (last && clip != lastClip[index]);
			if(draw) dc.FillSolidRect(
				((!rtl) ? (rect.left + x) : (rect.right - x)),
				rect.top, 1, rect.Height(), CMainFrame::gcolrefVuMeter[pen]);
			if(last) lastClip[index] = clip;
		}
		lastV[index] = v;
	} else
	{
		const int cy = std::max(1, rect.Height());
		int v = (vu * cy) >> 8;

		for(int ry = rect.bottom - 1; ry >= rect.top; ry -= 2)
		{
			const int y0 = rect.bottom - ry;
			int pen = Clamp((y0 * NUM_VUMETER_PENS) / cy, 0, NUM_VUMETER_PENS - 1);
			const bool last = (ry <= rect.top + 1);

			// Darken everything above volume, unless it's the clip indicator
			if(v <= y0 && (!last || !clip))
				pen += NUM_VUMETER_PENS;

			bool draw = redraw || (v < lastV[index] && v<=y0 && y0<=lastV[index]) || (lastV[index] < v && lastV[index]<=y0 && y0<=v);
			draw = draw || (last && clip != lastClip[index]);
			if(draw) dc.FillSolidRect(rect.left, ry, rect.Width(), 1, CMainFrame::gcolrefVuMeter[pen]);
			if(last) lastClip[index] = clip;
		}
		lastV[index] = v;
	}
}


void CStereoVU::OnLButtonDown(uint32, mfcless::Point)
{
	// Reset clip indicator.
	CMainFrame::GetMainFrame()->m_VUMeterInput.ResetClipped();
	CMainFrame::GetMainFrame()->m_VUMeterOutput.ResetClipped();
}


OPENMPT_NAMESPACE_END
