/*
 * Reporting.cpp
 * -------------
 * Purpose: A class for showing notifications, prompts, etc...
 * Notes  : mfcless twin of upstream's mptrack/Reporting.cpp. Message boxes
 *          are QMessageBox.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "Reporting.h"
#include "../mptrack/Mainfrm.h"

#include "mfcless/QtString.h"

#include <QtWidgets/QApplication>
#include <QtWidgets/QMessageBox>


OPENMPT_NAMESPACE_BEGIN


static inline uint32 LogLevelToFlags(LogLevel level)
{
	switch(level)
	{
	case LogDebug: return nfOk; break;
	case LogNotification: return nfOk; break;
	case LogInformation: return nfOk | nfIconInformation; break;
	case LogWarning: return nfOk | nfIconWarning; break;
	case LogError: return nfOk | nfIconError; break;
	}
	return nfOk;
}


static mpt::ustring GetTitle()
{
	return MAINFRAME_TITLE;
}


static mpt::ustring FillEmptyCaption(const mpt::ustring &caption, LogLevel level)
{
	mpt::ustring result = caption;
	if(result.empty())
	{
		result = GetTitle() + U_(" - ");
		switch(level)
		{
		case LogDebug: result += U_("Debug"); break;
		case LogNotification: result += U_("Notification"); break;
		case LogInformation: result += U_("Information"); break;
		case LogWarning: result += U_("Warning"); break;
		case LogError: result += U_("Error"); break;
		}
	}
	return result;
}


static mpt::ustring FillEmptyCaption(const mpt::ustring &caption)
{
	mpt::ustring result = caption;
	if(result.empty())
	{
		result = GetTitle();
	}
	return result;
}


static uint32 ShowNotificationImpl(mpt::ustring text, const mpt::ustring &caption, uint32 flags, const QWidget *parent)
{
	if(parent == nullptr)
	{
		parent = QApplication::activeModalWidget();
		if(parent == nullptr)
			parent = QApplication::activeWindow();
		if(parent == nullptr)
			parent = CMainFrame::GetMainFrame();
	}
	QMessageBox box(const_cast<QWidget *>(parent));
	box.setWindowTitle(mfcless::ToQString(caption.empty() ? mpt::ustring(MAINFRAME_TITLE) : caption));
	box.setText(mfcless::ToQString(text));
	switch(flags & 0xF0)
	{
	case nfIconError: box.setIcon(QMessageBox::Critical); break;
	case nfIconQuestion: box.setIcon(QMessageBox::Question); break;
	case nfIconWarning: box.setIcon(QMessageBox::Warning); break;
	case nfIconInformation: box.setIcon(QMessageBox::Information); break;
	default: break;
	}
	QMessageBox::StandardButton first = QMessageBox::Ok, second = QMessageBox::NoButton;
	switch(flags & 0x0F)
	{
	case nfOkCancel: box.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel); second = QMessageBox::Cancel; break;
	case nfYesNoCancel: box.setStandardButtons(QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel); first = QMessageBox::Yes; second = QMessageBox::No; break;
	case nfYesNo: box.setStandardButtons(QMessageBox::Yes | QMessageBox::No); first = QMessageBox::Yes; second = QMessageBox::No; break;
	case nfRetryCancel: box.setStandardButtons(QMessageBox::Retry | QMessageBox::Cancel); first = QMessageBox::Retry; second = QMessageBox::Cancel; break;
	default: box.setStandardButtons(QMessageBox::Ok); break;
	}
	box.setDefaultButton((flags & nfDefaultButton2) && second != QMessageBox::NoButton ? second : first);
	switch(box.exec())
	{
	case QMessageBox::Ok: return naOk;
	case QMessageBox::Yes: return naYes;
	case QMessageBox::No: return naNo;
	case QMessageBox::Retry: return naRetry;
	default: return naCancel;
	}
}


uint32 Reporting::CustomNotification(const AnyStringLocale &text, const AnyStringLocale &caption, uint32 flags, const QWidget *parent)
{
	return ShowNotificationImpl(mpt::ToUnicode(text), FillEmptyCaption(mpt::ToUnicode(caption)), flags, parent);
}


void Reporting::Notification(const AnyStringLocale &text, const QWidget *parent)
{
	ShowNotificationImpl(mpt::ToUnicode(text), FillEmptyCaption(mpt::ustring(), LogNotification), LogLevelToFlags(LogNotification), parent);
}
void Reporting::Notification(const AnyStringLocale &text, const AnyStringLocale &caption, const QWidget *parent)
{
	ShowNotificationImpl(mpt::ToUnicode(text), FillEmptyCaption(mpt::ToUnicode(caption), LogNotification), LogLevelToFlags(LogNotification), parent);
}


void Reporting::Information(const AnyStringLocale &text, const QWidget *parent)
{
	ShowNotificationImpl(mpt::ToUnicode(text), FillEmptyCaption(mpt::ustring(), LogInformation), LogLevelToFlags(LogInformation), parent);
}
void Reporting::Information(const AnyStringLocale &text, const AnyStringLocale &caption, const QWidget *parent)
{
	ShowNotificationImpl(mpt::ToUnicode(text), FillEmptyCaption(mpt::ToUnicode(caption), LogInformation), LogLevelToFlags(LogInformation), parent);
}


void Reporting::Warning(const AnyStringLocale &text, const QWidget *parent)
{
	ShowNotificationImpl(mpt::ToUnicode(text), FillEmptyCaption(mpt::ustring(), LogWarning), LogLevelToFlags(LogWarning), parent);
}
void Reporting::Warning(const AnyStringLocale &text, const AnyStringLocale &caption, const QWidget *parent)
{
	ShowNotificationImpl(mpt::ToUnicode(text), FillEmptyCaption(mpt::ToUnicode(caption), LogWarning), LogLevelToFlags(LogWarning), parent);
}


void Reporting::Error(const AnyStringLocale &text, const QWidget *parent)
{
	ShowNotificationImpl(mpt::ToUnicode(text), FillEmptyCaption(mpt::ustring(), LogError), LogLevelToFlags(LogError), parent);
}
void Reporting::Error(const AnyStringLocale &text, const AnyStringLocale &caption, const QWidget *parent)
{
	ShowNotificationImpl(mpt::ToUnicode(text), FillEmptyCaption(mpt::ToUnicode(caption), LogError), LogLevelToFlags(LogError), parent);
}


void Reporting::Message(LogLevel level, const AnyStringLocale &text, const QWidget *parent)
{
	ShowNotificationImpl(mpt::ToUnicode(text), FillEmptyCaption(mpt::ustring(), level), LogLevelToFlags(level), parent);
}
void Reporting::Message(LogLevel level, const AnyStringLocale &text, const AnyStringLocale &caption, const QWidget *parent)
{
	ShowNotificationImpl(mpt::ToUnicode(text), FillEmptyCaption(mpt::ToUnicode(caption), level), LogLevelToFlags(level), parent);
}


ConfirmAnswer Reporting::Confirm(const AnyStringLocale &text, bool showCancel, bool defaultNo, const QWidget *parent)
{
	return Confirm(mpt::ToUnicode(text), GetTitle() + U_(" - Confirmation"), showCancel, defaultNo, parent);
}


ConfirmAnswer Reporting::Confirm(const AnyStringLocale &text, const AnyStringLocale &caption, bool showCancel, bool defaultNo, const QWidget *parent)
{
	uint32 result = ShowNotificationImpl(mpt::ToUnicode(text), FillEmptyCaption(mpt::ToUnicode(caption)), (showCancel ? nfYesNoCancel : nfYesNo) | nfIconQuestion | (defaultNo ? nfDefaultButton2 : 0), parent);
	switch(result)
	{
	case naYes:
		return cnfYes;
	case naNo:
		return cnfNo;
	default:
	case naCancel:
		return cnfCancel;
	}
}


RetryAnswer Reporting::RetryCancel(const AnyStringLocale &text, const QWidget *parent)
{
	return RetryCancel(mpt::ToUnicode(text), GetTitle(), parent);
}


RetryAnswer Reporting::RetryCancel(const AnyStringLocale &text, const AnyStringLocale &caption, const QWidget *parent)
{
	uint32 result = ShowNotificationImpl(mpt::ToUnicode(text), FillEmptyCaption(mpt::ToUnicode(caption)), nfRetryCancel, parent);
	switch(result)
	{
	case naRetry:
		return rtyRetry;
	default:
	case naCancel:
		return rtyCancel;
	}
}


OPENMPT_NAMESPACE_END
