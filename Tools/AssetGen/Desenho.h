#pragma once
#include <cstdint>
#include <random>
#include <string>
#include <vector>

// Pequena "tela" RGBA para desenhar pixel art por código
namespace gen {

using Cor = uint32_t;  // bytes R,G,B,A (little-endian)

Cor hex(const char* h, int alfa = 255);
Cor misturar(Cor a, Cor b, float t);
Cor clarear(Cor c, float f);  // f>0 clareia, f<0 escurece
inline int alfaDe(Cor c) { return static_cast<int>(c >> 24); }
constexpr Cor TRANSP = 0;

struct Img {
    int w = 0, h = 0;
    std::vector<Cor> p;
    Img() = default;
    Img(int w_, int h_) : w(w_), h(h_), p(static_cast<size_t>(w_) * h_, TRANSP) {}

    bool dentro(int x, int y) const { return x >= 0 && y >= 0 && x < w && y < h; }
    void px(int x, int y, Cor c) { if (dentro(x, y)) p[y * w + x] = c; }
    Cor em(int x, int y) const { return dentro(x, y) ? p[y * w + x] : TRANSP; }
    void rect(int x, int y, int rw, int rh, Cor c);
    void linha(int x0, int y0, int x1, int y1, Cor c);
    void elipse(int cx, int cy, int rx, int ry, Cor c);
    // Contorno escuro de 1 px em volta do que é opaco (dá leitura à pixel art)
    void contornar(Cor c);
    void colar(const Img& o, int x, int y);
    // Salpica pixels de `c` sobre os opacos (textura)
    void salpicar(std::mt19937& r, int n, Cor c, int x0 = 0, int y0 = 0, int x1 = -1, int y1 = -1);
};

}
