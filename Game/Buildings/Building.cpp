#include "Game/Buildings/Building.h"

#include <format>

#include "Game/Kingdom/Kingdom.h"

namespace koa {

ResultadoMelhoria avaliarMelhoria(const BuildingData& b, const BuildingDef& def, const Inventory& inv) {
    ResultadoMelhoria r;
    if (!b.unlocked) {
        r.motivo = "Ainda bloqueada.";
        return r;
    }
    r.proximo = def.nivel(b.level + 1);
    if (!r.proximo) {
        r.motivo = "Nível máximo alcançado.";
        return r;
    }
    r.custo = r.proximo->custo;
    r.faltando = inv.faltando(r.custo);
    r.possivel = r.faltando.empty();
    if (!r.possivel) r.motivo = "Faltam recursos.";
    return r;
}

bool melhorar(BuildingData& b, const BuildingDef& def, Inventory& inv) {
    const ResultadoMelhoria r = avaliarMelhoria(b, def, inv);
    if (!r.possivel || !inv.pagar(r.custo)) return false;
    b.level = r.proximo->nivel;
    b.name = r.proximo->nome;
    b.constructed = true;
    return true;
}

std::vector<std::string> spritesAparencia(const BuildingDef& def, int nivel, KingdomEra era) {
    return {std::format("{}_n{}_e{}", def.aparencia, nivel, static_cast<int>(era)), std::format("{}_n{}", def.aparencia, nivel)};
}

}
