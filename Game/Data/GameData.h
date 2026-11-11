#pragma once
#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include <glm/vec2.hpp>
#include <nlohmann/json_fwd.hpp>

#include "Game/Inventory/Inventory.h"

namespace koa {

struct RecursoDef {
    std::string id, nome, icone;
};

struct NivelConstrucao {
    int nivel = 1;
    std::string nome, descricao;
    Custo custo;  // para chegar a este nível
};

// Definição fixa de um tipo de construção (vem de Data/Buildings)
struct BuildingDef {
    std::string id;
    std::string aparencia;            // prefixo dos sprites ("casa" → casa_n1, casa_n2_e3...)
    glm::ivec2 tamanho{4, 3};         // base em tiles (é o que colide)
    std::vector<NivelConstrucao> niveis;

    const NivelConstrucao* nivel(int n) const;
    int nivelMax() const { return niveis.empty() ? 0 : niveis.back().nivel; }
};

// Todas as definições de dados do jogo
struct GameData {
    std::map<std::string, RecursoDef> recursos;
    std::map<std::string, BuildingDef> construcoes;

    bool carregar(const std::filesystem::path& pastaData, std::string* erro = nullptr);
    bool lerJson(const nlohmann::json& recursos, const nlohmann::json& construcoes, std::string* erro = nullptr);
    std::string nomeRecurso(const std::string& id) const;
};

}
