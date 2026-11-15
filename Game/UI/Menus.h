#pragma once
#include "Game/App/Contexto.h"
#include "Game/UI/Cliques.h"

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
    // Mouse: onde as linhas e as barras de volume apareceram no último quadro
    mutable Cliques cliques_;
    mutable float barraX_ = 0.0f, barraL_ = 1.0f;
};

}
