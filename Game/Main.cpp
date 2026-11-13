// Kingdom of Ashes: The Last Crown — protótipo 0.1 "The Forgotten Kingdom"
// Argumentos: --saves <pasta>   usa outra pasta de saves (testes não mexem nos saves do jogador)
//             --roteiro <pasta> joga sozinho o caminho do protótipo, tira capturas e confere tudo
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <string>

#include <stb_image.h>

#include "Engine/Core/Application.h"
#include "Engine/Core/Logger.h"
#include "Game/App/Contexto.h"
#include "Game/Demo/Roteiro.h"
#include "Game/Scenes/TitleScene.h"

namespace {

// Procura a pasta com Data/ e Assets/: ao lado do exe (pacote) ou subindo até a raiz do projeto (desenvolvimento)
std::filesystem::path localizarRaiz() {
    const char* base = SDL_GetBasePath();
    std::filesystem::path p = base ? std::filesystem::path(reinterpret_cast<const char8_t*>(base)) : std::filesystem::current_path();
    for (int i = 0; i < 6; ++i) {
        std::error_code ec;
        if (std::filesystem::exists(p / "Data/World/vilarejo.json", ec) && std::filesystem::exists(p / "Assets/Sprites/atlas.png", ec))
            return p;
        if (!p.has_parent_path() || p.parent_path() == p) break;
        p = p.parent_path();
    }
    return base ? std::filesystem::path(reinterpret_cast<const char8_t*>(base)) : std::filesystem::current_path();
}

std::filesystem::path pastaUsuario() {
    char* pref = SDL_GetPrefPath("Nidragnnar", "KingdomOfAshes");
    std::filesystem::path p = pref ? std::filesystem::path(reinterpret_cast<const char8_t*>(pref)) : std::filesystem::current_path();
    SDL_free(pref);
    return p;
}

}

int main(int argc, char* argv[]) {
    std::filesystem::path pastaSaves, pastaRoteiro;
    for (int i = 1; i + 1 < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--saves") pastaSaves = std::filesystem::path(reinterpret_cast<const char8_t*>(argv[++i]));
        else if (a == "--roteiro") pastaRoteiro = std::filesystem::path(reinterpret_cast<const char8_t*>(argv[++i]));
    }
    const std::filesystem::path usuario = pastaUsuario();
    if (pastaSaves.empty()) pastaSaves = usuario / "Saves";
    ke::log::init((pastaRoteiro.empty() ? usuario : pastaRoteiro) / "kingdom.log");

    ke::AppConfig cfg;
    cfg.titulo = "Kingdom of Ashes: The Last Crown";
    cfg.raizRecursos = localizarRaiz();
    ke::log::info("Kingdom of Ashes {} | recursos em {} | saves em {}", koa::VERSAO_JOGO, cfg.raizRecursos.string(), pastaSaves.string());

    ke::Application app(cfg);
    if (!app.iniciar()) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Kingdom of Ashes", "Não foi possível iniciar o jogo (veja kingdom.log).", nullptr);
        return 1;
    }
    if (!app.recursos().carregarAtlas("Assets/Sprites/atlas.png", "Assets/Sprites/atlas.json")) return 1;

    koa::Contexto ctx{app};
    ctx.pastaData = cfg.raizRecursos / "Data";
    ctx.saves = std::make_unique<koa::SaveSystem>(pastaSaves);
    // Preferências ao lado dos saves (num teste, na pasta do teste: nunca mexe nas do jogador)
    ctx.pastaUsuario = pastaSaves.parent_path();
    ctx.carregarOpcoes();
    ctx.aplicarOpcoes();
    // Ícone da janela (a coroa)
    {
        int w = 0, h = 0, canais = 0;
        const std::string icone = (cfg.raizRecursos / "Assets/UI/icone.png").string();
        if (unsigned char* px = stbi_load(icone.c_str(), &w, &h, &canais, 4)) {
            if (SDL_Surface* s = SDL_CreateSurfaceFrom(w, h, SDL_PIXELFORMAT_ABGR8888, px, w * 4)) {
                SDL_SetWindowIcon(app.janela().sdl(), s);
                SDL_DestroySurface(s);
            }
            stbi_image_free(px);
        }
    }
    std::string erro;
    if (!ctx.dados.carregar(ctx.pastaData, &erro)) {
        ke::log::erro("dados: {}", erro);
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Kingdom of Ashes", ("Dados do jogo inválidos: " + erro).c_str(), nullptr);
        return 1;
    }
    std::unique_ptr<koa::Roteiro> roteiro;
    if (!pastaRoteiro.empty()) {
        roteiro = std::make_unique<koa::Roteiro>(ctx, pastaRoteiro);
        ctx.roteiro = roteiro.get();
    }

    app.cenas().trocar(std::make_unique<koa::TitleScene>(ctx));
    app.executar();
    ke::log::info("encerrado (código {})", ctx.codigoSaida);
    return ctx.codigoSaida;
}
