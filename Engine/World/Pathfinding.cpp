#include "Engine/World/Pathfinding.h"

#include <algorithm>
#include <cstdlib>
#include <queue>

namespace ke {

std::vector<glm::ivec2> buscarCaminho(int L, int A, glm::ivec2 ini, glm::ivec2 fim, const std::function<bool(int, int)>& bloqueado,
                                      int limiteNos) {
    auto dentro = [&](glm::ivec2 p) { return p.x >= 0 && p.y >= 0 && p.x < L && p.y < A; };
    if (!dentro(ini) || !dentro(fim) || ini == fim) return {};
    const auto idx = [L](glm::ivec2 p) { return p.y * L + p.x; };
    const auto h = [&](glm::ivec2 p) { return std::abs(p.x - fim.x) + std::abs(p.y - fim.y); };

    std::vector<int> custo(static_cast<size_t>(L) * A, -1), veioDe(static_cast<size_t>(L) * A, -1);
    using No = std::pair<int, int>;  // (f, índice)
    std::priority_queue<No, std::vector<No>, std::greater<>> aberta;
    custo[idx(ini)] = 0;
    aberta.push({h(ini), idx(ini)});
    int visitados = 0;
    const glm::ivec2 dirs[4] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    while (!aberta.empty() && visitados++ < limiteNos) {
        const int atual = aberta.top().second;
        aberta.pop();
        const glm::ivec2 p{atual % L, atual / L};
        if (p == fim) {
            std::vector<glm::ivec2> caminho;
            for (int i = atual; i != idx(ini); i = veioDe[i]) caminho.push_back({i % L, i / L});
            std::ranges::reverse(caminho);
            return caminho;
        }
        for (const glm::ivec2 d : dirs) {
            const glm::ivec2 v = p + d;
            // O destino é aceito mesmo bloqueado (o NPC para ao lado da porta, por exemplo)
            if (!dentro(v) || (v != fim && bloqueado(v.x, v.y))) continue;
            const int c = custo[atual] + 1;
            if (custo[idx(v)] >= 0 && custo[idx(v)] <= c) continue;
            custo[idx(v)] = c;
            veioDe[idx(v)] = atual;
            aberta.push({c + h(v), idx(v)});
        }
    }
    return {};
}

}
