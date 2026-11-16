#include "Game/Render/WorldRenderer.h"

#include <algorithm>
#include <cmath>
#include <format>

#include "Game/Exploration/Components.h"
#include "Game/Combat/Combate.h"
#include "Game/NPC/Npc.h"

namespace koa {

WorldRenderer::WorldRenderer(ke::ResourceManager& rec) : rec_(rec) {
    for (int t = 0; t < T_QTD; ++t)
        for (int v = 0; v < 4; ++v)
            for (int e = 0; e < 4; ++e) tiles_[t][v][e] = &comEra(std::format("terreno_{}_{}", t, v), e);
}

const ke::SpriteRegion& WorldRenderer::comEra(const std::string& nome, int era) {
    // "nome_eN" quando existe; senão, o da era anterior mais próxima (cache por nome)
    auto it = cacheEra_.find(nome);
    if (it == cacheEra_.end()) {
        std::array<const ke::SpriteRegion*, 4> v{};
        v[0] = &rec_.regiao(nome);
        for (int e = 1; e < 4; ++e) {
            const std::string n = std::format("{}_e{}", nome, e);
            v[e] = rec_.temRegiao(n) ? &rec_.regiao(n) : v[e - 1];
        }
        it = cacheEra_.emplace(nome, v).first;
    }
    return *it->second[std::clamp(era, 0, 3)];
}

const ke::SpriteRegion& WorldRenderer::spriteConstrucao(const World& w, const BuildingData& b) {
    const BuildingDef* d = w.def(b);
    if (!d) return rec_.regiao("?");
    if (!b.constructed) return rec_.regiao(std::format("obra_{}x{}", d->tamanho.x, d->tamanho.y));  // canteiro com andaimes
    // Era no lugar da construção (a onda de transformação chega a ela no seu tempo)
    const int era = w.eraEm(w.pontoPorta(b) - glm::vec2(0, 8));
    const auto nomes = spritesAparencia(*d, b.level, static_cast<KingdomEra>(era));
    if (rec_.temRegiao(nomes.front())) return rec_.regiao(nomes.front());
    return comEra(nomes.back(), era);
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
            const int era = w.eraNoTile({x, y});
            int terreno = std::min<int>(t.terreno, T_QTD - 1);
            if (era >= 2 && terreno == T_LAMA) terreno = T_CALCADA;  // no Reino Renascido as estradas são calçadas
            r.sprite(*tiles_[terreno][v][era], {x * lado, y * lado});
        }

    // Enfeites só aparecem quando a era chega ao lugar deles
    // (e os da festa, só durante o festival)
    auto escondido = [&](entt::entity e, glm::vec2 p) {
        const auto* en = w.reg.try_get<Enfeite>(e);
        return en && (w.eraEm(p) < en->eraMin || (en->festival && !w.festivalAgora()));
    };

    // Fase 15: a terra dos canteiros é chão (fica por baixo de quem pisa nela)
    for (auto [e, p, v, c] : w.reg.view<const Posicao, const Visual, const Canteiro>().each())
        r.sprite(rec_.regiao(v.sprite), glm::floor(p.v + glm::vec2(-8.0f, -13.5f)));

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
        if (v.sprite.empty() || !visivel(p.v) || escondido(e, p.v) || w.reg.all_of<Canteiro>(e)) continue;
        const int era = w.eraEm(p.v);
        std::string nome = v.sprite;
        if (const auto* a = w.reg.try_get<Animado>(e))
            nome = std::format("{}_{}", a->base, static_cast<int>(tempo * a->fps + p.v.x) % a->quadros);
        if (const auto* tr = w.reg.try_get<TrocaPorEra>(e); tr && era >= tr->era) nome = tr->sprite;
        const ke::SpriteRegion& s = comEra(nome, era);
        const bool grande = w.reg.all_of<Decoracao>(e);
        glm::vec2 pos{p.v.x - s.tamanho.x / 2, p.v.y - s.tamanho.y + (grande ? 0 : 2)};
        if (v.tremor > 0.0f) pos.x += std::round(std::sin(tempo * 70.0f) * 1.5f);
        lista.push_back({p.v.y, &s, glm::floor(pos), false, v.piscar});
    }
    for (const auto& b : w.reino.construcoes) {
        if (!w.noVilarejo()) break;  // as construções do reino ficam no vilarejo
        if (b.defId == "muralha") {
            // Fase 13: um trecho de muro por fileira e o portão no vão da estrada (ruínas no nível 1)
            const float x = b.tile.x * static_cast<float>(lado);
            for (int f = 0; f < 7; ++f) {
                if (f == 3 || f == 4) continue;
                const float base = (b.tile.y + f + 1) * static_cast<float>(lado);
                const int era = w.eraEm({x + 8, base});
                const ke::SpriteRegion& s = comEra(b.level < 2 ? "muralha_ruina" : std::format("muralha_n{}", std::min(b.level, 4)), era);
                lista.push_back({base, &s, {x + 8 - s.tamanho.x / 2.0f, base - s.tamanho.y}});
            }
            if (b.level >= 2) {
                const float base = (b.tile.y + 5) * static_cast<float>(lado);
                const ke::SpriteRegion& s =
                    comEra(w.portaoDePe() ? std::format("portao_n{}", std::min(b.level, 4)) : "portao_quebrado", w.eraEm({x + 8, base}));
                lista.push_back({base - 8.0f, &s, {x + 8 - s.tamanho.x / 2.0f, base - s.tamanho.y}});
            }
            continue;
        }
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
    // Fase 15: galinhas e vacas passeando no terreiro
    for (auto [e, p, a] : w.reg.view<const Posicao, const Animal>().each()) {
        if (!visivel(p.v)) continue;
        const ke::SpriteRegion& s = rec_.regiao(std::format("{}_{}", a.tipo, a.andando ? static_cast<int>(a.tempoAnim * 6.0f) % 2 : 0));
        r.sprite(sombra, {std::floor(p.v.x) - 8, std::floor(p.v.y) - 3});
        lista.push_back({p.v.y, &s, glm::floor(p.v - glm::vec2(s.tamanho.x / 2.0f, s.tamanho.y - 1.0f)), a.olhaEsquerda});
    }
    // Inimigos: sprite de 2 quadros, virado para o lado em que andam; piscam ao levar golpe
    std::vector<std::pair<glm::vec2, float>> barras;  // vida dos feridos (desenhada por cima)
    // Fase 13: soldados dos postos no portão
    for (auto [e, p, s] : w.reg.view<const Posicao, const Soldado>().each()) {
        if (!visivel(p.v)) continue;
        const ke::SpriteRegion& sp = rec_.regiao(std::format("soldado_{}", s.andando ? static_cast<int>(s.tempoAnim * 7.0f) % 2 : 0));
        r.sprite(sombra, {std::floor(p.v.x) - 8, std::floor(p.v.y) - 3});
        lista.push_back({p.v.y, &sp, glm::floor(p.v - glm::vec2(8, 22)), s.dir == Direcao::Esquerda, s.piscar});
        if (s.vida < s.vidaMax) barras.push_back({p.v - glm::vec2(0, 27), static_cast<float>(s.vida) / s.vidaMax});
    }
    for (auto [e, p, i] : w.reg.view<const Posicao, const Inimigo>().each()) {
        if (!visivel(p.v)) continue;
        const ke::SpriteRegion& s = rec_.regiao(std::format("inimigo_{}_{}", i.tipo, static_cast<int>(i.tempoAnim * 6.0f + p.v.x) % 2));
        const float largura = static_cast<float>(s.tamanho.x);
        r.sprite(sombra, {std::floor(p.v.x) - 8, std::floor(p.v.y) - 3});
        lista.push_back({p.v.y, &s, glm::floor(p.v - glm::vec2(largura / 2, s.tamanho.y - 2.0f)), i.dir == Direcao::Esquerda, i.piscar});
        if (i.vida < i.vidaMax) barras.push_back({p.v - glm::vec2(0, s.tamanho.y + 3.0f), static_cast<float>(i.vida) / i.vidaMax});
    }
    {
        const Jogador& j = w.jogador;
        const char* dir = j.dir == Direcao::Baixo ? "baixo" : j.dir == Direcao::Cima ? "cima" : "lado";
        std::string nome;
        if (j.acao > 0.0f && j.atacando) nome = std::format("heroi_{}_espada_{}", dir, j.acao > Jogador::DURACAO_ATAQUE * 0.5f ? 0 : 1);
        else if (j.acao > 0.0f) nome = std::format("heroi_{}_golpe_{}", dir, j.acao > Jogador::DURACAO_ACAO * 0.5f ? 0 : 1);
        else nome = std::format("heroi_{}_{}", dir, j.andando ? static_cast<int>(j.tempoAnim * 8.0f) % 4 : 0);
        const ke::SpriteRegion& s = rec_.regiao(nome);
        // Invulnerável depois de um golpe: pisca
        const bool some = j.invulneravel > 0.0f && static_cast<int>(tempo * 20.0f) % 2 == 0;
        if (!some) lista.push_back({j.pos.y, &s, glm::floor(j.pos - glm::vec2(8, 22)), j.dir == Direcao::Esquerda});
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
    for (const auto& [p, fr] : barras) {
        r.retangulo(glm::floor(p - glm::vec2(8, 0)), {16, 3}, {0.1f, 0.05f, 0.05f, 0.9f});
        r.retangulo(glm::floor(p - glm::vec2(7, -1)), {14 * fr, 1}, {0.85f, 0.2f, 0.2f, 1});
    }

    // Fase 15: linha de pesca da ponta da vara até a boia (que afunda quando o peixe morde)
    if (w.pescando()) {
        const Jogador& j = w.jogador;
        const glm::vec2 dv = j.dir == Direcao::Esquerda ? glm::vec2(-1, 0) : j.dir == Direcao::Direita ? glm::vec2(1, 0)
                             : j.dir == Direcao::Cima   ? glm::vec2(0, -1) : glm::vec2(0, 1);
        const glm::vec2 ponta = j.pos + glm::vec2(dv.x * 10.0f, -20.0f + dv.y * 4.0f);
        const bool morde = w.peixeMordendo();
        const glm::vec2 boia = w.boiaPesca() + glm::vec2(0, morde ? 2.0f + std::round(std::sin(tempo * 30.0f)) : std::round(std::sin(tempo * 3.0f) * 0.6f));
        // A linha cai num arco suave
        const int passos = 16;
        for (int k = 0; k <= passos; ++k) {
            const float u = static_cast<float>(k) / passos;
            const glm::vec2 q = glm::mix(ponta, boia, u) + glm::vec2(0, std::sin(u * 3.14159f) * (morde ? 1.0f : 5.0f));
            r.retangulo(glm::floor(q), {1, 1}, {0.85f, 0.85f, 0.8f, 0.8f});
        }
        r.retangulo(glm::floor(boia - glm::vec2(1, 2)), {3, 2}, {0.9f, 0.2f, 0.15f, 1});
        r.retangulo(glm::floor(boia - glm::vec2(1, 0)), {3, 1}, {0.95f, 0.95f, 0.9f, 1});
        if (morde) r.contorno(glm::floor(boia - glm::vec2(4, 2)), {9, 5}, {0.8f, 0.9f, 1.0f, 0.6f + 0.4f * std::sin(tempo * 20.0f)});
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
        if (!visivel(p.v) || escondido(e, p.v)) continue;
        if (const auto* c = w.reg.try_get<Coletavel>(e); c && c->esgotado) continue;  // veio minerado não brilha
        const float cint = 0.85f + 0.15f * std::sin(tempo * 9.0f + p.v.x) * std::sin(tempo * 5.3f + p.v.y);
        const float raio = l.raio * cint;
        r.sprite(luz, p.v + l.desloc - raio, glm::vec4(l.cor, 0.38f * cint), false, raio * 2.0f / 64.0f);
        r.sprite(luz, p.v + l.desloc - raio * 0.35f, glm::vec4(1.0f, 0.85f, 0.6f, 0.35f), false, raio * 0.7f / 64.0f);
    }
    // Vidro das janelas acesas à noite (o mapa de luz ilumina em volta; isto faz o vidro brilhar)
    const float escuro = w.relogio.escuridao();
    if (escuro > 0.0f && w.noVilarejo()) {
        for (const auto& b : w.reino.construcoes) {
            if (b.level < 2 || b.defId == "muralha") continue;  // muralha não tem janela
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
        if (const auto* en = w.reg.try_get<Enfeite>(e); en && (w.eraEm(p.v) < en->eraMin || (en->festival && !w.festivalAgora()))) continue;
        if (const auto* c = w.reg.try_get<Coletavel>(e); c && c->esgotado) continue;
        const float cint = 0.9f + 0.1f * std::sin(tempo * 8.0f + p.v.x) * std::sin(tempo * 4.7f + p.v.y);
        const float raio = l.raio * 1.8f * cint;
        // Na Alta Fantasia as tochas viram chama mágica azul
        const bool magica = w.reg.all_of<Animado>(e) && w.reg.get<Animado>(e).base == "tocha" && w.eraEm(p.v) >= 3;
        const glm::vec3 corLuz = magica ? glm::vec3(0.45f, 0.7f, 1.0f) : l.cor;
        r.sprite(luz, p.v + l.desloc - raio, glm::vec4(corLuz * 1.2f, 1.0f), false, raio * 2.0f / 64.0f);
        r.sprite(luz, p.v + l.desloc - raio * 0.5f, glm::vec4(1.0f, 0.9f, 0.7f, 0.9f), false, raio / 64.0f);
    }
    // Criaturas: olhos das sombras e a aura do chefe (para não sumirem no breu)
    for (auto [e, p, i] : w.reg.view<const Posicao, const Inimigo>().each()) {
        if (i.tipo == "sombra") r.sprite(luz, p.v - glm::vec2(0, 8) - 14.0f, {0.6f, 0.3f, 0.8f, 0.8f}, false, 28.0f / 64.0f);
        else if (i.tipo == "cavaleiro_caido") {
            const float pulso = 0.8f + 0.2f * std::sin(tempo * 3.0f);
            r.sprite(luz, p.v - glm::vec2(0, 20) - 48.0f * pulso, {0.9f, 0.2f, 0.15f, 0.9f}, false, 96.0f * pulso / 64.0f);
        } else if (i.tipo == "golem_cinzas") {
            // Brasa no peito: mais fraca quando ele se rende
            const float pulso = (i.rendido ? 0.55f : 0.85f) + 0.15f * std::sin(tempo * (i.rendido ? 1.5f : 4.0f));
            r.sprite(luz, p.v - glm::vec2(0, 22) - 52.0f * pulso, {1.0f, 0.45f, 0.15f, 0.9f}, false, 104.0f * pulso / 64.0f);
        } else if (i.tipo == "aranha") {
            r.sprite(luz, p.v - glm::vec2(0, 6) - 10.0f, {0.4f, 0.9f, 0.6f, 0.7f}, false, 20.0f / 64.0f);
        }
    }
    // Janelas acesas das casas reconstruídas
    for (const auto& b : w.reino.construcoes) {
        if (b.level < 2 || !b.constructed || !w.noVilarejo() || b.defId == "muralha") continue;
        const ke::AABB c = w.caixaConstrucao(b);
        for (const float x : {0.25f, 0.75f}) {
            const glm::vec2 jan{c.min.x + (c.max.x - c.min.x) * x, c.max.y - 22.0f};
            r.sprite(luz, jan - 26.0f, {1.0f, 0.75f, 0.4f, 0.9f}, false, 52.0f / 64.0f);
        }
    }
    // O herói carrega uma luz fraquinha (para não andar no breu total); mais forte nas regiões escuras
    const float minimo = w.escuridaoMinAtual();
    const float raioHeroi = minimo > 0.7f ? 84.0f : minimo > 0.3f ? 60.0f : 40.0f;
    r.sprite(luz, w.jogador.pos - glm::vec2(0, 10) - raioHeroi, {0.6f, 0.58f, 0.66f, 0.7f}, false, raioHeroi * 2.0f / 64.0f);
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
