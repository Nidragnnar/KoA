#pragma once
#include <optional>
#include <random>
#include <string>
#include <vector>

#include "Engine/Core/SceneManager.h"
#include "Engine/Renderer/Camera2D.h"
#include "Engine/Renderer/ParticleSystem.h"
#include "Game/App/Contexto.h"
#include "Game/Render/WorldRenderer.h"
#include "Game/UI/Hud.h"
#include "Game/World/World.h"

namespace koa {

// O vilarejo jogável
class WorldScene : public ke::Scene {
public:
    WorldScene(Contexto& ctx, std::optional<DadosSave> save);

    void atualizar(double dt) override;
    void renderizar(float alfa) override;
    void interfaceDepuracao() override;
    ke::ColorGrade grade() const override { return gradeDaEra(mundo_.reino.eraVisual); }

    World& mundo() { return mundo_; }
    bool carregou() const { return carregou_; }
    const std::string& painelAberto() const { return painel_; }
    bool pausado() const { return pausa_; }
    bool salvar();

private:
    void entradaJogo();
    void entradaPainel();
    void entradaPausa();
    void tratarEventos();
    void efeitosAmbiente(float dt);
    void poeira(glm::vec2 centro, glm::vec2 meia, int n, glm::vec4 cor);
    void textoFlutuante(glm::vec2 pos, std::string t, glm::vec4 c) { flutuantes_.push_back({pos, std::move(t), c, 1.4f}); }

    Contexto& ctx_;
    World mundo_;
    WorldRenderer rend_;
    Hud hud_;
    ke::Camera2D cam_;
    ke::ParticleSystem part_;
    std::mt19937 rng_{42};

    bool carregou_ = false;
    std::string painel_;  // id da construção com o painel aberto ("" = fechado)
    bool pausa_ = false;
    int selPausa_ = 0;
    bool mostrarColisoes_ = false;
    float tempo_ = 0.0f, flash_ = 0.0f, tempoFumaca_ = 0.0f, tempoCinzas_ = 0.0f;

    struct Flutuante {
        glm::vec2 pos;
        std::string texto;
        glm::vec4 cor;
        float vida;
    };
    std::vector<Flutuante> flutuantes_;
};

}
