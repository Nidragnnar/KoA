#pragma once
#include <string>

#include "Game/App/Contexto.h"
#include "Game/UI/Cliques.h"

namespace koa {

// Telas de Opções e Controles, usadas pelo título e pelo menu de pausa.
// Fase 20: texto grande, velocidade do texto e a troca de teclas (Controles)
class TelaOpcoes {
public:
    bool aberta() const { return aberta_; }
    void abrirOpcoes() { aberta_ = true, controles_ = false, sel_ = 0; }
    // Pelo menu (título ou pausa), Esc em Controles fecha tudo; vindo das Opções, volta para elas
    void abrirControles(bool dasOpcoes = false) {
        aberta_ = true, controles_ = true, dasOpcoes_ = dasOpcoes, selControle_ = 0, esperando_ = false, aviso_.clear();
    }
    bool esperandoTecla() const { return controles_ && esperando_; }
    void entrada(Contexto& ctx);
    void desenhar(ke::Renderer2D& r, const ke::BitmapFont& f, const Contexto& ctx) const;

private:
    void entradaControles(Contexto& ctx, bool confirmar);
    void voltarDosControles();

    bool aberta_ = false, controles_ = false, esperando_ = false, dasOpcoes_ = false;
    int sel_ = 0, selControle_ = 0;
    std::string aviso_;
    // Mouse: onde as linhas e as barras de volume apareceram no último quadro
    mutable Cliques cliques_;
    mutable float barraX_ = 0.0f, barraL_ = 1.0f;
};

}
