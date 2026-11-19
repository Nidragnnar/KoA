#pragma once
#include <map>
#include <string>
#include <tuple>
#include <vector>

#include <nlohmann/json_fwd.hpp>

#include "Game/Inventory/Inventory.h"

// Fase 19: os povos vizinhos, os tratados e a reputação do herdeiro (Data/Kingdom/faccoes.json). Regras puras:
// o World guarda as relações, os tratados assinados e a reputação
namespace koa {

// Os três eixos da reputação (-100 a 100): justiça (justo + / tirano -), generosidade, devoção (devoto + / arcano -)
struct Reputacao {
    float justica = 0.0f, generosidade = 0.0f, devocao = 0.0f;

    float eixo(const std::string& nome) const;
    float& eixo(const std::string& nome);
};

struct FaccaoDef {
    std::string id, nome, lider, territorio, descricao;
    Reputacao gosta;  // pesos: para onde a relação anda com o jeito de governar
    float inicial = 0.0f;
};

struct EfeitosTratado {
    int pedraDia = 0, ouroDia = 0, seguranca = 0, soldados = 0, expedicao = 0, cristalDia = 0, dano = 0;
    float felicidade = 0.0f, forcaHorda = 0.0f;  // forcaHorda: fração (-0,15 = horda 15% mais fraca)

    EfeitosTratado& operator+=(const EfeitosTratado& o);
};

struct TratadoDef {
    std::string id, faccao, nome, descricao;
    int requer = 0;  // relação mínima para assinar
    Custo custo;
    EfeitosTratado efeitos;
};

struct ReacaoMarco {
    std::string marco;  // terminado em * vale para qualquer marco que comece assim
    std::map<std::string, float> faccoes;
    Reputacao reputacao;
};

struct DadosFaccoes {
    std::vector<std::pair<int, std::string>> faixas;  // (mínimo, nome), do maior para o menor
    std::vector<FaccaoDef> faccoes;
    std::vector<std::tuple<std::string, std::string, float>> rivais;  // (a, b, perda ao assinar com uma delas)
    std::vector<TratadoDef> tratados;
    std::vector<ReacaoMarco> reacoes;
    float limiteFama = 30.0f;
    std::map<std::string, std::pair<std::string, std::string>> marcosFama;  // eixo → (positivo, negativo)

    const FaccaoDef* faccao(const std::string& id) const;
    const TratadoDef* tratado(const std::string& id) const;
    std::string faixa(float relacao) const;  // "aliados", "neutros"...
    // O que o marco muda (somado de todas as reações que casam com ele)
    ReacaoMarco reacaoA(const std::string& marco) const;
};

bool lerFaccoes(const nlohmann::json& j, DadosFaccoes& d, std::string* erro);

// "+3 de pedra por dia, -2 de ouro por dia"
std::string textoTratado(const EfeitosTratado& e);

}
