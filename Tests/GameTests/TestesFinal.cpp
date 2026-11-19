// Fase 8: recursos que renascem, mercado, chefe liberando a Alta Fantasia e o guia dos primeiros dias
#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

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
    EXPECT_TRUE(w.carregar(dados(), PASTA_DATA));
    return w;
}

// Herói em P com uma árvore logo acima e a casa ao sul
const char* MAPA =
    "..........\n"
    "....T.....\n"
    "....P.....\n"
    "..........\n"
    "..........\n"
    "..........\n"
    "..........\n"
    "..........\n";

nlohmann::json defTeste() {
    nlohmann::json j = {{"construcoes", {{{"id", "casa_herdeiro"}, {"def", "casa"}, {"tile", {3, 4}}, {"nivel", 1}}}}};
    for (const auto& [id, n] : dados().npcs)
        for (const auto& r : n.rotina) j["locais"][r.local] = {0, 0};
    return j;
}

void passos(World& w, int n, EntradaMundo e = {}) {
    for (int i = 0; i < n; ++i) w.passo(1.0f / 60.0f, e);
}

void golpe(World& w) {
    w.passo(1.0f / 60.0f, {{0, 0}, true, true});
    passos(w, 25);
}

bool arvoreEsgotada(World& w) {
    for (auto [e, c] : w.reg.view<Coletavel>().each())
        if (c.recurso == "madeira") return c.esgotado;
    return false;
}

}

TEST(Renascer, ArvoreVoltaDepoisDeTresNoites) {
    World w;
    ASSERT_TRUE(w.carregarDeTexto(dados(), MAPA, defTeste()));
    w.jogador.dir = Direcao::Cima;
    passos(w, 1);
    ASSERT_EQ(w.alvo().tipo, TipoInteracao::Cortar);
    for (int i = 0; i < 3; ++i) golpe(w);
    ASSERT_TRUE(arvoreEsgotada(w));
    w.dormir(false);
    w.dormir(false);
    EXPECT_TRUE(arvoreEsgotada(w)) << "duas noites ainda não bastam";
    w.dormir(false);
    EXPECT_FALSE(arvoreEsgotada(w)) << "na terceira manhã a árvore voltou";
    for (auto [e, c, v] : w.reg.view<Coletavel, Visual>().each()) {
        EXPECT_EQ(v.sprite, c.spriteNormal) << "sem toco";
        EXPECT_EQ(c.hp, c.hpMax);
    }
}

TEST(Renascer, DiaEsgotadoSobreviveAoSave) {
    World w;
    ASSERT_TRUE(w.carregarDeTexto(dados(), MAPA, defTeste()));
    w.jogador.dir = Direcao::Cima;
    passos(w, 1);
    for (int i = 0; i < 3; ++i) golpe(w);
    w.dormir(false);
    const DadosSave s = w.capturar();
    World w2;
    ASSERT_TRUE(w2.carregarDeTexto(dados(), MAPA, defTeste()));
    w2.aplicar(s);
    EXPECT_TRUE(arvoreEsgotada(w2));
    w2.dormir(false);
    w2.dormir(false);
    EXPECT_FALSE(arvoreEsgotada(w2)) << "a contagem continua de onde parou";
}

TEST(Mercado, SoNegociaComMercadoEATaxaCaiComONivel) {
    World w = vilarejo();
    w.inventario.definir("ouro", 200);
    EXPECT_FALSE(w.podeNegociar("pedra", true).empty());
    EXPECT_FALSE(w.comprarNoMercado("pedra")) << "sem mercado não há compra";

    w.reino.construcoes.push_back({"mercado_teste", "mercado", "Mercado", 1, {0, 0}});
    const int nivel1 = w.precoCompraLote("pedra");
    EXPECT_GT(nivel1, 0);
    w.reino.construcoes.back().level = 3;
    EXPECT_LT(w.precoCompraLote("pedra"), nivel1) << "o Grande Bazar cobra menos taxa";
    const int preco = w.precoCompraLote("pedra"), pedra = w.inventario.quantidade("pedra");
    ASSERT_TRUE(w.comprarNoMercado("pedra"));
    EXPECT_EQ(w.inventario.quantidade("pedra"), pedra + 10);
    EXPECT_EQ(w.inventario.quantidade("ouro"), 200 - preco);
    EXPECT_GT(w.precoCompraLote("pedra"), preco) << "comprar sobe o preço";
    w.inventario.definir("ouro", 5);
    EXPECT_FALSE(w.comprarNoMercado("pedra")) << "ouro insuficiente";
    EXPECT_EQ(w.inventario.quantidade("ouro"), 5) << "falha não cobra nada";
}
TEST(EraFinal, AltaFantasiaSoDepoisDoChefe) {
    Kingdom k;
    for (int i = 0; i < 41; ++i) k.construcoes.push_back({"m" + std::to_string(i), "moradia", "", 1});
    ASSERT_GE(k.desenvolvimento(), 40);
    EXPECT_EQ(k.eraAlvo(), KingdomEra::Prosperity) << "sem o Cavaleiro Caído derrotado, para no Renascido";
    k.magiaLiberta = true;
    EXPECT_EQ(k.eraAlvo(), KingdomEra::HighFantasy);
    k.magiaLiberta = false;
    k.eraForcada = 3;
    EXPECT_EQ(k.eraAlvo(), KingdomEra::HighFantasy) << "a ferramenta de depuração ignora a regra";
}

TEST(EraFinal, MagiaLibertaVemDoMarcoNoSave) {
    World w = vilarejo();
    w.marcos.insert("chefe_cavaleiro_caido");
    const DadosSave s = w.capturar();
    World w2 = vilarejo();
    EXPECT_FALSE(w2.reino.magiaLiberta);
    w2.aplicar(s);
    EXPECT_TRUE(w2.reino.magiaLiberta);
}

TEST(Guia, ConduzOsPrimeirosDias) {
    World w = vilarejo();
    w.inventario.definir("madeira", 0);
    w.inventario.definir("pedra", 0);
    w.inventario.definir("ouro", 0);
    auto passo = [&] { return w.proximoPasso(); };
    EXPECT_NE(passo().find("madeira"), std::string::npos) << passo();
    w.inventario.definir("madeira", 15);
    EXPECT_NE(passo().find("pedra"), std::string::npos) << passo();
    w.inventario.definir("pedra", 8);
    EXPECT_NE(passo().find("baús"), std::string::npos) << passo();
    w.inventario.definir("ouro", 10);
    EXPECT_NE(passo().find("cabana"), std::string::npos) << passo();
    ASSERT_TRUE(w.melhorarConstrucao("casa_herdeiro"));
    EXPECT_NE(passo().find("morador"), std::string::npos) << passo();
    w.conversar(dados().npcs.begin()->first);
    EXPECT_NE(passo().find("horta"), std::string::npos) << passo();
    w.reino.construcoes.push_back({"campo_teste", "campo", "Horta", 1, {0, 0}});
    EXPECT_NE(passo().find("Durma"), std::string::npos) << passo();
    w.dormir(false);
    EXPECT_NE(passo().find("floresta"), std::string::npos) << passo();
    w.visitadas.insert("floresta");
    EXPECT_EQ(passo(), "") << "depois disso o guia some";
}
