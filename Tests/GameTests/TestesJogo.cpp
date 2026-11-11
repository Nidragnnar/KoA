#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include "Game/Exploration/Components.h"
#include "Game/World/World.h"

using namespace koa;
using nlohmann::json;

namespace {

GameData dadosReais() {
    GameData d;
    std::string erro;
    EXPECT_TRUE(d.carregar(PASTA_DATA, &erro)) << erro;
    return d;
}

// Mapinha de teste: herói em P, árvore logo acima, rocha à direita, baú à esquerda, casa ao sul
const char* MAPA =
    "..........\n"
    "....T.....\n"
    "...CPR....\n"
    "..........\n"
    "..........\n"
    "..........\n"
    "..........\n"
    "..........\n";

json defTeste() {
    json j = {{"construcoes", {{{"id", "casa_herdeiro"}, {"def", "casa"}, {"tile", {3, 4}}, {"nivel", 1}}}}};
    // Todos os locais das rotinas num canto, longe das interações do teste
    for (const auto& [id, n] : dadosReais().npcs)
        for (const auto& r : n.rotina) j["locais"][r.local] = {0, 0};
    return j;
}

void passos(World& w, int n, EntradaMundo e = {}) {
    for (int i = 0; i < n; ++i) w.passo(1.0f / 60.0f, e);
}

// Um toque em E seguido do tempo do golpe
void golpe(World& w) {
    w.passo(1.0f / 60.0f, {{0, 0}, true, true});
    passos(w, 25);
}

}

TEST(Inventario, PagaTudoOuNada) {
    Inventory inv;
    inv.adicionar("madeira", 10);
    inv.adicionar("pedra", 2);
    const Custo c{{"madeira", 5}, {"pedra", 3}};
    EXPECT_FALSE(inv.tem(c));
    EXPECT_EQ(inv.faltando(c).at("pedra"), 1);
    EXPECT_FALSE(inv.pagar(c));
    EXPECT_EQ(inv.quantidade("madeira"), 10) << "pagamento falho não pode descontar nada";
    inv.adicionar("pedra", 1);
    EXPECT_TRUE(inv.pagar(c));
    EXPECT_EQ(inv.quantidade("madeira"), 5);
    EXPECT_EQ(inv.quantidade("pedra"), 0);
}

TEST(Dados, CarregaConstrucoesERecursosDoJson) {
    const GameData d = dadosReais();
    ASSERT_TRUE(d.construcoes.contains("casa"));
    const BuildingDef& casa = d.construcoes.at("casa");
    EXPECT_EQ(casa.nivelMax(), 4);
    EXPECT_EQ(casa.nivel(1)->nome, "Cabana Abandonada");
    EXPECT_EQ(casa.nivel(2)->custo.at("madeira"), 15);
    EXPECT_EQ(d.nomeRecurso("ouro"), "Ouro");
}

TEST(Construcao, MelhoraSoComRecursosEMudaONome) {
    const GameData d = dadosReais();
    const BuildingDef& def = d.construcoes.at("casa");
    BuildingData b{"casa_herdeiro", "casa", "Cabana Abandonada", 1};
    Inventory inv;
    EXPECT_FALSE(melhorar(b, def, inv));
    EXPECT_EQ(b.level, 1);
    for (const auto& [id, q] : def.nivel(2)->custo) inv.adicionar(id, q);
    EXPECT_TRUE(melhorar(b, def, inv));
    EXPECT_EQ(b.level, 2);
    EXPECT_EQ(b.name, "Casa Reconstruída");
    EXPECT_TRUE(inv.faltando(def.nivel(2)->custo).size() == 3) << "recursos foram gastos";
}

TEST(Construcao, AparenciaSeparadaDoNivel) {
    const GameData d = dadosReais();
    const auto nomes = spritesAparencia(d.construcoes.at("casa"), 3, KingdomEra::Prosperity);
    ASSERT_EQ(nomes.size(), 2u);
    EXPECT_EQ(nomes[0], "casa_n3_e2");
    EXPECT_EQ(nomes[1], "casa_n3");
}

TEST(Mundo, CarregaOVilarejoReal) {
    const GameData d = dadosReais();
    World w;
    std::string erro;
    ASSERT_TRUE(w.carregar(d, PASTA_DATA, &erro)) << erro;
    EXPECT_EQ(w.mapa.largura(), 64);
    EXPECT_EQ(w.mapa.altura(), 40);
    EXPECT_NE(w.reino.construcao("casa_herdeiro"), nullptr);
    EXPECT_FALSE(ke::colide(w.mapa, w.jogador.caixa(), w.obstaculos())) << "o herói não pode nascer preso";
    int fogueiras = 0;
    for (auto [e, id] : w.reg.view<IdMundo>().each()) fogueiras += id.id.starts_with("fogueira_");
    EXPECT_EQ(fogueiras, 1) << "a fogueira da praça existe";
    // O mapa precisa ter recursos para a primeira casa
    int madeira = 0, pedra = 0, ouro = 0;
    for (auto [e, c] : w.reg.view<Coletavel>().each()) (c.recurso == "madeira" ? madeira : pedra) += c.rendimento;
    for (auto [e, s] : w.reg.view<Saqueavel>().each()) ouro += s.quantidade;
    const Custo& custo = d.construcoes.at("casa").nivel(2)->custo;
    EXPECT_GE(madeira, custo.at("madeira"));
    EXPECT_GE(pedra, custo.at("pedra"));
    EXPECT_GE(ouro, custo.at("ouro"));
}

TEST(Mundo, CortaArvoreEmTresGolpes) {
    const GameData d = dadosReais();
    World w;
    ASSERT_TRUE(w.carregarDeTexto(d, MAPA, defTeste()));
    w.jogador.dir = Direcao::Cima;
    passos(w, 1);
    ASSERT_EQ(w.alvo().tipo, TipoInteracao::Cortar);
    golpe(w);
    golpe(w);
    EXPECT_EQ(w.inventario.quantidade("madeira"), 0);
    golpe(w);
    EXPECT_EQ(w.inventario.quantidade("madeira"), 3);
    const auto ev = w.tirarEventos();
    EXPECT_TRUE(std::ranges::any_of(ev, [](const EventoJogo& e) { return e.tipo == EventoJogo::Tipo::Esgotou; }));
    // O toco continua bloqueando a passagem
    bool achouToco = false;
    for (auto [e, c, v] : w.reg.view<Coletavel, Visual>().each())
        if (c.esgotado) achouToco = v.sprite == "toco";
    EXPECT_TRUE(achouToco);
}

TEST(Mundo, QuebraRochaEAbreBau) {
    const GameData d = dadosReais();
    World w;
    ASSERT_TRUE(w.carregarDeTexto(d, MAPA, defTeste()));
    w.jogador.dir = Direcao::Direita;
    passos(w, 1);
    ASSERT_EQ(w.alvo().tipo, TipoInteracao::Quebrar);
    for (int i = 0; i < 4; ++i) golpe(w);
    EXPECT_EQ(w.inventario.quantidade("pedra"), 2);
    w.jogador.dir = Direcao::Esquerda;
    passos(w, 1);
    ASSERT_EQ(w.alvo().tipo, TipoInteracao::Saquear);
    golpe(w);
    EXPECT_GE(w.inventario.quantidade("ouro"), 6);
    golpe(w);  // baú aberto não dá ouro de novo
    EXPECT_LE(w.inventario.quantidade("ouro"), 12);
}

TEST(Mundo, SegurarEContinuaGolpeando) {
    const GameData d = dadosReais();
    World w;
    ASSERT_TRUE(w.carregarDeTexto(d, MAPA, defTeste()));
    w.jogador.dir = Direcao::Cima;
    passos(w, 60, {{0, 0}, false, true});  // 1 s segurando E
    EXPECT_EQ(w.inventario.quantidade("madeira"), 3);
}

TEST(Mundo, ToqueDuranteOGolpeNaoSePerde) {
    const GameData d = dadosReais();
    World w;
    ASSERT_TRUE(w.carregarDeTexto(d, MAPA, defTeste()));
    w.jogador.dir = Direcao::Cima;
    w.passo(1.0f / 60.0f, {{0, 0}, true, false});  // 1º golpe
    passos(w, 12);                                  // ainda no meio do movimento
    w.passo(1.0f / 60.0f, {{0, 0}, true, false});  // 2º toque cedo demais
    passos(w, 30);
    EXPECT_EQ(w.reg.get<Coletavel>(w.alvo().entidade).hp, 1) << "o segundo toque deveria ter virado golpe";
}

TEST(Mundo, ColisaoComObjetos) {
    const GameData d = dadosReais();
    World w;
    ASSERT_TRUE(w.carregarDeTexto(d, MAPA, defTeste()));
    const float y0 = w.jogador.pos.y;
    passos(w, 60, {{0, -1}, false, false});  // tenta atravessar a árvore
    EXPECT_GT(w.jogador.pos.y, y0 - 12.0f);
}

TEST(Mundo, PortaAbrePainelEReconstrucaoFunciona) {
    const GameData d = dadosReais();
    World w;
    ASSERT_TRUE(w.carregarDeTexto(d, MAPA, defTeste()));
    const BuildingData& casa = *w.reino.construcao("casa_herdeiro");
    w.jogador.pos = w.pontoPorta(casa) + glm::vec2(0, 9);
    w.jogador.dir = Direcao::Cima;
    w.passo(1.0f / 60.0f, {{0, 0}, true, true});
    const auto ev = w.tirarEventos();
    ASSERT_FALSE(ev.empty());
    EXPECT_EQ(ev[0].tipo, EventoJogo::Tipo::AbrirConstrucao);
    EXPECT_FALSE(w.melhorarConstrucao("casa_herdeiro"));
    for (const auto& [id, q] : d.construcoes.at("casa").nivel(2)->custo) w.inventario.adicionar(id, q);
    EXPECT_TRUE(w.melhorarConstrucao("casa_herdeiro"));
    EXPECT_EQ(w.reino.construcao("casa_herdeiro")->level, 2);
}

TEST(Reino, EraMudaAosPoucos) {
    Kingdom k;
    k.eraForcada = 2;
    k.atualizarEra(1.0f);
    EXPECT_GT(k.eraVisual, 0.0f);
    EXPECT_LT(k.eraVisual, 0.5f) << "a transição precisa ser gradual";
    for (int i = 0; i < 40; ++i) k.atualizarEra(1.0f);
    EXPECT_FLOAT_EQ(k.eraVisual, 2.0f);
    EXPECT_EQ(k.eraAtual(), KingdomEra::Prosperity);
}
