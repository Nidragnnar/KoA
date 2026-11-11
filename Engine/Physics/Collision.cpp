#include "Engine/Physics/Collision.h"

#include <cmath>

namespace ke {

bool colide(const TileMap& mapa, const AABB& caixa, std::span<const AABB> obstaculos) {
    const float lado = static_cast<float>(mapa.ladoTile());
    const int x0 = static_cast<int>(std::floor(caixa.min.x / lado));
    const int y0 = static_cast<int>(std::floor(caixa.min.y / lado));
    const int x1 = static_cast<int>(std::floor((caixa.max.x - 0.001f) / lado));
    const int y1 = static_cast<int>(std::floor((caixa.max.y - 0.001f) / lado));
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x)
            if (mapa.solido(x, y)) return true;
    for (const AABB& o : obstaculos)
        if (caixa.intersecta(o)) return true;
    return false;
}

namespace {

// Anda num eixo em passos de no máximo 1 px, parando antes de entrar em algo sólido
float moverEixo(const TileMap& mapa, AABB& caixa, float d, int eixo, std::span<const AABB> obst) {
    float feito = 0.0f;
    const float sinal = d > 0 ? 1.0f : -1.0f;
    float resta = std::abs(d);
    while (resta > 0.0f) {
        const float passo = std::min(resta, 1.0f) * sinal;
        glm::vec2 v{0.0f};
        v[eixo] = passo;
        const AABB nova = caixa.deslocada(v);
        if (colide(mapa, nova, obst)) break;
        caixa = nova;
        feito += passo;
        resta -= std::abs(passo);
    }
    return feito;
}

}

glm::vec2 moverComColisao(const TileMap& mapa, const AABB& caixaInicial, glm::vec2 delta, std::span<const AABB> obst,
                          float folgaQuina) {
    AABB caixa = caixaInicial;
    glm::vec2 feito{0.0f};
    for (int eixo = 0; eixo < 2; ++eixo) {
        if (delta[eixo] == 0.0f) continue;
        feito[eixo] = moverEixo(mapa, caixa, delta[eixo], eixo, obst);
        // Bloqueado andando reto: se a passagem está logo ao lado, empurra 1 px para a quina (como em jogos de tiles)
        const int outro = 1 - eixo;
        if (std::abs(feito[eixo]) < std::abs(delta[eixo]) * 0.5f && delta[outro] == 0.0f) {
            for (int k = 1; k <= static_cast<int>(folgaQuina); ++k) {
                for (float s : {-1.0f, 1.0f}) {
                    glm::vec2 lado{0.0f};
                    lado[outro] = s * k;
                    glm::vec2 frente{0.0f};
                    frente[eixo] = delta[eixo] > 0 ? 1.0f : -1.0f;
                    if (!colide(mapa, caixa.deslocada(lado), obst) && !colide(mapa, caixa.deslocada(lado + frente), obst)) {
                        glm::vec2 empurra{0.0f};
                        empurra[outro] = s * std::min(1.0f, std::abs(delta[eixo]));
                        caixa = caixa.deslocada(empurra);
                        feito += empurra;
                        goto proximoEixo;
                    }
                }
            }
        }
    proximoEixo:;
    }
    return feito;
}

}
