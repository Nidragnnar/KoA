#include "Engine/World/TileMap.h"

namespace ke {

TileMap::TileMap(int largura, int altura, int ladoTile)
    : largura_(largura), altura_(altura), ladoTile_(ladoTile),
      chunksX_((largura + CHUNK_TAMANHO - 1) / CHUNK_TAMANHO),
      chunksY_((altura + CHUNK_TAMANHO - 1) / CHUNK_TAMANHO),
      chunks_(static_cast<size_t>(chunksX_) * chunksY_) {}

Tile& TileMap::em(int x, int y) {
    return chunk(x / CHUNK_TAMANHO, y / CHUNK_TAMANHO).tiles[(y % CHUNK_TAMANHO) * CHUNK_TAMANHO + x % CHUNK_TAMANHO];
}

const Tile& TileMap::em(int x, int y) const {
    return chunk(x / CHUNK_TAMANHO, y / CHUNK_TAMANHO).tiles[(y % CHUNK_TAMANHO) * CHUNK_TAMANHO + x % CHUNK_TAMANHO];
}

}
