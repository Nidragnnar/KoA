#pragma once
#include <string>

#include <glm/vec3.hpp>

namespace koa {

enum class Estacao { Primavera, Verao, Outono, Inverno };
enum class Periodo { Manha, Tarde, Noite };

const char* nomeEstacao(Estacao e);
const char* nomeDiaSemana(int diaSemana);  // 0 = segunda

// Relógio do jogo: o dia vai das 6:00 às 2:00 da madrugada (26:00).
// 10 minutos do jogo passam a cada 7 segundos reais (o dia inteiro dura ~14 min).
struct Relogio {
    static constexpr float SEG_REAIS_POR_MINUTO = 0.7f;
    static constexpr int INICIO_DIA = 6 * 60;
    static constexpr int FIM_DIA = 26 * 60;  // 2:00 da manhã: desmaio
    static constexpr int DIAS_POR_ESTACAO = 7;  // fase 10: estações curtas, para o ano inteiro caber numa partida

    int dia = 1;  // conta desde o começo do jogo
    float minutos = INICIO_DIA;
    bool parado = false;

    // Devolve true quando o dia chega às 2:00 (o herói desmaia)
    bool avancar(float dtReal);
    void novoDia();

    int minutosExibidos() const { return static_cast<int>(minutos) / 10 * 10; }  // de 10 em 10, como num relógio de vila
    int hora() const { return static_cast<int>(minutos) / 60 % 24; }
    std::string textoHora() const;  // "08:40"
    std::string textoData() const;  // "Primavera, dia 3 (Quarta)"
    Estacao estacao() const { return static_cast<Estacao>((dia - 1) / DIAS_POR_ESTACAO % 4); }
    int diaDaEstacao() const { return (dia - 1) % DIAS_POR_ESTACAO + 1; }
    int ano() const { return (dia - 1) / (DIAS_POR_ESTACAO * 4) + 1; }
    int diaSemana() const { return (dia - 1) % 7; }
    Periodo periodo() const;
    // Cor da luz ambiente (branco de dia, laranja no entardecer, azul-escuro de noite)
    glm::vec3 luzAmbiente() const;
    // 0 de dia, 1 em plena noite
    float escuridao() const;
};

}
