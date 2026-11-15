#pragma once
#include <string>
#include <utility>
#include <vector>

#include <glm/vec2.hpp>
#include <nlohmann/json_fwd.hpp>

#include "Game/Inventory/Inventory.h"

// Fase 13: hordas avisadas com dias de antecedência e a defesa do portão leste (Data/Kingdom/defesa.json)
namespace koa {

struct SoldadoDef {
    int vida = 40, dano = 6;
    float velocidade = 44.0f, recarga = 0.8f, visao = 170.0f;
};

struct DadosDefesa {
    int diaMin = 10, populacaoMin = 12;      // a primeira horda só vem com o reino crescido
    int intervaloMin = 9, intervaloMax = 12;  // dias entre uma horda e o aviso da próxima
    int avisoDias = 3, hora = 20;
    int forcaBase = 8, forcaPorHorda = 3;
    std::vector<std::pair<std::string, int>> custo;  // criatura → pontos de força (da mais cara para a mais barata)
    std::vector<glm::ivec2> entrada, postoSoldados;
    glm::ivec2 praca{30, 21};
    Custo saque;                // o que cada criatura que chega à praça leva
    float saqueFelicidade = 2.0f;
    int ouroPorCriatura = 3;
    float alcanceTorre = 150.0f, recargaTorre = 1.1f;
    SoldadoDef soldado;
    float consertoMestreObras = 0.3f;  // parte do estrago que o Mestre das Obras conserta por noite
};

bool lerDefesa(const nlohmann::json& j, DadosDefesa& d, std::string* erro);

// Força da horda: cresce a cada horda e com a corrupção da floresta (de onde as criaturas vêm)
int forcaHorda(const DadosDefesa& d, int numero, float corrupcaoFloresta);
// Criaturas da horda: em rodízio da mais forte para a mais fraca, enquanto a força der
std::vector<std::string> composicaoHorda(const DadosDefesa& d, int forca);

// A batalha sem o herdeiro: as defesas contra a horda
struct ResultadoHorda {
    bool vitoria = false;
    int invasores = 0;    // criaturas que chegaram à praça
    int danoPortao = 0;
};
ResultadoHorda resolverSemHeroi(int defesa, int forca, int vidaPortao);

}
