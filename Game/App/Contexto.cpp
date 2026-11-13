#include "Game/App/Contexto.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <format>

#include <glm/common.hpp>
#include <nlohmann/json.hpp>

#include "Engine/Platform/FileSystem.h"

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
    // Cinzas: verde fechado e sombrio (natureza engolindo ruínas) → Despertar → Renascido: quente → Alta Fantasia: vivo e luminoso
    static const ke::ColorGrade eras[4] = {
        {0.68f, 1.10f, -0.07f, {0.84f, 0.92f, 1.00f}, 0.62f},
        {0.97f, 1.05f, -0.01f, {0.98f, 0.98f, 0.98f}, 0.35f},
        {1.00f, 1.02f, 0.02f, {1.04f, 1.00f, 0.94f}, 0.22f},  // as cores da era já vêm dos sprites
        {1.10f, 1.04f, 0.04f, {1.05f, 1.02f, 0.98f}, 0.14f},
    };
    e = std::clamp(e, 0.0f, 3.0f);
    const int a = std::min(2, static_cast<int>(e));
    const float t = e - a;
    const ke::ColorGrade &x = eras[a], &y = eras[a + 1];
    return {std::lerp(x.saturacao, y.saturacao, t), std::lerp(x.contraste, y.contraste, t), std::lerp(x.brilho, y.brilho, t),
            glm::mix(x.tinta, y.tinta, t), std::lerp(x.vinheta, y.vinheta, t)};
}

void Contexto::carregarOpcoes() {
    const auto t = ke::fs::lerTexto(pastaUsuario / "config.json");
    if (!t) return;
    const auto j = nlohmann::json::parse(*t, nullptr, false);
    if (j.is_discarded()) return;
    opcoes.musica = std::clamp(j.value("musica", 6), 0, 10);
    opcoes.efeitos = std::clamp(j.value("efeitos", 8), 0, 10);
    opcoes.telaCheia = j.value("tela_cheia", false);
}

void Contexto::salvarOpcoes() const {
    const nlohmann::json j = {{"musica", opcoes.musica}, {"efeitos", opcoes.efeitos}, {"tela_cheia", opcoes.telaCheia}};
    ke::fs::gravarTextoSeguro(pastaUsuario / "config.json", j.dump(2));
}

void Contexto::aplicarOpcoes() {
    app.audio().definirVolumes(opcoes.musica / 10.0f * 0.9f, opcoes.efeitos / 10.0f);
    app.janela().definirTelaCheia(opcoes.telaCheia);
}

std::string agoraTexto() {
    const auto agora = std::chrono::zoned_time{std::chrono::current_zone(), std::chrono::system_clock::now()};
    return std::format("{:%d/%m/%Y %H:%M}", agora);
}

}
