#include "Engine/Renderer/Camera2D.h"

#include <algorithm>
#include <cmath>
#include <random>

#include <glm/gtc/matrix_transform.hpp>

namespace ke {

glm::vec2 Camera2D::limitar(glm::vec2 c) const {
    for (int i = 0; i < 2; ++i) {
        if (limite[i] <= 0.0f) continue;
        if (limite[i] <= vista[i]) c[i] = limite[i] * 0.5f;  // mundo menor que a tela: centraliza
        else c[i] = std::clamp(c[i], vista[i] * 0.5f, limite[i] - vista[i] * 0.5f);
    }
    return c;
}

void Camera2D::seguir(glm::vec2 alvo, float dt, float suavidade) {
    const float t = 1.0f - std::exp(-suavidade * dt);
    centro = limitar(centro + (alvo - centro) * t);
}

void Camera2D::posicionar(glm::vec2 alvo) { centro = limitar(alvo); }

void Camera2D::atualizarTremor(float dt) {
    static std::mt19937 rng{7};
    std::uniform_real_distribution<float> d(-1.0f, 1.0f);
    tremor = std::max(0.0f, tremor - dt * 12.0f);
    deslocTremor_ = tremor > 0.0f ? glm::vec2(d(rng), d(rng)) * tremor : glm::vec2(0.0f);
}

glm::vec2 Camera2D::canto() const {
    const glm::vec2 c = centro - vista * 0.5f + deslocTremor_;
    return {std::floor(c.x), std::floor(c.y)};
}

glm::mat4 Camera2D::projecao() const {
    const glm::vec2 c = canto();
    return glm::ortho(c.x, c.x + vista.x, c.y + vista.y, c.y, -1.0f, 1.0f);
}

}
