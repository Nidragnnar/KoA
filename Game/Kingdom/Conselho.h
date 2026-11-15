#pragma once
#include <map>
#include <string>
#include <vector>

#include <nlohmann/json_fwd.hpp>

#include "Game/Inventory/Inventory.h"

// Fase 12: o Conselho Real (cargos e lealdade) e as decisões políticas (Data/Kingdom/conselho.json)
namespace koa {

// Um cargo do conselho: o que ele faz pelo reino e quem tem jeito para ele
struct CargoDef {
    std::string id, nome, descricao;
    std::string efeito;                    // seguranca | impostos | comida | obras
    float forca = 0.0f;                    // o efeito com o melhor conselheiro, leal (6 de segurança, +25% de impostos...)
    std::map<std::string, float> aptidao;  // morador → 0..1 (quem não está na lista: 0,5)

    float aptidaoDe(const std::string& npc) const {
        const auto it = aptidao.find(npc);
        return it == aptidao.end() ? 0.5f : it->second;
    }
};

// O que uma escolha muda (tudo opcional)
struct EfeitosDecisao {
    Custo recursos;                          // ganho (negativo = gasto, até o que houver)
    float felicidade = 0.0f;
    int populacao = 0;
    std::map<std::string, int> lealdade;     // cargo → variação (de quem estiver nele)
    std::map<std::string, int> amizade;      // morador → variação
    std::map<std::string, float> corrupcao;  // região → variação
    std::string lei, revogar;                // lei que passa a valer / que deixa de valer
    int imposto = -1;                        // 0 baixo, 1 médio, 2 alto (-1 = não muda)
};

struct OpcaoDecisao {
    std::string id, nome, descricao;
    EfeitosDecisao efeitos;
};

struct RequisitoDecisao {
    int populacaoMin = 0, eraMin = 0, impostoMin = -1;
    std::string marco, semMarco, semLei;
};

// Um caso que o conselho traz ao herdeiro
struct DecisaoDef {
    std::string id, titulo;
    std::vector<std::string> texto;
    RequisitoDecisao requer;
    std::vector<OpcaoDecisao> opcoes;
    std::map<std::string, std::pair<std::string, std::string>> conselho;  // cargo → (opção que apoia, o que diz)
    bool repetir = false;  // pode voltar depois de decidida

    const OpcaoDecisao* opcao(const std::string& qual) const;
};

// Lei que vale todo dia enquanto não for revogada
struct LeiDef {
    std::string id, nome;
    float felicidade = 0.0f;
    int ouroDia = 0, seguranca = 0;
};

struct DadosConselho {
    int obrasParaFormar = 4;     // obras de pé (sem a casa) para o povo pedir um conselho
    int diasEntreDecisoes = 3;
    std::vector<CargoDef> cargos;
    std::vector<DecisaoDef> decisoes;
    std::map<std::string, LeiDef> leis;

    const CargoDef* cargo(const std::string& id) const;
    const DecisaoDef* decisao(const std::string& id) const;
};

bool lerConselho(const nlohmann::json& j, DadosConselho& d, std::string* erro);

// ---------- Lealdade (0..100) ----------
namespace regra {
constexpr int LEALDADE_POR_CONVERSA = 2;  // uma vez por dia, ao conversar
constexpr int LEALDADE_APOIADO = 8;       // o herdeiro seguiu o conselho dele
constexpr int LEALDADE_CONTRARIADO = 6;   // o herdeiro decidiu contra
constexpr int LEALDADE_DESTITUIDO = 20;   // perdeu o cargo
constexpr int LEALDADE_INSATISFEITO = 25; // abaixo disto faz o mínimo (o cargo não rende)
}
float fatorLealdade(int lealdade);           // 1 com 50+, 0,6 entre 25 e 49, 0 abaixo de 25
const char* palavraLealdade(int lealdade);   // "Leal", "Satisfeito", "Desconfiado", "Insatisfeito"
int lealdadeInicial(int afinidade);          // 40 + amizade/20 (até 90)
// O efeito que o conselheiro rende no cargo (já com aptidão e lealdade)
float efeitoNoCargo(const CargoDef& c, const std::string& npc, int lealdade);

}
