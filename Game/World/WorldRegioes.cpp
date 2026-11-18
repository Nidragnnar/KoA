// Parte do World: regiões vivas, transformações e chefes com escolha (fase 11)
#include "Game/World/WorldInterno.h"

namespace koa {

using namespace interno;

// ---------- Fase 11: regiões vivas, transformações e chefes com escolha ----------

const TransformacaoRegiao* World::transformacao(const std::string& id) const {
    const auto it = regioes.find(id);
    if (it == regioes.end()) return nullptr;
    for (const auto& t : it->second.vida.transformacoes)
        if (marcos.contains(t.marco)) return &t;
    return nullptr;
}

std::string World::nomeRegiao(const std::string& id) const {
    if (const TransformacaoRegiao* t = transformacao(id)) return t->nome;
    const auto it = regioes.find(id);
    return it == regioes.end() ? id : it->second.nome;
}

bool World::regiaoHostil() const {
    if (const TransformacaoRegiao* t = transformacao(regiaoAtual)) return t->hostil;
    return infoAtual().hostil;
}

std::vector<std::string> World::regioesComVida() const {
    std::vector<std::string> v;
    for (const auto& [id, e] : estadoRegioes) v.push_back(id);
    return v;
}

ModificadorDia World::modificadorReino(Estacao e, int dia) const {
    ModificadorDia m = dados().estacoes.de(e).mod;
    for (const auto& id : regioesComVida())
        if (const TransformacaoRegiao* t = transformacao(id)) m.felicidade += t->felicidade;  // o povo sente o pacto
    // Fase 12: o que o conselho rende e as leis que valem
    for (const CargoDef& c : dados().conselho.cargos) {
        const float v = efeitoCargo(c.id);
        if (c.efeito == "seguranca") m.seguranca += static_cast<int>(std::round(v));
        else if (c.efeito == "impostos") m.impostos *= 1.0f + v;
        else if (c.efeito == "comida") m.comida *= 1.0f + v;
        else if (c.efeito == "obras") m.madeiraDia += static_cast<int>(std::round(v)), m.pedraDia += static_cast<int>(std::round(v));
    }
    for (const auto& id : leis)
        if (const auto it = dados().conselho.leis.find(id); it != dados().conselho.leis.end()) {
            m.felicidade += it->second.felicidade;
            m.ouroDia += it->second.ouroDia;
            m.seguranca += it->second.seguranca;
        }
    // Fase 14: relíquias expostas na guilda (e as maldições que ainda valem)
    const EfeitosReliquia r = efeitosReliquias();
    m.felicidade += r.felicidade;
    m.ouroDia += r.ouroDia;
    m.seguranca += r.seguranca;
    m.comida *= std::max(0.0f, 1.0f + r.comida);
    // Fase 16: as alas do castelo
    const EfeitosAla a = efeitosAlas();
    m.felicidade += a.felicidade;
    m.seguranca += a.seguranca;
    m.impostos *= 1.0f + a.impostos;
    m.comida *= 1.0f + a.comida;
    m.viajantes += a.viajantes;
    m.ouroDia += a.ouroDia;
    m.diasCeleiro += a.diasCeleiro;
    // Fase 18: o que o povo rende pelas profissões (e a alegria dos desejos cumpridos) e o tempo do dia
    const EfeitosPovo p = efeitosDoPovo(dados().povo, povo);
    m.madeiraDia += static_cast<int>(p.madeiraDia);
    m.pedraDia += static_cast<int>(p.pedraDia);
    m.comidaDia += static_cast<int>(p.comidaDia);
    m.ouroDia += static_cast<int>(p.ouroDia);
    m.seguranca += static_cast<int>(p.seguranca);
    m.felicidade += p.felicidade + tempoDoDia(dia < 0 ? relogio.dia : dia).felicidade;
    return m;
}

void World::noiteDasRegioes(std::vector<std::pair<std::string, int>>& producao, std::vector<std::string>& notas) {
    for (auto& [id, e] : estadoRegioes) {
        if (!visitadas.contains(id)) continue;  // terra que o herdeiro ainda não pisou fica como está
        const std::string nome = nomeRegiao(id);
        const ResultadoNoite r = noiteDaRegiao(e, regioes.at(id).vida, transformacao(id), relogio.dia);
        for (const auto& [recurso, n] : r.producao) producao.push_back({recurso + "|" + nome, n});
        if (r.tomada) {
            notas.push_back(nome + ": as criaturas estão tomando conta. Volte lá para expulsá-las.");
            marcar("tomada:" + id, nome);
        }
        if (r.voltaram) notas.push_back(nome + ": sem visitas, a terra voltou a escurecer e as criaturas estão voltando.");
    }
}

void World::receberItem(const std::string& item) {
    const auto it = dados().equipamentos.find(item);
    if (it == dados().equipamentos.end()) return;
    equip.possuidos.insert(item);
    // Equipa na hora se for melhor que o atual
    const EquipDef& d = it->second;
    if (d.tipo == "arma" && d.dano > dados().equipamentos.at(equip.arma).dano) equip.arma = item;
    if (d.tipo == "armadura" && d.defesa > dados().equipamentos.at(equip.armadura).defesa) equip.armadura = item;
}

void World::anunciarTransformacao(const std::string& regiao) {
    eventos_.push_back({EventoJogo::Tipo::Transformou, jogador.pos, regiao, "", 0, nomeRegiao(regiao)});
    // As criaturas que a terra transformada afasta somem na hora (não só na manhã seguinte)
    const TransformacaoRegiao* t = transformacao(regiao);
    const auto est = estadoRegioes.find(regiao);
    if (t && regiao == regiaoAtual && (est == estadoRegioes.end() || est->second.corrupcao < t->voltamAcima)) {
        std::vector<entt::entity> remover;
        for (auto [e, p, i] : reg.view<const Posicao, const Inimigo>().each())
            if (t->semInimigos.contains(i.tipo)) {
                remover.push_back(e);
                eventos_.push_back({EventoJogo::Tipo::Esgotou, p.v, "purificou"});  // somem num sopro de luz
            }
        reg.destroy(remover.begin(), remover.end());
    }
    atualizarEspeciais();
    bloqueio_.clear();
}

bool World::usarCoracao() {
    const std::string marco = "purificou:" + regiaoAtual;
    if (marcos.contains(marco)) {
        eventos_.push_back({EventoJogo::Tipo::Aviso, jogador.pos, "bom", "", 0, "O Coração da Floresta pulsa em paz. A floresta lembra de você."});
        return false;
    }
    const auto it = estadoRegioes.find(regiaoAtual);
    if (it == estadoRegioes.end()) return false;
    EstadoRegiao& e = it->second;
    if (e.corrupcao > regra::PURIFICAR) {
        eventos_.push_back({EventoJogo::Tipo::Aviso, jogador.pos, "", "", 0,
                            std::format("Raízes de cinza sufocam o altar (corrupção {:.0f}). Derrote criaturas aqui até ficar abaixo de {:.0f}.",
                                        e.corrupcao, regra::PURIFICAR)});
        return false;
    }
    marcar(marco, nomeRegiao(regiaoAtual));
    e.corrupcao = 0.0f;
    e.seguranca = 100.0f;
    anunciarTransformacao(regiaoAtual);
    atualizarMissoes();
    return true;
}

bool World::escorarEntulho() {
    if (!inventario.remover("madeira", MADEIRA_ENTULHO)) {
        eventos_.push_back({EventoJogo::Tipo::Aviso, jogador.pos, "", "", 0, std::format("Escorar a passagem pede {} de madeira.", MADEIRA_ENTULHO)});
        return false;
    }
    for (auto [e, p, it] : reg.view<const Posicao, const Interativo>().each())
        if (it.acao == "entulho") eventos_.push_back({EventoJogo::Tipo::Esgotou, p.v, "entulho"});
    eventos_.push_back({EventoJogo::Tipo::Aviso, jogador.pos, "bom", "", 0, "Passagem escorada: o caminho para as minas está aberto."});
    marcar("minas_abertas");  // o entulho some (atualizarEspeciais)
    atualizarMissoes();
    return true;
}

const EscolhaDef* World::escolhaDe(entt::entity e) const {
    const auto* i = reg.try_get<Inimigo>(e);
    if (!i || !i->rendido) return nullptr;
    const auto it = dados().escolhas.find(dados().inimigos.at(i->tipo).escolha);
    return it == dados().escolhas.end() ? nullptr : &it->second;
}

bool World::escolher(const std::string& id, const std::string& opcao) {
    const auto it = dados().escolhas.find(id);
    if (it == dados().escolhas.end()) return false;
    const EscolhaDef& d = it->second;
    const OpcaoEscolha* o = d.opcao(opcao);
    if (!o) return false;
    if (!opcaoDisponivel(*o)) {
        eventos_.push_back({EventoJogo::Tipo::Aviso, jogador.pos, "", "", 0, o->requerTexto.empty() ? "Ainda não é possível." : o->requerTexto});
        return false;
    }
    // O chefe rendido sai de cena: despedaçado, devolvido à luz (vira guardião) ou fundido à Chama
    std::vector<entt::entity> remover;
    glm::vec2 onde = jogador.pos;
    for (auto [e, p, i] : reg.view<const Posicao, const Inimigo>().each())
        if (i.tipo == d.inimigo) {
            onde = p.v;
            remover.push_back(e);
        }
    reg.destroy(remover.begin(), remover.end());
    eventos_.push_back({EventoJogo::Tipo::Esgotou, onde - glm::vec2(0, 10), opcao});
    for (const auto& [r, q] : o->recompensa) {
        inventario.adicionar(r, q);
        eventos_.push_back({EventoJogo::Tipo::Ganhou, onde - glm::vec2(0, 20), "escolha", r, q});
    }
    if (!o->item.empty()) {
        receberItem(o->item);
        eventos_.push_back({EventoJogo::Tipo::Forjou, jogador.pos, o->item, "", 0, dados().equipamentos.at(o->item).nome});
    }
    if (opcao == "derrotar") ++abates[d.inimigo];  // só despedaçar conta como criatura vencida
    if (const auto er = estadoRegioes.find(d.regiao); er != estadoRegioes.end() && o->corrupcao >= 0.0f) {
        er->second.corrupcao = o->corrupcao;
        er->second.seguranca = 100.0f;
    }
    if (!d.inimigo.empty()) marcar("chefe_" + d.inimigo, dados().inimigos.at(d.inimigo).nome);  // não volta mais
    marcar("escolha:" + id + ":" + opcao, o->nome);
    anunciarTransformacao(d.regiao);
    atualizarMissoes();
    return true;
}

void World::atualizarEspeciais() {
    if (!dados_) return;
    std::vector<entt::entity> remover;
    for (auto [e, it] : reg.view<const Interativo>().each())
        if (it.acao == "entulho" && marcos.contains("minas_abertas")) remover.push_back(e);
    for (auto [e] : reg.view<EnfeiteRegiao>().each()) remover.push_back(e);
    if (!remover.empty()) bloqueio_.clear();
    reg.destroy(remover.begin(), remover.end());
    // O altar mostra se a região já foi purificada
    const bool puro = marcos.contains("purificou:" + regiaoAtual);
    for (auto [e, it, v, l] : reg.view<const Interativo, Visual, Luz>().each())
        if (it.acao == "altar") {
            v.sprite = puro ? "altar_purificado" : "altar_corrompido";
            l = puro ? Luz{{0.55f, 1.0f, 0.7f}, 48.0f, {0.0f, -12.0f}} : Luz{{0.8f, 0.25f, 0.2f}, 30.0f, {0.0f, -10.0f}};
        }
    // Enfeites da região transformada (o guardião das minas, flores e cristais)
    if (const TransformacaoRegiao* t = transformacao(regiaoAtual))
        for (const EnfeiteRegiaoDef& en : t->enfeites) {
            const auto e = reg.create();
            reg.emplace<Posicao>(e, baseTile(en.tile.x, en.tile.y));
            reg.emplace<Visual>(e, en.sprite);
            reg.emplace<EnfeiteRegiao>(e);
            if (en.solido) reg.emplace<Colisor>(e, glm::vec2{10, 6}, glm::vec2{0, -6});
            if (en.temLuz) reg.emplace<Luz>(e, en.corLuz, en.raioLuz, glm::vec2{0.0f, -10.0f});
            bloqueio_.clear();
        }
}

}
