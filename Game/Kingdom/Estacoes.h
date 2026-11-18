#pragma once
#include <array>
#include <string>
#include <vector>

#include <glm/vec2.hpp>
#include <nlohmann/json_fwd.hpp>

#include "Game/Kingdom/Clock.h"

// Fase 10: estações que mudam a economia, acontecimentos raros e o Festival da Colheita
namespace koa {

// O que muda na noite do reino (vem da estação)
struct ModificadorDia {
    float comida = 1.0f;      // produção das hortas
    float consumo = 1.0f;     // quanto cada morador come
    float ouro = 1.0f;        // ouro das construções (comércio)
    float felicidade = 0.0f;  // somado ao alvo de felicidade
    int viajantes = 0;        // famílias a mais quando o povo está feliz e há casa
    // Fase 12: conselho e leis
    float impostos = 1.0f;    // multiplica os impostos (Mestre da Moeda)
    int seguranca = 0;        // somado à segurança (Marechal, patrulha)
    int ouroDia = 0;          // ouro a mais (ou a menos) por noite (leis)
    int madeiraDia = 0, pedraDia = 0;  // Mestre das Obras
    // Fase 17: quantos dias de consumo cabem no celeiro (o que passar disso começa a estragar)
    float diasCeleiro = 12.0f;
    int comidaDia = 0;  // fase 18: comida que o povo traz (lavradores, pescadores, pastores)
};

struct EstacaoDef {
    std::string id, nome;
    ModificadorDia mod;
    float inimigos = 1.0f;   // vida e dano das criaturas
    int mercadorACada = 0;   // 0 = o intervalo normal do mercador
    std::string resumo;      // linha do painel K ("hortas 40%, criaturas +30%")
    std::string chegada;     // nota no relatório do primeiro dia
};

// Acontecimento raro de uma noite (lua de sangue, aurora, meteoros)
struct EventoDef {
    std::string id, nome;
    std::string anuncio;  // relatório da manhã: o que os velhos dizem
    std::string aviso;    // quando começa, às 19:00
    float chance = 0.0f;  // por dia
    std::string requer;   // marco necessário ("" = nenhum)
    int eraMin = 0;
    float inimigos = 1.0f;       // dano das criaturas durante a noite
    float ouroInimigos = 1.0f;   // ouro que elas deixam durante a noite
    float felicidade = 0.0f;     // na manhã seguinte
    int ouroMeteorito = 0;       // > 0: cai uma estrela perto do vilarejo
};

struct FestivalDef {
    std::string nome = "Festival da Colheita";
    Estacao estacao = Estacao::Outono;
    int diaDaEstacao = 7;
    int inicio = 17 * 60, fim = 23 * 60;
    float felicidade = 10.0f;          // na manhã seguinte
    std::vector<glm::ivec2> lugares;   // onde os moradores se reúnem
};

struct DadosEstacoes {
    std::array<EstacaoDef, 4> estacoes;
    std::vector<EventoDef> eventos;
    FestivalDef festival;
    std::vector<glm::ivec2> meteoritos;  // onde uma estrela pode cair
    int diasSemEventos = 3;              // os primeiros dias são calmos

    const EstacaoDef& de(Estacao e) const { return estacoes[static_cast<int>(e)]; }
};

bool lerEstacoes(const nlohmann::json& j, DadosEstacoes& d, std::string* erro);

}
