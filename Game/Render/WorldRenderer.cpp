#include "Game/Render/WorldRenderer.h"

#include <algorithm>
#include <cmath>
#include <format>

#include "Game/Exploration/Components.h"
#include "Game/NPC/Npc.h"

namespace koa {

WorldRenderer::WorldRenderer(ke::ResourceManager& rec) : rec_(rec) {
    for (int t = 0; t < T_QTD; ++t)
        for (int v = 0; v < 4; ++v) tiles_[t][v] = &rec_.regiao(std::format("terreno_{}_{}", t, v));
}

const ke::SpriteRegion& WorldRenderer::spriteConstrucao(const World& w, const BuildingData& b) {
    const BuildingDef* d = w.def(b);
    if (!d) return rec_.regiao("?");
    if (!b.constructed) return rec_.regiao(std::format("obra_{}x{}", d->tamanho.x, d->tamanho.y));  // canteiro com andaimes
    // Do mais específico (nível + era) ao genérico (só nível)
    for (const auto& nome : spritesAparencia(*d, b.level, w.reino.eraAtual()))
        if (rec_.temRegiao(nome)) return rec_.regiao(nome);
    return rec_.regiao(spritesAparencia(*d, b.level, w.reino.eraAtual()).back());
}

bool WorldRenderer::chamine(const World& w, const BuildingData& b, glm::vec2& saida) {
    const ke::AABB c = w.caixaConstrucao(b);
    const glm::vec2 topoSprite{(c.min.x + c.max.x) * 0.5f - 32.0f, c.max.y - 88.0f};
    if (b.defId != "casa" || !b.constructed) return false;  // por enquanto só a casa do herdeiro solta fumaça
    if (b.level == 2) saida = topoSprite + glm::vec2(47, 8);
    else if (b.level == 3) saida = topoSprite + glm::vec2(13, 4);
    else return false;
    return true;
}

namespace {

struct Desenhavel {
    float y;
    const ke::SpriteRegion* sprite;
    glm::vec2 pos;
    bool espelhar = false;
    float piscar = 0.0f;
};

}

void WorldRenderer::desenhar(ke::Renderer2D& r, const World& w, const ke::Camera2D& cam, float tempo, const ke::ParticleSystem& part,
                             bool mostrarColisoes) {
    const glm::vec2 canto = cam.canto();
    const int lado = w.mapa.ladoTile();
    const int x0 = std::max(0, static_cast<int>(canto.x) / lado - 1), y0 = std::max(0, static_cast<int>(canto.y) / lado - 1);
    const int x1 = std::min(w.mapa.largura() - 1, static_cast<int>(canto.x + cam.vista.x) / lado + 1);
    const int y1 = std::min(w.mapa.altura() - 1, static_cast<int>(canto.y + cam.vista.y) / lado + 1);

    // 1) Terreno (só o que a câmera vê)
    const int quadroAgua = static_cast<int>(tempo * 2.0f);
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x) {
            const ke::Tile& t = w.mapa.em(x, y);
            const int v = t.terreno == T_AGUA ? (t.variante + quadroAgua) % 4 : t.variante;
            r.sprite(*tiles_[std::min<int>(t.terreno, T_QTD - 1)][v], {x * lado, y * lado});
        }

    // 2) Sombras no chão
    const ke::SpriteRegion& sombra = rec_.regiao("sombra");
    for (auto [e, p, v] : w.reg.view<const Posicao, const Visual>().each()) {
        if (v.sprite.empty() || w.reg.all_of<Decoracao>(e) || !w.reg.all_of<Colisor>(e)) continue;
        r.sprite(sombra, {p.v.x - 8, p.v.y - 2});
    }
    r.sprite(sombra, {w.jogador.pos.x - 8, w.jogador.pos.y - 3});

    // 3) Objetos, construções e herói em ordem de profundidade
    std::vector<Desenhavel> lista;
    const glm::vec2 margemMin = canto - glm::vec2(96, 32), margemMax = canto + cam.vista + glm::vec2(96, 140);
    auto visivel = [&](glm::vec2 p) { return p.x > margemMin.x && p.x < margemMax.x && p.y > margemMin.y && p.y < margemMax.y; };
    for (auto [e, p, v] : w.reg.view<const Posicao, const Visual>().each()) {
        if (v.sprite.empty() || !visivel(p.v)) continue;
        std::string nome = v.sprite;
        if (const auto* a = w.reg.try_get<Animado>(e))
            nome = std::format("{}_{}", a->base, static_cast<int>(tempo * a->fps + p.v.x) % a->quadros);
        const ke::SpriteRegion& s = rec_.regiao(nome);
        const bool grande = w.reg.all_of<Decoracao>(e);
        glm::vec2 pos{p.v.x - s.tamanho.x / 2, p.v.y - s.tamanho.y + (grande ? 0 : 2)};
        if (v.tremor > 0.0f) pos.x += std::round(std::sin(tempo * 70.0f) * 1.5f);
        lista.push_back({p.v.y, &s, glm::floor(pos), false, v.piscar});
    }
    for (const auto& b : w.reino.construcoes) {
        const ke::SpriteRegion& s = spriteConstrucao(w, b);
        const ke::AABB c = w.caixaConstrucao(b);
        lista.push_back({c.max.y, &s, {std::floor((c.min.x + c.max.x) * 0.5f - s.tamanho.x / 2), c.max.y - s.tamanho.y}});
    }
    for (auto [e, p, m, mv, a] : w.reg.view<const Posicao, const Morador, const Movimento, const Agenda>().each()) {
        if (a.dormindo || !visivel(p.v)) continue;
        const char* dir = mv.dir == Direcao::Baixo ? "baixo" : mv.dir == Direcao::Cima ? "cima" : "lado";
        const ke::SpriteRegion& s =
            rec_.regiao(std::format("npc_{}_{}_{}", m.id, dir, mv.andando ? static_cast<int>(mv.tempoAnim * 7.0f) % 4 : 0));
        r.sprite(sombra, {std::floor(p.v.x) - 8, std::floor(p.v.y) - 3});
        lista.push_back({p.v.y, &s, glm::floor(p.v - glm::vec2(8, 22)), mv.dir == Direcao::Esquerda});
    }
    for (auto [e, p, mv, a] : w.reg.view<const Posicao, const Movimento, const Aldeao>().each()) {
        if (a.dormindo || !visivel(p.v)) continue;
        const char* dir = mv.dir == Direcao::Baixo ? "baixo" : mv.dir == Direcao::Cima ? "cima" : "lado";
        const ke::SpriteRegion& s =
            rec_.regiao(std::format("aldeao_{}_{}_{}", a.variante, dir, mv.andando ? static_cast<int>(mv.tempoAnim * 7.0f) % 4 : 0));
        r.sprite(sombra, {std::floor(p.v.x) - 8, std::floor(p.v.y) - 3});
        lista.push_back({p.v.y, &s, glm::floor(p.v - glm::vec2(8, 22)), mv.dir == Direcao::Esquerda});
    }
    {
        const Jogador& j = w.jogador;
        const char* dir = j.dir == Direcao::Baixo ? "baixo" : j.dir == Direcao::Cima ? "cima" : "lado";
        std::string nome;
        if (j.acao > 0.0f) nome = std::format("heroi_{}_golpe_{}", dir, j.acao > Jogador::DURACAO_ACAO * 0.5f ? 0 : 1);
        else nome = std::format("heroi_{}_{}", dir, j.andando ? static_cast<int>(j.tempoAnim * 8.0f) % 4 : 0);
        const ke::SpriteRegion& s = rec_.regiao(nome);
        lista.push_back({j.pos.y, &s, glm::floor(j.pos - glm::vec2(8, 22)), j.dir == Direcao::Esquerda});
    }
    std::ranges::stable_sort(lista, {}, &Desenhavel::y);
    for (const auto& d : lista) {
        r.sprite(*d.sprite, d.pos, glm::vec4(1.0f), d.espelhar);
        if (d.piscar > 0.0f) {
            r.definirMistura(ke::Mistura::Aditiva);
            r.sprite(*d.sprite, d.pos, {1, 1, 1, 0.7f}, d.espelhar);
            r.definirMistura(ke::Mistura::Normal);
        }
    }

    // 4) Partículas (poeira, lascas, cinzas)
    for (const auto& p : part.particulas()) {
        if (p.aditiva) continue;
        glm::vec4 c = p.cor;
        c.a *= std::clamp(p.vida / p.vidaMax * 1.5f, 0.0f, 1.0f);
        r.retangulo(glm::floor(p.pos), glm::vec2(p.tamanho), c);
    }

    // 5) Luz das tochas e brilhos (somados à cena)
    r.definirMistura(ke::Mistura::Aditiva);
    const ke::SpriteRegion& luz = rec_.regiao("luz");
    for (auto [e, p, l] : w.reg.view<const Posicao, const Luz>().each()) {
        if (!visivel(p.v)) continue;
        const float cint = 0.85f + 0.15f * std::sin(tempo * 9.0f + p.v.x) * std::sin(tempo * 5.3f + p.v.y);
        const float raio = l.raio * cint;
        r.sprite(luz, p.v + l.desloc - raio, glm::vec4(l.cor, 0.38f * cint), false, raio * 2.0f / 64.0f);
        r.sprite(luz, p.v + l.desloc - raio * 0.35f, glm::vec4(1.0f, 0.85f, 0.6f, 0.35f), false, raio * 0.7f / 64.0f);
    }
    // Vidro das janelas acesas à noite (o mapa de luz ilumina em volta; isto faz o vidro brilhar)
    const float escuro = w.relogio.escuridao();
    if (escuro > 0.0f) {
        for (const auto& b : w.reino.construcoes) {
            if (b.level < 2) continue;
            const ke::AABB c = w.caixaConstrucao(b);
            for (const float x : {0.25f, 0.75f}) {
                const glm::vec2 jan{c.min.x + (c.max.x - c.min.x) * x, c.max.y - 22.0f};
                r.sprite(luz, jan - 8.0f, {1.0f, 0.7f, 0.35f, 0.8f * escuro}, false, 16.0f / 64.0f);
            }
        }
    }
    for (const auto& p : part.particulas()) {
        if (!p.aditiva) continue;
        glm::vec4 c = p.cor;
        c.a *= std::clamp(p.vida / p.vidaMax, 0.0f, 1.0f);
        r.retangulo(glm::floor(p.pos), glm::vec2(p.tamanho), c);
    }
    r.definirMistura(ke::Mistura::Normal);

    // 6) Névoa rasteira: manchas grandes e lentas (some conforme a era avança)
    const float nevoa = std::max(0.0f, 1.0f - w.reino.eraVisual);
    if (nevoa > 0.01f) {
        for (int i = 0; i < 9; ++i) {
            const float vel = 4.0f + i * 1.3f;
            const float largura = w.tamanhoPixels().x + 400.0f;
            const float x = std::fmod(i * 173.0f + tempo * vel, largura) - 200.0f;
            const float y = std::fmod(i * 97.0f, w.tamanhoPixels().y) + std::sin(tempo * 0.2f + i) * 12.0f;
            const float esc = 3.0f + (i % 3);
            r.sprite(luz, {x, y}, {0.62f, 0.64f, 0.68f, 0.07f * nevoa}, false, esc);
        }
    }

    if (mostrarColisoes) {
        for (const auto& c : w.obstaculos()) r.contorno(c.min, c.max - c.min, {1, 0.2f, 0.2f, 0.9f});
        const ke::AABB j = w.jogador.caixa();
        r.contorno(j.min, j.max - j.min, {0.2f, 1, 0.3f, 1});
        for (int y = y0; y <= y1; ++y)
            for (int x = x0; x <= x1; ++x)
                if (w.mapa.em(x, y).solido) r.retangulo({x * lado, y * lado}, {lado, lado}, {0.2f, 0.4f, 1.0f, 0.25f});
    }
}

void WorldRenderer::desenharLuzes(ke::Renderer2D& r, const World& w, const ke::Camera2D& cam, float tempo) {
    const ke::SpriteRegion& luz = rec_.regiao("luz");
    const glm::vec2 canto = cam.canto();
    r.definirMistura(ke::Mistura::Aditiva);
    for (auto [e, p, l] : w.reg.view<const Posicao, const Luz>().each()) {
        if (p.v.x < canto.x - 200 || p.v.x > canto.x + cam.vista.x + 200 || p.v.y < canto.y - 200 || p.v.y > canto.y + cam.vista.y + 200)
            continue;
        const float cint = 0.9f + 0.1f * std::sin(tempo * 8.0f + p.v.x) * std::sin(tempo * 4.7f + p.v.y);
        const float raio = l.raio * 1.8f * cint;
        r.sprite(luz, p.v + l.desloc - raio, glm::vec4(l.cor * 1.2f, 1.0f), false, raio * 2.0f / 64.0f);
        r.sprite(luz, p.v + l.desloc - raio * 0.5f, glm::vec4(1.0f, 0.9f, 0.7f, 0.9f), false, raio / 64.0f);
    }
    // Janelas acesas das casas reconstruídas
    for (const auto& b : w.reino.construcoes) {
        if (b.level < 2) continue;
        const ke::AABB c = w.caixaConstrucao(b);
        for (const float x : {0.25f, 0.75f}) {
            const glm::vec2 jan{c.min.x + (c.max.x - c.min.x) * x, c.max.y - 22.0f};
            r.sprite(luz, jan - 26.0f, {1.0f, 0.75f, 0.4f, 0.9f}, false, 52.0f / 64.0f);
        }
    }
    // O herói carrega uma luz fraquinha (para não andar no breu total)
    r.sprite(luz, w.jogador.pos - glm::vec2(0, 10) - 40.0f, {0.55f, 0.55f, 0.65f, 0.6f}, false, 80.0f / 64.0f);
    r.definirMistura(ke::Mistura::Normal);
}

void WorldRenderer::desenharAlvo(ke::Renderer2D& r, const ke::BitmapFont& f, const World& w, float tempo) {
    const AlvoInteracao& a = w.alvo();
    if (a.tipo == TipoInteracao::Nenhuma) return;
    glm::vec2 topo = a.ponto - glm::vec2(0, 18);
    if (a.tipo == TipoInteracao::Conversar) {
        topo = w.reg.get<Posicao>(a.entidade).v - glm::vec2(0, 28);
    } else if (a.tipo == TipoInteracao::Construcao) {
        if (const BuildingData* b = w.reino.construcao(w.reg.get<ConstrucaoRef>(a.entidade).id)) topo = w.pontoPorta(*b) - glm::vec2(0, 30);
    } else if (const auto* v = w.reg.try_get<Visual>(a.entidade)) {
        topo.y = w.reg.get<Posicao>(a.entidade).v.y - rec_.regiao(v->sprite).tamanho.y - 4.0f;
    }
    // Setinha que flutua
    const float b = std::round(std::sin(tempo * 5.0f) * 1.5f);
    const glm::vec4 ouro{0.95f, 0.78f, 0.36f, 1.0f};
    for (int i = 0; i < 4; ++i) r.retangulo({std::floor(topo.x) - 3 + i, std::floor(topo.y) + b + i}, {7 - 2 * i, 1}, ouro);
    const std::string texto = "E  " + a.rotulo;
    const float l = static_cast<float>(f.largura(texto));
    const glm::vec2 p{std::floor(topo.x - l / 2), std::floor(topo.y - 12 + b)};
    r.retangulo(p - glm::vec2(3, 2), {l + 6, 12}, {0.05f, 0.04f, 0.04f, 0.75f});
    r.retangulo(p - glm::vec2(3, 2), {10, 12}, {0.45f, 0.36f, 0.22f, 0.9f});
    f.desenhar(r, texto, p, {0.92f, 0.88f, 0.80f, 1.0f});
}

}
