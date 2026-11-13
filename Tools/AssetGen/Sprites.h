#pragma once
#include <string>
#include <utility>
#include <vector>

#include "Desenho.h"

namespace gen {

// Todos os sprites provisórios da era das cinzas, com o nome usado no atlas
std::vector<std::pair<std::string, Img>> gerarTodos();

// Ícone do jogo (a última coroa sobre brasas), quadrado de `lado` pixels
Img iconeCoroa(int lado);

}
