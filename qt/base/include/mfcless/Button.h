/*
 * Button.h
 * --------
 * Purpose: A push button that upstream may draw itself (owner draw): it
 *          calls DrawItem, which by default asks the owner (the window it is
 *          in) with OnDrawItem, as Windows' WM_DRAWITEM does.
 * Notes  : A button is owner-drawn if its dialog template says so
 *          (BS_OWNERDRAW) or after SetOwnerDraw.
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */

#pragma once

#include "mfcless/Graphics.h"
#include "mfcless/Window.h"

#include <QtWidgets/QPushButton>

namespace mfcless
{

class Button : public Window<QPushButton>
{
	Q_OBJECT
public:
	explicit Button(QWidget *parent = nullptr);

	void SetOwnerDraw(bool ownerDraw) { m_ownerDraw = ownerDraw; update(); }
	bool IsOwnerDraw() const;

	// Draws the button; the default asks the owner.
	virtual void DrawItem(DrawItemInfo *info);

protected:
	void OnPaint() override;

	bool m_ownerDraw = false;
};

} // namespace mfcless
