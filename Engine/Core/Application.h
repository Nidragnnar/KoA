#pragma once
#include <filesystem>
#include <functional>
#include <string>

#include "Engine/Core/SceneManager.h"
#include "Engine/Core/Time.h"
#include "Engine/Debug/DebugUI.h"
#include "Engine/Platform/Input.h"
#include "Engine/Platform/Window.h"
#include "Engine/Renderer/Camera2D.h"
#include "Engine/Renderer/RenderTarget.h"
#include "Engine/Renderer/Renderer2D.h"
#include "Engine/Resources/ResourceManager.h"
#include "Engine/UI/BitmapFont.h"

namespace ke {

struct AppConfig {
    std::string titulo = "Kingdom Engine";
    int largura = 1280, altura = 720;   // janela inicial
    int baseL = 640, baseA = 360;       // resolução interna
    std::filesystem::path raizRecursos;  // pasta com Assets/ e Data/
};

// Inicialização, laço principal (passo fixo de 60 Hz) e encerramento
class Application {
public:
    explicit Application(AppConfig cfg) : cfg_(std::move(cfg)) {}
    ~Application();

    bool iniciar();
    void executar();
    void parar() { rodando_ = false; }

    Window& janela() { return janela_; }
    Input& input() { return input_; }
    Renderer2D& renderer() { return renderer_; }
    RenderTarget& tela() { return tela_; }
    ResourceManager& recursos() { return recursos_; }
    SceneManager& cenas() { return cenas_; }
    DebugUI& depuracao() { return debug_; }
    BitmapFont& fonte() { return fonte_; }
    const AppConfig& config() const { return cfg_; }
    double tempo() const { return tempo_; }
    double fps() const { return fps_; }

    // Grava a próxima imagem apresentada (BMP) e chama `aoTerminar` depois
    void pedirCaptura(const std::filesystem::path& arquivo, std::function<void()> aoTerminar = {});

private:
    // A ordem importa: o que vem primeiro é destruído por último (SDL → janela/contexto GL → recursos GL)
    struct GuardaSdl {
        bool ativo = false;
        ~GuardaSdl();
    } sdl_;
    AppConfig cfg_;
    Window janela_;
    Input input_;
    Renderer2D renderer_;
    RenderTarget tela_;
    ResourceManager recursos_;
    SceneManager cenas_;
    DebugUI debug_;
    BitmapFont fonte_;
    FixedTimestep passo_;
    bool rodando_ = false;
    double tempo_ = 0.0, fps_ = 0.0;
    std::filesystem::path capturaPendente_;
    std::function<void()> aposCaptura_;
};

}
