#pragma once
#include <glm/vec2.hpp>

#include "Engine/Renderer/Renderer2D.h"
#include "Engine/Resources/ResourceManager.h"
#include "Game/Kingdom/Castelo.h"

// Fase 16: o brasão montado com máscaras brancas tingidas (escudo_<forma>_<divisão>_a/b, simbolo_<id>, pano_<divisão>_a/b)
namespace koa {

inline constexpr glm::vec2 TAMANHO_ESCUDO{22.0f, 26.0f};
inline constexpr glm::vec2 TAMANHO_PANO{14.0f, 22.0f};

// Escudo com o canto superior esquerdo em `pos` (TAMANHO_ESCUDO × escala); `alfa` para fantasmas e prévias
void desenharEscudo(ke::Renderer2D& r, ke::ResourceManager& rec, const DadosCastelo& d, const Brasao& b, glm::vec2 pos, float escala = 1.0f,
                    float alfa = 1.0f);
// Pano de estandarte (TAMANHO_PANO × escala), com o símbolo no meio
void desenharPano(ke::Renderer2D& r, ke::ResourceManager& rec, const DadosCastelo& d, const Brasao& b, glm::vec2 pos, float escala = 1.0f);
// Estandarte inteiro (mastro + pano) com a base do mastro em `base`
void desenharEstandarte(ke::Renderer2D& r, ke::ResourceManager& rec, const DadosCastelo& d, const Brasao& b, glm::vec2 base, float tempo);

glm::vec4 corTintura(const DadosCastelo& d, const std::string& id);

}
