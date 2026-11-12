// Gera Assets/Sprites/atlas.png e atlas.json
// Uso: AssetGen <pasta Assets/Sprites> [--procedurais <pasta>]
// Se existir Assets/Arte/<nome>.png com o tamanho do sprite, ele entra no lugar do procedural.
// --procedurais grava cada sprite procedural em <pasta>/<nome>.png (base para gerar a arte final)
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>

#include <nlohmann/json.hpp>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include "Eras.h"
#include "Sprites.h"

namespace fs = std::filesystem;

namespace {

bool gravarPng(const fs::path& arquivo, const gen::Img& img) {
    return stbi_write_png(arquivo.string().c_str(), img.w, img.h, 4, img.p.data(), img.w * 4) != 0;
}

// Troca o sprite pela arte importada quando o PNG existe e tem exatamente o mesmo tamanho
bool importar(const fs::path& pastaArte, const std::string& nome, gen::Img& img) {
    const fs::path arquivo = pastaArte / (nome + ".png");
    if (!fs::exists(arquivo)) return false;
    int w = 0, h = 0, canais = 0;
    unsigned char* dados = stbi_load(arquivo.string().c_str(), &w, &h, &canais, 4);
    if (!dados) {
        std::fprintf(stderr, "aviso: %s ilegível, mantido o procedural\n", arquivo.string().c_str());
        return false;
    }
    const bool ok = w == img.w && h == img.h;
    if (ok) std::memcpy(img.p.data(), dados, img.p.size() * 4);
    else std::fprintf(stderr, "aviso: %s tem %dx%d, o sprite pede %dx%d; mantido o procedural\n", arquivo.string().c_str(), w, h, img.w, img.h);
    stbi_image_free(dados);
    return ok;
}

}

int main(int argc, char** argv) {
    fs::path saida = "Assets/Sprites", pastaProcedurais;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--procedurais") == 0 && i + 1 < argc) pastaProcedurais = argv[++i];
        else saida = argv[i];
    }
    fs::create_directories(saida);

    auto sprites = gen::gerarTodos();
    if (!pastaProcedurais.empty()) {
        fs::create_directories(pastaProcedurais);
        for (const auto& [nome, img] : sprites)
            if (!gravarPng(pastaProcedurais / (nome + ".png"), img)) std::fprintf(stderr, "falha ao gravar %s\n", nome.c_str());
    }
    // Arte final (NidraIA) em Assets/Arte, ao lado de Assets/Sprites
    const fs::path pastaArte = fs::absolute(saida).parent_path() / "Arte";
    int importados = 0;
    for (auto& [nome, img] : sprites) importados += importar(pastaArte, nome, img) ? 1 : 0;
    // Versões de cada era, derivadas da arte já importada (o estilo do NidraIA acompanha a evolução)
    auto eras = gen::gerarEras(sprites);
    const size_t qtdEras = eras.size();
    for (auto& e : eras) sprites.push_back(std::move(e));

    // Empacotamento em prateleiras, dos mais altos aos mais baixos (1 px de folga entre eles)
    std::vector<size_t> ordem(sprites.size());
    for (size_t i = 0; i < ordem.size(); ++i) ordem[i] = i;
    std::ranges::stable_sort(ordem, [&](size_t a, size_t b) { return sprites[a].second.h > sprites[b].second.h; });

    const int L = 1024;
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
    std::printf("%zu sprites (%d da arte importada, %zu de eras) -> %s (%dx%d)\n", sprites.size(), importados, qtdEras, png.c_str(), L, A);
    return 0;
}
