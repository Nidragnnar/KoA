#pragma once
#include <string>
#include <string_view>

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

namespace ke {

class Renderer2D;
class Texture;

std::u32string decodificarUtf8(std::string_view s);

// Fonte de pixel 8×8 (ASCII + acentos do português), 16 glifos por linha na imagem
class BitmapFont {
public:
    void definirTextura(const Texture* t) { textura_ = t; }
    // Sombra de 1 px por baixo, para ler em cima de qualquer fundo
    void desenhar(Renderer2D& r, std::string_view utf8, glm::vec2 pos, glm::vec4 cor, bool sombra = true, int escala = 1) const;
    void desenharCentralizado(Renderer2D& r, std::string_view utf8, glm::vec2 centro, glm::vec4 cor, bool sombra = true,
                              int escala = 1) const;
    int largura(std::string_view utf8, int escala = 1) const;
    static constexpr int ALTURA = 8;

private:
    const Texture* textura_ = nullptr;
};

}
