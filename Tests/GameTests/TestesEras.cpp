// Fase 7: transformação em onda, enfeites, anúncio de era, floresta que clareia e falas por era
#include <gtest/gtest.h>

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

}

TEST(Eras, ComecaTudoNaEraDasCinzas) {
    World w = vilarejo();
    for (int y = 0; y < w.mapa.altura(); y += 3)
        for (int x = 0; x < w.mapa.largura(); x += 3) ASSERT_EQ(w.eraNoTile({x, y}), 0);
}

TEST(Eras, TransformacaoEmOndaAPartirDaCasa) {
    World w = vilarejo();
    const BuildingData& casa = *w.reino.construcao("casa_herdeiro");
    const glm::vec2 porta = w.pontoPorta(casa);
    const glm::ivec2 perto{static_cast<int>(porta.x) / 16, static_cast<int>(porta.y) / 16};
    w.reino.eraVisual = 0.5f;  // meio da transição para a era 1
    EXPECT_EQ(w.eraNoTile(perto), 1) << "perto da casa já mudou";
    EXPECT_EQ(w.eraNoTile({62, 38}), 0) << "o canto mais distante ainda não";
    w.reino.eraVisual = 1.0f;
    EXPECT_EQ(w.eraNoTile({62, 38}), 1) << "com a era completa, tudo mudou";
    EXPECT_EQ(w.eraNoTile(perto), 1) << "e nada pula para a era seguinte antes da hora";
    w.reino.eraVisual = 3.0f;
    EXPECT_EQ(w.eraNoTile({5, 5}), 3);
}

TEST(Eras, ForaDoVilarejoNaoMudaMasAFlorestaClareia) {
    World w = vilarejo();
    w.reino.eraVisual = 3.0f;
    w.entrarRegiao("floresta", {3, 17}, Direcao::Direita);
    EXPECT_EQ(w.eraNoTile({10, 10}), 0) << "a floresta segue amaldiçoada";
    w.relogio.minutos = 12 * 60;
    const float clara = w.escuridao();
    w.reino.eraVisual = 0.0f;
    EXPECT_GT(w.escuridao(), clara) << "mas fica menos escura com o reino próspero";
}

TEST(Eras, EnfeitesEFonteDependemDaEra) {
    World w = vilarejo();
    int enfeites = 0, cristais = 0;
    for (auto [e, en, v] : w.reg.view<Enfeite, Visual>().each()) {
        ++enfeites;
        cristais += v.sprite == "cristal" && en.eraMin == 3;
    }
    EXPECT_GE(enfeites, 10);
    EXPECT_EQ(cristais, 5);
    bool fonte = false;
    for (auto [e, t] : w.reg.view<TrocaPorEra>().each()) fonte = t.sprite == "fonte" && t.era == 2;
    EXPECT_TRUE(fonte) << "o poço vira fonte no Reino Renascido";
}

TEST(Eras, AnunciaAViradaUmaVez) {
    World w = vilarejo();
    w.reino.eraForcada = 1;
    w.passo(1.0f / 60.0f, {});
    w.passo(1.0f / 60.0f, {});
    const auto ev = w.tirarEventos();
    const auto n = std::ranges::count_if(ev, [](const EventoJogo& e) { return e.tipo == EventoJogo::Tipo::EraMudou; });
    EXPECT_EQ(n, 1);
    const auto it = std::ranges::find_if(ev, [](const EventoJogo& e) { return e.tipo == EventoJogo::Tipo::EraMudou; });
    EXPECT_EQ(it->texto, "O Despertar da Coroa");
}

TEST(Eras, MoradoresComentamANovaEra) {
    World w = vilarejo();
    w.conversar("edda");  // primeiro encontro
    w.dormir(false);
    w.reino.eraVisual = 1.0f;
    const Conversa c = w.conversar("edda");
    EXPECT_NE(c.falas[0].find("passarinho"), std::string::npos) << c.falas[0];
}
