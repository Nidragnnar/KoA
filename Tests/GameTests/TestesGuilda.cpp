// Fase 14: a Guilda dos Aventureiros, expedições, relíquias amaldiçoadas e o save v9
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
    return w;
}

bool temNota(const World& w, const std::string& trecho) {
    return std::ranges::any_of(w.ultimoRelatorio.notas, [&](const std::string& n) { return n.find(trecho) != std::string::npos; });
}

// Guilda pronta no nível pedido; na manhã seguinte os aventureiros chegam
World comGuilda(int nivel) {
    World w = vilarejo();
    w.reino.construcoes.push_back({"guilda_t", "guilda", "Guilda", nivel, {0, 0}, true, true});
    w.inventario.definir("comida", 300);
    w.dormir(false);
    return w;
}

void dormirAte(World& w, int dia) {
    while (w.relogio.dia < dia) w.dormir(false);
}

}

TEST(Guilda, DadosDosAventureirosDestinosEReliquias) {
    const DadosGuilda& g = dados().guilda;
    EXPECT_EQ(g.aventureiros.size(), 6u);
    EXPECT_EQ(g.destinos.size(), 5u);
    EXPECT_EQ(g.reliquias.size(), 11u);
    EXPECT_EQ(std::ranges::count_if(g.reliquias, [](const ReliquiaDef& r) { return r.amaldicoada; }), 3);
    EXPECT_EQ(g.destino("torre_selo")->requer, "reliquia:mapa_torre");
    EXPECT_EQ(poderAventureiro(*g.aventureiro("lira"), 2, *g.destino("biblioteca")), 3 + 1 + 1) << "força + nível - 1 + especialidade";
    EXPECT_STREQ(chanceEmPalavras(3), "Boas chances");
    EXPECT_STREQ(chanceEmPalavras(-3), "Suicida");
    EXPECT_EQ(desfechoPorMargem(0), Desfecho::Sucesso);
    EXPECT_EQ(desfechoPorMargem(-2), Desfecho::DurasPenas);
    for (int dia = 1; dia < 200; ++dia) {
        const int s = sorteExpedicao("pantano", dia);
        EXPECT_TRUE(s >= -2 && s <= 2);
    }
}

TEST(Guilda, SoComOReinoEstavel) {
    World w = vilarejo();
    const BuildingDef& g = dados().construcoes.at("guilda");
    EXPECT_FALSE(w.monumentoLiberado(g)) << "a guilda espera a estabilidade";
    for (int i = 0; i < 4; ++i) w.reino.construcoes.push_back({"moradia_t" + std::to_string(i), "moradia", "Moradia", 1, {0, 0}, true, true});
    w.reino.construcoes.push_back({"posto_t", "posto_guarda", "Posto", 2, {0, 0}, true, true});
    w.estado.populacao = 15;
    w.estado.felicidade = 80.0f;
    w.inventario.definir("comida", 300);
    w.dormir(false);
    EXPECT_TRUE(w.marcos.contains("estabilidade"));
    EXPECT_TRUE(temNota(w, "a Guilda pode ser construída"));
    EXPECT_TRUE(w.monumentoLiberado(g));
}

TEST(Guilda, CadaNivelRecrutaMaisAventureiros) {
    World w = comGuilda(1);
    EXPECT_EQ(w.aventureiros.size(), 3u);
    EXPECT_TRUE(temNota(w, "Kael, o Batedor entrou para a guilda"));
    EXPECT_EQ(w.vagasReliquias(), 1);
    World w3 = comGuilda(3);
    EXPECT_EQ(w3.aventureiros.size(), 6u);
    EXPECT_EQ(w3.vagasReliquias(), 3);
}

TEST(Guilda, PartirGastaComidaETiraOGrupoDaVila) {
    World w = comGuilda(1);
    w.inventario.definir("comida", 5);
    EXPECT_FALSE(w.partir("ruinas_vale", {"kael", "ysolde"})) << "2 dias × 2 aventureiros × 2 = 8 de comida";
    w.inventario.definir("comida", 20);
    EXPECT_FALSE(w.partir("ruinas_vale", {})) << "ninguém no grupo";
    EXPECT_FALSE(w.partir("torre_selo", {"kael"})) << "sem o mapa, ninguém sabe o caminho";
    ASSERT_TRUE(w.partir("ruinas_vale", {"kael", "ysolde"}));
    EXPECT_EQ(w.inventario.quantidade("comida"), 12);
    EXPECT_FALSE(w.aventureiroPronto("kael"));
    EXPECT_EQ(w.situacaoAventureiro("kael"), "em expedição");
    EXPECT_EQ(w.expedicao.volta, w.relogio.dia + 2);
    EXPECT_FALSE(w.partir("ruinas_vale", {"tomas"})) << "uma expedição por vez";
    EXPECT_TRUE(w.marcos.contains("expedicao:ruinas_vale"));
}

TEST(Guilda, GrupoForteVoltaEmTriunfoEMaisExperiente) {
    World w = comGuilda(3);
    const std::vector<std::string> grupo{"garrick", "ysolde", "lira"};
    const DestinoDef& d = *dados().guilda.destino("ruinas_vale");
    ASSERT_GE(w.margemSemSorte(d, grupo) - 2, 3) << "triunfo mesmo com a pior sorte";
    const int madeira = w.inventario.quantidade("madeira");
    ASSERT_TRUE(w.partir("ruinas_vale", grupo));
    dormirAte(w, w.expedicao.volta);
    EXPECT_TRUE(w.expedicao.destino.empty());
    EXPECT_TRUE(temNota(w, "foi um triunfo"));
    EXPECT_GE(w.inventario.quantidade("madeira"), madeira + 15) << "pelo menos 10 × 1,5";
    for (const auto& id : grupo) {
        EXPECT_EQ(w.nivelAventureiro(id), 2);
        EXPECT_TRUE(w.aventureiroPronto(id));
    }
}

TEST(Guilda, GrupoFracoDemaisEhUmDesastre) {
    World w = comGuilda(3);
    const DestinoDef& d = *dados().guilda.destino("fortaleza");
    ASSERT_LE(w.margemSemSorte(d, {"tomas"}) + 2, -3);
    const int ouro = w.inventario.quantidade("ouro");
    ASSERT_TRUE(w.partir("fortaleza", {"tomas"}));
    dormirAte(w, w.expedicao.volta);
    EXPECT_TRUE(temNota(w, "foi um desastre"));
    EXPECT_TRUE(temNota(w, "Irmão Tomás desapareceu"));
    EXPECT_GE(w.inventario.quantidade("ouro"), ouro) << "não perde nada além da comida";
    EXPECT_EQ(w.situacaoAventureiro("tomas"), "desaparecido");
    EXPECT_FALSE(w.partir("ruinas_vale", {"tomas"}));
}

TEST(Guilda, FeridoEsperaEDesaparecidoPodeVoltar) {
    World w = comGuilda(1);
    w.aventureiros["kael"].feridoAte = w.relogio.dia + 2;
    EXPECT_EQ(w.situacaoAventureiro("kael"), "ferido (2 dias)");
    EXPECT_FALSE(w.partir("ruinas_vale", {"kael"}));
    w.dormir(false);
    w.dormir(false);
    EXPECT_TRUE(w.aventureiroPronto("kael"));
    w.aventureiros["ysolde"].sumido = true;
    w.aventureiros["ysolde"].voltaDia = w.relogio.dia + 1;
    w.dormir(false);
    EXPECT_FALSE(w.aventureiros["ysolde"].sumido);
    EXPECT_TRUE(temNota(w, "Ysolde, a Arqueira voltou"));
}

TEST(Guilda, ReliquiaExpostaRendeEMaldicaoSoAparecePostaAMostra) {
    World w = comGuilda(1);  // uma vaga
    const float alvo = w.resumo().felicidadeAlvo;
    w.ganharReliquia("lamina_hadrian");
    w.ganharReliquia("coroa_ferro");
    EXPECT_TRUE(w.marcos.contains("reliquia:lamina_hadrian"));
    const int dano = w.danoArma();
    ASSERT_TRUE(w.exporReliquia("lamina_hadrian", true));
    EXPECT_EQ(w.danoArma(), dano + 3);
    EXPECT_FALSE(w.exporReliquia("coroa_ferro", true)) << "só uma vaga no Salão";
    ASSERT_TRUE(w.exporReliquia("lamina_hadrian", false));
    EXPECT_FALSE(w.estadoReliquia("coroa_ferro")->conhecida);
    const int seg = w.resumo().seguranca;
    ASSERT_TRUE(w.exporReliquia("coroa_ferro", true));
    EXPECT_TRUE(w.estadoReliquia("coroa_ferro")->conhecida) << "a maldição se revela";
    EXPECT_TRUE(std::ranges::any_of(w.tirarEventos(), [](const EventoJogo& e) { return e.tipo == EventoJogo::Tipo::Reliquia && e.recurso == "maldicao"; }));
    EXPECT_EQ(w.resumo().seguranca, seg + 6);
    EXPECT_FLOAT_EQ(w.resumo().felicidadeAlvo, alvo - 6.0f) << "a coroa tira 6 (a guilda já estava na conta)";
    // Maelis purifica: só com o rito da floresta e 3 cristais
    EXPECT_FALSE(w.purificarReliquia("coroa_ferro"));
    w.marcar("purificou:floresta");
    w.inventario.definir("cristal", 3);
    ASSERT_TRUE(w.purificarReliquia("coroa_ferro"));
    EXPECT_EQ(w.inventario.quantidade("cristal"), 0);
    EXPECT_FLOAT_EQ(w.resumo().felicidadeAlvo, alvo);
    EXPECT_EQ(w.resumo().seguranca, seg + 6) << "o bônus fica";
}

TEST(Guilda, OMapaAbreATorreDoSelo) {
    World w = comGuilda(1);
    const DestinoDef& torre = *dados().guilda.destino("torre_selo");
    EXPECT_FALSE(w.destinoAberto(torre));
    w.ganharReliquia("mapa_torre");
    EXPECT_TRUE(w.destinoAberto(torre));
}

TEST(Guilda, OlhoDaChamaChamaALuaDeSangue) {
    World w = comGuilda(1);
    w.marcar("visitou:floresta");
    auto luas = [&] {
        int n = 0;
        for (int d = 4; d < 1500; ++d)
            if (const EventoDef* ev = w.eventoDoDia(d); ev && ev->id == "lua_sangue") ++n;
        return n;
    };
    const int antes = luas();
    w.ganharReliquia("olho_chama");
    ASSERT_TRUE(w.exporReliquia("olho_chama", true));
    EXPECT_GT(luas(), antes * 3 / 2);
}

TEST(SaveV9, GuardaAventureirosExpedicaoEReliquias) {
    const auto pasta = std::filesystem::temp_directory_path() / "koa_teste_save_v9";
    std::filesystem::remove_all(pasta);
    World w = comGuilda(2);
    ASSERT_TRUE(w.partir("biblioteca", {"lira", "tomas"}));
    w.aventureiros["kael"].feridoAte = w.relogio.dia + 3;
    w.aventureiros["kael"].nivel = 3;
    w.ganharReliquia("ampulheta");
    ASSERT_TRUE(w.exporReliquia("ampulheta", true));
    SaveSystem s(pasta);
    ASSERT_TRUE(s.salvar(w.capturar()));
    EXPECT_TRUE(std::filesystem::exists(pasta / "Save_001/guild.json"));
    std::string erro;
    const auto d = s.carregar("Save_001", &erro);
    ASSERT_TRUE(d) << erro;
    World w2 = vilarejo();
    w2.aplicar(*d);
    EXPECT_EQ(w2.aventureiros.size(), w.aventureiros.size());
    EXPECT_EQ(w2.expedicao.destino, "biblioteca");
    EXPECT_EQ(w2.expedicao.grupo, (std::vector<std::string>{"lira", "tomas"}));
    EXPECT_EQ(w2.nivelAventureiro("kael"), 3);
    EXPECT_FALSE(w2.aventureiroPronto("kael"));
    ASSERT_NE(w2.estadoReliquia("ampulheta"), nullptr);
    EXPECT_TRUE(w2.estadoReliquia("ampulheta")->exposta);
    EXPECT_TRUE(w2.estadoReliquia("ampulheta")->conhecida);
    std::filesystem::remove_all(pasta);
}

TEST(SaveV9, SaveV8ComecaSemGuilda) {
    const auto pasta = std::filesystem::temp_directory_path() / "koa_teste_save_v8_para_v9";
    std::filesystem::remove_all(pasta);
    World w = vilarejo();
    SaveSystem s(pasta);
    ASSERT_TRUE(s.salvar(w.capturar()));
    std::filesystem::remove(pasta / "Save_001/guild.json");
    auto meta = *ke::fs::lerTexto(pasta / "Save_001/metadata.json");
    const std::string atual = "\"versao_formato\": " + std::to_string(VERSAO_SAVE);
    meta.replace(meta.find(atual), atual.size(), "\"versao_formato\": 8");
    ke::fs::gravarTextoSeguro(pasta / "Save_001/metadata.json", meta);
    std::string erro;
    const auto d = s.carregar("Save_001", &erro);
    ASSERT_TRUE(d) << erro;
    World w2 = vilarejo();
    w2.aplicar(*d);
    EXPECT_TRUE(w2.aventureiros.empty());
    EXPECT_TRUE(w2.expedicao.destino.empty());
    EXPECT_TRUE(w2.reliquias.empty());
    std::filesystem::remove_all(pasta);
}
