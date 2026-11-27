/*
 * CImageListEx.cpp
 * ----------------
 * Purpose: A class that extends MFC's CImageList to handle alpha-blended images properly.
 * Notes  : mfcless twin of upstream's mptrack/CImageListEx.cpp. Qt draws
 *          alpha-blended images everywhere, so the 1-bit mask fallback is gone.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */

#include "CImageListEx.h"
#include "Image.h"
#include "MPTrackUtil.h"
#include "../misc/mptColor.h"

#include "mfcless/System.h"


OPENMPT_NAMESPACE_BEGIN

bool CImageListEx::Create(uint32 resourceID, int cx, int cy, int /*nInitial*/, int /*nGrow*/, double scaling, bool disabled, const mpt::span<const int> invertImages)
{
	std::unique_ptr<RawGDIDIB> bitmap;
	try
	{
		bitmap = LoadPixelImage(GetResource(resourceID, U_("PNG")), scaling, cx, cy);
		cx = mpt::saturate_round<int>(cx * scaling);
		cy = mpt::saturate_round<int>(cy * scaling);
	} catch(...)
	{
		return false;
	}

	const RawGDIDIB::Pixel buttonColor = mfcless::SystemColor(mfcless::SysColor::ButtonFace);
	const bool isDark = mpt::Color::GetLuma(buttonColor.r, buttonColor.g, buttonColor.b) < 128;
	if(isDark)
	{
		// Invert brightness of icons on dark themes
		for(const int img : invertImages)
		{
			for(int y = 0; y < cy; y++)
			{
				RawGDIDIB::Pixel *pixel = &(*bitmap)(img * cx, y);
				for(int x = 0; x < cx; x++, pixel++)
				{
					auto hsv = mpt::Color::RGB{pixel->r / 255.0f, pixel->g / 255.0f, pixel->b / 255.0f}.ToHSV();
					hsv.v = (1.0f - hsv.v) * (1.0f - hsv.s) + (hsv.v) * hsv.s;
					const auto rgb = hsv.ToRGB();
					pixel->r = mpt::saturate_trunc<uint8>(rgb.r * 255.0f);
					pixel->g = mpt::saturate_trunc<uint8>(rgb.g * 255.0f);
					pixel->b = mpt::saturate_trunc<uint8>(rgb.b * 255.0f);
				}
			}
		}
	}

	if(disabled)
	{
		// Grayed out icons
		for(auto &pixel : bitmap->Pixels())
		{
			if(pixel.a != 0)
			{
				uint8 y = mpt::Color::GetLuma(pixel.r, pixel.g, pixel.b);
				pixel.r = pixel.g = pixel.b = y;
				if(isDark)
					pixel.a -= pixel.a / 3;
				else
					pixel.a /= 2;
			}
		}
	}

	// mfcless: cut the strip into images.
	const QPixmap strip = QPixmap::fromImage(ToQImage(*bitmap));
	m_images.clear();
	m_size = QSize(cx, cy);
	if(cx <= 0 || cy <= 0)
		return false;
	for(int y = 0; y + cy <= strip.height(); y += cy)
	{
		for(int x = 0; x + cx <= strip.width(); x += cx)
		{
			m_images.push_back(strip.copy(x, y, cx, cy));
		}
	}
	return !m_images.empty();
}


OPENMPT_NAMESPACE_END
