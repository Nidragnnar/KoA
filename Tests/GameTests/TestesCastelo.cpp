// Fase 16: o castelo reconstruído, as alas, o brasão da linhagem e o save v11
#include <gtest/gtest.h>

#include "Engine/Platform/FileSystem.h"
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
    return w;
}

void ricos(World& w) {
    for (const char* r : {"madeira", "pedra", "ouro"}) w.inventario.definir(r, 5000);
    w.inventario.definir("cristal", 100);
}

// Castelo no nível pedido (as eras que ele pede já marcadas)
World comCastelo(int nivel) {
    World w = vilarejo();
    ricos(w);
    w.marcar("era:2", "", true);
    w.marcar("era:3", "", true);
    while (w.nivelCastelo() < nivel) EXPECT_TRUE(w.melhorarConstrucao("castelo"));
    return w;
}

}

TEST(Castelo, DadosDoCasteloEDoBrasao) {
    const BuildingDef& c = dados().construcoes.at("castelo");
    EXPECT_EQ(c.nivelMax(), 4);
    EXPECT_FALSE(c.construivel);
    EXPECT_EQ(c.nivel(3)->requer, "era:2");
    EXPECT_EQ(c.nivel(4)->requer, "era:3");
    const DadosCastelo& d = dados().castelo;
    EXPECT_EQ(d.alas.size(), 6u);
    EXPECT_EQ(d.formas.size(), 4u);
    EXPECT_EQ(d.divisoes.size(), 6u);
    EXPECT_EQ(d.tinturas.size(), 8u);
    EXPECT_EQ(d.simbolos.size(), 8u);
    EXPECT_EQ(d.lemas.size(), 6u);
    EXPECT_EQ(d.vagas(1), 0);
    EXPECT_EQ(d.vagas(2), 1);
    EXPECT_EQ(d.vagas(4), 3);
    EXPECT_EQ(d.vagas(9), 3) << "nível acima do último usa o último";
    EXPECT_EQ(textoBrasao(d, d.inicial), "De goles, uma coroa de ouro");
    Brasao b = d.inicial;
    b.divisao = "partido";
    b.cor2 = "azure";
    b.simbolo = "leao";
    EXPECT_EQ(textoBrasao(d, b), "Partido de goles e azure, um leão de ouro");
    b.divisao = "chefe";
    EXPECT_EQ(textoBrasao(d, b), "De goles com chefe de azure, um leão de ouro");
    EXPECT_EQ(textoAla(d.ala("tesouro")->efeitos), "+15% de impostos, +3 de ouro por dia");
}

TEST(Castelo, GirarAsPecasDoEditor) {
    const DadosCastelo& d = dados().castelo;
    Brasao b = d.inicial;
    girarPeca(d, b, 0, 1);
    EXPECT_EQ(b.forma, "redondo");
    girarPeca(d, b, 0, -2);
    EXPECT_EQ(b.forma, "quadrado") << "dá a volta na lista";
    girarPeca(d, b, 2, 1);
    EXPECT_EQ(b.cor1, "azure");
    girarPeca(d, b, 6, -1);
    EXPECT_EQ(b.lema, 5);
    girarPeca(d, b, 6, 1);
    EXPECT_EQ(b.lema, 0);
    Brasao estranho = d.inicial;
    estranho.simbolo = "unicornio";
    estranho.lema = 99;
    const Brasao v = d.validar(estranho);
    EXPECT_EQ(v.simbolo, "coroa");
    EXPECT_EQ(v.lema, 0);
}

TEST(Castelo, ComecaEmRuinasNoLugarDaDecoracao) {
    World w = vilarejo();
    const BuildingData* c = w.reino.construcao("castelo");
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->level, 1);
    EXPECT_EQ(c->tile, glm::ivec2(27, 2));
    EXPECT_EQ(w.nivelCastelo(), 1);
    EXPECT_EQ(w.vagasAlas(), 0);
    for (auto [e, v] : w.reg.view<const Visual>().each()) EXPECT_NE(v.sprite, "castelo_ruina") << "a decoração antiga saiu";
    // As ruínas seguem bloqueando a passagem
    const ke::AABB caixa = w.caixaConstrucao(*c);
    EXPECT_TRUE(std::ranges::any_of(w.obstaculos(), [&](const ke::AABB& o) { return o.min == caixa.min && o.max == caixa.max; }));
    // E não contam como obra nem como desenvolvimento
    EXPECT_FALSE(w.marcos.contains("construiu:castelo"));
}

TEST(Castelo, ReconstruirPedeAsEras) {
    World w = vilarejo();
    ricos(w);
    const int antes = w.reino.desenvolvimento();
    ASSERT_TRUE(w.melhorarConstrucao("castelo"));
    EXPECT_EQ(w.nivelCastelo(), 2);
    EXPECT_GT(w.reino.desenvolvimento(), antes);
    EXPECT_TRUE(w.marcos.contains("castelo_n2"));
    EXPECT_EQ(w.vagasAlas(), 1);
    // O Salão Real espera o Reino Renascido
    w.marcos.erase("era:2");
    EXPECT_FALSE(w.requisitoProximoNivel(*w.reino.construcao("castelo")).empty());
    const int pedra = w.inventario.quantidade("pedra");
    EXPECT_FALSE(w.melhorarConstrucao("castelo"));
    EXPECT_EQ(w.inventario.quantidade("pedra"), pedra) << "bloqueado não cobra nada";
    w.marcar("era:2", "", true);
    EXPECT_TRUE(w.requisitoProximoNivel(*w.reino.construcao("castelo")).empty());
    ASSERT_TRUE(w.melhorarConstrucao("castelo"));
    EXPECT_EQ(w.vagasAlas(), 2);
    w.marcos.erase("era:3");
    EXPECT_FALSE(w.melhorarConstrucao("castelo"));
    w.marcar("era:3", "", true);
    ASSERT_TRUE(w.melhorarConstrucao("castelo"));
    EXPECT_EQ(w.nivelCastelo(), 4);
    EXPECT_EQ(w.vagasAlas(), 3);
    EXPECT_TRUE(w.marcos.contains("castelo_n4"));
}

TEST(Castelo, AlasPorVagaCadaUmaUmaVez) {
    World w = comCastelo(2);
    EXPECT_FALSE(World(vilarejo()).erguerAla("jardim")) << "sem a Torre de Menagem não há vaga";
    const int ouro = w.inventario.quantidade("ouro");
    ASSERT_TRUE(w.erguerAla("jardim"));
    EXPECT_EQ(w.inventario.quantidade("ouro"), ouro - 50);
    EXPECT_TRUE(w.marcos.contains("ala:jardim"));
    EXPECT_FALSE(w.erguerAla("quartel")) << "a única vaga já foi usada";
    ASSERT_TRUE(w.melhorarConstrucao("castelo"));
    EXPECT_FALSE(w.erguerAla("jardim")) << "a mesma ala não sobe duas vezes";
    EXPECT_TRUE(w.erguerAla("quartel"));
    World pobre = comCastelo(2);
    pobre.inventario.definir("ouro", 0);
    EXPECT_FALSE(pobre.erguerAla("tesouro"));
    EXPECT_TRUE(pobre.alas.empty());
}

TEST(Castelo, OsEfeitosDasAlas) {
    World w = comCastelo(4);
    w.estado.imposto = Imposto::Medio;
    const ResumoReino antes = w.resumo();
    const int defesa = w.defesaDoReino();
    const DestinoDef& dest = *dados().guilda.destino("ruinas_vale");
    const int margem = w.margemSemSorte(dest, {"kael"});
    ASSERT_TRUE(w.erguerAla("jardim"));
    ASSERT_TRUE(w.erguerAla("quartel"));
    ASSERT_TRUE(w.erguerAla("biblioteca"));
    const ResumoReino depois = w.resumo();
    EXPECT_NEAR(depois.felicidadeAlvo, std::min(100.0f, antes.felicidadeAlvo + 6.0f), 0.01f) << "jardim +5 e biblioteca +1";
    EXPECT_EQ(depois.seguranca, antes.seguranca + 8);
    EXPECT_EQ(w.defesaDoReino(), defesa + 6 + 4) << "um soldado a mais (6) e +8 de segurança (metade)";
    EXPECT_EQ(w.margemSemSorte(dest, {"kael"}), margem + 1);
    // Tesouro e celeiro: impostos e comida
    World t = comCastelo(4);
    t.estado.imposto = Imposto::Medio;
    const ResumoReino r0 = t.resumo();
    ASSERT_TRUE(t.erguerAla("tesouro"));
    ASSERT_TRUE(t.erguerAla("celeiro"));
    const ResumoReino r1 = t.resumo();
    EXPECT_GT(r1.impostos, r0.impostos);
    EXPECT_EQ(r1.ouroProducao, r0.ouroProducao + 3);
    EXPECT_GE(r1.comidaProduzida, r0.comidaProduzida);
}

TEST(Brasao, DefinirViraPaginaNasCronicas) {
    World w = vilarejo();
    EXPECT_FALSE(w.brasaoCriado);
    EXPECT_EQ(w.brasao, dados().castelo.inicial);
    Brasao b = w.brasao;
    b.forma = "pontudo";
    b.divisao = "esquartelado";
    b.cor2 = "prata";
    b.simbolo = "dragao";
    b.lema = 2;
    w.tirarEventos();
    w.definirBrasao(b);
    EXPECT_TRUE(w.brasaoCriado);
    EXPECT_EQ(w.brasao, b);
    EXPECT_TRUE(w.marcos.contains("brasao"));
    EXPECT_TRUE(std::ranges::any_of(w.cronicas, [](const EntradaCronica& c) { return c.marco == "brasao"; }));
    const auto ev = w.tirarEventos();
    EXPECT_TRUE(std::ranges::any_of(ev, [](const EventoJogo& e) { return e.tipo == EventoJogo::Tipo::BrasaoNovo; }));
    w.definirBrasao(b);  // igual: nada de novo
    EXPECT_FALSE(std::ranges::any_of(w.tirarEventos(), [](const EventoJogo& e) { return e.tipo == EventoJogo::Tipo::BrasaoNovo; }));
    b.simbolo = "inexistente";
    w.definirBrasao(b);
    EXPECT_EQ(w.brasao.simbolo, "coroa") << "peça que não existe volta à inicial";
}

TEST(SaveV11, GuardaAlasEBrasao) {
    const auto pasta = std::filesystem::temp_directory_path() / "koa_teste_save_v11";
    std::filesystem::remove_all(pasta);
    World w = comCastelo(3);
    ASSERT_TRUE(w.erguerAla("tesouro"));
    ASSERT_TRUE(w.erguerAla("salao"));
    Brasao b = w.brasao;
    b.forma = "redondo";
    b.cor1 = "sinople";
    b.simbolo = "arvore";
    b.lema = 4;
    w.definirBrasao(b);
    SaveSystem s(pasta);
    ASSERT_TRUE(s.salvar(w.capturar()));
    EXPECT_TRUE(std::filesystem::exists(pasta / "Save_001/castle.json"));
    std::string erro;
    const auto d = s.carregar("Save_001", &erro);
    ASSERT_TRUE(d) << erro;
    World w2 = vilarejo();
    w2.aplicar(*d);
    EXPECT_EQ(w2.nivelCastelo(), 3);
    EXPECT_EQ(w2.alas, (std::vector<std::string>{"tesouro", "salao"}));
    EXPECT_EQ(w2.brasao, b);
    EXPECT_TRUE(w2.brasaoCriado);
    std::filesystem::remove_all(pasta);
}

TEST(SaveV11, SaveV10ComecaComOBrasaoAntigo) {
    const auto pasta = std::filesystem::temp_directory_path() / "koa_teste_save_v10_para_v11";
    std::filesystem::remove_all(pasta);
    World w = vilarejo();
    SaveSystem s(pasta);
    auto dados10 = w.capturar();
    // Um save v10 não tinha o castelo entre as construções
    std::erase_if(dados10.construcoes, [](const EstadoConstrucao& c) { return c.id == "castelo"; });
    ASSERT_TRUE(s.salvar(dados10));
    std::filesystem::remove(pasta / "Save_001/castle.json");
    auto meta = *ke::fs::lerTexto(pasta / "Save_001/metadata.json");
    const std::string atual = "\"versao_formato\": " + std::to_string(VERSAO_SAVE);
    meta.replace(meta.find(atual), atual.size(), "\"versao_formato\": 10");
    ke::fs::gravarTextoSeguro(pasta / "Save_001/metadata.json", meta);
    std::string erro;
    const auto d = s.carregar("Save_001", &erro);
    ASSERT_TRUE(d) << erro;
    EXPECT_FALSE(d->temBrasao);
    World w2 = vilarejo();
    w2.aplicar(*d);
    EXPECT_EQ(w2.nivelCastelo(), 1) << "o castelo continua nas ruínas";
    EXPECT_TRUE(w2.alas.empty());
    EXPECT_EQ(w2.brasao, dados().castelo.inicial);
    EXPECT_FALSE(w2.brasaoCriado);
    std::filesystem::remove_all(pasta);
}

TEST(SaveV11, AlasAlemDasVagasNaoEntram) {
    World w = comCastelo(2);
    DadosSave d = w.capturar();
    d.alas = {"jardim", "quartel", "nao_existe"};
    World w2 = vilarejo();
    w2.aplicar(d);
    EXPECT_EQ(w2.alas, (std::vector<std::string>{"jardim"}));
}
