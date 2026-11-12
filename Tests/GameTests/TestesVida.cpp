// Fase 4: relógio, moradores, rotinas, diálogos, amizade, sono e save v2
#include <gtest/gtest.h>

#include <fstream>

#include <glm/geometric.hpp>
#include <nlohmann/json.hpp>

#include "Engine/Platform/FileSystem.h"
#include "Engine/World/Pathfinding.h"
#include "Game/Exploration/Components.h"
#include "Game/World/World.h"

using namespace koa;
using nlohmann::json;

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

}

TEST(Relogio, DiaVaiDas6As2DaManha) {
    Relogio r;
    EXPECT_EQ(r.textoHora(), "06:00");
    EXPECT_FALSE(r.avancar(7.0f));  // 7 s reais = 10 minutos
    EXPECT_EQ(r.textoHora(), "06:10");
    r.minutos = 23 * 60 + 55;
    EXPECT_EQ(r.textoHora(), "23:50") << "o relógio anda de 10 em 10";
    r.minutos = 25 * 60 + 30;
    EXPECT_EQ(r.textoHora(), "01:30");
    EXPECT_TRUE(r.avancar(30.0f)) << "às 2:00 o dia acaba";
    r.novoDia();
    EXPECT_EQ(r.dia, 2);
    EXPECT_EQ(r.textoHora(), "06:00");
}

TEST(Relogio, EstacoesESemana) {
    Relogio r;
    EXPECT_EQ(r.textoData(), "Primavera, dia 1 (Segunda)");
    r.dia = 29;
    EXPECT_EQ(r.estacao(), Estacao::Verao);
    EXPECT_EQ(r.diaDaEstacao(), 1);
    r.dia = 3 * 28 + 5;
    EXPECT_EQ(r.estacao(), Estacao::Inverno);
    r.dia = 4 * 28 + 1;
    EXPECT_EQ(r.estacao(), Estacao::Primavera);
    EXPECT_EQ(r.ano(), 2);
}

TEST(Relogio, EscureceAoAnoitecer) {
    Relogio r;
    r.minutos = 12 * 60;
    EXPECT_FLOAT_EQ(r.escuridao(), 0.0f);
    EXPECT_EQ(r.luzAmbiente(), glm::vec3(1.0f));
    r.minutos = 19 * 60;
    EXPECT_GT(r.escuridao(), 0.2f);
    EXPECT_LT(r.escuridao(), 0.8f);
    r.minutos = 23 * 60;
    EXPECT_FLOAT_EQ(r.escuridao(), 1.0f);
    EXPECT_LT(r.luzAmbiente().r, 0.3f);
}

TEST(Caminho, ContornaParedeEFalhaQuandoFechado) {
    // Parede vertical em x = 5 com uma brecha em y = 8
    auto bloqueado = [](int x, int y) { return x == 5 && y != 8; };
    const auto c = ke::buscarCaminho(10, 10, {1, 1}, {8, 1}, bloqueado);
    ASSERT_FALSE(c.empty());
    EXPECT_EQ(c.back(), glm::ivec2(8, 1));
    EXPECT_TRUE(std::ranges::any_of(c, [](glm::ivec2 p) { return p == glm::ivec2(5, 8); })) << "tem que passar pela brecha";
    auto fechado = [](int x, int) { return x == 5; };
    EXPECT_TRUE(ke::buscarCaminho(10, 10, {1, 1}, {8, 1}, fechado).empty());
}

TEST(Moradores, CarregamComRotinaEDialogos) {
    ASSERT_EQ(dados().npcs.size(), 5u);
    const NpcDef& bram = dados().npcs.at("bram");
    EXPECT_EQ(bram.profissao, "Ferreiro");
    EXPECT_EQ(bram.rotinaEm(6 * 60)->local, "casa_bram");
    EXPECT_EQ(bram.rotinaEm(9 * 60)->acao, "trabalhar");
    EXPECT_EQ(bram.rotinaEm(12 * 60 + 30)->local, "poco");
    EXPECT_EQ(bram.rotinaEm(23 * 60)->acao, "dormir");
    EXPECT_FALSE(bram.dialogos.empty());
}

TEST(Moradores, EscolhaDeFalaSegueCondicoes) {
    const NpcDef& bram = dados().npcs.at("bram");
    Relacionamento rel;
    ContextoFala ctx{Periodo::Manha, Estacao::Primavera, 1};
    EXPECT_EQ(escolherFala(bram, rel, ctx)->id, "bram_conhece") << "primeiro encontro vem antes de tudo";
    rel.conheceu = true;
    ctx.nivelCasa = 2;
    EXPECT_EQ(escolherFala(bram, rel, ctx)->id, "bram_casa") << "reage à casa reconstruída";
    rel.memoria.insert("bram_casa");
    EXPECT_NE(escolherFala(bram, rel, ctx)->id, "bram_casa") << "fala 'uma_vez' não se repete";
    ctx.periodo = Periodo::Noite;
    EXPECT_NE(escolherFala(bram, rel, ctx)->falas[0].find("fogueira"), std::string::npos);
    rel.falouHoje = true;
    EXPECT_NE(escolherFala(bram, rel, ctx)->falas[0].find("Já falei"), std::string::npos);
}

TEST(Moradores, ApareceNoLugarDaRotina) {
    World w = vilarejo();
    w.relogio.minutos = 9 * 60;
    w.posicionarMoradores();
    const glm::ivec2 forja = w.locais.at("forja_ruina");
    const glm::vec2 p = w.reg.get<Posicao>(w.morador("bram")).v;
    EXPECT_LE(std::abs(static_cast<int>(p.x) / 16 - forja.x), 1);
    EXPECT_LE(std::abs(static_cast<int>(p.y) / 16 - forja.y), 1);
    w.relogio.minutos = 23 * 60;
    w.posicionarMoradores();
    EXPECT_TRUE(w.reg.get<Agenda>(w.morador("bram")).dormindo);
}

TEST(Moradores, AndamAteONovoLocalNaTrocaDeRotina) {
    World w = vilarejo();
    w.relogio.minutos = 12 * 60 + 30;  // Bram no poço
    w.posicionarMoradores();
    const entt::entity bram = w.morador("bram");
    w.jogador.pos = w.reg.get<Posicao>(bram).v;  // perto: simulação completa
    w.relogio.minutos = 13 * 60 - 0.1f;
    for (int i = 0; i < 60 * 40; ++i) w.passo(1.0f / 60.0f, {});  // 40 s reais
    const glm::ivec2 forja = w.locais.at("forja_ruina");
    const glm::vec2 p = w.reg.get<Posicao>(bram).v;
    EXPECT_LE(glm::distance(p, glm::vec2(forja * 16) + glm::vec2(8, 12)), 24.0f) << "chegou à forja andando";
}

TEST(Moradores, AmizadeSobeUmaVezPorDiaELembra) {
    World w = vilarejo();
    w.reino.construcao("casa_herdeiro")->level = 2;
    Conversa c1 = w.conversar("edda");
    EXPECT_EQ(c1.nome, "Edda");
    EXPECT_TRUE(c1.ganhouAmizade);
    EXPECT_NE(c1.falas[0].find("Majestade"), std::string::npos) << "primeiro encontro";
    Conversa c2 = w.conversar("edda");
    EXPECT_FALSE(c2.ganhouAmizade) << "amizade só uma vez por dia";
    EXPECT_NE(c2.falas[0].find("chaminé"), std::string::npos) << "a novidade (casa nova) vem antes do 'já falamos hoje'";
    EXPECT_TRUE(w.reg.get<Relacionamento>(w.morador("edda")).memoria.contains("edda_casa"));
    Conversa c2b = w.conversar("edda");
    EXPECT_NE(c2b.falas[0].find("Volte mais tarde"), std::string::npos) << "lembrou que já comentou a casa";
    EXPECT_EQ(w.reg.get<Relacionamento>(w.morador("edda")).afinidade, 25);
    w.dormir(false);
    Conversa c3 = w.conversar("edda");
    EXPECT_TRUE(c3.ganhouAmizade) << "dia novo, amizade nova";
    EXPECT_EQ(w.reg.get<Relacionamento>(w.morador("edda")).afinidade, 50);
    w.encerrarConversa("edda");
    EXPECT_FALSE(w.reg.get<Agenda>(w.morador("edda")).conversando);
}

TEST(Sono, DormirComecaONovoDiaNaPortaDeCasa) {
    World w = vilarejo();
    w.relogio.minutos = 20 * 60;
    w.dormir(false);
    EXPECT_EQ(w.relogio.dia, 2);
    EXPECT_FLOAT_EQ(w.relogio.minutos, 360.0f);
    const BuildingData& casa = *w.reino.construcao("casa_herdeiro");
    EXPECT_LT(glm::distance(w.jogador.pos, w.pontoPorta(casa)), 16.0f);
}

TEST(Sono, DesmaioAs2CustaOuro) {
    World w = vilarejo();
    w.inventario.definir("ouro", 120);
    w.relogio.minutos = Relogio::FIM_DIA - 0.01f;
    w.passo(1.0f / 60.0f, {});
    w.passo(1.0f / 60.0f, {});
    const auto ev = w.tirarEventos();
    EXPECT_EQ(std::ranges::count_if(ev, [](const EventoJogo& e) { return e.tipo == EventoJogo::Tipo::Desmaiou; }), 1)
        << "o aviso de desmaio sai uma vez só";
    w.dormir(true);
    const auto& v = w.ultimoRelatorio.variacoes;
    const auto perda = std::ranges::find_if(v, [](const auto& p) { return p.first == "ouro (desmaio)"; });
    ASSERT_NE(perda, v.end());
    EXPECT_EQ(perda->second, -12) << "10% de 120";
    EXPECT_EQ(w.relogio.dia, 2);
}

TEST(SaveV2, GuardaRelogioEAmizades) {
    const auto pasta = std::filesystem::temp_directory_path() / "koa_teste_save_v2";
    std::filesystem::remove_all(pasta);
    World w = vilarejo();
    w.relogio.dia = 9;
    w.relogio.minutos = 15 * 60;
    w.conversar("tilda");
    SaveSystem s(pasta);
    ASSERT_TRUE(s.salvar(w.capturar()));
    EXPECT_TRUE(std::filesystem::exists(pasta / "Save_001/npcs.json"));
    EXPECT_TRUE(std::filesystem::exists(pasta / "Save_001/relationships.json"));
    World w2 = vilarejo();
    const auto d = s.carregar();
    ASSERT_TRUE(d);
    w2.aplicar(*d);
    EXPECT_EQ(w2.relogio.dia, 9);
    EXPECT_FLOAT_EQ(w2.relogio.minutos, 15 * 60);
    const auto& rel = w2.reg.get<Relacionamento>(w2.morador("tilda"));
    EXPECT_EQ(rel.afinidade, 25);
    EXPECT_TRUE(rel.conheceu);
    EXPECT_TRUE(rel.falouHoje);
    std::filesystem::remove_all(pasta);
}

TEST(SaveV2, MigraSaveDaVersao1) {
    const auto pasta = std::filesystem::temp_directory_path() / "koa_teste_save_v1";
    std::filesystem::remove_all(pasta);
    const auto dir = pasta / "Save_001";
    // Save exatamente como o 0.1 gravava
    ke::fs::gravarTextoSeguro(dir / "metadata.json", R"({"versao_formato": 1, "versao_jogo": "0.1.0", "data_hora": "", "tempo_jogado": 30})");
    ke::fs::gravarTextoSeguro(dir / "player.json", R"({"x": 344, "y": 318, "direcao": 0})");
    ke::fs::gravarTextoSeguro(dir / "inventory.json", R"({"madeira": 4, "pedra": 2, "ouro": 7})");
    ke::fs::gravarTextoSeguro(dir / "kingdom.json", R"({"construcoes": {"casa_herdeiro": 2}, "era_visual": 0})");
    ke::fs::gravarTextoSeguro(dir / "world.json", R"({"objetos": []})");
    SaveSystem s(pasta);
    std::string erro;
    const auto d = s.carregar("Save_001", &erro);
    ASSERT_TRUE(d) << erro;
    EXPECT_EQ(d->dia, 1);
    EXPECT_FLOAT_EQ(d->minutos, 360.0f);
    EXPECT_TRUE(d->moradores.empty());
    EXPECT_EQ(d->inventario.at("ouro"), 7);
    ASSERT_EQ(d->construcoes.size(), 1u);
    EXPECT_EQ(d->construcoes[0].id, "casa_herdeiro");
    EXPECT_EQ(d->construcoes[0].nivel, 2);
    EXPECT_FALSE(d->temPovo) << "save antigo fica com o povo inicial do mapa";
    std::filesystem::remove_all(pasta);
}
