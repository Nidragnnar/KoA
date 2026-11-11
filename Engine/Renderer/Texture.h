#pragma once
#include <cstdint>
#include <filesystem>

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

namespace ke {

// Textura RGBA com filtro nearest (pixel art nítida)
class Texture {
public:
    Texture() = default;
    ~Texture();
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    bool carregar(const std::filesystem::path& arquivo);
    void criar(int largura, int altura, const uint8_t* rgba);
    void vincular(int unidade = 0) const;

    unsigned id() const { return id_; }
    int largura() const { return largura_; }
    int altura() const { return altura_; }

private:
    unsigned id_ = 0;
    int largura_ = 0, altura_ = 0;
};

// Pedaço de uma textura (quadro do atlas)
struct SpriteRegion {
    const Texture* textura = nullptr;
    glm::vec4 uv{0.0f, 0.0f, 1.0f, 1.0f};  // u0, v0, u1, v1
    glm::ivec2 tamanho{0};                  // em pixels
};

}
