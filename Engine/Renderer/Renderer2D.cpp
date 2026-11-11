#include "Engine/Renderer/Renderer2D.h"

#include <glad/glad.h>

namespace ke {

namespace {
constexpr int MAX_QUADS = 16384;
}

Renderer2D::~Renderer2D() {
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (ebo_) glDeleteBuffers(1, &ebo_);
    if (vao_) glDeleteVertexArrays(1, &vao_);
}

bool Renderer2D::iniciar(const std::filesystem::path& pasta) {
    if (!shader_.carregar(pasta / "sprite.vert", pasta / "sprite.frag")) return false;
    const uint8_t branco[4] = {255, 255, 255, 255};
    branca_.criar(1, 1, branco);

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glGenBuffers(1, &ebo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertice) * 4 * MAX_QUADS, nullptr, GL_DYNAMIC_DRAW);
    std::vector<unsigned> idx(6 * MAX_QUADS);
    for (unsigned q = 0; q < MAX_QUADS; ++q) {
        const unsigned b = q * 4;
        unsigned* i = &idx[q * 6];
        i[0] = b; i[1] = b + 1; i[2] = b + 2; i[3] = b; i[4] = b + 2; i[5] = b + 3;
    }
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * sizeof(unsigned), idx.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertice), reinterpret_cast<void*>(offsetof(Vertice, pos)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertice), reinterpret_cast<void*>(offsetof(Vertice, uv)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertice), reinterpret_cast<void*>(offsetof(Vertice, cor)));
    glBindVertexArray(0);
    vertices_.reserve(4 * MAX_QUADS);
    return true;
}

void Renderer2D::comecar(const glm::mat4& projecao) {
    chamadas_ = 0;
    shader_.usar();
    shader_.definir("uProjecao", projecao);
    shader_.definir("uTextura", 0);
    glEnable(GL_BLEND);
    mistura_ = Mistura::Normal;
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    vertices_.clear();
    texturaAtual_ = nullptr;
}

void Renderer2D::terminar() {
    descarregar();
    chamadasUltimo_ = chamadas_;
}

void Renderer2D::descarregar() {
    if (vertices_.empty() || !texturaAtual_) return;
    texturaAtual_->vincular(0);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferSubData(GL_ARRAY_BUFFER, 0, vertices_.size() * sizeof(Vertice), vertices_.data());
    glDrawElements(GL_TRIANGLES, static_cast<int>(vertices_.size() / 4 * 6), GL_UNSIGNED_INT, nullptr);
    vertices_.clear();
    ++chamadas_;
}

void Renderer2D::definirMistura(Mistura m) {
    if (m == mistura_) return;
    descarregar();
    mistura_ = m;
    if (m == Mistura::Aditiva) glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    else if (m == Mistura::Multiplicar) glBlendFunc(GL_DST_COLOR, GL_ZERO);
    else glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void Renderer2D::quad(const Texture& t, glm::vec2 p, glm::vec2 s, glm::vec4 uv, glm::vec4 c) {
    if (texturaAtual_ != &t || vertices_.size() >= 4 * MAX_QUADS) {
        descarregar();
        texturaAtual_ = &t;
    }
    vertices_.push_back({p, {uv.x, uv.y}, c});
    vertices_.push_back({{p.x + s.x, p.y}, {uv.z, uv.y}, c});
    vertices_.push_back({p + s, {uv.z, uv.w}, c});
    vertices_.push_back({{p.x, p.y + s.y}, {uv.x, uv.w}, c});
}

void Renderer2D::sprite(const SpriteRegion& r, glm::vec2 pos, glm::vec4 cor, bool espelhar, float escala) {
    if (!r.textura) return;
    glm::vec4 uv = r.uv;
    if (espelhar) std::swap(uv.x, uv.z);
    quad(*r.textura, pos, glm::vec2(r.tamanho) * escala, uv, cor);
}

void Renderer2D::retangulo(glm::vec2 pos, glm::vec2 tam, glm::vec4 cor) { quad(branca_, pos, tam, {0, 0, 1, 1}, cor); }

void Renderer2D::contorno(glm::vec2 p, glm::vec2 s, glm::vec4 c) {
    retangulo(p, {s.x, 1}, c);
    retangulo({p.x, p.y + s.y - 1}, {s.x, 1}, c);
    retangulo(p, {1, s.y}, c);
    retangulo({p.x + s.x - 1, p.y}, {1, s.y}, c);
}

}
