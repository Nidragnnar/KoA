#pragma once
#include <vector>

#include <glm/vec2.hpp>

#include "Engine/Platform/Input.h"

namespace koa {

// Áreas clicáveis registradas enquanto a interface é desenhada; o passo de lógica seguinte consulta.
// Assim o mouse usa exatamente o lugar onde cada opção apareceu na tela.
class Cliques {
public:
    static constexpr int PAINEL = 9999;  // o corpo do painel (clicar fora dele fecha)
    static constexpr int FECHAR = 9000;  // rodapé "fechar/voltar"

    void limpar() { areas_.clear(); }
    void area(glm::vec2 pos, glm::vec2 tam, int id) { areas_.push_back({pos, pos + tam, id}); }
    // A última registrada vence (fica por cima); -1 = nenhuma
    int em(glm::vec2 p) const {
        for (auto it = areas_.rbegin(); it != areas_.rend(); ++it)
            if (p.x >= it->min.x && p.y >= it->min.y && p.x < it->max.x && p.y < it->max.y) return it->id;
        return -1;
    }
    bool vazio() const { return areas_.empty(); }

private:
    struct Area {
        glm::vec2 min, max;
        int id;
    };
    std::vector<Area> areas_;
};

// Lista de opções (ids 0..n-1): o mouse passando por cima seleciona; o clique confirma. true = clicou numa opção
inline bool listaComMouse(const ke::Input& in, const Cliques& c, int& sel, int n) {
    const int id = c.em(in.mouse());
    if (id < 0 || id >= n) return false;
    if (in.mouseMoveu() || in.clicou(ke::BotaoMouse::Esquerdo)) sel = id;
    return in.clicou(ke::BotaoMouse::Esquerdo);
}

// Clique esquerdo fora do painel ou no rodapé "fechar"
inline bool cliqueFecha(const ke::Input& in, const Cliques& c) {
    if (!in.clicou(ke::BotaoMouse::Esquerdo) || c.vazio()) return false;
    const int id = c.em(in.mouse());
    return id < 0 || id == Cliques::FECHAR;
}

}
