// Fase 19: os povos vizinhos (relações, tratados, missões), a reputação do herdeiro e a intriga no conselho; save v14
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
    w.inventario.definir("comida", 300);
    return w;
}

void ricos(World& w) {
    for (const char* r : {"madeira", "pedra", "ouro"}) w.inventario.definir(r, 2000);
    w.inventario.definir("cristal", 50);
}

bool temNota(const World& w, const std::string& trecho) {
    return std::ranges::any_of(w.ultimoRelatorio.notas, [&](const std::string& n) { return n.find(trecho) != std::string::npos; });
}

}

TEST(Faccoes, DadosDosVizinhos) {
    const DadosFaccoes& d = dados().faccoes;
    ASSERT_EQ(d.faccoes.size(), 3u);
    for (const FaccaoDef& f : d.faccoes) {
        SCOPED_TRACE(f.id);
        EXPECT_EQ(std::ranges::count_if(d.tratados, [&](const TratadoDef& t) { return t.faccao == f.id; }), 2);
        EXPECT_EQ(std::ranges::count_if(dados().missoes, [&](const MissaoDef& m) { return m.faccao == f.id; }), 2);
        // Cada povo reage a pelo menos 3 decisões do conselho
        EXPECT_GE(std::ranges::count_if(d.reacoes, [&](const ReacaoMarco& r) { return r.marco.starts_with("decisao:") && r.faccoes.contains(f.id); }), 3);
    }
    EXPECT_EQ(d.faixa(60), "aliados");
    EXPECT_EQ(d.faixa(0), "neutros");
    EXPECT_EQ(d.faixa(-70), "hostis");
    EXPECT_EQ(textoTratado(d.tratado("rota_pedra")->efeitos), "+3 de pedra por dia, -2 de ouro por dia");
}

TEST(Faccoes, OsVizinhosReagemAoQueAcontece) {
    World w = vilarejo();
    const float culto = w.relacaoCom("culto"), ordem = w.relacaoCom("ordem");
    w.marcar("decisao:pregadores:tolerar", "");
    EXPECT_FLOAT_EQ(w.relacaoCom("culto"), culto + 15.0f);
    EXPECT_FLOAT_EQ(w.relacaoCom("ordem"), ordem - 10.0f);
    EXPECT_FLOAT_EQ(w.reputacao.devocao, -5.0f);
    const float anoes = w.relacaoCom("anoes");
    w.marcar("horda:3", "");  // marco com * nos dados
    EXPECT_FLOAT_EQ(w.relacaoCom("anoes"), anoes + 3.0f);
    w.marcar("horda:3", "");  // o mesmo marco não conta duas vezes
    EXPECT_FLOAT_EQ(w.relacaoCom("anoes"), anoes + 3.0f);
}

TEST(Faccoes, TratadosPedemRelacaoEPesamNosRivais) {
    World w = vilarejo();
    ricos(w);
    EXPECT_FALSE(w.podeAssinar("rota_pedra").empty()) << "os anões ainda não confiam";
    EXPECT_FALSE(w.assinarTratado("rota_pedra"));
    w.mudarRelacao("anoes", 20.0f);
    const ResumoReino antes = w.resumo();
    ASSERT_TRUE(w.assinarTratado("rota_pedra"));
    EXPECT_EQ(w.resumo().pedraDia, antes.pedraDia + 3);
    EXPECT_TRUE(w.marcos.contains("tratado:rota_pedra"));
    // Assinar com a Ordem tira do Culto (rivais)
    w.mudarRelacao("ordem", 20.0f);
    const float culto = w.relacaoCom("culto");
    ASSERT_TRUE(w.assinarTratado("pacto_saber"));
    EXPECT_FLOAT_EQ(w.relacaoCom("culto"), culto - 20.0f);
    // Os efeitos que não passam pelo dia: dano e hordas
    w.mudarRelacao("culto", 100.0f);
    const int dano = w.danoArma();
    ASSERT_TRUE(w.assinarTratado("chama_compartilhada"));
    EXPECT_EQ(w.danoArma(), dano + 3);
    w.mudarRelacao("ordem", 100.0f);
    w.hordaDia = w.relogio.dia + 5;
    const int forca = w.forcaProximaHorda();
    ASSERT_TRUE(w.assinarTratado("guarda_arcana"));
    EXPECT_LT(w.forcaProximaHorda(), forca);
    // Romper custa relação
    const float anoes = w.relacaoCom("anoes");
    ASSERT_TRUE(w.romperTratado("rota_pedra"));
    EXPECT_FLOAT_EQ(w.relacaoCom("anoes"), anoes - 15.0f);
}

TEST(Faccoes, RelacaoQueDespencaRompeOTratado) {
    World w = vilarejo();
    ricos(w);
    ASSERT_TRUE(w.assinarTratado("tributo_cinzas"));  // o Culto aceita até quem desconfia dele
    w.mudarRelacao("culto", -60.0f);
    w.dormir(false);
    EXPECT_FALSE(w.tratados.contains("tributo_cinzas"));
    EXPECT_TRUE(temNota(w, "romperam o tratado Tributo das Cinzas"));
    EXPECT_TRUE(w.marcos.contains("rompeu:tributo_cinzas"));
}

TEST(Faccoes, MissaoDeFaccaoSobeARelacao) {
    World w = vilarejo();
    ASSERT_TRUE(w.aceitarMissaoFaccao("montanha_respira"));
    EXPECT_FALSE(w.aceitarMissaoFaccao("montanha_respira")) << "a mesma missão não começa duas vezes";
    const float anoes = w.relacaoCom("anoes");
    w.marcar("minas_abertas", "");
    w.dormir(false);  // a missão de marco se confere com o mundo (na região nova ou de noite)
    EXPECT_TRUE(w.missoes.at("montanha_respira").concluida);
    EXPECT_NEAR(w.relacaoCom("anoes"), anoes + 10.0f + 20.0f, 1.0f) << "a reação ao marco e a missão (menos a deriva da noite)";
}

TEST(Reputacao, FamaMudaTitulosEFalas) {
    World w = vilarejo();
    for (const char* m : {"decisao:refugiados:acolher", "decisao:nobre_isencao:recusar", "decisao:panelaco:baixar", "decisao:disputa_campo:dividir",
                          "decisao:desvio:prender"})
        w.marcar(m, "");
    EXPECT_GE(w.reputacao.justica, 30.0f);
    EXPECT_TRUE(w.marcos.contains("fama:justo"));
    EXPECT_FALSE(w.marcos.contains("fama:tirano"));
    // A fama entra nos títulos (com o reino de pé)
    const auto& titulos = dados().lore.titulos;
    EXPECT_TRUE(std::ranges::any_of(titulos, [](const TituloDef& t) { return t.id == "justo"; }));
    // Imposto alto, noite após noite, puxa para o lado do tirano
    World t = vilarejo();
    t.estado.imposto = Imposto::Alto;
    for (int i = 0; i < 70; ++i) t.dormir(false);
    EXPECT_LE(t.reputacao.justica, -30.0f);
    EXPECT_TRUE(t.marcos.contains("fama:tirano"));
}

TEST(Intriga, ConselheiroDescontenteDesviaOuroEPodeSerPego) {
    World w = vilarejo();
    ricos(w);
    w.marcar("conselho_formado", "", true);
    ASSERT_TRUE(w.nomear("tesoureiro", "edda"));
    w.lealdade["edda"] = 5;
    int noites = 0;
    while (w.suspeito.empty() && noites < 30) {
        w.lealdade["edda"] = 5;  // segue descontente
        w.dormir(false);
        ++noites;
    }
    ASSERT_EQ(w.suspeito, "edda") << "em " << noites << " noites";
    EXPECT_GT(w.desvios["edda"], 0);
    EXPECT_EQ(w.decisaoPendente, "desvio");
    EXPECT_TRUE(temNota(w, "anda desviando"));
    const int levou = w.desvios["edda"], ouro = w.inventario.quantidade("ouro");
    ASSERT_TRUE(w.decidir("prender"));
    EXPECT_TRUE(w.ocupante("tesoureiro").empty()) << "saiu do conselho";
    EXPECT_EQ(w.inventario.quantidade("ouro"), ouro + levou / 2);
    EXPECT_TRUE(w.desvios.empty());
    EXPECT_TRUE(w.suspeito.empty());
}

TEST(Intriga, ConselheiroLealNaoDesvia) {
    World w = vilarejo();
    ricos(w);
    w.marcar("conselho_formado", "", true);
    ASSERT_TRUE(w.nomear("tesoureiro", "edda"));
    w.lealdade["edda"] = 80;
    for (int i = 0; i < 5; ++i) w.dormir(false);
    EXPECT_TRUE(w.desvios.empty());
}

TEST(SaveV14, GuardaOsVizinhosERefazOsAntigosPelosMarcos) {
    const auto pasta = std::filesystem::temp_directory_path() / "koa_teste_save_v14";
    std::filesystem::remove_all(pasta);
    World w = vilarejo();
    ricos(w);
    w.marcar("decisao:pregadores:tolerar", "");
    w.mudarRelacao("anoes", 25.0f);
    ASSERT_TRUE(w.assinarTratado("rota_pedra"));
    w.desvios["bram"] = 12;
    SaveSystem s(pasta);
    ASSERT_TRUE(s.salvar(w.capturar()));
    EXPECT_TRUE(std::filesystem::exists(pasta / "Save_001/factions.json"));
    auto d = s.carregar();
    ASSERT_TRUE(d);
    World w2 = vilarejo();
    w2.aplicar(*d);
    EXPECT_FLOAT_EQ(w2.relacaoCom("anoes"), w.relacaoCom("anoes"));
    EXPECT_TRUE(w2.tratados.contains("rota_pedra"));
    EXPECT_FLOAT_EQ(w2.reputacao.devocao, w.reputacao.devocao);
    EXPECT_EQ(w2.desvios["bram"], 12);
    // Save v13: refeito pelo que já aconteceu (o marco dos pregadores continua lá)
    std::filesystem::remove(pasta / "Save_001/factions.json");
    auto meta = *ke::fs::lerTexto(pasta / "Save_001/metadata.json");
    const std::string atual = "\"versao_formato\": " + std::to_string(VERSAO_SAVE);
    meta.replace(meta.find(atual), atual.size(), "\"versao_formato\": 13");
    ke::fs::gravarTextoSeguro(pasta / "Save_001/metadata.json", meta);
    d = s.carregar();
    ASSERT_TRUE(d);
    EXPECT_FALSE(d->temFaccoes);
    World w3 = vilarejo();
    w3.aplicar(*d);
    const float inicialCulto = dados().faccoes.faccao("culto")->inicial;
    EXPECT_FLOAT_EQ(w3.relacaoCom("culto"), inicialCulto + 15.0f) << "os pregadores tolerados, refeitos pelo marco";
    EXPECT_FLOAT_EQ(w3.reputacao.devocao, -5.0f);
    EXPECT_TRUE(w3.tratados.empty()) << "tratados não se refazem";
    std::filesystem::remove_all(pasta);
}
