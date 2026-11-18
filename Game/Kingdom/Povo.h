#pragma once
#include <cstdint>
#include <set>
#include <string>
#include <vector>

#include <nlohmann/json_fwd.hpp>

// Fase 18: o povo com nome (Data/NPCs/povo.json). Cada habitante além dos moradores com nome do npcs.json
// tem nome, família, profissão, traço e um desejo; as regras aqui são puras (o World guarda as pessoas)
namespace koa {

struct EfeitosPovo {
    float madeiraDia = 0.0f, pedraDia = 0.0f, comidaDia = 0.0f, ouroDia = 0.0f, seguranca = 0.0f, felicidade = 0.0f;

    EfeitosPovo& operator+=(const EfeitosPovo& o);
};

struct ProfissaoDef {
    std::string id, nome;
    int peso = 1;
    std::string requer;  // construção pronta que precisa existir para alguém chegar com essa profissão ("" = nenhuma)
    EfeitosPovo efeitos;
};

struct TracoDef {
    std::string id, nome;
    float rende = 1.0f;  // multiplica o que a profissão rende
    int paciencia = 10;  // dias que aguenta infeliz com o desejo esperando
};

struct CondicaoDesejo {
    std::string tipo, alvo;  // obra, marco, felicidade, seguranca, comida_dias, castelo, brasao
    float valor = 0.0f;
    int nivel = 1;
};

struct DesejoDef {
    std::string id, texto, fala;
    CondicaoDesejo condicao;
};

struct DadosPovo {
    std::vector<std::string> nomes, sobrenomes;
    std::vector<ProfissaoDef> profissoes;
    std::vector<TracoDef> tracos;
    std::vector<DesejoDef> desejos;

    const ProfissaoDef* profissao(const std::string& id) const;
    const TracoDef* traco(const std::string& id) const;
    const DesejoDef* desejo(const std::string& id) const;
};

bool lerPovo(const nlohmann::json& j, DadosPovo& d, std::string* erro);

// Uma pessoa do povo (vai para o save)
struct Habitante {
    int id = 0;
    std::string nome, familia, profissao, traco, desejo;
    bool cumprido = false;  // o desejo já se realizou
    int chegou = 1;         // dia em que chegou ao vilarejo
    int esperando = 0;      // noites com o desejo esperando
    int variante = 0;       // roupa (aldeao_<variante>_...)

    std::string nomeCompleto() const { return nome + " " + familia; }
};

// Número fixo a partir de dois inteiros (gerar a mesma pessoa com a mesma sorte)
uint32_t misturar(uint32_t a, uint32_t b);

// Uma pessoa nova: profissões só das obras prontas, sorteio fixo pela sorte
Habitante gerarHabitante(const DadosPovo& d, int id, const std::string& familia, int dia, uint32_t sorte, const std::set<std::string>& obrasProntas);
std::string sobrenomeSorteado(const DadosPovo& d, uint32_t sorte);
// O que o povo rende numa noite, somado (profissão × traço) e a alegria dos desejos cumpridos (+0,2 cada, até +5)
EfeitosPovo efeitosDoPovo(const DadosPovo& d, const std::vector<Habitante>& povo);

}
