#pragma once
#include <span>

#include <glm/vec2.hpp>

#include "Engine/Physics/Collision.h"

namespace koa {

enum class Direcao { Baixo = 0, Cima = 1, Esquerda = 2, Direita = 3 };
glm::vec2 vetor(Direcao d);

// Protagonista: posição dos pés (centro da base), em pixels do mundo
struct Jogador {
    glm::vec2 pos{0.0f};
    Direcao dir = Direcao::Baixo;
    bool andando = false;
    float tempoAnim = 0.0f;  // avança enquanto anda
    float acao = 0.0f;       // > 0 enquanto o golpe está em curso
    float velocidade = 74.0f;

    static constexpr float DURACAO_ACAO = 0.32f;
    ke::AABB caixa() const { return {{pos.x - 5.0f, pos.y - 5.0f}, {pos.x + 5.0f, pos.y + 1.0f}}; }
};

// Anda com colisão; os sprites têm 4 direções (a diagonal usa a do eixo dominante)
void moverJogador(Jogador& j, glm::vec2 entrada, float dt, const ke::TileMap& mapa, std::span<const ke::AABB> obstaculos);

}
