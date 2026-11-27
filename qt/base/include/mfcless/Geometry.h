/*
 * Geometry.h
 * ----------
 * Purpose: Points, sizes and rectangles as upstream's code uses them: with
 *          public members (x, y; cx, cy; left, top, right, bottom), and
 *          rectangles that end before right and bottom, as on Windows.
 * Notes  : They convert to and from Qt's QPoint, QSize and QRect, so they
 *          can be handed to Qt directly. A QRect's right() is the last pixel
 *          inside; a Rect's right is the first one outside.
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */

#pragma once

#include <QtCore/QPoint>
#include <QtCore/QRect>
#include <QtCore/QSize>

#include <algorithm>

namespace mfcless
{

struct Size
{
	int cx = 0, cy = 0;

	constexpr Size() = default;
	constexpr Size(int w, int h) : cx(w), cy(h) {}
	constexpr Size(const QSize &s) : cx(s.width()), cy(s.height()) {}
	constexpr operator QSize() const { return QSize(cx, cy); }

	constexpr bool operator==(const Size &o) const { return cx == o.cx && cy == o.cy; }
	constexpr bool operator!=(const Size &o) const { return !(*this == o); }
	constexpr Size operator+(const Size &o) const { return {cx + o.cx, cy + o.cy}; }
	constexpr Size operator-(const Size &o) const { return {cx - o.cx, cy - o.cy}; }
	constexpr Size operator-() const { return {-cx, -cy}; }
	Size &operator+=(const Size &o) { cx += o.cx; cy += o.cy; return *this; }
	Size &operator-=(const Size &o) { cx -= o.cx; cy -= o.cy; return *this; }
	void SetSize(int w, int h) { cx = w; cy = h; }
};

struct Point
{
	int x = 0, y = 0;

	constexpr Point() = default;
	constexpr Point(int px, int py) : x(px), y(py) {}
	constexpr Point(const QPoint &p) : x(p.x()), y(p.y()) {}
	constexpr operator QPoint() const { return QPoint(x, y); }

	constexpr bool operator==(const Point &o) const { return x == o.x && y == o.y; }
	constexpr bool operator!=(const Point &o) const { return !(*this == o); }
	constexpr Point operator+(const Point &o) const { return {x + o.x, y + o.y}; }
	constexpr Point operator+(const Size &s) const { return {x + s.cx, y + s.cy}; }
	// The difference of two points is a size, as on Windows.
	constexpr Size operator-(const Point &o) const { return {x - o.x, y - o.y}; }
	constexpr Point operator-(const Size &s) const { return {x - s.cx, y - s.cy}; }
	constexpr Point operator-() const { return {-x, -y}; }
	Point &operator+=(const Point &o) { x += o.x; y += o.y; return *this; }
	Point &operator+=(const Size &s) { x += s.cx; y += s.cy; return *this; }
	Point &operator-=(const Point &o) { x -= o.x; y -= o.y; return *this; }
	Point &operator-=(const Size &s) { x -= s.cx; y -= s.cy; return *this; }
	void SetPoint(int px, int py) { x = px; y = py; }
	void Offset(int dx, int dy) { x += dx; y += dy; }
};

struct Rect
{
	int left = 0, top = 0, right = 0, bottom = 0;

	constexpr Rect() = default;
	constexpr Rect(int l, int t, int r, int b) : left(l), top(t), right(r), bottom(b) {}
	constexpr Rect(const Point &topLeft, const mfcless::Size &size) : left(topLeft.x), top(topLeft.y), right(topLeft.x + size.cx), bottom(topLeft.y + size.cy) {}
	constexpr Rect(const Point &topLeft, const Point &bottomRight) : left(topLeft.x), top(topLeft.y), right(bottomRight.x), bottom(bottomRight.y) {}
	constexpr Rect(const QRect &r) : left(r.left()), top(r.top()), right(r.left() + r.width()), bottom(r.top() + r.height()) {}
	constexpr operator QRect() const { return QRect(left, top, right - left, bottom - top); }

	constexpr int Width() const { return right - left; }
	constexpr int Height() const { return bottom - top; }
	constexpr mfcless::Size GetSize() const { return {Width(), Height()}; }
	constexpr mfcless::Size Size() const { return GetSize(); }
	constexpr Point TopLeft() const { return {left, top}; }
	constexpr Point BottomRight() const { return {right, bottom}; }
	constexpr Point CenterPoint() const { return {(left + right) / 2, (top + bottom) / 2}; }
	constexpr bool IsRectEmpty() const { return right <= left || bottom <= top; }
	constexpr bool IsRectNull() const { return left == 0 && top == 0 && right == 0 && bottom == 0; }
	constexpr bool PtInRect(const Point &p) const { return p.x >= left && p.x < right && p.y >= top && p.y < bottom; }
	constexpr bool EqualRect(const Rect &o) const { return *this == o; }

	constexpr bool operator==(const Rect &o) const { return left == o.left && top == o.top && right == o.right && bottom == o.bottom; }
	constexpr bool operator!=(const Rect &o) const { return !(*this == o); }

	void SetRect(int l, int t, int r, int b) { left = l; top = t; right = r; bottom = b; }
	void SetRectEmpty() { left = top = right = bottom = 0; }
	void OffsetRect(int dx, int dy) { left += dx; right += dx; top += dy; bottom += dy; }
	void OffsetRect(const Point &p) { OffsetRect(p.x, p.y); }
	void OffsetRect(const mfcless::Size &s) { OffsetRect(s.cx, s.cy); }
	void InflateRect(int dx, int dy) { left -= dx; right += dx; top -= dy; bottom += dy; }
	void InflateRect(int l, int t, int r, int b) { left -= l; top -= t; right += r; bottom += b; }
	void InflateRect(const mfcless::Size &s) { InflateRect(s.cx, s.cy); }
	void DeflateRect(int dx, int dy) { InflateRect(-dx, -dy); }
	void DeflateRect(int l, int t, int r, int b) { InflateRect(-l, -t, -r, -b); }
	void DeflateRect(const mfcless::Size &s) { InflateRect(-s.cx, -s.cy); }
	void NormalizeRect()
	{
		if(left > right)
			std::swap(left, right);
		if(top > bottom)
			std::swap(top, bottom);
	}
	void MoveToX(int x) { right += x - left; left = x; }
	void MoveToY(int y) { bottom += y - top; top = y; }
	void MoveToXY(int x, int y) { MoveToX(x); MoveToY(y); }
	void MoveToXY(const Point &p) { MoveToXY(p.x, p.y); }
	// False (and empty) if they do not overlap.
	bool IntersectRect(const Rect &a, const Rect &b)
	{
		*this = Rect(std::max(a.left, b.left), std::max(a.top, b.top), std::min(a.right, b.right), std::min(a.bottom, b.bottom));
		if(IsRectEmpty())
		{
			SetRectEmpty();
			return false;
		}
		return true;
	}
	bool UnionRect(const Rect &a, const Rect &b)
	{
		if(a.IsRectEmpty())
			*this = b;
		else if(b.IsRectEmpty())
			*this = a;
		else
			*this = Rect(std::min(a.left, b.left), std::min(a.top, b.top), std::max(a.right, b.right), std::max(a.bottom, b.bottom));
		return !IsRectEmpty();
	}
	Rect operator&(const Rect &o) const { Rect r; r.IntersectRect(*this, o); return r; }
	Rect operator|(const Rect &o) const { Rect r; r.UnionRect(*this, o); return r; }
	Rect &operator&=(const Rect &o) { IntersectRect(*this, o); return *this; }
	Rect &operator|=(const Rect &o) { UnionRect(*this, o); return *this; }
	Rect operator+(const Point &p) const { Rect r = *this; r.OffsetRect(p); return r; }
	Rect operator-(const Point &p) const { Rect r = *this; r.OffsetRect(-p.x, -p.y); return r; }
	Rect &operator+=(const Point &p) { OffsetRect(p); return *this; }
	Rect &operator-=(const Point &p) { OffsetRect(-p.x, -p.y); return *this; }
};

} // namespace mfcless
