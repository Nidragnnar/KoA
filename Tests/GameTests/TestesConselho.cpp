// Fase 12: o Conselho Real (cargos, lealdade), as decisões políticas, as leis e o save v7
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

// Obras prontas sem passar pelo menu (o teste é do conselho)
void obrasProntas(World& w, int n) {
    for (int i = 0; i < n; ++i) w.reino.construcoes.push_back({"moradia_t" + std::to_string(i), "moradia", "Moradia", 1, {0, 0}, true, true});
}

// Reino com o conselho já formado
World comConselho() {
    World w = vilarejo();
    obrasProntas(w, 4);
    w.dormir(false);
    EXPECT_TRUE(w.conselhoFormado());
    return w;
}

bool temNota(const World& w, const std::string& trecho) {
    return std::ranges::any_of(w.ultimoRelatorio.notas, [&](const std::string& n) { return n.find(trecho) != std::string::npos; });
}

bool temPagina(const World& w, const std::string& marco) {
    return std::ranges::any_of(w.cronicas, [&](const EntradaCronica& c) { return c.marco == marco; });
}

void amizade(World& w, const std::string& npc, int a) {
    for (auto [e, m, rel] : w.reg.view<const Morador, Relacionamento>().each())
        if (m.id == npc) rel.afinidade = a;
}

int afinidade(World& w, const std::string& npc) {
    for (auto [e, m, rel] : w.reg.view<const Morador, const Relacionamento>().each())
        if (m.id == npc) return rel.afinidade;
    return -1;
}

}

TEST(Conselho, DadosDosCargosLeisEDecisoes) {
    const DadosConselho& c = dados().conselho;
    EXPECT_EQ(c.cargos.size(), 4u);
    EXPECT_EQ(c.decisoes.size(), 7u);
    EXPECT_EQ(c.leis.size(), 4u);
    EXPECT_FLOAT_EQ(c.cargo("marechal")->aptidaoDe("oswin"), 1.0f);
    EXPECT_FLOAT_EQ(c.cargo("obras")->aptidaoDe("bram"), 1.0f);
    EXPECT_FLOAT_EQ(c.cargo("marechal")->aptidaoDe("corvin"), 0.5f) << "quem não está na lista vale 0,5";
    for (const auto& d : c.decisoes) EXPECT_EQ(d.opcoes.size(), 3u) << d.id;
}

TEST(Conselho, SeFormaQuandoOReinoTemQuatroObras) {
    World w = vilarejo();
    obrasProntas(w, 3);
    w.dormir(false);
    EXPECT_FALSE(w.conselhoFormado());
    EXPECT_FALSE(w.nomear("marechal", "oswin")) << "sem conselho não há cargo";
    obrasProntas(w, 1);
    w.dormir(false);
    EXPECT_TRUE(w.conselhoFormado());
    EXPECT_TRUE(temNota(w, "Conselho Real"));
    EXPECT_TRUE(temPagina(w, "conselho_formado"));
}

TEST(Conselho, LealdadeComecaPelaAmizadeETrocaDeCargoMagoa) {
    World w = comConselho();
    amizade(w, "oswin", 400);
    const int segAntes = w.resumo().seguranca;
    ASSERT_TRUE(w.nomear("marechal", "oswin"));
    EXPECT_EQ(w.lealdade.at("oswin"), 60) << "40 + amizade/20";
    EXPECT_EQ(w.resumo().seguranca, segAntes + 6) << "Oswin nasceu para Marechal";
    EXPECT_TRUE(w.marcos.contains("cargo:marechal:oswin"));
    EXPECT_TRUE(temPagina(w, "cargo:marechal:oswin"));
    // Transferido para outro cargo: sem mágoa, e o antigo fica vago
    ASSERT_TRUE(w.nomear("obras", "oswin"));
    EXPECT_EQ(w.ocupante("marechal"), "");
    EXPECT_EQ(w.lealdade.at("oswin"), 60);
    // Tirado do cargo para outro entrar: perde 20
    ASSERT_TRUE(w.nomear("obras", "bram"));
    EXPECT_EQ(w.lealdade.at("oswin"), 40);
    EXPECT_EQ(w.cargoDe("bram"), "obras");
    EXPECT_FALSE(w.nomear("tesoureiro", "corvin")) << "o mercador viajante não serve no conselho";
}

TEST(Conselho, CadaCargoRendeNaNoite) {
    World w = comConselho();
    w.relogio.dia = 15;  // outono: hortas valem mais, mas o teste compara antes e depois
    w.reino.construcoes.push_back({"campo_t", "campo", "Horta", 1, {0, 0}, true, true});
    const ResumoReino antes = w.resumo();
    for (const auto& [c, npc] : {std::pair{"tesoureiro", "edda"}, {"intendente", "tilda"}, {"obras", "bram"}}) {
        amizade(w, npc, 300);
        ASSERT_TRUE(w.nomear(c, npc));
    }
    const ResumoReino depois = w.resumo();
    EXPECT_GT(depois.impostos, antes.impostos);
    EXPECT_GT(depois.comidaProduzida, antes.comidaProduzida);
    EXPECT_EQ(depois.madeiraDia, antes.madeiraDia + 3);
    EXPECT_EQ(depois.pedraDia, antes.pedraDia + 3);
    const int madeira = w.inventario.quantidade("madeira");
    w.dormir(false);
    EXPECT_GE(w.inventario.quantidade("madeira"), madeira + 3) << "o Mestre das Obras manda madeira toda noite";
}

TEST(Conselho, DescontenteFazSoOMinimo) {
    World w = comConselho();
    ASSERT_TRUE(w.nomear("marechal", "oswin"));
    w.lealdade["oswin"] = 26;
    EXPECT_GT(w.efeitoCargo("marechal"), 0.0f);
    w.estado.felicidade = 10.0f;  // povo infeliz: o conselho perde o ânimo
    w.dormir(false);
    EXPECT_LT(w.lealdade.at("oswin"), regra::LEALDADE_INSATISFEITO);
    EXPECT_FLOAT_EQ(w.efeitoCargo("marechal"), 0.0f);
    EXPECT_TRUE(temNota(w, "descontente"));
}

TEST(Conselho, UmCasoChegaACadaPoucosDias) {
    World w = comConselho();
    w.marcar("visitou:floresta");
    const int formado = w.ultimaDecisao;
    int dias = 0;
    while (w.decisaoPendente.empty() && dias < 10) {
        w.dormir(false);
        ++dias;
    }
    ASSERT_FALSE(w.decisaoPendente.empty());
    EXPECT_EQ(w.ultimaDecisao - formado, dados().conselho.diasEntreDecisoes);
    EXPECT_TRUE(temNota(w, "O conselho pede sua decisão"));
    EXPECT_TRUE(w.decisaoPossivel(*w.decisaoAtual()));
    // Enquanto ninguém decide, não chega outro caso
    const std::string caso = w.decisaoPendente;
    for (int i = 0; i < 5; ++i) w.dormir(false);
    EXPECT_EQ(w.decisaoPendente, caso);
}

TEST(Conselho, DecidirSegueOuContrariaOConselhoECriaLei) {
    World w = comConselho();
    for (const auto& [c, npc] : {std::pair{"marechal", "oswin"}, {"tesoureiro", "edda"}, {"obras", "bram"}}) ASSERT_TRUE(w.nomear(c, npc));
    const int lOswin = w.lealdade.at("oswin"), lEdda = w.lealdade.at("edda"), lBram = w.lealdade.at("bram");
    w.inventario.definir("ouro", 100);
    const ResumoReino antes = w.resumo();
    w.trazerDecisao("lenhadores");
    ASSERT_TRUE(w.decidir("patrulha"));
    EXPECT_EQ(w.inventario.quantidade("ouro"), 60);
    EXPECT_TRUE(w.leis.contains("patrulha"));
    EXPECT_EQ(w.resumo().seguranca, antes.seguranca + 4);
    EXPECT_EQ(w.resumo().ouroProducao, antes.ouroProducao - 2);
    EXPECT_EQ(w.lealdade.at("oswin"), lOswin + regra::LEALDADE_APOIADO) << "o Marechal pediu a patrulha";
    EXPECT_EQ(w.lealdade.at("edda"), lEdda - regra::LEALDADE_CONTRARIADO);
    EXPECT_EQ(w.lealdade.at("bram"), lBram - regra::LEALDADE_CONTRARIADO);
    EXPECT_TRUE(w.decisaoPendente.empty());
    EXPECT_TRUE(w.marcos.contains("decisao:lenhadores:patrulha"));
    EXPECT_TRUE(temPagina(w, "decisao:lenhadores:patrulha"));
    EXPECT_FALSE(w.decisaoPossivel(*dados().conselho.decisao("lenhadores"))) << "caso decidido não volta";
    EXPECT_FALSE(w.decidir("patrulha")) << "sem caso pendente não há o que decidir";
}

TEST(Conselho, GastoVaiAteOQueHouverEAmizadeMuda) {
    World w = comConselho();
    w.inventario.definir("comida", 10);
    const int pop = w.estado.populacao;
    w.trazerDecisao("refugiados");
    ASSERT_TRUE(w.decidir("acolher"));
    EXPECT_EQ(w.inventario.quantidade("comida"), 0);
    EXPECT_EQ(w.estado.populacao, pop + 3);
    amizade(w, "maelis", 300);
    w.trazerDecisao("refugiados");
    ASSERT_TRUE(w.decidir("recusar"));
    EXPECT_EQ(afinidade(w, "maelis"), 200);
}

TEST(Conselho, ProtestoVoltaQuandoOImpostoSobeDeNovo) {
    World w = comConselho();
    const DecisaoDef& p = *dados().conselho.decisao("panelaco");
    EXPECT_FALSE(w.decisaoPossivel(p)) << "com imposto médio ninguém protesta";
    w.estado.imposto = Imposto::Alto;
    EXPECT_TRUE(w.decisaoPossivel(p));
    w.trazerDecisao("panelaco");
    ASSERT_TRUE(w.decidir("baixar"));
    EXPECT_EQ(w.estado.imposto, Imposto::Medio);
    EXPECT_FALSE(w.decisaoPossivel(p));
    w.estado.imposto = Imposto::Alto;
    EXPECT_TRUE(w.decisaoPossivel(p)) << "este caso pode se repetir";
}

TEST(SaveV7, GuardaConselhoLeisECasoPendente) {
    const auto pasta = std::filesystem::temp_directory_path() / "koa_teste_save_v7";
    std::filesystem::remove_all(pasta);
    World w = comConselho();
    ASSERT_TRUE(w.nomear("marechal", "oswin"));
    w.lealdade["oswin"] = 77;
    w.leis.insert("patrulha");
    w.trazerDecisao("refugiados");
    SaveSystem s(pasta);
    ASSERT_TRUE(s.salvar(w.capturar()));
    EXPECT_TRUE(std::filesystem::exists(pasta / "Save_001/council.json"));
    std::string erro;
    const auto d = s.carregar("Save_001", &erro);
    ASSERT_TRUE(d) << erro;
    World w2 = vilarejo();
    w2.aplicar(*d);
    EXPECT_TRUE(w2.conselhoFormado());
    EXPECT_EQ(w2.ocupante("marechal"), "oswin");
    EXPECT_EQ(w2.lealdade.at("oswin"), 77);
    EXPECT_TRUE(w2.leis.contains("patrulha"));
    EXPECT_EQ(w2.decisaoPendente, "refugiados");
    std::filesystem::remove_all(pasta);
}

TEST(SaveV7, SaveV6ComecaSemConselho) {
    const auto pasta = std::filesystem::temp_directory_path() / "koa_teste_save_v6_para_v7";
    std::filesystem::remove_all(pasta);
    World w = vilarejo();
    obrasProntas(w, 5);
    SaveSystem s(pasta);
    ASSERT_TRUE(s.salvar(w.capturar()));
    std::filesystem::remove(pasta / "Save_001/council.json");
    auto meta = *ke::fs::lerTexto(pasta / "Save_001/metadata.json");
    const std::string atual = "\"versao_formato\": " + std::to_string(VERSAO_SAVE);
    meta.replace(meta.find(atual), atual.size(), "\"versao_formato\": 6");
    ke::fs::gravarTextoSeguro(pasta / "Save_001/metadata.json", meta);
    std::string erro;
    const auto d = s.carregar("Save_001", &erro);
    ASSERT_TRUE(d) << erro;
    World w2 = vilarejo();
    w2.aplicar(*d);
    EXPECT_TRUE(w2.conselho.empty());
    EXPECT_TRUE(w2.decisaoPendente.empty());
    std::filesystem::remove_all(pasta);
}
