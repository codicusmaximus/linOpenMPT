/*
 * ResizableDialog.h
 * -----------------
 * Purpose: A wrapper for resizable MFC dialogs that fixes various issues.
 * Notes  : mfcless twin of upstream's mptrack/ResizableDialog.h. The dynamic
 *          layout (how each control moves and grows with the dialog) comes
 *          from the dialog's AFX_DIALOG_LAYOUT resource, as on Windows, and
 *          DialogBase applies it (mfcless::DialogLayout) to every dialog with
 *          a resizable frame. The minimum size is the template's size.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "DialogBase.h"

OPENMPT_NAMESPACE_BEGIN

class ResizableDialog : public DialogBase
{
	Q_OBJECT
public:
	ResizableDialog(QWidget *parent = nullptr);
	explicit ResizableDialog(uint32 nIDTemplate, QWidget *pParentWnd = nullptr);

protected:
	bool OnInitDialog() override;
	virtual void ResizeDynamicLayout();

	void EnableAutoLayout(bool enable);
};

OPENMPT_NAMESPACE_END
