#pragma once
#include <array>

#include "Engine/Renderer/Camera2D.h"
#include "Engine/Renderer/ParticleSystem.h"
#include "Engine/Renderer/Renderer2D.h"
#include "Engine/Resources/ResourceManager.h"
#include "Engine/UI/BitmapFont.h"
#include "Game/World/World.h"

namespace koa {

// Desenha o mundo: terreno por chunks visíveis, objetos ordenados pela base (y-sort), luz e névoa
class WorldRenderer {
public:
    explicit WorldRenderer(ke::ResourceManager& rec);

    void desenhar(ke::Renderer2D& r, const World& w, const ke::Camera2D& cam, float tempo, const ke::ParticleSystem& part,
                  bool mostrarColisoes);
    // Marcador sobre o alvo de interação e a dica "[E] ..."
    void desenharAlvo(ke::Renderer2D& r, const ke::BitmapFont& f, const World& w, float tempo);

    const ke::SpriteRegion& spriteConstrucao(const World& w, const BuildingData& b);
    // Ponto do mundo de onde sai fumaça de chaminé (ou nenhum)
    bool chamine(const World& w, const BuildingData& b, glm::vec2& saida);

private:
    ke::ResourceManager& rec_;
    std::array<std::array<const ke::SpriteRegion*, 4>, T_QTD> tiles_{};
};

}
