#pragma once
#include <string>
#include <vector>

#include "Engine/Core/SceneManager.h"
#include "Engine/Renderer/Camera2D.h"
#include "Engine/Renderer/Lighting.h"
#include "Engine/Renderer/ParticleSystem.h"
#include "Game/App/Contexto.h"
#include "Game/Render/WorldRenderer.h"
#include "Game/World/World.h"

namespace koa {

// Tela de título: o vilarejo em ruínas ao fundo, com a câmera passeando
class TitleScene : public ke::Scene {
public:
    explicit TitleScene(Contexto& ctx);

    void atualizar(double dt) override;
    void renderizar(float alfa) override;
    ke::ColorGrade grade() const override { return gradeDaEra(0.0f); }

    void escolher(int opcao);  // 0 novo jogo, 1 continuar, 2 sair
    bool temSave() const { return temSave_; }

private:
    Contexto& ctx_;
    World fundo_;
    WorldRenderer rend_;
    ke::Camera2D cam_;
    ke::ParticleSystem part_;
    ke::LightMap luz_;
    int sel_ = 0;
    bool temSave_ = false;
    float tempo_ = 0.0f;
    std::string erro_;
};

}
