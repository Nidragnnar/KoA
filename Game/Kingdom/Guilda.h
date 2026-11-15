#pragma once
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json_fwd.hpp>

#include "Game/Inventory/Inventory.h"

// Fase 14: a Guilda dos Aventureiros, expedições e relíquias da antiga Valdren (Data/Kingdom/guilda.json)
namespace koa {

struct AventureiroDef {
    std::string id, nome, descricao;
    int forca = 1;
    std::string especialidade;  // combate | exploracao | saber
};

struct DestinoDef {
    std::string id, nome, descricao, tipo, requer;
    int dias = 2, perigo = 1;
    std::map<std::string, std::pair<int, int>> ganhos;  // recurso → [mín, máx]
    std::vector<std::pair<std::string, float>> reliquias;  // relíquia → chance
};

// Efeitos de uma relíquia exposta (ou da maldição dela)
struct EfeitosReliquia {
    float felicidade = 0.0f, comida = 0.0f, luaSangue = 1.0f;
    int ouroDia = 0, seguranca = 0, dano = 0, defesa = 0;
};

struct ReliquiaDef {
    std::string id, nome, texto;
    bool amaldicoada = false;
    EfeitosReliquia efeitos, maldicao;
};

struct DadosGuilda {
    float felicidadeMin = 55.0f;
    int populacaoMin = 14, grupoMax = 3, comidaPorDia = 2;
    std::vector<int> diasFerido{3, 4, 5};  // por gravidade: margem 0, a duras penas, desastre
    float chanceVolta = 0.4f;
    int voltaMin = 6, voltaMax = 9;
    std::vector<AventureiroDef> aventureiros;
    std::vector<DestinoDef> destinos;
    std::vector<ReliquiaDef> reliquias;

    const AventureiroDef* aventureiro(const std::string& id) const;
    const DestinoDef* destino(const std::string& id) const;
    const ReliquiaDef* reliquia(const std::string& id) const;
};

bool lerGuilda(const nlohmann::json& j, DadosGuilda& d, std::string* erro);

// ---------- Estado (vai para o save) ----------

struct EstadoAventureiro {
    int nivel = 1;          // sobe a cada expedição que dá certo (até 3)
    int feridoAte = 0;      // dia em que volta a estar pronto
    bool fora = false;      // em expedição
    bool sumido = false;    // desapareceu numa expedição
    int voltaDia = 0;       // sumido que reaparece neste dia (0 = não volta)
};

struct Expedicao {
    std::string destino;
    std::vector<std::string> grupo;
    int partida = 0, volta = 0;
};

struct EstadoReliquia {
    std::string id;
    bool exposta = false;
    bool conhecida = false;   // a maldição já se revelou (ou foi examinada)
    bool purificada = false;
};

// "+3 de felicidade, +2 de defesa" (o painel e os avisos mostram assim)
std::string textoEfeitos(const EfeitosReliquia& e);

// ---------- Regras ----------

enum class Desfecho { Triunfo, Sucesso, DurasPenas, Desastre };
const char* nomeDesfecho(Desfecho d);

// Poder de um aventureiro naquele destino: força + nível - 1 + 1 se for da especialidade
int poderAventureiro(const AventureiroDef& a, int nivel, const DestinoDef& d);
// A chance em palavras (sem a sorte): "Boas chances", "Arriscado", "Muito arriscado", "Suicida"
const char* chanceEmPalavras(int margemSemSorte);
Desfecho desfechoPorMargem(int margem);
// Sorte da expedição (-2..+2), fixa pelo destino e pelo dia da partida (o mesmo plano dá o mesmo resultado)
int sorteExpedicao(const std::string& destino, int partida);

}
