// Parte do World: Crônicas, títulos e mistério (fase 9); estações, acontecimentos e festival (fase 10)
#include "Game/World/WorldInterno.h"

namespace koa {

using namespace interno;

// ---------- Fase 9: Crônicas, títulos e o mistério ----------

bool World::marcar(const std::string& marco, const std::string& nome, bool silencioso) {
    if (!marcos.insert(marco).second) return false;
    if (dados_) reagirAoMarco(marco);  // fase 19: os vizinhos e a fama reagem ao que aconteceu
    // Página própria ("construiu:memorial") ou a do grupo ("construiu:*")
    const auto& paginas = dados().lore.paginas;
    auto it = paginas.find(marco);
    if (const auto dp = marco.find(':'); it == paginas.end() && dp != std::string::npos) it = paginas.find(marco.substr(0, dp + 1) + "*");
    if (it != paginas.end()) {
        EntradaCronica c{marco, relogio.dia, static_cast<int>(reino.eraAlvo()), preencher(it->second, {{"nome", nome}})};
        cronicas.push_back(c);
        if (!silencioso) eventos_.push_back({EventoJogo::Tipo::Cronica, jogador.pos, marco, "", 0, c.texto});
    }
    if (marco.starts_with("pista:") && pistasEncontradas() == static_cast<int>(dados().lore.pistas.size()))
        marcar("misterio:primeiro_ato", "", silencioso);  // todas as peças: a primeira verdade sobre a Queda
    if (!marco.starts_with("titulo:")) verificarTitulos(silencioso);
    atualizarPistas();  // um acontecimento pode revelar (ou recolher) fragmentos
    atualizarEspeciais();  // ... ou abrir uma passagem e transformar uma região
    return true;
}

int World::contarObrasProntas(bool tipos) const {
    std::set<std::string> defs;
    int n = 0;
    for (const auto& b : reino.construcoes)
        if (contaComoObra(b)) {  // ruínas e a casa do herdeiro não são obra
            ++n;
            defs.insert(b.defId);
        }
    return tipos ? static_cast<int>(defs.size()) : n;
}

int World::pistasEncontradas() const {
    int n = 0;
    for (const auto& p : dados().lore.pistas) n += marcos.contains("pista:" + p.id);
    return n;
}

void World::verificarTitulos(bool silencioso) {
    SituacaoTitulos s;
    s.marcos = &marcos;
    s.construcoes = contarObrasProntas(false);
    s.tipos = contarObrasProntas(true);
    for (const auto& [tipo, n] : abates) s.abates += n;
    s.populacao = estado.populacao;
    s.era = static_cast<int>(reino.eraAlvo());
    s.pistas = pistasEncontradas();
    const auto& titulos = dados().lore.titulos;
    // O primeiro é o de quem acabou de chegar: não se conquista
    for (size_t i = 1; i < titulos.size(); ++i) {
        const TituloDef& t = titulos[i];
        if (marcos.contains("titulo:" + t.id) || !cumpre(t.requer, s)) continue;
        marcar("titulo:" + t.id, t.nome, silencioso);
        if (!silencioso) eventos_.push_back({EventoJogo::Tipo::Titulo, jogador.pos, t.id, "", 0, t.nome});
    }
}

const TituloDef& World::titulo() const {
    const auto& titulos = dados().lore.titulos;
    for (auto it = titulos.rbegin(); it != titulos.rend(); ++it)
        if (marcos.contains("titulo:" + it->id)) return *it;
    return titulos.front();
}

void World::atualizarPistas() {
    if (!dados_) return;
    auto visivel = [&](const PistaDef& p) {
        return p.regiao == regiaoAtual && (p.requer.empty() || marcos.contains(p.requer)) && !marcos.contains("pista:" + p.id);
    };
    std::set<std::string> existentes;
    std::vector<entt::entity> remover;
    for (auto [e, pi] : reg.view<const Pista>().each()) {
        const PistaDef* d = dados().lore.pista(pi.id);
        if (!d || !visivel(*d)) remover.push_back(e);
        else existentes.insert(pi.id);
    }
    reg.destroy(remover.begin(), remover.end());
    for (const PistaDef& p : dados().lore.pistas) {
        if (!visivel(p) || existentes.contains(p.id)) continue;
        const auto e = reg.create();
        reg.emplace<Posicao>(e, baseTile(p.tile.x, p.tile.y));
        reg.emplace<Visual>(e, "pista_0");
        reg.emplace<Animado>(e, "pista", 2, 2.5f);
        reg.emplace<Luz>(e, glm::vec3{1.0f, 0.85f, 0.5f}, 22.0f, glm::vec2{0.0f, -3.0f});  // brilha à noite, para ser achada
        reg.emplace<Pista>(e, p.id);
    }
}

void World::reconstruirCronicas() {
    // Save de antes da fase 9: refaz as páginas do que já tinha acontecido, sem data (dia 0)
    const std::set<std::string> antigos = marcos;
    marcos.clear();
    cronicas.clear();
    marcar("chegada", "", true);
    if (const BuildingData* casa = reino.construcao("casa_herdeiro"))
        for (int n = 2; n <= casa->level; ++n) marcar(n == 2 ? "casa_reconstruida" : std::format("casa_n{}", n), "", true);
    for (const auto& b : reino.construcoes)
        if (contaComoObra(b) && !def(b)->ruinas) marcar("construiu:" + b.defId, b.name, true);  // muralha e castelo têm páginas por nível
    if (const BuildingData* c = porFuncao("castelo"))
        for (int n = 2; n <= c->level; ++n)
            if (const BuildingDef* d = def(*c); d && d->nivel(n)) marcar(std::format("castelo_n{}", n), d->nivel(n)->nome, true);
    for (const auto& v : visitadas)
        if (v != "vilarejo" && regioes.contains(v)) marcar("visitou:" + v, regioes.at(v).nome, true);
    for (const MissaoDef& m : dados().missoes)
        if (const auto it = missoes.find(m.id); it != missoes.end() && it->second.concluida) marcar("missao:" + m.id, m.nome, true);
    for (const auto& m : antigos) {
        const std::string tipo = m.starts_with("chefe_") ? m.substr(6) : "";
        marcar(m, dados().inimigos.contains(tipo) ? dados().inimigos.at(tipo).nome : "", true);
    }
    for (int e = 1; e <= static_cast<int>(reino.eraAlvo()); ++e) marcar(std::format("era:{}", e), nomeEra(static_cast<KingdomEra>(e)), true);
    for (const int n : {15, 25, 40})
        if (estado.populacao >= n) marcar(std::format("povo:{}", n), std::to_string(n), true);
    for (auto& c : cronicas) c.dia = 0;
}

bool World::comprar(const std::string& npc, const std::string& itemId) {
    const auto it = dados().npcs.find(npc);
    if (it == dados().npcs.end()) return false;
    const auto item = std::ranges::find(it->second.loja, itemId, &ItemLoja::id);
    if (item == it->second.loja.end()) return false;
    if (itemEsgotado(*item)) {
        eventos_.push_back({EventoJogo::Tipo::Aviso, jogador.pos, "", "", 0, "Você já tem isso."});
        return false;
    }
    if (!inventario.pagar(item->custo)) {
        eventos_.push_back({EventoJogo::Tipo::Aviso, jogador.pos, "", "", 0, "Falta ouro para isso."});
        return false;
    }
    for (const auto& [r, q] : item->ganho) {
        inventario.adicionar(r, q);
        eventos_.push_back({EventoJogo::Tipo::Ganhou, jogador.pos - glm::vec2(0, 10), "loja", r, q});
    }
    if (!item->pista.empty()) {
        eventos_.push_back({EventoJogo::Tipo::LerPista, jogador.pos, item->pista});
        marcar("pista:" + item->pista, dados().lore.pista(item->pista) ? dados().lore.pista(item->pista)->titulo : item->nome);
    }
    return true;
}

// ---------- Fase 10: estações, acontecimentos e festival ----------

bool World::festivalNoDia(int dia) const {
    const FestivalDef& f = dados().estacoes.festival;
    const Relogio r{dia};
    return r.estacao() == f.estacao && r.diaDaEstacao() == f.diaDaEstacao;
}

bool World::festivalAgora() const {
    const FestivalDef& f = dados().estacoes.festival;
    return festivalHoje() && relogio.minutos >= f.inicio && relogio.minutos < f.fim;
}

const EventoDef* World::eventoDoDia(int dia) const {
    const DadosEstacoes& de = dados().estacoes;
    if (dia <= de.diasSemEventos || festivalNoDia(dia)) return nullptr;
    // Sorteio fixo: o mesmo dia sempre dá o mesmo acontecimento (não precisa ir para o save)
    const float sorte = static_cast<float>(hashTile(dia, 7919) % 10000) / 10000.0f;
    float acumulado = 0.0f;
    // Fase 11: o pacto com a Chama atrai a Lua de Sangue
    float luaSangue = 1.0f;
    for (const auto& id : regioesComVida())
        if (const TransformacaoRegiao* t = transformacao(id)) luaSangue *= t->luaSangue;
    luaSangue *= efeitosReliquias().luaSangue;  // fase 14: o Olho da Chama
    for (const EventoDef& ev : de.eventos) {
        if (!ev.requer.empty() && !marcos.contains(ev.requer)) continue;
        if (static_cast<int>(reino.eraAlvo()) < ev.eraMin) continue;
        acumulado += ev.chance * (ev.id == "lua_sangue" ? luaSangue : 1.0f);
        if (sorte < acumulado) return &ev;
    }
    return nullptr;
}

bool World::eventoAgora(const std::string& id) const {
    const EventoDef* ev = eventoHoje();
    return ev && ev->id == id && relogio.minutos >= 19 * 60;
}

float World::forcaInimigos() const {
    float f = estacaoAtual().inimigos;
    if (const EventoDef* ev = eventoHoje(); ev && eventoAgora(ev->id)) f *= ev->inimigos;
    return f;
}

void World::notasDaManha(int ontem) {
    auto& notas = ultimoRelatorio.notas;
    const DadosEstacoes& de = dados().estacoes;
    // O que a noite de ontem deixou
    if (festivalNoDia(ontem)) {
        estado.felicidade = std::min(100.0f, estado.felicidade + de.festival.felicidade);
        ultimoRelatorio.felicidadeDepois = estado.felicidade;
        notas.push_back(std::format("O {} ainda alegra o povo (+{:.0f} de felicidade).", de.festival.nome, de.festival.felicidade));
    }
    if (const EventoDef* ev = eventoDoDia(ontem); ev && ev->felicidade > 0.0f) {
        estado.felicidade = std::min(100.0f, estado.felicidade + ev->felicidade);
        ultimoRelatorio.felicidadeDepois = estado.felicidade;
        notas.push_back(std::format("A {} deixou todos sonhadores (+{:.0f} de felicidade).", ev->nome, ev->felicidade));
    }
    if (const EventoDef* ev = eventoDoDia(ontem); ev && ev->ouroMeteorito > 0) notas.push_back("Uma estrela caiu perto do vilarejo durante a noite!");
    // Estação nova, aviso do inverno, festival e o que os velhos dizem de hoje
    const EstacaoDef& hoje = estacaoAtual();
    if (relogio.diaDaEstacao() == 1) {
        notas.push_back(hoje.chegada);
        marcar("estacao:" + hoje.id, hoje.nome);
        if (relogio.estacao() == Estacao::Primavera && relogio.ano() > 1) marcar(std::format("ano:{}", relogio.ano()), std::to_string(relogio.ano()));
    }
    if (relogio.estacao() == Estacao::Outono && relogio.diaDaEstacao() >= 4) {
        const int faltam = Relogio::DIAS_POR_ESTACAO - relogio.diaDaEstacao() + 1;
        notas.push_back(std::format("O inverno chega em {} dia{}: as hortas quase param no frio. Guarde comida.", faltam, faltam > 1 ? "s" : ""));
    }
    if (festivalHoje()) notas.push_back(std::format("Hoje é dia de {}! O povo se reúne na praça às {}:00.", de.festival.nome, de.festival.inicio / 60));
    if (const EventoDef* ev = eventoHoje()) notas.push_back(ev->anuncio);
}

void World::atualizarMeteorito() {
    if (!noVilarejo()) return;
    // A estrela da noite de ontem fica no chão até ser recolhida (ou até o dia seguinte)
    const int noite = relogio.dia - 1;
    const EventoDef* ev = eventoDoDia(noite);
    const bool deveExistir = ev && ev->ouroMeteorito > 0 && !marcos.contains(std::format("meteorito:{}", noite));
    std::vector<entt::entity> remover;
    bool existe = false;
    for (auto [e, m] : reg.view<const Meteorito>().each()) {
        if (deveExistir && m.dia == noite) existe = true;
        else remover.push_back(e);
    }
    reg.destroy(remover.begin(), remover.end());
    if (!deveExistir || existe || dados().estacoes.meteoritos.empty()) return;
    // Um dos lugares possíveis, sorteado pelo dia (pulando os que viraram construção)
    const auto& lugares = dados().estacoes.meteoritos;
    const auto& obst = obstaculos();
    for (size_t k = 0; k < lugares.size(); ++k) {
        const glm::ivec2 t = lugares[(hashTile(noite, 31) + k) % lugares.size()];
        const glm::vec2 p = baseTile(t.x, t.y);
        const ke::AABB caixa{p - glm::vec2(8, 10), p + glm::vec2(8, 2)};
        if (std::ranges::any_of(obst, [&](const ke::AABB& o) { return caixa.intersecta(o); })) continue;
        const auto e = reg.create();
        reg.emplace<Posicao>(e, p);
        reg.emplace<Visual>(e, "meteorito");
        reg.emplace<Colisor>(e, glm::vec2{6, 4}, glm::vec2{0, -4});
        reg.emplace<Saqueavel>(e, "ouro", ev->ouroMeteorito, false, "meteorito_aberto");
        reg.emplace<Luz>(e, glm::vec3{0.55f, 0.75f, 1.0f}, 34.0f, glm::vec2{0.0f, -5.0f});
        reg.emplace<Meteorito>(e, noite);
        bloqueio_.clear();
        return;
    }
}

std::vector<std::string> World::inscricao(const BuildingData& b) const {
    std::vector<std::string> l;
    auto diaDe = [&](const std::string& marco) {
        for (const auto& c : cronicas)
            if (c.marco == marco) return c.dia;
        return -1;
    };
    if (funcaoDe(b) == "memorial") {
        l.push_back("Que Valdren não esqueça o que perdeu nem o que reencontrou.");
        for (const PistaDef& p : dados().lore.pistas)
            if (marcos.contains("pista:" + p.id)) l.push_back("- " + p.titulo);
        if (l.size() == 1) l.push_back("Ninguém ainda sabe o que causou a Queda das Cinzas.");
        else l.push_back(std::format("{} de {} fragmentos da Queda reunidos.", pistasEncontradas(), dados().lore.pistas.size()));
    } else if (funcaoDe(b) == "estatua") {
        l.push_back(titulo().nome + ", herdeiro de Valdren.");
        if (const int d = diaDe("chefe_cavaleiro_caido"); d > 0) l.push_back(std::format("Derrotou o Cavaleiro Caído no dia {}.", d));
        else if (d == 0) l.push_back("Derrotou o Cavaleiro Caído.");
        int abatidos = 0;
        for (const auto& [tipo, n] : abates) abatidos += n;
        l.push_back(std::format("Criaturas vencidas: {}.", abatidos));
        l.push_back(std::format("Páginas nas Crônicas: {}.", cronicas.size()));
    } else if (funcaoDe(b) == "jardim") {
        l.push_back("Aqui florescem os nomes de quem ergueu o reino.");
        for (auto [e, m, rel] : reg.view<const Morador, const Relacionamento>().each())
            if (rel.afinidade >= 300) l.push_back("- " + dados().npcs.at(m.id).nome + ", amigo do herdeiro");
        for (const MissaoDef& m : dados().missoes)
            if (const auto it = missoes.find(m.id); it != missoes.end() && it->second.concluida) l.push_back("- " + m.nome);
        if (l.size() == 1) l.push_back("Ainda há canteiros vazios. Faça amigos e ajude o povo.");
    }
    return l;
}

}
