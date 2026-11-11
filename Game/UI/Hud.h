#pragma once
#include <map>
#include <string>
#include <vector>

#include "Engine/Renderer/Renderer2D.h"
#include "Engine/Resources/ResourceManager.h"
#include "Engine/UI/BitmapFont.h"
#include "Game/World/World.h"

namespace koa {

// Interface na tela (coordenadas da tela interna 640×360)
class Hud {
public:
    Hud(ke::ResourceManager& rec, const ke::BitmapFont& f, const GameData& dados) : rec_(rec), f_(f), dados_(dados) {}

    void atualizar(float dt);
    void aviso(std::string texto, glm::vec4 cor);
    void pulsar(const std::string& recurso) { pulso_[recurso] = 0.4f; }

    void desenhar(ke::Renderer2D& r, const World& w, float tempo);
    void painelConstrucao(ke::Renderer2D& r, const World& w, const std::string& id, const std::vector<std::string>& opcoes, int sel,
                          float tempo);
    void menuPausa(ke::Renderer2D& r, const std::vector<std::string>& opcoes, int selecionada);
    // `letras`: quantos caracteres da fala atual já apareceram
    void caixaDialogo(ke::Renderer2D& r, const Conversa& c, int linha, int letras, float tempo);
    static int tamanhoUtf8(const std::string& s);

private:
    struct Aviso {
        std::string texto;
        glm::vec4 cor;
        float vida;
    };
    ke::ResourceManager& rec_;
    const ke::BitmapFont& f_;
    const GameData& dados_;
    std::vector<Aviso> avisos_;
    std::map<std::string, float> pulso_;
    float tempoDica_ = 12.0f;
};

}
