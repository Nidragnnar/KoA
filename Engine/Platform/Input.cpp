#include "Engine/Platform/Input.h"

#include <SDL3/SDL_events.h>

namespace ke {

namespace {

int botao(uint8_t b) {
    switch (b) {
    case SDL_BUTTON_LEFT: return 0;
    case SDL_BUTTON_RIGHT: return 1;
    case SDL_BUTTON_MIDDLE: return 2;
    default: return -1;
    }
}

}

glm::vec2 Input::paraTela(float x, float y) const {
    // Coordenadas da janela (pontos) → pixels → tela interna
    if (tela_.largura <= 0 || tela_.altura <= 0) return {x, y};
    const float px = x * densidade_ - tela_.x, py = y * densidade_ - tela_.y;
    return {px * tela_.baseL / tela_.largura, py * tela_.baseA / tela_.altura};
}

void Input::processar(const SDL_Event& e) {
    if (e.type == SDL_EVENT_KEY_DOWN) {
        segurando_[e.key.scancode] = true;
        pressionou_[e.key.scancode] = true;  // repetição do teclado também conta (útil em menus)
        if (!e.key.repeat) ultima_ = e.key.scancode;
    } else if (e.type == SDL_EVENT_KEY_UP) {
        segurando_[e.key.scancode] = false;
    } else if (ignorarMouse_ && (e.type == SDL_EVENT_MOUSE_MOTION || e.type == SDL_EVENT_MOUSE_BUTTON_DOWN || e.type == SDL_EVENT_MOUSE_BUTTON_UP ||
                                 e.type == SDL_EVENT_MOUSE_WHEEL)) {
        return;  // roteiro: só o mouse simulado
    } else if (e.type == SDL_EVENT_MOUSE_MOTION) {
        mouse_ = paraTela(e.motion.x, e.motion.y);
        moveu_ = true;
    } else if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        mouse_ = paraTela(e.button.x, e.button.y);
        if (const int b = botao(e.button.button); b >= 0) segurandoMouse_[b] = clicou_[b] = true;
    } else if (e.type == SDL_EVENT_MOUSE_BUTTON_UP) {
        if (const int b = botao(e.button.button); b >= 0) segurandoMouse_[b] = false;
    } else if (e.type == SDL_EVENT_MOUSE_WHEEL) {
        roda_ += e.wheel.y;
    } else if (e.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
        segurando_.fill(false);  // não fica andando sozinho ao trocar de janela
        segurandoMouse_.fill(false);
    }
}

}
