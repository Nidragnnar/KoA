// A Quinta Era, Coroa Eterna (1.18): o estado do reino depois do final da Queda
#include <gtest/gtest.h>

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

// O reino na Alta Fantasia, com o final escolhido na Torre
World comFinal(const std::string& final) {
    World w = vilarejo();
    for (int e = 1; e <= 3; ++e) w.marcar("era:" + std::to_string(e), "", true);
    w.reino.eraConquistada = 3;
    w.marcar("misterio:primeiro_ato", "", true);
    w.marcar("misterio:segundo_ato", "", true);
    w.marcar("final:" + final, "", true);
    return w;
}

}

TEST(CoroaEterna, ViraPeloFinalDaQueda) {
    EXPECT_EQ(dados().coroa.finais.size(), 3u);
    // Sem final, nada; com final mas sem a Alta Fantasia, também não
    World w = vilarejo();
    w.dormir(false);
    EXPECT_FALSE(w.coroaEterna());
    World cedo = vilarejo();
    cedo.marcar("final:aprisionar", "", true);
    cedo.dormir(false);
    EXPECT_FALSE(cedo.coroaEterna()) << "a Coroa pede o Reino da Alta Fantasia";
    // Com tudo: na noite seguinte, a Quinta Era
    World c = comFinal("aprisionar");
    EXPECT_FALSE(c.coroaEterna());
    c.dormir(false);
    ASSERT_TRUE(c.coroaEterna());
    const auto ev = c.tirarEventos();
    EXPECT_TRUE(std::ranges::any_of(ev, [](const EventoJogo& e) { return e.tipo == EventoJogo::Tipo::LerPista && e.alvo == "epilogo"; }))
        << "o epílogo abre na tela";
    EXPECT_TRUE(std::ranges::any_of(ev, [](const EventoJogo& e) { return e.tipo == EventoJogo::Tipo::EraMudou && e.quantidade == 4; }));
    EXPECT_EQ(c.titulo().id, "coroa_eterna");
    ASSERT_NE(c.coroaAtual(), nullptr);
    EXPECT_EQ(c.coroaAtual()->epilogo.size(), 3u);
    c.dormir(false);
    EXPECT_EQ(std::ranges::count_if(c.cronicas, [](const EntradaCronica& e) { return e.marco == "era:coroa_eterna"; }), 1) << "uma vez só";
}

TEST(CoroaEterna, CadaFinalDaUmaCoroa) {
    std::map<std::string, float> saturacao;
    for (const char* f : {"aprisionar", "unir", "destruir"}) {
        SCOPED_TRACE(f);
        World w = comFinal(f);
        const ResumoReino antes = w.resumo();
        w.dormir(false);
        ASSERT_TRUE(w.coroaEterna());
        const FinalCoroa& c = *w.coroaAtual();
        saturacao[f] = c.saturacao;
        EXPECT_EQ(w.marcos.contains("coroa:cristal"), c.cristais) << "o cristal vivo nas ruas";
        EXPECT_GE(w.resumo().seguranca, antes.seguranca + c.seguranca - 1);
    }
    EXPECT_NE(saturacao["aprisionar"], saturacao["unir"]);
    EXPECT_FALSE(dados().coroa.finais.at("destruir").cristais) << "apagada a Chama, não há cristal";
}

TEST(CoroaEterna, OEcoDaChamaFortaleceAsHordas) {
    World w = comFinal("unir");
    w.hordaDia = w.relogio.dia + 3;
    const int antes = w.forcaProximaHorda();
    w.dormir(false);
    ASSERT_TRUE(w.coroaEterna());
    w.hordaDia = w.relogio.dia + 2;  // a mesma distância da horda (o dia mudou)
    const int depois = w.forcaProximaHorda();
    EXPECT_GE(depois, static_cast<int>(antes * 1.2f)) << "+25% pelo eco da Chama";
}
