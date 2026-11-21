#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include <nlohmann/json_fwd.hpp>

#include "Game/Inventory/Inventory.h"

// Mundo vivo (versão 1.14): aldeias vizinhas, caravanas, viajantes com pedidos curtos e a terra tomada que avança
// (Data/World/mundo.json). Uma simulação de fundo barata, só à noite, por regras puras; o World guarda o estado
namespace koa {

struct CaravanaDef {
    std::string nome;
    int aCada = 5;
    std::map<std::string, float> mercado;  // bem → pressão quando chega (negativa: barateia)
    int pedagio = 0;
};

struct AldeiaDef {
    std::string id, nome, perto, faccao, descricao;
    int povo = 20;
    float base = 60.0f;            // prosperidade de uma terra em paz
    float corrupcaoFixa = -1.0f;   // para quem fica perto de região sem vida (a cripta)
    CaravanaDef caravana;
};

struct AjudaDef {
    std::string id, texto, quando;  // quando: prosperidade_abaixo, corrupcao_acima, sorte (valor em %)
    float valor = 0.0f, ajuda = 10.0f, limpa = 0.0f;
    Custo custo;
    int prazo = 2;
};

struct ViajanteDef {
    std::string id, texto, requer;
    Custo custo;
    float felicidade = 0.0f, generosidade = 0.0f;
    int soldados = 0;
    std::map<std::string, float> corrupcao;  // região → mudança
};

struct DadosMundo {
    std::string requer;
    int noticiasPorManha = 2, someAbaixo = 6, tributo = 10;
    float cresceAcima = 60.0f, minguaAbaixo = 30.0f, abandono = 20.0f, chanceViajante = 0.25f;
    float avancoPressao = 1.5f, avancoAldeia = 3.0f;
    std::map<std::string, std::vector<std::string>> vizinhanca;
    std::vector<AldeiaDef> aldeias;
    std::vector<AjudaDef> ajudas;
    std::vector<ViajanteDef> viajantes;

    const AldeiaDef* aldeia(const std::string& id) const;
    const AjudaDef* ajuda(const std::string& id) const;
    const ViajanteDef* viajante(const std::string& id) const;
};

bool lerMundo(const nlohmann::json& j, DadosMundo& d, std::string* erro);

// O que vai para o save
struct EstadoAldeia {
    int povo = 0;
    float prosperidade = 50.0f;
    int gratidao = 0;         // quantas vezes Valdren ajudou
    bool sumiu = false;
    int proximaCaravana = 0;  // dia
    int ultimoTributo = 0;
};

struct PedidoAberto {
    std::string def;     // id da ajuda ou do viajante
    std::string aldeia;  // quem pede ("" = um viajante)
    int prazo = 0;       // último dia para atender
};

struct EstadoMundo {
    std::map<std::string, EstadoAldeia> aldeias;
    std::vector<PedidoAberto> pedidos;
    std::vector<std::string> noticias;  // as desta manhã (todas; o relatório mostra só as primeiras)
};

struct Noticia {
    int prioridade = 0;  // maior = mais importante
    std::string texto;
};

// Sorteio de 0 a 1, fixo para a partida, o dia e o assunto
float sorteMundo(uint32_t semente, int dia, const std::string& assunto);
float alvoProsperidade(const AldeiaDef& a, float corrupcao);
// Uma noite na aldeia: a prosperidade anda para o alvo; o povo cresce ou mingua
void noiteDaAldeia(const DadosMundo& d, EstadoAldeia& e, const AldeiaDef& a, float corrupcao, float sorte);
// O pedido de ajuda que a aldeia faria hoje (nullptr = nenhum); `sorte` de 0 a 1
const AjudaDef* ajudaNecessaria(const DadosMundo& d, const EstadoAldeia& e, float corrupcao, float sorte);
// Chance de a caravana ser atacada (de 5% a 60%)
float riscoCaravana(float corrupcaoRota, int segurancaReino);
// As notícias da manhã, da mais importante para a menos (na mesma importância, na ordem em que aconteceram)
std::vector<std::string> ordenarNoticias(std::vector<Noticia> n);

}
