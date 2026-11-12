#pragma once
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <glm/vec2.hpp>
#include <nlohmann/json_fwd.hpp>

namespace koa {

constexpr int VERSAO_SAVE = 3;  // sobe quando o formato muda (com migração). v2: relógio e moradores; v3: obras e povo

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
