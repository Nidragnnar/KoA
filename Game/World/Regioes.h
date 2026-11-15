#pragma once
#include <map>
#include <set>
#include <string>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <nlohmann/json_fwd.hpp>

#include "Game/Inventory/Inventory.h"

// Fase 11: regiões vivas (corrupção, segurança e prosperidade), regiões que se transformam e chefes com escolha
namespace koa {

// Enfeite que só existe na região transformada (o guardião das minas, flores e cristais da floresta)
struct EnfeiteRegiaoDef {
    std::string sprite;
    glm::ivec2 tile{0};
    bool temLuz = false;
    glm::vec3 corLuz{1.0f};
    float raioLuz = 40.0f;
    bool solido = false;  // ocupa o lugar (o guardião), em vez de só enfeitar o chão
};

// Como a região fica depois de um acontecimento (floresta purificada, minas depois do golem...)
struct TransformacaoRegiao {
    std::string marco;            // vale quando este marco existe (a primeira da lista que valer)
    std::string nome;
    float escuridaoMin = 0.0f;
    glm::vec3 ambiente{1.0f};
    bool hostil = true;
    int era = 0;                  // visual da região: terreno e árvores com a cara desta era
    std::set<std::string> semInimigos;  // tipos que somem enquanto a corrupção não volta a subir
    float voltamAcima = 60.0f;    // a partir daqui eles voltam (região abandonada)
    float pisoCorrupcao = 0.0f;   // a corrupção nunca fica abaixo disto (o pacto)
    float felicidade = 0.0f;      // somado ao alvo de felicidade do reino
    float luaSangue = 1.0f;       // multiplica a chance da Lua de Sangue
    int rendimentoVeio = 0;       // cristais por veio (0 = o normal do mapa)
    float prosperidade = 1.0f;    // quanto da terra livre vira prosperidade (0..1)
    std::map<std::string, float> producao;  // recurso → por ponto de prosperidade, por noite (troca o da região)
    std::string dica;
    std::vector<EnfeiteRegiaoDef> enfeites;
};

// "vida" no json da região: só as regiões com isto têm corrupção, segurança e prosperidade
struct VidaRegiaoDef {
    bool ativa = false;
    float corrupcaoInicial = 70.0f;
    float prosperidade = 0.5f;    // terra amaldiçoada rende metade
    std::map<std::string, float> producao;
    std::vector<std::pair<std::string, glm::ivec2>> invasao;  // criaturas a mais quando a região é tomada
    std::vector<TransformacaoRegiao> transformacoes;
    std::string dica;             // o que fazer para melhorar (painel R)
};

// O que muda dia a dia (vai para o save)
struct EstadoRegiao {
    float corrupcao = 70.0f;      // 0..100: quanto as cinzas dominam a terra
    float seguranca = 0.0f;       // 0..100: sobe a cada criatura vencida, cai a cada noite
    float prosperidade = 0.0f;    // 0..100: o que a terra rende para o reino
    int ultimaVisita = 1;         // dia da última visita do herdeiro
};

// Regras (os mesmos números para todas as regiões)
namespace regra {
constexpr float CORRUPCAO_POR_ABATE = 1.5f;
constexpr float SEGURANCA_POR_ABATE = 6.0f;
constexpr float PRESSAO_NOITE = 2.0f;      // a corrupção sempre empurra um pouco
constexpr float PRESSAO_ABANDONO = 3.0f;   // e mais quando ninguém aparece
constexpr int DIAS_ABANDONO = 3;
constexpr float SEGURANCA_FICA = 0.6f;     // quanto da segurança sobra de uma noite para outra
constexpr float LIMPEZA_SEGURANCA = 0.05f; // segurança que segura a corrupção à noite
constexpr float INVASAO = 85.0f;           // a partir daqui a região é tomada (criaturas a mais)
constexpr float PURIFICAR = 25.0f;         // abaixo disto o Coração da Floresta aceita o rito
}

// Criatura vencida na região: a corrupção cede e a segurança sobe
void registrarAbate(EstadoRegiao& e, const TransformacaoRegiao* t);

struct ResultadoNoite {
    std::map<std::string, int> producao;
    bool tomada = false;   // passou do limite de invasão esta noite
    bool voltaram = false; // região transformada que voltou a ter as criaturas que tinham sumido
};
// Uma noite na região: segurança cai, corrupção avança (mais se abandonada), prosperidade persegue a terra livre
ResultadoNoite noiteDaRegiao(EstadoRegiao& e, const VidaRegiaoDef& v, const TransformacaoRegiao* t, int dia);
// Quanto das criaturas renasce de manhã (corrupção alta = todas; terra limpa = um terço)
float fracaoCriaturas(float corrupcao);
// Produção da noite com a prosperidade atual (o painel R mostra isto)
std::map<std::string, int> producaoRegiao(const EstadoRegiao& e, const VidaRegiaoDef& v, const TransformacaoRegiao* t);
bool abandonada(const EstadoRegiao& e, int dia);

bool lerVidaRegiao(const nlohmann::json& j, VidaRegiaoDef& v);

// ---------- Chefes com escolha ----------

struct OpcaoEscolha {
    std::string id, nome, descricao;
    std::string requer, requerTexto;  // marco necessário e o que dizer quando falta
    Custo recompensa;
    std::string item;                 // equipamento dado na hora
    float corrupcao = -1.0f;          // a corrupção da região vira isto (-1 = não muda)
};

struct EscolhaDef {
    std::string id, titulo;
    std::string inimigo;              // o chefe que se rende ("golem_cinzas")
    std::string regiao;
    std::vector<std::string> texto;
    std::vector<OpcaoEscolha> opcoes;

    const OpcaoEscolha* opcao(const std::string& qual) const;
};

bool lerEscolhas(const nlohmann::json& j, std::map<std::string, EscolhaDef>& saida, std::string* erro);

}
