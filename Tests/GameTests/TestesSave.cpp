#include <gtest/gtest.h>

#include <fstream>

#include "Game/Exploration/Components.h"
#include "Game/World/World.h"

using namespace koa;

namespace {

struct PastaTemp {
    std::filesystem::path p;
    PastaTemp() : p(std::filesystem::temp_directory_path() / ("koa_teste_" + std::to_string(::testing::UnitTest::GetInstance()->random_seed()) +
                                                                "_" + ::testing::UnitTest::GetInstance()->current_test_info()->name())) {
        std::filesystem::remove_all(p);
    }
    ~PastaTemp() { std::filesystem::remove_all(p); }
};

GameData dados() {
    GameData d;
    d.carregar(PASTA_DATA);
    return d;
}

}

TEST(Save, IdaEVoltaPreservaOEstado) {
    PastaTemp tmp;
    const GameData d = dados();
    World w;
    ASSERT_TRUE(w.carregar(d, PASTA_DATA));
    // Mexe em tudo que o save precisa guardar
    w.jogador.pos += glm::vec2(16, 0);
    w.jogador.dir = Direcao::Esquerda;
    w.inventario.definir("madeira", 15);
    w.inventario.definir("pedra", 8);
    w.inventario.definir("ouro", 10);
    ASSERT_TRUE(w.melhorarConstrucao("casa_herdeiro"));
    int mexidos = 0;
    for (auto [e, c] : w.reg.view<Coletavel>().each()) {
        if (mexidos == 0) c.esgotado = true, c.hp = 0;
        if (mexidos == 1) c.hp = 1;
        if (++mexidos == 2) break;
    }
    const auto baus = w.reg.view<Saqueavel>();
    ASSERT_FALSE(baus.empty());
    baus.get<Saqueavel>(*baus.begin()).aberto = true;

    SaveSystem saves(tmp.p);
    std::string erro;
    ASSERT_TRUE(saves.salvar(w.capturar(), "Save_001", &erro)) << erro;
    for (const char* f : {"metadata.json", "player.json", "kingdom.json", "world.json", "inventory.json"})
        EXPECT_TRUE(std::filesystem::exists(tmp.p / "Save_001" / f)) << f;

    World w2;
    ASSERT_TRUE(w2.carregar(d, PASTA_DATA));
    const auto lido = saves.carregar("Save_001", &erro);
    ASSERT_TRUE(lido) << erro;
    w2.aplicar(*lido);
    EXPECT_EQ(w2.jogador.pos, w.jogador.pos);
    EXPECT_EQ(w2.jogador.dir, Direcao::Esquerda);
    EXPECT_EQ(w2.inventario.todos(), w.inventario.todos());
    EXPECT_EQ(w2.reino.construcao("casa_herdeiro")->level, 2);
    EXPECT_EQ(w2.reino.construcao("casa_herdeiro")->name, "Casa Reconstruída");
    EXPECT_EQ(w2.capturar().objetos.size(), w.capturar().objetos.size());
}

TEST(Save, FazBackupAntesDeSobrescrever) {
    PastaTemp tmp;
    SaveSystem saves(tmp.p);
    DadosSave a;
    a.inventario["ouro"] = 1;
    ASSERT_TRUE(saves.salvar(a));
    a.inventario["ouro"] = 2;
    ASSERT_TRUE(saves.salvar(a));
    std::ifstream bak(tmp.p / "Save_001.bak" / "inventory.json");
    ASSERT_TRUE(bak.good());
    const std::string texto((std::istreambuf_iterator<char>(bak)), {});
    EXPECT_NE(texto.find("1"), std::string::npos) << "o backup é o save anterior";
}

TEST(Save, CorrompidoCaiNoBackup) {
    PastaTemp tmp;
    SaveSystem saves(tmp.p);
    DadosSave a;
    a.inventario["ouro"] = 7;
    ASSERT_TRUE(saves.salvar(a));
    ASSERT_TRUE(saves.salvar(a));  // agora existe .bak
    std::ofstream(tmp.p / "Save_001" / "player.json") << "{ quebrado";
    std::string erro, aviso;
    const auto d = saves.carregar("Save_001", &erro, &aviso);
    ASSERT_TRUE(d);
    EXPECT_FALSE(aviso.empty());
    EXPECT_EQ(d->inventario.at("ouro"), 7);
}

TEST(Save, RecusaVersaoDoFuturoEValidaValores) {
    PastaTemp tmp;
    SaveSystem saves(tmp.p);
    DadosSave a;
    a.inventario["madeira"] = 5;
    ASSERT_TRUE(saves.salvar(a));
    std::ofstream(tmp.p / "Save_001" / "inventory.json") << R"({"madeira": -40, "pedra": 3})";
    auto d = saves.carregar();
    ASSERT_TRUE(d);
    EXPECT_EQ(d->inventario.at("madeira"), 0) << "quantidade negativa vira zero";
    std::ofstream(tmp.p / "Save_001" / "metadata.json") << R"({"versao_formato": 99})";
    std::filesystem::remove_all(tmp.p / "Save_001.bak");
    std::string erro;
    EXPECT_FALSE(saves.carregar("Save_001", &erro));
    EXPECT_NE(erro.find("99"), std::string::npos);
}

TEST(Save, PosicaoInvalidaVoltaAoInicio) {
    const GameData d = dados();
    World w;
    ASSERT_TRUE(w.carregar(d, PASTA_DATA));
    DadosSave s = w.capturar();
    s.posJogador = {-500, -500};
    w.aplicar(s);
    EXPECT_EQ(w.jogador.pos, w.spawn);
}
