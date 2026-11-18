#include "Game/UI/Menus.h"

#include <format>

namespace koa {

namespace {

const char* CONTROLES[][2] = {
    {"WASD / setas", "andar"},
    {"E / Espaço", "interagir (cortar, falar, plantar, pescar)"},
    {"segurar E", "continuar golpeando árvores e rochas"},
    {"clique / J", "atacar (o clique vira para o cursor)"},
    {"H", "comer (o melhor prato, ou 3 de comida = +25)"},
    {"F", "dar presente na conversa; comer no fogão"},
    {"B", "obras: escolher e posicionar"},
    {"K", "painel do reino e impostos"},
    {"R", "terras do reino: corrupção e o que rendem"},
    {"G", "Conselho Real: cargos, leis e decisões"},
    {"L", "diário: missões e equipamento"},
    {"C", "Crônicas do Reino e fragmentos"},
    {"N", "mensagens: reler os últimos avisos"},
    {"F5", "salvar"},
    {"M", "som ligado/desligado"},
    {"F11", "tela cheia"},
    {"Esc", "menu"},
    {"mouse", "escolher e confirmar nos menus e painéis"},
};

}

void TelaOpcoes::entrada(Contexto& ctx) {
    ke::Input& in = ctx.app.input();
    ctx.opcoes.telaCheia = ctx.app.janela().telaCheia();  // F11 também muda a tela cheia
    bool confirmar = in.pressionou(SDL_SCANCODE_E) || in.pressionou(SDL_SCANCODE_RETURN) || in.pressionou(SDL_SCANCODE_SPACE);
    if (controles_) {
        if (in.pressionou(SDL_SCANCODE_ESCAPE) || confirmar || in.clicou(ke::BotaoMouse::Esquerdo)) aberta_ = false;
        return;
    }
    if (in.pressionou(SDL_SCANCODE_W) || in.pressionou(SDL_SCANCODE_UP)) sel_ = (sel_ + 3) % 4;
    if (in.pressionou(SDL_SCANCODE_S) || in.pressionou(SDL_SCANCODE_DOWN)) sel_ = (sel_ + 1) % 4;
    const int d = (in.pressionou(SDL_SCANCODE_D) || in.pressionou(SDL_SCANCODE_RIGHT) ? 1 : 0) -
                  (in.pressionou(SDL_SCANCODE_A) || in.pressionou(SDL_SCANCODE_LEFT) ? 1 : 0);
    bool mudou = false;
    // Mouse: passar seleciona a linha; clicar na barra de volume põe o volume onde clicou; clicar fora volta
    if (cliqueFecha(in, cliques_)) {
        aberta_ = false;
        return;
    }
    const bool clique = listaComMouse(in, cliques_, sel_, 4);
    if (clique && sel_ <= 1 && in.mouse().x >= barraX_ - 4) {
        const int v = std::clamp(static_cast<int>((in.mouse().x - barraX_) / barraL_ * 10.0f) + 1, 0, 10);
        (sel_ == 0 ? ctx.opcoes.musica : ctx.opcoes.efeitos) = in.mouse().x < barraX_ ? 0 : v;
        mudou = true;
    } else if (clique) {
        confirmar = true;
    }
    if (d != 0 && sel_ == 0) ctx.opcoes.musica = std::clamp(ctx.opcoes.musica + d, 0, 10), mudou = true;
    if (d != 0 && sel_ == 1) ctx.opcoes.efeitos = std::clamp(ctx.opcoes.efeitos + d, 0, 10), mudou = true;
    if ((d != 0 || confirmar) && sel_ == 2) ctx.opcoes.telaCheia = !ctx.opcoes.telaCheia, mudou = true;
    if (mudou) {
        ctx.aplicarOpcoes();
        if (sel_ == 1) ctx.app.audio().tocar(ke::Som::Moeda);  // amostra do volume dos efeitos
        ctx.salvarOpcoes();
    }
    if (in.pressionou(SDL_SCANCODE_ESCAPE) || (confirmar && sel_ == 3)) aberta_ = false;
}

void TelaOpcoes::desenhar(ke::Renderer2D& r, const ke::BitmapFont& f, const Contexto& ctx) const {
    r.retangulo({0, 0}, {640, 360}, {0, 0, 0, 0.6f});
    cliques_.limpar();
    if (controles_) {
        const glm::vec2 p{80, 20}, s{480, 320};  // cresce com a lista (18 linhas)
        desenharPainel(r, p, s);
        f.desenharCentralizado(r, "Controles", {320, p.y + 12}, cor::ouro);
        float y = p.y + 30;
        for (const auto& [tecla, acao] : CONTROLES) {
            f.desenhar(r, tecla, {p.x + 20, y}, cor::ouro);
            f.desenhar(r, acao, {p.x + 124, y}, cor::texto);
            y += 15;
        }
        f.desenharCentralizado(r, "Esc ou clique voltar", {320, p.y + s.y - 12}, cor::apagado);
        return;
    }
    const glm::vec2 p{190, 100}, s{260, 140};
    desenharPainel(r, p, s);
    cliques_.area(p, s, Cliques::PAINEL);
    barraX_ = p.x + 130;
    barraL_ = static_cast<float>(std::max(1, f.largura("||||||||||")));
    f.desenharCentralizado(r, "Opções", {320, p.y + 12}, cor::ouro);
    auto barraVol = [&](int v) {
        std::string b;
        for (int i = 0; i < 10; ++i) b += i < v ? '|' : '.';
        return b;
    };
    const std::string linhas[4][2] = {{"Música", barraVol(ctx.opcoes.musica)},
                                      {"Efeitos", barraVol(ctx.opcoes.efeitos)},
                                      {"Tela cheia", ctx.opcoes.telaCheia ? "Sim" : "Não"},
                                      {"Voltar", ""}};
    for (int i = 0; i < 4; ++i) {
        const float y = p.y + 36 + i * 20.0f;
        if (i == sel_) r.retangulo({p.x + 10, y - 5}, {s.x - 20, 13}, {0.45f, 0.36f, 0.22f, 0.45f});
        cliques_.area({p.x + 10, y - 7}, {s.x - 20, 17}, i);
        f.desenhar(r, linhas[i][0], {p.x + 24, y - 2}, i == sel_ ? cor::ouro : cor::texto);
        f.desenhar(r, linhas[i][1], {p.x + 130, y - 2}, cor::texto);
    }
    f.desenharCentralizado(r, "W/S escolher   A/D ou clique ajustar   Esc voltar", {320, p.y + s.y - 12}, cor::apagado);
}

}
