#pragma once
#include <string>
#include <vector>

namespace ke {

// Sequência de quadros nomeados (regiões do atlas) a uma taxa fixa
struct AnimationClip {
    std::vector<std::string> quadros;
    float fps = 8.0f;
    bool repetir = true;

    // Quadro a exibir após `tempo` segundos
    const std::string& quadro(float tempo) const;
    float duracao() const { return quadros.size() / fps; }
};

}
