#include "Engine/Renderer/Animation.h"

#include <algorithm>

namespace ke {

const std::string& AnimationClip::quadro(float tempo) const {
    static const std::string vazio;
    if (quadros.empty()) return vazio;
    int i = static_cast<int>(tempo * fps);
    const int n = static_cast<int>(quadros.size());
    i = repetir ? ((i % n) + n) % n : std::clamp(i, 0, n - 1);
    return quadros[i];
}

}
