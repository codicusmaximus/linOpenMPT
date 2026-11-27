/*
 * Mainbar.cpp
 * -----------
 * Purpose: Implementation of OpenMPT's window toolbar and parent container of the tree view.
 * Notes  : mfcless twin of upstream's mptrack/Mainbar.cpp.
 *          Hand-edited from the twin.py draft.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "Mainbar.h"
#include "mfcless/Widgets.h"
#include <QtCore/QRegularExpression>
#include <QtGui/QFontMetrics>
#include <QtGui/QRegularExpressionValidator>
#include <QtWidgets/QMenu>
#include <QtWidgets/QSplitter>
#include <QtWidgets/QToolButton>
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
	if(nChar >= u8'0' + MIN_BASEOCTAVE && nChar <= u8'0' + MAX_BASEOCTAVE)
		m_owner.SetBaseOctave(nChar - u8'0');
	else if(nChar == u8'+' && m_owner.GetBaseOctave() < MAX_BASEOCTAVE)
		m_owner.SetBaseOctave(m_owner.GetBaseOctave() + 1);
	else if(nChar == u8'-' && m_owner.GetBaseOctave() > MIN_BASEOCTAVE)
		m_owner.SetBaseOctave(m_owner.GetBaseOctave() - 1);
}

/////////////////////////////////////////////////////////////////////
// CToolBarEx: custom toolbar base class
// mfcless: a QToolBar whose items are kept by index, as CToolBar's buttons
// are: buttons (actions with a command ID), separators and controls.

CToolBarEx::CToolBarEx(QWidget *parent)
	: Window(parent)
{
	connect(this, &QToolBar::orientationChanged, this, [this](Qt::Orientation orientation) {
		if((orientation == Qt::Vertical) != m_bVertical)
		{
			if(orientation == Qt::Vertical)
				SetVertical();
			else
				SetHorizontal();
		}
	});
	// The buttons' states, as MFC updates them when the program is idle.
	m_updateTimer.setInterval(200);
	connect(&m_updateTimer, &QTimer::timeout, this, &CToolBarEx::UpdateButtonStates);
	m_updateTimer.start();
}


void CToolBarEx::SetHorizontal()
{
	m_bVertical = false;
}


void CToolBarEx::SetVertical()
{
	m_bVertical = true;
}


void CToolBarEx::SetButtons(const uint32 *ids, int count)
{
	clear();
	m_items.clear();
	int image = 0;
	for(int i = 0; i < count; i++)
	{
		Item item;
		item.id = ids[i];
		if(ids[i] == ID_SEPARATOR)
		{
			item.action = addSeparator();
			item.separator = true;
		} else
		{
			item.image = image++;
			item.action = addAction(QString());
			const uint32 id = ids[i];
			connect(item.action, &QAction::triggered, this, [this, id] { OnButtonCommand(id); });
			connect(item.action, &QAction::hovered, this, [this, id, action = item.action] {
				action->setToolTip(GetButtonToolTip(id));
			});
		}
		m_items.push_back(item);
	}
	UpdateButtonImages();
}


void CToolBarEx::SetImageLists(CImageListEx *images, CImageListEx *disabledImages)
{
	m_images = images;
	m_disabledImages = disabledImages;
	if(m_images)
		setIconSize(m_images->GetImageSize());
	UpdateButtonImages();
}


void CToolBarEx::UpdateButtonImages()
{
	for(const auto &item : m_items)
	{
		if(item.separator || item.control || item.image < 0 || !m_images)
			continue;
		QIcon icon;
		icon.addPixmap(m_images->GetImage(item.image), QIcon::Normal);
		if(m_disabledImages)
			icon.addPixmap(m_disabledImages->GetImage(item.image), QIcon::Disabled);
		item.action->setIcon(icon);
	}
}


int CToolBarEx::CommandToIndex(uint32 id) const
{
	for(size_t i = 0; i < m_items.size(); i++)
	{
		if(!m_items[i].separator && m_items[i].id == id)
			return static_cast<int>(i);
	}
	return -1;
}


QAction *CToolBarEx::GetButton(uint32 id) const
{
	const int index = CommandToIndex(id);
	return (index >= 0) ? m_items[index].action : nullptr;
}


void CToolBarEx::SetButtonCommand(int index, uint32 id, int image)
{
	if(index < 0 || static_cast<size_t>(index) >= m_items.size())
		return;
	Item &item = m_items[index];
	if(item.separator || item.control)
		return;
	if(item.id == id && item.image == image)
		return;
	item.id = id;
	item.image = image;
	// Re-connect the action to the new command.
	disconnect(item.action, &QAction::triggered, this, nullptr);
	disconnect(item.action, &QAction::hovered, this, nullptr);
	connect(item.action, &QAction::triggered, this, [this, id] { OnButtonCommand(id); });
	connect(item.action, &QAction::hovered, this, [this, id, action = item.action] {
		action->setToolTip(GetButtonToolTip(id));
	});
	UpdateButtonImages();
}


void CToolBarEx::SetDropDown(uint32 id)
{
	QAction *action = GetButton(id);
	if(!action)
		return;
	auto *button = qobject_cast<QToolButton *>(widgetForAction(action));
	if(!button)
		return;
	// The arrow beside the button sends DropDown (TBN_DROPDOWN); the menu the
	// owner shows is its own, the empty one only makes the arrow.
	button->setPopupMode(QToolButton::MenuButtonPopup);
	auto *menu = new QMenu(button);
	button->setMenu(menu);
	connect(menu, &QMenu::aboutToShow, this, [this, id, menu, button] {
		QMetaObject::invokeMethod(menu, &QMenu::close, Qt::QueuedConnection);
		const QPoint pos = button->mapToGlobal(QPoint(0, button->height()));
		QMetaObject::invokeMethod(this, [this, id, pos] { OnButtonDropDown(id, pos); }, Qt::QueuedConnection);
	});
}


void CToolBarEx::SetControl(int index, uint32 id, QWidget *control)
{
	if(index < 0 || static_cast<size_t>(index) >= m_items.size())
		return;
	Item &item = m_items[index];
	QAction *placeholder = item.action;
	item.action = insertWidget(placeholder, control);
	removeAction(placeholder);
	delete placeholder;
	item.id = id;
	item.separator = false;
	item.control = control;
	control->setProperty("mfcless_id", id);
}


void CToolBarEx::UpdateControl(bool show, QWidget &wnd, int index, int /*id*/, int height)
{
	if(show && height)
	{
		// Centred in the bar, as upstream places it.
		wnd.setFixedHeight(height);
	}
	SetButtonVisibility(index, show);
}


void CToolBarEx::EnableFlatButtons(bool flat)
{
	m_flat = flat;
	for(const auto &item : m_items)
	{
		if(item.separator || item.control)
			continue;
		if(auto *button = qobject_cast<QToolButton *>(widgetForAction(item.action)))
			button->setAutoRaise(flat);
	}
	update();
}


void CToolBarEx::SetButtonVisibility(int index, bool visible)
{
	if(index >= 0 && static_cast<size_t>(index) < m_items.size())
		m_items[index].action->setVisible(visible);
}


void CToolBarEx::SetButtonHidden(uint32 id, bool hidden)
{
	if(QAction *action = GetButton(id))
		action->setVisible(!hidden);
}


void CToolBarEx::OnButtonCommand(uint32 id)
{
	if(CMainFrame *mainFrm = CMainFrame::GetMainFrame())
		mainFrm->RouteCommand(id);
}


void CToolBarEx::UpdateButtonStates()
{
	CMainFrame *mainFrm = CMainFrame::GetMainFrame();
	if(!mainFrm || !isVisible())
		return;
	for(const auto &item : m_items)
	{
		if(item.separator || item.control || !item.action->isVisible())
			continue;
		mfcless::CommandState state;
		state.id = item.id;
		mainFrm->UpdateCommandState(item.id, state);
		item.action->setEnabled(state.enabled);
		if(state.checked.has_value())
		{
			item.action->setCheckable(true);
			item.action->setChecked(*state.checked);
		} else if(item.action->isCheckable())
		{
			item.action->setChecked(false);
			item.action->setCheckable(false);
		}
	}
}


/////////////////////////////////////////////////////////////////////
// CMainToolBar

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

#define SCALEPIXELS(x)      (HighDPISupport::ScalePixels(x, this))
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
	// TTN_NEEDTEXT: GetButtonToolTip. TBN_DROPDOWN: OnButtonDropDown.
	AddCommandRange(ID_SELECT_MIDI_DEVICE, ID_SELECT_MIDI_DEVICE + MAX_MIDI_DEVICES, &CMainToolBar::OnSelectMIDIDevice);
	// WM_DPICHANGED_AFTERPARENT: OnDPIChanged. WM_SETTINGCHANGE: only a
	// Windows workaround (spin buttons vanishing after a UAC prompt).
	AddNotify(IDC_EDIT_CURRENTSPEED, mfcless::Notify::EditChange, &CMainToolBar::OnSpeedChanged);
	AddNotify(IDC_EDIT_CURRENTTEMPO, mfcless::Notify::EditChange, &CMainToolBar::OnTempoChanged);
	AddNotify(IDC_EDIT_RPB, mfcless::Notify::EditChange, &CMainToolBar::OnRPBChanged);
	AddNotify(IDC_EDIT_GLOBALVOL, mfcless::Notify::EditChange, &CMainToolBar::OnGlobalVolChanged);
}


// A read-only field of the bar (ES_READONLY | ES_AUTOHSCROLL), centred or not.
static void InitBarEdit(QLineEdit &edit, bool center)
{
	edit.setReadOnly(true);
	edit.setFrame(true);
	if(center)
		edit.setAlignment(Qt::AlignCenter);
}


// A static text of the bar (SS_CENTER | SS_CENTERIMAGE).
static QLabel *MakeBarLabel(const mpt::uchar *text, QWidget *parent)
{
	auto *label = new QLabel(mfcless::Text(text).value, parent);
	label->setAlignment(Qt::AlignCenter);
	return label;
}


bool CMainToolBar::Create(QWidget *parent)
{
	setParent(parent);
	setObjectName(QStringLiteral("MainToolBar"));
	setWindowTitle(QStringLiteral("Main"));
	setMovable(true);
	setFloatable(true);
	setAllowedAreas(Qt::AllToolBarAreas);  // EnableDocking(CBRS_ALIGN_ANY)

	SetButtons(MainButtons, static_cast<int>(std::size(MainButtons)));

	// Octave
	m_EditOctave.setParent(this);
	InitBarEdit(m_EditOctave, true);
	m_SpinOctave = new mfcless::UpDown(this);
	m_SpinOctave->SetBuddy(&m_EditOctave);
	// Tempo
	m_StaticTempo = MakeBarLabel(UL_("Tempo:"), this);
	m_EditTempo = new CNumberEdit(this);
	InitBarEdit(*m_EditTempo, false);
	m_SpinTempo = new mfcless::UpDown(this);
	m_SpinTempo->SetArrowKeys(true);  // UDS_ARROWKEYS
	m_SpinTempo->SetBuddy(m_EditTempo);
	// Speed
	m_StaticSpeed = MakeBarLabel(UL_("Ticks/Row:"), this);
	m_EditSpeed = new QLineEdit(this);
	InitBarEdit(*m_EditSpeed, false);
	m_SpinSpeed = new mfcless::UpDown(this);
	m_SpinSpeed->SetArrowKeys(true);  // UDS_ARROWKEYS
	m_SpinSpeed->SetBuddy(m_EditSpeed);
	// Rows per Beat
	m_StaticRowsPerBeat = MakeBarLabel(UL_("Rows/Beat:"), this);
	m_EditRowsPerBeat = new QLineEdit(this);
	InitBarEdit(*m_EditRowsPerBeat, false);
	m_SpinRowsPerBeat = new mfcless::UpDown(this);
	m_SpinRowsPerBeat->SetArrowKeys(true);  // UDS_ARROWKEYS
	m_SpinRowsPerBeat->SetBuddy(m_EditRowsPerBeat);
	// Global Volume
	m_StaticGlobalVolume = MakeBarLabel(UL_("Global Volume:"), this);
	m_EditGlobalVolume = new QLineEdit(this);
	InitBarEdit(*m_EditGlobalVolume, false);
	m_SpinGlobalVolume = new mfcless::UpDown(this);
	m_SpinGlobalVolume->SetArrowKeys(true);  // UDS_ARROWKEYS
	m_SpinGlobalVolume->SetBuddy(m_EditGlobalVolume);
	// Only digits (ES_NUMBER)
	for(QLineEdit *edit : {m_EditSpeed, m_EditRowsPerBeat, m_EditGlobalVolume})
		edit->setValidator(new QRegularExpressionValidator(QRegularExpression(QStringLiteral("[0-9]*")), edit));

	// VU Meter
	m_VuMeter = new CStereoVU(this);
	m_VuMeter->setFrameStyle(QFrame::Box | QFrame::Plain);  // WS_BORDER

	SetControl(EDITOCTAVE_INDEX, IDC_EDIT_BASEOCTAVE, &m_EditOctave);
	SetControl(SPINOCTAVE_INDEX, IDC_SPIN_BASEOCTAVE, m_SpinOctave);
	SetControl(TEMPOTEXT_INDEX, IDC_TEXT_CURRENTTEMPO, m_StaticTempo);
	SetControl(EDITTEMPO_INDEX, IDC_EDIT_CURRENTTEMPO, m_EditTempo);
	SetControl(SPINTEMPO_INDEX, IDC_SPIN_CURRENTTEMPO, m_SpinTempo);
	SetControl(SPEEDTEXT_INDEX, IDC_TEXT_CURRENTSPEED, m_StaticSpeed);
	SetControl(EDITSPEED_INDEX, IDC_EDIT_CURRENTSPEED, m_EditSpeed);
	SetControl(SPINSPEED_INDEX, IDC_SPIN_CURRENTSPEED, m_SpinSpeed);
	SetControl(RPBTEXT_INDEX, IDC_TEXT_RPB, m_StaticRowsPerBeat);
	SetControl(EDITRPB_INDEX, IDC_EDIT_RPB, m_EditRowsPerBeat);
	SetControl(SPINRPB_INDEX, IDC_SPIN_RPB, m_SpinRowsPerBeat);
	SetControl(GLOBALVOLTEXT_INDEX, IDC_TEXT_GLOBALVOL, m_StaticGlobalVolume);
	SetControl(EDITGLOBALVOL_INDEX, IDC_EDIT_GLOBALVOL, m_EditGlobalVolume);
	SetControl(SPINGLOBALVOL_INDEX, IDC_SPIN_GLOBALVOL, m_SpinGlobalVolume);
	SetControl(VUMETER_INDEX, IDC_VUMETER, m_VuMeter);

	// The controls notify the bar, as they would their parent window.
	mfcless::ConnectNotifications(m_EditTempo, IDC_EDIT_CURRENTTEMPO, this);
	mfcless::ConnectNotifications(m_EditSpeed, IDC_EDIT_CURRENTSPEED, this);
	mfcless::ConnectNotifications(m_EditRowsPerBeat, IDC_EDIT_RPB, this);
	mfcless::ConnectNotifications(m_EditGlobalVolume, IDC_EDIT_GLOBALVOL, this);
	for(auto [spin, id] : {std::pair{m_SpinOctave, IDC_SPIN_BASEOCTAVE}, {m_SpinTempo, IDC_SPIN_CURRENTTEMPO}, {m_SpinSpeed, IDC_SPIN_CURRENTSPEED}, {m_SpinRowsPerBeat, IDC_SPIN_RPB}, {m_SpinGlobalVolume, IDC_SPIN_GLOBALVOL}})
		mfcless::ConnectNotifications(spin, id, this);

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
	m_EditTempo->setMaxLength(9);
	static_assert(MAX_GLOBAL_VOLUME <= 999);
	m_EditGlobalVolume->setMaxLength(3);

	// Display everything
	SetBaseOctave(4);
	SetCurrentSong(nullptr);

	SetButtonHidden(ID_UPDATE_AVAILABLE, true);

	return true;
}


void CMainToolBar::OnDPIChanged()
{
	m_font = QFont();
	m_fontValid = false;
	UpdateSizes();
	RefreshToolbar();
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
	if(!m_fontValid)
	{
		HighDPISupport::CreateGUIFont(m_font, this);
		m_fontValid = true;
	}
	const double scaling = HighDPISupport::GetDpiForWindow(this) / 96.0;
	const int imgSize = HighDPISupport::ScalePixels(16, this);
	m_ImageList.Create(IDB_MAINBAR, 16, 16, IMGLIST_NUMIMAGES, 1, scaling, false);
	m_ImageListDisabled.Create(IDB_MAINBAR, 16, 16, IMGLIST_NUMIMAGES, 1, scaling, true);

	struct TextWndInfo
	{
		QWidget &wnd;
		const mpt::uchar *measureText;
	};
	const TextWndInfo TextWnds[] =
	{
		{m_EditOctave,          UL_("Octave 9")      },
		{*m_StaticTempo,        UL_("Tempo")         },
		{*m_EditTempo,          UL_("999.9999")      },
		{*m_StaticSpeed,        UL_("Ticks/Row:")    },
		{*m_EditSpeed,          UL_("999")           },
		{*m_StaticRowsPerBeat,  UL_("Rows/Beat:")    },
		{*m_EditRowsPerBeat,    UL_("9999")          },
		{*m_StaticGlobalVolume, UL_("Global Volume:")},
		{*m_EditGlobalVolume,   UL_("999")           },
	};

	const QFontMetrics metrics(m_font);
	const int textPaddingX = HighDPISupport::ScalePixels(10, this), textPaddingY = HighDPISupport::ScalePixels(4, this), textMinHeight = HighDPISupport::ScalePixels(20, this);
	for(auto &info : TextWnds)
	{
		info.wnd.setFont(m_font);
		const QSize size = metrics.size(Qt::TextSingleLine, mfcless::Text(info.measureText).value);
		const int width = size.width() + textPaddingX;
		const int height = std::max(size.height() + textPaddingY, textMinHeight);
		info.wnd.setFixedSize(width, height);
	}

	SetImageLists(&m_ImageList, &m_ImageListDisabled);
	setIconSize(QSize(imgSize, imgSize));
	setFont(m_font);

	// Dropdown menus for New and MIDI buttons
	SetDropDown(ID_FILE_NEW);
	SetDropDown(ID_MIDI_RECORD);

	const int spinnerWidth = SPINNER_WIDTH, spinnerHeight = SPINNER_HEIGHT;
	for(mfcless::UpDown *spin : {m_SpinOctave, m_SpinTempo, m_SpinSpeed, m_SpinRowsPerBeat, m_SpinGlobalVolume})
		spin->setFixedSize(spinnerWidth, spinnerHeight);
}


void CMainToolBar::Init(CMainFrame *pMainFrm)
{
	EnableFlatButtons(TrackerSettings::Instance().patternSetup & PatternSetup::FlatToolbarButtons);
	MPT_UNREFERENCED_PARAMETER(pMainFrm);
	SetHorizontal();
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
	UpdateControl(visibleItems[MainToolBarItem::Octave], *m_SpinOctave, SPINOCTAVE_INDEX, IDC_SPIN_BASEOCTAVE);
	SetButtonVisibility(OCTAVE_DIVIDER_INDEX, m_bVertical && visibleItems[MainToolBarItem::Octave]);

	UpdateControl(visibleItems[MainToolBarItem::Tempo], *m_StaticTempo, TEMPOTEXT_INDEX, IDC_TEXT_CURRENTTEMPO);
	UpdateControl(visibleItems[MainToolBarItem::Tempo], *m_EditTempo, EDITTEMPO_INDEX, IDC_EDIT_CURRENTTEMPO);
	UpdateControl(visibleItems[MainToolBarItem::Tempo], *m_SpinTempo, SPINTEMPO_INDEX, IDC_SPIN_CURRENTTEMPO);
	SetButtonVisibility(TEMPO_DIVIDER_INDEX, m_bVertical && visibleItems[MainToolBarItem::Tempo]);

	UpdateControl(visibleItems[MainToolBarItem::Speed], *m_StaticSpeed, SPEEDTEXT_INDEX, IDC_TEXT_CURRENTSPEED);
	UpdateControl(visibleItems[MainToolBarItem::Speed], *m_EditSpeed, EDITSPEED_INDEX, IDC_EDIT_CURRENTSPEED);
	UpdateControl(visibleItems[MainToolBarItem::Speed], *m_SpinSpeed, SPINSPEED_INDEX, IDC_SPIN_CURRENTSPEED);
	SetButtonVisibility(SPEED_DIVIDER_INDEX, m_bVertical && visibleItems[MainToolBarItem::Speed]);

	UpdateControl(visibleItems[MainToolBarItem::RowsPerBeat], *m_StaticRowsPerBeat, RPBTEXT_INDEX, IDC_TEXT_RPB);
	UpdateControl(visibleItems[MainToolBarItem::RowsPerBeat], *m_EditRowsPerBeat, EDITRPB_INDEX, IDC_EDIT_RPB);
	UpdateControl(visibleItems[MainToolBarItem::RowsPerBeat], *m_SpinRowsPerBeat, SPINRPB_INDEX, IDC_SPIN_RPB);
	SetButtonVisibility(RPB_DIVIDER_INDEX, m_bVertical && visibleItems[MainToolBarItem::RowsPerBeat]);

	UpdateControl(visibleItems[MainToolBarItem::GlobalVolume], *m_StaticGlobalVolume, GLOBALVOLTEXT_INDEX, IDC_TEXT_GLOBALVOL);
	UpdateControl(visibleItems[MainToolBarItem::GlobalVolume], *m_EditGlobalVolume, EDITGLOBALVOL_INDEX, IDC_EDIT_GLOBALVOL);
	UpdateControl(visibleItems[MainToolBarItem::GlobalVolume], *m_SpinGlobalVolume, SPINGLOBALVOL_INDEX, IDC_SPIN_GLOBALVOL);
	SetButtonVisibility(GLOBALVOL_DIVIDER_INDEX, visibleItems.test_any_except(MainToolBarItem::VUMeter));

	m_VuMeter->SetOrientation(!m_bVertical);
	if(m_bVertical)
		m_VuMeter->setFixedSize(VUMETER_HEIGHT, VUMETER_HEIGHT);
	else
		m_VuMeter->setFixedSize(VUMETER_WIDTH, VUMETER_HEIGHT);
	UpdateControl(visibleItems[MainToolBarItem::VUMeter], *m_VuMeter, VUMETER_INDEX, IDC_VUMETER, VUMETER_HEIGHT);
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
	m_EditOctave.setText(mfcless::Text(MPT_UFORMAT("Octave {}")(octave)).value);
	m_SpinOctave->SetPos(octave);
}


bool CMainToolBar::ShowUpdateInfo(const mpt::ustring &newVersion, const mpt::ustring &infoURL, bool showHighLight)
{
	SetButtonHidden(ID_UPDATE_AVAILABLE, false);
	RefreshToolbar();

	// Trying to show the tooltip while the window is minimized hangs the application during TTM_TRACKACTIVATE.
	if(!showHighLight || CMainFrame::GetMainFrame()->IsIconic())
		return true;

	mfcless::Rect rect(actionGeometry(GetButton(ID_UPDATE_AVAILABLE)));
	mfcless::Point pt = rect.CenterPoint();
	ClientToScreen(&pt);
	if(!isFloating())
	{
		mfcless::Rect windowRect;
		mfcless::GetWindowRect(CMainFrame::GetMainFrame(), &windowRect);
		LimitMax(pt.x, windowRect.right);
	}

	return m_tooltip.ShowUpdate(*this, newVersion, infoURL, rect, pt, ID_UPDATE_AVAILABLE);
}


void CMainToolBar::RemoveUpdateInfo()
{
	if(m_tooltip)
		m_tooltip.Pop();
	SetButtonHidden(ID_UPDATE_AVAILABLE, true);
}


static void EnableEdit(QLineEdit *edit, mfcless::UpDown *spin, bool enable)
{
	if(!enable)
		edit->setText(QStringLiteral("---"));
	edit->setEnabled(enable);
	edit->setReadOnly(!enable);
	spin->setEnabled(enable);
}


void CMainToolBar::SetCurrentSong(CSoundFile *pSndFile)
{
	// Update Info
	m_updating = true;
	const QWidget *focus = mfcless::GetFocus();
	const FlagSet<MainToolBarItem> visibleItems = TrackerSettings::Instance().mainToolBarVisibleItems.Get();
	if(pSndFile)
	{
		// Update play/pause button
		if(m_currentTempo == TEMPO(0, 0))
			SetButtonCommand(PLAY_STARTPAUSE_INDEX, ID_PLAYER_PAUSE, TOOLBAR_IMAGE_PAUSE);
		// Update Speed
		int nSpeed = pSndFile->m_PlayState.m_nMusicSpeed;
		if(nSpeed != m_currentSpeed && focus != m_EditSpeed && visibleItems[MainToolBarItem::Speed])
		{
			if(m_currentSpeed < 0)
				EnableEdit(m_EditSpeed, m_SpinSpeed, true);

			m_currentSpeed = nSpeed;
			m_EditSpeed->setText(QString::number(m_currentSpeed));
		}
		TEMPO nTempo = pSndFile->m_PlayState.m_nMusicTempo;
		if(nTempo != m_currentTempo && focus != m_EditTempo && visibleItems[MainToolBarItem::Tempo])
		{
			if(m_currentTempo <= TEMPO(0, 0))
				EnableEdit(m_EditTempo, m_SpinTempo, true);

			m_currentTempo = nTempo;
			m_EditTempo->SetTempoValue(m_currentTempo);
		}
		int nRowsPerBeat = pSndFile->m_PlayState.m_nCurrentRowsPerBeat;
		if(nRowsPerBeat != m_currentRowsPerBeat && focus != m_EditRowsPerBeat && visibleItems[MainToolBarItem::RowsPerBeat])
		{
			if(m_currentRowsPerBeat < 0)
				EnableEdit(m_EditRowsPerBeat, m_SpinRowsPerBeat, true);

			m_currentRowsPerBeat = nRowsPerBeat;
			m_EditRowsPerBeat->setText(QString::number(m_currentRowsPerBeat));
		}
		int globalVol = pSndFile->m_PlayState.m_nGlobalVolume;
		if(globalVol != m_currentGlobalVolume && focus != m_EditGlobalVolume && visibleItems[MainToolBarItem::GlobalVolume])
		{
			if(m_currentGlobalVolume < 0)
				EnableEdit(m_EditGlobalVolume, m_SpinGlobalVolume, true);

			m_currentGlobalVolume = globalVol;
			uint32 displayVolume = Util::muldivr_unsigned(m_currentGlobalVolume, pSndFile->GlobalVolumeRange(), MAX_GLOBAL_VOLUME);
			m_EditGlobalVolume->setText(QString::number(displayVolume));
		}
	} else
	{
		if(m_currentTempo > TEMPO(0, 0))
		{
			EnableEdit(m_EditTempo, m_SpinTempo, false);
			SetButtonCommand(PLAY_STARTPAUSE_INDEX, ID_PLAYER_PLAY, TOOLBAR_IMAGE_PLAY);
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
	if(focus && !focus->isEnabled() && focus->parentWidget() == this)
		CMainFrame::GetMainFrame()->setFocus();

	m_updating = false;
}


void CMainToolBar::OnVScroll(mfcless::ScrollCode nCode, int nPos, QWidget *pScrollBar)
{
	CMainFrame *pMainFrm;

	CToolBarEx::OnVScroll(nCode, nPos, pScrollBar);
	// Avoid auto-setting input focus to edit control
	if(auto *activeView = CMainFrame::GetMainFrame()->GetActiveView(); activeView != nullptr)
		activeView->ViewWidget()->setFocus();
	else if(pScrollBar)
		pScrollBar->setFocus();

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
		uint32 newSpeed = m_EditSpeed->text().toUInt(&ok);
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
		if(sndFile && mfcless::GetWindowTextLength(m_EditTempo) > 0)
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
		uint32 newRPB = m_EditRowsPerBeat->text().toUInt(&ok);
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
		uint32 newGlobalVol = m_EditGlobalVolume->text().toUInt(&ok);
		CSoundFile *sndFile = mainFrm->GetSoundFilePlaying();
		if(sndFile && ok)
		{
			sndFile->m_PlayState.m_nGlobalVolume = Clamp(Util::muldivr_unsigned(newGlobalVol, MAX_GLOBAL_VOLUME, sndFile->GlobalVolumeRange()), uint32(0), MAX_GLOBAL_VOLUME);
		}
		m_currentGlobalVolume = -2;  // Force display update once focus moves away from this input field
	}
}


void CMainToolBar::OnButtonDropDown(uint32 id, QPoint pos)
{
	switch(id)
	{
	case ID_FILE_NEW:
		{
			auto *mainFrm = CMainFrame::GetMainFrame();
			auto [newMenu, newPos] = CMainFrame::FindMenuItemByCommand(*mainFrm->GetMenu(), ID_FILE_NEWIT);
			auto [templateMenu, templatePos] = CMainFrame::FindMenuItemByCommand(*mainFrm->GetMenu(), ID_FILE_OPENTEMPLATE);
			if(templateMenu)
				newMenu->AppendMenu(mfcless::menu::Popup, templateMenu, UL_("&Templates"));
			newMenu->TrackPopupMenu(mfcless::menu::LeftAlign | mfcless::menu::RightButton, pos.x(), pos.y(), this);
			if(templateMenu)
				newMenu->RemoveMenu(newMenu->GetMenuItemCount() - 1, mfcless::menu::ByPosition);
		}
		break;
	case ID_MIDI_RECORD:
		// Show a list of MIDI devices
		{
			mfcless::Menu hMenu = mfcless::Menu::CreatePopup();
			const auto names = MidiInputNames();
			uint32 numDevs = static_cast<uint32>(names.size());
			if(numDevs > MAX_MIDI_DEVICES) numDevs = MAX_MIDI_DEVICES;
			uint32 current = TrackerSettings::Instance().GetCurrentMIDIDevice();
			for(uint32 i = 0; i < numDevs; i++)
			{
				hMenu.AppendMenu(mfcless::menu::String | (i == current ? mfcless::menu::Checked : 0), ID_SELECT_MIDI_DEVICE + i, theApp.GetFriendlyMIDIPortName(names[i], true));
			}
			if(!numDevs)
			{
				hMenu.AppendMenu(mfcless::menu::String | mfcless::menu::Grayed, 0, UL_("No MIDI input devices found"));
			}
			hMenu.TrackPopupMenu(mfcless::menu::LeftAlign | mfcless::menu::RightButton, pos.x(), pos.y(), this);
			hMenu.DestroyMenu();
		}
		break;
	}
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


QString CMainToolBar::GetButtonToolTip(uint32 id)
{
	const mpt::uchar *s = nullptr;
	CommandID cmd = kcNull;
	switch(id)
	{
	case ID_FILE_NEW: s = UL_("New"); cmd = kcFileNew; break;
	case ID_FILE_OPEN: s = UL_("Open"); cmd = kcFileOpen; break;
	case ID_FILE_SAVE: s = UL_("Save"); cmd = kcFileSave; break;
	case ID_EDIT_CUT: s = UL_("Cut"); cmd = kcEditCut; break;
	case ID_EDIT_COPY: s = UL_("Copy"); cmd = kcEditCopy; break;
	case ID_EDIT_PASTE: s = UL_("Paste"); cmd = kcEditPaste; break;
	case ID_MIDI_RECORD: s = UL_("MIDI Record"); cmd = kcMidiRecord; break;
	case ID_PLAYER_STOP: s = UL_("Stop"); cmd = kcStopSong; break;
	case ID_PLAYER_PLAY: s = UL_("Play"); cmd = kcPlayPauseSong; break;
	case ID_PLAYER_PAUSE: s = UL_("Pause"); cmd = kcPlayPauseSong; break;
	case ID_PLAYER_PLAYFROMSTART: s = UL_("Play From Start"); cmd = kcPlaySongFromStart; break;
	case ID_VIEW_OPTIONS: s = UL_("Setup"); cmd = kcViewOptions; break;
	case ID_PANIC: s = UL_("Stop all hanging plugin and sample voices"); cmd = kcPanic; break;
	case ID_UPDATE_AVAILABLE: s = UL_("A new update is available."); break;
	}

	if(s == nullptr)
		return {};

	mpt::ustring fmt = s;
	if(cmd != kcNull)
	{
		auto keyText = CMainFrame::GetInputHandler()->m_activeCommandSet->GetKeyTextFromCommand(cmd, 0);
		if(!keyText.empty())
			fmt += MPT_UFORMAT(" ({})")(keyText);
	}
	return mfcless::Text(fmt).value;
}


///////////////////////////////////////////////////////////////////////////////////////////////////
// CModTreeBar

void CModTreeBar::RegisterMessageMap()
{
	mfcless::Window<QWidget>::RegisterMessageMap();
	// The bar's outer edge (upstream's non-client border) is the main
	// window's splitter; the gap between the two trees is dragged here.
	// ON_WM_SIZE(), ON_WM_MOUSEMOVE(), ON_WM_LBUTTONDOWN(), ON_WM_LBUTTONUP(), ON_WM_TIMER() (mfcless::Window)
	AddNotify(IDC_EDIT1, mfcless::Notify::EditChange, &CModTreeBar::OnFilterChanged);
	AddNotify(IDC_EDIT1, mfcless::Notify::KillFocus, &CModTreeBar::OnFilterLostFocus);
	AddCommand(ID_CLOSE_LIBRARY_FILTER, &CModTreeBar::CloseTreeFilter);
}


CModTreeBar::CModTreeBar()
{
	m_nTreeSplitRatio = TrackerSettings::Instance().glTreeSplitRatio;
	setMouseTracking(true);
	// The trees of the IDD_TREEVIEW template (TVS_HASBUTTONS | TVS_HASLINES | WS_BORDER).
	m_pModTreeData = new CModTree(nullptr);
	m_pModTreeData->setParent(this);
	m_pModTreeData->setProperty("mfcless_id", IDC_TREEDATA);
	m_pModTree = new CModTree(m_pModTreeData);
	m_pModTree->setParent(this);
	m_pModTree->setProperty("mfcless_id", IDC_TREEVIEW);
	for(CModTree *tree : {m_pModTree, m_pModTreeData})
		tree->SetStyle(mfcless::tree::HasButtons | mfcless::tree::HasLines);
	m_status.reset();
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
	ApplyWidth();
}


bool CModTreeBar::PreTranslateMessage(mfcless::Message *pMsg)
{
	if(m_filterEdit && pMsg->hwnd == m_filterEdit && pMsg->message == mfcless::msg::KeyDown && m_filterSource != nullptr)
	{
		switch(pMsg->wParam)
		{
		case mfcless::vk::RETURN:
			if(const auto modItem = m_filterSource->GetModItem(m_filterSource->GetSelectedItem()); (modItem.type == CModTree::MODITEM_INSLIB_FOLDER || modItem.type == CModTree::MODITEM_INSLIB_SONG))
			{
				mfcless::PostCommand(m_filterSource, ID_MODTREE_EXECUTE);
			}
			[[fallthrough]];
		case mfcless::vk::ESCAPE:
			CloseTreeFilter();
			return true;

		case mfcless::vk::TAB:
			if(m_filterSource)
			{
				m_filterSource->setFocus();
				return true;
			}
			break;

		case mfcless::vk::UP:
		case mfcless::vk::DOWN:
			if(const auto selectedItem = m_filterSource->GetSelectedItem(); selectedItem != nullptr)
			{
				const auto item = m_filterSource->GetNextItem(selectedItem, (pMsg->wParam == mfcless::vk::UP) ? mfcless::tree::Previous : mfcless::tree::Next);
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
	if(!show && isAncestorOf(mfcless::GetFocus()))
		CMainFrame::GetMainFrame()->setFocus();
	setVisible(show);
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
	return HighDPISupport::ScalePixels(3, this);
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
		if(m_filterSource == m_pModTree)
		{
			int editHeight = HighDPISupport::ScalePixels(20, this);
			m_pModTree->setGeometry(0, 0, rect.Width(), cytree - editHeight);
			m_pModTreeData->setGeometry(0, cytree + padding, rect.Width(), cydata);
			m_filterEdit->setGeometry(0, cytree - editHeight, rect.Width(), editHeight);
		} else if(m_filterSource == m_pModTreeData)
		{
			int editHeight = HighDPISupport::ScalePixels(20, this);
			m_pModTree->setGeometry(0, 0, rect.Width(), cytree);
			m_pModTreeData->setGeometry(0, cytree + padding, rect.Width(), cydata - editHeight);
			m_filterEdit->setGeometry(0, cytree + padding + cydata - editHeight, rect.Width(), editHeight);
		} else
		{
			m_pModTree->setGeometry(0, 0, rect.Width(), cytree);
			m_pModTreeData->setGeometry(0, cytree + padding, rect.Width(), cydata);
		}
	}
}


// The gap between the two trees, where the split is dragged.
mfcless::Rect CModTreeBar::SplitterRect() const
{
	if(!m_pModTree || !m_pModTreeData)
		return {};
	const QRect upper = m_pModTree->geometry(), lower = m_pModTreeData->geometry();
	const int top = (m_filterSource == m_pModTree && m_filterEdit) ? m_filterEdit->geometry().bottom() + 1 : upper.bottom() + 1;
	return mfcless::Rect(0, top, width(), lower.top());
}


QSplitter *CModTreeBar::ParentSplitter() const
{
	return qobject_cast<QSplitter *>(parentWidget());
}


void CModTreeBar::ApplyWidth()
{
	QSplitter *splitter = ParentSplitter();
	if(!splitter)
		return;
	if(!m_splitterConnected)
	{
		m_splitterConnected = true;
		connect(splitter, &QSplitter::splitterMoved, this, [this](int, int) {
			// The bar's new width (upstream: the tracker on its edge).
			if(isVisible() && width() > Padding())
				TrackerSettings::Instance().glTreeWindowWidth = HighDPISupport::ScalePixelsInv(width(), this);
		});
	}
	const int index = splitter->indexOf(this);
	if(index < 0 || splitter->count() < 2)
		return;
	QList<int> sizes = splitter->sizes();
	int total = 0;
	for(int sz : sizes)
		total += sz;
	const int barWidth = std::max(HighDPISupport::ScalePixels(TrackerSettings::Instance().glTreeWindowWidth, this), Padding() + 1);
	if(total <= 0)
		total = std::max(splitter->width(), barWidth * 4);
	for(int i = 0; i < sizes.size(); i++)
		sizes[i] = (i == index) ? barWidth : std::max(total - barWidth, 1);
	splitter->setSizes(sizes);
}


void CModTreeBar::DoMouseMove(mfcless::Point pt)
{
	mfcless::Rect rect;
	GetClientRect(&rect);

	if(m_status[MTB_CAPTURE | MTB_DRAGGING] && !HasCapture())
	{
		CancelTracking();
	}
	if(m_status[MTB_DRAGGING])
	{
		// Live split (Qt's splitters move the panes while dragging).
		m_nTrackPos = std::clamp(static_cast<int>(pt.y) - static_cast<int>(ptDragging.y), 0, rect.Height());
		const int padding = Padding();
		int cyavail = rect.Height() - padding;
		if(cyavail < padding + 1)
			cyavail = padding + 1;
		m_nTreeSplitRatio = std::clamp(static_cast<int>(m_nTrackPos * 256) / cyavail, 0, 256);
		RecalcLayout();
	} else
	{
		const bool overSplitter = SplitterRect().PtInRect(pt);
		setCursor(overSplitter ? Qt::SplitVCursor : Qt::ArrowCursor);
	}
}


void CModTreeBar::DoLButtonDown(mfcless::Point pt)
{
	if(SplitterRect().PtInRect(pt))
	{
		ptDragging = mfcless::Point(0, pt.y - SplitterRect().top);
		m_status.set(MTB_DRAGGING | MTB_VERTICAL | MTB_CAPTURE);
		SetCapture();
	}
}


void CModTreeBar::DoLButtonUp()
{
	if(m_status[MTB_DRAGGING])
	{
		TrackerSettings::Instance().glTreeSplitRatio = m_nTreeSplitRatio;
		CancelTracking();
		RecalcLayout();
	}
}


void CModTreeBar::CancelTracking()
{
	m_status.reset(MTB_TRACKER | MTB_DRAGGING | MTB_CAPTURE);
	ReleaseCapture();
	unsetCursor();
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
	m_barOnLeft = left;
	if(QSplitter *splitter = ParentSplitter())
	{
		const int index = splitter->indexOf(this);
		const int wanted = left ? 0 : splitter->count() - 1;
		if(index >= 0 && index != wanted)
		{
			const int barWidth = width();
			splitter->insertWidget(wanted, this);
			splitter->setStretchFactor(splitter->indexOf(this), 0);
			for(int i = 0; i < splitter->count(); i++)
			{
				if(splitter->widget(i) != this)
					splitter->setStretchFactor(i, 1);
			}
			if(barWidth > 0)
				ApplyWidth();
		}
		if(!m_splitterConnected)
			ApplyWidth();  // the first time the bar is in the splitter
	}
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// CModTreeBar message handlers

void CModTreeBar::OnSize(uint32 nType, int cx, int cy)
{
	Window::OnSize(nType, cx, cy);
	RecalcLayout();
}


void CModTreeBar::OnMouseMove(uint32, mfcless::Point point)
{
	DoMouseMove(point);
}


void CModTreeBar::OnLButtonDown(uint32, mfcless::Point point)
{
	DoLButtonDown(point);
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
		m_filterEdit = new QLineEdit(this);
		m_filterEdit->setProperty("mfcless_id", IDC_EDIT1);
		m_filterEdit->setFont(font());
		mfcless::ConnectNotifications(m_filterEdit, IDC_EDIT1, this);
		m_filterEdit->show();
	} else if(m_filterSource != &source)
	{
		m_filterEdit->clear();
	}
	m_filterEdit->setFocus();
	m_filterSource = &source;
	RecalcLayout();
}


void CModTreeBar::OnFilterChanged()
{
	if(!m_filterSource)
		return;

	const mpt::ustring filter = mfcless::FromQString(m_filterEdit->text());
	const auto length = m_filterEdit->text().length();
	if(length < 1 || length > 2)
	{
		CancelTimer();
		m_filterSource->SetInstrumentLibraryFilter(filter);
	} else
	{
		if(!m_filterTimer)
			m_filterTimer = SetTimer(1, static_cast<int>(360 - length * 120));
	}
}


void CModTreeBar::OnTimer(uintptr_t id)
{
	if(id != m_filterTimer)
		return;

	if(m_filterSource)
		m_filterSource->SetInstrumentLibraryFilter(mfcless::FromQString(m_filterEdit->text()));
	CancelTimer();
}


void CModTreeBar::OnFilterLostFocus()
{
	if(m_filterEdit && m_filterEdit->text().isEmpty())
		CloseTreeFilter();
}


void CModTreeBar::CloseTreeFilter()
{
	CancelTimer();
	if(m_filterSource)
	{
		m_filterSource->SetInstrumentLibraryFilter({});
		if(mfcless::GetFocus() == m_filterEdit)
			m_filterSource->setFocus();
		m_filterSource = nullptr;
	}
	if(m_filterEdit)
	{
		// Deleted later: this may run from the edit's own focus-out.
		m_filterEdit->hide();
		m_filterEdit->deleteLater();
		m_filterEdit = nullptr;
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
			// mfcless: Qt draws only in the paint event, which redraws it all.
			update();
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
