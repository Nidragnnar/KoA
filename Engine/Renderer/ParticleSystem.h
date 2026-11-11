#pragma once
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

namespace ke {

struct Particula {
    glm::vec2 pos{0.0f}, vel{0.0f};
    glm::vec4 cor{1.0f};
    float vida = 1.0f, vidaMax = 1.0f;
    float tamanho = 1.0f;
    float gravidade = 0.0f;
    float atrito = 0.0f;   // fração da velocidade perdida por segundo
    bool aditiva = false;  // brilho (somado à cena)
};

// Só a simulação; quem desenha é o jogo (cada partícula vira um quadrado)
class ParticleSystem {
public:
    void emitir(const Particula& p) { if (particulas_.size() < 4000) particulas_.push_back(p); }
    void atualizar(float dt);
    const std::vector<Particula>& particulas() const { return particulas_; }
    void limpar() { particulas_.clear(); }

private:
    std::vector<Particula> particulas_;
};

}
