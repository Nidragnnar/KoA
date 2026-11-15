#pragma once
#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>

namespace koa {

struct Contexto;
class TitleScene;
class WorldScene;
class World;

// Teste ponta a ponta (--roteiro <pasta>): joga sozinho pelo caminho do protótipo 0.1,
// tira capturas em cada etapa e confere o resultado. Sai com código 0 se tudo passou.
class Roteiro {
public:
    Roteiro(Contexto& ctx, std::filesystem::path pastaCapturas) : ctx_(ctx), pasta_(std::move(pastaCapturas)) {}

    void naTitulo(TitleScene& t);
    void noMundo(WorldScene& w);

private:
    void capturar(const std::string& nome);
    void verificar(bool ok, const std::string& descricao);
    void proxima() { ++etapa_; quadro_ = 0; }
    void terminar();
    bool posicionarJunto(World& w, entt::entity e);
    entt::entity proximoAlvo(World& w);

    Contexto& ctx_;
    std::filesystem::path pasta_;
    int fase_ = 0;       // 0: primeira partida; 1: depois de voltar ao título e continuar
    int etapa_ = 0;
    int quadro_ = 0;     // passos de lógica dentro da etapa
    int quadrosTotais_ = 0;
    bool aguardando_ = false;
    bool terminou_ = false;
    int falhas_ = 0;
    std::vector<std::string> relatorio_;
    entt::entity alvo_ = entt::null;
    float xInicial_ = 0.0f, yInicial_ = 0.0f;
    glm::ivec2 alvoObra_{0};
    int comidaAntes_ = 0, popAntes_ = 0;
    int abatesAntes_ = 0, vidaAntes_ = 0, diaAntes_ = 0;
    bool trocouRegiao_ = false;
    std::map<std::string, int> inventarioSalvo_;
    int esgotadosSalvos_ = 0;
    // Fase 9
    int etapaLoja_ = 0;     // quadro em que a banca do mercador abriu
    std::string memorial_;  // id do memorial construído
};

}
