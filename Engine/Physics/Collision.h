#pragma once
#include <span>

#include <glm/vec2.hpp>

#include "Engine/World/TileMap.h"

namespace ke {

struct AABB {
    glm::vec2 min{0.0f}, max{0.0f};
    bool intersecta(const AABB& o) const { return min.x < o.max.x && max.x > o.min.x && min.y < o.max.y && max.y > o.min.y; }
    AABB deslocada(glm::vec2 d) const { return {min + d, max + d}; }
};

// A caixa bate em algum tile sólido ou obstáculo?
bool colide(const TileMap& mapa, const AABB& caixa, std::span<const AABB> obstaculos);

// Move eixo por eixo, parando encostado no que bloqueia; desliza em quinas de até `folgaQuina` px.
// Devolve o deslocamento realmente aplicado.
glm::vec2 moverComColisao(const TileMap& mapa, const AABB& caixa, glm::vec2 delta, std::span<const AABB> obstaculos,
                          float folgaQuina = 5.0f);

}
