/*
 * SampleConfigDlg.cpp
 * -------------------
 * Purpose: Implementation of the sample/instrument editor settings dialog.
 * Notes  : mfcless twin of upstream's mptrack/SampleConfigDlg.cpp.
 *          Hand-edited from the twin.py draft.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "SampleConfigDlg.h"
#include "mfcless/Widgets.h"
#include "Mainfrm.h"
#include "Moddoc.h"
#include "Mptrack.h"
#include "resource.h"
#include "TrackerSettings.h"


OPENMPT_NAMESPACE_BEGIN

void COptionsSampleEditor::RegisterMessageMap()
{
	PropertyPage::RegisterMessageMap();
	AddScroll(Qt::Horizontal, &COptionsSampleEditor::OnHScroll);
	AddNotify(IDC_EDIT_UNDOSIZE, mfcless::Notify::EditChange, &COptionsSampleEditor::OnUndoSizeChanged);
	AddNotify(IDC_EDIT_FINETUNE, mfcless::Notify::EditChange, &COptionsSampleEditor::OnSettingsChanged);
	AddNotify(IDC_FLAC_COMPRESSION, mfcless::Notify::EditChange, &COptionsSampleEditor::OnSettingsChanged);
	AddNotify(IDC_DEFAULT_FORMAT, mfcless::Notify::SelChange, &COptionsSampleEditor::OnSettingsChanged);
	AddNotify(IDC_VOLUME_HANDLING, mfcless::Notify::SelChange, &COptionsSampleEditor::OnSettingsChanged);
	AddCommand(IDC_RADIO1, &COptionsSampleEditor::OnSettingsChanged);
	AddCommand(IDC_RADIO2, &COptionsSampleEditor::OnSettingsChanged);
	AddCommand(IDC_RADIO3, &COptionsSampleEditor::OnSettingsChanged);
	AddCommand(IDC_RADIO4, &COptionsSampleEditor::OnSettingsChanged);
	AddCommand(IDC_RADIO5, &COptionsSampleEditor::OnSettingsChanged);
	AddCommand(IDC_RADIO6, &COptionsSampleEditor::OnSettingsChanged);
	AddCommand(IDC_COMPRESS_ITI, &COptionsSampleEditor::OnSettingsChanged);
	AddCommand(IDC_PREVIEW_SAMPLES, &COptionsSampleEditor::OnSettingsChanged);
	AddCommand(IDC_NORMALIZE, &COptionsSampleEditor::OnSettingsChanged);
	AddCommand(IDC_CURSORINHEX, &COptionsSampleEditor::OnSettingsChanged);
}


void COptionsSampleEditor::DoDataExchange(DataExchange &dx)
{
	PropertyPage::DoDataExchange(dx);
	//{{AFX_DATA_MAP(COptionsSampleEditor)
	ExchangeControl(dx, IDC_DEFAULT_FORMAT, m_cbnDefaultSampleFormat);
	ExchangeControl(dx, IDC_VOLUME_HANDLING, m_cbnDefaultVolumeHandling);
	ExchangeControl(dx, IDC_EDIT_UNDOSIZE, m_undoBufferEdit);
	ExchangeControl(dx, IDC_EDIT_FINETUNE, m_finetuneEdit);
	//}}AFX_DATA_MAP
}


COptionsSampleEditor::COptionsSampleEditor() : PropertyPage{IDD_OPTIONS_SAMPLEEDITOR}
{
	m_undoBufferEdit->SetAccessibleSuffix(UL_("%"));
	m_finetuneEdit->SetAccessibleSuffix(UL_("cents"));
}


bool COptionsSampleEditor::OnInitDialog()
{
	PropertyPage::OnInitDialog();
	SetItemInt(IDC_EDIT_UNDOSIZE, TrackerSettings::Instance().m_SampleUndoBufferSize.Get().GetSizeInPercent());
	SetItemInt(IDC_EDIT_FINETUNE, TrackerSettings::Instance().m_nFinetuneStep);
	Item<mfcless::UpDown>(IDC_SPIN1)->SetRange(0, 100);
	Item<mfcless::UpDown>(IDC_SPIN2)->SetRange(1, 200);
	RecalcUndoSize();

	mfcless::SetItemData(m_cbnDefaultSampleFormat, mfcless::AddItem(m_cbnDefaultSampleFormat, UL_("FLAC")), dfFLAC);
	mfcless::SetItemData(m_cbnDefaultSampleFormat, mfcless::AddItem(m_cbnDefaultSampleFormat, UL_("WAV")), dfWAV);
	mfcless::SetItemData(m_cbnDefaultSampleFormat, mfcless::AddItem(m_cbnDefaultSampleFormat, UL_("RAW")), dfRAW);
	mfcless::SetItemData(m_cbnDefaultSampleFormat, mfcless::AddItem(m_cbnDefaultSampleFormat, UL_("S3I")), dfS3I);
	mfcless::SetItemData(m_cbnDefaultSampleFormat, mfcless::AddItem(m_cbnDefaultSampleFormat, UL_("IFF")), dfIFF);
	m_cbnDefaultSampleFormat->setCurrentIndex(TrackerSettings::Instance().m_defaultSampleFormat);

	QSlider *slider = Item<QSlider>(IDC_SLIDER1);
	slider->setRange(0, 8);
	slider->setTickInterval(1);
	slider->setValue(TrackerSettings::Instance().m_FLACCompressionLevel);

	CheckRadio(IDC_RADIO1, IDC_RADIO3, IDC_RADIO1 + TrackerSettings::Instance().sampleEditorKeyBehaviour);
	CheckRadio(IDC_RADIO4, IDC_RADIO6, IDC_RADIO4 + static_cast<int>(TrackerSettings::Instance().m_followSamplePlayCursor.Get()));

	SetChecked(IDC_COMPRESS_ITI, TrackerSettings::Instance().compressITI ? 1 : 0);

	mfcless::SetItemData(m_cbnDefaultVolumeHandling, mfcless::AddItem(m_cbnDefaultVolumeHandling, UL_("MIDI volume")), PLUGIN_VOLUMEHANDLING_MIDI);
	mfcless::SetItemData(m_cbnDefaultVolumeHandling, mfcless::AddItem(m_cbnDefaultVolumeHandling, UL_("Dry/Wet ratio")), PLUGIN_VOLUMEHANDLING_DRYWET);
	mfcless::SetItemData(m_cbnDefaultVolumeHandling, mfcless::AddItem(m_cbnDefaultVolumeHandling, UL_("None")), PLUGIN_VOLUMEHANDLING_IGNORE);
	m_cbnDefaultVolumeHandling->setCurrentIndex(TrackerSettings::Instance().DefaultPlugVolumeHandling);

	SetChecked(IDC_PREVIEW_SAMPLES, TrackerSettings::Instance().previewInFileDialogs ? 1 : 0);
	SetChecked(IDC_NORMALIZE, TrackerSettings::Instance().m_MayNormalizeSamplesOnLoad ? 1 : 0);
	SetChecked(IDC_CURSORINHEX, TrackerSettings::Instance().cursorPositionInHex ? 1 : 0);

	return true;
}


void COptionsSampleEditor::OnOK()
{
	PropertyPage::OnOK();

	TrackerSettings::Instance().m_nFinetuneStep = GetItemInt(IDC_EDIT_FINETUNE);
	TrackerSettings::Instance().m_SampleUndoBufferSize = SampleUndoBufferSize(GetItemInt(IDC_EDIT_UNDOSIZE));
	TrackerSettings::Instance().m_defaultSampleFormat = static_cast<SampleEditorDefaultFormat>(mfcless::GetItemData(m_cbnDefaultSampleFormat, m_cbnDefaultSampleFormat->currentIndex()));
	TrackerSettings::Instance().m_followSamplePlayCursor = static_cast<FollowSamplePlayCursor>(GetCheckedRadio(IDC_RADIO4, IDC_RADIO6) - IDC_RADIO4);
	TrackerSettings::Instance().m_FLACCompressionLevel = Item<QSlider>(IDC_SLIDER1)->value();
	TrackerSettings::Instance().sampleEditorKeyBehaviour = static_cast<SampleEditorKeyBehaviour>(GetCheckedRadio(IDC_RADIO1, IDC_RADIO3) - IDC_RADIO1);
	TrackerSettings::Instance().compressITI = IsChecked(IDC_COMPRESS_ITI) != 0;
	TrackerSettings::Instance().DefaultPlugVolumeHandling = static_cast<PlugVolumeHandling>(mfcless::GetItemData(m_cbnDefaultVolumeHandling, m_cbnDefaultVolumeHandling->currentIndex()));
	TrackerSettings::Instance().previewInFileDialogs = IsChecked(IDC_PREVIEW_SAMPLES) != 0;
	TrackerSettings::Instance().m_MayNormalizeSamplesOnLoad = IsChecked(IDC_NORMALIZE) != 0;
	TrackerSettings::Instance().cursorPositionInHex = IsChecked(IDC_CURSORINHEX) != 0;

	for(auto modDoc : theApp.GetOpenDocuments())
	{
		modDoc->GetSampleUndo().RestrictBufferSize();
	}
}


bool COptionsSampleEditor::OnSetActive()
{
	CMainFrame::m_nLastOptionsPage = OPTIONS_PAGE_SAMPLEDITOR;
	return PropertyPage::OnSetActive();
}


void COptionsSampleEditor::OnUndoSizeChanged()
{
	RecalcUndoSize();
	OnSettingsChanged();
}


void COptionsSampleEditor::RecalcUndoSize()
{
	uint32 sizePercent = GetItemInt(IDC_EDIT_UNDOSIZE);
	uint32 sizeMB = mpt::saturate_cast<uint32>(SampleUndoBufferSize(sizePercent).GetSizeInBytes() >> 20);
	mpt::ustring text = UL_("% of physical memory (");
	if(sizePercent)
		text += mfcless::PrintF(UL_("%u MiB)"), sizeMB);
	else
		text.append(UL_("disabled)"));
	SetItemText(IDC_UNDOSIZE, text);
}

OPENMPT_NAMESPACE_END
