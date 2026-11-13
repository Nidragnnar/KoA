#pragma once
#include "Game/App/Contexto.h"

namespace koa {

// Telas de Opções e Controles, usadas pelo título e pelo menu de pausa
class TelaOpcoes {
public:
    bool aberta() const { return aberta_; }
    void abrirOpcoes() { aberta_ = true, controles_ = false, sel_ = 0; }
    void abrirControles() { aberta_ = true, controles_ = true; }
    void entrada(Contexto& ctx);
    void desenhar(ke::Renderer2D& r, const ke::BitmapFont& f, const Contexto& ctx) const;

private:
    bool aberta_ = false, controles_ = false;
    int sel_ = 0;
};

}
