#ifndef _POINT_H_
#define _POINT_H_

#include <algorithm>

namespace Landstalker {

struct Point
{
    int x;
    int y;
};

struct Size
{
    int width;
    int height;
};

struct Rect
{
    Rect() : left(0), top(0), width(0), height(0) {}
    Rect(int l, int t, int w, int h) : left(l), top(t), width(w), height(h) {}
    Rect(const Point top_left, const Size size)
        : left(top_left.x), top(top_left.y), width(size.width), height(size.height) {}

    bool operator==(const Rect& rhs) const
    {
        return (left == rhs.left) && (top == rhs.top) &&
               (width == rhs.width) && (height == rhs.height);
    }
    bool operator!=(const Rect& rhs) const
    {
        return !(*this == rhs);
    }

    int GetLeft() const { return left; }
    int GetTop() const { return top; }
    int GetRight() const { return left + width; }
    int GetBottom() const { return top + height; }
    int GetWidth() const { return width; }
    int GetHeight() const { return height; }
    Point GetTopLeft() const { return Point{left, top}; }
    Point GetTopRight() const { return Point{left + width,  top}; }
    Point GetBottomLeft() const { return Point{left, top + height}; }
    Point GetBottomRight() const { return Point{left + width, top + height}; }
    Size GetSize() const { return Size{width, height}; }
    int GetArea() const { return width * height; }
    bool Contains(const Point& p) const
    {
        return (p.x >= left) && (p.x < left + width) &&
               (p.y >= top)  && (p.y < top + height);
    }
    bool Contains(const Rect& r) const
    {
        return (r.left >= left) && (r.left + r.width <= left + width) &&
               (r.top  >= top)  && (r.top + r.height <= top + height);
    }
    bool Collides(const Rect& rhs) const
    {
        return !(rhs.left >= left + width ||
                 rhs.left + rhs.width <= left ||
                 rhs.top  >= top + height ||
                 rhs.top + rhs.height <= top);
    }
    Rect GetIntersection(const Rect& rhs) const
    {
        int r_left = std::max(left, rhs.left);
        int r_top = std::max(top, rhs.top);
        int r_right = std::min(left + width, rhs.left + rhs.width);
        int r_bottom = std::min(top + height, rhs.top + rhs.height);
        if (r_right > r_left && r_bottom > r_top)
        {
            return Rect(r_left, r_top, r_right - r_left, r_bottom - r_top);
        }
        return Rect(0, 0, 0, 0); // No intersection
    }
    Rect GetUnion(const Rect& rhs) const
    {
        int r_left = std::min(left, rhs.left);
        int r_top = std::min(top, rhs.top);
        int r_right = std::max(left + width, rhs.left + rhs.width);
        int r_bottom = std::max(top + height, rhs.top + rhs.height);
        return Rect(r_left, r_top, r_right - r_left, r_bottom - r_top);
    }
    Rect Translate(int dx, int dy) const
    {
        return Rect(left + dx, top + dy, width, height);
    }
    Rect Inflate(int dw, int dh) const
    {
        return Rect(left - dw, top - dh, width + 2 * dw, height + 2 * dh);
    }

    int left;
    int top;
    int width;
    int height;
};

} // namespace Landstalker

#endif // _POINT_H_
