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
    if (++quadrosTotais_ > 60 * 320) {  // mais de 5 minutos de jogo simulado: algo travou
        verificar(false, "roteiro terminou dentro do tempo limite");
        terminar();
        return;
    }

    if (fase_ == 1) {
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
        case 24:
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
