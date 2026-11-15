#include "Engine/Core/Application.h"

#include <algorithm>

#include <SDL3/SDL.h>
#include <glad/glad.h>

#include "Engine/Core/Logger.h"
#include "Engine/Debug/Screenshot.h"

namespace ke {

Application::GuardaSdl::~GuardaSdl() {
    if (ativo) SDL_Quit();
}

Application::~Application() {
    debug_.encerrar();
    cenas_ = {};  // cenas podem guardar recursos de GPU: saem enquanto o contexto existe
}

bool Application::iniciar() {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_AUDIO)) {
        log::erro("SDL_Init: {}", SDL_GetError());
        return false;
    }
    sdl_.ativo = true;
    if (!janela_.criar(cfg_.titulo, cfg_.largura, cfg_.altura)) return false;
    recursos_.definirRaiz(cfg_.raizRecursos);
    const auto shaders = recursos_.caminho("Assets/Shaders");
    if (!renderer_.iniciar(shaders)) return false;
    if (!tela_.criar(cfg_.baseL, cfg_.baseA, shaders)) return false;
    fonte_.definirTextura(recursos_.textura("Assets/UI/fonte.png"));
    if (!debug_.iniciar(janela_.sdl(), janela_.contexto())) log::aviso("ImGui não iniciou (ferramentas de depuração indisponíveis)");
    audio_.abrir();  // sem som não é erro: o jogo segue mudo
    rodando_ = true;
    return true;
}

void Application::pedirCaptura(const std::filesystem::path& arquivo, std::function<void()> aoTerminar) {
    capturaPendente_ = arquivo;
    aposCaptura_ = std::move(aoTerminar);
}

void Application::executar() {
    uint64_t anterior = SDL_GetPerformanceCounter();
    const double freq = static_cast<double>(SDL_GetPerformanceFrequency());
    double acumFps = 0.0;
    int quadros = 0;
    // Desempenho no log a cada 10 s: FPS e tempo de CPU por quadro (sem a espera da sincronia vertical)
    double acumLog = 0.0, cpuSoma = 0.0, cpuPior = 0.0;
    int quadrosLog = 0;

    while (rodando_) {
        cenas_.aplicarTroca();
        Scene* cena = cenas_.atual();
        if (!cena) break;

        // Onde a tela interna está na janela (o letterbox é centralizado: a faixa de cima tem a altura da de baixo)
        const Retangulo& a = tela_.ultimaArea();
        input_.definirTela(a.x, a.y, a.largura, a.altura, cfg_.baseL, cfg_.baseA, SDL_GetWindowPixelDensity(janela_.sdl()));
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) rodando_ = false;
            debug_.processar(e);
            if (e.type == SDL_EVENT_KEY_DOWN && e.key.scancode == SDL_SCANCODE_F1 && !e.key.repeat) debug_.visivel = !debug_.visivel;
            if (e.type == SDL_EVENT_KEY_DOWN && e.key.scancode == SDL_SCANCODE_F11 && !e.key.repeat) janela_.alternarTelaCheia();
            if (e.type == SDL_EVENT_KEY_DOWN && e.key.scancode == SDL_SCANCODE_M && !e.key.repeat) audio_.alternarMudo();
            if (debug_.querTeclado() && (e.type == SDL_EVENT_KEY_DOWN || e.type == SDL_EVENT_KEY_UP)) continue;
            if (debug_.querMouse() && (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN || e.type == SDL_EVENT_MOUSE_WHEEL)) continue;
            input_.processar(e);
        }

        const uint64_t agora = SDL_GetPerformanceCounter();
        const double dt = (agora - anterior) / freq;
        anterior = agora;
        acumFps += dt;
        ++quadros;
        if (acumFps >= 0.5) {
            fps_ = quadros / acumFps;
            acumFps = 0.0;
            quadros = 0;
        }

        const int passos = passo_.avancar(dt);
        for (int i = 0; i < passos; ++i) {
            cena->atualizar(passo_.passo());
            tempo_ += passo_.passo();
            input_.fimDoPasso();
        }

        tela_.comecar();
        cena->renderizar(static_cast<float>(passo_.alfa()));
        const glm::ivec2 tam = janela_.tamanhoPixels();
        tela_.apresentar(tam, cena->grade(), static_cast<float>(tempo_));

        if (debug_.visivel) {
            debug_.novoQuadro();
            cena->interfaceDepuracao();
            debug_.renderizar();
        }

        if (!capturaPendente_.empty()) {
            salvarCaptura(capturaPendente_, tam.x, tam.y);
            capturaPendente_.clear();
            if (auto f = std::move(aposCaptura_)) f();
        }
        const double cpu = (SDL_GetPerformanceCounter() - agora) / freq * 1000.0;
        cpuSoma += cpu;
        cpuPior = std::max(cpuPior, cpu);
        ++quadrosLog;
        acumLog += dt;
        if (acumLog >= 10.0) {
            log::info("desempenho: {:.0f} FPS, CPU {:.2f} ms/quadro (pior {:.2f} ms)", quadrosLog / acumLog, cpuSoma / quadrosLog, cpuPior);
            acumLog = cpuSoma = cpuPior = 0.0;
            quadrosLog = 0;
        }
        janela_.trocarBuffers();
    }
}

}
