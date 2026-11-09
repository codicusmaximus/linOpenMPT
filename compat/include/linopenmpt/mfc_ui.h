// linOpenMPT compatibility layer: declarations of MFC UI classes.
//
// Upstream's shared headers (Mptrack.h, Mainfrm.h, Moddoc.h, ...) mention MFC
// window classes in member declarations and signatures. These skeletons let
// those headers compile unchanged. They have no behaviour. Virtual functions
// have do-nothing defaults (as MFC's base implementations mostly do), so the
// classes' vtables exist; non-virtual methods are declared but not defined,
// so any direct UI call that reaches a Linux binary shows up as a link error,
// which is exactly the list of things the Qt UI has to provide. Simple value types (CRect, CPoint, CSize) and the document base
// class are implemented, because non-UI logic uses them directly.

#pragma once

#include "afx.h"
#include "mfc_msgmap.h"
#include "../afxres.h"
#include "win32_gdi.h"
#include "win32_commctrl.h"
#include "win32_shell.h"

// ---- window messages and constants used in shared headers ----------------
#define DT_TOP 0x0000
#define DT_LEFT 0x0000
#define DT_CENTER 0x0001
#define DT_RIGHT 0x0002
#define DT_VCENTER 0x0004
#define DT_BOTTOM 0x0008
#define DT_SINGLELINE 0x0020
#define DT_NOPREFIX 0x0800
#define HOTKEYF_SHIFT 0x01
#define HOTKEYF_CONTROL 0x02
#define HOTKEYF_ALT 0x04
#define HOTKEYF_EXT 0x08


// MFC's opaque list iterator.
struct __POSITION { };
using POSITION = __POSITION *;
#include "mfc_coll.h"

struct CCreateContext
{
	CRuntimeClass *m_pNewViewClass = nullptr;
	class CDocument *m_pCurrentDoc = nullptr;
	class CDocTemplate *m_pNewDocTemplate = nullptr;
	class CView *m_pLastView = nullptr;
	class CFrameWnd *m_pCurrentFrame = nullptr;
};

struct CPrintInfo;

// ---- window styles, SetWindowPos flags, misc constants ---------------------
#define WS_OVERLAPPED 0x00000000L
#define WS_POPUP 0x80000000L
#define WS_CHILD 0x40000000L
#define WS_MINIMIZE 0x20000000L
#define WS_VISIBLE 0x10000000L
#define WS_DISABLED 0x08000000L
#define WS_CLIPSIBLINGS 0x04000000L
#define WS_CLIPCHILDREN 0x02000000L
#define WS_MAXIMIZE 0x01000000L
#define WS_CAPTION 0x00C00000L
#define WS_BORDER 0x00800000L
#define WS_DLGFRAME 0x00400000L
#define WS_VSCROLL 0x00200000L
#define WS_HSCROLL 0x00100000L
#define WS_SYSMENU 0x00080000L
#define WS_THICKFRAME 0x00040000L
#define WS_GROUP 0x00020000L
#define WS_TABSTOP 0x00010000L
#define WS_MINIMIZEBOX 0x00020000L
#define WS_MAXIMIZEBOX 0x00010000L
#define WS_OVERLAPPEDWINDOW (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX)
#define WS_EX_TOOLWINDOW 0x00000080L
#define WS_EX_CLIENTEDGE 0x00000200L
#define WS_EX_STATICEDGE 0x00020000L
#define ES_LEFT 0x0000L
#define ES_CENTER 0x0001L
#define ES_RIGHT 0x0002L
#define ES_MULTILINE 0x0004L
#define ES_AUTOVSCROLL 0x0040L
#define ES_AUTOHSCROLL 0x0080L
#define ES_READONLY 0x0800L
#define ES_WANTRETURN 0x1000L
#define ES_NUMBER 0x2000L
#define SWP_NOSIZE 0x0001
#define SWP_NOMOVE 0x0002
#define SWP_NOZORDER 0x0004
#define SWP_NOREDRAW 0x0008
#define SWP_NOACTIVATE 0x0010
#define SWP_FRAMECHANGED 0x0020
#define SWP_SHOWWINDOW 0x0040
#define SWP_HIDEWINDOW 0x0080
#define SWP_NOOWNERZORDER 0x0200
#define GWL_STYLE (-16)
#define GWL_EXSTYLE (-20)
#define GWLP_WNDPROC (-4)
#define GWLP_ID (-12)
#define GWLP_USERDATA (-21)
#define LB_ERR (-1)
#define CB_ERR (-1)
#define LB_ERRSPACE (-2)
#define CB_ERRSPACE (-2)
LONG_PTR GetWindowLongPtr(HWND hwnd, int index);
LONG_PTR SetWindowLongPtr(HWND hwnd, int index, LONG_PTR value);
LONG GetWindowLong(HWND hwnd, int index);
LONG SetWindowLong(HWND hwnd, int index, LONG value);

// Owner-drawn controls.
#define ODT_MENU 1
#define ODT_LISTBOX 2
#define ODT_COMBOBOX 3
#define ODT_BUTTON 4
#define ODS_SELECTED 0x0001
#define ODS_GRAYED 0x0002
#define ODS_DISABLED 0x0004
#define ODS_CHECKED 0x0008
#define ODS_FOCUS 0x0010
struct DRAWITEMSTRUCT
{
	UINT CtlType; UINT CtlID; UINT itemID; UINT itemAction; UINT itemState;
	HWND hwndItem; HDC hDC; RECT rcItem; ULONG_PTR itemData;
};
using LPDRAWITEMSTRUCT = DRAWITEMSTRUCT *;
struct MEASUREITEMSTRUCT
{
	UINT CtlType; UINT CtlID; UINT itemID; UINT itemWidth; UINT itemHeight; ULONG_PTR itemData;
};
using LPMEASUREITEMSTRUCT = MEASUREITEMSTRUCT *;

struct MINMAXINFO { POINT ptReserved; POINT ptMaxSize; POINT ptMaxPosition; POINT ptMinTrackSize; POINT ptMaxTrackSize; };
struct _EXCEPTION_RECORD;
struct _CONTEXT;
struct _EXCEPTION_POINTERS { _EXCEPTION_RECORD *ExceptionRecord; _CONTEXT *ContextRecord; };
using EXCEPTION_POINTERS = _EXCEPTION_POINTERS;

// ---- value types ---------------------------------------------------------
class CSize : public SIZE
{
public:
	CSize() noexcept { cx = 0; cy = 0; }
	CSize(int x, int y) noexcept { cx = x; cy = y; }
	CSize(SIZE s) noexcept { cx = s.cx; cy = s.cy; }
	explicit CSize(POINT p) noexcept { cx = p.x; cy = p.y; }
	explicit CSize(DWORD packed) noexcept { cx = static_cast<short>(LOWORD(packed)); cy = static_cast<short>(HIWORD(packed)); }
	CSize operator-() const noexcept { return CSize(-cx, -cy); }
	CSize &operator+=(SIZE o) noexcept { cx += o.cx; cy += o.cy; return *this; }
	CSize &operator-=(SIZE o) noexcept { cx -= o.cx; cy -= o.cy; return *this; }
	bool operator==(const SIZE &o) const noexcept { return cx == o.cx && cy == o.cy; }
	bool operator!=(const SIZE &o) const noexcept { return !(*this == o); }
	CSize operator+(SIZE o) const noexcept { return CSize(cx + o.cx, cy + o.cy); }
	CSize operator-(SIZE o) const noexcept { return CSize(cx - o.cx, cy - o.cy); }
};

class CPoint : public POINT
{
public:
	CPoint() noexcept { x = 0; y = 0; }
	CPoint(int x_, int y_) noexcept { x = x_; y = y_; }
	CPoint(POINT p) noexcept { x = p.x; y = p.y; }
	CPoint(SIZE s) noexcept { x = s.cx; y = s.cy; }
	explicit CPoint(DWORD packed) noexcept { x = static_cast<short>(LOWORD(packed)); y = static_cast<short>(HIWORD(packed)); }
	CPoint operator+(POINT p) const noexcept { return CPoint(x + p.x, y + p.y); }
	CPoint operator-() const noexcept { return CPoint(-x, -y); }
	CPoint &operator+=(POINT p) noexcept { x += p.x; y += p.y; return *this; }
	CPoint &operator-=(POINT p) noexcept { x -= p.x; y -= p.y; return *this; }
	void Offset(int dx, int dy) noexcept { x += dx; y += dy; }
	void SetPoint(int x_, int y_) noexcept { x = x_; y = y_; }
	bool operator==(const POINT &o) const noexcept { return x == o.x && y == o.y; }
	bool operator!=(const POINT &o) const noexcept { return !(*this == o); }
	CPoint operator+(SIZE s) const noexcept { return CPoint(x + s.cx, y + s.cy); }
	CPoint operator-(SIZE s) const noexcept { return CPoint(x - s.cx, y - s.cy); }
	CSize operator-(POINT p) const noexcept { return CSize(x - p.x, y - p.y); }
	operator CSize() const noexcept { return CSize(x, y); }
	CPoint &operator+=(SIZE s) noexcept { x += s.cx; y += s.cy; return *this; }
	CPoint &operator-=(SIZE s) noexcept { x -= s.cx; y -= s.cy; return *this; }
};

class CRect : public RECT
{
public:
	CRect() noexcept { left = top = right = bottom = 0; }
	CRect(int l, int t, int r, int b) noexcept { left = l; top = t; right = r; bottom = b; }
	CRect(const RECT &r) noexcept { left = r.left; top = r.top; right = r.right; bottom = r.bottom; }
	CRect(const RECT *r) noexcept { left = r->left; top = r->top; right = r->right; bottom = r->bottom; }
	CRect(POINT p, SIZE s) noexcept { left = p.x; top = p.y; right = p.x + s.cx; bottom = p.y + s.cy; }
	CRect(POINT tl, POINT br) noexcept { left = tl.x; top = tl.y; right = br.x; bottom = br.y; }
	int Width() const noexcept { return right - left; }
	int Height() const noexcept { return bottom - top; }
	CSize Size() const noexcept { return CSize(Width(), Height()); }
	CPoint TopLeft() const noexcept { return CPoint(left, top); }
	CPoint BottomRight() const noexcept { return CPoint(right, bottom); }
	CPoint CenterPoint() const noexcept { return CPoint((left + right) / 2, (top + bottom) / 2); }
	bool IsRectEmpty() const noexcept { return right <= left || bottom <= top; }
	bool IsRectNull() const noexcept { return left == 0 && top == 0 && right == 0 && bottom == 0; }
	bool PtInRect(POINT p) const noexcept { return p.x >= left && p.x < right && p.y >= top && p.y < bottom; }
	void SetRect(int l, int t, int r, int b) noexcept { left = l; top = t; right = r; bottom = b; }
	void SetRectEmpty() noexcept { left = top = right = bottom = 0; }
	void OffsetRect(int dx, int dy) noexcept { left += dx; right += dx; top += dy; bottom += dy; }
	void OffsetRect(POINT p) noexcept { OffsetRect(p.x, p.y); }
	void OffsetRect(SIZE s) noexcept { OffsetRect(s.cx, s.cy); }
	void MoveToXY(int x, int y) noexcept { OffsetRect(x - left, y - top); }
	void MoveToX(int x) noexcept { OffsetRect(x - left, 0); }
	void MoveToY(int y) noexcept { OffsetRect(0, y - top); }
	void InflateRect(int dx, int dy) noexcept { left -= dx; top -= dy; right += dx; bottom += dy; }
	void InflateRect(int l, int t, int r, int b) noexcept { left -= l; top -= t; right += r; bottom += b; }
	void DeflateRect(int dx, int dy) noexcept { InflateRect(-dx, -dy); }
	void DeflateRect(int l, int t, int r, int b) noexcept { InflateRect(-l, -t, -r, -b); }
	void NormalizeRect() noexcept
	{
		if(left > right) std::swap(left, right);
		if(top > bottom) std::swap(top, bottom);
	}
	bool IntersectRect(const RECT *a, const RECT *b) noexcept
	{
		left = std::max(a->left, b->left); top = std::max(a->top, b->top);
		right = std::min(a->right, b->right); bottom = std::min(a->bottom, b->bottom);
		if(IsRectEmpty()) { SetRectEmpty(); return false; }
		return true;
	}
	bool UnionRect(const RECT *a, const RECT *b) noexcept
	{
		left = std::min(a->left, b->left); top = std::min(a->top, b->top);
		right = std::max(a->right, b->right); bottom = std::max(a->bottom, b->bottom);
		return !IsRectEmpty();
	}
	operator RECT *() noexcept { return this; }
	operator const RECT *() const noexcept { return this; }
	bool operator==(const RECT &o) const noexcept { return left == o.left && top == o.top && right == o.right && bottom == o.bottom; }
	bool operator!=(const RECT &o) const noexcept { return !(*this == o); }
	CRect operator&(const RECT &o) const noexcept { CRect r; r.IntersectRect(this, &o); return r; }
	CRect operator|(const RECT &o) const noexcept { CRect r; r.UnionRect(this, &o); return r; }
	CRect &operator&=(const RECT &o) noexcept { IntersectRect(this, &o); return *this; }
	CRect &operator|=(const RECT &o) noexcept { UnionRect(this, &o); return *this; }
	CRect operator+(POINT p) const noexcept { CRect r(*this); r.OffsetRect(p); return r; }
	CRect operator-(POINT p) const noexcept { CRect r(*this); r.OffsetRect(-p.x, -p.y); return r; }
	CRect operator+(SIZE s) const noexcept { CRect r(*this); r.OffsetRect(s); return r; }
	CRect operator-(SIZE s) const noexcept { CRect r(*this); r.OffsetRect(-s.cx, -s.cy); return r; }
	CRect &operator+=(POINT p) noexcept { OffsetRect(p); return *this; }
	CRect &operator-=(POINT p) noexcept { OffsetRect(-p.x, -p.y); return *this; }
	CRect &operator+=(SIZE s) noexcept { OffsetRect(s); return *this; }
	CRect &operator-=(SIZE s) noexcept { OffsetRect(-s.cx, -s.cy); return *this; }
	CRect &operator+=(const RECT *r) noexcept { InflateRect(r->left, r->top, r->right, r->bottom); return *this; }
	CRect &operator-=(const RECT *r) noexcept { DeflateRect(r->left, r->top, r->right, r->bottom); return *this; }
	CPoint &TopLeft() noexcept { return *reinterpret_cast<CPoint *>(&left); }
	CPoint &BottomRight() noexcept { return *reinterpret_cast<CPoint *>(&right); }
	void InflateRect(SIZE s) noexcept { InflateRect(s.cx, s.cy); }
	void InflateRect(const RECT *r) noexcept { InflateRect(r->left, r->top, r->right, r->bottom); }
	void DeflateRect(SIZE s) noexcept { DeflateRect(s.cx, s.cy); }
	void DeflateRect(const RECT *r) noexcept { DeflateRect(r->left, r->top, r->right, r->bottom); }
	bool EqualRect(const RECT *r) const noexcept { return *this == *r; }
	void CopyRect(const RECT *r) noexcept { *this = CRect(r); }
	void SetRect(POINT tl, POINT br) noexcept { SetRect(tl.x, tl.y, br.x, br.y); }
	// Like Windows: only subtracts when b covers a fully along one axis.
	bool SubtractRect(const RECT *a, const RECT *b) noexcept
	{
		CRect r(a);
		if(b->top <= r.top && b->bottom >= r.bottom)
		{
			if(b->left <= r.left && b->right > r.left) r.left = std::min<LONG>(b->right, r.right);
			else if(b->right >= r.right && b->left < r.right) r.right = std::max<LONG>(b->left, r.left);
		} else if(b->left <= r.left && b->right >= r.right)
		{
			if(b->top <= r.top && b->bottom > r.top) r.top = std::min<LONG>(b->bottom, r.bottom);
			else if(b->bottom >= r.bottom && b->top < r.bottom) r.bottom = std::max<LONG>(b->top, r.top);
		}
		*this = r;
		return !IsRectEmpty();
	}
};

// ---- runtime class / message map macros ----------------------------------
// CRuntimeClass and the DECLARE_/IMPLEMENT_DYNAMIC macros are in afx.h.

// ---- command routing -----------------------------------------------------
class CCmdTarget : public CObject
{
	LINOPENMPT_MFC_DYNAMIC(CCmdTarget)
public:
	virtual ~CCmdTarget() = default;
	virtual BOOL OnCmdMsg(UINT id, int code, void *extra, void *handlerInfo);
protected:
	static const AFX_MSGMAP *GetThisMessageMap();
	virtual const AFX_MSGMAP *GetMessageMap() const { return GetThisMessageMap(); }
public:
	void BeginWaitCursor();
	void EndWaitCursor();
	void RestoreWaitCursor();
};

class CCmdUI
{
public:
	UINT m_nID = 0;
	virtual void Enable(BOOL on = TRUE) { }
	virtual void SetCheck(int check = 1) { }
	virtual void SetRadio(BOOL on = TRUE) { }
	virtual void SetText(LPCTSTR text) { }
	virtual ~CCmdUI() = default;
};

// Dialog data exchange (UpdateData, DoDataExchange, DDX_ and DDV_).
class CDataExchange
{
public:
	CDataExchange() = default;
	CDataExchange(CWnd *dlgWnd, BOOL saveAndValidate) : m_bSaveAndValidate(saveAndValidate), m_pDlgWnd(dlgWnd) { }
	BOOL m_bSaveAndValidate = FALSE;
	CWnd *m_pDlgWnd = nullptr;
	// The control being exchanged, where Fail() puts the focus.
	UINT m_idLastControl = 0;
	BOOL m_bEditLastControl = FALSE;
	HWND PrepareCtrl(int id);
	HWND PrepareEditCtrl(int id);
	// Validation failed: focus the control and throw a CUserException.
	[[noreturn]] void Fail();
};

// ---- GDI -----------------------------------------------------------------
class CWnd;
class CGdiObject : public CObject
{
public:
	HGDIOBJ m_hObject = nullptr;
	// Deletes the GDI object, as in MFC.
	virtual ~CGdiObject();
	operator HGDIOBJ() const { return m_hObject; }
	HGDIOBJ GetSafeHandle() const { return this ? m_hObject : nullptr; }
	BOOL Attach(HGDIOBJ object) { m_hObject = object; return object != nullptr; }
	HGDIOBJ Detach() { HGDIOBJ h = m_hObject; m_hObject = nullptr; return h; }
	BOOL DeleteObject();
	int GetObject(int size, LPVOID buffer) const;
	static CGdiObject *FromHandle(HGDIOBJ object);
};
class CPen : public CGdiObject
{
public:
	CPen() = default;
	CPen(int style, int width, COLORREF color);
	BOOL CreatePen(int style, int width, COLORREF color);
	BOOL CreatePenIndirect(LOGPEN *pen);
	operator HPEN() const { return static_cast<HPEN>(m_hObject); }
	static CPen *FromHandle(HPEN pen);
};
class CBrush : public CGdiObject
{
public:
	CBrush() = default;
	explicit CBrush(COLORREF color);
	BOOL CreateSolidBrush(COLORREF color);
	BOOL CreatePatternBrush(class CBitmap *bitmap);
	BOOL CreateSysColorBrush(int index);
	operator HBRUSH() const { return static_cast<HBRUSH>(m_hObject); }
	static CBrush *FromHandle(HBRUSH brush);
};
class CFont : public CGdiObject
{
public:
	BOOL CreateFontIndirect(const LOGFONT *font);
	BOOL CreateFont(int height, int width, int escapement, int orientation, int weight, BYTE italic, BYTE underline, BYTE strikeOut,
		BYTE charSet, BYTE outPrecision, BYTE clipPrecision, BYTE quality, BYTE pitchAndFamily, LPCTSTR faceName);
	BOOL CreatePointFont(int pointSize, LPCTSTR faceName, class CDC *dc = nullptr);
	int GetLogFont(LOGFONT *font);
	operator HFONT() const { return static_cast<HFONT>(m_hObject); }
	static CFont *FromHandle(HFONT font);
};
class CBitmap : public CGdiObject
{
public:
	BOOL LoadBitmap(UINT id);
	BOOL LoadBitmap(LPCTSTR name);
	BOOL CreateBitmap(int w, int h, UINT planes, UINT bitCount, const void *bits);
	BOOL CreateCompatibleBitmap(class CDC *dc, int w, int h);
	int GetBitmap(BITMAP *bitmap);
	CSize GetBitmapDimension() const;
	operator HBITMAP() const { return static_cast<HBITMAP>(m_hObject); }
	static CBitmap *FromHandle(HBITMAP bitmap);
};
class CRgn : public CGdiObject
{
public:
	BOOL CreateRectRgn(int left, int top, int right, int bottom);
	BOOL CreateRectRgnIndirect(LPCRECT rect);
	int CombineRgn(const CRgn *src1, const CRgn *src2, int mode);
	BOOL PtInRegion(int x, int y) const;
	operator HRGN() const { return static_cast<HRGN>(m_hObject); }
};
class CPalette : public CGdiObject { };

class CDC : public CObject
{
public:
	HDC m_hDC = nullptr;
	HDC m_hAttribDC = nullptr;
	CDC() = default;
	// Deletes the DC if it still has one, as in MFC.
	virtual ~CDC();
	operator HDC() const { return m_hDC; }
	HDC GetSafeHdc() const { return this ? m_hDC : nullptr; }
	BOOL Attach(HDC dc) { m_hDC = m_hAttribDC = dc; return dc != nullptr; }
	HDC Detach() { HDC h = m_hDC; m_hDC = m_hAttribDC = nullptr; return h; }
	static CDC *FromHandle(HDC dc);
	BOOL CreateCompatibleDC(CDC *dc);
	BOOL DeleteDC();
	int SaveDC();
	BOOL RestoreDC(int saved);
	int GetDeviceCaps(int index) const;
	CWnd *GetWindow() const;

	// Objects
	CPen *SelectObject(CPen *pen);
	CBrush *SelectObject(CBrush *brush);
	CFont *SelectObject(CFont *font);
	CBitmap *SelectObject(CBitmap *bitmap);
	int SelectObject(CRgn *region);
	CGdiObject *SelectObject(CGdiObject *object);
	HGDIOBJ SelectObject(HGDIOBJ object);
	CGdiObject *SelectStockObject(int index);
	CFont *GetCurrentFont() const;
	CBitmap *GetCurrentBitmap() const;
	static CBrush *GetHalftoneBrush();

	// Attributes
	COLORREF SetTextColor(COLORREF color);
	COLORREF GetTextColor() const;
	COLORREF SetBkColor(COLORREF color);
	COLORREF GetBkColor() const;
	int SetBkMode(int mode);
	int GetBkMode() const;
	UINT SetTextAlign(UINT flags);
	int SetROP2(int mode);
	int SetMapMode(int mode);
	COLORREF SetDCBrushColor(COLORREF color);
	COLORREF SetDCPenColor(COLORREF color);
	CPoint SetViewportOrg(int x, int y);
	CPoint SetViewportOrg(POINT point);
	CPoint OffsetViewportOrg(int dx, int dy);
	CPoint SetWindowOrg(int x, int y);
	CPoint GetViewportOrg() const;
	void LPtoDP(LPPOINT points, int count = 1) const;
	void LPtoDP(LPRECT rect) const;
	void DPtoLP(LPPOINT points, int count = 1) const;
	void DPtoLP(LPRECT rect) const;

	// Clipping
	int GetClipBox(LPRECT rect) const;
	int IntersectClipRect(int left, int top, int right, int bottom);
	int IntersectClipRect(LPCRECT rect);
	int ExcludeClipRect(int left, int top, int right, int bottom);
	int ExcludeClipRect(LPCRECT rect);
	int SelectClipRgn(CRgn *region);
	BOOL RectVisible(LPCRECT rect) const;
	BOOL PtVisible(int x, int y) const;

	// Lines and shapes
	CPoint MoveTo(int x, int y);
	CPoint MoveTo(POINT point);
	BOOL LineTo(int x, int y);
	BOOL LineTo(POINT point);
	BOOL Polyline(const POINT *points, int count);
	BOOL Polygon(const POINT *points, int count);
	BOOL PolyPolyline(const POINT *points, const DWORD *counts, int polylines);
	BOOL Rectangle(int left, int top, int right, int bottom);
	BOOL Rectangle(LPCRECT rect);
	BOOL RoundRect(int left, int top, int right, int bottom, int w, int h);
	BOOL Ellipse(int left, int top, int right, int bottom);
	BOOL Ellipse(LPCRECT rect);
	void FillRect(LPCRECT rect, CBrush *brush);
	void FrameRect(LPCRECT rect, CBrush *brush);
	void InvertRect(LPCRECT rect);
	void FillSolidRect(LPCRECT rect, COLORREF color);
	void FillSolidRect(int x, int y, int cx, int cy, COLORREF color);
	void Draw3dRect(LPCRECT rect, COLORREF topLeft, COLORREF bottomRight);
	void Draw3dRect(int x, int y, int cx, int cy, COLORREF topLeft, COLORREF bottomRight);
	void DrawFocusRect(LPCRECT rect);
	BOOL DrawEdge(LPRECT rect, UINT edge, UINT flags);
	BOOL DrawFrameControl(LPRECT rect, UINT type, UINT state);
	void DrawDragRect(LPCRECT rect, SIZE size, LPCRECT lastRect, SIZE lastSize, CBrush *brush = nullptr, CBrush *lastBrush = nullptr);
	COLORREF SetPixel(int x, int y, COLORREF color);
	COLORREF SetPixel(POINT point, COLORREF color);
	COLORREF GetPixel(int x, int y) const;
	BOOL SetPixelV(int x, int y, COLORREF color);
	BOOL DrawIcon(int x, int y, HICON icon);
	BOOL DrawIcon(POINT point, HICON icon);
	BOOL DrawState(CPoint pt, CSize size, HICON icon, UINT flags, HBRUSH brush = nullptr);

	// Bitmaps
	BOOL BitBlt(int x, int y, int w, int h, CDC *src, int xSrc, int ySrc, DWORD rop);
	BOOL StretchBlt(int x, int y, int w, int h, CDC *src, int xSrc, int ySrc, int wSrc, int hSrc, DWORD rop);
	BOOL PatBlt(int x, int y, int w, int h, DWORD rop);
	BOOL TransparentBlt(int x, int y, int w, int h, CDC *src, int xSrc, int ySrc, int wSrc, int hSrc, UINT transparent);
	BOOL AlphaBlend(int x, int y, int w, int h, CDC *src, int xSrc, int ySrc, int wSrc, int hSrc, BLENDFUNCTION blend);

	// Text
	BOOL TextOut(int x, int y, LPCTSTR text, int count);
	BOOL TextOut(int x, int y, const CString &text);
	BOOL ExtTextOut(int x, int y, UINT options, LPCRECT rect, LPCTSTR text, UINT count, LPINT dx);
	BOOL ExtTextOut(int x, int y, UINT options, LPCRECT rect, const CString &text, LPINT dx);
	int DrawText(LPCTSTR text, int count, LPRECT rect, UINT format);
	int DrawText(const CString &text, LPRECT rect, UINT format);
	CSize GetTextExtent(LPCTSTR text, int count) const;
	CSize GetTextExtent(const CString &text) const;
	CSize GetOutputTextExtent(LPCTSTR text, int count) const;
	CSize GetOutputTextExtent(const CString &text) const;
	BOOL GetTextMetrics(LPTEXTMETRIC metrics) const;
	int GetTextFace(int count, LPTSTR face) const;
	int GetTextFace(CString &face) const;
};
class CPaintDC : public CDC
{
public:
	explicit CPaintDC(CWnd *wnd);
	~CPaintDC() override;
	PAINTSTRUCT m_ps{};
protected:
	HWND m_hWnd = nullptr;
};
class CClientDC : public CDC
{
public:
	explicit CClientDC(CWnd *wnd);
	~CClientDC() override;
protected:
	HWND m_hWnd = nullptr;
};
class CWindowDC : public CDC
{
public:
	explicit CWindowDC(CWnd *wnd);
	~CWindowDC() override;
protected:
	HWND m_hWnd = nullptr;
};

class CImageList : public CObject
{
public:
	HIMAGELIST m_hImageList = nullptr;
	CImageList() = default;
	CImageList(const CImageList &) = delete;
	CImageList &operator=(const CImageList &) = delete;
	virtual ~CImageList();
	static CImageList *FromHandle(HIMAGELIST list);
	operator HIMAGELIST() const { return m_hImageList; }
	HIMAGELIST GetSafeHandle() const { return this ? m_hImageList : nullptr; }
	BOOL Create(int cx, int cy, UINT flags, int initial, int grow);
	BOOL Create(UINT bitmapID, int cx, int grow, COLORREF mask);
	BOOL DeleteImageList();
	int Add(CBitmap *bitmap, CBitmap *mask);
	int Add(CBitmap *bitmap, COLORREF mask);
	int Add(HICON icon);
	BOOL Remove(int image);
	BOOL Replace(int image, CBitmap *bitmap, CBitmap *mask);
	int GetImageCount() const;
	BOOL Draw(CDC *dc, int image, POINT pt, UINT style);
	COLORREF SetBkColor(COLORREF color);
	BOOL SetImageCount(UINT count);
	HICON ExtractIcon(int image);
	BOOL Attach(HIMAGELIST list) { m_hImageList = list; return list != nullptr; }
	HIMAGELIST Detach() { HIMAGELIST h = m_hImageList; m_hImageList = nullptr; return h; }
};

// ---- windows -------------------------------------------------------------
class CMenu : public CObject
{
public:
	HMENU m_hMenu = nullptr;
	// Destroys the menu it holds, as MFC does.
	virtual ~CMenu();
	operator HMENU() const { return m_hMenu; }
	HMENU GetSafeHmenu() const { return this ? m_hMenu : nullptr; }
	BOOL Attach(HMENU menu) { m_hMenu = menu; return menu != nullptr; }
	HMENU Detach() { HMENU h = m_hMenu; m_hMenu = nullptr; return h; }
	static CMenu *FromHandle(HMENU menu);
	BOOL CreateMenu();
	BOOL CreatePopupMenu();
	BOOL LoadMenu(UINT id);
	BOOL LoadMenu(LPCTSTR name);
	BOOL DestroyMenu();
	BOOL AppendMenu(UINT flags, UINT_PTR id = 0, LPCTSTR text = nullptr);
	BOOL InsertMenu(UINT position, UINT flags, UINT_PTR id = 0, LPCTSTR text = nullptr);
	BOOL InsertMenuItem(UINT item, LPMENUITEMINFO info, BOOL byPos = FALSE);
	BOOL ModifyMenu(UINT position, UINT flags, UINT_PTR id = 0, LPCTSTR text = nullptr);
	BOOL RemoveMenu(UINT position, UINT flags);
	BOOL DeleteMenu(UINT position, UINT flags);
	UINT CheckMenuItem(UINT id, UINT check);
	BOOL CheckMenuRadioItem(UINT first, UINT last, UINT item, UINT flags);
	UINT EnableMenuItem(UINT id, UINT enable);
	UINT GetMenuState(UINT id, UINT flags) const;
	int GetMenuItemCount() const;
	UINT GetMenuItemID(int pos) const;
	int GetMenuString(UINT id, LPTSTR text, int max, UINT flags) const;
	int GetMenuString(UINT id, CString &text, UINT flags) const;
	BOOL GetMenuItemInfo(UINT item, LPMENUITEMINFO info, BOOL byPos = FALSE);
	BOOL SetMenuItemInfo(UINT item, LPMENUITEMINFO info, BOOL byPos = FALSE);
	BOOL SetDefaultItem(UINT item, BOOL byPos = FALSE);
	CMenu *GetSubMenu(int pos) const;
	BOOL TrackPopupMenu(UINT flags, int x, int y, CWnd *wnd, LPCRECT rect = nullptr);
	BOOL TrackPopupMenuEx(UINT flags, int x, int y, CWnd *wnd, LPTPMPARAMS params);
};

// CWnd::m_nFlags and RunModalLoop() flags, as in MFC.
#define WF_TOOLTIPS 0x0001
#define WF_TEMPHIDE 0x0002
#define WF_STAYDISABLED 0x0004
#define WF_MODALLOOP 0x0008
#define WF_CONTINUEMODAL 0x0010
#define WF_STAYACTIVE 0x0020
#define WF_NOPOPMSG 0x0040
#define WF_MODALDISABLE 0x0080
#define MLF_NOIDLEMSG 0x0001
#define MLF_NOKICKIDLE 0x0002
#define MLF_SHOWONIDLE 0x0004

class CWnd : public CCmdTarget
{
	LINOPENMPT_MFC_DYNAMIC(CWnd)
public:
	HWND m_hWnd = nullptr;
	// The window procedure MFC subclassed, called for default processing.
	WNDPROC m_pfnSuper = nullptr;
	UINT m_nFlags = 0;
	int m_nModalResult = 0;
	CWnd() = default;
	// Destroys the window if it still exists, as MFC does.
	virtual ~CWnd();
	HWND GetSafeHwnd() const { return this ? m_hWnd : nullptr; }
	operator HWND() const { return m_hWnd; }
	static CWnd *FromHandle(HWND h);
	BOOL IsWindowVisible() const;
	BOOL IsWindowEnabled() const;
	BOOL EnableWindow(BOOL enable = TRUE);
	BOOL ShowWindow(int cmd);
	void NotifyWinEvent(DWORD event, LONG idObjectType, LONG idObject);
	void Invalidate(BOOL erase = TRUE);
	void InvalidateRect(LPCRECT rect, BOOL erase = TRUE);
	void UpdateWindow();
	void GetClientRect(LPRECT rect) const;
	void GetWindowRect(LPRECT rect) const;
	void SetWindowText(LPCTSTR text);
	int GetWindowText(LPTSTR buf, int max) const;
	void GetWindowText(CString &text) const;
	int GetWindowTextLength() const;
	CWnd *GetDlgItem(int id) const;
	CWnd *GetParent() const;
	CWnd *SetFocus();
	static CWnd *GetFocus();
	BOOL PostMessage(UINT msg, WPARAM wp = 0, LPARAM lp = 0);
	LRESULT SendMessage(UINT msg, WPARAM wp = 0, LPARAM lp = 0);
	BOOL SendNotifyMessage(UINT msg, WPARAM wp, LPARAM lp);
	void SendMessageToDescendants(UINT msg, WPARAM wp = 0, LPARAM lp = 0, BOOL deep = TRUE, BOOL onlyPerm = FALSE);
	BOOL GetWindowPlacement(WINDOWPLACEMENT *placement) const;
	BOOL SetWindowPlacement(const WINDOWPLACEMENT *placement);
	void SetRedraw(BOOL redraw = TRUE);
	void MoveWindow(int x, int y, int w, int h, BOOL repaint = TRUE);
	void MoveWindow(LPCRECT rect, BOOL repaint = TRUE);
	BOOL SetWindowPos(const CWnd *after, int x, int y, int cx, int cy, UINT flags);
	void ScreenToClient(LPPOINT p) const;
	void ClientToScreen(LPPOINT p) const;
	void ScreenToClient(LPRECT r) const;
	void ClientToScreen(LPRECT r) const;
	CDC *GetDC();
	int ReleaseDC(CDC *dc);
	void SetFont(CFont *font, BOOL redraw = TRUE);
	CFont *GetFont() const;
	int MessageBox(LPCTSTR text, LPCTSTR caption = nullptr, UINT type = MB_OK);
	UINT_PTR SetTimer(UINT_PTR id, UINT elapse, void *proc = nullptr);
	BOOL KillTimer(UINT_PTR id);
	CMenu *GetMenu() const;
	int GetDlgCtrlID() const;
	void CheckDlgButton(int id, UINT check);
	void CheckRadioButton(int firstID, int lastID, int checkID);
	int GetCheckedRadioButton(int firstID, int lastID);
	int GetDlgItemText(int id, LPTSTR buf, int max) const;
	int GetDlgItemText(int id, CString &text) const;
	static CWnd *GetActiveWindow();
	static CWnd *GetForegroundWindow();
	static CWnd *GetDesktopWindow();
	HICON SetIcon(HICON icon, BOOL bigIcon);
	HICON GetIcon(BOOL bigIcon) const;
	UINT IsDlgButtonChecked(int id) const;
	void SetDlgItemText(int id, LPCTSTR text);
	UINT GetDlgItemInt(int id, BOOL *ok = nullptr, BOOL isSigned = TRUE) const;
	void SetDlgItemInt(int id, UINT value, BOOL isSigned = TRUE);
	// Message processing, implemented in platform/mfc as in MFC: the window
	// procedure of every MFC window calls WindowProc, which dispatches
	// through the message map (OnWndMsg) or falls back to DefWindowProc.
	virtual BOOL PreTranslateMessage(MSG *msg);
	virtual BOOL OnCommand(WPARAM wp, LPARAM lp);
	virtual BOOL OnNotify(WPARAM wp, LPARAM lp, LRESULT *result);
	virtual LRESULT WindowProc(UINT msg, WPARAM wp, LPARAM lp);
	virtual BOOL OnWndMsg(UINT msg, WPARAM wp, LPARAM lp, LRESULT *result);
	// Messages a parent reflects back to the control that sent them.
	virtual BOOL OnChildNotify(UINT msg, WPARAM wp, LPARAM lp, LRESULT *result);
	BOOL ReflectChildNotify(UINT msg, WPARAM wp, LPARAM lp, LRESULT *result);
	static BOOL ReflectLastMsg(HWND child, LRESULT *result = nullptr);
	BOOL SendChildNotifyLastMsg(LRESULT *result = nullptr);
	// The message being processed, as GetCurrentMessage() in MFC.
	static const MSG *GetCurrentMessage();
	// PreTranslateMessage() of each window from msg->hwnd up to stop.
	static BOOL WalkPreTranslateTree(HWND stop, MSG *msg);
	virtual void PreSubclassWindow() { }
	virtual WNDPROC *GetSuperWndProcAddr() { return &m_pfnSuper; }
	virtual void DoDataExchange(CDataExchange *dx) { }
	virtual BOOL PreCreateWindow(CREATESTRUCT &cs);
	virtual BOOL DestroyWindow();
	virtual void PostNcDestroy() { }
	virtual INT_PTR OnToolHitTest(CPoint point, TOOLINFO *ti) const { return 0; }
	virtual BOOL OnGesturePan(CPoint from, CPoint to) { return FALSE; }
	virtual BOOL OnGestureZoom(CPoint center, long delta) { return FALSE; }
	virtual BOOL OnGestureRotate(CPoint center, double radians) { return FALSE; }
	virtual BOOL OnGestureTwoFingerTap(CPoint center) { return FALSE; }
	virtual BOOL OnGesturePressAndTap(CPoint press, long delta) { return FALSE; }
	CWnd *SetParent(CWnd *newParent);
	virtual BOOL Create(LPCTSTR className, LPCTSTR windowName, DWORD style, const RECT &rect, CWnd *parent, UINT id, CCreateContext *context = nullptr);
	BOOL CreateEx(DWORD exStyle, LPCTSTR className, LPCTSTR windowName, DWORD style, const RECT &rect, CWnd *parent, UINT id, LPVOID param = nullptr);
	BOOL CreateEx(DWORD exStyle, LPCTSTR className, LPCTSTR windowName, DWORD style, int x, int y, int width, int height, HWND parent, HMENU menuOrId, LPVOID param = nullptr);
	BOOL SubclassDlgItem(UINT id, CWnd *parent);
	BOOL SubclassWindow(HWND hwnd);
	HWND UnsubclassWindow();
	DWORD GetStyle() const;
	DWORD GetExStyle() const;
	BOOL ModifyStyle(DWORD remove, DWORD add, UINT flags = 0);
	BOOL ModifyStyleEx(DWORD remove, DWORD add, UINT flags = 0);
	BOOL OpenClipboard();
	void DragAcceptFiles(BOOL accept = TRUE);
	CWnd *SetCapture();
	static CWnd *GetCapture();
	BOOL IsIconic() const;
	BOOL IsZoomed() const;
	BOOL IsChild(const CWnd *wnd) const;
	CWnd *GetTopLevelParent() const;
	class CFrameWnd *GetParentFrame() const;
	CWnd *GetWindow(UINT cmd) const;
	CWnd *GetNextDlgTabItem(CWnd *ctrl, BOOL previous = FALSE) const;
	CWnd *SetActiveWindow();
	BOOL SetForegroundWindow();
	void BringWindowToTop();
	void CenterWindow(CWnd *alternateOwner = nullptr);
	int SetScrollPos(int bar, int pos, BOOL redraw = TRUE);
	int GetScrollPos(int bar) const;
	void SetScrollRange(int bar, int minPos, int maxPos, BOOL redraw = TRUE);
	BOOL SetScrollInfo(int bar, struct SCROLLINFO *info, BOOL redraw = TRUE);
	BOOL GetScrollInfo(int bar, struct SCROLLINFO *info, UINT mask = 0xFFFF);
	void ShowScrollBar(UINT bar, BOOL show = TRUE);
	void ScrollWindow(int dx, int dy, LPCRECT rect = nullptr, LPCRECT clip = nullptr);
	int ScrollWindowEx(int dx, int dy, LPCRECT scroll, LPCRECT clip, CRgn *update, LPRECT updateRect, UINT flags);
	BOOL RedrawWindow(LPCRECT update = nullptr, CRgn *updateRgn = nullptr, UINT flags = 0);
	void ValidateRect(LPCRECT rect);
	CDC *GetWindowDC();
	CMenu *GetSystemMenu(BOOL revert) const;
	BOOL SetMenu(CMenu *menu);
	void DrawMenuBar();
	LRESULT Default();
	virtual LRESULT DefWindowProc(UINT msg, WPARAM wp, LPARAM lp);
	static CWnd *FromHandlePermanent(HWND hwnd);
	// SetWindowPos() insert-after placeholders.
	static const CWnd wndTop;
	static const CWnd wndBottom;
	static const CWnd wndTopMost;
	static const CWnd wndNoTopMost;
	BOOL Attach(HWND hwnd);
	HWND Detach();
	CWnd *GetNextWindow(UINT flag = GW_HWNDNEXT) const;
	void MapWindowPoints(CWnd *to, LPPOINT points, UINT count) const;
	void MapWindowPoints(CWnd *to, LPRECT rect) const;
	BOOL EnableToolTips(BOOL enable = TRUE);
	BOOL EnableTrackingToolTips(BOOL enable = TRUE);
	int GetScrollLimit(int bar);
	void GetScrollRange(int bar, LPINT minPos, LPINT maxPos) const;
	virtual class CScrollBar *GetScrollBarCtrl(int bar) const { return nullptr; }
	void EnableScrollBarCtrl(int bar, BOOL enable = TRUE);
	BOOL EnableScrollBar(int bar, UINT arrows = ESB_ENABLE_BOTH);
	void EnableDynamicLayout(BOOL enable = TRUE);
	BOOL IsDynamicLayoutEnabled() const;
	BOOL UpdateData(BOOL saveAndValidate = TRUE);
	// Modal loops (dialogs, and any window run modally), as in MFC.
	virtual int RunModalLoop(DWORD flags = 0);
	virtual BOOL ContinueModal();
	virtual void EndModalLoop(int result);
	// The dialog manager for this window's keyboard input.
	BOOL IsDialogMessage(LPMSG msg);
	BOOL PreTranslateInput(LPMSG msg);
	// Dialog creation from a template (CDialog, CDialogBar, CFormView).
	BOOL CreateDlg(LPCTSTR templateName, CWnd *parent);
	BOOL CreateDlgIndirect(LPCDLGTEMPLATE dlgTemplate, CWnd *parent, HINSTANCE instance = nullptr);
	// The RT_DLGINIT resource: list and combo box strings from the template.
	BOOL ExecuteDlgInit(LPCTSTR templateName);
	BOOL ExecuteDlgInit(LPVOID resource);
	CWnd *ChildWindowFromPoint(POINT point) const;
	static CWnd *WindowFromPoint(POINT point);
	BOOL ShowOwnedPopups(BOOL show = TRUE);
	CWnd *GetOwner() const;
	void SetOwner(CWnd *owner);
	CWnd *GetDescendantWindow(int id, BOOL onlyPerm = FALSE) const;
	BOOL IsWindowEnabled();
	void SetWindowContextHelpId(DWORD id);
	// Default message handlers, as in MFC. Upstream calls these as
	// CWnd::OnXxx (or Base::OnXxx) to get default processing.
	afx_msg int OnCreate(LPCREATESTRUCT cs);
	afx_msg void OnDestroy();
	afx_msg void OnClose();
	afx_msg void OnSize(UINT type, int cx, int cy);
	afx_msg void OnMove(int x, int y);
	afx_msg void OnPaint();
	afx_msg BOOL OnEraseBkgnd(CDC *dc);
	afx_msg void OnSetFocus(CWnd *oldWnd);
	afx_msg void OnKillFocus(CWnd *newWnd);
	afx_msg void OnTimer(UINT_PTR id);
	afx_msg void OnHScroll(UINT code, UINT pos, class CScrollBar *scrollBar);
	afx_msg void OnVScroll(UINT code, UINT pos, class CScrollBar *scrollBar);
	afx_msg void OnMouseMove(UINT flags, CPoint point);
	afx_msg void OnLButtonDown(UINT flags, CPoint point);
	afx_msg void OnLButtonUp(UINT flags, CPoint point);
	afx_msg void OnLButtonDblClk(UINT flags, CPoint point);
	afx_msg void OnRButtonDown(UINT flags, CPoint point);
	afx_msg void OnRButtonUp(UINT flags, CPoint point);
	afx_msg void OnRButtonDblClk(UINT flags, CPoint point);
	afx_msg void OnMButtonDown(UINT flags, CPoint point);
	afx_msg void OnMButtonUp(UINT flags, CPoint point);
	afx_msg void OnMButtonDblClk(UINT flags, CPoint point);
	afx_msg void OnXButtonDown(UINT flags, UINT button, CPoint point);
	afx_msg void OnXButtonUp(UINT flags, UINT button, CPoint point);
	afx_msg BOOL OnMouseWheel(UINT flags, short delta, CPoint point);
	afx_msg void OnMouseHWheel(UINT flags, short delta, CPoint point);
	afx_msg void OnMouseHover(UINT flags, CPoint point);
	afx_msg void OnMouseLeave();
	afx_msg int OnMouseActivate(CWnd *topParent, UINT hitTest, UINT message);
	afx_msg void OnChar(UINT ch, UINT repCnt, UINT flags);
	afx_msg void OnKeyDown(UINT ch, UINT repCnt, UINT flags);
	afx_msg void OnKeyUp(UINT ch, UINT repCnt, UINT flags);
	afx_msg void OnSysKeyDown(UINT ch, UINT repCnt, UINT flags);
	afx_msg UINT OnGetDlgCode();
	afx_msg void OnShowWindow(BOOL show, UINT status);
	afx_msg void OnContextMenu(CWnd *wnd, CPoint pos);
	afx_msg void OnDropFiles(HDROP drop);
	afx_msg void OnGetMinMaxInfo(MINMAXINFO *mmi);
	afx_msg BOOL OnSetCursor(CWnd *wnd, UINT hitTest, UINT message);
	afx_msg void OnActivate(UINT state, CWnd *other, BOOL minimized);
	afx_msg void OnActivateApp(BOOL active, DWORD threadID);
	afx_msg void OnSysCommand(UINT id, LPARAM lParam);
	afx_msg void OnNcCalcSize(BOOL calcValidRects, NCCALCSIZE_PARAMS *params);
	afx_msg LRESULT OnNcHitTest(CPoint point);
	afx_msg void OnNcPaint();
	afx_msg BOOL OnNcActivate(BOOL active);
	afx_msg void OnNcMouseMove(UINT hitTest, CPoint point);
	afx_msg void OnNcLButtonDown(UINT hitTest, CPoint point);
	afx_msg void OnNcLButtonUp(UINT hitTest, CPoint point);
	afx_msg void OnNcLButtonDblClk(UINT hitTest, CPoint point);
	afx_msg void OnInitMenu(CMenu *menu);
	afx_msg void OnInitMenuPopup(CMenu *popup, UINT index, BOOL sysMenu);
	afx_msg void OnMenuSelect(UINT item, UINT flags, HMENU sysMenu);
	afx_msg void OnDrawItem(int id, LPDRAWITEMSTRUCT dis);
	afx_msg void OnMeasureItem(int id, LPMEASUREITEMSTRUCT mis);
	afx_msg BOOL OnDeviceChange(UINT eventType, DWORD_PTR data);
	afx_msg BOOL OnQueryEndSession();
	afx_msg void OnEndSession(BOOL ending);
	afx_msg HBRUSH OnCtlColor(CDC *dc, CWnd *wnd, UINT ctlColor);
	afx_msg void OnNcDestroy();
	afx_msg void OnSysKeyUp(UINT ch, UINT repCnt, UINT flags);
	afx_msg void OnSysChar(UINT ch, UINT repCnt, UINT flags);
	afx_msg void OnCaptureChanged(CWnd *wnd);
	afx_msg void OnWindowPosChanged(WINDOWPOS *pos);
	afx_msg void OnWindowPosChanging(WINDOWPOS *pos);
	afx_msg void OnParentNotify(UINT message, LPARAM lParam);
	afx_msg void OnEnable(BOOL enable);
	// Active Accessibility hooks (no-ops on Linux; Qt provides accessibility).
	void EnableActiveAccessibility() { }
	virtual HRESULT get_accName(VARIANT child, BSTR *name) { return 0; }
	virtual HRESULT get_accValue(VARIANT child, BSTR *value) { return 0; }
	virtual HRESULT get_accDescription(VARIANT child, BSTR *description) { return 0; }
	virtual HRESULT get_accRole(VARIANT child, VARIANT *role) { return 0; }
};

class CMFCDynamicLayout
{
public:
	// How far a control moves or grows, in percent of the dialog's growth.
	struct MoveSettings
	{
		int m_nXRatio = 0;
		int m_nYRatio = 0;
		bool IsHorizontal() const { return m_nXRatio > 0; }
		bool IsVertical() const { return m_nYRatio > 0; }
		bool IsNone() const { return !IsHorizontal() && !IsVertical(); }
	};
	using SizeSettings = MoveSettings;
	BOOL AddItem(HWND hwnd, MoveSettings move, SizeSettings size);
};

// MFC's reader for AFX_DIALOG_LAYOUT resources: a version WORD (0), then
// four WORDs per dialog control, in the dialog's child order: move X, move
// Y, size X and size Y ratios.
class CMFCDynamicLayoutData
{
public:
	struct Item
	{
		CMFCDynamicLayout::MoveSettings m_moveSettings;
		CMFCDynamicLayout::SizeSettings m_sizeSettings;
	};
	BOOL ReadResource(LPVOID resource, UINT size)
	{
		m_listCtrls.RemoveAll();
		const BYTE *data = static_cast<const BYTE *>(resource);
		if(data == nullptr || size < 2)
			return FALSE;
		auto word = [data](UINT offset) { return static_cast<int>(static_cast<short>(data[offset] | (data[offset + 1] << 8))); };
		if(word(0) != 0)
			return FALSE;
		for(UINT offset = 2; offset + 8 <= size; offset += 8)
		{
			Item item;
			item.m_moveSettings.m_nXRatio = word(offset);
			item.m_moveSettings.m_nYRatio = word(offset + 2);
			item.m_sizeSettings.m_nXRatio = word(offset + 4);
			item.m_sizeSettings.m_nYRatio = word(offset + 6);
			m_listCtrls.AddTail(item);
		}
		return TRUE;
	}

protected:
	CList<Item, Item &> m_listCtrls;
};

class CStatic : public CWnd
{
public:
	BOOL Create(LPCTSTR text, DWORD style, const RECT &rect, CWnd *parent, UINT id = 0xffff);
	HICON SetIcon(HICON icon);
	HICON GetIcon() const;
	HBITMAP SetBitmap(HBITMAP bitmap);
	HBITMAP GetBitmap() const;
	virtual void DrawItem(LPDRAWITEMSTRUCT dis) { }
	BOOL OnChildNotify(UINT msg, WPARAM wp, LPARAM lp, LRESULT *result) override;
};
class CButton : public CWnd
{
public:
	BOOL Create(LPCTSTR caption, DWORD style, const RECT &rect, CWnd *parent, UINT id);
	int GetCheck() const;
	void SetCheck(int check);
	UINT GetState() const;
	void SetState(BOOL highlight);
	UINT GetButtonStyle() const;
	void SetButtonStyle(UINT style, BOOL redraw = TRUE);
	HICON SetIcon(HICON icon);
	HICON GetIcon() const;
	HBITMAP SetBitmap(HBITMAP bitmap);
	HBITMAP GetBitmap() const;
	virtual void DrawItem(LPDRAWITEMSTRUCT dis) { }
	BOOL OnChildNotify(UINT msg, WPARAM wp, LPARAM lp, LRESULT *result) override;
};
class CEdit : public CWnd
{
public:
	void SetSel(int start, int end, BOOL noScroll = FALSE);
	void GetSel(int &start, int &end) const;
	void ReplaceSel(LPCTSTR text, BOOL canUndo = FALSE);
	void SetLimitText(UINT max);
	UINT GetLimitText() const;
	void LimitText(int max = 0);
	BOOL SetReadOnly(BOOL ro = TRUE);
	BOOL Create(DWORD style, const RECT &rect, CWnd *parent, UINT id);
	int GetLineCount() const;
	int LineIndex(int line = -1) const;
	int LineFromChar(int index = -1) const;
	int LineLength(int line = -1) const;
	int GetLine(int index, LPTSTR buffer, int maxLength) const;
	void LineScroll(int lines, int chars = 0);
	int GetFirstVisibleLine() const;
	BOOL Undo();
	BOOL CanUndo() const;
	void Clear();
	void Copy();
	void Cut();
	void Paste();
	void SetMargins(UINT left, UINT right);
	void SetPasswordChar(TCHAR ch);
	BOOL GetModify() const;
	void SetModify(BOOL modified = TRUE);
	BOOL FmtLines(BOOL addEOL);
	int CharFromPos(CPoint pt) const;
	CPoint PosFromChar(UINT ch) const;
};
class CComboBox : public CWnd
{
public:
	BOOL Create(DWORD style, const RECT &rect, CWnd *parent, UINT id);
	virtual void DrawItem(LPDRAWITEMSTRUCT dis) { }
	virtual void MeasureItem(LPMEASUREITEMSTRUCT mis) { }
	virtual int CompareItem(LPCOMPAREITEMSTRUCT cis) { return 0; }
	virtual void DeleteItem(LPDELETEITEMSTRUCT dis) { }
	BOOL OnChildNotify(UINT msg, WPARAM wp, LPARAM lp, LRESULT *result) override;
	BOOL GetComboBoxInfo(PCOMBOBOXINFO info) const;
	int InitStorage(int items, UINT bytes);
	void Clear();
	void Copy();
	void Cut();
	void Paste();
	BOOL LimitText(int max);
	DWORD GetEditSel() const;
	BOOL SetEditSel(int start, int end);
	int GetLBTextLen(int index) const;
	int GetLBText(int index, LPTSTR text) const;
	int FindString(int start, LPCTSTR text) const;
	int SelectString(int start, LPCTSTR text);
	void *GetItemDataPtr(int index) const;
	int SetItemDataPtr(int index, void *data);
	void ShowDropDown(BOOL show = TRUE);
	BOOL GetDroppedState() const;
	int SetDroppedWidth(UINT width);
	int GetDroppedWidth() const;
	int GetTopIndex() const;
	int SetTopIndex(int index);
	int SetItemHeight(int index, UINT height);
	int GetItemHeight(int index) const;
	int GetCount() const;
	int GetCurSel() const;
	int SetCurSel(int sel);
	int AddString(LPCTSTR text);
	int InsertString(int index, LPCTSTR text);
	int DeleteString(UINT index);
	void ResetContent();
	DWORD_PTR GetItemData(int index) const;
	int SetItemData(int index, DWORD_PTR data);
	void GetLBText(int index, CString &text) const;
	int FindStringExact(int start, LPCTSTR text) const;
};
class CComboBoxEx : public CComboBox
{
public:
	int InsertItem(const COMBOBOXEXITEM *item);
	int DeleteItem(int index);
	BOOL GetItem(COMBOBOXEXITEM *item);
	BOOL SetItem(const COMBOBOXEXITEM *item);
	CImageList *SetImageList(CImageList *list);
	CImageList *GetImageList() const;
	CEdit *GetEditCtrl() const;
	CComboBox *GetComboBoxCtrl() const;
	DWORD SetExtendedStyle(DWORD mask, DWORD style);
};
class CListBox : public CWnd
{
public:
	BOOL Create(DWORD style, const RECT &rect, CWnd *parent, UINT id);
	virtual void DrawItem(LPDRAWITEMSTRUCT dis) { }
	virtual void MeasureItem(LPMEASUREITEMSTRUCT mis) { }
	virtual int CompareItem(LPCOMPAREITEMSTRUCT cis) { return 0; }
	virtual void DeleteItem(LPDELETEITEMSTRUCT dis) { }
	virtual int VKeyToItem(UINT key, UINT index) { return -1; }
	virtual int CharToItem(UINT ch, UINT index) { return -1; }
	BOOL OnChildNotify(UINT msg, WPARAM wp, LPARAM lp, LRESULT *result) override;
	int GetCount() const;
	int GetCurSel() const;
	int SetCurSel(int sel);
	int AddString(LPCTSTR text);
	int InsertString(int index, LPCTSTR text);
	int DeleteString(UINT index);
	void ResetContent();
	DWORD_PTR GetItemData(int index) const;
	int SetItemData(int index, DWORD_PTR data);
	int GetText(int index, LPTSTR buf) const;
	void GetText(int index, CString &text) const;
	int GetSel(int index) const;
	int SetSel(int index, BOOL select = TRUE);
	int GetSelCount() const;
	int GetTopIndex() const;
	int SetTopIndex(int index);
	void *GetItemDataPtr(int index) const;
	int SetItemDataPtr(int index, void *data);
	int GetItemRect(int index, LPRECT rect) const;
	int GetItemHeight(int index) const;
	int SetItemHeight(int index, UINT height);
	int FindString(int start, LPCTSTR text) const;
	int FindStringExact(int start, LPCTSTR text) const;
	int GetTextLen(int index) const;
	int GetCaretIndex() const;
	int SetCaretIndex(int index, BOOL scroll = TRUE);
	int GetSelItems(int maxItems, LPINT items) const;
	int SelItemRange(BOOL select, int first, int last);
	UINT ItemFromPoint(CPoint pt, BOOL &outside) const;
};
// A list box with a check box on each item (owner-drawn, as in MFC); the
// parent gets CLBN_CHKCHANGE when the user changes one.
class CCheckListBox : public CListBox
{
public:
	void SetCheckStyle(UINT style);
	UINT GetCheckStyle() const { return m_nStyle; }
	void SetCheck(int index, int check);
	int GetCheck(int index);
	void Enable(int index, BOOL enabled = TRUE);
	BOOL IsEnabled(int index);
	void DrawItem(LPDRAWITEMSTRUCT dis) override;
	void MeasureItem(LPMEASUREITEMSTRUCT mis) override;
	LRESULT WindowProc(UINT msg, WPARAM wp, LPARAM lp) override;
	void PreSubclassWindow() override;
protected:
	struct CheckState
	{
		int check = 0;
		bool enabled = true;
	};
	int CheckBoxSize() const;
	int MinItemHeight() const;
	void ToggleCheck(int index);
	void InvalidateCheck(int index);
	UINT m_nStyle = BS_CHECKBOX;
	std::vector<CheckState> m_checks;
};
class CScrollBar : public CWnd
{
public:
	BOOL Create(DWORD style, const RECT &rect, CWnd *parent, UINT id);
	void GetScrollRange(LPINT minPos, LPINT maxPos) const;
	BOOL SetScrollInfo(LPSCROLLINFO info, BOOL redraw = TRUE);
	BOOL GetScrollInfo(LPSCROLLINFO info, UINT mask = SIF_ALL);
	int GetScrollLimit();
	void ShowScrollBar(BOOL show = TRUE);
	int GetScrollPos() const;
	int SetScrollPos(int pos, BOOL redraw = TRUE);
	void SetScrollRange(int minPos, int maxPos, BOOL redraw = TRUE);
};
class CSliderCtrl : public CWnd
{
public:
	BOOL Create(DWORD style, const RECT &rect, CWnd *parent, UINT id);
	int GetLineSize() const;
	int GetPageSize() const;
	void GetRange(int &minPos, int &maxPos) const;
	void SetRangeMin(int minPos, BOOL redraw = FALSE);
	void SetRangeMax(int maxPos, BOOL redraw = FALSE);
	void ClearTics(BOOL redraw = FALSE);
	BOOL SetTic(int pos);
	void SetSelection(int minPos, int maxPos);
	void ClearSel(BOOL redraw = FALSE);
	class CToolTipCtrl *GetToolTips() const;
	int GetPos() const;
	void SetPos(int pos);
	void SetRange(int minPos, int maxPos, BOOL redraw = FALSE);
	int GetRangeMin() const;
	int GetRangeMax() const;
	void SetTicFreq(int freq);
	void SetPageSize(int size);
	void SetLineSize(int size);
};
class CSpinButtonCtrl : public CWnd
{
public:
	BOOL Create(DWORD style, const RECT &rect, CWnd *parent, UINT id);
	int GetPos32(BOOL *error = nullptr) const;
	int SetPos32(int pos);
	int GetPos() const;
	int SetPos(int pos);
	void SetRange(short lower, short upper);
	DWORD GetRange() const;
	void GetRange(int &lower, int &upper) const;
	void SetRange32(int lower, int upper);
	void GetRange32(int &lower, int &upper) const;
	CWnd *SetBuddy(CWnd *buddy);
	CWnd *GetBuddy() const;
	UINT SetBase(int base);
	BOOL SetAccel(int count, struct UDACCEL *accel);
};
class CProgressCtrl : public CWnd
{
public:
	BOOL Create(DWORD style, const RECT &rect, CWnd *parent, UINT id);
	void SetRange32(int lower, int upper);
	void SetRange(short lower, short upper);
	int SetPos(int pos);
	int GetPos() const;
	int OffsetPos(int pos);
	int SetStep(int step);
	int StepIt();
	BOOL SetMarquee(BOOL on, UINT interval);
	int SetState(int state);
};
class CListCtrl : public CWnd
{
public:
	BOOL Create(DWORD style, const RECT &rect, CWnd *parent, UINT id);
	void SetItemCount(int count);
	BOOL SetItemCountEx(int count, DWORD flags = 0);
	int InsertColumn(int col, LPCTSTR heading, int format = LVCFMT_LEFT, int width = -1, int subItem = -1);
	BOOL DeleteColumn(int col);
	int GetColumnWidth(int col) const;
	BOOL SetColumnWidth(int col, int width);
	int GetItemCount() const;
	int InsertItem(int item, LPCTSTR text);
	int InsertItem(int item, LPCTSTR text, int image);
	int InsertItem(const LVITEM *item);
	int InsertItem(UINT mask, int item, LPCTSTR text, UINT state, UINT stateMask, int image, LPARAM lParam);
	BOOL SetItem(const LVITEM *item);
	BOOL GetItem(LVITEM *item) const;
	BOOL DeleteItem(int item);
	BOOL DeleteAllItems();
	BOOL SetItemText(int item, int subItem, LPCTSTR text);
	CString GetItemText(int item, int subItem) const;
	BOOL SetItemData(int item, DWORD_PTR data);
	DWORD_PTR GetItemData(int item) const;
	BOOL SetItemState(int item, UINT state, UINT mask);
	UINT GetItemState(int item, UINT mask) const;
	BOOL SetCheck(int item, BOOL check = TRUE);
	BOOL GetCheck(int item) const;
	UINT GetSelectedCount() const;
	int GetSelectionMark() const;
	int SetSelectionMark(int item);
	int GetNextItem(int item, int flags) const;
	POSITION GetFirstSelectedItemPosition() const;
	int GetNextSelectedItem(POSITION &pos) const;
	BOOL EnsureVisible(int item, BOOL partialOK);
	DWORD GetExtendedStyle() const;
	DWORD SetExtendedStyle(DWORD style);
	CImageList *SetImageList(CImageList *list, int type);
	int GetTopIndex() const;
	int GetCountPerPage() const;
	BOOL GetItemRect(int item, LPRECT rect, UINT code) const;
	int HitTest(CPoint pt, UINT *flags = nullptr) const;
	LRESULT EnableGroupView(BOOL enable);
	int InsertGroup(int index, LVGROUP *group);
	void RemoveAllGroups();
	BOOL SortItems(int (*compare)(LPARAM, LPARAM, LPARAM), DWORD_PTR data);
	CEdit *EditLabel(int item);
	CEdit *GetEditControl() const;
	class CHeaderCtrl *GetHeaderCtrl() const;
	BOOL GetColumn(int col, LVCOLUMN *column) const;
	BOOL SetColumn(int col, const LVCOLUMN *column);
	BOOL RedrawItems(int first, int last);
	BOOL Update(int item);
	BOOL GetSubItemRect(int item, int subItem, int area, CRect &rect);
	int SubItemHitTest(LVHITTESTINFO *info);
	COLORREF GetBkColor() const;
	BOOL SetBkColor(COLORREF color);
	COLORREF GetTextColor() const;
	BOOL SetTextColor(COLORREF color);
	COLORREF GetTextBkColor() const;
	BOOL SetTextBkColor(COLORREF color);
	BOOL GetViewRect(LPRECT rect) const;
	CSize ApproximateViewRect(CSize size = CSize(-1, -1), int count = -1) const;
};
class CMFCListCtrl : public CListCtrl
{
public:
	virtual int OnCompareItems(LPARAM item1, LPARAM item2, int column);
	virtual COLORREF OnGetCellTextColor(int row, int column);
	virtual COLORREF OnGetCellBkColor(int row, int column);
	virtual void Sort(int column, BOOL ascending = TRUE, BOOL add = FALSE);
	void EnableMarkSortedColumn(BOOL mark = TRUE, BOOL redraw = TRUE);
	int GetSortColumn() const { return m_iSortedColumn; }
	BOOL IsAscending() const { return m_bAscending; }

protected:
	afx_msg void OnCustomDraw(NMHDR *nmhdr, LRESULT *result);
	afx_msg BOOL OnHeaderItemClick(UINT id, NMHDR *nmhdr, LRESULT *result);
	static int CALLBACK CompareProc(LPARAM item1, LPARAM item2, LPARAM self);
	int m_iSortedColumn = -1;
	BOOL m_bAscending = TRUE;
	BOOL m_bMarkSortedColumn = FALSE;
	DECLARE_MESSAGE_MAP()
};
class CTreeCtrl : public CWnd
{
public:
	BOOL Create(DWORD style, const RECT &rect, CWnd *parent, UINT id);
	HTREEITEM InsertItem(LPTVINSERTSTRUCT insert);
	HTREEITEM InsertItem(UINT mask, LPCTSTR text, int image, int selectedImage, UINT state, UINT stateMask, LPARAM lParam, HTREEITEM parent, HTREEITEM after);
	HTREEITEM InsertItem(LPCTSTR text, HTREEITEM parent = TVI_ROOT, HTREEITEM after = TVI_LAST);
	HTREEITEM InsertItem(LPCTSTR text, int image, int selectedImage, HTREEITEM parent = TVI_ROOT, HTREEITEM after = TVI_LAST);
	BOOL DeleteItem(HTREEITEM item);
	BOOL DeleteAllItems();
	BOOL Expand(HTREEITEM item, UINT code);
	HTREEITEM GetNextItem(HTREEITEM item, UINT code) const;
	HTREEITEM GetChildItem(HTREEITEM item) const;
	HTREEITEM GetNextSiblingItem(HTREEITEM item) const;
	HTREEITEM GetPrevSiblingItem(HTREEITEM item) const;
	HTREEITEM GetParentItem(HTREEITEM item) const;
	HTREEITEM GetFirstVisibleItem() const;
	HTREEITEM GetNextVisibleItem(HTREEITEM item) const;
	HTREEITEM GetPrevVisibleItem(HTREEITEM item) const;
	HTREEITEM GetSelectedItem() const;
	HTREEITEM GetDropHilightItem() const;
	HTREEITEM GetRootItem() const;
	BOOL ItemHasChildren(HTREEITEM item) const;
	BOOL GetItem(TVITEM *item) const;
	BOOL SetItem(TVITEM *item);
	BOOL SetItem(HTREEITEM item, UINT mask, LPCTSTR text, int image, int selectedImage, UINT state, UINT stateMask, LPARAM lParam);
	CString GetItemText(HTREEITEM item) const;
	BOOL SetItemText(HTREEITEM item, LPCTSTR text);
	BOOL GetItemImage(HTREEITEM item, int &image, int &selectedImage) const;
	BOOL SetItemImage(HTREEITEM item, int image, int selectedImage);
	UINT GetItemState(HTREEITEM item, UINT mask) const;
	BOOL SetItemState(HTREEITEM item, UINT state, UINT mask);
	DWORD_PTR GetItemData(HTREEITEM item) const;
	BOOL SetItemData(HTREEITEM item, DWORD_PTR data);
	BOOL GetItemRect(HTREEITEM item, LPRECT rect, BOOL textOnly) const;
	BOOL Select(HTREEITEM item, UINT code);
	BOOL SelectItem(HTREEITEM item);
	BOOL SelectDropTarget(HTREEITEM item);
	BOOL SelectSetFirstVisible(HTREEITEM item);
	BOOL EnsureVisible(HTREEITEM item);
	HTREEITEM HitTest(CPoint pt, UINT *flags = nullptr) const;
	HTREEITEM HitTest(TVHITTESTINFO *info) const;
	CImageList *SetImageList(CImageList *list, int type);
	CImageList *GetImageList(UINT type) const;
	UINT GetIndent() const;
	void SetIndent(UINT indent);
	UINT GetCount() const;
	UINT GetVisibleCount() const;
	CEdit *EditLabel(HTREEITEM item);
	BOOL EndEditLabelNow(BOOL cancel);
	CEdit *GetEditControl() const;
	BOOL SortChildren(HTREEITEM item);
	DWORD SetExtendedStyle(DWORD mask, DWORD style);
	class CToolTipCtrl *GetToolTips() const;
};
class CTabCtrl : public CWnd
{
public:
	virtual BOOL Create(DWORD style, const RECT &rect, CWnd *parent, UINT id);
	LONG InsertItem(int item, LPCTSTR text, int image);
	LONG InsertItem(UINT mask, int item, LPCTSTR text, int image, LPARAM lParam);
	LONG InsertItem(UINT mask, int item, LPCTSTR text, int image, LPARAM lParam, DWORD state, DWORD stateMask);
	BOOL InsertItem(int item, LPCTSTR text);
	LONG InsertItem(int item, TCITEM *tcItem);
	BOOL DeleteItem(int item);
	BOOL DeleteAllItems();
	int GetItemCount() const;
	int GetCurSel() const;
	int SetCurSel(int item);
	int GetCurFocus() const;
	BOOL GetItem(int item, TCITEM *tcItem) const;
	BOOL SetItem(int item, TCITEM *tcItem);
	BOOL GetItemRect(int item, LPRECT rect) const;
	void AdjustRect(BOOL larger, LPRECT rect);
	CImageList *SetImageList(CImageList *list);
};
class CToolTipCtrl : public CWnd
{
public:
	BOOL AddTool(CWnd *wnd, LPCTSTR text = nullptr, LPCRECT rect = nullptr, UINT_PTR id = 0);
	BOOL AddTool(CWnd *wnd, UINT textID, LPCRECT rect = nullptr, UINT_PTR id = 0);
	void DelTool(CWnd *wnd, UINT_PTR id = 0);
	void Activate(BOOL activate);
	void RelayEvent(LPMSG msg);
	BOOL Create(CWnd *parent, DWORD style = 0);
	BOOL CreateEx(CWnd *parent, DWORD style = 0, DWORD exStyle = 0);
	void UpdateTipText(LPCTSTR text, CWnd *wnd, UINT_PTR id = 0);
	void SetToolRect(CWnd *wnd, UINT_PTR id, LPCRECT rect);
	int SetMaxTipWidth(int width);
	void SetDelayTime(UINT delay);
	void SetDelayTime(DWORD duration, int time);
	void Pop();
	void Update();
	BOOL SetTitle(UINT icon, LPCTSTR title);
	int GetToolCount() const;
};
class CToolBarCtrl : public CWnd
{
public:
	void SetButtonStructSize(int size);
	BOOL Create(DWORD style, const RECT &rect, CWnd *parent, UINT id);
	BOOL AddButtons(int count, LPCTBBUTTON buttons);
	BOOL InsertButton(int index, LPCTBBUTTON button);
	BOOL DeleteButton(int index);
	int GetButtonCount() const;
	BOOL GetButton(int index, LPTBBUTTON button) const;
	int CommandToIndex(UINT id) const;
	BOOL GetButtonInfo(int id, TBBUTTONINFO *info) const;
	BOOL SetButtonInfo(int id, TBBUTTONINFO *info);
	BOOL CheckButton(int id, BOOL check = TRUE);
	BOOL EnableButton(int id, BOOL enable = TRUE);
	BOOL PressButton(int id, BOOL press = TRUE);
	BOOL HideButton(int id, BOOL hide = TRUE);
	BOOL IsButtonChecked(int id) const;
	BOOL IsButtonEnabled(int id) const;
	BOOL IsButtonPressed(int id) const;
	BOOL SetState(int id, UINT state);
	int GetState(int id) const;
	BOOL GetItemRect(int index, LPRECT rect) const;
	BOOL GetRect(int id, LPRECT rect) const;
	BOOL SetBitmapSize(CSize size);
	BOOL SetButtonSize(CSize size);
	CSize GetButtonSize() const;
	BOOL SetIndent(int indent);
	DWORD GetExtendedStyle() const;
	DWORD SetExtendedStyle(DWORD style);
	CImageList *SetImageList(CImageList *list);
	CImageList *SetDisabledImageList(CImageList *list);
	BOOL ChangeBitmap(int id, int bitmap);
	void AutoSize();
	int HitTest(LPPOINT pt) const;
	int GetHotItem() const;
	class CToolTipCtrl *GetToolTips() const;
};
class CReBarCtrl : public CWnd { };
class CHeaderCtrl : public CWnd
{
public:
	BOOL Create(DWORD style, const RECT &rect, CWnd *parent, UINT id);
	int GetItemCount() const;
	BOOL GetItemRect(int index, LPRECT rect) const;
	BOOL GetItem(int index, HDITEM *item) const;
	BOOL SetItem(int index, HDITEM *item);
	int InsertItem(int index, HDITEM *item);
	BOOL DeleteItem(int index);
	BOOL GetOrderArray(LPINT array, int count = -1) const;
	BOOL SetOrderArray(int count, LPINT array);
	int OrderToIndex(int order) const;
	int HitTest(LPHDHITTESTINFO info);
	CImageList *SetImageList(CImageList *list);
	BOOL Layout(HDLAYOUT *layout);
};
class CStatusBarCtrl : public CWnd
{
public:
	BOOL SetParts(int parts, int *widths);
	BOOL SetText(LPCTSTR text, int pane, int type);
	BOOL SetMinHeight(int height);
};
// Control bar styles (MFC).
#define CBRS_BORDER_LEFT 0x0100L
#define CBRS_BORDER_TOP 0x0200L
#define CBRS_BORDER_RIGHT 0x0400L
#define CBRS_BORDER_BOTTOM 0x0800L
#define CBRS_BORDER_ANY 0x0F00L
#define CBRS_BORDER_3D 0x00800000L
#define CBRS_TOOLTIPS 0x0010L
#define CBRS_FLYBY 0x0020L
#define CBRS_FLOAT_MULTI 0x0040L
#define CBRS_HIDE_INPLACE 0x0008L
#define CBRS_SIZE_DYNAMIC 0x0004L
#define CBRS_SIZE_FIXED 0x0002L
#define CBRS_FLOATING 0x0001L
#define CBRS_GRIPPER 0x00400000L
#define CBRS_ORIENT_HORZ (CBRS_ALIGN_TOP | CBRS_ALIGN_BOTTOM)
#define CBRS_ORIENT_VERT (CBRS_ALIGN_LEFT | CBRS_ALIGN_RIGHT)
#define CBRS_ORIENT_ANY (CBRS_ORIENT_HORZ | CBRS_ORIENT_VERT)
#define CBRS_ALL 0x0040FFFFL
#define CBRS_NOALIGN 0x00000000L
#define CBRS_LEFT (CBRS_ALIGN_LEFT | CBRS_BORDER_RIGHT)
#define CBRS_TOP (CBRS_ALIGN_TOP | CBRS_BORDER_BOTTOM)
#define CBRS_RIGHT (CBRS_ALIGN_RIGHT | CBRS_BORDER_LEFT)
#define CBRS_BOTTOM (CBRS_ALIGN_BOTTOM | CBRS_BORDER_TOP)
#define CCS_TOP 0x00000001L
#define CCS_NOMOVEY 0x00000002L
#define CCS_BOTTOM 0x00000003L
#define CCS_NORESIZE 0x00000004L
#define CCS_NOPARENTALIGN 0x00000008L
#define CCS_ADJUSTABLE 0x00000020L
#define CCS_NODIVIDER 0x00000040L
#define CCS_VERT 0x00000080L
// CalcDynamicLayout modes.
#define LM_STRETCH 0x0001
#define LM_HORZ 0x0002
#define LM_MRUWIDTH 0x0004
#define LM_HORZDOCK 0x0008
#define LM_VERTDOCK 0x0010
#define LM_LENGTHY 0x0020
#define LM_COMMIT 0x0040
// Splitter cursors.
// MFC's standard cursors (afxres.h values).
#define AFX_IDC_CONTEXTHELP 30977
#define AFX_IDC_MAGNIFY 30978
#define AFX_IDC_SMALLARROWS 30979
#define AFX_IDC_HSPLITBAR 30980
#define AFX_IDC_VSPLITBAR 30981
#define AFX_IDC_NODROPCRSR 30982
class CControlBar : public CWnd
{
	LINOPENMPT_MFC_DYNAMIC(CControlBar)
protected:
	CSize m_sizeDefault;
	int m_cxLeftBorder = 0, m_cxRightBorder = 0, m_cyTopBorder = 0, m_cyBottomBorder = 0;
	DWORD m_dwStyle = 0;
public:
	DWORD GetBarStyle();
	void SetBarStyle(DWORD style);
	BOOL IsFloating() const;
	BOOL IsVisible() const;
	void EnableDocking(DWORD dockStyle);
	CFrameWnd *GetDockingFrame() const;
	int GetCount() const;
	BOOL m_bAutoDelete = FALSE;
	virtual CSize CalcFixedLayout(BOOL stretch, BOOL horz) { return {}; }
	virtual CSize CalcDynamicLayout(int length, DWORD mode) { return {}; }
	virtual void DelayShow(BOOL show) { }
};
class CToolBar : public CControlBar
{
public:
	BOOL Create(CWnd *parent, DWORD style = WS_CHILD | WS_VISIBLE | CBRS_TOP, UINT id = AFX_IDW_TOOLBAR);
	BOOL CreateEx(CWnd *parent, DWORD ctrlStyle = TBSTYLE_FLAT, DWORD style = WS_CHILD | WS_VISIBLE | CBRS_ALIGN_TOP, CRect borders = CRect(0, 0, 0, 0), UINT id = AFX_IDW_TOOLBAR);
	BOOL LoadToolBar(UINT id);
	BOOL LoadBitmap(UINT id);
	BOOL SetButtons(const UINT *ids, int count);
	void SetSizes(SIZE button, SIZE image);
	void SetHeight(int height);
	int CommandToIndex(UINT id) const;
	UINT GetItemID(int index) const;
	void GetItemRect(int index, LPRECT rect) const;
	UINT GetButtonStyle(int index) const;
	void SetButtonStyle(int index, UINT style);
	void GetButtonInfo(int index, UINT &id, UINT &style, int &image) const;
	void SetButtonInfo(int index, UINT id, UINT style, int image);
	BOOL SetButtonText(int index, LPCTSTR text);
	CString GetButtonText(int index) const;
	CToolBarCtrl &GetToolBarCtrl() const;
};
class CStatusBar : public CControlBar
{
public:
	BOOL Create(CWnd *parent, DWORD style = WS_CHILD | WS_VISIBLE | CBRS_BOTTOM, UINT id = AFX_IDW_STATUS_BAR);
	BOOL SetIndicators(const UINT *ids, int count);
	int CommandToIndex(UINT id) const;
	void GetPaneInfo(int index, UINT &id, UINT &style, int &width) const;
	void SetPaneInfo(int index, UINT id, UINT style, int width);
	BOOL SetPaneText(int index, LPCTSTR text, BOOL update = TRUE);
	CString GetPaneText(int index) const;
	void GetItemRect(int index, LPRECT rect) const;
	CStatusBarCtrl &GetStatusBarCtrl() const;
};
#define SBPS_NORMAL 0x0000
#define SBPS_NOBORDERS 0x0100
#define SBPS_POPOUT 0x0200
#define SBPS_OWNERDRAW 0x1000
#define SBPS_DISABLED 0x04000000
#define SBPS_STRETCH 0x08000000
class CDialogBar : public CControlBar
{
public:
	virtual BOOL Create(CWnd *parent, UINT templateID, UINT style, UINT id) { return FALSE; }
	virtual BOOL Create(CWnd *parent, LPCTSTR templateName, UINT style, UINT id) { return FALSE; }
protected:
	afx_msg LRESULT HandleInitDialog(WPARAM wParam, LPARAM lParam);
	LPCTSTR m_lpszTemplateName = nullptr;
};
class CReBar : public CControlBar { };
class CSplitterWnd : public CWnd
{
public:
	virtual BOOL CreateStatic(CWnd *parent, int rows, int cols, DWORD style = 0, UINT id = 0) { return FALSE; }
	virtual BOOL CreateView(int row, int col, CRuntimeClass *viewClass, SIZE initialSize, CCreateContext *context) { return FALSE; }
	virtual void DeleteView(int row, int col) { }
	virtual void RecalcLayout() { }
	CWnd *GetPane(int row, int col) const;
	void SetRowInfo(int row, int ideal, int min);
	void GetRowInfo(int row, int &cur, int &min) const;
	void SetColumnInfo(int col, int ideal, int min);
	void GetColumnInfo(int col, int &cur, int &min) const;
	int IdFromRowCol(int row, int col) const;
};

class CDialog : public CWnd
{
	LINOPENMPT_MFC_DYNAMIC(CDialog)
public:
	CDialog() = default;
	explicit CDialog(UINT id, CWnd *parent = nullptr) : m_lpszTemplateName(MAKEINTRESOURCE(id)), m_nIDHelp(id), m_pParentWnd(parent) { }
	explicit CDialog(LPCTSTR templateName, CWnd *parent = nullptr) : m_lpszTemplateName(templateName), m_pParentWnd(parent) { }
	// Modeless dialogs.
	virtual BOOL Create(LPCTSTR templateName, CWnd *parent = nullptr);
	virtual BOOL Create(UINT id, CWnd *parent = nullptr);
	virtual BOOL CreateIndirect(LPCDLGTEMPLATE dlgTemplate, CWnd *parent = nullptr, void *dialogInit = nullptr);
	// Modal dialogs.
	BOOL InitModalIndirect(LPCDLGTEMPLATE dlgTemplate, CWnd *parent = nullptr, void *dialogInit = nullptr);
	virtual INT_PTR DoModal();
	void EndDialog(int result);
	virtual BOOL OnInitDialog();
	virtual void OnSetFont(CFont *font) { }
	virtual void OnOK();
	virtual void OnCancel();
	BOOL PreTranslateMessage(MSG *msg) override;
	void GotoDlgCtrl(CWnd *ctrl);
	void NextDlgCtrl() const;
	void PrevDlgCtrl() const;
	void SetDefID(UINT id);
	DWORD GetDefID() const;
	void MapDialogRect(LPRECT rect) const;
	CMFCDynamicLayout *GetDynamicLayout();
	virtual void ResizeDynamicLayout() { }
	LPCTSTR m_lpszTemplateName = nullptr;
	UINT m_nIDHelp = 0;
	CWnd *m_pParentWnd = nullptr;
protected:
	virtual void PreInitDialog() { }
	afx_msg LRESULT HandleInitDialog(WPARAM, LPARAM);
	afx_msg LRESULT HandleSetFont(WPARAM, LPARAM);
	// The owner of a modal dialog, disabled while it runs.
	HWND PreModal();
	void PostModal();
	LPCDLGTEMPLATE m_lpDialogTemplate = nullptr;
	void *m_lpDialogInit = nullptr;
	HWND m_hWndTop = nullptr;
	DECLARE_MESSAGE_MAP()
};
class CDialogEx : public CDialog
{
public:
	using CDialog::CDialog;
};
class CPropertyPage : public CDialog
{
	LINOPENMPT_MFC_DYNAMIC(CPropertyPage)
public:
	CPropertyPage() = default;
	explicit CPropertyPage(UINT id, UINT caption = 0);
	virtual BOOL OnSetActive() { return 0; }
	virtual BOOL OnKillActive() { return 0; }
	virtual BOOL OnApply() { return 0; }
	void SetModified(BOOL changed = TRUE);
};
class CPropertySheet : public CWnd
{
public:
	CPropertySheet() = default;
	explicit CPropertySheet(UINT captionID, CWnd *parent = nullptr, UINT selectPage = 0);
	explicit CPropertySheet(LPCTSTR caption, CWnd *parent = nullptr, UINT selectPage = 0);
	virtual INT_PTR DoModal() { return IDCANCEL; }
	virtual BOOL OnInitDialog() { return TRUE; }
	void AddPage(CPropertyPage *page);
	void RemovePage(CPropertyPage *page);
	void RemovePage(int page);
	int GetPageCount() const;
	CPropertyPage *GetPage(int page) const;
	CPropertyPage *GetActivePage() const;
	int GetActiveIndex() const;
	BOOL SetActivePage(int page);
	BOOL SetActivePage(CPropertyPage *page);
	void SetTitle(LPCTSTR text, UINT style = 0);
	void EndDialog(int result);
	struct { DWORD dwFlags = 0; } m_psh;
};
// ---- common dialogs --------------------------------------------------------
#define OFN_READONLY 0x00000001
#define OFN_OVERWRITEPROMPT 0x00000002
#define OFN_HIDEREADONLY 0x00000004
#define OFN_NOCHANGEDIR 0x00000008
#define OFN_SHOWHELP 0x00000010
#define OFN_ENABLEHOOK 0x00000020
#define OFN_NOVALIDATE 0x00000100
#define OFN_ALLOWMULTISELECT 0x00000200
#define OFN_EXTENSIONDIFFERENT 0x00000400
#define OFN_PATHMUSTEXIST 0x00000800
#define OFN_FILEMUSTEXIST 0x00001000
#define OFN_CREATEPROMPT 0x00002000
#define OFN_NOREADONLYRETURN 0x00008000
#define OFN_EXPLORER 0x00080000
#define OFN_NODEREFERENCELINKS 0x00100000
#define OFN_LONGNAMES 0x00200000
#define OFN_ENABLESIZING 0x00800000
#define OFN_DONTADDTORECENT 0x02000000
#define OFN_FORCESHOWHIDDEN 0x10000000
struct OPENFILENAME
{
	DWORD lStructSize; HWND hwndOwner; HINSTANCE hInstance; LPCTSTR lpstrFilter; LPTSTR lpstrCustomFilter;
	DWORD nMaxCustFilter; DWORD nFilterIndex; LPTSTR lpstrFile; DWORD nMaxFile; LPTSTR lpstrFileTitle; DWORD nMaxFileTitle;
	LPCTSTR lpstrInitialDir; LPCTSTR lpstrTitle; DWORD Flags; WORD nFileOffset; WORD nFileExtension; LPCTSTR lpstrDefExt;
	LPARAM lCustData; void *lpfnHook; LPCTSTR lpTemplateName; void *pvReserved; DWORD dwReserved; DWORD FlagsEx;
};
using LPOPENFILENAME = OPENFILENAME *;
class CFileDialog : public CDialog
{
public:
	explicit CFileDialog(BOOL openFileDialog, LPCTSTR defExt = nullptr, LPCTSTR fileName = nullptr, DWORD flags = OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
		LPCTSTR filter = nullptr, CWnd *parent = nullptr, DWORD size = 0, BOOL vistaStyle = TRUE);
	INT_PTR DoModal() override { return IDCANCEL; }
	OPENFILENAME &GetOFN() { return m_ofn; }
	const OPENFILENAME &GetOFN() const { return m_ofn; }
	CString GetPathName() const;
	CString GetFileName() const;
	CString GetFileExt() const;
	CString GetFileTitle() const;
	CString GetFolderPath() const;
	POSITION GetStartPosition() const;
	CString GetNextPathName(POSITION &pos) const;
	BOOL GetReadOnlyPref() const;
	void SetDefExt(LPCSTR ext);
	HRESULT AddCheckButton(DWORD id, const CString &label, BOOL checked);
	HRESULT GetCheckButtonState(DWORD id, BOOL &checked);
	OPENFILENAME m_ofn{};
	BOOL m_bOpenFileDialog = TRUE;
	BOOL m_bPickFoldersMode = FALSE;
	BOOL m_bPickNonFileSysFoldersMode = FALSE;
protected:
	virtual void OnFileNameChange() { }
	virtual void OnFolderChange() { }
	virtual void OnTypeChange() { }
	virtual BOOL OnFileNameOK() { return FALSE; }
};

#define CF_SCREENFONTS 0x00000001
#define CF_PRINTERFONTS 0x00000002
#define CF_BOTH (CF_SCREENFONTS | CF_PRINTERFONTS)
#define CF_SHOWHELP 0x00000004
#define CF_ENABLEHOOK 0x00000008
#define CF_INITTOLOGFONTSTRUCT 0x00000040
#define CF_USESTYLE 0x00000080
#define CF_EFFECTS 0x00000100
#define CF_APPLY 0x00000200
#define CF_ANSIONLY 0x00000400
#define CF_SCRIPTSONLY CF_ANSIONLY
#define CF_NOVECTORFONTS 0x00000800
#define CF_NOSIMULATIONS 0x00001000
#define CF_LIMITSIZE 0x00002000
#define CF_FIXEDPITCHONLY 0x00004000
#define CF_WYSIWYG 0x00008000
#define CF_FORCEFONTEXIST 0x00010000
#define CF_SCALABLEONLY 0x00020000
#define CF_TTONLY 0x00040000
#define CF_NOFACESEL 0x00080000
#define CF_NOSTYLESEL 0x00100000
#define CF_NOSIZESEL 0x00200000
#define CF_SELECTSCRIPT 0x00400000
#define CF_NOSCRIPTSEL 0x00800000
#define CF_NOVERTFONTS 0x01000000
#define CF_INACTIVEFONTS 0x02000000
struct CHOOSEFONT
{
	DWORD lStructSize; HWND hwndOwner; HDC hDC; LOGFONT *lpLogFont; INT iPointSize; DWORD Flags; COLORREF rgbColors;
	LPARAM lCustData; void *lpfnHook; LPCTSTR lpTemplateName; HINSTANCE hInstance; LPTSTR lpszStyle; WORD nFontType;
	WORD ___MISSING_ALIGNMENT__; INT nSizeMin; INT nSizeMax;
};
class CFontDialog : public CDialog
{
public:
	explicit CFontDialog(const LOGFONT *initial = nullptr, DWORD flags = CF_EFFECTS | CF_SCREENFONTS, CDC *printerDC = nullptr, CWnd *parent = nullptr);
	INT_PTR DoModal() override { return IDCANCEL; }
	CString GetFaceName() const;
	CString GetStyleName() const;
	int GetSize() const;
	COLORREF GetColor() const;
	int GetWeight() const;
	BOOL IsStrikeOut() const;
	BOOL IsUnderline() const;
	BOOL IsBold() const;
	BOOL IsItalic() const;
	void GetCurrentFont(LOGFONT *font);
	CHOOSEFONT m_cf{};
	LOGFONT m_lf{};
};

#define CC_RGBINIT 0x00000001
#define CC_FULLOPEN 0x00000002
#define CC_PREVENTFULLOPEN 0x00000004
#define CC_SHOWHELP 0x00000008
#define CC_ENABLEHOOK 0x00000010
#define CC_SOLIDCOLOR 0x00000080
#define CC_ANYCOLOR 0x00000100
struct CHOOSECOLOR
{
	DWORD lStructSize; HWND hwndOwner; HWND hInstance; COLORREF rgbResult; COLORREF *lpCustColors; DWORD Flags;
	LPARAM lCustData; void *lpfnHook; LPCTSTR lpTemplateName;
};
using LPCHOOSECOLOR = CHOOSECOLOR *;
BOOL ChooseColor(LPCHOOSECOLOR cc);
class CColorDialog : public CDialog
{
public:
	explicit CColorDialog(COLORREF init = 0, DWORD flags = 0, CWnd *parent = nullptr);
	INT_PTR DoModal() override { return IDCANCEL; }
	COLORREF GetColor() const;
	void SetCurrentColor(COLORREF color);
	static COLORREF *GetSavedCustomColors();
	CHOOSECOLOR m_cc{};
};

class CHotKeyCtrl : public CWnd
{
public:
	void SetHotKey(WORD virtualKey, WORD modifiers);
	DWORD GetHotKey() const;
	void GetHotKey(WORD &virtualKey, WORD &modifiers) const;
	void SetRules(WORD invalidComb, WORD modifiers);
	static CString GetKeyName(UINT virtualKey, BOOL extended);
	CString GetHotKeyName() const;
};

class CFrameWnd : public CWnd
{
	LINOPENMPT_MFC_DYNAMIC(CFrameWnd)
public:
	virtual void OnUpdateFrameTitle(BOOL addToTitle) { }
	virtual void RecalcLayout(BOOL notify = TRUE) { }
	void SetMessageText(LPCTSTR text);
	virtual BOOL OnCreateClient(LPCREATESTRUCT cs, CCreateContext *context) { return TRUE; }
	void SetActiveView(class CView *view, BOOL notify = TRUE);
	void EnableDocking(DWORD dockStyle);
	void DockControlBar(CControlBar *bar, UINT dockBarID = 0, LPCRECT rect = nullptr);
	void FloatControlBar(CControlBar *bar, CPoint point, DWORD style = CBRS_ALIGN_TOP);
	void ShowControlBar(CControlBar *bar, BOOL show, BOOL delay);
	CControlBar *GetControlBar(UINT id);
	void AddControlBar(CControlBar *bar);
	void RemoveControlBar(CControlBar *bar);
	void LoadBarState(LPCTSTR profileName);
	void SaveBarState(LPCTSTR profileName) const;
	void UpdateFrameTitleForDocument(LPCTSTR docName);
	afx_msg BOOL OnBarCheck(UINT id);
	afx_msg void OnUpdateControlBarMenu(CCmdUI *cmdUI);
	BOOL m_bAutoMenuEnable = TRUE;
	virtual void ActivateFrame(int cmdShow = -1) { }
	virtual BOOL LoadFrame(UINT resourceID, DWORD style = 0, CWnd *parent = nullptr, CCreateContext *context = nullptr) { return FALSE; }
	virtual void OnUpdateFrameMenu(HMENU menuAlt) { }
	virtual class CDocument *GetActiveDocument() { return nullptr; }
	virtual class CView *GetActiveView() const { return nullptr; }
	virtual CFrameWnd *GetActiveFrame() { return nullptr; }
	// Title of the frame, and its index among frames showing one document.
	CString m_strTitle;
	int m_nWindow = -1;
	CString GetTitle() const { return m_strTitle; }
	void SetTitle(LPCTSTR title) { m_strTitle = title; }
};
// CFrameWnd styles.
#define FWS_ADDTOTITLE 0x00008000L
#define FWS_PREFIXTITLE 0x00040000L
#define FWS_SNAPTOBARS 0x00080000L
void AFXAPI AfxSetWindowText(HWND hwnd, LPCTSTR text);
class CMDIFrameWnd : public CFrameWnd
{
	LINOPENMPT_MFC_DYNAMIC(CMDIFrameWnd)
public:
	HWND m_hWndMDIClient = nullptr;
	class CMDIChildWnd *MDIGetActive(BOOL *maximized = nullptr) const;
	void MDIActivate(CWnd *child);
	void MDINext();
	void MDIPrev();
	void MDICascade();
	void MDITile(int type);
	void MDIIconArrange();
};
class CMDIChildWnd : public CFrameWnd
{
	LINOPENMPT_MFC_DYNAMIC(CMDIChildWnd)
public:
	void MDIActivate();
	void MDIDestroy();
	void MDIMaximize();
	void MDIRestore();
	CMDIFrameWnd *GetMDIFrame();
	afx_msg void OnMDIActivate(BOOL activate, CWnd *activateWnd, CWnd *deactivateWnd) { }
};
class CMDIFrameWndEx : public CMDIFrameWnd { LINOPENMPT_MFC_DYNAMIC(CMDIFrameWndEx) };
class CMDIChildWndEx : public CMDIChildWnd { LINOPENMPT_MFC_DYNAMIC(CMDIChildWndEx) };

class CView : public CWnd
{
	LINOPENMPT_MFC_DYNAMIC(CView)
public:
	class CDocument *GetDocument() const;
	virtual void OnDraw(CDC *dc) { }
	virtual void OnInitialUpdate() { }
	virtual void OnUpdate(CView *sender, LPARAM hint, CObject *hintObj) { }
	virtual void OnActivateView(BOOL activate, CView *activateView, CView *deactiveView) { }
	virtual void OnPrepareDC(CDC *dc, CPrintInfo *info = nullptr) { }
protected:
	class CDocument *m_pDocument = nullptr;
};
#define MM_TEXT 1
class CScrollView : public CView
{
	LINOPENMPT_MFC_DYNAMIC(CScrollView)
public:
	static const SIZE sizeDefault;
	virtual BOOL OnScroll(UINT scrollCode, UINT pos, BOOL doScroll = TRUE) { return FALSE; }
	virtual BOOL OnScrollBy(CSize sizeScroll, BOOL doScroll = TRUE) { return FALSE; }
	void SetScrollSizes(int mapMode, SIZE total, const SIZE &page = sizeDefault, const SIZE &line = sizeDefault);
	void ScrollToDevicePosition(POINT pt);
	BOOL DoMouseWheel(UINT flags, short delta, CPoint pt);
	CPoint GetDeviceScrollPosition() const;
protected:
	int m_nMapMode = 0;
	CSize m_totalLog, m_totalDev, m_pageDev, m_lineDev;
public:
	CPoint GetScrollPosition() const;
	void ScrollToPosition(POINT pt);
	CSize GetTotalSize() const;
	void GetDeviceScrollSizes(int &mapMode, SIZE &total, SIZE &page, SIZE &line) const;
	int GetScrollPos(int bar) const;
	int SetScrollPos(int bar, int pos, BOOL redraw = TRUE);
};
class CFormView : public CScrollView
{
	LINOPENMPT_MFC_DYNAMIC(CFormView)
protected:
	explicit CFormView(UINT idTemplate) : m_lpszTemplateName(MAKEINTRESOURCE(idTemplate)), m_nIDHelp(idTemplate) { }
	explicit CFormView(LPCTSTR templateName) : m_lpszTemplateName(templateName) { }
	LPCTSTR m_lpszTemplateName = nullptr;
	UINT m_nIDHelp = 0;
};

// ---- documents -----------------------------------------------------------
class CDocTemplate : public CCmdTarget
{
	LINOPENMPT_MFC_DYNAMIC(CDocTemplate)
public:
	enum DocStringIndex { windowTitle, docName, fileNewName, filterName, filterExt, regFileTypeId, regFileTypeName };
	CDocTemplate(UINT resourceID, CRuntimeClass *docClass, CRuntimeClass *frameClass, CRuntimeClass *viewClass)
		: m_nIDResource(resourceID), m_pDocClass(docClass), m_pFrameClass(frameClass), m_pViewClass(viewClass) { }
	virtual CDocument *OpenDocumentFile(LPCTSTR pathName, BOOL addToMRU, BOOL makeVisible) = 0;
	virtual CDocument *OpenDocumentFile(LPCTSTR pathName, BOOL makeVisible = TRUE) { return OpenDocumentFile(pathName, TRUE, makeVisible); }
	virtual void AddDocument(CDocument *doc) { }
	virtual void RemoveDocument(CDocument *doc) { }
	virtual POSITION GetFirstDocPosition() const = 0;
	virtual CDocument *GetNextDoc(POSITION &pos) const = 0;
	virtual CDocument *CreateNewDocument() { return nullptr; }
	virtual CFrameWnd *CreateNewFrame(CDocument *doc, CFrameWnd *other) { return nullptr; }
	virtual void InitialUpdateFrame(CFrameWnd *frame, CDocument *doc, BOOL makeVisible = TRUE) { }
	virtual BOOL SaveAllModified() { return TRUE; }
	virtual void CloseAllDocuments(BOOL endSession) { }
	virtual BOOL GetDocString(CString &string, DocStringIndex index) const { return FALSE; }
	virtual void SetDefaultTitle(CDocument *doc) { }
protected:
	UINT m_nUntitledCount = 0;
	UINT m_nIDResource;
	CRuntimeClass *m_pDocClass;
	CRuntimeClass *m_pFrameClass;
	CRuntimeClass *m_pViewClass;
};

class CMultiDocTemplate : public CDocTemplate
{
	LINOPENMPT_MFC_DYNAMIC(CMultiDocTemplate)
public:
	CMultiDocTemplate(UINT resourceID, CRuntimeClass *docClass, CRuntimeClass *frameClass, CRuntimeClass *viewClass)
		: CDocTemplate(resourceID, docClass, frameClass, viewClass) { }
	CDocument *OpenDocumentFile(LPCTSTR pathName, BOOL addToMRU, BOOL makeVisible) override { return nullptr; }
	using CDocTemplate::OpenDocumentFile;
	// Not used by upstream, which keeps its own document set in CModDocTemplate.
	POSITION GetFirstDocPosition() const override { return nullptr; }
	CDocument *GetNextDoc(POSITION &pos) const override { pos = nullptr; return nullptr; }
	HMENU m_hMenuShared = nullptr;
	HACCEL m_hAccelTable = nullptr;
};

class CDocManager : public CObject
{
public:
	virtual ~CDocManager() = default;
	virtual void AddDocTemplate(CDocTemplate *tmpl) { }
	virtual POSITION GetFirstDocTemplatePosition() const { return nullptr; }
	virtual CDocTemplate *GetNextDocTemplate(POSITION &pos) const { pos = nullptr; return nullptr; }
	virtual CDocument *OpenDocumentFile(LPCTSTR fileName) { return OpenDocumentFile(fileName, TRUE); }
	virtual CDocument *OpenDocumentFile(LPCTSTR fileName, BOOL addToMRU) { return nullptr; }
	virtual BOOL SaveAllModified() { return TRUE; }
	virtual void CloseAllDocuments(BOOL endSession) { }
	virtual int GetOpenDocumentCount() { return 0; }
	virtual BOOL OnDDECommand(LPTSTR command) { return FALSE; }
	virtual BOOL DoPromptFileName(CString &fileName, UINT titleID, DWORD flags, BOOL openFileDialog, CDocTemplate *tmpl) { return FALSE; }
};

class CDocument : public CCmdTarget
{
	LINOPENMPT_MFC_DYNAMIC(CDocument)
protected:
	CString m_strTitle;
	CString m_strPathName;
	// BOOL in MFC. Upstream updates it with InterlockedExchange through a
	// long *, and asserts sizeof(long) == sizeof(m_bModified). long is
	// 8 bytes on Linux, so the member is a long here.
	long m_bModified = FALSE;
	BOOL m_bAutoDelete = TRUE;
	BOOL m_bEmbedded = FALSE;
public:
	CDocument() = default;
	virtual ~CDocument() = default;
	const CString &GetTitle() const { return m_strTitle; }
	virtual void SetTitle(LPCTSTR title) { m_strTitle = title; }
	const CString &GetPathName() const { return m_strPathName; }
	virtual void SetPathName(LPCTSTR path, BOOL addToMRU = TRUE) { (void)addToMRU; m_strPathName = path; }
	virtual BOOL IsModified() { return m_bModified; }
	virtual void SetModifiedFlag(BOOL modified = TRUE) { m_bModified = modified; }
	virtual BOOL OnNewDocument() { m_bModified = FALSE; m_strPathName.Empty(); return TRUE; }
	virtual BOOL OnOpenDocument(LPCTSTR path) { (void)path; return TRUE; }
	virtual BOOL OnSaveDocument(LPCTSTR path) { (void)path; m_bModified = FALSE; return TRUE; }
	virtual void OnCloseDocument() { }
	virtual BOOL SaveModified() { return TRUE; }
	virtual BOOL DoSave(LPCTSTR path, BOOL replace = TRUE) { return 0; }
	virtual BOOL DoFileSave() { return 0; }
	virtual void DeleteContents() { }
	virtual BOOL CanCloseFrame(CFrameWnd *frame) { return TRUE; }
	virtual void PreCloseFrame(CFrameWnd *frame) { }
	virtual void OnChangedViewList() { }
	virtual void SetDocumentTitle() { }
	afx_msg void OnFileClose();
	afx_msg void OnFileSave();
	afx_msg void OnFileSaveAs();
	void UpdateAllViews(CView *sender, LPARAM hint = 0, CObject *hintObj = nullptr);
	virtual void UpdateFrameCounts() { }
	CDocTemplate *GetDocTemplate() const;
	POSITION GetFirstViewPosition() const;
	CView *GetNextView(POSITION &pos) const;
};

// Defined in <afxdatarecovery.h>.
class CDataRecoveryHandler;

// ---- application ---------------------------------------------------------
#define AFX_RESTART_MANAGER_SUPPORT_RESTART 0x01
#define AFX_RESTART_MANAGER_REOPEN_PREVIOUS_FILES 0x08
#define AFX_RESTART_MANAGER_SUPPORT_NO_AUTOSAVE 0x02
#define AFX_RESTART_MANAGER_SUPPORT_AUTOSAVE_AT_RESTART 0x04
#define AFX_RESTART_MANAGER_SUPPORT_AUTOSAVE_AT_INTERVAL 0x10
#define AFX_RESTART_MANAGER_SUPPORT_RECOVERY 0x20

class CWinThread : public CCmdTarget
{
	LINOPENMPT_MFC_DYNAMIC(CWinThread)
public:
	CWnd *m_pMainWnd = nullptr;
	MSG m_msgCur{};
	virtual CWnd *GetMainWnd() { return m_pMainWnd; }
	virtual BOOL InitInstance() { return 0; }
	virtual int ExitInstance() { return static_cast<int>(m_msgCur.wParam); }
	// The message loop and its idle processing (platform/mfc).
	virtual int Run();
	virtual BOOL OnIdle(LONG count);
	virtual BOOL PreTranslateMessage(MSG *msg);
	virtual LRESULT ProcessWndProcException(CException *e, const MSG *msg);
};

// Command line as MFC parses it: ParseParam() is called for each argument,
// with flags (-x or /x) passed without their prefix.
class CCommandLineInfo : public CObject
{
public:
	enum
	{
		FileNew,
		FileOpen,
		FilePrint,
		FilePrintTo,
		FileDDE,
		AppRegister,
		AppUnregister,
		RestartByRestartManager,
		FileNothing = -1
	} m_nShellCommand = FileNew;
	BOOL m_bShowSplash = TRUE;
	BOOL m_bRunEmbedded = FALSE;
	BOOL m_bRunAutomated = FALSE;
	CString m_strFileName;
	CString m_strPrinterName;
	CString m_strDriverName;
	CString m_strPortName;
	CString m_strRestartIdentifier;
	virtual ~CCommandLineInfo() = default;
	virtual void ParseParam(LPCTSTR param, BOOL flag, BOOL last)
	{
		if(!flag && m_strFileName.IsEmpty())
			m_strFileName = param;
	}
};

class CWinApp : public CWinThread
{
	LINOPENMPT_MFC_DYNAMIC(CWinApp)
public:
	// Registers the application object (AfxGetApp), as in MFC.
	explicit CWinApp(LPCTSTR appName = nullptr);
	~CWinApp() override;
	BOOL OnIdle(LONG count) override;
	LPCTSTR m_pszAppName = nullptr;
	DWORD m_dwRestartManagerSupportFlags = 0;
	int m_nAutosaveInterval = 5 * 60 * 1000;
	CDataRecoveryHandler *m_pDataRecoveryHandler = nullptr;
	BOOL SupportsRestartManager() const { return (m_dwRestartManagerSupportFlags & AFX_RESTART_MANAGER_SUPPORT_RESTART) != 0; }
	BOOL SupportsApplicationRecovery() const { return (m_dwRestartManagerSupportFlags & AFX_RESTART_MANAGER_SUPPORT_RECOVERY) != 0; }
	LPCTSTR m_pszExeName = nullptr;
	HINSTANCE m_hInstance = nullptr;
	virtual CDocument *OpenDocumentFile(LPCTSTR path) { return nullptr; }
	virtual CDocument *OpenDocumentFile(LPCTSTR path, BOOL addToMRU) { return nullptr; }
	virtual void AddToRecentFileList(LPCTSTR path) { }
	virtual CDataRecoveryHandler *GetDataRecoveryHandler() { return nullptr; }
	virtual BOOL SaveAllModified() { return 0; }
	virtual void WinHelp(DWORD_PTR data, UINT cmd = 0) { }
	void AddDocTemplate(CDocTemplate *tmpl);
	HCURSOR LoadCursor(UINT id) const;
	HCURSOR LoadCursor(LPCTSTR name) const;
	HCURSOR LoadStandardCursor(LPCTSTR name) const;
	HICON LoadIcon(UINT id) const;
	HICON LoadIcon(LPCTSTR name) const;
	HICON LoadStandardIcon(LPCTSTR name) const;
	CDocManager *m_pDocManager = nullptr;
	CString m_strRecentFiles;
	LPCTSTR m_pszProfileName = nullptr;
	LPCTSTR m_pszRegistryKey = nullptr;
	LPCTSTR m_pszHelpFilePath = nullptr;
	int m_nCmdShow = SW_SHOWNORMAL;
	void ParseCommandLine(CCommandLineInfo &info);
	BOOL ProcessShellCommand(CCommandLineInfo &info);
	void EnableShellOpen();
	void LoadStdProfileSettings(UINT maxMRU = 4);
	void SetRegistryKey(LPCTSTR key);
};


// ---- run-time classes of the classes above ---------------------------------
LINOPENMPT_MFC_IMPLEMENT_DYNAMIC(CCmdTarget, CObject)
LINOPENMPT_MFC_IMPLEMENT_DYNAMIC(CWnd, CCmdTarget)
LINOPENMPT_MFC_IMPLEMENT_DYNAMIC(CControlBar, CWnd)
LINOPENMPT_MFC_IMPLEMENT_DYNAMIC(CDialog, CWnd)
LINOPENMPT_MFC_IMPLEMENT_DYNAMIC(CPropertyPage, CDialog)
LINOPENMPT_MFC_IMPLEMENT_DYNAMIC(CFrameWnd, CWnd)
LINOPENMPT_MFC_IMPLEMENT_DYNAMIC(CMDIFrameWnd, CFrameWnd)
LINOPENMPT_MFC_IMPLEMENT_DYNAMIC(CMDIChildWnd, CFrameWnd)
LINOPENMPT_MFC_IMPLEMENT_DYNAMIC(CView, CWnd)
LINOPENMPT_MFC_IMPLEMENT_DYNAMIC(CScrollView, CView)
LINOPENMPT_MFC_IMPLEMENT_DYNAMIC(CDocTemplate, CCmdTarget)
LINOPENMPT_MFC_IMPLEMENT_DYNAMIC(CMultiDocTemplate, CDocTemplate)
LINOPENMPT_MFC_IMPLEMENT_DYNAMIC(CDocument, CCmdTarget)
LINOPENMPT_MFC_IMPLEMENT_DYNAMIC(CWinThread, CCmdTarget)
LINOPENMPT_MFC_IMPLEMENT_DYNAMIC(CWinApp, CWinThread)
LINOPENMPT_MFC_IMPLEMENT_DYNAMIC(CFormView, CScrollView)
LINOPENMPT_MFC_IMPLEMENT_DYNAMIC(CMDIFrameWndEx, CMDIFrameWnd)
LINOPENMPT_MFC_IMPLEMENT_DYNAMIC(CMDIChildWndEx, CMDIChildWnd)

CWinApp *AfxGetApp();
CWinThread *AfxGetThread();
LPCTSTR AfxRegisterWndClass(UINT classStyle, HCURSOR cursor = nullptr, HBRUSH background = nullptr, HICON icon = nullptr);
HINSTANCE AfxGetResourceHandle();
HINSTANCE AfxGetInstanceHandle();
void AfxSetResourceHandle(HINSTANCE instance);
CWnd *AfxGetMainWnd();
int AfxMessageBox(LPCTSTR text, UINT type = MB_OK, UINT help = 0);

// ---- dialog data exchange ------------------------------------------------
void DDX_Control(CDataExchange *dx, int id, CWnd &control);
void DDX_Text(CDataExchange *dx, int id, CString &value);
void DDX_Text(CDataExchange *dx, int id, int &value);
void DDX_Text(CDataExchange *dx, int id, UINT &value);
void DDX_Text(CDataExchange *dx, int id, long &value);
void DDX_Text(CDataExchange *dx, int id, DWORD &value);
void DDX_Text(CDataExchange *dx, int id, float &value);
void DDX_Text(CDataExchange *dx, int id, double &value);
void DDX_Check(CDataExchange *dx, int id, int &value);
void DDX_Radio(CDataExchange *dx, int id, int &value);
void DDX_CBIndex(CDataExchange *dx, int id, int &index);
void DDX_CBString(CDataExchange *dx, int id, CString &value);
void DDX_LBIndex(CDataExchange *dx, int id, int &index);
void DDX_Slider(CDataExchange *dx, int id, int &value);
void DDV_MaxChars(CDataExchange *dx, const CString &value, int maxChars);
void DDV_MinMaxInt(CDataExchange *dx, int value, int minVal, int maxVal);
void DDV_MinMaxUInt(CDataExchange *dx, UINT value, UINT minVal, UINT maxVal);
