/*
 * OPLInstrDlg.cpp
 * ---------------
 * Purpose: Editor for OPL-based synth instruments
 * Notes  : mfcless twin of upstream's mptrack/OPLInstrDlg.cpp.
 *          Hand-edited from the twin.py draft.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */

#include "OPLInstrDlg.h"
#include "mfcless/Widgets.h"
#include "resource.h"
#include "UpdateHints.h"
#include "WindowMessages.h"
#include "../soundlib/OPL.h"
#include "../soundlib/Sndfile.h"

OPENMPT_NAMESPACE_BEGIN

void OPLInstrDlg::RegisterMessageMap()
{
	DialogBase::RegisterMessageMap();
	AddScroll(Qt::Horizontal, &OPLInstrDlg::OnHScroll);
	AddMessage(WM_MOD_DRAGONDROPPING, &OPLInstrDlg::OnDragonDropping);
	AddCommand(IDC_CHECK1, &OPLInstrDlg::ParamsChanged);
	AddCommand(IDC_CHECK2, &OPLInstrDlg::ParamsChanged);
	AddCommand(IDC_CHECK3, &OPLInstrDlg::ParamsChanged);
	AddCommand(IDC_CHECK4, &OPLInstrDlg::ParamsChanged);
	AddCommand(IDC_CHECK5, &OPLInstrDlg::ParamsChanged);
	AddCommand(IDC_CHECK6, &OPLInstrDlg::ParamsChanged);
	AddCommand(IDC_CHECK7, &OPLInstrDlg::ParamsChanged);
	AddCommand(IDC_CHECK8, &OPLInstrDlg::ParamsChanged);
	AddCommand(IDC_CHECK9, &OPLInstrDlg::ParamsChanged);
	AddNotify(IDC_COMBO1, mfcless::Notify::SelChange, &OPLInstrDlg::ParamsChanged);
	AddNotify(IDC_COMBO2, mfcless::Notify::SelChange, &OPLInstrDlg::ParamsChanged);
}


void OPLInstrDlg::DoDataExchange(DataExchange &dx)
{
	DialogBase::DoDataExchange(dx);
	ExchangeControl(dx, IDC_CHECK1, m_additive);
	ExchangeControl(dx, IDC_SLIDER1, m_feedback);

	for(int op = 0; op < 2; op++)
	{
		const int slider = op * 7;
		const int check = op * 4;
		ExchangeControl(dx, IDC_SLIDER2 + slider, m_attackRate[op]);
		ExchangeControl(dx, IDC_SLIDER3 + slider, m_decayRate[op]);
		ExchangeControl(dx, IDC_SLIDER4 + slider, m_sustainLevel[op]);
		ExchangeControl(dx, IDC_SLIDER5 + slider, m_releaseRate[op]);
		ExchangeControl(dx, IDC_CHECK2  + check, m_sustain[op]);
		ExchangeControl(dx, IDC_SLIDER6 + slider, m_volume[op]);
		ExchangeControl(dx, IDC_CHECK3  + check, m_scaleEnv[op]);
		ExchangeControl(dx, IDC_SLIDER7 + slider, m_levelScaling[op]);
		ExchangeControl(dx, IDC_SLIDER8 + slider, m_freqMultiplier[op]);
		ExchangeControl(dx, IDC_COMBO1  + op, m_waveform[op]);
		ExchangeControl(dx, IDC_CHECK4  + check, m_vibrato[op]);
		ExchangeControl(dx, IDC_CHECK5  + check, m_tremolo[op]);
	}
}


OPLInstrDlg::OPLInstrDlg(QWidget &parent, const CSoundFile &sndFile)
	: m_parent(parent)
	, m_sndFile(sndFile)
{
	Create(IDD_OPL_PARAMS, &parent);
	const mfcless::Rect rect = GetClientRect();
	m_windowSize = mfcless::Size(rect.Width(), rect.Height());
}


OPLInstrDlg::~OPLInstrDlg()
{
	DestroyWindow();
}


bool OPLInstrDlg::OnInitDialog()
{
	DialogBase::OnInitDialog();
	m_feedback->setRange(0, 7);
	for(int op = 0; op < 2; op++)
	{
		m_attackRate[op]->setRange(0, 15);
		m_decayRate[op]->setRange(0, 15);
		m_sustainLevel[op]->setRange(0, 15);
		m_releaseRate[op]->setRange(0, 15);
		m_volume[op]->setRange(0, 63);
		m_volume[op]->setTickInterval(4);
		m_levelScaling[op]->setRange(0, 3);
		m_freqMultiplier[op]->setRange(0, 15);
	}

	return true;
}


bool OPLInstrDlg::PreTranslateMessage(mfcless::Message *pMsg)
{
	if(pMsg)
	{
		// Forward key presses and drag&drop support to parent editor: the
		// message filter passes them on to the parent's PreTranslateMessage
		// next (the dialog is its child).
		if(pMsg->message == mfcless::msg::SysKeyUp || pMsg->message == mfcless::msg::KeyUp ||
			pMsg->message == mfcless::msg::SysKeyDown || pMsg->message == mfcless::msg::KeyDown ||
			pMsg->message == mfcless::msg::XButtonUp)
		{
			return false;
		}
	}
	return DialogBase::PreTranslateMessage(pMsg);
}


intptr_t OPLInstrDlg::OnDragonDropping(uintptr_t wParam, intptr_t lParam)
{
	return mfcless::SendModMessage(&m_parent, WM_MOD_DRAGONDROPPING, wParam, lParam);
}


// Swap OPL Key Scale Level bits for a "human-readable" value.
static uint8 KeyScaleLevel(uint8 kslVolume)
{
	static constexpr uint8 KSLFix[4] = { 0x00, 0x80, 0x40, 0xC0 };
	return KSLFix[kslVolume >> 6];
}


void OPLInstrDlg::SetEnabled(bool enabled)
{
	if(!enabled)
		m_lastFocusItem = mfcless::GetFocus();

	for(QWidget *child : findChildren<QWidget *>())
		child->setEnabled(enabled);
	setVisible(enabled);

	if(enabled && !isAncestorOf(mfcless::GetFocus()))
	{
		if(m_lastFocusItem && isAncestorOf(m_lastFocusItem))
			m_lastFocusItem->setFocus();
		else
			focusNextChild();
	}
}


void OPLInstrDlg::SetPatch(OPLPatch &patch)
{
	SetRedraw(false);

	m_additive->setChecked((patch[10] & OPL::CONNECTION_BIT) ? 1 : 0);
	m_feedback->setValue((patch[10] & OPL::FEEDBACK_MASK) >> 1);
	for(int op = 0; op < 2; op++)
	{
		m_attackRate[op]->setValue(15 - (patch[4 + op] >> 4));
		m_decayRate[op]->setValue(15 - (patch[4 + op] & 0x0F));
		m_sustainLevel[op]->setValue(15 - (patch[6 + op] >> 4));
		m_releaseRate[op]->setValue(15 - (patch[6 + op] & 0x0F));
		m_volume[op]->setValue(63 - (patch[2 + op] & OPL::TOTAL_LEVEL_MASK));
		m_levelScaling[op]->setValue(KeyScaleLevel(patch[2 + op]) >> 6);
		m_freqMultiplier[op]->setValue(patch[0 + op] & OPL::MULTIPLE_MASK);

		m_sustain[op]->setChecked((patch[0 + op] & OPL::SUSTAIN_ON) ? 1 : 0);
		m_scaleEnv[op]->setChecked((patch[0 + op] & OPL::KSR) ? 1 : 0);
		m_vibrato[op]->setChecked((patch[0 + op] & OPL::VIBRATO_ON) ? 1 : 0);
		m_tremolo[op]->setChecked((patch[0 + op] & OPL::TREMOLO_ON) ? 1 : 0);

		const auto waveform = patch[8 + op];
		const int numWaveforms = (m_sndFile.GetType() == MOD_TYPE_S3M && waveform < 4) ? 4 : 8;
		if(numWaveforms != m_waveform[op]->count())
		{
			m_waveform[op]->clear();
			static constexpr const mpt::uchar *waveformNames[] =
			{
				UL_("Sine"), UL_("Half Sine"), UL_("Absolute Sine"), UL_("Pulse Sine"),
				UL_("Sine (Even Periods)"), UL_("Absolute Sine (Even Periods)"), UL_("Square"), UL_("Derived Square")
			};
			for(int i = 0; i < numWaveforms; i++)
			{
				mfcless::AddItem(m_waveform[op], waveformNames[i]);
			}
		}
		m_waveform[op]->setCurrentIndex(waveform);
	}
	SetRedraw(true);
	m_patch = &patch;
}


void OPLInstrDlg::ParamsChanged()
{
	OPLPatch patch{{}};
	if(m_additive->isChecked() != 0) patch[10] |= OPL::CONNECTION_BIT;
	patch[10] |= static_cast<uint8>(m_feedback->value() << 1);
	for(int op = 0; op < 2; op++)
	{
		patch[op] = static_cast<uint8>(m_freqMultiplier[op]->value());
		if(m_sustain[op]->isChecked() != 0) patch[op] |= OPL::SUSTAIN_ON;
		if(m_scaleEnv[op]->isChecked() != 0) patch[op] |= OPL::KSR;
		if(m_vibrato[op]->isChecked() != 0) patch[op] |= OPL::VIBRATO_ON;
		if(m_tremolo[op]->isChecked() != 0) patch[op] |= OPL::TREMOLO_ON;
		patch[2 + op] = static_cast<uint8>((63 - m_volume[op]->value()) | KeyScaleLevel(static_cast<uint8>(m_levelScaling[op]->value() << 6)));
		patch[4 + op] = static_cast<uint8>(((15 - m_attackRate[op]->value()) << 4) | (15 - m_decayRate[op]->value()));
		patch[6 + op] = static_cast<uint8>(((15 - m_sustainLevel[op]->value()) << 4) | (15 - m_releaseRate[op]->value()));
		patch[8 + op] = static_cast<uint8>(m_waveform[op]->currentIndex());
	}

	if(*m_patch != patch)
	{
		mfcless::SendModMessage(&m_parent, WM_MOD_VIEWMSG, VIEWMSG_PREPAREUNDO);
		*m_patch = patch;
		mfcless::SendModMessage(&m_parent, WM_MOD_VIEWMSG, VIEWMSG_SETMODIFIED, SampleHint().Data().AsLPARAM());
	}
}


mpt::ustring OPLInstrDlg::GetToolTipText(uint32 id, QWidget *) const
{
	static constexpr const char *feedback[] = {"disabled", "\xCF\x80/16", "\xCF\x80/8", "\xCF\x80/4", "\xCF\x80/2", "\xCF\x80", "2\xCF\x80", "4\xCF\x80"};
	static constexpr const mpt::uchar *ksl[] = {UL_("disabled"), UL_("1.5 dB / octave"), UL_("3 dB / octave"), UL_("6 dB / octave")};

	mpt::ustring text;
	const QWidget *wnd = GetDlgItem(static_cast<int>(id));
	const QSlider *slider = static_cast<const QSlider *>(wnd);
	switch(id)
	{
	case IDC_SLIDER1:
		// Feedback
		text = mpt::ToUnicode(mpt::Charset::UTF8, feedback[slider->value() & 7]);
		break;

	case IDC_SLIDER2:
	case IDC_SLIDER3:
	case IDC_SLIDER5:
	case IDC_SLIDER9:
	case IDC_SLIDER10:
	case IDC_SLIDER12:
		// Attack / Decay / Release
		text = UL_("faster < ") + mpt::ufmt::val(slider->value()) + UL_(" > slower");
		break;
	case IDC_SLIDER4:
	case IDC_SLIDER11:
		// Sustain Level
		{
			const int pos = slider->value();
			text = mpt::ufmt::val((pos == 0) ? -93 : ((-15 + pos) * 3)) + UL_(" dB");
		}
		break;
	case IDC_SLIDER6:
	case IDC_SLIDER13:
		// Volume Level
		text = mpt::ufmt::fix((-63 + slider->value()) * 0.75, 2) + UL_(" dB");
		break;
	case IDC_SLIDER7:
	case IDC_SLIDER14:
		// Key Scale Level
		text = ksl[slider->value() & 3];
		break;
	case IDC_SLIDER8:
	case IDC_SLIDER15:
		// Frequency Multiplier
		if(slider->value() == 0)
			text = UL_("0.5");
		else
			text = mpt::ufmt::val(slider->value());
		break;
	}

	return text;
}

OPENMPT_NAMESPACE_END
