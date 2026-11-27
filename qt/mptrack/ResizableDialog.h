/*
 * ResizableDialog.h
 * -----------------
 * Purpose: A wrapper for resizable MFC dialogs that fixes various issues.
 * Notes  : mfcless twin of upstream's mptrack/ResizableDialog.h. The dynamic
 *          layout (how each control moves and grows with the dialog) comes
 *          from the dialog's AFX_DIALOG_LAYOUT resource, as on Windows.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "DialogBase.h"

#include <QtCore/QPoint>
#include <QtCore/QRect>
#include <QtCore/QSize>

OPENMPT_NAMESPACE_BEGIN

class ResizableDialog : public DialogBase
{
	Q_OBJECT
public:
	struct DynamicItem
	{
		QWidget *hwnd = nullptr;
		QPoint initialPoint;
		QSize initialSize;
		// Percentages of the dialog's growth, as in AFX_DIALOG_LAYOUT.
		int moveX = 0, moveY = 0;
		int sizeX = 0, sizeY = 0;
	};

	ResizableDialog(QWidget *parent = nullptr);
	explicit ResizableDialog(uint32 nIDTemplate, QWidget *pParentWnd = nullptr);

protected:
	bool OnInitDialog() override;
	virtual void ResizeDynamicLayout();
	void resizeEvent(QResizeEvent *e) override;

	void EnableAutoLayout(bool enable) { m_enableAutoLayout = enable; }

private:
	void LoadDynamicLayout(uint32 templateID);
	QRect AdjustItemRect(const DynamicItem &item, const QSize windowSize) const;

	std::vector<DynamicItem> m_dynamicItems;
	QPoint m_minSize;
	QSize m_originalClientSize;
	int m_originalDPI = 0;
	bool m_enableAutoLayout = true;
};

OPENMPT_NAMESPACE_END
