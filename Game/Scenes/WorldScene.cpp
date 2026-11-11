#include "Game/Scenes/WorldScene.h"

#include <format>

#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>

#include "Engine/Core/Logger.h"
#include "Game/Demo/Roteiro.h"
#include "Game/Exploration/Components.h"
#include "Game/Scenes/TitleScene.h"

namespace koa {

namespace {
const std::vector<std::string> OPCOES_PAUSA = {"Continuar", "Salvar", "Voltar ao título", "Sair do jogo"};
constexpr float LETRAS_POR_SEGUNDO = 45.0f;
}

WorldScene::WorldScene(Contexto& ctx, std::optional<DadosSave> save)
    : ctx_(ctx), rend_(ctx.app.recursos()), hud_(ctx.app.recursos(), ctx.app.fonte(), ctx.dados) {
    std::string erro;
    carregou_ = mundo_.carregar(ctx_.dados, ctx_.pastaData, &erro);
    if (!carregou_) ke::log::erro("mundo não carregou: {}", erro);
    if (save) {
        mundo_.aplicar(*save);
        hud_.aviso("Bem-vindo de volta, herdeiro.", cor::texto);
    } else {
        hud_.aviso("As terras de Valdren aguardam seu herdeiro.", cor::texto);
    }
    luz_.criar(640, 360);
    cam_.vista = {640, 360};
    cam_.limite = mundo_.tamanhoPixels();
    cam_.posicionar(mundo_.jogador.pos - glm::vec2(0, 10));
}

ke::ColorGrade WorldScene::grade() const {
    ke::ColorGrade g = gradeDaEra(mundo_.reino.eraVisual);
    // Cada estação puxa a cor um pouco para o seu lado
    static const glm::vec3 tintas[4] = {{1.00f, 1.00f, 1.00f}, {1.03f, 1.01f, 0.95f}, {1.06f, 0.97f, 0.88f}, {0.92f, 0.97f, 1.06f}};
    g.tinta *= tintas[static_cast<int>(mundo_.relogio.estacao())];
    return g;
}

bool WorldScene::salvar() {
    DadosSave d = mundo_.capturar();
    d.versaoJogo = VERSAO_JOGO;
    d.dataHora = agoraTexto();
    std::string erro;
    if (!ctx_.saves->salvar(d, "Save_001", &erro)) {
        hud_.aviso("Não consegui salvar: " + erro, cor::ruim);
        ke::log::erro("salvar: {}", erro);
        return false;
    }
    hud_.aviso("Jogo salvo.", cor::bom);
    ke::log::info("jogo salvo em {}", ctx_.saves->pasta().string());
    return true;
}

std::vector<std::string> WorldScene::opcoesPainel() const {
    std::vector<std::string> o;
    const BuildingData* b = mundo_.reino.construcao(painel_);
    const BuildingDef* d = b ? mundo_.def(*b) : nullptr;
    if (d && d->nivel(b->level + 1)) o.push_back("Reconstruir");
    if (painel_ == "casa_herdeiro") o.push_back("Dormir até amanhã");
    o.push_back("Fechar");
    return o;
}

void WorldScene::entradaJogo() {
    ke::Input& in = ctx_.app.input();
    EntradaMundo e;
    if (in.segurando(SDL_SCANCODE_A) || in.segurando(SDL_SCANCODE_LEFT)) e.mover.x -= 1;
    if (in.segurando(SDL_SCANCODE_D) || in.segurando(SDL_SCANCODE_RIGHT)) e.mover.x += 1;
    if (in.segurando(SDL_SCANCODE_W) || in.segurando(SDL_SCANCODE_UP)) e.mover.y -= 1;
    if (in.segurando(SDL_SCANCODE_S) || in.segurando(SDL_SCANCODE_DOWN)) e.mover.y += 1;
    e.interagir = in.pressionou(SDL_SCANCODE_E) || in.pressionou(SDL_SCANCODE_SPACE);
    e.interagirSegurando = in.segurando(SDL_SCANCODE_E) || in.segurando(SDL_SCANCODE_SPACE);
    if (in.pressionou(SDL_SCANCODE_ESCAPE)) {
        pausa_ = true;
        selPausa_ = 0;
        return;
    }
    if (in.pressionou(SDL_SCANCODE_F5)) salvar();
    mundo_.passo(static_cast<float>(1.0 / 60.0), e);
}

void WorldScene::entradaPainel() {
    ke::Input& in = ctx_.app.input();
    const auto opcoes = opcoesPainel();
    const int n = static_cast<int>(opcoes.size());
    selPainel_ = std::clamp(selPainel_, 0, n - 1);
    if (in.pressionou(SDL_SCANCODE_ESCAPE) || in.pressionou(SDL_SCANCODE_Q)) {
        painel_.clear();
        return;
    }
    if (in.pressionou(SDL_SCANCODE_W) || in.pressionou(SDL_SCANCODE_UP)) selPainel_ = (selPainel_ + n - 1) % n;
    if (in.pressionou(SDL_SCANCODE_S) || in.pressionou(SDL_SCANCODE_DOWN)) selPainel_ = (selPainel_ + 1) % n;
    if (in.pressionou(SDL_SCANCODE_E) || in.pressionou(SDL_SCANCODE_RETURN) || in.pressionou(SDL_SCANCODE_SPACE)) {
        const std::string& o = opcoes[selPainel_];
        if (o == "Reconstruir") {
            if (mundo_.melhorarConstrucao(painel_)) painel_.clear();
        } else if (o == "Dormir até amanhã") {
            painel_.clear();
            iniciarSono(false);
        } else {
            painel_.clear();
        }
    }
    mundo_.passo(static_cast<float>(1.0 / 60.0), {});  // o mundo segue vivo (tochas, era) com o herói parado
}

void WorldScene::entradaPausa() {
    ke::Input& in = ctx_.app.input();
    const int n = static_cast<int>(OPCOES_PAUSA.size());
    if (in.pressionou(SDL_SCANCODE_ESCAPE)) pausa_ = false;
    if (in.pressionou(SDL_SCANCODE_W) || in.pressionou(SDL_SCANCODE_UP)) selPausa_ = (selPausa_ + n - 1) % n;
    if (in.pressionou(SDL_SCANCODE_S) || in.pressionou(SDL_SCANCODE_DOWN)) selPausa_ = (selPausa_ + 1) % n;
    if (in.pressionou(SDL_SCANCODE_RETURN) || in.pressionou(SDL_SCANCODE_E) || in.pressionou(SDL_SCANCODE_SPACE)) {
        switch (selPausa_) {
        case 0: pausa_ = false; break;
        case 1: salvar(); break;
        case 2:
            salvar();
            ctx_.app.cenas().trocar(std::make_unique<TitleScene>(ctx_));
            break;
        case 3:
            salvar();
            ctx_.app.parar();
            break;
        }
    }
}

void WorldScene::entradaDialogo(float dt) {
    ke::Input& in = ctx_.app.input();
    const int tamanho = Hud::tamanhoUtf8(conversa_->falas[linhaFala_]);
    letras_ = std::min(static_cast<float>(tamanho), letras_ + dt * LETRAS_POR_SEGUNDO);
    if (in.pressionou(SDL_SCANCODE_E) || in.pressionou(SDL_SCANCODE_SPACE) || in.pressionou(SDL_SCANCODE_RETURN) ||
        in.pressionou(SDL_SCANCODE_ESCAPE)) {
        if (letras_ < tamanho && !in.pressionou(SDL_SCANCODE_ESCAPE)) {
            letras_ = static_cast<float>(tamanho);  // primeiro toque: mostra a fala inteira
        } else if (linhaFala_ + 1 < static_cast<int>(conversa_->falas.size()) && !in.pressionou(SDL_SCANCODE_ESCAPE)) {
            ++linhaFala_;
            letras_ = 0.0f;
        } else {
            mundo_.encerrarConversa(conversa_->npc);
            conversa_.reset();
        }
    }
    mundo_.passo(dt, {});
}

void WorldScene::iniciarSono(bool desmaio) {
    transicao_ = 0.0f;
    desmaio_ = desmaio;
}

void WorldScene::poeira(glm::vec2 c, glm::vec2 meia, int n, glm::vec4 corBase) {
    std::uniform_real_distribution<float> u(-1.0f, 1.0f), v(0.4f, 1.0f);
    for (int i = 0; i < n; ++i) {
        ke::Particula p;
        p.pos = c + glm::vec2(u(rng_) * meia.x, u(rng_) * meia.y);
        p.vel = {u(rng_) * 30.0f, -v(rng_) * 30.0f};
        p.cor = corBase * glm::vec4(glm::vec3(v(rng_)), 1.0f);
        p.vida = p.vidaMax = 0.5f + v(rng_) * 0.6f;
        p.tamanho = v(rng_) > 0.7f ? 2.0f : 1.0f;
        p.gravidade = 60.0f;
        p.atrito = 2.5f;
        part_.emitir(p);
    }
}

void WorldScene::tratarEventos() {
    for (const EventoJogo& e : mundo_.tirarEventos()) {
        switch (e.tipo) {
        case EventoJogo::Tipo::Golpe: {
            const bool arvore = e.alvo == "arvore";
            poeira(e.pos, {4, 3}, 7, arvore ? glm::vec4(0.42f, 0.33f, 0.24f, 1) : glm::vec4(0.55f, 0.53f, 0.5f, 1));
            cam_.tremor = std::max(cam_.tremor, 1.2f);
            break;
        }
        case EventoJogo::Tipo::Esgotou: poeira(e.pos, {7, 5}, 16, {0.38f, 0.34f, 0.3f, 1}); break;
        case EventoJogo::Tipo::Ganhou:
            textoFlutuante(e.pos - glm::vec2(0, 14), std::format("+{} {}", e.quantidade, ctx_.dados.nomeRecurso(e.recurso)), cor::ouro);
            hud_.pulsar(e.recurso);
            break;
        case EventoJogo::Tipo::AbrirConstrucao:
            painel_ = e.alvo;
            selPainel_ = 0;
            break;
        case EventoJogo::Tipo::Construiu: {
            if (const BuildingData* b = mundo_.reino.construcao(e.alvo)) {
                const ke::AABB c = mundo_.caixaConstrucao(*b);
                poeira((c.min + c.max) * 0.5f, (c.max - c.min) * 0.55f, 120, {0.6f, 0.56f, 0.5f, 1});
            }
            flash_ = 1.0f;
            cam_.tremor = 3.0f;
            hud_.aviso(e.texto + "!", cor::ouro);
            ke::log::info("construção {} subiu para o nível {} ({})", e.alvo, e.quantidade, e.texto);
            salvar();  // salvamento automático após cada obra
            break;
        }
        case EventoJogo::Tipo::Aviso: hud_.aviso(e.texto, cor::ruim); break;
        case EventoJogo::Tipo::AbrirConversa:
            conversa_ = mundo_.conversar(e.alvo);
            linhaFala_ = 0;
            letras_ = 0.0f;
            break;
        case EventoJogo::Tipo::Desmaiou:
            hud_.aviso("Você não aguenta mais ficar de pé...", cor::ruim);
            iniciarSono(true);
            break;
        case EventoJogo::Tipo::NovoDia:
            hud_.aviso(e.texto, cor::ouro);
            if (e.alvo == "desmaio")
                hud_.aviso(e.quantidade > 0 ? std::format("Acordou em casa, dolorido. Faltam {} de ouro.", e.quantidade)
                                            : "Acordou em casa, dolorido e confuso.",
                           cor::ruim);
            ke::log::info("novo dia: {}{}", e.texto, e.alvo == "desmaio" ? " (desmaio)" : "");
            salvar();  // salvamento automático a cada manhã
            break;
        }
    }
}

void WorldScene::efeitosAmbiente(float dt) {
    std::uniform_real_distribution<float> u(0.0f, 1.0f);
    // Cinzas caindo na era sombria; no inverno, neve
    const bool inverno = mundo_.relogio.estacao() == Estacao::Inverno;
    const float taxa = inverno ? 30.0f : 12.0f * std::max(0.0f, 1.0f - mundo_.reino.eraVisual);
    tempoCinzas_ += dt * taxa;
    while (tempoCinzas_ >= 1.0f) {
        tempoCinzas_ -= 1.0f;
        ke::Particula p;
        p.pos = cam_.canto() + glm::vec2(u(rng_) * 700.0f - 30.0f, -4.0f);
        p.vel = {6.0f + u(rng_) * 10.0f, 10.0f + u(rng_) * 12.0f};
        const float g = inverno ? 0.9f + u(rng_) * 0.1f : 0.55f + u(rng_) * 0.25f;
        p.cor = {g, g, g * 1.02f, inverno ? 0.85f : 0.55f};
        p.vida = p.vidaMax = 14.0f;
        p.tamanho = u(rng_) > 0.8f ? 2.0f : 1.0f;
        part_.emitir(p);
    }
    // Fumaça das chaminés das casas reconstruídas
    tempoFumaca_ += dt;
    if (tempoFumaca_ > 0.22f) {
        tempoFumaca_ = 0.0f;
        for (const auto& b : mundo_.reino.construcoes) {
            glm::vec2 c;
            if (!rend_.chamine(mundo_, b, c)) continue;
            ke::Particula p;
            p.pos = c + glm::vec2(u(rng_) * 4.0f, 0.0f);
            p.vel = {4.0f + u(rng_) * 4.0f, -12.0f - u(rng_) * 6.0f};
            const float g = 0.45f + u(rng_) * 0.15f;
            p.cor = {g, g, g, 0.45f};
            p.vida = p.vidaMax = 2.5f;
            p.tamanho = 2.0f;
            part_.emitir(p);
        }
    }
    // Faíscas das tochas e da fogueira
    for (auto [e, pos, l] : mundo_.reg.view<const Posicao, const Luz>().each()) {
        if (u(rng_) > (l.raio > 60.0f ? 0.15f : 0.04f)) continue;
        ke::Particula p;
        p.pos = pos.v + l.desloc + glm::vec2(u(rng_) * 4.0f - 2.0f, -2.0f);
        p.vel = {u(rng_) * 8.0f - 4.0f, -20.0f - u(rng_) * 15.0f};
        p.cor = {1.0f, 0.6f, 0.25f, 0.9f};
        p.vida = p.vidaMax = 0.6f + u(rng_) * 0.5f;
        p.aditiva = true;
        part_.emitir(p);
    }
}

void WorldScene::atualizar(double dtD) {
    const float dt = static_cast<float>(dtD);
    tempo_ += dt;
    if (ctx_.roteiro) ctx_.roteiro->noMundo(*this);

    // O relógio para em menus, painéis, conversas e no sono
    mundo_.relogio.parado = pausa_ || !painel_.empty() || conversa_.has_value() || transicao_ >= 0.0f;

    if (transicao_ >= 0.0f) {
        const float antes = transicao_;
        transicao_ += dt * 1.25f;
        if (antes < 1.0f && transicao_ >= 1.0f) mundo_.dormir(desmaio_);  // tela preta: troca o dia
        if (transicao_ >= 2.0f) transicao_ = -1.0f;
        mundo_.passo(dt, {});
    } else if (pausa_) entradaPausa();
    else if (conversa_) entradaDialogo(dt);
    else if (!painel_.empty()) entradaPainel();
    else entradaJogo();

    tratarEventos();
    efeitosAmbiente(dt);
    part_.atualizar(dt);
    hud_.atualizar(dt);
    for (auto& f : flutuantes_) {
        f.vida -= dt;
        f.pos.y -= dt * 14.0f;
    }
    std::erase_if(flutuantes_, [](const Flutuante& f) { return f.vida <= 0.0f; });
    flash_ = std::max(0.0f, flash_ - dt * 1.5f);
    cam_.seguir(mundo_.jogador.pos - glm::vec2(0, 10), dt, 8.0f);
    cam_.atualizarTremor(dt);
}

void WorldScene::renderizar(float) {
    ke::Renderer2D& r = ctx_.app.renderer();
    const ke::BitmapFont& f = ctx_.app.fonte();

    // 1) Mapa de luz (só quando escurece)
    const bool noite = mundo_.relogio.escuridao() > 0.0f || mundo_.relogio.minutos < 7 * 60;
    if (noite) {
        luz_.comecar(mundo_.relogio.luzAmbiente());
        r.comecar(cam_.projecao());
        rend_.desenharLuzes(r, mundo_, cam_, tempo_);
        r.terminar();
        luz_.terminar();
    }

    // 2) Mundo
    r.comecar(cam_.projecao());
    rend_.desenhar(r, mundo_, cam_, tempo_, part_, mostrarColisoes_);
    if (noite) {
        // A cena é multiplicada pela luz (a textura do framebuffer vem de cabeça para baixo)
        r.definirMistura(ke::Mistura::Multiplicar);
        r.quad(luz_.textura(), cam_.canto(), cam_.vista, {0, 1, 1, 0});
        r.definirMistura(ke::Mistura::Normal);
    }
    if (painel_.empty() && !pausa_ && !conversa_ && transicao_ < 0.0f) rend_.desenharAlvo(r, f, mundo_, tempo_);
    for (const auto& t : flutuantes_) {
        glm::vec4 c = t.cor;
        c.a = std::min(1.0f, t.vida * 1.5f);
        f.desenharCentralizado(r, t.texto, t.pos, c);
    }
    r.terminar();

    // 3) Interface em coordenadas da tela
    r.comecar(glm::ortho(0.0f, 640.0f, 360.0f, 0.0f, -1.0f, 1.0f));
    if (flash_ > 0.0f) r.retangulo({0, 0}, {640, 360}, {1.0f, 0.95f, 0.85f, flash_ * 0.6f});
    hud_.desenhar(r, mundo_, tempo_);
    if (!painel_.empty()) hud_.painelConstrucao(r, mundo_, painel_, opcoesPainel(), selPainel_, tempo_);
    if (conversa_) hud_.caixaDialogo(r, *conversa_, linhaFala_, static_cast<int>(letras_), tempo_);
    if (pausa_) hud_.menuPausa(r, OPCOES_PAUSA, selPausa_);
    if (transicao_ >= 0.0f) {
        const float a = transicao_ < 1.0f ? transicao_ : 2.0f - transicao_;
        r.retangulo({0, 0}, {640, 360}, {0, 0, 0, std::clamp(a * 1.2f, 0.0f, 1.0f)});
        if (transicao_ > 0.7f && transicao_ < 1.3f)
            f.desenharCentralizado(r, desmaio_ ? "Tudo fica escuro..." : "Zzz...", {320, 180}, cor::apagado);
    }
    r.terminar();
}

void WorldScene::interfaceDepuracao() {
    ImGui::SetNextWindowPos({10, 10}, ImGuiCond_FirstUseEver);
    ImGui::Begin("Kingdom Engine — depuração (F1)");
    ImGui::Text("FPS %.0f | chamadas de desenho %d", ctx_.app.fps(), ctx_.app.renderer().chamadasDesenho());
    const Jogador& j = mundo_.jogador;
    ImGui::Text("Herói: (%.1f, %.1f) tile (%d, %d)", j.pos.x, j.pos.y, static_cast<int>(j.pos.x) / 16, static_cast<int>(j.pos.y) / 16);
    ImGui::Text("Entidades: %d | partículas: %d", static_cast<int>(mundo_.reg.storage<entt::entity>().size()),
                static_cast<int>(part_.particulas().size()));
    ImGui::Checkbox("Mostrar colisões", &mostrarColisoes_);
    if (ImGui::CollapsingHeader("Tempo", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("%s  %s", mundo_.relogio.textoData().c_str(), mundo_.relogio.textoHora().c_str());
        ImGui::SliderFloat("Minutos", &mundo_.relogio.minutos, 360.0f, 26.0f * 60.0f - 1.0f, "%.0f");
        ImGui::InputInt("Dia", &mundo_.relogio.dia);
        mundo_.relogio.dia = std::max(1, mundo_.relogio.dia);
        if (ImGui::Button("Dormir agora")) iniciarSono(false);
    }
    if (ImGui::CollapsingHeader("Moradores")) {
        for (auto [e, m, p, a, rel] : mundo_.reg.view<const Morador, const Posicao, const Agenda, Relacionamento>().each()) {
            const NpcDef& d = ctx_.dados.npcs.at(m.id);
            const ItemRotina* it = a.itemAtual >= 0 ? &d.rotina[a.itemAtual] : nullptr;
            ImGui::Text("%s: %s em %s%s", d.nome.c_str(), it ? it->acao.c_str() : "-", it ? it->local.c_str() : "-",
                        a.dormindo ? " (dormindo)" : "");
            ImGui::SliderInt(("Amizade##" + m.id).c_str(), &rel.afinidade, 0, 1000);
        }
    }
    if (ImGui::CollapsingHeader("Inventário", ImGuiTreeNodeFlags_DefaultOpen)) {
        for (const auto& [id, r] : ctx_.dados.recursos) {
            int q = mundo_.inventario.quantidade(id);
            if (ImGui::InputInt(r.nome.c_str(), &q, 1, 10)) mundo_.inventario.definir(id, q);
        }
    }
    if (ImGui::CollapsingHeader("Reino", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Desenvolvimento %d | era-alvo: %s", mundo_.reino.desenvolvimento(), nomeEra(mundo_.reino.eraAlvo()));
        ImGui::SliderInt("Forçar era (-1 = auto)", &mundo_.reino.eraForcada, -1, 3);
        ImGui::SliderFloat("Era visual", &mundo_.reino.eraVisual, 0.0f, 3.0f);
        for (auto& b : mundo_.reino.construcoes) {
            const BuildingDef* d = mundo_.def(b);
            if (d && ImGui::SliderInt(b.id.c_str(), &b.level, 1, d->nivelMax()))
                if (const auto* nv = d->nivel(b.level)) b.name = nv->nome;
        }
    }
    if (ImGui::Button("Salvar (F5)")) salvar();
    ImGui::SameLine();
    if (ImGui::Button("Recarregar save")) {
        std::string erro, aviso;
        if (auto d = ctx_.saves->carregar("Save_001", &erro, &aviso)) ctx_.app.cenas().trocar(std::make_unique<WorldScene>(ctx_, d));
    }
    ImGui::End();
}

}
