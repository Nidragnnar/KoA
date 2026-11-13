#pragma once
#include <string>

#include <glm/vec2.hpp>

struct SDL_Window;
typedef struct SDL_GLContextState* SDL_GLContext;

namespace ke {

// Janela SDL3 com contexto OpenGL 3.3 core
class Window {
public:
    Window() = default;
    ~Window() { destruir(); }
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    bool criar(const std::string& titulo, int largura, int altura);
    void destruir();
    void trocarBuffers();
    void alternarTelaCheia();
    void definirTelaCheia(bool sim);
    bool telaCheia() const { return telaCheia_; }

    glm::ivec2 tamanhoPixels() const;  // área desenhável real (DPI incluso)
    SDL_Window* sdl() const { return janela_; }
    SDL_GLContext contexto() const { return contexto_; }

private:
    SDL_Window* janela_ = nullptr;
    SDL_GLContext contexto_ = nullptr;
    bool telaCheia_ = false;
};

}
