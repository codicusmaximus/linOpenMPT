/*
 * Reporting.h
 * -----------
 * Purpose: A class for showing notifications, prompts, etc...
 * Notes  : mfcless twin of upstream's mptrack/Reporting.h: message boxes are
 *          QMessageBox, parents are QWidgets.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"

class QWidget;


OPENMPT_NAMESPACE_BEGIN


enum ConfirmAnswer
{
	cnfYes,
	cnfNo,
	cnfCancel,
};


enum RetryAnswer
{
	rtyRetry,
	rtyCancel,
};


// mfcless: the buttons and icon of CustomNotification, and its answers.
// The values are those of Windows' message boxes.
enum NotificationFlags : uint32
{
	nfOk = 0x0, nfOkCancel = 0x1, nfYesNoCancel = 0x3, nfYesNo = 0x4, nfRetryCancel = 0x5,
	nfIconError = 0x10, nfIconQuestion = 0x20, nfIconWarning = 0x30, nfIconInformation = 0x40,
	nfDefaultButton2 = 0x100,
};
enum NotificationAnswer : uint32
{
	naOk = 1, naCancel = 2, naRetry = 4, naYes = 6, naNo = 7,
};


class Reporting
{

public:

	// TODO Quick'n'dirty workaround for MsgBox(). Shouldn't be public.
	static uint32 CustomNotification(const AnyStringLocale &text, const AnyStringLocale &caption, uint32 flags, const QWidget *parent);

	// Show a simple notification
	static void Notification(const AnyStringLocale &text, const QWidget *parent = nullptr);
	static void Notification(const AnyStringLocale &text, const AnyStringLocale &caption, const QWidget *parent = nullptr);

	// Show a simple information
	static void Information(const AnyStringLocale &text, const QWidget *parent = nullptr);
	static void Information(const AnyStringLocale &text, const AnyStringLocale &caption, const QWidget *parent = nullptr);

	// Show a simple warning
	static void Warning(const AnyStringLocale &text, const QWidget *parent = nullptr);
	static void Warning(const AnyStringLocale &text, const AnyStringLocale &caption, const QWidget *parent = nullptr);

	// Show an error box.
	static void Error(const AnyStringLocale &text, const QWidget *parent = nullptr);
	static void Error(const AnyStringLocale &text, const AnyStringLocale &caption, const QWidget *parent = nullptr);

	// Simplified version of the above
	static void Message(LogLevel level, const AnyStringLocale &text, const QWidget *parent = nullptr);
	static void Message(LogLevel level, const AnyStringLocale &text, const AnyStringLocale &caption, const QWidget *parent = nullptr);

	// Show a confirmation dialog.
	static ConfirmAnswer Confirm(const AnyStringLocale &text, bool showCancel = false, bool defaultNo = false, const QWidget *parent = nullptr);
	static ConfirmAnswer Confirm(const AnyStringLocale &text, const AnyStringLocale &caption, bool showCancel = false, bool defaultNo = false, const QWidget *parent = nullptr);
	// work-around string literals for caption decaying to bool and catching the wrong overload instead of converting to a string.
	static ConfirmAnswer Confirm(const AnyStringLocale &text, const char *caption, bool showCancel = false, bool defaultNo = false, const QWidget *parent = nullptr) { return Confirm(text, AnyStringLocale(caption), showCancel, defaultNo, parent); }
	static ConfirmAnswer Confirm(const AnyStringLocale &text, const wchar_t *caption, bool showCancel = false, bool defaultNo = false, const QWidget *parent = nullptr) { return Confirm(text, AnyStringLocale(caption), showCancel, defaultNo, parent); }

	// Show a confirmation dialog.
	static RetryAnswer RetryCancel(const AnyStringLocale &text, const QWidget *parent = nullptr);
	static RetryAnswer RetryCancel(const AnyStringLocale &text, const AnyStringLocale &caption, const QWidget *parent = nullptr);

};


OPENMPT_NAMESPACE_END
