#pragma once
#include <SDL3/SDL_scancode.h>

// Fase 20: as ações do jogo com tecla trocável (Opções → Controles). O Input pergunta pela ação, nunca pela tecla.
// Dentro dos painéis, W/S/setas escolhem, E/Enter/Espaço confirmam e Esc fecha (essas não mudam)
namespace koa {

struct AcaoTecla {
    const char* id;
    const char* nome;
    SDL_Scancode padrao;
};

inline constexpr AcaoTecla ACOES_TECLAS[] = {
    {"cima", "andar para cima", SDL_SCANCODE_W},
    {"baixo", "andar para baixo", SDL_SCANCODE_S},
    {"esquerda", "andar para a esquerda", SDL_SCANCODE_A},
    {"direita", "andar para a direita", SDL_SCANCODE_D},
    {"interagir", "interagir (cortar, falar, pescar)", SDL_SCANCODE_E},
    {"atacar", "atacar", SDL_SCANCODE_J},
    {"comer", "comer", SDL_SCANCODE_H},
    {"presente", "dar presente; comer no fogão", SDL_SCANCODE_F},
    {"livro", "Livro do Reino (todas as abas)", SDL_SCANCODE_TAB},
    {"obras", "obras", SDL_SCANCODE_B},
    {"reino", "reino e impostos", SDL_SCANCODE_K},
    {"mercado", "mercado", SDL_SCANCODE_V},
    {"terras", "terras do reino", SDL_SCANCODE_R},
    {"conselho", "Conselho Real", SDL_SCANCODE_G},
    {"povo", "o povo", SDL_SCANCODE_P},
    {"vizinhos", "os vizinhos e a fama", SDL_SCANCODE_T},
    {"diario", "diário", SDL_SCANCODE_L},
    {"cronicas", "Crônicas do Reino", SDL_SCANCODE_C},
    {"mensagens", "mensagens", SDL_SCANCODE_N},
    {"salvar", "salvar", SDL_SCANCODE_F5},
    {"som", "som ligado/desligado", SDL_SCANCODE_M},
};

// Teclas que não podem ser escolhidas (menu, tela cheia e a confirmação dos painéis)
inline bool teclaReservada(SDL_Scancode t) {
    return t == SDL_SCANCODE_ESCAPE || t == SDL_SCANCODE_F11 || t == SDL_SCANCODE_RETURN || t == SDL_SCANCODE_UNKNOWN;
}

}
