#pragma once
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include <glm/vec2.hpp>
#include <nlohmann/json_fwd.hpp>

#include "Game/Kingdom/Clock.h"
#include "Game/Lore/Lore.h"
#include "Game/Player/Player.h"

namespace koa {

// ---------- Dados fixos (Data/NPCs e Data/Dialogues) ----------

struct ItemRotina {
    int minuto = 0;     // desde 0:00 (6:00 = 360)
    std::string local;  // id em "locais" do vilarejo
    std::string acao;   // texto ("trabalhar", "dormir"...)
};

struct CondicoesFala {
    std::optional<bool> primeiroEncontro, jaFalouHoje;
    std::optional<Periodo> periodo;
    std::optional<Estacao> estacao;
    int casaNivelMin = 0;
    int afinidadeMin = 0;
    int eraMin = 0;  // 1 Despertar, 2 Renascido, 3 Alta Fantasia
    // Fase 9: o mundo lembra
    std::string marco;     // só depois deste acontecimento
    std::string semMarco;  // só antes dele
    int pistasMin = 0;     // fragmentos da Queda já encontrados
};

struct EntradaFala {
    std::string id;  // obrigatório quando uma_vez
    bool umaVez = false;
    CondicoesFala se;
    std::vector<std::string> falas;
};

struct NpcDef {
    std::string id, nome, profissao, personalidade, casa;
    std::vector<std::string> gosta, odeia;
    std::vector<ItemRotina> rotina;  // em ordem de horário
    std::vector<EntradaFala> dialogos;
    // Fase 9: visitante (o mercador) só aparece a cada `visitaACada` dias, depois do marco `visitaRequer`
    std::string visitaRequer;
    int visitaACada = 0;
    std::vector<ItemLoja> loja;

    bool visitante() const { return visitaACada > 0; }
    // Item de rotina valendo no minuto dado (o último que já começou; antes do primeiro, o último do dia anterior)
    const ItemRotina* rotinaEm(float minuto) const;
};

bool lerNpcs(const nlohmann::json& npcs, const nlohmann::json& dialogos, std::map<std::string, NpcDef>& saida, std::string* erro);
int lerHora(const std::string& hhmm);  // "07:30" → 450

// ---------- Componentes ECS dos moradores ----------

struct Morador {
    std::string id;
    int indice = 0;  // para espalhar quem divide o mesmo local
};

struct Movimento {
    Direcao dir = Direcao::Baixo;
    bool andando = false;
    float tempoAnim = 0.0f;
    float velocidade = 38.0f;
    std::vector<glm::vec2> caminho;  // pontos em pixels, do próximo ao destino
    float acumuladoLod = 0.0f;        // simulação simplificada quando longe do herói
};

struct Agenda {
    int itemAtual = -1;  // índice na rotina
    bool dormindo = false;  // dentro de casa: não aparece
    bool conversando = false;  // parado ouvindo o herói
};

struct Relacionamento {
    int afinidade = 0;  // 0..1000 (100 = um coração)
    bool falouHoje = false;
    bool conheceu = false;
    std::set<std::string> memoria;  // falas "uma_vez" já ditas (acontecimentos que lembra)

    int coracoes() const { return afinidade / 100; }
};

// Aldeões sem nome: aparecem conforme a população cresce e passeiam pelo vilarejo
struct Aldeao {
    int variante = 0;
    float espera = 0.0f;  // segundos parado antes de escolher outro lugar
    bool dormindo = false;
};

// Situação do mundo para escolher a fala
struct ContextoFala {
    Periodo periodo = Periodo::Manha;
    Estacao estacao = Estacao::Primavera;
    int nivelCasa = 1;
    int era = 0;
    const std::set<std::string>* marcos = nullptr;
    int pistas = 0;
};

// Primeira entrada cujas condições batem (ou nenhuma)
const EntradaFala* escolherFala(const NpcDef& def, const Relacionamento& rel, const ContextoFala& ctx);

}
