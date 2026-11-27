/*
 * ProgressDialog.cpp
 * ------------------
 * Purpose: An abortable, progress-indicating dialog, e.g. for showing conversion progress.
 * Notes  : mfcless twin of upstream's mptrack/ProgressDialog.cpp.
 *          Draft from qt/tools/twin.py, not yet edited by hand.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "ProgressDialog.h"
#include "mfcless/Widgets.h"
#include "Mptrack.h"
#include "resource.h"

OPENMPT_NAMESPACE_BEGIN

void CProgressDialog::RegisterMessageMap()
{
	DialogBase::RegisterMessageMap();
	AddCommand(IDC_BUTTON1, &CProgressDialog::Run);
}

CProgressDialog::CProgressDialog(QWidget *parent, uint32 resourceID)
	: DialogBase{resourceID <= 0 ? IDD_PROGRESS : resourceID, parent}
	, m_customDialog{resourceID > 0}
{ }

CProgressDialog::~CProgressDialog()
{
	if(m_taskBarList)
	{
		m_taskBarList->SetProgressState(*theApp.m_pMainWnd, TBPF_NOPROGRESS);
		m_taskBarList->Release();
	}

	if(IsWindow(this))
	{
		// This should only happen if this dialog gets destroyed as part of stack unwinding
		EndDialog(IDCANCEL);
		DestroyWindow();
	}
}

bool CProgressDialog::OnInitDialog()
{
	DialogBase::OnInitDialog();
	if(!m_customDialog)
		mfcless::PostCommand(this, IDC_BUTTON1);
	return true;
}


void CProgressDialog::SetTitle(const mpt::uchar *title)
{
	SetWindowText(title);
}



void CProgressDialog::SetAbortText(const mpt::uchar *abort)
{
	SetItemText(IDCANCEL, abort);
}


void CProgressDialog::SetText(const mpt::uchar *text)
{
	SetItemText(IDC_TEXT1, text);
}


void CProgressDialog::SetRange(uint64 min, uint64 max)
{
	MPT_ASSERT(min <= max);
	m_min = min;
	m_max = max;
	m_shift = 0;
	// Is the range too big for 32-bit values?
	while(max > int32_max)
	{
		m_shift++;
		max >>= 1;
	}
	::SendMessage(::GetDlgItem(this, IDC_PROGRESS1), PBM_SETRANGE32, static_cast<uint32>(m_min >> m_shift), static_cast<uint32>(m_max >> m_shift));
}


void CProgressDialog::SetProgress(uint64 progress)
{
	::SendMessage(::GetDlgItem(this, IDC_PROGRESS1), PBM_SETPOS, static_cast<uint32>(progress >> m_shift), 0);
	if(m_taskBarList != nullptr)
	{
		m_taskBarList->SetProgressValue(*theApp.m_pMainWnd, progress - m_min, m_max - m_min);
	}
}


void CProgressDialog::EnableTaskbarProgress()
{
	if(CoCreateInstance(CLSID_TaskbarList, nullptr, CLSCTX_ALL, IID_ITaskbarList3, (void**)&m_taskBarList) != S_OK)
	{
		return;
	}
	if(m_taskBarList != nullptr)
	{
		m_taskBarList->SetProgressState(*theApp.m_pMainWnd, TBPF_NORMAL);
	}
}


void CProgressDialog::ProcessMessages()
{
	MSG msg;
	while(::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
	{
		::TranslateMessage(&msg);
		::DispatchMessage(&msg);
	}
}

OPENMPT_NAMESPACE_END
