#pragma once

union SDL_Event;
struct SDL_Window;
typedef struct SDL_GLContextState* SDL_GLContext;

namespace ke {

// Dear ImGui: ferramentas internas e depuração (F1)
class DebugUI {
public:
    bool iniciar(SDL_Window* janela, SDL_GLContext contexto);
    void encerrar();
    void processar(const SDL_Event& e);
    void novoQuadro();
    void renderizar();

    bool visivel = false;
    bool querTeclado() const;

private:
    bool iniciado_ = false;
};

}
