#include "Game/World/World.h"

#include <algorithm>
#include <format>
#include <sstream>

#include <glm/geometric.hpp>
#include <nlohmann/json.hpp>

#include "Engine/Platform/FileSystem.h"
#include "Engine/World/Pathfinding.h"
#include "Game/Exploration/Components.h"

namespace koa {

uint32_t hashTile(int x, int y) {
    uint32_t h = static_cast<uint32_t>(x) * 73856093u ^ static_cast<uint32_t>(y) * 19349663u;
    h ^= h >> 13;
    h *= 0x5bd1e995u;
    return h ^ (h >> 15);
}

namespace {

constexpr int LADO = 16;

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

glm::vec2 baseTile(int x, int y) { return {x * LADO + LADO / 2.0f, y * LADO + LADO - 2.0f}; }

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
    const bool ok = lerMapa(texto, info, erro);
    spawn = spawnVila;
    prefixoIds_.clear();
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
    rng_.seed(2026);
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
            reg.emplace<Enfeite>(e, en.value("era", 1), en.value("festival", false));
            if (en.contains("luz")) {
                const auto& l = en["luz"];
                reg.emplace<Luz>(e, glm::vec3{l[0].get<float>(), l[1].get<float>(), l[2].get<float>()}, l[3].get<float>(),
                                 glm::vec2{0.0f, -12.0f});
            }
        }
        // Fase 13: a faixa da Muralha Leste (e o chão logo atrás dela) fica sem árvores nem pedras
        if (const BuildingData* m = muralha()) limpar(m->tile - glm::ivec2(1, 0), {2, 7});
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
    sincronizarAldeoes();
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
    // O mercador é visitante: não conta como morador
    const int nomeados = static_cast<int>(std::ranges::count_if(dados().npcs, [](const auto& n) { return !n.second.visitante(); }));
    const int alvo = std::clamp(estado.populacao - nomeados, 0, 16);
    std::vector<entt::entity> existentes;
    for (auto [e, a] : reg.view<Aldeao>().each()) existentes.push_back(e);
    while (static_cast<int>(existentes.size()) > alvo) {
        reg.destroy(existentes.back());
        existentes.pop_back();
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
        reg.emplace<Aldeao>(e, i % 4, 1.0f + i * 0.7f, false);
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
        reg.emplace<Luz>(e);
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

ke::AABB World::caixaConstrucao(const BuildingData& b) const {
    const BuildingDef* d = def(b);
    const glm::ivec2 t = d ? d->tamanho : glm::ivec2{1};
    return {glm::vec2(b.tile * LADO), glm::vec2((b.tile + t) * LADO)};
}

glm::vec2 World::pontoPorta(const BuildingData& b) const {
    const ke::AABB c = caixaConstrucao(b);
    // A Muralha Leste se usa por dentro, ao lado do portão
    if (b.defId == "muralha") return {c.min.x - 10.0f, c.min.y + 5 * LADO + 2.0f};
    return {(c.min.x + c.max.x) * 0.5f, c.max.y + 4.0f};
}

std::vector<ke::AABB> World::obstaculos() const {
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
            if (b.defId == "muralha") {
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
    for (auto [e, p, it] : reg.view<const Posicao, const Interativo>().each())
        considerar(e, p.v + glm::vec2(0, -5), 19.0f, TipoInteracao::Usar,
                   it.acao == "altar" ? "Coração da Floresta" : std::format("Escorar a passagem ({} madeira)", MADEIRA_ENTULHO));
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
    case TipoInteracao::Usar:
        jogador.acao = 0.2f;
        if (reg.get<Interativo>(a.entidade).acao == "altar") usarCoracao();
        else escorarEntulho();
        break;
    case TipoInteracao::Decidir:
        if (const EscolhaDef* d = escolhaDe(a.entidade)) eventos_.push_back({EventoJogo::Tipo::AbrirEscolha, a.ponto, d->id});
        break;
    case TipoInteracao::Nenhuma: break;
    }
}

void World::passo(float dt, const EntradaMundo& in) {
    const auto obst = obstaculos();
    // Recuo depois de levar dano
    if (glm::length(jogador.empurrao) > 1.0f) {
        jogador.pos += ke::moverComColisao(mapa, jogador.caixa(), jogador.empurrao * dt, obst, 0.0f);
        jogador.empurrao *= std::max(0.0f, 1.0f - 10.0f * dt);
    }
    jogador.invulneravel = std::max(0.0f, jogador.invulneravel - dt);
    moverJogador(jogador, jogador.vida > 0 ? in.mover : glm::vec2(0.0f), dt, mapa, obst);
    if (jogador.acao <= 0.0f) jogador.atacando = false;
    if (in.atacar && jogador.acao <= 0.0f && jogador.vida > 0) golpearComEspada();
    if (in.comer) {
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
    // Toque em E no meio de um golpe fica guardado um instante, em vez de se perder
    if (in.interagir) bufferInteragir_ = 0.25f;
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
    atualizarInimigos(dt, obst);
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

// ---------- Combate ----------

int World::danoArma() const {
    const auto it = dados().equipamentos.find(equip.arma);
    return it == dados().equipamentos.end() ? 1 : it->second.dano;
}

int World::defesaArmadura() const {
    const auto it = dados().equipamentos.find(equip.armadura);
    return it == dados().equipamentos.end() ? 0 : it->second.defesa;
}

int World::nivelForja() const {
    int n = 0;
    for (const auto& b : reino.construcoes)
        if (b.defId == "forja" && b.constructed) n = std::max(n, b.level);
    return n;
}

bool World::forjar(const std::string& id) {
    const auto it = dados().equipamentos.find(id);
    if (it == dados().equipamentos.end() || it->second.forja <= 0) return false;
    const EquipDef& d = it->second;
    auto aviso = [&](std::string t) {
        eventos_.push_back({EventoJogo::Tipo::Aviso, jogador.pos, id, "", 0, std::move(t)});
        return false;
    };
    if (equip.possuidos.contains(id)) return aviso("Você já tem " + d.nome + ".");
    if (nivelForja() < d.forja) return aviso("A forja precisa estar no nível " + std::to_string(d.forja) + ".");
    if (!inventario.pagar(d.custo)) return aviso("Faltam recursos para " + d.nome + ".");
    equip.possuidos.insert(id);
    // Equipa na hora se for melhor que o atual
    if (d.tipo == "arma" && d.dano > danoArma()) equip.arma = id;
    if (d.tipo == "armadura" && d.defesa > defesaArmadura()) equip.armadura = id;
    eventos_.push_back({EventoJogo::Tipo::Forjou, jogador.pos, id, "", 0, d.nome});
    return true;
}

int World::nivelMercado() const {
    int n = 0;
    for (const auto& b : reino.construcoes)
        if (b.defId == "mercado" && b.constructed) n = std::max(n, b.level);
    return n;
}

int World::precoLote(const std::string& recurso) const {
    const int n = nivelMercado();
    if (n == 0) return 0;
    const int base = recurso == "pedra" ? 30 : recurso == "madeira" ? 20 : 0;
    static const float desconto[] = {1.0f, 1.0f, 0.85f, 0.7f};
    return static_cast<int>(base * desconto[std::min(n, 3)]);
}

bool World::comprarLote(const std::string& recurso) {
    const int preco = precoLote(recurso);
    if (preco <= 0) return false;
    if (!inventario.remover("ouro", preco)) {
        eventos_.push_back({EventoJogo::Tipo::Aviso, jogador.pos, "", "", 0, std::format("Precisa de {} de ouro.", preco)});
        return false;
    }
    inventario.adicionar(recurso, 10);
    eventos_.push_back({EventoJogo::Tipo::Ganhou, jogador.pos - glm::vec2(0, 10), "mercado", recurso, 10});
    return true;
}

void World::criarInimigo(const std::string& tipo, glm::ivec2 tile) {
    criarInimigoEm(tipo, {tile.x * LADO + LADO / 2.0f, tile.y * LADO + 12.0f});
}

entt::entity World::criarInimigoEm(const std::string& tipo, glm::vec2 p) {
    const InimigoDef& d = dados().inimigos.at(tipo);
    const auto e = reg.create();
    reg.emplace<Posicao>(e, p);
    Inimigo& i = reg.emplace<Inimigo>(e);
    i.tipo = tipo;
    i.vida = i.vidaMax = static_cast<int>(std::round(d.vida * estacaoAtual().inimigos));  // no inverno, mais resistentes
    i.origem = i.destino = p;
    i.espera = 0.5f + (static_cast<int>(p.x) / LADO % 5) * 0.4f;
    return e;
}

void World::renascerInimigos() {
    // Em todas as regiões: some quem sobrou e todos voltam ao lugar de origem
    // criarInimigo usa `reg`; para as regiões guardadas, a guardada entra em `reg` por um instante
    auto refazerAtivo = [&](const InfoRegiao& info) {
        std::vector<entt::entity> velhos;
        for (auto [e, i] : reg.view<Inimigo>().each()) velhos.push_back(e);
        reg.destroy(velhos.begin(), velhos.end());
        // Fase 11: quanto mais corrompida a região, mais criaturas voltam (sempre as mesmas, pelo lugar)
        const auto est = estadoRegioes.find(info.id);
        const bool viva = est != estadoRegioes.end();
        const float corrupcao = viva ? est->second.corrupcao : 100.0f;
        const TransformacaoRegiao* tr = viva ? transformacao(info.id) : nullptr;
        const int fracao = static_cast<int>(std::round(fracaoCriaturas(corrupcao) * 100.0f));
        for (const auto& [tipo, tile] : info.spawns) {
            const InimigoDef& d = dados().inimigos.at(tipo);
            if (d.chefe && marcos.contains("chefe_" + tipo)) continue;  // o chefe não volta
            if (tr && tr->semInimigos.contains(tipo) && corrupcao < tr->voltamAcima) continue;  // a terra purificada as afastou
            if (!d.chefe && static_cast<int>(hashTile(tile.x, tile.y) % 100) >= fracao) continue;
            criarInimigo(tipo, tile);
        }
        // Região tomada: chegam mais criaturas pelas bordas
        if (viva && corrupcao >= regra::INVASAO)
            for (const auto& [tipo, tile] : info.vida.invasao) criarInimigo(tipo, tile);
    };
    refazerAtivo(infoAtual());
    for (auto& [id, g] : guardadas_) {
        std::swap(reg, g.reg);
        refazerAtivo(regioes.at(id));
        std::swap(reg, g.reg);
    }
}

void World::golpearComEspada() {
    jogador.acao = Jogador::DURACAO_ATAQUE;
    jogador.atacando = true;
    ++jogador.golpeId;
    const glm::vec2 frente = vetor(jogador.dir);
    const glm::vec2 centro = jogador.pos + glm::vec2(0, -7) + frente * 13.0f;
    const glm::vec2 meia = (jogador.dir == Direcao::Esquerda || jogador.dir == Direcao::Direita) ? glm::vec2(11, 10) : glm::vec2(12, 9);
    const ke::AABB area{centro - meia, centro + meia};
    eventos_.push_back({EventoJogo::Tipo::Ataque, centro});
    std::vector<entt::entity> mortos;
    for (auto [e, p, i] : reg.view<Posicao, Inimigo>().each()) {
        if (i.ultimoGolpe == jogador.golpeId || i.vida <= 0 || i.rendido) continue;
        const InimigoDef& d = dados().inimigos.at(i.tipo);
        const glm::vec2 meiaIni = glm::vec2(d.tamanho) * glm::vec2(0.35f, 0.4f);
        const glm::vec2 ci = p.v - glm::vec2(0, d.tamanho.y * 0.4f);
        if (!area.intersecta({ci - meiaIni, ci + meiaIni})) continue;
        i.ultimoGolpe = jogador.golpeId;
        const int dano = danoContra(danoArma(), d.defesa);
        i.vida -= dano;
        i.piscar = 0.12f;
        i.empurrao = frente * (d.chefe ? 60.0f : 160.0f);
        i.perseguindo = true;
        eventos_.push_back({EventoJogo::Tipo::AcertouInimigo, ci, i.tipo, "", dano});
        // Fase 11: chefe com escolha se rende com 30% da vida e espera a decisão do herdeiro
        if (!d.escolha.empty() && i.vida <= i.vidaMax * 3 / 10) {
            i.vida = std::max(1, i.vida);
            i.rendido = true;
            i.perseguindo = false;
            i.empurrao = {0, 0};
            eventos_.push_back({EventoJogo::Tipo::AbrirEscolha, ci, d.escolha});
            continue;
        }
        if (i.vida <= 0) mortos.push_back(e);
    }
    for (const entt::entity e : mortos) derrotarInimigo(e);
    if (!mortos.empty()) atualizarMissoes();
}

void World::derrotarInimigo(entt::entity e) {
    const Inimigo& i = reg.get<Inimigo>(e);
    const InimigoDef& d = dados().inimigos.at(i.tipo);
    const glm::vec2 p = reg.get<Posicao>(e).v;
    std::uniform_int_distribution<int> ouro(d.ouroMin, std::max(d.ouroMin, d.ouroMax));
    int o = ouro(rng_);
    if (const EventoDef* ev = eventoHoje(); ev && eventoAgora(ev->id)) o = static_cast<int>(std::round(o * ev->ouroInimigos));
    inventario.adicionar("ouro", o);
    ++abates[i.tipo];
    if (i.horda) ++vencidasNaBatalha_;
    // Cada criatura vencida enfraquece a corrupção da região
    if (const auto it = estadoRegioes.find(regiaoAtual); it != estadoRegioes.end()) registrarAbate(it->second, transformacao(regiaoAtual));
    eventos_.push_back({EventoJogo::Tipo::InimigoDerrotado, p, i.tipo, "ouro", o, d.nome});
    if (d.chefe) {
        reino.magiaLiberta = true;  // com o Cavaleiro Caído derrotado, a magia pode voltar ao reino
        marcar("chefe_" + i.tipo, d.nome);
    }
    reg.destroy(e);
}

void World::ferirHeroi(int dano, glm::vec2 de) {
    if (jogador.invulneravel > 0.0f || jogador.vida <= 0) return;
    const int final = danoContra(dano, defesaArmadura());
    jogador.vida = std::max(0, jogador.vida - final);
    jogador.invulneravel = 0.9f;
    const glm::vec2 d = jogador.pos - de;
    jogador.empurrao = (glm::length(d) > 0.1f ? glm::normalize(d) : glm::vec2(0, 1)) * 170.0f;
    eventos_.push_back({EventoJogo::Tipo::HeroiFerido, jogador.pos, "", "", final});
    if (jogador.vida == 0 && !derrotaPendente_) {
        derrotaPendente_ = true;  // a cena escurece e chama reviver()
        eventos_.push_back({EventoJogo::Tipo::HeroiDerrotado, jogador.pos});
    }
}

void World::atualizarInimigos(float dt, const std::vector<ke::AABB>& obst) {
    std::uniform_real_distribution<float> u(-1.0f, 1.0f), espera(1.0f, 3.5f);
    // Fase 13: a horda só esbarra na muralha e no portão (passa por cima do resto, como uma enxurrada)
    std::vector<ke::AABB> barreiras;
    if (noVilarejo())
        if (const BuildingData* m = muralha(); m && m->level >= 2) {
            const ke::AABB c = caixaConstrucao(*m), g = caixaPortao();
            barreiras.push_back({c.min, {c.max.x, g.min.y}});
            barreiras.push_back({{c.min.x, g.max.y}, c.max});
            if (portaoDePe()) barreiras.push_back(g);
        }
    const glm::vec2 praca = glm::vec2(dados().defesa.praca) * static_cast<float>(LADO) + glm::vec2(8, 12);
    std::vector<entt::entity> saqueadores, soldadosCaidos;
    for (auto [e, p, i] : reg.view<Posicao, Inimigo>().each()) {
        const InimigoDef& d = dados().inimigos.at(i.tipo);
        i.piscar = std::max(0.0f, i.piscar - dt);
        i.recarga = std::max(0.0f, i.recarga - dt);
        if (i.rendido) {  // de joelhos: não anda nem fere
            i.andando = false;
            continue;
        }
        const ke::AABB caixa{p.v - glm::vec2(5, 5), p.v + glm::vec2(5, 1)};
        // Recuo do golpe tem prioridade sobre andar
        if (glm::length(i.empurrao) > 1.0f) {
            p.v += ke::moverComColisao(mapa, caixa, i.empurrao * dt, obst, 0.0f);
            i.empurrao *= std::max(0.0f, 1.0f - 9.0f * dt);
            continue;
        }
        if (i.horda) {
            // Alvo vivo mais perto (o herói ou um soldado); sem ninguém por perto, marcha para o portão ou para a praça
            glm::vec2 alvo{0.0f};
            float distAlvo = 1e9f;
            entt::entity soldadoAlvo = entt::null;
            if (jogador.vida > 0) alvo = jogador.pos, distAlvo = glm::distance(jogador.pos, p.v);
            for (auto [s, ps, so] : reg.view<const Posicao, const Soldado>().each())
                if (so.vida > 0 && glm::distance(ps.v, p.v) < distAlvo) alvo = ps.v, distAlvo = glm::distance(ps.v, p.v), soldadoAlvo = s;
            const bool persegue = distAlvo < d.visao;
            glm::vec2 destino = praca;
            const ke::AABB g = caixaPortao();
            if (persegue) destino = alvo;
            else if (portaoDePe()) destino = {g.max.x + 7.0f, std::clamp(p.v.y, g.min.y + 6.0f, g.max.y - 1.0f)};
            const glm::vec2 dirv = destino - p.v;
            i.andando = false;
            if (glm::length(dirv) > 2.0f) {
                const glm::vec2 feito = ke::moverComColisao(mapa, caixa, glm::normalize(dirv) * d.velocidade * dt, barreiras, 3.0f);
                p.v += feito;
                i.andando = glm::length(feito) > 0.01f;
                if (std::abs(dirv.x) > std::abs(dirv.y)) i.dir = dirv.x < 0 ? Direcao::Esquerda : Direcao::Direita;
                else i.dir = dirv.y < 0 ? Direcao::Cima : Direcao::Baixo;
            }
            if (i.andando) i.tempoAnim += dt;
            // Encostada no portão: golpeia até ele ceder
            if (!persegue && portaoDePe() && p.v.x - g.max.x < 14.0f && p.v.y > g.min.y && p.v.y < g.max.y + 6.0f && i.recarga <= 0.0f) {
                portaoVida = std::max(0, portaoVida - static_cast<int>(std::round(d.dano * forcaInimigos())));
                i.recarga = 1.2f;
                eventos_.push_back({EventoJogo::Tipo::PortaoGolpeado, {g.max.x, p.v.y - 8.0f}, i.tipo, "", portaoVida});
                if (portaoVida == 0) {
                    eventos_.push_back({EventoJogo::Tipo::PortaoCaiu, (g.min + g.max) * 0.5f, "", "", 0, "O portão leste caiu! As criaturas avançam para a praça!"});
                    bloqueio_.clear();
                }
            }
            // Chegou à praça: saqueia e foge com o que pegou
            if (!persegue && glm::distance(p.v, praca) < 22.0f) saqueadores.push_back(e);
            // Fere quem estiver encostado
            if (persegue && distAlvo < d.tamanho.x * 0.45f + 6.0f && i.recarga <= 0.0f) {
                const int dano = static_cast<int>(std::round(d.dano * forcaInimigos()));
                if (soldadoAlvo != entt::null) {
                    Soldado& so = reg.get<Soldado>(soldadoAlvo);
                    so.vida -= danoContra(dano, 2);
                    so.piscar = 0.12f;
                    if (so.vida <= 0) soldadosCaidos.push_back(soldadoAlvo);
                } else {
                    ferirHeroi(dano, p.v);
                }
                i.recarga = 1.0f;
            }
            continue;
        }
        const glm::vec2 paraHeroi = jogador.pos - p.v;
        const float dist = glm::length(paraHeroi);
        const bool longeDeCasa = glm::distance(p.v, i.origem) > 220.0f;
        if (!i.perseguindo && dist < d.visao && jogador.vida > 0) i.perseguindo = true;
        if (i.perseguindo && (dist > d.visao * 2.2f || longeDeCasa || jogador.vida <= 0)) i.perseguindo = false;

        glm::vec2 alvo = i.destino;
        float vel = d.velocidade;
        if (i.perseguindo) {
            alvo = jogador.pos;
            if (d.chefe) {  // de tempos em tempos, o chefe arranca em direção ao herói
                i.recargaInvestida -= dt;
                if (i.recargaInvestida <= 0.0f && dist < 110.0f) i.investida = 0.55f, i.recargaInvestida = 3.2f;
                if (i.investida > 0.0f) i.investida -= dt, vel *= 3.4f;
            }
        } else {
            if (glm::distance(p.v, i.destino) < 3.0f) {
                i.espera -= dt;
                if (i.espera <= 0.0f) {
                    i.destino = i.origem + glm::vec2(u(rng_), u(rng_)) * 40.0f;
                    i.espera = espera(rng_);
                }
                vel = 0.0f;
            }
            vel *= 0.5f;  // vagando, sem pressa
        }
        const glm::vec2 dirv = alvo - p.v;
        i.andando = false;
        if (vel > 0.0f && glm::length(dirv) > 2.0f) {
            const glm::vec2 passo = glm::normalize(dirv) * vel * dt;
            const glm::vec2 feito = ke::moverComColisao(mapa, caixa, passo, obst, 3.0f);
            p.v += feito;
            i.andando = glm::length(feito) > 0.01f;
            if (std::abs(dirv.x) > std::abs(dirv.y)) i.dir = dirv.x < 0 ? Direcao::Esquerda : Direcao::Direita;
            else i.dir = dirv.y < 0 ? Direcao::Cima : Direcao::Baixo;
            if (!i.andando && !i.perseguindo) i.destino = p.v;  // travou em algo: escolhe outro rumo depois
        }
        if (i.andando) i.tempoAnim += dt;
        // Encostou no herói: fere (e espera para ferir de novo)
        const float alcance = d.tamanho.x * 0.45f + 6.0f;
        if (dist < alcance && i.recarga <= 0.0f) {
            ferirHeroi(static_cast<int>(std::round(d.dano * forcaInimigos())), p.v);
            i.recarga = 1.0f;
        }
    }
    // Fase 13: quem chegou à praça leva o saque e some; soldado ferido sai da batalha
    for (const entt::entity e : saqueadores) {
        saquear(reg.get<Posicao>(e).v);
        reg.destroy(e);
    }
    for (const entt::entity s : soldadosCaidos) {
        eventos_.push_back({EventoJogo::Tipo::SoldadoCaiu, reg.get<Posicao>(s).v, "", "", 0, "Um soldado caiu ferido."});
        reg.destroy(s);
    }
}

void World::reviver() {
    // Derrotado: alguém do vilarejo o leva para casa; acorda no dia seguinte
    derrotaPendente_ = false;
    dormir(true);
    ultimoRelatorio.notas.insert(ultimoRelatorio.notas.begin(), "Você caiu em combate. Oswin o trouxe de volta para casa.");
}

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

void World::dormir(bool desmaio) {
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
    ultimoRelatorio = simularDia(reino, dados(), estado, inventario, modificadorReino(Relogio{ontem}.estacao()));
    for (const auto& [chave, n] : producaoRegioes) {
        inventario.adicionar(chave.substr(0, chave.find('|')), n);
        ultimoRelatorio.variacoes.push_back({chave, n});  // "madeira|Floresta Encantada"
    }
    for (const auto& n : notasRegioes) ultimoRelatorio.notas.push_back(n);
    notasDaManha(ontem);
    noiteDoConselho(ultimoRelatorio.notas);
    manhaDaDefesa();
    for (const auto& [defId, nome] : novas) marcar("construiu:" + defId, nome);
    for (const int n : {15, 25, 40})
        if (estado.populacao >= n) marcar(std::format("povo:{}", n), std::to_string(n));
    // O mercador viajante chega pela estrada leste nos dias de visita
    for (const auto& [id, d] : dados().npcs)
        if (d.visitante() && visitanteHoje(d)) {
            ultimoRelatorio.notas.push_back(d.nome + " armou a banca na praça hoje.");
            marcar("visita:" + id, d.nome);
        }
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
    if (!melhorar(*b, *d, inventario)) {
        eventos_.push_back({EventoJogo::Tipo::Aviso, pontoPorta(*b), id, "", 0, avaliarMelhoria(*b, *d, inventario).motivo});
        return false;
    }
    eventos_.push_back({EventoJogo::Tipo::Construiu, pontoPorta(*b), id, "", b->level, b->name});
    if (id == "casa_herdeiro") marcar(b->level == 2 ? "casa_reconstruida" : std::format("casa_n{}", b->level), b->name);
    if (b->defId == "muralha") {
        portaoVida = portaoVidaMax();  // muralha nova, portão novo
        bloqueio_.clear();
        marcar(std::format("muralha_n{}", b->level), b->name);
    }
    return true;
}

DadosSave World::capturar() const {
    DadosSave d;
    d.tempoJogado = tempoJogado;
    d.posJogador = jogador.pos;
    d.direcao = static_cast<int>(jogador.dir);
    d.inventario = inventario.todos();
    for (const auto& b : reino.construcoes) d.construcoes.push_back({b.id, b.defId, b.tile, true, b.level, b.constructed});
    d.eraVisual = reino.eraVisual;
    d.temPovo = true;
    d.populacao = estado.populacao;
    d.felicidade = estado.felicidade;
    d.imposto = static_cast<int>(estado.imposto);
    // Objetos e moradores de todas as regiões (a ativa e as guardadas)
    auto guardar = [&](const entt::registry& r) {
        for (auto [e, id] : r.view<const IdMundo>().each()) {
            if (const auto* c = r.try_get<Coletavel>(e); c && (c->hp < c->hpMax || c->esgotado))
                d.objetos.push_back({id.id, c->hp, c->esgotado, false, c->diaEsgotado});
            if (const auto* s = r.try_get<Saqueavel>(e); s && s->aberto) d.objetos.push_back({id.id, 0, false, true});
        }
        for (auto [e, m, rel] : r.view<const Morador, const Relacionamento>().each())
            d.moradores.push_back({m.id, rel.afinidade, rel.falouHoje, rel.conheceu, {rel.memoria.begin(), rel.memoria.end()}});
    };
    guardar(reg);
    for (const auto& [id, g] : guardadas_) guardar(g.reg);
    d.dia = relogio.dia;
    d.minutos = relogio.minutos;
    // Fase 6
    d.regiao = regiaoAtual;
    d.vida = jogador.vida;
    d.arma = equip.arma;
    d.armadura = equip.armadura;
    d.equipamentos = {equip.possuidos.begin(), equip.possuidos.end()};
    for (const auto& [id, m] : missoes) d.missoes[id] = {m.concluida, m.base};
    d.abates = abates;
    d.visitadas = {visitadas.begin(), visitadas.end()};
    d.marcos = {marcos.begin(), marcos.end()};
    d.temCronicas = true;
    d.cronicas = cronicas;
    d.temRegioes = true;
    d.regioes = estadoRegioes;
    d.conselho = conselho;
    d.lealdade = lealdade;
    d.leis = {leis.begin(), leis.end()};
    d.decisaoPendente = decisaoPendente;
    d.ultimaDecisao = ultimaDecisao;
    d.temDefesa = true;
    d.hordaDia = hordaDia;
    d.hordaNumero = hordaNumero;
    d.proximaHorda = proximaHorda;
    d.portaoVida = portaoVida;
    d.notasDefesa = notasDefesa;
    return d;
}

void World::aplicar(const DadosSave& d) {
    tempoJogado = d.tempoJogado;
    // Posição fora do mapa ou dentro de algo sólido → volta ao ponto inicial
    jogador.pos = d.posJogador;
    const bool foraDaVila = d.regiao != "vilarejo" && regioes.contains(d.regiao);
    if (!foraDaVila && ke::colide(mapa, jogador.caixa(), {})) jogador.pos = spawn;
    jogador.dir = static_cast<Direcao>(std::clamp(d.direcao, 0, 3));
    inventario = {};
    for (const auto& [id, q] : d.inventario) inventario.definir(id, q);
    for (const EstadoConstrucao& c : d.construcoes) {
        BuildingData* b = reino.construcao(c.id);
        if (!b) {
            // Obra colocada pelo jogador: recria no lugar salvo (se a definição ainda existir)
            const auto it = dados().construcoes.find(c.def);
            if (!c.temPosicao || it == dados().construcoes.end()) continue;
            reino.construcoes.push_back({c.id, c.def, "", 1, c.tile, true, c.construida});
            b = &reino.construcoes.back();
            criarEntidadeConstrucao(*b);
        }
        const BuildingDef* df = def(*b);
        if (!df) continue;
        if (const auto* nv = df->nivel(std::clamp(c.nivel, 1, df->nivelMax()))) {
            b->level = nv->nivel;
            b->name = nv->nome;
        }
        b->constructed = c.construida;
    }
    if (d.temPovo) {
        estado.populacao = d.populacao;
        estado.felicidade = d.felicidade;
        estado.imposto = static_cast<Imposto>(std::clamp(d.imposto, 0, 2));
    }
    reino.eraVisual = d.eraVisual;
    eraAnunciada_ = static_cast<int>(reino.eraAlvo());  // ao carregar, não anuncia de novo a era em que já está
    std::unordered_map<std::string, const EstadoObjeto*> porId;
    for (const auto& o : d.objetos) porId[o.id] = &o;
    auto restaurar = [&](entt::registry& r) {
        for (auto [e, id] : r.view<const IdMundo>().each()) {
            const auto it = porId.find(id.id);
            if (it == porId.end()) continue;
            if (auto* c = r.try_get<Coletavel>(e)) {
                c->hp = std::clamp(it->second->hp, 0, c->hpMax);
                c->esgotado = it->second->esgotado || c->hp == 0;
                c->diaEsgotado = it->second->diaEsgotado;
                if (c->esgotado) {
                    c->hp = 0;
                    r.get<Visual>(e).sprite = c->spriteEsgotado;
                }
            }
            if (auto* s = r.try_get<Saqueavel>(e); s && it->second->aberto) {
                s->aberto = true;
                r.get<Visual>(e).sprite = s->spriteAberto;
            }
        }
    };
    restaurar(reg);
    for (auto& [id, g] : guardadas_) restaurar(g.reg);
    // Fase 6: equipamento, missões e acontecimentos (validados contra os dados atuais)
    equip = {};
    for (const auto& id : d.equipamentos)
        if (dados().equipamentos.contains(id)) equip.possuidos.insert(id);
    if (equip.possuidos.contains(d.arma) && dados().equipamentos.at(d.arma).tipo == "arma") equip.arma = d.arma;
    if (equip.possuidos.contains(d.armadura) && dados().equipamentos.at(d.armadura).tipo == "armadura") equip.armadura = d.armadura;
    missoes.clear();
    for (const auto& [id, m] : d.missoes)
        if (std::ranges::any_of(dados().missoes, [&](const MissaoDef& x) { return x.id == id; })) missoes[id] = {m.first, m.second};
    abates = d.abates;
    visitadas = {d.visitadas.begin(), d.visitadas.end()};
    visitadas.insert("vilarejo");
    marcos = {d.marcos.begin(), d.marcos.end()};
    reino.magiaLiberta = marcos.contains("chefe_cavaleiro_caido");
    eraAnunciada_ = static_cast<int>(reino.eraAlvo());
    cronicas = d.cronicas;
    if (!d.temCronicas) reconstruirCronicas();
    verificarTitulos(true);  // títulos novos desta versão valem para o que já foi feito
    jogador.vida = std::clamp(d.vida, 1, jogador.vidaMax);
    relogio.dia = std::max(1, d.dia);
    relogio.minutos = std::clamp(d.minutos, static_cast<float>(Relogio::INICIO_DIA), static_cast<float>(Relogio::FIM_DIA) - 1.0f);
    // Fase 11: estado das regiões vivas (save antigo: como no mapa, visitadas hoje para não contar abandono)
    for (auto& [id, e] : estadoRegioes) {
        if (const auto it = d.regioes.find(id); d.temRegioes && it != d.regioes.end()) e = it->second;
        else e.ultimaVisita = relogio.dia;
        e.ultimaVisita = std::min(e.ultimaVisita, relogio.dia);
    }
    // Fase 12: conselho (validado contra os dados atuais: cargo, morador e lei precisam existir)
    conselho.clear();
    for (const auto& [cargo, npc] : d.conselho)
        if (dados().conselho.cargo(cargo) && dados().npcs.contains(npc) && !dados().npcs.at(npc).visitante()) conselho[cargo] = npc;
    lealdade.clear();
    for (const auto& [npc, l] : d.lealdade)
        if (dados().npcs.contains(npc)) lealdade[npc] = std::clamp(l, 0, 100);
    leis.clear();
    for (const auto& l : d.leis)
        if (dados().conselho.leis.contains(l)) leis.insert(l);
    decisaoPendente = dados().conselho.decisao(d.decisaoPendente) ? d.decisaoPendente : "";
    ultimaDecisao = std::min(d.ultimaDecisao, relogio.dia);
    // Fase 13: hordas e portão (a batalha em si não se salva: se a noite da horda for carregada depois das 20:00,
    // ela recomeça do portão)
    batalha_ = false;
    hordaDia = d.temDefesa && d.hordaDia >= relogio.dia ? d.hordaDia : 0;
    hordaNumero = d.hordaNumero;
    proximaHorda = d.proximaHorda;
    portaoVida = d.temDefesa ? std::clamp(d.portaoVida, 0, portaoVidaMax()) : portaoVidaMax();
    notasDefesa = d.notasDefesa;
    renascerInimigos();  // o chefe derrotado não volta (e a corrupção salva decide quantas criaturas há)
    bloqueio_.clear();
    for (const auto& m : d.moradores) {
        const entt::entity e = morador(m.id);
        if (e == entt::null) continue;  // morador que não existe mais nesta versão
        auto& r = reg.get<Relacionamento>(e);
        r.afinidade = std::clamp(m.afinidade, 0, 1000);
        r.falouHoje = m.falouHoje;
        r.conheceu = m.conheceu;
        r.memoria = {m.memoria.begin(), m.memoria.end()};
    }
    posicionarMoradores();
    sincronizarAldeoes();
    atualizarPistas();
    atualizarEspeciais();
    atualizarMeteorito();
    avisoEvento_ = avisoFestival_ = 0;
    // Salvou fora do vilarejo: volta para a mesma região e lugar
    if (foraDaVila) {
        const glm::vec2 p = d.posJogador;
        entrarRegiao(d.regiao, {static_cast<int>(p.x) / LADO, static_cast<int>(p.y) / LADO}, jogador.dir);
        jogador.pos = p;
        if (ke::colide(mapa, jogador.caixa(), {})) {  // lugar inválido: entrada da região
            const auto& s = infoAtual().saidas;
            if (!s.empty()) jogador.pos = {s.front().tile.x * LADO + 24.0f, s.front().tile.y * LADO + 12.0f};
        }
        tirarEventos();
    }
}

// ---------- Fase 9: Crônicas, títulos e o mistério ----------

bool World::marcar(const std::string& marco, const std::string& nome, bool silencioso) {
    if (!marcos.insert(marco).second) return false;
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
        if (b.constructed && b.defId != "casa" && !(b.defId == "muralha" && b.level < 2)) {  // ruínas não são obra
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
        if (b.constructed && b.defId != "casa" && b.defId != "muralha") marcar("construiu:" + b.defId, b.name, true);
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
    const auto obst = obstaculos();
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
    if (b.defId == "memorial") {
        l.push_back("Que Valdren não esqueça o que perdeu nem o que reencontrou.");
        for (const PistaDef& p : dados().lore.pistas)
            if (marcos.contains("pista:" + p.id)) l.push_back("- " + p.titulo);
        if (l.size() == 1) l.push_back("Ninguém ainda sabe o que causou a Queda das Cinzas.");
        else l.push_back(std::format("{} de {} fragmentos da Queda reunidos.", pistasEncontradas(), dados().lore.pistas.size()));
    } else if (b.defId == "estatua") {
        l.push_back(titulo().nome + ", herdeiro de Valdren.");
        if (const int d = diaDe("chefe_cavaleiro_caido"); d > 0) l.push_back(std::format("Derrotou o Cavaleiro Caído no dia {}.", d));
        else if (d == 0) l.push_back("Derrotou o Cavaleiro Caído.");
        int abatidos = 0;
        for (const auto& [tipo, n] : abates) abatidos += n;
        l.push_back(std::format("Criaturas vencidas: {}.", abatidos));
        l.push_back(std::format("Páginas nas Crônicas: {}.", cronicas.size()));
    } else if (b.defId == "jardim") {
        l.push_back("Aqui florescem os nomes de quem ergueu o reino.");
        for (auto [e, m, rel] : reg.view<const Morador, const Relacionamento>().each())
            if (rel.afinidade >= 300) l.push_back("- " + dados().npcs.at(m.id).nome + ", amigo do herdeiro");
        for (const MissaoDef& m : dados().missoes)
            if (const auto it = missoes.find(m.id); it != missoes.end() && it->second.concluida) l.push_back("- " + m.nome);
        if (l.size() == 1) l.push_back("Ainda há canteiros vazios. Faça amigos e ajude o povo.");
    }
    return l;
}

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

ModificadorDia World::modificadorReino(Estacao e) const {
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
    if (d.tipo == "arma" && d.dano > danoArma()) equip.arma = item;
    if (d.tipo == "armadura" && d.defesa > defesaArmadura()) equip.armadura = item;
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
    decisaoPendente.clear();  // o caso está resolvido antes de virar página nas Crônicas
    marcar("decisao:" + id + ":" + opcao, nome);
    eventos_.push_back({EventoJogo::Tipo::Decidiu, jogador.pos, id, opcao, 0, nome});
    return true;
}

// ---------- Fase 13: hordas e a defesa do portão leste ----------

const BuildingData* World::muralha() const {
    for (const auto& b : reino.construcoes)
        if (b.defId == "muralha") return &b;
    return nullptr;
}

int World::portaoVidaMax() const {
    const BuildingData* m = muralha();
    const BuildingDef* d = m ? def(*m) : nullptr;
    const NivelConstrucao* nv = d ? d->nivel(m->level) : nullptr;
    return nv && m->constructed ? static_cast<int>(nv->efeito("vida_portao")) : 0;
}

ke::AABB World::caixaPortao() const {
    const BuildingData* m = muralha();
    if (!m) return {{-100, -100}, {-99, -99}};
    // O vão do portão são a 4ª e a 5ª fileiras da muralha (a estrada)
    const ke::AABB c = caixaConstrucao(*m);
    return {{c.min.x, c.min.y + 3 * LADO}, {c.max.x, c.min.y + 5 * LADO}};
}

int World::danoTorres() const {
    int n = 0;
    for (const auto& b : reino.construcoes)
        if (b.defId == "torre" && b.constructed)
            if (const BuildingDef* d = def(b); d && d->nivel(b.level)) n += static_cast<int>(d->nivel(b.level)->efeito("dano_torre"));
    return n;
}

int World::soldadosDosPostos() const {
    int n = 0;
    for (const auto& b : reino.construcoes)
        if (b.defId == "posto_guarda" && b.constructed)
            if (const BuildingDef* d = def(b); d && d->nivel(b.level)) n += static_cast<int>(d->nivel(b.level)->efeito("soldados"));
    if (n > 0 && efeitoCargo("marechal") > 0.0f) ++n;  // o Marechal leva mais um para o portão
    return n;
}

int World::criaturasDaHorda() const {
    int n = 0;
    for (auto [e, i] : reg.view<const Inimigo>().each()) n += i.horda;
    return n;
}

int World::soldadosNoPortao() const {
    int n = 0;
    for (auto e : reg.view<const Soldado>()) n += e != entt::null;
    return n;
}

int World::forcaProximaHorda() const {
    const auto it = estadoRegioes.find("floresta");
    int f = forcaHorda(dados().defesa, hordaNumero + 1, it == estadoRegioes.end() ? 70.0f : it->second.corrupcao);
    if (const EventoDef* ev = eventoDoDia(hordaDia > 0 ? hordaDia : relogio.dia); ev && ev->id == "lua_sangue")
        f = f * 3 / 2;  // horda em noite de Lua de Sangue vem maior
    return f;
}

int World::defesaDoReino() const {
    return portaoVida / 8 + danoTorres() * 3 + soldadosDosPostos() * 6 + resumo().seguranca / 2;
}

void World::agendarHorda(int dia) { hordaDia = std::max(dia, relogio.dia); }

void World::iniciarBatalha() {
    if (batalha_ || !noVilarejo()) return;
    const DadosDefesa& dd = dados().defesa;
    const int forca = forcaProximaHorda();
    ++hordaNumero;
    batalha_ = true;
    vencidasNaBatalha_ = saquesNaBatalha_ = 0;
    recargaTorre_.clear();
    // As criaturas saem da estrada leste, em fileiras
    const auto criaturas = composicaoHorda(dd, forca);
    for (size_t k = 0; k < criaturas.size(); ++k) {
        const glm::ivec2 t = dd.entrada[k % dd.entrada.size()];
        const float atras = static_cast<float>(k / dd.entrada.size()) * 10.0f;
        const glm::vec2 p{std::min(t.x * LADO + LADO / 2.0f + atras, mapa.largura() * LADO - 4.0f), t.y * LADO + 12.0f};
        reg.get<Inimigo>(criarInimigoEm(criaturas[k], p)).horda = true;
    }
    // Os soldados dos postos correm para o portão
    const SoldadoDef& sd = dd.soldado;
    const int nSoldados = soldadosDosPostos();
    for (int k = 0; k < nSoldados && !dd.postoSoldados.empty(); ++k) {
        const glm::ivec2 t = dd.postoSoldados[k % dd.postoSoldados.size()];
        const glm::vec2 p{t.x * LADO + LADO / 2.0f - (k / static_cast<int>(dd.postoSoldados.size())) * 12.0f, t.y * LADO + 12.0f};
        const auto e = reg.create();
        reg.emplace<Posicao>(e, p);
        Soldado& s = reg.emplace<Soldado>(e);
        s.vida = s.vidaMax = sd.vida;
        s.posto = p;
        s.dir = Direcao::Direita;
    }
    eventos_.push_back({EventoJogo::Tipo::HordaChegou, jogador.pos, "", "", static_cast<int>(criaturas.size()),
                        std::format("Uma horda de {} criaturas chegou ao portão leste!", criaturas.size())});
}

void World::atualizarSoldados(float dt) {
    const SoldadoDef& sd = dados().defesa.soldado;
    const auto obst = obstaculos();
    std::vector<entt::entity> mortos;
    for (auto [e, p, s] : reg.view<Posicao, Soldado>().each()) {
        s.recarga = std::max(0.0f, s.recarga - dt);
        s.piscar = std::max(0.0f, s.piscar - dt);
        // A criatura da horda mais perto (dentro da vista); sem nenhuma, volta para o posto
        entt::entity alvo = entt::null;
        float dist = sd.visao;
        for (auto [ie, ip, ii] : reg.view<const Posicao, const Inimigo>().each())
            if (ii.horda && ii.vida > 0 && glm::distance(ip.v, p.v) < dist) dist = glm::distance(ip.v, p.v), alvo = ie;
        const glm::vec2 destino = alvo != entt::null ? reg.get<Posicao>(alvo).v : s.posto;
        const glm::vec2 dirv = destino - p.v;
        s.andando = false;
        if (glm::length(dirv) > (alvo != entt::null ? 12.0f : 3.0f)) {
            const ke::AABB caixa{p.v - glm::vec2(4, 4), p.v + glm::vec2(4, 1)};
            const glm::vec2 feito = ke::moverComColisao(mapa, caixa, glm::normalize(dirv) * sd.velocidade * dt, obst, 3.0f);
            p.v += feito;
            s.andando = glm::length(feito) > 0.01f;
            if (std::abs(dirv.x) > std::abs(dirv.y)) s.dir = dirv.x < 0 ? Direcao::Esquerda : Direcao::Direita;
            else s.dir = dirv.y < 0 ? Direcao::Cima : Direcao::Baixo;
        }
        if (s.andando) s.tempoAnim += dt;
        // Lança na criatura encostada
        if (alvo != entt::null && dist < 16.0f && s.recarga <= 0.0f) {
            Inimigo& i = reg.get<Inimigo>(alvo);
            const int dano = danoContra(sd.dano, dados().inimigos.at(i.tipo).defesa);
            i.vida -= dano;
            i.piscar = 0.12f;
            s.recarga = sd.recarga;
            eventos_.push_back({EventoJogo::Tipo::AcertouInimigo, reg.get<Posicao>(alvo).v - glm::vec2(0, 8), i.tipo, "", dano});
            if (i.vida <= 0 && std::ranges::find(mortos, alvo) == mortos.end()) mortos.push_back(alvo);
        }
    }
    for (const entt::entity e : mortos) derrotarInimigo(e);
}

void World::atualizarDefesa(float dt) {
    const DadosDefesa& dd = dados().defesa;
    // Torres: uma flecha na criatura mais perto, a cada poucos segundos
    std::vector<entt::entity> mortos;
    for (const auto& b : reino.construcoes) {
        if (b.defId != "torre" || !b.constructed) continue;
        const BuildingDef* d = def(b);
        const NivelConstrucao* nv = d ? d->nivel(b.level) : nullptr;
        if (!nv) continue;
        float& rc = recargaTorre_[b.id];
        rc -= dt;
        if (rc > 0.0f) continue;
        const ke::AABB c = caixaConstrucao(b);
        const glm::vec2 origem{(c.min.x + c.max.x) * 0.5f, c.min.y - 12.0f};
        entt::entity alvo = entt::null;
        float dist = dd.alcanceTorre;
        for (auto [ie, ip, ii] : reg.view<const Posicao, const Inimigo>().each())
            if (ii.horda && ii.vida > 0 && glm::distance(ip.v, origem) < dist) dist = glm::distance(ip.v, origem), alvo = ie;
        if (alvo == entt::null) continue;
        Inimigo& i = reg.get<Inimigo>(alvo);
        const glm::vec2 p = reg.get<Posicao>(alvo).v;
        const int dano = danoContra(static_cast<int>(nv->efeito("dano_torre")), dados().inimigos.at(i.tipo).defesa);
        i.vida -= dano;
        i.piscar = 0.12f;
        rc = dd.recargaTorre;
        eventos_.push_back({EventoJogo::Tipo::Flecha, origem, b.id, "", dano, "", p - glm::vec2(0, 8)});
        eventos_.push_back({EventoJogo::Tipo::AcertouInimigo, p - glm::vec2(0, 8), i.tipo, "", dano});
        if (i.vida <= 0 && std::ranges::find(mortos, alvo) == mortos.end()) mortos.push_back(alvo);
    }
    for (const entt::entity e : mortos) derrotarInimigo(e);
    atualizarSoldados(dt);
    if (criaturasDaHorda() == 0) terminarBatalha();
}

void World::saquear(glm::vec2 onde) {
    const DadosDefesa& dd = dados().defesa;
    for (const auto& [r, q] : dd.saque) inventario.remover(r, std::min(q, inventario.quantidade(r)));
    estado.felicidade = std::max(0.0f, estado.felicidade - dd.saqueFelicidade);
    ++saquesNaBatalha_;
    eventos_.push_back({EventoJogo::Tipo::Saque, onde, "", "", 0, "Uma criatura saqueou a praça!"});
}

void World::terminarBatalha() {
    const DadosDefesa& dd = dados().defesa;
    batalha_ = false;
    std::vector<entt::entity> soldados;
    for (auto e : reg.view<Soldado>()) soldados.push_back(e);
    reg.destroy(soldados.begin(), soldados.end());
    const int bonus = vencidasNaBatalha_ * dd.ouroPorCriatura;
    inventario.adicionar("ouro", bonus);
    const bool limpa = saquesNaBatalha_ == 0;
    std::string texto = limpa ? std::format("Batalha do Portão: vitória! {} criaturas vencidas; o povo juntou {} de ouro para os defensores.",
                                            vencidasNaBatalha_, bonus)
                              : std::format("Batalha do Portão: a horda foi vencida, mas {} criatura{} saqueou a praça.", saquesNaBatalha_,
                                            saquesNaBatalha_ > 1 ? "s" : "");
    notasDefesa.push_back(texto);
    if (portaoVidaMax() > 0 && portaoVida < portaoVidaMax())
        notasDefesa.push_back(std::format("O portão ficou com {} de {} de vida. Repare no painel da muralha.", portaoVida, portaoVidaMax()));
    marcar(std::string(limpa ? "horda:" : "horda_saqueada:") + std::to_string(hordaNumero), std::to_string(relogio.dia));
    eventos_.push_back({EventoJogo::Tipo::BatalhaFim, jogador.pos, limpa ? "vitoria" : "saque", "ouro", bonus, texto});
    hordaDia = 0;
    proximaHorda = relogio.dia + dd.intervaloMin + static_cast<int>(hashTile(hordaNumero, 77) % (dd.intervaloMax - dd.intervaloMin + 1));
}

void World::resolverHordaSemHeroi(const std::string& motivo) {
    const DadosDefesa& dd = dados().defesa;
    // A força é a das criaturas que ainda estão na batalha, ou a da horda inteira (se ela nem começou com o herdeiro)
    int forca = 0;
    if (batalha_) {
        std::vector<entt::entity> v;
        for (auto [e, i] : reg.view<const Inimigo>().each())
            if (i.horda) {
                v.push_back(e);
                for (const auto& [tipo, pontos] : dd.custo)
                    if (tipo == i.tipo) forca += pontos;
            }
        reg.destroy(v.begin(), v.end());
        std::vector<entt::entity> soldados;
        for (auto e : reg.view<Soldado>()) soldados.push_back(e);
        reg.destroy(soldados.begin(), soldados.end());
    } else {
        forca = forcaProximaHorda();
        ++hordaNumero;
        vencidasNaBatalha_ = saquesNaBatalha_ = 0;
    }
    batalha_ = false;
    const ResultadoHorda r = resolverSemHeroi(defesaDoReino(), forca * 6, portaoVida);
    portaoVida = std::max(0, portaoVida - r.danoPortao);
    for (int k = 0; k < r.invasores; ++k) saquear(glm::vec2(dd.praca) * static_cast<float>(LADO));
    const char* onde = motivo == "longe" || motivo == "saiu" ? "Com o herdeiro longe, a horda atacou o portão leste."
                                                             : "A horda atacou o portão leste enquanto o herdeiro dormia.";
    notasDefesa.push_back(r.vitoria ? std::string(onde) + " As defesas seguraram sozinhas."
                                    : std::format("{} As defesas não bastaram: {} criatura{} saqueou a praça.", onde, saquesNaBatalha_,
                                                  saquesNaBatalha_ > 1 ? "s" : ""));
    if (portaoVidaMax() > 0 && portaoVida < portaoVidaMax())
        notasDefesa.push_back(std::format("O portão ficou com {} de {} de vida. Repare no painel da muralha.", portaoVida, portaoVidaMax()));
    marcar(std::string(saquesNaBatalha_ == 0 ? "horda:" : "horda_saqueada:") + std::to_string(hordaNumero), std::to_string(relogio.dia));
    if (motivo == "longe" || motivo == "saiu")
        eventos_.push_back({EventoJogo::Tipo::Aviso, jogador.pos, r.vitoria ? "bom" : "", "", 0,
                            r.vitoria ? "A horda atacou Valdren; as defesas seguraram sem você." : "A horda atacou Valdren sem você e saqueou a praça!"});
    hordaDia = 0;
    proximaHorda = relogio.dia + dd.intervaloMin + static_cast<int>(hashTile(hordaNumero, 77) % (dd.intervaloMax - dd.intervaloMin + 1));
}

void World::manhaDaDefesa() {
    const DadosDefesa& dd = dados().defesa;
    auto& notas = ultimoRelatorio.notas;
    for (const auto& n : notasDefesa) notas.push_back(n);
    notasDefesa.clear();
    // O Mestre das Obras conserta parte do portão durante a noite
    const int max = portaoVidaMax();
    if (portaoVida < max && efeitoCargo("obras") > 0.0f) {
        const int conserto = std::max(1, static_cast<int>(std::round((max - portaoVida) * dd.consertoMestreObras)));
        portaoVida = std::min(max, portaoVida + conserto);
        notas.push_back(std::format("O Mestre das Obras consertou parte do portão (+{} de vida).", conserto));
    }
    // Os batedores avistam a próxima horda
    if (hordaDia == 0 && relogio.dia >= std::max(dd.diaMin, proximaHorda) && estado.populacao >= dd.populacaoMin) {
        const int aviso = dd.avisoDias + (efeitoCargo("marechal") > 0.0f ? 1 : 0);  // o Marechal tem batedores melhores
        agendarHorda(relogio.dia + aviso);
        notas.push_back(std::format("Batedores avistaram uma horda na floresta: ela ataca o portão leste em {} dias, às {}:00. "
                                    "Reforce a muralha, as torres e os postos da guarda.",
                                    aviso, dd.hora));
        marcar("horda_avisada");
    } else if (hordaDia == relogio.dia) {
        notas.push_back(std::format("A horda chega hoje, às {}:00! Fique no vilarejo para lutar no portão.", dd.hora));
    }
}

Custo World::custoReparo() const {
    const int falta = portaoVidaMax() - portaoVida;
    if (falta <= 0) return {};
    return {{"pedra", (falta + 9) / 10}, {"madeira", (falta + 19) / 20}};
}

bool World::repararPortao() {
    const Custo c = custoReparo();
    if (c.empty() || batalha_) return false;
    if (!inventario.pagar(c)) {
        eventos_.push_back({EventoJogo::Tipo::Aviso, jogador.pos, "", "", 0, "Faltam pedra e madeira para o reparo."});
        return false;
    }
    portaoVida = portaoVidaMax();
    eventos_.push_back({EventoJogo::Tipo::Aviso, jogador.pos, "bom", "", 0, "O portão está inteiro de novo."});
    return true;
}

}
