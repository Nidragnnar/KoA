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
    return carregarDeTexto(dados, *texto, j, erro);
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
                criarObjeto(c, x, y);
            }
            ke::Tile& tile = mapa.em(x, y);
            tile.terreno = static_cast<uint16_t>(t);
            tile.variante = static_cast<uint8_t>(hashTile(x, y) % 4);
            tile.solido = t == T_AGUA || t == T_MURO;
        }
    }

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
    } catch (const std::exception& ex) {
        if (erro) *erro = std::string("vilarejo.json: ") + ex.what();
        return false;
    }
    jogador.pos = spawn;
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
        reg.emplace<IdMundo>(e, std::format("{}_{}_{}", tipo, x, y));
        return e;
    };
    switch (c) {
    case 'T': {  // árvore morta
        const auto e = novo("arvore");
        reg.emplace<Visual>(e, std::format("arvore_morta_{}", h % 3));
        reg.emplace<Colisor>(e, glm::vec2{4, 3}, glm::vec2{0, -2});
        reg.emplace<Coletavel>(e, "arvore", "madeira", 3, 3, 3, false, "toco", glm::vec2{4, 3});
        break;
    }
    case 'R': {  // rocha
        const auto e = novo("rocha");
        reg.emplace<Visual>(e, std::format("rocha_{}", h % 2));
        reg.emplace<Colisor>(e, glm::vec2{6, 4}, glm::vec2{0, -4});
        reg.emplace<Coletavel>(e, "rocha", "pedra", 4, 4, 2, false, "", glm::vec2{0});
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
    case 'W': {  // poço em ruínas
        const auto e = novo("poco");
        reg.emplace<Visual>(e, "poco");
        reg.emplace<Colisor>(e, glm::vec2{8, 5}, glm::vec2{0, -5});
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
    moverJogador(jogador, in.mover, dt, mapa, obst);
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
    reino.atualizarEra(dt);
    tempoJogado += dt;
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
    ContextoFala ctx{relogio.periodo(), relogio.estacao(), 1};
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
    if (fala && fala->umaVez) rel.memoria.insert(fala->id);
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
    if (const BuildingData* casa = reino.construcao("casa_herdeiro")) jogador.pos = pontoPorta(*casa) + glm::vec2(0, 8);
    jogador.dir = Direcao::Baixo;
    jogador.acao = 0.0f;
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
    for (auto [e, id] : reg.view<const IdMundo>().each()) {
        if (const auto* c = reg.try_get<Coletavel>(e); c && (c->hp < c->hpMax || c->esgotado))
            d.objetos.push_back({id.id, c->hp, c->esgotado, false});
        if (const auto* s = reg.try_get<Saqueavel>(e); s && s->aberto) d.objetos.push_back({id.id, 0, false, true});
    }
    d.dia = relogio.dia;
    d.minutos = relogio.minutos;
    for (auto [e, m, r] : reg.view<const Morador, const Relacionamento>().each())
        d.moradores.push_back({m.id, r.afinidade, r.falouHoje, r.conheceu, {r.memoria.begin(), r.memoria.end()}});
    return d;
}

void World::aplicar(const DadosSave& d) {
    tempoJogado = d.tempoJogado;
    // Posição fora do mapa ou dentro de algo sólido → volta ao ponto inicial
    jogador.pos = d.posJogador;
    if (ke::colide(mapa, jogador.caixa(), {})) jogador.pos = spawn;
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
    std::unordered_map<std::string, const EstadoObjeto*> porId;
    for (const auto& o : d.objetos) porId[o.id] = &o;
    for (auto [e, id] : reg.view<const IdMundo>().each()) {
        const auto it = porId.find(id.id);
        if (it == porId.end()) continue;
        if (auto* c = reg.try_get<Coletavel>(e)) {
            c->hp = std::clamp(it->second->hp, 0, c->hpMax);
            c->esgotado = it->second->esgotado || c->hp == 0;
            if (c->esgotado) {
                c->hp = 0;
                reg.get<Visual>(e).sprite = c->spriteEsgotado;
            }
        }
        if (auto* s = reg.try_get<Saqueavel>(e); s && it->second->aberto) {
            s->aberto = true;
            reg.get<Visual>(e).sprite = s->spriteAberto;
        }
    }
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
}

}
