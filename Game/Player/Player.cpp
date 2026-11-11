#include "Game/Player/Player.h"

#include <cmath>

#include <glm/geometric.hpp>

namespace koa {

glm::vec2 vetor(Direcao d) {
    switch (d) {
    case Direcao::Baixo: return {0, 1};
    case Direcao::Cima: return {0, -1};
    case Direcao::Esquerda: return {-1, 0};
    case Direcao::Direita: return {1, 0};
    }
    return {0, 1};
}

void moverJogador(Jogador& j, glm::vec2 entrada, float dt, const ke::TileMap& mapa, std::span<const ke::AABB> obst) {
    j.acao = std::max(0.0f, j.acao - dt);
    const bool querAndar = glm::dot(entrada, entrada) > 0.01f;
    // Durante o golpe o herói fica plantado (como em jogos de fazenda)
    if (!querAndar || j.acao > 0.0f) {
        j.andando = false;
        return;
    }
    const glm::vec2 dir = glm::normalize(entrada);
    if (std::abs(dir.x) > std::abs(dir.y) + 0.01f) j.dir = dir.x < 0 ? Direcao::Esquerda : Direcao::Direita;
    else if (std::abs(dir.y) > 0.01f) j.dir = dir.y < 0 ? Direcao::Cima : Direcao::Baixo;

    const glm::vec2 feito = ke::moverComColisao(mapa, j.caixa(), dir * j.velocidade * dt, obst);
    j.pos += feito;
    j.andando = glm::dot(feito, feito) > 0.0001f;
    if (j.andando) j.tempoAnim += dt;
}

}
