#pragma once
#include <filesystem>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "Engine/Renderer/Shader.h"

namespace ke {

// Ajuste de cor da imagem final: é aqui que a era do reino muda o "clima" da tela
struct ColorGrade {
    float saturacao = 1.0f;
    float contraste = 1.0f;
    float brilho = 0.0f;
    glm::vec3 tinta{1.0f};
    float vinheta = 0.0f;
};

struct Retangulo {
    int x = 0, y = 0, largura = 0, altura = 0;
};

// Maior escala inteira que cabe na janela; se a janela for menor que a base, encolhe proporcionalmente
Retangulo calcularLetterbox(int janelaL, int janelaA, int baseL, int baseA);

// Tela interna de baixa resolução (640×360) ampliada sem borrar
class RenderTarget {
public:
    ~RenderTarget();
    bool criar(int largura, int altura, const std::filesystem::path& pastaShaders);
    void comecar();  // passa a desenhar na tela interna
    void apresentar(glm::ivec2 janela, const ColorGrade& grade, float tempo);

    int largura() const { return largura_; }
    int altura() const { return altura_; }
    const Retangulo& ultimaArea() const { return area_; }

private:
    unsigned fbo_ = 0, cor_ = 0, vao_ = 0;
    int largura_ = 0, altura_ = 0;
    Shader posProcesso_;
    Retangulo area_;
};

}
