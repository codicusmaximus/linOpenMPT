/*
 * CDecimalSupport.h
 * -----------------
 * Purpose: Decimal number support for edit controls.
 * Notes  : mfcless twin of upstream's mptrack/CDecimalSupport.h (originally by
 *          Alexander Uckun). Upstream filters typed and pasted characters in
 *          a subclassed edit control; here a validator on the QLineEdit does
 *          the same, with the locale's decimal separator and negative sign.
 *          The interface (GetDecimalValue, SetDecimalValue, ...) is
 *          upstream's.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */

#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "AccessibleControls.h"
#include "../soundlib/Snd_defs.h"

OPENMPT_NAMESPACE_BEGIN

template <class T>
class CDecimalSupport
{
protected:
	bool m_allowNegative = true, m_allowFractions = true;

	QLineEdit *Edit() const { return static_cast<const T *>(this)->get(); }

public:
	/// Read the edit's text as a number, with the locale's decimal separator.
	bool GetDecimalValue(double &d) const;
	/// Display d with count digits after the decimal separator.
	void SetFixedValue(double d, int count);
	/// Display d with at most count significant digits.
	void SetDecimalValue(double d, int count);
	void SetDecimalValue(double d) { SetDecimalValue(d, 6); }

	void AllowNegative(bool allow) { m_allowNegative = allow; UpdateValidator(); }
	void AllowFractions(bool allow) { m_allowFractions = allow; UpdateValidator(); }

protected:
	void UpdateValidator();
};


class CNumberEdit : public AccessibleEdit, public CDecimalSupport<CNumberEdit>
{
public:
	void Attach(QLineEdit *widget) { AccessibleEdit::Attach(widget); UpdateValidator(); }

	void SetTempoValue(const TEMPO &t);
	TEMPO GetTempoValue();
};

extern template class CDecimalSupport<CNumberEdit>;

OPENMPT_NAMESPACE_END
