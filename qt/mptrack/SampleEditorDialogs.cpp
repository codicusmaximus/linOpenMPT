/*
 * SampleEditorDialogs.cpp
 * -----------------------
 * Purpose: Code for various dialogs that are used in the sample editor.
 * Notes  : mfcless twin of upstream's mptrack/SampleEditorDialogs.cpp.
 *          Draft from qt/tools/twin.py, not yet edited by hand.
 * Authors: Olivier Lapicque
 *          OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "SampleEditorDialogs.h"
#include "HighDPISupport.h"
#include "Mptrack.h"
#include "Reporting.h"
#include "resource.h"
#include "../common/misc_util.h"
#include "../soundlib/Snd_defs.h"
#include "../soundlib/ModSample.h"
#include "ProgressDialog.h"


OPENMPT_NAMESPACE_BEGIN


//////////////////////////////////////////////////////////////////////////
// Sample amplification dialog

void CAmpDlg::RegisterMessageMap()
{
	DialogBase::RegisterMessageMap();
	// ON_WM_DESTROY(): by hand (Qt event override)
	AddNotify(IDC_EDIT2, mfcless::Notify::EditChange, &CAmpDlg::EnableFadeIn);
	AddNotify(IDC_EDIT3, mfcless::Notify::EditChange, &CAmpDlg::EnableFadeOut);
}

void CAmpDlg::DoDataExchange(DataExchange &dx)
{
	DialogBase::DoDataExchange(dx);
	//{{AFX_DATA_MAP(CAmpDlg)
	ExchangeControl(dx, IDC_COMBO1, m_fadeBox);
	//}}AFX_DATA_MAP
}

CAmpDlg::CAmpDlg(QWidget *parent, AmpSettings &settings, int16 factorMin, int16 factorMax)
	: DialogBase(IDD_SAMPLE_AMPLIFY, parent)
	, m_settings(settings)
	, m_nFactorMin(factorMin)
	, m_nFactorMax(factorMax)
{}

bool CAmpDlg::OnInitDialog()
{
	DialogBase::OnInitDialog();
	CSpinButtonCtrl *spin = (CSpinButtonCtrl *)GetDlgItem(IDC_SPIN1);
	spin->SetRange32(m_nFactorMin, m_nFactorMax);
	spin->SetPos32(m_settings.factor);
	spin = (CSpinButtonCtrl *)GetDlgItem(IDC_SPIN2);
	spin->SetRange32(0, 100);
	spin->SetPos32(m_settings.fadeInStart);
	spin = (CSpinButtonCtrl *)GetDlgItem(IDC_SPIN3);
	spin->SetRange32(0, 100);
	spin->SetPos32(m_settings.fadeOutEnd);

	SetItemInt(IDC_EDIT1, m_settings.factor);
	SetItemInt(IDC_EDIT2, m_settings.fadeInStart);
	SetItemInt(IDC_EDIT3, m_settings.fadeOutEnd);
	m_edit->SubclassDlgItem(IDC_EDIT1, this);
	m_edit->AllowFractions(false);
	m_edit->AllowNegative(m_nFactorMin < 0);
	m_editFadeIn->SubclassDlgItem(IDC_EDIT2, this);
	m_editFadeIn->AllowFractions(false);
	m_editFadeIn->AllowNegative(m_nFactorMin < 0);
	m_editFadeOut->SubclassDlgItem(IDC_EDIT3, this);
	m_editFadeOut->AllowFractions(false);
	m_editFadeOut->AllowNegative(m_nFactorMin < 0);

	const struct
	{
		const mpt::uchar *name;
		Fade::Law id;
	} fadeLaws[] =
	{
		{ U_("Linear"),       Fade::kLinear },
		{ U_("Exponential"),  Fade::kPow },
		{ U_("Square Root"),  Fade::kSqrt },
		{ U_("Logarithmic"),  Fade::kLog },
		{ U_("Quarter Sine"), Fade::kQuarterSine },
		{ U_("Half Sine"),    Fade::kHalfSine },
	};

	// Add fade laws to list
	COMBOBOXEXITEM cbi;
	MemsetZero(cbi);
	cbi.mask = CBEIF_IMAGE | CBEIF_SELECTEDIMAGE | CBEIF_TEXT | CBEIF_LPARAM;
	for(int i = 0; i < static_cast<int>(std::size(fadeLaws)); i++)
	{
		cbi.iItem = i;
		cbi.pszText = const_cast<LPTSTR>(fadeLaws[i].name);
		cbi.iImage = cbi.iSelectedImage = i;
		cbi.lParam = fadeLaws[i].id;
		m_fadeBox->InsertItem(&cbi);
		if(fadeLaws[i].id == m_settings.fadeLaw) m_fadeBox->setCurrentIndex(i);
	}

	OnDPIChanged();

	m_locked = false;

	return true;
}


void CAmpDlg::OnDPIChanged()
{
	// Create icons for fade laws
	const int items = m_fadeBox->count();
	const int iconSize = HighDPISupport::ScalePixels(16, m_hWnd);
	const int imgWidth = iconSize * items;
	std::vector<uint32> bits(imgWidth * iconSize, mfcless::Rgb(255, 0, 255));
	const uint32 col = GetSysColor(COLOR_WINDOWTEXT);
	for(int i = 0, baseX = 0; i < items; i++, baseX += iconSize)
	{
		Fade::Func fadeFunc = Fade::GetFadeFunc(static_cast<Fade::Law>(mfcless::GetItemData(m_fadeBox, i)));
		int oldVal = iconSize - 1;
		for(int x = 0; x < iconSize; x++)
		{
			int val = iconSize - 1 - mpt::saturate_round<int>(iconSize * fadeFunc(static_cast<double>(x) / iconSize));
			Limit(val, 0, iconSize - 1);
			if(oldVal > val && x > 0)
			{
				int dy = (oldVal - val) / 2;
				for(int y = oldVal * imgWidth; dy != 0; y -= imgWidth, dy--)
				{
					bits[baseX + (x - 1) + y] = col;
				}
				oldVal -= dy + 1;
			}
			for(int y = oldVal * imgWidth; y >= val * imgWidth; y -= imgWidth)
			{
				bits[baseX + x + y] = col;
			}
			oldVal = val;
		}
	}
	mfcless::Bitmap bitmap;
	bitmap.CreateBitmap(imgWidth, iconSize, 1, 32, bits.data());
	m_list.DeleteImageList();
	m_list.Create(iconSize, iconSize, ILC_COLOR32 | ILC_MASK, 0, 1);
	m_list.Add(&bitmap, mfcless::Rgb(255, 0, 255));
	bitmap.DeleteObject();
	m_fadeBox->SetImageList(&m_list);
}


void CAmpDlg::OnDestroy()
{
	m_list.DeleteImageList();
}


void CAmpDlg::OnOK()
{
	m_settings.factor = static_cast<int16>(Clamp(static_cast<int>(GetItemInt(IDC_EDIT1)), m_nFactorMin, m_nFactorMax));
	m_settings.fadeInStart = Clamp(static_cast<int>(GetItemInt(IDC_EDIT2)), m_nFactorMin, m_nFactorMax);
	m_settings.fadeOutEnd = Clamp(static_cast<int>(GetItemInt(IDC_EDIT3)), m_nFactorMin, m_nFactorMax);
	m_settings.fadeIn = (IsChecked(IDC_CHECK1) != 0);
	m_settings.fadeOut = (IsChecked(IDC_CHECK2) != 0);
	m_settings.fadeLaw = static_cast<Fade::Law>(mfcless::GetItemData(m_fadeBox, m_fadeBox->currentIndex()));
	DialogBase::OnOK();
}


void CAmpDlg::EnableFadeIn()
{
	if(!m_locked)
		SetChecked(IDC_CHECK1, 1);
}


void CAmpDlg::EnableFadeOut()
{
	if(!m_locked)
		SetChecked(IDC_CHECK2, 1);
}


//////////////////////////////////////////////////////////////
// Sample import dialog

SampleIO CRawSampleDlg::m_format(SampleIO::_8bit, SampleIO::mono, SampleIO::littleEndian, SampleIO::signedPCM);
SmpLength CRawSampleDlg::m_offset = 0;

void CRawSampleDlg::RegisterMessageMap()
{
	DialogBase::RegisterMessageMap();
	AddCommandRange(IDC_RADIO1, IDC_RADIO4, &CRawSampleDlg::OnBitDepthChanged);
	AddCommandRange(IDC_RADIO7, IDC_RADIO10, &CRawSampleDlg::OnEncodingChanged);
	AddCommand(IDC_BUTTON1, &CRawSampleDlg::OnAutodetectFormat);
}


void CRawSampleDlg::DoDataExchange(DataExchange &dx)
{
	DialogBase::DoDataExchange(dx);
	//{{AFX_DATA_MAP(CRawSampleDlg)
	ExchangeControl(dx, IDC_SPIN1, m_SpinOffset);
	//}}AFX_DATA_MAP
}


CRawSampleDlg::CRawSampleDlg(FileReader &file, QWidget *parent)
	: DialogBase{IDD_LOADRAWSAMPLE, parent}
	, m_file{file}
{
}


bool CRawSampleDlg::OnInitDialog()
{
	DialogBase::OnInitDialog();
	if(const auto filename = m_file.GetOptionalFileName(); filename)
	{
		mpt::ustring title;
		GetWindowText(title);
		title += U_(" - ") + filename->GetFilename().ToCString();
		SetWindowText(title);
	}
	m_SpinOffset->SetRange(0, mpt::saturate_cast<int>(m_file.GetLength() - 1u));
	UpdateDialog();
	return true;
}


void CRawSampleDlg::OnOK()
{
	const int bitDepth = GetCheckedRadio(IDC_RADIO1, IDC_RADIO4);
	const int channels = GetCheckedRadio(IDC_RADIO5, IDC_RADIO6);
	const int encoding = GetCheckedRadio(IDC_RADIO7, IDC_RADIO10);
	const int endianness = GetCheckedRadio(IDC_RADIO11, IDC_RADIO12);
	if(bitDepth == IDC_RADIO1)
		m_format |= SampleIO::_8bit;
	else if(bitDepth == IDC_RADIO2)
		m_format |= SampleIO::_16bit;
	else if(bitDepth == IDC_RADIO3)
		m_format |= SampleIO::_24bit;
	else if(bitDepth == IDC_RADIO4)
		m_format |= SampleIO::_32bit;
	if(channels == IDC_RADIO5)
		m_format |= SampleIO::mono;
	else if(channels == IDC_RADIO6)
		m_format |= SampleIO::stereoInterleaved;
	if(encoding == IDC_RADIO7)
		m_format |= SampleIO::signedPCM;
	else if(encoding == IDC_RADIO8)
		m_format |= SampleIO::unsignedPCM;
	else if(encoding == IDC_RADIO9)
		m_format |= SampleIO::deltaPCM;
	else if(encoding == IDC_RADIO10)
		m_format |= SampleIO::floatPCM;
	if(endianness == IDC_RADIO11)
		m_format |= SampleIO::littleEndian;
	else if(endianness == IDC_RADIO12)
		m_format |= SampleIO::bigEndian;
	m_rememberFormat = IsChecked(IDC_CHK_REMEMBERSETTINGS) != 0;
	m_offset = GetItemInt(IDC_EDIT1);
	DialogBase::OnOK();
}


void CRawSampleDlg::UpdateDialog()
{
	const int bitDepthID = IDC_RADIO1 + m_format.GetBitDepth() / 8 - 1;
	CheckRadio(IDC_RADIO1, IDC_RADIO4, bitDepthID);
	CheckRadio(IDC_RADIO5, IDC_RADIO6, (m_format.GetChannelFormat() == SampleIO::mono) ? IDC_RADIO5 : IDC_RADIO6);
	int encodingID = IDC_RADIO7;
	switch(m_format.GetEncoding())
	{
	case SampleIO::signedPCM: encodingID = IDC_RADIO7; break;
	case SampleIO::unsignedPCM: encodingID = IDC_RADIO8; break;
	case SampleIO::deltaPCM: encodingID = IDC_RADIO9; break;
	case SampleIO::floatPCM: encodingID = IDC_RADIO10; break;
	default: MPT_ASSERT_NOTREACHED();
	}
	CheckRadio(IDC_RADIO7, IDC_RADIO10, encodingID);
	CheckRadio(IDC_RADIO11, IDC_RADIO12, (m_format.GetEndianness() == SampleIO::littleEndian) ? IDC_RADIO11 : IDC_RADIO12);
	SetChecked(IDC_CHK_REMEMBERSETTINGS, (m_rememberFormat ? 1 : 0));
	SetItemInt(IDC_EDIT1, m_offset);

	OnBitDepthChanged(bitDepthID);
	OnEncodingChanged(encodingID);
}


void CRawSampleDlg::OnBitDepthChanged(uint32 id)
{
	const auto bits = (id - IDC_RADIO1 + 1) * 8;
	// 8-bit: endianness doesn't matter
	bool enableEndianness = (bits == 8) ? false : true;
	EnableItem(IDC_RADIO11, enableEndianness);
	EnableItem(IDC_RADIO12, enableEndianness);
	if(bits == 8)
		CheckRadio(IDC_RADIO11, IDC_RADIO12, IDC_RADIO11);

	const bool hasUnsignedDelta = (bits <= 16) ? true : false;
	const bool hasFloat = (bits == 32) ? true : false;

	EnableItem(IDC_RADIO8, hasUnsignedDelta);
	EnableItem(IDC_RADIO9, hasUnsignedDelta);
	EnableItem(IDC_RADIO10, hasFloat);

	const int encoding = GetCheckedRadio(IDC_RADIO7, IDC_RADIO10);
	if((encoding == IDC_RADIO8 && !hasUnsignedDelta)
	   || (encoding == IDC_RADIO9 && !hasUnsignedDelta)
	   || (encoding == IDC_RADIO10 && !hasFloat))
		CheckRadio(IDC_RADIO7, IDC_RADIO10, IDC_RADIO7);
}


void CRawSampleDlg::OnEncodingChanged(uint32 id)
{
	const bool isUnsignedDelta = (id == IDC_RADIO8) || (id == IDC_RADIO9);
	const bool isFloat         = (id == IDC_RADIO10);

	EnableItem(IDC_RADIO1, isFloat ? false : true);
	EnableItem(IDC_RADIO2, isFloat ? false : true);
	EnableItem(IDC_RADIO3, (isFloat || isUnsignedDelta) ? false : true);
	EnableItem(IDC_RADIO4, isUnsignedDelta ? false : true);

	const int bitDepth = GetCheckedRadio(IDC_RADIO1, IDC_RADIO4);
	if(bitDepth != IDC_RADIO4 && isFloat)
		CheckRadio(IDC_RADIO1, IDC_RADIO4, IDC_RADIO4);
	if((bitDepth == IDC_RADIO3 || bitDepth == IDC_RADIO4) && isUnsignedDelta)
		CheckRadio(IDC_RADIO1, IDC_RADIO4, IDC_RADIO1);
}


class AutodetectFormatDlg : public CProgressDialog
{
	CRawSampleDlg &m_parent;

public:
	SampleIO m_bestFormat;

	AutodetectFormatDlg(CRawSampleDlg &parent)
		: CProgressDialog(&parent)
		, m_parent(parent) {}

	void Run() override
	{
		// Probed raw formats... little-endian and stereo versions are automatically checked as well.
		static constexpr SampleIO ProbeFormats[] =
			{
				// 8-Bit
				{SampleIO::_8bit, SampleIO::mono, SampleIO::bigEndian, SampleIO::signedPCM},
				{SampleIO::_8bit, SampleIO::mono, SampleIO::bigEndian, SampleIO::unsignedPCM},
				{SampleIO::_8bit, SampleIO::mono, SampleIO::bigEndian, SampleIO::deltaPCM},
				// 16-Bit
				{SampleIO::_16bit, SampleIO::mono, SampleIO::bigEndian, SampleIO::signedPCM},
				{SampleIO::_16bit, SampleIO::mono, SampleIO::bigEndian, SampleIO::unsignedPCM},
				{SampleIO::_16bit, SampleIO::mono, SampleIO::bigEndian, SampleIO::deltaPCM},
				// 24-Bit
				{SampleIO::_24bit, SampleIO::mono, SampleIO::bigEndian, SampleIO::signedPCM},
				// 32-Bit
				{SampleIO::_32bit, SampleIO::mono, SampleIO::bigEndian, SampleIO::signedPCM},
				{SampleIO::_32bit, SampleIO::mono, SampleIO::bigEndian, SampleIO::floatPCM},
			};

		SetTitle(U_("Raw Import"));
		SetText(U_("Determining raw format..."));
		SetRange(0, std::size(ProbeFormats) * 4);

		double bestError = DBL_MAX;
		uint64 progress  = 0;

		for(SampleIO format : ProbeFormats)
		{
			for(const auto endianness : {SampleIO::littleEndian, SampleIO::bigEndian})
			{
				if(endianness == SampleIO::bigEndian && format.GetBitDepth() == SampleIO::_8bit)
					continue;
				format |= endianness;
				for(const auto channels : {SampleIO::mono, SampleIO::stereoInterleaved})
				{
					format |= channels;

					ModSample sample;
					m_parent.m_file.Seek(m_parent.m_offset);
					const auto bytesPerSample = format.GetNumChannels() * format.GetBitDepth() / 8u;
					sample.nLength            = mpt::saturate_cast<SmpLength>(m_parent.m_file.BytesLeft() / bytesPerSample);
					if(!format.ReadSample(sample, m_parent.m_file))
						continue;

					const uint8 numChannels = sample.GetNumChannels();
					double error            = 0.0;
					for(uint8 chn = 0; chn < numChannels; chn++)
					{
						const auto ComputeSampleError = [](auto *v, SmpLength length, uint8 numChannels)
						{
							const double factor = 1.0 / (1u << (sizeof(*v) * 8u - 1u));
							double error        = 0.0;
							int32 prev          = 0;
							for(SmpLength i = length; i != 0; i--, v += numChannels)
							{
								auto diff = (*v - prev) * factor;
								error += diff * diff;
								prev = *v;
							}
							return error;
						};

						if(sample.uFlags[CHN_16BIT])
							error += ComputeSampleError(sample.sample16() + chn, sample.nLength, numChannels);
						else
							error += ComputeSampleError(sample.sample8() + chn, sample.nLength, numChannels);
					}
					sample.FreeSample();

					double errorFactor = format.GetBitDepth() * format.GetBitDepth() / 64;
					// Delta PCM often produces slightly worse error compared to signed PCM for real delta samples, so give it a bit of an advantage.
					if(format.GetEncoding() == SampleIO::deltaPCM)
						errorFactor *= 0.75;

					error *= errorFactor;

					if(error < bestError)
					{
						bestError  = error;
						m_bestFormat = format;
					}

					SetProgress(++progress);
					ProcessMessages();
					if(m_abort)
					{
						EndDialog(IDCANCEL);
						return;
					}
				}
			}
		}

		EndDialog(IDOK);
	}
};


void CRawSampleDlg::OnAutodetectFormat()
{
	m_offset = GetItemInt(IDC_EDIT1);
	AutodetectFormatDlg dlg(*this);
	if(dlg.DoModal() == IDOK)
	{
		m_format = dlg.m_bestFormat;
		UpdateDialog();
	}
}


/////////////////////////////////////////////////////////////////////////
// Add silence / resize sample dialog

void AddSilenceDlg::RegisterMessageMap()
{
	DialogBase::RegisterMessageMap();
	AddNotify(IDC_COMBO1, mfcless::Notify::SelChange, &AddSilenceDlg::OnUnitChanged);
	AddCommand(IDC_RADIO_ADDSILENCE_BEGIN, &AddSilenceDlg::OnEditModeChanged);
	AddCommand(IDC_RADIO_ADDSILENCE_END, &AddSilenceDlg::OnEditModeChanged);
	AddCommand(IDC_RADIO_RESIZETO, &AddSilenceDlg::OnEditModeChanged);
	AddCommand(IDC_RADIO1, &AddSilenceDlg::OnEditModeChanged);
}

SmpLength AddSilenceDlg::m_addSamples = 32;
SmpLength AddSilenceDlg::m_createSamples = 64;

void AddSilenceDlg::DoDataExchange(DataExchange &dx)
{
	DialogBase::DoDataExchange(dx);
	//{{AFX_DATA_MAP(AddSilenceDlg)
	ExchangeControl(dx, IDC_COMBO1, m_ComboUnit);
	//}}AFX_DATA_MAP
}


AddSilenceDlg::AddSilenceDlg(QWidget *parent, SmpLength origLength, uint32 sampleRate, bool allowOPL)
	: DialogBase(IDD_ADDSILENCE, parent)
	, m_numSamples(m_addSamples)
	, m_sampleRate(sampleRate)
	, m_allowOPL(allowOPL)
{
	if(origLength > 0)
	{
		m_length = origLength;
		m_editOption = kSilenceAtEnd;
	} else
	{
		m_length = m_createSamples;
		m_editOption = kResize;
	}
}


bool AddSilenceDlg::OnInitDialog()
{
	DialogBase::OnInitDialog();

	CSpinButtonCtrl *spin = static_cast<CSpinButtonCtrl *>(GetDlgItem(IDC_SPIN_ADDSILENCE));
	if(spin)
	{
		spin->SetRange32(0, int32_max);
		spin->SetPos32(m_numSamples);
	}

	m_ComboUnit->SetAccessibleName(U_("Length unit"));
	m_ComboUnit->AddString(U_("samples"));
	m_ComboUnit->AddString(U_("ms"));
	m_ComboUnit->SetCurSel(m_unit);
	if(m_sampleRate == 0)
	{
		// Can't do any conversions if samplerate is unknown
		m_ComboUnit->setEnabled(false);
	}

	int buttonID = IDC_RADIO_ADDSILENCE_END;
	switch(m_editOption)
	{
	case kSilenceAtBeginning: buttonID = IDC_RADIO_ADDSILENCE_BEGIN; break;
	case kSilenceAtEnd:       buttonID = IDC_RADIO_ADDSILENCE_END; break;
	case kResize:             buttonID = IDC_RADIO_RESIZETO; break;
	default: break;
	}
	SetChecked(buttonID, 1);

	m_EditAmount->SubclassDlgItem(IDC_EDIT_ADDSILENCE, this);
	m_EditAmount->AllowNegative(false);
	m_EditAmount->AllowFractions(m_unit == kMilliseconds);
	m_EditAmount->SetAccessibleSuffix((m_unit == kSamples) ? U_("samples") : U_("ms"));
	SetDlgItemInt(IDC_EDIT_ADDSILENCE, (m_editOption == kResize) ? m_length : m_numSamples, false);
	EnableItem(IDC_RADIO1, m_allowOPL ? true : false);

	return true;
}


void AddSilenceDlg::OnOK()
{
	m_numSamples = GetEditLength();
	switch(m_editOption = GetEditMode())
	{
	case kSilenceAtBeginning:
	case kSilenceAtEnd:
		m_addSamples = m_numSamples;
		break;
	case kResize:
		m_createSamples = m_numSamples;
		break;
	default:
		break;
	}
	DialogBase::OnOK();
}


SmpLength AddSilenceDlg::GetEditLength() const
{
	if(m_unit == kMilliseconds)
	{
		double ms = 0.0;
		m_EditAmount->GetDecimalValue(ms);
		return mpt::saturate_round<SmpLength>(ms * m_sampleRate / 1000.0);
	} else
	{
		return GetItemInt(IDC_EDIT_ADDSILENCE);
	}
}


void AddSilenceDlg::OnEditModeChanged()
{
	AddSilenceOptions newEditOption = GetEditMode();
	EnableItem(IDC_EDIT_ADDSILENCE, (newEditOption == kOPLInstrument) ? false : true);
	if(newEditOption != kResize && m_editOption == kResize)
	{
		// Switch to "add silence"
		m_length = GetEditLength();
		if(m_unit == kMilliseconds)
			m_EditAmount->SetDecimalValue(m_numSamples * 1000.0 / m_sampleRate);
		else
			SetItemInt(IDC_EDIT_ADDSILENCE, m_numSamples);
	} else if(newEditOption == kResize && m_editOption != kResize)
	{
		// Switch to "resize"
		m_numSamples = GetEditLength();
		if(m_unit == kMilliseconds)
			m_EditAmount->SetDecimalValue(m_length * 1000.0 / m_sampleRate);
		else
			SetItemInt(IDC_EDIT_ADDSILENCE, m_length);
	}
	m_editOption = newEditOption;
}


void AddSilenceDlg::OnUnitChanged()
{
	const auto unit = static_cast<Unit>(m_ComboUnit->GetCurSel());
	if(m_unit == unit)
		return;

	m_unit = unit;
	m_EditAmount->AllowFractions(m_unit == kMilliseconds);
	m_EditAmount->SetAccessibleSuffix((m_unit == kSamples) ? U_("samples") : U_("ms"));
	if(m_unit == kSamples)
	{
		// Convert from milliseconds to samples
		double ms = 0.0;
		m_EditAmount->GetDecimalValue(ms);
		SetItemInt(IDC_EDIT_ADDSILENCE, mpt::saturate_round<SmpLength>(ms * m_sampleRate / 1000.0));
	} else
	{
		// Convert from samples to milliseconds
		SmpLength duration = GetItemInt(IDC_EDIT_ADDSILENCE);
		m_EditAmount->SetDecimalValue(duration * 1000.0 / m_sampleRate);
	}
}


AddSilenceDlg::AddSilenceOptions AddSilenceDlg::GetEditMode() const
{
	if(IsChecked(IDC_RADIO_ADDSILENCE_BEGIN)) return kSilenceAtBeginning;
	else if(IsChecked(IDC_RADIO_ADDSILENCE_END)) return kSilenceAtEnd;
	else if(IsChecked(IDC_RADIO_RESIZETO)) return kResize;
	else if(IsChecked(IDC_RADIO1)) return kOPLInstrument;
	MPT_ASSERT_NOTREACHED();
	return kSilenceAtEnd;
}


/////////////////////////////////////////////////////////////////////////
// Sample grid dialog

void CSampleGridDlg::DoDataExchange(DataExchange &dx)
{
	DialogBase::DoDataExchange(dx);
	//{{AFX_DATA_MAP(CSampleGridDlg)
	ExchangeControl(dx, IDC_EDIT1, m_EditSegments);
	ExchangeControl(dx, IDC_SPIN1, m_SpinSegments);
	//}}AFX_DATA_MAP
}


CSampleGridDlg::CSampleGridDlg(QWidget* parent, SmpLength nSegments, SmpLength nMaxSegments)
	: DialogBase{IDD_SAMPLE_GRID_SIZE, parent}
	, m_nSegments{nSegments}
	, m_nMaxSegments{nMaxSegments}
{
}


bool CSampleGridDlg::OnInitDialog()
{
	DialogBase::OnInitDialog();
	m_SpinSegments->SetRange(0, m_nMaxSegments);
	m_SpinSegments->SetPos(m_nSegments);
	SetItemInt(IDC_EDIT1, m_nSegments);
	GetDlgItem(IDC_EDIT1)->SetFocus();
	return true;
}


void CSampleGridDlg::OnOK()
{
	m_nSegments = GetItemInt(IDC_EDIT1);
	DialogBase::OnOK();
}


/////////////////////////////////////////////////////////////////////////
// Sample cross-fade dialog

uint32 CSampleXFadeDlg::m_fadeLength  = 20000;
uint32 CSampleXFadeDlg::m_fadeLaw = 50000;
bool CSampleXFadeDlg::m_afterloopFade = true;
bool CSampleXFadeDlg::m_useSustainLoop = false;

void CSampleXFadeDlg::RegisterMessageMap()
{
	DialogBase::RegisterMessageMap();
	AddScroll(Qt::Horizontal, &CSampleXFadeDlg::OnHScroll);
	AddCommand(IDC_RADIO1, &CSampleXFadeDlg::OnLoopTypeChanged);
	AddCommand(IDC_RADIO2, &CSampleXFadeDlg::OnLoopTypeChanged);
	AddNotify(IDC_EDIT1, mfcless::Notify::EditChange, &CSampleXFadeDlg::OnFadeLengthChanged);
}


void CSampleXFadeDlg::DoDataExchange(DataExchange &dx)
{
	DialogBase::DoDataExchange(dx);
	//{{AFX_DATA_MAP(CSampleGridDlg)
	ExchangeControl(dx, IDC_EDIT1, m_EditSamples);
	ExchangeControl(dx, IDC_SPIN1, m_SpinSamples);
	ExchangeControl(dx, IDC_SLIDER1, m_SliderLength);
	ExchangeControl(dx, IDC_SLIDER2, m_SliderFadeLaw);
	ExchangeControl(dx, IDC_RADIO1, m_RadioNormalLoop);
	ExchangeControl(dx, IDC_RADIO2, m_RadioSustainLoop);
	//}}AFX_DATA_MAP
}


CSampleXFadeDlg::CSampleXFadeDlg(QWidget *parent, ModSample &sample)
	: DialogBase{IDD_SAMPLE_XFADE, parent}
	, m_sample{sample}
{
}

bool CSampleXFadeDlg::OnInitDialog()
{
	DialogBase::OnInitDialog();
	const bool hasNormal = (m_sample.uFlags[CHN_LOOP] || !m_sample.uFlags[CHN_SUSTAINLOOP]) && m_sample.nLoopStart > 0;
	const bool hasSustain = m_sample.uFlags[CHN_SUSTAINLOOP] && m_sample.nSustainStart > 0;
	const bool hasBothLoops = hasNormal && hasSustain;
	m_RadioNormalLoop->setEnabled(hasBothLoops);
	m_RadioSustainLoop->setEnabled(hasBothLoops);
	CheckRadio(IDC_RADIO1, IDC_RADIO2, ((m_useSustainLoop && hasSustain) || !hasNormal) ? IDC_RADIO2 : IDC_RADIO1);

	m_SliderLength->setRange(0, 100000);
	m_SliderLength->setValue(m_fadeLength);
	m_SliderFadeLaw->setRange(0, 100000);
	m_SliderFadeLaw->setValue(m_fadeLaw);

	OnLoopTypeChanged();

	return true;
}


void CSampleXFadeDlg::OnOK()
{
	m_fadeLength = m_SliderLength->value();
	m_fadeLaw = m_SliderFadeLaw->value();
	m_afterloopFade = IsChecked(IDC_CHECK1) != 0;
	m_useSustainLoop = IsChecked(IDC_RADIO2) != 0;
	Limit(m_fadeLength, uint32(0), uint32(100000));
	DialogBase::OnOK();
}


void CSampleXFadeDlg::OnLoopTypeChanged()
{
	SmpLength loopStart = m_sample.nLoopStart, loopEnd = m_sample.nLoopEnd;
	if(IsChecked(IDC_RADIO2))
	{
		loopStart = m_sample.nSustainStart;
		loopEnd = m_sample.nSustainEnd;
	}
	m_maxLength = std::min({ m_sample.nLength, loopStart, loopEnd / 2u });
	m_loopLength = loopEnd - loopStart;

	m_editLocked = true;
	m_SpinSamples->SetRange(0, std::min(m_loopLength, m_maxLength));
	GotoDlgCtrl(GetDlgItem(IDC_EDIT1));
	SetChecked(IDC_CHECK1, m_afterloopFade ? 1 : 0);

	SmpLength numSamples = PercentToSamples(m_SliderLength->value());
	numSamples = std::min({ numSamples, m_loopLength, m_maxLength });
	m_SpinSamples->SetPos(numSamples);
	SetItemInt(IDC_EDIT1, numSamples);

	m_editLocked = false;
}


void CSampleXFadeDlg::OnFadeLengthChanged()
{
	if(m_editLocked) return;
	SmpLength numSamples = GetItemInt(IDC_EDIT1);
	numSamples = std::min({ numSamples, m_loopLength, m_maxLength });
	m_SliderLength->setValue(SamplesToPercent(numSamples));
}


void CSampleXFadeDlg::OnHScroll(mfcless::ScrollCode code, int pos, QWidget *sb)
{
	if(sb == (CScrollBar *)(m_SliderLength))
	{
		m_editLocked = true;
		SmpLength numSamples = PercentToSamples(m_SliderLength->value());
		if(numSamples > m_maxLength)
		{
			numSamples = m_maxLength;
			m_SliderLength->setValue(SamplesToPercent(numSamples));
		}
		m_SpinSamples->SetPos(numSamples);
		SetItemInt(IDC_EDIT1, numSamples);
		m_editLocked = false;
	}
}


mpt::ustring CSampleXFadeDlg::GetToolTipText(uint32 id, QWidget *) const
{
	mpt::ustring s;
	switch(id)
	{
	case IDC_SLIDER1:
		{
			uint32 percent = m_SliderLength->value();
			s.Format(U_("%u.%03u%% of the loop (%u samples)"), percent / 1000, percent % 1000, PercentToSamples(percent));
		}
		break;
	case IDC_SLIDER2:
		s = U_("Slide towards constant power for fixing badly looped samples.");
		break;
	}
	
	return s;
}


/////////////////////////////////////////////////////////////////////////
// Resampling dialog

CResamplingDlg::ResamplingOption CResamplingDlg::m_lastChoice = CResamplingDlg::Upsample;
uint32 CResamplingDlg::m_lastFrequency = 0;
bool CResamplingDlg::m_updatePatternCommands = false;
bool CResamplingDlg::m_updatePatternNotes = false;

void CResamplingDlg::RegisterMessageMap()
{
	DialogBase::RegisterMessageMap();
	AddNotify(IDC_EDIT1, mfcless::Notify::SetFocus, &CResamplingDlg::OnFocusEdit);
	AddNotify(IDC_EDIT1, mfcless::Notify::EditChange, &CResamplingDlg::OnEditChanged);
}


CResamplingDlg::CResamplingDlg(QWidget *parent, uint32 frequency, ResamplingMode srcMode, bool resampleAll, bool allowAdjustNotes)
	: DialogBase{IDD_RESAMPLE, parent}
	, m_srcMode{srcMode}
	, m_frequency{frequency}
	, m_resampleAll{resampleAll}
	, m_allowAdjustNotes{allowAdjustNotes}
{
}


bool CResamplingDlg::OnInitDialog()
{
	DialogBase::OnInitDialog();
	SetWindowText(m_resampleAll ? U_("Resample All") : U_("Resample"));

	CheckRadio(IDC_RADIO1, IDC_RADIO3, IDC_RADIO1 + m_lastChoice);
	if(m_frequency > 0)
	{
		mpt::uchar s[32];
		wsprintf(s, U_("&Upsample (%u Hz)"), m_frequency * 2);
		SetItemText(IDC_RADIO1, s);
		wsprintf(s, U_("&Downsample (%u Hz)"), m_frequency / 2);
		SetItemText(IDC_RADIO2, s);

		if(!m_lastFrequency)
			m_lastFrequency = m_frequency;
	}
	if(!m_lastFrequency)
		m_lastFrequency = 48000;


	SetItemInt(IDC_EDIT1, m_lastFrequency);
	CSpinButtonCtrl *spin = static_cast<CSpinButtonCtrl *>(GetDlgItem(IDC_SPIN1));
	spin->SetRange32(1, 999999);
	spin->SetPos32(m_lastFrequency);

	CComboBox *cbnResampling = static_cast<CComboBox *>(GetDlgItem(IDC_COMBO_FILTER));
	cbnResampling->SetRedraw(false);
	const auto resamplingModes = Resampling::AllModesWithDefault();
	for(auto mode : resamplingModes)
	{
		mpt::ustring desc = U_("r8brain (High Quality)");
		if(mode != SRCMODE_DEFAULT)
			desc = CTrackApp::GetResamplingModeName(mode, 1, true);

		int index = cbnResampling->AddString(desc);
		cbnResampling->SetItemData(index, mode);
		if(m_srcMode == mode)
			cbnResampling->SetCurSel(index);
	}
	cbnResampling->SetRedraw(true);

	SetChecked(IDC_CHECK1, m_updatePatternCommands ? 1 : 0);
	SetChecked(IDC_CHECK2, (m_updatePatternNotes && m_allowAdjustNotes) ? 1 : 0);
	EnableItem(IDC_CHECK2, m_allowAdjustNotes ? true : false);

	return true;
}


void CResamplingDlg::OnOK()
{
	const int choice = GetCheckedRadio(IDC_RADIO1, IDC_RADIO3);
	if(choice == IDC_RADIO1)
	{
		m_lastChoice = Upsample;
		m_frequency *= 2;
	} else if(choice == IDC_RADIO2)
	{
		m_lastChoice = Downsample;
		m_frequency /= 2;
	} else
	{
		m_lastChoice = Custom;
		uint32 newFrequency = GetItemInt(IDC_EDIT1);
		if(newFrequency > 0)
		{
			m_lastFrequency = m_frequency = newFrequency;
		} else
		{
			MessageBeep(MB_ICONWARNING);
			GotoDlgCtrl(GetDlgItem(IDC_EDIT1));
			return;
		}
	}

	CComboBox *cbnResampling = static_cast<CComboBox *>(GetDlgItem(IDC_COMBO_FILTER));
	m_srcMode = static_cast<ResamplingMode>(cbnResampling->GetItemData(cbnResampling->GetCurSel()));

	m_updatePatternCommands = IsChecked(IDC_CHECK1) != 0;
	m_updatePatternNotes = IsChecked(IDC_CHECK2) != 0;

	DialogBase::OnOK();
}


void CResamplingDlg::OnFocusEdit()
{
	if(m_lastInputDevice == InputDevice::Mouse)
		CheckRadio(IDC_RADIO1, IDC_RADIO3, IDC_RADIO3);
}


void CResamplingDlg::OnEditChanged()
{
	if(m_lastInputDevice != InputDevice::Unknown)
		CheckRadio(IDC_RADIO1, IDC_RADIO3, IDC_RADIO3);
}


////////////////////////////////////////////////////////////////////////////////////////////
// Sample mix dialog

SmpLength CMixSampleDlg::sampleOffset = 0;
int CMixSampleDlg::amplifyOriginal = 50;
int CMixSampleDlg::amplifyMix = 50;


void CMixSampleDlg::DoDataExchange(DataExchange &dx)
{
	DialogBase::DoDataExchange(dx);
	//{{AFX_DATA_MAP(CMixSampleDlg)
	ExchangeControl(dx, IDC_EDIT_OFFSET, m_EditOffset);
	ExchangeControl(dx, IDC_SPIN_OFFSET, m_SpinOffset);
	ExchangeControl(dx, IDC_SPIN_SAMPVOL1, m_SpinVolOriginal);
	ExchangeControl(dx, IDC_SPIN_SAMPVOL2, m_SpinVolMix);
	//}}AFX_DATA_MAP
}


CMixSampleDlg::CMixSampleDlg(QWidget *parent)
	: DialogBase(IDD_MIXSAMPLES, parent)
{ }


bool CMixSampleDlg::OnInitDialog()
{
	DialogBase::OnInitDialog();

	// Offset
	m_SpinOffset->SetRange(0, MAX_SAMPLE_LENGTH);
	SetItemInt(IDC_EDIT_OFFSET, sampleOffset);

	// Volumes
	m_SpinVolOriginal->SetRange(-10000, 10000);
	m_SpinVolMix->SetRange(-10000, 10000);

	m_EditVolOriginal->SubclassDlgItem(IDC_EDIT_SAMPVOL1, this);
	m_EditVolOriginal->AllowNegative(true);
	m_EditVolMix->SubclassDlgItem(IDC_EDIT_SAMPVOL2, this);
	m_EditVolMix->AllowNegative(true);

	SetItemInt(IDC_EDIT_SAMPVOL1, amplifyOriginal);
	SetItemInt(IDC_EDIT_SAMPVOL2, amplifyMix);

	return true;
}


void CMixSampleDlg::OnOK()
{
	DialogBase::OnOK();
	sampleOffset = Clamp<SmpLength, SmpLength>(GetItemInt(IDC_EDIT_OFFSET), 0, MAX_SAMPLE_LENGTH);
	amplifyOriginal = Clamp<int, int>(GetItemInt(IDC_EDIT_SAMPVOL1), -10000, 10000);
	amplifyMix = Clamp<int, int>(GetItemInt(IDC_EDIT_SAMPVOL2), -10000, 10000);
}

OPENMPT_NAMESPACE_END
