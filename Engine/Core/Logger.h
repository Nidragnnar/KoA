#pragma once
#include <filesystem>
#include <format>
#include <string_view>

// Registro em arquivo (e no depurador): o exe é janela, sem console
namespace ke::log {

void init(const std::filesystem::path& arquivo);
void escrever(std::string_view nivel, std::string_view msg);

template <class... A> void info(std::format_string<A...> f, A&&... a) { escrever("INFO", std::format(f, std::forward<A>(a)...)); }
template <class... A> void aviso(std::format_string<A...> f, A&&... a) { escrever("AVISO", std::format(f, std::forward<A>(a)...)); }
template <class... A> void erro(std::format_string<A...> f, A&&... a) { escrever("ERRO", std::format(f, std::forward<A>(a)...)); }

}
