#pragma once
#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include <glm/vec2.hpp>
#include <nlohmann/json_fwd.hpp>

#include "Game/Combat/Combate.h"
#include "Game/Inventory/Inventory.h"
#include "Game/Kingdom/Conselho.h"
#include "Game/Kingdom/Defesa.h"
#include "Game/Kingdom/Guilda.h"
#include "Game/Kingdom/Estacoes.h"
#include "Game/Lore/Lore.h"
#include "Game/NPC/Npc.h"
#include "Game/World/Regioes.h"

namespace koa {

struct RecursoDef {
    std::string id, nome, icone;
};

struct NivelConstrucao {
    int nivel = 1;
    std::string nome, descricao;
    Custo custo;  // para chegar a este nível (o nível 1 é o custo da obra)
    std::map<std::string, float> efeitos;  // populacao_max, felicidade, seguranca, comida_dia, ouro_dia...

    float efeito(const std::string& qual) const {
        const auto it = efeitos.find(qual);
        return it == efeitos.end() ? 0.0f : it->second;
    }
};

// Definição fixa de um tipo de construção (vem de Data/Buildings)
struct BuildingDef {
    std::string id;
    std::string aparencia;            // prefixo dos sprites ("casa" → casa_n1, casa_n2_e3...)
    glm::ivec2 tamanho{4, 3};         // base em tiles (é o que colide)
    bool construivel = false;         // aparece no menu de obras
    int limite = 0;                   // quantas podem existir (0 = sem limite)
    std::vector<NivelConstrucao> niveis;
    // Fase 9: monumentos só liberam depois de um acontecimento (marco)
    std::string requer, requerTexto;
    bool monumento = false;

    const NivelConstrucao* nivel(int n) const;
    int nivelMax() const { return niveis.empty() ? 0 : niveis.back().nivel; }
};

// Todas as definições de dados do jogo
struct GameData {
    std::map<std::string, RecursoDef> recursos;
    std::map<std::string, BuildingDef> construcoes;
    std::vector<std::string> ordemConstrucoes;  // como estão no arquivo (menu de obras)
    std::map<std::string, NpcDef> npcs;
    std::map<std::string, InimigoDef> inimigos;
    std::map<std::string, EquipDef> equipamentos;
    std::vector<std::string> ordemEquipamentos;
    std::vector<MissaoDef> missoes;
    Lore lore;  // Crônicas, títulos e pistas da Queda
    DadosEstacoes estacoes;  // estações, acontecimentos raros e o festival
    std::map<std::string, EscolhaDef> escolhas;  // fase 11: chefes que se rendem
    DadosConselho conselho;                      // fase 12: cargos, leis e decisões políticas
    DadosDefesa defesa;                          // fase 13: hordas e a defesa do portão
    DadosGuilda guilda;                          // fase 14: aventureiros, expedições e relíquias

    const InimigoDef* inimigoPorLetra(char c) const {
        for (const auto& [id, d] : inimigos)
            if (d.letra == c) return &d;
        return nullptr;
    }

    bool carregar(const std::filesystem::path& pastaData, std::string* erro = nullptr);
    bool lerJson(const nlohmann::json& recursos, const nlohmann::json& construcoes, std::string* erro = nullptr);
    std::string nomeRecurso(const std::string& id) const;
};

}
