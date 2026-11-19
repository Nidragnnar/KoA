// Parte do World: o Conselho Real e as decisões políticas (fase 12)
#include "Game/World/WorldInterno.h"

namespace koa {

using namespace interno;

// ---------- Fase 12: o Conselho Real e as decisões políticas ----------

std::string World::ocupante(const std::string& cargo) const {
    const auto it = conselho.find(cargo);
    return it == conselho.end() ? "" : it->second;
}

std::string World::cargoDe(const std::string& npc) const {
    for (const auto& [cargo, quem] : conselho)
        if (quem == npc) return cargo;
    return "";
}

std::vector<std::string> World::candidatos() const {
    std::vector<std::string> v;
    for (const auto& [id, d] : dados().npcs)
        if (!d.visitante()) v.push_back(id);
    return v;
}

Relacionamento* World::relacao(const std::string& npc) {
    for (auto [e, m, rel] : reg.view<const Morador, Relacionamento>().each())
        if (m.id == npc) return &rel;
    for (auto& [id, g] : guardadas_)
        for (auto [e, m, rel] : g.reg.view<const Morador, Relacionamento>().each())
            if (m.id == npc) return &rel;
    return nullptr;
}

bool World::nomear(const std::string& cargo, const std::string& npc) {
    const CargoDef* c = dados().conselho.cargo(cargo);
    if (!conselhoFormado() || !c) return false;
    if (!npc.empty() && (!dados().npcs.contains(npc) || dados().npcs.at(npc).visitante())) return false;
    const std::string antes = ocupante(cargo);
    if (antes == npc) return true;
    // Quem perde o cargo leva a mágoa junto
    if (!antes.empty()) {
        lealdade[antes] = std::max(0, lealdade[antes] - regra::LEALDADE_DESTITUIDO);
        conselho.erase(cargo);
    }
    if (npc.empty()) return true;
    // Já servia em outro cargo: é transferido (sem mágoa)
    if (const std::string outro = cargoDe(npc); !outro.empty()) conselho.erase(outro);
    if (!lealdade.contains(npc)) {
        const Relacionamento* rel = relacao(npc);
        lealdade[npc] = lealdadeInicial(rel ? rel->afinidade : 0);
    }
    conselho[cargo] = npc;
    const std::string nome = dados().npcs.at(npc).nome;
    eventos_.push_back({EventoJogo::Tipo::Nomeou, jogador.pos, cargo, npc, 0, nome + " agora é " + c->nome + "."});
    marcar("cargo:" + cargo + ":" + npc, nome + ", " + c->nome);
    return true;
}

float World::efeitoCargo(const std::string& cargo) const {
    const CargoDef* c = dados().conselho.cargo(cargo);
    const std::string npc = ocupante(cargo);
    if (!c || npc.empty()) return 0.0f;
    const auto it = lealdade.find(npc);
    return efeitoNoCargo(*c, npc, it == lealdade.end() ? 0 : it->second);
}

bool World::decisaoPossivel(const DecisaoDef& d) const {
    if (d.manual) return false;
    const RequisitoDecisao& r = d.requer;
    if (estado.populacao < r.populacaoMin || static_cast<int>(reino.eraAlvo()) < r.eraMin) return false;
    if (r.impostoMin >= 0 && static_cast<int>(estado.imposto) < r.impostoMin) return false;
    if (!r.marco.empty() && !marcos.contains(r.marco)) return false;
    if (!r.semMarco.empty() && marcos.contains(r.semMarco)) return false;
    if (!r.semLei.empty() && leis.contains(r.semLei)) return false;
    if (d.repetir) return true;
    // Caso já decidido não volta (o marco é "decisao:<id>:<opção>")
    const std::string prefixo = "decisao:" + d.id + ":";
    const auto it = marcos.lower_bound(prefixo);
    return it == marcos.end() || !it->starts_with(prefixo);
}

void World::trazerDecisao(const std::string& id) {
    if (!dados().conselho.decisao(id)) return;
    decisaoPendente = id;
    ultimaDecisao = relogio.dia;
}

void World::noiteDoConselho(std::vector<std::string>& notas) {
    const DadosConselho& dc = dados().conselho;
    if (!conselhoFormado()) {
        if (contarObrasProntas(false) >= dc.obrasParaFormar) {
            marcar("conselho_formado");
            ultimaDecisao = relogio.dia;  // o primeiro caso vem em alguns dias
            notas.push_back("O povo pede um Conselho Real. Aperte G para nomear os conselheiros.");
        }
        return;
    }
    // Povo feliz deixa o conselho de bom humor; povo infeliz, de mau
    for (const auto& [cargo, npc] : conselho) {
        int& l = lealdade[npc];
        const int antes = l;
        if (estado.felicidade >= 60.0f) l += 1;
        else if (estado.felicidade < 30.0f) l -= 2;
        l = std::clamp(l, 0, 100);
        if (antes >= regra::LEALDADE_INSATISFEITO && l < regra::LEALDADE_INSATISFEITO)
            notas.push_back(std::format("{} ({}) está descontente e faz só o mínimo.", dados().npcs.at(npc).nome, dc.cargo(cargo)->nome));
    }
    // Um caso novo a cada poucos dias (a ordem varia com o dia; casos já decididos não voltam)
    ultimaDecisao = std::min(ultimaDecisao, relogio.dia);  // o calendário voltou (depuração): conta de hoje
    if (!decisaoPendente.empty() || relogio.dia - ultimaDecisao < dc.diasEntreDecisoes || dc.decisoes.empty()) return;
    const size_t n = dc.decisoes.size();
    for (size_t k = 0; k < n; ++k) {
        const DecisaoDef& d = dc.decisoes[(hashTile(relogio.dia, 101) + k) % n];
        if (!decisaoPossivel(d)) continue;
        trazerDecisao(d.id);
        notas.push_back("O conselho pede sua decisão: " + d.titulo + ".");
        return;
    }
}

bool World::decidir(const std::string& opcao) {
    const DecisaoDef* d = decisaoAtual();
    const OpcaoDecisao* o = d ? d->opcao(opcao) : nullptr;
    if (!o) return false;
    const EfeitosDecisao& e = o->efeitos;
    for (const auto& [r, q] : e.recursos) {
        if (q >= 0) inventario.adicionar(r, q);
        else inventario.remover(r, std::min(-q, inventario.quantidade(r)));  // gasta até o que houver
    }
    estado.felicidade = std::clamp(estado.felicidade + e.felicidade, 0.0f, 100.0f);
    if (e.populacao != 0) {
        estado.populacao = std::max(3, estado.populacao + e.populacao);
        if (noVilarejo()) sincronizarAldeoes();
    }
    // O conselho: quem foi ouvido fica mais leal; quem foi contrariado, menos
    for (const auto& [cargo, npc] : conselho) {
        int& l = lealdade[npc];
        if (const auto it = d->conselho.find(cargo); it != d->conselho.end())
            l += it->second.first == opcao ? regra::LEALDADE_APOIADO : -regra::LEALDADE_CONTRARIADO;
        if (const auto it = e.lealdade.find(cargo); it != e.lealdade.end()) l += it->second;
        l = std::clamp(l, 0, 100);
    }
    for (const auto& [npc, n] : e.amizade)
        if (Relacionamento* rel = relacao(npc)) rel->afinidade = std::clamp(rel->afinidade + n, 0, 1000);
    for (const auto& [regiao, n] : e.corrupcao)
        if (const auto it = estadoRegioes.find(regiao); it != estadoRegioes.end())
            it->second.corrupcao = std::clamp(it->second.corrupcao + n, 0.0f, 100.0f);
    if (!e.lei.empty()) leis.insert(e.lei);
    if (!e.revogar.empty()) leis.erase(e.revogar);
    if (e.imposto >= 0) estado.imposto = static_cast<Imposto>(e.imposto);
    const std::string id = d->id, nome = o->nome;
    if (id == "desvio") resolverDesvio(opcao);  // fase 19: tirar do cargo, perdoar ou fingir que não viu
    decisaoPendente.clear();  // o caso está resolvido antes de virar página nas Crônicas
    marcar("decisao:" + id + ":" + opcao, nome);
    eventos_.push_back({EventoJogo::Tipo::Decidiu, jogador.pos, id, opcao, 0, nome});
    return true;
}

}
