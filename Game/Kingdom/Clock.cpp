#include "Game/Kingdom/Clock.h"

#include <algorithm>
#include <cmath>
#include <format>

#include <glm/common.hpp>

namespace koa {

const char* nomeEstacao(Estacao e) {
    switch (e) {
    case Estacao::Primavera: return "Primavera";
    case Estacao::Verao: return "Verão";
    case Estacao::Outono: return "Outono";
    case Estacao::Inverno: return "Inverno";
    }
    return "?";
}

const char* nomeDiaSemana(int d) {
    static const char* nomes[] = {"Segunda", "Terça", "Quarta", "Quinta", "Sexta", "Sábado", "Domingo"};
    return nomes[((d % 7) + 7) % 7];
}

bool Relogio::avancar(float dt) {
    if (parado) return false;
    minutos += dt / SEG_REAIS_POR_MINUTO;
    if (minutos >= FIM_DIA) {
        minutos = FIM_DIA;
        return true;
    }
    return false;
}

void Relogio::novoDia() {
    ++dia;
    minutos = INICIO_DIA;
}

std::string Relogio::textoHora() const {
    const int m = minutosExibidos();
    return std::format("{:02}:{:02}", m / 60 % 24, m % 60);
}

std::string Relogio::textoData() const {
    return std::format("{}, dia {} ({})", nomeEstacao(estacao()), diaDaEstacao(), nomeDiaSemana(diaSemana()));
}

Periodo Relogio::periodo() const {
    if (minutos < 12 * 60) return Periodo::Manha;
    if (minutos < 18 * 60) return Periodo::Tarde;
    return Periodo::Noite;
}

float Relogio::escuridao() const {
    const float h = minutos / 60.0f;
    if (h < 7.0f) return std::clamp(1.0f - (h - 6.0f), 0.0f, 1.0f) * 0.5f;  // amanhecer
    if (h < 17.5f) return 0.0f;
    if (h < 21.0f) return (h - 17.5f) / 3.5f;                                // entardecer
    return 1.0f;
}

glm::vec3 Relogio::luzAmbiente() const {
    const glm::vec3 claro{1.0f}, tarde{1.0f, 0.78f, 0.60f}, noite{0.27f, 0.31f, 0.50f};
    const float h = minutos / 60.0f;
    if (h < 7.0f) return glm::mix(glm::mix(noite, tarde, 0.5f), claro, std::clamp(h - 6.0f, 0.0f, 1.0f));
    if (h < 17.5f) return claro;
    if (h < 19.0f) return glm::mix(claro, tarde, (h - 17.5f) / 1.5f);
    if (h < 21.0f) return glm::mix(tarde, noite, (h - 19.0f) / 2.0f);
    return noite;
}

}
