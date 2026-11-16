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
    std::string spriteNormal;      // para voltar quando renasce
    int diaEsgotado = 0;           // dia em que acabou (renasce alguns dias depois)

    int diasParaRenascer() const { return tipo == "arvore" ? 3 : tipo == "veio" ? 5 : 4; }
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
    bool festival = false;  // fase 10: só aparece durante o festival
};

// Estrela caída numa noite de meteoros (vale ouro; some no dia seguinte)
struct Meteorito {
    int dia = 0;  // a noite em que caiu
};

// Objeto que vira outro a partir de uma era (o poço vira fonte)
struct TrocaPorEra {
    int era = 2;
    std::string sprite;
};

// Fragmento da Queda das Cinzas no chão (carta, diário, inscrição): E para ler
struct Pista {
    std::string id;
};

// Fase 11: objeto que se usa com E ("altar" = Coração da Floresta, "entulho" = passagem das minas)
struct Interativo {
    std::string acao;
};

// Enfeite que só existe enquanto a região está transformada (some e volta com ela)
struct EnfeiteRegiao {};

// Fase 15: um canteiro da horta do herdeiro (o solo; a planta é outra entidade com PlantaCanteiro)
struct Canteiro {
    int indice = 0;
};
struct PlantaCanteiro {
    int indice = 0;
};

// Fase 15: galinha ou vaca, que passeia perto do galinheiro/curral
struct Animal {
    std::string tipo;   // "galinha" | "vaca"
    std::string chave;  // "<construção>#<n>" (o carinho do dia fica guardado por ela)
    glm::vec2 casa{0.0f}, destino{0.0f};
    float espera = 0.0f, tempoAnim = 0.0f;
    bool andando = false, olhaEsquerda = false;
};

// Fonte de luz (tochas)
struct Luz {
    glm::vec3 cor{1.0f, 0.6f, 0.3f};
    float raio = 48.0f;
    glm::vec2 desloc{0.0f, -14.0f};
};

}
