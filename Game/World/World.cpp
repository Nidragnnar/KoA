// O núcleo do mundo: carregar o mapa, o passo de cada quadro, interação, regiões, eras e a noite.
// Cada sistema mora no seu arquivo (World<Domínio>.cpp); o que eles dividem está em WorldInterno.h
#include "Game/World/WorldInterno.h"

namespace koa {

uint32_t hashTile(int x, int y) {
    uint32_t h = static_cast<uint32_t>(x) * 73856093u ^ static_cast<uint32_t>(y) * 19349663u;
    h ^= h >> 13;
    h *= 0x5bd1e995u;
    return h ^ (h >> 15);
}

using namespace interno;

namespace {

// Caractere de terreno do arquivo de mapa; -1 se for objeto
int terrenoDe(char c) {
    switch (c) {
    case '.': return T_GRAMA;
    case ',': return T_TERRA;
    case '=': return T_LAMA;
    case '~': return T_AGUA;
    case ':': return T_CALCADA;
    case '#': return T_MURO;
    default: return -1;
    }
}

}

bool World::carregar(const GameData& dados, const std::filesystem::path& pasta, std::string* erro) {
    const auto def = ke::fs::lerTexto(pasta / "World/vilarejo.json");
    if (!def) {
        if (erro) *erro = "Data/World/vilarejo.json ausente";
        return false;
    }
    const auto j = nlohmann::json::parse(*def, nullptr, false);
    if (j.is_discarded()) {
        if (erro) *erro = "vilarejo.json inválido";
        return false;
    }
    const auto texto = ke::fs::lerTexto(pasta / "World" / j.value("mapa", "vilarejo.txt"));
    if (!texto) {
        if (erro) *erro = "arquivo do mapa ausente";
        return false;
    }
    if (!carregarDeTexto(dados, *texto, j, erro)) return false;
    // Outras regiões (floresta, cripta...): carregadas agora e guardadas fora de cena
    for (const auto& id : j.value("regioes", std::vector<std::string>{})) {
        const auto jr = ke::fs::lerTexto(pasta / "World" / (id + ".json"));
        const auto jj = jr ? nlohmann::json::parse(*jr, nullptr, false) : nlohmann::json();
        const auto tr = jr && !jj.is_discarded() ? ke::fs::lerTexto(pasta / "World" / jj.value("mapa", id + ".txt")) : std::nullopt;
        if (!tr) {
            if (erro) *erro = "região " + id + ": json ou mapa ausente/inválido";
            return false;
        }
        if (!adicionarRegiao(id, *tr, jj, erro)) return false;
    }
    for (const auto& [id, info] : regioes)
        for (const Saida& s : info.saidas)
            if (!regioes.contains(s.destino)) {
                if (erro) *erro = "saída de " + id + " leva a região inexistente: " + s.destino;
                return false;
            }
    return true;
}

InfoRegiao World::lerInfoRegiao(const std::string& id, const nlohmann::json& j) {
    InfoRegiao info;
    info.id = id;
    info.nome = j.value("nome", id);
    info.escuridaoMin = j.value("escuridao_min", 0.0f);
    if (j.contains("ambiente")) info.ambiente = {j["ambiente"][0].get<float>(), j["ambiente"][1].get<float>(), j["ambiente"][2].get<float>()};
    info.hostil = j.value("hostil", false);
    info.curaComEra = j.value("cura_com_era", false);
    info.subterranea = j.value("subterranea", false);
    info.estilo = j.value("estilo", "");
    if (j.contains("luz_tochas")) info.luzTochas = {j["luz_tochas"][0].get<float>(), j["luz_tochas"][1].get<float>(), j["luz_tochas"][2].get<float>()};
    for (const auto& s : j.value("saidas", nlohmann::json::array())) {
        Saida x;
        x.tile = {s.at("tile")[0].get<int>(), s.at("tile")[1].get<int>()};
        x.tam = {s.at("tam")[0].get<int>(), s.at("tam")[1].get<int>()};
        x.destino = s.at("destino").get<std::string>();
        x.chegada = {s.at("chegada")[0].get<int>(), s.at("chegada")[1].get<int>()};
        const std::string d = s.value("dir", "baixo");
        x.dir = d == "cima" ? Direcao::Cima : d == "esquerda" ? Direcao::Esquerda : d == "direita" ? Direcao::Direita : Direcao::Baixo;
        x.requer = s.value("requer", "");
        info.saidas.push_back(x);
    }
    lerVidaRegiao(j, info.vida);
    return info;
}

std::string World::estiloVisual() const {
    if (noVilarejo()) return coroaEterna() ? "coroa" : "";
    const auto it = regioes.find(regiaoAtual);
    return it == regioes.end() ? "" : it->second.estilo;
}

bool World::adicionarRegiao(const std::string& id, const std::string& texto, const nlohmann::json& j, std::string* erro) {
    // Carrega nos membros ativos e depois guarda: o vilarejo volta para a cena
    RegiaoGuardada r;
    std::swap(mapa, r.mapa);
    std::swap(reg, r.reg);
    prefixoIds_ = id + ":";
    InfoRegiao info;
    try {
        info = lerInfoRegiao(id, j);
    } catch (const std::exception& ex) {
        if (erro) *erro = "região " + id + ": " + ex.what();
        return false;
    }
    const glm::vec2 spawnVila = spawn;
    luzTochas_ = info.luzTochas;
    const bool ok = lerMapa(texto, info, erro);
    spawn = spawnVila;
    prefixoIds_.clear();
    luzTochas_ = InfoRegiao{}.luzTochas;
    std::swap(mapa, r.mapa);
    std::swap(reg, r.reg);
    if (!ok) return false;
    if (info.vida.ativa) {
        for (const auto& [tipo, t] : info.vida.invasao)
            if (!dados().inimigos.contains(tipo)) {
                if (erro) *erro = "região " + id + ": invasão com inimigo inexistente: " + tipo;
                return false;
            }
        EstadoRegiao e;
        e.corrupcao = info.vida.corrupcaoInicial;
        estadoRegioes[id] = e;
    }
    regioes[id] = std::move(info);
    guardadas_[id] = std::move(r);
    return true;
}

bool World::lerMapa(const std::string& texto, InfoRegiao& info, std::string* erro) {
    std::vector<std::string> linhas;
    std::istringstream ss(texto);
    for (std::string l; std::getline(ss, l);) {
        if (!l.empty() && l.back() == '\r') l.pop_back();
        linhas.push_back(l);
    }
    while (!linhas.empty() && linhas.back().empty()) linhas.pop_back();
    if (linhas.empty()) {
        if (erro) *erro = "mapa vazio";
        return false;
    }
    int largura = 0;
    for (const auto& l : linhas) largura = std::max(largura, static_cast<int>(l.size()));
    const int altura = static_cast<int>(linhas.size());
    mapa = ke::TileMap(largura, altura, LADO);

    auto charEm = [&](int x, int y) -> char {
        if (y < 0 || y >= altura || x < 0 || x >= static_cast<int>(linhas[y].size())) return '.';
        return linhas[y][x];
    };

    for (int y = 0; y < altura; ++y) {
        for (int x = 0; x < largura; ++x) {
            const char c = charEm(x, y);
            int t = terrenoDe(c);
            if (t < 0) {
                // Objeto: o chão embaixo é o do vizinho (esquerda, direita, cima)
                for (const char v : {charEm(x - 1, y), charEm(x + 1, y), charEm(x, y - 1)})
                    if ((t = terrenoDe(v)) >= 0) break;
                if (t < 0 || t == T_MURO || t == T_AGUA) t = T_GRAMA;
                if (const InimigoDef* ini = dados().inimigoPorLetra(c)) {
                    info.spawns.push_back({ini->id, {x, y}});
                    criarInimigo(ini->id, {x, y});
                } else {
                    criarObjeto(c, x, y);
                }
            }
            ke::Tile& tile = mapa.em(x, y);
            tile.terreno = static_cast<uint16_t>(t);
            tile.variante = static_cast<uint8_t>(hashTile(x, y) % 4);
            tile.solido = t == T_AGUA || t == T_MURO;
        }
    }
    return true;
}

bool World::carregarDeTexto(const GameData& dados, const std::string& texto, const nlohmann::json& j, std::string* erro) {
    dados_ = &dados;
    reg.clear();
    reino = {};
    inventario = {};
    jogador = {};
    tempoJogado = 0.0;
    relogio = {};
    locais.clear();
    bloqueio_.clear();
    desmaioPendente_ = false;
    estado = {};
    ultimoRelatorio = {};
    semear(2026);
    regiaoAtual = "vilarejo";
    regioes.clear();
    guardadas_.clear();
    prefixoIds_.clear();
    equip = {};
    missoes.clear();
    abates.clear();
    visitadas = {"vilarejo"};
    marcos.clear();
    cronicas.clear();
    derrotaPendente_ = false;
    avisoEvento_ = avisoFestival_ = 0;
    estadoRegioes.clear();
    conselho.clear();
    lealdade.clear();
    leis.clear();
    decisaoPendente.clear();
    ultimaDecisao = 0;
    hordaDia = hordaNumero = proximaHorda = portaoVida = 0;
    notasDefesa.clear();
    batalha_ = false;
    vencidasNaBatalha_ = saquesNaBatalha_ = 0;
    recargaTorre_.clear();
    aventureiros.clear();
    expedicao = {};
    reliquias.clear();
    canteiros.clear();
    buffDano = buffDefesa = 0;
    presenteDia.clear();
    carinhos.clear();
    pesca_ = {};
    alas.clear();
    brasao = dados_ ? dados_->castelo.inicial : Brasao{};
    brasaoCriado = false;
    sujarObstaculos();

    InfoRegiao info;
    try {
        info = lerInfoRegiao("vilarejo", j);
    } catch (const std::exception& ex) {
        if (erro) *erro = std::string("vilarejo.json: ") + ex.what();
        return false;
    }
    if (!lerMapa(texto, info, erro)) return false;
    regioes["vilarejo"] = info;

    try {
        if (j.contains("inventario_inicial"))
            for (const auto& [id, q] : j["inventario_inicial"].items()) inventario.definir(id, q.get<int>());
        for (const auto& c : j.value("construcoes", nlohmann::json::array())) {
            BuildingData b;
            b.id = c.at("id").get<std::string>();
            b.defId = c.at("def").get<std::string>();
            b.tile = {c.at("tile")[0].get<int>(), c.at("tile")[1].get<int>()};
            b.level = c.value("nivel", 1);
            const auto it = dados.construcoes.find(b.defId);
            if (it == dados.construcoes.end()) throw std::runtime_error("definição inexistente: " + b.defId);
            if (const auto* nv = it->second.nivel(b.level)) b.name = nv->nome;
            reino.construcoes.push_back(b);
            criarEntidadeConstrucao(b);
        }
        if (j.contains("reino_inicial")) {
            const auto& r = j["reino_inicial"];
            estado.populacao = r.value("populacao", 8);
            estado.populacaoBase = r.value("populacao_base", 8);
            estado.felicidade = r.value("felicidade", 40.0f);
            estado.imposto = static_cast<Imposto>(std::clamp(r.value("imposto", 1), 0, 2));
        }
        // Limpa objetos (árvores, rochas...) que estejam em cima de uma área reservada
        auto limpar = [&](glm::ivec2 t0, glm::ivec2 tam) {
            std::vector<entt::entity> remover;
            for (auto [e, p, id] : reg.view<const Posicao, const IdMundo>().each()) {
                const glm::ivec2 t{static_cast<int>(p.v.x) / LADO, static_cast<int>(p.v.y) / LADO};
                if (t.x >= t0.x && t.y >= t0.y && t.x < t0.x + tam.x && t.y < t0.y + tam.y) remover.push_back(e);
            }
            reg.destroy(remover.begin(), remover.end());
        };
        // Fase 17: o que começa em ruínas vem dos dados (o desenvolvimento do reino não conta o nível 1 delas)
        reino.defsEmRuinas.clear();
        for (const auto& [id, d] : dados.construcoes)
            if (d.ruinas) reino.defsEmRuinas.insert(id);
        // Fase 16: nada fica em cima das construções fixas (o castelo ocupa o lugar da antiga decoração)
        for (const auto& b : reino.construcoes)
            if (const BuildingDef* d = def(b)) limpar(b.tile, d->tamanho);
        // Guardado numa variável: iterar .items() de um temporário lê memória já liberada
        const nlohmann::json jLocais = j.value("locais", nlohmann::json::object());
        for (const auto& [nome, t] : jLocais.items()) {
            locais[nome] = {t[0].get<int>(), t[1].get<int>()};
            limpar(locais[nome], {1, 1});
        }
        for (const auto& d : j.value("decoracoes", nlohmann::json::array())) {
            const glm::ivec2 tile{d.at("tile")[0].get<int>(), d.at("tile")[1].get<int>()};
            const glm::ivec2 base{d.at("base")[0].get<int>(), d.at("base")[1].get<int>()};
            limpar(tile, base);
            const auto e = reg.create();
            reg.emplace<Posicao>(e, glm::vec2{(tile.x + base.x * 0.5f) * LADO, static_cast<float>((tile.y + base.y) * LADO)});
            reg.emplace<Visual>(e, d.at("sprite").get<std::string>());
            reg.emplace<Colisor>(e, glm::vec2{base.x * LADO * 0.5f, base.y * LADO * 0.5f}, glm::vec2{0.0f, -base.y * LADO * 0.5f});
            reg.emplace<Decoracao>(e, base);
        }
        // Enfeites das eras: aparecem quando a transformação chega até eles
        for (const auto& en : j.value("enfeites", nlohmann::json::array())) {
            const glm::ivec2 t{en.at("tile")[0].get<int>(), en.at("tile")[1].get<int>()};
            limpar(t, {1, 1});
            const auto e = reg.create();
            reg.emplace<Posicao>(e, baseTile(t.x, t.y));
            reg.emplace<Visual>(e, en.at("sprite").get<std::string>());
            reg.emplace<Enfeite>(e, en.value("era", 1), en.value("festival", false), en.value("marco", ""));
            if (en.contains("luz")) {
                const auto& l = en["luz"];
                reg.emplace<Luz>(e, glm::vec3{l[0].get<float>(), l[1].get<float>(), l[2].get<float>()}, l[3].get<float>(),
                                 glm::vec2{0.0f, -12.0f});
            }
        }
        // Fase 13: a faixa da Muralha Leste (e o chão logo atrás dela) fica sem árvores nem pedras
        if (const BuildingData* m = muralha()) limpar(m->tile - glm::ivec2(1, 0), {2, 7});
        // Fase 15: a horta do herdeiro também
        for (const glm::ivec2 t : dados.cotidiano.canteiros) limpar(t, {1, 1});
    } catch (const std::exception& ex) {
        if (erro) *erro = std::string("vilarejo.json: ") + ex.what();
        return false;
    }
    jogador.pos = spawn;
    if (const BuildingData* casa = reino.construcao("casa_herdeiro")) {
        const glm::vec2 porta = pontoPorta(*casa);
        centroReino_ = {static_cast<int>(porta.x) / LADO, static_cast<int>(porta.y) / LADO};
    }
    eraAnunciada_ = 0;
    for (const auto& [id, def] : dados.npcs)
        for (const auto& r : def.rotina)
            if (!locais.contains(r.local)) {
                if (erro) *erro = "rotina de " + id + " usa local inexistente: " + r.local;
                return false;
            }
    criarMoradores();
    posicionarMoradores();
    gerarPovoInicial();  // fase 18: os sobreviventes sem nome do mapa ganham nome
    iniciarFaccoes();    // fase 19: os vizinhos começam como nos dados
    iniciarLinhagem();   // fase 21: o fundador da dinastia
    iniciarMundoVivo();  // 1.14: as aldeias vizinhas como nos dados
    sincronizarAldeoes();
    criarCanteiros();
    marcar("chegada", "", true);  // a primeira página das Crônicas
    return true;
}

entt::entity World::criarEntidadeConstrucao(const BuildingData& b) {
    const BuildingDef* d = def(b);
    const glm::ivec2 t = d ? d->tamanho : glm::ivec2{1};
    const auto e = reg.create();
    reg.emplace<Posicao>(e, glm::vec2{(b.tile.x + t.x * 0.5f) * LADO, static_cast<float>((b.tile.y + t.y) * LADO)});
    reg.emplace<ConstrucaoRef>(e, b.id);
    return e;
}

std::string World::proximoPasso() const {
    const BuildingData* casa = reino.construcao("casa_herdeiro");
    if (casa && casa->level < 2) {
        if (inventario.quantidade("madeira") < 15) return "Corte árvores (E) até ter 15 de madeira";
        if (inventario.quantidade("pedra") < 8) return "Quebre rochas (E) até ter 8 de pedra";
        if (inventario.quantidade("ouro") < 10) return "Abra baús (E) para achar 10 de ouro";
        return "Reconstrua a cabana: E na porta de casa";
    }
    if (!marcos.contains("primeira_conversa") && missoes.empty()) return "Converse com um morador (E)";
    if (contarConstrucoes("campo") == 0) return "Construa uma horta: aperte B";
    if (relogio.dia <= 1) return "Durma: E na porta de casa";
    if (!visitadas.contains("floresta")) return "Vá à floresta pela trilha a leste";
    return "";
}

int World::contarConstrucoes(const std::string& d) const {
    return static_cast<int>(std::ranges::count_if(reino.construcoes, [&](const BuildingData& b) { return b.defId == d; }));
}

bool World::podeConstruir(const std::string& defId, glm::ivec2 tile, std::string* motivo) const {
    auto falha = [&](const char* m) {
        if (motivo) *motivo = m;
        return false;
    };
    const auto it = dados().construcoes.find(defId);
    if (it == dados().construcoes.end() || !it->second.construivel) return falha("Isso não se constrói.");
    const BuildingDef& d = it->second;
    if (!monumentoLiberado(d)) {
        if (motivo) *motivo = d.requerTexto.empty() ? "Ainda não é hora desta obra." : d.requerTexto;
        return false;
    }
    if (d.limite > 0 && contarConstrucoes(defId) >= d.limite) return falha("O reino já tem o máximo desta construção.");
    // Base dentro do mapa, longe da borda, e a fileira da porta livre
    if (tile.x < 1 || tile.y < 1 || tile.x + d.tamanho.x > mapa.largura() - 1 || tile.y + d.tamanho.y + 1 > mapa.altura() - 1)
        return falha("Fora dos limites do vilarejo.");
    for (int y = tile.y; y <= tile.y + d.tamanho.y; ++y)
        for (int x = tile.x; x < tile.x + d.tamanho.x; ++x)
            if (mapa.em(x, y).solido) return falha("Terreno impróprio (água ou muro).");
    const ke::AABB area{glm::vec2(tile * LADO) + 1.0f, glm::vec2((tile + d.tamanho + glm::ivec2(0, 1)) * LADO) - 1.0f};
    for (const ke::AABB& o : obstaculos())
        if (area.intersecta(o)) return falha("Há árvores, pedras ou construções no caminho.");
    if (const BuildingData* m = muralha(); m && area.intersecta(caixaConstrucao(*m))) return falha("A muralha leste fica aí.");
    for (const glm::ivec2 t : dados().cotidiano.canteiros)  // fase 15: a horta do herdeiro
        if (area.intersecta({glm::vec2(t * LADO), glm::vec2((t + 1) * LADO)})) return falha("Aí é a horta do herdeiro.");
    if (area.intersecta(jogador.caixa())) return falha("Saia de cima do terreno.");
    // Os pontos das rotinas (praça, poço, fogueira, casas) ficam livres: é onde o povo vive
    for (const auto& [nome, t] : locais)
        if (t.x >= tile.x - 1 && t.x <= tile.x + d.tamanho.x && t.y >= tile.y - 1 && t.y <= tile.y + d.tamanho.y + 1)
            return falha("Os moradores usam esse lugar (praça, poço, casas).");
    if (motivo) motivo->clear();
    return true;
}

std::string World::construir(const std::string& defId, glm::ivec2 tile) {
    std::string motivo;
    if (!podeConstruir(defId, tile, &motivo)) {
        eventos_.push_back({EventoJogo::Tipo::Aviso, jogador.pos, defId, "", 0, motivo});
        return "";
    }
    const BuildingDef& d = dados().construcoes.at(defId);
    const NivelConstrucao* n1 = d.nivel(1);
    if (!n1 || !inventario.pagar(n1->custo)) {
        eventos_.push_back({EventoJogo::Tipo::Aviso, jogador.pos, defId, "", 0, "Faltam recursos para a obra."});
        return "";
    }
    int n = 1;
    std::string id;
    do id = std::format("{}_{}", defId, n++);
    while (reino.construcao(id));
    BuildingData b{id, defId, n1->nome, 1, tile, true, false};  // constructed = false: em obras até amanhã
    reino.construcoes.push_back(b);
    criarEntidadeConstrucao(b);
    bloqueio_.clear();
    eventos_.push_back({EventoJogo::Tipo::ObraIniciada, pontoPorta(b), id, "", 1, n1->nome});
    return id;
}

void World::sincronizarAldeoes() {
    // Fase 18: cada aldeão na rua é uma pessoa do povo (no máximo 16 andando ao mesmo tempo)
    const int alvo = std::min(static_cast<int>(povo.size()), 16);
    std::vector<entt::entity> existentes;
    for (auto [e, a] : reg.view<Aldeao>().each()) existentes.push_back(e);
    while (static_cast<int>(existentes.size()) > alvo) {
        reg.destroy(existentes.back());
        existentes.pop_back();
    }
    for (size_t i = 0; i < existentes.size(); ++i) {
        Aldeao& a = reg.get<Aldeao>(existentes[i]);
        a.habitante = povo[i].id;
        a.variante = povo[i].variante;
    }
    std::vector<glm::ivec2> pontos;
    for (const auto& [n, t] : locais) pontos.push_back(t);
    if (pontos.empty()) return;
    std::uniform_int_distribution<size_t> qual(0, pontos.size() - 1);
    for (int i = static_cast<int>(existentes.size()); i < alvo; ++i) {
        const auto e = reg.create();
        const glm::ivec2 t = pontos[qual(rng_)];
        reg.emplace<Posicao>(e, glm::vec2{t.x * LADO + LADO / 2.0f, t.y * LADO + 12.0f});
        reg.emplace<Movimento>(e).velocidade = 30.0f;
        reg.emplace<Aldeao>(e, povo[i].variante, povo[i].id, 1.0f + i * 0.7f, false);
    }
}

void World::atualizarAldeoes(float dt) {
    std::vector<glm::ivec2> pontos;
    for (const auto& [n, t] : locais) pontos.push_back(t);
    for (const auto& b : reino.construcoes)
        if (b.constructed) {
            const glm::vec2 p = pontoPorta(b);
            pontos.push_back({static_cast<int>(p.x) / LADO, static_cast<int>(p.y) / LADO});
        }
    // Festival: o povo todo vai para a praça (e só dorme quando a festa acaba)
    const bool festa = festivalAgora();
    if (festa) {
        pontos.clear();
        for (const char* l : {"praca", "praca_fogueira", "poco", "taverna_ruina"})
            if (locais.contains(l)) pontos.push_back(locais.at(l));
        for (const glm::ivec2 l : dados().estacoes.festival.lugares) pontos.push_back(l + glm::ivec2(0, 2));
    }
    if (pontos.empty()) return;
    std::uniform_int_distribution<size_t> qual(0, pontos.size() - 1);
    std::uniform_real_distribution<float> espera(3.0f, 10.0f);
    const bool noite = relogio.minutos >= 21 * 60 && !festa;
    for (auto [e, p, mv, a] : reg.view<Posicao, Movimento, Aldeao>().each()) {
        a.dormindo = noite;  // de noite todo mundo está em casa
        if (a.dormindo) {
            mv.andando = false;
            continue;
        }
        if (mv.caminho.empty()) {
            mv.andando = false;
            a.espera -= dt;
            if (a.espera <= 0.0f) {
                a.espera = espera(rng_);
                caminharPara(e, pontos[qual(rng_)]);
            }
            continue;
        }
        seguirCaminho(p.v, mv, dt);
    }
}

void World::criarObjeto(char c, int x, int y) {
    const glm::vec2 base = baseTile(x, y);
    const uint32_t h = hashTile(x, y);
    auto novo = [&](const char* tipo) {
        const auto e = reg.create();
        reg.emplace<Posicao>(e, base);
        reg.emplace<IdMundo>(e, std::format("{}{}_{}_{}", prefixoIds_, tipo, x, y));
        return e;
    };
    switch (c) {
    case 'T': {  // árvore morta
        const auto e = novo("arvore");
        reg.emplace<Visual>(e, std::format("arvore_morta_{}", h % 3));
        reg.emplace<Colisor>(e, glm::vec2{4, 3}, glm::vec2{0, -2});
        reg.emplace<Coletavel>(e, "arvore", "madeira", 3, 3, 3, false, "toco", glm::vec2{4, 3}, reg.get<Visual>(e).sprite);
        break;
    }
    case 'R': {  // rocha
        const auto e = novo("rocha");
        reg.emplace<Visual>(e, std::format("rocha_{}", h % 2));
        reg.emplace<Colisor>(e, glm::vec2{6, 4}, glm::vec2{0, -4});
        reg.emplace<Coletavel>(e, "rocha", "pedra", 4, 4, 2, false, "", glm::vec2{0}, reg.get<Visual>(e).sprite);
        break;
    }
    case 'C': {  // baú esquecido
        const auto e = novo("bau");
        reg.emplace<Visual>(e, "bau");
        reg.emplace<Colisor>(e, glm::vec2{6, 4}, glm::vec2{0, -4});
        reg.emplace<Saqueavel>(e, "ouro", 6 + static_cast<int>(h % 7), false, "bau_aberto");
        break;
    }
    case 'G': {  // túmulo
        const auto e = novo("tumulo");
        reg.emplace<Visual>(e, std::format("tumulo_{}", h % 2));
        reg.emplace<Colisor>(e, glm::vec2{5, 3}, glm::vec2{0, -3});
        break;
    }
    case 't': {  // tocha
        const auto e = novo("tocha");
        reg.emplace<Visual>(e, "tocha_0");
        reg.emplace<Animado>(e, "tocha", 3, 9.0f);
        reg.emplace<Colisor>(e, glm::vec2{2, 2}, glm::vec2{0, -2});
        reg.emplace<Luz>(e).cor = luzTochas_;
        break;
    }
    case 'f': {  // cerca quebrada
        const auto e = novo("cerca");
        reg.emplace<Visual>(e, std::format("cerca_{}", h % 2));
        reg.emplace<Colisor>(e, glm::vec2{8, 2}, glm::vec2{0, -3});
        break;
    }
    case 'B': {  // arbusto seco (só enfeite)
        const auto e = novo("arbusto");
        reg.emplace<Visual>(e, std::format("arbusto_{}", h % 2));
        break;
    }
    case 'W': {  // poço em ruínas (vira fonte no Reino Renascido)
        const auto e = novo("poco");
        reg.emplace<Visual>(e, "poco");
        reg.emplace<Colisor>(e, glm::vec2{8, 5}, glm::vec2{0, -5});
        reg.emplace<TrocaPorEra>(e, 2, "fonte");
        break;
    }
    case 'F': {  // fogueira da praça
        const auto e = novo("fogueira");
        reg.emplace<Visual>(e, "fogueira_0");
        reg.emplace<Animado>(e, "fogueira", 4, 10.0f);
        reg.emplace<Colisor>(e, glm::vec2{6, 3}, glm::vec2{0, -3});
        reg.emplace<Luz>(e, glm::vec3{1.0f, 0.55f, 0.25f}, 80.0f, glm::vec2{0.0f, -6.0f});
        if (prefixoIds_.empty()) reg.emplace<Interativo>(e, "cozinha");  // fase 15: a fogueira da praça é onde se cozinha
        break;
    }
    case 'D':    // escada que desce (cripta)
    case 'U': {  // escada que sobe
        const auto e = reg.create();
        reg.emplace<Posicao>(e, base + glm::vec2(0, 2));
        reg.emplace<Visual>(e, c == 'D' ? "escada_desce" : "escada_sobe");
        reg.emplace<Decoracao>(e, glm::ivec2{1});
        break;
    }
    case 'K': {  // veio de cristal (minas)
        const auto e = novo("veio");
        reg.emplace<Visual>(e, "veio_cristal");
        reg.emplace<Colisor>(e, glm::vec2{6, 4}, glm::vec2{0, -4});
        reg.emplace<Coletavel>(e, "veio", "cristal", 5, 5, 1, false, "veio_vazio", glm::vec2{6, 4}, "veio_cristal");
        reg.emplace<Luz>(e, glm::vec3{0.45f, 0.8f, 1.0f}, 26.0f, glm::vec2{0.0f, -6.0f});
        break;
    }
    case 'A': {  // Coração da Floresta: altar tomado por raízes de cinza
        const auto e = reg.create();
        reg.emplace<Posicao>(e, base);
        reg.emplace<Visual>(e, "altar_corrompido");
        reg.emplace<Colisor>(e, glm::vec2{10, 5}, glm::vec2{0, -5});
        reg.emplace<Interativo>(e, "altar");
        reg.emplace<Luz>(e, glm::vec3{0.8f, 0.25f, 0.2f}, 30.0f, glm::vec2{0.0f, -10.0f});
        break;
    }
    case 'Z': {  // entulho que fecha a passagem das minas
        const auto e = reg.create();
        reg.emplace<Posicao>(e, base);
        reg.emplace<Visual>(e, "entulho");
        reg.emplace<Colisor>(e, glm::vec2{8, 6}, glm::vec2{0, -6});
        reg.emplace<Interativo>(e, "entulho");
        break;
    }
    case 'P': spawn = base; break;
    default: break;
    }
}

const BuildingDef* World::def(const BuildingData& b) const {
    if (!dados_) return nullptr;
    const auto it = dados_->construcoes.find(b.defId);
    return it == dados_->construcoes.end() ? nullptr : &it->second;
}

const std::string& World::funcaoDe(const BuildingData& b) const {
    static const std::string nenhuma;
    const BuildingDef* d = def(b);
    return d ? d->funcao : nenhuma;
}

const BuildingData* World::porFuncao(const std::string& funcao) const {
    for (const auto& b : reino.construcoes)
        if (funcaoDe(b) == funcao) return &b;
    return nullptr;
}

float World::efeitoMaximo(const std::string& qual) const {
    float v = 0.0f;
    for (const auto& b : reino.construcoes)
        if (const BuildingDef* d = def(b); b.constructed && d && d->nivel(b.level)) v = std::max(v, d->nivel(b.level)->efeito(qual));
    return v;
}

float World::efeitoTotal(const std::string& qual) const {
    float v = 0.0f;
    for (const auto& b : reino.construcoes)
        if (const BuildingDef* d = def(b); b.constructed && d && d->nivel(b.level)) v += d->nivel(b.level)->efeito(qual);
    return v;
}

bool World::contaComoObra(const BuildingData& b) const {
    const BuildingDef* d = def(b);
    return b.constructed && d && d->contaComoObra && !(d->ruinas && b.level < 2);
}

ke::AABB World::caixaConstrucao(const BuildingData& b) const {
    const BuildingDef* d = def(b);
    const glm::ivec2 t = d ? d->tamanho : glm::ivec2{1};
    return {glm::vec2(b.tile * LADO), glm::vec2((b.tile + t) * LADO)};
}

glm::vec2 World::pontoPorta(const BuildingData& b) const {
    const ke::AABB c = caixaConstrucao(b);
    // A Muralha Leste se usa por dentro, ao lado do portão
    if (funcaoDe(b) == "muralha") return {c.min.x - 10.0f, c.min.y + 5 * LADO + 2.0f};
    return {(c.min.x + c.max.x) * 0.5f, c.max.y + 4.0f};
}

// Fase 17: os obstáculos só mudam quando algo é coletado, renasce, sobe, é criado ou destruído, ou quando o herdeiro
// troca de região. A assinatura pega o que muda sozinho; o resto marca o cache como sujo (sujarObstaculos)
const std::vector<ke::AABB>& World::obstaculos() const {
    size_t assinatura = reg.view<const Colisor>().size() * 1000003u + std::hash<std::string>{}(regiaoAtual) + reino.construcoes.size() * 7919u;
    for (const auto& b : reino.construcoes) assinatura = assinatura * 31u + static_cast<size_t>(b.level) * 2u + (b.constructed ? 1u : 0u);
    if (obstaculosSujos_ || assinatura != assinaturaObstaculos_) {
        obstaculosCache_ = calcularObstaculos();
        assinaturaObstaculos_ = assinatura;
        obstaculosSujos_ = false;
    }
    return obstaculosCache_;
}

std::vector<ke::AABB> World::calcularObstaculos() const {
    std::vector<ke::AABB> v;
    for (auto [e, p, c] : reg.view<const Posicao, const Colisor>().each()) {
        glm::vec2 meia = c.meia;
        if (const auto* col = reg.try_get<Coletavel>(e); col && col->esgotado) meia = col->meiaEsgotado;
        if (meia.x <= 0.0f || meia.y <= 0.0f) continue;
        const glm::vec2 centro = p.v + c.desloc;
        v.push_back({centro - meia, centro + meia});
    }
    if (noVilarejo())  // as construções do reino só existem no vilarejo
        for (const auto& b : reino.construcoes) {
            if (funcaoDe(b) == "muralha") {
                // Fase 13: em ruínas não segura nada; de pé, os trechos de muro seguram todos, mas o vão do portão
                // deixa o herdeiro e o povo passarem (só as criaturas esbarram nele)
                if (b.level < 2) continue;
                const ke::AABB c = caixaConstrucao(b), g = caixaPortao();
                v.push_back({c.min, {c.max.x, g.min.y}});
                v.push_back({{c.min.x, g.max.y}, c.max});
                continue;
            }
            v.push_back(caixaConstrucao(b));
        }
    return v;
}

AlvoInteracao World::procurarAlvo() const {
    const glm::vec2 ponto = jogador.pos + vetor(jogador.dir) * 11.0f + glm::vec2(0.0f, -3.0f);
    AlvoInteracao melhor;
    float melhorDist = 1e9f;
    auto considerar = [&](entt::entity e, glm::vec2 alvo, float alcance, TipoInteracao tipo, std::string rotulo) {
        const float d = glm::distance(ponto, alvo);
        if (d <= alcance && d < melhorDist) {
            melhorDist = d;
            melhor = {e, tipo, std::move(rotulo), alvo};
        }
    };
    for (auto [e, p, c] : reg.view<const Posicao, const Coletavel>().each()) {
        if (c.esgotado) continue;
        const bool arvore = c.tipo == "arvore";
        considerar(e, p.v + glm::vec2(0, -4), 13.0f, arvore ? TipoInteracao::Cortar : TipoInteracao::Quebrar,
                   arvore ? "Cortar árvore" : c.tipo == "veio" ? "Minerar cristal" : "Quebrar rocha");
    }
    // Fase 11: o altar, o entulho e o chefe rendido
    for (auto [e, p, it] : reg.view<const Posicao, const Interativo>().each()) {
        std::string rotulo;
        float alcance = 19.0f;
        if (it.acao == "altar") rotulo = "Coração da Floresta";
        else if (it.acao == "entulho") rotulo = std::format("Escorar a passagem ({} madeira)", MADEIRA_ENTULHO);
        else if (it.acao == "cozinha") rotulo = "Cozinhar";
        else if (it.acao == "canteiro") rotulo = rotuloCanteiro(reg.get<Canteiro>(e).indice), alcance = 13.0f;
        else if (it.acao == "animal") {
            const Animal& a = reg.get<Animal>(e);
            const bool feito = carinhos.contains(a.chave) && carinhos.at(a.chave) == relogio.dia;
            rotulo = feito ? "Já recebeu carinho hoje" : a.tipo == "vaca" ? "Fazer carinho na vaca" : "Fazer carinho na galinha";
            alcance = 15.0f;
        }
        considerar(e, p.v + glm::vec2(0, it.acao == "canteiro" ? 0 : -5), alcance, TipoInteracao::Usar, rotulo);
    }
    for (auto [e, p, i] : reg.view<const Posicao, const Inimigo>().each())
        if (i.rendido) considerar(e, p.v + glm::vec2(0, -10), 26.0f, TipoInteracao::Decidir, "Decidir o destino de " + dados().inimigos.at(i.tipo).nome);
    for (auto [e, p, s] : reg.view<const Posicao, const Saqueavel>().each())
        if (!s.aberto)
            considerar(e, p.v + glm::vec2(0, -4), 13.0f, TipoInteracao::Saquear, reg.all_of<Meteorito>(e) ? "Recolher a estrela caída" : "Abrir baú");
    for (auto [e, p, r] : reg.view<const Posicao, const ConstrucaoRef>().each())
        if (const BuildingData* b = reino.construcao(r.id)) considerar(e, pontoPorta(*b), 15.0f, TipoInteracao::Construcao, b->name);
    for (auto [e, p, pi] : reg.view<const Posicao, const Pista>().each())
        if (const PistaDef* d = dados().lore.pista(pi.id)) considerar(e, p.v + glm::vec2(0, -3), 14.0f, TipoInteracao::Ler, "Ler: " + d->titulo);
    // Moradores têm prioridade quando estão perto: falar com alguém é o que se espera
    for (auto [e, p, m, a] : reg.view<const Posicao, const Morador, const Agenda>().each()) {
        if (a.dormindo) continue;
        const NpcDef& d = dados().npcs.at(m.id);
        const float dist = glm::distance(ponto, p.v + glm::vec2(0, -6));
        if (dist <= 16.0f && dist - 6.0f < melhorDist) {
            melhorDist = dist - 6.0f;
            melhor = {e, TipoInteracao::Conversar, "Falar com " + d.nome, p.v + glm::vec2(0, -6)};
        }
    }
    // Fase 18: o povo com nome também conversa (quem estiver mais perto da frente do herdeiro)
    for (auto [e, p, a] : reg.view<const Posicao, const Aldeao>().each())
            if (const Habitante* h = habitante(a.habitante); h && !a.dormindo) considerar(e, p.v + glm::vec2(0, -6), 14.0f, TipoInteracao::Habitante, "Falar com " + h->nome);
    // Fase 15: nada por perto e água logo à frente: dá para pescar
    if (melhor.tipo == TipoInteracao::Nenhuma) {
        const glm::ivec2 t{static_cast<int>(ponto.x) / LADO, static_cast<int>(ponto.y) / LADO};
        if (mapa.dentro(t.x, t.y) && mapa.em(t.x, t.y).terreno == T_AGUA) melhor = {entt::null, TipoInteracao::Pescar, "Pescar", ponto};
    }
    return melhor;
}

void World::executar(const AlvoInteracao& a) {
    const glm::vec2 delta = a.ponto - jogador.pos;
    // Vira para o alvo antes de agir
    if (std::abs(delta.x) > std::abs(delta.y)) jogador.dir = delta.x < 0 ? Direcao::Esquerda : Direcao::Direita;
    else jogador.dir = delta.y < 0 ? Direcao::Cima : Direcao::Baixo;

    switch (a.tipo) {
    case TipoInteracao::Cortar:
    case TipoInteracao::Quebrar: {
        auto& c = reg.get<Coletavel>(a.entidade);
        auto& v = reg.get<Visual>(a.entidade);
        jogador.acao = Jogador::DURACAO_ACAO;
        c.hp = std::max(0, c.hp - 1);
        v.tremor = 0.18f;
        v.piscar = 0.07f;
        eventos_.push_back({EventoJogo::Tipo::Golpe, a.ponto, c.tipo});
        if (c.hp == 0) {
            c.esgotado = true;
            c.diaEsgotado = relogio.dia;
            sujarObstaculos();  // o que sobra (toco, pedras) tem outro tamanho
            v.sprite = c.spriteEsgotado;
            bloqueio_.clear();  // o caminho dos moradores pode ter mudado
            // Veios das minas rendem conforme o destino do golem (as Minas Arcanas dão mais)
            int rende = c.rendimento;
            if (const TransformacaoRegiao* t = transformacao(regiaoAtual); c.tipo == "veio" && t && t->rendimentoVeio > 0) rende = t->rendimentoVeio;
            inventario.adicionar(c.recurso, rende);
            eventos_.push_back({EventoJogo::Tipo::Esgotou, a.ponto, c.tipo});
            eventos_.push_back({EventoJogo::Tipo::Ganhou, a.ponto, c.tipo, c.recurso, rende});
            if (c.tipo == "veio") marcar("primeiro_cristal");
        }
        break;
    }
    case TipoInteracao::Saquear: {
        auto& s = reg.get<Saqueavel>(a.entidade);
        jogador.acao = 0.2f;
        s.aberto = true;
        reg.get<Visual>(a.entidade).sprite = s.spriteAberto;
        inventario.adicionar(s.recurso, s.quantidade);
        eventos_.push_back({EventoJogo::Tipo::Ganhou, a.ponto, "bau", s.recurso, s.quantidade});
        if (const auto* m = reg.try_get<Meteorito>(a.entidade)) {
            marcar(std::format("meteorito:{}", m->dia));  // não volta a aparecer ao carregar
            marcar("estrela_caida");
        }
        break;
    }
    case TipoInteracao::Construcao:
        eventos_.push_back({EventoJogo::Tipo::AbrirConstrucao, a.ponto, reg.get<ConstrucaoRef>(a.entidade).id});
        break;
    case TipoInteracao::Conversar: {
        // O morador para e se vira para o herói
        reg.get<Agenda>(a.entidade).conversando = true;
        auto& mv = reg.get<Movimento>(a.entidade);
        mv.andando = false;
        const glm::vec2 d = jogador.pos - reg.get<Posicao>(a.entidade).v;
        if (std::abs(d.x) > std::abs(d.y)) mv.dir = d.x < 0 ? Direcao::Esquerda : Direcao::Direita;
        else mv.dir = d.y < 0 ? Direcao::Cima : Direcao::Baixo;
        eventos_.push_back({EventoJogo::Tipo::AbrirConversa, a.ponto, reg.get<Morador>(a.entidade).id});
        break;
    }
    case TipoInteracao::Ler: {
        // O fragmento vai para as Crônicas e some do chão (atualizarPistas o remove)
        const std::string id = reg.get<Pista>(a.entidade).id;
        jogador.acao = 0.2f;
        eventos_.push_back({EventoJogo::Tipo::LerPista, a.ponto, id});
        marcar("pista:" + id, dados().lore.pista(id)->titulo);
        break;
    }
    case TipoInteracao::Usar: {
        jogador.acao = 0.2f;
        const std::string acao = reg.get<Interativo>(a.entidade).acao;
        if (acao == "altar") usarCoracao();
        else if (acao == "entulho") escorarEntulho();
        else if (acao == "cozinha") eventos_.push_back({EventoJogo::Tipo::AbrirCozinha, a.ponto});
        else if (acao == "canteiro") usarCanteiro(reg.get<Canteiro>(a.entidade).indice);
        else if (acao == "animal") fazerCarinho(a.entidade);
        break;
    }
    case TipoInteracao::Pescar: iniciarPesca(a.ponto); break;
    case TipoInteracao::Habitante:
        if (auto* al = reg.try_get<Aldeao>(a.entidade)) {
            if (auto* mv = reg.try_get<Movimento>(a.entidade)) mv->caminho.clear(), mv->andando = false;
            al->espera = 6.0f;  // para e conversa
            eventos_.push_back({EventoJogo::Tipo::AbrirFicha, a.ponto, "", "", al->habitante});
        }
        break;
    case TipoInteracao::Decidir:
        if (const EscolhaDef* d = escolhaDe(a.entidade)) eventos_.push_back({EventoJogo::Tipo::AbrirEscolha, a.ponto, d->id});
        break;
    case TipoInteracao::Nenhuma: break;
    }
}

void World::passo(float dt, const EntradaMundo& in) {
    const auto& obst = obstaculos();
    // Recuo depois de levar dano
    if (glm::length(jogador.empurrao) > 1.0f) {
        jogador.pos += ke::moverComColisao(mapa, jogador.caixa(), jogador.empurrao * dt, obst, 0.0f);
        jogador.empurrao *= std::max(0.0f, 1.0f - 10.0f * dt);
    }
    jogador.invulneravel = std::max(0.0f, jogador.invulneravel - dt);
    // 1.16: esquiva (para onde anda, ou para a frente): um impulso curto, invulnerável quase o tempo todo
    jogador.recargaEsquiva = std::max(0.0f, jogador.recargaEsquiva - dt);
    if (in.esquivar && jogador.recargaEsquiva <= 0.0f && jogador.acao <= 0.0f && jogador.vida > 0) {
        jogador.esquiva = Jogador::DURACAO_ESQUIVA;
        jogador.recargaEsquiva = Jogador::RECARGA_ESQUIVA;
        jogador.dirEsquiva = glm::length(in.mover) > 0.1f ? glm::normalize(in.mover) : vetor(jogador.dir);
        jogador.carga = 0.0f;
        eventos_.push_back({EventoJogo::Tipo::Esquivou, jogador.pos, "inicio"});
    }
    if (jogador.esquiva > 0.0f) {
        jogador.pos += ke::moverComColisao(mapa, jogador.caixa(), jogador.dirEsquiva * jogador.velocidade * 2.6f * dt, obst, 0.0f);
        jogador.esquiva = std::max(0.0f, jogador.esquiva - dt);
        jogador.andando = true;
    } else {  // fase 18: na neve todo mundo anda mais devagar
        const float velocidade = jogador.velocidade;
        jogador.velocidade *= tempoAqui().velocidade;
        moverJogador(jogador, jogador.vida > 0 ? in.mover : glm::vec2(0.0f), dt, mapa, obst);
        jogador.velocidade = velocidade;
    }
    if (jogador.acao <= 0.0f) jogador.atacando = false, jogador.carregado = false;
    if (in.atacar && jogador.acao <= 0.0f && jogador.vida > 0 && jogador.esquiva <= 0.0f) golpearComEspada();
    // 1.16: segurando o ataque depois do golpe, a espada carrega; 1.19: com a carga cheia, o golpe forte sai sozinho
    // (sem precisar soltar), e quem continua segurando carrega o próximo
    if (in.atacarSegurando && jogador.acao <= 0.0f && jogador.vida > 0 && jogador.esquiva <= 0.0f) {
        jogador.carga += dt;
        if (jogador.carga >= Jogador::CARGA_CHEIA) {
            golpearComEspada(true);
            jogador.carga = 0.0f;
        }
    } else if (!in.atacarSegurando) {
        jogador.carga = 0.0f;
    }
    // Fase 15: ferido, o H come o prato que mais cura (e só depois a comida comum)
    const ReceitaDef* melhorPrato = nullptr;
    if (in.comer && jogador.vida < jogador.vidaMax)
        for (const ReceitaDef& r : dados().cotidiano.receitas)
            if (r.efeitos.vida > 0 && inventario.quantidade(r.id) > 0 && (!melhorPrato || r.efeitos.vida > melhorPrato->efeitos.vida)) melhorPrato = &r;
    if (in.comer && melhorPrato) {
        comerPrato(melhorPrato->id);
    } else if (in.comer) {
        if (jogador.vida >= jogador.vidaMax) {
            eventos_.push_back({EventoJogo::Tipo::Aviso, jogador.pos, "", "", 0, "Você não está ferido."});
        } else if (!inventario.remover("comida", 3)) {
            eventos_.push_back({EventoJogo::Tipo::Aviso, jogador.pos, "", "", 0, "Sem comida (precisa de 3)."});
        } else {
            const int antes = jogador.vida;
            jogador.vida = std::min(jogador.vidaMax, jogador.vida + 25);
            eventos_.push_back({EventoJogo::Tipo::Comeu, jogador.pos, "", "comida", jogador.vida - antes});
        }
    }
    for (auto [e, v] : reg.view<Visual>().each()) {
        v.tremor = std::max(0.0f, v.tremor - dt);
        v.piscar = std::max(0.0f, v.piscar - dt);
    }
    alvo_ = procurarAlvo();
    // Fase 15: pescando, o E é para puxar a linha (nada mais acontece até ela sair da água)
    const bool pescava = pesca_.ativa;  // o E que puxa a linha não pode lançar outra logo em seguida
    if (pescava) {
        atualizarPesca(dt, in);
        alvo_ = {};
    }
    // Toque em E no meio de um golpe fica guardado um instante, em vez de se perder
    if (in.interagir && !pescava) bufferInteragir_ = 0.25f;
    const bool golpeContinuo = in.interagirSegurando &&
                               (alvo_.tipo == TipoInteracao::Cortar || alvo_.tipo == TipoInteracao::Quebrar);
    if ((bufferInteragir_ > 0.0f || golpeContinuo) && jogador.acao <= 0.0f && alvo_.tipo != TipoInteracao::Nenhuma) {
        bufferInteragir_ = 0.0f;
        executar(alvo_);
        alvo_ = procurarAlvo();  // o alvo pode ter sumido (rocha quebrada, baú aberto)
    }
    bufferInteragir_ = std::max(0.0f, bufferInteragir_ - dt);
    if (relogio.avancar(dt) && !desmaioPendente_) {
        desmaioPendente_ = true;  // 2:00 da manhã: a cena escurece e chama dormir(true)
        eventos_.push_back({EventoJogo::Tipo::Desmaiou, jogador.pos});
    }
    atualizarMoradores(dt);
    atualizarAldeoes(dt);
    if (noVilarejo()) atualizarAnimais(dt);
    atualizarInimigos(dt, obst);
    atualizarProjeteis(dt);  // 1.16: tiros de quem ataca de longe
    atualizarBrasas(dt);     // 1.16: o chão em brasa do Golem
    // Fase 13: a horda chega na hora avisada; sem o herdeiro no vilarejo, as defesas lutam sozinhas
    if (hordaDia == relogio.dia && !batalha_ && relogio.minutos >= dados().defesa.hora * 60) {
        if (noVilarejo()) iniciarBatalha();
        else resolverHordaSemHeroi("longe");
    }
    if (batalha_) atualizarDefesa(dt);
    if (jogador.vida > 0) verificarSaidas();
    reino.atualizarEra(dt);
    // Virada de era: anuncia uma vez (o visual muda devagar, em onda)
    const int alvoEra = static_cast<int>(reino.eraAlvo());
    if (alvoEra > eraAnunciada_) {
        eraAnunciada_ = alvoEra;
        eventos_.push_back({EventoJogo::Tipo::EraMudou, jogador.pos, "", "", alvoEra, nomeEra(reino.eraAlvo())});
        for (int e = 1; e <= alvoEra; ++e) marcar(std::format("era:{}", e), nomeEra(static_cast<KingdomEra>(e)));
        reino.eraConquistada = std::max(reino.eraConquistada, alvoEra);
    }
    // Fase 10: o acontecimento da noite e o festival começam (anunciados uma vez por dia)
    if (const EventoDef* ev = eventoHoje(); ev && eventoAgora(ev->id) && avisoEvento_ != relogio.dia) {
        avisoEvento_ = relogio.dia;
        eventos_.push_back({EventoJogo::Tipo::EventoRaro, jogador.pos, ev->id, "", 0, ev->aviso});
        marcar("evento:" + ev->id, ev->nome);
    }
    if (festivalAgora() && avisoFestival_ != relogio.dia) {
        avisoFestival_ = relogio.dia;
        const FestivalDef& f = dados().estacoes.festival;
        eventos_.push_back({EventoJogo::Tipo::Festival, jogador.pos, "", "", 0, f.nome + " começou na praça!"});
        marcar(std::format("festival_era{}:{}", static_cast<int>(reino.eraAlvo()), relogio.ano()), std::to_string(relogio.ano()));
    }
    tempoJogado += dt;
}

// ---------- Regiões ----------

void World::entrarRegiao(const std::string& id, glm::ivec2 chegada, Direcao dir) {
    // Fase 13: o herdeiro saiu do vilarejo no meio da batalha; as defesas terminam sem ele
    if (batalha_ && noVilarejo() && id != regiaoAtual) resolverHordaSemHeroi("saiu");
    if (id != regiaoAtual) {
        const auto it = guardadas_.find(id);
        if (it == guardadas_.end()) return;
        // A região atual sai de cena e a nova entra (troca barata: só ponteiros internos)
        RegiaoGuardada& atual = guardadas_[regiaoAtual];
        std::swap(atual.mapa, mapa);
        std::swap(atual.reg, reg);
        RegiaoGuardada& nova = guardadas_.at(id);
        std::swap(nova.mapa, mapa);
        std::swap(nova.reg, reg);
        guardadas_.erase(id);
        regiaoAtual = id;
        bloqueio_.clear();
        if (noVilarejo()) {
            posicionarMoradores();  // o tempo passou enquanto o herói estava fora
            sincronizarAldeoes();
        }
    }
    jogador.pos = {chegada.x * LADO + LADO / 2.0f, chegada.y * LADO + 12.0f};
    jogador.dir = dir;
    jogador.empurrao = {0, 0};
    jogador.acao = 0.0f;
    visitadas.insert(id);
    if (const auto it = estadoRegioes.find(id); it != estadoRegioes.end()) it->second.ultimaVisita = relogio.dia;
    eventos_.push_back({EventoJogo::Tipo::MudouRegiao, jogador.pos, id, "", 0, nomeRegiao(id)});
    atualizarPistas();
    atualizarEspeciais();
    if (id != "vilarejo") marcar("visitou:" + id, regioes.at(id).nome);
    atualizarMissoes();
}

void World::verificarSaidas() {
    const glm::ivec2 t{static_cast<int>(jogador.pos.x) / LADO, static_cast<int>(jogador.pos.y - 1) / LADO};
    for (const Saida& s : infoAtual().saidas) {
        if (!s.requer.empty() && !marcos.contains(s.requer)) continue;  // passagem ainda fechada
        if (t.x >= s.tile.x && t.y >= s.tile.y && t.x < s.tile.x + s.tam.x && t.y < s.tile.y + s.tam.y) {
            const Saida copia = s;  // entrarRegiao troca a região (e a lista de saídas)
            entrarRegiao(copia.destino, copia.chegada, copia.dir);
            return;
        }
    }
}

float World::escuridaoMinAtual() const {
    if (const TransformacaoRegiao* t = transformacao(regiaoAtual)) return t->escuridaoMin;
    float minimo = infoAtual().escuridaoMin;
    if (infoAtual().curaComEra) minimo *= 1.0f - 0.2f * reino.eraVisual;  // a floresta clareia com o reino
    return minimo;
}

float World::escuridao() const { return std::max(relogio.escuridao(), escuridaoMinAtual()); }

int World::eraNoTile(glm::ivec2 t) const {
    if (!noVilarejo()) {
        // Fora do vilarejo a terra segue amaldiçoada, a não ser que tenha sido transformada
        const TransformacaoRegiao* tr = transformacao(regiaoAtual);
        if (!tr) return 0;
        const auto it = estadoRegioes.find(regiaoAtual);
        // Abandonada de novo, a cor vai embora antes das criaturas voltarem
        return it != estadoRegioes.end() && it->second.corrupcao >= tr->voltamAcima ? std::min(tr->era, 1) : tr->era;
    }
    const float s = reino.eraVisual;
    if (s <= 0.0f) return 0;
    // Atraso de cada tile: mais longe da casa do herdeiro = muda depois (com um pouco de acaso nas bordas)
    const float dist = glm::length(glm::vec2(t - centroReino_)) / 42.0f;
    const float atraso = std::min(0.98f, 0.8f * std::min(dist, 1.0f) + 0.18f * (hashTile(t.x, t.y) % 100) / 100.0f);
    const int teto = std::min(3, static_cast<int>(std::ceil(s)));
    return std::clamp(static_cast<int>(std::floor(s + 1.0f - atraso)), 0, teto);
}

glm::vec3 World::luzAmbiente() const {
    const InfoRegiao& i = infoAtual();
    const TransformacaoRegiao* t = transformacao(regiaoAtual);
    const float minimo = t ? t->escuridaoMin : i.escuridaoMin;
    const glm::vec3 ambiente = t ? t->ambiente : i.ambiente;
    glm::vec3 luz = relogio.luzAmbiente();
    // Penumbra da região, que ainda escurece mais à noite
    if (minimo > 0.0f) luz = glm::min(glm::mix(glm::vec3(1.0f), ambiente, minimo / 0.9f), luz);
    // Lua de Sangue: a noite fica vermelha
    if (eventoAgora("lua_sangue")) luz = glm::mix(luz, glm::vec3(0.62f, 0.16f, 0.16f), 0.6f * relogio.escuridao());
    return luz;
}

void World::dormir(bool desmaio) {
    sujarObstaculos();  // a noite cria e remove muita coisa (estrelas, criaturas, obras)
    int perda = 0;
    if (desmaio) {
        // Desmaiar de cansaço custa 10% do ouro (no máximo 50): alguém "cuidou" de você
        perda = std::min(50, inventario.quantidade("ouro") / 10);
        inventario.remover("ouro", perda);
    }
    const int ontem = relogio.dia;
    // Fase 13: dormir na noite da horda (antes dela ou no meio da batalha) deixa as defesas lutando sozinhas
    if (batalha_ || hordaDia == ontem) resolverHordaSemHeroi(batalha_ ? "dormiu" : "dormiu_antes");
    relogio.novoDia();
    desmaioPendente_ = false;
    derrotaPendente_ = false;
    // Acorda sempre em casa, no vilarejo (mesmo que tenha desmaiado na floresta)
    if (!noVilarejo()) {
        entrarRegiao("vilarejo", {0, 0}, Direcao::Baixo);
        std::erase_if(eventos_, [](const EventoJogo& e) { return e.tipo == EventoJogo::Tipo::MudouRegiao; });
    }
    if (const BuildingData* casa = reino.construcao("casa_herdeiro")) jogador.pos = pontoPorta(*casa) + glm::vec2(0, 8);
    jogador.dir = Direcao::Baixo;
    jogador.acao = 0.0f;
    jogador.atacando = false;
    jogador.vida = jogador.vidaMax;  // uma noite de sono cura tudo
    jogador.empurrao = {0, 0};
    buffDano = buffDefesa = 0;  // fase 15: o efeito dos pratos acaba com o sono
    pesca_ = {};
    // Fase 11: a noite nas terras de fora (antes das criaturas voltarem: quantas voltam depende da corrupção)
    std::vector<std::pair<std::string, int>> producaoRegioes;
    std::vector<std::string> notasRegioes;
    noiteDasRegioes(producaoRegioes, notasRegioes);
    renascerInimigos();
    // Tocos voltam a ser árvores e rochas reaparecem depois de alguns dias (em todas as regiões)
    int renasceram = 0;
    auto renascer = [&](entt::registry& r, bool vila) {
        for (auto [e, c, v, p] : r.view<Coletavel, Visual, Posicao>().each()) {
            if (!c.esgotado || relogio.dia - c.diaEsgotado < c.diasParaRenascer()) continue;
            // Onde agora há uma construção, a rocha não volta
            if (vila && std::ranges::any_of(reino.construcoes, [&](const BuildingData& b) {
                    const ke::AABB cx = caixaConstrucao(b);
                    return p.v.x >= cx.min.x && p.v.x <= cx.max.x && p.v.y >= cx.min.y && p.v.y <= cx.max.y + 16.0f;
                }))
                continue;
            c.esgotado = false;
            c.hp = c.hpMax;
            v.sprite = c.spriteNormal;
            sujarObstaculos();
            ++renasceram;
        }
    };
    renascer(reg, true);
    for (auto& [id, g] : guardadas_) renascer(g.reg, false);
    for (auto [e, r] : reg.view<Relacionamento>().each()) r.falouHoje = false;
    // Obras que começaram ontem ficam prontas durante a noite
    std::vector<std::string> prontas;
    std::vector<std::pair<std::string, std::string>> novas;  // (definição, nome) para as Crônicas
    for (auto& b : reino.construcoes)
        if (!b.constructed) {
            b.constructed = true;
            prontas.push_back(b.name);
            novas.emplace_back(b.defId, b.name);
        }
    // A noite do reino: impostos, produção, comida, felicidade e população
    // (a noite foi da estação de ontem: a primeira noite do inverno ainda é de outono)
    const int povoAntes = estado.populacao;
    ultimoRelatorio = simularDia(reino, dados(), estado, inventario, modificadorReino(Relogio{ontem}.estacao(), ontem));
    // Fase 18: quem chegou e quem partiu tem nome (a fome e o descontentamento escolhem quem vai)
    ajustarPovo(estado.populacao < povoAntes && std::ranges::any_of(ultimoRelatorio.notas, [](const std::string& n) { return n.starts_with("Faltou comida"); }),
                ultimoRelatorio.notas);
    for (const auto& [chave, n] : producaoRegioes) {
        inventario.adicionar(chave.substr(0, chave.find('|')), n);
        ultimoRelatorio.variacoes.push_back({chave, n});  // "madeira|Floresta Encantada"
    }
    for (const auto& n : notasRegioes) ultimoRelatorio.notas.push_back(n);
    // Fase 15: a horta e os bichos durante a noite
    sincronizarAnimais();
    noiteDoCotidiano(ontem, ultimoRelatorio.variacoes, ultimoRelatorio.notas);
    manhaDoPovo(ultimoRelatorio.notas);  // fase 18: depois da horta crescer (a chuva de hoje rega para amanhã)
    notasDaManha(ontem);
    noiteDoConselho(ultimoRelatorio.notas);
    noiteDasFaccoes(ultimoRelatorio.notas);  // fase 19: vizinhos, tratados, fama e o ouro que some do cofre
    manhaDaDefesa();
    manhaDaGuilda(ultimoRelatorio.notas);
    for (const auto& [defId, nome] : novas) marcar("construiu:" + defId, nome);
    for (const int n : {15, 25, 40})
        if (estado.populacao >= n) marcar(std::format("povo:{}", n), std::to_string(n));
    // O mercador viajante chega pela estrada leste nos dias de visita
    for (const auto& [id, d] : dados().npcs)
        if (d.visitante() && visitanteHoje(d)) {
            ultimoRelatorio.notas.push_back(d.nome + " armou a banca na praça hoje.");
            marcar("visita:" + id, d.nome);
        }
    manhaDoMercado(ultimoRelatorio.notas);  // fase 20: preços da manhã e encomendas que chegam
    manhaDasAlas(ultimoRelatorio.notas);    // fase 20: o empréstimo que volta
    noiteDaLinhagem(ultimoRelatorio.notas); // fase 21: nascimentos, maioridade, velhice e sucessão
    noiteDoMundo(ultimoRelatorio.notas);    // 1.14: aldeias, caravanas, viajantes e a terra tomada (até 2 notícias)
    noiteDaCoroa(ultimoRelatorio.notas);    // 1.18: a Quinta Era
    if (perda > 0) ultimoRelatorio.variacoes.push_back({"ouro (desmaio)", -perda});
    for (auto it = prontas.rbegin(); it != prontas.rend(); ++it) ultimoRelatorio.notas.insert(ultimoRelatorio.notas.begin(), "Obra concluída: " + *it + "!");
    bloqueio_.clear();
    posicionarMoradores();
    sincronizarAldeoes();
    for (auto [e, mv, a] : reg.view<Movimento, Aldeao>().each()) {
        a.dormindo = false;
        mv.caminho.clear();
    }
    if (renasceram > 0) ultimoRelatorio.notas.push_back(std::format("{} árvores e rochas voltaram a crescer.", renasceram));
    atualizarMeteorito();
    atualizarMissoes();  // ex.: "junte comida" pode ter se cumprido com a produção da noite
    eventos_.push_back({EventoJogo::Tipo::NovoDia, jogador.pos, desmaio ? "desmaio" : "", "ouro", perda, relogio.textoData()});
}

bool World::melhorarConstrucao(const std::string& id) {
    BuildingData* b = reino.construcao(id);
    const BuildingDef* d = b ? def(*b) : nullptr;
    if (!b || !d) return false;
    if (!b->constructed) {
        eventos_.push_back({EventoJogo::Tipo::Aviso, pontoPorta(*b), id, "", 0, "Ainda em obras: fica pronta amanhã."});
        return false;
    }
    if (const std::string falta = requisitoProximoNivel(*b); !falta.empty()) {  // fase 16: nível que espera um marco
        eventos_.push_back({EventoJogo::Tipo::Aviso, pontoPorta(*b), id, "", 0, falta});
        return false;
    }
    if (!melhorar(*b, *d, inventario)) {
        eventos_.push_back({EventoJogo::Tipo::Aviso, pontoPorta(*b), id, "", 0, avaliarMelhoria(*b, *d, inventario).motivo});
        return false;
    }
    eventos_.push_back({EventoJogo::Tipo::Construiu, pontoPorta(*b), id, "", b->level, b->name});
    if (funcaoDe(*b) == "casa") marcar(b->level == 2 ? "casa_reconstruida" : std::format("casa_n{}", b->level), b->name);
    if (funcaoDe(*b) == "muralha") {
        portaoVida = portaoVidaMax();  // muralha nova, portão novo
        bloqueio_.clear();
        marcar(std::format("muralha_n{}", b->level), b->name);
    }
    if (funcaoDe(*b) == "castelo") marcar(std::format("castelo_n{}", b->level), b->name);
    return true;
}

}
