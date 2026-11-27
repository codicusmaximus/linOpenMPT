/*
 * ProgressDialog.h
 * ----------------
 * Purpose: An abortable, progress-indicating dialog, e.g. for showing conversion progress.
 * Notes  : mfcless twin of upstream's mptrack/ProgressDialog.h.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "DialogBase.h"

class QWidget;

OPENMPT_NAMESPACE_BEGIN

class CProgressDialog : public DialogBase
{
	Q_OBJECT
private:
	bool m_taskBarProgress = false;  // mfcless: no taskbar progress on the desktop; kept as a flag
	uint64 m_min = 0, m_max = 0, m_shift = 0;
	const bool m_customDialog;

public:
	bool m_abort = false;

	CProgressDialog(QWidget *parent = nullptr, uint32 resourceID = 0);
	~CProgressDialog();

	// Set the window title
	void SetTitle(const mpt::ustring &title);
	// Set the text on abort button
	void SetAbortText(const mpt::ustring &abort);
	// Set the text to be displayed along the progress bar.
	void SetText(const mpt::ustring &text);
	// Set the minimum and maximum value of the progress bar.
	void SetRange(uint64 min, uint64 max);
	// Set the current progress.
	void SetProgress(uint64 progress);
	// Show the progress in the task bar as well
	void EnableTaskbarProgress();
	// Process all queued Windows messages
	void ProcessMessages();
	// Run method for this dialog that must implement the action to be carried out - unless a custom resourceID was provided,
	// in which case the user is responsible for running the dialog and the implementation of this function may be a no-op.
	virtual void Run() = 0;

protected:
	bool OnInitDialog() override;
	void OnCancel() override { m_abort = true; }

	void RegisterMessageMap() override;
};

OPENMPT_NAMESPACE_END
