// Gera Assets/Sprites/atlas.png e atlas.json; uso: AssetGen <pasta Assets/Sprites>
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>

#include <nlohmann/json.hpp>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include "Sprites.h"

int main(int argc, char** argv) {
    const std::filesystem::path saida = argc > 1 ? argv[1] : "Assets/Sprites";
    std::filesystem::create_directories(saida);

    auto sprites = gen::gerarTodos();
    // Empacotamento em prateleiras, dos mais altos aos mais baixos (1 px de folga entre eles)
    std::vector<size_t> ordem(sprites.size());
    for (size_t i = 0; i < ordem.size(); ++i) ordem[i] = i;
    std::ranges::stable_sort(ordem, [&](size_t a, size_t b) { return sprites[a].second.h > sprites[b].second.h; });

    const int L = 512;
    int x = 0, y = 0, alturaPrateleira = 0;
    nlohmann::json j = nlohmann::json::object();
    std::vector<std::tuple<size_t, int, int>> posicoes;
    for (size_t i : ordem) {
        const auto& img = sprites[i].second;
        if (x + img.w > L) {
            x = 0;
            y += alturaPrateleira + 1;
            alturaPrateleira = 0;
        }
        posicoes.emplace_back(i, x, y);
        j[sprites[i].first] = {{"x", x}, {"y", y}, {"w", img.w}, {"h", img.h}};
        x += img.w + 1;
        alturaPrateleira = std::max(alturaPrateleira, img.h);
    }
    int A = 64;
    while (A < y + alturaPrateleira) A *= 2;

    gen::Img atlas(L, A);
    for (const auto& [i, px, py] : posicoes) atlas.colar(sprites[i].second, px, py);
    const auto png = (saida / "atlas.png").string();
    if (!stbi_write_png(png.c_str(), L, A, 4, atlas.p.data(), L * 4)) {
        std::fprintf(stderr, "falha ao gravar %s\n", png.c_str());
        return 1;
    }
    std::ofstream(saida / "atlas.json") << j.dump(1);
    std::printf("%zu sprites -> %s (%dx%d)\n", sprites.size(), png.c_str(), L, A);
    return 0;
}
