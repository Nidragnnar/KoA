#include "Engine/Platform/Window.h"

#include <SDL3/SDL.h>
#include <glad/glad.h>

#include "Engine/Core/Logger.h"

namespace ke {

bool Window::criar(const std::string& titulo, int largura, int altura) {
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    janela_ = SDL_CreateWindow(titulo.c_str(), largura, altura, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!janela_) {
        log::erro("SDL_CreateWindow: {}", SDL_GetError());
        return false;
    }
    SDL_SetWindowMinimumSize(janela_, 640, 360);
    contexto_ = SDL_GL_CreateContext(janela_);
    if (!contexto_) {
        log::erro("SDL_GL_CreateContext: {}", SDL_GetError());
        return false;
    }
    SDL_GL_MakeCurrent(janela_, contexto_);
    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(SDL_GL_GetProcAddress))) {
        log::erro("glad não carregou o OpenGL");
        return false;
    }
    SDL_GL_SetSwapInterval(1);  // sincronizado com o monitor
    log::info("OpenGL {} | {}", reinterpret_cast<const char*>(glGetString(GL_VERSION)),
              reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
    return true;
}

void Window::destruir() {
    if (contexto_) SDL_GL_DestroyContext(contexto_);
    if (janela_) SDL_DestroyWindow(janela_);
    contexto_ = nullptr;
    janela_ = nullptr;
}

void Window::trocarBuffers() { SDL_GL_SwapWindow(janela_); }

void Window::alternarTelaCheia() {
    telaCheia_ = !telaCheia_;
    SDL_SetWindowFullscreen(janela_, telaCheia_);
}

glm::ivec2 Window::tamanhoPixels() const {
    int w = 0, h = 0;
    SDL_GetWindowSizeInPixels(janela_, &w, &h);
    return {w, h};
}

}
