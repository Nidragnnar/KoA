#pragma once
#include <optional>
#include <random>
#include <string>
#include <vector>

#include "Engine/Core/SceneManager.h"
#include "Engine/Renderer/Camera2D.h"
#include "Engine/Renderer/Lighting.h"
#include "Engine/Renderer/ParticleSystem.h"
#include "Game/App/Contexto.h"
#include "Game/Render/WorldRenderer.h"
#include "Game/UI/Hud.h"
#include "Game/UI/Menus.h"
#include "Game/World/World.h"

namespace koa {

// O vilarejo jogável
class WorldScene : public ke::Scene {
public:
    WorldScene(Contexto& ctx, std::optional<DadosSave> save);

    void atualizar(double dt) override;
    void renderizar(float alfa) override;
    void interfaceDepuracao() override;
    ke::ColorGrade grade() const override;

    World& mundo() { return mundo_; }
    bool carregou() const { return carregou_; }
    const std::string& painelAberto() const { return painel_; }
    bool pausado() const { return pausa_; }
    const std::optional<Conversa>& conversa() const { return conversa_; }
    bool emTransicao() const { return transicao_ >= 0.0f; }
    bool salvar();
    // Fase 5 (o roteiro de teste consulta)
    bool menuObrasAberto() const { return menuObras_; }
    const std::string& colocando() const { return colocando_; }
    glm::ivec2 tileFantasma() const;
    bool painelReinoAberto() const { return painelReino_; }
    bool relatorioAberto() const { return relatorio_; }
    // Fase 6
    bool diarioAberto() const { return diario_; }
    bool forjaAberta() const { return forja_; }
    std::vector<std::string> opcoesForja() const;
    // Fase 7
    void chamarDragao() { proximoDragao_ = 0.0f; }
    bool dragaoVoando() const { return dragaoAtivo_; }

private:
    std::vector<std::string> opcoesObras() const;
    void entradaMenuObras();
    void entradaColocacao();
    void entradaPainelReino();
    void entradaForja();
    std::vector<std::string> opcoesPainel() const;
    void entradaJogo();
    void entradaPainel();
    void entradaPausa();
    void entradaDialogo(float dt);
    void tratarEventos();
    void efeitosAmbiente(float dt);
    void iniciarSono(bool desmaio);
    void poeira(glm::vec2 centro, glm::vec2 meia, int n, glm::vec4 cor);
    void textoFlutuante(glm::vec2 pos, std::string t, glm::vec4 c) { flutuantes_.push_back({pos, std::move(t), c, 1.4f}); }

    Contexto& ctx_;
    World mundo_;
    WorldRenderer rend_;
    Hud hud_;
    TelaOpcoes telaOpcoes_;
    ke::Camera2D cam_;
    ke::ParticleSystem part_;
    ke::LightMap luz_;
    std::mt19937 rng_{42};

    bool carregou_ = false;
    std::string painel_;  // id da construção com o painel aberto ("" = fechado)
    int selPainel_ = 0;
    bool pausa_ = false;
    int selPausa_ = 0;
    std::optional<Conversa> conversa_;
    int linhaFala_ = 0;
    float letras_ = 0.0f;
    bool menuObras_ = false;
    int selObra_ = 0;
    std::string colocando_;  // definição sendo posicionada ("" = nenhuma)
    bool painelReino_ = false;
    bool relatorio_ = false;
    bool diario_ = false;
    bool forja_ = false;
    int selForja_ = 0;
    bool derrota_ = false;       // a transição em curso é de derrota em combate
    float feridoFlash_ = 0.0f;   // tela avermelhada ao levar dano
    float tituloRegiao_ = 0.0f;  // nome da região aparece grande ao entrar
    std::string nomeRegiao_;
    // Fase 7
    float tituloEra_ = 0.0f;     // anúncio da nova era
    std::string nomeEra_;
    float tempoAtmosfera_ = 0.0f;
    bool dragaoAtivo_ = false;  // sobrevoo na Alta Fantasia
    float dragaoX_ = 0.0f, dragaoY_ = 0.0f, proximoDragao_ = 20.0f;
    float transicao_ = -1.0f;  // -1 = nenhuma; 0..2 = escurece, troca o dia (em 1), clareia
    bool desmaio_ = false;
    bool mostrarColisoes_ = false;
    float tempo_ = 0.0f, flash_ = 0.0f, tempoFumaca_ = 0.0f, tempoCinzas_ = 0.0f;

    struct Flutuante {
        glm::vec2 pos;
        std::string texto;
        glm::vec4 cor;
        float vida;
    };
    std::vector<Flutuante> flutuantes_;
};

}
