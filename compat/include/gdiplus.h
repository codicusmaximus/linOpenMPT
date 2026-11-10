// linOpenMPT compatibility layer: the part of GDI+ that OpenMPT uses
// (Image.cpp's PNG/SVG loading and scaling, and the splash screen). Declared
// only; the platform layer implements it on QImage and QPainter in step 3.
#pragma once
#include "atlbase.h"
#include "linopenmpt/win32_gdi.h"

namespace Gdiplus
{
#include <gdipluspixelformats.h>

enum Status
{
	Ok = 0,
	GenericError = 1,
	InvalidParameter = 2,
	OutOfMemory = 3,
	ObjectBusy = 4,
	InsufficientBuffer = 5,
	NotImplemented = 6,
	Win32Error = 7,
	WrongState = 8,
	Aborted = 9,
	FileNotFound = 10,
	ValueOverflow = 11,
	AccessDenied = 12,
	UnknownImageFormat = 13,
};
enum Unit { UnitWorld, UnitDisplay, UnitPixel, UnitPoint, UnitInch, UnitDocument, UnitMillimeter };
enum InterpolationMode
{
	InterpolationModeInvalid = -1,
	InterpolationModeDefault = 0,
	InterpolationModeLowQuality = 1,
	InterpolationModeHighQuality = 2,
	InterpolationModeBilinear = 3,
	InterpolationModeBicubic = 4,
	InterpolationModeNearestNeighbor = 5,
	InterpolationModeHighQualityBilinear = 6,
	InterpolationModeHighQualityBicubic = 7,
};
enum SmoothingMode
{
	SmoothingModeInvalid = -1,
	SmoothingModeDefault = 0,
	SmoothingModeHighSpeed = 1,
	SmoothingModeHighQuality = 2,
	SmoothingModeNone = 3,
	SmoothingModeAntiAlias = 4,
};
enum PixelOffsetMode
{
	PixelOffsetModeInvalid = -1,
	PixelOffsetModeDefault = 0,
	PixelOffsetModeHighSpeed = 1,
	PixelOffsetModeHighQuality = 2,
	PixelOffsetModeNone = 3,
	PixelOffsetModeHalf = 4,
};
enum ImageLockMode
{
	ImageLockModeRead = 0x0001,
	ImageLockModeWrite = 0x0002,
	ImageLockModeUserInputBuf = 0x0004,
};

struct Point
{
	INT X = 0, Y = 0;
	Point() = default;
	Point(INT x, INT y) : X(x), Y(y) { }
};
struct Size
{
	INT Width = 0, Height = 0;
	Size() = default;
	Size(INT w, INT h) : Width(w), Height(h) { }
};
struct Rect
{
	INT X = 0, Y = 0, Width = 0, Height = 0;
	Rect() = default;
	Rect(INT x, INT y, INT w, INT h) : X(x), Y(y), Width(w), Height(h) { }
	Rect(const Point &p, const Size &s) : X(p.X), Y(p.Y), Width(s.Width), Height(s.Height) { }
};
class Color
{
public:
	Color() = default;
	Color(BYTE a, BYTE r, BYTE g, BYTE b) : m_argb((ARGB(a) << ALPHA_SHIFT) | (ARGB(r) << RED_SHIFT) | (ARGB(g) << GREEN_SHIFT) | (ARGB(b) << BLUE_SHIFT)) { }
	Color(BYTE r, BYTE g, BYTE b) : Color(255, r, g, b) { }
	ARGB GetValue() const { return m_argb; }
private:
	ARGB m_argb = 0xFF000000;
};
struct BitmapData
{
	UINT Width;
	UINT Height;
	INT Stride;
	INT PixelFormat; // a PixelFormat (the member hides the type name)
	void *Scan0;
	UINT_PTR Reserved;
};

struct GdiplusStartupInput
{
	UINT GdiplusVersion = 1;
	void *DebugEventCallback = nullptr;
	BOOL SuppressBackgroundThread = FALSE;
	BOOL SuppressExternalCodecs = FALSE;
};
struct GdiplusStartupOutput;
Status GdiplusStartup(ULONG_PTR *token, const GdiplusStartupInput *input, GdiplusStartupOutput *output);
void GdiplusShutdown(ULONG_PTR token);

// Images own a platform image (a QImage, or a QSvgRenderer for metafiles).
class Image
{
public:
	virtual ~Image();
	UINT GetWidth();
	UINT GetHeight();
	Status GetLastStatus() const;
protected:
	Image() = default;
	void *m_native = nullptr;
	Status m_status = Ok;
	friend class Graphics;
};
class Bitmap : public Image
{
public:
	Bitmap(IStream *stream, BOOL useEmbeddedColorManagement = FALSE);
	Bitmap(INT width, INT height, PixelFormat format = PixelFormat32bppARGB);
	Status LockBits(const Rect *rect, UINT flags, PixelFormat format, BitmapData *lockedData);
	Status UnlockBits(BitmapData *lockedData);
};
class Metafile : public Image
{
public:
	explicit Metafile(IStream *stream);
};

class Graphics
{
public:
	explicit Graphics(HDC dc);
	~Graphics();
	static Graphics *FromImage(Image *image);
	static Graphics *FromHDC(HDC dc);
	Status SetInterpolationMode(InterpolationMode mode);
	Status SetSmoothingMode(SmoothingMode mode);
	Status SetPixelOffsetMode(PixelOffsetMode mode);
	Status Clear(const Color &color);
	Status DrawImage(Image *image, INT x, INT y);
	Status DrawImage(Image *image, INT x, INT y, INT width, INT height);
	Status DrawImage(Image *image, const Rect &dest, INT srcX, INT srcY, INT srcWidth, INT srcHeight, Unit srcUnit);
private:
	Graphics() = default;
	void *m_native = nullptr;
};

} // namespace Gdiplus
