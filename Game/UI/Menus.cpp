#include "Game/UI/Menus.h"

#include <format>

#include <SDL3/SDL_keyboard.h>

#include "Game/UI/Teclas.h"

namespace koa {

namespace {

// Linhas da tela de Opções
enum LinhaOpcao { MUSICA, EFEITOS, TELA_CHEIA, TEXTO_GRANDE, VELOCIDADE, CONTROLES, VOLTAR, N_OPCOES };
const char* VELOCIDADES[] = {"Lenta", "Normal", "Rápida", "Instantânea"};

// Teclas que não mudam (aparecem no fim da tela de controles)
const char* FIXAS[][2] = {
    {"segurar interagir", "continuar golpeando árvores e rochas"},
    {"clique", "atacar (vira para o cursor); escolher nos painéis"},
    {"F11 / Esc", "tela cheia / menu"},
};

constexpr int N_ACOES = static_cast<int>(std::size(ACOES_TECLAS));

std::string nomeTecla(SDL_Scancode t) {
    const char* n = SDL_GetScancodeName(t);
    return n && *n ? n : "?";
}

}

void TelaOpcoes::entrada(Contexto& ctx) {
    ke::Input& in = ctx.app.input();
    ctx.opcoes.telaCheia = ctx.app.janela().telaCheia();  // F11 também muda a tela cheia
    bool confirmar = in.pressionou(SDL_SCANCODE_E) || in.pressionou(SDL_SCANCODE_RETURN) || in.pressionou(SDL_SCANCODE_SPACE);
    if (controles_) {
        entradaControles(ctx, confirmar);
        return;
    }
    if (in.pressionou(SDL_SCANCODE_W) || in.pressionou(SDL_SCANCODE_UP)) sel_ = (sel_ + N_OPCOES - 1) % N_OPCOES;
    if (in.pressionou(SDL_SCANCODE_S) || in.pressionou(SDL_SCANCODE_DOWN)) sel_ = (sel_ + 1) % N_OPCOES;
    const int d = (in.pressionou(SDL_SCANCODE_D) || in.pressionou(SDL_SCANCODE_RIGHT) ? 1 : 0) -
                  (in.pressionou(SDL_SCANCODE_A) || in.pressionou(SDL_SCANCODE_LEFT) ? 1 : 0);
    bool mudou = false;
    // Mouse: passar seleciona a linha; clicar na barra de volume põe o volume onde clicou; clicar fora volta
    if (cliqueFecha(in, cliques_)) {
        aberta_ = false;
        return;
    }
    const bool clique = listaComMouse(in, cliques_, sel_, N_OPCOES);
    if (clique && sel_ <= EFEITOS && in.mouse().x >= barraX_ - 4) {
        const int v = std::clamp(static_cast<int>((in.mouse().x - barraX_) / barraL_ * 10.0f) + 1, 0, 10);
        (sel_ == MUSICA ? ctx.opcoes.musica : ctx.opcoes.efeitos) = in.mouse().x < barraX_ ? 0 : v;
        mudou = true;
    } else if (clique) {
        confirmar = true;
    }
    if (d != 0 && sel_ == MUSICA) ctx.opcoes.musica = std::clamp(ctx.opcoes.musica + d, 0, 10), mudou = true;
    if (d != 0 && sel_ == EFEITOS) ctx.opcoes.efeitos = std::clamp(ctx.opcoes.efeitos + d, 0, 10), mudou = true;
    if ((d != 0 || confirmar) && sel_ == TELA_CHEIA) ctx.opcoes.telaCheia = !ctx.opcoes.telaCheia, mudou = true;
    if ((d != 0 || confirmar) && sel_ == TEXTO_GRANDE) ctx.opcoes.textoGrande = !ctx.opcoes.textoGrande, mudou = true;
    if ((d != 0 || confirmar) && sel_ == VELOCIDADE) ctx.opcoes.velocidadeTexto = (ctx.opcoes.velocidadeTexto + (d < 0 ? 3 : 1)) % 4, mudou = true;
    if (mudou) {
        ctx.aplicarOpcoes();
        if (sel_ == EFEITOS) ctx.app.audio().tocar(ke::Som::Moeda);  // amostra do volume dos efeitos
        ctx.salvarOpcoes();
    }
    if (confirmar && sel_ == CONTROLES) abrirControles(true);
    if (in.pressionou(SDL_SCANCODE_ESCAPE) || (confirmar && sel_ == VOLTAR)) aberta_ = false;
}

// Controles: cada ação com a tecla de agora; E (ou clique) e depois a tecla nova troca. Quem já usava a tecla fica com a antiga
void TelaOpcoes::entradaControles(Contexto& ctx, bool confirmar) {
    ke::Input& in = ctx.app.input();
    const int n = N_ACOES + 2;  // + restaurar padrão + voltar
    if (esperando_) {
        if (in.pressionou(SDL_SCANCODE_ESCAPE) || in.clicou(ke::BotaoMouse::Esquerdo)) {
            esperando_ = false;
            return;
        }
        const SDL_Scancode t = in.ultimaPressionada();
        if (t == SDL_SCANCODE_UNKNOWN) return;
        esperando_ = false;
        if (teclaReservada(t)) {
            aviso_ = "Essa tecla é do jogo (Esc, Enter e F11 não mudam).";
            return;
        }
        const AcaoTecla& a = ACOES_TECLAS[sel_];
        const SDL_Scancode antiga = in.tecla(a.id, a.padrao);
        for (const AcaoTecla& outra : ACOES_TECLAS)
            if (outra.id != a.id && in.tecla(outra.id, outra.padrao) == t) {
                ctx.opcoes.teclas[outra.id] = nomeTecla(antiga);
                aviso_ = std::format("{} passou para {}.", outra.nome, nomeTecla(antiga));
            }
        ctx.opcoes.teclas[a.id] = nomeTecla(t);
        if (aviso_.empty() || !aviso_.ends_with(nomeTecla(antiga) + ".")) aviso_ = std::format("{}: {}.", a.nome, nomeTecla(t));
        // Só guarda o que difere do padrão
        for (const AcaoTecla& x : ACOES_TECLAS)
            if (const auto it = ctx.opcoes.teclas.find(x.id); it != ctx.opcoes.teclas.end() && it->second == nomeTecla(x.padrao)) ctx.opcoes.teclas.erase(it);
        ctx.aplicarOpcoes();
        ctx.salvarOpcoes();
        return;
    }
    if (cliqueFecha(in, cliques_) || in.pressionou(SDL_SCANCODE_ESCAPE)) {
        voltarDosControles();
        return;
    }
    if (in.pressionou(SDL_SCANCODE_W) || in.pressionou(SDL_SCANCODE_UP)) selControle_ = (selControle_ + n - 1) % n;
    if (in.pressionou(SDL_SCANCODE_S) || in.pressionou(SDL_SCANCODE_DOWN)) selControle_ = (selControle_ + 1) % n;
    if (listaComMouse(in, cliques_, selControle_, n)) confirmar = true;
    sel_ = std::min(selControle_, N_ACOES - 1);
    if (!confirmar) return;
    aviso_.clear();
    if (selControle_ < N_ACOES) {
        sel_ = selControle_;
        esperando_ = true;
    } else if (selControle_ == N_ACOES) {
        ctx.opcoes.teclas.clear();
        ctx.aplicarOpcoes();
        ctx.salvarOpcoes();
        aviso_ = "Teclas de volta ao padrão.";
    } else {
        voltarDosControles();
    }
}

void TelaOpcoes::voltarDosControles() {
    controles_ = false;
    sel_ = CONTROLES;
    if (!dasOpcoes_) aberta_ = false;
}

void TelaOpcoes::desenhar(ke::Renderer2D& r, const ke::BitmapFont& f, const Contexto& ctx) const {
    r.retangulo({0, 0}, {640, 360}, {0, 0, 0, 0.6f});
    cliques_.limpar();
    const ke::Input& in = ctx.app.input();
    if (controles_) {
        const glm::vec2 p{70, 8}, s{500, 344};
        desenharPainel(r, p, s);
        cliques_.area(p, s, Cliques::PAINEL);
        f.desenharCentralizado(r, "Controles", {320, p.y + 10}, cor::ouro);
        float y = p.y + 24;
        for (int i = 0; i < N_ACOES + 2; ++i, y += 11) {
            const bool s1 = i == selControle_;
            if (s1) r.retangulo({p.x + 10, y - 2}, {s.x - 20, 11}, {0.45f, 0.36f, 0.22f, 0.45f});
            cliques_.area({p.x + 10, y - 2}, {s.x - 20, 11}, i);
            if (i < N_ACOES) {
                const AcaoTecla& a = ACOES_TECLAS[i];
                const SDL_Scancode t = in.tecla(a.id, a.padrao);
                const std::string tecla = s1 && esperando_ ? "aperte a tecla..." : nomeTecla(t);
                f.desenhar(r, tecla, {p.x + 20, y}, s1 && esperando_ ? cor::bom : t == a.padrao ? cor::ouro : cor::bom);
                f.desenhar(r, a.nome, {p.x + 170, y}, s1 ? cor::ouro : cor::texto);
            } else {
                f.desenhar(r, i == N_ACOES ? "Restaurar o padrão" : "Voltar", {p.x + 20, y}, s1 ? cor::ouro : cor::texto);
            }
        }
        for (const auto& [tecla, acao] : FIXAS) {
            f.desenhar(r, tecla, {p.x + 20, y}, cor::apagado);
            f.desenhar(r, acao, {p.x + 170, y}, cor::apagado);
            y += 11;
        }
        f.desenharCentralizado(r, aviso_.empty() ? "W/S escolher   E ou clique trocar a tecla   Esc voltar" : aviso_, {320, p.y + s.y - 10},
                               aviso_.empty() ? cor::apagado : cor::bom);
        return;
    }
    const glm::vec2 p{180, 70}, s{280, 200};
    desenharPainel(r, p, s);
    cliques_.area(p, s, Cliques::PAINEL);
    barraX_ = p.x + 150;
    barraL_ = static_cast<float>(std::max(1, f.largura("||||||||||")));
    f.desenharCentralizado(r, "Opções", {320, p.y + 12}, cor::ouro);
    auto barraVol = [&](int v) {
        std::string b;
        for (int i = 0; i < 10; ++i) b += i < v ? '|' : '.';
        return b;
    };
    const std::string linhas[N_OPCOES][2] = {{"Música", barraVol(ctx.opcoes.musica)},
                                             {"Efeitos", barraVol(ctx.opcoes.efeitos)},
                                             {"Tela cheia", ctx.opcoes.telaCheia ? "Sim" : "Não"},
                                             {"Texto grande", ctx.opcoes.textoGrande ? "Sim" : "Não"},
                                             {"Velocidade do texto", VELOCIDADES[std::clamp(ctx.opcoes.velocidadeTexto, 0, 3)]},
                                             {"Controles e teclas", ""},
                                             {"Voltar", ""}};
    for (int i = 0; i < N_OPCOES; ++i) {
        const float y = p.y + 34 + i * 20.0f;
        if (i == sel_) r.retangulo({p.x + 10, y - 5}, {s.x - 20, 13}, {0.45f, 0.36f, 0.22f, 0.45f});
        cliques_.area({p.x + 10, y - 7}, {s.x - 20, 17}, i);
        f.desenhar(r, linhas[i][0], {p.x + 18, y - 2}, i == sel_ ? cor::ouro : cor::texto);
        f.desenhar(r, linhas[i][1], {p.x + 150, y - 2}, cor::texto);
    }
    f.desenharCentralizado(r, "W/S escolher   A/D ou clique ajustar   Esc voltar", {320, p.y + s.y - 12}, cor::apagado);
}

}
