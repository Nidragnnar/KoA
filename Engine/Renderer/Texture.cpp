#include "Engine/Renderer/Texture.h"

#include <glad/glad.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "Engine/Core/Logger.h"

namespace ke {

Texture::~Texture() {
    if (id_) glDeleteTextures(1, &id_);
}

bool Texture::carregar(const std::filesystem::path& arquivo) {
    int w = 0, h = 0, canais = 0;
    uint8_t* px = stbi_load(arquivo.string().c_str(), &w, &h, &canais, 4);
    if (!px) {
        log::erro("imagem não carregou: {} ({})", arquivo.string(), stbi_failure_reason());
        return false;
    }
    criar(w, h, px);
    stbi_image_free(px);
    return true;
}

void Texture::criar(int largura, int altura, const uint8_t* rgba) {
    if (!id_) glGenTextures(1, &id_);
    largura_ = largura;
    altura_ = altura;
    glBindTexture(GL_TEXTURE_2D, id_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, largura, altura, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
}

void Texture::vincular(int unidade) const {
    glActiveTexture(GL_TEXTURE0 + unidade);
    glBindTexture(GL_TEXTURE_2D, id_);
}

}
