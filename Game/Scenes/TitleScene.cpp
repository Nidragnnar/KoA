#include "Game/Scenes/TitleScene.h"

#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

#include "Engine/Core/Logger.h"
#include "Game/Demo/Roteiro.h"
#include "Game/Scenes/WorldScene.h"

namespace koa {

namespace {
const char* OPCOES[] = {"Novo Jogo", "Continuar", "Sair"};
}

TitleScene::TitleScene(Contexto& ctx) : ctx_(ctx), rend_(ctx.app.recursos()) {
    fundo_.carregar(ctx_.dados, ctx_.pastaData);
    fundo_.jogador.pos = {-100, -100};  // o herói ainda não chegou
    fundo_.relogio.minutos = 22 * 60;   // o vilarejo à noite, só tochas e a fogueira
    fundo_.relogio.parado = true;
    fundo_.posicionarMoradores();
    luz_.criar(640, 360);
    cam_.vista = {640, 360};
    cam_.limite = fundo_.tamanhoPixels();
    temSave_ = ctx_.saves->existe();
    sel_ = temSave_ ? 1 : 0;
}

void TitleScene::escolher(int opcao) {
    if (opcao == 0) {
        ke::log::info("novo jogo");
        ctx_.app.cenas().trocar(std::make_unique<WorldScene>(ctx_, std::nullopt));
    } else if (opcao == 1) {
        std::string erro, aviso;
        auto d = ctx_.saves->carregar("Save_001", &erro, &aviso);
        if (!d) {
            erro_ = "Save ilegível: " + erro;
            ke::log::erro("continuar: {}", erro);
            return;
        }
        if (!aviso.empty()) ke::log::aviso("{}", aviso);
        ke::log::info("continuando save de {}", d->dataHora);
        ctx_.app.cenas().trocar(std::make_unique<WorldScene>(ctx_, d));
    } else {
        ctx_.app.parar();
    }
}

void TitleScene::atualizar(double dtD) {
    const float dt = static_cast<float>(dtD);
    tempo_ += dt;
    if (ctx_.roteiro) ctx_.roteiro->naTitulo(*this);
    ke::Input& in = ctx_.app.input();
    auto mover = [&](int d) {
        do sel_ = (sel_ + d + 3) % 3;
        while (sel_ == 1 && !temSave_);
    };
    if (in.pressionou(SDL_SCANCODE_W) || in.pressionou(SDL_SCANCODE_UP)) mover(-1);
    if (in.pressionou(SDL_SCANCODE_S) || in.pressionou(SDL_SCANCODE_DOWN)) mover(1);
    if (in.pressionou(SDL_SCANCODE_RETURN) || in.pressionou(SDL_SCANCODE_E) || in.pressionou(SDL_SCANCODE_SPACE)) escolher(sel_);
    if (in.pressionou(SDL_SCANCODE_ESCAPE)) ctx_.app.parar();

    // Passeio lento pela vila, do castelo para a praça e de volta
    const glm::vec2 a{500, 130}, b{460, 330};
    cam_.posicionar(glm::mix(a, b, 0.5f + 0.5f * std::sin(tempo_ * 0.05f)));
    fundo_.passo(dt, {});
    std::uniform_real_distribution<float> u(0.0f, 1.0f);
    static std::mt19937 rng{3};
    for (int i = 0; i < 1; ++i) {
        ke::Particula p;
        p.pos = cam_.canto() + glm::vec2(u(rng) * 700.0f - 30.0f, -4.0f);
        p.vel = {8.0f + u(rng) * 10.0f, 12.0f + u(rng) * 10.0f};
        const float g = 0.6f + u(rng) * 0.2f;
        p.cor = {g, g, g, 0.55f};
        p.vida = p.vidaMax = 14.0f;
        part_.emitir(p);
    }
    part_.atualizar(dt);
}

void TitleScene::renderizar(float) {
    ke::Renderer2D& r = ctx_.app.renderer();
    const ke::BitmapFont& f = ctx_.app.fonte();
    luz_.comecar(fundo_.relogio.luzAmbiente());
    r.comecar(cam_.projecao());
    rend_.desenharLuzes(r, fundo_, cam_, tempo_);
    r.terminar();
    luz_.terminar();
    r.comecar(cam_.projecao());
    rend_.desenhar(r, fundo_, cam_, tempo_, part_, false);
    r.definirMistura(ke::Mistura::Multiplicar);
    r.quad(luz_.textura(), cam_.canto(), cam_.vista, {0, 1, 1, 0});
    r.definirMistura(ke::Mistura::Normal);
    r.terminar();

    r.comecar(glm::ortho(0.0f, 640.0f, 360.0f, 0.0f, -1.0f, 1.0f));
    r.retangulo({0, 0}, {640, 360}, {0.03f, 0.03f, 0.04f, 0.25f});
    for (int i = 0; i < 60; ++i) r.retangulo({0, 300.0f + i}, {640, 1}, {0.02f, 0.02f, 0.03f, i / 60.0f * 0.8f});
    const float brilho = 0.85f + 0.15f * std::sin(tempo_ * 1.5f);
    f.desenharCentralizado(r, "KINGDOM OF ASHES", {320, 92}, {cor::ouro.r * brilho, cor::ouro.g * brilho, cor::ouro.b * brilho, 1}, true, 3);
    f.desenharCentralizado(r, "The Last Crown", {320, 124}, cor::texto, true, 2);
    f.desenharCentralizado(r, "— O último herdeiro retorna às terras das cinzas —", {320, 148}, cor::apagado);

    for (int i = 0; i < 3; ++i) {
        const bool desabilitada = i == 1 && !temSave_;
        const bool s = i == sel_;
        const float y = 214.0f + i * 22.0f;
        if (s) {
            r.retangulo({250, y - 8}, {140, 17}, {0.45f, 0.36f, 0.22f, 0.45f});
            r.contorno({250, y - 8}, {140, 17}, cor::borda);
        }
        f.desenharCentralizado(r, OPCOES[i], {320, y}, desabilitada ? glm::vec4(0.35f, 0.33f, 0.3f, 1) : s ? cor::ouro : cor::texto, true, 1);
    }
    if (!erro_.empty()) f.desenharCentralizado(r, erro_, {320, 290}, cor::ruim);
    f.desenhar(r, std::string("v") + VERSAO_JOGO + " — protótipo The Forgotten Kingdom", {6, 348}, cor::apagado);
    const std::string ajuda = "W/S escolher   Enter confirmar";
    f.desenhar(r, ajuda, {634.0f - f.largura(ajuda), 348}, cor::apagado);
    r.terminar();
}

}
