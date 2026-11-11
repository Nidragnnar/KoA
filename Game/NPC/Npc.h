#pragma once
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include <glm/vec2.hpp>
#include <nlohmann/json_fwd.hpp>

#include "Game/Kingdom/Clock.h"
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

// Situação do mundo para escolher a fala
struct ContextoFala {
    Periodo periodo = Periodo::Manha;
    Estacao estacao = Estacao::Primavera;
    int nivelCasa = 1;
};

// Primeira entrada cujas condições batem (ou nenhuma)
const EntradaFala* escolherFala(const NpcDef& def, const Relacionamento& rel, const ContextoFala& ctx);

}
