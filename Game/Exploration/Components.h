#pragma once
#include <string>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

// Componentes ECS (EnTT) dos objetos do mundo
namespace koa {

struct Posicao {
    glm::vec2 v{0.0f};  // base do objeto (onde toca o chão), em pixels
};

struct Colisor {
    glm::vec2 meia{4.0f};    // meia largura / meia altura
    glm::vec2 desloc{0.0f};  // centro em relação à base
};

struct Visual {
    std::string sprite;
    float tremor = 0.0f;  // sacudida ao levar golpe (segundos restantes)
    float piscar = 0.0f;  // clarão branco ao levar golpe
};

// Árvores e rochas: levam golpes e rendem recursos
struct Coletavel {
    std::string tipo;     // "arvore" | "rocha"
    std::string recurso;  // "madeira" | "pedra"
    int hp = 3, hpMax = 3;
    int rendimento = 3;
    bool esgotado = false;
    std::string spriteEsgotado;  // vazio = some
    glm::vec2 meiaEsgotado{0.0f};  // colisor que sobra (toco); zero = nenhum
};

// Baús e restos com ouro
struct Saqueavel {
    std::string recurso = "ouro";
    int quantidade = 8;
    bool aberto = false;
    std::string spriteAberto;
};

// Liga o objeto à construção do reino
struct ConstrucaoRef {
    std::string id;
};

// Identidade estável para o salvamento ("arvore_12_5")
struct IdMundo {
    std::string id;
};

// Sprite animado: base_0, base_1... trocados a `fps`
struct Animado {
    std::string base;
    int quadros = 1;
    float fps = 8.0f;
};

// Decoração grande (ruína do castelo): ocupa uma base de tiles
struct Decoracao {
    glm::ivec2 base{1};
};

// Enfeite que só aparece quando a era chega ao lugar dele (estandartes, lampiões, cristais)
struct Enfeite {
    int eraMin = 1;
};

// Objeto que vira outro a partir de uma era (o poço vira fonte)
struct TrocaPorEra {
    int era = 2;
    std::string sprite;
};

// Fonte de luz (tochas)
struct Luz {
    glm::vec3 cor{1.0f, 0.6f, 0.3f};
    float raio = 48.0f;
    glm::vec2 desloc{0.0f, -14.0f};
};

}
