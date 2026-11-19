#pragma once
#include <algorithm>
#include <deque>
#include <map>
#include <string>
#include <vector>

#include "Engine/Renderer/Renderer2D.h"
#include "Engine/Resources/ResourceManager.h"
#include "Engine/UI/BitmapFont.h"
#include "Game/UI/Cliques.h"
#include "Game/World/World.h"

namespace koa {

// Interface na tela (coordenadas da tela interna 640×360)
class Hud {
public:
    Hud(ke::ResourceManager& rec, const ke::BitmapFont& f, const GameData& dados) : rec_(rec), f_(f), dados_(dados) {}

    void atualizar(float dt);
    void aviso(std::string texto, glm::vec4 cor);
    void pulsar(const std::string& recurso) { pulso_[recurso] = 0.4f; }
    void acertarRelogio(const Relogio& r);  // fase 17: a hora que vai nas mensagens novas

    void desenhar(ke::Renderer2D& r, const World& w, float tempo);
    void painelConstrucao(ke::Renderer2D& r, const World& w, const std::string& id, const std::vector<std::string>& opcoes, int sel,
                          float tempo);
    void menuPausa(ke::Renderer2D& r, const std::vector<std::string>& opcoes, int selecionada);
    // `letras`: quantos caracteres da fala atual já apareceram
    void caixaDialogo(ke::Renderer2D& r, const Conversa& c, int linha, int letras, float tempo);
    static int tamanhoUtf8(const std::string& s);
    // Fase 5
    void menuObras(ke::Renderer2D& r, const World& w, const std::vector<std::string>& defs, int sel);
    void dicaColocacao(ke::Renderer2D& r, const std::string& nome, bool valido, const std::string& motivo);
    void painelReino(ke::Renderer2D& r, const World& w);
    void relatorioManha(ke::Renderer2D& r, const World& w);
    // Fase 6
    void combate(ke::Renderer2D& r, const World& w, float tempo);  // vida, chefe e missões ativas
    void lojaForja(ke::Renderer2D& r, const World& w, const std::vector<std::string>& itens, int sel);
    void diario(ke::Renderer2D& r, const World& w);
    // Fase 9
    void cronicas(ke::Renderer2D& r, const World& w, int aba, int rolagem, int selFragmento);
    void leitura(ke::Renderer2D& r, const PistaDef& p);
    void lojaMercador(ke::Renderer2D& r, const World& w, const NpcDef& npc, int sel);
    static int paginasVisiveis() { return 5; }  // quantas páginas das Crônicas cabem de uma vez
    // Fase 11
    void painelRegioes(ke::Renderer2D& r, const World& w);
    void painelEscolha(ke::Renderer2D& r, const World& w, const EscolhaDef& d, int sel, float tempo);
    static bool mostraCristal(const World& w);  // a linha de cristal aparece depois das minas
    // Fase 12: `proposto` é quem o herdeiro está olhando para o cargo selecionado (A/D), antes de nomear
    void painelConselho(ke::Renderer2D& r, const World& w, int sel, const std::string& proposto, float tempo);
    void painelDecisao(ke::Renderer2D& r, const World& w, const DecisaoDef& d, int sel, float tempo);
    std::string textoEfeito(const CargoDef& c, float valor) const;  // "+6 segurança", "+22% impostos"...
    // Fase 14: abas 0 expedição, 1 aventureiros, 2 relíquias. Na expedição, passo 0 escolhe o destino e 1 monta o grupo
    void painelGuilda(ke::Renderer2D& r, const World& w, int aba, int sel, int passo, const std::string& destino,
                      const std::vector<std::string>& grupo, float tempo);
    std::vector<std::string> aventureirosRecrutados(const World& w) const;  // na ordem dos dados
    // Fase 15: o fogão (receitas na ordem dos dados; o botão "Comer" usa o id 500)
    void painelCozinha(ke::Renderer2D& r, const World& w, int sel, float tempo);
    // Fase 16: alas do castelo (lista na ordem dos dados) e o editor do brasão (linhas 0..6; setas nos ids 700+linha e 720+linha,
    // "Salvar" no id 800)
    void painelAlas(ke::Renderer2D& r, const World& w, int sel, float tempo);
    void editorBrasao(ke::Renderer2D& r, const World& w, const Brasao& rascunho, int sel, float tempo);
    // Fase 17: o histórico dos avisos (mais novo primeiro), relido com N
    struct Mensagem {
        std::string texto;
        glm::vec4 cor;
        std::string quando;  // "Verão 5, 10:00"
    };
    const std::deque<Mensagem>& historico() const { return historico_; }
    void painelMensagens(ke::Renderer2D& r, int rolagem);
    // Fase 18: a ficha de um habitante e a lista do povo (ids das linhas = índice em World::povo)
    void fichaHabitante(ke::Renderer2D& r, const World& w, int id);
    void painelPovo(ke::Renderer2D& r, const World& w, int sel, int rolagem);
    static int linhasPovo() { return 18; }
    // Fase 19: os vizinhos (reputação, relações, tratados e missões). As linhas que se usam, na ordem:
    // "t:<tratado>" e "m:<missão>" (ids de clique = índice nesta lista)
    std::vector<std::string> acoesVizinhos() const;
    void painelVizinhos(ke::Renderer2D& r, const World& w, int sel, float tempo);
    // Fase 20: o mercado (linhas = World::bensNoMercado; ids de clique: 2k comprar, 2k+1 vender, 2000+k a linha;
    // no Corvin, abas 900/901 e as encomendas pelo índice)
    void painelMercado(ke::Renderer2D& r, const World& w, bool corvin, int aba, int sel, int rolagem, float tempo);
    static int linhasMercado() { return 12; }
    void marcaSimNao(ke::Renderer2D& r, glm::vec2 pos, bool sim);  // ✓ ou ✗ desenhados (além da cor)
    // Fase 20: o Livro do Reino (faixa de abas embaixo do painel; ids de clique 7100+aba) e o botão no canto (id 7000)
    static constexpr int ID_LIVRO = 7000, ID_ABA = 7100;
    void abasLivro(ke::Renderer2D& r, const std::vector<std::string>& nomes, const std::vector<std::string>& teclas, int ativa);
    // Dicas ao passar o mouse: as áreas entram enquanto a interface é desenhada; a dica vai por cima de tudo no fim
    void areaDica(glm::vec2 pos, glm::vec2 tam, std::string texto) { dicas_.push_back({pos, pos + tam, std::move(texto)}); }
    void desenharDica(ke::Renderer2D& r, glm::vec2 mouse);
    void limparDicas() { dicas_.clear(); }
    void definirEscalaTexto(int e) { escalaTexto_ = std::clamp(e, 1, 2); }
    int escalaTexto() const { return escalaTexto_; }
    void definirTeclaLivro(std::string t) { teclaLivro_ = std::move(t); }

    // Onde cada opção dos painéis apareceu no último quadro (a cena limpa antes de desenhar)
    Cliques cliques;

private:
    void painelClicavel(glm::vec2 p, glm::vec2 s) { cliques.area(p, s, Cliques::PAINEL); }
    void rodapeFechar(const std::string& texto, glm::vec2 centro) {
        const float l = static_cast<float>(f_.largura(texto));
        cliques.area(centro - glm::vec2(l / 2 + 4, 7), {l + 8, 14}, Cliques::FECHAR);
    }
    void custoEmLinha(ke::Renderer2D& r, const Custo& c, const Inventory& inv, glm::vec2 pos);
    void barra(ke::Renderer2D& r, glm::vec2 pos, float largura, float fracao, glm::vec4 cor);

public:

private:
    struct Aviso {
        std::string texto;
        glm::vec4 cor;
        float vida;
    };
    ke::ResourceManager& rec_;
    const ke::BitmapFont& f_;
    const GameData& dados_;
    std::vector<Aviso> avisos_;
    std::map<std::string, float> pulso_;
    std::deque<Mensagem> historico_;
    std::string quandoVisto_;  // o relógio do último quadro, já escrito (para datar as mensagens)
    float tempoDica_ = 12.0f;
    struct Dica {
        glm::vec2 min, max;
        std::string texto;
    };
    std::vector<Dica> dicas_;
    int escalaTexto_ = 1;  // fase 20: 2 = texto grande (conversas, avisos e dicas)
    std::string teclaLivro_ = "Tab";
};

}
