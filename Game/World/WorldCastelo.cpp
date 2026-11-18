// Parte do World: o castelo, as alas e o brasão (fase 16)
#include "Game/World/WorldInterno.h"

namespace koa {

using namespace interno;

// ---------- Fase 16: o castelo, as alas e o brasão ----------

int World::nivelCastelo() const {
    const BuildingData* c = reino.construcao("castelo");
    return c && c->constructed ? c->level : 0;
}

int World::vagasAlas() const { return dados().castelo.vagas(nivelCastelo()); }

EfeitosAla World::efeitosAlas() const {
    EfeitosAla e;
    for (const auto& id : alas)
        if (const AlaDef* a = dados().castelo.ala(id)) e += a->efeitos;
    return e;
}

bool World::erguerAla(const std::string& id) {
    const AlaDef* a = dados().castelo.ala(id);
    const BuildingData* c = reino.construcao("castelo");
    if (!a || !c) return false;
    auto aviso = [&](const std::string& t) { eventos_.push_back({EventoJogo::Tipo::Aviso, pontoPorta(*c), "castelo", "", 0, t}); };
    if (std::ranges::find(alas, id) != alas.end()) return aviso(a->nome + " já foi erguida."), false;
    if (static_cast<int>(alas.size()) >= vagasAlas())
        return aviso(vagasAlas() == 0 ? "Reconstrua a Torre de Menagem para erguer alas." : "Sem vaga: reconstrua o castelo para erguer mais alas."), false;
    if (!inventario.pagar(a->custo)) return aviso("Faltam recursos para a " + a->nome + "."), false;
    alas.push_back(id);
    eventos_.push_back({EventoJogo::Tipo::AlaErguida, pontoPorta(*c), id, "", static_cast<int>(alas.size()), a->nome});
    marcar("ala:" + id, a->nome);
    return true;
}

void World::definirBrasao(const Brasao& b) {
    const DadosCastelo& dc = dados().castelo;
    const Brasao novo = dc.validar(b);
    const bool mudou = !brasaoCriado || novo != brasao;
    brasao = novo;
    brasaoCriado = true;
    if (!mudou) return;
    const std::string texto = textoBrasao(dc, brasao);
    eventos_.push_back({EventoJogo::Tipo::BrasaoNovo, jogador.pos, "", "", 0, texto});
    marcar("brasao", texto);  // a primeira vez vira página nas Crônicas
}

std::string World::requisitoProximoNivel(const BuildingData& b) const {
    const BuildingDef* d = def(b);
    const NivelConstrucao* prox = d ? d->nivel(b.level + 1) : nullptr;
    if (!prox || prox->requer.empty() || marcos.contains(prox->requer)) return "";
    return prox->requerTexto.empty() ? "Ainda não é hora de reconstruir." : prox->requerTexto;
}

}
