#pragma once
#include <filesystem>
#include <string>
#include <vector>

#include <entt/entity/registry.hpp>
#include <nlohmann/json_fwd.hpp>

#include "Engine/World/TileMap.h"
#include <random>
#include <set>

#include "Game/Data/GameData.h"
#include "Game/Economy/Economia.h"
#include "Game/Inventory/Inventory.h"
#include "Game/Kingdom/Clock.h"
#include "Game/Kingdom/Kingdom.h"
#include "Game/NPC/Npc.h"
#include "Game/Player/Player.h"
#include "Game/Save/SaveSystem.h"

namespace koa {

// Tipos de terreno do tilemap
enum Terreno : uint16_t { T_GRAMA = 0, T_TERRA, T_LAMA, T_AGUA, T_CALCADA, T_MURO, T_QTD };

enum class TipoInteracao { Nenhuma, Cortar, Quebrar, Saquear, Construcao, Conversar };

// Uma conversa pronta para a caixa de diálogo
struct Conversa {
    std::string npc, nome, profissao;
    std::vector<std::string> falas;
    int coracoes = 0;
    bool ganhouAmizade = false;
    std::string missao;  // missão oferecida nesta conversa ("" = nenhuma)
};

struct AlvoInteracao {
    entt::entity entidade = entt::null;
    TipoInteracao tipo = TipoInteracao::Nenhuma;
    std::string rotulo;  // "Cortar árvore", "Cabana Abandonada"...
    glm::vec2 ponto{0.0f};
};

// O que aconteceu neste passo, para a apresentação (partículas, textos, sons)
struct EventoJogo {
    enum class Tipo {
        Golpe, Esgotou, Ganhou, AbrirConstrucao, Construiu, Aviso, AbrirConversa, Desmaiou, NovoDia, ObraIniciada,
        // Fase 6
        Ataque, AcertouInimigo, InimigoDerrotado, HeroiFerido, HeroiDerrotado, MudouRegiao, MissaoNova, MissaoConcluida, Comeu, Forjou,
        // Fase 7
        EraMudou
    };
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
    bool atacar = false;               // J: golpe de espada
    bool comer = false;                // H: comer para recuperar vida
};

// Saída de uma região para outra (pisar no retângulo leva ao destino)
struct Saida {
    glm::ivec2 tile{0}, tam{1};
    std::string destino;
    glm::ivec2 chegada{0};
    Direcao dir = Direcao::Baixo;
};

struct InfoRegiao {
    std::string id, nome;
    float escuridaoMin = 0.0f;      // a floresta é sempre meio escura; a cripta, quase breu
    glm::vec3 ambiente{1.0f};       // cor da penumbra da região
    bool hostil = false;
    std::vector<Saida> saidas;
    std::vector<std::pair<std::string, glm::ivec2>> spawns;  // inimigos (renascem a cada manhã)
    bool curaComEra = false;        // a floresta clareia conforme o reino prospera
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

    // Moradores
    Conversa conversar(const std::string& npc);   // escolhe a fala, ganha amizade (1×/dia), guarda lembranças
    void encerrarConversa(const std::string& npc);
    entt::entity morador(const std::string& id) const;
    void posicionarMoradores();                   // cada um no lugar da rotina na hora atual
    // Dormir (ou desmaiar às 2:00): novo dia às 6:00 na porta de casa
    void dormir(bool desmaio);
    bool aguardandoDesmaio() const { return desmaioPendente_; }

    // Obras: escolher o lugar e construir (fica pronta na manhã seguinte)
    bool podeConstruir(const std::string& def, glm::ivec2 tile, std::string* motivo = nullptr) const;
    std::string construir(const std::string& def, glm::ivec2 tile);  // id da obra, ou "" se não deu
    int contarConstrucoes(const std::string& def) const;
    // Guia dos primeiros dias: o que fazer agora ("" quando o herdeiro já se virou sozinho)
    std::string proximoPasso() const;
    ResumoReino resumo() const { return calcularResumo(reino, *dados_, estado); }
    void sincronizarAldeoes();  // tantos aldeões quanto a população pede

    // Regiões (vilarejo, floresta, cripta): só a ativa vive em `mapa`/`reg`
    bool noVilarejo() const { return regiaoAtual == "vilarejo"; }
    const InfoRegiao& infoAtual() const { return regioes.at(regiaoAtual); }
    void entrarRegiao(const std::string& id, glm::ivec2 chegada, Direcao dir);
    float escuridao() const;      // relógio + escuridão mínima da região
    glm::vec3 luzAmbiente() const;

    // Combate e equipamento
    int danoArma() const;
    int defesaArmadura() const;
    int nivelForja() const;       // maior nível de forja pronta (0 = nenhuma)
    bool forjar(const std::string& equip);
    // Mercado: troca ouro por um lote de 10 (pedra ou madeira); mais barato quanto maior o mercado
    int nivelMercado() const;
    int precoLote(const std::string& recurso) const;  // 0 = não vende
    bool comprarLote(const std::string& recurso);
    void reviver();               // depois de derrotado: acorda em casa no dia seguinte
    void renascerInimigos();      // de manhã as criaturas voltam (o chefe não, se já caiu)

    // Eras: a transformação se espalha em onda a partir da casa do herdeiro (nunca tudo de uma vez)
    int eraNoTile(glm::ivec2 t) const;
    int eraEm(glm::vec2 px) const { return eraNoTile({static_cast<int>(px.x) / 16, static_cast<int>(px.y - 1) / 16}); }

    // Missões
    std::vector<const MissaoDef*> missoesAtivas() const;
    int progressoMissao(const MissaoDef& m) const;

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
    Relogio relogio;
    std::map<std::string, glm::ivec2> locais;  // pontos das rotinas, em tiles
    EstadoReino estado;
    RelatorioDia ultimoRelatorio;  // da última noite
    // Fase 6
    std::string regiaoAtual = "vilarejo";
    std::map<std::string, InfoRegiao> regioes;
    Equipamento equip;
    std::map<std::string, EstadoMissao> missoes;  // começadas (ativas ou concluídas)
    std::map<std::string, int> abates;            // inimigos derrotados por tipo (desde sempre)
    std::set<std::string> visitadas;              // regiões já visitadas
    std::set<std::string> marcos;                 // acontecimentos únicos ("chefe_cavaleiro_caido")

private:
    bool lerMapa(const std::string& texto, InfoRegiao& info, std::string* erro);
    bool adicionarRegiao(const std::string& id, const std::string& texto, const nlohmann::json& j, std::string* erro);
    static InfoRegiao lerInfoRegiao(const std::string& id, const nlohmann::json& j);
    void criarInimigo(const std::string& tipo, glm::ivec2 tile);
    void atualizarInimigos(float dt, const std::vector<ke::AABB>& obst);
    void golpearComEspada();
    void ferirHeroi(int dano, glm::vec2 de);
    void verificarSaidas();
    void atualizarMissoes();
    void iniciarMissao(const MissaoDef& m);
    void criarObjeto(char c, int x, int y);
    void criarMoradores();
    void atualizarMoradores(float dt);
    void atualizarAldeoes(float dt);
    bool seguirCaminho(glm::vec2& pos, Movimento& mv, float dt);  // anda (com simulação simplificada se longe)
    entt::entity criarEntidadeConstrucao(const BuildingData& b);
    void caminharPara(entt::entity e, glm::ivec2 destino);
    glm::ivec2 destinoDe(entt::entity e, const ItemRotina& item) const;
    bool tileBloqueado(int x, int y);
    const GameData& dados() const { return *dados_; }
    AlvoInteracao procurarAlvo() const;
    void executar(const AlvoInteracao& a);

    const GameData* dados_ = nullptr;
    AlvoInteracao alvo_;
    std::vector<EventoJogo> eventos_;
    float bufferInteragir_ = 0.0f;
    bool desmaioPendente_ = false;
    std::vector<uint8_t> bloqueio_;  // grade do A* (tile sólido ou com obstáculo); vazia = refazer
    std::mt19937 rng_{2026};        // determinístico: os testes repetem igual
    struct RegiaoGuardada {
        ke::TileMap mapa;
        entt::registry reg;
    };
    std::map<std::string, RegiaoGuardada> guardadas_;  // regiões fora de cena
    std::string prefixoIds_;        // "floresta:" ao carregar outra região (ids únicos no save)
    bool derrotaPendente_ = false;
    glm::ivec2 centroReino_{32, 16};  // de onde a onda das eras parte
    int eraAnunciada_ = 0;          // última era anunciada (para avisar só na virada)
};

uint32_t hashTile(int x, int y);

}
