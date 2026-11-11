#pragma once
#include <glm/vec3.hpp>

#include "Engine/Renderer/Texture.h"

namespace ke {

// Mapa de luz: começa na cor ambiente (noite = escuro azulado), as luzes somam brilho,
// e o resultado multiplica a cena. De dia o ambiente é branco e nada muda.
class LightMap {
public:
    ~LightMap();
    bool criar(int largura, int altura);
    void comecar(glm::vec3 ambiente);  // desenhe as luzes (mistura aditiva) depois disto
    void terminar();                   // volta ao framebuffer anterior
    const Texture& textura() const { return cor_; }

private:
    Texture cor_;
    unsigned fbo_ = 0;
    int anterior_ = 0;
    int viewportAnterior_[4] = {};
};

}
