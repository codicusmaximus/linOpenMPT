/*
 * Image.h
 * -------
 * Purpose: Bitmap and Vector image file handling.
 * Notes  : mfcless twin of upstream's mptrack/Image.h. Images are decoded
 *          by Qt (QImage) instead of GDI+.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"

#include "../common/FileReaderFwd.h"

#include <QtGui/QImage>


OPENMPT_NAMESPACE_BEGIN


class bad_image : public std::runtime_error { public: bad_image() : std::runtime_error("") { } };


class RawGDIDIB
{
public:
	struct Pixel
	{
		// Component order must be compatible with Microsoft DIBs!
		// (mfcless: and with QImage::Format_ARGB32 on little-endian machines.)
		uint8 b;
		uint8 g;
		uint8 r;
		uint8 a;
		constexpr Pixel() noexcept
		    : b(0), g(0), r(0), a(0) {}
		constexpr Pixel(uint8 r, uint8 g, uint8 b, uint8 a) noexcept
		    : b(b), g(g), r(r), a(a) {}
		constexpr Pixel(uint32 color) noexcept
		    : b(static_cast<uint8>(color >> 16)), g(static_cast<uint8>(color >> 8)), r(static_cast<uint8>(color)), a(0) {}
	};
private:
	uint32 width;
	uint32 height;
	std::vector<Pixel> pixels;
public:
	RawGDIDIB(uint32 width, uint32 height);
public:
	constexpr uint32 Width() const noexcept { return width; }
	constexpr uint32 Height() const noexcept { return height; }
	MPT_FORCEINLINE Pixel &operator()(uint32 x, uint32 y) noexcept { return pixels[y * width + x]; }
	MPT_FORCEINLINE const Pixel &operator()(uint32 x, uint32 y) const noexcept { return pixels[y * width + x]; }
	std::vector<Pixel> &Pixels() { return pixels; }
	const std::vector<Pixel> &Pixels() const { return pixels; }
};


// mfcless: the image as a QImage (a copy, ARGB32).
QImage ToQImage(const RawGDIDIB &src);

// mfcless: an image file (PNG, BMP, ...) decoded and scaled. Throws bad_image.
QImage LoadImage(mpt::const_byte_span file, double scaling = 1.0, int spriteWidth = 0, int spriteHeight = 0);

std::unique_ptr<RawGDIDIB> LoadPixelImage(mpt::const_byte_span file, double scaling = 1.0, int spriteWidth = 0, int spriteHeight = 0);
std::unique_ptr<RawGDIDIB> LoadPixelImage(FileReader file, double scaling = 1.0, int spriteWidth = 0, int spriteHeight = 0);


OPENMPT_NAMESPACE_END
