#pragma once
#include <map>
#include <set>
#include <string>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <nlohmann/json_fwd.hpp>

#include "Game/Inventory/Inventory.h"
#include "Game/Player/Player.h"

namespace koa {

// ---------- Dados fixos (Data/Enemies, Data/Items/equipamentos, Data/Quests) ----------

struct InimigoDef {
    std::string id, nome;
    char letra = '?';
    int vida = 5, dano = 5, defesa = 0;
    float velocidade = 30.0f, visao = 100.0f;
    int ouroMin = 0, ouroMax = 0;
    glm::ivec2 tamanho{16, 16};
    bool chefe = false;
    std::string escolha;  // fase 11: chefe que se rende e deixa o herdeiro decidir (Data/Lore/escolhas.json)
    // 1.16: combate com escolhas
    std::string comportamento;  // "" (vai para cima), "distancia" (fica longe e atira) ou "bando" (chama os do mesmo tipo)
    struct Tiro {
        int dano = 0;
        float velocidade = 100.0f, recarga = 2.5f, alcance = 160.0f;
    } tiro;
    std::string aparencia;    // sprite de outra criatura ("" = o próprio)
    glm::vec3 tinta{1.0f};
    float escala = 1.0f;
    float fase2 = 0.0f;       // chefe: fração da vida em que muda de padrão (0 = nunca)
    std::string efeitoFase2;  // "investida" ou "brasa"
};

struct EquipDef {
    std::string id, nome, tipo, descricao;  // tipo: "arma" | "armadura"
    int dano = 0, defesa = 0;
    int forja = 0;  // nível mínimo da forja para fabricar (0 = não se fabrica)
    Custo custo;
    std::string efeito;  // 1.16: "queima" ou "atravessa"
};

struct ObjetivoMissao {
    std::string tipo, alvo;  // derrotar | construir | ter | visitar
    int quantidade = 1;
};

struct MissaoDef {
    std::string id, nome, npc, descricao, requer;
    ObjetivoMissao objetivo;
    Custo recompensa;  // recursos (ouro, comida...)
    int amizade = 0;
    std::string item;  // equipamento dado de presente
    std::string faccao;  // fase 19: missão de um povo vizinho (aceita no painel T; npc vazio)
    int relacao = 0;     // quanto a relação com a facção sobe ao cumprir
};

bool lerCombate(const nlohmann::json& inimigos, const nlohmann::json& equipamentos, const nlohmann::json& missoes,
                std::map<std::string, InimigoDef>& defsInimigos, std::map<std::string, EquipDef>& defsEquip,
                std::vector<std::string>& ordemEquip, std::vector<MissaoDef>& defsMissoes, std::string* erro);

// ---------- Componentes ECS ----------

struct Inimigo {
    std::string tipo;
    int vida = 1, vidaMax = 1;
    glm::vec2 origem{0.0f};       // onde nasceu (volta para lá se o herói fugir)
    glm::vec2 destino{0.0f};      // para onde vaga quando não está perseguindo
    glm::vec2 empurrao{0.0f};     // recuo depois de levar golpe
    Direcao dir = Direcao::Baixo;
    float espera = 0.0f;          // parado antes de vagar de novo
    float recarga = 0.0f;         // tempo até poder ferir de novo
    float piscar = 0.0f;
    float tempoAnim = 0.0f;
    float investida = 0.0f, recargaInvestida = 3.0f;  // chefe: arrancada
    int ultimoGolpe = -1;         // cada golpe do herói acerta uma vez só
    bool perseguindo = false;
    bool andando = false;
    bool rendido = false;         // chefe de joelhos, esperando a decisão do herdeiro
    bool horda = false;           // fase 13: marcha para o portão e a praça, em vez de vagar
    // 1.16
    int fase = 1;                 // chefe: 2 depois de perder vida o bastante
    float queimando = 0.0f, tickQueima = 0.0f;  // Lâmina da Chama
    float recargaTiro = 1.0f, recargaBrasa = 1.5f;
};

// 1.16: tiro de quem ataca de longe (o herói pode esquivar)
struct Projetil {
    glm::vec2 vel{0.0f};
    int dano = 0;
    float vida = 1.5f;
    std::string tipo;  // quem atirou (para a cor)
};

// 1.16: chão em brasa deixado pelo Golem na segunda fase
struct Brasa {
    float raio = 18.0f, vida = 4.0f, tick = 0.0f;
};

// Fase 13: soldado dos postos da guarda, que vai ao portão na noite da horda
struct Soldado {
    int vida = 40, vidaMax = 40;
    float recarga = 0.0f;
    glm::vec2 posto{0.0f};        // onde ele volta quando não há criatura por perto
    Direcao dir = Direcao::Baixo;
    bool andando = false;
    float tempoAnim = 0.0f, piscar = 0.0f;
};

// ---------- Estado do herói e das missões ----------

struct Equipamento {
    std::string arma = "espada_enferrujada", armadura = "roupa_exilio";
    std::set<std::string> possuidos{"espada_enferrujada", "roupa_exilio"};
};

struct EstadoMissao {
    bool concluida = false;
    int base = 0;  // contagem quando a missão começou (para "derrotar N")
};

// Dano final de um golpe: nunca menos que 1
inline int danoContra(int ataque, int defesa) { return ataque - defesa < 1 ? 1 : ataque - defesa; }

}
