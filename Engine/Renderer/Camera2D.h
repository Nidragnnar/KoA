#pragma once
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>

namespace ke {

// Câmera de vista fixa (resolução interna) que segue um alvo e não mostra além do mapa
class Camera2D {
public:
    glm::vec2 centro{0.0f};
    glm::vec2 vista{640.0f, 360.0f};
    glm::vec2 limite{0.0f};  // tamanho do mundo em pixels (0 = sem limite)
    float tremor = 0.0f;     // intensidade de sacudida (decai sozinha)

    void seguir(glm::vec2 alvo, float dt, float suavidade = 10.0f);
    void posicionar(glm::vec2 alvo);
    void atualizarTremor(float dt);
    // Canto superior esquerdo em pixels inteiros (pixel-perfect)
    glm::vec2 canto() const;
    glm::mat4 projecao() const;

private:
    glm::vec2 limitar(glm::vec2 c) const;
    glm::vec2 deslocTremor_{0.0f};
};

}
