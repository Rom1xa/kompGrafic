#pragma once

#include "Canvas.hpp"
#include <cmath>
#include <algorithm>

inline void bresenhamLine(Canvas& c, int x0, int y0, int x1, int y1, char ch = '*') {
    int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        c.setPixel(x0, y0, ch);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

inline void bresenhamCircle(Canvas& c, int cx, int cy, int r, char ch = '*') {
    int x = r, y = 0;
    int err = 1 - r;
    while (x >= y) {
        c.setPixel(cx + x, cy + y, ch);
        c.setPixel(cx + y, cy + x, ch);
        c.setPixel(cx - y, cy + x, ch);
        c.setPixel(cx - x, cy + y, ch);
        c.setPixel(cx - x, cy - y, ch);
        c.setPixel(cx - y, cy - x, ch);
        c.setPixel(cx + y, cy - x, ch);
        c.setPixel(cx + x, cy - y, ch);
        ++y;
        if (err <= 0)
            err += 2 * y + 1;
        else {
            --x;
            err += 2 * (y - x) + 1;
        }
    }
}

inline void drawTriangle(Canvas& c, int x1, int y1, int x2, int y2, int x3, int y3, char ch = '*') {
    bresenhamLine(c, x1, y1, x2, y2, ch);
    bresenhamLine(c, x2, y2, x3, y3, ch);
    bresenhamLine(c, x3, y3, x1, y1, ch);
}

