#pragma once
#include <map>
#include <set>
#include <string>
#include <vector>

#include <glm/vec2.hpp>
#include <nlohmann/json_fwd.hpp>

#include "Game/Inventory/Inventory.h"

// Fase 9: Crônicas do Reino, títulos do herdeiro e o mistério da Queda das Cinzas
namespace koa {

// Uma página das Crônicas: o que aconteceu, em que dia e em que era
struct EntradaCronica {
    std::string marco;
    int dia = 0;  // 0 = antes das Crônicas (veio de um save antigo)
    int era = 0;
    std::string texto;
    int reinado = 1;  // fase 21: em que reinado aconteceu (as Crônicas se dividem por reinado)
};

// O que é preciso para ganhar um título (vale tudo o que estiver preenchido)
struct RequisitoTitulo {
    std::vector<std::string> marcos;
    int construcoesMin = 0;  // obras prontas, sem contar a casa
    int tiposMin = 0;        // tipos diferentes de obra prontos
    int abatesMin = 0;       // criaturas derrotadas, de todos os tipos
    int populacaoMin = 0;
    int eraMin = 0;
    int pistasMin = 0;       // fragmentos da Queda encontrados
};

struct TituloDef {
    std::string id, nome, descricao;
    RequisitoTitulo requer;
};

// Fragmento da Queda das Cinzas: fica no mapa (ou vem do mercador, sem região)
struct PistaDef {
    std::string id, titulo;
    std::string regiao;  // "" = não fica no mapa
    glm::ivec2 tile{0};
    std::string requer;  // marco para aparecer ("" = desde o começo)
    std::vector<std::string> texto;
};

// O que o mercador viajante vende
struct ItemLoja {
    std::string id, nome;
    Custo custo, ganho;
    std::string pista;  // compra que revela um fragmento (só uma vez)
};

// Situação do reino para conferir os títulos
struct SituacaoTitulos {
    const std::set<std::string>* marcos = nullptr;
    int construcoes = 0, tipos = 0, abates = 0, populacao = 0, era = 0, pistas = 0;
};
bool cumpre(const RequisitoTitulo& r, const SituacaoTitulos& s);

struct Lore {
    std::map<std::string, std::string> paginas;  // marco → texto da página ({nome} vira o nome do acontecimento)
    std::vector<TituloDef> titulos;              // do mais humilde ao mais alto
    std::vector<PistaDef> pistas;

    const PistaDef* pista(const std::string& id) const;
    const TituloDef* titulo(const std::string& id) const;
};

bool lerLore(const nlohmann::json& cronicas, const nlohmann::json& titulos, const nlohmann::json& pistas, Lore& saida, std::string* erro);
std::vector<ItemLoja> lerLoja(const nlohmann::json& j);
// Troca cada {chave} pelo valor
std::string preencher(std::string texto, const std::map<std::string, std::string>& valores);

}
