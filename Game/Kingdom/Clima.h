#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include <glm/vec3.hpp>
#include <nlohmann/json_fwd.hpp>

#include "Game/Kingdom/Clock.h"

// Fase 18: o tempo de cada dia (Data/World/clima.json), sorteado de forma fixa pelo dia e pela sorte da partida
namespace koa {

struct TempoDef {
    std::string id, nome, aviso;
    bool rega = false, raios = false;
    float felicidade = 0.0f, velocidade = 1.0f, forcaHorda = 1.0f, nevoa = 0.0f;
    glm::vec3 luz{1.0f};
    int particulas = 0;
};

struct DadosClima {
    std::map<Estacao, std::vector<std::pair<std::string, int>>> pesos;  // estação → (tempo, peso)
    std::vector<TempoDef> tempos;

    const TempoDef* tempo(const std::string& id) const;
};

bool lerClima(const nlohmann::json& j, DadosClima& d, std::string* erro);

// O tempo do dia: o mesmo dia da mesma partida sempre dá o mesmo tempo (nada disso vai para o save)
const TempoDef& tempoDoDia(const DadosClima& d, int dia, uint32_t semente);

}
