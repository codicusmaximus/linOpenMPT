/*
 * TempoSwingDialog.cpp
 * --------------------
 * Purpose: Implementation of the tempo swing configuration dialog.
 * Notes  : mfcless twin of upstream's mptrack/TempoSwingDialog.cpp.
 *          Hand-edited from the twin.py draft: the rows of sliders are in
 *          a widget inside the container frame, which the scroll bar moves.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "TempoSwingDialog.h"
#include "mfcless/Widgets.h"
#include "HighDPISupport.h"
#include "Mainfrm.h"
#include "resource.h"


OPENMPT_NAMESPACE_BEGIN

void CTempoSwingDlg::RowCtls::SetValue(TempoSwing::value_type v)
{
	int32 val = Util::muldivr(static_cast<int32>(v) - TempoSwing::Unity, CTempoSwingDlg::SliderUnity, TempoSwing::Unity);
	valueSlider->setValue(val);
}


TempoSwing::value_type CTempoSwingDlg::RowCtls::GetValue() const
{
	return Util::muldivr(valueSlider->value(), TempoSwing::Unity, SliderUnity) + TempoSwing::Unity;
}


struct TempoSwingMeasurements
{
	enum
	{
		edRowLabelWidth = 64,                       // Label "Row 999:"
		edSliderWidth = 220,                        // Setting slider
		edSliderHeight = 20,                        // Setting slider
		edValueLabelWidth = 64,                     // Label "100%"
		edPaddingX = 8,                             // Spacing between elements
		edPaddingY = 4,                             // Spacing between elements
		edPaddingTop = 64,                          // Spacing from top of dialog
		edRowHeight = edSliderHeight + edPaddingY,  // Height of one set of controls
		edFooterHeight = 32,                        // Buttons
		edScrollbarWidth = 16,                      // Width of optional scrollbar
	};

	const int rowLabelWidth;
	const int sliderWidth;
	const int sliderHeight;
	const int valueLabelWidth;
	const int paddingX;
	const int paddingY;
	const int paddingTop;
	const int rowHeight;
	const int footerHeight;
	const int scrollbarWidth;

	TempoSwingMeasurements(QWidget * hWnd)
		: rowLabelWidth(HighDPISupport::ScalePixels(edRowLabelWidth, hWnd))
		, sliderWidth(HighDPISupport::ScalePixels(edSliderWidth, hWnd))
		, sliderHeight(HighDPISupport::ScalePixels(edSliderHeight, hWnd))
		, valueLabelWidth(HighDPISupport::ScalePixels(edValueLabelWidth, hWnd))
		, paddingX(HighDPISupport::ScalePixels(edPaddingX, hWnd))
		, paddingY(HighDPISupport::ScalePixels(edPaddingY, hWnd))
		, paddingTop(HighDPISupport::ScalePixels(edPaddingTop, hWnd))
		, rowHeight(HighDPISupport::ScalePixels(edRowHeight, hWnd))
		, footerHeight(HighDPISupport::ScalePixels(edFooterHeight, hWnd))
		, scrollbarWidth(HighDPISupport::ScalePixels(edScrollbarWidth, hWnd))
	{
	}


	mfcless::Rect RowLabelRect(const mfcless::Rect rect) const noexcept
	{
		return mfcless::Rect{rect.left, rect.top, rect.right, rect.top + rowHeight};
	}

	mfcless::Rect ValueLabelRect(const mfcless::Rect rect) const noexcept
	{
		return mfcless::Rect{rect.right - valueLabelWidth, rect.top, rect.right, rect.top + sliderHeight};
	}

	mfcless::Rect ValueSliderRect(const mfcless::Rect rect) const noexcept
	{
		return mfcless::Rect{rect.left + rowLabelWidth, rect.top, rect.right - valueLabelWidth, rect.top + sliderHeight};
	}
};



void CTempoSwingDlg::RegisterMessageMap()
{
	DialogBase::RegisterMessageMap();
	AddCommand(IDC_BUTTON1, &CTempoSwingDlg::OnReset);
	AddCommand(IDC_BUTTON2, &CTempoSwingDlg::OnUseGlobal);
	AddCommand(IDC_CHECK1, &CTempoSwingDlg::OnToggleGroup);
	AddNotify(IDC_EDIT1, mfcless::Notify::EditChange, &CTempoSwingDlg::OnGroupChanged);
}

int CTempoSwingDlg::m_groupSize = 1;

CTempoSwingDlg::CTempoSwingDlg(QWidget *parent, const TempoSwing &currentTempoSwing, CSoundFile &sndFile, PATTERNINDEX pattern)
	: DialogBase(IDD_TEMPO_SWING, parent)
	, m_scrollPos(0)
	, m_tempoSwing(currentTempoSwing)
	, m_origTempoSwing(pattern == PATTERNINDEX_INVALID ? sndFile.m_tempoSwing : sndFile.Patterns[pattern].GetTempoSwing())
	, m_sndFile(sndFile)
	, m_pattern(pattern)
{
	m_groupSize = std::min(m_groupSize, static_cast<int>(m_tempoSwing.size()));
}


void CTempoSwingDlg::DoDataExchange(DataExchange &dx)
{
	DialogBase::DoDataExchange(dx);
	ExchangeControl(dx, IDC_CHECK1, m_checkGroup);
	ExchangeControl(dx, IDC_SCROLLBAR1, m_scrollBar);
	ExchangeControl(dx, IDC_CONTAINER, m_container);
}


bool CTempoSwingDlg::OnInitDialog()
{
	DialogBase::OnInitDialog();

	TempoSwingMeasurements m{this};
	mfcless::Rect rect = GetClientRect();

	const int mainWindowHeight = CMainFrame::GetMainFrame()->height();
	const int frameHeight = frameGeometry().height() - height();

	const int realHeight = static_cast<int>(m_tempoSwing.size()) * m.rowHeight;
	const int displayHeight = std::max(m.rowHeight, std::min(realHeight, mainWindowHeight - frameHeight - m.paddingTop - m.footerHeight));

	const int containerWidth = rect.right - m.scrollbarWidth;
	m_container->setGeometry(0, m.paddingTop, containerWidth, displayHeight);
	m_content = new QWidget(m_container);
	m_content->setGeometry(0, 0, containerWidth, realHeight);
	m_content->show();

	// Need scrollbar?
	if(realHeight > displayHeight)
	{
		m_scrollBar->setRange(0, realHeight - displayHeight);
		m_scrollBar->setPageStep(displayHeight);
		m_scrollBar->setSingleStep(1);
		m_scrollBar->setValue(0);
		m_scrollBar->setGeometry(containerWidth, m.paddingTop, m_scrollBar->width(), displayHeight);
		connect(m_scrollBar, &QScrollBar::valueChanged, this, [this](int pos) {
			m_content->move(0, -pos);
			m_scrollPos = pos;
		});
	} else
	{
		m_scrollBar->setVisible(false);
	}

	rect = mfcless::Rect(0, 0, containerWidth, realHeight);
	rect.DeflateRect(m.paddingX, 0, m.paddingX, 0);

	ShowItem(IDC_BUTTON2, m_pattern != PATTERNINDEX_INVALID);

	m_controls.resize(m_tempoSwing.size());
	for(size_t i = 0; i < m_controls.size(); i++)
	{
		m_controls[i] = std::make_unique<RowCtls>();
		auto &r = m_controls[i];
		// Row label
		r->rowLabel = new QLabel(mfcless::ToQString(MPT_UFORMAT("Row {}:")(i + 1)), m_content);
		r->rowLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
		r->rowLabel->setGeometry(QRect(m.RowLabelRect(rect)));

		// Value label
		r->valueLabel = new QLabel(QStringLiteral("100%"), m_content);
		r->valueLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
		r->valueLabel->setGeometry(QRect(m.ValueLabelRect(rect)));

		// Value slider
		r->valueSlider = new QSlider(Qt::Horizontal, m_content);
		r->valueSlider->setGeometry(QRect(m.ValueSliderRect(rect)));
		r->valueSlider->setRange(-SliderResolution / 2, SliderResolution / 2);
		r->valueSlider->setTickPosition(QSlider::TicksBelow);
		r->valueSlider->setTickInterval(SliderResolution / 8);
		r->valueSlider->setPageStep(SliderResolution / 8);
		r->SetValue(m_tempoSwing[i]);
		QSlider *slider = r->valueSlider;
		connect(slider, &QSlider::valueChanged, this, [this, slider] { if(!m_settingSliders) OnSliderChanged(slider); });
		rect.MoveToY(rect.top + m.rowHeight);
	}

	Item<mfcless::UpDown>(IDC_SPIN1)->SetRange(1, static_cast<int>(m_tempoSwing.size()));
	SetItemInt(IDC_EDIT1, m_groupSize);
	OnToggleGroup();

	OnSliderChanged(nullptr);
	{
		// Buttons at dialog bottom
		const int buttonY = m.paddingTop + displayHeight + m.paddingY;
		for(auto i : { IDOK, IDCANCEL, IDC_BUTTON2 })
		{
			if(auto wnd = GetDlgItem(i))
				wnd->move(wnd->x(), buttonY);
		}
	}

	setFixedSize(width(), displayHeight + m.paddingTop + m.footerHeight);

	return true;
}


void CTempoSwingDlg::OnDPIChanged()
{
	DialogBase::OnDPIChanged();
}


void CTempoSwingDlg::OnOK()
{
	DialogBase::OnOK();
	// If this is the default setup, just clear the vector.
	if(m_pattern == PATTERNINDEX_INVALID)
	{
		if(static_cast<size_t>(std::count(m_tempoSwing.begin(), m_tempoSwing.end(), static_cast<TempoSwing::value_type>(TempoSwing::Unity))) == m_tempoSwing.size())
		{
			m_tempoSwing.clear();
		}
	} else
	{
		if(m_tempoSwing == m_sndFile.m_tempoSwing)
		{
			m_tempoSwing.clear();
		}
	}
	OnClose();
}


void CTempoSwingDlg::OnCancel()
{
	DialogBase::OnCancel();
	OnClose();
}


void CTempoSwingDlg::OnClose()
{
	// Restore original swing properties after preview
	if(m_pattern == PATTERNINDEX_INVALID)
	{
		m_sndFile.m_tempoSwing = m_origTempoSwing;
	} else
	{
		m_sndFile.Patterns[m_pattern].SetTempoSwing(m_origTempoSwing);
	}
}


void CTempoSwingDlg::OnReset()
{
	m_settingSliders = true;
	for(auto &control : m_controls)
	{
		control->valueSlider->setValue(0);
	}
	m_settingSliders = false;
	OnSliderChanged(nullptr);
}


void CTempoSwingDlg::OnUseGlobal()
{
	if(m_sndFile.m_tempoSwing.empty())
	{
		OnReset();
		return;
	}
	m_settingSliders = true;
	for(size_t i = 0; i < m_controls.size(); i++)
	{
		m_controls[i]->SetValue(m_sndFile.m_tempoSwing[i % m_sndFile.m_tempoSwing.size()]);
	}
	m_settingSliders = false;
	OnSliderChanged(nullptr);
}


void CTempoSwingDlg::OnToggleGroup()
{
	const bool checked = m_checkGroup->isChecked() != 0;
	EnableItem(IDC_EDIT1, checked);
	EnableItem(IDC_SPIN1, checked);
}


void CTempoSwingDlg::OnGroupChanged()
{
	int val = GetItemInt(IDC_EDIT1);
	if(val > 0) m_groupSize = std::min(val, static_cast<int>(m_tempoSwing.size()));
}


// Upstream's SliderContainer::OnHScroll (the sliders' parent).
void CTempoSwingDlg::OnSliderChanged(QSlider *slider)
{
	if(slider != nullptr && m_checkGroup->isChecked())
	{
		// Edit groups
		size_t editedGroup = 0;
		const int editedValue = slider->value();
		for(size_t i = 0; i < m_controls.size(); i++)
		{
			if(m_controls[i]->valueSlider == slider)
			{
				editedGroup = (i / m_groupSize) % 2u;
				break;
			}
		}
		m_settingSliders = true;
		for(size_t i = 0; i < m_controls.size(); i++)
		{
			if((i / m_groupSize) % 2u == editedGroup)
			{
				m_controls[i]->valueSlider->setValue(editedValue);
			}
		}
		m_settingSliders = false;
	}

	for(size_t i = 0; i < m_controls.size(); i++)
	{
		m_tempoSwing[i] = m_controls[i]->GetValue();
	}
	m_tempoSwing.Normalize();
	// Apply preview
	if(m_pattern == PATTERNINDEX_INVALID)
	{
		m_sndFile.m_tempoSwing = m_tempoSwing;
	} else
	{
		m_sndFile.Patterns[m_pattern].SetTempoSwing(m_tempoSwing);
	}

	for(size_t i = 0; i < m_tempoSwing.size(); i++)
	{
		const int32 percent = Util::muldivr(m_tempoSwing[i], 100, TempoSwing::Unity);
		m_controls[i]->valueLabel->setText(QStringLiteral("%1%").arg(percent));
		// The slider's tooltip (TTN_NEEDTEXT)
		const int32 val = percent - 100;
		m_controls[i]->valueSlider->setToolTip(QStringLiteral("%1%2").arg(val > 0 ? QStringLiteral("+") : QString()).arg(val));
	}
}

OPENMPT_NAMESPACE_END
