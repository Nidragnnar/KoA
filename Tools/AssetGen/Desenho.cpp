#include "Desenho.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace gen {

namespace {
int canal(Cor c, int i) { return static_cast<int>((c >> (8 * i)) & 255); }
Cor montar(int r, int g, int b, int a) {
    auto f = [](int v) { return static_cast<Cor>(std::clamp(v, 0, 255)); };
    return f(r) | (f(g) << 8) | (f(b) << 16) | (f(a) << 24);
}
}

Cor hex(const char* h, int alfa) {
    const unsigned long v = std::strtoul(h + (h[0] == '#' ? 1 : 0), nullptr, 16);
    return montar(static_cast<int>((v >> 16) & 255), static_cast<int>((v >> 8) & 255), static_cast<int>(v & 255), alfa);
}

Cor misturar(Cor a, Cor b, float t) {
    auto m = [&](int i) { return static_cast<int>(std::lround(canal(a, i) + (canal(b, i) - canal(a, i)) * t)); };
    return montar(m(0), m(1), m(2), m(3));
}

Cor clarear(Cor c, float f) {
    const Cor alvo = f >= 0 ? montar(255, 255, 255, canal(c, 3)) : montar(0, 0, 0, canal(c, 3));
    return misturar(c, alvo, std::abs(f));
}

void Img::rect(int x, int y, int rw, int rh, Cor c) {
    for (int j = y; j < y + rh; ++j)
        for (int i = x; i < x + rw; ++i) px(i, j, c);
}

void Img::linha(int x0, int y0, int x1, int y1, Cor c) {
    const int dx = std::abs(x1 - x0), dy = -std::abs(y1 - y0);
    const int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        px(x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        const int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void Img::elipse(int cx, int cy, int rx, int ry, Cor c) {
    for (int j = -ry; j <= ry; ++j)
        for (int i = -rx; i <= rx; ++i)
            if (static_cast<float>(i * i) / (rx * rx + 0.3f) + static_cast<float>(j * j) / (ry * ry + 0.3f) <= 1.0f) px(cx + i, cy + j, c);
}

void Img::contornar(Cor c) {
    const std::vector<Cor> orig = p;
    auto opaco = [&](int x, int y) { return dentro(x, y) && alfaDe(orig[y * w + x]) > 128; };
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            if (!opaco(x, y) && (opaco(x - 1, y) || opaco(x + 1, y) || opaco(x, y - 1) || opaco(x, y + 1))) p[y * w + x] = c;
}

void Img::colar(const Img& o, int x, int y) {
    for (int j = 0; j < o.h; ++j)
        for (int i = 0; i < o.w; ++i)
            if (alfaDe(o.p[j * o.w + i]) > 0) px(x + i, y + j, o.p[j * o.w + i]);
}

void Img::salpicar(std::mt19937& r, int n, Cor c, int x0, int y0, int x1, int y1) {
    if (x1 < 0) x1 = w - 1;
    if (y1 < 0) y1 = h - 1;
    std::uniform_int_distribution<int> dx(x0, x1), dy(y0, y1);
    for (int k = 0; k < n; ++k) {
        const int x = dx(r), y = dy(r);
        if (alfaDe(em(x, y)) > 0) px(x, y, c);
    }
}

}
