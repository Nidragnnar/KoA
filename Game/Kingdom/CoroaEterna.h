#pragma once
#include <map>
#include <string>
#include <vector>

#include <glm/vec3.hpp>
#include <nlohmann/json_fwd.hpp>

// A Quinta Era, Coroa Eterna (versão 1.18): o estado do reino depois do terceiro ato (Data/Kingdom/coroa_eterna.json).
// Cada final da Queda dá uma Coroa diferente: a cor da tela, os efeitos no reino, o cristal nas ruas e o epílogo
namespace koa {

struct FinalCoroa {
    std::string nome;
    float saturacao = 1.0f, contraste = 1.0f, brilho = 0.0f, vinheta = 0.1f;
    glm::vec3 tinta{1.0f};
    float felicidade = 0.0f, comida = 0.0f;  // comida: fração (+0,15 = hortas 15% mais fartas)
    int seguranca = 0, ouroDia = 0;
    bool cristais = false;
    std::vector<std::string> epilogo;
};

struct DadosCoroa {
    int requerEra = 3, ecoACada = 14;
    float ecoForca = 0.25f;
    std::map<std::string, FinalCoroa> finais;  // aprisionar, unir, destruir
};

bool lerCoroa(const nlohmann::json& j, DadosCoroa& d, std::string* erro);

}