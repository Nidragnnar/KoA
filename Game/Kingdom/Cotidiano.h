#pragma once
#include <map>
#include <set>
#include <string>
#include <vector>

#include <glm/vec2.hpp>
#include <nlohmann/json_fwd.hpp>

#include "Game/Inventory/Inventory.h"
#include "Game/Kingdom/Clock.h"

// Fase 15: vida cotidiana (Data/World/vida.json): a horta do herdeiro, pesca, animais, cozinha e presentes
namespace koa {

struct CulturaDef {
    std::string id, nome, semente;
    int dias = 2, rendimento = 2;
    std::set<Estacao> estacoes;  // em que estações cresce
};

struct PeixeDef {
    std::string id;
    float chance = 1.0f;              // tentada em ordem: o primeiro que passar é o pescado
    std::set<std::string> regioes;
    std::set<Estacao> estacoes;       // vazio = o ano todo
};

struct EfeitosPrato {
    int vida = 0, dano = 0, defesa = 0;  // dano e defesa valem até dormir
    float felicidade = 0.0f;             // servido ao povo
};

struct ReceitaDef {
    std::string id, nome, descricao;
    Custo ingredientes;
    EfeitosPrato efeitos;
};

struct AnimalDef {
    std::string animal, produto;
};

struct DadosCotidiano {
    std::vector<glm::ivec2> canteiros;
    std::vector<CulturaDef> culturas;
    float esperaMin = 1.5f, esperaMax = 4.0f, janela = 0.8f;
    std::vector<PeixeDef> peixes;
    std::map<std::string, AnimalDef> animais;  // construção → animal e produto
    std::vector<ReceitaDef> receitas;
    std::map<std::string, std::vector<std::string>> presentes;  // morador → favoritos
    std::map<std::string, std::string> itens;                    // nomes para a interface

    const CulturaDef* cultura(const std::string& id) const;
    const ReceitaDef* receita(const std::string& id) const;
};

bool lerCotidiano(const nlohmann::json& j, DadosCotidiano& d, std::string* erro);

// O peixe que morde: em ordem, o primeiro cuja chance passar (sorte de 0 a 1, um sorteio por tentativa)
const PeixeDef* sortearPeixe(const DadosCotidiano& d, const std::string& regiao, Estacao e, const std::vector<float>& sortes);
// "+40 de vida, +3 de dano até dormir"
std::string textoPrato(const EfeitosPrato& e);

// ---------- Estado (vai para o save) ----------

struct EstadoCanteiro {
    std::string cultura;  // "" = terra vazia
    int crescidos = 0;    // dias que já cresceu
    int regadoDia = 0;    // dia da última rega
    bool maduro = false;
};

}
