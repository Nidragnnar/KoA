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

// ---------- Fase 20: ações das alas ----------

int World::diasParaAgir(const std::string& id) const {
    const AlaDef* a = dados().castelo.ala(id);
    const auto it = ultimaAcaoAla.find(id);
    if (!a || !a->acao || it == ultimaAcaoAla.end()) return 0;
    return std::max(0, it->second + a->acao->recarga - relogio.dia);
}

std::string World::podeAgirAla(const std::string& id) const {
    const AlaDef* a = dados().castelo.ala(id);
    if (!a || !a->acao) return "Esta ala não tem o que fazer.";
    if (std::ranges::find(alas, id) == alas.end()) return "Erga a ala primeiro.";
    if (const int d = diasParaAgir(id); d > 0) return std::format("De novo em {} dia{}.", d, d == 1 ? "" : "s");
    if (a->acao->emprestimo > 0 && emprestimoVolta > 0) return "O empréstimo anterior ainda não voltou.";
    if (a->acao->decifrar &&
        std::ranges::none_of(dados().lore.pistas, [&](const PistaDef& p) { return !marcos.contains("pista:" + p.id) && !marcos.contains("decifrou:" + p.id); }))
        return "Não sobrou fragmento para decifrar.";
    if (!inventario.tem(a->acao->custo)) return "Faltam recursos.";
    return "";
}

bool World::agirAla(const std::string& id) {
    const std::string falta = podeAgirAla(id);
    const AlaDef* a = dados().castelo.ala(id);
    const BuildingData* c = reino.construcao("castelo");
    const glm::vec2 onde = c ? pontoPorta(*c) : jogador.pos;
    if (!falta.empty()) {
        eventos_.push_back({EventoJogo::Tipo::Aviso, onde, "", "", 0, falta});
        return false;
    }
    const AcaoAla& x = *a->acao;
    inventario.pagar(x.custo);
    ultimaAcaoAla[id] = relogio.dia;
    std::string texto = x.nome + ".";
    if (x.felicidade != 0.0f) {
        estado.felicidade = std::clamp(estado.felicidade + x.felicidade, 0.0f, 100.0f);
        texto += std::format(" O povo se alegra ({:+.0f}).", x.felicidade);
    }
    if (x.soldadosHorda > 0) {
        guardaTreinada += x.soldadosHorda;
        texto += std::format(" {} soldados a mais na próxima horda.", guardaTreinada);
    }
    if (x.emprestimo > 0) {
        emprestimoValor = x.custo.contains("ouro") ? x.custo.at("ouro") : 0;
        emprestimoVolta = x.emprestimo;
        emprestimoDia = relogio.dia + x.diasEmprestimo;
        texto += std::format(" Volta {} de ouro no dia {}.", emprestimoVolta, emprestimoDia);
    }
    if (x.decifrar) {
        // O primeiro fragmento que ainda falta (na ordem dos dados), de preferência um que já dá para achar
        const PistaDef *alvo = nullptr, *depois = nullptr;
        for (const PistaDef& p : dados().lore.pistas) {
            if (marcos.contains("pista:" + p.id) || marcos.contains("decifrou:" + p.id)) continue;
            if (p.requer.empty() || marcos.contains(p.requer)) {
                alvo = &p;
                break;
            }
            if (!depois) depois = &p;
        }
        if (!alvo) alvo = depois;
        if (alvo) {
            const std::string onde2 = alvo->regiao.empty() ? "nas mãos de um mercador viajante" : "em " + nomeRegiao(alvo->regiao);
            texto += std::format(" Os escribas apontam: \"{}\", {}.", alvo->titulo, onde2);
            marcar("decifrou:" + alvo->id, alvo->titulo, true);
        }
    }
    eventos_.push_back({EventoJogo::Tipo::Aviso, onde, "bom", "", 0, texto});
    marcar("ala_acao:" + x.id, x.nome, true);
    reagirAoMarco("ala_acao:" + x.id);  // os vizinhos e a fama reagem a cada vez (o marco só conta na primeira)
    return true;
}

// O empréstimo volta: com juros se o povo está bem; só o que foi emprestado se anda triste
void World::manhaDasAlas(std::vector<std::string>& notas) {
    if (emprestimoVolta <= 0 || relogio.dia < emprestimoDia) return;
    const int volta = estado.felicidade >= 40.0f ? emprestimoVolta : emprestimoValor;
    inventario.adicionar("ouro", volta);
    ultimoRelatorio.variacoes.push_back({"ouro|empréstimo", volta});
    notas.push_back(volta > emprestimoValor ? std::format("As famílias pagaram o empréstimo: {} de ouro voltaram ao tesouro.", volta)
                                            : std::format("O povo anda triste: o empréstimo voltou sem juros ({} de ouro).", volta));
    emprestimoVolta = emprestimoValor = emprestimoDia = 0;
}
}
