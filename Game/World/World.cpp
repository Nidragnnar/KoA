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
        info.saidas.push_back(x);
    }
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
    derrotaPendente_ = false;

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
            reg.emplace<Enfeite>(e, en.value("era", 1));
            if (en.contains("luz")) {
                const auto& l = en["luz"];
                reg.emplace<Luz>(e, glm::vec3{l[0].get<float>(), l[1].get<float>(), l[2].get<float>()}, l[3].get<float>(),
                                 glm::vec2{0.0f, -12.0f});
            }
        }
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
    const int nomeados = static_cast<int>(dados().npcs.size());
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
    std::uniform_int_distribution<size_t> qual(0, pontos.size() - 1);
    std::uniform_real_distribution<float> espera(3.0f, 10.0f);
    const bool noite = relogio.minutos >= 21 * 60;
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
        for (const auto& b : reino.construcoes) v.push_back(caixaConstrucao(b));
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
                   arvore ? "Cortar árvore" : "Quebrar rocha");
    }
    for (auto [e, p, s] : reg.view<const Posicao, const Saqueavel>().each())
        if (!s.aberto) considerar(e, p.v + glm::vec2(0, -4), 13.0f, TipoInteracao::Saquear, "Abrir baú");
    for (auto [e, p, r] : reg.view<const Posicao, const ConstrucaoRef>().each())
        if (const BuildingData* b = reino.construcao(r.id)) considerar(e, pontoPorta(*b), 15.0f, TipoInteracao::Construcao, b->name);
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
            inventario.adicionar(c.recurso, c.rendimento);
            eventos_.push_back({EventoJogo::Tipo::Esgotou, a.ponto, c.tipo});
            eventos_.push_back({EventoJogo::Tipo::Ganhou, a.ponto, c.tipo, c.recurso, c.rendimento});
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
    if (jogador.vida > 0) verificarSaidas();
    reino.atualizarEra(dt);
    // Virada de era: anuncia uma vez (o visual muda devagar, em onda)
    const int alvoEra = static_cast<int>(reino.eraAlvo());
    if (alvoEra > eraAnunciada_) {
        eraAnunciada_ = alvoEra;
        eventos_.push_back({EventoJogo::Tipo::EraMudou, jogador.pos, "", "", alvoEra, nomeEra(reino.eraAlvo())});
    }
    tempoJogado += dt;
}

// ---------- Regiões ----------

void World::entrarRegiao(const std::string& id, glm::ivec2 chegada, Direcao dir) {
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
    eventos_.push_back({EventoJogo::Tipo::MudouRegiao, jogador.pos, id, "", 0, regioes.at(id).nome});
    atualizarMissoes();
}

void World::verificarSaidas() {
    const glm::ivec2 t{static_cast<int>(jogador.pos.x) / LADO, static_cast<int>(jogador.pos.y - 1) / LADO};
    for (const Saida& s : infoAtual().saidas) {
        if (t.x >= s.tile.x && t.y >= s.tile.y && t.x < s.tile.x + s.tam.x && t.y < s.tile.y + s.tam.y) {
            const Saida copia = s;  // entrarRegiao troca a região (e a lista de saídas)
            entrarRegiao(copia.destino, copia.chegada, copia.dir);
            return;
        }
    }
}

float World::escuridao() const {
    float minimo = infoAtual().escuridaoMin;
    if (infoAtual().curaComEra) minimo *= 1.0f - 0.2f * reino.eraVisual;  // a floresta clareia com o reino
    return std::max(relogio.escuridao(), minimo);
}

int World::eraNoTile(glm::ivec2 t) const {
    if (!noVilarejo()) return 0;  // as terras fora do vilarejo seguem amaldiçoadas
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
    if (i.escuridaoMin <= 0.0f) return relogio.luzAmbiente();
    // Penumbra da região, que ainda escurece mais à noite
    const glm::vec3 base = glm::mix(glm::vec3(1.0f), i.ambiente, i.escuridaoMin / 0.9f);
    return glm::min(base, relogio.luzAmbiente());
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
    const InimigoDef& d = dados().inimigos.at(tipo);
    const auto e = reg.create();
    const glm::vec2 p{tile.x * LADO + LADO / 2.0f, tile.y * LADO + 12.0f};
    reg.emplace<Posicao>(e, p);
    Inimigo& i = reg.emplace<Inimigo>(e);
    i.tipo = tipo;
    i.vida = i.vidaMax = d.vida;
    i.origem = i.destino = p;
    i.espera = 0.5f + (tile.x % 5) * 0.4f;
}

void World::renascerInimigos() {
    // Em todas as regiões: some quem sobrou e todos voltam ao lugar de origem
    // criarInimigo usa `reg`; para as regiões guardadas, a guardada entra em `reg` por um instante
    auto refazerAtivo = [&](const InfoRegiao& info) {
        std::vector<entt::entity> velhos;
        for (auto [e, i] : reg.view<Inimigo>().each()) velhos.push_back(e);
        reg.destroy(velhos.begin(), velhos.end());
        for (const auto& [tipo, tile] : info.spawns) {
            if (dados().inimigos.at(tipo).chefe && marcos.contains("chefe_" + tipo)) continue;  // o chefe não volta
            criarInimigo(tipo, tile);
        }
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
        if (i.ultimoGolpe == jogador.golpeId || i.vida <= 0) continue;
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
        if (i.vida <= 0) mortos.push_back(e);
    }
    for (const entt::entity e : mortos) {
        const Inimigo& i = reg.get<Inimigo>(e);
        const InimigoDef& d = dados().inimigos.at(i.tipo);
        const glm::vec2 p = reg.get<Posicao>(e).v;
        std::uniform_int_distribution<int> ouro(d.ouroMin, std::max(d.ouroMin, d.ouroMax));
        const int o = ouro(rng_);
        inventario.adicionar("ouro", o);
        ++abates[i.tipo];
        if (d.chefe) {
            marcos.insert("chefe_" + i.tipo);
            reino.magiaLiberta = true;  // com o Cavaleiro Caído derrotado, a magia pode voltar ao reino
        }
        eventos_.push_back({EventoJogo::Tipo::InimigoDerrotado, p, i.tipo, "ouro", o, d.nome});
        reg.destroy(e);
    }
    if (!mortos.empty()) atualizarMissoes();
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
    for (auto [e, p, i] : reg.view<Posicao, Inimigo>().each()) {
        const InimigoDef& d = dados().inimigos.at(i.tipo);
        i.piscar = std::max(0.0f, i.piscar - dt);
        i.recarga = std::max(0.0f, i.recarga - dt);
        const ke::AABB caixa{p.v - glm::vec2(5, 5), p.v + glm::vec2(5, 1)};
        // Recuo do golpe tem prioridade sobre andar
        if (glm::length(i.empurrao) > 1.0f) {
            p.v += ke::moverComColisao(mapa, caixa, i.empurrao * dt, obst, 0.0f);
            i.empurrao *= std::max(0.0f, 1.0f - 9.0f * dt);
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
            ferirHeroi(d.dano, p.v);
            i.recarga = 1.0f;
        }
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
        if (!m.item.empty() && dados().equipamentos.contains(m.item)) {
            equip.possuidos.insert(m.item);
            const EquipDef& d = dados().equipamentos.at(m.item);
            if (d.tipo == "arma" && d.dano > danoArma()) equip.arma = m.item;
            if (d.tipo == "armadura" && d.defesa > defesaArmadura()) equip.armadura = m.item;
        }
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
    for (auto [e, m, p, a] : reg.view<const Morador, Posicao, Agenda>().each()) {
        if (a.itemAtual < 0) continue;
        const ItemRotina& item = dados().npcs.at(m.id).rotina[a.itemAtual];
        const glm::ivec2 t = destinoDe(e, item);
        p.v = {t.x * LADO + LADO / 2.0f, t.y * LADO + 12.0f};
        a.dormindo = item.acao == "dormir";
    }
}

void World::atualizarMoradores(float dt) {
    for (auto [e, m, p, mv, a] : reg.view<const Morador, Posicao, Movimento, Agenda>().each()) {
        const NpcDef& def = dados().npcs.at(m.id);
        const ItemRotina* item = def.rotinaEm(relogio.minutos);
        if (!item) continue;
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
    ContextoFala ctx{relogio.periodo(), relogio.estacao(), 1, static_cast<int>(reino.eraAtual())};
    if (const BuildingData* casa = reino.construcao("casa_herdeiro")) ctx.nivelCasa = casa->level;
    const EntradaFala* fala = escolherFala(def, rel, ctx);
    c.npc = id;
    c.nome = def.nome;
    c.profissao = def.profissao;
    c.falas = fala ? fala->falas : std::vector<std::string>{"..."};
    // Amizade cresce uma vez por dia de conversa
    if (!rel.falouHoje) {
        rel.afinidade = std::min(1000, rel.afinidade + 25);
        c.ganhouAmizade = true;
    }
    rel.falouHoje = true;
    rel.conheceu = true;
    marcos.insert("primeira_conversa");
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
    for (auto& b : reino.construcoes)
        if (!b.constructed) {
            b.constructed = true;
            prontas.push_back(b.name);
        }
    // A noite do reino: impostos, produção, comida, felicidade e população
    ultimoRelatorio = simularDia(reino, dados(), estado, inventario);
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
    jogador.vida = std::clamp(d.vida, 1, jogador.vidaMax);
    renascerInimigos();  // o chefe derrotado não volta
    bloqueio_.clear();
    relogio.dia = std::max(1, d.dia);
    relogio.minutos = std::clamp(d.minutos, static_cast<float>(Relogio::INICIO_DIA), static_cast<float>(Relogio::FIM_DIA) - 1.0f);
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

}
