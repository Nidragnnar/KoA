#pragma once
#include <string>
#include <vector>

#include <glm/vec2.hpp>

#include "Game/Data/GameData.h"
#include "Game/Inventory/Inventory.h"

namespace koa {

enum class KingdomEra;

// Estado de uma construção no mapa (os dados fixos ficam em BuildingDef)
struct BuildingData {
    std::string id;     // instância ("casa_herdeiro")
    std::string defId;  // definição ("casa")
    std::string name;   // nome do nível atual
    int level = 1;
    glm::ivec2 tile{0};  // canto superior esquerdo da base, em tiles
    bool unlocked = true;
    bool constructed = true;
};

struct ResultadoMelhoria {
    bool possivel = false;
    std::string motivo;  // por que não (texto para o jogador)
    Custo custo;         // custo do próximo nível
    Custo faltando;
    const NivelConstrucao* proximo = nullptr;
};

ResultadoMelhoria avaliarMelhoria(const BuildingData& b, const BuildingDef& def, const Inventory& inv);
// Paga e sobe um nível; falha sem mexer em nada se não puder
bool melhorar(BuildingData& b, const BuildingDef& def, Inventory& inv);

// Sprites candidatos para a aparência, do mais específico ao genérico:
// a mesma construção pode ganhar visual próprio em cada era sem mudar a lógica
std::vector<std::string> spritesAparencia(const BuildingDef& def, int nivel, KingdomEra era);

}
