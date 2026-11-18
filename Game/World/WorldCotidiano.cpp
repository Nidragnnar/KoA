// Parte do World: vida cotidiana: horta, pesca, animais, cozinha e presentes (fase 15)
#include "Game/World/WorldInterno.h"

namespace koa {

using namespace interno;

// ---------- Fase 15: vida cotidiana (horta, pesca, animais, cozinha e presentes) ----------

namespace {

// Os bichos e a horta vivem no vilarejo, mesmo com o herdeiro fora dele
entt::registry& registroVila(World& w, std::map<std::string, entt::registry*>& guardados) {
    const auto it = guardados.find("vilarejo");
    return it == guardados.end() ? w.reg : *it->second;
}

}

void World::criarCanteiros() {
    const auto& pos = dados().cotidiano.canteiros;
    canteiros.assign(pos.size(), {});
    for (size_t k = 0; k < pos.size(); ++k) {
        const glm::vec2 base = baseTile(pos[k].x, pos[k].y);
        const auto solo = reg.create();
        reg.emplace<Posicao>(solo, base - glm::vec2(0, 0.5f));  // sempre por baixo da planta
        reg.emplace<Visual>(solo, "canteiro_seco");
        reg.emplace<Canteiro>(solo, static_cast<int>(k));
        reg.emplace<Interativo>(solo, "canteiro");
        const auto planta = reg.create();
        reg.emplace<Posicao>(planta, base);
        reg.emplace<Visual>(planta, "");
        reg.emplace<PlantaCanteiro>(planta, static_cast<int>(k));
    }
}

void World::atualizarCanteiros() {
    std::map<std::string, entt::registry*> g;
    for (auto& [id, r] : guardadas_) g[id] = &r.reg;
    entt::registry& r = registroVila(*this, g);
    for (auto [e, c, v] : r.view<const Canteiro, Visual>().each())
        if (c.indice < static_cast<int>(canteiros.size())) v.sprite = canteiros[c.indice].regadoDia == relogio.dia ? "canteiro_molhado" : "canteiro_seco";
    for (auto [e, pc, v] : r.view<const PlantaCanteiro, Visual>().each()) {
        if (pc.indice >= static_cast<int>(canteiros.size())) continue;
        const EstadoCanteiro& c = canteiros[pc.indice];
        const CulturaDef* d = dados().cotidiano.cultura(c.cultura);
        if (!d) {
            v.sprite.clear();
            continue;
        }
        const int estagio = c.maduro ? 2 : c.crescidos * 2 >= d->dias ? 1 : 0;
        v.sprite = std::format("planta_{}_{}", d->id, estagio);
    }
}

const CulturaDef* World::sementeDaEstacao() const {
    const CulturaDef* melhor = nullptr;
    for (const CulturaDef& c : dados().cotidiano.culturas)
        if (c.estacoes.contains(relogio.estacao()) && inventario.quantidade(c.semente) > 0 &&
            (!melhor || inventario.quantidade(c.semente) > inventario.quantidade(melhor->semente)))
            melhor = &c;
    return melhor;
}

std::string World::rotuloCanteiro(int i) const {
    if (i < 0 || i >= static_cast<int>(canteiros.size())) return "";
    const EstadoCanteiro& c = canteiros[i];
    const CulturaDef* d = dados().cotidiano.cultura(c.cultura);
    if (!d) {
        if (const CulturaDef* s = sementeDaEstacao()) return std::format("Plantar {} ({} sementes)", s->nome, inventario.quantidade(s->semente));
        return relogio.estacao() == Estacao::Inverno ? "Inverno: nada cresce" : "Sem sementes (o Corvin vende)";
    }
    if (c.maduro) return "Colher " + d->nome;
    if (c.regadoDia == relogio.dia) return d->nome + std::format(" (regado hoje, {}/{} dias)", c.crescidos, d->dias);
    return "Regar " + d->nome;
}

bool World::usarCanteiro(int i) {
    if (i < 0 || i >= static_cast<int>(canteiros.size())) return false;
    EstadoCanteiro& c = canteiros[i];
    const glm::vec2 onde = baseTile(dados().cotidiano.canteiros[i].x, dados().cotidiano.canteiros[i].y);
    auto aviso = [&](const std::string& t, bool bom) { eventos_.push_back({EventoJogo::Tipo::Aviso, jogador.pos, bom ? "bom" : "", "", 0, t}); };
    const CulturaDef* d = dados().cotidiano.cultura(c.cultura);
    if (!d) {
        const CulturaDef* s = sementeDaEstacao();
        if (!s) {
            aviso(relogio.estacao() == Estacao::Inverno ? "No inverno a terra está dura: espere a primavera." : "Sem sementes para esta estação.", false);
            return false;
        }
        inventario.remover(s->semente, 1);
        c = {s->id, 0, 0, false};
        eventos_.push_back({EventoJogo::Tipo::Carinho, onde, "planta", "plantou"});
        aviso(std::format("Plantou {}. Regue todo dia (E).", s->nome), true);
    } else if (c.maduro) {
        inventario.adicionar(d->id, d->rendimento);
        eventos_.push_back({EventoJogo::Tipo::Ganhou, onde, "colheita", d->id, d->rendimento});
        c = {};
        marcar("primeira_colheita");
        atualizarMissoes();
    } else if (c.regadoDia == relogio.dia) {
        aviso(d->nome + " já foi regado hoje.", false);
        return false;
    } else {
        c.regadoDia = relogio.dia;
        eventos_.push_back({EventoJogo::Tipo::Carinho, onde, "rega", "regou"});
    }
    atualizarCanteiros();
    return true;
}

void World::sincronizarAnimais() {
    std::map<std::string, entt::registry*> g;
    for (auto& [id, r] : guardadas_) g[id] = &r.reg;
    entt::registry& r = registroVila(*this, g);
    const DadosCotidiano& dc = dados().cotidiano;
    // Quantos bichos cada galinheiro/curral pronto pede
    std::map<std::string, std::pair<std::string, glm::vec2>> desejados;  // chave → (animal, casa)
    for (const auto& b : reino.construcoes) {
        const auto it = dc.animais.find(b.defId);
        const BuildingDef* d = def(b);
        if (it == dc.animais.end() || !b.constructed || !d || !d->nivel(b.level)) continue;
        const int n = static_cast<int>(d->nivel(b.level)->efeito("animais"));
        const ke::AABB c = caixaConstrucao(b);
        const glm::vec2 casa{(c.min.x + c.max.x) * 0.5f, c.max.y + 18.0f};  // o terreiro na frente
        for (int k = 0; k < n; ++k) desejados[std::format("{}#{}", b.id, k)] = {it->second.animal, casa};
    }
    std::vector<entt::entity> remover;
    std::set<std::string> existem;
    for (auto [e, a] : r.view<const Animal>().each()) {
        if (!desejados.contains(a.chave)) remover.push_back(e);
        else existem.insert(a.chave);
    }
    r.destroy(remover.begin(), remover.end());
    std::uniform_real_distribution<float> u(-1.0f, 1.0f);
    for (const auto& [chave, par] : desejados) {
        if (existem.contains(chave)) continue;
        const auto e = r.create();
        const glm::vec2 p = par.second + glm::vec2(u(rng_) * 24.0f, u(rng_) * 8.0f);
        r.emplace<Posicao>(e, p);
        r.emplace<Interativo>(e, "animal");
        Animal& a = r.emplace<Animal>(e);
        a.tipo = par.first;
        a.chave = chave;
        a.casa = par.second;
        a.destino = p;
        a.espera = 1.0f + (u(rng_) + 1.0f);
    }
}

void World::atualizarAnimais(float dt) {
    std::uniform_real_distribution<float> u(-1.0f, 1.0f), espera(1.5f, 4.0f);
    for (auto [e, p, a] : reg.view<Posicao, Animal>().each()) {
        a.andando = false;
        if (a.espera > 0.0f) {
            a.espera -= dt;
            if (a.espera <= 0.0f) a.destino = a.casa + glm::vec2(u(rng_), u(rng_) * 0.5f) * (a.tipo == "vaca" ? 36.0f : 28.0f);
            continue;
        }
        const glm::vec2 d = a.destino - p.v;
        if (glm::length(d) < 2.0f) {
            a.espera = espera(rng_);
            continue;
        }
        const float vel = a.tipo == "vaca" ? 12.0f : 22.0f;
        const ke::AABB caixa{p.v - glm::vec2(3, 3), p.v + glm::vec2(3, 1)};
        const glm::vec2 feito = ke::moverComColisao(mapa, caixa, glm::normalize(d) * vel * dt, obstaculos(), 0.0f);  // fase 17: nada de atravessar casas
        p.v += feito;
        a.andando = glm::length(feito) > 0.01f;
        if (!a.andando) a.espera = espera(rng_);  // esbarrou na água: pensa de novo
        if (std::abs(d.x) > 0.5f) a.olhaEsquerda = d.x < 0;
        a.tempoAnim += dt;
    }
}

bool World::fazerCarinho(entt::entity e) {
    const Animal* a = reg.try_get<Animal>(e);
    if (!a) return false;
    if (carinhos.contains(a->chave) && carinhos.at(a->chave) == relogio.dia) {
        eventos_.push_back({EventoJogo::Tipo::Aviso, jogador.pos, "", "", 0, "Já recebeu carinho hoje."});
        return false;
    }
    carinhos[a->chave] = relogio.dia;
    eventos_.push_back({EventoJogo::Tipo::Carinho, reg.get<Posicao>(e).v - glm::vec2(0, 10), a->tipo, "carinho"});
    return true;
}

void World::noiteDoCotidiano(int ontem, std::vector<std::pair<std::string, int>>& variacoes, std::vector<std::string>& notas) {
    const DadosCotidiano& dc = dados().cotidiano;
    // A horta: só cresce o que foi regado ontem, na estação certa
    const Estacao estOntem = Relogio{ontem}.estacao();
    std::vector<std::string> prontos;
    bool geada = false, secos = false;
    for (auto& c : canteiros) {
        const CulturaDef* d = dc.cultura(c.cultura);
        if (!d || c.maduro) continue;
        if (c.regadoDia != ontem) {
            secos = true;
            continue;
        }
        if (!d->estacoes.contains(estOntem)) {
            geada = true;
            continue;
        }
        if (++c.crescidos >= d->dias) {
            c.maduro = true;
            prontos.push_back(d->nome);
        }
    }
    if (!prontos.empty()) {
        std::string lista;
        for (const auto& n : prontos) lista += (lista.empty() ? "" : ", ") + n;
        notas.push_back("Na horta: " + lista + " pronto para colher.");
    }
    if (geada) notas.push_back("O frio segurou a horta: o que foi plantado fora da estação não cresce.");
    if (secos) notas.push_back("Parte da horta ficou sem rega e não cresceu.");
    // Os bichos: cada um come 1 de comida e dá o produto (dois com carinho ontem)
    std::map<std::string, int> produtos;
    int comeram = 0;
    bool fome = false;
    std::map<std::string, entt::registry*> g;
    for (auto& [id, rg] : guardadas_) g[id] = &rg.reg;
    for (auto [e, a] : registroVila(*this, g).view<const Animal>().each()) {
        std::string produto;
        for (const auto& [b, ad] : dc.animais)
            if (ad.animal == a.tipo) produto = ad.produto;
        if (!inventario.remover("comida", 1)) {
            fome = true;
            continue;
        }
        ++comeram;
        produtos[produto] += 1 + (carinhos.contains(a.chave) && carinhos.at(a.chave) == ontem ? 1 : 0);
    }
    if (comeram > 0) variacoes.push_back({"comida|animais", -comeram});
    for (const auto& [p, n] : produtos) {
        inventario.adicionar(p, n);
        variacoes.push_back({p, n});
    }
    if (fome) notas.push_back("Faltou comida para os animais: alguns não produziram.");
    atualizarCanteiros();
}

void World::iniciarPesca(glm::vec2 ponto) {
    const DadosCotidiano& dc = dados().cotidiano;
    std::uniform_real_distribution<float> espera(dc.esperaMin, dc.esperaMax);
    // A boia cai o mais longe possível na água à frente (até ~2 tiles)
    for (float d = 34.0f; d >= 12.0f; d -= 2.0f) {
        const glm::vec2 p = jogador.pos + vetor(jogador.dir) * d + glm::vec2(0.0f, -3.0f);
        const glm::ivec2 t{static_cast<int>(p.x) / LADO, static_cast<int>(p.y) / LADO};
        if (mapa.dentro(t.x, t.y) && mapa.em(t.x, t.y).terreno == T_AGUA) {
            ponto = p;
            break;
        }
    }
    pesca_ = {true, false, espera(rng_), 0.0f, ponto};
    jogador.acao = 0.2f;
    eventos_.push_back({EventoJogo::Tipo::Mordida, ponto, "lancou", "", 0, "Linha na água... espere a mordida."});
}

void World::atualizarPesca(float dt, const EntradaMundo& in) {
    auto encerrar = [&](const std::string& t) {
        pesca_ = {};
        if (!t.empty()) eventos_.push_back({EventoJogo::Tipo::Aviso, jogador.pos, "", "", 0, t});
    };
    if (glm::length(in.mover) > 0.1f) return encerrar("Você recolheu a linha.");
    if (!pesca_.mordeu) {
        if (in.interagir) return encerrar("Cedo demais: o peixe se assustou.");
        pesca_.espera -= dt;
        if (pesca_.espera <= 0.0f) {
            pesca_.mordeu = true;
            pesca_.janela = dados().cotidiano.janela;
            eventos_.push_back({EventoJogo::Tipo::Mordida, pesca_.boia, "mordeu", "", 0, "Puxe! (E)"});
        }
        return;
    }
    if (in.interagir) {
        std::uniform_real_distribution<float> u(0.0f, 1.0f);
        const std::vector<float> sortes{u(rng_), u(rng_), u(rng_), u(rng_)};
        const glm::vec2 onde = pesca_.boia;
        pesca_ = {};
        jogador.acao = 0.3f;
        if (const PeixeDef* p = sortearPeixe(dados().cotidiano, regiaoAtual, relogio.estacao(), sortes)) {
            inventario.adicionar(p->id, 1);
            eventos_.push_back({EventoJogo::Tipo::Ganhou, onde, "pesca", p->id, 1});
            marcar("pescou:" + p->id, dados().nomeRecurso(p->id));
        }
        return;
    }
    pesca_.janela -= dt;
    if (pesca_.janela <= 0.0f) encerrar("O peixe escapou.");
}

bool World::cozinhar(const std::string& id) {
    const ReceitaDef* r = dados().cotidiano.receita(id);
    if (!r) return false;
    if (!inventario.pagar(r->ingredientes)) {
        eventos_.push_back({EventoJogo::Tipo::Aviso, jogador.pos, "", "", 0, "Faltam ingredientes para " + r->nome + "."});
        return false;
    }
    inventario.adicionar(id, 1);
    eventos_.push_back({EventoJogo::Tipo::Ganhou, jogador.pos - glm::vec2(0, 10), "cozinha", id, 1});
    marcar("cozinhou:" + id, r->nome);
    return true;
}

bool World::comerPrato(const std::string& id) {
    const ReceitaDef* r = dados().cotidiano.receita(id);
    if (!r || !inventario.remover(id, 1)) return false;
    const EfeitosPrato& e = r->efeitos;
    const int antes = jogador.vida;
    jogador.vida = std::min(jogador.vidaMax, jogador.vida + e.vida);
    buffDano = std::max(buffDano, e.dano);
    buffDefesa = std::max(buffDefesa, e.defesa);
    if (e.felicidade != 0.0f) {
        estado.felicidade = std::clamp(estado.felicidade + e.felicidade, 0.0f, 100.0f);
        marcar("serviu:" + id, r->nome);
    }
    eventos_.push_back({EventoJogo::Tipo::Comeu, jogador.pos, id, "comida", jogador.vida - antes});
    eventos_.push_back({EventoJogo::Tipo::Aviso, jogador.pos, "bom", "", 0,
                        e.felicidade != 0.0f ? std::format("{} servido na praça: {}.", r->nome, textoPrato(e)) : std::format("{}: {}.", r->nome, textoPrato(e))});
    return true;
}

bool World::temPrato() const {
    for (const ReceitaDef& r : dados().cotidiano.receitas)
        if (r.efeitos.vida > 0 && inventario.quantidade(r.id) > 0) return true;
    return false;
}

std::string World::presenteParaDar(const std::string& npc) const {
    if (const auto it = presenteDia.find(npc); it != presenteDia.end() && it->second == relogio.dia) return "";
    const DadosCotidiano& dc = dados().cotidiano;
    if (const auto it = dc.presentes.find(npc); it != dc.presentes.end())
        for (const auto& item : it->second)
            if (inventario.quantidade(item) > 0) return item;
    // Qualquer prato pronto também agrada (menos)
    for (const ReceitaDef& r : dc.receitas)
        if (inventario.quantidade(r.id) > 0) return r.id;
    return "";
}

bool World::darPresente(const std::string& npc) {
    const std::string item = presenteParaDar(npc);
    if (item.empty() || !dados().npcs.contains(npc)) return false;
    const auto& favoritos = dados().cotidiano.presentes;
    const bool favorito = favoritos.contains(npc) && std::ranges::find(favoritos.at(npc), item) != favoritos.at(npc).end();
    inventario.remover(item, 1);
    if (Relacionamento* rel = relacao(npc)) rel->afinidade = std::min(1000, rel->afinidade + (favorito ? 100 : 30));
    presenteDia[npc] = relogio.dia;
    const std::string nome = dados().npcs.at(npc).nome;
    eventos_.push_back({EventoJogo::Tipo::Presente, jogador.pos, npc, item, favorito ? 1 : 0,
                        std::format("{} {} {}!", nome, favorito ? "adorou o presente:" : "agradeceu por", dados().nomeRecurso(item))});
    marcar("presente:" + npc, nome);
    return true;
}

}
