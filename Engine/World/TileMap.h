#pragma once
#include <array>
#include <cstdint>
#include <vector>

namespace ke {

constexpr int CHUNK_TAMANHO = 32;  // tiles por lado de um chunk

struct Tile {
    uint16_t terreno = 0;  // índice do tipo de terreno (definido pelo jogo)
    uint8_t variante = 0;  // variação visual
    bool solido = false;   // bloqueia passagem (água, muro...)
};

// Bloco de 32×32 tiles: unidade de carga, salvamento e atualização do mundo
struct Chunk {
    std::array<Tile, CHUNK_TAMANHO * CHUNK_TAMANHO> tiles{};
    bool alterado = false;  // mudou desde o carregamento (só ele precisa ir ao save)
};

class TileMap {
public:
    TileMap() = default;
    TileMap(int largura, int altura, int ladoTile);

    int largura() const { return largura_; }
    int altura() const { return altura_; }
    int ladoTile() const { return ladoTile_; }
    int chunksX() const { return chunksX_; }
    int chunksY() const { return chunksY_; }

    bool dentro(int x, int y) const { return x >= 0 && y >= 0 && x < largura_ && y < altura_; }
    Tile& em(int x, int y);
    const Tile& em(int x, int y) const;
    // Fora do mapa conta como sólido: ninguém sai pela borda
    bool solido(int x, int y) const { return !dentro(x, y) || em(x, y).solido; }

    Chunk& chunk(int cx, int cy) { return chunks_[cy * chunksX_ + cx]; }
    const Chunk& chunk(int cx, int cy) const { return chunks_[cy * chunksX_ + cx]; }

private:
    int largura_ = 0, altura_ = 0, ladoTile_ = 16;
    int chunksX_ = 0, chunksY_ = 0;
    std::vector<Chunk> chunks_;
};

}
