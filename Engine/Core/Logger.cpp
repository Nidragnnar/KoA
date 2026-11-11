#include "Engine/Core/Logger.h"

#include <chrono>
#include <cstdio>
#include <fstream>
#include <mutex>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace ke::log {

namespace {
std::ofstream g_arquivo;
std::mutex g_trava;
}

void init(const std::filesystem::path& arquivo) {
    std::lock_guard lk(g_trava);
    std::error_code ec;
    std::filesystem::create_directories(arquivo.parent_path(), ec);
    g_arquivo.open(arquivo, std::ios::out | std::ios::trunc);
}

void escrever(std::string_view nivel, std::string_view msg) {
    std::lock_guard lk(g_trava);
    const auto agora = std::chrono::floor<std::chrono::milliseconds>(std::chrono::system_clock::now());
    const std::string linha = std::format("{:%H:%M:%S} [{}] {}\n", agora, nivel, msg);
    if (g_arquivo) g_arquivo << linha << std::flush;
    OutputDebugStringA(linha.c_str());
    std::fputs(linha.c_str(), stdout);
}

}
