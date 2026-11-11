#include "Engine/Debug/Screenshot.h"

#include <cstring>
#include <vector>

#include <SDL3/SDL.h>
#include <glad/glad.h>

#include "Engine/Core/Logger.h"

namespace ke {

bool salvarCaptura(const std::filesystem::path& arquivo, int l, int a) {
    std::vector<uint8_t> px(static_cast<size_t>(l) * a * 4), linha(static_cast<size_t>(l) * 4);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, l, a, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    // O OpenGL lê de baixo para cima
    for (int y = 0; y < a / 2; ++y) {
        uint8_t* c = &px[static_cast<size_t>(y) * l * 4];
        uint8_t* b = &px[static_cast<size_t>(a - 1 - y) * l * 4];
        std::memcpy(linha.data(), c, linha.size());
        std::memcpy(c, b, linha.size());
        std::memcpy(b, linha.data(), linha.size());
    }
    std::error_code ec;
    std::filesystem::create_directories(arquivo.parent_path(), ec);
    SDL_Surface* s = SDL_CreateSurfaceFrom(l, a, SDL_PIXELFORMAT_ABGR8888, px.data(), l * 4);
    const bool ok = s && SDL_SaveBMP(s, arquivo.string().c_str());
    if (s) SDL_DestroySurface(s);
    if (!ok) log::erro("captura falhou: {}", SDL_GetError());
    else log::info("captura salva: {}", arquivo.string());
    return ok;
}

}
