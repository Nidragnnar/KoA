// Fase 9: Crônicas do Reino, títulos, fragmentos da Queda, mercador viajante e monumentos
#include <gtest/gtest.h>

#include <filesystem>

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
    EXPECT_TRUE(w.carregar(dados(), PASTA_DATA));
    return w;
}

void reconstruirCasa(World& w) {
    for (const char* r : {"madeira", "pedra", "ouro"}) w.inventario.definir(r, 100);
    ASSERT_TRUE(w.melhorarConstrucao("casa_herdeiro"));
}

int pistasNoChao(World& w) {
    int n = 0;
    for (auto e : w.reg.view<Pista>()) n += e != entt::null;
    return n;
}

bool temPagina(const World& w, const std::string& marco) {
    return std::ranges::any_of(w.cronicas, [&](const EntradaCronica& c) { return c.marco == marco; });
}

glm::ivec2 lugarLivre(const World& w, const std::string& def, glm::ivec2 perto) {
    for (int raio = 0; raio < 30; ++raio)
        for (int dy = -raio; dy <= raio; ++dy)
            for (int dx = -raio; dx <= raio; ++dx)
                if (std::max(std::abs(dx), std::abs(dy)) == raio && w.podeConstruir(def, perto + glm::ivec2(dx, dy))) return perto + glm::ivec2(dx, dy);
    return {-1, -1};
}

}

TEST(Cronicas, AChegadaEhAPrimeiraPagina) {
    World w = vilarejo();
    ASSERT_FALSE(w.cronicas.empty());
    EXPECT_EQ(w.cronicas.front().marco, "chegada");
    EXPECT_EQ(w.cronicas.front().dia, 1);
    EXPECT_EQ(w.titulo().id, "exilado") << "quem acabou de chegar ainda não conquistou nada";
}

TEST(Cronicas, CadaAcontecimentoUmaVezComDiaEEra) {
    World w = vilarejo();
    w.relogio.dia = 5;
    EXPECT_TRUE(w.marcar("visitou:floresta", "Floresta Amaldiçoada"));
    EXPECT_FALSE(w.marcar("visitou:floresta", "Floresta Amaldiçoada")) << "o mesmo acontecimento não entra duas vezes";
    ASSERT_TRUE(temPagina(w, "visitou:floresta"));
    EXPECT_EQ(w.cronicas.back().dia, 5);
    EXPECT_EQ(w.cronicas.back().era, 0);
    // Página do grupo ("missao:*") com o nome do acontecimento
    w.marcar("missao:pao_para_todos", "Pão para Todos");
    EXPECT_EQ(w.cronicas.back().texto, "Missão cumprida: Pão para Todos.");
    // Acontecimento sem página: fica só como marco
    const size_t antes = w.cronicas.size();
    EXPECT_TRUE(w.marcar("primeira_conversa"));
    EXPECT_EQ(w.cronicas.size(), antes);
    const auto ev = w.tirarEventos();
    EXPECT_TRUE(std::ranges::any_of(ev, [](const EventoJogo& e) { return e.tipo == EventoJogo::Tipo::Cronica; }));
}

TEST(Titulos, CasaReconstruidaFazOSenhorDasCinzas) {
    World w = vilarejo();
    w.tirarEventos();
    reconstruirCasa(w);
    EXPECT_EQ(w.titulo().id, "senhor_cinzas");
    EXPECT_TRUE(temPagina(w, "casa_reconstruida"));
    EXPECT_TRUE(temPagina(w, "titulo:senhor_cinzas"));
    const auto ev = w.tirarEventos();
    EXPECT_TRUE(std::ranges::any_of(ev, [](const EventoJogo& e) { return e.tipo == EventoJogo::Tipo::Titulo && e.texto == "Senhor das Cinzas"; }));
}

TEST(Titulos, ProtetorPedeEstradaLimpaEVinteCriaturas) {
    World w = vilarejo();
    w.abates["sombra"] = 19;
    w.marcar("missao:limpar_estrada", "Limpar a Estrada");
    EXPECT_FALSE(w.marcos.contains("titulo:protetor"));
    w.abates["lobo"] = 1;
    w.marcar("qualquer_coisa");  // os títulos são conferidos a cada acontecimento
    EXPECT_TRUE(w.marcos.contains("titulo:protetor"));
    EXPECT_EQ(w.titulo().nome, "Protetor dos Povos");
}

TEST(Titulos, MoradoresChamamPeloTitulo) {
    World w = vilarejo();
    reconstruirCasa(w);
    w.marcar("missao:limpar_estrada", "Limpar a Estrada");
    // Oswin: apresentação, depois a casa, depois a estrada limpa (fala com {titulo})
    w.conversar("oswin");
    w.conversar("oswin");
    const Conversa c = w.conversar("oswin");
    std::string tudo;
    for (const auto& f : c.falas) tudo += f;
    EXPECT_NE(tudo.find("Senhor das Cinzas"), std::string::npos) << tudo;
    EXPECT_EQ(tudo.find("{titulo}"), std::string::npos);
}

TEST(Pistas, AparecemNoChaoEAsLidasSomem) {
    World w = vilarejo();
    EXPECT_EQ(pistasNoChao(w), 2) << "ordem chamuscada e lápide; o poço só depois da era 1";
    w.marcar("era:1", "O Despertar da Coroa");
    EXPECT_EQ(pistasNoChao(w), 3);
    // Ler de verdade: herói logo abaixo do pergaminho, olhando para cima, aperta E
    const PistaDef& p = *dados().lore.pista("ordem_chamuscada");
    w.jogador.pos = {p.tile.x * 16 + 8.0f, (p.tile.y + 1) * 16 + 14.0f};
    w.jogador.dir = Direcao::Cima;
    w.passo(1.0f / 60.0f, {});
    ASSERT_EQ(w.alvo().tipo, TipoInteracao::Ler);
    w.passo(1.0f / 60.0f, {{0, 0}, true, true});
    EXPECT_TRUE(w.marcos.contains("pista:ordem_chamuscada"));
    EXPECT_EQ(w.pistasEncontradas(), 1);
    EXPECT_EQ(pistasNoChao(w), 2) << "o fragmento lido sai do chão";
    const auto ev = w.tirarEventos();
    EXPECT_TRUE(std::ranges::any_of(ev, [](const EventoJogo& e) { return e.tipo == EventoJogo::Tipo::LerPista && e.alvo == "ordem_chamuscada"; }));
}

TEST(Pistas, JuramentoSoDepoisDoCavaleiro) {
    World w = vilarejo();
    w.entrarRegiao("cripta", {13, 15}, Direcao::Cima);
    EXPECT_EQ(pistasNoChao(w), 0);
    w.marcar("chefe_cavaleiro_caido", "Cavaleiro Caído");
    EXPECT_EQ(pistasNoChao(w), 1);
}

TEST(Pistas, TodasJuntasRevelamOPrimeiroAto) {
    World w = vilarejo();
    for (const PistaDef& p : dados().lore.pistas) w.marcar("pista:" + p.id, p.titulo);
    EXPECT_TRUE(w.marcos.contains("misterio:primeiro_ato"));
    EXPECT_TRUE(temPagina(w, "misterio:primeiro_ato"));
    EXPECT_EQ(w.titulo().id, "guardiao_memoria");
}

TEST(Mercador, SoVemDepoisDaEstradaACadaTresDias) {
    World w = vilarejo();
    const NpcDef& corvin = dados().npcs.at("corvin");
    auto escondido = [&] { return w.reg.get<Agenda>(w.morador("corvin")).dormindo; };
    w.relogio.dia = 3;
    w.relogio.minutos = 10 * 60;
    w.posicionarMoradores();
    EXPECT_FALSE(w.visitanteHoje(corvin));
    EXPECT_TRUE(escondido()) << "com a estrada fechada, nenhum mercador";
    w.marcar("missao:limpar_estrada", "Limpar a Estrada");
    w.posicionarMoradores();
    EXPECT_TRUE(w.visitanteHoje(corvin));
    EXPECT_FALSE(escondido()) << "dia 3: está na banca";
    w.relogio.dia = 4;
    w.posicionarMoradores();
    EXPECT_TRUE(escondido()) << "dia 4: foi embora";
    EXPECT_EQ(w.reg.view<Aldeao>().size(), static_cast<size_t>(w.estado.populacao - 5)) << "o mercador não conta como morador";
}

TEST(Mercador, VendeEACartaSaiUmaVezSo) {
    World w = vilarejo();
    w.inventario.definir("ouro", 100);
    const int comida = w.inventario.quantidade("comida");
    EXPECT_TRUE(w.comprar("corvin", "comida"));
    EXPECT_EQ(w.inventario.quantidade("comida"), comida + 10);
    EXPECT_EQ(w.inventario.quantidade("ouro"), 85);
    EXPECT_TRUE(w.comprar("corvin", "carta"));
    EXPECT_TRUE(w.marcos.contains("pista:carta_selada"));
    EXPECT_EQ(w.inventario.quantidade("ouro"), 45);
    EXPECT_FALSE(w.comprar("corvin", "carta")) << "a carta é uma só";
    EXPECT_EQ(w.inventario.quantidade("ouro"), 45);
    w.inventario.definir("ouro", 10);
    EXPECT_FALSE(w.comprar("corvin", "pedra"));
    EXPECT_EQ(w.inventario.quantidade("ouro"), 10) << "sem ouro não leva nada";
}

TEST(Monumentos, PedemOAcontecimentoEContamAHistoria) {
    World w = vilarejo();
    for (const char* r : {"madeira", "pedra", "ouro"}) w.inventario.definir(r, 500);
    std::string motivo;
    EXPECT_FALSE(w.podeConstruir("memorial", {20, 24}, &motivo));
    EXPECT_EQ(motivo, "Reconstrua a sua casa primeiro.");
    reconstruirCasa(w);
    const glm::ivec2 t = lugarLivre(w, "memorial", {22, 24});
    ASSERT_NE(t.x, -1);
    ASSERT_FALSE(w.construir("memorial", t).empty());
    w.dormir(false);
    EXPECT_TRUE(temPagina(w, "construiu:memorial"));
    const BuildingData& b = *w.reino.construcao("memorial_1");
    auto texto = [&] {
        std::string s;
        for (const auto& l : w.inscricao(b)) s += l + "\n";
        return s;
    };
    EXPECT_NE(texto().find("Ninguém ainda sabe"), std::string::npos) << texto();
    w.marcar("pista:lapide_rainha", "A lápide sem corpo");
    EXPECT_NE(texto().find("A lápide sem corpo"), std::string::npos) << texto();
    EXPECT_FALSE(w.podeConstruir("estatua", lugarLivre(w, "torre", {40, 25}))) << "a estátua pede o Cavaleiro Caído";
}

TEST(SaveV5, CronicasEMarcosVoltamIguais) {
    World w = vilarejo();
    reconstruirCasa(w);
    w.marcar("pista:ordem_chamuscada", "Ordem real, chamuscada");
    const DadosSave s = w.capturar();
    World w2 = vilarejo();
    w2.aplicar(s);
    ASSERT_EQ(w2.cronicas.size(), w.cronicas.size());
    for (size_t i = 0; i < w.cronicas.size(); ++i) {
        EXPECT_EQ(w2.cronicas[i].marco, w.cronicas[i].marco);
        EXPECT_EQ(w2.cronicas[i].texto, w.cronicas[i].texto);
    }
    EXPECT_EQ(w2.titulo().id, "senhor_cinzas");
    EXPECT_EQ(w2.pistasEncontradas(), 1);
}

// Monta saves prontos para testar a fase 9 à mão (só roda com KOA_CENARIOS = pasta dos saves)
TEST(Cenarios, MontarSavesDeTeste) {
    char* pasta = nullptr;
    size_t n = 0;
    if (_dupenv_s(&pasta, &n, "KOA_CENARIOS") != 0 || !pasta) GTEST_SKIP() << "KOA_CENARIOS não definida";
    SaveSystem s(pasta);
    free(pasta);
    auto gravar = [&](World& w, const std::string& slot) {
        DadosSave d = w.capturar();
        d.versaoJogo = "1.1.0";
        d.dataHora = "cenário de teste";
        ASSERT_TRUE(s.salvar(d, slot));
    };
    auto obra = [](World& w, const std::string& def, glm::ivec2 perto) {
        const glm::ivec2 t = lugarLivre(w, def, perto);
        ASSERT_NE(t.x, -1) << def;
        ASSERT_FALSE(w.construir(def, t).empty()) << def;
    };
    // Save_002: estrada recém-limpa. Dormindo, amanhã é dia 3 e Corvin aparece; castelo e cemitério têm fragmentos
    {
        World w = vilarejo();
        reconstruirCasa(w);
        obra(w, "campo", {15, 24});
        obra(w, "taverna", {40, 24});
        w.dormir(false);
        w.abates["sombra"] = 5;
        w.missoes["limpar_estrada"] = {true, 0};
        w.visitadas.insert("floresta");
        w.marcar("visitou:floresta", "Floresta Amaldiçoada", true);
        w.marcar("missao:limpar_estrada", "Limpar a Estrada", true);
        w.inventario.definir("madeira", 60);
        w.inventario.definir("pedra", 60);
        w.inventario.definir("ouro", 150);
        w.inventario.definir("comida", 60);
        w.relogio.dia = 2;
        w.relogio.minutos = 20 * 60;
        w.tirarEventos();
        gravar(w, "Save_002");
    }
    // Save_003: depois do Cavaleiro Caído, já no Despertar. Estátua e Jardim liberados; fragmento novo na cripta e no poço
    {
        World w = vilarejo();
        reconstruirCasa(w);
        for (const char* r : {"madeira", "pedra", "ouro"}) w.inventario.definir(r, 600);
        const std::pair<const char*, glm::ivec2> obras[] = {{"campo", {15, 24}}, {"taverna", {40, 24}}, {"moradia", {12, 10}}, {"moradia", {45, 12}},
                                                             {"mercado", {44, 26}}, {"forja", {36, 12}}, {"capela", {50, 22}}};
        for (const auto& [def, perto] : obras) obra(w, def, perto);
        w.dormir(false);
        w.abates = {{"sombra", 12}, {"lobo", 6}, {"esqueleto", 6}, {"cavaleiro_caido", 1}};
        for (const MissaoDef& m : dados().missoes) w.missoes[m.id] = {true, 0};
        for (const char* r : {"floresta", "cripta"}) w.visitadas.insert(r);
        w.marcar("visitou:floresta", "Floresta Amaldiçoada", true);
        w.marcar("visitou:cripta", "Cripta do Cavaleiro", true);
        w.marcar("missao:limpar_estrada", "Limpar a Estrada", true);
        w.reino.magiaLiberta = true;
        w.marcar("chefe_cavaleiro_caido", "Cavaleiro Caído", true);
        w.reino.eraVisual = 1.0f;
        w.marcar("era:1", "O Despertar da Coroa", true);
        w.inventario.definir("madeira", 120);
        w.inventario.definir("pedra", 120);
        w.inventario.definir("ouro", 300);
        w.inventario.definir("comida", 80);
        w.relogio.dia = 8;
        w.relogio.minutos = 9 * 60;
        w.tirarEventos();
        EXPECT_EQ(static_cast<int>(w.reino.eraAlvo()), 1) << "desenvolvimento " << w.reino.desenvolvimento();
        gravar(w, "Save_003");
    }
}

TEST(SaveV5, SaveV4GanhaCronicasDoQueJaAconteceu) {
    const auto pasta = std::filesystem::temp_directory_path() / "koa_teste_save_v4_para_v5";
    std::filesystem::remove_all(pasta);
    World w = vilarejo();
    reconstruirCasa(w);
    w.visitadas.insert("floresta");
    SaveSystem s(pasta);
    ASSERT_TRUE(s.salvar(w.capturar()));
    // Vira um save v4: sem chronicles.json e sem os marcos novos
    std::filesystem::remove(pasta / "Save_001/chronicles.json");
    ke::fs::gravarTextoSeguro(pasta / "Save_001/quests.json", R"({"missoes": {}, "abates": {}, "visitadas": ["vilarejo", "floresta"], "marcos": []})");
    auto meta = *ke::fs::lerTexto(pasta / "Save_001/metadata.json");
    const std::string atual = "\"versao_formato\": " + std::to_string(VERSAO_SAVE);
    meta.replace(meta.find(atual), atual.size(), "\"versao_formato\": 4");
    ke::fs::gravarTextoSeguro(pasta / "Save_001/metadata.json", meta);
    std::string erro;
    const auto d = s.carregar("Save_001", &erro);
    ASSERT_TRUE(d) << erro;
    EXPECT_FALSE(d->temCronicas);
    World w2 = vilarejo();
    w2.aplicar(*d);
    EXPECT_TRUE(temPagina(w2, "chegada"));
    EXPECT_TRUE(temPagina(w2, "casa_reconstruida"));
    EXPECT_TRUE(temPagina(w2, "visitou:floresta"));
    for (const auto& c : w2.cronicas) EXPECT_EQ(c.dia, 0) << "páginas de antes das Crônicas ficam sem data: " << c.marco;
    EXPECT_EQ(w2.titulo().id, "senhor_cinzas");
    std::filesystem::remove_all(pasta);
}
