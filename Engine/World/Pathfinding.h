#pragma once
#include <functional>
#include <vector>

#include <glm/vec2.hpp>

namespace ke {

// A* em grade de tiles (4 vizinhos). `bloqueado(x, y)` diz se o tile não pode ser pisado.
// Devolve os tiles do caminho sem o inicial; vazio se não houver caminho (ou já estiver no destino).
std::vector<glm::ivec2> buscarCaminho(int largura, int altura, glm::ivec2 inicio, glm::ivec2 fim,
                                      const std::function<bool(int, int)>& bloqueado, int limiteNos = 20000);

}
