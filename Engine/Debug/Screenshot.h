#pragma once
#include <filesystem>

namespace ke {

// Lê a imagem já desenhada na janela e grava em BMP
bool salvarCaptura(const std::filesystem::path& arquivo, int largura, int altura);

}
