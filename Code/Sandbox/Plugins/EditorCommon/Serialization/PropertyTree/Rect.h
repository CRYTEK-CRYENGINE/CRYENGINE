// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

namespace property_tree {

struct Point
{
	Point() {}
	Point(int x, int y) : x_(x), y_(y) {}

	Point operator+(const Point& p) const { return Point(x_ + p.x_, y_ + p.y_); }
	Point operator-(const Point& p) const { return Point(x_ - p.x_, y_ - p.y_); }
	Point& operator+=(const Point& p) { *this = *this + p; return *this; }
	bool operator!=(const Point& rhs) const { return x_ != rhs.x_ || y_ != rhs.y_; }

	int manhattanLength() const { return abs(x_) + abs(y_); }

	int x_;
	int y_;

	// weird accessors to mimic QPoint
	int x() const { return x_; }
	int y() const { return y_; }
	void setX(int x) { x_ = x; }
	void setY(int y) { y_ = y; }
};

struct Rect
{
	int x;
	int y;
	int w;
	int h;

	int left() const { return x; }
	int top() const { return y; }
	void setTop(int top) { y = top; }
	int right() const { return x + w; }
	int bottom() const { return y + h; }

	Point topLeft() const { return Point(x, y); }
	Point bottomRight() const { return Point(x+w, y+h); }

	int width() const { return w; }
	int height() const { return h; }

	Rect() : x(0), y(0), w(0), h(0) {}
	Rect(int x, int y, int w, int h) : x(x), y(y), w(w), h(h) {}

	bool isValid() const { return w >= 0 && h >= 0; }

	template<class TPoint>
	bool contains(const TPoint& p) const {
		if (p.x() < x || p.x() >= x + w)
			return false;
		if (p.y() < y || p.y() >= y + h)
			return false;
		return true;
	}

	Point center() const { return Point(x + w / 2, y + h / 2); }

	Rect adjusted(int l, int t, int r, int b) const {
		return Rect(x + l, y + t,
			x + w + r - (x + l),
			y + h + b - (y + t));
	}

	Rect translated(int x, int y) const	{
		Rect r = *this;
		r.x += x;
		r.y += y;
		return r;
	}

	Rect united(const Rect& rhs) const {
		int newLeft = x < rhs.x ? x :rhs.x;
		int newTop = y < rhs.y ? y : rhs.y;
		int newRight = x + w > rhs.x + rhs.w ? x + w : rhs.x + rhs.w;
		int newBottom = y + h > rhs.y + rhs.h ? y + h : rhs.y + rhs.h;
		return Rect(newLeft, newTop, newRight - newLeft, newBottom - newTop);
	}
};

}

using property_tree::Rect; // temporary
using property_tree::Point; // temporary

