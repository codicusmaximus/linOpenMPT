/*
 * ExternalSamples.h
 * -----------------
 * Purpose: Dialogs for locating missing external samples and handling modified samples
 * Notes  : mfcless twin of upstream's mptrack/ExternalSamples.h.
 *          Hand-edited from the twin.py draft.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"

#include "CListCtrl.h"
#include "ResizableDialog.h"
#include "Snd_defs.h"

class QWidget;

OPENMPT_NAMESPACE_BEGIN

class CModDoc;
class CSoundFile;

class MissingExternalSamplesDlg : public ResizableDialog
{
	Q_OBJECT
protected:
	CModDoc &m_modDoc;
	CSoundFile &m_sndFile;
	CListCtrlEx *m_List = nullptr;
	bool m_isScanning = false;

public:
	MissingExternalSamplesDlg(CModDoc &modDoc, QWidget *parent);

protected:
	void GenerateList();
	bool SetSample(SAMPLEINDEX smp, const mpt::PathString &fileName);
	
	void DoDataExchange(DataExchange &dx) override;
	bool OnInitDialog() override;

	void OnSetPath();
	void OnScanFolder();

	void RegisterMessageMap() override;
};


class ModifiedExternalSamplesDlg : public ResizableDialog
{
	Q_OBJECT
protected:
	CModDoc &m_modDoc;
	CSoundFile &m_sndFile;
	CListCtrlEx *m_List = nullptr;

public:
	ModifiedExternalSamplesDlg(CModDoc &modDoc, QWidget *parent);

protected:
	void GenerateList();
	void Execute(bool doSave);

	void DoDataExchange(DataExchange &dx) override;
	bool OnInitDialog() override;
	void OnOK() override { Execute(false); }

	void OnSaveSelected() { Execute(true); }
	void OnCheckAll();
	void OnSelectionChanged();

	void RegisterMessageMap() override;
};

OPENMPT_NAMESPACE_END
