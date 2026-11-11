#include "Engine/Platform/FileSystem.h"

#include <fstream>
#include <sstream>

namespace ke::fs {

std::optional<std::string> lerTexto(const std::filesystem::path& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return std::nullopt;
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

bool gravarTextoSeguro(const std::filesystem::path& p, std::string_view conteudo) {
    std::error_code ec;
    std::filesystem::create_directories(p.parent_path(), ec);
    auto tmp = p;
    tmp += ".tmp";
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f) return false;
        f.write(conteudo.data(), static_cast<std::streamsize>(conteudo.size()));
        if (!f) return false;
    }
    std::filesystem::rename(tmp, p, ec);
    return !ec;
}

bool copiarPasta(const std::filesystem::path& origem, const std::filesystem::path& destino) {
    std::error_code ec;
    std::filesystem::remove_all(destino, ec);
    std::filesystem::create_directories(destino, ec);
    std::filesystem::copy(origem, destino, std::filesystem::copy_options::recursive, ec);
    return !ec;
}

}
