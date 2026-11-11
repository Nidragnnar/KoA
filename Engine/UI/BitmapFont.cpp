#include "Engine/UI/BitmapFont.h"

#include "Engine/Renderer/Renderer2D.h"

namespace ke {

namespace {

constexpr int LADO = 8, COLUNAS = 16;

const std::u32string& caracteres() {
    static const std::u32string lista = [] {
        std::u32string s;
        for (char32_t c = 32; c <= 126; ++c) s.push_back(c);
        s += U"ÁÂÃÀÇÉÊÍÓÔÕÚáâãàçéêíóôõú—º–×";
        return s;
    }();
    return lista;
}

int indice(char32_t c) {
    const size_t i = caracteres().find(c);
    return static_cast<int>(i != std::u32string::npos ? i : caracteres().find(U'?'));
}

}

std::u32string decodificarUtf8(std::string_view s) {
    std::u32string r;
    for (size_t i = 0; i < s.size();) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        int extras;
        char32_t cp;
        if (c < 0x80) extras = 0, cp = c;
        else if ((c >> 5) == 0x6) extras = 1, cp = c & 0x1F;
        else if ((c >> 4) == 0xE) extras = 2, cp = c & 0x0F;
        else if ((c >> 3) == 0x1E) extras = 3, cp = c & 0x07;
        else extras = -1, cp = 0;
        bool valido = extras >= 0 && i + extras < s.size();
        for (int k = 1; valido && k <= extras; ++k) {
            const unsigned char cont = static_cast<unsigned char>(s[i + k]);
            if ((cont >> 6) != 0x2) valido = false;
            else cp = (cp << 6) | (cont & 0x3F);
        }
        if (!valido) {
            r.push_back(U'?');
            ++i;
            continue;
        }
        r.push_back(cp);
        i += extras + 1;
    }
    return r;
}

void BitmapFont::desenhar(Renderer2D& r, std::string_view utf8, glm::vec2 pos, glm::vec4 cor, bool sombra, int escala) const {
    if (!textura_) return;
    const float L = static_cast<float>(textura_->largura()), A = static_cast<float>(textura_->altura());
    const float lado = static_cast<float>(LADO * escala);
    const std::u32string texto = decodificarUtf8(utf8);
    for (int passada = sombra ? 0 : 1; passada < 2; ++passada) {
        glm::vec2 p = glm::floor(pos) + (passada == 0 ? glm::vec2(static_cast<float>(escala)) : glm::vec2(0));
        const glm::vec4 c = passada == 0 ? glm::vec4(0.05f, 0.04f, 0.04f, cor.a * 0.85f) : cor;
        for (char32_t ch : texto) {
            if (ch != U' ') {
                const int i = indice(ch);
                const float u = (i % COLUNAS) * LADO / L, v = (i / COLUNAS) * LADO / A;
                r.quad(*textura_, p, {lado, lado}, {u, v, u + LADO / L, v + LADO / A}, c);
            }
            p.x += lado;
        }
    }
}

void BitmapFont::desenharCentralizado(Renderer2D& r, std::string_view utf8, glm::vec2 c, glm::vec4 cor, bool sombra, int escala) const {
    desenhar(r, utf8, {std::floor(c.x - largura(utf8, escala) * 0.5f), std::floor(c.y - LADO * escala * 0.5f)}, cor, sombra, escala);
}

int BitmapFont::largura(std::string_view utf8, int escala) const {
    return static_cast<int>(decodificarUtf8(utf8).size()) * LADO * escala;
}

}
