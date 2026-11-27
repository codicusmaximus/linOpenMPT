/*
 * ResizableDialog.cpp
 * -------------------
 * Purpose: A wrapper for resizable MFC dialogs that fixes various issues.
 * Notes  : mfcless twin of upstream's mptrack/ResizableDialog.cpp. Upstream
 *          reads the AFX_DIALOG_LAYOUT resource itself to work around MFC's
 *          dynamic layout; here DialogBase's mfcless::DialogLayout already
 *          does the same, and Qt does the DPI scaling.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "ResizableDialog.h"
#include "mfcless/DialogBuilder.h"
#include "mfcless/Resources.h"
#include "resource.h"

#include <QtGui/QIcon>
#include <QtGui/QPixmap>


OPENMPT_NAMESPACE_BEGIN


ResizableDialog::ResizableDialog(QWidget *parent)
	: DialogBase{parent}
{ }


ResizableDialog::ResizableDialog(uint32 nIDTemplate, QWidget *pParentWnd)
	: DialogBase{nIDTemplate, pParentWnd}
{ }


bool ResizableDialog::OnInitDialog()
{
	DialogBase::OnInitDialog();
	setWindowIcon(QIcon(QPixmap::fromImage(mfcless::res::Icon(IDR_MODULETYPE))));
	return true;
}


void ResizableDialog::ResizeDynamicLayout()
{
	for(auto *layout : findChildren<mfcless::DialogLayout *>(Qt::FindDirectChildrenOnly))
		layout->Apply();
}


void ResizableDialog::EnableAutoLayout(bool enable)
{
	for(auto *layout : findChildren<mfcless::DialogLayout *>(Qt::FindDirectChildrenOnly))
		layout->SetEnabled(enable);
}

OPENMPT_NAMESPACE_END
