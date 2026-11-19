cmake_minimum_required(VERSION 3.25)
project(KingdomOfAshes VERSION 1.11.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Runtime estático: os amigos não precisam instalar o Visual C++ Redistributable
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

include(cmake/Dependencias.cmake)

# Avisos e acentos só no nosso código; includes a partir da raiz ("Engine/...", "Game/...")
function(configurar_alvo alvo)
    target_compile_options(${alvo} PRIVATE /W4 /utf-8 /permissive- /bigobj)
    target_compile_definitions(${alvo} PRIVATE NOMINMAX)
    target_include_directories(${alvo} PUBLIC ${CMAKE_SOURCE_DIR})
endfunction()

add_subdirectory(Engine)
add_subdirectory(Game)
add_subdirectory(Tools)

enable_testing()
add_subdirectory(Tests)
