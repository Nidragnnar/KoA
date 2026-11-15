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
#include "Game/World/Regioes.h"

namespace koa {

// Tipos de terreno do tilemap
enum Terreno : uint16_t { T_GRAMA = 0, T_TERRA, T_LAMA, T_AGUA, T_CALCADA, T_MURO, T_QTD };

// Usar: altar e entulho (fase 11); Decidir: chefe rendido
enum class TipoInteracao { Nenhuma, Cortar, Quebrar, Saquear, Construcao, Conversar, Ler, Usar, Decidir };

// Uma conversa pronta para a caixa de diálogo
struct Conversa {
    std::string npc, nome, profissao;
    std::vector<std::string> falas;
    int coracoes = 0;
    bool ganhouAmizade = false;
    std::string missao;  // missão oferecida nesta conversa ("" = nenhuma)
    bool loja = false;   // ao fim da conversa abre a banca (mercador)
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
        EraMudou,
        // Fase 9
        Cronica, Titulo, LerPista,
        // Fase 10
        EventoRaro, Festival,
        // Fase 11
        AbrirEscolha, Transformou,
        // Fase 12
        Decidiu, Nomeou,
        // Fase 13
        HordaChegou, Flecha, PortaoGolpeado, PortaoCaiu, Saque, SoldadoCaiu, BatalhaFim,
        // Fase 14
        Partiu, Reliquia
    };
    Tipo tipo;
    glm::vec2 pos{0.0f};
    std::string alvo;     // tipo do objeto ou id da construção
    std::string recurso;
    int quantidade = 0;
    std::string texto;
    glm::vec2 ate{0.0f};  // fase 13: para onde a flecha voou
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
    std::string requer;  // marco para a passagem estar aberta ("" = sempre)
};

struct InfoRegiao {
    std::string id, nome;
    float escuridaoMin = 0.0f;      // a floresta é sempre meio escura; a cripta, quase breu
    glm::vec3 ambiente{1.0f};       // cor da penumbra da região
    bool hostil = false;
    std::vector<Saida> saidas;
    std::vector<std::pair<std::string, glm::ivec2>> spawns;  // inimigos (renascem a cada manhã)
    bool curaComEra = false;        // a floresta clareia conforme o reino prospera
    VidaRegiaoDef vida;             // fase 11: corrupção, segurança, prosperidade e transformações
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
    ResumoReino resumo() const { return calcularResumo(reino, *dados_, estado, modificadorReino(relogio.estacao())); }
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

    // Fase 9: Crônicas, títulos, o mistério, o mercador e os monumentos
    // Registra um acontecimento (uma vez só); se ele tiver página, entra nas Crônicas. true = era novo
    bool marcar(const std::string& marco, const std::string& nome = "", bool silencioso = false);
    const TituloDef& titulo() const;  // o mais alto conquistado
    int pistasEncontradas() const;
    bool visitanteHoje(const NpcDef& d) const;  // o mercador está no vilarejo hoje?
    bool comprar(const std::string& npc, const std::string& item);
    bool itemEsgotado(const ItemLoja& i) const { return !i.pista.empty() && marcos.contains("pista:" + i.pista); }
    bool monumentoLiberado(const BuildingDef& d) const { return d.requer.empty() || marcos.contains(d.requer); }
    std::vector<std::string> inscricao(const BuildingData& b) const;  // o que o monumento registra da história real
    std::vector<EntradaCronica> cronicas;

    // Fase 10: estações, acontecimentos raros e o festival (tudo derivado do dia: não vai para o save)
    const EstacaoDef& estacaoAtual() const { return dados_->estacoes.de(relogio.estacao()); }
    const EventoDef* eventoDoDia(int dia) const;  // sorteio fixo por dia ("" nos primeiros dias e no festival)
    const EventoDef* eventoHoje() const { return eventoDoDia(relogio.dia); }
    bool eventoAgora(const std::string& id) const;  // o acontecimento de hoje já começou (à noite)?
    bool festivalNoDia(int dia) const;
    bool festivalHoje() const { return festivalNoDia(relogio.dia); }
    bool festivalAgora() const;
    float forcaInimigos() const;  // dano das criaturas: estação × lua de sangue

    // Fase 11: regiões vivas, regiões que se transformam e chefes com escolha
    std::map<std::string, EstadoRegiao> estadoRegioes;  // só as regiões com "vida"
    const TransformacaoRegiao* transformacao(const std::string& regiao) const;  // a que vale agora (nullptr = original)
    std::string nomeRegiao(const std::string& regiao) const;
    bool regiaoHostil() const;    // a região ativa (a floresta purificada deixa de ser)
    float escuridaoMinAtual() const;
    ModificadorDia modificadorReino(Estacao e) const;  // estação + efeitos das regiões (o pacto pesa no povo)
    std::vector<std::string> regioesComVida() const;   // na ordem do mapa
    bool usarCoracao();           // o rito do Coração da Floresta
    bool escorarEntulho();        // abre a passagem das minas
    const EscolhaDef* escolhaDe(entt::entity e) const;
    bool opcaoDisponivel(const OpcaoEscolha& o) const { return o.requer.empty() || marcos.contains(o.requer); }
    bool escolher(const std::string& escolha, const std::string& opcao);
    static constexpr int MADEIRA_ENTULHO = 15;

    // Fase 12: o Conselho Real e as decisões políticas
    std::map<std::string, std::string> conselho;  // cargo → morador (sem entrada = vago)
    std::map<std::string, int> lealdade;          // morador → 0..100 (quem já foi nomeado alguma vez)
    std::set<std::string> leis;                   // leis valendo
    std::string decisaoPendente;                  // caso esperando o herdeiro ("" = nenhum)
    int ultimaDecisao = 0;                        // dia em que o último caso chegou
    bool conselhoFormado() const { return marcos.contains("conselho_formado"); }
    std::string ocupante(const std::string& cargo) const;
    std::string cargoDe(const std::string& npc) const;
    std::vector<std::string> candidatos() const;  // moradores que podem ser nomeados (o mercador não)
    bool nomear(const std::string& cargo, const std::string& npc);  // "" deixa o cargo vago
    float efeitoCargo(const std::string& cargo) const;  // o que o cargo rende hoje (0 = vago ou insatisfeito)
    const DecisaoDef* decisaoAtual() const { return decisaoPendente.empty() ? nullptr : dados_->conselho.decisao(decisaoPendente); }
    bool decisaoPossivel(const DecisaoDef& d) const;
    bool decidir(const std::string& opcao);
    void trazerDecisao(const std::string& id);    // o conselho traz este caso agora (testes e roteiro)

    // Fase 13: hordas e a defesa do portão leste
    int hordaDia = 0;        // dia do ataque avisado (0 = nenhum)
    int hordaNumero = 0;     // hordas que já vieram
    int proximaHorda = 0;    // a próxima não é avisada antes deste dia
    int portaoVida = 0;      // vida do portão da Muralha Leste
    std::vector<std::string> notasDefesa;  // o que a batalha deixou para o relatório da manhã
    const BuildingData* muralha() const;
    int portaoVidaMax() const;
    bool portaoDePe() const { return portaoVidaMax() > 0 && portaoVida > 0; }
    ke::AABB caixaPortao() const;           // o vão do portão na muralha (só segura criaturas)
    int forcaProximaHorda() const;
    int defesaDoReino() const;              // a conta da batalha sem o herdeiro
    void agendarHorda(int dia);             // os batedores avisam (testes e roteiro)
    void iniciarBatalha();
    bool emBatalha() const { return batalha_; }
    int criaturasDaHorda() const;
    int soldadosNoPortao() const;
    Custo custoReparo() const;
    bool repararPortao();

    // Fase 14: a Guilda dos Aventureiros, expedições e relíquias
    std::map<std::string, EstadoAventureiro> aventureiros;  // recrutados pela guilda
    Expedicao expedicao;                                    // destino vazio = nenhuma em curso
    std::vector<EstadoReliquia> reliquias;                  // na ordem em que chegaram
    int nivelGuilda() const;                                // 0 = sem guilda pronta
    int vagasReliquias() const;
    int reliquiasExpostas() const;
    bool aventureiroPronto(const std::string& id) const;
    std::string situacaoAventureiro(const std::string& id) const;  // "pronto", "ferido por 2 dias"...
    int nivelAventureiro(const std::string& id) const;
    bool destinoAberto(const DestinoDef& d) const;
    int comidaExpedicao(const DestinoDef& d, int tamanho) const;
    int margemSemSorte(const DestinoDef& d, const std::vector<std::string>& grupo) const;
    bool partir(const std::string& destino, const std::vector<std::string>& grupo);
    void ganharReliquia(const std::string& id);
    bool exporReliquia(const std::string& id, bool expor);
    bool purificarReliquia(const std::string& id);
    EfeitosReliquia efeitosReliquias() const;  // somados das expostas (com as maldições que valem)
    const EstadoReliquia* estadoReliquia(const std::string& id) const;

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
    // Fase 9
    void verificarTitulos(bool silencioso);
    void atualizarPistas();       // fragmentos visíveis na região ativa (os já lidos somem)
    void reconstruirCronicas();   // save antigo: páginas do que já tinha acontecido
    int contarObrasProntas(bool tipos) const;
    // Fase 10
    void atualizarMeteorito();  // a estrela caída na noite de meteoros (some no dia seguinte)
    void notasDaManha(int ontem);
    int avisoEvento_ = 0, avisoFestival_ = 0;  // dia em que já foi anunciado
    // Fase 11
    void atualizarEspeciais();    // altar (cara de corrompido ou purificado), entulho e enfeites da região transformada
    void noiteDasRegioes(std::vector<std::pair<std::string, int>>& producao, std::vector<std::string>& notas);
    void receberItem(const std::string& item);
    void anunciarTransformacao(const std::string& regiao);
    // Fase 12
    void noiteDoConselho(std::vector<std::string>& notas);
    Relacionamento* relacao(const std::string& npc);  // o morador pode estar em qualquer registro
    // Fase 13
    entt::entity criarInimigoEm(const std::string& tipo, glm::vec2 pos);
    void derrotarInimigo(entt::entity e);  // ouro, abates, Crônicas; some do mapa
    void atualizarDefesa(float dt);    // torres, soldados e o fim da batalha
    void atualizarSoldados(float dt);
    void terminarBatalha();
    void resolverHordaSemHeroi(const std::string& motivo);
    void saquear(glm::vec2 onde);      // uma criatura chegou à praça
    void manhaDaDefesa();              // avisos de horda e o conserto do Mestre das Obras
    int danoTorres() const;
    int soldadosDosPostos() const;
    // Fase 14
    void manhaDaGuilda(std::vector<std::string>& notas);
    void voltaDaExpedicao(std::vector<std::string>& notas);
    bool batalha_ = false;
    int vencidasNaBatalha_ = 0, saquesNaBatalha_ = 0;
    std::map<std::string, float> recargaTorre_;  // id da torre → segundos até a próxima flecha

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
