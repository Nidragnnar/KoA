#pragma once
#include <filesystem>
#include <memory>
#include <string>

#include <glm/vec4.hpp>

#include "Engine/Core/Application.h"
#include "Game/Data/GameData.h"
#include "Game/Kingdom/Kingdom.h"
#include "Game/Save/SaveSystem.h"

namespace koa {

class Roteiro;

constexpr const char* VERSAO_JOGO = "1.5.0";
constexpr const char* SLOTS[] = {"Save_001", "Save_002", "Save_003"};

// Preferências do jogador (config.json na pasta do usuário)
struct Opcoes {
    int musica = 6, efeitos = 8;  // 0..10
    bool telaCheia = false;
};

// O que as cenas compartilham
struct Contexto {
    ke::Application& app;
    GameData dados;
    std::filesystem::path pastaData;
    std::filesystem::path pastaUsuario;
    std::unique_ptr<SaveSystem> saves;
    std::string slot = "Save_001";  // espaço de save em uso
    Opcoes opcoes;
    Roteiro* roteiro = nullptr;  // teste automatizado (--roteiro)
    int codigoSaida = 0;

    void carregarOpcoes();
    void salvarOpcoes() const;
    void aplicarOpcoes();  // volume e tela cheia
};

// Cores da interface
namespace cor {
inline const glm::vec4 texto{0.92f, 0.88f, 0.80f, 1.0f};
inline const glm::vec4 apagado{0.55f, 0.52f, 0.48f, 1.0f};
inline const glm::vec4 ouro{0.95f, 0.78f, 0.36f, 1.0f};
inline const glm::vec4 bom{0.55f, 0.85f, 0.45f, 1.0f};
inline const glm::vec4 ruim{0.92f, 0.38f, 0.32f, 1.0f};
inline const glm::vec4 painel{0.07f, 0.06f, 0.06f, 0.88f};
inline const glm::vec4 borda{0.45f, 0.36f, 0.22f, 1.0f};
}

// Moldura de painel no estilo da interface
void desenharPainel(ke::Renderer2D& r, glm::vec2 pos, glm::vec2 tam);
// Ajuste de cor da tela para a era (contínua de 0 a 3)
ke::ColorGrade gradeDaEra(float eraVisual);
std::string agoraTexto();

}
