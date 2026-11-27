/*
 * Image.cpp
 * ---------
 * Purpose: Bitmap and Vector image file handling.
 * Notes  : mfcless twin of upstream's mptrack/Image.cpp. Decoding and
 *          scaling are Qt's.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */

#include "Image.h"
#include "../common/FileReader.h"

#include <QtGui/QPainter>


OPENMPT_NAMESPACE_BEGIN


RawGDIDIB::RawGDIDIB(uint32 width, uint32 height)
	: width(width)
	, height(height)
	, pixels(width * height)
{
	MPT_ASSERT(width > 0);
	MPT_ASSERT(height > 0);
}


QImage ToQImage(const RawGDIDIB &src)
{
	QImage image(static_cast<int>(src.Width()), static_cast<int>(src.Height()), QImage::Format_ARGB32);
	for(uint32 y = 0; y < src.Height(); y++)
	{
		auto *line = reinterpret_cast<QRgb *>(image.scanLine(static_cast<int>(y)));
		for(uint32 x = 0; x < src.Width(); x++)
		{
			const RawGDIDIB::Pixel &p = src(x, y);
			line[x] = qRgba(p.r, p.g, p.b, p.a);
		}
	}
	return image;
}


QImage LoadImage(mpt::const_byte_span file, double scaling, int spriteWidth, int spriteHeight)
{
	QImage image;
	if(!image.loadFromData(reinterpret_cast<const uchar *>(file.data()), static_cast<int>(file.size())))
		throw bad_image();
	image = image.convertToFormat(QImage::Format_ARGB32);
	if(scaling != 1.0)
	{
		// Sprites are scaled one by one, so that they do not bleed into each other.
		const int sw = spriteWidth > 0 ? spriteWidth : image.width();
		const int sh = spriteHeight > 0 ? spriteHeight : image.height();
		const int newSW = mpt::saturate_round<int>(sw * scaling), newSH = mpt::saturate_round<int>(sh * scaling);
		const int cols = image.width() / sw, rows = image.height() / sh;
		QImage result(cols * newSW, rows * newSH, QImage::Format_ARGB32);
		result.fill(Qt::transparent);
		QPainter p(&result);
		p.setRenderHint(QPainter::SmoothPixmapTransform, true);
		for(int r = 0; r < rows; r++)
		{
			for(int c = 0; c < cols; c++)
			{
				p.drawImage(QRect(c * newSW, r * newSH, newSW, newSH), image, QRect(c * sw, r * sh, sw, sh));
			}
		}
		p.end();
		image = result;
	}
	return image;
}


static std::unique_ptr<RawGDIDIB> ToRawGDIDIB(const QImage &image)
{
	auto result = std::make_unique<RawGDIDIB>(image.width(), image.height());
	for(int y = 0; y < image.height(); y++)
	{
		const auto *line = reinterpret_cast<const QRgb *>(image.constScanLine(y));
		for(int x = 0; x < image.width(); x++)
		{
			(*result)(x, y) = RawGDIDIB::Pixel(static_cast<uint8>(qRed(line[x])), static_cast<uint8>(qGreen(line[x])), static_cast<uint8>(qBlue(line[x])), static_cast<uint8>(qAlpha(line[x])));
		}
	}
	return result;
}


std::unique_ptr<RawGDIDIB> LoadPixelImage(mpt::const_byte_span file, double scaling, int spriteWidth, int spriteHeight)
{
	return ToRawGDIDIB(LoadImage(file, scaling, spriteWidth, spriteHeight));
}


std::unique_ptr<RawGDIDIB> LoadPixelImage(FileReader file, double scaling, int spriteWidth, int spriteHeight)
{
	FileReader::PinnedView view = file.GetPinnedView();
	return LoadPixelImage(view.span(), scaling, spriteWidth, spriteHeight);
}


OPENMPT_NAMESPACE_END
