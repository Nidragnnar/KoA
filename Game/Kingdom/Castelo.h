#pragma once
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <glm/vec3.hpp>
#include <nlohmann/json_fwd.hpp>

#include "Game/Inventory/Inventory.h"

// Fase 16: o castelo dos reis, as alas e o brasão da linhagem (Data/Kingdom/castelo.json)
namespace koa {

struct EfeitosAla {
    float felicidade = 0.0f, impostos = 0.0f, comida = 0.0f;  // impostos e comida: fração (0.15 = +15%)
    int seguranca = 0, viajantes = 0, ouroDia = 0, soldados = 0, expedicao = 0;
    float diasCeleiro = 0.0f;  // fase 17: dias de comida a mais que cabem no celeiro

    EfeitosAla& operator+=(const EfeitosAla& o);
};

// Fase 20: o que a ala erguida deixa fazer (com tempo de recarga em dias)
struct AcaoAla {
    std::string id, nome, descricao;
    Custo custo;
    int recarga = 7;
    float felicidade = 0.0f;
    int soldadosHorda = 0, emprestimo = 0, diasEmprestimo = 0;
    bool decifrar = false;
};

struct AlaDef {
    std::string id, nome, descricao;
    Custo custo;
    EfeitosAla efeitos;
    std::optional<AcaoAla> acao;
};

struct PecaBrasao {
    std::string id, nome;
};

struct Tintura {
    std::string id, nome;
    glm::vec3 cor{1.0f};
};

// Um brasão montado (o que vai para o save)
struct Brasao {
    std::string forma = "classico", divisao = "pleno", cor1 = "goles", cor2 = "ouro", simbolo = "coroa", corSimbolo = "ouro";
    int lema = 0;

    bool operator==(const Brasao&) const = default;
};

struct DadosCastelo {
    std::vector<int> vagasPorNivel{0, 0, 1, 2, 3};  // índice = nível do castelo
    std::vector<AlaDef> alas;
    std::vector<PecaBrasao> formas, divisoes, simbolos;
    std::vector<Tintura> tinturas;
    std::vector<std::string> lemas;
    Brasao inicial;

    const AlaDef* ala(const std::string& id) const;
    const Tintura* tintura(const std::string& id) const;
    int vagas(int nivelCastelo) const;
    // As peças de cada linha do editor, na ordem (0 forma, 1 divisão, 2 cor 1, 3 cor 2, 4 símbolo, 5 cor do símbolo)
    std::vector<std::string> opcoes(int linha) const;
    std::string nomePeca(int linha, const std::string& id) const;
    // Brasão com peças que não existem mais volta ao inicial nelas
    Brasao validar(Brasao b) const;
};

bool lerCastelo(const nlohmann::json& j, DadosCastelo& d, std::string* erro);

// O brasão descrito como os arautos fazem: "Partido de goles e azure, um leão de ouro"
std::string textoBrasao(const DadosCastelo& d, const Brasao& b);
// "+5 de felicidade, +15% de impostos"
std::string textoAla(const EfeitosAla& e);
// Troca a peça da linha do editor pela anterior (-1) ou pela próxima (+1)
void girarPeca(const DadosCastelo& d, Brasao& b, int linha, int passo);
inline constexpr int LINHAS_BRASAO = 7;  // forma, divisão, cor 1, cor 2, símbolo, cor do símbolo, lema

}
