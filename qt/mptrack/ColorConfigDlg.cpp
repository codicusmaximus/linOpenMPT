/*
 * ColorConfigDlg.cpp
 * ------------------
 * Purpose: Implementation of the display setup dialog.
 * Notes  : mfcless twin of upstream's mptrack/ColorConfigDlg.cpp.
 *          Draft from qt/tools/twin.py, not yet edited by hand.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "ColorConfigDlg.h"
#include "mfcless/Widgets.h"
#include "ColorSchemes.h"
#include "FileDialog.h"
#include "HighDPISupport.h"
#include "Mainfrm.h"
#include "Mptrack.h"
#include "Reporting.h"
#include "resource.h"
#include "Settings.h"
#include "WindowMessages.h"
#include "../common/mptStringBuffer.h"


OPENMPT_NAMESPACE_BEGIN


class CFontDialogEx : public CFontDialog
{
	using CFontDialog::CFontDialog;

public:
	intptr_t DoModal() override
	{
		// CHOOSEFONT is not compatible with mixed-DPI. Temporarily disable mixed-DPI awareness just for this dialog.
		// https://blogs.windows.com/windowsdeveloper/2016/10/24/high-dpi-scaling-improvements-for-desktop-applications-and-mixed-mode-dpi-scaling-in-the-windows-10-anniversary-update/
		HighDPISupport::DPIAwarenessBypass bypass;
		mfcless::DC * dc = ::GetDC(nullptr);
		uint32 dpi = ::GetDeviceCaps(dc, LOGPIXELSX);
		::ReleaseDC(nullptr, dc);
		m_cf.lpLogFont->lfHeight = -mfcless::MulDiv(m_cf.lpLogFont->lfHeight, dpi, 720);
		return CFontDialog::DoModal();
	}
};


static constexpr struct ColorDescriptions
{
	const mpt::uchar *name;
	int previewImage;
	std::array<ModColor, 3> colorIndex;
	std::array<const mpt::uchar *, 3> descText;
} colorDefs[] =
{
	{ UL_("Pattern Editor"),      0, { MODCOLOR_BACKNORMAL, MODCOLOR_TEXTNORMAL, MODCOLOR_BACKHILIGHT }, { UL_("Background:"), UL_("Foreground:"), UL_("Highlighted:") } },
	{ UL_("Active Row"),          0, { MODCOLOR_BACKCURROW, MODCOLOR_TEXTCURROW, MODCOLOR_BACKRECORDROW }, { UL_("Background:"), UL_("Foreground:"), UL_("Background (Record):")}},
	{ UL_("Pattern Selection"),   0, { MODCOLOR_BACKSELECTED, MODCOLOR_TEXTSELECTED, {} }, { UL_("Background:"), UL_("Foreground:"), nullptr } },
	{ UL_("Play Cursor"),         0, { MODCOLOR_BACKPLAYCURSOR, MODCOLOR_TEXTPLAYCURSOR, {} }, { UL_("Background:"), UL_("Foreground:"), nullptr } },
	{ UL_("Note Highlight"),      0, { MODCOLOR_NOTE, MODCOLOR_INSTRUMENT, MODCOLOR_VOLUME }, { UL_("Note:"), UL_("Instrument:"), UL_("Volume:") } },
	{ UL_("Effect Highlight"),    0, { MODCOLOR_PANNING, MODCOLOR_PITCH, MODCOLOR_GLOBALS }, { UL_("Panning Effects:"), UL_("Pitch Effects:"), UL_("Global Effects:") } },
	{ UL_("Invalid Commands"),    0, { MODCOLOR_DODGY_COMMANDS, {}, {} }, { UL_("Invalid Note:"), nullptr, nullptr } },
	{ UL_("Channel Separator"),   0, { MODCOLOR_SEPHILITE, MODCOLOR_SEPFACE, MODCOLOR_SEPSHADOW }, { UL_("Highlight:"), UL_("Face:"), UL_("Shadow:") } },
	{ UL_("Next/Prev Pattern"),   0, { MODCOLOR_BLENDCOLOR, {}, {} }, { UL_("Blend Colour:"), nullptr, nullptr } },
	{ UL_("Sample Waveform"),     1, { MODCOLOR_SAMPLE, MODCOLOR_BACKSAMPLE, MODCOLOR_SAMPLESELECTED }, { UL_("Sample Data:"), UL_("Background:"), UL_("Selection:") } },
	{ UL_("Sample Markers"),      1, { MODCOLOR_SAMPLE_LOOPMARKER, MODCOLOR_SAMPLE_SUSTAINMARKER, MODCOLOR_SAMPLE_CUEPOINT}, { UL_("Loop Marker:"), UL_("Sustain Marker:"), UL_("Cue Point:") } },
	{ UL_("Instrument Editor"),   2, { MODCOLOR_ENVELOPES, MODCOLOR_ENVELOPE_RELEASE, MODCOLOR_BACKENV }, { UL_("Envelopes:"), UL_("Release Envelope:"), UL_("Background:") } },
	{ UL_("VU-Meters"),           0, { MODCOLOR_VUMETER_HI, MODCOLOR_VUMETER_MED, MODCOLOR_VUMETER_LO }, { UL_("Hi:"), UL_("Med:"), UL_("Lo:") } },
	{ UL_("VU-Meters (Plugins)"), 0, { MODCOLOR_VUMETER_HI_VST, MODCOLOR_VUMETER_MED_VST, MODCOLOR_VUMETER_LO_VST }, { UL_("Hi:"), UL_("Med:"), UL_("Lo:") } }
};

#define PREVIEWBMP_WIDTH	88
#define PREVIEWBMP_HEIGHT	39


void COptionsColors::RegisterMessageMap()
{
	PropertyPage::RegisterMessageMap();
	// ON_WM_DRAWITEM(): by hand (Qt event override)
	AddScroll(Qt::Vertical, &COptionsColors::OnVScroll);
	AddNotify(IDC_COMBO1, mfcless::Notify::SelChange, &COptionsColors::OnColorSelChanged);
	AddNotify(IDC_COMBO2, mfcless::Notify::SelChange, &COptionsColors::OnSettingsChanged);
	AddNotify(IDC_COMBO3, mfcless::Notify::SelChange, &COptionsColors::OnPresetChange);
	AddNotify(IDC_COMBO4, mfcless::Notify::SelChange, &COptionsColors::OnSettingsChanged);
	AddNotify(IDC_PRIMARYHILITE, mfcless::Notify::EditChange, &COptionsColors::OnSettingsChanged);
	AddNotify(IDC_SECONDARYHILITE, mfcless::Notify::EditChange, &COptionsColors::OnSettingsChanged);
	AddCommand(IDC_BUTTON1, &COptionsColors::OnSelectColor1);
	AddCommand(IDC_BUTTON2, &COptionsColors::OnSelectColor2);
	AddCommand(IDC_BUTTON3, &COptionsColors::OnSelectColor3);
	AddCommand(IDC_BUTTON9, &COptionsColors::OnChoosePatternFont);
	AddCommand(IDC_BUTTON10, &COptionsColors::OnChooseCommentFont);
	AddCommand(IDC_BUTTON11, &COptionsColors::OnClearWindowCache);
	AddCommand(IDC_LOAD_COLORSCHEME, &COptionsColors::OnLoadColorScheme);
	AddCommand(IDC_SAVE_COLORSCHEME, &COptionsColors::OnSaveColorScheme);
	AddCommand(IDC_CHECK1, &COptionsColors::OnHighlightsChanged);
	AddCommand(IDC_CHECK2, &COptionsColors::OnPreviewChanged);
	AddCommand(IDC_CHECK3, &COptionsColors::OnSettingsChanged);
	AddCommand(IDC_CHECK4, &COptionsColors::OnHighlightsChanged);
	AddCommand(IDC_CHECK5, &COptionsColors::OnSettingsChanged);
	AddCommand(IDC_RADIO1, &COptionsColors::OnSettingsChanged);
	AddCommand(IDC_RADIO2, &COptionsColors::OnSettingsChanged);
	AddCommand(IDC_RADIO3, &COptionsColors::OnSettingsChanged);
	AddCommand(IDC_RADIO4, &COptionsColors::OnSettingsChanged);
	AddCommand(IDC_RADIO5, &COptionsColors::OnSettingsChanged);
}


void COptionsColors::DoDataExchange(DataExchange &dx)
{
	PropertyPage::DoDataExchange(dx);
	//{{AFX_DATA_MAP(COptionsColors)
	ExchangeControl(dx, IDC_COMBO1, m_ComboItem);
	ExchangeControl(dx, IDC_COMBO2, m_ComboFont);
	ExchangeControl(dx, IDC_COMBO3, m_ComboPreset);
	ExchangeControl(dx, IDC_COMBO4, m_ComboDPIAwareness);
	ExchangeControl(dx, IDC_BUTTON1, m_BtnColor[0]);
	ExchangeControl(dx, IDC_BUTTON2, m_BtnColor[1]);
	ExchangeControl(dx, IDC_BUTTON3, m_BtnColor[2]);
	ExchangeControl(dx, IDC_BUTTON4, m_BtnPreview);
	ExchangeControl(dx, IDC_TEXT1, m_TxtColor[0]);
	ExchangeControl(dx, IDC_TEXT2, m_TxtColor[1]);
	ExchangeControl(dx, IDC_TEXT3, m_TxtColor[2]);
	ExchangeControl(dx, IDC_SPIN1, m_ColorSpin);
	ExchangeControl(dx, IDC_SPIN2, m_spinRowsPerMeasure);
	ExchangeControl(dx, IDC_SPIN3, m_spinRowsPerBeat);
	ExchangeControl(dx, IDC_PRIMARYHILITE, m_rpmEdit);
	ExchangeControl(dx, IDC_SECONDARYHILITE, m_rpbEdit);
	//}}AFX_DATA_MAP
}


COptionsColors::COptionsColors()
    : CPropertyPage{IDD_OPTIONS_COLORS}
    , CustomColors{TrackerSettings::Instance().rgbCustomColors}
{
	m_rpbEdit->SetAccessibleSuffix(UL_("rows per beat"));
	m_rpmEdit->SetAccessibleSuffix(UL_("rows per measure"));
}


COptionsColors::~COptionsColors()
{
	delete m_pPreviewDib;
}


static mpt::ustring FormatFontName(const FontSetting &font)
{
	return mpt::ToUnicode(font.name + U_(", ") + mpt::ufmt::val(font.size / 10));
}


bool COptionsColors::OnInitDialog()
{
	PropertyPage::OnInitDialog();
	
	m_pPreviewDib = LoadDib(MAKEINTRESOURCE(IDB_COLORSETUP));
	
	m_ComboDPIAwareness->setUpdatesEnabled(false);
	static constexpr std::pair<const mpt::uchar *, DPIAwarenessMode> DPIModes[] =
	{
		{UL_("Not DPI-Aware"),                DPIAwarenessMode::NoDPIAwareness           },
		{UL_("Not DPI-Aware; GDI upscaling"), DPIAwarenessMode::NoDPIAwarenessGDIUpscaled},
		{UL_("System DPI-Aware"),             DPIAwarenessMode::SystemDPIAware           },
		{UL_("Per-Monitor DPI-Aware"),        DPIAwarenessMode::PerMonitorDPIAware       },
	};
	const DPIAwarenessMode currentDPIMode = TrackerSettings::Instance().dpiAwareness;
	for(const auto dpiMode : DPIModes)
	{
		int item = mfcless::AddItem(m_ComboDPIAwareness, dpiMode.first);
		mfcless::SetItemData(m_ComboDPIAwareness, item, static_cast<uintptr_t>(dpiMode.second));
		if(currentDPIMode == dpiMode.second)
			m_ComboDPIAwareness->setCurrentIndex(item);
	}
	m_ComboDPIAwareness->setUpdatesEnabled(true);
	
	m_ComboItem->setUpdatesEnabled(false);
	for (size_t i = 0; i < std::size(colorDefs); i++)
	{
		mfcless::SetItemData(m_ComboItem, mfcless::AddItem(m_ComboItem, colorDefs[i].name), i);
	}
	m_ComboItem->setUpdatesEnabled(true);
	m_ComboItem->setCurrentIndex(0);

	m_BtnPreview->SetWindowPos(nullptr,
		0, 0,
		HighDPISupport::ScalePixels(PREVIEWBMP_WIDTH * 2, this) + 2, HighDPISupport::ScalePixels(PREVIEWBMP_HEIGHT * 2, this) + 2,
		mfcless::swp::NoMove | mfcless::swp::NoZOrder | mfcless::swp::NoActivate);
	const FlagSet<PatternSetup> patternSetup = TrackerSettings::Instance().patternSetup;
	if(patternSetup[PatternSetup::HighlightMeasures]) SetChecked(IDC_CHECK1, 1);
	if(patternSetup[PatternSetup::EffectHighlight]) SetChecked(IDC_CHECK2, 1);
	if(patternSetup[PatternSetup::HighlightBeats]) SetChecked(IDC_CHECK4, 1);
	SetChecked(IDC_CHECK3, TrackerSettings::Instance().patternIgnoreSongTimeSignature ? 1 : 0);
	SetChecked(IDC_CHECK5, TrackerSettings::Instance().rememberSongWindows ? 1 : 0);
	CheckRadio(IDC_RADIO1, IDC_RADIO2, TrackerSettings::Instance().accidentalFlats ? IDC_RADIO2 : IDC_RADIO1);
	CheckRadio(IDC_RADIO3, IDC_RADIO5, IDC_RADIO3 + static_cast<int>(TrackerSettings::Instance().defaultRainbowChannelColors.Get()));
	EnableItem(IDC_CHECK3, (IsChecked(IDC_CHECK1) || IsChecked(IDC_CHECK4)) ? true : false);

	SetItemInt(IDC_PRIMARYHILITE, TrackerSettings::Instance().m_nRowHighlightMeasures);
	SetItemInt(IDC_SECONDARYHILITE, TrackerSettings::Instance().m_nRowHighlightBeats);
	m_spinRowsPerMeasure->SetRange(0, MAX_ROWS_PER_MEASURE);
	m_spinRowsPerBeat->SetRange(0, MAX_ROWS_PER_BEAT);

	patternFont = TrackerSettings::Instance().patternFont;
	m_ComboFont->setUpdatesEnabled(false);
	mfcless::AddItem(m_ComboFont, UL_("Built-in (small)"));
	mfcless::AddItem(m_ComboFont, UL_("Built-in (large)"));
	mfcless::AddItem(m_ComboFont, UL_("Built-in (small, x2)"));
	mfcless::AddItem(m_ComboFont, UL_("Built-in (large, x2)"));
	mfcless::AddItem(m_ComboFont, UL_("Built-in (small, x3)"));
	mfcless::AddItem(m_ComboFont, UL_("Built-in (large, x3)"));
	int sel = 0;
	if(patternFont.name == PATTERNFONT_SMALL)
	{
		sel = patternFont.size * 2;
	} else if(patternFont.name == PATTERNFONT_LARGE)
	{
		sel = patternFont.size * 2 + 1;
	} else
	{
		mfcless::AddItem(m_ComboFont, FormatFontName(patternFont));
		sel = 6;
	}
	m_ComboFont->setUpdatesEnabled(true);
	m_ComboFont->setCurrentIndex(sel);

	commentFont = TrackerSettings::Instance().commentsFont;
	SetItemText(IDC_BUTTON10, FormatFontName(commentFont));

	m_ComboPreset->setUpdatesEnabled(false);
	const size_t numItems = 2 + std::size(ColorSchemes);
	m_ComboPreset->InitStorage(static_cast<int>(numItems), static_cast<uint32>(numItems * 20 * sizeof(mpt::uchar)));
	mfcless::AddItem(m_ComboPreset, UL_("Choose a Colour Scheme..."));
	mfcless::AddItem(m_ComboPreset, UL_("OpenMPT (Default)"));
	for(const auto &preset : ColorSchemes)
	{
		m_ComboPreset->SetItemDataPtr(mfcless::AddItem(m_ComboPreset, preset.name), const_cast<ColorScheme *>(&preset));
	}
	m_ComboPreset->setCurrentIndex(0);
	m_ComboPreset->setUpdatesEnabled(true);

	m_ColorSpin->SetRange(-1, 1);
	
	OnColorSelChanged();
	return true;
}


bool COptionsColors::OnKillActive()
{
	int highlightMeasures = GetItemInt(IDC_PRIMARYHILITE);
	int highlightBeats = GetItemInt(IDC_SECONDARYHILITE);

	if(highlightBeats > highlightMeasures)
	{
		Reporting::Warning("Error: Primary highlight must be greater than or equal secondary highlight.");
		mfcless::GotoDlgCtrl(GetDlgItem(IDC_PRIMARYHILITE));
		return 0;
	}

	return PropertyPage::OnKillActive();
}


void COptionsColors::OnOK()
{
	if(m_ComboDPIAwareness->currentIndex() >= 0)
	{
		const DPIAwarenessMode currentDPIMode = TrackerSettings::Instance().dpiAwareness;
		const DPIAwarenessMode newDPIMode = static_cast<DPIAwarenessMode>(mfcless::GetItemData(m_ComboDPIAwareness, m_ComboDPIAwareness->currentIndex()));
		if(newDPIMode != currentDPIMode)
		{
			TrackerSettings::Instance().dpiAwareness = newDPIMode;
			Reporting::Information(UL_("You need to restart OpenMPT for the new DPI-awareness setting to take effect."));
		}
	}

	FlagSet<PatternSetup> patternSetup = TrackerSettings::Instance().patternSetup;
	patternSetup.set(PatternSetup::HighlightMeasures, IsChecked(IDC_CHECK1) != 0);
	patternSetup.set(PatternSetup::EffectHighlight, IsChecked(IDC_CHECK2) != 0);
	patternSetup.set(PatternSetup::HighlightBeats, IsChecked(IDC_CHECK4) != 0);
	TrackerSettings::Instance().patternSetup = patternSetup;
	TrackerSettings::Instance().patternIgnoreSongTimeSignature = IsChecked(IDC_CHECK3) != 0;
	TrackerSettings::Instance().rememberSongWindows = IsChecked(IDC_CHECK5) != 0;
	TrackerSettings::Instance().accidentalFlats = IsChecked(IDC_RADIO2) != 0;

	int channelColors = GetCheckedRadio(IDC_RADIO3, IDC_RADIO5);
	if(channelColors == IDC_RADIO3)
		TrackerSettings::Instance().defaultRainbowChannelColors = DefaultChannelColors::NoColors;
	else if(channelColors == IDC_RADIO4)
		TrackerSettings::Instance().defaultRainbowChannelColors = DefaultChannelColors::Rainbow;
	else
		TrackerSettings::Instance().defaultRainbowChannelColors = DefaultChannelColors::Random;

	FontSetting newPatternFont = patternFont;
	const int fontSel = m_ComboFont->currentIndex();
	switch(fontSel)
	{
	case 0:
	case 2:
	case 4:
	default:
		newPatternFont.name = PATTERNFONT_SMALL;
		newPatternFont.size = fontSel / 2;
		break;
	case 1:
	case 3:
	case 5:
		newPatternFont.name = PATTERNFONT_LARGE;
		newPatternFont.size = fontSel / 2;
		break;
	case 6:
		break;
	}
	TrackerSettings::Instance().patternFont = newPatternFont;
	TrackerSettings::Instance().commentsFont = commentFont;

	TrackerSettings::Instance().m_nRowHighlightMeasures = GetItemInt(IDC_PRIMARYHILITE);
	TrackerSettings::Instance().m_nRowHighlightBeats = GetItemInt(IDC_SECONDARYHILITE);

	TrackerSettings::Instance().rgbCustomColors = CustomColors;
	CMainFrame::UpdateColors();
	CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
	if (pMainFrm) pMainFrm->PostMessage(WM_MOD_INVALIDATEPATTERNS, HINT_MPTOPTIONS);
	CSoundFile::SetDefaultNoteNames();
	PropertyPage::OnOK();
}


bool COptionsColors::OnSetActive()
{
	CMainFrame::m_nLastOptionsPage = OPTIONS_PAGE_COLORS;
	return PropertyPage::OnSetActive();
}


void COptionsColors::OnChoosePatternFont()
{
	LOGFONT lf;
	MemsetZero(lf);
	const int32 size = patternFont.size < 10 ? 120 : patternFont.size;
	// Point size to pixels
	lf.lfHeight = size;
	lf.lfWeight = patternFont.flags[FontSetting::Bold] ? FW_BOLD : FW_NORMAL;
	lf.lfItalic = patternFont.flags[FontSetting::Italic] ? true : false;
	mpt::String::WriteWinBuf(lf.lfFaceName) = mpt::ToUnicode(patternFont.name);
	CFontDialogEx dlg(&lf);
	dlg.m_cf.hwndOwner = this;
	if(patternFont.name != PATTERNFONT_SMALL && patternFont.name != PATTERNFONT_LARGE)
	{
		dlg.m_cf.lpLogFont = &lf;
	}
	dlg.m_cf.Flags &= ~CF_EFFECTS;
	dlg.m_cf.Flags |= /*CF_FIXEDPITCHONLY | */CF_FORCEFONTEXIST | CF_NOSCRIPTSEL;
	if(dlg.DoModal() == IDOK)
	{
		while(m_ComboFont->count() > 6)
		{
			m_ComboFont->removeItem(6);
		}
		patternFont.name = mpt::ToUnicode(dlg.GetFaceName());
		patternFont.size = dlg.GetSize();
		patternFont.flags = FontSetting::None;
		if(dlg.IsBold()) patternFont.flags |= FontSetting::Bold;
		if(dlg.IsItalic()) patternFont.flags |= FontSetting::Italic;
		mfcless::AddItem(m_ComboFont, FormatFontName(patternFont));
		m_ComboFont->setCurrentIndex(6);
		OnSettingsChanged();
	}
}


void COptionsColors::OnChooseCommentFont()
{
	LOGFONT lf;
	MemsetZero(lf);
	// Point size to pixels
	lf.lfHeight = commentFont.size;
	lf.lfWeight = commentFont.flags[FontSetting::Bold] ? FW_BOLD : FW_NORMAL;
	lf.lfItalic = commentFont.flags[FontSetting::Italic] ? true : false;
	mpt::String::WriteWinBuf(lf.lfFaceName) = mpt::ToUnicode(commentFont.name);
	CFontDialogEx dlg(&lf);
	dlg.m_cf.hwndOwner = this;
	dlg.m_cf.lpLogFont = &lf;
	dlg.m_cf.Flags &= ~CF_EFFECTS;
	dlg.m_cf.Flags |= CF_FORCEFONTEXIST | CF_NOSCRIPTSEL;
	if(dlg.DoModal() == IDOK)
	{
		commentFont.name = mpt::ToUnicode(dlg.GetFaceName());
		commentFont.size = dlg.GetSize();
		commentFont.flags = FontSetting::None;
		if(dlg.IsBold()) commentFont.flags |= FontSetting::Bold;
		if(dlg.IsItalic()) commentFont.flags |= FontSetting::Italic;
		SetItemText(IDC_BUTTON10, FormatFontName(commentFont));
		OnSettingsChanged();
	}
}


void COptionsColors::OnDrawItem(int nIdCtl, mfcless::DrawItemInfo * lpdis)
{
	if(!lpdis || nIdCtl != IDC_BUTTON4 || !m_pPreviewDib)
	{
		PropertyPage::OnDrawItem(nIdCtl, lpdis);
		return;
	}

	const int img = colorDefs[m_nColorItem].previewImage;
	auto &p = m_pPreviewDib->bmiColors;
	if (IsChecked(IDC_CHECK2))
	{
		p[1] = rgb2quad(CustomColors[MODCOLOR_GLOBALS]);
		p[3] = rgb2quad(CustomColors[MODCOLOR_PITCH]);
		p[5] = rgb2quad(CustomColors[MODCOLOR_INSTRUMENT]);
		p[6] = rgb2quad(CustomColors[MODCOLOR_VOLUME]);
		p[12] = rgb2quad(CustomColors[MODCOLOR_NOTE]);
		p[14] = rgb2quad(CustomColors[MODCOLOR_PANNING]);
	} else
	{
		p[1] = rgb2quad(CustomColors[MODCOLOR_TEXTNORMAL]);
		p[3] = rgb2quad(CustomColors[MODCOLOR_TEXTNORMAL]);
		p[5] = rgb2quad(CustomColors[MODCOLOR_TEXTNORMAL]);
		p[6] = rgb2quad(CustomColors[MODCOLOR_TEXTNORMAL]);
		p[12] = rgb2quad(CustomColors[MODCOLOR_TEXTNORMAL]);
		p[14] = rgb2quad(CustomColors[MODCOLOR_TEXTNORMAL]);
	}
	p[4] = rgb2quad(CustomColors[MODCOLOR_TEXTNORMAL]);
	p[8] = rgb2quad(CustomColors[MODCOLOR_TEXTNORMAL]);
	p[9] = rgb2quad(CustomColors[MODCOLOR_SAMPLE]);
	p[10] = rgb2quad(CustomColors[MODCOLOR_BACKNORMAL]);
	p[11] = rgb2quad(CustomColors[MODCOLOR_BACKHILIGHT]);
	p[13] = rgb2quad(CustomColors[MODCOLOR_ENVELOPES]);
	p[15] = rgb2quad(img ? mfcless::Rgb(255, 255, 255) : CustomColors[MODCOLOR_BACKNORMAL]);
	// Special cases: same bitmap, different palette
	switch(m_nColorItem)
	{
	// Current Row
	case 1:
		p[8] = rgb2quad(CustomColors[MODCOLOR_TEXTCURROW]);
		p[11] = rgb2quad(CustomColors[MODCOLOR_BACKCURROW]);
		break;
	// Selection
	case 2:
		p[5] = rgb2quad(CustomColors[MODCOLOR_TEXTSELECTED]);
		p[6] = rgb2quad(CustomColors[MODCOLOR_TEXTSELECTED]);
		p[8] = rgb2quad(CustomColors[MODCOLOR_TEXTSELECTED]);
		p[11] = rgb2quad(CustomColors[MODCOLOR_BACKSELECTED]);
		p[12] = rgb2quad(CustomColors[MODCOLOR_TEXTSELECTED]);
		break;
	// Play Cursor
	case 3:
		p[8] = rgb2quad(CustomColors[MODCOLOR_TEXTPLAYCURSOR]);
		p[11] = rgb2quad(CustomColors[MODCOLOR_BACKPLAYCURSOR]);
		break;
	// Sample Editor
	case 9:
	case 10:
		p[0] = rgb2quad(CustomColors[MODCOLOR_BACKSAMPLE]);
		p[7] = rgb2quad(mfcless::SystemColor(mfcless::SysColor::ButtonFace));
		p[8] = rgb2quad(mfcless::SystemColor(mfcless::SysColor::ButtonShadow));
		p[10] = rgb2quad(CustomColors[MODCOLOR_SAMPLE_LOOPMARKER]);
		p[11] = rgb2quad(CustomColors[MODCOLOR_SAMPLE_CUEPOINT]);
		p[14] = rgb2quad(CustomColors[MODCOLOR_SAMPLE_SUSTAINMARKER]);
		p[15] = rgb2quad(CustomColors[MODCOLOR_SAMPLESELECTED]);
		break;
	// Envelope Editor
	case 11:
		p[0] = rgb2quad(CustomColors[MODCOLOR_BACKENV]);
		p[2] = rgb2quad(CustomColors[MODCOLOR_ENVELOPE_RELEASE]);
		break;
	}

	mfcless::Rect rect = lpdis->rcItem;
	mfcless::DC * hdc = lpdis->hDC;
	::DrawEdge(hdc, rect, mfcless::edge::SunkenInner, mfcless::edge::Rect | mfcless::edge::Adjust);
	StretchDIBits(	hdc,
					rect.left,
					rect.top,
					rect.Width(),
					rect.Height(),
					0,
					m_pPreviewDib->bmiHeader.biHeight - ((img+1) * PREVIEWBMP_HEIGHT),
					m_pPreviewDib->bmiHeader.biWidth,
					PREVIEWBMP_HEIGHT,
					m_pPreviewDib->lpDibBits,
					(LPBITMAPINFO)m_pPreviewDib,
					DIB_RGB_COLORS,
					mfcless::rop::SrcCopy);
}


static uint32 rgbCustomColors[16] =
{
	0x808080,	0x0000FF,	0x00FF00,	0x00FFFF,
	0xFF0000,	0xFF00FF,	0xFFFF00,	0xFFFFFF,
	0xC0C0C0,	0x80FFFF,	0xE0E8E0,	0x606060,
	0x505050,	0x404040,	0x004000,	0x000000,
};


void COptionsColors::SelectColor(int colorIndex)
{
	auto &color = CustomColors[colorDefs[m_nColorItem].colorIndex[colorIndex]];
	CHOOSECOLOR cc;
	cc.lStructSize = sizeof(CHOOSECOLOR);
	cc.hwndOwner = this;
	cc.hInstance = NULL;
	cc.rgbResult = color;
	cc.lpCustColors = rgbCustomColors;
	cc.Flags = CC_RGBINIT | CC_FULLOPEN;
	cc.lCustData = 0;
	cc.lpfnHook = nullptr;
	cc.lpTemplateName = nullptr;
	if(::ChooseColor(&cc))
	{
		color = cc.rgbResult;
		m_BtnColor[colorIndex].SetColor(color);
		OnSettingsChanged();
	}
}


void COptionsColors::OnVScroll(mfcless::ScrollCode code, int pos, QWidget *pScrollBar)
{
	PropertyPage::OnVScroll(nSBCode, nPos, pScrollBar);
	int newSel = m_ComboItem->currentIndex() - m_ColorSpin->GetPos();
	if(newSel >= 0)
	{
		m_ComboItem->setCurrentIndex(newSel);
		OnColorSelChanged();
	}
	m_ColorSpin->SetPos(0);
}


void COptionsColors::OnColorSelChanged()
{
	int sel = m_ComboItem->currentIndex();
	if (sel >= 0)
	{
		m_nColorItem = static_cast<uint32>(mfcless::GetItemData(m_ComboItem, sel));
		OnUpdateDialog();
	}
}


void COptionsColors::OnHighlightsChanged()
{
	EnableItem(IDC_CHECK3, (IsChecked(IDC_CHECK1) || IsChecked(IDC_CHECK4)) ? true : false);
	OnSettingsChanged();
}


void COptionsColors::OnSettingsChanged()
{
	SetModified(true);
}

void COptionsColors::OnUpdateDialog()
{
	const ColorDescriptions &cd = colorDefs[m_nColorItem];
	for(int i = 0; i < 3; i++)
	{
		if(cd.descText[i])
		{
			mfcless::SetWindowText(m_TxtColor[i], cd.descText[i]);
			m_BtnColor[i].SetColor(CustomColors[cd.colorIndex[i]]);
		}
		m_TxtColor[i].ShowWindow(cd.descText[i] ? mfcless::show::Show : mfcless::show::Hide);
		m_BtnColor[i].ShowWindow(cd.descText[i] ? mfcless::show::Show : mfcless::show::Hide);
	}

	m_BtnPreview->update(false);
}


void COptionsColors::OnPreviewChanged()
{
	OnSettingsChanged();
	m_BtnPreview->update(false);
	for(int i = 0; i < 3; i++)
	{
		m_BtnColor[i].SetColor(CustomColors[colorDefs[m_nColorItem].colorIndex[i]]);
	}
}


void COptionsColors::OnPresetChange()
{
	auto curSel = m_ComboPreset->currentIndex();
	if(curSel == 0)
		return;
	TrackerSettings::GetDefaultColourScheme(CustomColors);
	auto scheme = static_cast<const ColorScheme *>(m_ComboPreset->GetItemDataPtr(curSel));
	if(scheme != nullptr)
	{
		for(const auto &c : scheme->colors)
		{
			CustomColors[c.id] = c.color;
		}
	}
	OnPreviewChanged();
}


void COptionsColors::OnLoadColorScheme()
{
	FileDialog dlg = OpenFileDialog()
		.DefaultExtension(U_("mptcolor"))
		.ExtensionFilter(U_("OpenMPT Color Schemes|*.mptcolor||"))
		.WorkingDirectory(theApp.GetConfigPath());
	if(!dlg.Show(this)) return;

	// Ensure that all colours are reset (for outdated colour schemes)
	TrackerSettings::GetDefaultColourScheme(CustomColors);
	{
		IniFileSettingsContainer file(dlg.GetFirstFile());
		for(uint32 i = 0; i < MAX_MODCOLORS; i++)
		{
			CustomColors[i] = file.Read<int32>(U_("Colors"), MPT_UFORMAT("Color{}")(mpt::ufmt::dec0<2>(i)), CustomColors[i]);
			// For old color schemes that don't have this color yet
			if(i == MODCOLOR_BACKCURROW)
				CustomColors[MODCOLOR_BACKRECORDROW] = CustomColors[MODCOLOR_BACKCURROW];
		}
	}
	OnPreviewChanged();
}

void COptionsColors::OnSaveColorScheme()
{
	FileDialog dlg = SaveFileDialog()
		.DefaultExtension(U_("mptcolor"))
		.ExtensionFilter(U_("OpenMPT Color Schemes|*.mptcolor||"))
		.WorkingDirectory(theApp.GetConfigPath());
	if(!dlg.Show(this)) return;

	{
		IniFileSettingsContainer file(dlg.GetFirstFile());
		for(uint32 i = 0; i < MAX_MODCOLORS; i++)
		{
			file.Write<int32>(U_("Colors"), MPT_UFORMAT("Color{}")(mpt::ufmt::dec0<2>(i)), CustomColors[i]);
		}
	}
}


void COptionsColors::OnClearWindowCache()
{
	SettingsContainer &settings = theApp.GetSongSettings();
	// First, forget all settings...
	settings.ForgetAll();
	// Then make sure they are gone for good.
	::DeleteFile(theApp.GetSongSettingsFilename().AsNative().c_str());
}


OPENMPT_NAMESPACE_END
