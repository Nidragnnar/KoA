#include "Game/Demo/Roteiro.h"

#include <format>
#include <fstream>

#include <glm/geometric.hpp>

#include "Engine/Core/Logger.h"
#include "Game/App/Contexto.h"
#include "Game/Exploration/Components.h"
#include "Game/NPC/Npc.h"
#include "Game/Scenes/TitleScene.h"
#include "Game/Scenes/WorldScene.h"

namespace koa {

namespace {

// Primeiro lugar livre para a obra, em espiral a partir de um ponto
glm::ivec2 lugarLivre(const World& w, const std::string& def, glm::ivec2 perto) {
    for (int raio = 0; raio < 30; ++raio)
        for (int dy = -raio; dy <= raio; ++dy)
            for (int dx = -raio; dx <= raio; ++dx)
                if (std::max(std::abs(dx), std::abs(dy)) == raio && w.podeConstruir(def, perto + glm::ivec2(dx, dy))) return perto + glm::ivec2(dx, dy);
    return {-1, -1};
}

entt::entity inimigoMaisPerto(World& w, const std::string& tipo) {
    entt::entity melhor = entt::null;
    float d = 1e9f;
    for (auto [e, p, i] : w.reg.view<Posicao, Inimigo>().each())
        if (i.tipo == tipo && glm::distance(p.v, w.jogador.pos) < d) d = glm::distance(p.v, w.jogador.pos), melhor = e;
    return melhor;
}

int contarEsgotados(World& w) {
    int n = 0;
    for (auto [e, c] : w.reg.view<Coletavel>().each()) n += c.esgotado;
    return n;
}

// Fase 15: herdeiro de frente para a água, sem nada por perto (o E vira "Pescar")
bool beiraDaAgua(World& w) {
    for (int y = 1; y + 1 < w.mapa.altura(); ++y)
        for (int x = 1; x + 1 < w.mapa.largura(); ++x) {
            if (w.mapa.em(x, y).terreno != T_AGUA || w.mapa.em(x, y - 1).terreno != T_AGUA || w.mapa.em(x, y + 1).terreno == T_AGUA || w.mapa.em(x, y + 1).solido) continue;  // duas fileiras de água: a boia cai longe
            w.jogador.pos = {x * 16.0f + 8.0f, (y + 1) * 16.0f + 6.0f};
            w.jogador.dir = Direcao::Cima;
            w.passo(1.0f / 60.0f, {});
            if (w.alvo().tipo == TipoInteracao::Pescar) return true;
        }
    return false;
}

int peixes(const World& w) {
    return w.inventario.quantidade("peixe") + w.inventario.quantidade("truta") + w.inventario.quantidade("peixe_dourado");
}

// Diante do canteiro i (virado para cima)
void diante(World& w, glm::vec2 alvo) {
    w.jogador.pos = alvo + glm::vec2(0, 12);
    w.jogador.dir = Direcao::Cima;
}
}

void Roteiro::capturar(const std::string& nome) {
    aguardando_ = true;
    ctx_.app.pedirCaptura(pasta_ / (nome + ".bmp"), [this] { aguardando_ = false; });
    relatorio_.push_back("captura: " + nome);
}

void Roteiro::verificar(bool ok, const std::string& d) {
    relatorio_.push_back(std::format("[{}] {}", ok ? "OK" : "FALHOU", d));
    ke::log::info("roteiro: [{}] {}", ok ? "OK" : "FALHOU", d);
    if (!ok) ++falhas_;
}

void Roteiro::terminar() {
    if (terminou_) return;
    terminou_ = true;
    relatorio_.push_back(std::format("RESULTADO: {} falha(s)", falhas_));
    std::filesystem::create_directories(pasta_);
    std::ofstream f(pasta_ / "relatorio.txt");
    for (const auto& l : relatorio_) f << l << "\n";
    ctx_.codigoSaida = falhas_ == 0 ? 0 : 1;
    ctx_.app.parar();
}

bool Roteiro::posicionarJunto(World& w, entt::entity e) {
    const glm::vec2 p = w.reg.get<Posicao>(e).v;
    const std::pair<glm::vec2, Direcao> lados[] = {
        {{0, 12}, Direcao::Cima}, {{0, -12}, Direcao::Baixo}, {{-12, -1}, Direcao::Direita}, {{12, -1}, Direcao::Esquerda}};
    const auto obst = w.obstaculos();
    for (const auto& [d, dir] : lados) {
        Jogador teste = w.jogador;
        teste.pos = p + d;
        if (ke::colide(w.mapa, teste.caixa(), obst)) continue;
        w.jogador.pos = teste.pos;
        w.jogador.dir = dir;
        return true;
    }
    return false;
}

// Próximo objeto útil, mais perto do herói, para o recurso que ainda falta
entt::entity Roteiro::proximoAlvo(World& w) {
    const BuildingData* casa = w.reino.construcao("casa_herdeiro");
    const BuildingDef* def = casa ? w.def(*casa) : nullptr;
    if (!def) return entt::null;
    const Custo falta = w.inventario.faltando(def->nivel(2)->custo);
    entt::entity melhor = entt::null;
    float dist = 1e9f;
    const auto obst = w.obstaculos();
    auto acessivel = [&](glm::vec2 p) {
        for (const glm::vec2 d : {glm::vec2{0, 12}, {0, -12}, {-12, -1}, {12, -1}}) {
            Jogador t = w.jogador;
            t.pos = p + d;
            if (!ke::colide(w.mapa, t.caixa(), obst)) return true;
        }
        return false;
    };
    for (auto [e, p, c] : w.reg.view<Posicao, Coletavel>().each()) {
        if (c.esgotado || !falta.contains(c.recurso)) continue;
        const float d = glm::distance(p.v, w.jogador.pos);
        if (d < dist && acessivel(p.v)) dist = d, melhor = e;
    }
    for (auto [e, p, s] : w.reg.view<Posicao, Saqueavel>().each()) {
        if (s.aberto || !falta.contains(s.recurso)) continue;
        const float d = glm::distance(p.v, w.jogador.pos);
        if (d < dist && acessivel(p.v)) dist = d, melhor = e;
    }
    return melhor;
}

void Roteiro::naTitulo(TitleScene& t) {
    if (aguardando_ || terminou_) return;
    ++quadro_;
    if (quadro_ < 50) return;
    if (quadro_ == 50) {
        capturar(fase_ == 0 ? "01_titulo" : "09_titulo_continuar");
        return;
    }
    ke::Input& in = ctx_.app.input();
    if (fase_ == 0) {
        // Novo Jogo (1ª opção sem save) -> espaços -> o primeiro, vazio
        switch (quadro_) {
        case 51:
            verificar(!t.temSave(), "pasta de saves de teste começa vazia");
            in.simularToque(SDL_SCANCODE_RETURN);
            return;
        case 70: capturar("01b_novo_jogo_espacos"); return;
        case 72: in.simularToque(SDL_SCANCODE_RETURN); break;
        default: return;
        }
    } else {
        // Passa por Carregar, Opções e Controles e volta para Continuar
        // (menu: Continuar, Novo Jogo, Carregar, Opções, Controles, Sair)
        switch (quadro_) {
        case 51: verificar(t.temSave(), "título oferece Continuar depois de salvar"); [[fallthrough]];
        case 53:
        case 78:
        case 103: in.simularToque(SDL_SCANCODE_S); return;
        case 55:
        case 80:
        case 105: in.simularToque(SDL_SCANCODE_RETURN); return;
        case 75: capturar("09b_carregar_espacos"); return;
        case 100: capturar("09c_opcoes"); return;
        case 125: capturar("09d_controles"); return;
        case 76:
        case 101:
        case 126: in.simularToque(SDL_SCANCODE_ESCAPE); return;
        case 128:
        case 130:
        case 132:
        case 134: in.simularToque(SDL_SCANCODE_W); return;
        case 136: in.simularToque(SDL_SCANCODE_RETURN); break;  // "Continuar"
        default: return;
        }
    }
    etapa_ = 0;
    quadro_ = 0;
}

void Roteiro::noMundo(WorldScene& cena) {
    if (aguardando_ || terminou_) return;
    World& w = cena.mundo();
    ke::Input& in = ctx_.app.input();
    ++quadro_;
    if (++quadrosTotais_ > 60 * 560) {  // mais de 9 minutos de jogo simulado: algo travou
        verificar(false, "roteiro terminou dentro do tempo limite");
        terminar();
        return;
    }

    if (fase_ == 1) {
        // Fase 12: um caso do conselho abre sozinho depois do relatório da manhã; antes da etapa dele, fica para depois
        if (cena.decisaoAberta() && (etapa_ < 38 || etapa_ >= 42)) {
            in.simularToque(SDL_SCANCODE_ESCAPE);
            return;
        }
        // Fase 13: as hordas esperam a etapa delas (senão uma batalha cairia no meio das capturas antigas)
        if (etapa_ < 40) {
            w.hordaDia = 0;
            w.proximaHorda = std::max(w.proximaHorda, w.relogio.dia + 100);
        }
        switch (etapa_) {
        case 0: {
            if (quadro_ < 40) return;
            verificar(w.inventario.todos() == inventarioSalvo_, "inventário igual ao salvo");
            verificar(w.reino.construcao("casa_herdeiro")->level == 2, "casa continua no nível 2 após carregar");
            verificar(contarEsgotados(w) == esgotadosSalvos_, std::format("{} árvores/rochas esgotadas restauradas", esgotadosSalvos_));
            verificar(w.relogio.dia == 2, std::format("dia restaurado do save (dia {})", w.relogio.dia));
            const auto& rel = w.reg.get<Relacionamento>(w.morador("bram"));
            verificar(rel.conheceu && rel.afinidade == 25, std::format("amizade com Bram restaurada ({} pontos)", rel.afinidade));
            capturar("10_carregado");
            proxima();
            return;
        }
        case 1: {  // troca de atividade: às 13:00 o Bram volta do poço para a forja, andando
            const entt::entity bram = w.morador("bram");
            if (quadro_ == 1) {
                w.relogio.minutos = 12 * 60;
                w.posicionarMoradores();
                w.jogador.pos = w.reg.get<Posicao>(bram).v + glm::vec2(30, 20);
                xInicial_ = w.reg.get<Posicao>(bram).v.x;
                yInicial_ = w.reg.get<Posicao>(bram).v.y;
                w.relogio.minutos = 13 * 60 - 0.5f;
            }
            if (quadro_ == 90) capturar("11_morador_andando");
            if (quadro_ == 100) {
                const glm::vec2 agora = w.reg.get<Posicao>(bram).v;
                verificar(glm::distance(agora, {xInicial_, yInicial_}) > 24.0f,
                          std::format("Bram anda sozinho na troca de rotina ({:.0f} px)", glm::distance(agora, {xInicial_, yInicial_})));
                verificar(w.reg.get<Movimento>(bram).andando || w.reg.get<Movimento>(bram).caminho.empty(), "Bram segue um caminho (A*)");
                proxima();
            }
            return;
        }
        case 2:  // noite: tochas, fogueira e janelas acesas
            if (quadro_ == 1) {
                w.relogio.minutos = 21 * 60 + 30;
                w.posicionarMoradores();
                w.jogador.pos = {29 * 16 + 8, 25 * 16 + 14};  // olhando a fogueira de longe (sem a etiqueta na frente)
                w.jogador.dir = Direcao::Direita;
            }
            if (quadro_ == 40) {
                verificar(w.relogio.escuridao() > 0.9f, "21:30 já é noite fechada");
                capturar("12_noite_fogueira");
            }
            if (quadro_ == 45) {
                const BuildingData* casa = w.reino.construcao("casa_herdeiro");
                w.jogador.pos = w.pontoPorta(*casa) + glm::vec2(0, 30);
            }
            if (quadro_ == 80) {
                capturar("12b_noite_casa");
                proxima();
            }
            return;
        case 3:  // ficar acordado até as 2:00: desmaio
            if (quadro_ == 1) w.relogio.minutos = Relogio::FIM_DIA - 0.3f;
            if (quadro_ == 200) {
                verificar(w.relogio.dia == 3 && w.relogio.minutos < 6 * 60 + 30, std::format("desmaio às 2:00 leva ao dia 3 (dia {})", w.relogio.dia));
                capturar("13_depois_do_desmaio");
                proxima();
            }
            return;
        case 4: {  // fase 5: menu de obras → posicionar um campo com as teclas de verdade
            if (quadro_ == 1) {
                // Recursos de teste (equivale a horas de coleta)
                for (const char* r : {"madeira", "pedra", "ouro"}) w.inventario.definir(r, 400);
                w.inventario.definir("comida", 120);
                w.relogio.minutos = 9 * 60;
            }
            if (quadro_ == 2 && cena.relatorioAberto()) in.simularToque(SDL_SCANCODE_E);  // relatório do desmaio
            if (quadro_ == 5) in.simularToque(SDL_SCANCODE_B);
            if (quadro_ == 20) {
                verificar(cena.menuObrasAberto(), "B abre o menu de obras");
                capturar("15_menu_obras");
            }
            if (quadro_ == 22) in.simularToque(SDL_SCANCODE_S);  // 2ª opção: campo
            if (quadro_ == 26) in.simularToque(SDL_SCANCODE_E);
            if (quadro_ == 30) {
                verificar(cena.colocando() == "campo", "E no menu começa a posicionar o campo");
                const glm::ivec2 t = lugarLivre(w, "campo", {30, 27});
                alvoObra_ = t;
                const glm::ivec2 tam = ctx_.dados.construcoes.at("campo").tamanho;
                w.jogador.pos = {(t.x + tam.x / 2) * 16 + 8.0f, (t.y + tam.y + 1) * 16 + 12.0f};
                w.jogador.dir = Direcao::Cima;
            }
            if (quadro_ == 45) {
                verificar(cena.tileFantasma() == alvoObra_ && w.podeConstruir("campo", alvoObra_), "a prévia fica verde num terreno livre");
                capturar("16_colocando_campo");
            }
            if (quadro_ == 50) in.simularToque(SDL_SCANCODE_E);
            if (quadro_ == 70) {
                const BuildingData* b = w.reino.construcao("campo_1");
                verificar(b && !b->constructed && b->tile == alvoObra_, "o campo vira obra no lugar escolhido");
                verificar(w.inventario.quantidade("madeira") == 400 - 8, "a obra cobrou o custo");
                capturar("17_obra_campo");
                proxima();
            }
            return;
        }
        case 5:  // mais obras: taverna (felicidade) e moradia (população)
            if (quadro_ == 1) {
                const std::string t = w.construir("taverna", lugarLivre(w, "taverna", {30, 27}));
                const std::string m = w.construir("moradia", lugarLivre(w, "moradia", {44, 27}));
                verificar(!t.empty() && !m.empty(), "taverna e moradia começaram");
            }
            if (quadro_ == 10) proxima();
            return;
        case 6:  // painel do reino e impostos
            if (quadro_ == 5) in.simularToque(SDL_SCANCODE_K);
            if (quadro_ == 15) in.simularToque(SDL_SCANCODE_A);  // impostos: médio → baixo
            if (quadro_ == 30) {
                verificar(cena.painelReinoAberto(), "K abre o painel do reino");
                verificar(w.estado.imposto == Imposto::Baixo, "A/D muda os impostos");
                capturar("18_painel_reino");
            }
            if (quadro_ == 35) in.simularToque(SDL_SCANCODE_K);
            if (quadro_ == 45) {
                verificar(!cena.painelReinoAberto(), "K fecha o painel");
                proxima();
            }
            return;
        case 7: {  // dormir: obras prontas e relatório da manhã
            if (quadro_ == 1) {
                const BuildingData* casa = w.reino.construcao("casa_herdeiro");
                w.jogador.pos = w.pontoPorta(*casa) + glm::vec2(0, 9);
                w.jogador.dir = Direcao::Cima;
                comidaAntes_ = w.inventario.quantidade("comida");
                popAntes_ = w.estado.populacao;
            }
            if (quadro_ == 10) in.simularToque(SDL_SCANCODE_E);
            if (quadro_ == 20) in.simularToque(SDL_SCANCODE_S);
            if (quadro_ == 26) in.simularToque(SDL_SCANCODE_E);
            if (quadro_ == 200) {
                verificar(cena.relatorioAberto(), "de manhã aparece o relatório");
                bool prontas = true;
                for (const auto& b : w.reino.construcoes) prontas = prontas && b.constructed;
                verificar(prontas, "as obras ficaram prontas durante a noite");
                const int esperado = comidaAntes_ + 5 - popAntes_;
                verificar(w.inventario.quantidade("comida") == esperado,
                          std::format("comida: {} (esperado {} = {} + 5 do campo - {} comidos)", w.inventario.quantidade("comida"), esperado,
                                      comidaAntes_, popAntes_));
                capturar("19_relatorio_manha");
            }
            if (quadro_ == 205) in.simularToque(SDL_SCANCODE_E);
            if (quadro_ == 215) {
                verificar(!cena.relatorioAberto(), "E fecha o relatório");
                proxima();
            }
            return;
        }
        case 8: {  // uma semana depois: o vilarejo cresce
            if (quadro_ == 1) {
                popAntes_ = w.estado.populacao;
                for (int i = 0; i < 6; ++i) w.dormir(false);
                w.relogio.minutos = 10 * 60;
                w.posicionarMoradores();
                w.jogador.pos = {30 * 16 + 8, 24 * 16 + 12};
                w.jogador.dir = Direcao::Baixo;
            }
            if (quadro_ == 5) in.simularToque(SDL_SCANCODE_E);  // fecha o relatório
            if (quadro_ == 150) {
                int aldeoes = 0;
                for (auto [e, a] : w.reg.view<Aldeao>().each()) ++aldeoes;
                verificar(w.estado.populacao > popAntes_, std::format("a população cresceu: {} -> {}", popAntes_, w.estado.populacao));
                // O mercador é visitante: não é um dos moradores que contam na população
                const int moradores = static_cast<int>(std::ranges::count_if(ctx_.dados.npcs, [](const auto& n) { return !n.second.visitante(); }));
                verificar(aldeoes == w.estado.populacao - moradores,
                          std::format("{} aldeões andando pelo vilarejo", aldeoes));
                capturar("20_vilarejo_crescendo");
                proxima();
            }
            return;
        }
        case 9: {  // fase 6: Oswin oferece a missão
            const entt::entity oswin = w.morador("oswin");
            if (quadro_ == 1) {
                w.relogio.minutos = 9 * 60;
                w.posicionarMoradores();
                w.reg.get<Agenda>(oswin).dormindo = false;
                w.reg.get<Movimento>(oswin).caminho.clear();
                w.jogador.pos = w.reg.get<Posicao>(oswin).v + glm::vec2(0, 14);
                w.jogador.dir = Direcao::Cima;
            }
            if (quadro_ == 8) in.simularToque(SDL_SCANCODE_E);
            if (quadro_ > 20 && cena.conversa() && quadro_ % 10 == 0) in.simularToque(SDL_SCANCODE_E);
            if (quadro_ > 30 && !cena.conversa()) {
                verificar(w.missoes.contains("limpar_estrada"), "Oswin entrega a missão Limpar a Estrada");
                capturar("21_missao_aceita");
                proxima();
            }
            if (quadro_ > 800) verificar(false, "conversa com Oswin terminou"), terminar();
            return;
        }
        case 10:  // andar até a saída leste e entrar na floresta
            if (quadro_ == 1) {
                w.jogador.pos = {60 * 16 + 8, 19 * 16 + 12};
                w.jogador.dir = Direcao::Direita;
                in.simularSegurar(SDL_SCANCODE_D, true);
            }
            if (w.regiaoAtual == "floresta" && !trocouRegiao_) {
                trocouRegiao_ = true;
                in.limparSimulacao();
                quadro_ = 1000;  // marca: chegou
            }
            if (quadro_ == 1030) {
                verificar(true, "andar até a borda leste leva à Floresta Amaldiçoada");
                capturar("22_floresta");
                proxima();
            }
            if (quadro_ == 300) verificar(false, "chegou à floresta"), terminar();
            return;
        case 11: {  // combate de verdade: J até cinco sombras caírem
            if (quadro_ == 1) abatesAntes_ = w.abates["sombra"], alvo_ = entt::null;
            if (!w.reg.valid(alvo_)) {
                if (w.abates["sombra"] - abatesAntes_ >= 5) {
                    verificar(w.missoes.at("limpar_estrada").concluida, "5 sombras derrotadas: missão cumprida");
                    proxima();
                    return;
                }
                alvo_ = inimigoMaisPerto(w, "sombra");
                if (alvo_ == entt::null) verificar(false, "ainda havia sombras"), terminar();
                return;
            }
            if (quadro_ % 18 == 0) {
                w.jogador.pos = w.reg.get<Posicao>(alvo_).v + glm::vec2(0, 12);
                w.jogador.dir = Direcao::Cima;
                w.jogador.vida = w.jogador.vidaMax;
                in.simularToque(SDL_SCANCODE_J);
            }
            if (quadro_ == 26) capturar("23_combate");
            if (quadro_ > 3000) verificar(false, "combate terminou a tempo"), terminar();
            return;
        }
        case 12: {  // levar dano e comer
            if (quadro_ == 1) {
                const entt::entity lobo = inimigoMaisPerto(w, "lobo");
                w.jogador.vida = w.jogador.vidaMax;
                w.jogador.pos = w.reg.get<Posicao>(lobo).v + glm::vec2(6, 0);
            }
            if (quadro_ == 20) {
                verificar(w.jogador.vida < w.jogador.vidaMax, std::format("o lobo fere o herói (vida {})", w.jogador.vida));
                w.jogador.pos = {3 * 16 + 8, 17 * 16 + 12};  // foge para a entrada
                capturar("24_ferido");
            }
            if (quadro_ == 30) {
                vidaAntes_ = w.jogador.vida;
                in.simularToque(SDL_SCANCODE_H);
            }
            if (quadro_ == 40) {
                verificar(w.jogador.vida > vidaAntes_, std::format("H come e recupera vida ({} -> {})", vidaAntes_, w.jogador.vida));
                proxima();
            }
            return;
        }
        case 13: {  // a cripta e o Cavaleiro Caído
            if (quadro_ == 1) {
                w.jogador.pos = {46 * 16 + 8, 5 * 16 + 12};  // em cima da escada
            }
            if (quadro_ == 10) {
                verificar(w.regiaoAtual == "cripta", "a escada leva à cripta");
                // Recompensa que se ganharia com mais horas de jogo
                w.equip.possuidos.insert("espada_linhagem");
                w.equip.arma = "espada_linhagem";
                alvo_ = inimigoMaisPerto(w, "cavaleiro_caido");
            }
            if (quadro_ == 40) capturar("25_cripta");
            if (quadro_ > 40) {
                if (w.reg.valid(alvo_)) {
                    if (quadro_ % 18 == 0) {
                        w.jogador.pos = w.reg.get<Posicao>(alvo_).v + glm::vec2(0, 16);
                        w.jogador.dir = Direcao::Cima;
                        w.jogador.vida = w.jogador.vidaMax;
                        in.simularToque(SDL_SCANCODE_J);
                    }
                    if (quadro_ == 80) capturar("26_chefe");
                } else {
                    verificar(w.marcos.contains("chefe_cavaleiro_caido"), "o Cavaleiro Caído foi derrotado");
                    proxima();
                }
            }
            if (quadro_ > 4000) verificar(false, "luta com o chefe terminou"), terminar();
            return;
        }
        case 14: {  // cair em combate: acorda em casa no dia seguinte
            if (quadro_ == 1) {
                diaAntes_ = w.relogio.dia;
                const entt::entity e = inimigoMaisPerto(w, "esqueleto");
                w.jogador.vida = 1;
                w.jogador.invulneravel = 0.0f;
                w.jogador.pos = w.reg.get<Posicao>(e).v + glm::vec2(5, 0);
            }
            if (quadro_ == 200) {
                verificar(w.noVilarejo() && w.jogador.vida == w.jogador.vidaMax && w.relogio.dia == diaAntes_ + 1,
                          "derrotado, acorda em casa no dia seguinte com a vida cheia");
                capturar("27_derrota_relatorio");
            }
            if (quadro_ == 205) in.simularToque(SDL_SCANCODE_E);
            if (quadro_ == 215) proxima();
            return;
        }
        case 15: case 16: case 17: {  // fase 7: as eras (mesma vista da praça, de dia)
            const int era = etapa_ - 14;
            if (quadro_ == 1) {
                w.reino.eraForcada = era;
                w.reino.eraVisual = static_cast<float>(era);
                w.relogio.minutos = 10 * 60;
                w.posicionarMoradores();
                w.jogador.pos = {31 * 16 + 8, 24 * 16 + 12};
                w.jogador.dir = Direcao::Baixo;
            }
            if (quadro_ == 60 && era == 1) capturar("28_anuncio_era");
            if (quadro_ == 330) {
                verificar(w.eraNoTile({31, 20}) == era && w.eraNoTile({5, 35}) == era, std::format("o vilarejo inteiro chegou à era {}", era));
                capturar(std::format("29_era_{}", era));
                proxima();
            }
            return;
        }
        case 18:  // Alta Fantasia à noite, com o dragão
            if (quadro_ == 1) {
                w.relogio.minutos = 21 * 60 + 30;
                cena.chamarDragao();
            }
            if (quadro_ == 120) {
                verificar(cena.dragaoVoando(), "o dragão sobrevoa o reino na Alta Fantasia");
                capturar("30_era_3_noite_dragao");
                proxima();
            }
            return;
        case 19:  // meio da transição: a onda de mudança, vista de perto
            if (quadro_ == 1) {
                w.reino.eraForcada = 2;
                w.reino.eraVisual = 1.5f;
                w.relogio.minutos = 11 * 60;
                w.jogador.pos = {40 * 16 + 8, 22 * 16 + 12};
            }
            if (quadro_ == 5) w.reino.eraVisual = 1.5f;  // segura no meio do caminho para a captura
            if (quadro_ == 8) {
                verificar(w.eraNoTile({22, 17}) == 2 && w.eraNoTile({60, 34}) == 1, "a transformação avança em onda a partir da casa");
                capturar("31_onda_de_transformacao");
                proxima();
            }
            return;
        // ---------- Fase 9: Crônicas, títulos, fragmentos, mercador e monumentos ----------
        case 20: {  // o livro das Crônicas
            auto temPagina = [&](const std::string& m) {
                return std::ranges::any_of(w.cronicas, [&](const EntradaCronica& c) { return c.marco == m; });
            };
            if (quadro_ == 1) {
                w.relogio.minutos = 10 * 60;
                verificar(temPagina("chegada") && temPagina("casa_reconstruida") && temPagina("chefe_cavaleiro_caido") && temPagina("era:3"),
                          std::format("as Crônicas registraram a jornada ({} páginas)", w.cronicas.size()));
                verificar(w.marcos.contains("titulo:libertador"), "o herdeiro virou " + w.titulo().nome);
                in.simularToque(SDL_SCANCODE_C);
            }
            if (quadro_ == 20) {
                verificar(cena.cronicasAbertas(), "C abre as Crônicas");
                capturar("32_cronicas");
            }
            if (quadro_ == 22) in.simularToque(SDL_SCANCODE_D);
            if (quadro_ == 40) capturar("33_fragmentos");
            if (quadro_ == 42) in.simularToque(SDL_SCANCODE_C);
            if (quadro_ == 50) {
                verificar(!cena.cronicasAbertas(), "C fecha as Crônicas");
                proxima();
            }
            return;
        }
        case 21: {  // um fragmento da Queda: a inscrição do poço (aparece na era 1)
            const PistaDef& p = *ctx_.dados.lore.pista("inscricao_poco");
            if (quadro_ == 1) {
                w.jogador.pos = {p.tile.x * 16 + 8.0f, (p.tile.y + 1) * 16 + 14.0f};
                w.jogador.dir = Direcao::Cima;
            }
            if (quadro_ == 5) {
                verificar(w.alvo().tipo == TipoInteracao::Ler, "o fragmento brilha no chão: " + w.alvo().rotulo);
                in.simularToque(SDL_SCANCODE_E);
            }
            if (quadro_ == 25) {
                verificar(cena.lendo() == p.id && w.marcos.contains("pista:" + p.id), "E lê o fragmento e o guarda nas Crônicas");
                capturar("34_leitura_fragmento");
            }
            if (quadro_ == 27) in.simularToque(SDL_SCANCODE_E);
            if (quadro_ == 35) {
                verificar(cena.lendo().empty(), "E fecha a leitura");
                proxima();
            }
            return;
        }
        case 22: {  // o mercador viajante: conversa, banca e a carta selada
            const entt::entity corvin = w.morador("corvin");
            if (quadro_ == 1) {
                w.relogio.dia += (3 - w.relogio.dia % 3) % 3;  // um dia de visita
                w.relogio.minutos = 10 * 60;
                w.posicionarMoradores();
                w.inventario.definir("ouro", 200);
                verificar(!w.reg.get<Agenda>(corvin).dormindo, "com a estrada limpa, Corvin armou a banca na praça");
                posicionarJunto(w, corvin);
            }
            if (quadro_ == 5) in.simularToque(SDL_SCANCODE_E);
            // Avança a conversa até a banca abrir
            if (quadro_ > 10 && quadro_ % 10 == 0 && cena.conversa()) in.simularToque(SDL_SCANCODE_E);
            if (!cena.lojaAberta().empty() && etapaLoja_ == 0) {
                etapaLoja_ = quadro_;
                capturar("35_banca_mercador");
                return;
            }
            if (etapaLoja_ > 0) {
                const int q = quadro_ - etapaLoja_;
                if (q == 2 || q == 4 || q == 6) in.simularToque(SDL_SCANCODE_S);  // a carta é o 4º item
                if (q == 8) in.simularToque(SDL_SCANCODE_E);
                if (q == 30) {
                    verificar(w.marcos.contains("pista:carta_selada") && w.inventario.quantidade("ouro") == 160,
                              "a carta selada custou 40 de ouro e virou fragmento");
                    capturar("36_carta_selada");
                }
                if (q == 32) in.simularToque(SDL_SCANCODE_E);
                if (q == 40) in.simularToque(SDL_SCANCODE_ESCAPE);
                if (q == 50) {
                    verificar(cena.lojaAberta().empty(), "Esc fecha a banca");
                    proxima();
                }
            }
            if (quadro_ > 600) verificar(false, "a banca do mercador abriu"), terminar();
            return;
        }
        case 23: {  // monumento: o Memorial das Cinzas guarda os fragmentos achados
            if (quadro_ == 1) {
                for (const char* r : {"madeira", "pedra", "ouro"}) w.inventario.definir(r, 300);
                const glm::ivec2 t = lugarLivre(w, "memorial", {22, 26});
                memorial_ = w.construir("memorial", t);
                verificar(!memorial_.empty(), "o Memorial das Cinzas começou");
                w.dormir(false);
            }
            if (quadro_ == 10) in.simularToque(SDL_SCANCODE_E);  // fecha o relatório da manhã
            if (quadro_ == 20) {
                const BuildingData* b = w.reino.construcao(memorial_);
                verificar(b && b->constructed, "o Memorial ficou pronto durante a noite");
                w.relogio.minutos = 10 * 60;
                if (b) {
                    w.jogador.pos = w.pontoPorta(*b) + glm::vec2(0, 6);
                    w.jogador.dir = Direcao::Cima;
                }
            }
            if (quadro_ == 25) in.simularToque(SDL_SCANCODE_E);
            if (quadro_ == 45) {
                verificar(cena.painelAberto() == memorial_, "E no Memorial mostra a inscrição");
                capturar("37_memorial");
            }
            if (quadro_ == 47) in.simularToque(SDL_SCANCODE_ESCAPE);
            if (quadro_ == 55) proxima();
            return;
        }
        // ---------- Fase 10: festival, acontecimentos raros e inverno ----------
        case 24: {  // Festival da Colheita: começa às 17:00 e o povo vai para a praça
            const auto& lugares = ctx_.dados.estacoes.festival.lugares;
            if (quadro_ == 1) {
                w.relogio.dia = 21;  // 7º dia do outono
                w.relogio.minutos = 16 * 60 + 59;
                w.posicionarMoradores();
                w.jogador.pos = {33 * 16 + 8, 25 * 16 + 12};
                w.jogador.dir = Direcao::Cima;
            }
            if (quadro_ == 720) {  // uns 12 s: tempo de todo mundo atravessar o vilarejo a pé
                int naFesta = 0;
                for (auto [e, m, p] : w.reg.view<const Morador, const Posicao>().each()) {
                    const glm::ivec2 t{static_cast<int>(p.v.x) / 16, static_cast<int>(p.v.y) / 16};
                    for (const glm::ivec2 l : lugares) naFesta += glm::length(glm::vec2(t - l)) <= 1.5f;
                }
                verificar(w.festivalAgora() && w.marcos.contains(std::format("festival_era{}:1", static_cast<int>(w.reino.eraAlvo()))),
                          "às 17:00 o Festival da Colheita começa e entra nas Crônicas");
                verificar(naFesta >= 4, std::format("os moradores foram para a praça ({} perto da fogueira)", naFesta));
                capturar("38_festival");
            }
            if (quadro_ == 722) {
                w.relogio.minutos = 21 * 60;
                w.posicionarMoradores();
            }
            if (quadro_ == 870) {
                capturar("39_festival_noite");
                proxima();
            }
            return;
        }
        case 25: case 26: case 27: {  // as três noites raras: Lua de Sangue, aurora e chuva de meteoros
            static const char* ids[] = {"lua_sangue", "aurora", "meteoros"};
            static const char* fotos[] = {"40_lua_de_sangue", "41_aurora", "42_meteoros"};
            const std::string id = ids[etapa_ - 25];
            if (quadro_ == 1) {
                int dia = -1;
                for (int d = w.relogio.dia + 1; d < w.relogio.dia + 2000 && dia < 0; ++d)
                    if (const EventoDef* ev = w.eventoDoDia(d); ev && ev->id == id) dia = d;
                verificar(dia > 0, "existe uma noite de " + id);
                if (dia > 0) w.relogio.dia = dia;
                w.relogio.minutos = 21 * 60 + 30;
                w.posicionarMoradores();
                w.jogador.pos = {31 * 16 + 8, 24 * 16 + 12};
            }
            if (quadro_ == 120) {
                verificar(w.eventoAgora(id), "à noite começa: " + id);
                if (id == "lua_sangue") verificar(w.luzAmbiente().r > w.luzAmbiente().b, "a Lua de Sangue deixa a noite vermelha");
                capturar(fotos[etapa_ - 25]);
                proxima();
            }
            return;
        }
        case 28: {  // a estrela caída: recolher na manhã seguinte
            if (quadro_ == 1) {
                diaAntes_ = w.relogio.dia;
                ouroAntes_ = 0;
                w.dormir(false);
            }
            if (quadro_ == 10) in.simularToque(SDL_SCANCODE_E);  // fecha o relatório da manhã
            if (quadro_ == 20) {
                entt::entity estrela = entt::null;
                for (auto [e, m] : w.reg.view<Meteorito>().each()) estrela = e;
                verificar(estrela != entt::null, "uma estrela caiu perto do vilarejo");
                if (estrela != entt::null) {
                    w.jogador.pos = w.reg.get<Posicao>(estrela).v + glm::vec2(0, 12);
                    w.jogador.dir = Direcao::Cima;
                }
                ouroAntes_ = w.inventario.quantidade("ouro");
            }
            if (quadro_ == 24) capturar("43_estrela_caida");
            if (quadro_ == 26) in.simularToque(SDL_SCANCODE_E);
            if (quadro_ == 50) {
                verificar(w.inventario.quantidade("ouro") == ouroAntes_ + 40 && w.marcos.contains(std::format("meteorito:{}", diaAntes_)),
                          "E recolhe a estrela (+40 de ouro)");
                proxima();
            }
            return;
        }
        case 29: {  // inverno: neve, hortas fracas e criaturas mais fortes (painel K mostra)
            if (quadro_ == 1) {
                w.relogio.dia = 4 * 7 * ((w.relogio.dia - 1) / 28) + 23;  // 2º dia do inverno deste ano
                w.relogio.minutos = 11 * 60;
                w.posicionarMoradores();
                w.jogador.pos = {31 * 16 + 8, 24 * 16 + 12};
            }
            if (quadro_ == 240) {
                verificar(w.relogio.estacao() == Estacao::Inverno && w.forcaInimigos() > 1.2f, "no inverno as criaturas ficam mais fortes");
                capturar("44_inverno");
            }
            if (quadro_ == 242) in.simularToque(SDL_SCANCODE_K);
            if (quadro_ == 260) capturar("45_painel_reino_inverno");
            if (quadro_ == 262) in.simularToque(SDL_SCANCODE_K);
            if (quadro_ == 270) proxima();
            return;
        }
        // ---------- Fase 11: regiões vivas, a floresta purificada, as minas e o golem ----------
        case 30: {  // painel R: as terras de fora do vilarejo
            if (quadro_ == 1) {
                w.relogio.dia = 30;  // primavera do ano 2 (sem neve nas capturas)
                w.relogio.minutos = 10 * 60;
                w.posicionarMoradores();
                w.jogador.pos = {31 * 16 + 8, 24 * 16 + 12};
            }
            if (quadro_ == 5) in.simularToque(SDL_SCANCODE_R);
            if (quadro_ == 20) {
                verificar(cena.painelRegioesAberto(), "R abre as Terras do Reino");
                capturar("46_painel_regioes");
            }
            if (quadro_ == 22) in.simularToque(SDL_SCANCODE_R);
            if (quadro_ == 30) {
                verificar(!cena.painelRegioesAberto(), "R fecha o painel");
                proxima();
            }
            return;
        }
        case 31: {  // o Coração da Floresta: com pouca corrupção, E purifica a floresta
            w.jogador.vida = w.jogador.vidaMax;
            if (quadro_ == 1) {
                w.entrarRegiao("floresta", {27, 12}, Direcao::Cima);
                // Equivale a uns dias caçando criaturas na floresta (cada uma tira 1,5 de corrupção)
                w.estadoRegioes.at("floresta").corrupcao = 20.0f;
                std::vector<entt::entity> perto;  // as das ruínas já tinham sido vencidas
                for (auto [e, p, i] : w.reg.view<Posicao, Inimigo>().each())
                    if (glm::distance(p.v, glm::vec2(27 * 16 + 8, 11 * 16)) < 170.0f) perto.push_back(e);
                w.reg.destroy(perto.begin(), perto.end());
            }
            if (quadro_ <= 172) {  // parado diante do altar, meio de lado (para o altar aparecer na captura)
                w.jogador.pos = {27 * 16 - 7, 11 * 16 + 14};
                w.jogador.dir = Direcao::Cima;
            }
            if (quadro_ == 170) {
                verificar(w.alvo().tipo == TipoInteracao::Usar, "o altar aparece como alvo: " + w.alvo().rotulo);
                capturar("47_coracao_corrompido");
            }
            if (quadro_ == 172) in.simularToque(SDL_SCANCODE_E);
            if (quadro_ == 190) {
                verificar(w.marcos.contains("purificou:floresta") && w.nomeRegiao("floresta") == "Floresta Encantada",
                          "E no Coração purifica: Floresta Encantada");
                verificar(w.eraNoTile({20, 20}) == 3 && !w.regiaoHostil(), "a floresta purificada fica clara e em paz");
            }
            if (quadro_ == 191) w.jogador.pos = {24 * 16 + 8, 18 * 16 + 12};  // um passo atrás, para ver o altar
            if (quadro_ == 360) {
                capturar("48_floresta_encantada");
                proxima();
            }
            return;
        }
        case 32: {  // a passagem desabada: escorar com madeira e descer às minas
            w.jogador.vida = w.jogador.vidaMax;
            if (quadro_ == 1) {
                w.jogador.pos = {29 * 16 + 8, 33 * 16 + 12};
                w.jogador.dir = Direcao::Baixo;
                w.inventario.definir("madeira", World::MADEIRA_ENTULHO);
            }
            if (quadro_ == 20) capturar("49_passagem_minas");
            if (quadro_ == 22) in.simularToque(SDL_SCANCODE_E);
            if (quadro_ == 40) {
                verificar(w.marcos.contains("minas_abertas") && w.inventario.quantidade("madeira") == 0, "15 de madeira escoram a passagem");
                in.simularSegurar(SDL_SCANCODE_S, true);
            }
            if (w.regiaoAtual == "minas" && quadro_ < 1000) {
                in.limparSimulacao();
                quadro_ = 1000;
            }
            if (quadro_ == 1060) {
                verificar(w.nomeRegiao("minas") == "Minas Abandonadas", "a trilha desce às Minas Abandonadas");
                capturar("50_minas");
                proxima();
            }
            if (quadro_ == 400) verificar(false, "chegou às minas"), terminar();
            return;
        }
        case 33: {  // um veio de cristal e o Golem de Cinzas, que se rende
            w.jogador.vida = w.jogador.vidaMax;
            if (quadro_ == 1) {
                w.jogador.pos = {20 * 16 + 8, 19 * 16 + 12};
                w.jogador.dir = Direcao::Cima;
                for (auto [e, i] : w.reg.view<Inimigo>().each())
                    if (i.tipo == "aranha") i.perseguindo = false, i.origem = i.destino = w.reg.get<Posicao>(e).v;
            }
            if (quadro_ % 24 == 0 && quadro_ <= 120) {
                w.jogador.pos = {20 * 16 + 8, 19 * 16 + 12};
                w.jogador.dir = Direcao::Cima;
                in.simularToque(SDL_SCANCODE_E);
            }
            if (quadro_ == 60) capturar("51_veio_cristal");
            if (quadro_ == 140) {
                verificar(w.inventario.quantidade("cristal") >= 1, std::format("o veio rendeu cristal ({})", w.inventario.quantidade("cristal")));
                alvo_ = inimigoMaisPerto(w, "golem_cinzas");
                verificar(alvo_ != entt::null, "o Golem de Cinzas guarda o fundo das minas");
                if (alvo_ != entt::null) w.reg.get<Inimigo>(alvo_).vida = 70;  // a luta inteira levaria minutos
            }
            if (quadro_ > 140 && cena.escolhaAberta().empty()) {
                if (alvo_ == entt::null || !w.reg.valid(alvo_)) verificar(false, "o golem existe até a decisão"), terminar();
                else if (quadro_ % 18 == 0) {
                    w.jogador.pos = w.reg.get<Posicao>(alvo_).v + glm::vec2(0, 16);
                    w.jogador.dir = Direcao::Cima;
                    in.simularToque(SDL_SCANCODE_J);
                }
                if (quadro_ == 170) capturar("52_golem");
            }
            if (!cena.escolhaAberta().empty() && !escolhaAbriu_) {
                escolhaAbriu_ = true;
                quadro_ = 2000;
            }
            if (quadro_ == 2030) {
                verificar(cena.escolhaAberta() == "golem_cinzas" && w.reg.get<Inimigo>(alvo_).rendido, "com 30% da vida o golem se rende e pede uma decisão");
                capturar("53_escolha_golem");
                proxima();
            }
            if (quadro_ == 1500) verificar(false, "o golem se rendeu a tempo"), terminar();
            return;
        }
        case 34: {  // devolver a luz ao cristal (o rito aprendido na floresta): Minas Arcanas
            w.jogador.vida = w.jogador.vidaMax;
            if (quadro_ == 5) in.simularToque(SDL_SCANCODE_S);  // 2ª opção: purificar
            if (quadro_ == 10) in.simularToque(SDL_SCANCODE_E);
            if (quadro_ == 30) {
                verificar(cena.escolhaAberta().empty() && w.marcos.contains("escolha:golem_cinzas:purificar"), "E decide: o golem é purificado");
                bool guardiao = false;
                for (auto [e, v] : w.reg.view<const EnfeiteRegiao, const Visual>().each()) guardiao = guardiao || v.sprite == "guardiao_cristal";
                verificar(w.nomeRegiao("minas") == "Minas Arcanas" && guardiao, "as Minas Arcanas ganham o Guardião de Cristal");
                w.jogador.pos = {25 * 16 + 8, 27 * 16 + 4};
                w.jogador.dir = Direcao::Cima;
            }
            if (quadro_ == 260) {
                capturar("54_minas_arcanas");
                proxima();
            }
            return;
        }
        case 35: {  // as terras depois de tudo: duas regiões no painel R
            if (quadro_ == 1) {
                w.dormir(false);
                w.relogio.minutos = 10 * 60;
            }
            if (quadro_ == 10) in.simularToque(SDL_SCANCODE_E);  // fecha o relatório da manhã
            if (quadro_ == 20) in.simularToque(SDL_SCANCODE_R);
            if (quadro_ == 40) {
                verificar(cena.painelRegioesAberto(), "R mostra as duas terras");
                capturar("55_terras_do_reino");
            }
            if (quadro_ == 42) in.simularToque(SDL_SCANCODE_R);
            if (quadro_ == 50) proxima();
            return;
        }
        case 36: {  // mouse: clique numa opção do menu de pausa e ataque virado para o cursor
            if (quadro_ == 5) in.simularToque(SDL_SCANCODE_ESCAPE);
            if (quadro_ == 15) in.simularMouse({320, 160});  // passa por cima de "Salvar" (2ª opção)
            if (quadro_ == 25) {
                verificar(cena.pausado(), "Esc abre a pausa");
                capturar("56_pausa_mouse");
            }
            if (quadro_ == 27) in.simularClique(ke::BotaoMouse::Esquerdo, {320, 144});  // "Continuar"
            if (quadro_ == 35) {
                verificar(!cena.pausado(), "clicar em Continuar fecha a pausa");
                w.jogador.pos = {31 * 16 + 8, 24 * 16 + 12};
                w.jogador.dir = Direcao::Direita;
                w.jogador.acao = 0.0f;
            }
            if (quadro_ == 45) in.simularClique(ke::BotaoMouse::Esquerdo, {20, 180});  // bem à esquerda do herói
            if (quadro_ == 47) {
                verificar(w.jogador.dir == Direcao::Esquerda && w.jogador.atacando, "o clique esquerdo ataca virado para o cursor");
                proxima();
            }
            return;
        }
        // ---------- Fase 12: o Conselho Real e as decisões ----------
        case 37: {  // G: três cargos já nomeados; o Marechal o herdeiro escolhe com A/D e E
            if (quadro_ == 1) {
                verificar(w.conselhoFormado(), "com as obras de pé, o povo pediu um Conselho Real");
                w.decisaoPendente.clear();
                for (const auto& [c, npc] : {std::pair{"tesoureiro", "edda"}, {"intendente", "tilda"}, {"obras", "bram"}}) w.nomear(c, npc);
                w.nomear("marechal", "");
                w.relogio.minutos = 10 * 60;
            }
            if (quadro_ == 5) in.simularToque(SDL_SCANCODE_G);
            // vago → Bram → Edda → Maelis → Oswin
            if (quadro_ == 10 || quadro_ == 12 || quadro_ == 14 || quadro_ == 16) in.simularToque(SDL_SCANCODE_D);
            if (quadro_ == 25) {
                verificar(cena.conselhoAberto(), "G abre o Conselho Real");
                capturar("57_conselho_escolhendo");
            }
            if (quadro_ == 27) in.simularToque(SDL_SCANCODE_E);
            if (quadro_ == 40) {
                verificar(w.ocupante("marechal") == "oswin", "A/D e E nomeiam Oswin Marechal");
                verificar(w.resumo().seguranca > 0, std::format("o Marechal traz segurança ({})", w.resumo().seguranca));
                capturar("58_conselho");
            }
            if (quadro_ == 42) in.simularToque(SDL_SCANCODE_G);
            if (quadro_ == 50) {
                verificar(!cena.conselhoAberto(), "G fecha o conselho");
                proxima();
            }
            return;
        }
        case 38: {  // o caso chega de manhã, abre depois do relatório, e a decisão vira lei
            if (quadro_ == 1) {
                w.dormir(false);
                w.trazerDecisao("lenhadores");  // a mesa do conselho naquela manhã
                w.inventario.definir("ouro", 200);
                lealdadeAntes_ = w.lealdade.at("oswin");
            }
            if (quadro_ == 10) in.simularToque(SDL_SCANCODE_E);  // fecha o relatório da manhã
            if (quadro_ == 30) {
                verificar(cena.decisaoAberta(), "o caso do conselho abre depois do relatório");
                capturar("59_decisao");
            }
            if (quadro_ == 32) in.simularToque(SDL_SCANCODE_E);  // 1ª opção: a patrulha
            if (quadro_ == 50) {
                verificar(!cena.decisaoAberta() && w.leis.contains("patrulha") && w.inventario.quantidade("ouro") == 160,
                          "E decide: a patrulha vira lei (custou 40 de ouro)");
                verificar(w.lealdade.at("oswin") > lealdadeAntes_, "Oswin, que pediu a patrulha, ficou mais leal");
                in.simularToque(SDL_SCANCODE_G);
            }
            if (quadro_ == 65) capturar("60_conselho_com_lei");
            if (quadro_ == 67) in.simularToque(SDL_SCANCODE_G);
            if (quadro_ == 75) proxima();
            return;
        }
        // ---------- Fase 13: a Muralha Leste e a batalha no portão ----------
        case 39: {  // E na muralha em ruínas: reconstruir como paliçada; e uma torre e um posto atrás dela
            if (quadro_ == 1) {
                for (const char* r : {"madeira", "pedra", "ouro"}) w.inventario.definir(r, 400);
                w.relogio.minutos = 10 * 60;
                const BuildingData* m = w.muralha();
                verificar(m && m->level == 1 && w.portaoVidaMax() == 0, "a Muralha Leste começa em ruínas");
                if (m) {
                    w.jogador.pos = w.pontoPorta(*m) + glm::vec2(-6, 0);
                    w.jogador.dir = Direcao::Direita;
                }
            }
            if (quadro_ == 10) in.simularToque(SDL_SCANCODE_E);
            if (quadro_ == 25) {
                verificar(cena.painelAberto() == "muralha_leste", "E na muralha abre o painel");
                capturar("61_muralha_em_ruinas");
            }
            if (quadro_ == 27) in.simularToque(SDL_SCANCODE_E);  // Reconstruir
            if (quadro_ == 120) {
                verificar(w.muralha()->level == 2 && w.portaoVida == 150, "a paliçada fecha a estrada com um portão de 150 de vida");
                const std::string t = w.construir("torre", lugarLivre(w, "torre", {56, 14}));
                const std::string p = w.construir("posto_guarda", lugarLivre(w, "posto_guarda", {55, 23}));
                verificar(!t.empty() && !p.empty(), "uma torre e um posto da guarda atrás da muralha");
                w.jogador.pos = {57 * 16 + 8, 19 * 16 + 14};
                w.jogador.dir = Direcao::Direita;
            }
            if (quadro_ == 140) {
                capturar("62_palicada");
                proxima();
            }
            return;
        }
        case 40: {  // de manhã, os batedores avisam: a horda vem em alguns dias
            if (quadro_ == 1) {
                w.proximaHorda = 0;
                w.estado.populacao = std::max(w.estado.populacao, ctx_.dados.defesa.populacaoMin);
                w.relogio.dia = std::max(w.relogio.dia, ctx_.dados.defesa.diaMin);
                diaAntes_ = w.relogio.dia;
                w.dormir(false);
            }
            if (quadro_ == 20) {
                verificar(w.hordaDia > w.relogio.dia, std::format("os batedores avisaram a horda para o dia {} (hoje é {})", w.hordaDia, w.relogio.dia));
                capturar("63_aviso_da_horda");
            }
            if (quadro_ == 22) in.simularToque(SDL_SCANCODE_E);  // fecha o relatório
            if (quadro_ == 30) proxima();
            return;
        }
        case 41: {  // às 20:00 a horda chega; torre, soldados e o herdeiro lutam no portão
            if (quadro_ == 1) {
                w.relogio.dia = w.hordaDia;
                w.relogio.minutos = 20 * 60 - 1.0f;
                w.posicionarMoradores();
                w.jogador.pos = {58 * 16 + 8, 19 * 16 + 14};
                w.jogador.dir = Direcao::Direita;
                ouroAntes_ = w.inventario.quantidade("ouro");
            }
            if (quadro_ == 90) {
                verificar(w.emBatalha() && w.criaturasDaHorda() > 0 && w.soldadosNoPortao() > 0,
                          std::format("a horda chegou ao portão ({} criaturas, {} soldados)", w.criaturasDaHorda(), w.soldadosNoPortao()));
                capturar("64_batalha_no_portao");
            }
            if (quadro_ > 90 && w.emBatalha() && quadro_ % 18 == 0) {
                // O herdeiro vai à criatura mais perto do portão e golpeia
                entt::entity alvo = entt::null;
                float d = 1e9f;
                for (auto [e, p, i] : w.reg.view<Posicao, Inimigo>().each())
                    if (i.horda && glm::distance(p.v, w.jogador.pos) < d) d = glm::distance(p.v, w.jogador.pos), alvo = e;
                if (alvo != entt::null) {
                    w.jogador.pos = w.reg.get<Posicao>(alvo).v + glm::vec2(0, 12);
                    w.jogador.dir = Direcao::Cima;
                    w.jogador.vida = w.jogador.vidaMax;
                    in.simularToque(SDL_SCANCODE_J);
                }
            }
            if (quadro_ > 90 && !w.emBatalha() && quadro_ < 5000) {
                verificar(w.marcos.contains("horda:1") || w.marcos.contains("horda_saqueada:1"), "a primeira horda foi vencida e entrou nas Crônicas");
                verificar(w.inventario.quantidade("ouro") > ouroAntes_, "a vitória rende ouro");
                quadro_ = 5000;
            }
            if (quadro_ == 5040) {
                capturar("65_depois_da_batalha");
                proxima();
            }
            if (quadro_ == 3000) verificar(false, "a batalha terminou a tempo"), terminar();
            return;
        }
        // ---------- Fase 14: a guilda, uma expedição e as relíquias ----------
        case 42: {  // com o reino estável, a guilda é construída; de manhã chegam os aventureiros
            if (quadro_ == 1) {
                w.marcar("estabilidade");  // o roteiro tem um reino grande e feliz; aqui não espera a manhã certa
                for (const char* r : {"madeira", "pedra", "ouro"}) w.inventario.definir(r, 400);
                w.inventario.definir("comida", 200);
                guilda_ = w.construir("guilda", lugarLivre(w, "guilda", {44, 25}));
                verificar(!guilda_.empty(), "a guilda começou (reino estável)");
                w.dormir(false);
            }
            if (quadro_ == 10) in.simularToque(SDL_SCANCODE_E);  // fecha o relatório
            if (quadro_ == 20) {
                verificar(w.nivelGuilda() == 1 && w.aventureiros.size() == 3, std::format("a guilda ficou pronta com {} aventureiros", w.aventureiros.size()));
                w.relogio.minutos = 10 * 60;
                if (const BuildingData* b = w.reino.construcao(guilda_)) {
                    w.jogador.pos = w.pontoPorta(*b) + glm::vec2(0, 6);
                    w.jogador.dir = Direcao::Cima;
                }
            }
            if (quadro_ == 25) in.simularToque(SDL_SCANCODE_E);  // painel da construção
            if (quadro_ == 35) in.simularToque(SDL_SCANCODE_S);  // Reconstruir → Abrir a guilda
            if (quadro_ == 40) in.simularToque(SDL_SCANCODE_E);
            if (quadro_ == 60) {
                verificar(cena.guildaAberta(), "\"Abrir a guilda\" abre o painel dos aventureiros");
                capturar("66_guilda_destinos");
            }
            if (quadro_ == 62) proxima();
            return;
        }
        case 43: {  // Ruínas do Vale com Kael e Ysolde
            if (quadro_ == 2) in.simularToque(SDL_SCANCODE_E);   // destino 1: Ruínas do Vale
            if (quadro_ == 8) in.simularToque(SDL_SCANCODE_E);   // Kael
            if (quadro_ == 12) in.simularToque(SDL_SCANCODE_S);
            if (quadro_ == 16) in.simularToque(SDL_SCANCODE_E);  // Ysolde
            if (quadro_ == 20 || quadro_ == 24) in.simularToque(SDL_SCANCODE_S);  // até "Partir"
            if (quadro_ == 35) capturar("67_grupo_da_expedicao");
            if (quadro_ == 37) in.simularToque(SDL_SCANCODE_E);
            if (quadro_ == 50) {
                verificar(w.expedicao.destino == "ruinas_vale" && w.expedicao.grupo.size() == 2, "a expedição partiu com Kael e Ysolde");
                in.simularToque(SDL_SCANCODE_ESCAPE);
            }
            if (quadro_ == 60) {
                verificar(!cena.guildaAberta(), "Esc fecha a guilda");
                proxima();
            }
            return;
        }
        case 44: {  // dois dias depois, a volta no relatório da manhã
            if (quadro_ == 1) {
                diaAntes_ = w.expedicao.volta;
                while (w.relogio.dia < diaAntes_ - 1) w.dormir(false);
                w.dormir(false);  // a manhã da volta (o relatório desta fica na tela)
            }
            if (quadro_ == 20) {
                const bool voltou = std::ranges::any_of(w.ultimoRelatorio.notas, [](const std::string& n) { return n.find("A expedição a Ruínas do Vale Cinzento voltou") != std::string::npos; });
                verificar(voltou && w.expedicao.destino.empty(), "a expedição voltou e contou como foi no relatório");
                capturar("68_volta_da_expedicao");
            }
            if (quadro_ == 22) in.simularToque(SDL_SCANCODE_E);
            if (quadro_ == 30) proxima();
            return;
        }
        case 45: {  // relíquias: expor a Coroa de Ferro Negro revela a maldição
            if (quadro_ == 1) {
                w.ganharReliquia("diario_rainha");
                w.ganharReliquia("coroa_ferro");
                for (auto& r : w.reliquias) r.exposta = false;
                if (const BuildingData* b = w.reino.construcao(guilda_)) {
                    w.jogador.pos = w.pontoPorta(*b) + glm::vec2(0, 6);
                    w.jogador.dir = Direcao::Cima;
                }
            }
            if (quadro_ == 5) in.simularToque(SDL_SCANCODE_E);
            if (quadro_ == 12) in.simularToque(SDL_SCANCODE_S);
            if (quadro_ == 16) in.simularToque(SDL_SCANCODE_E);  // Abrir a guilda
            if (quadro_ == 22 || quadro_ == 26) in.simularToque(SDL_SCANCODE_D);  // aba das relíquias
            if (quadro_ == 30) in.simularToque(SDL_SCANCODE_S);  // a coroa
            if (quadro_ == 40) capturar("69_reliquias");
            if (quadro_ == 42) in.simularToque(SDL_SCANCODE_E);  // expor
            if (quadro_ == 60) {
                const EstadoReliquia* c = w.estadoReliquia("coroa_ferro");
                verificar(c && c->exposta && c->conhecida, "exposta, a Coroa de Ferro Negro revela a maldição");
                capturar("70_maldicao");
            }
            if (quadro_ == 62) in.simularToque(SDL_SCANCODE_ESCAPE);
            if (quadro_ == 70) proxima();
            return;
        }
        // ---------- Fase 15: a horta, a pesca, o fogão, os animais e um presente ----------
        case 46: {  // horta: plantar com E, regar, ver crescer e colher (no verão, tudo cresce)
            if (cena.decisaoAberta() && quadro_ % 5 == 0) in.simularToque(SDL_SCANCODE_ESCAPE);  // caso do conselho: fica para depois
            const auto& pos = ctx_.dados.cotidiano.canteiros;
            auto base = [&](int i) { return glm::vec2(pos[i].x * 16.0f + 8.0f, pos[i].y * 16.0f + 14.0f); };
            if (quadro_ == 1) {
                while (!(w.relogio.estacao() == Estacao::Verao && w.relogio.diaDaEstacao() == 1)) ++w.relogio.dia;
                w.relogio.minutos = 9 * 60;
                for (auto& c : w.canteiros) c = {};
                w.inventario.definir("semente_nabo", 4);
                w.inventario.definir("semente_trigo", 1);
                w.inventario.definir("semente_abobora", 1);
                w.inventario.definir("nabo", 0);
                diante(w, base(0));
            }
            if (quadro_ == 5) in.simularToque(SDL_SCANCODE_E);  // planta
            if (quadro_ == 15) verificar(w.canteiros[0].cultura == "nabo", "E no canteiro vazio planta a semente da estação");
            if (quadro_ == 20) in.simularToque(SDL_SCANCODE_E);  // rega
            if (quadro_ == 30) {
                verificar(w.canteiros[0].regadoDia == w.relogio.dia, "E de novo rega o canteiro");
                // Os outros três já com idades diferentes, para ver os estágios
                w.canteiros[1] = {"trigo", 2, 0, false};
                w.canteiros[2] = {"abobora", 2, 0, false};
                w.canteiros[3] = {"nabo", 0, 0, false};
                for (int i = 1; i < 4; ++i) w.usarCanteiro(i);
                diante(w, base(1) + glm::vec2(8, 0));
            }
            if (quadro_ == 40) capturar("71_horta");
            if (quadro_ == 42) {
                for (int d = 0; d < 2; ++d) {
                    for (int i = 0; i < 4; ++i)
                        if (!w.canteiros[i].maduro && w.canteiros[i].regadoDia != w.relogio.dia) w.usarCanteiro(i);
                    w.dormir(false);
                }
            }
            if (quadro_ == 55) in.simularToque(SDL_SCANCODE_E);  // fecha o relatório
            if (quadro_ == 65) {
                int maduros = 0;
                for (const auto& c : w.canteiros) maduros += c.maduro;
                verificar(maduros == 4, std::format("regados todo dia, {} de 4 canteiros prontos para colher", maduros));
                w.relogio.minutos = 9 * 60;
                diante(w, base(1) + glm::vec2(8, 0));
            }
            if (quadro_ == 75) capturar("72_horta_madura");
            if (quadro_ == 77) diante(w, base(0));
            if (quadro_ == 80) in.simularToque(SDL_SCANCODE_E);  // colhe
            if (quadro_ == 95) {
                verificar(w.inventario.quantidade("nabo") == 2 && w.marcos.contains("primeira_colheita"), "E colhe o nabo (+2) e a primeira colheita vai para as Crônicas");
                for (int i = 1; i < 4; ++i) w.usarCanteiro(i);
                proxima();
            }
            return;
        }
        case 47: {  // pesca: E de frente para a água, espera a mordida e puxa com o clique
            if (cena.decisaoAberta() && quadro_ % 5 == 0) in.simularToque(SDL_SCANCODE_ESCAPE);  // caso do conselho: fica para depois
            if (quadro_ == 1) {
                w.relogio.minutos = 10 * 60;
                abatesAntes_ = 0;
                const bool achou = beiraDaAgua(w);
                verificar(achou, "há água no vilarejo para pescar");
                if (!achou) return proxima();
                comidaAntes_ = peixes(w);
            }
            if (quadro_ == 5) in.simularToque(SDL_SCANCODE_E);
            if (quadro_ == 12) verificar(w.pescando(), "E de frente para a água lança a linha");
            if (quadro_ == 25) capturar("73_pescando");
            if (quadro_ > 27 && quadro_ < 600 && w.peixeMordendo() && abatesAntes_ == 0) {
                abatesAntes_ = quadro_;
                capturar("74_mordida");
            }
            if (abatesAntes_ > 0 && quadro_ == abatesAntes_ + 3) in.simularClique(ke::BotaoMouse::Esquerdo, {340, 170});  // o clique puxa (não ataca)
            if (abatesAntes_ > 0 && quadro_ == abatesAntes_ + 15) {
                verificar(!w.pescando() && peixes(w) == comidaAntes_ + 1, std::format("o clique na hora da mordida pesca ({} peixe)", peixes(w) - comidaAntes_));
                abatesAntes_ = 0;
                proxima();
            }
            if (quadro_ == 600) verificar(false, "o peixe mordeu a tempo"), proxima();
            return;
        }
        case 48: {  // o fogão: a fogueira da praça abre a cozinha; sopa, comer pelo botão e o banquete
            if (cena.decisaoAberta() && quadro_ % 5 == 0) in.simularToque(SDL_SCANCODE_ESCAPE);  // caso do conselho: fica para depois
            if (quadro_ == 1) {
                for (auto [e, p, i] : w.reg.view<const Posicao, const Interativo>().each())
                    if (i.acao == "cozinha") w.jogador.pos = p.v + glm::vec2(0, 14), w.jogador.dir = Direcao::Cima;
                w.inventario.definir("nabo", 2);
                for (const char* r : {"peixe", "trigo"}) w.inventario.definir(r, 3);
                w.inventario.definir("abobora", 1);
                w.inventario.definir("peixe_dourado", 1);
                w.inventario.definir("ovo", 3);
                w.jogador.vida = 40;
                vidaAntes_ = 40;
            }
            if (quadro_ == 5) in.simularToque(SDL_SCANCODE_E);
            if (quadro_ == 15) verificar(cena.cozinhaAberta(), "E na fogueira abre o fogão");
            if (quadro_ == 18) in.simularToque(SDL_SCANCODE_E);  // sopa de nabo
            if (quadro_ == 25) {
                verificar(w.inventario.quantidade("sopa_nabo") == 1 && w.inventario.quantidade("nabo") == 0, "cozinhou a sopa com os 2 nabos");
                capturar("75_cozinha");
            }
            if (quadro_ == 30) in.simularClique(ke::BotaoMouse::Esquerdo, {320, 283});  // botão "comer agora"
            if (quadro_ == 40) verificar(w.jogador.vida == vidaAntes_ + 40, std::format("o botão comer cura (+{} de vida)", w.jogador.vida - vidaAntes_));
            if (quadro_ == 45) in.simularClique(ke::BotaoMouse::Esquerdo, {200, 250});  // clique no banquete: cozinha
            if (quadro_ == 55) {
                verificar(w.inventario.quantidade("banquete") == 1, "o clique na receita do banquete cozinha");
                ouroAntes_ = static_cast<int>(w.estado.felicidade);
            }
            if (quadro_ == 58) in.simularToque(SDL_SCANCODE_F);  // servir ao povo
            if (quadro_ == 68) {
                verificar(w.inventario.quantidade("banquete") == 0 && w.marcos.contains("cozinhou:banquete"), "F serve o banquete ao povo (página nas Crônicas)");
                capturar("76_banquete");
            }
            if (quadro_ == 72) in.simularToque(SDL_SCANCODE_ESCAPE);
            if (quadro_ == 80) {
                verificar(!cena.cozinhaAberta(), "Esc fecha o fogão");
                proxima();
            }
            return;
        }
        case 49: {  // galinheiro e curral: de manhã chegam os bichos; carinho rende um ovo a mais
            if (cena.decisaoAberta() && quadro_ % 5 == 0) in.simularToque(SDL_SCANCODE_ESCAPE);  // caso do conselho: fica para depois
            if (quadro_ == 1) {
                for (const char* r : {"madeira", "pedra", "ouro"}) w.inventario.definir(r, 400);
                w.inventario.definir("comida", 200);
                w.inventario.definir("ovo", 0);
                const std::string g = w.construir("galinheiro", lugarLivre(w, "galinheiro", {23, 12}));
                const std::string c = w.construir("curral", lugarLivre(w, "curral", {27, 12}));
                verificar(!g.empty() && !c.empty(), "galinheiro e curral começaram");
                w.dormir(false);
            }
            if (quadro_ == 10) in.simularToque(SDL_SCANCODE_E);  // relatório
            if (quadro_ == 20) {
                int galinhas = 0, vacas = 0;
                for (auto [e, a] : w.reg.view<const Animal>().each()) (a.tipo == "galinha" ? galinhas : vacas) += 1;
                verificar(galinhas == 3 && vacas == 2, std::format("prontos, chegaram {} galinhas e {} vacas", galinhas, vacas));
                w.relogio.minutos = 10 * 60;
                for (auto [e, p, a] : w.reg.view<Posicao, Animal>().each())
                    if (a.tipo == "galinha") {
                        a.espera = 30.0f;  // parada, para o carinho alcançar
                        diante(w, p.v);
                        break;
                    }
            }
            if (quadro_ == 25) in.simularToque(SDL_SCANCODE_E);
            if (quadro_ == 35) {
                verificar(!w.carinhos.empty(), "E perto da galinha faz carinho");
                capturar("77_animais");
            }
            if (quadro_ == 40) {
                diaAntes_ = w.inventario.quantidade("ovo");
                w.dormir(false);
            }
            if (quadro_ == 55) {
                verificar(w.inventario.quantidade("ovo") == diaAntes_ + 4 && w.inventario.quantidade("leite") >= 2,
                          std::format("de manhã: +{} ovos (um a mais pelo carinho) e leite", w.inventario.quantidade("ovo") - diaAntes_));
                capturar("78_relatorio_animais");
            }
            if (quadro_ == 57) in.simularToque(SDL_SCANCODE_E);
            if (quadro_ == 65) proxima();
            return;
        }
        case 50: {  // presente: F na conversa dá o prato favorito do Bram
            if (cena.decisaoAberta() && quadro_ % 5 == 0) in.simularToque(SDL_SCANCODE_ESCAPE);  // caso do conselho: fica para depois
            const entt::entity bram = w.morador("bram");
            if (quadro_ == 1) {
                w.relogio.minutos = 10 * 60;
                w.inventario.definir("ensopado", 1);
                w.presenteDia.erase("bram");
                w.reg.get<Agenda>(bram).dormindo = false;
                w.reg.get<Movimento>(bram).caminho.clear();
                w.jogador.pos = w.reg.get<Posicao>(bram).v + glm::vec2(0, 14);
                w.jogador.dir = Direcao::Cima;
                popAntes_ = w.coracoesDe("bram");
            }
            if (quadro_ == 8) in.simularToque(SDL_SCANCODE_E);
            if (quadro_ == 25) {
                verificar(cena.conversa() && cena.conversa()->presente == "ensopado", "a conversa oferece o presente (F dar Ensopado)");
                capturar("79_presente");
            }
            if (quadro_ == 28) in.simularToque(SDL_SCANCODE_F);
            if (quadro_ == 40) {
                verificar(w.inventario.quantidade("ensopado") == 0 && w.coracoesDe("bram") == popAntes_ + 1 && w.presenteDia.contains("bram"),
                          "F dá o favorito: +1 coração para o Bram");
                capturar("80_presente_dado");
            }
            if (quadro_ > 45 && cena.conversa() && quadro_ % 10 == 0) in.simularToque(SDL_SCANCODE_E);
            if (quadro_ > 50 && !cena.conversa()) proxima();
            if (quadro_ == 400) verificar(false, "a conversa terminou"), proxima();
            return;
        }
        // ---------- Fase 16: o castelo, as alas e o brasão ----------
        case 51: {  // o castelo: E no portão abre o painel; "Reconstruir" levanta a Torre de Menagem
            if (cena.decisaoAberta() && quadro_ % 5 == 0) in.simularToque(SDL_SCANCODE_ESCAPE);  // caso do conselho: fica para depois
            const BuildingData* c = w.reino.construcao("castelo");
            if (quadro_ == 1) {
                verificar(c && c->level == 1, "o castelo começa em ruínas, como construção do reino");
                for (const char* r : {"madeira", "pedra", "ouro"}) w.inventario.definir(r, 3000);
                w.inventario.definir("cristal", 60);
                w.marcar("era:2", "", true);
                w.marcar("era:3", "", true);
                w.relogio.minutos = 10 * 60;
                if (c) w.jogador.pos = w.pontoPorta(*c) + glm::vec2(0, 6), w.jogador.dir = Direcao::Cima;
            }
            if (quadro_ == 5) in.simularToque(SDL_SCANCODE_E);
            if (quadro_ == 15) {
                verificar(cena.painelAberto() == "castelo", "E no portão do castelo abre o painel");
                capturar("81_painel_castelo");
            }
            if (quadro_ == 18) in.simularToque(SDL_SCANCODE_E);  // Reconstruir
            if (quadro_ == 30) {
                verificar(w.nivelCastelo() == 2 && w.vagasAlas() == 1, "a Torre de Menagem subiu e abriu a primeira vaga de ala");
                w.jogador.pos.y += 40.0f;  // um passo atrás, para o castelo caber na tela
            }
            if (quadro_ == 45) capturar("82_torre_de_menagem");
            if (quadro_ == 48) {
                w.melhorarConstrucao("castelo");
                w.melhorarConstrucao("castelo");
            }
            if (quadro_ == 65) {
                verificar(w.nivelCastelo() == 4 && w.marcos.contains("castelo_n4"), "com as eras certas, o castelo chega ao Castelo Real");
                capturar("83_castelo_real");
            }
            if (quadro_ == 68) proxima();
            return;
        }
        case 52: {  // alas: tesouro pelo teclado, jardim pelo clique
            if (cena.decisaoAberta() && quadro_ % 5 == 0) in.simularToque(SDL_SCANCODE_ESCAPE);
            if (quadro_ == 1)
                if (const BuildingData* c = w.reino.construcao("castelo")) w.jogador.pos = w.pontoPorta(*c) + glm::vec2(0, 6), w.jogador.dir = Direcao::Cima;
            if (quadro_ == 5) in.simularToque(SDL_SCANCODE_E);
            if (quadro_ == 12) in.simularToque(SDL_SCANCODE_E);  // "Alas do castelo" (o primeiro, já no nível máximo)
            if (quadro_ == 20) verificar(cena.alasAbertas(), "\"Alas do castelo\" abre a lista das alas");
            if (quadro_ == 22) in.simularToque(SDL_SCANCODE_E);  // Sala do Tesouro
            if (quadro_ == 30) in.simularClique(ke::BotaoMouse::Esquerdo, {200, 165});  // Jardim Real
            if (quadro_ == 40) {
                verificar(w.alas == std::vector<std::string>{"tesouro", "jardim"}, std::format("ergueu {} alas (tesouro pelo E, jardim pelo clique)", w.alas.size()));
                capturar("84_alas");
            }
            if (quadro_ == 42) in.simularToque(SDL_SCANCODE_ESCAPE);
            if (quadro_ == 50) {
                verificar(!cena.alasAbertas(), "Esc fecha as alas");
                proxima();
            }
            return;
        }
        case 53: {  // o brasão: teclado e mouse no editor, salvar e ver nos estandartes e nas bandeiras
            if (cena.decisaoAberta() && quadro_ % 5 == 0) in.simularToque(SDL_SCANCODE_ESCAPE);
            if (quadro_ == 1)
                if (const BuildingData* c = w.reino.construcao("castelo")) w.jogador.pos = w.pontoPorta(*c) + glm::vec2(0, 6), w.jogador.dir = Direcao::Cima;
            if (quadro_ == 5) in.simularToque(SDL_SCANCODE_E);
            if (quadro_ == 12) in.simularToque(SDL_SCANCODE_S);
            if (quadro_ == 16) in.simularToque(SDL_SCANCODE_E);  // "Brasão da linhagem"
            if (quadro_ == 24) verificar(cena.brasaoAberto(), "\"Brasão da linhagem\" abre o editor");
            // Forma redonda, partido, campo azure, leão (teclado) e depois dragão (clique na seta), símbolo prata, segundo lema
            const int passos[][2] = {{26, SDL_SCANCODE_D}, {29, SDL_SCANCODE_S}, {32, SDL_SCANCODE_D}, {35, SDL_SCANCODE_S}, {38, SDL_SCANCODE_D},
                                     {41, SDL_SCANCODE_S}, {44, SDL_SCANCODE_S}, {47, SDL_SCANCODE_D}, {50, SDL_SCANCODE_S}, {53, SDL_SCANCODE_D},
                                     {56, SDL_SCANCODE_S}, {59, SDL_SCANCODE_D}};
            for (const auto& [q, tecla] : passos)
                if (quadro_ == q) in.simularToque(static_cast<SDL_Scancode>(tecla));
            if (quadro_ == 62) in.simularClique(ke::BotaoMouse::Esquerdo, {335, 178});  // seta ">" do símbolo
            if (quadro_ == 72) capturar("85_editor_brasao");
            if (quadro_ == 75) in.simularClique(ke::BotaoMouse::Esquerdo, {265, 308});  // Salvar
            if (quadro_ == 85) {
                const Brasao esperado{"redondo", "partido", "azure", "ouro", "dragao", "prata", 1};
                verificar(!cena.brasaoAberto() && w.brasaoCriado && w.brasao == esperado,
                          "o brasão montado no editor foi salvo: " + textoBrasao(ctx_.dados.castelo, w.brasao));
                verificar(w.marcos.contains("brasao"), "o brasão novo vira página nas Crônicas");
                w.jogador.pos = {31.0f * 16, 19.0f * 16};  // a praça, entre os estandartes
                w.jogador.dir = Direcao::Baixo;
            }
            if (quadro_ == 100) capturar("86_estandartes");
            if (quadro_ == 102) w.jogador.pos = {32.0f * 16, 10.0f * 16};
            if (quadro_ == 118) capturar("87_bandeiras_castelo");
            if (quadro_ == 120) proxima();
            return;
        }
        case 54:
            w.reino.eraForcada = 0;
            w.reino.eraVisual = 0.0f;
            terminar();
            return;
        }
        return;
    }

    switch (etapa_) {
    case 0:  // chegada
        if (quadro_ == 60) {
            verificar(cena.carregou(), "mapa e dados carregados");
            capturar("02_vilarejo_inicio");
            proxima();
        }
        return;
    case 1:  // andar para a direita segurando D
        if (quadro_ == 1) {
            xInicial_ = w.jogador.pos.x;
            in.simularSegurar(SDL_SCANCODE_D, true);
        }
        if (quadro_ == 40) capturar("03_andando");
        if (quadro_ == 55) {
            in.limparSimulacao();
            verificar(w.jogador.pos.x - xInicial_ > 40.0f, std::format("herói andou {:.0f} px para a direita", w.jogador.pos.x - xInicial_));
            proxima();
        }
        return;
    case 2:  // cortar a primeira árvore, golpe a golpe
        if (quadro_ == 1) {
            // Árvore acessível mais perto do herói
            alvo_ = entt::null;
            float dist = 1e9f;
            const Jogador original = w.jogador;
            for (auto [e, p, c] : w.reg.view<Posicao, Coletavel>().each()) {
                if (c.tipo != "arvore" || c.esgotado) continue;
                const float d = glm::distance(p.v, original.pos);
                if (d >= dist) continue;
                if (posicionarJunto(w, e)) dist = d, alvo_ = e;
                w.jogador = original;
            }
            verificar(alvo_ != entt::null && posicionarJunto(w, alvo_), "achou uma árvore acessível");
            return;
        }
        if (quadro_ % 24 == 0 && quadro_ <= 72) in.simularToque(SDL_SCANCODE_E);
        if (quadro_ == 54) capturar("04_cortando");
        if (quadro_ == 100) {
            verificar(w.reg.get<Coletavel>(alvo_).esgotado, "árvore virou toco após 3 golpes");
            verificar(w.inventario.quantidade("madeira") == 3, std::format("madeira = {} (esperado 3)", w.inventario.quantidade("madeira")));
            proxima();
        }
        return;
    case 3: {  // juntar o resto dos recursos para a casa
        const BuildingData* casa = w.reino.construcao("casa_herdeiro");
        if (w.inventario.tem(w.def(*casa)->nivel(2)->custo)) {
            capturar("05_recursos_juntados");
            proxima();
            return;
        }
        const bool feito = alvo_ == entt::null || !w.reg.valid(alvo_) ||
                           (w.reg.all_of<Coletavel>(alvo_) && w.reg.get<Coletavel>(alvo_).esgotado) ||
                           (w.reg.all_of<Saqueavel>(alvo_) && w.reg.get<Saqueavel>(alvo_).aberto);
        if (feito) {
            alvo_ = proximoAlvo(w);
            if (alvo_ == entt::null || !posicionarJunto(w, alvo_)) {
                verificar(false, "há recursos suficientes no mapa para a casa");
                terminar();
                return;
            }
            quadro_ = 0;
            return;
        }
        if (quadro_ % 22 == 0) in.simularToque(SDL_SCANCODE_E);
        return;
    }
    case 4:  // ir até a porta e abrir o painel
        if (quadro_ == 1) {
            const BuildingData* casa = w.reino.construcao("casa_herdeiro");
            w.jogador.pos = w.pontoPorta(*casa) + glm::vec2(0, 9);
            w.jogador.dir = Direcao::Cima;
        }
        if (quadro_ == 15) in.simularToque(SDL_SCANCODE_E);
        if (quadro_ == 30) {
            verificar(cena.painelAberto() == "casa_herdeiro", "E na porta abre o painel da construção");
            capturar("06_painel_casa");
            proxima();
        }
        return;
    case 5:  // confirmar a reconstrução
        if (quadro_ == 5) in.simularToque(SDL_SCANCODE_E);
        if (quadro_ == 20) capturar("07_reconstruindo");
        if (quadro_ == 120) {
            verificar(w.reino.construcao("casa_herdeiro")->level == 2, "casa subiu para o nível 2 (Casa Reconstruída)");
            verificar(ctx_.saves->existe(), "salvamento automático após a obra");
            capturar("08_casa_reconstruida");
            proxima();
        }
        return;
    case 6: {  // conversar com o Bram até o fim
        const entt::entity bram = w.morador("bram");
        if (quadro_ == 1) {
            verificar(bram != entt::null, "Bram existe no vilarejo");
            w.reg.get<Agenda>(bram).dormindo = false;
            w.jogador.pos = w.reg.get<Posicao>(bram).v + glm::vec2(0, 14);
            w.jogador.dir = Direcao::Cima;
            w.reg.get<Movimento>(bram).caminho.clear();
        }
        if (quadro_ == 10) in.simularToque(SDL_SCANCODE_E);
        if (quadro_ == 30) verificar(cena.conversa().has_value() && cena.conversa()->npc == "bram", "E perto do Bram abre a conversa");
        if (quadro_ == 120) capturar("08c_dialogo_bram");
        if (quadro_ > 130 && cena.conversa() && quadro_ % 12 == 0) in.simularToque(SDL_SCANCODE_E);
        if (quadro_ > 130 && !cena.conversa()) {
            const auto& rel = w.reg.get<Relacionamento>(bram);
            verificar(rel.afinidade == 25 && rel.conheceu, std::format("amizade com Bram subiu para {} na primeira conversa", rel.afinidade));
            verificar(!w.reg.get<Agenda>(bram).conversando, "Bram volta à rotina depois da conversa");
            proxima();
        }
        if (quadro_ > 600) {
            verificar(false, "conversa terminou");
            terminar();
        }
        return;
    }
    case 7:  // dormir pela casa: novo dia
        if (quadro_ == 1) {
            const BuildingData* casa = w.reino.construcao("casa_herdeiro");
            w.jogador.pos = w.pontoPorta(*casa) + glm::vec2(0, 9);
            w.jogador.dir = Direcao::Cima;
        }
        if (quadro_ == 10) in.simularToque(SDL_SCANCODE_E);
        if (quadro_ == 20) in.simularToque(SDL_SCANCODE_S);  // "Dormir até amanhã"
        if (quadro_ == 26) capturar("08d_painel_dormir");
        if (quadro_ == 30) in.simularToque(SDL_SCANCODE_E);
        if (quadro_ == 60) verificar(cena.emTransicao(), "a tela escurece ao dormir");
        if (quadro_ == 200) {
            verificar(w.relogio.dia == 2 && w.relogio.minutos < 6 * 60 + 30, std::format("acordou no dia {} às {}", w.relogio.dia, w.relogio.textoHora()));
            verificar(!w.reg.get<Relacionamento>(w.morador("bram")).falouHoje, "novo dia libera nova conversa");
            verificar(cena.relatorioAberto(), "de manhã aparece o relatório");
            capturar("08e_novo_dia");
        }
        if (quadro_ == 205) in.simularToque(SDL_SCANCODE_E);  // fecha o relatório
        if (quadro_ == 215) {
            inventarioSalvo_ = w.inventario.todos();
            esgotadosSalvos_ = contarEsgotados(w);
            proxima();
        }
        return;
    case 8:  // menu de pausa → voltar ao título (que também salva)
        if (quadro_ == 5) in.simularToque(SDL_SCANCODE_ESCAPE);
        // "Voltar ao título" é a 5ª opção (Continuar, Salvar, Opções, Controles, Voltar...)
        if (quadro_ == 10 || quadro_ == 13 || quadro_ == 16 || quadro_ == 19) in.simularToque(SDL_SCANCODE_S);
        if (quadro_ == 25) {
            verificar(cena.pausado(), "Esc abre o menu de pausa");
            capturar("08b_menu_pausa");
        }
        if (quadro_ == 30) {
            in.simularToque(SDL_SCANCODE_RETURN);
            fase_ = 1;
            etapa_ = 0;
            quadro_ = 0;
        }
        return;
    }
}

}
