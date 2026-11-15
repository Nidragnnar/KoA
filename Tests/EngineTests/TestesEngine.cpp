#include <gtest/gtest.h>

#include <cmath>
#include <vector>

#include "Engine/Audio/Sintetizador.h"
#include "Engine/Core/Time.h"
#include "Engine/Physics/Collision.h"
#include "Engine/Platform/Input.h"

#include <SDL3/SDL_events.h>
#include "Engine/Renderer/Animation.h"
#include "Engine/Renderer/ParticleSystem.h"
#include "Engine/Renderer/RenderTarget.h"
#include "Engine/World/TileMap.h"

using namespace ke;

TEST(PassoFixo, AcumulaEDevolvePassosInteiros) {
    FixedTimestep p(1.0 / 60.0);
    EXPECT_EQ(p.avancar(1.0 / 120.0), 0);
    EXPECT_EQ(p.avancar(1.0 / 120.0), 1);
    EXPECT_EQ(p.avancar(3.0 / 60.0), 3);
}

TEST(PassoFixo, EngasgoLongoNaoViraRajada) {
    FixedTimestep p(1.0 / 60.0, 5);
    EXPECT_EQ(p.avancar(2.0), 5);
    EXPECT_LT(p.alfa(), 1.01);
}

TEST(TileMap, ChunksDe32CobremOMapaInteiro) {
    TileMap m(64, 40, 16);
    EXPECT_EQ(m.chunksX(), 2);
    EXPECT_EQ(m.chunksY(), 2);
    m.em(63, 39).terreno = 7;
    EXPECT_EQ(m.chunk(1, 1).tiles[(39 % 32) * 32 + 31].terreno, 7);
    EXPECT_TRUE(m.solido(-1, 0));
    EXPECT_TRUE(m.solido(64, 0));
    EXPECT_FALSE(m.solido(0, 0));
}

TEST(Colisao, ParaEncostadoNaParede) {
    TileMap m(10, 10, 16);
    for (int y = 0; y < 10; ++y) m.em(5, y).solido = true;  // parede em x = 80..96
    const AABB caixa{{60, 40}, {70, 46}};
    const glm::vec2 feito = moverComColisao(m, caixa, {30, 0}, {});
    EXPECT_FLOAT_EQ(caixa.max.x + feito.x, 80.0f);
}

TEST(Colisao, DeslizaPelaQuinaQuandoQuaseAlinhado) {
    TileMap m(10, 10, 16);
    m.em(5, 2).solido = true;  // bloco em y 32..48; caixa raspando 2 px na parte de baixo
    const AABB caixa{{60, 46}, {70, 52}};
    glm::vec2 total{0};
    AABB c = caixa;
    for (int i = 0; i < 20; ++i) {
        const glm::vec2 d = moverComColisao(m, c, {1.5f, 0}, {});
        c = c.deslocada(d);
        total += d;
    }
    EXPECT_GT(c.min.x, 80.0f) << "deveria ter contornado o bloco";
    EXPECT_GE(c.min.y, 48.0f);
}

TEST(Colisao, ObstaculosExtrasBloqueiam) {
    TileMap m(10, 10, 16);
    const AABB obst[] = {{{50, 0}, {60, 160}}};
    const glm::vec2 feito = moverComColisao(m, {{30, 30}, {40, 36}}, {40, 0}, obst);
    EXPECT_FLOAT_EQ(feito.x, 10.0f);
}

TEST(Letterbox, EscalaInteiraQuandoCabe) {
    const Retangulo r = calcularLetterbox(1920, 1080, 640, 360);
    EXPECT_EQ(r.largura, 1920);
    EXPECT_EQ(r.altura, 1080);
    const Retangulo r2 = calcularLetterbox(1366, 768, 640, 360);
    EXPECT_EQ(r2.largura, 1280);  // 2× com bordas
    EXPECT_EQ(r2.x, 43);
}

TEST(Mouse, CoordenadasDaJanelaViramTelaInterna) {
    // Janela 1366×768: a tela interna aparece em 2× com faixas de 43 px dos lados e 24 px em cima e embaixo
    const Retangulo r = calcularLetterbox(1366, 768, 640, 360);
    Input in;
    in.definirTela(r.x, r.y, r.largura, r.altura, 640, 360, 1.0f);
    SDL_Event e{};
    e.type = SDL_EVENT_MOUSE_MOTION;
    e.motion.x = 43.0f + 2.0f * 100.0f;
    e.motion.y = 24.0f + 2.0f * 50.0f;
    in.processar(e);
    EXPECT_FLOAT_EQ(in.mouse().x, 100.0f);
    EXPECT_FLOAT_EQ(in.mouse().y, 50.0f);
    EXPECT_TRUE(in.mouseMoveu());
    SDL_Event c{};
    c.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    c.button.button = SDL_BUTTON_LEFT;
    c.button.x = 43.0f + 2.0f * 320.0f;
    c.button.y = 24.0f;
    in.processar(c);
    EXPECT_TRUE(in.clicou(BotaoMouse::Esquerdo));
    EXPECT_FLOAT_EQ(in.mouse().x, 320.0f);
    in.fimDoPasso();
    EXPECT_FALSE(in.clicou(BotaoMouse::Esquerdo)) << "o clique vale um passo só";
    EXPECT_TRUE(in.segurandoMouse(BotaoMouse::Esquerdo));
    // Tela de alta densidade (2 pixels por ponto): a posição em pontos é convertida
    in.definirTela(r.x, r.y, r.largura, r.altura, 640, 360, 2.0f);
    e.motion.x = (43.0f + 2.0f * 100.0f) / 2.0f;
    e.motion.y = (24.0f + 2.0f * 50.0f) / 2.0f;
    in.processar(e);
    EXPECT_FLOAT_EQ(in.mouse().x, 100.0f);
}

TEST(Animacao, RepeteOuParaNoUltimoQuadro) {
    AnimationClip c{{"a", "b", "c"}, 10.0f, true};
    EXPECT_EQ(c.quadro(0.0f), "a");
    EXPECT_EQ(c.quadro(0.25f), "c");
    EXPECT_EQ(c.quadro(0.35f), "a");
    c.repetir = false;
    EXPECT_EQ(c.quadro(5.0f), "c");
}

namespace {
// Energia média de um trecho de áudio
double energia(ke::Sintetizador& s, int quadros) {
    std::vector<float> buf(static_cast<size_t>(quadros) * 2);
    s.gerar(buf.data(), quadros);
    double e = 0.0;
    for (float x : buf) {
        EXPECT_TRUE(std::isfinite(x));
        EXPECT_LE(std::abs(x), 1.0f);
        e += x * x;
    }
    return e / buf.size();
}
}

TEST(Audio, MusicaSaiLimpaEmTodasAsEras) {
    for (float era : {0.0f, 1.0f, 2.0f, 3.0f}) {
        ke::Sintetizador s(48000, 3);
        s.definirEra(era);
        EXPECT_GT(energia(s, 48000 * 8), 1e-6) << "era " << era << " precisa tocar algo em 8 s";
        EXPECT_LT(s.vozesAtivas(), 50u) << "as vozes terminam (não acumulam)";
    }
}

TEST(Audio, EfeitoSomaEnergiaETermina) {
    ke::Sintetizador s(48000, 3);
    s.definirVolumes(0.0f, 1.0f);  // só efeitos
    const double antes = energia(s, 4800);
    s.tocar(ke::Som::Golpe);
    s.tocar(ke::Som::Magia);
    EXPECT_GT(energia(s, 4800), antes + 1e-5);
    energia(s, 48000 * 3);
    EXPECT_LT(energia(s, 4800), 1e-9) << "efeitos curtos acabam sozinhos (a música está muda neste teste)";
}

TEST(Audio, TensaoTrocaATrilha) {
    ke::Sintetizador a(48000, 5), b(48000, 5);
    a.definirEra(3.0f);
    b.definirEra(3.0f);
    b.definirTensao(1.0f);
    std::vector<float> x(48000 * 2), y(48000 * 2);
    a.gerar(x.data(), 48000);
    b.gerar(y.data(), 48000);
    EXPECT_NE(x, y);
}

TEST(Particulas, MorremNoFimDaVida) {
    ParticleSystem s;
    Particula p;
    p.vida = p.vidaMax = 0.5f;
    s.emitir(p);
    s.atualizar(0.3f);
    EXPECT_EQ(s.particulas().size(), 1u);
    s.atualizar(0.3f);
    EXPECT_TRUE(s.particulas().empty());
}
