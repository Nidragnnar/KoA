#pragma once
#include <algorithm>
#include <future>
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

// Fase 17: cada tela por cima do jogo. A pilha de abertos é a única fonte: o relógio para e o alvo some com ela
enum class Painel {
    Pausa, Leitura, Relatorio, Conversa, Escolha, Decisao, Conselho, Guilda, Cozinha, Alas, Brasao,
    Regioes, Construcao, Forja, Loja, Cronicas, Diario, MenuObras, Reino, Mensagens, Ficha, Povo, Vizinhos,
    Mercado,  // fase 20
    Familia,  // fase 21
    Guarda    // 1.15
};

// O vilarejo jogável
class WorldScene : public ke::Scene {
public:
    WorldScene(Contexto& ctx, std::optional<DadosSave> save);
    ~WorldScene() override;  // espera a gravação em andamento

    void atualizar(double dt) override;
    void renderizar(float alfa) override;
    void interfaceDepuracao() override;
    ke::ColorGrade grade() const override;

    World& mundo() { return mundo_; }
    bool carregou() const { return carregou_; }
    const std::string& painelAberto() const { return painel_; }
    bool aberto(Painel p) const { return std::ranges::find(pilha_, p) != pilha_.end(); }
    const std::vector<Painel>& pilha() const { return pilha_; }
    bool pausado() const { return aberto(Painel::Pausa); }
    const std::optional<Conversa>& conversa() const { return conversa_; }
    bool emTransicao() const { return transicao_ >= 0.0f; }
    bool salvar();
    void esperarGravacao() { verificarGravacao(true); }  // o save no disco fica completo (o roteiro e a troca de cena usam)
    // Fase 5 (o roteiro de teste consulta)
    bool menuObrasAberto() const { return aberto(Painel::MenuObras); }
    const std::string& colocando() const { return colocando_; }
    glm::ivec2 tileFantasma() const;
    bool painelReinoAberto() const { return aberto(Painel::Reino); }
    bool relatorioAberto() const { return aberto(Painel::Relatorio); }
    // Fase 6
    bool diarioAberto() const { return aberto(Painel::Diario); }
    bool forjaAberta() const { return aberto(Painel::Forja); }
    std::vector<std::string> opcoesForja() const;
    // Fase 7
    void chamarDragao() { proximoDragao_ = 0.0f; }
    bool dragaoVoando() const { return dragaoAtivo_; }
    // Fase 9
    bool cronicasAbertas() const { return aberto(Painel::Cronicas); }
    const std::string& lendo() const { return leitura_; }
    const std::string& lojaAberta() const { return loja_; }
    // Fase 11
    bool painelRegioesAberto() const { return aberto(Painel::Regioes); }
    const std::string& escolhaAberta() const { return escolha_; }
    // Fase 12
    bool conselhoAberto() const { return aberto(Painel::Conselho); }
    bool decisaoAberta() const { return aberto(Painel::Decisao); }
    void avisar(const std::string& texto) { hud_.aviso(texto, cor::texto); }  // fase 20: o roteiro mostra o texto grande
    // Fase 14
    bool guildaAberta() const { return aberto(Painel::Guilda); }
    // Fase 15
    bool cozinhaAberta() const { return aberto(Painel::Cozinha); }
    // Fase 16
    bool alasAbertas() const { return aberto(Painel::Alas); }
    bool brasaoAberto() const { return aberto(Painel::Brasao); }
    const Brasao& rascunhoBrasao() const { return rascunho_; }

private:
    void entradaEscolha();
    void entradaConselho();
    void entradaDecisao();
    void entradaGuilda();
    void entradaCozinha();
    void entradaAlas();
    void entradaBrasao();
    void entradaMensagens();
    void entradaFicha();
    void entradaPovo();
    void entradaVizinhos();
    void entradaMercado();
    void abrirMercado(bool corvin, int aba);
    // Fase 20: o Livro do Reino (abas = painéis que já existiam)
    int abaAberta() const;  // -1 = nenhuma aba do Livro por cima
    void abrirAba(int k);
    bool entradaLivro();
    void entradaFamilia();  // fase 21
    void entradaRegioes();  // mundo vivo: os pedidos nas Terras
    void entradaGuarda();   // 1.15: a Guarda Real    // true = a faixa de abas tratou a entrada deste passo
    void desenharClima(ke::Renderer2D& r);  // fase 18: chuva, neve e raios por cima do mundo
    void verificarGravacao(bool esperar);
    void darPresenteNaConversa();
    std::string propostoConselho() const;  // candidato que o herdeiro está olhando no cargo selecionado
    std::vector<std::string> opcoesObras() const;
    void entradaMenuObras();
    void entradaColocacao();
    void entradaPainelReino();
    void entradaForja();
    void entradaCronicas();
    void entradaLoja();
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
    // Fase 17: abrir põe na pilha (uma vez); fechar tira e limpa o conteúdo do painel
    void abrir(Painel p) {
        if (!aberto(p)) pilha_.push_back(p);
    }
    void fechar(Painel p);

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
    std::vector<Painel> pilha_;  // fase 17: telas abertas, a de cima recebe a entrada
    struct Gravacao {
        std::string erro;  // "" = deu certo
        double ms = 0.0;
    };
    std::future<Gravacao> gravando_;  // fase 17: o salvamento em segundo plano
    double msCaptura_ = 0.0;
    int rolagemMensagens_ = 0;
    // Fase 18: a ficha de quem o herdeiro parou para conversar e a lista do povo (P)
    int fichaId_ = 0, selPovo_ = 0, rolagemPovo_ = 0;
    float proximoRaio_ = 4.0f;
    int selVizinhos_ = 0;  // fase 19
    bool mercadoCorvin_ = false;  // fase 20: o painel do mercado aberto na banca do Corvin (só vende e encomenda)
    int abaMercado_ = 0, selMercado_ = 0, rolagemMercado_ = 0;
    int abaLivro_ = 0;  // a última aba do Livro aberta
    int selGuarda_ = 0, confirmarDispensa_ = 0;  // 1.15
    int selRegioes_ = 0;  // mundo vivo: o pedido escolhido nas Terras
    int selFamilia_ = 0, confirmarSucessao_ = 0;  // fase 21: a linha escolhida; o filho cuja sucessão espera confirmação
    std::string painel_;  // id da construção com o painel aberto ("" = fechado)
    int selPainel_ = 0;
    int selPausa_ = 0;
    std::optional<Conversa> conversa_;
    int linhaFala_ = 0;
    float letras_ = 0.0f;
    int selObra_ = 0;
    std::string colocando_;  // definição sendo posicionada ("" = nenhuma)
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
    // Fase 9
    int abaCronicas_ = 0, rolagem_ = 0, selFragmento_ = 0;
    std::string leitura_;  // fragmento sendo lido ("" = nenhum)
    std::string loja_;     // banca aberta (id do mercador)
    int selLoja_ = 0;
    float tituloNovo_ = 0.0f;  // anúncio de título conquistado
    std::string nomeTitulo_;
    // Fase 10
    float tempoFolhas_ = 0.0f, tempoMeteoro_ = 0.0f, tempoFogos_ = 0.0f;
    ke::ParticleSystem ceu_;  // fogos e estrelas cadentes: desenhados depois da escuridão da noite (brilham de verdade)
    // Fase 11
    std::string escolha_;  // chefe rendido esperando a decisão ("" = nenhuma)
    int selEscolha_ = 0;
    float tempoEncanto_ = 0.0f;  // vagalumes e brilhos da terra purificada
    // Fase 12
    int selConselho_ = 0, selDecisao_ = 0;
    int proposta_ = -1;  // índice em [vago, candidatos...] para o cargo selecionado (-1 = quem já ocupa)
    // Fase 14: painel da guilda
    int abaGuilda_ = 0, selGuilda_ = 0, passoExpedicao_ = 0;
    std::string destinoSel_;
    std::vector<std::string> grupoSel_;
    // Fase 15: o fogão (cozinhar e comer pratos)
    int selCozinha_ = 0;
    // Fase 16: alas do castelo e o editor do brasão (o rascunho só vale ao salvar)
    int selAla_ = 0, selBrasao_ = 0;
    Brasao rascunho_;
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
