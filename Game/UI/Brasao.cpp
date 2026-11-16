#include "Game/UI/Brasao.h"

#include <cmath>
#include <format>

namespace koa {

namespace {

const glm::vec4 CONTORNO{0.086f, 0.067f, 0.059f, 1.0f};

// Símbolo com contorno escuro (desenhado deslocado nos 4 lados) e a cor por cima
void simbolo(ke::Renderer2D& r, ke::ResourceManager& rec, const DadosCastelo& d, const Brasao& b, glm::vec2 pos, float escala, float alfa) {
    const ke::SpriteRegion& s = rec.regiao("simbolo_" + b.simbolo);
    for (const glm::vec2 o : {glm::vec2(-1, 0), glm::vec2(1, 0), glm::vec2(0, -1), glm::vec2(0, 1)})
        r.sprite(s, pos + o * escala, CONTORNO * glm::vec4(1, 1, 1, alfa), false, escala);
    glm::vec4 c = corTintura(d, b.corSimbolo);
    c.a = alfa;
    r.sprite(s, pos, c, false, escala);
}

}

glm::vec4 corTintura(const DadosCastelo& d, const std::string& id) {
    const Tintura* t = d.tintura(id);
    return t ? glm::vec4(t->cor, 1.0f) : glm::vec4(1.0f);
}

void desenharEscudo(ke::Renderer2D& r, ke::ResourceManager& rec, const DadosCastelo& d, const Brasao& b, glm::vec2 pos, float escala, float alfa) {
    const std::string base = std::format("escudo_{}_{}", b.forma, b.divisao);
    glm::vec4 c1 = corTintura(d, b.cor1), c2 = corTintura(d, b.cor2);
    c1.a = c2.a = alfa;
    r.sprite(rec.regiao(base + "_a"), pos, c1, false, escala);
    r.sprite(rec.regiao(base + "_b"), pos, c2, false, escala);
    r.sprite(rec.regiao(std::format("escudo_{}_borda", b.forma)), pos, {1, 1, 1, alfa}, false, escala);
    simbolo(r, rec, d, b, pos + glm::vec2(5.0f, 7.0f) * escala, escala, alfa);
}

void desenharPano(ke::Renderer2D& r, ke::ResourceManager& rec, const DadosCastelo& d, const Brasao& b, glm::vec2 pos, float escala) {
    r.sprite(rec.regiao(std::format("pano_{}_a", b.divisao)), pos, corTintura(d, b.cor1), false, escala);
    r.sprite(rec.regiao(std::format("pano_{}_b", b.divisao)), pos, corTintura(d, b.cor2), false, escala);
    r.sprite(rec.regiao("pano_borda"), pos, glm::vec4(1.0f), false, escala);
    simbolo(r, rec, d, b, pos + glm::vec2(1.0f, 5.0f) * escala, escala, 1.0f);
}

void desenharEstandarte(ke::Renderer2D& r, ke::ResourceManager& rec, const DadosCastelo& d, const Brasao& b, glm::vec2 base, float tempo) {
    const ke::SpriteRegion& m = rec.regiao("estandarte_mastro");
    const glm::vec2 topo = glm::floor(base - glm::vec2(1.0f, m.tamanho.y));
    r.sprite(m, topo);
    // O pano balança um pixel com o vento
    const float vento = std::round(std::sin(tempo * 2.2f + base.x * 0.13f) * 0.6f);
    desenharPano(r, rec, d, b, topo + glm::vec2(2.0f + vento, 4.0f));
}

}
