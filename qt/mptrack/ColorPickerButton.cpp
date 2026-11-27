/*
 * ColorPickerButton.cpp
 * ---------------------
 * Purpose: A button for picking UI colors
 * Notes  : mfcless twin of upstream's mptrack/ColorPickerButton.cpp. The
 *          colour dialog is Qt's, with the channel colours in use as its
 *          custom colours.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "ColorPickerButton.h"
#include "HighDPISupport.h"
#include "Sndfile.h"

#include "mfcless/QtString.h"
#include "mfcless/System.h"

#include <QtWidgets/QColorDialog>

#include <algorithm>


OPENMPT_NAMESPACE_BEGIN


void ColorPickerButton::SetColor(uint32 color)
{
	m_color = color;
	SetWindowText(mfcless::ToQString(MPT_UFORMAT("Colour: {}% red, {}% green, {}% blue")
		(Util::muldivr(mfcless::RedOf(color), 100, 255), Util::muldivr(mfcless::GreenOf(color), 100, 255), Util::muldivr(mfcless::BlueOf(color), 100, 255))));
	Invalidate();
}


std::optional<uint32> ColorPickerButton::PickColor(const CSoundFile &sndFile, CHANNELINDEX chn)
{
	static std::array<uint32, 16> colors = {0};
	// Build a set of currently used channel colors to be displayed in the color picker.
	// Channels that are close to the currently edited channel are preferred.
	std::map<uint32, int> usedColors;
	for(CHANNELINDEX i = 0; i < sndFile.GetNumChannels(); i++)
	{
		auto color = sndFile.ChnSettings[i].color;
		if(color == ModChannelSettings::INVALID_COLOR)
			continue;
		const int distance = std::abs(static_cast<int>(i) - chn);
		usedColors[color] = usedColors.count(color) ? std::min(distance, usedColors[color]) : distance;
	}
	std::vector<std::pair<uint32, int>> sortedColors(usedColors.begin(), usedColors.end());
	std::sort(sortedColors.begin(), sortedColors.end(), [](const auto &l, const auto &r) { return l.second < r.second; });

	size_t numColors = std::min(colors.size(), sortedColors.size());
	if(numColors < colors.size())
	{
		// Try to keep as many currently unused colors as possible by shifting them to the end
		std::stable_sort(colors.begin(), colors.end(), [&usedColors](uint32 l, uint32 r) { return usedColors.count(l) > usedColors.count(r); });
	}
	auto col = sortedColors.begin();
	for(size_t i = 0; i < numColors; i++)
	{
		colors[i] = (col++)->first;
	}

	for(int i = 0; i < static_cast<int>(colors.size()) && i < QColorDialog::customCount(); i++)
		QColorDialog::setCustomColor(i, mfcless::ToQColor(colors[i]));
	const QColor picked = QColorDialog::getColor(mfcless::ToQColor(m_color), this);
	for(int i = 0; i < static_cast<int>(colors.size()) && i < QColorDialog::customCount(); i++)
		colors[i] = mfcless::FromQColor(QColorDialog::customColor(i));
	if(picked.isValid())
	{
		SetColor(mfcless::FromQColor(picked));
		return mfcless::FromQColor(picked);
	}

	return {};
}


void ColorPickerButton::DrawItem(mfcless::DrawItemInfo *dis)
{
	if(dis == nullptr)
		return;
	mfcless::DC *hdc = dis->hDC;
	mfcless::Rect rect = dis->rcItem;
	hdc->DrawEdge(&rect, (dis->itemState & mfcless::ods::Selected) ? mfcless::edge::SunkenInner : mfcless::edge::RaisedInner, mfcless::edge::Rect | mfcless::edge::Adjust);
	if(m_color == ModChannelSettings::INVALID_COLOR || (dis->itemState & mfcless::ods::Disabled))
	{
		hdc->FillSolidRect(rect, mfcless::SystemColor(mfcless::SysColor::ButtonFace));
	} else
	{
		hdc->FillSolidRect(rect, m_color);
	}
	if((dis->itemState & mfcless::ods::Focus))
	{
		int offset = HighDPISupport::ScalePixels(1, dis->hwndItem);
		rect.DeflateRect(offset, offset);
		hdc->DrawFocusRect(rect);
	}
}


OPENMPT_NAMESPACE_END
