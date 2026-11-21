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
    // Combate
    int vida = 100, vidaMax = 100;
    float invulneravel = 0.0f;     // depois de levar dano, um instante sem levar outro
    glm::vec2 empurrao{0.0f};      // recuo ao ser atingido
    bool atacando = false;         // a ação em curso é golpe de espada (não de ferramenta)
    int golpeId = 0;
    // 1.16: esquiva (um impulso curto; invulnerável quase o tempo todo) e golpe carregado (segurar o ataque)
    float esquiva = 0.0f, recargaEsquiva = 0.0f, carga = 0.0f;
    glm::vec2 dirEsquiva{0.0f, 1.0f};
    bool carregado = false;  // o golpe em curso é o carregado
    static constexpr float DURACAO_ESQUIVA = 0.3f, INVULNERAVEL_ATE = 0.05f, RECARGA_ESQUIVA = 0.9f, CARGA_CHEIA = 0.6f;
    bool esquivando() const { return esquiva > INVULNERAVEL_ATE; }

    static constexpr float DURACAO_ACAO = 0.32f;
    static constexpr float DURACAO_ATAQUE = 0.28f;
    ke::AABB caixa() const { return {{pos.x - 5.0f, pos.y - 5.0f}, {pos.x + 5.0f, pos.y + 1.0f}}; }
};

// Anda com colisão; os sprites têm 4 direções (a diagonal usa a do eixo dominante)
void moverJogador(Jogador& j, glm::vec2 entrada, float dt, const ke::TileMap& mapa, std::span<const ke::AABB> obstaculos);

}
