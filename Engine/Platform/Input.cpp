#include "Engine/Platform/Input.h"

#include <SDL3/SDL_events.h>

namespace ke {

void Input::processar(const SDL_Event& e) {
    if (e.type == SDL_EVENT_KEY_DOWN) {
        segurando_[e.key.scancode] = true;
        pressionou_[e.key.scancode] = true;  // repetição do teclado também conta (útil em menus)
    } else if (e.type == SDL_EVENT_KEY_UP) {
        segurando_[e.key.scancode] = false;
    } else if (e.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
        segurando_.fill(false);  // não fica andando sozinho ao trocar de janela
    }
}

}
