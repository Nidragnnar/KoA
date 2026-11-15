#include "Game/Kingdom/Kingdom.h"

#include <algorithm>

namespace koa {

const char* nomeEra(KingdomEra e) {
    switch (e) {
    case KingdomEra::DarkAge: return "Reino das Cinzas";
    case KingdomEra::Restoration: return "O Despertar da Coroa";
    case KingdomEra::Prosperity: return "Reino Renascido";
    case KingdomEra::HighFantasy: return "Reino da Alta Fantasia";
    }
    return "?";
}

KingdomEra eraPorDesenvolvimento(int d) {
    if (d >= 40) return KingdomEra::HighFantasy;
    if (d >= 22) return KingdomEra::Prosperity;
    if (d >= 8) return KingdomEra::Restoration;
    return KingdomEra::DarkAge;
}

// Soma dos níveis de tudo que está de pé (a cabana inicial não conta)
int Kingdom::desenvolvimento() const {
    int d = 0;
    for (const auto& c : construcoes)
        if (c.constructed) d += c.level - (c.defId == "muralha" ? 1 : 0);  // a muralha começa em ruínas (nível 1 não conta)
    return std::max(0, d - 1);
}

KingdomEra Kingdom::eraAlvo() const {
    if (eraForcada >= 0) return static_cast<KingdomEra>(std::clamp(eraForcada, 0, 3));
    const KingdomEra e = eraPorDesenvolvimento(desenvolvimento());
    // A magia só volta de vez quando o Cavaleiro Caído é derrotado na cripta
    if (e == KingdomEra::HighFantasy && !magiaLiberta) return KingdomEra::Prosperity;
    return e;
}

void Kingdom::atualizarEra(float dt) {
    // Transição gradual: cerca de 8 segundos por era
    const float alvo = static_cast<float>(eraAlvo());
    const float passo = dt / 8.0f;
    if (eraVisual < alvo) eraVisual = std::min(alvo, eraVisual + passo);
    else eraVisual = std::max(alvo, eraVisual - passo);
}

BuildingData* Kingdom::construcao(const std::string& id) {
    for (auto& c : construcoes)
        if (c.id == id) return &c;
    return nullptr;
}

const BuildingData* Kingdom::construcao(const std::string& id) const {
    for (const auto& c : construcoes)
        if (c.id == id) return &c;
    return nullptr;
}

}
