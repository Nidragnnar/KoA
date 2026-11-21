#pragma once
#include <array>
#include <map>
#include <string>

#include <SDL3/SDL_scancode.h>
#include <glm/vec2.hpp>

union SDL_Event;

namespace ke {

enum class BotaoMouse { Esquerdo = 0, Direito = 1, Meio = 2 };

// Teclado e mouse: "segurando" é o estado atual; "pressionou"/"clicou" acumulam até o próximo passo de lógica
class Input {
public:
    void processar(const SDL_Event& e);
    void fimDoPasso() {
        pressionou_.fill(false);
        clicou_.fill(false);
        moveu_ = false;
        roda_ = 0.0f;
        ultima_ = SDL_SCANCODE_UNKNOWN;
    }
    void soltarTudo() {
        segurando_.fill(false);
        pressionou_.fill(false);
        segurandoMouse_.fill(false);
        clicou_.fill(false);
    }

    bool segurando(SDL_Scancode t) const { return segurando_[t] || simuladoSeg_[t]; }
    bool pressionou(SDL_Scancode t) const { return pressionou_[t]; }

    // Ações com tecla trocável: o jogo dá nome e tecla a cada uma; trocar a tecla não muda o código que pergunta pela ação
    void mapear(const std::string& acao, SDL_Scancode t) { acoes_[acao] = t; }
    SDL_Scancode tecla(const std::string& acao, SDL_Scancode padrao = SDL_SCANCODE_UNKNOWN) const {
        const auto it = acoes_.find(acao);
        return it == acoes_.end() ? padrao : it->second;
    }
    bool pressionouAcao(const std::string& acao) const {
        const SDL_Scancode t = tecla(acao);
        return t != SDL_SCANCODE_UNKNOWN && pressionou_[t];
    }
    bool segurandoAcao(const std::string& acao) const {
        const SDL_Scancode t = tecla(acao);
        return t != SDL_SCANCODE_UNKNOWN && segurando(t);
    }
    const std::map<std::string, SDL_Scancode>& acoes() const { return acoes_; }
    // A última tecla apertada neste passo (para o jogador escolher uma tecla nova); UNKNOWN = nenhuma
    SDL_Scancode ultimaPressionada() const { return ultima_; }

    // Mouse em coordenadas da tela interna (640×360), já descontadas as faixas pretas e a escala
    glm::vec2 mouse() const { return mouse_; }
    bool mouseMoveu() const { return moveu_; }
    bool clicou(BotaoMouse b) const { return clicou_[static_cast<int>(b)]; }
    bool segurandoMouse(BotaoMouse b) const { return segurandoMouse_[static_cast<int>(b)]; }
    float roda() const { return roda_; }  // > 0 = para cima
    // Onde a tela interna aparece na janela (em pixels, a partir do canto de cima) e a densidade de pixels
    void definirTela(int x, int y, int largura, int altura, int baseL, int baseA, float densidade) {
        tela_ = {x, y, largura, altura, baseL, baseA};
        densidade_ = densidade > 0.0f ? densidade : 1.0f;
    }

    // Para roteiros de teste: aperta/segura teclas e clica como se fosse o jogador
    void simularToque(SDL_Scancode t) { pressionou_[t] = true, ultima_ = t; }
    void simularSegurar(SDL_Scancode t, bool s) { simuladoSeg_[t] = s; }
    void limparSimulacao() { simuladoSeg_.fill(false); }
    // Nos roteiros de teste, o mouse de verdade (a pessoa mexendo no computador) não interfere: só vale o simulado
    void ignorarMouseReal(bool i) { ignorarMouse_ = i; }
    void simularMouse(glm::vec2 p) { mouse_ = p, moveu_ = true; }
    void simularClique(BotaoMouse b, glm::vec2 p) { simularMouse(p), clicou_[static_cast<int>(b)] = true; }

private:
    glm::vec2 paraTela(float x, float y) const;

    std::array<bool, SDL_SCANCODE_COUNT> segurando_{};
    std::array<bool, SDL_SCANCODE_COUNT> pressionou_{};
    std::array<bool, SDL_SCANCODE_COUNT> simuladoSeg_{};
    std::array<bool, 3> segurandoMouse_{}, clicou_{};
    glm::vec2 mouse_{-1.0f};
    bool moveu_ = false;
    float roda_ = 0.0f;
    struct {
        int x = 0, y = 0, largura = 0, altura = 0, baseL = 640, baseA = 360;
    } tela_;
    float densidade_ = 1.0f;
    std::map<std::string, SDL_Scancode> acoes_;
    SDL_Scancode ultima_ = SDL_SCANCODE_UNKNOWN;
    bool ignorarMouse_ = false;
};

}
