#include "Game/Scenes/TitleScene.h"

#include <cmath>
#include <format>

#include <glm/gtc/matrix_transform.hpp>

#include "Engine/Core/Logger.h"
#include "Game/Demo/Roteiro.h"
#include "Game/Scenes/WorldScene.h"

namespace koa {

TitleScene::TitleScene(Contexto& ctx) : ctx_(ctx), rend_(ctx.app.recursos()) {
    fundo_.carregar(ctx_.dados, ctx_.pastaData);
    fundo_.jogador.pos = {-100, -100};  // o herói ainda não chegou
    fundo_.relogio.minutos = 22 * 60;   // o vilarejo à noite, só tochas e a fogueira
    fundo_.relogio.parado = true;
    fundo_.posicionarMoradores();
    luz_.criar(640, 360);
    cam_.vista = {640, 360};
    cam_.limite = fundo_.tamanhoPixels();
    lerResumos();
}

void TitleScene::lerResumos() {
    maisRecente_.clear();
    double maisNovo = -1.0;
    for (int i = 0; i < 3; ++i) {
        resumos_[i] = {};
        if (!ctx_.saves->existe(SLOTS[i])) continue;
        std::string erro;
        const auto d = ctx_.saves->carregar(SLOTS[i], &erro);
        if (!d) {
            resumos_[i] = {true, "Save ilegível", ""};
            continue;
        }
        const int minutos = static_cast<int>(d->tempoJogado / 60.0);
        const char* eras[] = {"Reino das Cinzas", "O Despertar da Coroa", "Reino Renascido", "Reino da Alta Fantasia"};
        resumos_[i] = {true, std::format("Dia {}  {}  {}h{:02}", d->dia, eras[std::clamp(static_cast<int>(d->eraVisual + 0.5f), 0, 3)],
                                         minutos / 60, minutos % 60),
                       d->dataHora};
        // O mais recente pelo arquivo (data_hora é texto para gente ler)
        std::error_code ec;
        const auto quando = std::filesystem::last_write_time(ctx_.saves->pasta() / SLOTS[i] / "metadata.json", ec);
        const double t = ec ? 0.0 : static_cast<double>(quando.time_since_epoch().count());
        if (t > maisNovo) maisNovo = t, maisRecente_ = SLOTS[i];
    }
}

std::vector<std::string> TitleScene::opcoes() const {
    std::vector<std::string> v;
    if (temSave()) v.push_back("Continuar");
    v.push_back("Novo Jogo");
    if (temSave()) v.push_back("Carregar");
    v.push_back("Opções");
    v.push_back("Controles");
    v.push_back("Sair");
    return v;
}

void TitleScene::novoJogo(const std::string& slot) {
    ctx_.slot = slot;
    ke::log::info("novo jogo no espaço {}", slot);
    ctx_.app.cenas().trocar(std::make_unique<WorldScene>(ctx_, std::nullopt));
}

void TitleScene::carregarSlot(const std::string& slot) {
    std::string erro, aviso;
    auto d = ctx_.saves->carregar(slot, &erro, &aviso);
    if (!d) {
        erro_ = "Save ilegível: " + erro;
        ke::log::erro("carregar {}: {}", slot, erro);
        return;
    }
    if (!aviso.empty()) ke::log::aviso("{}", aviso);
    ctx_.slot = slot;
    ke::log::info("carregando {} ({})", slot, d->dataHora);
    ctx_.app.cenas().trocar(std::make_unique<WorldScene>(ctx_, d));
}

void TitleScene::continuar() {
    if (!maisRecente_.empty()) carregarSlot(maisRecente_);
}

void TitleScene::escolherMenu(const std::string& o) {
    ctx_.app.audio().tocar(ke::Som::Menu);
    if (o == "Continuar") continuar();
    else if (o == "Novo Jogo" || o == "Carregar") {
        slots_ = o == "Novo Jogo" ? Slots::Novo : Slots::Carregar;
        selSlot_ = 0;
        confirmarSubstituir_ = false;
    } else if (o == "Opções") telaOpcoes_.abrirOpcoes();
    else if (o == "Controles") telaOpcoes_.abrirControles();
    else ctx_.app.parar();
}

void TitleScene::entradaSlots() {
    ke::Input& in = ctx_.app.input();
    if (in.pressionou(SDL_SCANCODE_ESCAPE)) {
        slots_ = Slots::Fechado;
        return;
    }
    if (in.pressionou(SDL_SCANCODE_W) || in.pressionou(SDL_SCANCODE_UP)) selSlot_ = (selSlot_ + 2) % 3, confirmarSubstituir_ = false;
    if (in.pressionou(SDL_SCANCODE_S) || in.pressionou(SDL_SCANCODE_DOWN)) selSlot_ = (selSlot_ + 1) % 3, confirmarSubstituir_ = false;
    if (!(in.pressionou(SDL_SCANCODE_E) || in.pressionou(SDL_SCANCODE_RETURN) || in.pressionou(SDL_SCANCODE_SPACE))) return;
    const ResumoSlot& r = resumos_[selSlot_];
    if (slots_ == Slots::Carregar) {
        if (r.existe) carregarSlot(SLOTS[selSlot_]);
        return;
    }
    // Novo jogo: espaço ocupado pede confirmação (o save antigo ainda fica no .bak quando o novo for salvo)
    if (r.existe && !confirmarSubstituir_) {
        confirmarSubstituir_ = true;
        return;
    }
    novoJogo(SLOTS[selSlot_]);
}

void TitleScene::atualizar(double dtD) {
    const float dt = static_cast<float>(dtD);
    tempo_ += dt;
    ctx_.app.audio().definir(0.0f, 1.0f, 0.0f);  // a melodia melancólica das cinzas, à noite
    if (ctx_.roteiro) ctx_.roteiro->naTitulo(*this);
    ke::Input& in = ctx_.app.input();
    if (telaOpcoes_.aberta()) telaOpcoes_.entrada(ctx_);
    else if (slots_ != Slots::Fechado) entradaSlots();
    else {
        const auto ops = opcoes();
        const int n = static_cast<int>(ops.size());
        sel_ = std::clamp(sel_, 0, n - 1);
        if (in.pressionou(SDL_SCANCODE_W) || in.pressionou(SDL_SCANCODE_UP)) sel_ = (sel_ + n - 1) % n;
        if (in.pressionou(SDL_SCANCODE_S) || in.pressionou(SDL_SCANCODE_DOWN)) sel_ = (sel_ + 1) % n;
        if (in.pressionou(SDL_SCANCODE_RETURN) || in.pressionou(SDL_SCANCODE_E) || in.pressionou(SDL_SCANCODE_SPACE)) escolherMenu(ops[sel_]);
        if (in.pressionou(SDL_SCANCODE_ESCAPE)) ctx_.app.parar();
    }

    // Passeio lento pela vila, do castelo para a praça e de volta
    const glm::vec2 a{500, 130}, b{460, 330};
    cam_.posicionar(glm::mix(a, b, 0.5f + 0.5f * std::sin(tempo_ * 0.05f)));
    fundo_.passo(dt, {});
    std::uniform_real_distribution<float> u(0.0f, 1.0f);
    static std::mt19937 rng{3};
    ke::Particula p;
    p.pos = cam_.canto() + glm::vec2(u(rng) * 700.0f - 30.0f, -4.0f);
    p.vel = {8.0f + u(rng) * 10.0f, 12.0f + u(rng) * 10.0f};
    const float g = 0.6f + u(rng) * 0.2f;
    p.cor = {g, g, g, 0.55f};
    p.vida = p.vidaMax = 14.0f;
    part_.emitir(p);
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
    f.desenharCentralizado(r, "KINGDOM OF ASHES", {320, 80}, {cor::ouro.r * brilho, cor::ouro.g * brilho, cor::ouro.b * brilho, 1}, true, 3);
    f.desenharCentralizado(r, "The Last Crown", {320, 112}, cor::texto, true, 2);
    f.desenharCentralizado(r, "— O último herdeiro retorna às terras das cinzas —", {320, 136}, cor::apagado);

    // Uma camada por vez: opções/controles, espaços de save ou o menu principal
    const bool menu = slots_ == Slots::Fechado && !telaOpcoes_.aberta();
    if (telaOpcoes_.aberta()) telaOpcoes_.desenhar(r, f, ctx_);
    else if (menu) {
        const auto ops = opcoes();
        for (size_t i = 0; i < ops.size(); ++i) {
            const bool s = static_cast<int>(i) == sel_;
            const float y = 186.0f + i * 20.0f;
            if (s) {
                r.retangulo({250, y - 8}, {140, 16}, {0.45f, 0.36f, 0.22f, 0.45f});
                r.contorno({250, y - 8}, {140, 16}, cor::borda);
            }
            f.desenharCentralizado(r, ops[i], {320, y}, s ? cor::ouro : cor::texto);
        }
    } else {
        const glm::vec2 p{150, 170}, s{340, 130};
        desenharPainel(r, p, s);
        f.desenharCentralizado(r, slots_ == Slots::Novo ? "Novo jogo: escolha um espaço" : "Carregar: escolha um espaço", {320, p.y + 12},
                               cor::ouro);
        for (int i = 0; i < 3; ++i) {
            const float y = p.y + 32 + i * 28.0f;
            const bool sel = i == selSlot_;
            if (sel) r.retangulo({p.x + 8, y - 4}, {s.x - 16, 25}, {0.45f, 0.36f, 0.22f, 0.45f});
            const ResumoSlot& rs = resumos_[i];
            f.desenhar(r, std::format("Espaço {}", i + 1), {p.x + 16, y}, sel ? cor::ouro : cor::texto);
            f.desenhar(r, rs.existe ? rs.texto : "Vazio", {p.x + 96, y}, rs.existe ? cor::texto : cor::apagado);
            if (rs.existe) f.desenhar(r, rs.data, {p.x + 96, y + 10}, cor::apagado);
        }
        const std::string dica = confirmarSubstituir_ ? "Este espaço tem um jogo salvo. E de novo para substituir."
                                                      : "W/S escolher   E confirmar   Esc voltar";
        const float ld = static_cast<float>(f.largura(dica));
        r.retangulo({320 - ld / 2 - 6, p.y + s.y + 4}, {ld + 12, 12}, {0.03f, 0.03f, 0.04f, 0.8f});
        f.desenharCentralizado(r, dica, {320, p.y + s.y + 10}, confirmarSubstituir_ ? cor::ruim : cor::apagado);
    }
    if (!erro_.empty()) f.desenharCentralizado(r, erro_, {320, 318}, cor::ruim);
    f.desenhar(r, std::string("v") + VERSAO_JOGO, {6, 348}, cor::apagado);
    if (menu) {
        const std::string ajuda = "W/S escolher   Enter confirmar";
        f.desenhar(r, ajuda, {634.0f - f.largura(ajuda), 348}, cor::apagado);
    }
    r.terminar();
}

}
