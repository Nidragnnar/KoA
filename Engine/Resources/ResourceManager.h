#pragma once
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>

#include "Engine/Renderer/Texture.h"

namespace ke {

// Texturas e atlas carregados uma vez e compartilhados por nome
class ResourceManager {
public:
    void definirRaiz(const std::filesystem::path& raiz) { raiz_ = raiz; }
    const std::filesystem::path& raiz() const { return raiz_; }
    std::filesystem::path caminho(const std::string& relativo) const { return raiz_ / relativo; }

    const Texture* textura(const std::string& relativo);
    // atlas.png + atlas.json ({"nome": {"x","y","w","h"}})
    bool carregarAtlas(const std::string& png, const std::string& json);
    // Região do atlas; nome desconhecido devolve um quadrado magenta (e avisa uma vez no log)
    const SpriteRegion& regiao(const std::string& nome);
    bool temRegiao(const std::string& nome) const { return regioes_.contains(nome); }

private:
    std::filesystem::path raiz_;
    std::unordered_map<std::string, std::unique_ptr<Texture>> texturas_;
    std::unordered_map<std::string, SpriteRegion> regioes_;
    std::unordered_map<std::string, SpriteRegion> faltando_;
    Texture magenta_;
};

}
