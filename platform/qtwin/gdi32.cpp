// linOpenMPT platform layer: Win32 GDI on QPainter.
//
// Device contexts:
//  - window DCs (BeginPaint, GetDC) draw into the window's backing image
//    (Win32Widget::surface), through one QPainter shared by all of the
//    window's DCs;
//  - memory DCs (CreateCompatibleDC) draw into the bitmap selected into
//    them, a QImage;
//  - the screen DC (GetDC(nullptr)) only measures text.
// Each DC keeps its own GDI state (objects, colours, origin, clipping), and
// every drawing call applies that state to the painter first, so several DCs
// on one window behave independently, as on Windows.
//
// Coordinates are logical (device-independent) pixels. Backing images carry
// the screen's device pixel ratio, so drawing is sharp on HiDPI screens.
#include "QtWin.h"

#include <QtGui/QFontDatabase>
#include <QtGui/QFontInfo>
#include <QtGui/QFontMetrics>
#include <QtGui/QGuiApplication>
#include <QtGui/QPainterPath>
#include <QtGui/QPolygon>
#include <QtGui/QScreen>
#include <QtWidgets/QApplication>
#include <QtWidgets/QStyle>
#include <QtWidgets/QStyleOption>

#include <algorithm>
#include <climits>
#include <cstring>
#include <set>
#include <unordered_set>

namespace linopenmpt::qtwin
{

QColor ToQColor(COLORREF color)
{
	return QColor(GetRValue(color), GetGValue(color), GetBValue(color));
}

COLORREF ToColorRef(const QColor &color)
{
	return RGB(color.red(), color.green(), color.blue());
}

namespace
{

// ---- objects ------------------------------------------------------------------

enum class Kind { Pen, Brush, Font, Bitmap, Region };

struct GdiObject
{
	explicit GdiObject(Kind k) : kind(k) { }
	virtual ~GdiObject() = default;
	const Kind kind;
	bool stock = false;
};

struct Pen : GdiObject
{
	Pen() : GdiObject(Kind::Pen) { }
	UINT style = PS_SOLID;
	int width = 1;
	COLORREF color = 0;
	bool dcPen = false;
};

struct Brush : GdiObject
{
	Brush() : GdiObject(Kind::Brush) { }
	UINT style = BS_SOLID;
	COLORREF color = 0;
	int hatch = 0;
	QImage pattern;
	bool dcBrush = false;
};

struct Font : GdiObject
{
	Font() : GdiObject(Kind::Font) { }
	LOGFONTW lf{};
	QFont font;
};

struct DC;

struct Bitmap : GdiObject
{
	Bitmap() : GdiObject(Kind::Bitmap) { }
	~Bitmap() override { painter.reset(); }
	// DIB sections own their pixels here, and image wraps them, so writes
	// through the pointer CreateDIBSection returned show up in drawing.
	std::vector<uchar> storage;
	QImage image;
	int bitsPerPixel = 32;
	bool bottomUp = false;
	bool dibSection = false;
	DC *selectedInto = nullptr;
	std::unique_ptr<QPainter> painter;
	void EndPainter() { painter.reset(); }
};

struct Region : GdiObject
{
	Region() : GdiObject(Kind::Region) { }
	QRegion region;
};

std::unordered_set<GdiObject *> &Objects()
{
	static std::unordered_set<GdiObject *> objects;
	return objects;
}

template <typename T>
T *Register(T *object)
{
	Objects().insert(object);
	return object;
}

template <typename T, Kind K>
T *As(const void *handle)
{
	auto *o = static_cast<GdiObject *>(const_cast<void *>(handle));
	if(o == nullptr || !Objects().count(o) || o->kind != K)
		return nullptr;
	return static_cast<T *>(o);
}

Pen *ToPen(const void *h) { return As<Pen, Kind::Pen>(h); }
Font *ToFont(const void *h) { return As<Font, Kind::Font>(h); }
Bitmap *ToBitmap(const void *h) { return As<Bitmap, Kind::Bitmap>(h); }
Region *ToRegion(const void *h) { return As<Region, Kind::Region>(h); }
Brush *ToBrush(const void *h);

QFont DefaultFont()
{
	return QApplication::font();
}

QFont FixedFont()
{
	return QFontDatabase::systemFont(QFontDatabase::FixedFont);
}

void InstallFontSubstitutes()
{
	static bool done = false;
	if(done)
		return;
	done = true;
	// Windows UI and monospace faces that upstream names explicitly.
	const QString ui = DefaultFont().family();
	const QString fixed = FixedFont().family();
	for(const char *face : {"MS Shell Dlg", "MS Shell Dlg 2", "MS Sans Serif", "Segoe UI", "Tahoma", "Microsoft Sans Serif", "Arial"})
		QFont::insertSubstitution(QString::fromLatin1(face), ui);
	for(const char *face : {"Courier New", "Courier", "Lucida Console", "Consolas", "Fixedsys", "Terminal"})
		QFont::insertSubstitution(QString::fromLatin1(face), fixed);
}

QFont FontFromLogFont(const LOGFONTW &lf)
{
	InstallFontSubstitutes();
	const QString face = ToQString(lf.lfFaceName, static_cast<int>(wcsnlen(lf.lfFaceName, LF_FACESIZE)));
	QFont font = DefaultFont();
	if(!face.isEmpty())
		font.setFamily(face);
	else if((lf.lfPitchAndFamily & 0x03) == FIXED_PITCH || (lf.lfPitchAndFamily & 0xF0) == FF_MODERN)
		font = FixedFont();
	if((lf.lfPitchAndFamily & 0x03) == FIXED_PITCH)
	{
		font.setFixedPitch(true);
		font.setStyleHint(QFont::TypeWriter, QFont::PreferDefault);
	}
	if(lf.lfHeight < 0)
	{
		// Character (em) height.
		font.setPixelSize(static_cast<int>(-lf.lfHeight));
	} else if(lf.lfHeight > 0)
	{
		// Cell height: em height plus internal leading.
		font.setPixelSize(static_cast<int>(lf.lfHeight));
		const int cell = QFontMetrics(font).height();
		if(cell > lf.lfHeight)
			font.setPixelSize(std::max(1, static_cast<int>(lf.lfHeight * lf.lfHeight / cell)));
	}
	if(lf.lfWeight > 0)
		font.setWeight(static_cast<QFont::Weight>(std::clamp<int>(lf.lfWeight, 100, 900)));
	font.setItalic(lf.lfItalic != 0);
	font.setUnderline(lf.lfUnderline != 0);
	font.setStrikeOut(lf.lfStrikeOut != 0);
	if(lf.lfQuality == NONANTIALIASED_QUALITY)
		font.setStyleStrategy(QFont::NoAntialias);
	return font;
}

LOGFONTW LogFontFromFont(const QFont &font)
{
	LOGFONTW lf{};
	const QFontInfo info(font);
	lf.lfHeight = -info.pixelSize();
	lf.lfWeight = font.weight();
	lf.lfItalic = font.italic();
	lf.lfUnderline = font.underline();
	lf.lfStrikeOut = font.strikeOut();
	lf.lfCharSet = DEFAULT_CHARSET;
	lf.lfPitchAndFamily = info.fixedPitch() ? FIXED_PITCH : VARIABLE_PITCH;
	const std::wstring face = ToWString(info.family());
	std::wcsncpy(lf.lfFaceName, face.c_str(), LF_FACESIZE - 1);
	return lf;
}

Font *MakeFont(const QFont &qfont, bool stock)
{
	auto *f = Register(new Font);
	f->font = qfont;
	f->lf = LogFontFromFont(qfont);
	f->stock = stock;
	return f;
}

// ---- stock objects ------------------------------------------------------------

Bitmap *DefaultBitmap();

GdiObject *Stock(int index)
{
	static GdiObject *objects[DC_PEN + 1] = {};
	if(index < 0 || index > DC_PEN)
		return nullptr;
	if(objects[index])
		return objects[index];
	GdiObject *o = nullptr;
	auto solid = [](COLORREF c, UINT style = BS_SOLID)
	{
		auto *b = Register(new Brush);
		b->color = c;
		b->style = style;
		return b;
	};
	auto pen = [](COLORREF c, UINT style = PS_SOLID)
	{
		auto *p = Register(new Pen);
		p->color = c;
		p->style = style;
		return p;
	};
	switch(index)
	{
	case WHITE_BRUSH: o = solid(RGB(255, 255, 255)); break;
	case LTGRAY_BRUSH: o = solid(RGB(192, 192, 192)); break;
	case GRAY_BRUSH: o = solid(RGB(128, 128, 128)); break;
	case DKGRAY_BRUSH: o = solid(RGB(64, 64, 64)); break;
	case BLACK_BRUSH: o = solid(RGB(0, 0, 0)); break;
	case NULL_BRUSH: o = solid(0, BS_NULL); break;
	case WHITE_PEN: o = pen(RGB(255, 255, 255)); break;
	case BLACK_PEN: o = pen(RGB(0, 0, 0)); break;
	case NULL_PEN: o = pen(0, PS_NULL); break;
	case OEM_FIXED_FONT:
	case ANSI_FIXED_FONT:
	case SYSTEM_FIXED_FONT:
		o = MakeFont(FixedFont(), true);
		break;
	case ANSI_VAR_FONT:
	case SYSTEM_FONT:
	case DEVICE_DEFAULT_FONT:
	case DEFAULT_GUI_FONT:
		o = MakeFont(DefaultFont(), true);
		break;
	case DC_BRUSH:
		{
			auto *b = solid(RGB(255, 255, 255));
			b->dcBrush = true;
			o = b;
		}
		break;
	case DC_PEN:
		{
			auto *p = pen(RGB(0, 0, 0));
			p->dcPen = true;
			o = p;
		}
		break;
	default:
		return nullptr;
	}
	o->stock = true;
	objects[index] = o;
	return o;
}

// System colours from the application's palette, so that GDI drawing
// matches the Qt widgets and follows the desktop's colour scheme.
DWORD PaletteSysColor(int index)
{
	if(qobject_cast<QGuiApplication *>(QCoreApplication::instance()) == nullptr)
		return 0xFFFFFFFF;
	const QPalette pal = QGuiApplication::palette();
	auto c = [&](QPalette::ColorRole role, QPalette::ColorGroup group = QPalette::Active)
	{
		const QColor q = pal.color(group, role);
		return static_cast<DWORD>(RGB(q.red(), q.green(), q.blue()));
	};
	switch(index)
	{
	case COLOR_SCROLLBAR: return c(QPalette::Button);
	case COLOR_BACKGROUND: return c(QPalette::Dark);
	case COLOR_ACTIVECAPTION: return c(QPalette::Highlight);
	case COLOR_INACTIVECAPTION: return c(QPalette::Mid);
	case COLOR_MENU: return c(QPalette::Window);
	case COLOR_WINDOW: return c(QPalette::Base);
	case COLOR_WINDOWFRAME: return c(QPalette::Shadow);
	case COLOR_MENUTEXT: return c(QPalette::WindowText);
	case COLOR_WINDOWTEXT: return c(QPalette::Text);
	case COLOR_CAPTIONTEXT: return c(QPalette::HighlightedText);
	case COLOR_ACTIVEBORDER: return c(QPalette::Window);
	case COLOR_INACTIVEBORDER: return c(QPalette::Window);
	case COLOR_APPWORKSPACE: return c(QPalette::Mid);
	case COLOR_HIGHLIGHT: return c(QPalette::Highlight);
	case COLOR_HIGHLIGHTTEXT: return c(QPalette::HighlightedText);
	case COLOR_BTNFACE: return c(QPalette::Window);
	case COLOR_BTNSHADOW: return c(QPalette::Dark);
	case COLOR_GRAYTEXT: return c(QPalette::Text, QPalette::Disabled);
	case COLOR_BTNTEXT: return c(QPalette::ButtonText);
	case COLOR_INACTIVECAPTIONTEXT: return c(QPalette::WindowText);
	case COLOR_BTNHIGHLIGHT: return c(QPalette::Light);
	case COLOR_3DDKSHADOW: return c(QPalette::Shadow);
	case COLOR_3DLIGHT: return c(QPalette::Midlight);
	case COLOR_INFOTEXT: return c(QPalette::ToolTipText);
	case COLOR_INFOBK: return c(QPalette::ToolTipBase);
	case COLOR_HOTLIGHT: return c(QPalette::Link);
	case COLOR_GRADIENTACTIVECAPTION: return c(QPalette::Highlight);
	case COLOR_GRADIENTINACTIVECAPTION: return c(QPalette::Mid);
	case COLOR_MENUHILIGHT: return c(QPalette::Highlight);
	case COLOR_MENUBAR: return c(QPalette::Window);
	default: return 0xFFFFFFFF;
	}
}

const bool g_sysColorsFromPalette = []()
{
	LinOpenMPTSetSysColorProvider(PaletteSysColor);
	return true;
}();

Brush *SysColorBrush(int index)
{
	static std::map<int, Brush *> brushes;
	auto &b = brushes[index];
	if(b == nullptr)
	{
		b = Register(new Brush);
		b->stock = true;
	}
	// System colour brushes change with the colour scheme, as on Windows.
	b->color = ::GetSysColor(index);
	return b;
}

Brush *ToBrush(const void *h)
{
	// (HBRUSH)(COLOR_xxx + 1) means that system colour.
	const auto value = reinterpret_cast<UINT_PTR>(h);
	if(value >= 1 && value <= 31)
		return SysColorBrush(static_cast<int>(value - 1));
	return As<Brush, Kind::Brush>(h);
}

// ---- device contexts ----------------------------------------------------------

struct DCState
{
	Pen *pen = nullptr;
	Brush *brush = nullptr;
	Font *font = nullptr;
	COLORREF textColor = RGB(0, 0, 0);
	COLORREF bkColor = RGB(255, 255, 255);
	COLORREF dcBrushColor = RGB(255, 255, 255);
	COLORREF dcPenColor = RGB(0, 0, 0);
	int bkMode = OPAQUE;
	int rop2 = R2_COPYPEN;
	UINT textAlign = TA_LEFT | TA_TOP;
	int stretchMode = BLACKONWHITE;
	QPoint origin; // viewport origin: logical (0, 0) in device pixels
	QPoint position; // MoveToEx
	QRegion clip; // device coordinates
	bool hasClip = false;
};

struct DC
{
	enum Type { Window, Memory, Screen };
	Type type = Screen;
	Win32Widget *window = nullptr;
	Bitmap *bitmap = nullptr; // memory DCs
	bool holdsSurfacePainter = false;
	bool paintDC = false; // from BeginPaint
	bool wholeWindow = false; // GetWindowDC: the non-client area too
	QRegion paintClip; // the update region, for BeginPaint DCs
	DCState s;
	std::vector<DCState> saved;
};

std::unordered_set<DC *> &DCs()
{
	static std::unordered_set<DC *> dcs;
	return dcs;
}

DC *ToDC(HDC h)
{
	auto *dc = reinterpret_cast<DC *>(h);
	return (dc != nullptr && DCs().count(dc)) ? dc : nullptr;
}

HDC Handle(DC *dc)
{
	return reinterpret_cast<HDC>(dc);
}

DC *NewDC(DC::Type type, Win32Widget *window)
{
	auto *dc = new DC;
	dc->type = type;
	dc->window = window;
	dc->s.pen = static_cast<Pen *>(Stock(BLACK_PEN));
	dc->s.brush = static_cast<Brush *>(Stock(WHITE_BRUSH));
	dc->s.font = static_cast<Font *>(Stock(SYSTEM_FONT));
	if(type == DC::Memory)
	{
		dc->bitmap = DefaultBitmap();
	}
	if(window != nullptr)
	{
		// Windows gives new window DCs the window's font only for CS_OWNDC
		// classes; MFC code selects its fonts explicitly. WM_SETFONT fonts
		// are used for controls.
		if(Font *f = ToFont(window->data.font))
			dc->s.font = f;
	}
	DCs().insert(dc);
	return dc;
}

void ReleaseSurfacePainter(DC &dc)
{
	if(!dc.holdsSurfacePainter)
		return;
	dc.holdsSurfacePainter = false;
	if(Win32Widget *w = dc.window)
	{
		if(--w->surfacePainterRefs <= 0)
		{
			w->surfacePainterRefs = 0;
			w->surfacePainter.reset();
		}
	}
}

void DestroyDC(DC *dc)
{
	if(dc == nullptr)
		return;
	ReleaseSurfacePainter(*dc);
	if(dc->bitmap && dc->bitmap->selectedInto == dc)
	{
		dc->bitmap->EndPainter();
		dc->bitmap->selectedInto = nullptr;
	}
	DCs().erase(dc);
	delete dc;
}

Bitmap *DefaultBitmap()
{
	// Every memory DC starts with this 1x1 monochrome bitmap selected. It
	// is shared, so it never tracks the DC it is selected into.
	static Bitmap *bitmap = []
	{
		auto *b = Register(new Bitmap);
		b->image = QImage(1, 1, QImage::Format_RGB32);
		b->image.fill(Qt::black);
		b->bitsPerPixel = 1;
		b->stock = true;
		return b;
	}();
	return bitmap;
}

bool Paintable(const QImage &image)
{
	switch(image.format())
	{
	case QImage::Format_RGB32:
	case QImage::Format_ARGB32:
	case QImage::Format_ARGB32_Premultiplied:
	case QImage::Format_RGB16:
	case QImage::Format_RGB555:
	case QImage::Format_RGB888:
	case QImage::Format_BGR888:
		return true;
	default:
		return false;
	}
}

// Where a window DC's device coordinates start in the window's widget:
// client DCs start at the client area, window DCs at the window's corner.
QPoint DeviceOffset(const DC &dc)
{
	return (dc.type == DC::Window && dc.window && !dc.wholeWindow) ? ClientOrigin(dc.window) : QPoint();
}

QRect DeviceBounds(const DC &dc)
{
	switch(dc.type)
	{
	case DC::Window:
		if(dc.window == nullptr)
			return QRect();
		return dc.wholeWindow ? dc.window->rect() : QRect(QPoint(0, 0), ClientRect(dc.window).size());
	case DC::Memory:
		return dc.bitmap ? QRect(QPoint(0, 0), (QSizeF(dc.bitmap->image.size()) / dc.bitmap->image.devicePixelRatio()).toSize()) : QRect();
	case DC::Screen:
		if(QScreen *screen = QGuiApplication::primaryScreen())
			return screen->virtualGeometry().translated(-screen->virtualGeometry().topLeft());
		return QRect(0, 0, 1920, 1080);
	}
	return QRect();
}

// The region drawing is limited to, in device coordinates.
QRegion EffectiveClip(const DC &dc)
{
	QRegion r(DeviceBounds(dc));
	if(dc.s.hasClip)
		r &= dc.s.clip;
	if(dc.paintDC)
		r &= dc.paintClip;
	return r;
}

// Returns the painter for this DC's device, with the DC's clipping and
// origin applied, or nullptr when the DC cannot be drawn on.
QPainter *Painter(DC &dc)
{
	QPainter *p = nullptr;
	switch(dc.type)
	{
	case DC::Window:
		{
			Win32Widget *w = dc.window;
			if(w == nullptr)
				return nullptr;
			EnsureSurface(w);
			if(w->surface.isNull())
				return nullptr;
			if(!dc.holdsSurfacePainter)
			{
				if(w->surfacePainterRefs++ == 0 || !w->surfacePainter)
					w->surfacePainter = std::make_unique<QPainter>(&w->surface);
				dc.holdsSurfacePainter = true;
			}
			p = w->surfacePainter.get();
			// Drawing outside WM_PAINT shows up on the next screen update.
			if(!dc.paintDC)
				w->update();
		}
		break;
	case DC::Memory:
		{
			Bitmap *b = dc.bitmap;
			if(b == nullptr || b->stock || b->image.isNull() || !Paintable(b->image))
				return nullptr;
			if(!b->painter)
				b->painter = std::make_unique<QPainter>(&b->image);
			p = b->painter.get();
		}
		break;
	case DC::Screen:
		return nullptr;
	}
	if(p == nullptr || !p->isActive())
		return nullptr;
	p->resetTransform();
	p->translate(DeviceOffset(dc));
	p->setRenderHint(QPainter::Antialiasing, false);
	p->setRenderHint(QPainter::SmoothPixmapTransform, dc.s.stretchMode == HALFTONE);
	p->setCompositionMode(QPainter::CompositionMode_SourceOver);
	p->setOpacity(1.0);
	p->setClipRegion(EffectiveClip(dc));
	p->translate(dc.s.origin);
	return p;
}

// ---- drawing helpers ------------------------------------------------------------

COLORREF Plain(COLORREF c)
{
	return c & 0x00FFFFFF;
}

QPainter::CompositionMode Rop2Mode(int rop2)
{
	switch(rop2)
	{
	case R2_XORPEN: return QPainter::RasterOp_SourceXorDestination;
	case R2_NOT: return QPainter::RasterOp_NotDestination;
	case R2_NOTXORPEN: return QPainter::RasterOp_NotSourceXorDestination;
	default: return QPainter::CompositionMode_SourceOver;
	}
}

QColor Rop2Color(const DC &dc, COLORREF c)
{
	if(dc.s.rop2 == R2_BLACK)
		return Qt::black;
	if(dc.s.rop2 == R2_WHITE)
		return Qt::white;
	return ToQColor(Plain(c));
}

COLORREF PenColor(const DC &dc)
{
	if(dc.s.pen == nullptr)
		return 0;
	return dc.s.pen->dcPen ? dc.s.dcPenColor : dc.s.pen->color;
}

bool HasPen(const DC &dc)
{
	return dc.s.pen != nullptr && dc.s.pen->style != PS_NULL;
}

int PenWidth(const DC &dc)
{
	return dc.s.pen ? std::max(1, dc.s.pen->width) : 1;
}

QPen MakePen(const DC &dc)
{
	if(!HasPen(dc))
		return QPen(Qt::NoPen);
	QPen pen(Rop2Color(dc, PenColor(dc)));
	pen.setWidth(PenWidth(dc));
	switch(dc.s.pen->style & 0x0F)
	{
	case PS_DASH: pen.setStyle(Qt::DashLine); break;
	case PS_DOT: pen.setStyle(Qt::DotLine); break;
	case PS_DASHDOT: pen.setStyle(Qt::DashDotLine); break;
	case PS_DASHDOTDOT: pen.setStyle(Qt::DashDotDotLine); break;
	default: pen.setStyle(Qt::SolidLine); break;
	}
	pen.setCapStyle((dc.s.pen->style & PS_ENDCAP_FLAT) ? Qt::FlatCap : (pen.width() > 1 ? Qt::RoundCap : Qt::SquareCap));
	pen.setJoinStyle(pen.width() > 1 ? Qt::RoundJoin : Qt::MiterJoin);
	return pen;
}

bool HasBrush(const Brush *b)
{
	return b != nullptr && b->style != BS_NULL;
}

QBrush MakeBrush(const DC &dc, const Brush *b)
{
	if(!HasBrush(b))
		return QBrush(Qt::NoBrush);
	const COLORREF color = b->dcBrush ? dc.s.dcBrushColor : b->color;
	if(b->style == BS_PATTERN && !b->pattern.isNull())
		return QBrush(b->pattern);
	if(b->style == BS_HATCHED)
	{
		Qt::BrushStyle style = Qt::HorPattern;
		switch(b->hatch)
		{
		case HS_VERTICAL: style = Qt::VerPattern; break;
		case HS_FDIAGONAL: style = Qt::FDiagPattern; break;
		case HS_BDIAGONAL: style = Qt::BDiagPattern; break;
		case HS_CROSS: style = Qt::CrossPattern; break;
		case HS_DIAGCROSS: style = Qt::DiagCrossPattern; break;
		default: break;
		}
		return QBrush(Rop2Color(dc, color), style);
	}
	return QBrush(Rop2Color(dc, color));
}

// Fills an area with a brush, including the background of hatched brushes.
void FillArea(DC &dc, QPainter &p, const QRegion &area, const Brush *b, QPainter::CompositionMode mode = QPainter::CompositionMode_SourceOver)
{
	if(!HasBrush(b))
		return;
	p.setCompositionMode(mode);
	if(b->style == BS_HATCHED && dc.s.bkMode == OPAQUE)
	{
		for(const QRect &r : area)
			p.fillRect(r, ToQColor(Plain(dc.s.bkColor)));
	}
	const QBrush brush = MakeBrush(dc, b);
	// Hatch patterns are transparent between the lines.
	if(b->style == BS_HATCHED && mode == QPainter::CompositionMode_Source)
		p.setCompositionMode(QPainter::CompositionMode_SourceOver);
	for(const QRect &r : area)
		p.fillRect(r, brush);
	p.setCompositionMode(QPainter::CompositionMode_SourceOver);
}

QRect RectOf(int left, int top, int right, int bottom)
{
	return QRect(QPoint(std::min(left, right), std::min(top, bottom)), QPoint(std::max(left, right) - 1, std::max(top, bottom) - 1));
}

QRect RectOf(const RECT &r)
{
	return RectOf(r.left, r.top, r.right, r.bottom);
}

RECT ToRECT(const QRect &r)
{
	return RECT{r.left(), r.top(), r.left() + r.width(), r.top() + r.height()};
}

int RegionType(const QRegion &r)
{
	if(r.isEmpty())
		return NULLREGION;
	return r.rectCount() == 1 ? SIMPLEREGION : COMPLEXREGION;
}

// A line from a to b, excluding b, as GDI draws it.
void DrawLine(DC &dc, QPainter &p, QPoint a, QPoint b)
{
	if(!HasPen(dc) || a == b)
		return;
	const QPen pen = MakePen(dc);
	p.setCompositionMode(Rop2Mode(dc.s.rop2));
	if(pen.width() == 1 && pen.style() == Qt::SolidLine && (a.x() == b.x() || a.y() == b.y()))
	{
		// Exact pixels for the common case.
		QRect r;
		if(a.y() == b.y())
			r = a.x() < b.x() ? QRect(a.x(), a.y(), b.x() - a.x(), 1) : QRect(b.x() + 1, a.y(), a.x() - b.x(), 1);
		else
			r = a.y() < b.y() ? QRect(a.x(), a.y(), 1, b.y() - a.y()) : QRect(a.x(), b.y() + 1, 1, a.y() - b.y());
		p.fillRect(r, pen.color());
	} else
	{
		QPen linePen = pen;
		if(pen.width() == 1)
			linePen.setCapStyle(Qt::FlatCap);
		p.setPen(linePen);
		p.drawLine(a, b);
	}
	p.setCompositionMode(QPainter::CompositionMode_SourceOver);
}

QFont DCFont(const DC &dc)
{
	return dc.s.font ? dc.s.font->font : DefaultFont();
}

// ---- bitmaps --------------------------------------------------------------------

int DibStride(int width, int bpp)
{
	return ((width * bpp + 31) / 32) * 4;
}

QList<QRgb> DibColorTable(const BITMAPINFO *bmi, int bpp)
{
	QList<QRgb> table;
	if(bpp > 8)
		return table;
	int count = static_cast<int>(bmi->bmiHeader.biClrUsed);
	if(count <= 0 || count > (1 << bpp))
		count = 1 << bpp;
	for(int i = 0; i < count; i++)
	{
		const RGBQUAD &q = bmi->bmiColors[i];
		table.push_back(qRgb(q.rgbRed, q.rgbGreen, q.rgbBlue));
	}
	return table;
}

QImage::Format DibFormat(int bpp)
{
	switch(bpp)
	{
	case 32: return QImage::Format_RGB32;
	case 24: return QImage::Format_BGR888;
	case 16: return QImage::Format_RGB555;
	case 8: return QImage::Format_Indexed8;
	case 1: return QImage::Format_Mono;
	default: return QImage::Format_Invalid;
	}
}

// A top-down image of DIB bits. Wraps the bits when it can; copies them
// for bottom-up and 4-bit DIBs.
QImage DibImage(const void *bits, const BITMAPINFO *bmi, int lines = -1)
{
	if(bits == nullptr || bmi == nullptr)
		return QImage();
	const BITMAPINFOHEADER &h = bmi->bmiHeader;
	const int width = h.biWidth;
	int height = h.biHeight < 0 ? -h.biHeight : h.biHeight;
	if(lines >= 0)
		height = std::min(height, lines);
	const int bpp = h.biBitCount;
	if(width <= 0 || height <= 0)
		return QImage();
	const int stride = DibStride(width, bpp);
	const auto *data = static_cast<const uchar *>(bits);
	QImage image;
	if(bpp == 4)
	{
		image = QImage(width, height, QImage::Format_Indexed8);
		for(int y = 0; y < height; y++)
		{
			const uchar *src = data + y * stride;
			uchar *dst = image.scanLine(y);
			for(int x = 0; x < width; x++)
				dst[x] = (x & 1) ? (src[x / 2] & 0x0F) : (src[x / 2] >> 4);
		}
	} else
	{
		const QImage::Format format = DibFormat(bpp);
		if(format == QImage::Format_Invalid)
			return QImage();
		image = QImage(data, width, height, stride, format);
	}
	if(bpp <= 8)
		image.setColorTable(DibColorTable(bmi, bpp));
	if(h.biHeight > 0)
		image = image.mirrored(false, true);
	return image;
}

// The image a DC's pixels are in, top-down, and its pixel ratio.
const QImage *SourceImage(const DC &dc, QImage &holder)
{
	switch(dc.type)
	{
	case DC::Window:
		{
			if(dc.window == nullptr || dc.window->surface.isNull())
				return nullptr;
			const QPoint offset = DeviceOffset(dc);
			if(offset.isNull())
				return &dc.window->surface;
			// A client DC of a window with a non-client area: its pixels start
			// at the client area.
			const qreal dpr = dc.window->surface.devicePixelRatio();
			const QRect area(offset, DeviceBounds(dc).size());
			holder = dc.window->surface.copy(QRectF(area.x() * dpr, area.y() * dpr, area.width() * dpr, area.height() * dpr).toAlignedRect());
			holder.setDevicePixelRatio(dpr);
			return &holder;
		}
	case DC::Memory:
		if(dc.bitmap == nullptr || dc.bitmap->image.isNull())
			return nullptr;
		if(dc.bitmap->bottomUp)
		{
			holder = dc.bitmap->image.mirrored(false, true);
			return &holder;
		}
		return &dc.bitmap->image;
	case DC::Screen:
		break;
	}
	return nullptr;
}

bool SameDevice(const DC &a, const DC &b)
{
	if(a.type != b.type)
		return false;
	if(a.type == DC::Window)
		return a.window == b.window;
	return a.bitmap == b.bitmap;
}

QPainter::CompositionMode RopMode(DWORD rop)
{
	switch(rop)
	{
	case SRCPAINT: return QPainter::RasterOp_SourceOrDestination;
	case SRCAND: return QPainter::RasterOp_SourceAndDestination;
	case SRCINVERT: return QPainter::RasterOp_SourceXorDestination;
	case SRCERASE: return QPainter::RasterOp_SourceAndNotDestination;
	case NOTSRCCOPY: return QPainter::RasterOp_NotSource;
	default: return QPainter::CompositionMode_Source;
	}
}

bool NeedsSource(DWORD rop)
{
	switch(rop)
	{
	case PATCOPY:
	case PATINVERT:
	case DSTINVERT:
	case BLACKNESS:
	case WHITENESS:
		return false;
	default:
		return true;
	}
}

// Draws (a part of) a source image onto the destination DC. Rectangles are
// logical; the source rectangle is in the source image's logical pixels.
BOOL DrawImage(DC &dest, const QRect &target, const QImage &source, QRectF sourceRect, QPainter::CompositionMode mode, qreal opacity = 1.0)
{
	QPainter *p = Painter(dest);
	if(p == nullptr || source.isNull())
		return FALSE;
	QImage image = source;
	// Monochrome sources take the destination's text and background
	// colours, as on Windows.
	if(image.format() == QImage::Format_Mono || image.format() == QImage::Format_MonoLSB)
	{
		image = image.copy();
		image.setColorTable({ToQColor(Plain(dest.s.textColor)).rgb(), ToQColor(Plain(dest.s.bkColor)).rgb()});
	}
	const qreal dpr = source.devicePixelRatio();
	sourceRect = QRectF(sourceRect.x() * dpr, sourceRect.y() * dpr, sourceRect.width() * dpr, sourceRect.height() * dpr);
	p->setCompositionMode(mode);
	p->setOpacity(opacity);
	bool mirrorX = false, mirrorY = false;
	QRect t = target;
	if(t.width() < 0)
	{
		mirrorX = true;
		t = QRect(t.x() + t.width(), t.y(), -t.width(), t.height());
	}
	if(t.height() < 0)
	{
		mirrorY = true;
		t = QRect(t.x(), t.y() + t.height(), t.width(), -t.height());
	}
	if(sourceRect.width() < 0)
	{
		mirrorX = !mirrorX;
		sourceRect = QRectF(sourceRect.x() + sourceRect.width(), sourceRect.y(), -sourceRect.width(), sourceRect.height());
	}
	if(sourceRect.height() < 0)
	{
		mirrorY = !mirrorY;
		sourceRect = QRectF(sourceRect.x(), sourceRect.y() + sourceRect.height(), sourceRect.width(), -sourceRect.height());
	}
	if(mirrorX || mirrorY)
	{
		image = image.copy(sourceRect.toAlignedRect()).mirrored(mirrorX, mirrorY);
		image.setDevicePixelRatio(1.0);
		sourceRect = QRectF(image.rect());
	}
	p->drawImage(QRectF(t), image, sourceRect);
	p->setOpacity(1.0);
	p->setCompositionMode(QPainter::CompositionMode_SourceOver);
	return TRUE;
}

BOOL Blit(HDC destDC, int x, int y, int w, int h, HDC srcDC, int xSrc, int ySrc, int wSrc, int hSrc, DWORD rop)
{
	DC *dest = ToDC(destDC);
	if(dest == nullptr)
		return FALSE;
	if(!NeedsSource(rop))
		return ::PatBlt(destDC, x, y, w, h, rop);
	DC *src = ToDC(srcDC);
	if(src == nullptr)
		return FALSE;
	QImage holder;
	const QImage *image = SourceImage(*src, holder);
	if(image == nullptr)
		return FALSE;
	QImage copy;
	if(SameDevice(*dest, *src))
	{
		// Overlapping copies within one image.
		copy = image->copy();
		image = &copy;
	}
	const QRectF sourceRect(xSrc + src->s.origin.x(), ySrc + src->s.origin.y(), wSrc, hSrc);
	return DrawImage(*dest, QRect(x, y, w, h), *image, sourceRect, RopMode(rop));
}

Bitmap *NewBitmap(int width, int height)
{
	auto *b = Register(new Bitmap);
	if(width > 0 && height > 0)
	{
		b->image = QImage(width, height, QImage::Format_RGB32);
		b->image.fill(Qt::black);
	}
	return b;
}

} // namespace

QPainter *DCPainter(HDC hdc)
{
	DC *dc = ToDC(hdc);
	return dc != nullptr ? Painter(*dc) : nullptr;
}

QFont DCFont(HDC hdc)
{
	DC *dc = ToDC(hdc);
	return dc != nullptr && dc->s.font != nullptr ? dc->s.font->font : DefaultFont();
}

QFont FontOf(HFONT font)
{
	Font *f = ToFont(font);
	return f != nullptr ? f->font : DefaultFont();
}

COLORREF DCTextColor(HDC hdc)
{
	DC *dc = ToDC(hdc);
	return dc != nullptr ? dc->s.textColor : 0;
}

QImage DecodeDib(const void *bits, const BITMAPINFO *bmi, int lines)
{
	return DibImage(bits, bmi, lines).copy();
}

QImage BitmapImage(HBITMAP bitmap)
{
	Bitmap *b = ToBitmap(bitmap);
	if(b == nullptr)
		return QImage();
	b->EndPainter();
	return b->image.copy();
}

// ---- backing images -------------------------------------------------------------

bool EnsureSurface(Win32Widget *w)
{
	if(w == nullptr)
		return false;
	const qreal dpr = w->devicePixelRatioF();
	const QSize pixels = (QSizeF(w->size()) * dpr).toSize();
	if(!w->surface.isNull() && w->surface.size() == pixels && qFuzzyCompare(w->surface.devicePixelRatio(), dpr))
		return false;
	const bool hadPainter = w->surfacePainter != nullptr;
	w->surfacePainter.reset();
	// Transparent windows (group boxes) show their parent where they draw nothing.
	QImage image(pixels.expandedTo(QSize(1, 1)), w->data.transparent ? QImage::Format_ARGB32_Premultiplied : QImage::Format_RGB32);
	image.setDevicePixelRatio(dpr);
	image.fill(w->data.transparent ? QColor(Qt::transparent) : ToQColor(::GetSysColor(COLOR_BTNFACE)));
	if(!w->surface.isNull())
	{
		QPainter p(&image);
		p.setCompositionMode(QPainter::CompositionMode_Source);
		p.drawImage(QPointF(0, 0), w->surface);
	}
	w->surface = std::move(image);
	if(hadPainter)
		w->surfacePainter = std::make_unique<QPainter>(&w->surface);
	return true;
}

void ScrollSurface(Win32Widget *w, int dx, int dy, const QRect &area)
{
	if(w == nullptr || w->surface.isNull() || area.isEmpty() || (dx == 0 && dy == 0))
		return;
	const qreal dpr = w->surface.devicePixelRatio();
	const QRect source = area & area.translated(-dx, -dy);
	if(source.isEmpty())
		return;
	const QRect sourcePixels = QRectF(source.x() * dpr, source.y() * dpr, source.width() * dpr, source.height() * dpr).toAlignedRect();
	const QImage moved = w->surface.copy(sourcePixels);
	const bool hadPainter = w->surfacePainter != nullptr;
	w->surfacePainter.reset();
	{
		QPainter p(&w->surface);
		p.setCompositionMode(QPainter::CompositionMode_Source);
		p.setClipRect(area);
		p.drawImage(QRectF(source.translated(dx, dy)), moved);
	}
	if(hadPainter)
		w->surfacePainter = std::make_unique<QPainter>(&w->surface);
}

void ReleaseWindowDCs(Win32Widget *w)
{
	for(DC *dc : DCs())
	{
		if(dc->window == w)
		{
			dc->holdsSurfacePainter = false;
			dc->window = nullptr;
		}
	}
	w->surfacePainter.reset();
	w->surfacePainterRefs = 0;
}

} // namespace linopenmpt::qtwin

using namespace linopenmpt::qtwin;

// ---- device contexts --------------------------------------------------------------

HDC GetDC(HWND hwnd)
{
	if(hwnd == nullptr)
		return Handle(NewDC(DC::Screen, nullptr));
	Win32Widget *w = FromHandle(hwnd);
	if(w == nullptr)
		return nullptr;
	return Handle(NewDC(DC::Window, w));
}

HDC GetWindowDC(HWND hwnd)
{
	// The whole window, non-client area included (top-level frames and
	// captions belong to the window manager).
	Win32Widget *w = FromHandle(hwnd);
	if(w == nullptr)
		return hwnd == nullptr ? GetDC(nullptr) : nullptr;
	DC *dc = NewDC(DC::Window, w);
	dc->wholeWindow = true;
	return Handle(dc);
}

int ReleaseDC(HWND, HDC hdc)
{
	DC *dc = ToDC(hdc);
	if(dc == nullptr)
		return 0;
	if(dc->window)
		dc->window->update();
	DestroyDC(dc);
	return 1;
}

HWND WindowFromDC(HDC hdc)
{
	DC *dc = ToDC(hdc);
	return (dc && dc->window) ? dc->window->hwnd() : nullptr;
}

HDC BeginPaint(HWND hwnd, LPPAINTSTRUCT ps)
{
	Win32Widget *w = FromHandle(hwnd);
	if(w == nullptr || ps == nullptr)
		return nullptr;
	EnsureSurface(w);
	const QRegion update = w->invalid & QRect(QPoint(0, 0), ClientRect(w).size());
	w->invalid = QRegion();
	DC *dc = NewDC(DC::Window, w);
	dc->paintDC = true;
	dc->paintClip = update;
	*ps = PAINTSTRUCT{};
	ps->hdc = Handle(dc);
	ps->rcPaint = ToRECT(update.boundingRect());
	if(w->erasePending)
	{
		w->erasePending = false;
		ps->fErase = ::SendMessage(hwnd, WM_ERASEBKGND, reinterpret_cast<WPARAM>(ps->hdc), 0) ? FALSE : TRUE;
	}
	return ps->hdc;
}

BOOL EndPaint(HWND hwnd, const PAINTSTRUCT *ps)
{
	if(ps == nullptr)
		return FALSE;
	if(Win32Widget *w = FromHandle(hwnd))
		w->update(QRect(ps->rcPaint.left, ps->rcPaint.top, ps->rcPaint.right - ps->rcPaint.left, ps->rcPaint.bottom - ps->rcPaint.top).translated(ClientOrigin(w)));
	DestroyDC(ToDC(ps->hdc));
	return TRUE;
}

HDC CreateCompatibleDC(HDC)
{
	return Handle(NewDC(DC::Memory, nullptr));
}

BOOL DeleteDC(HDC hdc)
{
	DC *dc = ToDC(hdc);
	if(dc == nullptr)
		return FALSE;
	DestroyDC(dc);
	return TRUE;
}

int SaveDC(HDC hdc)
{
	DC *dc = ToDC(hdc);
	if(dc == nullptr)
		return 0;
	dc->saved.push_back(dc->s);
	return static_cast<int>(dc->saved.size());
}

BOOL RestoreDC(HDC hdc, int saved)
{
	DC *dc = ToDC(hdc);
	if(dc == nullptr || dc->saved.empty())
		return FALSE;
	std::size_t level = saved < 0 ? dc->saved.size() + saved + 1 : static_cast<std::size_t>(saved);
	if(level < 1 || level > dc->saved.size())
		return FALSE;
	dc->s = dc->saved[level - 1];
	dc->saved.resize(level - 1);
	// Restored objects may have been deleted in between; fall back to stock.
	if(!Objects().count(dc->s.pen))
		dc->s.pen = static_cast<Pen *>(Stock(BLACK_PEN));
	if(!Objects().count(dc->s.brush))
		dc->s.brush = static_cast<Brush *>(Stock(WHITE_BRUSH));
	if(!Objects().count(dc->s.font))
		dc->s.font = static_cast<Font *>(Stock(SYSTEM_FONT));
	return TRUE;
}

int GetDeviceCaps(HDC hdc, int index)
{
	DC *dc = ToDC(hdc);
	switch(index)
	{
	case LOGPIXELSX:
	case LOGPIXELSY:
		// Coordinates are device-independent pixels.
		return 96;
	case HORZRES:
		return dc ? DeviceBounds(*dc).width() : DeviceBounds(DC{}).width();
	case VERTRES:
		return dc ? DeviceBounds(*dc).height() : DeviceBounds(DC{}).height();
	case BITSPIXEL:
		return 32;
	case PLANES:
		return 1;
	default:
		return 0;
	}
}

// ---- objects ----------------------------------------------------------------------

HGDIOBJ GetStockObject(int index)
{
	return Stock(index);
}

HBRUSH GetSysColorBrush(int index)
{
	return reinterpret_cast<HBRUSH>(static_cast<GdiObject *>(SysColorBrush(index)));
}

HGDIOBJ SelectObject(HDC hdc, HGDIOBJ object)
{
	DC *dc = ToDC(hdc);
	if(dc == nullptr || object == nullptr)
		return nullptr;
	if(Pen *pen = ToPen(object))
	{
		Pen *old = dc->s.pen;
		dc->s.pen = pen;
		return old;
	}
	if(Brush *brush = ToBrush(object))
	{
		Brush *old = dc->s.brush;
		dc->s.brush = brush;
		return old;
	}
	if(Font *font = ToFont(object))
	{
		Font *old = dc->s.font;
		dc->s.font = font;
		return old;
	}
	if(Bitmap *bitmap = ToBitmap(object))
	{
		if(dc->type != DC::Memory)
			return nullptr;
		if(bitmap->selectedInto != nullptr && bitmap->selectedInto != dc)
			return nullptr; // a bitmap can be in one DC at a time
		Bitmap *old = dc->bitmap;
		if(old && old != bitmap)
		{
			old->EndPainter();
			if(!old->stock)
				old->selectedInto = nullptr;
		}
		dc->bitmap = bitmap;
		if(!bitmap->stock)
			bitmap->selectedInto = dc;
		return old;
	}
	if(Region *region = ToRegion(object))
	{
		return reinterpret_cast<HGDIOBJ>(static_cast<INT_PTR>(::SelectClipRgn(hdc, reinterpret_cast<HRGN>(static_cast<GdiObject *>(region)))));
	}
	return nullptr;
}

HGDIOBJ GetCurrentObject(HDC hdc, UINT type)
{
	DC *dc = ToDC(hdc);
	if(dc == nullptr)
		return nullptr;
	switch(type)
	{
	case OBJ_PEN: return dc->s.pen;
	case OBJ_BRUSH: return dc->s.brush;
	case OBJ_FONT: return dc->s.font;
	case OBJ_BITMAP: return dc->bitmap;
	default: return nullptr;
	}
}

DWORD GetObjectType(HGDIOBJ object)
{
	if(ToDC(static_cast<HDC>(object)))
		return static_cast<DC *>(object)->type == DC::Memory ? OBJ_MEMDC : OBJ_DC;
	auto *o = static_cast<GdiObject *>(object);
	if(o == nullptr || !Objects().count(o))
		return 0;
	switch(o->kind)
	{
	case Kind::Pen: return OBJ_PEN;
	case Kind::Brush: return OBJ_BRUSH;
	case Kind::Font: return OBJ_FONT;
	case Kind::Bitmap: return OBJ_BITMAP;
	case Kind::Region: return OBJ_REGION;
	}
	return 0;
}

BOOL DeleteObject(HGDIOBJ object)
{
	auto *o = static_cast<GdiObject *>(object);
	if(o == nullptr || !Objects().count(o))
		return FALSE;
	if(o->stock)
		return TRUE;
	// Windows refuses to delete objects that are selected into a DC.
	for(DC *dc : DCs())
	{
		if(dc->s.pen == o || dc->s.brush == o || dc->s.font == o || dc->bitmap == o)
			return FALSE;
	}
	Objects().erase(o);
	delete o;
	return TRUE;
}

int GetObject(HANDLE object, int size, LPVOID buffer)
{
	auto *o = static_cast<GdiObject *>(object);
	if(o == nullptr || !Objects().count(o))
		return 0;
	auto put = [&](const auto &value) -> int
	{
		const int needed = static_cast<int>(sizeof(value));
		if(buffer == nullptr)
			return needed;
		const int n = std::min(size, needed);
		std::memcpy(buffer, &value, n);
		return n;
	};
	switch(o->kind)
	{
	case Kind::Pen:
		{
			auto *p = static_cast<Pen *>(o);
			LOGPEN lp{p->style, POINT{p->width, 0}, p->color};
			return put(lp);
		}
	case Kind::Brush:
		{
			auto *b = static_cast<Brush *>(o);
			LOGBRUSH lb{b->style, b->color, static_cast<ULONG_PTR>(b->hatch)};
			return put(lb);
		}
	case Kind::Font:
		return put(static_cast<Font *>(o)->lf);
	case Kind::Bitmap:
		{
			auto *b = static_cast<Bitmap *>(o);
			BITMAP bm{};
			const QSize size = (QSizeF(b->image.size()) / b->image.devicePixelRatio()).toSize();
			bm.bmWidth = size.width();
			bm.bmHeight = size.height();
			bm.bmBitsPixel = static_cast<WORD>(b->bitsPerPixel);
			bm.bmPlanes = 1;
			bm.bmWidthBytes = DibStride(bm.bmWidth, b->bitsPerPixel);
			bm.bmBits = b->dibSection ? b->storage.data() : nullptr;
			return put(bm);
		}
	case Kind::Region:
		return 0;
	}
	return 0;
}

HPEN CreatePen(int style, int width, COLORREF color)
{
	auto *p = Register(new Pen);
	p->style = static_cast<UINT>(style);
	p->width = width;
	p->color = Plain(color);
	return reinterpret_cast<HPEN>(static_cast<GdiObject *>(p));
}

HPEN CreatePenIndirect(const LOGPEN *pen)
{
	if(pen == nullptr)
		return nullptr;
	return CreatePen(static_cast<int>(pen->lopnStyle), pen->lopnWidth.x, pen->lopnColor);
}

HBRUSH CreateSolidBrush(COLORREF color)
{
	auto *b = Register(new Brush);
	b->color = Plain(color);
	return reinterpret_cast<HBRUSH>(static_cast<GdiObject *>(b));
}

HBRUSH CreateHatchBrush(int hatch, COLORREF color)
{
	auto *b = Register(new Brush);
	b->style = BS_HATCHED;
	b->hatch = hatch;
	b->color = Plain(color);
	return reinterpret_cast<HBRUSH>(static_cast<GdiObject *>(b));
}

HBRUSH CreatePatternBrush(HBITMAP bitmap)
{
	Bitmap *bm = ToBitmap(bitmap);
	if(bm == nullptr)
		return nullptr;
	auto *b = Register(new Brush);
	b->style = BS_PATTERN;
	QImage holder = bm->bottomUp ? bm->image.mirrored(false, true) : bm->image.copy();
	if(holder.format() == QImage::Format_Mono)
		holder.setColorTable({qRgb(0, 0, 0), qRgb(255, 255, 255)});
	b->pattern = holder.convertToFormat(QImage::Format_RGB32);
	return reinterpret_cast<HBRUSH>(static_cast<GdiObject *>(b));
}

HBRUSH CreateBrushIndirect(const LOGBRUSH *brush)
{
	if(brush == nullptr)
		return nullptr;
	switch(brush->lbStyle)
	{
	case BS_NULL:
		return reinterpret_cast<HBRUSH>(Stock(NULL_BRUSH));
	case BS_HATCHED:
		return CreateHatchBrush(static_cast<int>(brush->lbHatch), brush->lbColor);
	case BS_PATTERN:
		return CreatePatternBrush(reinterpret_cast<HBITMAP>(brush->lbHatch));
	default:
		return CreateSolidBrush(brush->lbColor);
	}
}

HFONT CreateFontIndirect(const LOGFONT *font)
{
	if(font == nullptr)
		return nullptr;
	auto *f = Register(new Font);
	f->lf = *font;
	f->font = FontFromLogFont(*font);
	return reinterpret_cast<HFONT>(static_cast<GdiObject *>(f));
}

HFONT CreateFont(int height, int width, int escapement, int orientation, int weight, DWORD italic, DWORD underline, DWORD strikeOut,
	DWORD charSet, DWORD outPrecision, DWORD clipPrecision, DWORD quality, DWORD pitchAndFamily, LPCTSTR faceName)
{
	LOGFONTW lf{};
	lf.lfHeight = height;
	lf.lfWidth = width;
	lf.lfEscapement = escapement;
	lf.lfOrientation = orientation;
	lf.lfWeight = weight;
	lf.lfItalic = static_cast<BYTE>(italic);
	lf.lfUnderline = static_cast<BYTE>(underline);
	lf.lfStrikeOut = static_cast<BYTE>(strikeOut);
	lf.lfCharSet = static_cast<BYTE>(charSet);
	lf.lfOutPrecision = static_cast<BYTE>(outPrecision);
	lf.lfClipPrecision = static_cast<BYTE>(clipPrecision);
	lf.lfQuality = static_cast<BYTE>(quality);
	lf.lfPitchAndFamily = static_cast<BYTE>(pitchAndFamily);
	if(faceName)
		std::wcsncpy(lf.lfFaceName, faceName, LF_FACESIZE - 1);
	return CreateFontIndirect(&lf);
}

HBITMAP CreateCompatibleBitmap(HDC, int cx, int cy)
{
	Bitmap *b = NewBitmap(std::max(cx, 1), std::max(cy, 1));
	return reinterpret_cast<HBITMAP>(static_cast<GdiObject *>(b));
}

HBITMAP CreateBitmap(int w, int h, UINT, UINT bitCount, const void *bits)
{
	if(w <= 0 || h <= 0)
		return nullptr;
	Bitmap *b = Register(new Bitmap);
	b->bitsPerPixel = static_cast<int>(bitCount);
	if(bitCount == 1)
	{
		b->image = QImage(w, h, QImage::Format_Mono);
		b->image.setColorTable({qRgb(0, 0, 0), qRgb(255, 255, 255)});
		b->image.fill(0);
		if(bits)
		{
			// Device-dependent bitmap rows are WORD aligned.
			const int stride = ((w + 15) / 16) * 2;
			for(int y = 0; y < h; y++)
				std::memcpy(b->image.scanLine(y), static_cast<const uchar *>(bits) + y * stride, (w + 7) / 8);
		}
	} else
	{
		b->image = QImage(w, h, QImage::Format_RGB32);
		b->image.fill(Qt::black);
		if(bits && bitCount == 32)
		{
			for(int y = 0; y < h; y++)
				std::memcpy(b->image.scanLine(y), static_cast<const uchar *>(bits) + y * w * 4, w * 4);
		}
	}
	return reinterpret_cast<HBITMAP>(static_cast<GdiObject *>(b));
}

HBITMAP CreateDIBSection(HDC, const BITMAPINFO *bmi, UINT, void **bits, HANDLE, DWORD)
{
	if(bits)
		*bits = nullptr;
	if(bmi == nullptr)
		return nullptr;
	const BITMAPINFOHEADER &h = bmi->bmiHeader;
	const int width = h.biWidth;
	const int height = h.biHeight < 0 ? -h.biHeight : h.biHeight;
	const int bpp = h.biBitCount;
	if(width <= 0 || height <= 0)
		return nullptr;
	Bitmap *b = Register(new Bitmap);
	b->dibSection = true;
	b->bitsPerPixel = bpp;
	b->bottomUp = h.biHeight > 0;
	const int stride = DibStride(width, bpp);
	b->storage.assign(static_cast<std::size_t>(stride) * height, 0);
	const QImage::Format format = DibFormat(bpp);
	if(format != QImage::Format_Invalid)
	{
		b->image = QImage(b->storage.data(), width, height, stride, format);
		if(bpp <= 8)
			b->image.setColorTable(DibColorTable(bmi, bpp));
	}
	if(bits)
		*bits = b->storage.data();
	return reinterpret_cast<HBITMAP>(static_cast<GdiObject *>(b));
}

int SetDIBits(HDC, HBITMAP bitmap, UINT start, UINT lines, const void *bits, const BITMAPINFO *bmi, UINT)
{
	Bitmap *b = ToBitmap(bitmap);
	if(b == nullptr || bmi == nullptr || bits == nullptr || b->image.isNull() || !Paintable(b->image))
		return 0;
	const int fullHeight = bmi->bmiHeader.biHeight < 0 ? -bmi->bmiHeader.biHeight : bmi->bmiHeader.biHeight;
	const QImage source = DibImage(bits, bmi, static_cast<int>(lines));
	if(source.isNull())
		return 0;
	// Scan line 'start' counts from the bottom in bottom-up DIBs.
	const int top = bmi->bmiHeader.biHeight > 0 ? fullHeight - static_cast<int>(start) - source.height() : static_cast<int>(start);
	b->EndPainter();
	{
		QPainter p(&b->image);
		p.setCompositionMode(QPainter::CompositionMode_Source);
		p.drawImage(QPoint(0, top), source);
	}
	return source.height();
}

int GetDIBits(HDC, HBITMAP bitmap, UINT start, UINT lines, LPVOID bits, BITMAPINFO *bmi, UINT)
{
	Bitmap *b = ToBitmap(bitmap);
	if(b == nullptr || bmi == nullptr || b->image.isNull())
		return 0;
	BITMAPINFOHEADER &h = bmi->bmiHeader;
	const QImage top = b->bottomUp ? b->image.mirrored(false, true) : b->image;
	if(bits == nullptr)
	{
		h.biWidth = top.width();
		h.biHeight = top.height();
		h.biPlanes = 1;
		if(h.biBitCount == 0)
			h.biBitCount = 32;
		h.biCompression = BI_RGB;
		h.biSizeImage = DibStride(top.width(), h.biBitCount) * top.height();
		return top.height();
	}
	const int bpp = h.biBitCount;
	if(bpp != 32 && bpp != 24)
		return 0;
	const QImage converted = top.convertToFormat(bpp == 32 ? QImage::Format_RGB32 : QImage::Format_BGR888);
	const int width = std::min<int>(h.biWidth, converted.width());
	const int height = h.biHeight < 0 ? -h.biHeight : h.biHeight;
	const int stride = DibStride(h.biWidth, bpp);
	int copied = 0;
	for(UINT i = 0; i < lines; i++)
	{
		const int scan = static_cast<int>(start + i);
		if(scan >= height || scan >= converted.height())
			break;
		const int row = h.biHeight > 0 ? converted.height() - 1 - scan : scan;
		if(row < 0)
			break;
		std::memcpy(static_cast<uchar *>(bits) + i * stride, converted.constScanLine(row), width * bpp / 8);
		copied++;
	}
	return copied;
}

int StretchDIBits(HDC hdc, int xDest, int yDest, int wDest, int hDest, int xSrc, int ySrc, int wSrc, int hSrc,
	const void *bits, const BITMAPINFO *bmi, UINT, DWORD rop)
{
	DC *dc = ToDC(hdc);
	if(dc == nullptr)
		return 0;
	const QImage image = DibImage(bits, bmi);
	if(image.isNull())
		return 0;
	// Source coordinates of bottom-up DIBs count from the bottom.
	if(bmi->bmiHeader.biHeight > 0)
		ySrc = image.height() - ySrc - hSrc;
	return DrawImage(*dc, QRect(xDest, yDest, wDest, hDest), image, QRectF(xSrc, ySrc, wSrc, hSrc), RopMode(rop)) ? hSrc : 0;
}

int SetDIBitsToDevice(HDC hdc, int xDest, int yDest, DWORD w, DWORD h, int xSrc, int ySrc, UINT startScan, UINT lines,
	const void *bits, const BITMAPINFO *bmi, UINT)
{
	DC *dc = ToDC(hdc);
	if(dc == nullptr || bmi == nullptr)
		return 0;
	const int fullHeight = bmi->bmiHeader.biHeight < 0 ? -bmi->bmiHeader.biHeight : bmi->bmiHeader.biHeight;
	const QImage band = DibImage(bits, bmi, static_cast<int>(lines));
	if(band.isNull())
		return 0;
	int top = ySrc;
	if(bmi->bmiHeader.biHeight > 0)
	{
		// Bottom-up: band rows are scans startScan .. startScan + lines - 1,
		// counted from the bottom; ySrc too.
		const int bandTopScan = static_cast<int>(startScan) + band.height() - 1;
		const int srcTopScan = ySrc + static_cast<int>(h) - 1;
		top = bandTopScan - srcTopScan;
	} else
	{
		top = ySrc - static_cast<int>(startScan);
	}
	(void)fullHeight;
	DrawImage(*dc, QRect(xDest, yDest, static_cast<int>(w), static_cast<int>(h)), band, QRectF(xSrc, top, w, h), QPainter::CompositionMode_Source);
	return band.height();
}

// ---- blits --------------------------------------------------------------------------

BOOL BitBlt(HDC dest, int x, int y, int cx, int cy, HDC src, int x1, int y1, DWORD rop)
{
	return Blit(dest, x, y, cx, cy, src, x1, y1, cx, cy, rop);
}

BOOL StretchBlt(HDC dest, int x, int y, int w, int h, HDC src, int xSrc, int ySrc, int wSrc, int hSrc, DWORD rop)
{
	return Blit(dest, x, y, w, h, src, xSrc, ySrc, wSrc, hSrc, rop);
}

BOOL PatBlt(HDC hdc, int x, int y, int w, int h, DWORD rop)
{
	DC *dc = ToDC(hdc);
	if(dc == nullptr)
		return FALSE;
	QPainter *p = Painter(*dc);
	if(p == nullptr)
		return FALSE;
	QRect r(x, y, w, h);
	r = r.normalized();
	switch(rop)
	{
	case BLACKNESS:
		p->fillRect(r, Qt::black);
		break;
	case WHITENESS:
		p->fillRect(r, Qt::white);
		break;
	case DSTINVERT:
		p->setCompositionMode(QPainter::RasterOp_NotDestination);
		p->fillRect(r, Qt::black);
		break;
	case PATINVERT:
		FillArea(*dc, *p, r, dc->s.brush, QPainter::RasterOp_SourceXorDestination);
		break;
	default: // PATCOPY
		FillArea(*dc, *p, r, dc->s.brush, QPainter::CompositionMode_Source);
		break;
	}
	p->setCompositionMode(QPainter::CompositionMode_SourceOver);
	return TRUE;
}

BOOL TransparentBlt(HDC destDC, int x, int y, int w, int h, HDC srcDC, int xSrc, int ySrc, int wSrc, int hSrc, UINT transparent)
{
	DC *dest = ToDC(destDC);
	DC *src = ToDC(srcDC);
	if(dest == nullptr || src == nullptr)
		return FALSE;
	QImage holder;
	const QImage *image = SourceImage(*src, holder);
	if(image == nullptr)
		return FALSE;
	const qreal dpr = image->devicePixelRatio();
	const QRect pixels = QRectF((xSrc + src->s.origin.x()) * dpr, (ySrc + src->s.origin.y()) * dpr, wSrc * dpr, hSrc * dpr).toAlignedRect();
	QImage part = image->copy(pixels).convertToFormat(QImage::Format_ARGB32);
	const QRgb key = ToQColor(Plain(transparent)).rgb() & 0x00FFFFFF;
	for(int row = 0; row < part.height(); row++)
	{
		auto *line = reinterpret_cast<QRgb *>(part.scanLine(row));
		for(int col = 0; col < part.width(); col++)
		{
			if((line[col] & 0x00FFFFFF) == key)
				line[col] = 0;
		}
	}
	part.setDevicePixelRatio(1.0);
	return DrawImage(*dest, QRect(x, y, w, h), part, QRectF(part.rect()), QPainter::CompositionMode_SourceOver);
}

BOOL AlphaBlend(HDC destDC, int x, int y, int w, int h, HDC srcDC, int xSrc, int ySrc, int wSrc, int hSrc, BLENDFUNCTION blend)
{
	DC *dest = ToDC(destDC);
	DC *src = ToDC(srcDC);
	if(dest == nullptr || src == nullptr)
		return FALSE;
	QImage holder;
	const QImage *image = SourceImage(*src, holder);
	if(image == nullptr)
		return FALSE;
	QImage source = *image;
	if(blend.AlphaFormat & AC_SRC_ALPHA)
	{
		// Per-pixel alpha, premultiplied, as AlphaBlend expects.
		if(source.format() == QImage::Format_RGB32)
		{
			source = QImage(image->constBits(), image->width(), image->height(), image->bytesPerLine(), QImage::Format_ARGB32_Premultiplied);
			source.setDevicePixelRatio(image->devicePixelRatio());
		}
	}
	const QRectF sourceRect(xSrc + src->s.origin.x(), ySrc + src->s.origin.y(), wSrc, hSrc);
	return DrawImage(*dest, QRect(x, y, w, h), source, sourceRect, QPainter::CompositionMode_SourceOver, blend.SourceConstantAlpha / 255.0);
}

BOOL GdiFlush()
{
	return TRUE;
}

// ---- attributes ----------------------------------------------------------------------

#define LINOPENMPT_DC_ATTRIBUTE(Type, setter, getter, field, fallback) \
	Type setter(HDC hdc, Type value) \
	{ \
		DC *dc = ToDC(hdc); \
		if(dc == nullptr) \
			return fallback; \
		const Type old = dc->s.field; \
		dc->s.field = value; \
		return old; \
	}

COLORREF SetTextColor(HDC hdc, COLORREF color)
{
	DC *dc = ToDC(hdc);
	if(dc == nullptr)
		return CLR_INVALID;
	const COLORREF old = dc->s.textColor;
	dc->s.textColor = Plain(color);
	return old;
}

COLORREF GetTextColor(HDC hdc)
{
	DC *dc = ToDC(hdc);
	return dc ? dc->s.textColor : CLR_INVALID;
}

COLORREF SetBkColor(HDC hdc, COLORREF color)
{
	DC *dc = ToDC(hdc);
	if(dc == nullptr)
		return CLR_INVALID;
	const COLORREF old = dc->s.bkColor;
	dc->s.bkColor = Plain(color);
	return old;
}

COLORREF GetBkColor(HDC hdc)
{
	DC *dc = ToDC(hdc);
	return dc ? dc->s.bkColor : CLR_INVALID;
}

LINOPENMPT_DC_ATTRIBUTE(int, SetBkMode, GetBkMode, bkMode, 0)
LINOPENMPT_DC_ATTRIBUTE(UINT, SetTextAlign, GetTextAlign, textAlign, GDI_ERROR)
LINOPENMPT_DC_ATTRIBUTE(int, SetROP2, GetROP2, rop2, 0)
LINOPENMPT_DC_ATTRIBUTE(int, SetStretchBltMode, GetStretchBltMode, stretchMode, 0)
#undef LINOPENMPT_DC_ATTRIBUTE

int GetBkMode(HDC hdc)
{
	DC *dc = ToDC(hdc);
	return dc ? dc->s.bkMode : 0;
}

UINT GetTextAlign(HDC hdc)
{
	DC *dc = ToDC(hdc);
	return dc ? dc->s.textAlign : GDI_ERROR;
}

int GetROP2(HDC hdc)
{
	DC *dc = ToDC(hdc);
	return dc ? dc->s.rop2 : 0;
}

int SetMapMode(HDC hdc, int)
{
	// Only MM_TEXT is used.
	return ToDC(hdc) ? MM_TEXT : 0;
}

COLORREF SetDCBrushColor(HDC hdc, COLORREF color)
{
	DC *dc = ToDC(hdc);
	if(dc == nullptr)
		return CLR_INVALID;
	const COLORREF old = dc->s.dcBrushColor;
	dc->s.dcBrushColor = Plain(color);
	return old;
}

COLORREF SetDCPenColor(HDC hdc, COLORREF color)
{
	DC *dc = ToDC(hdc);
	if(dc == nullptr)
		return CLR_INVALID;
	const COLORREF old = dc->s.dcPenColor;
	dc->s.dcPenColor = Plain(color);
	return old;
}

BOOL SetBrushOrgEx(HDC hdc, int, int, LPPOINT old)
{
	if(old)
		*old = POINT{0, 0};
	return ToDC(hdc) != nullptr;
}

BOOL SetViewportOrgEx(HDC hdc, int x, int y, LPPOINT old)
{
	DC *dc = ToDC(hdc);
	if(dc == nullptr)
		return FALSE;
	if(old)
		*old = POINT{dc->s.origin.x(), dc->s.origin.y()};
	dc->s.origin = QPoint(x, y);
	return TRUE;
}

BOOL GetViewportOrgEx(HDC hdc, LPPOINT point)
{
	DC *dc = ToDC(hdc);
	if(dc == nullptr || point == nullptr)
		return FALSE;
	*point = POINT{dc->s.origin.x(), dc->s.origin.y()};
	return TRUE;
}

BOOL OffsetViewportOrgEx(HDC hdc, int dx, int dy, LPPOINT old)
{
	DC *dc = ToDC(hdc);
	if(dc == nullptr)
		return FALSE;
	if(old)
		*old = POINT{dc->s.origin.x(), dc->s.origin.y()};
	dc->s.origin += QPoint(dx, dy);
	return TRUE;
}

BOOL SetWindowOrgEx(HDC hdc, int x, int y, LPPOINT old)
{
	// In MM_TEXT, a window origin of (x, y) is a viewport origin of (-x, -y).
	DC *dc = ToDC(hdc);
	if(dc == nullptr)
		return FALSE;
	if(old)
		*old = POINT{-dc->s.origin.x(), -dc->s.origin.y()};
	dc->s.origin = QPoint(-x, -y);
	return TRUE;
}

BOOL LPtoDP(HDC hdc, LPPOINT points, int count)
{
	DC *dc = ToDC(hdc);
	if(dc == nullptr || points == nullptr)
		return FALSE;
	for(int i = 0; i < count; i++)
	{
		points[i].x += dc->s.origin.x();
		points[i].y += dc->s.origin.y();
	}
	return TRUE;
}

BOOL DPtoLP(HDC hdc, LPPOINT points, int count)
{
	DC *dc = ToDC(hdc);
	if(dc == nullptr || points == nullptr)
		return FALSE;
	for(int i = 0; i < count; i++)
	{
		points[i].x -= dc->s.origin.x();
		points[i].y -= dc->s.origin.y();
	}
	return TRUE;
}

// ---- pixels, lines and shapes -----------------------------------------------------------

COLORREF SetPixel(HDC hdc, int x, int y, COLORREF color)
{
	DC *dc = ToDC(hdc);
	if(dc == nullptr)
		return CLR_INVALID;
	if(QPainter *p = Painter(*dc))
	{
		p->fillRect(QRect(x, y, 1, 1), ToQColor(Plain(color)));
		return Plain(color);
	}
	return CLR_INVALID;
}

BOOL SetPixelV(HDC hdc, int x, int y, COLORREF color)
{
	return SetPixel(hdc, x, y, color) != CLR_INVALID;
}

COLORREF GetPixel(HDC hdc, int x, int y)
{
	DC *dc = ToDC(hdc);
	if(dc == nullptr)
		return CLR_INVALID;
	QImage holder;
	const QImage *image = SourceImage(*dc, holder);
	if(image == nullptr)
		return CLR_INVALID;
	const qreal dpr = image->devicePixelRatio();
	// The device pixel at the centre of the logical one.
	const QPoint pt(static_cast<int>((x + dc->s.origin.x() + 0.5) * dpr), static_cast<int>((y + dc->s.origin.y() + 0.5) * dpr));
	if(!image->rect().contains(pt))
		return CLR_INVALID;
	return ToColorRef(image->pixelColor(pt));
}

BOOL MoveToEx(HDC hdc, int x, int y, LPPOINT old)
{
	DC *dc = ToDC(hdc);
	if(dc == nullptr)
		return FALSE;
	if(old)
		*old = POINT{dc->s.position.x(), dc->s.position.y()};
	dc->s.position = QPoint(x, y);
	return TRUE;
}

BOOL GetCurrentPositionEx(HDC hdc, LPPOINT point)
{
	DC *dc = ToDC(hdc);
	if(dc == nullptr || point == nullptr)
		return FALSE;
	*point = POINT{dc->s.position.x(), dc->s.position.y()};
	return TRUE;
}

BOOL LineTo(HDC hdc, int x, int y)
{
	DC *dc = ToDC(hdc);
	if(dc == nullptr)
		return FALSE;
	const QPoint from = dc->s.position;
	dc->s.position = QPoint(x, y);
	if(QPainter *p = Painter(*dc))
		DrawLine(*dc, *p, from, QPoint(x, y));
	return TRUE;
}

BOOL Polyline(HDC hdc, const POINT *points, int count)
{
	DC *dc = ToDC(hdc);
	if(dc == nullptr || points == nullptr || count < 2)
		return FALSE;
	if(QPainter *p = Painter(*dc))
	{
		for(int i = 1; i < count; i++)
			DrawLine(*dc, *p, QPoint(points[i - 1].x, points[i - 1].y), QPoint(points[i].x, points[i].y));
	}
	return TRUE;
}

BOOL PolyPolyline(HDC hdc, const POINT *points, const DWORD *counts, int polylines)
{
	if(points == nullptr || counts == nullptr)
		return FALSE;
	for(int i = 0; i < polylines; i++)
	{
		Polyline(hdc, points, static_cast<int>(counts[i]));
		points += counts[i];
	}
	return TRUE;
}

BOOL Polygon(HDC hdc, const POINT *points, int count)
{
	DC *dc = ToDC(hdc);
	if(dc == nullptr || points == nullptr || count < 2)
		return FALSE;
	QPainter *p = Painter(*dc);
	if(p == nullptr)
		return TRUE;
	QPolygon polygon;
	for(int i = 0; i < count; i++)
		polygon << QPoint(points[i].x, points[i].y);
	p->setCompositionMode(Rop2Mode(dc->s.rop2));
	p->setPen(MakePen(*dc));
	p->setBrush(MakeBrush(*dc, dc->s.brush));
	p->drawPolygon(polygon, Qt::OddEvenFill);
	p->setBrush(Qt::NoBrush);
	p->setCompositionMode(QPainter::CompositionMode_SourceOver);
	return TRUE;
}

namespace
{
enum class Shape { Rect, Ellipse, RoundRect };
BOOL DrawShape(HDC hdc, int left, int top, int right, int bottom, Shape shape, int rw = 0, int rh = 0)
{
	DC *dc = ToDC(hdc);
	if(dc == nullptr)
		return FALSE;
	QPainter *p = Painter(*dc);
	if(p == nullptr)
		return TRUE;
	// GDI shapes cover [left, right - 1] x [top, bottom - 1], pen included.
	QRect outer = RectOf(left, top, right, bottom);
	if(outer.isEmpty())
		return TRUE;
	p->setCompositionMode(Rop2Mode(dc->s.rop2));
	if(shape == Shape::Rect && !(HasPen(*dc) && PenWidth(*dc) > 1))
	{
		// Exact pixels for the common case.
		QRect fill = HasPen(*dc) ? outer.adjusted(1, 1, -1, -1) : outer;
		if(HasBrush(dc->s.brush) && !fill.isEmpty())
			FillArea(*dc, *p, fill, dc->s.brush, Rop2Mode(dc->s.rop2));
		if(HasPen(*dc))
		{
			const QColor c = Rop2Color(*dc, PenColor(*dc));
			p->setCompositionMode(Rop2Mode(dc->s.rop2));
			p->fillRect(QRect(outer.left(), outer.top(), outer.width(), 1), c);
			if(outer.height() > 1)
				p->fillRect(QRect(outer.left(), outer.bottom(), outer.width(), 1), c);
			if(outer.height() > 2)
			{
				p->fillRect(QRect(outer.left(), outer.top() + 1, 1, outer.height() - 2), c);
				if(outer.width() > 1)
					p->fillRect(QRect(outer.right(), outer.top() + 1, 1, outer.height() - 2), c);
			}
		}
	} else
	{
		// A stroke centred on a rectangle inset by half the pen width covers
		// exactly the GDI area.
		QRectF r(outer);
		if(HasPen(*dc))
		{
			const qreal half = PenWidth(*dc) / 2.0;
			r.adjust(half, half, -half, -half);
		}
		QPen pen = MakePen(*dc);
		pen.setJoinStyle(Qt::MiterJoin);
		p->setPen(pen);
		p->setBrush(MakeBrush(*dc, dc->s.brush));
		switch(shape)
		{
		case Shape::Rect: p->drawRect(r); break;
		case Shape::Ellipse: p->drawEllipse(r); break;
		case Shape::RoundRect: p->drawRoundedRect(r, rw / 2.0, rh / 2.0); break;
		}
		p->setBrush(Qt::NoBrush);
	}
	p->setCompositionMode(QPainter::CompositionMode_SourceOver);
	return TRUE;
}
} // namespace

BOOL Rectangle(HDC hdc, int left, int top, int right, int bottom)
{
	return DrawShape(hdc, left, top, right, bottom, Shape::Rect);
}

BOOL Ellipse(HDC hdc, int left, int top, int right, int bottom)
{
	return DrawShape(hdc, left, top, right, bottom, Shape::Ellipse);
}

BOOL RoundRect(HDC hdc, int left, int top, int right, int bottom, int w, int h)
{
	return DrawShape(hdc, left, top, right, bottom, Shape::RoundRect, w, h);
}

int FillRect(HDC hdc, const RECT *rect, HBRUSH brush)
{
	DC *dc = ToDC(hdc);
	if(dc == nullptr || rect == nullptr)
		return 0;
	if(QPainter *p = Painter(*dc))
		FillArea(*dc, *p, RectOf(*rect), ToBrush(brush), QPainter::CompositionMode_Source);
	return 1;
}

int FrameRect(HDC hdc, const RECT *rect, HBRUSH brush)
{
	DC *dc = ToDC(hdc);
	if(dc == nullptr || rect == nullptr)
		return 0;
	const QRect r = RectOf(*rect);
	if(r.isEmpty())
		return 1;
	if(QPainter *p = Painter(*dc))
	{
		QRegion frame(r);
		frame -= r.adjusted(1, 1, -1, -1);
		FillArea(*dc, *p, frame, ToBrush(brush), QPainter::CompositionMode_Source);
	}
	return 1;
}

BOOL InvertRect(HDC hdc, const RECT *rect)
{
	DC *dc = ToDC(hdc);
	if(dc == nullptr || rect == nullptr)
		return FALSE;
	if(QPainter *p = Painter(*dc))
	{
		p->setCompositionMode(QPainter::RasterOp_NotDestination);
		p->fillRect(RectOf(*rect), Qt::black);
		p->setCompositionMode(QPainter::CompositionMode_SourceOver);
	}
	return TRUE;
}

BOOL DrawFocusRect(HDC hdc, const RECT *rect)
{
	DC *dc = ToDC(hdc);
	if(dc == nullptr || rect == nullptr)
		return FALSE;
	const QRect r = RectOf(*rect);
	if(r.isEmpty())
		return TRUE;
	if(QPainter *p = Painter(*dc))
	{
		// A dotted XOR frame, so drawing it twice removes it.
		p->setCompositionMode(QPainter::RasterOp_SourceXorDestination);
		const QColor c = Qt::white;
		for(int x = r.left(); x <= r.right(); x += 2)
		{
			p->fillRect(QRect(x, r.top(), 1, 1), c);
			if(r.height() > 1)
				p->fillRect(QRect(x, r.bottom(), 1, 1), c);
		}
		for(int y = r.top() + 2; y < r.bottom(); y += 2)
		{
			p->fillRect(QRect(r.left(), y, 1, 1), c);
			if(r.width() > 1)
				p->fillRect(QRect(r.right(), y, 1, 1), c);
		}
		p->setCompositionMode(QPainter::CompositionMode_SourceOver);
	}
	return TRUE;
}

BOOL DrawEdge(HDC hdc, LPRECT rect, UINT edge, UINT flags)
{
	DC *dc = ToDC(hdc);
	if(dc == nullptr || rect == nullptr)
		return FALSE;
	QPainter *p = Painter(*dc);
	QRect r = RectOf(*rect);
	auto line = [&](const QRect &part, int color)
	{
		if(p)
			p->fillRect(part, ToQColor(::GetSysColor(color)));
	};
	// Outer then inner border, each with a top-left and bottom-right colour.
	struct Border { UINT raised, sunken; int raisedTL, raisedBR, sunkenTL, sunkenBR; };
	const Border borders[2] = {
		{BDR_RAISEDOUTER, BDR_SUNKENOUTER, COLOR_3DLIGHT, COLOR_3DDKSHADOW, COLOR_3DSHADOW, COLOR_3DHIGHLIGHT},
		{BDR_RAISEDINNER, BDR_SUNKENINNER, COLOR_3DHIGHLIGHT, COLOR_3DSHADOW, COLOR_3DDKSHADOW, COLOR_3DLIGHT},
	};
	for(const Border &b : borders)
	{
		if(!(edge & (b.raised | b.sunken)) || r.isEmpty())
			continue;
		int tl = (edge & b.raised) ? b.raisedTL : b.sunkenTL;
		int br = (edge & b.raised) ? b.raisedBR : b.sunkenBR;
		if(flags & (BF_FLAT | BF_MONO))
			tl = br = (flags & BF_MONO) ? COLOR_WINDOWFRAME : COLOR_3DSHADOW;
		if(flags & BF_TOP)
			line(QRect(r.left(), r.top(), r.width(), 1), tl);
		if(flags & BF_LEFT)
			line(QRect(r.left(), r.top(), 1, r.height()), tl);
		if(flags & BF_BOTTOM)
			line(QRect(r.left(), r.bottom(), r.width(), 1), br);
		if(flags & BF_RIGHT)
			line(QRect(r.right(), r.top(), 1, r.height()), br);
		r.adjust((flags & BF_LEFT) ? 1 : 0, (flags & BF_TOP) ? 1 : 0, (flags & BF_RIGHT) ? -1 : 0, (flags & BF_BOTTOM) ? -1 : 0);
	}
	if((flags & BF_MIDDLE) && !r.isEmpty())
		line(r, (flags & BF_MONO) ? COLOR_WINDOW : COLOR_BTNFACE);
	if(flags & BF_ADJUST)
		*rect = ToRECT(r);
	return TRUE;
}

BOOL DrawFrameControl(HDC hdc, LPRECT rect, UINT type, UINT state)
{
	DC *dc = ToDC(hdc);
	if(dc == nullptr || rect == nullptr)
		return FALSE;
	QPainter *p = Painter(*dc);
	if(p == nullptr)
		return TRUE;
	// The native style draws the control, so it matches the Qt widgets.
	QStyle *style = QApplication::style();
	QStyleOptionButton option;
	option.rect = RectOf(*rect);
	option.state = (state & DFCS_INACTIVE) ? QStyle::State_None : QStyle::State_Enabled;
	if(state & DFCS_PUSHED)
		option.state |= QStyle::State_Sunken;
	else
		option.state |= QStyle::State_Raised;
	option.state |= (state & DFCS_CHECKED) ? QStyle::State_On : QStyle::State_Off;
	if(state & DFCS_FLAT)
		option.features |= QStyleOptionButton::Flat;
	option.palette = QApplication::palette();
	if(type == DFC_BUTTON)
	{
		if((state & 0x1F) == DFCS_BUTTONPUSH)
			style->drawControl(QStyle::CE_PushButtonBevel, &option, p);
		else if((state & 0x0F) == DFCS_BUTTONRADIO)
			style->drawPrimitive(QStyle::PE_IndicatorRadioButton, &option, p);
		else
			style->drawPrimitive(QStyle::PE_IndicatorCheckBox, &option, p);
	} else if(type == DFC_SCROLL && (state & (DFCS_SCROLLSIZEGRIP | DFCS_SCROLLSIZEGRIPRIGHT)))
	{
		QStyleOptionSizeGrip grip;
		grip.rect = option.rect;
		grip.state = option.state;
		grip.palette = option.palette;
		grip.corner = (state & DFCS_SCROLLSIZEGRIPRIGHT) ? Qt::BottomLeftCorner : Qt::BottomRightCorner;
		style->drawControl(QStyle::CE_SizeGrip, &grip, p);
	} else if(type == DFC_SCROLL)
	{
		style->drawPrimitive(QStyle::PE_PanelButtonCommand, &option, p);
		QStyle::PrimitiveElement arrow = QStyle::PE_IndicatorArrowDown;
		switch(state & 0x1F)
		{
		case DFCS_SCROLLUP: arrow = QStyle::PE_IndicatorArrowUp; break;
		case DFCS_SCROLLLEFT: arrow = QStyle::PE_IndicatorArrowLeft; break;
		case DFCS_SCROLLRIGHT: arrow = QStyle::PE_IndicatorArrowRight; break;
		default: break;
		}
		style->drawPrimitive(arrow, &option, p);
	} else
	{
		style->drawPrimitive(QStyle::PE_PanelButtonCommand, &option, p);
	}
	return TRUE;
}

BOOL DrawIconEx(HDC hdc, int x, int y, HICON icon, int cx, int cy, UINT, HBRUSH, UINT)
{
	DC *dc = ToDC(hdc);
	auto *data = reinterpret_cast<IconData *>(icon);
	if(dc == nullptr || data == nullptr || data->image.isNull())
		return FALSE;
	const QSize size = (QSizeF(data->image.size()) / data->image.devicePixelRatio()).toSize();
	if(cx <= 0)
		cx = size.width();
	if(cy <= 0)
		cy = size.height();
	if(QPainter *p = Painter(*dc))
	{
		p->setRenderHint(QPainter::SmoothPixmapTransform, true);
		p->drawImage(QRect(x, y, cx, cy), data->image);
	}
	return TRUE;
}

BOOL DrawIcon(HDC hdc, int x, int y, HICON icon)
{
	return DrawIconEx(hdc, x, y, icon, 0, 0, 0, nullptr, DI_NORMAL);
}

// ---- text --------------------------------------------------------------------------

namespace
{
QString TextOf(LPCTSTR text, int count)
{
	return ToQString(text, count);
}

int DrawTextImpl(HDC hdc, QString text, LPRECT rect, UINT format)
{
	DC *dc = ToDC(hdc);
	if(dc == nullptr || rect == nullptr)
		return 0;
	const QFont font = DCFont(*dc);
	const QFontMetrics fm(font);
	const bool singleLine = (format & DT_SINGLELINE) != 0;
	int flags = 0;
	if(format & DT_CENTER)
		flags |= Qt::AlignHCenter;
	else if(format & DT_RIGHT)
		flags |= Qt::AlignRight;
	else
		flags |= Qt::AlignLeft;
	if(singleLine)
	{
		flags |= Qt::TextSingleLine;
		if(format & DT_VCENTER)
			flags |= Qt::AlignVCenter;
		else if(format & DT_BOTTOM)
			flags |= Qt::AlignBottom;
		else
			flags |= Qt::AlignTop;
	} else
	{
		flags |= Qt::AlignTop;
	}
	if(format & DT_WORDBREAK)
		flags |= Qt::TextWordWrap;
	if(format & DT_EXPANDTABS)
		flags |= Qt::TextExpandTabs;
	if(!(format & DT_NOPREFIX))
		flags |= (format & DT_HIDEPREFIX) ? Qt::TextHideMnemonic : Qt::TextShowMnemonic;
	if(format & DT_NOCLIP)
		flags |= Qt::TextDontClip;
	text.remove(QLatin1Char('\r'));
	QRect r(rect->left, rect->top, rect->right - rect->left, rect->bottom - rect->top);
	if(format & (DT_END_ELLIPSIS | DT_WORD_ELLIPSIS | DT_PATH_ELLIPSIS))
	{
		const Qt::TextElideMode mode = (format & DT_PATH_ELLIPSIS) ? Qt::ElideMiddle : Qt::ElideRight;
		QStringList lines = text.split(QLatin1Char('\n'));
		for(QString &line : lines)
			line = fm.elidedText(line, mode, std::max(0, r.width()), flags & Qt::TextShowMnemonic);
		text = lines.join(QLatin1Char('\n'));
	}
	if(format & DT_CALCRECT)
	{
		const int width = (format & DT_WORDBREAK) ? std::max(r.width(), 1) : INT_MAX / 4;
		const QRect b = fm.boundingRect(QRect(r.left(), r.top(), width, INT_MAX / 4), (flags & ~(Qt::AlignHorizontal_Mask | Qt::AlignVertical_Mask)) | Qt::AlignLeft | Qt::AlignTop, text);
		const int height = text.isEmpty() ? fm.height() : b.height();
		rect->right = rect->left + (text.isEmpty() ? 0 : b.width());
		rect->bottom = rect->top + height;
		return height;
	}
	const QRect bound = fm.boundingRect(r, flags, text);
	if(QPainter *p = Painter(*dc))
	{
		if(!(format & DT_NOCLIP))
			p->setClipRect(r, Qt::IntersectClip);
		if(dc->s.bkMode == OPAQUE)
			p->fillRect((format & DT_NOCLIP) ? bound : (bound & r), ToQColor(dc->s.bkColor));
		p->setFont(font);
		p->setPen(ToQColor(dc->s.textColor));
		p->drawText(r, flags, text);
	}
	if(format & (DT_VCENTER | DT_BOTTOM))
		return bound.bottom() + 1 - r.top();
	return bound.height();
}

BOOL TextOutImpl(HDC hdc, int x, int y, UINT options, const RECT *rect, const QString &text, const INT *dx)
{
	DC *dc = ToDC(hdc);
	if(dc == nullptr)
		return FALSE;
	QPainter *p = Painter(*dc);
	if(p == nullptr)
		return TRUE;
	const QFont font = DCFont(*dc);
	const QFontMetrics fm(font);
	const UINT align = dc->s.textAlign;
	if(align & TA_UPDATECP)
	{
		x = dc->s.position.x();
		y = dc->s.position.y();
	}
	int width = 0;
	if(dx)
	{
		for(int i = 0; i < text.size(); i++)
			width += dx[i];
	} else
	{
		width = fm.horizontalAdvance(text);
	}
	if((align & TA_CENTER) == TA_CENTER)
		x -= width / 2;
	else if(align & TA_RIGHT)
		x -= width;
	int top = y;
	if((align & TA_BASELINE) == TA_BASELINE)
		top = y - fm.ascent();
	else if(align & TA_BOTTOM)
		top = y - fm.height();
	if(rect && (options & ETO_OPAQUE))
		p->fillRect(RectOf(*rect), ToQColor(dc->s.bkColor));
	if(rect && (options & ETO_CLIPPED))
		p->setClipRect(RectOf(*rect), Qt::IntersectClip);
	if(!text.isEmpty())
	{
		if(dc->s.bkMode == OPAQUE && !(rect && (options & ETO_OPAQUE)))
			p->fillRect(QRect(x, top, width, fm.height()), ToQColor(dc->s.bkColor));
		p->setFont(font);
		p->setPen(ToQColor(dc->s.textColor));
		if(dx)
		{
			int pos = x;
			for(int i = 0; i < text.size(); i++)
			{
				p->drawText(QPoint(pos, top + fm.ascent()), QString(text[i]));
				pos += dx[i];
			}
		} else
		{
			p->drawText(QPoint(x, top + fm.ascent()), text);
		}
	}
	if(align & TA_UPDATECP)
		dc->s.position.setX(((align & TA_RIGHT) && !((align & TA_CENTER) == TA_CENTER)) ? x : x + width);
	return TRUE;
}
} // namespace

int DrawText(HDC hdc, LPCTSTR text, int count, LPRECT rect, UINT format)
{
	return DrawTextImpl(hdc, TextOf(text, count), rect, format);
}

int DrawTextW(HDC hdc, LPCWSTR text, int count, LPRECT rect, UINT format)
{
	return DrawTextImpl(hdc, ToQString(text, count), rect, format);
}

int DrawTextA(HDC hdc, LPCSTR text, int count, LPRECT rect, UINT format)
{
	if(text == nullptr)
		return 0;
	return DrawTextImpl(hdc, QString::fromLocal8Bit(text, count < 0 ? static_cast<int>(std::strlen(text)) : count), rect, format);
}

BOOL TextOut(HDC hdc, int x, int y, LPCTSTR text, int count)
{
	return TextOutImpl(hdc, x, y, 0, nullptr, TextOf(text, count), nullptr);
}

BOOL ExtTextOut(HDC hdc, int x, int y, UINT options, const RECT *rect, LPCTSTR text, UINT count, const INT *dx)
{
	return TextOutImpl(hdc, x, y, options, rect, text ? TextOf(text, static_cast<int>(count)) : QString(), dx);
}

BOOL GetTextExtentPoint32(HDC hdc, LPCTSTR text, int count, LPSIZE size)
{
	DC *dc = ToDC(hdc);
	if(size == nullptr)
		return FALSE;
	const QFontMetrics fm(dc ? DCFont(*dc) : DefaultFont());
	const QString s = TextOf(text, count);
	size->cx = fm.horizontalAdvance(s);
	size->cy = fm.height();
	return TRUE;
}

BOOL GetTextMetrics(HDC hdc, LPTEXTMETRIC tm)
{
	DC *dc = ToDC(hdc);
	if(tm == nullptr)
		return FALSE;
	const QFont font = dc ? DCFont(*dc) : DefaultFont();
	const QFontMetrics fm(font);
	const QFontInfo info(font);
	*tm = TEXTMETRIC{};
	tm->tmHeight = fm.height();
	tm->tmAscent = fm.ascent();
	tm->tmDescent = fm.descent();
	tm->tmInternalLeading = std::max(0, fm.height() - info.pixelSize());
	tm->tmExternalLeading = std::max(0, fm.leading());
	tm->tmAveCharWidth = fm.averageCharWidth();
	tm->tmMaxCharWidth = fm.maxWidth();
	tm->tmWeight = font.weight();
	tm->tmDigitizedAspectX = 96;
	tm->tmDigitizedAspectY = 96;
	tm->tmFirstChar = 0x20;
	tm->tmLastChar = 0xFFFC;
	tm->tmDefaultChar = L'?';
	tm->tmBreakChar = L' ';
	tm->tmItalic = font.italic();
	tm->tmUnderlined = font.underline();
	tm->tmStruckOut = font.strikeOut();
	// TMPF_FIXED_PITCH (1) is set for variable-pitch fonts, as on Windows.
	tm->tmPitchAndFamily = info.fixedPitch() ? 0 : 1;
	tm->tmCharSet = ANSI_CHARSET;
	return TRUE;
}

int GetTextFace(HDC hdc, int count, LPTSTR face)
{
	DC *dc = ToDC(hdc);
	const std::wstring name = ToWString(QFontInfo(dc ? DCFont(*dc) : DefaultFont()).family());
	if(face == nullptr)
		return static_cast<int>(name.size()) + 1;
	if(count <= 0)
		return 0;
	const int n = std::min<int>(count - 1, static_cast<int>(name.size()));
	std::wmemcpy(face, name.c_str(), n);
	face[n] = 0;
	return n;
}

// ---- clipping and regions -------------------------------------------------------------

int IntersectClipRect(HDC hdc, int left, int top, int right, int bottom)
{
	DC *dc = ToDC(hdc);
	if(dc == nullptr)
		return 0;
	const QRect r = RectOf(left, top, right, bottom).translated(dc->s.origin);
	dc->s.clip = (dc->s.hasClip ? dc->s.clip : QRegion(DeviceBounds(*dc))) & r;
	dc->s.hasClip = true;
	return RegionType(EffectiveClip(*dc));
}

int ExcludeClipRect(HDC hdc, int left, int top, int right, int bottom)
{
	DC *dc = ToDC(hdc);
	if(dc == nullptr)
		return 0;
	const QRect r = RectOf(left, top, right, bottom).translated(dc->s.origin);
	dc->s.clip = (dc->s.hasClip ? dc->s.clip : QRegion(DeviceBounds(*dc))) - r;
	dc->s.hasClip = true;
	return RegionType(EffectiveClip(*dc));
}

int SelectClipRgn(HDC hdc, HRGN region)
{
	DC *dc = ToDC(hdc);
	if(dc == nullptr)
		return 0;
	if(region == nullptr)
	{
		dc->s.hasClip = false;
		dc->s.clip = QRegion();
	} else if(Region *r = ToRegion(region))
	{
		// Clip regions are in device coordinates.
		dc->s.clip = r->region;
		dc->s.hasClip = true;
	} else
	{
		return 0;
	}
	return RegionType(EffectiveClip(*dc));
}

int GetClipRgn(HDC hdc, HRGN region)
{
	DC *dc = ToDC(hdc);
	Region *r = ToRegion(region);
	if(dc == nullptr || r == nullptr)
		return -1;
	if(!dc->s.hasClip)
		return 0;
	r->region = dc->s.clip;
	return 1;
}

int GetClipBox(HDC hdc, LPRECT rect)
{
	DC *dc = ToDC(hdc);
	if(dc == nullptr || rect == nullptr)
		return 0;
	const QRegion clip = EffectiveClip(*dc);
	*rect = ToRECT(clip.boundingRect().translated(-dc->s.origin));
	return RegionType(clip);
}

BOOL RectVisible(HDC hdc, const RECT *rect)
{
	DC *dc = ToDC(hdc);
	if(dc == nullptr || rect == nullptr)
		return FALSE;
	return EffectiveClip(*dc).intersects(RectOf(*rect).translated(dc->s.origin));
}

BOOL PtVisible(HDC hdc, int x, int y)
{
	DC *dc = ToDC(hdc);
	if(dc == nullptr)
		return FALSE;
	return EffectiveClip(*dc).contains(QPoint(x, y) + dc->s.origin);
}

HRGN CreateRectRgn(int left, int top, int right, int bottom)
{
	auto *r = Register(new Region);
	r->region = QRegion(RectOf(left, top, right, bottom));
	return reinterpret_cast<HRGN>(static_cast<GdiObject *>(r));
}

HRGN CreateRectRgnIndirect(const RECT *rect)
{
	if(rect == nullptr)
		return nullptr;
	return CreateRectRgn(rect->left, rect->top, rect->right, rect->bottom);
}

int CombineRgn(HRGN dest, HRGN src1, HRGN src2, int mode)
{
	Region *d = ToRegion(dest);
	Region *a = ToRegion(src1);
	Region *b = ToRegion(src2);
	if(d == nullptr || a == nullptr || (mode != RGN_COPY && b == nullptr))
		return 0;
	switch(mode)
	{
	case RGN_AND: d->region = a->region & b->region; break;
	case RGN_OR: d->region = a->region | b->region; break;
	case RGN_XOR: d->region = a->region ^ b->region; break;
	case RGN_DIFF: d->region = a->region - b->region; break;
	case RGN_COPY: d->region = a->region; break;
	default: return 0;
	}
	return RegionType(d->region);
}

BOOL PtInRegion(HRGN region, int x, int y)
{
	Region *r = ToRegion(region);
	return r && r->region.contains(QPoint(x, y));
}

int OffsetRgn(HRGN region, int dx, int dy)
{
	Region *r = ToRegion(region);
	if(r == nullptr)
		return 0;
	r->region.translate(dx, dy);
	return RegionType(r->region);
}

int GetRgnBox(HRGN region, LPRECT rect)
{
	Region *r = ToRegion(region);
	if(r == nullptr || rect == nullptr)
		return 0;
	*rect = ToRECT(r->region.boundingRect());
	return RegionType(r->region);
}

BOOL FillRgn(HDC hdc, HRGN region, HBRUSH brush)
{
	DC *dc = ToDC(hdc);
	Region *r = ToRegion(region);
	if(dc == nullptr || r == nullptr)
		return FALSE;
	if(QPainter *p = Painter(*dc))
	{
		// Regions are in device coordinates.
		p->resetTransform();
		FillArea(*dc, *p, r->region, ToBrush(brush), QPainter::CompositionMode_Source);
	}
	return TRUE;
}

BOOL PaintRgn(HDC hdc, HRGN region)
{
	DC *dc = ToDC(hdc);
	if(dc == nullptr)
		return FALSE;
	return FillRgn(hdc, region, reinterpret_cast<HBRUSH>(static_cast<GdiObject *>(dc->s.brush)));
}

// ---- rectangles ------------------------------------------------------------------------

BOOL SetRect(LPRECT rect, int left, int top, int right, int bottom)
{
	if(rect == nullptr)
		return FALSE;
	*rect = RECT{left, top, right, bottom};
	return TRUE;
}

BOOL SetRectEmpty(LPRECT rect)
{
	return SetRect(rect, 0, 0, 0, 0);
}

BOOL CopyRect(LPRECT dest, const RECT *src)
{
	if(dest == nullptr || src == nullptr)
		return FALSE;
	*dest = *src;
	return TRUE;
}

BOOL InflateRect(LPRECT rect, int dx, int dy)
{
	if(rect == nullptr)
		return FALSE;
	rect->left -= dx;
	rect->top -= dy;
	rect->right += dx;
	rect->bottom += dy;
	return TRUE;
}

BOOL OffsetRect(LPRECT rect, int dx, int dy)
{
	if(rect == nullptr)
		return FALSE;
	rect->left += dx;
	rect->top += dy;
	rect->right += dx;
	rect->bottom += dy;
	return TRUE;
}

BOOL IsRectEmpty(const RECT *rect)
{
	return rect == nullptr || rect->right <= rect->left || rect->bottom <= rect->top;
}

BOOL PtInRect(const RECT *rect, POINT point)
{
	return rect != nullptr && point.x >= rect->left && point.x < rect->right && point.y >= rect->top && point.y < rect->bottom;
}

BOOL IntersectRect(LPRECT dest, const RECT *a, const RECT *b)
{
	if(dest == nullptr || a == nullptr || b == nullptr)
		return FALSE;
	RECT r{std::max(a->left, b->left), std::max(a->top, b->top), std::min(a->right, b->right), std::min(a->bottom, b->bottom)};
	if(IsRectEmpty(&r))
	{
		*dest = RECT{};
		return FALSE;
	}
	*dest = r;
	return TRUE;
}

BOOL UnionRect(LPRECT dest, const RECT *a, const RECT *b)
{
	if(dest == nullptr || a == nullptr || b == nullptr)
		return FALSE;
	if(IsRectEmpty(a))
	{
		*dest = *b;
	} else if(IsRectEmpty(b))
	{
		*dest = *a;
	} else
	{
		*dest = RECT{std::min(a->left, b->left), std::min(a->top, b->top), std::max(a->right, b->right), std::max(a->bottom, b->bottom)};
	}
	return !IsRectEmpty(dest);
}

BOOL SubtractRect(LPRECT dest, const RECT *a, const RECT *b)
{
	if(dest == nullptr || a == nullptr || b == nullptr)
		return FALSE;
	// Only shrinks a when b covers a full side of it, as on Windows.
	RECT r = *a;
	RECT i;
	if(IntersectRect(&i, a, b))
	{
		if(i.top == r.top && i.bottom == r.bottom)
		{
			if(i.left == r.left)
				r.left = i.right;
			else if(i.right == r.right)
				r.right = i.left;
		} else if(i.left == r.left && i.right == r.right)
		{
			if(i.top == r.top)
				r.top = i.bottom;
			else if(i.bottom == r.bottom)
				r.bottom = i.top;
		}
	}
	*dest = r;
	return !IsRectEmpty(dest);
}

BOOL EqualRect(const RECT *a, const RECT *b)
{
	return a && b && a->left == b->left && a->top == b->top && a->right == b->right && a->bottom == b->bottom;
}
