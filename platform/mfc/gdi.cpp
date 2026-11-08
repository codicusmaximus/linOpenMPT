// linOpenMPT MFC layer: CDC and the GDI object classes, on the Win32 GDI API
// as in MFC.
#include "MfcInternal.h"

#include <unordered_map>
#include <vector>

namespace linopenmpt::mfc
{
namespace
{

// Temporary wrappers for handles (FromHandle), deleted at idle time.
std::unordered_map<HGDIOBJ, std::unique_ptr<CGdiObject>> &TempGdiObjects()
{
	static auto *map = new std::unordered_map<HGDIOBJ, std::unique_ptr<CGdiObject>>;
	return *map;
}

std::unordered_map<HDC, std::unique_ptr<CDC>> &TempDCs()
{
	static auto *map = new std::unordered_map<HDC, std::unique_ptr<CDC>>;
	return *map;
}

std::unordered_map<HMENU, std::unique_ptr<CMenu>> &TempMenus()
{
	static auto *map = new std::unordered_map<HMENU, std::unique_ptr<CMenu>>;
	return *map;
}

template <typename T>
T *TempGdi(HGDIOBJ handle)
{
	if(handle == nullptr)
		return nullptr;
	auto &map = TempGdiObjects();
	auto &object = map[handle];
	if(!object || dynamic_cast<T *>(object.get()) == nullptr)
	{
		if(object)
			object->m_hObject = nullptr;
		auto created = std::make_unique<T>();
		created->m_hObject = handle;
		object = std::move(created);
	}
	return static_cast<T *>(object.get());
}

} // namespace

void DeleteTempGdiMaps()
{
	// Temporary objects never own their handles.
	for(auto &entry : TempGdiObjects())
		entry.second->m_hObject = nullptr;
	TempGdiObjects().clear();
	for(auto &entry : TempDCs())
		entry.second->m_hDC = entry.second->m_hAttribDC = nullptr;
	TempDCs().clear();
	for(auto &entry : TempMenus())
		entry.second->m_hMenu = nullptr;
	TempMenus().clear();
}

} // namespace linopenmpt::mfc

using namespace linopenmpt::mfc;

// ---- GDI objects ----------------------------------------------------------------------

CGdiObject::~CGdiObject()
{
	DeleteObject();
}

BOOL CGdiObject::DeleteObject()
{
	if(m_hObject == nullptr)
		return FALSE;
	return ::DeleteObject(Detach());
}

int CGdiObject::GetObject(int size, LPVOID buffer) const
{
	return ::GetObject(m_hObject, size, buffer);
}

CGdiObject *CGdiObject::FromHandle(HGDIOBJ object)
{
	switch(::GetObjectType(object))
	{
	case OBJ_PEN: return TempGdi<CPen>(object);
	case OBJ_BRUSH: return TempGdi<CBrush>(object);
	case OBJ_FONT: return TempGdi<CFont>(object);
	case OBJ_BITMAP: return TempGdi<CBitmap>(object);
	case OBJ_REGION: return TempGdi<CRgn>(object);
	default: return object ? TempGdi<CGdiObject>(object) : nullptr;
	}
}

CPen *CPen::FromHandle(HPEN pen) { return TempGdi<CPen>(pen); }
CBrush *CBrush::FromHandle(HBRUSH brush) { return TempGdi<CBrush>(brush); }
CFont *CFont::FromHandle(HFONT font) { return TempGdi<CFont>(font); }
CBitmap *CBitmap::FromHandle(HBITMAP bitmap) { return TempGdi<CBitmap>(bitmap); }

CPen::CPen(int style, int width, COLORREF color) { CreatePen(style, width, color); }
BOOL CPen::CreatePen(int style, int width, COLORREF color) { return Attach(::CreatePen(style, width, color)); }
BOOL CPen::CreatePenIndirect(LOGPEN *pen) { return Attach(::CreatePenIndirect(pen)); }

CBrush::CBrush(COLORREF color) { CreateSolidBrush(color); }
BOOL CBrush::CreateSolidBrush(COLORREF color) { return Attach(::CreateSolidBrush(color)); }
BOOL CBrush::CreatePatternBrush(CBitmap *bitmap) { return bitmap && Attach(::CreatePatternBrush(static_cast<HBITMAP>(bitmap->GetSafeHandle()))); }
BOOL CBrush::CreateSysColorBrush(int index) { return Attach(::GetSysColorBrush(index)); }

BOOL CFont::CreateFontIndirect(const LOGFONT *font) { return Attach(::CreateFontIndirect(font)); }
BOOL CFont::CreateFont(int height, int width, int escapement, int orientation, int weight, BYTE italic, BYTE underline, BYTE strikeOut,
	BYTE charSet, BYTE outPrecision, BYTE clipPrecision, BYTE quality, BYTE pitchAndFamily, LPCTSTR faceName)
{
	return Attach(::CreateFont(height, width, escapement, orientation, weight, italic, underline, strikeOut, charSet, outPrecision, clipPrecision, quality, pitchAndFamily, faceName));
}
BOOL CFont::CreatePointFont(int pointSize, LPCTSTR faceName, CDC *dc)
{
	// Tenths of a point, at the DC's (or the screen's) resolution.
	LOGFONT lf{};
	lf.lfCharSet = DEFAULT_CHARSET;
	if(faceName)
		std::wcsncpy(lf.lfFaceName, faceName, LF_FACESIZE - 1);
	const HDC hdc = dc ? dc->m_hAttribDC : ::GetDC(nullptr);
	lf.lfHeight = -::MulDiv(pointSize, ::GetDeviceCaps(hdc, LOGPIXELSY), 720);
	if(!dc)
		::ReleaseDC(nullptr, hdc);
	return CreateFontIndirect(&lf);
}
int CFont::GetLogFont(LOGFONT *font) { return GetObject(sizeof(LOGFONT), font); }

BOOL CBitmap::CreateBitmap(int w, int h, UINT planes, UINT bitCount, const void *bits) { return Attach(::CreateBitmap(w, h, planes, bitCount, bits)); }
BOOL CBitmap::CreateCompatibleBitmap(CDC *dc, int w, int h) { return Attach(::CreateCompatibleBitmap(dc ? dc->m_hDC : nullptr, w, h)); }
int CBitmap::GetBitmap(BITMAP *bitmap) { return GetObject(sizeof(BITMAP), bitmap); }
CSize CBitmap::GetBitmapDimension() const { return CSize(0, 0); }

BOOL CRgn::CreateRectRgn(int left, int top, int right, int bottom) { return Attach(::CreateRectRgn(left, top, right, bottom)); }
BOOL CRgn::CreateRectRgnIndirect(LPCRECT rect) { return Attach(::CreateRectRgnIndirect(rect)); }
int CRgn::CombineRgn(const CRgn *src1, const CRgn *src2, int mode)
{
	return ::CombineRgn(static_cast<HRGN>(m_hObject), src1 ? static_cast<HRGN>(src1->m_hObject) : nullptr, src2 ? static_cast<HRGN>(src2->m_hObject) : nullptr, mode);
}
BOOL CRgn::PtInRegion(int x, int y) const { return ::PtInRegion(static_cast<HRGN>(m_hObject), x, y); }

CMenu *CMenu::FromHandle(HMENU menu)
{
	if(menu == nullptr)
		return nullptr;
	auto &object = TempMenus()[menu];
	if(!object)
	{
		object = std::make_unique<CMenu>();
		object->m_hMenu = menu;
	}
	return object.get();
}

// ---- device contexts ---------------------------------------------------------------------

CDC::~CDC()
{
	if(m_hDC != nullptr)
		::DeleteDC(Detach());
}

CDC *CDC::FromHandle(HDC dc)
{
	if(dc == nullptr)
		return nullptr;
	auto &object = TempDCs()[dc];
	if(!object)
	{
		object = std::make_unique<CDC>();
		object->Attach(dc);
	}
	return object.get();
}

CPaintDC::CPaintDC(CWnd *wnd)
{
	m_hWnd = wnd->m_hWnd;
	Attach(::BeginPaint(m_hWnd, &m_ps));
}

CPaintDC::~CPaintDC()
{
	::EndPaint(m_hWnd, &m_ps);
	Detach();
}

CClientDC::CClientDC(CWnd *wnd)
{
	m_hWnd = wnd ? wnd->m_hWnd : nullptr;
	Attach(::GetDC(m_hWnd));
}

CClientDC::~CClientDC()
{
	::ReleaseDC(m_hWnd, Detach());
}

CWindowDC::CWindowDC(CWnd *wnd)
{
	m_hWnd = wnd ? wnd->m_hWnd : nullptr;
	Attach(::GetWindowDC(m_hWnd));
}

CWindowDC::~CWindowDC()
{
	::ReleaseDC(m_hWnd, Detach());
}

BOOL CDC::CreateCompatibleDC(CDC *dc) { return Attach(::CreateCompatibleDC(dc ? dc->m_hDC : nullptr)); }
BOOL CDC::DeleteDC()
{
	if(m_hDC == nullptr)
		return FALSE;
	return ::DeleteDC(Detach());
}
int CDC::SaveDC() { return ::SaveDC(m_hDC); }
BOOL CDC::RestoreDC(int saved) { return ::RestoreDC(m_hDC, saved); }
int CDC::GetDeviceCaps(int index) const { return ::GetDeviceCaps(m_hAttribDC, index); }
CWnd *CDC::GetWindow() const { return CWnd::FromHandle(::WindowFromDC(m_hDC)); }

// Objects
CPen *CDC::SelectObject(CPen *pen) { return CPen::FromHandle(static_cast<HPEN>(::SelectObject(m_hDC, pen ? pen->GetSafeHandle() : nullptr))); }
CBrush *CDC::SelectObject(CBrush *brush) { return CBrush::FromHandle(static_cast<HBRUSH>(::SelectObject(m_hDC, brush ? brush->GetSafeHandle() : nullptr))); }
CFont *CDC::SelectObject(CFont *font) { return CFont::FromHandle(static_cast<HFONT>(::SelectObject(m_hDC, font ? font->GetSafeHandle() : nullptr))); }
CBitmap *CDC::SelectObject(CBitmap *bitmap) { return CBitmap::FromHandle(static_cast<HBITMAP>(::SelectObject(m_hDC, bitmap ? bitmap->GetSafeHandle() : nullptr))); }
int CDC::SelectObject(CRgn *region) { return static_cast<int>(reinterpret_cast<INT_PTR>(::SelectObject(m_hDC, region ? region->GetSafeHandle() : nullptr))); }
CGdiObject *CDC::SelectObject(CGdiObject *object) { return CGdiObject::FromHandle(::SelectObject(m_hDC, object ? object->GetSafeHandle() : nullptr)); }
HGDIOBJ CDC::SelectObject(HGDIOBJ object) { return ::SelectObject(m_hDC, object); }
CGdiObject *CDC::SelectStockObject(int index) { return CGdiObject::FromHandle(::SelectObject(m_hDC, ::GetStockObject(index))); }
CFont *CDC::GetCurrentFont() const { return CFont::FromHandle(static_cast<HFONT>(::GetCurrentObject(m_hDC, OBJ_FONT))); }
CBitmap *CDC::GetCurrentBitmap() const { return CBitmap::FromHandle(static_cast<HBITMAP>(::GetCurrentObject(m_hDC, OBJ_BITMAP))); }

CBrush *CDC::GetHalftoneBrush()
{
	// The 50% grey checkerboard MFC uses for drag rectangles.
	static CBrush *brush = []
	{
		WORD pattern[8];
		for(int i = 0; i < 8; i++)
			pattern[i] = static_cast<WORD>(0x5555 << (i & 1));
		CBitmap bitmap;
		bitmap.CreateBitmap(8, 8, 1, 1, pattern);
		auto *b = new CBrush;
		b->CreatePatternBrush(&bitmap);
		return b;
	}();
	return brush;
}

// Attributes
COLORREF CDC::SetTextColor(COLORREF color) { return ::SetTextColor(m_hDC, color); }
COLORREF CDC::GetTextColor() const { return ::GetTextColor(m_hAttribDC); }
COLORREF CDC::SetBkColor(COLORREF color) { return ::SetBkColor(m_hDC, color); }
COLORREF CDC::GetBkColor() const { return ::GetBkColor(m_hAttribDC); }
int CDC::SetBkMode(int mode) { return ::SetBkMode(m_hDC, mode); }
int CDC::GetBkMode() const { return ::GetBkMode(m_hAttribDC); }
UINT CDC::SetTextAlign(UINT flags) { return ::SetTextAlign(m_hDC, flags); }
int CDC::SetROP2(int mode) { return ::SetROP2(m_hDC, mode); }
int CDC::SetMapMode(int mode) { return ::SetMapMode(m_hDC, mode); }
COLORREF CDC::SetDCBrushColor(COLORREF color) { return ::SetDCBrushColor(m_hDC, color); }
COLORREF CDC::SetDCPenColor(COLORREF color) { return ::SetDCPenColor(m_hDC, color); }
CPoint CDC::SetViewportOrg(int x, int y)
{
	POINT old{};
	::SetViewportOrgEx(m_hDC, x, y, &old);
	return CPoint(old);
}
CPoint CDC::SetViewportOrg(POINT point) { return SetViewportOrg(point.x, point.y); }
CPoint CDC::OffsetViewportOrg(int dx, int dy)
{
	POINT old{};
	::OffsetViewportOrgEx(m_hDC, dx, dy, &old);
	return CPoint(old);
}
CPoint CDC::SetWindowOrg(int x, int y)
{
	POINT old{};
	::SetWindowOrgEx(m_hDC, x, y, &old);
	return CPoint(old);
}
CPoint CDC::GetViewportOrg() const
{
	POINT p{};
	::GetViewportOrgEx(m_hAttribDC, &p);
	return CPoint(p);
}
void CDC::LPtoDP(LPPOINT points, int count) const { ::LPtoDP(m_hAttribDC, points, count); }
void CDC::LPtoDP(LPRECT rect) const { ::LPtoDP(m_hAttribDC, reinterpret_cast<LPPOINT>(rect), 2); }
void CDC::DPtoLP(LPPOINT points, int count) const { ::DPtoLP(m_hAttribDC, points, count); }
void CDC::DPtoLP(LPRECT rect) const { ::DPtoLP(m_hAttribDC, reinterpret_cast<LPPOINT>(rect), 2); }

// Clipping
int CDC::GetClipBox(LPRECT rect) const { return ::GetClipBox(m_hDC, rect); }
int CDC::IntersectClipRect(int left, int top, int right, int bottom) { return ::IntersectClipRect(m_hDC, left, top, right, bottom); }
int CDC::IntersectClipRect(LPCRECT rect) { return ::IntersectClipRect(m_hDC, rect->left, rect->top, rect->right, rect->bottom); }
int CDC::ExcludeClipRect(int left, int top, int right, int bottom) { return ::ExcludeClipRect(m_hDC, left, top, right, bottom); }
int CDC::ExcludeClipRect(LPCRECT rect) { return ::ExcludeClipRect(m_hDC, rect->left, rect->top, rect->right, rect->bottom); }
int CDC::SelectClipRgn(CRgn *region) { return ::SelectClipRgn(m_hDC, region ? static_cast<HRGN>(region->GetSafeHandle()) : nullptr); }
BOOL CDC::RectVisible(LPCRECT rect) const { return ::RectVisible(m_hDC, rect); }
BOOL CDC::PtVisible(int x, int y) const { return ::PtVisible(m_hDC, x, y); }

// Lines and shapes
CPoint CDC::MoveTo(int x, int y)
{
	POINT old{};
	::MoveToEx(m_hDC, x, y, &old);
	return CPoint(old);
}
CPoint CDC::MoveTo(POINT point) { return MoveTo(point.x, point.y); }
BOOL CDC::LineTo(int x, int y) { return ::LineTo(m_hDC, x, y); }
BOOL CDC::LineTo(POINT point) { return ::LineTo(m_hDC, point.x, point.y); }
BOOL CDC::Polyline(const POINT *points, int count) { return ::Polyline(m_hDC, points, count); }
BOOL CDC::Polygon(const POINT *points, int count) { return ::Polygon(m_hDC, points, count); }
BOOL CDC::PolyPolyline(const POINT *points, const DWORD *counts, int polylines) { return ::PolyPolyline(m_hDC, points, counts, polylines); }
BOOL CDC::Rectangle(int left, int top, int right, int bottom) { return ::Rectangle(m_hDC, left, top, right, bottom); }
BOOL CDC::Rectangle(LPCRECT rect) { return ::Rectangle(m_hDC, rect->left, rect->top, rect->right, rect->bottom); }
BOOL CDC::RoundRect(int left, int top, int right, int bottom, int w, int h) { return ::RoundRect(m_hDC, left, top, right, bottom, w, h); }
BOOL CDC::Ellipse(int left, int top, int right, int bottom) { return ::Ellipse(m_hDC, left, top, right, bottom); }
BOOL CDC::Ellipse(LPCRECT rect) { return ::Ellipse(m_hDC, rect->left, rect->top, rect->right, rect->bottom); }
void CDC::FillRect(LPCRECT rect, CBrush *brush) { ::FillRect(m_hDC, rect, brush ? static_cast<HBRUSH>(brush->GetSafeHandle()) : nullptr); }
void CDC::FrameRect(LPCRECT rect, CBrush *brush) { ::FrameRect(m_hDC, rect, brush ? static_cast<HBRUSH>(brush->GetSafeHandle()) : nullptr); }
void CDC::InvertRect(LPCRECT rect) { ::InvertRect(m_hDC, rect); }

void CDC::FillSolidRect(LPCRECT rect, COLORREF color)
{
	// As MFC does it, which leaves the background colour set.
	::SetBkColor(m_hDC, color);
	::ExtTextOut(m_hDC, 0, 0, ETO_OPAQUE, rect, nullptr, 0, nullptr);
}

void CDC::FillSolidRect(int x, int y, int cx, int cy, COLORREF color)
{
	const RECT rect{x, y, x + cx, y + cy};
	FillSolidRect(&rect, color);
}

void CDC::Draw3dRect(int x, int y, int cx, int cy, COLORREF topLeft, COLORREF bottomRight)
{
	FillSolidRect(x, y, cx - 1, 1, topLeft);
	FillSolidRect(x, y, 1, cy - 1, topLeft);
	FillSolidRect(x + cx, y, -1, cy, bottomRight);
	FillSolidRect(x, y + cy, cx, -1, bottomRight);
}

void CDC::Draw3dRect(LPCRECT rect, COLORREF topLeft, COLORREF bottomRight)
{
	Draw3dRect(rect->left, rect->top, rect->right - rect->left, rect->bottom - rect->top, topLeft, bottomRight);
}

void CDC::DrawFocusRect(LPCRECT rect) { ::DrawFocusRect(m_hDC, rect); }
BOOL CDC::DrawEdge(LPRECT rect, UINT edge, UINT flags) { return ::DrawEdge(m_hDC, rect, edge, flags); }
BOOL CDC::DrawFrameControl(LPRECT rect, UINT type, UINT state) { return ::DrawFrameControl(m_hDC, rect, type, state); }

void CDC::DrawDragRect(LPCRECT rect, SIZE size, LPCRECT lastRect, SIZE lastSize, CBrush *brush, CBrush *lastBrush)
{
	// XOR frames: drawing one twice removes it.
	auto frame = [this](LPCRECT r, SIZE s, CBrush *b)
	{
		if(r == nullptr || ::IsRectEmpty(r))
			return;
		CBrush *old = SelectObject(b ? b : GetHalftoneBrush());
		const int w = r->right - r->left;
		const int h = r->bottom - r->top;
		::PatBlt(m_hDC, r->left, r->top, w, s.cy, PATINVERT);
		::PatBlt(m_hDC, r->left, r->bottom - s.cy, w, s.cy, PATINVERT);
		::PatBlt(m_hDC, r->left, r->top + s.cy, s.cx, h - 2 * s.cy, PATINVERT);
		::PatBlt(m_hDC, r->right - s.cx, r->top + s.cy, s.cx, h - 2 * s.cy, PATINVERT);
		SelectObject(old);
	};
	frame(lastRect, lastSize, lastBrush ? lastBrush : brush);
	frame(rect, size, brush);
}

COLORREF CDC::SetPixel(int x, int y, COLORREF color) { return ::SetPixel(m_hDC, x, y, color); }
COLORREF CDC::SetPixel(POINT point, COLORREF color) { return ::SetPixel(m_hDC, point.x, point.y, color); }
COLORREF CDC::GetPixel(int x, int y) const { return ::GetPixel(m_hDC, x, y); }
BOOL CDC::SetPixelV(int x, int y, COLORREF color) { return ::SetPixelV(m_hDC, x, y, color); }
BOOL CDC::DrawIcon(int x, int y, HICON icon) { return ::DrawIcon(m_hDC, x, y, icon); }
BOOL CDC::DrawIcon(POINT point, HICON icon) { return ::DrawIcon(m_hDC, point.x, point.y, icon); }
BOOL CDC::DrawState(CPoint pt, CSize size, HICON icon, UINT, HBRUSH)
{
	return ::DrawIconEx(m_hDC, pt.x, pt.y, icon, size.cx, size.cy, 0, nullptr, DI_NORMAL);
}

// Bitmaps
BOOL CDC::BitBlt(int x, int y, int w, int h, CDC *src, int xSrc, int ySrc, DWORD rop)
{
	return ::BitBlt(m_hDC, x, y, w, h, src ? src->m_hDC : nullptr, xSrc, ySrc, rop);
}
BOOL CDC::StretchBlt(int x, int y, int w, int h, CDC *src, int xSrc, int ySrc, int wSrc, int hSrc, DWORD rop)
{
	return ::StretchBlt(m_hDC, x, y, w, h, src ? src->m_hDC : nullptr, xSrc, ySrc, wSrc, hSrc, rop);
}
BOOL CDC::PatBlt(int x, int y, int w, int h, DWORD rop) { return ::PatBlt(m_hDC, x, y, w, h, rop); }
BOOL CDC::TransparentBlt(int x, int y, int w, int h, CDC *src, int xSrc, int ySrc, int wSrc, int hSrc, UINT transparent)
{
	return ::TransparentBlt(m_hDC, x, y, w, h, src ? src->m_hDC : nullptr, xSrc, ySrc, wSrc, hSrc, transparent);
}
BOOL CDC::AlphaBlend(int x, int y, int w, int h, CDC *src, int xSrc, int ySrc, int wSrc, int hSrc, BLENDFUNCTION blend)
{
	return ::AlphaBlend(m_hDC, x, y, w, h, src ? src->m_hDC : nullptr, xSrc, ySrc, wSrc, hSrc, blend);
}

// Text
BOOL CDC::TextOut(int x, int y, LPCTSTR text, int count) { return ::TextOut(m_hDC, x, y, text, count); }
BOOL CDC::TextOut(int x, int y, const CString &text) { return ::TextOut(m_hDC, x, y, text.GetString(), text.GetLength()); }
BOOL CDC::ExtTextOut(int x, int y, UINT options, LPCRECT rect, LPCTSTR text, UINT count, LPINT dx)
{
	return ::ExtTextOut(m_hDC, x, y, options, rect, text, count, dx);
}
BOOL CDC::ExtTextOut(int x, int y, UINT options, LPCRECT rect, const CString &text, LPINT dx)
{
	return ::ExtTextOut(m_hDC, x, y, options, rect, text.GetString(), static_cast<UINT>(text.GetLength()), dx);
}
int CDC::DrawText(LPCTSTR text, int count, LPRECT rect, UINT format) { return ::DrawText(m_hDC, text, count, rect, format); }
int CDC::DrawText(const CString &text, LPRECT rect, UINT format) { return ::DrawText(m_hDC, text.GetString(), text.GetLength(), rect, format); }
CSize CDC::GetTextExtent(LPCTSTR text, int count) const
{
	SIZE size{};
	::GetTextExtentPoint32(m_hAttribDC, text, count, &size);
	return CSize(size);
}
CSize CDC::GetTextExtent(const CString &text) const { return GetTextExtent(text.GetString(), text.GetLength()); }
CSize CDC::GetOutputTextExtent(LPCTSTR text, int count) const
{
	SIZE size{};
	::GetTextExtentPoint32(m_hDC, text, count, &size);
	return CSize(size);
}
CSize CDC::GetOutputTextExtent(const CString &text) const { return GetOutputTextExtent(text.GetString(), text.GetLength()); }
BOOL CDC::GetTextMetrics(LPTEXTMETRIC metrics) const { return ::GetTextMetrics(m_hAttribDC, metrics); }
int CDC::GetTextFace(int count, LPTSTR face) const { return ::GetTextFace(m_hDC, count, face); }
int CDC::GetTextFace(CString &face) const
{
	TCHAR buf[LF_FACESIZE] = {};
	const int n = ::GetTextFace(m_hDC, LF_FACESIZE, buf);
	face = buf;
	return n;
}
