// Parte do World: missões e moradores (rotinas, conversas, amizade)
#include "Game/World/WorldInterno.h"

namespace koa {

using namespace interno;

// ---------- Missões ----------

int World::progressoMissao(const MissaoDef& m) const {
    const ObjetivoMissao& o = m.objetivo;
    const auto it = missoes.find(m.id);
    if (o.tipo == "derrotar") {
        const auto a = abates.find(o.alvo);
        return (a == abates.end() ? 0 : a->second) - (it == missoes.end() ? 0 : it->second.base);
    }
    if (o.tipo == "construir") {
        int n = 0;
        for (const auto& b : reino.construcoes) n += b.defId == o.alvo && b.constructed;
        return n;
    }
    if (o.tipo == "ter") return inventario.quantidade(o.alvo);
    if (o.tipo == "visitar") return visitadas.contains(o.alvo) ? 1 : 0;
    if (o.tipo == "marco") return marcos.contains(o.alvo) ? 1 : 0;  // fase 11: purificar, abrir as minas...
    return 0;
}

std::vector<const MissaoDef*> World::missoesAtivas() const {
    std::vector<const MissaoDef*> v;
    for (const MissaoDef& m : dados().missoes)
        if (const auto it = missoes.find(m.id); it != missoes.end() && !it->second.concluida) v.push_back(&m);
    return v;
}

void World::iniciarMissao(const MissaoDef& m) {
    EstadoMissao e;
    if (m.objetivo.tipo == "derrotar") {
        const auto a = abates.find(m.objetivo.alvo);
        e.base = a == abates.end() ? 0 : a->second;
    }
    missoes[m.id] = e;
    eventos_.push_back({EventoJogo::Tipo::MissaoNova, jogador.pos, m.id, "", 0, m.nome});
    atualizarMissoes();  // pode já estar cumprida (ex.: a forja já existe)
}

void World::atualizarMissoes() {
    for (const MissaoDef& m : dados().missoes) {
        auto it = missoes.find(m.id);
        if (it == missoes.end() || it->second.concluida) continue;
        if (progressoMissao(m) < m.objetivo.quantidade) continue;
        it->second.concluida = true;
        for (const auto& [r, q] : m.recompensa) inventario.adicionar(r, q);
        if (!m.item.empty()) receberItem(m.item);
        if (m.amizade > 0) {
            // O morador pode estar em outra região (guardada): procura em todas
            auto dar = [&](entt::registry& r) {
                for (auto [e, mo, rel] : r.view<const Morador, Relacionamento>().each())
                    if (mo.id == m.npc) rel.afinidade = std::min(1000, rel.afinidade + m.amizade);
            };
            dar(reg);
            for (auto& [id, g] : guardadas_) dar(g.reg);
        }
        eventos_.push_back({EventoJogo::Tipo::MissaoConcluida, jogador.pos, m.id, m.item, 0, m.nome});
        marcar("missao:" + m.id, m.nome);
    }
}

// ---------- Moradores ----------

void World::criarMoradores() {
    int i = 0;
    for (const auto& [id, def] : dados().npcs) {
        const auto e = reg.create();
        reg.emplace<Posicao>(e);
        reg.emplace<Morador>(e, id, i++);
        reg.emplace<Movimento>(e);
        reg.emplace<Agenda>(e);
        reg.emplace<Relacionamento>(e);
    }
}

entt::entity World::morador(const std::string& id) const {
    for (auto [e, m] : reg.view<const Morador>().each())
        if (m.id == id) return e;
    return entt::null;
}

bool World::tileBloqueado(int x, int y) {
    if (bloqueio_.empty()) {
        // Tiles sólidos + tudo que tem colisor (com 2 px de folga para não travar em quinas)
        bloqueio_.assign(static_cast<size_t>(mapa.largura()) * mapa.altura(), 0);
        for (int ty = 0; ty < mapa.altura(); ++ty)
            for (int tx = 0; tx < mapa.largura(); ++tx) bloqueio_[ty * mapa.largura() + tx] = mapa.em(tx, ty).solido;
        for (const ke::AABB& c : obstaculos()) {
            const int x0 = static_cast<int>(c.min.x + 2) / LADO, x1 = static_cast<int>(c.max.x - 2) / LADO;
            const int y0 = static_cast<int>(c.min.y + 2) / LADO, y1 = static_cast<int>(c.max.y - 2) / LADO;
            for (int ty = y0; ty <= y1; ++ty)
                for (int tx = x0; tx <= x1; ++tx)
                    if (mapa.dentro(tx, ty)) bloqueio_[ty * mapa.largura() + tx] = 1;
        }
    }
    return !mapa.dentro(x, y) || bloqueio_[y * mapa.largura() + x];
}

glm::ivec2 World::destinoDe(entt::entity e, const ItemRotina& item) const {
    // Quem divide o mesmo local fica lado a lado, não empilhado
    static const glm::ivec2 vizinhos[] = {{0, 0}, {1, 0}, {-1, 0}, {0, 1}, {1, 1}, {-1, 1}};
    const glm::ivec2 base = locais.at(item.local);
    const int i = reg.get<Morador>(e).indice;
    int n = 0;
    for (auto [o, m, a] : reg.view<const Morador, const Agenda>().each())
        if (o != e && m.indice < i && a.itemAtual >= 0 && dados().npcs.at(m.id).rotina[a.itemAtual].local == item.local) ++n;
    const glm::ivec2 t = base + vizinhos[n % 6];
    return mapa.dentro(t.x, t.y) && !mapa.em(t.x, t.y).solido ? t : base;
}

void World::caminharPara(entt::entity e, glm::ivec2 destino) {
    auto& mv = reg.get<Movimento>(e);
    const glm::vec2 p = reg.get<Posicao>(e).v;
    const glm::ivec2 inicio{static_cast<int>(p.x) / LADO, static_cast<int>(p.y) / LADO};
    mv.caminho.clear();
    const auto tiles = ke::buscarCaminho(mapa.largura(), mapa.altura(), inicio, destino, [this](int x, int y) { return tileBloqueado(x, y); });
    if (tiles.empty() && inicio != destino) {
        // Sem caminho (cercado por obstáculos): vai direto, para não ficar preso para sempre
        reg.get<Posicao>(e).v = {destino.x * LADO + LADO / 2.0f, destino.y * LADO + 12.0f};
        return;
    }
    for (const glm::ivec2 t : tiles) mv.caminho.push_back({t.x * LADO + LADO / 2.0f, t.y * LADO + 12.0f});
}

void World::posicionarMoradores() {
    for (auto [e, m, p, mv, a] : reg.view<const Morador, Posicao, Movimento, Agenda>().each()) {
        const NpcDef& def = dados().npcs.at(m.id);
        const ItemRotina* item = def.rotinaEm(relogio.minutos);
        a.itemAtual = item ? static_cast<int>(item - def.rotina.data()) : -1;
        a.conversando = false;
        mv.caminho.clear();
        mv.andando = false;
        mv.dir = Direcao::Baixo;
    }
    // Segunda passada: destinoDe consulta o item atual dos outros
    const auto& lugaresFesta = dados().estacoes.festival.lugares;
    for (auto [e, m, p, a] : reg.view<const Morador, Posicao, Agenda>().each()) {
        if (festivalAgora() && !dados().npcs.at(m.id).visitante()) {  // dia de festa: todos em volta da fogueira
            const glm::ivec2 t = lugaresFesta[m.indice % lugaresFesta.size()];
            a.itemAtual = -2;
            a.dormindo = false;
            p.v = {t.x * LADO + LADO / 2.0f, t.y * LADO + 12.0f};
            continue;
        }
        if (a.itemAtual < 0) continue;
        const ItemRotina& item = dados().npcs.at(m.id).rotina[a.itemAtual];
        const glm::ivec2 t = destinoDe(e, item);
        p.v = {t.x * LADO + LADO / 2.0f, t.y * LADO + 12.0f};
        const NpcDef& def = dados().npcs.at(m.id);
        a.dormindo = item.acao == "dormir" || (def.visitante() && !visitanteHoje(def));  // fora do dia de visita, nem aparece
    }
}

bool World::visitanteHoje(const NpcDef& d) const {
    if (!d.visitante()) return true;
    // No verão o comércio esquenta: o mercador vem mais vezes
    const int intervalo = estacaoAtual().mercadorACada > 0 ? estacaoAtual().mercadorACada : d.visitaACada;
    return (d.visitaRequer.empty() || marcos.contains(d.visitaRequer)) && relogio.dia % intervalo == 0;
}

void World::atualizarMoradores(float dt) {
    for (auto [e, m, p, mv, a] : reg.view<const Morador, Posicao, Movimento, Agenda>().each()) {
        const NpcDef& def = dados().npcs.at(m.id);
        const ItemRotina* item = def.rotinaEm(relogio.minutos);
        if (!item) continue;
        if (def.visitante() && !visitanteHoje(def)) {
            a.dormindo = true;
            mv.andando = false;
            continue;
        }
        // Festival: larga a rotina e vai para a praça
        const bool festa = festivalAgora() && !def.visitante();
        if (festa && a.itemAtual != -2) {
            const auto& lugares = dados().estacoes.festival.lugares;
            a.itemAtual = -2;
            a.dormindo = false;
            a.conversando = false;
            caminharPara(e, lugares[m.indice % lugares.size()]);
        }
        if (festa) {
            if (!a.conversando) seguirCaminho(p.v, mv, dt);
            else mv.andando = false;
            if (mv.caminho.empty() && !a.conversando) {  // parado, olhando para o centro da festa
                glm::vec2 centro{0.0f};
                for (const glm::ivec2 l : dados().estacoes.festival.lugares) centro += glm::vec2(l);
                centro = centro / static_cast<float>(dados().estacoes.festival.lugares.size()) * static_cast<float>(LADO) + 8.0f;
                const glm::vec2 d = centro - p.v;
                if (std::abs(d.x) > std::abs(d.y)) mv.dir = d.x < 0 ? Direcao::Esquerda : Direcao::Direita;
                else mv.dir = d.y < 0 ? Direcao::Cima : Direcao::Baixo;
            }
            continue;
        }
        const int idx = static_cast<int>(item - def.rotina.data());
        if (idx != a.itemAtual) {
            // Hora de mudar de atividade: sai de casa (se dormia) e vai para o próximo local
            a.itemAtual = idx;
            a.dormindo = false;
            caminharPara(e, destinoDe(e, *item));
        }
        if (a.conversando || a.dormindo) {
            mv.andando = false;
            continue;
        }
        seguirCaminho(p.v, mv, dt);
        // Chegou em casa na hora de dormir: entra e some até de manhã
        if (mv.caminho.empty() && item->acao == "dormir") {
            a.dormindo = true;
            mv.dir = Direcao::Baixo;
        }
    }
}

bool World::seguirCaminho(glm::vec2& pos, Movimento& mv, float dt) {
    // Longe do herói: anda em passos maiores, 4 vezes por segundo (ninguém está vendo)
    float passo = dt;
    if (glm::distance(pos, jogador.pos) > 380.0f) {
        mv.acumuladoLod += dt;
        if (mv.acumuladoLod < 0.25f) return false;
        passo = mv.acumuladoLod;
        mv.acumuladoLod = 0.0f;
    }
    float resta = mv.velocidade * passo;
    bool andou = false;
    while (resta > 0.0f && !mv.caminho.empty()) {
        const glm::vec2 alvo = mv.caminho.front();
        const glm::vec2 d = alvo - pos;
        const float dist = glm::length(d);
        if (dist <= resta) {
            pos = alvo;
            resta -= dist;
            mv.caminho.erase(mv.caminho.begin());
        } else {
            pos += d / dist * resta;
            resta = 0.0f;
        }
        if (dist > 0.01f) {
            if (std::abs(d.x) > std::abs(d.y)) mv.dir = d.x < 0 ? Direcao::Esquerda : Direcao::Direita;
            else mv.dir = d.y < 0 ? Direcao::Cima : Direcao::Baixo;
        }
        andou = true;
    }
    mv.andando = andou;
    if (andou) mv.tempoAnim += passo;
    return andou;
}

Conversa World::conversar(const std::string& id) {
    Conversa c;
    const entt::entity e = morador(id);
    if (e == entt::null) return c;
    const NpcDef& def = dados().npcs.at(id);
    auto& rel = reg.get<Relacionamento>(e);
    ContextoFala ctx{relogio.periodo(), relogio.estacao(), 1, static_cast<int>(reino.eraAtual()), &marcos, pistasEncontradas(),
                     festivalAgora(), eventoHoje() ? eventoHoje()->id : ""};
    if (const BuildingData* casa = reino.construcao("casa_herdeiro")) ctx.nivelCasa = casa->level;
    const EntradaFala* fala = escolherFala(def, rel, ctx);
    c.npc = id;
    c.nome = def.nome;
    c.profissao = def.profissao;
    c.falas = fala ? fala->falas : std::vector<std::string>{"..."};
    // O povo chama o herdeiro pelo título que ele conquistou
    for (auto& f : c.falas) f = preencher(f, {{"titulo", titulo().nome}});
    c.loja = !def.loja.empty();
    c.presente = presenteParaDar(id);  // fase 15: F dá na hora da conversa
    // Amizade cresce uma vez por dia de conversa
    if (!rel.falouHoje) {
        rel.afinidade = std::min(1000, rel.afinidade + (festivalAgora() ? 50 : 25));  // no festival, vale o dobro
        c.ganhouAmizade = true;
        // Conselheiro ouvido pelo herdeiro fica mais leal
        if (!cargoDe(id).empty()) lealdade[id] = std::min(100, lealdade[id] + regra::LEALDADE_POR_CONVERSA);
    }
    rel.falouHoje = true;
    rel.conheceu = true;
    if (!def.visitante()) marcar("primeira_conversa");
    if (fala && fala->umaVez) rel.memoria.insert(fala->id);
    // Missão: a primeira deste morador que ainda não começou (e cujo requisito já foi cumprido)
    for (const MissaoDef& m : dados().missoes) {
        if (m.npc != id || missoes.contains(m.id)) continue;
        if (!m.requer.empty() && !(missoes.contains(m.requer) && missoes.at(m.requer).concluida)) continue;
        c.falas.push_back("Missão: " + m.nome + ". " + m.descricao);
        c.missao = m.id;
        iniciarMissao(m);
        break;
    }
    c.coracoes = rel.coracoes();
    reg.get<Agenda>(e).conversando = true;
    return c;
}

void World::encerrarConversa(const std::string& id) {
    if (const entt::entity e = morador(id); e != entt::null) reg.get<Agenda>(e).conversando = false;
}

}
