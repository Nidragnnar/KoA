#pragma once
#include <memory>

#include "Engine/Renderer/RenderTarget.h"

union SDL_Event;

namespace ke {

class Scene {
public:
    virtual ~Scene() = default;
    virtual void entrar() {}
    virtual void atualizar(double dt) = 0;  // passo fixo de lógica
    virtual void renderizar(float alfa) = 0;  // desenha na tela interna
    virtual void interfaceDepuracao() {}       // janelas do ImGui (F1)
    virtual ColorGrade grade() const { return {}; }
};

// Troca de cena adiada para o início do próximo quadro (nunca no meio de um update)
class SceneManager {
public:
    void trocar(std::unique_ptr<Scene> nova) { pendente_ = std::move(nova); }
    void aplicarTroca();
    Scene* atual() const { return atual_.get(); }

private:
    std::unique_ptr<Scene> atual_, pendente_;
};

}
