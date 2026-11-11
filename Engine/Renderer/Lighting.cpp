#include "Engine/Renderer/Lighting.h"

#include <glad/glad.h>

#include "Engine/Core/Logger.h"

namespace ke {

LightMap::~LightMap() {
    if (fbo_) glDeleteFramebuffers(1, &fbo_);
}

bool LightMap::criar(int largura, int altura) {
    cor_.criar(largura, altura, nullptr);
    glGenFramebuffers(1, &fbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, cor_.id(), 0);
    const bool ok = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (!ok) log::erro("framebuffer do mapa de luz incompleto");
    return ok;
}

void LightMap::comecar(glm::vec3 ambiente) {
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &anterior_);
    glGetIntegerv(GL_VIEWPORT, viewportAnterior_);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glViewport(0, 0, cor_.largura(), cor_.altura());
    glClearColor(ambiente.r, ambiente.g, ambiente.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void LightMap::terminar() {
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<unsigned>(anterior_));
    glViewport(viewportAnterior_[0], viewportAnterior_[1], viewportAnterior_[2], viewportAnterior_[3]);
}

}
