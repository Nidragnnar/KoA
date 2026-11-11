#include "Game/App/Contexto.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <format>

#include <glm/common.hpp>

namespace koa {

void desenharPainel(ke::Renderer2D& r, glm::vec2 p, glm::vec2 s) {
    r.retangulo(p, s, cor::painel);
    r.contorno(p, s, cor::borda);
    r.contorno(p + 1.0f, s - 2.0f, {0.18f, 0.14f, 0.10f, 1.0f});
    // Cantos de ouro velho
    for (const glm::vec2 c : {p, glm::vec2{p.x + s.x - 3, p.y}, glm::vec2{p.x, p.y + s.y - 3}, p + s - 3.0f})
        r.retangulo(c, {3, 3}, cor::ouro);
}

ke::ColorGrade gradeDaEra(float e) {
    // Cinzas: frio e sem cor → Despertar → Renascido: quente → Alta Fantasia: vivo e luminoso
    static const ke::ColorGrade eras[4] = {
        {0.55f, 1.06f, -0.03f, {0.86f, 0.92f, 1.04f}, 0.60f},
        {0.80f, 1.04f, 0.00f, {0.97f, 0.97f, 1.00f}, 0.38f},
        {1.05f, 1.02f, 0.02f, {1.05f, 1.00f, 0.92f}, 0.22f},
        {1.28f, 1.05f, 0.04f, {1.06f, 1.02f, 0.97f}, 0.15f},
    };
    e = std::clamp(e, 0.0f, 3.0f);
    const int a = std::min(2, static_cast<int>(e));
    const float t = e - a;
    const ke::ColorGrade &x = eras[a], &y = eras[a + 1];
    return {std::lerp(x.saturacao, y.saturacao, t), std::lerp(x.contraste, y.contraste, t), std::lerp(x.brilho, y.brilho, t),
            glm::mix(x.tinta, y.tinta, t), std::lerp(x.vinheta, y.vinheta, t)};
}

std::string agoraTexto() {
    const auto agora = std::chrono::zoned_time{std::chrono::current_zone(), std::chrono::system_clock::now()};
    return std::format("{:%d/%m/%Y %H:%M}", agora);
}

}
