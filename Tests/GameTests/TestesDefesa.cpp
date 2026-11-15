// Fase 13: hordas avisadas, a Muralha Leste, torres, soldados, saques, reparos e o save v8
#include <gtest/gtest.h>

#include <glm/geometric.hpp>

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

void passos(World& w, int n, EntradaMundo e = {}) {
    for (int i = 0; i < n; ++i) w.passo(1.0f / 60.0f, e);
}

void obra(World& w, const std::string& id, const std::string& def, int nivel, glm::ivec2 tile) {
    w.reino.construcoes.push_back({id, def, def, nivel, tile, true, true});
}

// Muralha no nível pedido (pagando a reconstrução de verdade)
void erguerMuralha(World& w, int nivel) {
    for (const char* r : {"madeira", "pedra", "ouro"}) w.inventario.definir(r, 1000);
    while (w.muralha()->level < nivel) ASSERT_TRUE(w.melhorarConstrucao("muralha_leste"));
    w.tirarEventos();
}

// Herói num canto longe da estrada (as criaturas não o enxergam)
void longeDaEstrada(World& w) {
    w.jogador.pos = {3 * 16 + 8.0f, 3 * 16 + 12.0f};
    w.jogador.vida = w.jogador.vidaMax;
}

void comecarBatalha(World& w) {
    w.agendarHorda(w.relogio.dia);
    w.relogio.minutos = dados().defesa.hora * 60.0f - 0.2f;
    passos(w, 30);
    ASSERT_TRUE(w.emBatalha());
}

// Corre a batalha até acabar (ou o tempo esgotar)
bool batalharAteOFim(World& w, int segundos) {
    for (int i = 0; i < segundos * 60 && w.emBatalha(); ++i) {
        w.jogador.vida = w.jogador.vidaMax;
        w.passo(1.0f / 60.0f, {});
    }
    return !w.emBatalha();
}

bool temNota(const World& w, const std::string& trecho) {
    return std::ranges::any_of(w.ultimoRelatorio.notas, [&](const std::string& n) { return n.find(trecho) != std::string::npos; });
}

}

TEST(Defesa, DadosDaHordaEDaMuralha) {
    const DadosDefesa& d = dados().defesa;
    EXPECT_EQ(d.avisoDias, 3);
    EXPECT_EQ(d.hora, 20);
    EXPECT_EQ(d.custo.front().first, "esqueleto") << "da criatura mais cara para a mais barata";
    EXPECT_EQ(dados().construcoes.at("muralha").nivelMax(), 4);
    EXPECT_EQ(dados().construcoes.at("torre").limite, 3);
    const auto c = composicaoHorda(d, 8);
    EXPECT_EQ(c, (std::vector<std::string>{"esqueleto", "lobo", "sombra", "lobo"}));
    EXPECT_EQ(forcaHorda(d, 1, 70.0f), 15);
    EXPECT_EQ(forcaHorda(d, 3, 0.0f), 14);
}

TEST(Defesa, MuralhaEmRuinasNaoSeguraEDePeFechaTudoMenosOPortao) {
    World w = vilarejo();
    ASSERT_NE(w.muralha(), nullptr);
    EXPECT_EQ(w.portaoVidaMax(), 0);
    EXPECT_EQ(w.reino.desenvolvimento(), 0) << "as ruínas não contam para a era";
    // Em ruínas: anda-se por cima
    w.jogador.pos = {58 * 16 + 8.0f, 18 * 16 + 14.0f};
    passos(w, 45, {{1, 0}});  // sem chegar à saída para a floresta
    EXPECT_GT(w.jogador.pos.x, 61 * 16.0f);
    erguerMuralha(w, 2);
    EXPECT_EQ(w.portaoVida, 150);
    EXPECT_EQ(w.portaoVidaMax(), 150);
    EXPECT_EQ(w.reino.desenvolvimento(), 1);
    // De pé: o muro segura...
    w.jogador.pos = {58 * 16 + 8.0f, 18 * 16 + 14.0f};
    passos(w, 60, {{1, 0}});
    EXPECT_LT(w.jogador.pos.x, 60 * 16.0f);
    // ...mas pelo portão o herdeiro passa
    w.jogador.pos = {58 * 16 + 8.0f, 19 * 16 + 14.0f};
    passos(w, 45, {{1, 0}});  // sem chegar à saída para a floresta
    EXPECT_GT(w.jogador.pos.x, 61 * 16.0f);
    EXPECT_TRUE(w.marcos.contains("muralha_n2"));
}

TEST(Defesa, BatedoresAvisamComDiasDeAntecedencia) {
    World w = vilarejo();
    for (int i = 0; i < 3; ++i) obra(w, "moradia_t" + std::to_string(i), "moradia", 1, {0, 0});  // casa para todo mundo
    w.estado.populacao = 14;
    w.estado.felicidade = 60.0f;
    w.inventario.definir("comida", 200);
    w.relogio.dia = 5;
    w.dormir(false);
    EXPECT_EQ(w.hordaDia, 0) << "antes do dia 10 a floresta não manda horda";
    w.relogio.dia = 10;
    w.dormir(false);
    EXPECT_EQ(w.hordaDia, w.relogio.dia + 3);
    EXPECT_TRUE(temNota(w, "Batedores avistaram uma horda"));
    // Com um Marechal no conselho, os batedores avisam um dia antes
    World w2 = vilarejo();
    for (int i = 0; i < 4; ++i) obra(w2, "moradia_t" + std::to_string(i), "moradia", 1, {0, 0});
    w2.estado.populacao = 14;
    w2.estado.felicidade = 60.0f;
    w2.inventario.definir("comida", 200);
    w2.relogio.dia = 10;
    w2.dormir(false);  // forma o conselho (4 obras)
    w2.hordaDia = 0;
    ASSERT_TRUE(w2.nomear("marechal", "oswin"));
    w2.dormir(false);
    EXPECT_EQ(w2.hordaDia, w2.relogio.dia + 4);
}

TEST(Defesa, AsVinteHorasAHordaChegaComOsSoldadosDosPostos) {
    World w = vilarejo();
    obra(w, "posto_t", "posto_guarda", 1, {50, 24});
    longeDaEstrada(w);
    const int esperadas = static_cast<int>(composicaoHorda(dados().defesa, w.forcaProximaHorda()).size());
    comecarBatalha(w);
    EXPECT_EQ(w.criaturasDaHorda(), esperadas);
    EXPECT_EQ(w.soldadosNoPortao(), 2);
    EXPECT_EQ(w.hordaNumero, 1);
    const auto ev = w.tirarEventos();
    EXPECT_TRUE(std::ranges::any_of(ev, [](const EventoJogo& e) { return e.tipo == EventoJogo::Tipo::HordaChegou; }));
}

TEST(Defesa, OPortaoSeguraAsCriaturasEApanha) {
    World w = vilarejo();
    erguerMuralha(w, 3);
    longeDaEstrada(w);
    comecarBatalha(w);
    passos(w, 60 * 4);
    EXPECT_LT(w.portaoVida, w.portaoVidaMax());
    const float muro = w.caixaPortao().min.x;
    for (auto [e, p, i] : w.reg.view<const Posicao, const Inimigo>().each())
        if (i.horda) EXPECT_GT(p.v.x, muro) << "nenhuma criatura passou do portão de pé";
}

TEST(Defesa, SemPortaoElasSaqueiamAPraca) {
    World w = vilarejo();
    w.inventario.definir("comida", 100);
    w.inventario.definir("ouro", 100);
    longeDaEstrada(w);
    comecarBatalha(w);
    ASSERT_TRUE(batalharAteOFim(w, 120));
    EXPECT_LT(w.inventario.quantidade("comida"), 100);
    EXPECT_TRUE(w.marcos.contains("horda_saqueada:1"));
    EXPECT_EQ(w.hordaDia, 0);
    EXPECT_GT(w.proximaHorda, w.relogio.dia + 8);
    w.dormir(false);
    EXPECT_TRUE(temNota(w, "saqueou"));
}

TEST(Defesa, TorresESoldadosVencemAHordaSemSaque) {
    World w = vilarejo();
    erguerMuralha(w, 4);
    obra(w, "torre_a", "torre", 3, {54, 14});
    obra(w, "torre_b", "torre", 3, {54, 23});
    obra(w, "posto_a", "posto_guarda", 3, {50, 24});
    obra(w, "posto_b", "posto_guarda", 3, {48, 13});
    longeDaEstrada(w);
    const int ouro = w.inventario.quantidade("ouro");
    comecarBatalha(w);
    bool flecha = false;
    for (int i = 0; i < 60 * 3; ++i) {
        w.passo(1.0f / 60.0f, {});
        for (const auto& e : w.tirarEventos()) flecha = flecha || e.tipo == EventoJogo::Tipo::Flecha;
    }
    EXPECT_TRUE(flecha) << "as torres atiram";
    ASSERT_TRUE(batalharAteOFim(w, 120));
    EXPECT_TRUE(w.marcos.contains("horda:1"));
    EXPECT_GT(w.inventario.quantidade("ouro"), ouro) << "ouro das criaturas e da gratidão do povo";
    EXPECT_EQ(w.soldadosNoPortao(), 0) << "os soldados voltam aos postos";
}

TEST(Defesa, ComOHerdeiroLongeAsDefesasLutamSozinhas) {
    // Defesa forte: segura
    World w = vilarejo();
    erguerMuralha(w, 4);
    obra(w, "torre_a", "torre", 3, {54, 14});
    obra(w, "posto_a", "posto_guarda", 3, {50, 24});
    w.entrarRegiao("floresta", {3, 17}, Direcao::Direita);
    w.agendarHorda(w.relogio.dia);
    w.relogio.minutos = 20 * 60.0f;
    passos(w, 2);
    EXPECT_FALSE(w.emBatalha());
    EXPECT_EQ(w.hordaDia, 0);
    EXPECT_TRUE(w.marcos.contains("horda:1"));
    // Sem defesa nenhuma: a praça é saqueada
    World w2 = vilarejo();
    w2.inventario.definir("comida", 100);
    w2.entrarRegiao("floresta", {3, 17}, Direcao::Direita);
    w2.agendarHorda(w2.relogio.dia);
    w2.relogio.minutos = 20 * 60.0f;
    passos(w2, 2);
    EXPECT_TRUE(w2.marcos.contains("horda_saqueada:1"));
    EXPECT_LT(w2.inventario.quantidade("comida"), 100);
}

TEST(Defesa, DormirNaNoiteDaHordaDeixaAsDefesasLutando) {
    World w = vilarejo();
    erguerMuralha(w, 2);
    w.agendarHorda(w.relogio.dia);
    w.dormir(false);
    EXPECT_EQ(w.hordaDia, 0);
    EXPECT_EQ(w.hordaNumero, 1);
    EXPECT_TRUE(temNota(w, "enquanto o herdeiro dormia"));
}

TEST(Defesa, PortaoSeReparaPeloPainelEPeloMestreDasObras) {
    World w = vilarejo();
    erguerMuralha(w, 2);
    w.portaoVida = 50;
    const Custo c = w.custoReparo();
    EXPECT_EQ(c.at("pedra"), 10);
    EXPECT_EQ(c.at("madeira"), 5);
    w.inventario.definir("pedra", 10);
    w.inventario.definir("madeira", 5);
    ASSERT_TRUE(w.repararPortao());
    EXPECT_EQ(w.portaoVida, 150);
    EXPECT_EQ(w.inventario.quantidade("pedra"), 0);
    EXPECT_TRUE(w.custoReparo().empty());
    // Mestre das Obras contente conserta uma parte toda noite
    w.marcar("conselho_formado");
    ASSERT_TRUE(w.nomear("obras", "bram"));
    w.lealdade["bram"] = 80;
    w.portaoVida = 50;
    w.dormir(false);
    EXPECT_EQ(w.portaoVida, 80) << "30% de 100 que faltavam";
    EXPECT_TRUE(temNota(w, "consertou parte do portão"));
}

TEST(SaveV8, GuardaHordaEPortao) {
    const auto pasta = std::filesystem::temp_directory_path() / "koa_teste_save_v8";
    std::filesystem::remove_all(pasta);
    World w = vilarejo();
    erguerMuralha(w, 3);
    w.portaoVida = 123;
    w.agendarHorda(w.relogio.dia + 2);
    w.hordaNumero = 2;
    w.proximaHorda = 40;
    SaveSystem s(pasta);
    ASSERT_TRUE(s.salvar(w.capturar()));
    EXPECT_TRUE(std::filesystem::exists(pasta / "Save_001/defense.json"));
    std::string erro;
    const auto d = s.carregar("Save_001", &erro);
    ASSERT_TRUE(d) << erro;
    World w2 = vilarejo();
    w2.aplicar(*d);
    EXPECT_EQ(w2.muralha()->level, 3);
    EXPECT_EQ(w2.portaoVida, 123);
    EXPECT_EQ(w2.hordaDia, w.hordaDia);
    EXPECT_EQ(w2.hordaNumero, 2);
    EXPECT_EQ(w2.proximaHorda, 40);
    std::filesystem::remove_all(pasta);
}

TEST(SaveV8, SaveV7ComecaSemHordaEComPortaoInteiro) {
    const auto pasta = std::filesystem::temp_directory_path() / "koa_teste_save_v7_para_v8";
    std::filesystem::remove_all(pasta);
    World w = vilarejo();
    erguerMuralha(w, 2);
    SaveSystem s(pasta);
    ASSERT_TRUE(s.salvar(w.capturar()));
    std::filesystem::remove(pasta / "Save_001/defense.json");
    auto meta = *ke::fs::lerTexto(pasta / "Save_001/metadata.json");
    const std::string atual = "\"versao_formato\": " + std::to_string(VERSAO_SAVE);
    meta.replace(meta.find(atual), atual.size(), "\"versao_formato\": 7");
    ke::fs::gravarTextoSeguro(pasta / "Save_001/metadata.json", meta);
    std::string erro;
    const auto d = s.carregar("Save_001", &erro);
    ASSERT_TRUE(d) << erro;
    EXPECT_FALSE(d->temDefesa);
    World w2 = vilarejo();
    w2.aplicar(*d);
    EXPECT_EQ(w2.hordaDia, 0);
    EXPECT_EQ(w2.portaoVida, 150);
    std::filesystem::remove_all(pasta);
}
