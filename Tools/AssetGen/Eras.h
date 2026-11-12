#pragma once
#include <string>
#include <utility>
#include <vector>

#include "Desenho.h"

namespace gen {

// Versões de cada era (1 = Despertar, 2 = Renascido, 3 = Alta Fantasia) a partir da arte da era das cinzas.
// Recolorir em vez de redesenhar mantém o estilo da arte importada (NidraIA): o mesmo mundo, mais vivo.
// Devolve os sprites novos com o nome "<original>_e<era>".
std::vector<std::pair<std::string, Img>> gerarEras(const std::vector<std::pair<std::string, Img>>& base);

}
