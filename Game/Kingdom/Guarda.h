#pragma once
#include <map>
#include <string>
#include <vector>

#include <nlohmann/json_fwd.hpp>

#include "Game/Inventory/Inventory.h"

// Guarda Real (versão 1.15): esquadras formadas no Quartel Real, com postos fora do castelo (Data/Kingdom/guarda.json).
// Militar sem microgestão: no máximo 3 esquadras, nada de mapa de batalha; as lutas se resolvem por uma conta mostrada
namespace koa {

struct TipoEsquadra {
    std::string id, nome, descricao;
    float forca = 1.5f;                  // por soldado
    float regiao = 1.0f, escolta = 1.0f;  // multiplicadores no posto em que o tipo é melhor
    Custo custo;
    std::map<std::string, float> soldo;  // por soldado, por noite
    int requerForja = 0;
};

struct DadosGuarda {
    std::string requerAla = "quartel";
    int esquadrasMax = 3, soldadosMax = 8, nivelMax = 3;
    Custo reforco, treino;  // reforço: por soldado; treino: × (nível + 1)
    float limpeza = 0.08f, seguranca = 0.5f, ameaca = 0.45f, chanceLuta = 0.6f, escoltaCaravana = 0.4f, protecaoProsperidade = 1.5f;
    std::vector<std::string> nomes;
    std::vector<TipoEsquadra> tipos;

    const TipoEsquadra* tipo(const std::string& id) const;
};

bool lerGuarda(const nlohmann::json& j, DadosGuarda& d, std::string* erro);

// Uma esquadra (o que vai para o save). Posto: "reserva", "regiao:<id>", "escolta" ou "aldeia:<id>"
struct Esquadra {
    int id = 0;
    std::string tipo, nome, posto = "reserva";
    int soldados = 0, nivel = 0;
};

// Os modificadores que vêm de fora da esquadra
struct BonusGuarda {
    int nivelForja = 0;
    float marechal = 0.0f;  // +0,15 leal, -0,15 desleal
    bool heroiPresente = false;
};

float forcaEsquadra(const DadosGuarda& d, const Esquadra& e, const BonusGuarda& b);
// Soldo de uma noite (somado de todas)
Custo soldoDaNoite(const DadosGuarda& d, const std::vector<Esquadra>& esquadras);

// A luta numa região: a esquadra contra a ameaça da corrupção (a conta vai para o relatório)
struct ResultadoLuta {
    float forca = 0.0f, ameaca = 0.0f;
    bool vitoria = false;
    int perdas = 0;
};
ResultadoLuta lutaNaRegiao(const DadosGuarda& d, float forca, float corrupcao);

}
