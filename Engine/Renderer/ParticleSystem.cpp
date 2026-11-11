#include "Engine/Renderer/ParticleSystem.h"

#include <algorithm>

namespace ke {

void ParticleSystem::atualizar(float dt) {
    for (Particula& p : particulas_) {
        p.vel.y += p.gravidade * dt;
        p.vel *= std::max(0.0f, 1.0f - p.atrito * dt);
        p.pos += p.vel * dt;
        p.vida -= dt;
    }
    std::erase_if(particulas_, [](const Particula& p) { return p.vida <= 0.0f; });
}

}
