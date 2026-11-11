#pragma once
#include <filesystem>
#include <string>
#include <vector>

#include <entt/entity/registry.hpp>
#include <nlohmann/json_fwd.hpp>

#include "Engine/World/TileMap.h"
#include "Game/Data/GameData.h"
#include "Game/Inventory/Inventory.h"
#include "Game/Kingdom/Kingdom.h"
#include "Game/Player/Player.h"
#include "Game/Save/SaveSystem.h"

namespace koa {

// Tipos de terreno do tilemap
enum Terreno : uint16_t { T_GRAMA = 0, T_TERRA, T_LAMA, T_AGUA, T_CALCADA, T_MURO, T_QTD };

enum class TipoInteracao { Nenhuma, Cortar, Quebrar, Saquear, Construcao };

struct AlvoInteracao {
    entt::entity entidade = entt::null;
    TipoInteracao tipo = TipoInteracao::Nenhuma;
    std::string rotulo;  // "Cortar árvore", "Cabana Abandonada"...
    glm::vec2 ponto{0.0f};
};

// O que aconteceu neste passo, para a apresentação (partículas, textos, sons)
struct EventoJogo {
    enum class Tipo { Golpe, Esgotou, Ganhou, AbrirConstrucao, Construiu, Aviso };
    Tipo tipo;
    glm::vec2 pos{0.0f};
    std::string alvo;     // tipo do objeto ou id da construção
    std::string recurso;
    int quantidade = 0;
    std::string texto;
};

struct EntradaMundo {
    glm::vec2 mover{0.0f};
    bool interagir = false;            // toque em E
    bool interagirSegurando = false;   // E segurado: continua golpeando
};

// A partida em si: mapa, objetos (ECS), reino, herói e inventário. Sem janela nem OpenGL.
class World {
public:
    bool carregar(const GameData& dados, const std::filesystem::path& pastaData, std::string* erro = nullptr);
    bool carregarDeTexto(const GameData& dados, const std::string& mapaTxt, const nlohmann::json& def, std::string* erro = nullptr);

    void passo(float dt, const EntradaMundo& in);
    const AlvoInteracao& alvo() const { return alvo_; }
    bool melhorarConstrucao(const std::string& id);
    std::vector<EventoJogo> tirarEventos() { return std::exchange(eventos_, {}); }

    DadosSave capturar() const;
    void aplicar(const DadosSave& d);

    std::vector<ke::AABB> obstaculos() const;
    ke::AABB caixaConstrucao(const BuildingData& b) const;
    glm::vec2 pontoPorta(const BuildingData& b) const;
    const BuildingDef* def(const BuildingData& b) const;
    glm::vec2 tamanhoPixels() const { return {mapa.largura() * mapa.ladoTile(), mapa.altura() * mapa.ladoTile()}; }

    ke::TileMap mapa;
    entt::registry reg;
    Kingdom reino;
    Jogador jogador;
    Inventory inventario;
    glm::vec2 spawn{0.0f};
    double tempoJogado = 0.0;

private:
    void criarObjeto(char c, int x, int y);
    AlvoInteracao procurarAlvo() const;
    void executar(const AlvoInteracao& a);

    const GameData* dados_ = nullptr;
    AlvoInteracao alvo_;
    std::vector<EventoJogo> eventos_;
    float bufferInteragir_ = 0.0f;
};

uint32_t hashTile(int x, int y);

}
