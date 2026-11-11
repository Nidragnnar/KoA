#include "Engine/Core/SceneManager.h"

namespace ke {

void SceneManager::aplicarTroca() {
    if (!pendente_) return;
    atual_ = std::move(pendente_);
    atual_->entrar();
}

}
