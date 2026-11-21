// Combate com escolhas (1.16): esquiva, golpe carregado, efeito das armas, inimigos à distância e em bando, chefes com fases
#include <gtest/gtest.h>

#include <glm/geometric.hpp>

#include "Game/Exploration/Components.h"
#include "Game/World/World.h"

using namespace koa;

namespace {

const GameData& dados() {
    static const GameData d = [] {
        GameData g;
        std::string erro;
        EXPECT_TRUE(g.carregar(PASTA_DATA, &erro)) << erro;
        return g;
    }();
    return d;
}

World vilarejo() {
    World w;
    std::string erro;
    EXPECT_TRUE(w.carregar(dados(), PASTA_DATA, &erro)) << erro;
    w.inventario.definir("comida", 300);
    return w;
}

void passos(World& w, int n, EntradaMundo e = {}) {
    for (int i = 0; i < n; ++i) w.passo(1.0f / 60.0f, e);
}

entt::entity primeiro(World& w, const std::string& tipo) {
    for (auto [e, p, i] : w.reg.view<Posicao, Inimigo>().each())
        if (i.tipo == tipo) return e;
    return entt::null;
}

// Um tiro parado em cima do herói (acerta no próximo passo)
void tiroNoHeroi(World& w, int dano) {
    const auto t = w.reg.create();
    w.reg.emplace<Posicao>(t, w.jogador.pos - glm::vec2(0, 7));
    w.reg.emplace<Projetil>(t, Projetil{{0.0f, 0.0f}, dano, 1.0f, "arqueiro_espectral"});
}

// A criatura fica na frente do herói (que olha para cima), sem reagir
Inimigo& naFrente(World& w, entt::entity e, int vida = 100) {
    w.jogador.dir = Direcao::Cima;
    auto& p = w.reg.get<Posicao>(e);
    p.v = w.jogador.pos - glm::vec2(0, 14);
    auto& i = w.reg.get<Inimigo>(e);
    i.vida = i.vidaMax = vida;
    i.recarga = 99.0f;
    i.recargaTiro = 99.0f;
    return i;
}

}

TEST(CombateEscolhas, EsquivaAnulaOGolpeNaJanelaCerta) {
    World w = vilarejo();
    const int vida = w.jogador.vida;
    // Dentro da janela (o começo da esquiva): o tiro não fere
    w.jogador.esquiva = 0.2f;
    tiroNoHeroi(w, 10);
    passos(w, 1);
    EXPECT_EQ(w.jogador.vida, vida);
    EXPECT_TRUE(std::ranges::any_of(w.tirarEventos(), [](const EventoJogo& e) { return e.tipo == EventoJogo::Tipo::Esquivou && e.alvo == "desviou"; }));
    // No fim da esquiva (fora da janela): fere
    w.jogador.esquiva = 0.03f;
    tiroNoHeroi(w, 10);
    passos(w, 1);
    EXPECT_LT(w.jogador.vida, vida);
    // A esquiva pela entrada: impulso, recarga e invulnerável logo depois
    World e = vilarejo();
    const glm::vec2 antes = e.jogador.pos;
    EntradaMundo esq;
    esq.esquivar = true;
    esq.mover = {1, 0};
    passos(e, 1, esq);
    EXPECT_TRUE(e.jogador.esquivando());
    passos(e, 20);  // a esquiva dura 0,3 s
    EXPECT_FALSE(e.jogador.esquivando());
    EXPECT_GT(e.jogador.pos.x - antes.x, 30.0f) << "o impulso leva longe";
    EXPECT_GT(e.jogador.recargaEsquiva, 0.0f);
    passos(e, 1, esq);
    EXPECT_FALSE(e.jogador.esquivando()) << "não esquiva de novo antes da recarga";
}

TEST(CombateEscolhas, GolpeCarregadoDaODobro) {
    World w = vilarejo();
    w.entrarRegiao("floresta", {10, 10}, Direcao::Baixo);
    const entt::entity e = primeiro(w, "sombra");
    ASSERT_TRUE(e != entt::null);
    Inimigo& i = naFrente(w, e);
    EntradaMundo ataque;
    ataque.atacar = ataque.atacarSegurando = true;
    passos(w, 1, ataque);
    const int normal = 100 - i.vida;
    EXPECT_EQ(normal, w.danoArma());
    // Segura depois do golpe até a carga encher e solta
    naFrente(w, e);
    EntradaMundo segura;
    segura.atacarSegurando = true;
    passos(w, 60, segura);
    EXPECT_GE(w.jogador.carga, Jogador::CARGA_CHEIA);
    const int antes = i.vida;
    passos(w, 1);  // soltou
    EXPECT_EQ(antes - i.vida, 2 * w.danoArma());
    EXPECT_TRUE(w.jogador.carregado);
}

TEST(CombateEscolhas, EfeitosDasArmas) {
    World w = vilarejo();
    w.entrarRegiao("floresta", {10, 10}, Direcao::Baixo);
    // Lâmina da Chama: queima depois do corte (2 a cada meio segundo, por 3 segundos)
    w.equip.possuidos.insert("lamina_chama");
    w.equip.arma = "lamina_chama";
    const entt::entity e = primeiro(w, "sombra");
    ASSERT_TRUE(e != entt::null);
    Inimigo& i = naFrente(w, e, 200);
    w.golpearComEspada();
    const int cortado = i.vida;
    EXPECT_FLOAT_EQ(i.queimando, 3.0f);
    passos(w, 60 * 3 + 10);
    EXPECT_EQ(cortado - i.vida, 12) << "seis queimaduras de 2";
    // Lâmina de Cristal: atravessa a defesa
    World c = vilarejo();
    c.entrarRegiao("floresta", {10, 10}, Direcao::Baixo);
    c.equip.possuidos.insert("espada_cristal");
    c.equip.arma = "espada_cristal";
    const entt::entity l = primeiro(c, "lobo");  // defesa 1
    ASSERT_TRUE(l != entt::null);
    Inimigo& lobo = naFrente(c, l, 200);
    c.golpearComEspada();
    EXPECT_EQ(200 - lobo.vida, c.danoArma());
    EXPECT_EQ(c.efeitoArma(), "atravessa");
}

TEST(CombateEscolhas, InimigosADistanciaEEmBando) {
    World w = vilarejo();
    w.entrarRegiao("floresta", {10, 10}, Direcao::Baixo);
    const entt::entity a = primeiro(w, "arqueiro_espectral");
    ASSERT_TRUE(a != entt::null);
    auto& pa = w.reg.get<Posicao>(a);
    w.jogador.pos = pa.v + glm::vec2(110, 0);
    w.jogador.invulneravel = 99.0f;  // só para contar os tiros
    int tiros = 0;
    for (int k = 0; k < 60 * 4; ++k) {
        passos(w, 1);
        for (const auto& ev : w.tirarEventos()) tiros += ev.tipo == EventoJogo::Tipo::Tiro;
    }
    EXPECT_GE(tiros, 1) << "atira de longe";
    // O herói chega perto: o arqueiro recua
    w.jogador.pos = pa.v + glm::vec2(30, 0);
    const float perto = glm::distance(pa.v, w.jogador.pos);
    passos(w, 30);
    EXPECT_GT(glm::distance(pa.v, w.jogador.pos), perto);
    // Bando: um lobo da matilha vê o herói e os outros vêm junto
    World b = vilarejo();
    b.entrarRegiao("floresta", {10, 10}, Direcao::Baixo);
    std::vector<entt::entity> matilha;
    for (auto [e, p, i] : b.reg.view<Posicao, Inimigo>().each())
        if (i.tipo == "matilha") matilha.push_back(e);
    ASSERT_GE(matilha.size(), 2u);
    const glm::vec2 lider = b.reg.get<Posicao>(matilha[0]).v;
    for (size_t k = 0; k < matilha.size(); ++k) b.reg.get<Posicao>(matilha[k]).v = lider + glm::vec2(static_cast<float>(k) * 30.0f, 0);
    b.jogador.pos = lider - glm::vec2(60, 0);  // só o primeiro enxerga
    b.jogador.invulneravel = 99.0f;
    passos(b, 2);
    for (const entt::entity e : matilha) EXPECT_TRUE(b.reg.get<Inimigo>(e).perseguindo);
}

TEST(CombateEscolhas, ChefesMudamDePadraoNaVidaCerta) {
    World w = vilarejo();
    w.entrarRegiao("cripta", {10, 10}, Direcao::Baixo);
    const entt::entity c = primeiro(w, "cavaleiro_caido");
    ASSERT_TRUE(c != entt::null);
    Inimigo& cav = w.reg.get<Inimigo>(c);
    cav.vida = cav.vidaMax / 2 + 1;
    passos(w, 1);
    EXPECT_EQ(cav.fase, 1) << "acima de 50% ainda é a fase 1";
    cav.vida = cav.vidaMax / 2;
    passos(w, 1);
    EXPECT_EQ(cav.fase, 2);
    EXPECT_TRUE(std::ranges::any_of(w.tirarEventos(), [](const EventoJogo& e) { return e.tipo == EventoJogo::Tipo::FaseChefe; }));
    // Fase 2: investe e já recarrega em 1,6 s (antes, 3,2 s)
    w.jogador.pos = w.reg.get<Posicao>(c).v + glm::vec2(60, 0);
    w.jogador.invulneravel = 99.0f;
    cav.perseguindo = true;
    cav.recargaInvestida = 0.0f;
    passos(w, 1);
    EXPECT_GT(cav.investida, 0.0f);
    EXPECT_NEAR(cav.recargaInvestida, 1.6f, 0.05f);
    // O Golem: em 60%, o chão vira brasa onde o herói pisa (e a brasa queima)
    World g = vilarejo();
    g.entrarRegiao("minas", {10, 10}, Direcao::Baixo);
    const entt::entity q = primeiro(g, "golem_cinzas");
    ASSERT_TRUE(q != entt::null);
    Inimigo& golem = g.reg.get<Inimigo>(q);
    golem.vida = golem.vidaMax * 6 / 10;
    g.jogador.pos = g.reg.get<Posicao>(q).v + glm::vec2(90, 0);
    golem.perseguindo = true;
    const int vida = g.jogador.vida;
    passos(g, 60 * 3);
    EXPECT_EQ(golem.fase, 2);
    EXPECT_FALSE(g.reg.view<Brasa>().empty()) << "deixou brasa";
    EXPECT_LT(g.jogador.vida, vida);
}
