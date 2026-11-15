#pragma once
#include <string>
#include <vector>

#include "Game/Data/GameData.h"
#include "Game/Inventory/Inventory.h"
#include "Game/Kingdom/Kingdom.h"

namespace koa {

enum class Imposto { Baixo = 0, Medio = 1, Alto = 2 };
const char* nomeImposto(Imposto i);

// Estado do povo (o que muda dia a dia)
struct EstadoReino {
    int populacao = 8;
    int populacaoBase = 8;     // quantos cabem nas ruínas e barracos sem nenhuma obra
    float felicidade = 40.0f;  // 0..100
    Imposto imposto = Imposto::Medio;
};

// Números do reino calculados a partir das construções prontas (o painel K mostra isto)
struct ResumoReino {
    int populacaoMax = 0;
    int seguranca = 0;
    float felicidadeAlvo = 0.0f;  // para onde a felicidade caminha a cada noite
    int comidaProduzida = 0, comidaConsumida = 0;
    int impostos = 0, ouroProducao = 0;
    int madeiraDia = 0, pedraDia = 0;
};

// O que aconteceu durante a noite (relatório da manhã)
struct RelatorioDia {
    std::vector<std::pair<std::string, int>> variacoes;  // recurso → ganho (negativo = gasto)
    int populacaoAntes = 0, populacaoDepois = 0;
    float felicidadeAntes = 0.0f, felicidadeDepois = 0.0f;
    std::vector<std::string> notas;
};

// `mod`: efeito da estação (hortas, consumo, comércio, ânimo, viajantes)
ResumoReino calcularResumo(const Kingdom& k, const GameData& dados, const EstadoReino& e, const ModificadorDia& mod = {});
// Uma noite de simulação: impostos, produção, comida, felicidade e população
RelatorioDia simularDia(const Kingdom& k, const GameData& dados, EstadoReino& e, Inventory& inv, const ModificadorDia& mod = {});

}
