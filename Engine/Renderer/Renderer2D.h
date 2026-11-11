#pragma once
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include "Engine/Renderer/Shader.h"
#include "Engine/Renderer/Texture.h"

namespace ke {

enum class Mistura { Normal, Aditiva, Multiplicar };

// Lote de quadriláteros: junta tudo que usa a mesma textura numa única chamada de desenho
class Renderer2D {
public:
    ~Renderer2D();
    bool iniciar(const std::filesystem::path& pastaShaders);

    void comecar(const glm::mat4& projecao);
    void terminar();  // descarrega o lote pendente

    void quad(const Texture& t, glm::vec2 pos, glm::vec2 tam, glm::vec4 uv, glm::vec4 cor = glm::vec4(1.0f));
    // Sprite com o canto superior esquerdo em `pos`; espelhar inverte na horizontal
    void sprite(const SpriteRegion& r, glm::vec2 pos, glm::vec4 cor = glm::vec4(1.0f), bool espelhar = false, float escala = 1.0f);
    void retangulo(glm::vec2 pos, glm::vec2 tam, glm::vec4 cor);
    void contorno(glm::vec2 pos, glm::vec2 tam, glm::vec4 cor);
    void definirMistura(Mistura m);

    const Texture& branca() const { return branca_; }
    int chamadasDesenho() const { return chamadasUltimo_; }

private:
    struct Vertice {
        glm::vec2 pos, uv;
        glm::vec4 cor;
    };
    void descarregar();

    Shader shader_;
    Texture branca_;
    unsigned vao_ = 0, vbo_ = 0, ebo_ = 0;
    std::vector<Vertice> vertices_;
    const Texture* texturaAtual_ = nullptr;
    Mistura mistura_ = Mistura::Normal;
    int chamadas_ = 0, chamadasUltimo_ = 0;
};

}
