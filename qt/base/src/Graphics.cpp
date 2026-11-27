/*
 * Graphics.cpp
 * ------------
 * Purpose: Device contexts on top of QPainter.
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */

#include "mfcless/Graphics.h"

#include <QtGui/QFontMetrics>
#include <QtGui/QPainterPath>
#include <QtWidgets/QAbstractScrollArea>
#include <QtWidgets/QApplication>
#include <QtWidgets/QWidget>

#include <algorithm>

namespace mfcless
{

QPen Pen::ToQPen() const
{
	if(style == pen::Null)
		return QPen(Qt::NoPen);
	QPen p(ToQColor(color));
	p.setWidth(std::max(1, width));
	p.setCosmetic(width <= 1);
	p.setCapStyle(Qt::FlatCap);
	p.setJoinStyle(Qt::MiterJoin);
	switch(style)
	{
	case pen::Dash: p.setStyle(Qt::DashLine); break;
	case pen::Dot: p.setStyle(Qt::DotLine); break;
	case pen::DashDot: p.setStyle(Qt::DashDotLine); break;
	case pen::DashDotDot: p.setStyle(Qt::DashDotDotLine); break;
	default: p.setStyle(Qt::SolidLine); break;
	}
	return p;
}


bool Bitmap::CreateCompatibleBitmap(DC *, int width, int height)
{
	return CreateBitmap(width, height);
}

bool Bitmap::CreateBitmap(int width, int height)
{
	image = QImage(std::max(1, width), std::max(1, height), QImage::Format_RGB32);
	image.fill(Qt::black);
	return true;
}


DC::DC()
{
	m_font = QApplication::font();
}

DC::DC(QPaintDevice *device)
	: DC()
{
	Begin(device);
}

DC::DC(QPainter &painter)
	: DC()
{
	m_painter = &painter;
	m_device = painter.device();
	m_font = painter.font();
	ApplyPen();
	ApplyBrush();
}

DC::~DC()
{
	End();
}

void DC::Begin(QPaintDevice *device)
{
	End();
	if(!device)
		return;
	m_ownPainter = std::make_unique<QPainter>(device);
	m_painter = m_ownPainter.get();
	m_device = device;
	m_painter->setRenderHint(QPainter::Antialiasing, false);
	m_painter->setFont(m_font);
	ApplyPen();
	ApplyBrush();
}

void DC::End()
{
	if(m_ownPainter)
	{
		m_ownPainter->end();
		m_ownPainter.reset();
	}
	m_painter = nullptr;
	m_device = nullptr;
}

bool DC::CreateCompatibleDC(DC *)
{
	End();
	m_bitmap = nullptr;
	return true;
}

void DC::DeleteDC()
{
	End();
	m_bitmap = nullptr;
}

Bitmap *DC::SelectObject(Bitmap *bitmap)
{
	Bitmap *old = m_bitmap;
	End();
	m_bitmap = bitmap;
	if(bitmap && !bitmap->image.isNull())
		Begin(&bitmap->image);
	return old;
}

void DC::ApplyPen()
{
	if(!m_painter)
		return;
	Pen p = m_pen;
	if(m_dcPen)
		p = Pen(pen::Solid, 1, m_dcPenColor);
	m_painter->setPen(p.ToQPen());
}

void DC::ApplyBrush()
{
	if(!m_painter)
		return;
	if(m_dcBrush)
		m_painter->setBrush(ToQColor(m_dcBrushColor));
	else
		m_painter->setBrush(m_brush.ToQBrush());
}

Pen DC::SelectObject(const Pen &p)
{
	Pen old = m_pen;
	m_pen = p;
	m_dcPen = false;
	ApplyPen();
	return old;
}

Brush DC::SelectObject(const Brush &b)
{
	Brush old = m_brush;
	m_brush = b;
	m_dcBrush = false;
	ApplyBrush();
	return old;
}

QFont DC::SelectObject(const QFont &f)
{
	QFont old = m_font;
	m_font = f;
	if(m_painter)
		m_painter->setFont(f);
	return old;
}

void DC::SelectStockObject(Stock object)
{
	switch(object)
	{
	case Stock::NullPen: SelectObject(Pen(pen::Null, 0, 0)); break;
	case Stock::BlackPen: SelectObject(Pen(pen::Solid, 1, 0)); break;
	case Stock::WhitePen: SelectObject(Pen(pen::Solid, 1, 0xFFFFFF)); break;
	case Stock::DCPen: m_dcPen = true; ApplyPen(); break;
	case Stock::NullBrush: SelectObject(Brush::Null()); break;
	case Stock::BlackBrush: SelectObject(Brush(0)); break;
	case Stock::WhiteBrush: SelectObject(Brush(0xFFFFFF)); break;
	case Stock::GrayBrush: SelectObject(Brush(0x808080)); break;
	case Stock::DCBrush: m_dcBrush = true; ApplyBrush(); break;
	case Stock::DefaultGuiFont: SelectObject(QApplication::font()); break;
	}
}

uint32_t DC::SetTextColor(uint32_t color)
{
	std::swap(color, m_textColor);
	return color;
}

uint32_t DC::SetBkColor(uint32_t color)
{
	std::swap(color, m_bkColor);
	return color;
}

int DC::SetBkMode(int mode)
{
	std::swap(mode, m_bkMode);
	return mode;
}

uint32_t DC::SetDCPenColor(uint32_t color)
{
	std::swap(color, m_dcPenColor);
	if(m_dcPen)
		ApplyPen();
	return color;
}

uint32_t DC::SetDCBrushColor(uint32_t color)
{
	std::swap(color, m_dcBrushColor);
	if(m_dcBrush)
		ApplyBrush();
	return color;
}

Point DC::MoveTo(int x, int y)
{
	Point old = m_pos;
	m_pos = Point(x, y);
	return old;
}

bool DC::LineTo(int x, int y)
{
	if(m_painter && m_painter->pen().style() != Qt::NoPen)
	{
		// Windows leaves out the end point.
		int ex = x, ey = y;
		if(m_pos.y == y && m_pos.x != x)
			ex += (x > m_pos.x) ? -1 : 1;
		else if(m_pos.x == x && m_pos.y != y)
			ey += (y > m_pos.y) ? -1 : 1;
		if(m_pos.x != x || m_pos.y != y)
			m_painter->drawLine(m_pos.x, m_pos.y, ex, ey);
	}
	m_pos = Point(x, y);
	return true;
}

bool DC::Polyline(const Point *points, int count)
{
	if(!m_painter || count < 2)
		return false;
	std::vector<QPoint> pts(points, points + count);
	m_painter->drawPolyline(pts.data(), count);
	return true;
}

bool DC::Polygon(const Point *points, int count)
{
	if(!m_painter || count < 2)
		return false;
	std::vector<QPoint> pts(points, points + count);
	m_painter->drawPolygon(pts.data(), count);
	return true;
}

bool DC::Rectangle(int l, int t, int r, int b)
{
	if(!m_painter)
		return false;
	// The outline is inside the rectangle.
	if(m_painter->pen().style() == Qt::NoPen)
		m_painter->fillRect(QRect(l, t, r - l - 1, b - t - 1), m_painter->brush());
	else
		m_painter->drawRect(QRect(l, t, r - l - 1, b - t - 1));
	return true;
}

bool DC::Ellipse(int l, int t, int r, int b)
{
	if(!m_painter)
		return false;
	m_painter->drawEllipse(QRect(l, t, r - l - 1, b - t - 1));
	return true;
}

bool DC::RoundRect(const Rect &r, Point corner)
{
	if(!m_painter)
		return false;
	m_painter->drawRoundedRect(QRect(r.left, r.top, r.Width() - 1, r.Height() - 1), corner.x / 2.0, corner.y / 2.0);
	return true;
}

void DC::SetPixel(int x, int y, uint32_t color)
{
	if(m_painter)
		m_painter->fillRect(x, y, 1, 1, ToQColor(color));
}

void DC::FillSolidRect(const Rect &r, uint32_t color)
{
	m_bkColor = color;
	if(m_painter && !r.IsRectEmpty())
		m_painter->fillRect(QRect(r), ToQColor(color));
}

void DC::FillRect(const Rect &r, const Brush *brush)
{
	if(m_painter && brush && !brush->null && !r.IsRectEmpty())
		m_painter->fillRect(QRect(r), ToQColor(brush->color));
}

void DC::FrameRect(const Rect &r, const Brush *brush)
{
	if(!m_painter || !brush || brush->null || r.IsRectEmpty())
		return;
	const QColor c = ToQColor(brush->color);
	m_painter->fillRect(r.left, r.top, r.Width(), 1, c);
	m_painter->fillRect(r.left, r.bottom - 1, r.Width(), 1, c);
	m_painter->fillRect(r.left, r.top, 1, r.Height(), c);
	m_painter->fillRect(r.right - 1, r.top, 1, r.Height(), c);
}

void DC::Draw3dRect(const Rect &r, uint32_t topLeft, uint32_t bottomRight)
{
	if(!m_painter || r.IsRectEmpty())
		return;
	const QColor tl = ToQColor(topLeft), br = ToQColor(bottomRight);
	m_painter->fillRect(r.left, r.top, r.Width() - 1, 1, tl);
	m_painter->fillRect(r.left, r.top, 1, r.Height() - 1, tl);
	m_painter->fillRect(r.right - 1, r.top, 1, r.Height(), br);
	m_painter->fillRect(r.left, r.bottom - 1, r.Width(), 1, br);
}

bool DC::DrawEdge(Rect *r, uint32_t edgeType, uint32_t flags)
{
	if(!m_painter || !r)
		return false;
	const QPalette pal = QApplication::palette();
	const uint32_t light = FromQColor(pal.color(QPalette::Light)), highlight = FromQColor(pal.color(QPalette::Midlight));
	const uint32_t shadow = FromQColor(pal.color(QPalette::Mid)), dark = FromQColor(pal.color(QPalette::Dark));
	auto line = [&](Rect &rc, uint32_t tl, uint32_t br)
	{
		const QColor ctl = ToQColor(tl), cbr = ToQColor(br);
		if(flags & edge::Top)
			m_painter->fillRect(rc.left, rc.top, rc.Width(), 1, ctl);
		if(flags & edge::Left)
			m_painter->fillRect(rc.left, rc.top, 1, rc.Height(), ctl);
		if(flags & edge::Bottom)
			m_painter->fillRect(rc.left, rc.bottom - 1, rc.Width(), 1, cbr);
		if(flags & edge::Right)
			m_painter->fillRect(rc.right - 1, rc.top, 1, rc.Height(), cbr);
		rc.DeflateRect((flags & edge::Left) ? 1 : 0, (flags & edge::Top) ? 1 : 0, (flags & edge::Right) ? 1 : 0, (flags & edge::Bottom) ? 1 : 0);
	};
	Rect rc = *r;
	if(flags & edge::Flat)
	{
		line(rc, shadow, shadow);
	} else
	{
		if(edgeType & edge::RaisedOuter)
			line(rc, highlight, dark);
		else if(edgeType & edge::SunkenOuter)
			line(rc, shadow, light);
		if(edgeType & edge::RaisedInner)
			line(rc, light, shadow);
		else if(edgeType & edge::SunkenInner)
			line(rc, dark, highlight);
	}
	if(flags & edge::Middle)
		m_painter->fillRect(QRect(rc), pal.color(QPalette::Button));
	if(flags & edge::Adjust)
		*r = rc;
	return true;
}

void DC::DrawFocusRect(const Rect &r)
{
	if(!m_painter || r.IsRectEmpty())
		return;
	m_painter->save();
	QPen p(Qt::white);
	p.setStyle(Qt::DotLine);
	p.setCosmetic(true);
	m_painter->setPen(p);
	m_painter->setBrush(Qt::NoBrush);
	m_painter->setCompositionMode(QPainter::RasterOp_SourceXorDestination);
	m_painter->drawRect(QRect(r.left, r.top, r.Width() - 1, r.Height() - 1));
	m_painter->restore();
}

void DC::InvertRect(const Rect &r)
{
	if(!m_painter || r.IsRectEmpty())
		return;
	m_painter->save();
	m_painter->setCompositionMode(QPainter::RasterOp_SourceXorDestination);
	m_painter->fillRect(QRect(r), Qt::white);
	m_painter->restore();
}

bool DC::PatBlt(int x, int y, int width, int height, uint32_t rasterOp)
{
	if(!m_painter)
		return false;
	const QRect rc(x, y, width, height);
	const QColor brush = ToQColor(m_dcBrush ? m_dcBrushColor : m_brush.color);
	switch(rasterOp)
	{
	case rop::Blackness: m_painter->fillRect(rc, Qt::black); break;
	case rop::Whiteness: m_painter->fillRect(rc, Qt::white); break;
	case rop::DstInvert: InvertRect(Rect(rc)); break;
	case rop::PatInvert:
		m_painter->save();
		m_painter->setCompositionMode(QPainter::RasterOp_SourceXorDestination);
		m_painter->fillRect(rc, brush);
		m_painter->restore();
		break;
	default: m_painter->fillRect(rc, brush); break;
	}
	return true;
}

namespace
{

QPainter::CompositionMode ModeFor(uint32_t rasterOp)
{
	switch(rasterOp)
	{
	case rop::SrcPaint: return QPainter::RasterOp_SourceOrDestination;
	case rop::SrcAnd: return QPainter::RasterOp_SourceAndDestination;
	case rop::SrcInvert: return QPainter::RasterOp_SourceXorDestination;
	case rop::NotSrcCopy: return QPainter::RasterOp_NotSource;
	default: return QPainter::CompositionMode_Source;
	}
}

} // namespace

bool DC::BitBlt(int x, int y, int width, int height, DC *source, int xSrc, int ySrc, uint32_t rasterOp)
{
	return StretchBlt(x, y, width, height, source, xSrc, ySrc, width, height, rasterOp);
}

bool DC::StretchBlt(int x, int y, int width, int height, DC *source, int xSrc, int ySrc, int srcWidth, int srcHeight, uint32_t rasterOp)
{
	if(!m_painter)
		return false;
	if(rasterOp == rop::Blackness || rasterOp == rop::Whiteness || rasterOp == rop::DstInvert || rasterOp == rop::PatCopy || rasterOp == rop::PatInvert)
		return PatBlt(x, y, width, height, rasterOp);
	if(!source || !source->m_bitmap || source->m_bitmap->image.isNull())
		return false;
	// The source may be drawn on still.
	const QImage &img = source->m_bitmap->image;
	m_painter->save();
	m_painter->setCompositionMode(ModeFor(rasterOp));
	m_painter->drawImage(QRect(x, y, width, height), img, QRect(xSrc, ySrc, srcWidth, srcHeight));
	m_painter->restore();
	return true;
}

bool DC::TransparentBlt(int x, int y, int width, int height, DC *source, int xSrc, int ySrc, int srcWidth, int srcHeight, uint32_t transparent)
{
	if(!m_painter || !source || !source->m_bitmap || source->m_bitmap->image.isNull())
		return false;
	QImage part = source->m_bitmap->image.copy(xSrc, ySrc, srcWidth, srcHeight).convertToFormat(QImage::Format_ARGB32);
	const QRgb key = ToQColor(transparent).rgb() & 0xFFFFFF;
	for(int row = 0; row < part.height(); row++)
	{
		auto *line = reinterpret_cast<QRgb *>(part.scanLine(row));
		for(int col = 0; col < part.width(); col++)
		{
			if((line[col] & 0xFFFFFF) == key)
				line[col] = 0;
		}
	}
	m_painter->drawImage(QRect(x, y, width, height), part);
	return true;
}

void DC::DrawImage(int x, int y, const QImage &image)
{
	if(m_painter)
		m_painter->drawImage(x, y, image);
}

void DC::DrawImage(const Rect &target, const QImage &image)
{
	if(m_painter)
		m_painter->drawImage(QRect(target), image);
}

bool DC::TextOut(int x, int y, const Text &text)
{
	if(!m_painter)
		return false;
	const QFontMetrics fm(m_font);
	const QRect rc(x, y, fm.horizontalAdvance(text.value), fm.height());
	if(m_bkMode == bk::Opaque)
		m_painter->fillRect(rc, ToQColor(m_bkColor));
	m_painter->save();
	m_painter->setPen(ToQColor(m_textColor));
	m_painter->drawText(QPoint(x, y + fm.ascent()), text.value);
	m_painter->restore();
	return true;
}

bool DC::ExtTextOut(int x, int y, uint32_t options, const Rect *r, const Text &text)
{
	if(!m_painter)
		return false;
	if(r && (options & eto::Opaque))
		m_painter->fillRect(QRect(*r), ToQColor(m_bkColor));
	m_painter->save();
	if(r && (options & eto::Clipped))
		m_painter->setClipRect(QRect(*r), Qt::IntersectClip);
	const QFontMetrics fm(m_font);
	m_painter->setPen(ToQColor(m_textColor));
	if(!(options & eto::Opaque) && m_bkMode == bk::Opaque)
		m_painter->fillRect(QRect(x, y, fm.horizontalAdvance(text.value), fm.height()), ToQColor(m_bkColor));
	m_painter->drawText(QPoint(x, y + fm.ascent()), text.value);
	m_painter->restore();
	return true;
}

int DC::DrawText(const Text &text, Rect *r, uint32_t format)
{
	if(!r)
		return 0;
	int flags = 0;
	if(format & dt::Center)
		flags |= Qt::AlignHCenter;
	else if(format & dt::Right)
		flags |= Qt::AlignRight;
	else
		flags |= Qt::AlignLeft;
	if(format & dt::SingleLine)
	{
		if(format & dt::VCenter)
			flags |= Qt::AlignVCenter;
		else if(format & dt::Bottom)
			flags |= Qt::AlignBottom;
		else
			flags |= Qt::AlignTop;
	} else
	{
		flags |= Qt::AlignTop;
	}
	if(format & dt::WordBreak)
		flags |= Qt::TextWordWrap;
	if(format & dt::ExpandTabs)
		flags |= Qt::TextExpandTabs;
	if(format & dt::NoClip)
		flags |= Qt::TextDontClip;
	if(!(format & dt::SingleLine) && !(format & dt::WordBreak))
		flags |= Qt::TextDontClip;
	if(format & dt::HidePrefix)
		flags |= Qt::TextHideMnemonic;
	else if(!(format & dt::NoPrefix))
		flags |= Qt::TextShowMnemonic;
	QString s = text.value;
	const QFontMetrics fm(m_font);
	if(format & (dt::EndEllipsis | dt::PathEllipsis | dt::WordEllipsis))
		s = fm.elidedText(s, (format & dt::PathEllipsis) ? Qt::ElideMiddle : Qt::ElideRight, std::max(0, r->Width()), flags & Qt::TextShowMnemonic);
	if(format & dt::CalcRect)
	{
		const int width = (format & dt::WordBreak) ? std::max(1, r->Width()) : 100000;
		const QRect bounds = fm.boundingRect(QRect(r->left, r->top, width, 100000), flags & ~(Qt::AlignHCenter | Qt::AlignRight | Qt::AlignVCenter | Qt::AlignBottom), s);
		r->right = r->left + bounds.width();
		r->bottom = r->top + bounds.height();
		return bounds.height();
	}
	if(!m_painter)
		return 0;
	const QRect rc(*r);
	if(m_bkMode == bk::Opaque)
	{
		const QRect bounds = fm.boundingRect(rc, flags, s);
		m_painter->fillRect(bounds & rc, ToQColor(m_bkColor));
	}
	m_painter->save();
	m_painter->setPen(ToQColor(m_textColor));
	QRect drawn;
	m_painter->drawText(rc, flags, s, &drawn);
	m_painter->restore();
	return drawn.height();
}

Size DC::GetTextExtent(const Text &text) const
{
	const QFontMetrics fm(m_font);
	return Size(fm.horizontalAdvance(text.value), fm.height());
}

int DC::GetTextHeight() const
{
	return QFontMetrics(m_font).height();
}

int DC::IntersectClipRect(const Rect &r)
{
	if(!m_painter)
		return 0;
	if(m_painter->hasClipping())
		m_painter->setClipRect(QRect(r), Qt::IntersectClip);
	else
		m_painter->setClipRect(QRect(r));
	return r.IsRectEmpty() ? 1 : 2;  // NULLREGION, SIMPLEREGION
}

int DC::SelectClipRect(const Rect *r)
{
	if(!m_painter)
		return 0;
	if(r)
		m_painter->setClipRect(QRect(*r));
	else
		m_painter->setClipping(false);
	return 2;
}

Rect DC::GetClipBox() const
{
	if(!m_painter)
		return Rect();
	if(m_painter->hasClipping())
		return Rect(m_painter->clipBoundingRect().toAlignedRect());
	if(m_device)
		return Rect(0, 0, m_device->width(), m_device->height());
	return Rect();
}

bool DC::RectVisible(const Rect &r) const
{
	Rect clip = GetClipBox();
	Rect isect;
	return isect.IntersectRect(clip, r);
}

int DC::SaveDC()
{
	if(m_painter)
		m_painter->save();
	m_saved.push_back({m_pen, m_brush, m_font, m_textColor, m_bkColor, m_bkMode});
	return static_cast<int>(m_saved.size());
}

bool DC::RestoreDC(int saved)
{
	if(saved < 0)
		saved = static_cast<int>(m_saved.size()) + 1 + saved;
	if(saved <= 0 || saved > static_cast<int>(m_saved.size()))
		return false;
	while(static_cast<int>(m_saved.size()) >= saved)
	{
		const Saved s = m_saved.back();
		m_saved.pop_back();
		if(m_painter)
			m_painter->restore();
		m_pen = s.pen;
		m_brush = s.brush;
		m_font = s.font;
		m_textColor = s.textColor;
		m_bkColor = s.bkColor;
		m_bkMode = s.bkMode;
	}
	ApplyPen();
	ApplyBrush();
	if(m_painter)
		m_painter->setFont(m_font);
	return true;
}


static QWidget *PaintTarget(QWidget *widget)
{
	if(auto *area = qobject_cast<QAbstractScrollArea *>(widget))
		return area->viewport();
	return widget;
}

PaintDC::PaintDC(QWidget *widget)
	: DC(PaintTarget(widget))
{
	if(QWidget *target = PaintTarget(widget))
	{
		m_font = target->font();
		if(m_painter)
			m_painter->setFont(m_font);
		rcPaint = Rect(target->rect());
	}
}


int DC::DrawDib(int x, int y, int width, int height, int xSrc, int ySrc, int startScan, int scanCount, const void *bits, const DibHeader &header, const DibColor *colors)
{
	if(!m_painter || !bits || !colors || (header.biBitCount != 4 && header.biBitCount != 8))
		return 0;
	const int dibWidth = header.biWidth;
	const int dibHeight = std::abs(header.biHeight);
	const bool bottomUp = header.biHeight > 0;
	const int stride = ((dibWidth * header.biBitCount + 31) / 32) * 4;
	width = std::min(width, dibWidth - xSrc);
	if(width <= 0 || height <= 0)
		return 0;
	const int paletteSize = header.biClrUsed ? static_cast<int>(header.biClrUsed) : (1 << header.biBitCount);
	QList<QRgb> table(256, qRgb(0, 0, 0));
	for(int i = 0; i < paletteSize && i < 256; i++)
		table[i] = qRgb(colors[i].rgbRed, colors[i].rgbGreen, colors[i].rgbBlue);
	QImage image(width, height, QImage::Format_Indexed8);
	image.setColorTable(table);
	const auto *src = static_cast<const uint8_t *>(bits);
	for(int row = 0; row < height; row++)
	{
		// The line of the bitmap shown at row (from the top) of the target.
		const int dibLine = bottomUp ? (ySrc + height - 1 - row) : (ySrc + row);
		uint8_t *out = image.scanLine(row);
		const int scan = dibLine - startScan;
		if(dibLine < 0 || dibLine >= dibHeight || scan < 0 || scan >= scanCount)
		{
			std::fill(out, out + width, uint8_t(0));
			continue;
		}
		const uint8_t *line = src + static_cast<std::ptrdiff_t>(scan) * stride;
		if(header.biBitCount == 8)
		{
			std::copy(line + xSrc, line + xSrc + width, out);
		} else
		{
			for(int i = 0; i < width; i++)
			{
				const int px = xSrc + i;
				const uint8_t b = line[px >> 1];
				out[i] = (px & 1) ? (b & 0x0F) : (b >> 4);
			}
		}
	}
	m_painter->drawImage(QPoint(x, y), image);
	return height;
}

} // namespace mfcless
