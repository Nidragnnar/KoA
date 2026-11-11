#include "Engine/Resources/ResourceManager.h"

#include <nlohmann/json.hpp>

#include "Engine/Core/Logger.h"
#include "Engine/Platform/FileSystem.h"

namespace ke {

const Texture* ResourceManager::textura(const std::string& relativo) {
    if (auto it = texturas_.find(relativo); it != texturas_.end()) return it->second.get();
    auto t = std::make_unique<Texture>();
    if (!t->carregar(caminho(relativo))) return nullptr;
    return texturas_.emplace(relativo, std::move(t)).first->second.get();
}

bool ResourceManager::carregarAtlas(const std::string& png, const std::string& json) {
    const Texture* t = textura(png);
    const auto texto = fs::lerTexto(caminho(json));
    if (!t || !texto) {
        log::erro("atlas não encontrado: {}", json);
        return false;
    }
    const auto j = nlohmann::json::parse(*texto, nullptr, false);
    if (j.is_discarded()) {
        log::erro("atlas com JSON inválido: {}", json);
        return false;
    }
    const float L = static_cast<float>(t->largura()), A = static_cast<float>(t->altura());
    for (const auto& [nome, r] : j.items()) {
        const int x = r.value("x", 0), y = r.value("y", 0), w = r.value("w", 0), h = r.value("h", 0);
        regioes_[nome] = SpriteRegion{t, {x / L, y / A, (x + w) / L, (y + h) / A}, {w, h}};
    }
    log::info("atlas {}: {} regiões", png, regioes_.size());
    return true;
}

const SpriteRegion& ResourceManager::regiao(const std::string& nome) {
    if (auto it = regioes_.find(nome); it != regioes_.end()) return it->second;
    if (auto it = faltando_.find(nome); it != faltando_.end()) return it->second;
    log::aviso("sprite inexistente no atlas: {}", nome);
    if (!magenta_.id()) {
        const uint8_t px[4] = {255, 0, 255, 255};
        magenta_.criar(1, 1, px);
    }
    return faltando_[nome] = SpriteRegion{&magenta_, {0, 0, 1, 1}, {16, 16}};
}

}
