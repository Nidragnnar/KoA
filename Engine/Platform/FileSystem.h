#pragma once
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace ke::fs {

std::optional<std::string> lerTexto(const std::filesystem::path& p);
// Grava em .tmp e troca de nome: um corte de energia não deixa o arquivo pela metade
bool gravarTextoSeguro(const std::filesystem::path& p, std::string_view conteudo);
// Cópia recursiva de uma pasta (para backup de saves); apaga o destino antes
bool copiarPasta(const std::filesystem::path& origem, const std::filesystem::path& destino);

}
