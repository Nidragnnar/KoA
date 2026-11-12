#include "Eras.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <random>

namespace gen {

namespace {

struct Hsv {
    float h, s, v;  // h em graus
};

Hsv paraHsv(int r, int g, int b) {
    const float R = r / 255.0f, G = g / 255.0f, B = b / 255.0f;
    const float mx = std::max({R, G, B}), mn = std::min({R, G, B}), d = mx - mn;
    float h = 0.0f;
    if (d > 1e-5f) {
        if (mx == R) h = 60.0f * std::fmod((G - B) / d, 6.0f);
        else if (mx == G) h = 60.0f * ((B - R) / d + 2.0f);
        else h = 60.0f * ((R - G) / d + 4.0f);
    }
    if (h < 0) h += 360.0f;
    return {h, mx > 1e-5f ? d / mx : 0.0f, mx};
}

Cor deHsv(Hsv c, int a) {
    c.s = std::clamp(c.s, 0.0f, 1.0f);
    c.v = std::clamp(c.v, 0.0f, 1.0f);
    const float C = c.v * c.s, X = C * (1 - std::abs(std::fmod(c.h / 60.0f, 2.0f) - 1)), m = c.v - C;
    float r = 0, g = 0, b = 0;
    const int seg = static_cast<int>(c.h / 60.0f) % 6;
    if (seg == 0) r = C, g = X;
    else if (seg == 1) r = X, g = C;
    else if (seg == 2) g = C, b = X;
    else if (seg == 3) g = X, b = C;
    else if (seg == 4) r = X, b = C;
    else r = C, b = X;
    auto f = [](float v) { return static_cast<Cor>(std::clamp(static_cast<int>(std::lround(v * 255)), 0, 255)); };
    return f(r + m) | (f(g + m) << 8) | (f(b + m) << 16) | (static_cast<Cor>(a) << 24);
}

enum class Tipo { Grama, Terra, Lama, Agua, Pedra, Arvore, Predio, Tocha, Outro };

Tipo tipoDe(const std::string& n) {
    if (n.starts_with("terreno_0_")) return Tipo::Grama;
    if (n.starts_with("terreno_1_") || n.starts_with("campo_n")) return Tipo::Terra;  // horta é terra arada, não pedra
    if (n.starts_with("terreno_2_")) return Tipo::Lama;
    if (n.starts_with("terreno_3_")) return Tipo::Agua;
    if (n.starts_with("terreno_")) return Tipo::Pedra;
    if (n.starts_with("arvore_") || n.starts_with("arbusto_")) return Tipo::Arvore;
    if (n.starts_with("tocha_")) return Tipo::Tocha;
    if (n.starts_with("rocha_") || n.starts_with("tumulo_") || n == "poco") return Tipo::Pedra;
    return Tipo::Predio;
}

// O que ganha versão por era: terreno, vegetação, pedras, tochas e todas as construções do reino
bool evolui(const std::string& n) {
    static const char* prefixos[] = {"terreno_", "arvore_", "arbusto_", "toco", "rocha_", "tumulo_", "tocha_", "cerca_", "castelo_ruina",
                                     "poco", "barraco", "casa_n", "moradia_n", "taverna_n", "mercado_n", "capela_n", "forja_n", "posto_n",
                                     "torre_n", "campo_n"};
    for (const char* p : prefixos)
        if (n.starts_with(p)) return true;
    return false;
}

// Uma era: cor mais viva e clara a cada passo; pedra vai embranquecendo; a última ganha brilho mágico
Img recolorir(const Img& o, int era, Tipo tipo, uint32_t semente) {
    Img t = o;
    std::mt19937 r(semente);
    std::uniform_real_distribution<float> u(0.0f, 1.0f);
    const float k = era / 3.0f;
    for (int y = 0; y < t.h; ++y) {
        for (int x = 0; x < t.w; ++x) {
            const Cor c = t.em(x, y);
            const int a = alfaDe(c);
            if (a == 0) continue;
            Hsv h = paraHsv(c & 255, (c >> 8) & 255, (c >> 16) & 255);
            if (h.v < 0.09f) continue;  // contorno fica
            const bool verde = h.h > 65 && h.h < 175 && h.s > 0.12f;
            const bool cinza = h.s < 0.16f;
            const bool madeira = h.h >= 12 && h.h <= 50 && h.s >= 0.16f;
            const bool laranja = h.h < 55 && h.s > 0.45f && h.v > 0.5f;
            const bool frio = h.h >= 175 && h.h <= 265 && h.s > 0.06f;
            if (tipo == Tipo::Predio) {
                // Construções: o musgo some, os tons frios esquentam e a parede clareia
                if (verde && h.h < 160) {
                    if (era >= 2) h.h = 35.0f, h.s *= 0.35f, h.v *= 1.15f;
                    else h.s *= 0.6f;
                } else if (frio || verde) {
                    // Girar o matiz de azul até laranja passaria pelo verde: tira a cor e troca direto
                    if (era == 1) h.s *= 0.6f;
                    else h.h = 30.0f, h.s = std::min(0.55f, h.s * (0.6f + 0.4f * k));
                }
                h.v = h.v + (1.0f - h.v) * (cinza ? 0.25f : 0.12f) * k;  // pedra clareia mais que madeira
                if (cinza && era >= 2) h.h = 40.0f, h.s = 0.05f * k;
                t.px(x, y, deHsv(h, a));
                if (cinza && era == 3 && u(r) < 0.003f) t.px(x, y, hex("#e8c860"));  // detalhes dourados
                continue;
            }
            if (verde) {
                h.h += (115.0f - h.h) * 0.35f * k;  // verde mais fresco
                h.s *= 1.0f + 0.45f * k;
                h.v *= 1.0f + 0.35f * k;
            } else if (tipo == Tipo::Tocha && laranja && era == 3) {
                h.h = 195.0f + (h.h - 30.0f) * 0.5f;  // chama mágica azul
            } else if (tipo == Tipo::Agua) {
                h.s *= 1.0f + 0.4f * k;
                h.v *= 1.0f + 0.45f * k;
            } else if (cinza && tipo != Tipo::Terra) {
                h.v = h.v + (1.0f - h.v) * 0.22f * k;  // pedra clareando até o branco
                if (era >= 2) h.h = 40.0f, h.s = 0.05f * k;
            } else if (madeira) {
                h.s *= 1.0f + 0.15f * k;
                h.v *= 1.0f + 0.18f * k;
            } else {
                h.s *= 1.0f + 0.2f * k;
                h.v *= 1.0f + 0.15f * k;
            }
            t.px(x, y, deHsv(h, a));
            // Detalhes que só aparecem com o tempo
            const float sorte = u(r);
            if (tipo == Tipo::Grama && era >= 1 && sorte < 0.0045f * era) {
                static const char* flores[] = {"#f0e6d0", "#e8a0c0", "#f0d060", "#a0c0f0"};
                t.px(x, y, hex(flores[r() % (era >= 2 ? 4 : 2)]));
            } else if (tipo == Tipo::Arvore && verde && era >= 2 && sorte < 0.05f) {
                t.px(x, y, era == 3 && sorte < 0.02f ? hex("#c8f0ff") : hex("#f0b0d0"));  // flores e brilho
            } else if (tipo == Tipo::Agua && era == 3 && sorte < 0.01f) {
                t.px(x, y, hex("#d8f4ff"));  // cintilar
            }
        }
    }
    return t;
}

}

std::vector<std::pair<std::string, Img>> gerarEras(const std::vector<std::pair<std::string, Img>>& base) {
    std::vector<std::pair<std::string, Img>> novos;
    uint32_t semente = 9000;
    for (const auto& [nome, img] : base) {
        if (!evolui(nome)) continue;
        const Tipo tipo = tipoDe(nome);
        for (int era = 1; era <= 3; ++era) novos.emplace_back(std::format("{}_e{}", nome, era), recolorir(img, era, tipo, ++semente));
    }
    return novos;
}

}
