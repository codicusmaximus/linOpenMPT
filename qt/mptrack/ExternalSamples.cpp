/*
 * ExternalSamples.cpp
 * -------------------
 * Purpose: Dialogs for locating missing external samples and handling modified samples
 * Notes  : mfcless twin of upstream's mptrack/ExternalSamples.cpp.
 *          Draft from qt/tools/twin.py, not yet edited by hand.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "ExternalSamples.h"
#include "FileDialog.h"
#include "FolderScanner.h"
#include "Moddoc.h"
#include "MPTrackUtil.h"
#include "Reporting.h"
#include "resource.h"
#include "TrackerSettings.h"
#include "UpdateHints.h"
#include "mpt/fs/fs.hpp"

OPENMPT_NAMESPACE_BEGIN

void MissingExternalSamplesDlg::RegisterMessageMap()
{
	ResizableDialog::RegisterMessageMap();
	AddNotify(IDC_LIST1, mfcless::Notify::DoubleClicked, &MissingExternalSamplesDlg::OnSetPath);
	AddCommand(IDC_BUTTON1, &MissingExternalSamplesDlg::OnScanFolder);
}


void MissingExternalSamplesDlg::DoDataExchange(DataExchange &dx)
{
	ResizableDialog::DoDataExchange(dx);
	ExchangeControl(dx, IDC_LIST1, m_List);
}


MissingExternalSamplesDlg::MissingExternalSamplesDlg(CModDoc &modDoc, QWidget *parent)
    : ResizableDialog(IDD_MISSINGSAMPLES, parent)
    , m_modDoc(modDoc)
    , m_sndFile(modDoc.GetSoundFile())
{
}


bool MissingExternalSamplesDlg::OnInitDialog()
{
	ResizableDialog::OnInitDialog();

	// Initialize table
	const CListCtrlEx::Header headers[] =
	{
		{ U_("Sample"),            128, LVCFMT_LEFT },
		{ U_("External Filename"), 308, LVCFMT_LEFT },
	};
	m_List->SetHeaders(headers);
	m_List->SetExtendedStyle(m_List->GetExtendedStyle() | LVS_EX_FULLROWSELECT);

	GenerateList();
	SetWindowText((U_("Missing External Samples - ") + m_modDoc.GetPathNameMpt().GetFilename().ToWide()).c_str());

	return true;
}


void MissingExternalSamplesDlg::GenerateList()
{
	m_List->setUpdatesEnabled(false);
	m_List->DeleteAllItems();
	mpt::ustring s;
	for(SAMPLEINDEX smp = 1; smp <= m_sndFile.GetNumSamples(); smp++)
	{
		if(m_sndFile.IsExternalSampleMissing(smp))
		{
			s.Format(U_("%02u: "), smp);
			s += mpt::ToUnicode(m_sndFile.GetCharsetInternal(), m_sndFile.GetSampleName(smp));
			int insertAt = m_List->InsertItem(m_List->GetItemCount(), s);
			if(insertAt == -1)
				continue;
			m_List->SetItemText(insertAt, 1, m_sndFile.GetSamplePath(smp).ToCString());
			m_List->SetItemData(insertAt, smp);
		}
	}
	m_List->setUpdatesEnabled(true);

	// Yay, we managed to find all samples!
	if(!m_List->GetItemCount())
		OnOK();
}


void MissingExternalSamplesDlg::OnSetPath()
{
	const int item = m_List->GetSelectionMark();
	if(item == -1) return;
	const SAMPLEINDEX smp = static_cast<SAMPLEINDEX>(m_List->GetItemData(item));

	const mpt::PathString path = m_modDoc.GetSoundFile().GetSamplePath(smp);
	FileDialog dlg = OpenFileDialog()
		.ExtensionFilter(U_("All Samples|*.wav;*.flac|All files(*.*)|*.*||"));  // Only show samples that we actually can save as well.
	if(TrackerSettings::Instance().previewInFileDialogs)
		dlg.EnableAudioPreview();
	if(path.empty())
		dlg.WorkingDirectory(TrackerSettings::Instance().PathSamples.GetWorkingDir());
	else
		dlg.DefaultFilename(path);
	if(!dlg.Show()) return;
	TrackerSettings::Instance().PathSamples.SetWorkingDir(dlg.GetWorkingDirectory());

	SetSample(smp, dlg.GetFirstFile());
	m_modDoc.UpdateAllViews(nullptr, SampleHint(smp).Info().Names().Data());
	GenerateList();
}


void MissingExternalSamplesDlg::OnScanFolder()
{
	if(m_isScanning)
	{
		m_isScanning = false;
		return;
	}

	BrowseForFolder dlg(TrackerSettings::Instance().PathSamples.GetWorkingDir(), U_("Select a folder to search for missing samples..."));
	dlg.AddPlace(m_modDoc.GetPathNameMpt().GetDirectoryWithDrive());
	if(dlg.Show())
	{
		TrackerSettings::Instance().PathSamples.SetWorkingDir(dlg.GetDirectory());

		FolderScanner scan(dlg.GetDirectory(), FolderScanner::kOnlyFiles | FolderScanner::kFindInSubDirectories);
		mpt::PathString fileName;

		m_isScanning = true;
		SetItemText(IDC_BUTTON1, U_("&Cancel"));
		EnableItem(IDOK, false);
		BeginWaitCursor();

		DWORD64 lastTick = Util::GetTickCount64();
		int foundFiles = 0;

		bool anyMissing = true;
		while(scan.Next(fileName) && m_isScanning && anyMissing)
		{
			anyMissing = false;
			for(SAMPLEINDEX smp = 1; smp <= m_sndFile.GetNumSamples(); smp++)
			{
				if(m_sndFile.IsExternalSampleMissing(smp))
				{
					if(!mpt::PathCompareNoCase(m_sndFile.GetSamplePath(smp).GetFilename(), fileName.GetFilename()))
					{
						if(SetSample(smp, fileName))
						{
							foundFiles++;
						}
					} else
					{
						anyMissing = true;
					}
				}
			}

			const DWORD64 tick = Util::GetTickCount64();
			if(tick < lastTick || tick > lastTick + 100)
			{
				lastTick = tick;
				SetItemText(IDC_STATIC1, fileName.ToCString());
				MSG msg;
				while(::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
				{
					::TranslateMessage(&msg);
					::DispatchMessage(&msg);
				}
			}
		}
		EndWaitCursor();
		EnableItem(IDOK, true);
		SetItemText(IDC_BUTTON1, U_("&Scan Folder..."));

		m_modDoc.UpdateAllViews(nullptr, SampleHint().Info().Data().Names());

		if(foundFiles)
		{
			SetItemText(IDC_STATIC1, MPT_UFORMAT("{} sample paths were relocated.")(foundFiles));
		} else
		{
			SetItemText(IDC_STATIC1, U_("No matching sample names found."));
		}
		m_isScanning = false;
		GenerateList();
	}

}


bool MissingExternalSamplesDlg::SetSample(SAMPLEINDEX smp, const mpt::PathString &fileName)
{
	m_modDoc.GetSampleUndo().PrepareUndo(smp, sundo_replace, "Replace");
	const mpt::PathString oldPath = m_sndFile.GetSamplePath(smp);
	if(!m_sndFile.LoadExternalSample(smp, fileName))
	{
		Reporting::Information(U_("Unable to load sample:\n") + fileName.ToWide());
		m_modDoc.GetSampleUndo().RemoveLastUndoStep(smp);
		return false;
	} else
	{
		// Maybe we just put the file into its regular place, in which case the module has not really been modified.
		if(oldPath != fileName)
		{
			m_modDoc.SetModified();
		}
		return true;
	}
}


void ModifiedExternalSamplesDlg::RegisterMessageMap()
{
	ResizableDialog::RegisterMessageMap();
	AddCommand(IDC_SAVE, &ModifiedExternalSamplesDlg::OnSaveSelected);
	AddCommand(IDC_CHECK1, &ModifiedExternalSamplesDlg::OnCheckAll);
	AddNotify(IDC_LIST1, mfcless::Notify::ItemChanged, &ModifiedExternalSamplesDlg::OnSelectionChanged);
}


void ModifiedExternalSamplesDlg::DoDataExchange(DataExchange &dx)
{
	ResizableDialog::DoDataExchange(dx);
	ExchangeControl(dx, IDC_LIST1, m_List);
}


ModifiedExternalSamplesDlg::ModifiedExternalSamplesDlg(CModDoc &modDoc, QWidget *parent)
    : ResizableDialog(IDD_MODIFIEDSAMPLES, parent)
    , m_modDoc(modDoc)
    , m_sndFile(modDoc.GetSoundFile())
{
}


bool ModifiedExternalSamplesDlg::OnInitDialog()
{
	ResizableDialog::OnInitDialog();

	// Initialize table
	const CListCtrlEx::Header headers[] =
	{
		{U_("Sample"),            120, LVCFMT_LEFT},
		{U_("Status"),            54,  LVCFMT_LEFT},
		{U_("External Filename"), 262, LVCFMT_LEFT},
	};
	m_List->SetHeaders(headers);
	m_List->SetExtendedStyle(m_List->GetExtendedStyle() | LVS_EX_FULLROWSELECT | LVS_EX_CHECKBOXES);

	GenerateList();
	SetWindowText((U_("Modified External Samples - ") + m_modDoc.GetPathNameMpt().GetFilename().ToWide()).c_str());

	return true;
}


void ModifiedExternalSamplesDlg::GenerateList()
{
	m_List->setUpdatesEnabled(false);
	m_List->DeleteAllItems();
	mpt::ustring s;
	mpt::ustring status;
	for(SAMPLEINDEX smp = 1; smp <= m_sndFile.GetNumSamples(); smp++)
	{
		if(!m_sndFile.GetSample(smp).uFlags[SMP_KEEPONDISK])
			continue;

		if(m_sndFile.GetSample(smp).uFlags[SMP_MODIFIED])
			status = U_("modified");
		else if(!m_sndFile.GetSample(smp).HasSampleData())
			continue;  // Sample was already missing when the file was loaded, nothing we can do here
		else if(!mpt::native_fs{}.is_file(m_sndFile.GetSamplePath(smp)))
			status = U_("missing");
		else
			continue;

		s.Format(U_("%02u: "), smp);
		s += mpt::ToUnicode(m_sndFile.GetCharsetInternal(), m_sndFile.GetSampleName(smp));
		int insertAt = m_List->InsertItem(m_List->GetItemCount(), s);
		if(insertAt == -1)
			continue;
		m_List->SetItemText(insertAt, 1, status.c_str());
		m_List->SetItemText(insertAt, 2, m_sndFile.GetSamplePath(smp).ToCString());
		m_List->SetCheck(insertAt, true);
		m_List->SetItemData(insertAt, smp);
	}
	m_List->setUpdatesEnabled(true);

	SetChecked(IDC_CHECK1, 1);
	OnSelectionChanged(nullptr, nullptr);

	// Nothing modified?
	if(!m_List->GetItemCount())
		OnOK();
}


void ModifiedExternalSamplesDlg::OnCheckAll()
{
	const bool check = IsChecked(IDC_CHECK1) ? true : false;
	const int count = m_List->GetItemCount();
	for(int i = 0; i < count; i++)
	{
		m_List->SetCheck(i, check);
	}
}


void ModifiedExternalSamplesDlg::OnSelectionChanged()
{
	int numChecked = 0;
	const int count = m_List->GetItemCount();
	for(int i = 0; i < count; i++)
	{
		if(m_List->GetCheck(i))
			numChecked++;
	}
	const mpt::uchar *embedText, *saveText;
	if(numChecked == count)
	{
		embedText = U_("&Embed All");
		saveText = U_("&Save All");
	} else if(!numChecked)
	{
		embedText = U_("&Embed None");
		saveText = U_("&Save None");
	} else
	{
		embedText = U_("&Embed Selected");
		saveText = U_("&Save Selected");
	}

	GetDlgItem(IDOK)->SetWindowText(embedText);
	GetDlgItem(IDC_SAVE)->SetWindowText(saveText);
}


void ModifiedExternalSamplesDlg::Execute(bool doSave)
{
	ScopedLogCapturer log(m_modDoc, U_("Modified Samples"), this);

	bool ok = true;
	const int count = m_List->GetItemCount();
	for(int i = 0; i < count; i++)
	{
		if(!m_List->GetCheck(i))
			continue;

		SAMPLEINDEX smp = static_cast<SAMPLEINDEX>(m_List->GetItemData(i));
		if(doSave)
		{
			ok &= m_modDoc.SaveSample(smp);
		} else
		{
			m_sndFile.GetSample(smp).uFlags.reset(SMP_KEEPONDISK);
			m_modDoc.SetModified();
		}
	}
	
	if(ok)
		ResizableDialog::OnOK();
	else
		ResizableDialog::OnCancel();
}


OPENMPT_NAMESPACE_END
