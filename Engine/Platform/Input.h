#pragma once
#include <array>

#include <SDL3/SDL_scancode.h>

union SDL_Event;

namespace ke {

// Teclado: "segurando" é o estado atual; "pressionou" acumula toques até o próximo passo de lógica
class Input {
public:
    void processar(const SDL_Event& e);
    void fimDoPasso() { pressionou_.fill(false); }
    void soltarTudo() { segurando_.fill(false); pressionou_.fill(false); }

    bool segurando(SDL_Scancode t) const { return segurando_[t] || simuladoSeg_[t]; }
    bool pressionou(SDL_Scancode t) const { return pressionou_[t]; }

    // Para roteiros de teste: aperta/segura teclas como se fosse o jogador
    void simularToque(SDL_Scancode t) { pressionou_[t] = true; }
    void simularSegurar(SDL_Scancode t, bool s) { simuladoSeg_[t] = s; }
    void limparSimulacao() { simuladoSeg_.fill(false); }

private:
    std::array<bool, SDL_SCANCODE_COUNT> segurando_{};
    std::array<bool, SDL_SCANCODE_COUNT> pressionou_{};
    std::array<bool, SDL_SCANCODE_COUNT> simuladoSeg_{};
};

}
