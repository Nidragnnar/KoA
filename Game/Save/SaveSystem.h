#pragma once
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <glm/vec2.hpp>
#include <nlohmann/json_fwd.hpp>

#include "Game/Kingdom/Guilda.h"
#include "Game/Lore/Lore.h"
#include "Game/World/Regioes.h"

namespace koa {

// Sobe quando o formato muda (com migração). v2: relógio e moradores; v3: obras e povo; v4: regiões, combate e missões;
// v5: Crônicas do Reino; v6: regiões vivas (corrupção, segurança, prosperidade); v7: Conselho Real e leis;
// v8: hordas e a defesa do portão; v9: guilda, expedições e relíquias
constexpr int VERSAO_SAVE = 9;

struct EstadoConstrucao {
    std::string id, def;
    glm::ivec2 tile{0};
    bool temPosicao = false;  // saves antigos só guardavam o nível das construções fixas
    int nivel = 1;
    bool construida = true;
};

struct EstadoMorador {
    std::string id;
    int afinidade = 0;
    bool falouHoje = false;
    bool conheceu = false;
    std::vector<std::string> memoria;
};

struct EstadoObjeto {
    std::string id;
    int hp = 0;
    bool esgotado = false;
    bool aberto = false;
    int diaEsgotado = 0;  // para renascer na hora certa (saves antigos: 0 = renasce logo)
};

// Tudo que muda numa partida (os mapas e assets originais não vão para o save)
struct DadosSave {
    // metadata.json
    int versao = VERSAO_SAVE;
    std::string versaoJogo;
    std::string dataHora;
    double tempoJogado = 0.0;
    // player.json
    glm::vec2 posJogador{0.0f};
    int direcao = 0;
    // inventory.json
    std::map<std::string, int> inventario;
    // kingdom.json
    std::vector<EstadoConstrucao> construcoes;
    float eraVisual = 0.0f;
    bool temPovo = false;  // saves antigos não tinham: fica o povo inicial do mapa
    int populacao = 8;
    float felicidade = 40.0f;
    int imposto = 1;
    // world.json (relógio + só objetos alterados)
    int dia = 1;
    float minutos = 360.0f;
    std::vector<EstadoObjeto> objetos;
    // npcs.json (conheceu, memória) + relationships.json (afinidade)
    std::vector<EstadoMorador> moradores;
    // player.json (v4): região, vida e equipamento
    std::string regiao = "vilarejo";
    int vida = 100;
    std::string arma = "espada_enferrujada", armadura = "roupa_exilio";
    std::vector<std::string> equipamentos{"espada_enferrujada", "roupa_exilio"};
    // quests.json (v4)
    std::map<std::string, std::pair<bool, int>> missoes;  // id → (concluída, base)
    std::map<std::string, int> abates;
    std::vector<std::string> visitadas{"vilarejo"};
    std::vector<std::string> marcos;
    // chronicles.json (v5). Saves antigos não têm: as Crônicas são refeitas pelo que já aconteceu
    bool temCronicas = false;
    std::vector<EntradaCronica> cronicas;
    // regions.json (v6). Saves antigos não têm: cada região começa como no mapa, visitada hoje
    bool temRegioes = false;
    std::map<std::string, EstadoRegiao> regioes;
    // council.json (v7). Saves antigos: sem conselho (ele se forma na próxima manhã se o reino já tiver obras)
    std::map<std::string, std::string> conselho;  // cargo → morador
    std::map<std::string, int> lealdade;
    std::vector<std::string> leis;
    std::string decisaoPendente;
    int ultimaDecisao = 0;
    // defense.json (v8). Saves antigos: nenhuma horda avisada e o portão inteiro
    bool temDefesa = false;
    int hordaDia = 0, hordaNumero = 0, proximaHorda = 0, portaoVida = 0;
    std::vector<std::string> notasDefesa;
    // guild.json (v9). Saves antigos: sem aventureiros, sem expedição e sem relíquias
    std::map<std::string, EstadoAventureiro> aventureiros;
    Expedicao expedicao;
    std::vector<EstadoReliquia> reliquias;
};

// Saves em pastas (Saves/Save_001/*.json) com backup antes de sobrescrever
class SaveSystem {
public:
    explicit SaveSystem(std::filesystem::path pasta) : pasta_(std::move(pasta)) {}

    bool existe(const std::string& slot = "Save_001") const;
    bool salvar(const DadosSave& d, const std::string& slot = "Save_001", std::string* erro = nullptr);
    // Se o save estiver corrompido, tenta o backup (e avisa em `aviso`)
    std::optional<DadosSave> carregar(const std::string& slot = "Save_001", std::string* erro = nullptr, std::string* aviso = nullptr);
    const std::filesystem::path& pasta() const { return pasta_; }

    // Converte saves antigos para o formato atual (v1 é o primeiro; fica pronto para os próximos)
    static nlohmann::json migrar(nlohmann::json tudo, int deVersao);

private:
    std::optional<DadosSave> lerPasta(const std::filesystem::path& p, std::string* erro) const;
    std::filesystem::path pasta_;
};

}
