// Image lists (HIMAGELIST): equal-sized images kept with their transparency,
// drawn into DCs, as comctl32's ImageList_* functions do.
#include "QtWin.h"

#include <QtGui/QPainter>

#include <algorithm>
#include <vector>

namespace linopenmpt::qtwin
{

namespace
{

struct ImageList
{
	int cx = 16;
	int cy = 16;
	UINT flags = 0;
	int grow = 4;
	COLORREF bkColor = CLR_NONE;
	std::vector<QImage> images; // ARGB32 premultiplied, cx by cy
};

ImageList *ToList(HIMAGELIST list)
{
	return reinterpret_cast<ImageList *>(list);
}

// A bitmap's pixels as Windows keeps them: with ILC_COLOR32, an alpha
// channel is used when any pixel has one.
QImage SourceImage(const ImageList &list, HBITMAP bitmap)
{
	QImage image = BitmapImage(bitmap);
	if(image.isNull())
		return image;
	if((list.flags & 0xFE) == ILC_COLOR32 && image.depth() == 32)
	{
		bool alpha = false;
		for(int y = 0; y < image.height() && !alpha; y++)
		{
			const auto *line = reinterpret_cast<const quint32 *>(image.constScanLine(y));
			for(int x = 0; x < image.width(); x++)
			{
				if((line[x] >> 24) != 0 && (line[x] >> 24) != 0xFF)
				{
					alpha = true;
					break;
				}
			}
		}
		if(alpha)
		{
			QImage raw(image.constBits(), image.width(), image.height(), image.bytesPerLine(), QImage::Format_ARGB32);
			return raw.convertToFormat(QImage::Format_ARGB32_Premultiplied);
		}
	}
	image = image.convertToFormat(QImage::Format_ARGB32);
	for(int y = 0; y < image.height(); y++)
	{
		auto *line = reinterpret_cast<quint32 *>(image.scanLine(y));
		for(int x = 0; x < image.width(); x++)
			line[x] |= 0xFF000000u;
	}
	return image;
}

// Splits a strip of images, applying a mask (1 = transparent) or a mask colour.
int AddStrip(ImageList &list, QImage image, const QImage *mask, const COLORREF *maskColor, int at = -1)
{
	if(image.isNull() || list.cx <= 0)
		return -1;
	image = image.convertToFormat(QImage::Format_ARGB32);
	if(list.flags & ILC_MASK)
	{
		const QRgb key = maskColor != nullptr ? (ToQColor(*maskColor).rgb() & 0xFFFFFFu) : 0;
		for(int y = 0; y < image.height(); y++)
		{
			auto *line = reinterpret_cast<quint32 *>(image.scanLine(y));
			for(int x = 0; x < image.width(); x++)
			{
				bool transparent = false;
				if(mask != nullptr && x < mask->width() && y < mask->height())
					transparent = (mask->pixel(x, y) & 0xFFFFFFu) != 0;
				else if(maskColor != nullptr)
					transparent = (line[x] & 0xFFFFFFu) == key;
				if(transparent)
					line[x] = 0;
			}
		}
	}
	image = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
	const int count = std::max(1, image.width() / list.cx);
	int first = -1;
	for(int i = 0; i < count; i++)
	{
		QImage part(list.cx, list.cy, QImage::Format_ARGB32_Premultiplied);
		part.fill(Qt::transparent);
		{
			QPainter p(&part);
			p.setCompositionMode(QPainter::CompositionMode_Source);
			p.drawImage(0, 0, image, i * list.cx, 0, list.cx, list.cy);
		}
		if(at >= 0 && at < static_cast<int>(list.images.size()))
		{
			list.images[at] = part;
			return at;
		}
		list.images.push_back(part);
		if(first < 0)
			first = static_cast<int>(list.images.size()) - 1;
	}
	return first;
}

QImage MaskImage(HBITMAP mask)
{
	if(mask == nullptr)
		return QImage();
	return BitmapImage(mask).convertToFormat(QImage::Format_RGB32);
}

} // namespace

QImage ImageListImage(HIMAGELIST list, int index)
{
	const ImageList *l = ToList(list);
	if(l == nullptr || index < 0 || index >= static_cast<int>(l->images.size()))
		return QImage();
	return l->images[static_cast<std::size_t>(index)];
}

} // namespace linopenmpt::qtwin

using namespace linopenmpt::qtwin;

HIMAGELIST ImageList_Create(int cx, int cy, UINT flags, int initial, int grow)
{
	if(cx <= 0 || cy <= 0)
		return nullptr;
	auto *list = new ImageList;
	list->cx = cx;
	list->cy = cy;
	list->flags = flags;
	list->grow = std::max(grow, 1);
	list->images.reserve(static_cast<std::size_t>(std::max(initial, 0)));
	return reinterpret_cast<HIMAGELIST>(list);
}

BOOL ImageList_Destroy(HIMAGELIST list)
{
	if(list == nullptr)
		return FALSE;
	delete ToList(list);
	return TRUE;
}

HIMAGELIST ImageList_Duplicate(HIMAGELIST list)
{
	if(list == nullptr)
		return nullptr;
	return reinterpret_cast<HIMAGELIST>(new ImageList(*ToList(list)));
}

int ImageList_Add(HIMAGELIST list, HBITMAP image, HBITMAP mask)
{
	ImageList *l = ToList(list);
	if(l == nullptr)
		return -1;
	const QImage m = MaskImage(mask);
	return AddStrip(*l, SourceImage(*l, image), mask != nullptr ? &m : nullptr, nullptr);
}

int ImageList_AddMasked(HIMAGELIST list, HBITMAP image, COLORREF mask)
{
	ImageList *l = ToList(list);
	if(l == nullptr)
		return -1;
	return AddStrip(*l, SourceImage(*l, image), nullptr, &mask);
}

BOOL ImageList_Replace(HIMAGELIST list, int index, HBITMAP image, HBITMAP mask)
{
	ImageList *l = ToList(list);
	if(l == nullptr || index < 0 || index >= static_cast<int>(l->images.size()))
		return FALSE;
	const QImage m = MaskImage(mask);
	return AddStrip(*l, SourceImage(*l, image), mask != nullptr ? &m : nullptr, nullptr, index) == index;
}

int ImageList_ReplaceIcon(HIMAGELIST list, int index, HICON icon)
{
	ImageList *l = ToList(list);
	const auto *data = reinterpret_cast<const IconData *>(icon);
	if(l == nullptr || data == nullptr)
		return -1;
	QImage part(l->cx, l->cy, QImage::Format_ARGB32_Premultiplied);
	part.fill(Qt::transparent);
	{
		QPainter p(&part);
		p.setRenderHint(QPainter::SmoothPixmapTransform);
		QImage source = data->image;
		source.setDevicePixelRatio(1.0);
		p.drawImage(QRect(0, 0, l->cx, l->cy), source);
	}
	if(index >= 0 && index < static_cast<int>(l->images.size()))
	{
		l->images[static_cast<std::size_t>(index)] = part;
		return index;
	}
	if(index != -1)
		return -1;
	l->images.push_back(part);
	return static_cast<int>(l->images.size()) - 1;
}

int ImageList_GetImageCount(HIMAGELIST list)
{
	const ImageList *l = ToList(list);
	return l != nullptr ? static_cast<int>(l->images.size()) : 0;
}

BOOL ImageList_SetImageCount(HIMAGELIST list, UINT count)
{
	ImageList *l = ToList(list);
	if(l == nullptr)
		return FALSE;
	QImage blank(l->cx, l->cy, QImage::Format_ARGB32_Premultiplied);
	blank.fill(Qt::transparent);
	l->images.resize(count, blank);
	return TRUE;
}

BOOL ImageList_Remove(HIMAGELIST list, int index)
{
	ImageList *l = ToList(list);
	if(l == nullptr)
		return FALSE;
	if(index == -1)
	{
		l->images.clear();
		return TRUE;
	}
	if(index < 0 || index >= static_cast<int>(l->images.size()))
		return FALSE;
	l->images.erase(l->images.begin() + index);
	return TRUE;
}

BOOL ImageList_SetIconSize(HIMAGELIST list, int cx, int cy)
{
	ImageList *l = ToList(list);
	if(l == nullptr || cx <= 0 || cy <= 0)
		return FALSE;
	// As on Windows, the images go.
	l->cx = cx;
	l->cy = cy;
	l->images.clear();
	return TRUE;
}

BOOL ImageList_GetIconSize(HIMAGELIST list, int *cx, int *cy)
{
	const ImageList *l = ToList(list);
	if(l == nullptr)
		return FALSE;
	if(cx != nullptr)
		*cx = l->cx;
	if(cy != nullptr)
		*cy = l->cy;
	return TRUE;
}

COLORREF ImageList_SetBkColor(HIMAGELIST list, COLORREF color)
{
	ImageList *l = ToList(list);
	if(l == nullptr)
		return CLR_NONE;
	const COLORREF old = l->bkColor;
	l->bkColor = color;
	return old;
}

COLORREF ImageList_GetBkColor(HIMAGELIST list)
{
	const ImageList *l = ToList(list);
	return l != nullptr ? l->bkColor : CLR_NONE;
}

BOOL ImageList_DrawEx(HIMAGELIST list, int index, HDC dc, int x, int y, int dx, int dy, COLORREF bk, COLORREF fg, UINT style)
{
	const ImageList *l = ToList(list);
	if(l == nullptr || index < 0 || index >= static_cast<int>(l->images.size()))
		return FALSE;
	QPainter *p = DCPainter(dc);
	if(p == nullptr)
		return FALSE;
	const int w = dx > 0 ? std::min(dx, l->cx) : l->cx;
	const int h = dy > 0 ? std::min(dy, l->cy) : l->cy;
	const QImage &image = l->images[static_cast<std::size_t>(index)];
	p->save();
	if(style & ILD_MASK)
	{
		// The mask: black where the image is, white where it is transparent.
		QImage mask(w, h, QImage::Format_RGB32);
		for(int yy = 0; yy < h; yy++)
		{
			for(int xx = 0; xx < w; xx++)
				mask.setPixel(xx, yy, qAlpha(image.pixel(xx, yy)) >= 128 ? qRgb(0, 0, 0) : qRgb(255, 255, 255));
		}
		p->drawImage(x, y, mask);
		p->restore();
		return TRUE;
	}
	if(bk == CLR_DEFAULT)
		bk = l->bkColor;
	if(!(style & ILD_TRANSPARENT) && bk != CLR_NONE)
		p->fillRect(QRect(x, y, w, h), ToQColor(bk));
	p->drawImage(QPoint(x, y), image, QRect(0, 0, w, h));
	if(style & (ILD_BLEND25 | ILD_BLEND50))
	{
		// Selected or focused: tinted with the highlight (or fg) colour where the image is.
		QColor tint = ToQColor(fg == CLR_DEFAULT || fg == CLR_NONE ? ::GetSysColor(COLOR_HIGHLIGHT) : fg);
		tint.setAlphaF((style & ILD_BLEND50) ? 0.5 : 0.25);
		QImage tinted(image.copy(0, 0, w, h));
		{
			QPainter tp(&tinted);
			tp.setCompositionMode(QPainter::CompositionMode_SourceAtop);
			tp.fillRect(tinted.rect(), tint);
		}
		p->drawImage(x, y, tinted);
	}
	p->restore();
	return TRUE;
}

BOOL ImageList_Draw(HIMAGELIST list, int index, HDC dc, int x, int y, UINT style)
{
	return ImageList_DrawEx(list, index, dc, x, y, 0, 0, CLR_DEFAULT, CLR_DEFAULT, style);
}

HICON ImageList_GetIcon(HIMAGELIST list, int index, UINT)
{
	const QImage image = ImageListImage(list, index);
	if(image.isNull())
		return nullptr;
	auto *icon = new IconData;
	icon->image = image;
	return reinterpret_cast<HICON>(icon);
}

HIMAGELIST ImageList_LoadImage(HINSTANCE instance, LPCTSTR name, int cx, int grow, COLORREF mask, UINT type, UINT flags)
{
	if(type != IMAGE_BITMAP)
		return nullptr;
	HBITMAP bitmap = static_cast<HBITMAP>(::LoadImage(instance, name, IMAGE_BITMAP, 0, 0, flags));
	if(bitmap == nullptr)
		return nullptr;
	BITMAP bm{};
	::GetObject(bitmap, sizeof(bm), &bm);
	const int width = cx > 0 ? cx : bm.bmHeight;
	HIMAGELIST list = ImageList_Create(width, bm.bmHeight, (mask != CLR_NONE ? ILC_MASK : 0) | ILC_COLOR32, std::max(1, bm.bmWidth / std::max(width, 1)), grow);
	if(mask != CLR_NONE)
		ImageList_AddMasked(list, bitmap, mask);
	else
		ImageList_Add(list, bitmap, nullptr);
	::DeleteObject(bitmap);
	return list;
}
