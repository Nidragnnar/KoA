#pragma once
#include <set>
#include <string>
#include <vector>

#include "Game/Buildings/Building.h"

namespace koa {

enum class KingdomEra { DarkAge, Restoration, Prosperity, HighFantasy };

const char* nomeEra(KingdomEra e);
// Limiares provisórios de desenvolvimento (soma das melhorias feitas) para cada era
KingdomEra eraPorDesenvolvimento(int desenvolvimento);

// Estado do reino: construções e a era visual (que muda devagar, nunca de uma vez)
struct Kingdom {
    std::vector<BuildingData> construcoes;
    float eraVisual = 0.0f;  // 0..3 contínuo, persegue a era-alvo
    int eraForcada = -1;     // ferramenta de depuração: -1 = automática
    bool magiaLiberta = false;  // o Cavaleiro Caído foi derrotado: a Alta Fantasia fica possível

    int desenvolvimento() const;
    std::set<std::string> defsEmRuinas;  // fase 17: vem dos dados (construções com "ruinas": true)
    int eraConquistada = 0;              // fase 18: a maior era já alcançada (vem dos marcos era:N)
    KingdomEra eraAlvo() const;
    KingdomEra eraAtual() const { return static_cast<KingdomEra>(static_cast<int>(eraVisual + 0.5f)); }
    void atualizarEra(float dt);

    BuildingData* construcao(const std::string& id);
    const BuildingData* construcao(const std::string& id) const;
};

}
