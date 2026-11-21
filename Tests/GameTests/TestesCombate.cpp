// Fase 6: regiões, combate, equipamento, missões, chefe e save v4
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

entt::entity inimigoMaisPerto(World& w, const std::string& tipo) {
    entt::entity melhor = entt::null;
    float d = 1e9f;
    for (auto [e, p, i] : w.reg.view<Posicao, Inimigo>().each())
        if (i.tipo == tipo && glm::distance(p.v, w.jogador.pos) < d) d = glm::distance(p.v, w.jogador.pos), melhor = e;
    return melhor;
}

// Encosta no inimigo (por baixo, olhando para cima) e golpeia até ele cair
bool derrotar(World& w, entt::entity alvo) {
    for (int golpes = 0; golpes < 80 && w.reg.valid(alvo); ++golpes) {
        w.jogador.pos = w.reg.get<Posicao>(alvo).v + glm::vec2(0, 12);
        w.jogador.dir = Direcao::Cima;
        w.jogador.vida = w.jogador.vidaMax;  // o teste é do golpe, não da defesa
        w.passo(1.0f / 60.0f, {{0, 0}, false, false, true});
        passos(w, 18);
    }
    return !w.reg.valid(alvo);
}

void irPara(World& w, const std::string& regiao) {
    const Saida* s = nullptr;
    for (const auto& x : w.infoAtual().saidas)
        if (x.destino == regiao) s = &x;
    ASSERT_NE(s, nullptr) << "não há saída para " << regiao;
    w.jogador.pos = {s->tile.x * 16 + 8.0f, s->tile.y * 16 + 12.0f};
    passos(w, 1);
    ASSERT_EQ(w.regiaoAtual, regiao);
}

}

TEST(Regioes, CarregaFlorestaECripta) {
    World w = vilarejo();
    ASSERT_TRUE(w.regioes.contains("floresta"));
    ASSERT_TRUE(w.regioes.contains("cripta"));
    ASSERT_TRUE(w.regioes.contains("minas"));  // fase 11
    EXPECT_GT(w.regioes.at("floresta").spawns.size(), 10u);
    EXPECT_EQ(dados().inimigos.size(), 14u);  // 1.16: + 6 (à distância e em bando); 1.17: + 2 chefes (Sacerdotisa e Varek)
    EXPECT_EQ(dados().equipamentos.size(), 11u);
    EXPECT_EQ(dados().missoes.size(), 16u);  // fase 19: + 6 das facções
    EXPECT_GT(w.regioes.at("floresta").escuridaoMin, 0.3f) << "a floresta é escura mesmo de dia";
}

TEST(Regioes, PisarNaSaidaTrocaDeRegiaoEVolta) {
    World w = vilarejo();
    irPara(w, "floresta");
    EXPECT_EQ(w.mapa.largura(), 56);
    EXPECT_FALSE(w.noVilarejo());
    EXPECT_TRUE(w.visitadas.contains("floresta"));
    EXPECT_FALSE(ke::colide(w.mapa, w.jogador.caixa(), w.obstaculos())) << "chegou num lugar livre";
    irPara(w, "vilarejo");
    EXPECT_EQ(w.mapa.largura(), 64);
    EXPECT_NE(w.reino.construcao("casa_herdeiro"), nullptr);
}

TEST(Combate, EspadaDerrotaSombraEDaOuro) {
    World w = vilarejo();
    irPara(w, "floresta");
    const int ouro = w.inventario.quantidade("ouro");
    ASSERT_TRUE(derrotar(w, inimigoMaisPerto(w, "sombra")));
    EXPECT_EQ(w.abates["sombra"], 1);
    EXPECT_GT(w.inventario.quantidade("ouro"), ouro);
}

TEST(Combate, InimigoFereComPausaEntreGolpes) {
    World w = vilarejo();
    irPara(w, "floresta");
    const entt::entity lobo = inimigoMaisPerto(w, "lobo");
    w.jogador.pos = w.reg.get<Posicao>(lobo).v + glm::vec2(4, 0);
    passos(w, 2);
    EXPECT_LT(w.jogador.vida, 100) << "o lobo morde quem encosta";
    const int depois = w.jogador.vida;
    passos(w, 20);
    EXPECT_EQ(w.jogador.vida, depois) << "um instante de invulnerabilidade depois do dano";
}

TEST(Combate, ArmaduraReduzODano) {
    EXPECT_EQ(danoContra(11, 0), 11);
    EXPECT_EQ(danoContra(11, 5), 6);
    EXPECT_EQ(danoContra(3, 8), 1) << "sempre pelo menos 1";
}

TEST(Combate, DerrotadoAcordaEmCasaNoDiaSeguinte) {
    World w = vilarejo();
    irPara(w, "floresta");
    w.jogador.vida = 1;
    const entt::entity lobo = inimigoMaisPerto(w, "lobo");
    w.jogador.pos = w.reg.get<Posicao>(lobo).v + glm::vec2(4, 0);
    passos(w, 3);
    const auto ev = w.tirarEventos();
    ASSERT_TRUE(std::ranges::any_of(ev, [](const EventoJogo& e) { return e.tipo == EventoJogo::Tipo::HeroiDerrotado; }));
    w.reviver();
    EXPECT_TRUE(w.noVilarejo());
    EXPECT_EQ(w.jogador.vida, w.jogador.vidaMax) << "acorda com a vida cheia (125: o fundador é Valente)";
    EXPECT_EQ(w.relogio.dia, 2);
}

TEST(Equipamento, ForjarPedeForjaERecursos) {
    World w = vilarejo();
    for (const char* r : {"madeira", "pedra", "ouro"}) w.inventario.definir(r, 500);
    EXPECT_FALSE(w.forjar("espada_ferro")) << "sem forja";
    w.reino.construcoes.push_back({"forja_1", "forja", "Oficina Simples", 1, {2, 2}, true, true});
    EXPECT_TRUE(w.forjar("espada_ferro"));
    EXPECT_EQ(w.equip.arma, "espada_ferro");
    EXPECT_EQ(w.danoArma(), 5);
    EXPECT_FALSE(w.forjar("espada_aco")) << "precisa da forja nível 2";
    EXPECT_FALSE(w.forjar("espada_ferro")) << "já tem";
    EXPECT_TRUE(w.forjar("gibao"));
    EXPECT_EQ(w.defesaArmadura(), 2);
}

TEST(Missoes, OswinPedeLimparAEstrada) {
    World w = vilarejo();
    const Conversa c = w.conversar("oswin");
    EXPECT_EQ(c.missao, "limpar_estrada");
    EXPECT_NE(c.falas.back().find("Limpar a Estrada"), std::string::npos);
    ASSERT_EQ(w.missoesAtivas().size(), 1u);
    irPara(w, "floresta");
    const int ouro = w.inventario.quantidade("ouro");
    for (int i = 0; i < 5; ++i) ASSERT_TRUE(derrotar(w, inimigoMaisPerto(w, "sombra"))) << "sombra " << i;
    EXPECT_TRUE(w.missoes.at("limpar_estrada").concluida);
    EXPECT_TRUE(w.missoesAtivas().empty());
    EXPECT_GE(w.inventario.quantidade("ouro"), ouro + 40) << "recompensa paga";
    const auto ev = w.tirarEventos();
    EXPECT_TRUE(std::ranges::any_of(ev, [](const EventoJogo& e) { return e.tipo == EventoJogo::Tipo::MissaoConcluida; }));
}

TEST(Missoes, ObjetivoJaCumpridoConcluiNaHora) {
    World w = vilarejo();
    w.reino.construcoes.push_back({"forja_1", "forja", "Oficina Simples", 1, {2, 2}, true, true});
    w.conversar("bram");
    EXPECT_TRUE(w.missoes.at("martelo_bram").concluida);
    EXPECT_TRUE(w.equip.possuidos.contains("espada_ferro")) << "Bram dá a espada de presente";
}

TEST(Missoes, CavaleiroSoDepoisDaFloresta) {
    World w = vilarejo();
    EXPECT_EQ(w.conversar("maelis").missao, "floresta_sussurra");
    EXPECT_TRUE(w.conversar("maelis").missao.empty()) << "a do cavaleiro exige ter achado a cripta";
    irPara(w, "floresta");
    irPara(w, "cripta");
    EXPECT_TRUE(w.missoes.at("floresta_sussurra").concluida);
    irPara(w, "floresta");
    irPara(w, "vilarejo");
    EXPECT_EQ(w.conversar("maelis").missao, "cavaleiro_caido");
}

TEST(Chefe, CavaleiroCaidoNaoRenasce) {
    World w = vilarejo();
    irPara(w, "floresta");
    irPara(w, "cripta");
    w.equip.possuidos.insert("espada_linhagem");
    w.equip.arma = "espada_linhagem";
    const entt::entity chefe = inimigoMaisPerto(w, "cavaleiro_caido");
    ASSERT_TRUE(chefe != entt::null);
    ASSERT_TRUE(derrotar(w, chefe));
    EXPECT_TRUE(w.marcos.contains("chefe_cavaleiro_caido"));
    w.renascerInimigos();
    EXPECT_TRUE(inimigoMaisPerto(w, "cavaleiro_caido") == entt::null);
    EXPECT_TRUE(inimigoMaisPerto(w, "esqueleto") != entt::null) << "os comuns voltam";
}

TEST(SaveV4, GuardaRegiaoEquipamentoEMissoes) {
    const auto pasta = std::filesystem::temp_directory_path() / "koa_teste_save_v4";
    std::filesystem::remove_all(pasta);
    World w = vilarejo();
    w.conversar("oswin");
    irPara(w, "floresta");
    ASSERT_TRUE(derrotar(w, inimigoMaisPerto(w, "sombra")));
    w.equip.possuidos.insert("gibao");
    w.equip.armadura = "gibao";
    w.jogador.vida = 61;
    // uma árvore cortada na floresta precisa voltar cortada
    entt::entity arvore = entt::null;
    for (auto [e, c] : w.reg.view<Coletavel>().each())
        if (c.tipo == "arvore") arvore = e;
    w.reg.get<Coletavel>(arvore).esgotado = true;
    const std::string idArvore = w.reg.get<IdMundo>(arvore).id;
    EXPECT_TRUE(idArvore.starts_with("floresta:")) << "ids de outras regiões levam prefixo";
    const glm::vec2 pos = w.jogador.pos;
    SaveSystem s(pasta);
    ASSERT_TRUE(s.salvar(w.capturar()));
    EXPECT_TRUE(std::filesystem::exists(pasta / "Save_001/quests.json"));

    World w2 = vilarejo();
    const auto d = s.carregar();
    ASSERT_TRUE(d);
    w2.aplicar(*d);
    EXPECT_EQ(w2.regiaoAtual, "floresta");
    EXPECT_LT(glm::distance(w2.jogador.pos, pos), 1.0f);
    EXPECT_EQ(w2.jogador.vida, 61);
    EXPECT_EQ(w2.equip.armadura, "gibao");
    EXPECT_EQ(w2.abates["sombra"], 1);
    ASSERT_TRUE(w2.missoes.contains("limpar_estrada"));
    EXPECT_EQ(w2.progressoMissao(dados().missoes[0]), 1);
    bool achou = false;
    for (auto [e, c, id] : w2.reg.view<Coletavel, IdMundo>().each())
        if (id.id == idArvore) achou = c.esgotado;
    EXPECT_TRUE(achou);
    std::filesystem::remove_all(pasta);
}

TEST(SaveV4, MigraSaveV3SemMissoes) {
    const auto pasta = std::filesystem::temp_directory_path() / "koa_teste_save_v3_para_v4";
    std::filesystem::remove_all(pasta);
    World w = vilarejo();
    SaveSystem s(pasta);
    ASSERT_TRUE(s.salvar(w.capturar()));
    // Vira um save v3: sem quests.json e sem os campos novos do player
    std::filesystem::remove(pasta / "Save_001/quests.json");
    std::filesystem::remove(pasta / "Save_001/chronicles.json");
    ke::fs::gravarTextoSeguro(pasta / "Save_001/player.json", R"({"x": 344, "y": 318, "direcao": 0})");
    auto meta = *ke::fs::lerTexto(pasta / "Save_001/metadata.json");
    const std::string atual = "\"versao_formato\": " + std::to_string(VERSAO_SAVE);
    meta.replace(meta.find(atual), atual.size(), "\"versao_formato\": 3");
    ke::fs::gravarTextoSeguro(pasta / "Save_001/metadata.json", meta);
    std::string erro;
    const auto d = s.carregar("Save_001", &erro);
    ASSERT_TRUE(d) << erro;
    EXPECT_EQ(d->regiao, "vilarejo");
    EXPECT_EQ(d->vida, 100);
    EXPECT_EQ(d->arma, "espada_enferrujada");
    EXPECT_TRUE(d->missoes.empty());
    std::filesystem::remove_all(pasta);
}
