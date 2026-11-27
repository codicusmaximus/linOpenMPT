/*
 * Widgets.cpp
 * -----------
 * Purpose: The text of a control, as Windows' window text.
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */

#include "mfcless/Widgets.h"

namespace mfcless
{

QString WidgetText(const QWidget *w)
{
	if(auto *edit = qobject_cast<const QLineEdit *>(w))
		return edit->text();
	if(auto *combo = qobject_cast<const QComboBox *>(w))
		return combo->currentText();
	if(auto *button = qobject_cast<const QAbstractButton *>(w))
		return button->text();
	if(auto *label = qobject_cast<const QLabel *>(w))
		return label->text();
	if(auto *plain = qobject_cast<const QPlainTextEdit *>(w))
		return plain->toPlainText();
	if(auto *rich = qobject_cast<const QTextEdit *>(w))
		return rich->toPlainText();
	if(auto *group = qobject_cast<const QGroupBox *>(w))
		return group->title();
	return w->windowTitle();
}


void SetWidgetText(QWidget *w, const QString &text)
{
	if(auto *edit = qobject_cast<QLineEdit *>(w))
	{
		if(edit->text() != text)
			edit->setText(text);
	} else if(auto *combo = qobject_cast<QComboBox *>(w))
	{
		if(combo->isEditable())
			combo->setEditText(text);
		else
			combo->setCurrentIndex(combo->findText(text));
	} else if(auto *button = qobject_cast<QAbstractButton *>(w))
		button->setText(text);
	else if(auto *label = qobject_cast<QLabel *>(w))
		label->setText(text);
	else if(auto *plain = qobject_cast<QPlainTextEdit *>(w))
	{
		// Multi-line edits take Windows' CR LF line breaks as one.
		QString lines = text;
		lines.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
		if(plain->toPlainText() != lines)
			plain->setPlainText(lines);
	} else if(auto *rich = qobject_cast<QTextEdit *>(w))
	{
		QString lines = text;
		lines.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
		rich->setPlainText(lines);
	}
	else if(auto *group = qobject_cast<QGroupBox *>(w))
		group->setTitle(text);
	else
		w->setWindowTitle(text);
	w->update();
}

} // namespace mfcless
