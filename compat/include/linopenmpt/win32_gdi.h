// linOpenMPT compatibility layer: Win32 GDI (device contexts, pens, brushes,
// fonts, bitmaps, drawing), with Windows' values.
//
// Declared only; the UI layer implements them on Qt (QPainter, QImage) in
// plan step 3.

#pragma once

#include "win32_api.h"

// ---- stock objects and background modes ------------------------------------
#define WHITE_BRUSH 0
#define LTGRAY_BRUSH 1
#define GRAY_BRUSH 2
#define DKGRAY_BRUSH 3
#define BLACK_BRUSH 4
#define NULL_BRUSH 5
#define HOLLOW_BRUSH NULL_BRUSH
#define WHITE_PEN 6
#define BLACK_PEN 7
#define NULL_PEN 8
#define OEM_FIXED_FONT 10
#define ANSI_FIXED_FONT 11
#define ANSI_VAR_FONT 12
#define SYSTEM_FONT 13
#define DEVICE_DEFAULT_FONT 14
#define DEFAULT_PALETTE 15
#define SYSTEM_FIXED_FONT 16
#define DEFAULT_GUI_FONT 17
#define DC_BRUSH 18
#define DC_PEN 19
#define TRANSPARENT 1
#define OPAQUE 2

// ---- pens ------------------------------------------------------------------
#define PS_SOLID 0
#define PS_DASH 1
#define PS_DOT 2
#define PS_DASHDOT 3
#define PS_DASHDOTDOT 4
#define PS_NULL 5
#define PS_INSIDEFRAME 6
#define PS_ENDCAP_FLAT 0x00000200
#define PS_GEOMETRIC 0x00010000
struct LOGPEN { UINT lopnStyle; POINT lopnWidth; COLORREF lopnColor; };
struct LOGBRUSH { UINT lbStyle; COLORREF lbColor; ULONG_PTR lbHatch; };
#define BS_SOLID 0
#define BS_NULL 1
#define BS_HOLLOW BS_NULL
#define BS_HATCHED 2
#define BS_PATTERN 3

// ---- raster operations ------------------------------------------------------
#define SRCCOPY 0x00CC0020
#define SRCPAINT 0x00EE0086
#define SRCAND 0x008800C6
#define SRCINVERT 0x00660046
#define SRCERASE 0x00440328
#define NOTSRCCOPY 0x00330008
#define MERGECOPY 0x00C000CA
#define PATCOPY 0x00F00021
#define PATINVERT 0x005A0049
#define DSTINVERT 0x00550009
#define BLACKNESS 0x00000042
#define WHITENESS 0x00FF0062
#define R2_BLACK 1
#define R2_NOT 6
#define R2_XORPEN 7
#define R2_NOTXORPEN 10
#define R2_COPYPEN 13
#define R2_WHITE 16

// ---- text -----------------------------------------------------------------
#define TA_NOUPDATECP 0
#define TA_UPDATECP 1
#define TA_LEFT 0
#define TA_RIGHT 2
#define TA_CENTER 6
#define TA_TOP 0
#define TA_BOTTOM 8
#define TA_BASELINE 24
#define DT_WORDBREAK 0x0010
#define DT_EXPANDTABS 0x0040
#define DT_TABSTOP 0x0080
#define DT_NOCLIP 0x0100
#define DT_EXTERNALLEADING 0x0200
#define DT_CALCRECT 0x0400
#define DT_INTERNAL 0x1000
#define DT_EDITCONTROL 0x2000
#define DT_PATH_ELLIPSIS 0x4000
#define DT_END_ELLIPSIS 0x8000
#define DT_MODIFYSTRING 0x10000
#define DT_RTLREADING 0x20000
#define DT_WORD_ELLIPSIS 0x40000
#define DT_HIDEPREFIX 0x00100000
struct TEXTMETRICW
{
	LONG tmHeight; LONG tmAscent; LONG tmDescent; LONG tmInternalLeading; LONG tmExternalLeading;
	LONG tmAveCharWidth; LONG tmMaxCharWidth; LONG tmWeight; LONG tmOverhang;
	LONG tmDigitizedAspectX; LONG tmDigitizedAspectY;
	WCHAR tmFirstChar; WCHAR tmLastChar; WCHAR tmDefaultChar; WCHAR tmBreakChar;
	BYTE tmItalic; BYTE tmUnderlined; BYTE tmStruckOut; BYTE tmPitchAndFamily; BYTE tmCharSet;
};
using TEXTMETRIC = TEXTMETRICW;
using LPTEXTMETRIC = TEXTMETRIC *;

// ---- fonts ------------------------------------------------------------------
#define FW_DONTCARE 0
#define FW_THIN 100
#define FW_EXTRALIGHT 200
#define FW_LIGHT 300
#define FW_NORMAL 400
#define FW_REGULAR 400
#define FW_MEDIUM 500
#define FW_SEMIBOLD 600
#define FW_BOLD 700
#define FW_EXTRABOLD 800
#define FW_HEAVY 900
#define ANSI_CHARSET 0
#define DEFAULT_CHARSET 1
#define SYMBOL_CHARSET 2
#define OEM_CHARSET 255
#define OUT_DEFAULT_PRECIS 0
#define OUT_STRING_PRECIS 1
#define OUT_CHARACTER_PRECIS 2
#define OUT_STROKE_PRECIS 3
#define OUT_TT_PRECIS 4
#define OUT_DEVICE_PRECIS 5
#define OUT_RASTER_PRECIS 6
#define OUT_TT_ONLY_PRECIS 7
#define OUT_OUTLINE_PRECIS 8
#define CLIP_DEFAULT_PRECIS 0
#define CLIP_CHARACTER_PRECIS 1
#define CLIP_STROKE_PRECIS 2
#define DEFAULT_QUALITY 0
#define DRAFT_QUALITY 1
#define PROOF_QUALITY 2
#define NONANTIALIASED_QUALITY 3
#define ANTIALIASED_QUALITY 4
#define CLEARTYPE_QUALITY 5
#define DEFAULT_PITCH 0
#define FIXED_PITCH 1
#define VARIABLE_PITCH 2
#define FF_DONTCARE (0 << 4)
#define FF_ROMAN (1 << 4)
#define FF_SWISS (2 << 4)
#define FF_MODERN (3 << 4)
#define FF_SCRIPT (4 << 4)
#define FF_DECORATIVE (5 << 4)

// ---- bitmaps ---------------------------------------------------------------
#define BI_RGB 0
#define BI_RLE8 1
#define BI_RLE4 2
#define BI_BITFIELDS 3
#define DIB_RGB_COLORS 0
#define DIB_PAL_COLORS 1
struct BITMAP { LONG bmType; LONG bmWidth; LONG bmHeight; LONG bmWidthBytes; WORD bmPlanes; WORD bmBitsPixel; LPVOID bmBits; };
using LPBITMAPINFO = BITMAPINFO *;
struct BLENDFUNCTION { BYTE BlendOp; BYTE BlendFlags; BYTE SourceConstantAlpha; BYTE AlphaFormat; };
#define AC_SRC_OVER 0x00
#define AC_SRC_ALPHA 0x01

// ---- device capabilities -------------------------------------------------
#define HORZRES 8
#define VERTRES 10
#define BITSPIXEL 12
#define PLANES 14
#define LOGPIXELSX 88
#define LOGPIXELSY 90

// ---- 3D edges (DrawEdge) ---------------------------------------------------
#define BDR_RAISEDOUTER 0x0001
#define BDR_SUNKENOUTER 0x0002
#define BDR_RAISEDINNER 0x0004
#define BDR_SUNKENINNER 0x0008
#define BDR_OUTER (BDR_RAISEDOUTER | BDR_SUNKENOUTER)
#define BDR_INNER (BDR_RAISEDINNER | BDR_SUNKENINNER)
#define EDGE_RAISED (BDR_RAISEDOUTER | BDR_RAISEDINNER)
#define EDGE_SUNKEN (BDR_SUNKENOUTER | BDR_SUNKENINNER)
#define EDGE_ETCHED (BDR_SUNKENOUTER | BDR_RAISEDINNER)
#define EDGE_BUMP (BDR_RAISEDOUTER | BDR_SUNKENINNER)
#define BF_LEFT 0x0001
#define BF_TOP 0x0002
#define BF_RIGHT 0x0004
#define BF_BOTTOM 0x0008
#define BF_TOPLEFT (BF_TOP | BF_LEFT)
#define BF_BOTTOMRIGHT (BF_BOTTOM | BF_RIGHT)
#define BF_RECT (BF_LEFT | BF_TOP | BF_RIGHT | BF_BOTTOM)
#define BF_DIAGONAL 0x0010
#define BF_MIDDLE 0x0800
#define BF_SOFT 0x1000
#define BF_ADJUST 0x2000
#define BF_FLAT 0x4000
#define BF_MONO 0x8000
#define DFC_CAPTION 1
#define DFC_MENU 2
#define DFC_SCROLL 3
#define DFC_BUTTON 4
#define DFCS_BUTTONCHECK 0x0000
#define DFCS_BUTTONRADIO 0x0004
#define DFCS_BUTTONPUSH 0x0010
#define DFCS_INACTIVE 0x0100
#define DFCS_PUSHED 0x0200
#define DFCS_CHECKED 0x0400
#define DFCS_FLAT 0x4000

// ---- regions --------------------------------------------------------------
#define RGN_AND 1
#define RGN_OR 2
#define RGN_XOR 3
#define RGN_DIFF 4
#define RGN_COPY 5
#define NULLREGION 1
#define SIMPLEREGION 2
#define COMPLEXREGION 3

struct PAINTSTRUCT { HDC hdc; BOOL fErase; RECT rcPaint; BOOL fRestore; BOOL fIncUpdate; BYTE rgbReserved[32]; };
using LPPAINTSTRUCT = PAINTSTRUCT *;

// ---- functions -----------------------------------------------------------
HDC GetDC(HWND hwnd);
HDC GetWindowDC(HWND hwnd);
int ReleaseDC(HWND hwnd, HDC dc);
HDC BeginPaint(HWND hwnd, LPPAINTSTRUCT ps);
BOOL EndPaint(HWND hwnd, const PAINTSTRUCT *ps);
HDC CreateCompatibleDC(HDC dc);
BOOL DeleteDC(HDC dc);
int SaveDC(HDC dc);
BOOL RestoreDC(HDC dc, int saved);
int GetDeviceCaps(HDC dc, int index);
HGDIOBJ GetStockObject(int index);
HGDIOBJ SelectObject(HDC dc, HGDIOBJ object);
BOOL DeleteObject(HGDIOBJ object);
int GetObject(HANDLE object, int size, LPVOID buffer);
HPEN CreatePen(int style, int width, COLORREF color);
HBRUSH CreateSolidBrush(COLORREF color);
HBRUSH CreatePatternBrush(HBITMAP bitmap);
HFONT CreateFontIndirect(const LOGFONT *font);
HFONT CreateFont(int height, int width, int escapement, int orientation, int weight, DWORD italic, DWORD underline, DWORD strikeOut,
	DWORD charSet, DWORD outPrecision, DWORD clipPrecision, DWORD quality, DWORD pitchAndFamily, LPCTSTR faceName);
HBITMAP CreateCompatibleBitmap(HDC dc, int cx, int cy);
HBITMAP CreateBitmap(int w, int h, UINT planes, UINT bitCount, const void *bits);
HBITMAP CreateDIBSection(HDC dc, const BITMAPINFO *bmi, UINT usage, void **bits, HANDLE section, DWORD offset);
int StretchDIBits(HDC dc, int xDest, int yDest, int wDest, int hDest, int xSrc, int ySrc, int wSrc, int hSrc,
	const void *bits, const BITMAPINFO *bmi, UINT usage, DWORD rop);
int SetDIBits(HDC dc, HBITMAP bitmap, UINT start, UINT lines, const void *bits, const BITMAPINFO *bmi, UINT usage);
int GetDIBits(HDC dc, HBITMAP bitmap, UINT start, UINT lines, LPVOID bits, BITMAPINFO *bmi, UINT usage);
int SetDIBitsToDevice(HDC dc, int xDest, int yDest, DWORD w, DWORD h, int xSrc, int ySrc, UINT startScan, UINT lines,
	const void *bits, const BITMAPINFO *bmi, UINT usage);
BOOL BitBlt(HDC dest, int x, int y, int cx, int cy, HDC src, int x1, int y1, DWORD rop);
BOOL StretchBlt(HDC dest, int x, int y, int w, int h, HDC src, int xSrc, int ySrc, int wSrc, int hSrc, DWORD rop);
BOOL PatBlt(HDC dc, int x, int y, int w, int h, DWORD rop);
BOOL TransparentBlt(HDC dest, int x, int y, int w, int h, HDC src, int xSrc, int ySrc, int wSrc, int hSrc, UINT transparent);
BOOL AlphaBlend(HDC dest, int x, int y, int w, int h, HDC src, int xSrc, int ySrc, int wSrc, int hSrc, BLENDFUNCTION blend);
BOOL GdiFlush();
COLORREF SetTextColor(HDC dc, COLORREF color);
COLORREF GetTextColor(HDC dc);
COLORREF SetBkColor(HDC dc, COLORREF color);
COLORREF GetBkColor(HDC dc);
int SetBkMode(HDC dc, int mode);
UINT SetTextAlign(HDC dc, UINT align);
int SetROP2(HDC dc, int mode);
COLORREF SetDCBrushColor(HDC dc, COLORREF color);
COLORREF SetDCPenColor(HDC dc, COLORREF color);
COLORREF SetPixel(HDC dc, int x, int y, COLORREF color);
COLORREF GetPixel(HDC dc, int x, int y);
BOOL MoveToEx(HDC dc, int x, int y, LPPOINT old);
BOOL LineTo(HDC dc, int x, int y);
BOOL Polyline(HDC dc, const POINT *points, int count);
BOOL Polygon(HDC dc, const POINT *points, int count);
BOOL Rectangle(HDC dc, int left, int top, int right, int bottom);
BOOL Ellipse(HDC dc, int left, int top, int right, int bottom);
int FillRect(HDC dc, const RECT *rect, HBRUSH brush);
int FrameRect(HDC dc, const RECT *rect, HBRUSH brush);
BOOL InvertRect(HDC dc, const RECT *rect);
BOOL DrawFocusRect(HDC dc, const RECT *rect);
BOOL DrawEdge(HDC dc, LPRECT rect, UINT edge, UINT flags);
BOOL DrawFrameControl(HDC dc, LPRECT rect, UINT type, UINT state);
BOOL DrawIconEx(HDC dc, int x, int y, HICON icon, int cx, int cy, UINT step, HBRUSH flicker, UINT flags);
BOOL DrawIcon(HDC dc, int x, int y, HICON icon);
int DrawText(HDC dc, LPCTSTR text, int count, LPRECT rect, UINT format);
BOOL TextOut(HDC dc, int x, int y, LPCTSTR text, int count);
BOOL ExtTextOut(HDC dc, int x, int y, UINT options, const RECT *rect, LPCTSTR text, UINT count, const INT *dx);
BOOL GetTextExtentPoint32(HDC dc, LPCTSTR text, int count, LPSIZE size);
BOOL GetTextMetrics(HDC dc, LPTEXTMETRIC tm);
int GetTextFace(HDC dc, int count, LPTSTR face);
BOOL LPtoDP(HDC dc, LPPOINT points, int count);
BOOL DPtoLP(HDC dc, LPPOINT points, int count);
int IntersectClipRect(HDC dc, int left, int top, int right, int bottom);
int ExcludeClipRect(HDC dc, int left, int top, int right, int bottom);
int GetClipBox(HDC dc, LPRECT rect);
int SelectClipRgn(HDC dc, HRGN region);
HRGN CreateRectRgn(int left, int top, int right, int bottom);
HRGN CreateRectRgnIndirect(const RECT *rect);
int CombineRgn(HRGN dest, HRGN src1, HRGN src2, int mode);
BOOL RectVisible(HDC dc, const RECT *rect);
BOOL SetRect(LPRECT rect, int left, int top, int right, int bottom);
BOOL SetRectEmpty(LPRECT rect);
BOOL CopyRect(LPRECT dest, const RECT *src);
BOOL InflateRect(LPRECT rect, int dx, int dy);
BOOL OffsetRect(LPRECT rect, int dx, int dy);
BOOL IsRectEmpty(const RECT *rect);
BOOL PtInRect(const RECT *rect, POINT point);
BOOL IntersectRect(LPRECT dest, const RECT *a, const RECT *b);
BOOL UnionRect(LPRECT dest, const RECT *a, const RECT *b);
BOOL SubtractRect(LPRECT dest, const RECT *a, const RECT *b);
BOOL EqualRect(const RECT *a, const RECT *b);

// More of GDI, for MFC's CDC and friends.
#define HS_HORIZONTAL 0
#define HS_VERTICAL 1
#define HS_FDIAGONAL 2
#define HS_BDIAGONAL 3
#define HS_CROSS 4
#define HS_DIAGCROSS 5
#define OBJ_PEN 1
#define OBJ_BRUSH 2
#define OBJ_DC 3
#define OBJ_REGION 8
#define OBJ_FONT 6
#define OBJ_BITMAP 7
#define OBJ_MEMDC 10
#define MM_TEXT 1
#define BLACKONWHITE 1
#define WHITEONBLACK 2
#define COLORONCOLOR 3
#define HALFTONE 4
#define DI_MASK 0x0001
#define DI_IMAGE 0x0002
#define DI_NORMAL 0x0003
#define DI_COMPAT 0x0004
#define DI_DEFAULTSIZE 0x0008
#define ETO_OPAQUE 0x0002
#define ETO_CLIPPED 0x0004
#define ALTERNATE 1
#define WINDING 2
#define CLR_INVALID 0xFFFFFFFF
#define GDI_ERROR 0xFFFFFFFFL
#define HGDI_ERROR ((HANDLE)-1)
HPEN CreatePenIndirect(const LOGPEN *pen);
HBRUSH CreateBrushIndirect(const LOGBRUSH *brush);
HBRUSH CreateHatchBrush(int hatch, COLORREF color);
HGDIOBJ GetCurrentObject(HDC dc, UINT type);
DWORD GetObjectType(HGDIOBJ object);
int GetBkMode(HDC dc);
UINT GetTextAlign(HDC dc);
int GetROP2(HDC dc);
int SetMapMode(HDC dc, int mode);
int SetStretchBltMode(HDC dc, int mode);
BOOL SetBrushOrgEx(HDC dc, int x, int y, LPPOINT old);
BOOL SetViewportOrgEx(HDC dc, int x, int y, LPPOINT old);
BOOL GetViewportOrgEx(HDC dc, LPPOINT point);
BOOL OffsetViewportOrgEx(HDC dc, int dx, int dy, LPPOINT old);
BOOL SetWindowOrgEx(HDC dc, int x, int y, LPPOINT old);
BOOL GetCurrentPositionEx(HDC dc, LPPOINT point);
BOOL PolyPolyline(HDC dc, const POINT *points, const DWORD *counts, int polylines);
BOOL RoundRect(HDC dc, int left, int top, int right, int bottom, int w, int h);
BOOL PtVisible(HDC dc, int x, int y);
BOOL SetPixelV(HDC dc, int x, int y, COLORREF color);
BOOL FillRgn(HDC dc, HRGN region, HBRUSH brush);
BOOL PaintRgn(HDC dc, HRGN region);
BOOL PtInRegion(HRGN region, int x, int y);
int OffsetRgn(HRGN region, int dx, int dy);
int GetRgnBox(HRGN region, LPRECT rect);
int GetClipRgn(HDC dc, HRGN region);
HWND WindowFromDC(HDC dc);

// windowsx.h helpers.
#define SelectFont(dc, font) ((HFONT)SelectObject((dc), (HGDIOBJ)(HFONT)(font)))
#define SelectPen(dc, pen) ((HPEN)SelectObject((dc), (HGDIOBJ)(HPEN)(pen)))
#define SelectBrush(dc, brush) ((HBRUSH)SelectObject((dc), (HGDIOBJ)(HBRUSH)(brush)))
#define SelectBitmap(dc, bitmap) ((HBITMAP)SelectObject((dc), (HGDIOBJ)(HBITMAP)(bitmap)))
#define DeleteFont(font) DeleteObject((HGDIOBJ)(HFONT)(font))
#define DeletePen(pen) DeleteObject((HGDIOBJ)(HPEN)(pen))
#define DeleteBrush(brush) DeleteObject((HGDIOBJ)(HBRUSH)(brush))
#define DeleteBitmap(bitmap) DeleteObject((HGDIOBJ)(HBITMAP)(bitmap))
#define GetStockBrush(i) ((HBRUSH)GetStockObject(i))
#define GetStockPen(i) ((HPEN)GetStockObject(i))
#define GetStockFont(i) ((HFONT)GetStockObject(i))
