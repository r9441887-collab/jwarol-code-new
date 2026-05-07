#pragma once
#include <cmath>

struct Vec2 {
    float x, y;
    Vec2 operator+(const Vec2& o) const { return {x + o.x, y + o.y}; }
    Vec2 operator-(const Vec2& o) const { return {x - o.x, y - o.y}; }
    Vec2 operator*(float s) const { return {x * s, y * s}; }
};

struct Rect {
    float x, y, w, h;
    bool contains(Vec2 p) const {
        return p.x >= x && p.x <= x + w && p.y >= y && p.y <= y + h;
    }
    bool intersects(const Rect& o) const {
        return !(x + w < o.x || x > o.x + o.w || y + h < o.y || y > o.y + o.h);
    }
};