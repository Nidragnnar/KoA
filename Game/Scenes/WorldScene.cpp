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
const std::vector<std::string> OPCOES_PAUSA = {"Continuar", "Salvar", "Opções", "Controles", "Voltar ao título", "Sair do jogo"};
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
    if (!ctx_.saves->salvar(d, ctx_.slot, &erro)) {
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
    if (painel_ == "casa_herdeiro") o.push_back("Dormir até amanhã"), o.push_back("Cozinhar");
    if (b && b->defId == "forja" && b->constructed) o.push_back("Forjar equipamento");
    if (b && b->defId == "guilda" && b->constructed) o.push_back("Abrir a guilda");
    if (b && b->defId == "castelo") {  // fase 16
        o.push_back(std::format("Alas do castelo ({} de {})", mundo_.alas.size(), mundo_.vagasAlas()));
        o.push_back("Brasão da linhagem");
    }
    if (b && b->defId == "muralha") {
        const Custo c = mundo_.custoReparo();
        if (!c.empty()) o.push_back(std::format("Reparar portão ({} pedra, {} madeira)", c.at("pedra"), c.at("madeira")));
    }
    if (b && b->defId == "mercado" && b->constructed) {
        o.push_back(std::format("Comprar 10 pedra ({} ouro)", mundo_.precoLote("pedra")));
        o.push_back(std::format("Comprar 10 madeira ({} ouro)", mundo_.precoLote("madeira")));
    }
    o.push_back("Fechar");
    return o;
}

std::vector<std::string> WorldScene::opcoesForja() const {
    std::vector<std::string> v;
    for (const auto& id : ctx_.dados.ordemEquipamentos)
        if (ctx_.dados.equipamentos.at(id).forja > 0) v.push_back(id);
    return v;
}

void WorldScene::entradaForja() {
    ke::Input& in = ctx_.app.input();
    const auto opcoes = opcoesForja();
    const int n = static_cast<int>(opcoes.size());
    if (in.pressionou(SDL_SCANCODE_ESCAPE) || in.pressionou(SDL_SCANCODE_Q) || cliqueFecha(in, hud_.cliques)) forja_ = false;
    if (in.pressionou(SDL_SCANCODE_W) || in.pressionou(SDL_SCANCODE_UP)) selForja_ = (selForja_ + n - 1) % n;
    if (in.pressionou(SDL_SCANCODE_S) || in.pressionou(SDL_SCANCODE_DOWN)) selForja_ = (selForja_ + 1) % n;
    const bool clique = listaComMouse(in, hud_.cliques, selForja_, n);
    if (in.pressionou(SDL_SCANCODE_E) || in.pressionou(SDL_SCANCODE_RETURN) || in.pressionou(SDL_SCANCODE_SPACE) || clique)
        mundo_.forjar(opcoes[selForja_]);
}

void WorldScene::entradaCronicas() {
    ke::Input& in = ctx_.app.input();
    if (in.pressionou(SDL_SCANCODE_C) || in.pressionou(SDL_SCANCODE_ESCAPE) || cliqueFecha(in, hud_.cliques)) {
        cronicas_ = false;
        return;
    }
    if (in.pressionou(SDL_SCANCODE_A) || in.pressionou(SDL_SCANCODE_LEFT) || in.pressionou(SDL_SCANCODE_D) ||
        in.pressionou(SDL_SCANCODE_RIGHT))
        abaCronicas_ = 1 - abaCronicas_;
    // Abas também no clique (ids 100 e 101)
    if (const int id = hud_.cliques.em(in.mouse()); in.clicou(ke::BotaoMouse::Esquerdo) && (id == 100 || id == 101)) {
        abaCronicas_ = id - 100;
        return;
    }
    const bool cima = in.pressionou(SDL_SCANCODE_W) || in.pressionou(SDL_SCANCODE_UP) || in.roda() > 0.0f;
    const bool baixo = in.pressionou(SDL_SCANCODE_S) || in.pressionou(SDL_SCANCODE_DOWN) || in.roda() < 0.0f;
    if (abaCronicas_ == 0) {
        const int n = static_cast<int>(mundo_.cronicas.size());
        if (cima) rolagem_ = std::max(0, rolagem_ - 1);
        if (baixo) rolagem_ = std::min(std::max(0, n - 1), rolagem_ + 1);
    } else {
        const int n = static_cast<int>(ctx_.dados.lore.pistas.size());
        if (cima) selFragmento_ = (selFragmento_ + n - 1) % n;
        if (baixo) selFragmento_ = (selFragmento_ + 1) % n;
        const bool clique = listaComMouse(in, hud_.cliques, selFragmento_, n);
        const PistaDef& p = ctx_.dados.lore.pistas[selFragmento_];
        if ((in.pressionou(SDL_SCANCODE_E) || in.pressionou(SDL_SCANCODE_RETURN) || in.pressionou(SDL_SCANCODE_SPACE) || clique) &&
            mundo_.marcos.contains("pista:" + p.id))
            leitura_ = p.id;
    }
}

void WorldScene::entradaLoja() {
    ke::Input& in = ctx_.app.input();
    const auto& itens = ctx_.dados.npcs.at(loja_).loja;
    const int n = static_cast<int>(itens.size());
    if (in.pressionou(SDL_SCANCODE_ESCAPE) || in.pressionou(SDL_SCANCODE_Q) || cliqueFecha(in, hud_.cliques)) {
        loja_.clear();
        return;
    }
    if (in.pressionou(SDL_SCANCODE_W) || in.pressionou(SDL_SCANCODE_UP)) selLoja_ = (selLoja_ + n - 1) % n;
    if (in.pressionou(SDL_SCANCODE_S) || in.pressionou(SDL_SCANCODE_DOWN)) selLoja_ = (selLoja_ + 1) % n;
    const bool clique = listaComMouse(in, hud_.cliques, selLoja_, n);
    if (in.pressionou(SDL_SCANCODE_E) || in.pressionou(SDL_SCANCODE_RETURN) || in.pressionou(SDL_SCANCODE_SPACE) || clique)
        mundo_.comprar(loja_, itens[selLoja_].id);
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
    e.atacar = in.pressionou(SDL_SCANCODE_J);
    // Clique esquerdo: ataca virado para o cursor (o J ataca para onde o herói já olha)
    if (in.clicou(ke::BotaoMouse::Esquerdo) && mundo_.pescando()) {
        e.interagir = true;  // pescando, o clique puxa a linha
    } else if (in.clicou(ke::BotaoMouse::Esquerdo)) {
        e.atacar = true;
        if (mundo_.jogador.acao <= 0.0f) {
            const glm::vec2 d = cam_.canto() + in.mouse() - (mundo_.jogador.pos - glm::vec2(0, 10));
            if (std::abs(d.x) > std::abs(d.y)) mundo_.jogador.dir = d.x < 0 ? Direcao::Esquerda : Direcao::Direita;
            else if (std::abs(d.y) > 0.0f) mundo_.jogador.dir = d.y < 0 ? Direcao::Cima : Direcao::Baixo;
        }
    }
    e.comer = in.pressionou(SDL_SCANCODE_H);
    if (in.pressionou(SDL_SCANCODE_ESCAPE)) {
        pausa_ = true;
        selPausa_ = 0;
        return;
    }
    if (in.pressionou(SDL_SCANCODE_F5)) salvar();
    if (in.pressionou(SDL_SCANCODE_L)) {
        diario_ = true;
        return;
    }
    if (in.pressionou(SDL_SCANCODE_C)) {
        // Abre nas páginas mais recentes
        cronicas_ = true;
        rolagem_ = std::max(0, static_cast<int>(mundo_.cronicas.size()) - Hud::paginasVisiveis());
        return;
    }
    if (in.pressionou(SDL_SCANCODE_B)) {
        if (!mundo_.noVilarejo()) hud_.aviso("Só dá para construir no vilarejo.", cor::ruim);
        else menuObras_ = true;
        return;
    }
    if (in.pressionou(SDL_SCANCODE_K)) {
        painelReino_ = true;
        return;
    }
    if (in.pressionou(SDL_SCANCODE_R)) {
        painelRegioes_ = true;
        return;
    }
    if (in.pressionou(SDL_SCANCODE_G)) {
        painelConselho_ = true;
        selConselho_ = 0;
        proposta_ = -1;
        return;
    }
    mundo_.passo(static_cast<float>(1.0 / 60.0), e);
}

std::string WorldScene::propostoConselho() const {
    const auto& cargos = ctx_.dados.conselho.cargos;
    if (selConselho_ < 0 || selConselho_ >= static_cast<int>(cargos.size())) return "";
    if (proposta_ < 0) return mundo_.ocupante(cargos[selConselho_].id);
    const auto cand = mundo_.candidatos();
    return proposta_ == 0 || proposta_ > static_cast<int>(cand.size()) ? "" : cand[proposta_ - 1];
}

void WorldScene::entradaConselho() {
    ke::Input& in = ctx_.app.input();
    if (in.pressionou(SDL_SCANCODE_ESCAPE) || in.pressionou(SDL_SCANCODE_G) || cliqueFecha(in, hud_.cliques)) {
        painelConselho_ = false;
        return;
    }
    if (!mundo_.conselhoFormado()) return;
    const auto& cargos = ctx_.dados.conselho.cargos;
    const int nCargos = static_cast<int>(cargos.size());
    const int n = nCargos + (mundo_.decisaoAtual() ? 1 : 0);  // a última linha é o caso esperando
    selConselho_ = std::clamp(selConselho_, 0, n - 1);
    const int antes = selConselho_;
    if (in.pressionou(SDL_SCANCODE_W) || in.pressionou(SDL_SCANCODE_UP)) selConselho_ = (selConselho_ + n - 1) % n;
    if (in.pressionou(SDL_SCANCODE_S) || in.pressionou(SDL_SCANCODE_DOWN)) selConselho_ = (selConselho_ + 1) % n;
    const bool clique = listaComMouse(in, hud_.cliques, selConselho_, n);
    // Setas clicáveis do seletor (300 + linha = anterior, 310 + linha = próximo)
    const int seta = in.clicou(ke::BotaoMouse::Esquerdo) ? hud_.cliques.em(in.mouse()) : -1;
    if (seta >= 300 && seta < 320) selConselho_ = seta % 10;
    if (selConselho_ != antes) proposta_ = -1;  // outro cargo: começa por quem já está nele
    int dir = 0;
    if (in.pressionou(SDL_SCANCODE_A) || in.pressionou(SDL_SCANCODE_LEFT) || (seta >= 300 && seta < 310)) dir = -1;
    if (in.pressionou(SDL_SCANCODE_D) || in.pressionou(SDL_SCANCODE_RIGHT) || (seta >= 310 && seta < 320)) dir = 1;
    if (dir != 0 && selConselho_ < nCargos) {
        // Lista: vago, depois os moradores
        const auto cand = mundo_.candidatos();
        const int total = static_cast<int>(cand.size()) + 1;
        int atual = proposta_;
        if (atual < 0) {
            const std::string quem = mundo_.ocupante(cargos[selConselho_].id);
            const auto it = std::ranges::find(cand, quem);
            atual = quem.empty() || it == cand.end() ? 0 : static_cast<int>(it - cand.begin()) + 1;
        }
        proposta_ = (atual + dir + total) % total;
        ctx_.app.audio().tocar(ke::Som::Menu);
    }
    if (in.pressionou(SDL_SCANCODE_E) || in.pressionou(SDL_SCANCODE_RETURN) || in.pressionou(SDL_SCANCODE_SPACE) || clique) {
        if (selConselho_ == nCargos) {
            decisao_ = true;  // abre o caso por cima do conselho
            selDecisao_ = 0;
        } else if (const std::string quem = propostoConselho(); quem != mundo_.ocupante(cargos[selConselho_].id)) {
            mundo_.nomear(cargos[selConselho_].id, quem);
            proposta_ = -1;
        }
    }
}

void WorldScene::entradaGuilda() {
    ke::Input& in = ctx_.app.input();
    const DadosGuilda& dg = ctx_.dados.guilda;
    const auto recrutados = hud_.aventureirosRecrutados(mundo_);
    const bool montando = abaGuilda_ == 0 && passoExpedicao_ == 1 && mundo_.expedicao.destino.empty();
    // Esc volta da montagem do grupo para os destinos; senão fecha
    if (in.pressionou(SDL_SCANCODE_ESCAPE) || cliqueFecha(in, hud_.cliques)) {
        if (montando && in.pressionou(SDL_SCANCODE_ESCAPE)) passoExpedicao_ = 0, selGuilda_ = 0;
        else guilda_ = false;
        return;
    }
    // Abas: A/D ou clique (100..102)
    int aba = abaGuilda_;
    if (in.pressionou(SDL_SCANCODE_A) || in.pressionou(SDL_SCANCODE_LEFT)) aba = (aba + 2) % 3;
    if (in.pressionou(SDL_SCANCODE_D) || in.pressionou(SDL_SCANCODE_RIGHT)) aba = (aba + 1) % 3;
    const int clicado = in.clicou(ke::BotaoMouse::Esquerdo) ? hud_.cliques.em(in.mouse()) : -1;
    if (clicado >= 100 && clicado <= 102) aba = clicado - 100;
    if (aba != abaGuilda_) {
        abaGuilda_ = aba;
        selGuilda_ = passoExpedicao_ = 0;
        return;
    }
    // Tamanho da lista da aba atual
    int n = 0;
    if (abaGuilda_ == 0 && mundo_.expedicao.destino.empty()) n = passoExpedicao_ == 0 ? static_cast<int>(dg.destinos.size()) : static_cast<int>(recrutados.size()) + 1;
    else if (abaGuilda_ == 1) n = static_cast<int>(recrutados.size());
    else if (abaGuilda_ == 2) n = static_cast<int>(mundo_.reliquias.size());
    if (n == 0) return;
    selGuilda_ = std::clamp(selGuilda_, 0, n - 1);
    if (in.pressionou(SDL_SCANCODE_W) || in.pressionou(SDL_SCANCODE_UP)) selGuilda_ = (selGuilda_ + n - 1) % n;
    if (in.pressionou(SDL_SCANCODE_S) || in.pressionou(SDL_SCANCODE_DOWN)) selGuilda_ = (selGuilda_ + 1) % n;
    const bool clique = listaComMouse(in, hud_.cliques, selGuilda_, n);
    const bool confirmar = in.pressionou(SDL_SCANCODE_E) || in.pressionou(SDL_SCANCODE_RETURN) || in.pressionou(SDL_SCANCODE_SPACE) || clique;
    if (abaGuilda_ == 0 && passoExpedicao_ == 0 && confirmar) {
        const DestinoDef& d = dg.destinos[selGuilda_];
        if (mundo_.destinoAberto(d)) {
            destinoSel_ = d.id;
            grupoSel_.clear();
            passoExpedicao_ = 1;
            selGuilda_ = 0;
        } else {
            hud_.aviso("Ninguém conhece o caminho para lá.", cor::ruim);
        }
    } else if (montando && confirmar) {
        if (selGuilda_ < static_cast<int>(recrutados.size())) {
            // Põe ou tira do grupo (só quem está pronto, até o máximo)
            const std::string& id = recrutados[selGuilda_];
            if (const auto it = std::ranges::find(grupoSel_, id); it != grupoSel_.end()) grupoSel_.erase(it);
            else if (!mundo_.aventureiroPronto(id)) hud_.aviso(dg.aventureiro(id)->nome + " não está pronto.", cor::ruim);
            else if (static_cast<int>(grupoSel_.size()) >= dg.grupoMax) hud_.aviso(std::format("O grupo vai com até {} aventureiros.", dg.grupoMax), cor::ruim);
            else grupoSel_.push_back(id);
        } else if (mundo_.partir(destinoSel_, grupoSel_)) {
            passoExpedicao_ = 0;
            selGuilda_ = 0;
            grupoSel_.clear();
        }
    } else if (abaGuilda_ == 2) {
        // Clique na linha só escolhe (para ler); expor é no E ou no botão
        const std::string id = mundo_.reliquias[selGuilda_].id;
        const bool tecla = in.pressionou(SDL_SCANCODE_E) || in.pressionou(SDL_SCANCODE_RETURN) || in.pressionou(SDL_SCANCODE_SPACE);
        if (tecla || clicado == 500) mundo_.exporReliquia(id, !mundo_.reliquias[selGuilda_].exposta);
        if (in.pressionou(SDL_SCANCODE_P) || clicado == 501) mundo_.purificarReliquia(id);
    }
}

void WorldScene::entradaDecisao() {
    ke::Input& in = ctx_.app.input();
    const DecisaoDef* d = mundo_.decisaoAtual();
    if (!d || in.pressionou(SDL_SCANCODE_ESCAPE)) {  // Esc deixa para depois: o caso espera no conselho (G)
        decisao_ = false;
        return;
    }
    const int n = static_cast<int>(d->opcoes.size());
    if (in.pressionou(SDL_SCANCODE_W) || in.pressionou(SDL_SCANCODE_UP)) selDecisao_ = (selDecisao_ + n - 1) % n;
    if (in.pressionou(SDL_SCANCODE_S) || in.pressionou(SDL_SCANCODE_DOWN)) selDecisao_ = (selDecisao_ + 1) % n;
    const bool clique = listaComMouse(in, hud_.cliques, selDecisao_, n);
    if (in.pressionou(SDL_SCANCODE_E) || in.pressionou(SDL_SCANCODE_RETURN) || in.pressionou(SDL_SCANCODE_SPACE) || clique) {
        const std::string opcao = d->opcoes[selDecisao_].id;
        if (mundo_.decidir(opcao)) decisao_ = false;
    }
}

void WorldScene::entradaEscolha() {
    ke::Input& in = ctx_.app.input();
    const EscolhaDef& d = ctx_.dados.escolhas.at(escolha_);
    const int n = static_cast<int>(d.opcoes.size());
    // Esc adia a decisão: o chefe continua de joelhos (E perto dele abre de novo)
    if (in.pressionou(SDL_SCANCODE_ESCAPE)) {
        escolha_.clear();
        return;
    }
    if (in.pressionou(SDL_SCANCODE_W) || in.pressionou(SDL_SCANCODE_UP)) selEscolha_ = (selEscolha_ + n - 1) % n;
    if (in.pressionou(SDL_SCANCODE_S) || in.pressionou(SDL_SCANCODE_DOWN)) selEscolha_ = (selEscolha_ + 1) % n;
    // Clique fora do painel não fecha: uma decisão dessas não se perde por um clique sem querer
    const bool clique = listaComMouse(in, hud_.cliques, selEscolha_, n);
    if (in.pressionou(SDL_SCANCODE_E) || in.pressionou(SDL_SCANCODE_RETURN) || in.pressionou(SDL_SCANCODE_SPACE) || clique) {
        const std::string id = escolha_;
        if (mundo_.escolher(id, d.opcoes[selEscolha_].id)) escolha_.clear();
    }
    mundo_.passo(static_cast<float>(1.0 / 60.0), {});
}

std::vector<std::string> WorldScene::opcoesObras() const {
    std::vector<std::string> v;
    for (const auto& id : ctx_.dados.ordemConstrucoes)
        if (ctx_.dados.construcoes.at(id).construivel) v.push_back(id);
    return v;
}

void WorldScene::entradaMenuObras() {
    ke::Input& in = ctx_.app.input();
    const auto opcoes = opcoesObras();
    const int n = static_cast<int>(opcoes.size());
    if (in.pressionou(SDL_SCANCODE_ESCAPE) || in.pressionou(SDL_SCANCODE_B) || cliqueFecha(in, hud_.cliques)) {
        menuObras_ = false;
        return;
    }
    if (in.pressionou(SDL_SCANCODE_W) || in.pressionou(SDL_SCANCODE_UP)) selObra_ = (selObra_ + n - 1) % n;
    if (in.pressionou(SDL_SCANCODE_S) || in.pressionou(SDL_SCANCODE_DOWN)) selObra_ = (selObra_ + 1) % n;
    const bool clique = listaComMouse(in, hud_.cliques, selObra_, n);
    if (in.pressionou(SDL_SCANCODE_E) || in.pressionou(SDL_SCANCODE_RETURN) || in.pressionou(SDL_SCANCODE_SPACE) || clique) {
        const BuildingDef& d = ctx_.dados.construcoes.at(opcoes[selObra_]);
        if (!mundo_.monumentoLiberado(d)) {
            hud_.aviso(d.requerTexto, cor::ruim);
        } else if (d.limite > 0 && mundo_.contarConstrucoes(d.id) >= d.limite) {
            hud_.aviso("O reino já tem o máximo desta construção.", cor::ruim);
        } else if (!mundo_.inventario.tem(d.nivel(1)->custo)) {
            hud_.aviso("Faltam recursos para " + d.nivel(1)->nome + ".", cor::ruim);
        } else {
            menuObras_ = false;
            colocando_ = d.id;
        }
    }
}

glm::ivec2 WorldScene::tileFantasma() const {
    if (colocando_.empty()) return {0, 0};
    const glm::ivec2 t = ctx_.dados.construcoes.at(colocando_).tamanho;
    const glm::ivec2 p{static_cast<int>(mundo_.jogador.pos.x) / 16, static_cast<int>(mundo_.jogador.pos.y - 1) / 16};
    // A obra fica na frente do herói, com a porta (fileira de baixo + 1) livre
    switch (mundo_.jogador.dir) {
    case Direcao::Cima: return {p.x - t.x / 2, p.y - t.y - 1};
    case Direcao::Baixo: return {p.x - t.x / 2, p.y + 1};
    case Direcao::Esquerda: return {p.x - t.x - 1, p.y - t.y / 2};
    case Direcao::Direita: return {p.x + 2, p.y - t.y / 2};
    }
    return p;
}

void WorldScene::entradaColocacao() {
    ke::Input& in = ctx_.app.input();
    EntradaMundo e;
    if (in.segurando(SDL_SCANCODE_A) || in.segurando(SDL_SCANCODE_LEFT)) e.mover.x -= 1;
    if (in.segurando(SDL_SCANCODE_D) || in.segurando(SDL_SCANCODE_RIGHT)) e.mover.x += 1;
    if (in.segurando(SDL_SCANCODE_W) || in.segurando(SDL_SCANCODE_UP)) e.mover.y -= 1;
    if (in.segurando(SDL_SCANCODE_S) || in.segurando(SDL_SCANCODE_DOWN)) e.mover.y += 1;
    if (in.pressionou(SDL_SCANCODE_ESCAPE) || in.pressionou(SDL_SCANCODE_B)) {
        colocando_.clear();
        return;
    }
    if (in.pressionou(SDL_SCANCODE_E) || in.pressionou(SDL_SCANCODE_RETURN) || in.pressionou(SDL_SCANCODE_SPACE) ||
        in.clicou(ke::BotaoMouse::Esquerdo)) {
        if (!mundo_.construir(colocando_, tileFantasma()).empty()) colocando_.clear();
    }
    mundo_.passo(static_cast<float>(1.0 / 60.0), e);  // sem interagir: o E serve para confirmar
}

void WorldScene::entradaPainelReino() {
    ke::Input& in = ctx_.app.input();
    if (in.pressionou(SDL_SCANCODE_ESCAPE) || in.pressionou(SDL_SCANCODE_K) || in.pressionou(SDL_SCANCODE_E) || cliqueFecha(in, hud_.cliques))
        painelReino_ = false;
    int imp = static_cast<int>(mundo_.estado.imposto);
    const int seta = in.clicou(ke::BotaoMouse::Esquerdo) ? hud_.cliques.em(in.mouse()) : -1;  // 200 = <, 201 = >
    if (in.pressionou(SDL_SCANCODE_A) || in.pressionou(SDL_SCANCODE_LEFT) || seta == 200) imp = std::max(0, imp - 1);
    if (in.pressionou(SDL_SCANCODE_D) || in.pressionou(SDL_SCANCODE_RIGHT) || seta == 201) imp = std::min(2, imp + 1);
    if (imp != static_cast<int>(mundo_.estado.imposto)) {
        mundo_.estado.imposto = static_cast<Imposto>(imp);
        hud_.aviso(std::string("Impostos agora: ") + nomeImposto(mundo_.estado.imposto), cor::ouro);
    }
}

void WorldScene::entradaPainel() {
    ke::Input& in = ctx_.app.input();
    const auto opcoes = opcoesPainel();
    const int n = static_cast<int>(opcoes.size());
    selPainel_ = std::clamp(selPainel_, 0, n - 1);
    if (in.pressionou(SDL_SCANCODE_ESCAPE) || in.pressionou(SDL_SCANCODE_Q) || cliqueFecha(in, hud_.cliques)) {
        painel_.clear();
        return;
    }
    if (in.pressionou(SDL_SCANCODE_W) || in.pressionou(SDL_SCANCODE_UP)) selPainel_ = (selPainel_ + n - 1) % n;
    if (in.pressionou(SDL_SCANCODE_S) || in.pressionou(SDL_SCANCODE_DOWN)) selPainel_ = (selPainel_ + 1) % n;
    const bool clique = listaComMouse(in, hud_.cliques, selPainel_, n);
    if (in.pressionou(SDL_SCANCODE_E) || in.pressionou(SDL_SCANCODE_RETURN) || in.pressionou(SDL_SCANCODE_SPACE) || clique) {
        const std::string& o = opcoes[selPainel_];
        if (o == "Reconstruir") {
            if (mundo_.melhorarConstrucao(painel_)) painel_.clear();
        } else if (o == "Dormir até amanhã") {
            painel_.clear();
            iniciarSono(false);
        } else if (o == "Forjar equipamento") {
            painel_.clear();
            forja_ = true;
            selForja_ = 0;
        } else if (o.starts_with("Alas do castelo")) {
            painel_.clear();
            alas_ = true;
            selAla_ = 0;
        } else if (o == "Brasão da linhagem") {
            painel_.clear();
            brasaoEd_ = true;
            selBrasao_ = 0;
            rascunho_ = mundo_.brasao;
        } else if (o == "Cozinhar") {
            painel_.clear();
            cozinha_ = true;
            selCozinha_ = 0;
        } else if (o == "Abrir a guilda") {
            painel_.clear();
            guilda_ = true;
            abaGuilda_ = selGuilda_ = passoExpedicao_ = 0;
        } else if (o.starts_with("Reparar portão")) {
            mundo_.repararPortao();  // o painel fica aberto (mostra a vida nova)
        } else if (o.starts_with("Comprar 10 pedra")) {
            mundo_.comprarLote("pedra");  // o painel fica aberto para comprar de novo
        } else if (o.starts_with("Comprar 10 madeira")) {
            mundo_.comprarLote("madeira");
        } else {
            painel_.clear();
        }
    }
    mundo_.passo(static_cast<float>(1.0 / 60.0), {});  // o mundo segue vivo (tochas, era) com o herói parado
}

void WorldScene::entradaPausa() {
    if (telaOpcoes_.aberta()) {
        telaOpcoes_.entrada(ctx_);
        return;
    }
    ke::Input& in = ctx_.app.input();
    const int n = static_cast<int>(OPCOES_PAUSA.size());
    if (in.pressionou(SDL_SCANCODE_ESCAPE)) pausa_ = false;
    if (in.pressionou(SDL_SCANCODE_W) || in.pressionou(SDL_SCANCODE_UP)) selPausa_ = (selPausa_ + n - 1) % n;
    if (in.pressionou(SDL_SCANCODE_S) || in.pressionou(SDL_SCANCODE_DOWN)) selPausa_ = (selPausa_ + 1) % n;
    const bool clique = listaComMouse(in, hud_.cliques, selPausa_, n);
    if (in.pressionou(SDL_SCANCODE_RETURN) || in.pressionou(SDL_SCANCODE_E) || in.pressionou(SDL_SCANCODE_SPACE) || clique) {
        const std::string& o = OPCOES_PAUSA[selPausa_];
        if (o == "Continuar") pausa_ = false;
        else if (o == "Salvar") salvar();
        else if (o == "Opções") telaOpcoes_.abrirOpcoes();
        else if (o == "Controles") telaOpcoes_.abrirControles();
        else if (o == "Voltar ao título") {
            salvar();
            ctx_.app.cenas().trocar(std::make_unique<TitleScene>(ctx_));
        } else {
            salvar();
            ctx_.app.parar();
        }
    }
}

void WorldScene::entradaDialogo(float dt) {
    ke::Input& in = ctx_.app.input();
    const int tamanho = Hud::tamanhoUtf8(conversa_->falas[linhaFala_]);
    letras_ = std::min(static_cast<float>(tamanho), letras_ + dt * LETRAS_POR_SEGUNDO);
    // Fase 15: F (ou clique na dica) dá o presente sem encerrar a conversa
    if (!conversa_->presente.empty() &&
        (in.pressionou(SDL_SCANCODE_F) || (in.clicou(ke::BotaoMouse::Esquerdo) && hud_.cliques.em(in.mouse()) == 600))) {
        darPresenteNaConversa();
        mundo_.passo(dt, {});
        return;
    }
    if (in.pressionou(SDL_SCANCODE_E) || in.pressionou(SDL_SCANCODE_SPACE) || in.pressionou(SDL_SCANCODE_RETURN) ||
        in.pressionou(SDL_SCANCODE_ESCAPE) || in.clicou(ke::BotaoMouse::Esquerdo)) {
        if (letras_ < tamanho && !in.pressionou(SDL_SCANCODE_ESCAPE)) {
            letras_ = static_cast<float>(tamanho);  // primeiro toque: mostra a fala inteira
        } else if (linhaFala_ + 1 < static_cast<int>(conversa_->falas.size()) && !in.pressionou(SDL_SCANCODE_ESCAPE)) {
            ++linhaFala_;
            letras_ = 0.0f;
        } else {
            mundo_.encerrarConversa(conversa_->npc);
            if (conversa_->loja && !in.pressionou(SDL_SCANCODE_ESCAPE)) {  // o mercador abre a banca
                loja_ = conversa_->npc;
                selLoja_ = 0;
            }
            conversa_.reset();
        }
    }
    mundo_.passo(dt, {});
}

void WorldScene::darPresenteNaConversa() {
    if (!mundo_.darPresente(conversa_->npc)) return;
    conversa_->coracoes = mundo_.coracoesDe(conversa_->npc);
    conversa_->presente.clear();  // um presente por dia
}

void WorldScene::entradaCozinha() {
    ke::Input& in = ctx_.app.input();
    const auto& receitas = ctx_.dados.cotidiano.receitas;
    const int n = static_cast<int>(receitas.size());
    if (in.pressionou(SDL_SCANCODE_ESCAPE) || in.pressionou(SDL_SCANCODE_Q) || cliqueFecha(in, hud_.cliques)) {
        cozinha_ = false;
        return;
    }
    // Botão "comer" (id 500) antes da lista, para o clique nele não cozinhar
    if (in.pressionou(SDL_SCANCODE_F) || (in.clicou(ke::BotaoMouse::Esquerdo) && hud_.cliques.em(in.mouse()) == 500)) {
        mundo_.comerPrato(receitas[selCozinha_].id);
        return;
    }
    if (in.pressionou(SDL_SCANCODE_W) || in.pressionou(SDL_SCANCODE_UP)) selCozinha_ = (selCozinha_ + n - 1) % n;
    if (in.pressionou(SDL_SCANCODE_S) || in.pressionou(SDL_SCANCODE_DOWN)) selCozinha_ = (selCozinha_ + 1) % n;
    const bool clique = listaComMouse(in, hud_.cliques, selCozinha_, n);
    if (in.pressionou(SDL_SCANCODE_E) || in.pressionou(SDL_SCANCODE_RETURN) || in.pressionou(SDL_SCANCODE_SPACE) || clique)
        mundo_.cozinhar(receitas[selCozinha_].id);
}

void WorldScene::entradaAlas() {
    ke::Input& in = ctx_.app.input();
    const auto& alas = ctx_.dados.castelo.alas;
    const int n = static_cast<int>(alas.size());
    if (in.pressionou(SDL_SCANCODE_ESCAPE) || in.pressionou(SDL_SCANCODE_Q) || cliqueFecha(in, hud_.cliques)) {
        alas_ = false;
        return;
    }
    if (in.pressionou(SDL_SCANCODE_W) || in.pressionou(SDL_SCANCODE_UP)) selAla_ = (selAla_ + n - 1) % n;
    if (in.pressionou(SDL_SCANCODE_S) || in.pressionou(SDL_SCANCODE_DOWN)) selAla_ = (selAla_ + 1) % n;
    const bool clique = listaComMouse(in, hud_.cliques, selAla_, n);
    if (in.pressionou(SDL_SCANCODE_E) || in.pressionou(SDL_SCANCODE_RETURN) || in.pressionou(SDL_SCANCODE_SPACE) || clique)
        mundo_.erguerAla(alas[selAla_].id);
}

void WorldScene::entradaBrasao() {
    ke::Input& in = ctx_.app.input();
    if (in.pressionou(SDL_SCANCODE_ESCAPE) || in.pressionou(SDL_SCANCODE_Q) || cliqueFecha(in, hud_.cliques)) {
        brasaoEd_ = false;  // cancelar: o rascunho se perde
        return;
    }
    const DadosCastelo& dc = ctx_.dados.castelo;
    if (in.pressionou(SDL_SCANCODE_W) || in.pressionou(SDL_SCANCODE_UP)) selBrasao_ = (selBrasao_ + LINHAS_BRASAO - 1) % LINHAS_BRASAO;
    if (in.pressionou(SDL_SCANCODE_S) || in.pressionou(SDL_SCANCODE_DOWN)) selBrasao_ = (selBrasao_ + 1) % LINHAS_BRASAO;
    if (in.pressionou(SDL_SCANCODE_A) || in.pressionou(SDL_SCANCODE_LEFT)) girarPeca(dc, rascunho_, selBrasao_, -1);
    if (in.pressionou(SDL_SCANCODE_D) || in.pressionou(SDL_SCANCODE_RIGHT)) girarPeca(dc, rascunho_, selBrasao_, 1);
    // Mouse: setas (700+linha volta, 720+linha avança), linha escolhe, "Salvar" (800) grava
    const int id = hud_.cliques.em(in.mouse());
    bool salvar = in.pressionou(SDL_SCANCODE_E) || in.pressionou(SDL_SCANCODE_RETURN) || in.pressionou(SDL_SCANCODE_SPACE);
    if (in.clicou(ke::BotaoMouse::Esquerdo)) {
        if (id >= 700 && id < 700 + LINHAS_BRASAO) selBrasao_ = id - 700, girarPeca(dc, rascunho_, selBrasao_, -1);
        else if (id >= 720 && id < 720 + LINHAS_BRASAO) selBrasao_ = id - 720, girarPeca(dc, rascunho_, selBrasao_, 1);
        else if (id >= 0 && id < LINHAS_BRASAO) selBrasao_ = id;
        else if (id == 800) salvar = true;
    } else if (in.mouseMoveu() && id >= 0 && id < LINHAS_BRASAO) {
        selBrasao_ = id;
    }
    if (salvar) {
        mundo_.definirBrasao(rascunho_);
        brasaoEd_ = false;
    }
}

void WorldScene::iniciarSono(bool desmaio) {
    transicao_ = 0.0f;
    desmaio_ = desmaio;
    derrota_ = false;
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
    ke::AudioDevice& som = ctx_.app.audio();
    for (const EventoJogo& e : mundo_.tirarEventos()) {
        // Som de cada acontecimento
        switch (e.tipo) {
        case EventoJogo::Tipo::Golpe:
        case EventoJogo::Tipo::Ataque: som.tocar(ke::Som::Golpe); break;
        case EventoJogo::Tipo::AcertouInimigo: som.tocar(ke::Som::Acerto); break;
        case EventoJogo::Tipo::Ganhou:
        case EventoJogo::Tipo::InimigoDerrotado: som.tocar(e.recurso == "ouro" ? ke::Som::Moeda : ke::Som::Coleta); break;
        case EventoJogo::Tipo::Construiu:
        case EventoJogo::Tipo::ObraIniciada:
        case EventoJogo::Tipo::Forjou: som.tocar(ke::Som::Construir); break;
        case EventoJogo::Tipo::HeroiFerido: som.tocar(ke::Som::Dano); break;
        case EventoJogo::Tipo::EraMudou:
        case EventoJogo::Tipo::Titulo:
        case EventoJogo::Tipo::EventoRaro:
        case EventoJogo::Tipo::Festival:
        case EventoJogo::Tipo::Transformou:
        case EventoJogo::Tipo::MissaoConcluida: som.tocar(ke::Som::Magia); break;
        case EventoJogo::Tipo::AbrirConversa:
        case EventoJogo::Tipo::AbrirConstrucao:
        case EventoJogo::Tipo::LerPista:
        case EventoJogo::Tipo::AbrirEscolha:
        case EventoJogo::Tipo::MissaoNova: som.tocar(ke::Som::Menu); break;
        default: break;
        }
        switch (e.tipo) {
        case EventoJogo::Tipo::Golpe: {
            const bool arvore = e.alvo == "arvore";
            poeira(e.pos, {4, 3}, 7, arvore ? glm::vec4(0.42f, 0.33f, 0.24f, 1) : glm::vec4(0.55f, 0.53f, 0.5f, 1));
            cam_.tremor = std::max(cam_.tremor, 1.2f);
            break;
        }
        case EventoJogo::Tipo::Esgotou:
            if (e.alvo == "purificou" || e.alvo == "purificar") poeira(e.pos - glm::vec2(0, 6), {7, 7}, 24, {0.75f, 1.0f, 0.8f, 1});  // sopro de luz
            else if (e.alvo == "pacto") poeira(e.pos, {12, 14}, 60, {0.9f, 0.35f, 0.15f, 1});
            else if (e.alvo == "derrotar") poeira(e.pos, {12, 14}, 60, {0.45f, 0.42f, 0.4f, 1});
            else if (e.alvo == "entulho") poeira(e.pos, {9, 7}, 40, {0.5f, 0.45f, 0.38f, 1}), cam_.tremor = 2.0f;
            else poeira(e.pos, {7, 5}, 16, {0.38f, 0.34f, 0.3f, 1});
            break;
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
        case EventoJogo::Tipo::Aviso: hud_.aviso(e.texto, e.alvo == "bom" ? cor::bom : cor::ruim); break;
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
            ke::log::info("novo dia: {}{} | povo {} | felicidade {:.0f}", e.texto, e.alvo == "desmaio" ? " (desmaio)" : "",
                          mundo_.estado.populacao, mundo_.estado.felicidade);
            relatorio_ = true;  // relatório da manhã
            salvar();           // salvamento automático a cada manhã
            break;
        case EventoJogo::Tipo::Ataque: {
            // Rastro do golpe: um leque de faíscas claras na frente do herói
            std::uniform_real_distribution<float> u(-1.0f, 1.0f);
            for (int i = 0; i < 10; ++i) {
                ke::Particula p;
                p.pos = e.pos + glm::vec2(u(rng_) * 8.0f, u(rng_) * 6.0f);
                p.vel = vetor(mundo_.jogador.dir) * 40.0f + glm::vec2(u(rng_), u(rng_)) * 20.0f;
                p.cor = {0.9f, 0.92f, 1.0f, 0.8f};
                p.vida = p.vidaMax = 0.16f;
                p.aditiva = true;
                part_.emitir(p);
            }
            break;
        }
        case EventoJogo::Tipo::AcertouInimigo:
            textoFlutuante(e.pos - glm::vec2(0, 8), std::to_string(e.quantidade), {1.0f, 0.95f, 0.7f, 1});
            poeira(e.pos, {4, 4}, 8, {0.25f, 0.2f, 0.3f, 1});
            cam_.tremor = std::max(cam_.tremor, 1.6f);
            break;
        case EventoJogo::Tipo::InimigoDerrotado:
            poeira(e.pos - glm::vec2(0, 6), {7, 7}, 26, {0.2f, 0.16f, 0.26f, 1});
            textoFlutuante(e.pos - glm::vec2(0, 20), std::format("+{} Ouro", e.quantidade), cor::ouro);
            hud_.pulsar("ouro");
            if (ctx_.dados.inimigos.at(e.alvo).chefe) {
                hud_.aviso(e.texto + " foi derrotado!", cor::ouro);
                flash_ = 1.0f;
                cam_.tremor = 4.0f;
            }
            break;
        case EventoJogo::Tipo::HeroiFerido:
            feridoFlash_ = 0.5f;
            cam_.tremor = std::max(cam_.tremor, 2.5f);
            textoFlutuante(e.pos - glm::vec2(0, 26), std::format("-{}", e.quantidade), cor::ruim);
            break;
        case EventoJogo::Tipo::HeroiDerrotado:
            hud_.aviso("Suas forças acabaram...", cor::ruim);
            transicao_ = 0.0f;
            desmaio_ = true;
            derrota_ = true;
            break;
        case EventoJogo::Tipo::MudouRegiao:
            nomeRegiao_ = e.texto;
            tituloRegiao_ = 2.5f;
            flash_ = 0.0f;
            cam_.limite = mundo_.tamanhoPixels();
            cam_.posicionar(mundo_.jogador.pos - glm::vec2(0, 10));
            part_.limpar();
            ceu_.limpar();
            ke::log::info("entrou em {}", e.alvo);
            break;
        case EventoJogo::Tipo::MissaoNova: hud_.aviso("Nova missão: " + e.texto + " (L para ver)", cor::ouro); break;
        case EventoJogo::Tipo::MissaoConcluida:
            hud_.aviso("Missão cumprida: " + e.texto + "!", cor::bom);
            if (!e.recurso.empty()) hud_.aviso("Recebeu: " + ctx_.dados.equipamentos.at(e.recurso).nome, cor::ouro);
            flash_ = 0.6f;
            ke::log::info("missão concluída: {}", e.alvo);
            break;
        case EventoJogo::Tipo::Comeu:
            if (e.quantidade > 0) textoFlutuante(e.pos - glm::vec2(0, 26), std::format("+{} vida", e.quantidade), cor::bom);
            som.tocar(ke::Som::Coleta);
            break;
        // ---------- Fase 16: o castelo e o brasão ----------
        case EventoJogo::Tipo::AlaErguida:
            hud_.aviso("Ala erguida: " + e.texto + "!", cor::ouro);
            if (const BuildingData* b = mundo_.reino.construcao("castelo")) {
                const ke::AABB c = mundo_.caixaConstrucao(*b);
                poeira((c.min + c.max) * 0.5f, (c.max - c.min) * 0.5f, 80, {0.6f, 0.56f, 0.5f, 1});
            }
            flash_ = std::max(flash_, 0.5f);
            cam_.tremor = std::max(cam_.tremor, 2.0f);
            som.tocar(ke::Som::Construir);
            ke::log::info("ala do castelo: {}", e.alvo);
            salvar();
            break;
        case EventoJogo::Tipo::BrasaoNovo:
            hud_.aviso("Novo brasão: " + e.texto + ".", cor::ouro);
            flash_ = std::max(flash_, 0.4f);
            som.tocar(ke::Som::Magia);
            ke::log::info("brasão: {}", e.texto);
            salvar();
            break;
        // ---------- Fase 15: vida cotidiana ----------
        case EventoJogo::Tipo::AbrirCozinha:
            cozinha_ = true;
            selCozinha_ = 0;
            som.tocar(ke::Som::Menu);
            break;
        case EventoJogo::Tipo::Mordida:
            if (e.alvo == "mordeu") {
                textoFlutuante(e.pos - glm::vec2(0, 12), "!", cor::ouro);
                hud_.aviso(e.texto, cor::ouro);
                som.tocar(ke::Som::Acerto);
                poeira(e.pos, {3, 1}, 8, {0.7f, 0.85f, 1.0f, 1});  // respingo
            } else {
                hud_.aviso(e.texto, cor::texto);
                poeira(e.pos, {2, 1}, 5, {0.7f, 0.85f, 1.0f, 1});
            }
            break;
        case EventoJogo::Tipo::Presente:
            hud_.aviso(e.texto, e.quantidade > 0 ? cor::ouro : cor::bom);
            textoFlutuante(mundo_.jogador.pos - glm::vec2(0, 30), e.quantidade > 0 ? "++ amizade" : "+ amizade", cor::bom);
            som.tocar(ke::Som::Magia);
            ke::log::info("presente: {} para {}{}", e.recurso, e.alvo, e.quantidade > 0 ? " (favorito)" : "");
            break;
        case EventoJogo::Tipo::Carinho:
            if (e.recurso == "regou") poeira(e.pos - glm::vec2(0, 4), {6, 2}, 14, {0.45f, 0.65f, 0.95f, 1});
            else if (e.recurso == "plantou") poeira(e.pos - glm::vec2(0, 2), {5, 2}, 12, {0.42f, 0.3f, 0.2f, 1});
            else textoFlutuante(e.pos, "+ carinho", cor::bom), poeira(e.pos, {4, 3}, 10, {1.0f, 0.55f, 0.65f, 1});
            som.tocar(e.recurso == "carinho" ? ke::Som::Menu : ke::Som::Coleta);
            break;
        case EventoJogo::Tipo::EraMudou:
            nomeEra_ = e.texto;
            tituloEra_ = 5.0f;
            flash_ = 1.0f;
            cam_.tremor = 2.0f;
            ke::log::info("o reino entrou em uma nova era: {}", e.texto);
            break;
        case EventoJogo::Tipo::Forjou:
            hud_.aviso("Forjado: " + e.texto + "!", cor::ouro);
            cam_.tremor = 1.5f;
            break;
        case EventoJogo::Tipo::ObraIniciada: {
            if (const BuildingData* b = mundo_.reino.construcao(e.alvo)) {
                const ke::AABB c = mundo_.caixaConstrucao(*b);
                poeira((c.min + c.max) * 0.5f, (c.max - c.min) * 0.5f, 60, {0.5f, 0.42f, 0.32f, 1});
            }
            cam_.tremor = 2.0f;
            hud_.aviso("Obra iniciada: " + e.texto + ". Fica pronta amanhã.", cor::ouro);
            ke::log::info("obra iniciada: {} ({})", e.alvo, e.texto);
            break;
        }
        case EventoJogo::Tipo::Cronica:
            hud_.aviso("Nova página nas Crônicas (C para ler)", cor::ouro);
            ke::log::info("crônica [{}]: {}", e.alvo, e.texto);
            break;
        case EventoJogo::Tipo::Titulo:
            nomeTitulo_ = e.texto;
            tituloNovo_ = 4.5f;
            flash_ = std::max(flash_, 0.6f);
            ke::log::info("novo título: {}", e.texto);
            break;
        case EventoJogo::Tipo::LerPista:
            leitura_ = e.alvo;
            ke::log::info("fragmento lido: {}", e.alvo);
            break;
        case EventoJogo::Tipo::EventoRaro:
            hud_.aviso(e.texto, e.alvo == "lua_sangue" ? cor::ruim : cor::ouro);
            flash_ = std::max(flash_, 0.3f);
            ke::log::info("acontecimento: {}", e.alvo);
            break;
        case EventoJogo::Tipo::Festival:
            hud_.aviso(e.texto, cor::ouro);
            flash_ = std::max(flash_, 0.4f);
            ke::log::info("festival começou (dia {})", mundo_.relogio.dia);
            break;
        case EventoJogo::Tipo::AbrirEscolha:
            if (ctx_.dados.escolhas.contains(e.alvo)) {
                escolha_ = e.alvo;
                selEscolha_ = 0;
                cam_.tremor = std::max(cam_.tremor, 2.5f);
                ke::log::info("escolha aberta: {}", e.alvo);
            }
            break;
        case EventoJogo::Tipo::Decidiu:
            hud_.aviso("Decidido: " + e.texto, cor::ouro);
            flash_ = std::max(flash_, 0.3f);
            som.tocar(ke::Som::Construir);
            ke::log::info("decisão do conselho: {} -> {}", e.alvo, e.recurso);
            salvar();
            break;
        case EventoJogo::Tipo::Nomeou:
            hud_.aviso(e.texto, cor::ouro);
            som.tocar(ke::Som::Magia);
            ke::log::info("conselho: {} no cargo {}", e.recurso, e.alvo);
            break;
        // ---------- Fase 14: a guilda ----------
        case EventoJogo::Tipo::Partiu:
            hud_.aviso(e.texto, cor::ouro);
            som.tocar(ke::Som::Menu);
            ke::log::info("expedição: {} ({} dias)", e.alvo, e.quantidade);
            salvar();
            break;
        case EventoJogo::Tipo::Reliquia:
            hud_.aviso(e.recurso == "nova" ? "Nova relíquia: " + e.texto + "!" : e.texto,
                       e.recurso == "maldicao" ? cor::ruim : e.recurso == "purificada" ? cor::bom : cor::ouro);
            som.tocar(e.recurso == "maldicao" ? ke::Som::Dano : ke::Som::Magia);
            if (e.recurso == "maldicao") cam_.tremor = std::max(cam_.tremor, 2.0f), feridoFlash_ = 0.4f;
            ke::log::info("relíquia {}: {}", e.alvo, e.recurso);
            salvar();
            break;
        // ---------- Fase 13: a batalha no portão ----------
        case EventoJogo::Tipo::HordaChegou:
            nomeRegiao_ = "A horda chegou!";
            tituloRegiao_ = 3.0f;
            hud_.aviso(e.texto, cor::ruim);
            cam_.tremor = std::max(cam_.tremor, 3.0f);
            som.tocar(ke::Som::Dano);
            ke::log::info("horda: {} criaturas no portão (dia {})", e.quantidade, mundo_.relogio.dia);
            break;
        case EventoJogo::Tipo::Flecha: {
            // Rastro da flecha: pontinhos claros da torre até a criatura
            const glm::vec2 d = e.ate - e.pos;
            for (int i = 0; i < 10; ++i) {
                ke::Particula p;
                p.pos = e.pos + d * (i / 10.0f);
                p.vel = glm::normalize(d + glm::vec2(0.001f)) * 30.0f;
                p.cor = {1.0f, 0.9f, 0.6f, 0.9f};
                p.vida = p.vidaMax = 0.08f + i * 0.02f;
                p.aditiva = true;
                part_.emitir(p);
            }
            break;
        }
        case EventoJogo::Tipo::PortaoGolpeado:
            poeira(e.pos, {3, 6}, 6, {0.45f, 0.36f, 0.26f, 1});
            if (std::abs(e.pos.x - mundo_.jogador.pos.x) < 200.0f) {
                cam_.tremor = std::max(cam_.tremor, 0.8f);
                som.tocar(ke::Som::Golpe);
            }
            break;
        case EventoJogo::Tipo::PortaoCaiu:
            poeira(e.pos, {8, 16}, 60, {0.4f, 0.32f, 0.24f, 1});
            cam_.tremor = 4.0f;
            flash_ = std::max(flash_, 0.4f);
            hud_.aviso(e.texto, cor::ruim);
            som.tocar(ke::Som::Dano);
            break;
        case EventoJogo::Tipo::Saque:
            textoFlutuante(e.pos - glm::vec2(0, 20), "Saque!", cor::ruim);
            hud_.aviso(e.texto, cor::ruim);
            hud_.pulsar("comida");
            hud_.pulsar("ouro");
            break;
        case EventoJogo::Tipo::SoldadoCaiu:
            hud_.aviso(e.texto, cor::ruim);
            poeira(e.pos, {5, 6}, 14, {0.3f, 0.35f, 0.5f, 1});
            break;
        case EventoJogo::Tipo::BatalhaFim:
            hud_.aviso(e.alvo == "vitoria" ? "A horda foi vencida!" : "A horda foi vencida, mas a praça foi saqueada.",
                       e.alvo == "vitoria" ? cor::ouro : cor::ruim);
            if (e.quantidade > 0) textoFlutuante(mundo_.jogador.pos - glm::vec2(0, 30), std::format("+{} Ouro", e.quantidade), cor::ouro);
            flash_ = std::max(flash_, 0.8f);
            som.tocar(ke::Som::Magia);
            ke::log::info("fim da batalha: {}", e.texto);
            salvar();
            break;
        case EventoJogo::Tipo::Transformou:
            nomeRegiao_ = e.texto;
            tituloRegiao_ = 3.5f;
            flash_ = 1.0f;
            cam_.tremor = std::max(cam_.tremor, 2.5f);
            hud_.aviso(e.texto + "!", cor::ouro);
            ke::log::info("região transformada: {} -> {}", e.alvo, e.texto);
            salvar();  // um acontecimento desses não pode se perder
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
    // Fase 10: folhas de outono caindo (no vilarejo e na floresta)
    if (mundo_.relogio.estacao() == Estacao::Outono && mundo_.regiaoAtual != "cripta") {
        tempoFolhas_ += dt * 9.0f;
        while (tempoFolhas_ >= 1.0f) {
            tempoFolhas_ -= 1.0f;
            ke::Particula p;
            p.pos = cam_.canto() + glm::vec2(u(rng_) * 700.0f - 30.0f, -4.0f);
            p.vel = {12.0f + u(rng_) * 14.0f, 14.0f + u(rng_) * 10.0f};
            static const glm::vec4 folhas[] = {{0.85f, 0.45f, 0.15f, 0.9f}, {0.75f, 0.3f, 0.12f, 0.9f}, {0.9f, 0.7f, 0.25f, 0.9f}};
            p.cor = folhas[static_cast<int>(u(rng_) * 2.99f)];
            p.vida = p.vidaMax = 12.0f;
            p.tamanho = 2.0f;
            part_.emitir(p);
        }
    }
    const float escuro = mundo_.escuridao();
    // Chuva de meteoros: riscos de luz atravessando o céu
    if (mundo_.eventoAgora("meteoros") && escuro > 0.4f && mundo_.regiaoAtual != "cripta") {
        tempoMeteoro_ -= dt;
        if (tempoMeteoro_ <= 0.0f) {
            tempoMeteoro_ = 0.12f + u(rng_) * 0.45f;
            const glm::vec2 inicio = cam_.canto() + glm::vec2(u(rng_) * 640.0f - 60.0f, u(rng_) * 140.0f);
            const glm::vec2 dir = glm::normalize(glm::vec2(1.0f, 0.55f + u(rng_) * 0.3f));
            for (int i = 0; i < 14; ++i) {
                ke::Particula p;
                p.pos = inicio - dir * (i * 3.0f);
                p.vel = dir * 240.0f;
                p.cor = {0.75f + 0.25f * (i == 0), 0.85f, 1.0f, 1.0f - i * 0.065f};
                p.tamanho = i < 6 ? 2.0f : 1.0f;
                p.vida = p.vidaMax = 0.9f;
                p.aditiva = true;
                ceu_.emitir(p);
            }
        }
    }
    // Festival na era renascida em diante: fogos sobre a praça
    if (mundo_.festivalAgora() && mundo_.noVilarejo() && mundo_.reino.eraVisual >= 1.5f && escuro > 0.3f) {
        tempoFogos_ -= dt;
        if (tempoFogos_ <= 0.0f) {
            tempoFogos_ = 0.5f + u(rng_) * 0.9f;
            // No alto da tela, sobre a praça (sem sair do que se vê)
            const glm::vec2 centro{33.0f * 16 + (u(rng_) - 0.5f) * 260.0f, cam_.canto().y + 55.0f + u(rng_) * 80.0f};
            static const glm::vec4 cores[] = {{1.0f, 0.4f, 0.4f, 1}, {0.5f, 0.8f, 1.0f, 1}, {1.0f, 0.9f, 0.4f, 1}, {0.7f, 1.0f, 0.6f, 1}, {0.9f, 0.6f, 1.0f, 1}};
            const glm::vec4 c = cores[static_cast<int>(u(rng_) * 4.99f)];
            for (int i = 0; i < 40; ++i) {
                const float ang = i / 40.0f * 6.2832f;
                ke::Particula p;
                p.pos = centro;
                p.vel = glm::vec2(std::cos(ang), std::sin(ang)) * (55.0f + u(rng_) * 35.0f);
                p.cor = c;
                p.vida = p.vidaMax = 1.1f + u(rng_) * 0.4f;
                p.gravidade = 30.0f;
                p.atrito = 1.2f;
                p.tamanho = 2.0f;
                p.aditiva = true;
                ceu_.emitir(p);
            }
            cam_.tremor = std::max(cam_.tremor, 0.6f);
        }
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
    // Vida de cada era (no vilarejo; fora dele só na terra purificada)
    const float era = mundo_.noVilarejo() ? mundo_.reino.eraVisual : static_cast<float>(mundo_.eraEm(mundo_.jogador.pos));
    const bool noite = mundo_.relogio.escuridao() > 0.5f;
    tempoAtmosfera_ += dt;
    if (era >= 0.6f && tempoAtmosfera_ > 0.12f) {
        tempoAtmosfera_ = 0.0f;
        const glm::vec2 c = cam_.canto();
        ke::Particula p;
        p.pos = c + glm::vec2(u(rng_) * 640.0f, u(rng_) * 360.0f);
        if (noite) {  // vagalumes
            p.vel = {u(rng_) * 16.0f - 8.0f, u(rng_) * 10.0f - 6.0f};
            p.cor = {0.8f, 1.0f, 0.4f, 0.9f};
            p.vida = p.vidaMax = 2.0f + u(rng_) * 2.0f;
            p.aditiva = true;
            part_.emitir(p);
        } else if (era >= 1.6f) {  // pétalas e borboletas
            const bool petala = u(rng_) < 0.6f;
            p.pos.y = c.y - 4.0f;
            p.vel = petala ? glm::vec2(10.0f + u(rng_) * 10.0f, 14.0f + u(rng_) * 8.0f) : glm::vec2(u(rng_) * 30.0f - 15.0f, u(rng_) * 10.0f);
            static const glm::vec4 cores[] = {{0.95f, 0.7f, 0.8f, 0.9f}, {1.0f, 0.95f, 0.6f, 1}, {0.6f, 0.8f, 1.0f, 1}};
            p.cor = cores[petala ? 0 : 1 + static_cast<int>(u(rng_) * 2)];
            if (!petala) p.pos.y = c.y + u(rng_) * 360.0f;
            p.vida = p.vidaMax = 10.0f;
            p.tamanho = petala ? 1.0f : 2.0f;
            part_.emitir(p);
        }
        if (era >= 2.6f) {  // poeira mágica subindo
            ke::Particula m;
            m.pos = c + glm::vec2(u(rng_) * 640.0f, 360.0f * (0.3f + 0.7f * u(rng_)));
            m.vel = {u(rng_) * 6.0f - 3.0f, -10.0f - u(rng_) * 10.0f};
            m.cor = u(rng_) < 0.5f ? glm::vec4(0.6f, 0.85f, 1.0f, 0.8f) : glm::vec4(1.0f, 0.85f, 0.45f, 0.8f);
            m.vida = m.vidaMax = 3.0f;
            m.aditiva = true;
            part_.emitir(m);
        }
    }
    // O dragão da Alta Fantasia cruza o céu de vez em quando
    if (era >= 2.9f) {
        if (!dragaoAtivo_) {
            proximoDragao_ -= dt;
            if (proximoDragao_ <= 0.0f) {
                dragaoAtivo_ = true;
                dragaoX_ = cam_.canto().x - 100.0f;
                dragaoY_ = cam_.canto().y + 80.0f + u(rng_) * 120.0f;
            }
        } else {
            dragaoX_ += 95.0f * dt;
            dragaoY_ -= 8.0f * dt;
            if (dragaoX_ > cam_.canto().x + 760.0f) dragaoAtivo_ = false, proximoDragao_ = 35.0f + u(rng_) * 30.0f;
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
    mundo_.relogio.parado = pausa_ || !painel_.empty() || conversa_.has_value() || transicao_ >= 0.0f || menuObras_ || painelReino_ ||
                            relatorio_ || diario_ || forja_ || cronicas_ || !leitura_.empty() || !loja_.empty() || painelRegioes_ ||
                            !escolha_.empty() || painelConselho_ || decisao_ || guilda_ || cozinha_ || alas_ || brasaoEd_;
    feridoFlash_ = std::max(0.0f, feridoFlash_ - dt);
    tituloRegiao_ = std::max(0.0f, tituloRegiao_ - dt);
    tituloEra_ = std::max(0.0f, tituloEra_ - dt);
    if (tituloEra_ <= 0.0f) tituloNovo_ = std::max(0.0f, tituloNovo_ - dt);  // o título espera a era terminar de ser anunciada
    // Música: a era do reino no vilarejo; trilha de perigo na floresta e na cripta
    ctx_.app.audio().definir(mundo_.noVilarejo() ? mundo_.reino.eraVisual : static_cast<float>(mundo_.eraEm(mundo_.jogador.pos)),
                             mundo_.relogio.escuridao(), mundo_.regiaoHostil() || mundo_.emBatalha() ? 1.0f : 0.0f);

    if (transicao_ >= 0.0f) {
        const float antes = transicao_;
        transicao_ += dt * 1.25f;
        if (antes < 1.0f && transicao_ >= 1.0f) {  // tela preta: troca o dia
            if (derrota_) mundo_.reviver();
            else mundo_.dormir(desmaio_);
        }
        if (transicao_ >= 2.0f) transicao_ = -1.0f;
        mundo_.passo(dt, {});
    } else if (pausa_) entradaPausa();
    else if (!leitura_.empty()) {
        // Lendo um fragmento: E ou Esc fecha (e volta para as Crônicas, se veio de lá)
        ke::Input& in = ctx_.app.input();
        if (in.pressionou(SDL_SCANCODE_E) || in.pressionou(SDL_SCANCODE_RETURN) || in.pressionou(SDL_SCANCODE_ESCAPE) ||
            in.pressionou(SDL_SCANCODE_SPACE) || in.clicou(ke::BotaoMouse::Esquerdo))
            leitura_.clear();
        mundo_.passo(dt, {});
    } else if (relatorio_) {
        ke::Input& in = ctx_.app.input();
        if (in.pressionou(SDL_SCANCODE_E) || in.pressionou(SDL_SCANCODE_RETURN) || in.pressionou(SDL_SCANCODE_ESCAPE) ||
            in.pressionou(SDL_SCANCODE_SPACE) || in.clicou(ke::BotaoMouse::Esquerdo)) {
            relatorio_ = false;
            // Caso que o conselho trouxe esta manhã: abre logo depois do relatório
            if (mundo_.decisaoAtual() && mundo_.ultimaDecisao == mundo_.relogio.dia) decisao_ = true, selDecisao_ = 0;
        }
        mundo_.passo(dt, {});
    } else if (conversa_) entradaDialogo(dt);
    else if (!escolha_.empty()) entradaEscolha();
    else if (decisao_) entradaDecisao();
    else if (painelConselho_) entradaConselho();
    else if (guilda_) entradaGuilda();
    else if (cozinha_) entradaCozinha();
    else if (alas_) entradaAlas();
    else if (brasaoEd_) entradaBrasao();
    else if (painelRegioes_) {
        ke::Input& in = ctx_.app.input();
        if (in.pressionou(SDL_SCANCODE_R) || in.pressionou(SDL_SCANCODE_ESCAPE) || in.pressionou(SDL_SCANCODE_E) || cliqueFecha(in, hud_.cliques))
            painelRegioes_ = false;
    } else if (!painel_.empty()) entradaPainel();
    else if (forja_) entradaForja();
    else if (!loja_.empty()) entradaLoja();
    else if (cronicas_) entradaCronicas();
    else if (diario_) {
        ke::Input& in = ctx_.app.input();
        if (in.pressionou(SDL_SCANCODE_L) || in.pressionou(SDL_SCANCODE_ESCAPE) || in.pressionou(SDL_SCANCODE_E) || cliqueFecha(in, hud_.cliques))
            diario_ = false;
    } else if (menuObras_) entradaMenuObras();
    else if (painelReino_) entradaPainelReino();
    else if (!colocando_.empty()) entradaColocacao();
    else entradaJogo();

    tratarEventos();
    efeitosAmbiente(dt);
    part_.atualizar(dt);
    ceu_.atualizar(dt);
    hud_.atualizar(dt);
    for (auto& f : flutuantes_) {
        f.vida -= dt;
        f.pos.y -= dt * 14.0f;
    }
    std::erase_if(flutuantes_, [](const Flutuante& f) { return f.vida <= 0.0f; });
    flash_ = std::max(0.0f, flash_ - dt * 1.5f);
    // Sempre o tamanho do mapa atual (acordar em casa depois de cair na cripta também troca de mapa)
    if (cam_.limite != mundo_.tamanhoPixels()) {
        cam_.limite = mundo_.tamanhoPixels();
        cam_.posicionar(mundo_.jogador.pos - glm::vec2(0, 10));
    }
    cam_.seguir(mundo_.jogador.pos - glm::vec2(0, 10), dt, 8.0f);
    cam_.atualizarTremor(dt);
}

void WorldScene::renderizar(float) {
    ke::Renderer2D& r = ctx_.app.renderer();
    const ke::BitmapFont& f = ctx_.app.fonte();

    // 1) Mapa de luz (só quando escurece)
    const bool noite = mundo_.escuridao() > 0.0f || mundo_.relogio.minutos < 7 * 60;
    if (noite) {
        luz_.comecar(mundo_.luzAmbiente());
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
    // Fogos e estrelas cadentes: por cima da escuridão
    r.definirMistura(ke::Mistura::Aditiva);
    for (const auto& p : ceu_.particulas()) {
        glm::vec4 c = p.cor;
        c.a *= std::clamp(p.vida / p.vidaMax * 1.5f, 0.0f, 1.0f);
        r.retangulo(glm::floor(p.pos), glm::vec2(p.tamanho), c);
    }
    r.definirMistura(ke::Mistura::Normal);
    if (painel_.empty() && !pausa_ && !conversa_ && transicao_ < 0.0f && colocando_.empty() && !menuObras_ && !painelReino_ &&
        !relatorio_ && !diario_ && !forja_ && !cronicas_ && leitura_.empty() && loja_.empty() && !painelRegioes_ && escolha_.empty() &&
        !painelConselho_ && !decisao_ && !guilda_ && !cozinha_ && !alas_ && !brasaoEd_)
        rend_.desenharAlvo(r, f, mundo_, tempo_);
    std::string motivoObra;
    bool obraValida = false;
    if (!colocando_.empty()) {
        // Prévia da obra: verde onde dá para construir, vermelho onde não dá
        const BuildingDef& d = ctx_.dados.construcoes.at(colocando_);
        const glm::ivec2 t = tileFantasma();
        obraValida = mundo_.podeConstruir(colocando_, t, &motivoObra);
        const glm::vec4 c = obraValida ? glm::vec4(0.3f, 0.9f, 0.4f, 1) : glm::vec4(0.95f, 0.3f, 0.25f, 1);
        const float pulso = 0.25f + 0.08f * std::sin(tempo_ * 6.0f);
        r.retangulo(glm::vec2(t * 16), glm::vec2(d.tamanho * 16), {c.r, c.g, c.b, pulso});
        r.retangulo({t.x * 16.0f, (t.y + d.tamanho.y) * 16.0f}, {d.tamanho.x * 16.0f, 16.0f}, {c.r, c.g, c.b, pulso * 0.4f});
        r.contorno(glm::vec2(t * 16), glm::vec2(d.tamanho * 16), c);
        const ke::SpriteRegion& s = ctx_.app.recursos().regiao(std::format("{}_n1", d.aparencia));
        r.sprite(s, {t.x * 16.0f + d.tamanho.x * 8.0f - s.tamanho.x / 2.0f, (t.y + d.tamanho.y) * 16.0f - s.tamanho.y}, {1, 1, 1, 0.55f});
    }
    for (const auto& t : flutuantes_) {
        glm::vec4 c = t.cor;
        c.a = std::min(1.0f, t.vida * 1.5f);
        f.desenharCentralizado(r, t.texto, t.pos, c);
    }
    if (dragaoAtivo_) {
        // Sombra no chão bem abaixo e o dragão no alto, batendo as asas
        r.sprite(ctx_.app.recursos().regiao("sombra"), {dragaoX_ - 10.0f, dragaoY_ + 110.0f}, {1, 1, 1, 0.6f}, false, 3.0f);
        const ke::SpriteRegion& d = ctx_.app.recursos().regiao(std::format("dragao_{}", static_cast<int>(tempo_ * 4.0f) % 2));
        r.sprite(d, glm::floor(glm::vec2(dragaoX_, dragaoY_)));
    }
    r.terminar();

    // 3) Interface em coordenadas da tela (os painéis registram de novo onde o mouse pode clicar)
    hud_.cliques.limpar();
    r.comecar(glm::ortho(0.0f, 640.0f, 360.0f, 0.0f, -1.0f, 1.0f));
    // Aurora mágica: faixas de luz ondulando no alto do céu (fora da cripta)
    if (const float escuro = mundo_.escuridao(); mundo_.eventoAgora("aurora") && escuro > 0.3f && mundo_.regiaoAtual != "cripta") {
        r.definirMistura(ke::Mistura::Aditiva);
        static const glm::vec3 cores[3] = {{0.25f, 0.95f, 0.6f}, {0.3f, 0.7f, 1.0f}, {0.7f, 0.45f, 1.0f}};
        for (int k = 0; k < 3; ++k)
            for (int x = 0; x < 640; x += 4) {
                const float y = 30.0f + k * 26.0f + std::sin(x * 0.018f + tempo_ * 0.5f + k * 1.7f) * 16.0f;
                const float a = (0.14f + 0.08f * std::sin(x * 0.05f + tempo_ * 1.3f + k)) * escuro;
                r.retangulo({static_cast<float>(x), y}, {4, 26.0f + 10.0f * std::sin(x * 0.03f + k)}, {cores[k], a});
            }
        r.definirMistura(ke::Mistura::Normal);
    }
    if (flash_ > 0.0f) r.retangulo({0, 0}, {640, 360}, {1.0f, 0.95f, 0.85f, flash_ * 0.6f});
    if (feridoFlash_ > 0.0f) r.retangulo({0, 0}, {640, 360}, {0.7f, 0.05f, 0.05f, feridoFlash_ * 0.45f});
    hud_.desenhar(r, mundo_, tempo_);
    hud_.combate(r, mundo_, tempo_);
    if (tituloEra_ > 0.0f) {
        const float a = std::min(1.0f, std::min(tituloEra_, 5.0f - tituloEra_) * 1.5f);
        r.retangulo({0, 130}, {640, 60}, {0.02f, 0.02f, 0.03f, 0.55f * a});
        f.desenharCentralizado(r, "Uma nova era começa", {320, 146}, {cor::texto.r, cor::texto.g, cor::texto.b, a});
        f.desenharCentralizado(r, nomeEra_, {320, 168}, {cor::ouro.r, cor::ouro.g, cor::ouro.b, a}, true, 2);
    }
    if (tituloRegiao_ > 0.0f) {
        const float a = std::min(1.0f, tituloRegiao_);
        f.desenharCentralizado(r, nomeRegiao_, {320, 110}, {cor::ouro.r, cor::ouro.g, cor::ouro.b, a}, true, 2);
    }
    if (tituloNovo_ > 0.0f && tituloEra_ <= 0.0f) {
        const float a = std::min(1.0f, std::min(tituloNovo_, 4.5f - tituloNovo_) * 1.5f);
        r.retangulo({0, 130}, {640, 60}, {0.02f, 0.02f, 0.03f, 0.55f * a});
        f.desenharCentralizado(r, "O povo agora o chama de", {320, 146}, {cor::texto.r, cor::texto.g, cor::texto.b, a});
        f.desenharCentralizado(r, nomeTitulo_, {320, 168}, {cor::ouro.r, cor::ouro.g, cor::ouro.b, a}, true, 2);
    }
    if (forja_) hud_.lojaForja(r, mundo_, opcoesForja(), selForja_);
    if (!loja_.empty()) hud_.lojaMercador(r, mundo_, ctx_.dados.npcs.at(loja_), selLoja_);
    if (diario_) hud_.diario(r, mundo_);
    if (cronicas_) hud_.cronicas(r, mundo_, abaCronicas_, rolagem_, selFragmento_);
    if (!leitura_.empty())
        if (const PistaDef* p = ctx_.dados.lore.pista(leitura_)) hud_.leitura(r, *p);
    if (!painel_.empty()) hud_.painelConstrucao(r, mundo_, painel_, opcoesPainel(), selPainel_, tempo_);
    if (conversa_) hud_.caixaDialogo(r, *conversa_, linhaFala_, static_cast<int>(letras_), tempo_);
    if (menuObras_) hud_.menuObras(r, mundo_, opcoesObras(), selObra_);
    if (!colocando_.empty()) hud_.dicaColocacao(r, ctx_.dados.construcoes.at(colocando_).nivel(1)->nome, obraValida, motivoObra);
    if (painelReino_) hud_.painelReino(r, mundo_);
    if (painelRegioes_) hud_.painelRegioes(r, mundo_);
    if (painelConselho_) hud_.painelConselho(r, mundo_, selConselho_, propostoConselho(), tempo_);
    if (guilda_) hud_.painelGuilda(r, mundo_, abaGuilda_, selGuilda_, passoExpedicao_, destinoSel_, grupoSel_, tempo_);
    if (cozinha_) hud_.painelCozinha(r, mundo_, selCozinha_, tempo_);
    if (alas_) hud_.painelAlas(r, mundo_, selAla_, tempo_);
    if (brasaoEd_) hud_.editorBrasao(r, mundo_, rascunho_, selBrasao_, tempo_);
    if (decisao_ && mundo_.decisaoAtual()) {
        hud_.cliques.limpar();  // por cima do conselho: só as opções do caso valem para o mouse
        hud_.painelDecisao(r, mundo_, *mundo_.decisaoAtual(), selDecisao_, tempo_);
    }
    if (!escolha_.empty()) hud_.painelEscolha(r, mundo_, ctx_.dados.escolhas.at(escolha_), selEscolha_, tempo_);
    if (relatorio_ && transicao_ < 0.0f && leitura_.empty()) hud_.relatorioManha(r, mundo_);
    if (pausa_) hud_.menuPausa(r, OPCOES_PAUSA, selPausa_);
    if (pausa_ && telaOpcoes_.aberta()) telaOpcoes_.desenhar(r, f, ctx_);
    if (transicao_ >= 0.0f) {
        const float a = transicao_ < 1.0f ? transicao_ : 2.0f - transicao_;
        r.retangulo({0, 0}, {640, 360}, {0, 0, 0, std::clamp(a * 1.2f, 0.0f, 1.0f)});
        if (transicao_ > 0.7f && transicao_ < 1.3f)
            f.desenharCentralizado(r, derrota_ ? "Você caiu em combate..." : desmaio_ ? "Tudo fica escuro..." : "Zzz...", {320, 180},
                                   cor::apagado);
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
        if (ImGui::Button("Chamar o dragão (era 3)")) chamarDragao();
        ImGui::SameLine();
        if (ImGui::Button(ctx_.app.audio().mudo() ? "Som: mudo (M)" : "Som: ligado (M)")) ctx_.app.audio().alternarMudo();
        for (auto& b : mundo_.reino.construcoes) {
            const BuildingDef* d = mundo_.def(b);
            if (d && ImGui::SliderInt(b.id.c_str(), &b.level, 1, d->nivelMax()))
                if (const auto* nv = d->nivel(b.level)) b.name = nv->nome;
        }
    }
    if (ImGui::CollapsingHeader("Regiões")) {
        for (auto& [id, e] : mundo_.estadoRegioes) {
            ImGui::Text("%s (última visita: dia %d)", mundo_.nomeRegiao(id).c_str(), e.ultimaVisita);
            ImGui::SliderFloat(("Corrupção##" + id).c_str(), &e.corrupcao, 0.0f, 100.0f, "%.0f");
            ImGui::SliderFloat(("Segurança##" + id).c_str(), &e.seguranca, 0.0f, 100.0f, "%.0f");
            ImGui::SliderFloat(("Prosperidade##" + id).c_str(), &e.prosperidade, 0.0f, 100.0f, "%.0f");
        }
    }
    if (ImGui::Button("Salvar (F5)")) salvar();
    ImGui::SameLine();
    if (ImGui::Button("Recarregar save")) {
        std::string erro, aviso;
        if (auto d = ctx_.saves->carregar(ctx_.slot, &erro, &aviso)) ctx_.app.cenas().trocar(std::make_unique<WorldScene>(ctx_, d));
    }
    ImGui::End();
}

}
