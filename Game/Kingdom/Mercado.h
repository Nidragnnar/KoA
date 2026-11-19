#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include <nlohmann/json_fwd.hpp>

#include "Game/Inventory/Inventory.h"

// Fase 20: o mercado vivo (Data/Kingdom/mercado.json). Regras puras de preço; o World guarda o estado
namespace koa {

struct BemDef {
    std::string id, requer;  // requer: marco que faz o bem aparecer
    float base = 1.0f;       // preço de uma unidade
    int lote = 10;
    bool soVenda = false;    // o mercado compra do herdeiro, mas não vende
    std::map<std::string, float> estacoes;

    float fatorEstacao(const std::string& estacao) const;
};

struct EncomendaDef {
    std::string id, nome;
    Custo custo, ganho;
    int dias = 3;  // tempo mínimo até a entrega (chega na visita seguinte do Corvin)
};

struct ReacaoMercado {
    std::string marco;  // terminado em * vale para qualquer marco que comece assim
    std::map<std::string, float> pressao;
};

struct DadosMercado {
    int lote = 10;
    std::vector<float> taxas{0.2f, 0.15f, 0.1f};  // por nível do mercado (1..3)
    std::vector<int> limites{6, 9, 12};           // lotes negociados por dia, por nível
    float impacto = 0.06f, recupera = 0.25f, sorte = 0.06f, pressaoMin = -0.6f, pressaoMax = 1.0f, taxaCorvin = 0.25f;
    int diasHistorico = 14;
    std::vector<BemDef> bens;
    std::vector<ReacaoMercado> reacoes;
    std::vector<EncomendaDef> encomendas;

    const BemDef* bem(const std::string& id) const;
    const EncomendaDef* encomenda(const std::string& id) const;
    float taxa(int nivelMercado) const;  // nível 0 = Corvin
    int limite(int nivelMercado) const;
    std::map<std::string, float> reacaoA(const std::string& marco) const;
};

bool lerMercado(const nlohmann::json& j, DadosMercado& d, std::string* erro);

// O que vai para o save
struct EncomendaFeita {
    std::string id;
    int dia = 0;  // dia do pedido
};

struct EstadoMercado {
    std::map<std::string, float> pressao;                  // bem → quanto o preço está acima (+) ou abaixo (-) do normal
    std::map<std::string, std::vector<float>> historico;  // preço de uma unidade em cada manhã (o mais recente no fim)
    int negociadosHoje = 0;
    std::vector<EncomendaFeita> encomendas;
    int vendido = 0, comprado = 0;  // ouro recebido e pago no mercado desde o começo (para o Livro e o balanceamento)
};

// Sorte do dia de um bem (-sorte..+sorte), sempre a mesma para a mesma partida, dia e bem
float sorteDoDia(const DadosMercado& d, uint32_t semente, int dia, const std::string& bem);
// Preço de uma unidade: base × estação × (1 + pressão) × (1 + sorte)
float precoUnitario(const BemDef& b, const std::string& estacao, float pressao, float sorte);
// Lote inteiro: comprar arredonda para cima, vender para baixo (a taxa sempre fica com o mercado)
int precoCompra(float unitario, int lote, float taxa);
int precoVenda(float unitario, int lote, float taxa);
// Uma negociação mexe na pressão (comprar sobe, vender desce), dentro dos limites
void negociar(const DadosMercado& d, EstadoMercado& e, const std::string& bem, bool comprou);
// A noite: a pressão volta para o normal e o limite do dia recomeça
void noiteDoMercado(const DadosMercado& d, EstadoMercado& e);

}
