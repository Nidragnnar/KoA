#pragma once
#include <optional>
#include <string>
#include <vector>

#include "Engine/Core/SceneManager.h"
#include "Engine/Renderer/Camera2D.h"
#include "Engine/Renderer/Lighting.h"
#include "Engine/Renderer/ParticleSystem.h"
#include "Game/App/Contexto.h"
#include "Game/Render/WorldRenderer.h"
#include "Game/UI/Menus.h"
#include "Game/World/World.h"

namespace koa {

// Tela de título: o vilarejo à noite ao fundo, menu principal e os 3 espaços de save
class TitleScene : public ke::Scene {
public:
    explicit TitleScene(Contexto& ctx);

    void atualizar(double dt) override;
    void renderizar(float alfa) override;
    ke::ColorGrade grade() const override { return gradeDaEra(0.0f); }

    // Para o roteiro de teste e para o menu
    bool temSave() const { return !maisRecente_.empty(); }
    void novoJogo(const std::string& slot);
    void continuar();  // o save mais recente

private:
    struct ResumoSlot {
        bool existe = false;
        std::string texto;  // "Dia 12 · Reino Renascido · 02:14 jogadas"
        std::string data;
    };
    std::vector<std::string> opcoes() const;
    void escolherMenu(const std::string& opcao);
    void carregarSlot(const std::string& slot);
    void lerResumos();
    void entradaSlots();

    Contexto& ctx_;
    World fundo_;
    WorldRenderer rend_;
    ke::Camera2D cam_;
    ke::ParticleSystem part_;
    ke::LightMap luz_;
    TelaOpcoes telaOpcoes_;
    int sel_ = 0;
    float tempo_ = 0.0f;
    std::string erro_;
    ResumoSlot resumos_[3];
    std::string maisRecente_;   // slot do save mais recente ("" = nenhum)
    enum class Slots { Fechado, Novo, Carregar } slots_ = Slots::Fechado;
    int selSlot_ = 0;
    bool confirmarSubstituir_ = false;
    Cliques cliques_;  // onde as opções apareceram no último quadro (mouse)
};

}
