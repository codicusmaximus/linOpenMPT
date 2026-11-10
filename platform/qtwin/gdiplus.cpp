// The part of GDI+ OpenMPT uses (Image.cpp: decoding the PNG toolbar and
// icon images, scaling them for the display, cutting them into sprites), on
// QImage and QPainter. Images keep GDI+'s 32bppARGB layout in memory, which
// is QImage::Format_ARGB32 on little-endian machines.
#include "QtWin.h"

#include <gdiplus.h>

#include <QtGui/QImage>
#include <QtGui/QPainter>

using namespace linopenmpt::qtwin;

namespace Gdiplus
{

namespace
{

// What a Graphics draws with: a painter of its own (on an image) or one a
// DC lends it.
struct GraphicsData
{
	QPainter own;
	QPainter *painter = nullptr;
	bool smooth = true;
};

} // namespace

Status GdiplusStartup(ULONG_PTR *token, const GdiplusStartupInput *, GdiplusStartupOutput *)
{
	if(token)
		*token = 1;
	return Ok;
}

void GdiplusShutdown(ULONG_PTR)
{
}

// ---- images ------------------------------------------------------------------------

Image::~Image()
{
	delete static_cast<QImage *>(m_native);
}

UINT Image::GetWidth()
{
	const auto *image = static_cast<QImage *>(m_native);
	return image != nullptr ? static_cast<UINT>(image->width()) : 0;
}

UINT Image::GetHeight()
{
	const auto *image = static_cast<QImage *>(m_native);
	return image != nullptr ? static_cast<UINT>(image->height()) : 0;
}

Status Image::GetLastStatus() const
{
	return m_status;
}

Bitmap::Bitmap(IStream *stream, BOOL)
{
	QImage decoded;
	if(stream != nullptr)
		decoded = QImage::fromData(stream->Data(), static_cast<int>(stream->Size()));
	if(decoded.isNull())
	{
		m_status = UnknownImageFormat;
		return;
	}
	m_native = new QImage(decoded.convertToFormat(QImage::Format_ARGB32));
}

Bitmap::Bitmap(INT width, INT height, PixelFormat format)
{
	if(width <= 0 || height <= 0)
	{
		m_status = InvalidParameter;
		return;
	}
	auto *image = new QImage(width, height, format == PixelFormat32bppPARGB ? QImage::Format_ARGB32_Premultiplied : QImage::Format_ARGB32);
	if(image->isNull())
	{
		delete image;
		m_status = OutOfMemory;
		return;
	}
	image->fill(Qt::transparent);
	m_native = image;
}

Status Bitmap::LockBits(const Rect *rect, UINT, PixelFormat format, BitmapData *lockedData)
{
	auto *image = static_cast<QImage *>(m_native);
	if(image == nullptr || lockedData == nullptr)
		return InvalidParameter;
	const QRect area = rect != nullptr ? QRect(rect->X, rect->Y, rect->Width, rect->Height) : image->rect();
	if(!image->rect().contains(area))
		return InvalidParameter;
	const QImage::Format wanted = format == PixelFormat32bppPARGB ? QImage::Format_ARGB32_Premultiplied : QImage::Format_ARGB32;
	if(format != PixelFormat32bppARGB && format != PixelFormat32bppPARGB && format != PixelFormat32bppRGB)
		return NotImplemented;
	if(image->format() != wanted)
		*image = image->convertToFormat(wanted);
	lockedData->Width = static_cast<UINT>(area.width());
	lockedData->Height = static_cast<UINT>(area.height());
	lockedData->Stride = static_cast<INT>(image->bytesPerLine());
	lockedData->PixelFormat = format;
	lockedData->Scan0 = image->scanLine(area.top()) + area.left() * 4;
	lockedData->Reserved = 0;
	return Ok;
}

Status Bitmap::UnlockBits(BitmapData *)
{
	// The bits were the image's own.
	return Ok;
}

Metafile::Metafile(IStream *stream)
{
	// Vector images: Qt reads SVG only with its SVG module, which is not a
	// dependency; anything QImage reads works.
	QImage decoded;
	if(stream != nullptr)
		decoded = QImage::fromData(stream->Data(), static_cast<int>(stream->Size()));
	if(decoded.isNull())
	{
		m_status = UnknownImageFormat;
		return;
	}
	m_native = new QImage(decoded.convertToFormat(QImage::Format_ARGB32));
}

// ---- drawing ----------------------------------------------------------------------

Graphics::Graphics(HDC dc)
{
	auto *data = new GraphicsData;
	data->painter = DCPainter(dc);
	m_native = data;
}

Graphics::~Graphics()
{
	auto *data = static_cast<GraphicsData *>(m_native);
	if(data == nullptr)
		return;
	if(data->own.isActive())
		data->own.end();
	delete data;
}

Graphics *Graphics::FromImage(Image *image)
{
	auto *image32 = image != nullptr ? static_cast<QImage *>(image->m_native) : nullptr;
	if(image32 == nullptr)
		return nullptr;
	auto *graphics = new Graphics;
	auto *data = new GraphicsData;
	if(image32->format() != QImage::Format_ARGB32_Premultiplied && image32->format() != QImage::Format_ARGB32)
		*image32 = image32->convertToFormat(QImage::Format_ARGB32);
	data->own.begin(image32);
	data->painter = &data->own;
	graphics->m_native = data;
	return graphics;
}

Graphics *Graphics::FromHDC(HDC dc)
{
	return new Graphics(dc);
}

namespace
{

QPainter *PainterOf(void *native)
{
	auto *data = static_cast<GraphicsData *>(native);
	return data != nullptr && data->painter != nullptr && data->painter->isActive() ? data->painter : nullptr;
}

} // namespace

Status Graphics::SetInterpolationMode(InterpolationMode mode)
{
	auto *data = static_cast<GraphicsData *>(m_native);
	if(data == nullptr)
		return InvalidParameter;
	data->smooth = mode != InterpolationModeNearestNeighbor && mode != InterpolationModeLowQuality;
	if(QPainter *p = PainterOf(m_native))
		p->setRenderHint(QPainter::SmoothPixmapTransform, data->smooth);
	return Ok;
}

Status Graphics::SetSmoothingMode(SmoothingMode mode)
{
	if(QPainter *p = PainterOf(m_native))
		p->setRenderHint(QPainter::Antialiasing, mode == SmoothingModeAntiAlias || mode == SmoothingModeHighQuality);
	return Ok;
}

Status Graphics::SetPixelOffsetMode(PixelOffsetMode)
{
	// Qt samples pixel centres already, as PixelOffsetModeHalf does.
	return Ok;
}

Status Graphics::Clear(const Color &color)
{
	QPainter *p = PainterOf(m_native);
	if(p == nullptr)
		return WrongState;
	const ARGB argb = color.GetValue();
	const QPainter::CompositionMode old = p->compositionMode();
	p->setCompositionMode(QPainter::CompositionMode_Source);
	p->fillRect(QRect(0, 0, p->device()->width(), p->device()->height()), QColor::fromRgba(argb));
	p->setCompositionMode(old);
	return Ok;
}

Status Graphics::DrawImage(Image *image, INT x, INT y)
{
	QImage *src = image != nullptr ? static_cast<QImage *>(image->m_native) : nullptr;
	QPainter *p = PainterOf(m_native);
	if(src == nullptr || p == nullptr)
		return InvalidParameter;
	p->drawImage(QPoint(x, y), *src);
	return Ok;
}

Status Graphics::DrawImage(Image *image, INT x, INT y, INT width, INT height)
{
	QImage *src = image != nullptr ? static_cast<QImage *>(image->m_native) : nullptr;
	QPainter *p = PainterOf(m_native);
	if(src == nullptr || p == nullptr)
		return InvalidParameter;
	p->drawImage(QRect(x, y, width, height), *src);
	return Ok;
}

Status Graphics::DrawImage(Image *image, const Rect &dest, INT srcX, INT srcY, INT srcWidth, INT srcHeight, Unit)
{
	QImage *src = image != nullptr ? static_cast<QImage *>(image->m_native) : nullptr;
	QPainter *p = PainterOf(m_native);
	if(src == nullptr || p == nullptr)
		return InvalidParameter;
	p->drawImage(QRect(dest.X, dest.Y, dest.Width, dest.Height), *src, QRect(srcX, srcY, srcWidth, srcHeight));
	return Ok;
}

} // namespace Gdiplus
