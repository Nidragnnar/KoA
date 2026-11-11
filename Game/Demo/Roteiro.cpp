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
    if (fase_ == 0) {
        verificar(!t.temSave(), "pasta de saves de teste começa vazia");
        t.escolher(0);
    } else {
        verificar(t.temSave(), "título oferece Continuar depois de salvar");
        ctx_.app.input().simularToque(SDL_SCANCODE_RETURN);  // a opção já vem em "Continuar"
    }
    etapa_ = 0;
    quadro_ = 0;
}

void Roteiro::noMundo(WorldScene& cena) {
    if (aguardando_ || terminou_) return;
    World& w = cena.mundo();
    ke::Input& in = ctx_.app.input();
    ++quadro_;
    if (++quadrosTotais_ > 60 * 240) {  // 4 minutos de jogo simulado: algo travou
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
        case 4: case 5: case 6: {  // prévia das eras (só a cor da tela muda por enquanto)
            const int era = etapa_ - 3;
            if (quadro_ == 1) {
                w.reino.eraForcada = era;
                w.reino.eraVisual = static_cast<float>(era);
            }
            if (quadro_ == 20) {
                capturar(std::format("14_era_{}", era));
                proxima();
            }
            return;
        }
        case 7:
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
            capturar("08e_novo_dia");
            inventarioSalvo_ = w.inventario.todos();
            esgotadosSalvos_ = contarEsgotados(w);
            proxima();
        }
        return;
    case 8:  // menu de pausa → voltar ao título (que também salva)
        if (quadro_ == 5) in.simularToque(SDL_SCANCODE_ESCAPE);
        if (quadro_ == 10) in.simularToque(SDL_SCANCODE_S);
        if (quadro_ == 14) in.simularToque(SDL_SCANCODE_S);
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
