#pragma once
#include <array>
#include <string>
#include <unordered_map>

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
    // Luzes para o mapa de luz da noite (desenhar entre LightMap::comecar e terminar)
    void desenharLuzes(ke::Renderer2D& r, const World& w, const ke::Camera2D& cam, float tempo);
    // Marcador sobre o alvo de interação e a dica "[E] ..."
    void desenharAlvo(ke::Renderer2D& r, const ke::BitmapFont& f, const World& w, float tempo);

    const ke::SpriteRegion& spriteConstrucao(const World& w, const BuildingData& b);
    // Ponto do mundo de onde sai fumaça de chaminé (ou nenhum)
    bool chamine(const World& w, const BuildingData& b, glm::vec2& saida);

    // Versão do sprite para a era (cai para a era anterior se não houver)
    const ke::SpriteRegion& comEra(const std::string& nome, int era);
    // 1.20: "<estilo>_<nome>" quando existe (arte própria da região ou da Coroa Eterna); senão, a versão da era
    const ke::SpriteRegion& comEstilo(const std::string& estilo, const std::string& nome, int era);

private:
    using TabelaTiles = std::array<std::array<const ke::SpriteRegion*, 4>, T_QTD>;  // [terreno][variante]; nullptr = sem versão própria
    const TabelaTiles& tilesDoEstilo(const std::string& estilo);

    ke::ResourceManager& rec_;
    std::array<std::array<std::array<const ke::SpriteRegion*, 4>, 4>, T_QTD> tiles_{};  // [terreno][variante][era]
    std::unordered_map<std::string, std::array<const ke::SpriteRegion*, 4>> cacheEra_;
    std::unordered_map<std::string, TabelaTiles> tilesEstilo_;
    std::unordered_map<std::string, const ke::SpriteRegion*> cacheEstilo_;  // "estilo|nome" -> sprite próprio (nullptr = não tem)
};

}
