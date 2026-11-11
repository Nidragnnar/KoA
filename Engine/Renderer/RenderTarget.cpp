#include "Engine/Renderer/RenderTarget.h"

#include <algorithm>

#include <glad/glad.h>

#include "Engine/Core/Logger.h"

namespace ke {

Retangulo calcularLetterbox(int janL, int janA, int baseL, int baseA) {
    if (janL <= 0 || janA <= 0 || baseL <= 0 || baseA <= 0) return {};
    const int escala = std::min(janL / baseL, janA / baseA);
    int l, a;
    if (escala >= 1) {
        l = baseL * escala;
        a = baseA * escala;
    } else if (static_cast<long long>(janL) * baseA > static_cast<long long>(janA) * baseL) {
        a = janA;
        l = static_cast<int>(static_cast<long long>(janA) * baseL / baseA);
    } else {
        l = janL;
        a = static_cast<int>(static_cast<long long>(janL) * baseA / baseL);
    }
    return {(janL - l) / 2, (janA - a) / 2, l, a};
}

RenderTarget::~RenderTarget() {
    if (fbo_) glDeleteFramebuffers(1, &fbo_);
    if (cor_) glDeleteTextures(1, &cor_);
    if (vao_) glDeleteVertexArrays(1, &vao_);
}

bool RenderTarget::criar(int largura, int altura, const std::filesystem::path& pasta) {
    largura_ = largura;
    altura_ = altura;
    glGenTextures(1, &cor_);
    glBindTexture(GL_TEXTURE_2D, cor_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, largura, altura, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenFramebuffers(1, &fbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, cor_, 0);
    const bool ok = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (!ok) {
        log::erro("framebuffer da tela interna incompleto");
        return false;
    }
    glGenVertexArrays(1, &vao_);  // o triângulo de tela cheia sai do gl_VertexID
    return posProcesso_.carregar(pasta / "tela.vert", pasta / "tela.frag");
}

void RenderTarget::comecar() {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glViewport(0, 0, largura_, altura_);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void RenderTarget::apresentar(glm::ivec2 janela, const ColorGrade& g, float tempo) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, janela.x, janela.y);
    glClearColor(0.02f, 0.02f, 0.025f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    area_ = calcularLetterbox(janela.x, janela.y, largura_, altura_);
    glViewport(area_.x, area_.y, area_.largura, area_.altura);
    glDisable(GL_BLEND);
    posProcesso_.usar();
    posProcesso_.definir("uTela", 0);
    posProcesso_.definir("uSaturacao", g.saturacao);
    posProcesso_.definir("uContraste", g.contraste);
    posProcesso_.definir("uBrilho", g.brilho);
    posProcesso_.definir("uTinta", g.tinta);
    posProcesso_.definir("uVinheta", g.vinheta);
    posProcesso_.definir("uTempo", tempo);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, cor_);
    glBindVertexArray(vao_);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glViewport(0, 0, janela.x, janela.y);
}

}
