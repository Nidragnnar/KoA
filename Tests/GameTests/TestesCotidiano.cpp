// Fase 15: vida cotidiana (horta, pesca, cozinha, animais, presentes) e o save v10
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
    w.inventario.definir("comida", 300);  // o povo não passa fome durante os testes
    return w;
}

bool temNota(const World& w, const std::string& trecho) {
    return std::ranges::any_of(w.ultimoRelatorio.notas, [&](const std::string& n) { return n.find(trecho) != std::string::npos; });
}

bool contem(const std::string& t, const std::string& trecho) { return t.find(trecho) != std::string::npos; }

void quadro(World& w, bool e = false) {
    EntradaMundo in;
    in.interagir = e;
    w.passo(1.0f / 60.0f, in);
}

// Põe o herdeiro de frente para a água, sem nada por perto (o E vira "Pescar")
bool beiraDaAgua(World& w) {
    for (int y = 1; y + 1 < w.mapa.altura(); ++y)
        for (int x = 1; x + 1 < w.mapa.largura(); ++x) {
            if (w.mapa.em(x, y).terreno != T_AGUA || w.mapa.em(x, y + 1).terreno == T_AGUA || w.mapa.em(x, y + 1).solido) continue;
            w.jogador.pos = {x * 16.0f + 8.0f, (y + 1) * 16.0f + 6.0f};
            w.jogador.dir = Direcao::Cima;
            quadro(w);
            if (w.alvo().tipo == TipoInteracao::Pescar) return true;
        }
    return false;
}

int peixes(const World& w) {
    return w.inventario.quantidade("peixe") + w.inventario.quantidade("truta") + w.inventario.quantidade("peixe_dourado");
}

// Espera a mordida (no máximo 6 segundos de jogo)
bool esperarMordida(World& w) {
    for (int i = 0; i < 360 && w.pescando(); ++i) {
        if (w.peixeMordendo()) return true;
        quadro(w);
    }
    return false;
}

int animais(World& w, const std::string& tipo) {
    int n = 0;
    for (auto [e, a] : w.reg.view<const Animal>().each()) n += a.tipo == tipo;
    return n;
}

}

TEST(Cotidiano, DadosDaVidaNoCampo) {
    const DadosCotidiano& d = dados().cotidiano;
    EXPECT_EQ(d.canteiros.size(), 4u);
    EXPECT_EQ(d.culturas.size(), 3u);
    EXPECT_EQ(d.peixes.size(), 3u);
    EXPECT_EQ(d.receitas.size(), 7u);
    EXPECT_EQ(d.animais.at("galinheiro").produto, "ovo");
    EXPECT_EQ(d.animais.at("curral").produto, "leite");
    EXPECT_EQ(dados().nomeRecurso("semente_nabo"), "Semente de Nabo");
    // Sorteio: o dourado é raro e só a truta é da floresta
    EXPECT_EQ(sortearPeixe(d, "vilarejo", Estacao::Primavera, {0.05f})->id, "peixe_dourado");
    EXPECT_EQ(sortearPeixe(d, "floresta", Estacao::Primavera, {0.5f, 0.3f})->id, "truta");
    EXPECT_EQ(sortearPeixe(d, "vilarejo", Estacao::Primavera, {0.5f, 0.3f})->id, "peixe");
    EXPECT_TRUE(contem(textoPrato(d.receita("ensopado")->efeitos), "+3 de dano até dormir"));
    // O Corvin vende as sementes
    int sementes = 0;
    for (const auto& it : dados().npcs.at("corvin").loja) sementes += it.ganho.contains("semente_nabo") || it.ganho.contains("semente_trigo") || it.ganho.contains("semente_abobora");
    EXPECT_EQ(sementes, 3);
}

TEST(Horta, PlantarRegarCrescerEColher) {
    World w = vilarejo();
    ASSERT_EQ(w.canteiros.size(), 4u);
    EXPECT_EQ(w.inventario.quantidade("semente_nabo"), 4) << "o jogo começa com sementes de nabo";
    EXPECT_TRUE(contem(w.rotuloCanteiro(0), "Plantar Nabo"));
    ASSERT_TRUE(w.usarCanteiro(0));
    EXPECT_EQ(w.canteiros[0].cultura, "nabo");
    EXPECT_EQ(w.inventario.quantidade("semente_nabo"), 3);
    EXPECT_TRUE(contem(w.rotuloCanteiro(0), "Regar Nabo"));
    ASSERT_TRUE(w.usarCanteiro(0));
    EXPECT_FALSE(w.usarCanteiro(0)) << "uma rega por dia";
    w.dormir(false);
    EXPECT_EQ(w.canteiros[0].crescidos, 1);
    EXPECT_FALSE(w.canteiros[0].maduro);
    w.dormir(false);  // sem rega ontem: não cresce
    EXPECT_EQ(w.canteiros[0].crescidos, 1);
    EXPECT_TRUE(temNota(w, "sem rega"));
    ASSERT_TRUE(w.usarCanteiro(0));
    w.dormir(false);
    EXPECT_TRUE(w.canteiros[0].maduro);
    EXPECT_TRUE(temNota(w, "Nabo pronto para colher"));
    EXPECT_TRUE(contem(w.rotuloCanteiro(0), "Colher Nabo"));
    ASSERT_TRUE(w.usarCanteiro(0));
    EXPECT_EQ(w.inventario.quantidade("nabo"), 2);
    EXPECT_TRUE(w.canteiros[0].cultura.empty()) << "colhido, o canteiro volta a ficar vazio";
    EXPECT_TRUE(w.marcos.contains("primeira_colheita"));
}

TEST(Horta, CadaCulturaNaSuaEstacao) {
    World w = vilarejo();
    w.relogio.dia = 22;  // inverno
    EXPECT_TRUE(contem(w.rotuloCanteiro(0), "Inverno"));
    EXPECT_FALSE(w.usarCanteiro(0));
    // Plantado no outono: o primeiro dia cresce; com a geada do inverno, para
    w.relogio.dia = 21;
    w.inventario.definir("semente_abobora", 0);
    ASSERT_TRUE(w.usarCanteiro(1));
    ASSERT_TRUE(w.usarCanteiro(1));
    w.dormir(false);
    EXPECT_EQ(w.canteiros[1].crescidos, 1);
    ASSERT_TRUE(w.usarCanteiro(1));
    w.dormir(false);
    EXPECT_EQ(w.canteiros[1].crescidos, 1);
    EXPECT_TRUE(temNota(w, "frio segurou"));
    // A semente da estação: trigo não cresce no outono, abóbora sim
    w.relogio.dia = 15;  // outono
    w.inventario.definir("semente_nabo", 0);
    w.inventario.definir("semente_trigo", 9);
    w.inventario.definir("semente_abobora", 1);
    ASSERT_NE(w.sementeDaEstacao(), nullptr);
    EXPECT_EQ(w.sementeDaEstacao()->id, "abobora");
}

TEST(Horta, NaoDaParaConstruirEmCimaDaHorta) {
    World w = vilarejo();
    w.inventario.definir("madeira", 500);
    w.inventario.definir("pedra", 500);
    w.inventario.definir("ouro", 500);
    std::string motivo;
    const glm::ivec2 t = dados().cotidiano.canteiros[0];
    EXPECT_FALSE(w.podeConstruir("moradia", t, &motivo));
    EXPECT_TRUE(contem(motivo, "horta"));
}

TEST(Pesca, LancarEsperarAMordidaEPuxar) {
    World w = vilarejo();
    ASSERT_TRUE(beiraDaAgua(w)) << "o vilarejo precisa de água para pescar";
    // Puxar cedo demais assusta o peixe
    quadro(w, true);
    ASSERT_TRUE(w.pescando());
    quadro(w, true);
    EXPECT_FALSE(w.pescando());
    for (int i = 0; i < 30; ++i) quadro(w);
    EXPECT_FALSE(w.pescando()) << "o E que assustou o peixe não lança a linha de novo";
    // Agora com calma: espera a mordida e puxa
    quadro(w, true);
    ASSERT_TRUE(w.pescando());
    ASSERT_TRUE(esperarMordida(w));
    quadro(w, true);
    EXPECT_FALSE(w.pescando());
    EXPECT_EQ(peixes(w), 1);
    // Mordeu e ninguém puxou: escapa
    for (int i = 0; i < 30; ++i) quadro(w);
    quadro(w, true);
    ASSERT_TRUE(esperarMordida(w));
    for (int i = 0; i < 60; ++i) quadro(w);
    EXPECT_FALSE(w.pescando());
    EXPECT_EQ(peixes(w), 1);
}

TEST(Pesca, AndarRecolheALinha) {
    World w = vilarejo();
    ASSERT_TRUE(beiraDaAgua(w));
    quadro(w, true);
    ASSERT_TRUE(w.pescando());
    EntradaMundo in;
    in.mover = {0, 1};
    w.passo(1.0f / 60.0f, in);
    EXPECT_FALSE(w.pescando());
}

TEST(Cozinha, CozinharComerEOsEfeitosAteDormir) {
    World w = vilarejo();
    w.inventario.definir("nabo", 2);
    EXPECT_TRUE(w.cozinhar("sopa_nabo"));
    EXPECT_EQ(w.inventario.quantidade("nabo"), 0);
    EXPECT_EQ(w.inventario.quantidade("sopa_nabo"), 1);
    EXPECT_FALSE(w.cozinhar("sopa_nabo")) << "sem ingredientes";
    w.jogador.vida = 10;
    EXPECT_TRUE(w.comerPrato("sopa_nabo"));
    EXPECT_EQ(w.jogador.vida, 50);
    EXPECT_FALSE(w.comerPrato("sopa_nabo"));
    // Ensopado e omelete: força e defesa até dormir
    const int dano = w.danoArma(), defesa = w.defesaArmadura();
    w.inventario.definir("ensopado", 1);
    w.inventario.definir("omelete", 1);
    EXPECT_TRUE(w.comerPrato("ensopado"));
    EXPECT_TRUE(w.comerPrato("omelete"));
    EXPECT_EQ(w.danoArma(), dano + 3);
    EXPECT_EQ(w.defesaArmadura(), defesa + 2);
    w.dormir(false);
    EXPECT_EQ(w.danoArma(), dano);
    EXPECT_EQ(w.defesaArmadura(), defesa);
    // O banquete é servido ao povo
    w.inventario.definir("banquete", 1);
    const float feliz = w.estado.felicidade;
    EXPECT_TRUE(w.comerPrato("banquete"));
    EXPECT_NEAR(w.estado.felicidade, std::min(100.0f, feliz + 6.0f), 0.01f);
}

TEST(Cozinha, OHComeOPratoQueMaisCura) {
    World w = vilarejo();
    w.inventario.definir("sopa_nabo", 1);
    w.inventario.definir("pao", 1);
    w.jogador.vida = 20;
    EntradaMundo in;
    in.comer = true;
    w.passo(1.0f / 60.0f, in);
    EXPECT_EQ(w.inventario.quantidade("pao"), 0) << "o pão cura 50, a sopa 40";
    EXPECT_EQ(w.inventario.quantidade("sopa_nabo"), 1);
    EXPECT_EQ(w.jogador.vida, 70);
    EXPECT_TRUE(w.temPrato());
}

TEST(Cozinha, AFogueiraDoVilarejoEhOFogao) {
    World w = vilarejo();
    int fogoes = 0;
    for (auto [e, i] : w.reg.view<const Interativo>().each()) fogoes += i.acao == "cozinha";
    EXPECT_EQ(fogoes, 1);
}

TEST(Animais, GalinheiroECurralTrazemBichos) {
    World w = vilarejo();
    w.reino.construcoes.push_back({"galinheiro_t", "galinheiro", "Galinheiro", 1, {30, 22}, true, true});
    w.sincronizarAnimais();
    EXPECT_EQ(animais(w, "galinha"), 3);
    w.reino.construcoes.back().level = 2;
    w.sincronizarAnimais();
    EXPECT_EQ(animais(w, "galinha"), 5);
    w.reino.construcoes.push_back({"curral_t", "curral", "Curral", 1, {40, 22}, true, true});
    w.sincronizarAnimais();
    EXPECT_EQ(animais(w, "vaca"), 2);
    // Passeiam sem sumir
    for (int i = 0; i < 600; ++i) quadro(w);
    EXPECT_EQ(animais(w, "galinha"), 5);
}

TEST(Animais, ComemDaoOvosELeiteEGostamDeCarinho) {
    World w = vilarejo();
    w.reino.construcoes.push_back({"galinheiro_t", "galinheiro", "Galinheiro", 1, {30, 22}, true, true});
    w.reino.construcoes.push_back({"curral_t", "curral", "Curral", 1, {40, 22}, true, true});
    w.sincronizarAnimais();
    w.dormir(false);
    EXPECT_EQ(w.inventario.quantidade("ovo"), 3);
    EXPECT_EQ(w.inventario.quantidade("leite"), 2);
    // Carinho numa galinha: um ovo a mais amanhã (e só um carinho por dia)
    entt::entity galinha = entt::null;
    for (auto [e, a] : w.reg.view<const Animal>().each())
        if (a.tipo == "galinha") galinha = e;
    ASSERT_TRUE(galinha != entt::null);
    EXPECT_TRUE(w.fazerCarinho(galinha));
    EXPECT_FALSE(w.fazerCarinho(galinha));
    w.dormir(false);
    EXPECT_EQ(w.inventario.quantidade("ovo"), 3 + 4);
    // Sem comida, ninguém produz
    w.inventario.definir("comida", 0);
    w.dormir(false);
    EXPECT_EQ(w.inventario.quantidade("ovo"), 7);
    EXPECT_TRUE(temNota(w, "Faltou comida para os animais"));
}

TEST(Presentes, OFavoritoValeMaisEUmPorDia) {
    World w = vilarejo();
    EXPECT_TRUE(w.presenteParaDar("bram").empty()) << "sem nada para dar";
    w.inventario.definir("ensopado", 2);
    EXPECT_EQ(w.presenteParaDar("bram"), "ensopado");
    const int antes = w.coracoesDe("bram");
    EXPECT_TRUE(w.darPresente("bram"));
    EXPECT_EQ(w.coracoesDe("bram"), antes + 1) << "o favorito vale 100 de amizade";
    EXPECT_EQ(w.inventario.quantidade("ensopado"), 1);
    EXPECT_TRUE(w.presenteParaDar("bram").empty()) << "um presente por dia";
    EXPECT_FALSE(w.darPresente("bram"));
    EXPECT_TRUE(w.marcos.contains("presente:bram"));
    // Qualquer prato também agrada outro morador
    w.inventario.definir("ensopado", 0);
    w.inventario.definir("omelete", 1);
    EXPECT_EQ(w.presenteParaDar("edda"), "omelete");
    w.dormir(false);
    w.inventario.definir("ensopado", 1);
    EXPECT_EQ(w.presenteParaDar("bram"), "ensopado") << "no dia seguinte pode de novo";
}

TEST(Presentes, AConversaOfereceOPresente) {
    World w = vilarejo();
    w.inventario.definir("peixe_dourado", 1);
    const Conversa c = w.conversar("corvin");
    EXPECT_EQ(c.presente, "peixe_dourado");
}

TEST(Missoes, MaosNaTerraDaTilda) {
    World w = vilarejo();
    w.conversar("tilda");
    w.encerrarConversa("tilda");
    w.conversar("tilda");  // a segunda missão dela
    w.encerrarConversa("tilda");
    ASSERT_TRUE(w.missoes.contains("maos_na_terra"));
    ASSERT_TRUE(w.usarCanteiro(2));
    for (int d = 0; d < 2; ++d) {
        ASSERT_TRUE(w.usarCanteiro(2));
        w.dormir(false);
    }
    ASSERT_TRUE(w.canteiros[2].maduro);
    ASSERT_TRUE(w.usarCanteiro(2));
    EXPECT_TRUE(w.missoes.at("maos_na_terra").concluida);
    EXPECT_GE(w.inventario.quantidade("semente_trigo"), 5) << "a recompensa são sementes de trigo";
}

TEST(SaveV10, GuardaHortaPratosEPresentes) {
    const auto pasta = std::filesystem::temp_directory_path() / "koa_teste_save_v10";
    std::filesystem::remove_all(pasta);
    World w = vilarejo();
    ASSERT_TRUE(w.usarCanteiro(0));
    ASSERT_TRUE(w.usarCanteiro(0));
    w.inventario.definir("ensopado", 2);
    ASSERT_TRUE(w.comerPrato("ensopado"));
    ASSERT_TRUE(w.darPresente("bram"));
    w.carinhos["galinheiro_t#0"] = w.relogio.dia;
    SaveSystem s(pasta);
    ASSERT_TRUE(s.salvar(w.capturar()));
    EXPECT_TRUE(std::filesystem::exists(pasta / "Save_001/life.json"));
    std::string erro;
    const auto d = s.carregar("Save_001", &erro);
    ASSERT_TRUE(d) << erro;
    World w2 = vilarejo();
    w2.aplicar(*d);
    ASSERT_EQ(w2.canteiros.size(), 4u);
    EXPECT_EQ(w2.canteiros[0].cultura, "nabo");
    EXPECT_EQ(w2.canteiros[0].regadoDia, w.relogio.dia);
    EXPECT_EQ(w2.buffDano, 3);
    EXPECT_EQ(w2.presenteDia.at("bram"), w.relogio.dia);
    EXPECT_EQ(w2.carinhos.at("galinheiro_t#0"), w.relogio.dia);
    EXPECT_EQ(w2.inventario.quantidade("semente_nabo"), 3) << "save novo: nada de sementes extras";
    std::filesystem::remove_all(pasta);
}

TEST(SaveV10, SaveV9GanhaSementesParaComecar) {
    const auto pasta = std::filesystem::temp_directory_path() / "koa_teste_save_v9_para_v10";
    std::filesystem::remove_all(pasta);
    World w = vilarejo();
    w.inventario.definir("semente_nabo", 0);  // um save antigo não tinha sementes
    SaveSystem s(pasta);
    ASSERT_TRUE(s.salvar(w.capturar()));
    std::filesystem::remove(pasta / "Save_001/life.json");
    auto meta = *ke::fs::lerTexto(pasta / "Save_001/metadata.json");
    const std::string atual = "\"versao_formato\": " + std::to_string(VERSAO_SAVE);
    meta.replace(meta.find(atual), atual.size(), "\"versao_formato\": 9");
    ke::fs::gravarTextoSeguro(pasta / "Save_001/metadata.json", meta);
    std::string erro;
    const auto d = s.carregar("Save_001", &erro);
    ASSERT_TRUE(d) << erro;
    EXPECT_FALSE(d->temCotidiano);
    World w2 = vilarejo();
    w2.aplicar(*d);
    EXPECT_EQ(w2.inventario.quantidade("semente_nabo"), 4);
    ASSERT_EQ(w2.canteiros.size(), 4u);
    EXPECT_TRUE(w2.canteiros[0].cultura.empty());
    std::filesystem::remove_all(pasta);
}
