/*
 * Button.cpp
 * ----------
 * Purpose: A push button that upstream may draw itself (owner draw).
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */

#include "mfcless/Button.h"

namespace mfcless
{

Button::Button(QWidget *parent)
	: Window(parent)
{
}


bool Button::IsOwnerDraw() const
{
	return m_ownerDraw || (property("mfcless_style").toUInt() & 0x0F) == 0x0B;  // BS_OWNERDRAW
}


void Button::OnPaint()
{
	if(!IsOwnerDraw())
	{
		Default();
		return;
	}
	PaintDC dc(this);
	DrawItemInfo info;
	info.CtlType = odt::Button;
	info.CtlID = property("mfcless_id").toUInt();
	info.itemAction = 1;  // ODA_DRAWENTIRE
	if(isDown() || isChecked())
		info.itemState |= ods::Selected;
	if(!isEnabled())
		info.itemState |= ods::Disabled;
	if(hasFocus())
		info.itemState |= ods::Focus;
	if(isDefault())
		info.itemState |= ods::Default;
	info.hwndItem = this;
	info.hDC = &dc;
	info.rcItem = Rect(rect());
	DrawItem(&info);
}


void Button::DrawItem(DrawItemInfo *info)
{
	for(QWidget *w = parentWidget(); w; w = w->parentWidget())
	{
		if(auto *owner = dynamic_cast<WindowBase *>(w))
		{
			owner->OnDrawItem(static_cast<int>(info->CtlID), info);
			return;
		}
	}
}

} // namespace mfcless
