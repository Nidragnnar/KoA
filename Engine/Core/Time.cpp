#include "Engine/Core/Time.h"

#include <algorithm>

namespace ke {

int FixedTimestep::avancar(double dtReal) {
    // Engasgos longos (janela arrastada, depurador) não viram uma rajada de passos
    acumulado_ += std::clamp(dtReal, 0.0, passo_ * maxPassos_);
    int n = 0;
    while (acumulado_ >= passo_ && n < maxPassos_) {
        acumulado_ -= passo_;
        ++n;
    }
    if (n == maxPassos_) acumulado_ = std::min(acumulado_, passo_);
    return n;
}

}
