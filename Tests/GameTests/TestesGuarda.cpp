// Guarda Real (1.15): esquadras, postos, a conta das lutas fora do castelo, o soldo e o save v18
#include <gtest/gtest.h>

#include "Engine/Platform/FileSystem.h"
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
    for (const char* r : {"madeira", "pedra", "ouro", "comida"}) w.inventario.definir(r, 5000);
    w.inventario.definir("cristal", 100);
    return w;
}

// Castelo com o Quartel Real erguido e a floresta já visitada
World comQuartel() {
    World w = vilarejo();
    w.marcar("era:2", "", true);
    while (w.nivelCastelo() < 2) EXPECT_TRUE(w.melhorarConstrucao("castelo"));
    EXPECT_TRUE(w.erguerAla("quartel"));
    w.visitadas.insert("floresta");
    w.marcar("missao:limpar_estrada", "", true);
    return w;
}

}

TEST(Guarda, DadosEForca) {
    const DadosGuarda& d = dados().guarda;
    EXPECT_EQ(d.tipos.size(), 3u);
    EXPECT_EQ(d.esquadrasMax, 3);
    Esquadra e{1, "lanceiros", "Teste", "reserva", 8, 0};
    EXPECT_FLOAT_EQ(forcaEsquadra(d, e, {}), 8 * 2.2f);
    e.nivel = 2;
    EXPECT_FLOAT_EQ(forcaEsquadra(d, e, {}), 8 * 2.2f * 1.5f);
    EXPECT_FLOAT_EQ(forcaEsquadra(d, e, {3, 0.15f, false}), 8 * 2.2f * 1.5f * 1.4f * 1.15f);
    EXPECT_FLOAT_EQ(forcaEsquadra(d, e, {0, 0.0f, true}), 8 * 2.2f * 1.5f * 1.5f) << "com o herdeiro lá";
    Esquadra a{2, "arqueiros", "Arcos", "regiao:floresta", 8, 0};
    EXPECT_FLOAT_EQ(forcaEsquadra(d, a, {}), 8 * 2.0f * 1.3f) << "arqueiros guardam melhor as terras";
}

TEST(Guarda, FormarPedeQuartelELimite) {
    World w = vilarejo();
    EXPECT_EQ(w.podeFormar("lanceiros"), "Erga o Quartel Real no castelo.");
    World q = comQuartel();
    EXPECT_FALSE(q.podeFormar("cavaleiros").empty()) << "cavaleiros pedem a forja no nível 2";
    const int ouro = q.inventario.quantidade("ouro");
    ASSERT_TRUE(q.formarEsquadra("lanceiros"));
    EXPECT_EQ(q.inventario.quantidade("ouro"), ouro - 80);
    EXPECT_EQ(q.esquadras[0].soldados, 8);
    EXPECT_EQ(q.esquadras[0].posto, "reserva");
    ASSERT_TRUE(q.formarEsquadra("arqueiros"));
    ASSERT_TRUE(q.formarEsquadra("lanceiros"));
    EXPECT_FALSE(q.formarEsquadra("arqueiros")) << "no máximo 3 esquadras";
    EXPECT_NE(q.esquadras[0].nome, q.esquadras[1].nome);
}

// A conta da luta fora do castelo: força contra corrupção × 0,25
TEST(Guarda, ContaDaLutaNaRegiao) {
    const DadosGuarda& d = dados().guarda;
    const ResultadoLuta v = lutaNaRegiao(d, 17.6f, 70.0f);
    EXPECT_TRUE(v.vitoria);
    EXPECT_FLOAT_EQ(v.ameaca, 17.5f);
    EXPECT_EQ(v.perdas, 1) << "vitória apertada custa um";
    EXPECT_EQ(lutaNaRegiao(d, 30.0f, 40.0f).perdas, 0) << "vitória folgada não custa ninguém";
    const ResultadoLuta p = lutaNaRegiao(d, 10.0f, 90.0f);
    EXPECT_FALSE(p.vitoria);
    EXPECT_EQ(p.perdas, 3) << "faltaram 12,5: três baixas";
}

TEST(Guarda, GuardarARegiaoBaixaACorrupcao) {
    World livre = comQuartel(), guardada = comQuartel();
    ASSERT_TRUE(guardada.formarEsquadra("arqueiros"));
    ASSERT_TRUE(guardada.definirPosto(guardada.esquadras[0].id, "regiao:floresta"));
    EXPECT_TRUE(guardada.regiaoGuardada("floresta"));
    for (World* w : {&livre, &guardada}) w->estadoRegioes.at("floresta").corrupcao = 60.0f;
    for (int i = 0; i < 6; ++i) {
        livre.dormir(false);
        guardada.dormir(false);
    }
    const auto& g = guardada.estadoRegioes.at("floresta");
    EXPECT_LT(g.corrupcao, livre.estadoRegioes.at("floresta").corrupcao - 15.0f);
    EXPECT_GT(g.seguranca, livre.estadoRegioes.at("floresta").seguranca);
    EXPECT_FALSE(guardada.esquadras.empty());
}

TEST(Guarda, SoldoPorNoiteEDesercao) {
    World w = comQuartel();
    ASSERT_TRUE(w.formarEsquadra("lanceiros"));
    const Custo soldo = soldoDaNoite(dados().guarda, w.esquadras);
    EXPECT_EQ(soldo.at("ouro"), 4);  // 8 × 0,4 = 3,2: arredonda para cima
    EXPECT_EQ(soldo.at("comida"), 3);
    w.dormir(false);
    EXPECT_TRUE(std::ranges::any_of(w.ultimoRelatorio.variacoes, [](const auto& v) { return v.first == "ouro|soldo da guarda" && v.second == -4; }));
    // Sem ouro (nem o dos impostos da noite), um soldado deserta; sem ninguém, a esquadra se desfaz
    std::vector<Noticia> n;
    w.inventario.definir("ouro", 0);
    w.noiteDaGuarda(n);
    EXPECT_EQ(w.esquadras[0].soldados, 7);
    EXPECT_TRUE(std::ranges::any_of(n, [](const Noticia& x) { return x.texto.find("desertou") != std::string::npos; }));
    w.esquadras[0].soldados = 1;
    w.noiteDaGuarda(n);
    EXPECT_TRUE(w.esquadras.empty());
}

TEST(Guarda, ReservaNoPortaoEProtecaoDaAldeia) {
    World w = comQuartel();
    const int defesa = w.defesaDoReino();
    ASSERT_TRUE(w.formarEsquadra("lanceiros"));
    EXPECT_EQ(w.defesaDoReino(), defesa + 3 * 6) << "17,6 de força: 3 soldados a mais no portão";
    // Protegendo o Vau: a prosperidade sobe e ninguém pede contra criaturas ou bandidos
    ASSERT_TRUE(w.definirPosto(w.esquadras[0].id, "aldeia:vau_cinzento"));
    EXPECT_EQ(w.defesaDoReino(), defesa);
    w.estadoRegioes.at("floresta").corrupcao = 95.0f;
    auto& vau = w.mundoVivo.aldeias.at("vau_cinzento");
    vau.prosperidade = 50.0f;
    for (int i = 0; i < 10; ++i) {
        w.estadoRegioes.at("floresta").corrupcao = 95.0f;
        w.dormir(false);
        EXPECT_TRUE(std::ranges::none_of(w.mundoVivo.pedidos, [](const PedidoAberto& p) {
            return p.aldeia == "vau_cinzento" && (p.def == "criaturas" || p.def == "bandidos");
        }));
    }
}

TEST(SaveV18, GuardaAsEsquadrasEMigraOsAntigos) {
    const auto pasta = std::filesystem::temp_directory_path() / "koa_teste_save_v18";
    std::filesystem::remove_all(pasta);
    World w = comQuartel();
    ASSERT_TRUE(w.formarEsquadra("lanceiros"));
    ASSERT_TRUE(w.formarEsquadra("arqueiros"));
    ASSERT_TRUE(w.treinarEsquadra(w.esquadras[1].id));
    ASSERT_TRUE(w.definirPosto(w.esquadras[1].id, "regiao:floresta"));
    w.esquadras[0].soldados = 5;
    SaveSystem s(pasta);
    ASSERT_TRUE(s.salvar(w.capturar()));
    EXPECT_TRUE(std::filesystem::exists(pasta / "Save_001/guard.json"));
    auto d = s.carregar();
    ASSERT_TRUE(d);
    World w2 = comQuartel();
    w2.aplicar(*d);
    ASSERT_EQ(w2.esquadras.size(), 2u);
    EXPECT_EQ(w2.esquadras[0].soldados, 5);
    EXPECT_EQ(w2.esquadras[1].nivel, 1);
    EXPECT_EQ(w2.esquadras[1].posto, "regiao:floresta");
    EXPECT_EQ(w2.proximaEsquadra, w.proximaEsquadra);
    // Save v17: sem guard.json, sem esquadras
    std::filesystem::remove(pasta / "Save_001/guard.json");
    auto meta = *ke::fs::lerTexto(pasta / "Save_001/metadata.json");
    const std::string atual = "\"versao_formato\": " + std::to_string(VERSAO_SAVE);
    meta.replace(meta.find(atual), atual.size(), "\"versao_formato\": 17");
    ke::fs::gravarTextoSeguro(pasta / "Save_001/metadata.json", meta);
    d = s.carregar();
    ASSERT_TRUE(d);
    World w3 = comQuartel();
    w3.aplicar(*d);
    EXPECT_TRUE(w3.esquadras.empty());
    std::filesystem::remove_all(pasta);
}
