/*
 * CDecimalSupport.cpp
 * -------------------
 * Purpose: Various extensions of the CDecimalSupport implementation.
 * Notes  : mfcless twin of upstream's mptrack/CDecimalSupport.cpp, with the
 *          template's parsing and formatting (upstream's header) on a
 *          QLineEdit: numbers are shown and read with the locale's decimal
 *          separator and negative sign, and a validator lets through only
 *          what the number may contain, as upstream's character filter does.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "CDecimalSupport.h"
#include "mfcless/Widgets.h"

#include <QtCore/QLocale>
#include <QtCore/QRegularExpression>
#include <QtGui/QRegularExpressionValidator>

#include <cmath>

OPENMPT_NAMESPACE_BEGIN

namespace
{

QChar DecimalSeparator() { return QLocale::system().decimalPoint().at(0); }
QChar NegativeSign() { return QLocale::system().negativeSign().at(0); }

// Upstream's DecimalToText: digits after the separator without trailing
// zeros, and no separator if none are left.
QString TrimFraction(QString text)
{
	const int dot = text.indexOf(u'.');
	if(dot < 0)
		return text;
	int end = text.size();
	while(end > dot + 1 && text[end - 1] == u'0')
		end--;
	if(end == dot + 1)
		end = dot;
	text.truncate(end);
	return text;
}

QString ToDisplay(QString text)
{
	if(text.startsWith(u'-'))
		text[0] = NegativeSign();
	if(const int dot = text.indexOf(u'.'); dot >= 0)
		text[dot] = DecimalSeparator();
	return text;
}

} // namespace


template <class T>
bool CDecimalSupport<T>::GetDecimalValue(double &d) const
{
	QLineEdit *edit = Edit();
	if(!edit)
		return false;
	QString text = edit->text();
	// The locale's separator and negative sign become '.' and '-'.
	if(const QChar sep = DecimalSeparator(); sep != u'.')
	{
		if(const int pos = text.indexOf(sep); pos >= 0)
			text[pos] = u'.';
	}
	if(!text.isEmpty() && text[0] == NegativeSign())
		text[0] = u'-';
	bool ok = false;
	d = QLocale::c().toDouble(text.trimmed(), &ok);
	if(!ok)
	{
		// As strtod, the number at the start, if any.
		const QRegularExpressionMatch m = QRegularExpression(QStringLiteral(R"(^\s*[-+]?(\d+\.?\d*|\.\d+)([eE][-+]?\d+)?)")).match(text);
		d = m.hasMatch() ? QLocale::c().toDouble(m.captured(0).trimmed()) : 0.0;
		return text.trimmed().isEmpty() ? false : (m.hasMatch() && m.capturedLength(0) == text.size());
	}
	return true;
}


template <class T>
void CDecimalSupport<T>::SetFixedValue(double d, int count)
{
	if(QLineEdit *edit = Edit())
		mfcless::SetWindowText(edit, ToDisplay(TrimFraction(QString::number(d, 'f', std::max(count, 0)))));
}


template <class T>
void CDecimalSupport<T>::SetDecimalValue(double d, int count)
{
	// count significant digits, written out without an exponent.
	count = std::max(count, 1);
	int decimals = count;
	if(d != 0.0 && std::isfinite(d))
	{
		const int intDigits = static_cast<int>(std::floor(std::log10(std::abs(d)))) + 1;
		decimals = std::max(count - intDigits, 0);
	}
	if(QLineEdit *edit = Edit())
		mfcless::SetWindowText(edit, ToDisplay(TrimFraction(QString::number(d, 'f', decimals))));
}


template <class T>
void CDecimalSupport<T>::UpdateValidator()
{
	QLineEdit *edit = Edit();
	if(!edit || !edit->property("mfclessNumber").toBool())
		return;
	QString pattern = QStringLiteral("^");
	if(m_allowNegative)
		pattern += QStringLiteral("(?:-|%1)?").arg(QRegularExpression::escape(QString(NegativeSign())));
	pattern += QStringLiteral("\\d*");
	if(m_allowFractions)
		pattern += QStringLiteral("((?:\\.|%1)\\d*)?").arg(QRegularExpression::escape(QString(DecimalSeparator())));
	pattern += QStringLiteral("$");
	if(auto *old = edit->validator(); old && old->parent() == edit)
		delete old;
	edit->setValidator(new QRegularExpressionValidator(QRegularExpression(pattern), edit));
}


template class CDecimalSupport<CNumberEdit>;


void CNumberEdit::SetTempoValue(const TEMPO &t)
{
	SetFixedValue(t.ToDouble(), 4);
}


TEMPO CNumberEdit::GetTempoValue()
{
	double d;
	GetDecimalValue(d);
	return TEMPO(d);
}

OPENMPT_NAMESPACE_END
