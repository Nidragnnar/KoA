#include "Sprites.h"

#include <cmath>
#include <cstdio>
#include <format>
#include <fstream>
#include <numbers>

#include <nlohmann/json.hpp>

namespace gen {

namespace {

using Lista = std::vector<std::pair<std::string, Img>>;
const Cor CONTORNO = hex("#16110f");

// ---------- Terreno (16×16) ----------

Img grama(int v) {
    std::mt19937 r(100 + v);
    Img t(16, 16);
    // Grama verde e cheia (a era sombria é natureza engolindo as ruínas, não terra morta)
    t.rect(0, 0, 16, 16, hex("#33502e"));
    t.salpicar(r, 40, hex("#2b4527"));
    t.salpicar(r, 22, hex("#3d5e35"));
    std::uniform_int_distribution<int> d(0, 15);
    for (int k = 0; k < 6; ++k) {  // tufos de capim
        const int x = d(r), y = d(r) | 1;
        t.px(x, y, hex("#4a6e3e"));
        t.px(x, y - 1, hex("#557a46"));
        t.px(x + 1, y, hex("#223a20"));
    }
    if (v == 2) t.salpicar(r, 4, hex("#8a8a4c"));   // capim amarelado
    if (v == 3) {                                    // florzinhas brancas
        t.px(d(r), d(r), hex("#d8d8c8"));
        t.px(d(r), d(r), hex("#c8b8d8"));
    }
    return t;
}

Img terra(int v) {
    std::mt19937 r(200 + v);
    Img t(16, 16);
    t.rect(0, 0, 16, 16, hex("#4e4237"));
    t.salpicar(r, 30, hex("#42382f"));
    t.salpicar(r, 14, hex("#5b4e42"));
    std::uniform_int_distribution<int> d(1, 14);
    for (int k = 0; k < 2 + v % 2; ++k) {  // pedrinhas
        const int x = d(r), y = d(r);
        t.px(x, y, hex("#6d645b"));
        t.px(x + 1, y, hex("#5f574f"));
        t.px(x, y + 1, hex("#352d26"));
    }
    return t;
}

Img lama(int v) {
    std::mt19937 r(300 + v);
    Img t(16, 16);
    t.rect(0, 0, 16, 16, hex("#40382f"));
    t.salpicar(r, 28, hex("#352e27"));
    t.salpicar(r, 12, hex("#4c4339"));
    for (int x = 0; x < 16; ++x) {  // sulcos de carroça
        if ((x + v) % 5 != 0) t.px(x, 4 + (v & 1), hex("#342d27"));
        if ((x + v * 3) % 6 != 0) t.px(x, 11 - (v & 1), hex("#342d27"));
    }
    if (v % 2 == 0) {  // poça
        const int cx = 5 + v * 2, cy = 8;
        t.elipse(cx, cy, 3, 1, hex("#323a40"));
        t.px(cx - 1, cy - 1, hex("#56626a"));
        t.px(cx, cy - 1, hex("#4a555e"));
    }
    return t;
}

Img agua(int v) {
    std::mt19937 r(400);  // mesma base em todos os quadros: só as ondas andam
    Img t(16, 16);
    t.rect(0, 0, 16, 16, hex("#26323c"));
    t.salpicar(r, 20, hex("#202a33"));
    for (int k = 0; k < 4; ++k) {
        const int y = (k * 4 + 1) % 16;
        const int x = (k * 7 + v * 3) % 16;
        for (int i = 0; i < 3; ++i) t.px((x + i) % 16, y, hex("#3d505e"));
        t.px((x + 1) % 16, (y + 15) % 16, hex("#4d6170"));
    }
    return t;
}

Img calcada(int v) {
    std::mt19937 r(500 + v);
    Img t(16, 16);
    t.rect(0, 0, 16, 16, hex("#3d3b38"));  // juntas
    std::uniform_int_distribution<int> falta(0, 9), tom(-6, 6);
    for (int fy = 0; fy < 4; ++fy) {
        const int desl = (fy % 2) * 3;
        for (int fx = -1; fx < 3; ++fx) {
            const int x = fx * 6 + desl, y = fy * 4;
            if ((v == 1 || v == 3) && falta(r) < 2) {  // pedra arrancada: aparece a terra
                t.rect(x + 1, y + 1, 4, 2, hex("#4a3f35"));
                continue;
            }
            const Cor base = clarear(hex("#5a5752"), tom(r) / 100.0f);
            t.rect(x + 1, y + 1, 5, 3, base);
            t.rect(x + 1, y + 1, 5, 1, clarear(base, 0.12f));
            t.px(x + 5, y + 3, clarear(base, -0.15f));
        }
    }
    t.salpicar(r, 5, hex("#4c5644"));  // musgo
    return t;
}

Img muro(int v) {
    std::mt19937 r(600 + v);
    Img t(16, 16);
    t.rect(0, 0, 16, 16, hex("#44413d"));
    for (int fy = 0; fy < 4; ++fy)
        for (int fx = -1; fx < 3; ++fx) {
            const int x = fx * 7 + (fy % 2) * 3, y = fy * 4;
            t.rect(x + 1, y + 1, 6, 3, hex("#66625c"));
            t.rect(x + 1, y + 1, 6, 1, hex("#7a766f"));
        }
    t.linha(3 + v, 2, 7 + v, 9, hex("#2f2c29"));  // rachadura
    t.salpicar(r, 8, hex("#4a5440"));
    t.rect(0, 14, 16, 2, hex("#2f2c29"));  // sombra na base
    return t;
}

// ---------- Objetos ----------

void galho(Img& t, std::mt19937& r, float x, float y, float ang, float comp, int esp, int prof) {
    const Cor cor = prof > 2 ? hex("#3a2f28") : hex("#463a31");
    const int passos = static_cast<int>(comp);
    std::uniform_real_distribution<float> torto(-0.35f, 0.35f);
    for (int i = 0; i < passos; ++i) {
        ang += torto(r) * 0.4f;
        x += std::cos(ang);
        y += std::sin(ang);
        for (int e = 0; e < esp; ++e) t.px(static_cast<int>(x) + e, static_cast<int>(y), cor);
    }
    if (prof <= 0) return;
    std::uniform_real_distribution<float> abre(0.35f, 0.75f);
    galho(t, r, x, y, ang - abre(r), comp * 0.62f, std::max(1, esp - 1), prof - 1);
    galho(t, r, x, y, ang + abre(r), comp * 0.6f, std::max(1, esp - 1), prof - 1);
}

Img arvoreMorta(int v) {
    std::mt19937 r(700 + v);
    Img t(32, 48);
    // Raízes
    t.linha(12, 47, 15, 44, hex("#2e2520"));
    t.linha(20, 47, 17, 44, hex("#2e2520"));
    t.linha(9, 46, 14, 45, hex("#2e2520"));
    // Tronco que afina, levemente torto
    for (int y = 47; y >= 22; --y) {
        const float k = (47 - y) / 25.0f;
        const int larg = static_cast<int>(5 - k * 2.5f);
        const int x = 14 + static_cast<int>(std::sin(y * 0.25f + v) * 1.2f);
        t.rect(x, y, larg, 1, hex("#3a2f28"));
        t.px(x, y, hex("#2a221d"));
        if (larg > 2) t.px(x + larg - 1, y, hex("#4d4036"));
    }
    t.px(15, 35, hex("#1e1815"));  // nó
    t.px(16, 35, hex("#1e1815"));
    const float base = -std::numbers::pi_v<float> / 2;
    galho(t, r, 15, 24, base - 0.15f, 10, 2, 3);
    galho(t, r, 16, 29, base + 0.9f + v * 0.1f, 8, 2, 2);
    galho(t, r, 14, 31, base - 0.95f, 7, 1, 2);
    if (v < 2) {
        // Copa cheia e arredondada: tufos do escuro (embaixo) ao claro (em cima, onde bate luz)
        const std::pair<int, int> tufos[] = {{16, 14}, {10, 18}, {22, 18}, {13, 9}, {20, 9}, {16, 5}, {8, 24}, {24, 23}};
        const Cor tons[] = {hex("#1c3a1e"), hex("#264a26"), hex("#315a2e"), hex("#3e6a38")};
        for (int camada = 0; camada < 4; ++camada)
            for (const auto& [x, y] : tufos)
                t.elipse(x - camada / 2, y - camada, 5 - camada, 4 - camada, tons[camada]);
        t.salpicar(r, 30, hex("#4a7a40"), 4, 0, 28, 20);
        t.salpicar(r, 20, hex("#16301a"), 4, 10, 28, 28);
    }
    t.contornar(CONTORNO);
    return t;
}

Img toco() {
    Img t(16, 16);
    t.rect(4, 9, 8, 5, hex("#3a2f28"));
    t.rect(4, 9, 1, 5, hex("#2a221d"));
    t.elipse(8, 9, 4, 2, hex("#6a5844"));
    t.elipse(8, 9, 2, 1, hex("#5a4a3a"));
    t.px(8, 9, hex("#4a3c30"));
    t.linha(3, 14, 5, 13, hex("#2e2520"));
    t.linha(12, 14, 11, 13, hex("#2e2520"));
    t.contornar(CONTORNO);
    return t;
}

Img rocha(int v) {
    std::mt19937 r(800 + v);
    Img t(16, 16);
    if (v == 0) {
        t.elipse(8, 9, 6, 5, hex("#5f5c57"));
        t.elipse(7, 7, 4, 3, hex("#77736c"));
        t.rect(5, 6, 3, 1, hex("#8a867e"));
    } else {
        t.elipse(6, 10, 5, 4, hex("#5f5c57"));
        t.elipse(11, 8, 4, 4, hex("#66625c"));
        t.elipse(10, 7, 2, 2, hex("#7c786f"));
        t.elipse(5, 9, 3, 2, hex("#736f68"));
    }
    t.salpicar(r, 6, hex("#4c4944"));
    t.salpicar(r, 3, hex("#4a5440"));
    t.linha(8, 8, 10, 12, hex("#45423e"));
    t.rect(2, 13, 12, 1, hex("#45423e"));
    t.contornar(CONTORNO);
    return t;
}

Img bau(bool aberto) {
    Img t(16, 16);
    const Cor madeira = hex("#5a3f2a"), escura = hex("#3e2a1c"), ferro = hex("#4a4a52");
    t.rect(2, 7, 12, 7, madeira);
    t.rect(2, 13, 12, 1, escura);
    t.rect(4, 7, 1, 7, ferro);
    t.rect(11, 7, 1, 7, ferro);
    if (aberto) {
        t.rect(3, 4, 10, 3, hex("#1c1410"));  // interior vazio
        t.rect(2, 1, 12, 3, madeira);       // tampa levantada
        t.rect(2, 1, 12, 1, hex("#6a4c34"));
        t.px(6, 6, hex("#8a7438"));          // uma moeda esquecida
    } else {
        t.rect(2, 4, 12, 4, hex("#654832"));
        t.rect(2, 4, 12, 1, hex("#74563c"));
        t.rect(4, 4, 1, 4, ferro);
        t.rect(11, 4, 1, 4, ferro);
        t.rect(7, 7, 2, 2, hex("#b89a48"));  // fechadura
        t.px(7, 7, hex("#e0c470"));
    }
    t.contornar(CONTORNO);
    return t;
}

Img tumulo(int v) {
    Img t(16, 20);
    const int inc = v == 1 ? 1 : 0;
    t.elipse(8, 17, 6, 2, hex("#463b31"));  // montinho de terra
    for (int y = 4; y < 17; ++y) {
        const int desl = inc && y < 10 ? 1 : 0;
        t.rect(4 + desl, y, 8, 1, hex("#6a6863"));
        t.px(4 + desl, y, hex("#7e7c76"));
        t.px(11 + desl, y, hex("#52504c"));
    }
    t.elipse(8 + inc, 5, 4, 2, hex("#6a6863"));
    t.rect(6 + inc, 8, 4, 1, hex("#4c4a46"));  // inscrição gasta
    t.rect(7 + inc, 10, 2, 1, hex("#4c4a46"));
    t.linha(9 + inc, 4, 8 + inc, 9, hex("#4a4844"));  // rachadura
    t.px(5, 15, hex("#4a5440"));
    t.contornar(CONTORNO);
    return t;
}

Img tocha(int q) {
    Img t(8, 20);
    t.rect(3, 9, 2, 11, hex("#4a3828"));
    t.rect(3, 9, 1, 11, hex("#5c4632"));
    t.rect(2, 8, 4, 2, hex("#3c3c42"));  // braseiro
    const int h = 5 + (q == 1 ? 1 : 0);
    const int dx = q == 2 ? 1 : 0;
    t.elipse(4, 8 - h / 2, 2, h / 2, hex("#c8501e"));
    t.elipse(4 + dx, 7 - h / 2, 1, h / 2, hex("#f08a30"));
    t.px(4 + dx, 6, hex("#ffd27a"));
    t.px(4 - dx, 7, hex("#ffd27a"));
    t.px(4 + dx, 1 + q, hex("#a03818"));
    t.contornar(CONTORNO);
    return t;
}

Img cerca(int v) {
    Img t(16, 16);
    const Cor m = hex("#4e4034"), e = hex("#3a2f26");
    t.rect(2, 5, 2, 10, m);
    t.rect(2, 5, 1, 10, e);
    t.rect(12, 7, 2, 8, m);
    if (v == 0) {
        t.rect(2, 7, 12, 2, hex("#5a4a3c"));
        t.linha(4, 11, 9, 13, hex("#5a4a3c"));  // ripa caída
    } else {
        t.linha(3, 8, 10, 6, hex("#5a4a3c"));
        t.rect(9, 12, 5, 1, hex("#5a4a3c"));
    }
    t.contornar(CONTORNO);
    return t;
}

Img arbusto(int v) {
    std::mt19937 r(900 + v);
    Img t(16, 16);
    std::uniform_int_distribution<int> d(-5, 5);
    for (int k = 0; k < 9; ++k) t.linha(8, 14, 8 + d(r), 6 + std::abs(d(r)) / 2, k % 2 ? hex("#5a4c3c") : hex("#4a3f33"));
    t.salpicar(r, 6, hex("#6b6448"));
    return t;
}

Img poco() {
    Img t(24, 24);
    t.elipse(12, 15, 10, 6, hex("#5f5c57"));
    t.elipse(12, 14, 7, 4, hex("#15191d"));
    t.elipse(12, 15, 5, 2, hex("#232f38"));
    for (int a = 0; a < 10; ++a) {  // pedras da borda
        const float ang = a * 0.63f;
        t.px(12 + static_cast<int>(std::cos(ang) * 9), 15 + static_cast<int>(std::sin(ang) * 5), hex("#78746d"));
    }
    t.rect(2, 4, 2, 13, hex("#4a3828"));     // poste esquerdo
    t.rect(20, 7, 2, 10, hex("#4a3828"));    // poste direito (mais baixo: quebrado)
    t.linha(3, 4, 14, 6, hex("#5a4632"));    // travessa caída
    t.rect(2, 22, 20, 1, hex("#3a3733"));
    t.contornar(CONTORNO);
    return t;
}

// ---------- Construções ----------

void tabuasVerticais(Img& t, int x, int y, int w, int h, Cor base, std::mt19937& r) {
    std::uniform_int_distribution<int> tom(-8, 8);
    for (int i = 0; i < w; ++i) {
        const Cor c = clarear(base, ((i / 3) % 2 ? 0.05f : -0.03f) + tom(r) / 200.0f);
        t.rect(x + i, y, 1, h, c);
        if (i % 3 == 0) t.rect(x + i, y, 1, h, clarear(base, -0.22f));
    }
}

void palha(Img& t, int x0, int y0, int w, int h, Cor base, std::mt19937& r, bool buracos) {
    // Telhado de palha em triângulo achatado (frente do telhado)
    for (int y = 0; y < h; ++y) {
        const int recuo = (h - y) * w / (h * 5);
        for (int x = recuo; x < w - recuo; ++x) {
            Cor c = ((x + y * 3) % 5 == 0) ? clarear(base, -0.18f) : base;
            if (y % 4 == 3) c = clarear(base, -0.12f);
            t.px(x0 + x, y0 + y, c);
        }
    }
    std::uniform_int_distribution<int> dx(x0 + 8, x0 + w - 12), dy(y0 + 4, y0 + h - 6);
    if (buracos)
        for (int k = 0; k < 4; ++k) {  // buracos mostrando as vigas
            const int x = dx(r), y = dy(r);
            t.elipse(x, y, 3, 2, hex("#1a1412"));
            t.linha(x - 3, y, x + 3, y - 1, hex("#3a2e24"));
        }
    t.rect(x0, y0 + h - 1, w, 1, clarear(base, -0.3f));
}

Img casa(int nivel) {
    std::mt19937 r(1000 + nivel);
    Img t(64, 88);
    // A base (que colide) ocupa as 48 linhas de baixo: y 40..87
    t.elipse(32, 85, 30, 3, hex("#2a2420", 160));
    if (nivel == 1) {  // Cabana abandonada
        tabuasVerticais(t, 6, 52, 52, 33, hex("#55493e"), r);
        t.rect(14, 60, 1, 6, hex("#2a221d"));  // tábua faltando
        t.rect(40, 54, 2, 10, hex("#1e1815"));
        palha(t, 2, 22, 60, 32, hex("#5c5644"), r, true);
        t.linha(2, 53, 20, 49, hex("#3a2e24"));  // viga caída
        // Porta pendurada torta
        t.rect(27, 66, 10, 19, hex("#120e0c"));
        for (int y = 0; y < 18; ++y) t.rect(28 + y / 6, 67 + y, 7, 1, hex("#4a3c30"));
        // Janela pregada com tábuas em X
        t.rect(44, 62, 9, 8, hex("#141010"));
        t.linha(44, 62, 52, 69, hex("#5a4a3a"));
        t.linha(52, 62, 44, 69, hex("#5a4a3a"));
        // Entulho
        t.rect(6, 82, 8, 2, hex("#4a3c30"));
        t.linha(50, 86, 60, 82, hex("#4a3c30"));
        t.salpicar(r, 40, hex("#4a5440"), 6, 52, 57, 84);  // musgo
    } else if (nivel == 2) {  // Casa reconstruída
        t.rect(6, 80, 52, 6, hex("#5f5c57"));  // alicerce de pedra
        for (int x = 6; x < 58; x += 6) t.rect(x, 80, 1, 6, hex("#45423e"));
        tabuasVerticais(t, 6, 50, 52, 30, hex("#6e5238"), r);
        t.rect(6, 50, 52, 2, hex("#4a3624"));
        palha(t, 2, 20, 60, 32, hex("#8a7a4c"), r, false);
        t.rect(44, 8, 7, 22, hex("#5f5c57"));  // chaminé
        t.rect(44, 8, 7, 2, hex("#77736c"));
        t.rect(27, 64, 10, 21, hex("#5a3e28"));  // porta
        t.rect(27, 64, 10, 1, hex("#3a2a1c"));
        t.rect(29, 70, 6, 1, hex("#45454c"));
        t.px(35, 75, hex("#c9a54a"));
        t.rect(43, 61, 10, 9, hex("#2a3440"));  // janela com vidro
        t.rect(47, 61, 1, 9, hex("#4a3624"));
        t.rect(43, 65, 10, 1, hex("#4a3624"));
        t.rect(41, 61, 2, 9, hex("#5a4030"));  // venezianas
        t.rect(53, 61, 2, 9, hex("#5a4030"));
        t.rect(12, 62, 9, 8, hex("#2a3440"));
        t.rect(16, 62, 1, 8, hex("#4a3624"));
        t.rect(23, 60, 2, 4, hex("#3c3c42"));  // lampião
        t.px(23, 63, hex("#f0b050"));
    } else if (nivel == 3) {  // Mansão medieval
        t.rect(4, 64, 56, 22, hex("#66625c"));  // térreo de pedra
        for (int y = 64; y < 86; y += 4)
            for (int x = 4 + (y / 4 % 2) * 4; x < 60; x += 8) t.rect(x, y, 1, 4, hex("#4c4944"));
        for (int y = 67; y < 86; y += 4) t.rect(4, y, 56, 1, hex("#4c4944"));
        t.rect(4, 42, 56, 22, hex("#c2b8a2"));  // andar de cima em enxaimel
        for (int x = 4; x < 60; x += 9) t.rect(x, 42, 2, 22, hex("#3c2c20"));
        t.linha(6, 62, 13, 44, hex("#3c2c20"));
        t.linha(51, 44, 58, 62, hex("#3c2c20"));
        t.rect(4, 42, 56, 2, hex("#3c2c20"));
        t.rect(4, 62, 56, 2, hex("#3c2c20"));
        for (int y = 0; y < 26; ++y) {  // telhado de ardósia
            const int rec = (26 - y) / 2;
            for (int x = rec; x < 64 - rec; ++x) t.px(x, 16 + y, (x / 3 + y / 3) % 2 ? hex("#4a5262") : hex("#555e70"));
        }
        t.rect(10, 4, 6, 16, hex("#5f5c57"));
        t.rect(48, 6, 6, 14, hex("#5f5c57"));
        t.rect(27, 68, 10, 18, hex("#4a321e"));  // porta em arco
        t.elipse(32, 68, 5, 3, hex("#4a321e"));
        t.px(35, 77, hex("#c9a54a"));
        for (const int x : {10, 44}) {
            t.rect(x, 47, 9, 10, hex("#2a3440"));
            t.rect(x + 4, 47, 1, 10, hex("#3c2c20"));
            t.rect(x - 1, 57, 11, 2, hex("#4a3624"));  // floreira
            t.px(x + 1, 56, hex("#9a4a5a"));
            t.px(x + 6, 56, hex("#b0a050"));
        }
    } else {  // Residência encantada
        const Cor branca = hex("#d6cfc0"), sombra = hex("#a8a092"), ouro = hex("#d8b048");
        t.rect(4, 50, 50, 36, branca);
        for (int y = 54; y < 86; y += 6) t.rect(4, y, 50, 1, sombra);
        t.rect(4, 50, 50, 2, ouro);
        for (int y = 0; y < 30; ++y) {  // telhado azul
            const int rec = (30 - y) / 2;
            for (int x = rec; x < 58 - rec; ++x) t.px(x, 22 + y, (x / 3 + y / 2) % 2 ? hex("#34508a") : hex("#3c5a9a"));
        }
        t.rect(2, 51, 54, 1, ouro);
        // Torre com cone à direita
        t.rect(48, 26, 14, 60, branca);
        t.rect(48, 26, 2, 60, sombra);
        for (int y = 0; y < 22; ++y) {
            const int meia = y / 3;
            t.rect(55 - meia, 4 + y, meia * 2 + 1, 1, y % 3 ? hex("#34508a") : hex("#2c4478"));
        }
        t.rect(48, 26, 14, 1, ouro);
        t.elipse(55, 2, 2, 3, hex("#8fe0ff"));  // cristal no topo
        t.px(55, 1, hex("#ffffff"));
        // Janelas que brilham
        for (const int x : {10, 34}) {
            t.rect(x, 58, 8, 12, hex("#8fd0ff"));
            t.elipse(x + 4, 58, 4, 2, hex("#8fd0ff"));
            t.rect(x + 3, 56, 1, 14, ouro);
        }
        t.rect(53, 40, 5, 8, hex("#8fd0ff"));
        t.rect(22, 66, 10, 20, hex("#3a5a9a"));  // porta azul com arco dourado
        t.elipse(27, 66, 5, 3, hex("#3a5a9a"));
        t.rect(22, 64, 10, 1, ouro);
        t.px(30, 76, ouro);
    }
    t.contornar(CONTORNO);
    return t;
}

Img casteloRuina() {
    std::mt19937 r(2000);
    Img t(160, 112);
    const Cor pedra = hex("#5c5954"), clara = hex("#706c66"), escura = hex("#403d3a");
    auto blocos = [&](int x, int y, int w, int h) {
        t.rect(x, y, w, h, pedra);
        for (int j = y; j < y + h; j += 5) {
            t.rect(x, j, w, 1, escura);
            for (int i = x + ((j / 5) % 2) * 4; i < x + w; i += 8) t.rect(i, j, 1, 5, escura);
        }
        t.rect(x, y, w, 2, clara);
    };
    // Muralha de fundo com ameias quebradas
    blocos(16, 34, 128, 60);
    std::uniform_int_distribution<int> quebra(0, 3);
    for (int x = 16; x < 144; x += 8)
        if (quebra(r) > 0) blocos(x, 28 + quebra(r) * 2, 5, 6);
    // Torres
    blocos(2, 12, 30, 84);
    blocos(130, 30, 28, 66);  // torre direita desabada (mais baixa)
    for (int x = 2; x < 32; x += 7) blocos(x, 6, 4, 6);
    for (int k = 0; k < 18; ++k) t.px(130 + k * 3 / 2, 30 - (k % 4), TRANSP);
    // Janelas escuras
    for (const auto& [x, y] : {std::pair{12, 30}, {12, 56}, {140, 50}, {50, 50}, {104, 50}}) {
        t.rect(x, y, 6, 10, hex("#110d0c"));
        t.elipse(x + 3, y, 3, 2, hex("#110d0c"));
    }
    // Portão em arco desabado
    t.rect(66, 60, 28, 36, hex("#0e0b0a"));
    t.elipse(80, 60, 14, 9, hex("#0e0b0a"));
    t.linha(66, 70, 94, 88, hex("#4a3828"));  // ponte levadiça caída
    t.linha(66, 72, 94, 90, hex("#3a2c20"));
    // Rachaduras, fuligem e musgo
    for (int k = 0; k < 7; ++k) {
        std::uniform_int_distribution<int> px(20, 140), py(36, 90);
        const int x = px(r), y = py(r);
        t.linha(x, y, x + 3, y + 8, hex("#2e2b28"));
    }
    t.salpicar(r, 160, hex("#4a5440"), 0, 30, 159, 95);
    t.salpicar(r, 90, hex("#33302d"), 0, 10, 159, 95);
    // Entulho na frente
    for (int k = 0; k < 14; ++k) {
        std::uniform_int_distribution<int> px(4, 150), py(96, 108);
        const int x = px(r), y = py(r);
        t.elipse(x, y, 3 + k % 3, 2, k % 2 ? pedra : clara);
    }
    t.contornar(CONTORNO);
    return t;
}

// ---------- Herói ----------

struct Visu {
    Cor pele = hex("#d2a27e"), cabelo = hex("#2e2622"), tunica = hex("#4e5260"), capa = hex("#6a2a2a"), capaE = hex("#4a1c1c"),
        calca = hex("#3c3a44"), bota = hex("#2a2220"), cinto = hex("#3a2a1e"), ouro = hex("#b8963e"), olho = hex("#1a1414");
};

void machado(Img& t, int x0, int y0, int x1, int y1) {
    t.linha(x0, y0, x1, y1, hex("#6a4a2a"));
    t.rect(x1 - 1, y1 - 1, 3, 3, hex("#8a8a92"));
    t.px(x1 + 1, y1 - 1, hex("#b0b0b8"));
}

// dir: 0 baixo, 1 cima, 2 lado (virado para a direita); quadro 0..3 (andar) ou 4..5 (golpe)
Img heroi(int dir, int q) {
    const Visu c;
    Img t(16, 24);
    const bool golpe = q >= 4;
    const int passo = golpe ? 0 : (q == 1 ? 1 : q == 3 ? -1 : 0);
    const int sobe = (q == 1 || q == 3) ? 1 : 0;  // corpo sobe 1 px a cada passo
    const int y0 = -sobe;

    t.elipse(8, 22, 5, 1, hex("#000000", 70));  // sombra
    if (dir == 2) {
        // Pernas em passada
        t.rect(6 + passo, 17 + y0, 2, 4, c.calca);
        t.rect(8 - passo, 17 + y0, 2, 4, clarear(c.calca, -0.15f));
        t.rect(6 + passo, 21, 3, 2, c.bota);
        t.rect(8 - passo, 21, 3, 2, clarear(c.bota, -0.2f));
        t.rect(3, 9 + y0, 4, 10, c.capa);  // capa atrás
        t.rect(3, 9 + y0, 1, 10, c.capaE);
        t.rect(5, 10 + y0, 6, 8, c.tunica);
        t.rect(5, 15 + y0, 6, 1, c.cinto);
        t.rect(5, 2 + y0, 6, 7, c.pele);   // cabeça de perfil
        t.rect(4, 2 + y0, 4, 5, c.cabelo);
        t.rect(5, 1 + y0, 6, 2, c.cabelo);
        t.rect(5, 3 + y0, 6, 1, c.ouro);   // diadema
        t.px(10, 5 + y0, c.olho);
        if (golpe) {
            if (q == 4) { t.rect(7, 6 + y0, 2, 4, c.tunica); machado(t, 8, 7 + y0, 5, 1 + y0); }
            else { t.rect(9, 11 + y0, 4, 2, c.tunica); machado(t, 12, 12 + y0, 15, 15 + y0); }
        } else {
            t.rect(7 - passo, 11 + y0, 2, 5, clarear(c.tunica, -0.12f));  // braço balançando
            t.px(7 - passo, 16 + y0, c.pele);
        }
    } else if (dir == 0) {
        t.rect(5, 17 + y0, 2, 4 - (passo > 0), c.calca);
        t.rect(9, 17 + y0, 2, 4 - (passo < 0), c.calca);
        t.rect(5, 21 - (passo > 0), 3, 2, c.bota);
        t.rect(8, 21 - (passo < 0), 3, 2, c.bota);
        t.rect(3, 9 + y0, 10, 10, c.capaE);  // capa aparecendo dos lados
        t.rect(4, 10 + y0, 8, 8, c.tunica);
        t.rect(7, 10 + y0, 2, 5, clarear(c.tunica, 0.08f));
        t.rect(4, 15 + y0, 8, 1, c.cinto);
        t.px(8, 15 + y0, c.ouro);
        t.rect(4, 9 + y0, 8, 2, c.capa);  // gola da capa
        t.rect(5, 2 + y0, 6, 7, c.pele);
        t.rect(4, 1 + y0, 8, 3, c.cabelo);
        t.rect(4, 4 + y0, 1, 4, c.cabelo);
        t.rect(11, 4 + y0, 1, 4, c.cabelo);
        t.rect(5, 3 + y0, 6, 1, c.ouro);
        t.px(8, 3 + y0, hex("#9a2a2a"));  // pedra do diadema
        t.px(6, 6 + y0, c.olho);
        t.px(9, 6 + y0, c.olho);
        if (golpe) {
            if (q == 4) machado(t, 12, 9 + y0, 13, 2 + y0);
            else machado(t, 11, 14 + y0, 12, 21);
        } else {
            t.rect(3, 11 + y0 + (passo > 0), 1, 5, c.tunica);
            t.rect(12, 11 + y0 + (passo < 0), 1, 5, c.tunica);
        }
    } else {  // costas
        t.rect(5, 17 + y0, 2, 4 - (passo < 0), c.calca);
        t.rect(9, 17 + y0, 2, 4 - (passo > 0), c.calca);
        t.rect(5, 21 - (passo < 0), 3, 2, c.bota);
        t.rect(8, 21 - (passo > 0), 3, 2, c.bota);
        if (golpe) {
            if (q == 4) machado(t, 12, 10 + y0, 12, 1 + y0);
            else machado(t, 11, 8 + y0, 9, 0);
        }
        t.rect(3, 9 + y0, 10, 11, c.capa);  // capa cobrindo as costas
        t.rect(3, 9 + y0, 1, 11, c.capaE);
        t.rect(8, 11 + y0, 1, 8, c.capaE);
        t.rect(5, 2 + y0, 6, 7, c.cabelo);
        t.rect(4, 1 + y0, 8, 6, c.cabelo);
        t.rect(4, 3 + y0, 8, 1, c.ouro);
    }
    t.contornar(CONTORNO);
    return t;
}

// ---------- Moradores (lidos de Data/NPCs/npcs.json) ----------

struct AparenciaNpc {
    Cor pele, cabelo, roupa, roupa2;
    bool barba = false, avental = false, saia = false, capuz = false, elmo = false, chapeu = false;
};

// dir: 0 baixo, 1 cima, 2 lado (direita); quadro 0..3
Img morador(const AparenciaNpc& a, int dir, int q) {
    Img t(16, 24);
    const int passo = q == 1 ? 1 : q == 3 ? -1 : 0;
    const int y0 = (q == 1 || q == 3) ? -1 : 0;
    const Cor roupaE = clarear(a.roupa, -0.25f), roupaC = clarear(a.roupa, 0.15f), bota = hex("#2a2220"), olho = hex("#1a1414");
    const Cor cabeloE = clarear(a.cabelo, -0.25f), peleE = clarear(a.pele, -0.18f);
    t.elipse(8, 22, 5, 1, hex("#000000", 70));
    // Pernas (a saia esconde)
    if (a.saia) {
        t.rect(4, 16 + y0, 8, 5, roupaE);
        t.rect(4, 16 + y0, 2, 5, clarear(roupaE, -0.15f));
        t.rect(5 + passo, 21, 2, 2, bota);
        t.rect(9 - passo, 21, 2, 2, bota);
    } else if (dir == 2) {
        t.rect(6 + passo, 17 + y0, 2, 4, hex("#3c3a44"));
        t.rect(8 - passo, 17 + y0, 2, 4, hex("#2e2c36"));
        t.rect(6 + passo, 21, 3, 2, bota);
        t.rect(8 - passo, 21, 3, 2, clarear(bota, -0.2f));
    } else {
        t.rect(5, 17 + y0, 2, 4 - (passo > 0), hex("#3c3a44"));
        t.rect(9, 17 + y0, 2, 4 - (passo < 0), hex("#3c3a44"));
        t.rect(5, 21 - (passo > 0), 3, 2, bota);
        t.rect(8, 21 - (passo < 0), 3, 2, bota);
    }
    // Tronco com luz do lado esquerdo e sombra do direito
    const int tx = dir == 2 ? 5 : 4, tw = dir == 2 ? 6 : 8;
    t.rect(tx, 10 + y0, tw, 7, a.roupa);
    t.rect(tx, 10 + y0, 1, 7, roupaC);
    t.rect(tx + tw - 1, 10 + y0, 1, 7, roupaE);
    t.rect(tx, 15 + y0, tw, 1, a.roupa2);  // cinto/faixa
    if (a.avental && dir != 1) t.rect(tx + 1, 12 + y0, tw - 2, 5, hex("#6a5440"));
    // Braços
    if (dir == 2) {
        t.rect(7 - passo, 11 + y0, 2, 5, roupaE);
        t.px(7 - passo, 16 + y0, a.pele);
    } else {
        t.rect(3, 11 + y0 + (passo > 0), 1, 5, roupaE);
        t.rect(12, 11 + y0 + (passo < 0), 1, 5, roupaE);
        t.px(3, 16 + y0 + (passo > 0), peleE);
        t.px(12, 16 + y0 + (passo < 0), peleE);
    }
    // Cabeça
    if (dir == 1) {
        t.rect(5, 3 + y0, 6, 7, a.cabelo);
        t.rect(5, 3 + y0, 6, 1, clarear(a.cabelo, 0.15f));
        t.rect(10, 4 + y0, 1, 6, cabeloE);
    } else {
        const int hx = dir == 2 ? 6 : 5;
        t.rect(hx, 3 + y0, dir == 2 ? 5 : 6, 7, a.pele);
        t.rect(hx, 8 + y0, dir == 2 ? 5 : 6, 1, peleE);
        t.rect(hx, 2 + y0, dir == 2 ? 5 : 6, 2, a.cabelo);
        if (dir == 2) {
            t.rect(5, 2 + y0, 3, 5, a.cabelo);
            t.px(10, 5 + y0, olho);
        } else {
            t.rect(4, 3 + y0, 1, 4, a.cabelo);
            t.rect(11, 3 + y0, 1, 4, cabeloE);
            t.px(6, 6 + y0, olho);
            t.px(9, 6 + y0, olho);
        }
        if (a.barba) t.rect(hx + (dir == 2 ? 1 : 0), 8 + y0, dir == 2 ? 4 : 6, 3, a.cabelo);
    }
    // Cobertura da cabeça
    if (a.capuz) {
        t.rect(4, 1 + y0, 8, 3, a.roupa);
        t.rect(4, 1 + y0, 1, 8, a.roupa);
        t.rect(11, 1 + y0, 1, 8, roupaE);
        if (dir == 1) t.rect(4, 1 + y0, 8, 9, a.roupa);
    }
    if (a.elmo) {
        t.rect(4, 1 + y0, 8, 3, hex("#7a7a84"));
        t.rect(4, 1 + y0, 8, 1, hex("#a0a0aa"));
        t.rect(7, 0 + y0, 2, 1, a.roupa2);  // pluma
    }
    if (a.chapeu) {
        t.rect(3, 2 + y0, 10, 1, hex("#8a7448"));
        t.rect(5, 0 + y0, 6, 2, hex("#9a8454"));
    }
    t.contornar(CONTORNO);
    return t;
}

void moradores(Lista& l) {
    std::ifstream f("Data/NPCs/npcs.json");
    if (!f) {
        std::fprintf(stderr, "aviso: Data/NPCs/npcs.json não encontrado (rode a partir da raiz do projeto)\n");
        return;
    }
    const auto j = nlohmann::json::parse(f, nullptr, false);
    if (j.is_discarded()) return;
    const char* dirs[] = {"baixo", "cima", "lado"};
    for (const auto& n : j["npcs"]) {
        const auto& ap = n["aparencia"];
        AparenciaNpc a{hex(ap.value("pele", "#d0a080").c_str()), hex(ap.value("cabelo", "#3a2a20").c_str()),
                       hex(ap.value("roupa", "#5a5050").c_str()), hex(ap.value("roupa2", "#3a3030").c_str())};
        a.barba = ap.value("barba", false);
        a.avental = ap.value("avental", false);
        a.saia = ap.value("saia", false);
        a.capuz = ap.value("capuz", false);
        a.elmo = ap.value("elmo", false);
        a.chapeu = ap.value("chapeu", false);
        const std::string id = n["id"].get<std::string>();
        for (int d = 0; d < 3; ++d)
            for (int q = 0; q < 4; ++q) l.emplace_back(std::format("npc_{}_{}_{}", id, dirs[d], q), morador(a, d, q));
    }
}

Img barraco() {
    std::mt19937 r(3000);
    Img t(48, 44);
    t.elipse(24, 42, 22, 2, hex("#000000", 90));
    // Paredes de tábuas escuras remendadas
    tabuasVerticais(t, 4, 24, 40, 18, hex("#3e3328"), r);
    t.rect(4, 40, 40, 2, hex("#2a221c"));
    // Telhado de tábuas tortas, com lona remendada
    for (int y = 0; y < 16; ++y) {
        const int rec = (16 - y) / 2;
        for (int x = rec; x < 48 - rec; ++x) t.px(x, 10 + y, ((x + y / 3) % 6 == 0) ? hex("#2e2620") : hex("#4a3c30"));
    }
    t.rect(26, 12, 12, 8, hex("#5a5444"));  // lona
    t.rect(26, 12, 12, 1, hex("#6a6450"));
    t.linha(2, 26, 46, 24, hex("#2a221c"));
    // Porta (pano pendurado) e janelinha
    t.rect(20, 30, 8, 12, hex("#120e0c"));
    t.rect(20, 30, 8, 3, hex("#5a3a2a"));
    t.rect(34, 30, 5, 4, hex("#1a1612"));
    t.salpicar(r, 30, hex("#3a5230"), 4, 10, 43, 41);  // musgo
    // Lenha empilhada ao lado
    for (int i = 0; i < 3; ++i) t.elipse(7 + i * 3, 39 - (i % 2) * 2, 2, 2, hex("#5a4632"));
    t.contornar(CONTORNO);
    return t;
}

Img fogueira(int q) {
    Img t(16, 20);
    t.elipse(8, 17, 6, 2, hex("#2a2420"));
    for (int i = 0; i < 6; ++i) t.px(3 + i * 2, 17 + (i % 2), hex("#6a6460"));  // pedras em volta
    t.linha(3, 17, 12, 14, hex("#4a3420"));  // lenha cruzada
    t.linha(4, 14, 13, 17, hex("#3a2a1a"));
    const int alt = 7 + (q % 2) * 2;
    const int dx = q == 1 ? 1 : q == 3 ? -1 : 0;
    t.elipse(8, 15 - alt / 2, 4, alt / 2, hex("#c04a1c"));
    t.elipse(8 + dx, 14 - alt / 2, 3, alt / 2 - 1, hex("#f0882c"));
    t.elipse(8 - dx, 14 - alt / 3, 1, alt / 3, hex("#ffd27a"));
    t.px(8 + dx * 2, 15 - alt, hex("#e06020"));
    t.contornar(CONTORNO);
    return t;
}

Img coracao(bool cheio) {
    Img t(7, 6);
    const Cor c = cheio ? hex("#d84a5a") : hex("#3a2a2e");
    t.rect(1, 0, 2, 1, c);
    t.rect(4, 0, 2, 1, c);
    t.rect(0, 1, 7, 2, c);
    t.rect(1, 3, 5, 1, c);
    t.rect(2, 4, 3, 1, c);
    t.px(3, 5, c);
    if (cheio) t.px(1, 1, hex("#f0a0a8"));
    return t;
}

// ---------- Interface e efeitos ----------

Img icone(int tipo) {
    Img t(10, 10);
    if (tipo == 0) {  // madeira
        t.rect(1, 3, 8, 4, hex("#6e5238"));
        t.rect(1, 3, 8, 1, hex("#8a6a48"));
        t.elipse(8, 5, 1, 2, hex("#b09068"));
        t.px(8, 5, hex("#6e5238"));
    } else if (tipo == 1) {  // pedra
        t.elipse(5, 5, 4, 3, hex("#77736c"));
        t.elipse(4, 4, 2, 1, hex("#9a968e"));
    } else {  // ouro
        t.elipse(5, 6, 4, 2, hex("#a8842e"));
        t.elipse(5, 4, 4, 2, hex("#e0bc50"));
        t.px(4, 3, hex("#fff0a0"));
    }
    t.contornar(CONTORNO);
    return t;
}

Img luz() {
    Img t(64, 64);
    for (int y = 0; y < 64; ++y)
        for (int x = 0; x < 64; ++x) {
            const float d = std::hypot(x - 31.5f, y - 31.5f) / 32.0f;
            const float a = std::max(0.0f, 1.0f - d);
            t.px(x, y, (static_cast<Cor>(a * a * 255) << 24) | 0xFFFFFF);
        }
    return t;
}

Img sombra() {
    Img t(16, 6);
    t.elipse(8, 3, 7, 2, hex("#000000", 90));
    t.elipse(8, 3, 5, 1, hex("#000000", 120));
    return t;
}

}  // namespace

std::vector<std::pair<std::string, Img>> gerarTodos() {
    Lista l;
    const char* nomes[] = {"grama", "terra", "lama", "agua", "calcada", "muro"};
    Img (*tiles[])(int) = {grama, terra, lama, agua, calcada, muro};
    for (int t = 0; t < 6; ++t)
        for (int v = 0; v < 4; ++v) l.emplace_back(std::format("terreno_{}_{}", t, v), tiles[t](v));
    (void)nomes;
    for (int v = 0; v < 3; ++v) l.emplace_back(std::format("arvore_morta_{}", v), arvoreMorta(v));
    l.emplace_back("toco", toco());
    for (int v = 0; v < 2; ++v) l.emplace_back(std::format("rocha_{}", v), rocha(v));
    l.emplace_back("bau", bau(false));
    l.emplace_back("bau_aberto", bau(true));
    for (int v = 0; v < 2; ++v) l.emplace_back(std::format("tumulo_{}", v), tumulo(v));
    for (int q = 0; q < 3; ++q) l.emplace_back(std::format("tocha_{}", q), tocha(q));
    for (int v = 0; v < 2; ++v) l.emplace_back(std::format("cerca_{}", v), cerca(v));
    for (int v = 0; v < 2; ++v) l.emplace_back(std::format("arbusto_{}", v), arbusto(v));
    l.emplace_back("poco", poco());
    for (int n = 1; n <= 4; ++n) l.emplace_back(std::format("casa_n{}", n), casa(n));
    l.emplace_back("castelo_ruina", casteloRuina());
    const char* dirs[] = {"baixo", "cima", "lado"};
    for (int d = 0; d < 3; ++d) {
        for (int q = 0; q < 4; ++q) l.emplace_back(std::format("heroi_{}_{}", dirs[d], q), heroi(d, q));
        for (int q = 0; q < 2; ++q) l.emplace_back(std::format("heroi_{}_golpe_{}", dirs[d], q), heroi(d, 4 + q));
    }
    const char* icones[] = {"icone_madeira", "icone_pedra", "icone_ouro"};
    for (int i = 0; i < 3; ++i) l.emplace_back(icones[i], icone(i));
    l.emplace_back("luz", luz());
    l.emplace_back("sombra", sombra());
    l.emplace_back("barraco", barraco());
    for (int q = 0; q < 4; ++q) l.emplace_back(std::format("fogueira_{}", q), fogueira(q));
    l.emplace_back("coracao_cheio", coracao(true));
    l.emplace_back("coracao_vazio", coracao(false));
    moradores(l);
    return l;
}

}
