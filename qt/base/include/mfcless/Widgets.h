/*
 * Widgets.h
 * ---------
 * Purpose: The Qt widgets and mfcless helpers that the tracker's dialogs and
 *          views use, in one include for the twin sources (whose upstream
 *          files got all of MFC's controls from stdafx.h).
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */

#pragma once

#include <QtCore/QTimer>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <QtGui/QKeyEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QPaintEvent>
#include <QtGui/QPainter>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QSlider>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QTabBar>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QTreeWidget>

#include "mfcless/Button.h"
#include "mfcless/Clipboard.h"
#include "mfcless/Controls.h"
#include "mfcless/Graphics.h"
#include "mfcless/Items.h"
#include "mfcless/Keyboard.h"
#include "mfcless/ListCtrl.h"
#include "mfcless/Menus.h"
#include "mfcless/MessageMap.h"
#include "mfcless/ModMessage.h"
#include "mfcless/QtString.h"
#include "mfcless/Resources.h"
#include "mfcless/System.h"
#include "mfcless/Text.h"
#include "mfcless/Window.h"

namespace mfcless
{

// What a control shows (Windows' window text): an edit field's text, a
// button's or label's caption, a combo box's current text, or else the
// window title.
QString WidgetText(const QWidget *w);
void SetWidgetText(QWidget *w, const QString &text);

inline void SetWindowText(QWidget *w, const Text &text) { if(w) SetWidgetText(w, text.value); }
inline void SetWindowText(QWidget &w, const Text &text) { SetWidgetText(&w, text.value); }
inline ::mpt::ustring GetWindowTextString(const QWidget *w) { return w ? FromQString(WidgetText(w)) : ::mpt::ustring(); }
inline ::mpt::ustring GetWindowTextString(const QWidget &w) { return FromQString(WidgetText(&w)); }
inline void GetWindowText(const QWidget *w, ::mpt::ustring &out) { out = GetWindowTextString(w); }
inline void GetWindowText(const QWidget &w, ::mpt::ustring &out) { out = GetWindowTextString(&w); }
inline void GetWindowText(const QWidget *w, QString &out) { out = w ? WidgetText(w) : QString(); }
// Into a fixed buffer of characters (UTF-8 or wide), cut to fit and
// terminated.
template <typename Tchar, std::size_t N>
inline int GetWindowText(const QWidget *w, Tchar (&buf)[N], int size = static_cast<int>(N))
{
	const QString text = w ? WidgetText(w) : QString();
	std::size_t n = 0;
	if constexpr(sizeof(Tchar) == 1)
	{
		const QByteArray utf8 = text.toUtf8();
		n = std::min<std::size_t>(static_cast<std::size_t>(utf8.size()), static_cast<std::size_t>(std::max(size, 1)) - 1);
		for(std::size_t i = 0; i < n; i++)
			buf[i] = static_cast<Tchar>(utf8[static_cast<qsizetype>(i)]);
	} else
	{
		const std::wstring wide = text.toStdWString();
		n = std::min<std::size_t>(wide.size(), static_cast<std::size_t>(std::max(size, 1)) - 1);
		for(std::size_t i = 0; i < n; i++)
			buf[i] = static_cast<Tchar>(wide[i]);
	}
	buf[n] = 0;
	return static_cast<int>(n);
}
inline int GetWindowTextLength(const QWidget *w) { return w ? static_cast<int>(WidgetText(w).size()) : 0; }

// The wait cursor (an hourglass while something takes long).
inline void BeginWaitCursor() { QApplication::setOverrideCursor(Qt::WaitCursor); }
inline void EndWaitCursor() { QApplication::restoreOverrideCursor(); }

// The widget with the keyboard focus (GetFocus).
inline QWidget *GetFocus() { return QApplication::focusWidget(); }

// A short system beep (MessageBeep).
inline void MessageBeep(uint32_t = 0) { QApplication::beep(); }

// number * numerator / denominator, rounded, in 64 bits (MulDiv).
inline int MulDiv(int number, int numerator, int denominator)
{
	if(denominator == 0)
		return -1;
	const int64_t product = static_cast<int64_t>(number) * numerator;
	return static_cast<int>(std::llround(static_cast<double>(static_cast<int64_t>(number) * numerator) / denominator));
}

} // namespace mfcless
