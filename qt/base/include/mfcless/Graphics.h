/*
 * Graphics.h
 * ----------
 * Purpose: Drawing as upstream's views and controls draw: a device context
 *          (DC) with a current pen, brush, font, text colours and position,
 *          on top of QPainter, so that their drawing code stays as it is.
 * Notes  : Colours are 0x00BBGGRR (mfcless::Rgb). Rectangles end before
 *          right and bottom, and lines before their end point, as on
 *          Windows.
 *          Qt draws on a widget only while it paints (its paint event, which
 *          calls OnPaint): a PaintDC is made there. Drawing at other times
 *          goes to an off-screen bitmap (a memory DC) or asks for a repaint.
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */

#pragma once

#include "mfcless/Geometry.h"
#include "mfcless/Text.h"

#include <QtGui/QColor>
#include <QtCore/QPointer>
#include <QtGui/QFont>
#include <QtGui/QImage>
#include <QtGui/QPainter>

#include <cstdint>
#include <memory>
#include <vector>

class QPaintDevice;
class QWidget;

namespace mfcless
{

inline QColor ToQColor(uint32_t c)
{
	return QColor(static_cast<int>(c & 0xFF), static_cast<int>((c >> 8) & 0xFF), static_cast<int>((c >> 16) & 0xFF));
}
inline uint32_t FromQColor(const QColor &c)
{
	return static_cast<uint32_t>(c.red()) | (static_cast<uint32_t>(c.green()) << 8) | (static_cast<uint32_t>(c.blue()) << 16);
}

// Pen styles.
namespace pen
{
	inline constexpr int Solid = 0, Dash = 1, Dot = 2, DashDot = 3, DashDotDot = 4, Null = 5, InsideFrame = 6;
}
// Background modes (SetBkMode).
namespace bk
{
	inline constexpr int Transparent = 1, Opaque = 2;
}
// DrawText formats.
namespace dt
{
	inline constexpr uint32_t Top = 0x0, Left = 0x0, Center = 0x1, Right = 0x2, VCenter = 0x4, Bottom = 0x8,
		WordBreak = 0x10, SingleLine = 0x20, ExpandTabs = 0x40, TabStop = 0x80, NoClip = 0x100, ExternalLeading = 0x200,
		CalcRect = 0x400, NoPrefix = 0x800, Internal = 0x1000, EditControl = 0x2000, PathEllipsis = 0x4000,
		EndEllipsis = 0x8000, ModifyString = 0x10000, RtlReading = 0x20000, WordEllipsis = 0x40000, HidePrefix = 0x100000;
}
// ExtTextOut options.
namespace eto
{
	inline constexpr uint32_t Opaque = 0x2, Clipped = 0x4;
}
// Raster operations of BitBlt and PatBlt.
namespace rop
{
	inline constexpr uint32_t SrcCopy = 0x00CC0020, SrcPaint = 0x00EE0086, SrcAnd = 0x008800C6, SrcInvert = 0x00660046,
		NotSrcCopy = 0x00330008, PatCopy = 0x00F00021, PatInvert = 0x005A0049, DstInvert = 0x00550009,
		Blackness = 0x00000042, Whiteness = 0x00FF0062;
}
// DrawEdge edges and borders.
namespace edge
{
	inline constexpr uint32_t RaisedOuter = 0x1, SunkenOuter = 0x2, RaisedInner = 0x4, SunkenInner = 0x8,
		Raised = RaisedOuter | RaisedInner, Sunken = SunkenOuter | SunkenInner, Etched = SunkenOuter | RaisedInner, Bump = RaisedOuter | SunkenInner,
		Left = 0x1, Top = 0x2, Right = 0x4, Bottom = 0x8, Rect = 0xF, Middle = 0x800, Soft = 0x1000, Adjust = 0x2000, Flat = 0x4000, Mono = 0x8000;
}

struct Pen
{
	int style = pen::Solid;
	int width = 1;
	uint32_t color = 0;

	Pen() = default;
	Pen(int penStyle, int penWidth, uint32_t penColor) : style(penStyle), width(penWidth), color(penColor) {}
	bool CreatePen(int penStyle, int penWidth, uint32_t penColor) { *this = Pen(penStyle, penWidth, penColor); return true; }
	void DeleteObject() { *this = Pen(); }
	QPen ToQPen() const;
};

struct Brush
{
	uint32_t color = 0xFFFFFF;
	bool null = false;

	Brush() = default;
	explicit Brush(uint32_t brushColor) : color(brushColor) {}
	bool CreateSolidBrush(uint32_t brushColor) { *this = Brush(brushColor); return true; }
	void DeleteObject() { *this = Brush(); }
	static Brush Null() { Brush b; b.null = true; return b; }
	QBrush ToQBrush() const { return null ? QBrush(Qt::NoBrush) : QBrush(ToQColor(color)); }
};

class DC;

// The header and palette entries of a device-independent bitmap, laid out
// as in BMP files and Windows' bitmap resources (so they can be copied from
// them). Rows run bottom-up if biHeight is positive.
struct DibHeader
{
	uint32_t biSize = 40;
	int32_t biWidth = 0;
	int32_t biHeight = 0;
	uint16_t biPlanes = 1;
	uint16_t biBitCount = 8;
	uint32_t biCompression = 0;
	uint32_t biSizeImage = 0;
	int32_t biXPelsPerMeter = 0;
	int32_t biYPelsPerMeter = 0;
	uint32_t biClrUsed = 0;
	uint32_t biClrImportant = 0;
};
static_assert(sizeof(DibHeader) == 40);
struct DibColor
{
	uint8_t rgbBlue = 0, rgbGreen = 0, rgbRed = 0, rgbReserved = 0;
};

// A bitmap to draw on (selected into a memory DC) or to draw from.
class Bitmap
{
public:
	Bitmap() = default;
	explicit Bitmap(QImage img) : image(std::move(img)) {}
	bool CreateCompatibleBitmap(DC *dc, int width, int height);
	bool CreateBitmap(int width, int height);
	void DeleteObject() { image = QImage(); }
	bool IsNull() const { return image.isNull(); }
	Size GetSize() const { return Size(image.width(), image.height()); }

	QImage image;
};

enum class Stock
{
	NullPen, BlackPen, WhitePen, DCPen,
	NullBrush, BlackBrush, WhiteBrush, GrayBrush, DCBrush,
	DefaultGuiFont,
};

class DC
{
public:
	// A memory DC (CreateCompatibleDC) until a bitmap is selected into it.
	DC();
	// Draws on a device (a widget while it paints, an image).
	explicit DC(QPaintDevice *device);
	// Draws with a painter that someone else owns.
	explicit DC(QPainter &painter);
	virtual ~DC();
	DC(const DC &) = delete;
	DC &operator=(const DC &) = delete;

	QPainter *GetPainter() const { return m_painter; }
	bool IsValid() const { return m_painter != nullptr; }
	explicit operator bool() const { return IsValid(); }

	// Memory DCs.
	bool CreateCompatibleDC(DC *other);
	void DeleteDC();
	// Selects a bitmap into a memory DC and draws on it from then on.
	// Returns the bitmap selected before.
	Bitmap *SelectObject(Bitmap *bitmap);
	Bitmap *GetSelectedBitmap() const { return m_bitmap; }

	Pen SelectObject(const Pen &p);
	Pen SelectObject(const Pen *p) { return SelectObject(p ? *p : Pen()); }
	Brush SelectObject(const Brush &b);
	Brush SelectObject(const Brush *b) { return SelectObject(b ? *b : Brush()); }
	QFont SelectObject(const QFont &f);
	QFont SelectObject(const QFont *f) { return SelectObject(f ? *f : QFont()); }
	void SelectStockObject(Stock object);
	const Pen &GetCurrentPen() const { return m_pen; }
	const Brush &GetCurrentBrush() const { return m_brush; }
	const QFont &GetCurrentFont() const { return m_font; }

	uint32_t SetTextColor(uint32_t color);
	uint32_t GetTextColor() const { return m_textColor; }
	uint32_t SetBkColor(uint32_t color);
	uint32_t GetBkColor() const { return m_bkColor; }
	int SetBkMode(int mode);
	int GetBkMode() const { return m_bkMode; }
	// The colours of the stock DC pen and brush.
	uint32_t SetDCPenColor(uint32_t color);
	uint32_t SetDCBrushColor(uint32_t color);

	Point MoveTo(int x, int y);
	Point MoveTo(Point p) { return MoveTo(p.x, p.y); }
	Point GetCurrentPosition() const { return m_pos; }
	bool LineTo(int x, int y);
	bool LineTo(Point p) { return LineTo(p.x, p.y); }
	bool Polyline(const Point *points, int count);
	bool Polygon(const Point *points, int count);
	bool Rectangle(int l, int t, int r, int b);
	bool Rectangle(const Rect &r) { return Rectangle(r.left, r.top, r.right, r.bottom); }
	bool Ellipse(int l, int t, int r, int b);
	bool Ellipse(const Rect &r) { return Ellipse(r.left, r.top, r.right, r.bottom); }
	bool RoundRect(const Rect &r, Point corner);
	void SetPixel(int x, int y, uint32_t color);
	void SetPixel(Point p, uint32_t color) { SetPixel(p.x, p.y, color); }

	// Also sets the background colour, as Windows' does.
	void FillSolidRect(const Rect &r, uint32_t color);
	void FillSolidRect(int x, int y, int cx, int cy, uint32_t color) { FillSolidRect(Rect(x, y, x + cx, y + cy), color); }
	void FillSolidRect(const Rect *r, uint32_t color) { if(r) FillSolidRect(*r, color); }
	void FillRect(const Rect &r, const Brush *brush);
	void FillRect(const Rect &r, const Brush &brush) { FillRect(r, &brush); }
	void FillRect(const Rect *r, const Brush &brush) { FillRect(*r, &brush); }
	void FillRect(const Rect *r, const Brush *brush) { FillRect(*r, brush); }
	void FrameRect(const Rect &r, const Brush *brush);
	void FrameRect(const Rect &r, const Brush &brush) { FrameRect(r, &brush); }
	void Draw3dRect(const Rect &r, uint32_t topLeft, uint32_t bottomRight);
	void Draw3dRect(int x, int y, int cx, int cy, uint32_t topLeft, uint32_t bottomRight) { Draw3dRect(Rect(x, y, x + cx, y + cy), topLeft, bottomRight); }
	bool DrawEdge(Rect *r, uint32_t edgeType, uint32_t flags);
	bool DrawEdge(Rect r, uint32_t edgeType, uint32_t flags) { return DrawEdge(&r, edgeType, flags); }
	void DrawFocusRect(const Rect &r);
	void DrawFocusRect(const Rect *r) { DrawFocusRect(*r); }
	void InvertRect(const Rect &r);
	bool PatBlt(int x, int y, int width, int height, uint32_t rasterOp);

	bool BitBlt(int x, int y, int width, int height, DC *source, int xSrc, int ySrc, uint32_t rasterOp = rop::SrcCopy);
	bool StretchBlt(int x, int y, int width, int height, DC *source, int xSrc, int ySrc, int srcWidth, int srcHeight, uint32_t rasterOp = rop::SrcCopy);
	// Pixels of the transparent colour are left out.
	bool TransparentBlt(int x, int y, int width, int height, DC *source, int xSrc, int ySrc, int srcWidth, int srcHeight, uint32_t transparent);
	void DrawImage(int x, int y, const QImage &image);
	// Draws scan lines of a 4- or 8-bit palette bitmap (SetDIBitsToDevice):
	// the part of width x height at xSrc, ySrc (ySrc counted from the
	// bottom line for a bottom-up bitmap), from bits with scanCount lines
	// that start at line startScan.
	int DrawDib(int x, int y, int width, int height, int xSrc, int ySrc, int startScan, int scanCount, const void *bits, const DibHeader &header, const DibColor *colors);
	void DrawImage(const Rect &target, const QImage &image);
	// StretchDIBits of a whole 4- or 8-bit palette bitmap's part.
	int StretchDib(int x, int y, int width, int height, int xSrc, int ySrc, int srcWidth, int srcHeight, const void *bits, const DibHeader &header, const DibColor *colors);

	bool TextOut(int x, int y, const Text &text);
	bool ExtTextOut(int x, int y, uint32_t options, const Rect *r, const Text &text);
	// Returns the height of the text; with dt::CalcRect, measures into r.
	int DrawText(const Text &text, Rect *r, uint32_t format);
	int DrawText(const Text &text, Rect r, uint32_t format) { return DrawText(text, &r, format); }
	// With a length (-1: all of it).
	int DrawText(const Text &text, int length, Rect *r, uint32_t format) { return DrawText(length < 0 ? text : Text(text.value.left(length)), r, format); }
	int DrawText(const Text &text, int length, const Rect &r, uint32_t format) { Rect rr = r; return DrawText(text, length, &rr, format); }
	Size GetTextExtent(const Text &text) const;
	int GetTextHeight() const;

	int IntersectClipRect(const Rect &r);
	int IntersectClipRect(const Rect *r) { return IntersectClipRect(*r); }
	int IntersectClipRect(int l, int t, int r, int b) { return IntersectClipRect(Rect(l, t, r, b)); }
	int SelectClipRect(const Rect *r);
	Rect GetClipBox() const;
	bool RectVisible(const Rect &r) const;

	int SaveDC();
	bool RestoreDC(int saved);

protected:
	void Begin(QPaintDevice *device);
	void End();
	void ApplyPen();
	void ApplyBrush();

	QPainter *m_painter = nullptr;
	std::unique_ptr<QPainter> m_ownPainter;
	Bitmap *m_bitmap = nullptr;
	QPaintDevice *m_device = nullptr;
	Pen m_pen;
	Brush m_brush;
	QFont m_font;
	uint32_t m_dcPenColor = 0, m_dcBrushColor = 0xFFFFFF;
	bool m_dcPen = false, m_dcBrush = false;
	uint32_t m_textColor = 0, m_bkColor = 0xFFFFFF;
	int m_bkMode = bk::Opaque;
	Point m_pos;
	struct Saved
	{
		Pen pen;
		Brush brush;
		QFont font;
		uint32_t textColor, bkColor;
		int bkMode;
	};
	std::vector<Saved> m_saved;
};

// FillRect(dc, rect, brush) in all the forms upstream writes it.
inline void FillRect(DC &dc, const Rect &r, const Brush &b) { dc.FillRect(r, &b); }
inline void FillRect(DC &dc, const Rect *r, const Brush &b) { dc.FillRect(*r, &b); }
inline void FillRect(DC *dc, const Rect &r, const Brush &b) { dc->FillRect(r, &b); }
inline void FillRect(DC *dc, const Rect *r, const Brush &b) { dc->FillRect(*r, &b); }

// Owner-drawn controls: the item states of DrawItemInfo.
namespace ods
{
	inline constexpr uint32_t Selected = 0x1, Grayed = 0x2, Disabled = 0x4, Checked = 0x8, Focus = 0x10, Default = 0x20;
}
// Control types of DrawItemInfo.
namespace odt
{
	inline constexpr uint32_t Menu = 1, ListBox = 2, ComboBox = 3, Button = 4, Static = 5;
}

// What an owner-drawn control asks its owner (or itself) to draw.
struct DrawItemInfo
{
	uint32_t CtlType = odt::Button;
	uint32_t CtlID = 0;
	uint32_t itemID = 0;
	uint32_t itemAction = 0;
	uint32_t itemState = 0;
	QWidget *hwndItem = nullptr;
	DC *hDC = nullptr;
	Rect rcItem;
	uintptr_t itemData = 0;
};

// The DC of OnPaint: draws on the widget being painted (a scroll area's
// viewport). rcPaint is the area to paint.
class PaintDC : public DC
{
public:
	explicit PaintDC(QWidget *widget);
	Rect rcPaint;
};

// CClientDC / CWindowDC: Qt draws on a widget only in its paint event, so
// this DC draws nothing; it measures text in the widget's font, and when it
// goes the widget is repainted (its OnPaint draws everything again).
class ClientDC : public DC
{
public:
	explicit ClientDC(QWidget *widget);
	~ClientDC();
	ClientDC(const ClientDC &) = delete;
	ClientDC &operator=(const ClientDC &) = delete;

private:
	QPointer<QWidget> m_widget;
};

} // namespace mfcless
