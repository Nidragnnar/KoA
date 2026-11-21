#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include <nlohmann/json_fwd.hpp>

#include "Game/Inventory/Inventory.h"

// Fase 21: a linhagem do herdeiro (Data/Kingdom/linhagem.json): casamento, filhos, traços, tutores e sucessão.
// O tempo corre como sempre; na sucessão o jogo salta alguns anos. Regras puras: o World guarda a família
namespace koa {

struct EfeitosTracos {
    int vida = 0, dano = 0, expedicao = 0, descobrir = 0;  // descobrir: pontos a mais na chance de pegar um desvio
    float felicidade = 0.0f, taxaMercado = 0.0f, consumo = 0.0f, devocaoNoite = 0.0f;

    EfeitosTracos& operator+=(const EfeitosTracos& o);
};

struct TracoLinhagem {
    std::string id, nome, descricao;
    EfeitosTracos efeitos;
    bool ruim = false;  // só vem por sorteio (sem tutor leal)
};

// Com quem o herdeiro pode casar: um morador (npc) ou alguém de uma facção aliada (faccao + nome)
struct ConjugeDef {
    std::string npc, faccao, nome, traco;
    Custo dote;
    std::string id() const { return npc.empty() ? faccao : npc; }
};

struct DadosLinhagem {
    std::string fundadorNome = "Aren";
    std::vector<std::string> fundadorTracos;
    int amizadeMin = 700, gestacao = 7, entreFilhos = 14, filhosMax = 3, maioridade = 28, tutorLealdade = 40;
    int saltoAnos = 15, reinadoVelho = 120, reinadoMax = 168;
    float relacaoMin = 50.0f, saltoMemoria = 0.3f;
    Custo festa;
    std::vector<TracoLinhagem> tracos;
    std::vector<ConjugeDef> conjuges;
    std::map<std::string, std::string> tutores;                     // cargo do conselho → traço que ele ensina
    std::map<std::string, std::map<std::string, float>> reacoesTraco;  // traço → facção → mudança na relação

    const TracoLinhagem* traco(const std::string& id) const;
    const ConjugeDef* conjuge(const std::string& id) const;  // pelo npc ou pela facção
};

bool lerLinhagem(const nlohmann::json& j, DadosLinhagem& d, std::string* erro);

// Um membro da família (o que vai para o save)
struct Membro {
    int id = 0;
    std::string nome;
    std::vector<std::string> tracos;
    int genitor = 0, outroGenitor = 0;  // quem reinava e o cônjuge dele (0 = fundador)
    int conjuge = 0;                     // id do cônjuge na família (0 = sem)
    std::string npc, faccao;             // o cônjuge que veio de fora: o morador ou a facção
    int nasceu = 0;                      // dia em que nasceu (0 = antes do jogo)
    int reinado = 0;                     // o reinado em que nasceu
    int reinou = 0;                      // número do reinado dele (0 = não reinou)
    bool vivo = true, adulto = true;
    std::string tutor;                   // cargo do conselho que educa (filhos)
};

struct Linhagem {
    std::vector<Membro> membros;
    int atual = 0;              // quem reina
    int reinado = 1;            // quantos reinados já houve (1 = o primeiro)
    int inicioReinado = 1;      // dia em que o reinado atual começou
    int anosSaltados = 0;       // anos somados pelos saltos das sucessões
    int proximoId = 1;
    int proximoNascimento = 0;  // dia do próximo nascimento (0 = sem casamento)

    Membro* membro(int id);
    const Membro* membro(int id) const;
    std::vector<const Membro*> filhosDe(int id) const;  // na ordem em que nasceram
};

EfeitosTracos efeitosDe(const DadosLinhagem& d, const std::vector<std::string>& tracos);
// Os dois traços de quem chega à maioridade: um herdado (de um dos genitores) e um do tutor leal (ou sorteado)
std::vector<std::string> tracosDaMaioridade(const DadosLinhagem& d, const std::vector<std::string>& genitor, const std::vector<std::string>& outro,
                                            const std::string& tracoDoTutor, uint32_t sorte);
std::string textoTracos(const DadosLinhagem& d, const std::vector<std::string>& tracos);  // "Valente, Sábio"

}
