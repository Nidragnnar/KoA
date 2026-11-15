// Fase 11: regiões vivas (corrupção, segurança, prosperidade), a floresta purificada, as minas e o golem com escolha
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

int contar(World& w, const std::string& tipo) {
    int n = 0;
    for (auto [e, i] : w.reg.view<Inimigo>().each()) n += i.tipo == tipo;
    return n;
}

int inimigos(World& w) {
    int n = 0;
    for (auto [e, i] : w.reg.view<Inimigo>().each()) ++n;
    return n;
}

// Tira as criaturas da região ativa (menos as do tipo pedido): o teste é de outra coisa
void espantar(World& w, const std::string& menos = "") {
    std::vector<entt::entity> v;
    for (auto [e, i] : w.reg.view<Inimigo>().each())
        if (i.tipo != menos) v.push_back(e);
    w.reg.destroy(v.begin(), v.end());
}

// Golpe de espada com o herói embaixo do inimigo, olhando para cima
void golpear(World& w, entt::entity alvo) {
    w.jogador.pos = w.reg.get<Posicao>(alvo).v + glm::vec2(0, 16);
    w.jogador.dir = Direcao::Cima;
    w.jogador.acao = 0.0f;
    w.jogador.vida = w.jogador.vidaMax;
    w.passo(1.0f / 60.0f, {{0, 0}, false, false, true});
}

entt::entity primeiro(World& w, const std::string& tipo) {
    for (auto [e, i] : w.reg.view<Inimigo>().each())
        if (i.tipo == tipo) return e;
    return entt::null;
}

// E na frente de um tile (o herói fica embaixo, olhando para cima)
void usarTile(World& w, glm::ivec2 t) {
    w.jogador.pos = {t.x * 16 + 8.0f, (t.y + 1) * 16 + 12.0f};
    w.jogador.dir = Direcao::Cima;
    w.jogador.acao = 0.0f;
    w.passo(1.0f / 60.0f, {{0, 0}, true});
}

bool temNota(const World& w, const std::string& trecho) {
    return std::ranges::any_of(w.ultimoRelatorio.notas, [&](const std::string& n) { return n.find(trecho) != std::string::npos; });
}

bool temPagina(const World& w, const std::string& marco) {
    return std::ranges::any_of(w.cronicas, [&](const EntradaCronica& c) { return c.marco == marco; });
}

bool avisou(const std::vector<EventoJogo>& ev, const std::string& trecho) {
    return std::ranges::any_of(ev, [&](const EventoJogo& e) { return e.tipo == EventoJogo::Tipo::Aviso && e.texto.find(trecho) != std::string::npos; });
}

void irFloresta(World& w) { w.entrarRegiao("floresta", {3, 17}, Direcao::Direita); }

void irMinas(World& w) {
    w.marcar("minas_abertas");
    w.entrarRegiao("minas", {25, 2}, Direcao::Baixo);
}

// O golem até se render (180 de vida; rende-se com 30%)
entt::entity renderGolem(World& w) {
    irMinas(w);
    espantar(w, "golem_cinzas");
    const entt::entity g = primeiro(w, "golem_cinzas");
    EXPECT_TRUE(g != entt::null);
    if (g == entt::null) return g;
    w.reg.get<Inimigo>(g).vida = 55;
    golpear(w, g);
    return g;
}

}

TEST(RegioesVivas, DadosDaFase11) {
    World w = vilarejo();
    for (const char* id : {"floresta", "minas"}) {
        ASSERT_TRUE(w.estadoRegioes.contains(id)) << id;
        EXPECT_TRUE(w.regioes.at(id).vida.ativa);
    }
    EXPECT_FALSE(w.estadoRegioes.contains("cripta")) << "a cripta é um calabouço, sem estado próprio";
    EXPECT_FLOAT_EQ(w.estadoRegioes.at("floresta").corrupcao, 70.0f);
    EXPECT_FLOAT_EQ(w.estadoRegioes.at("minas").corrupcao, 80.0f);
    EXPECT_EQ(w.regioes.at("minas").vida.transformacoes.size(), 3u);
    const EscolhaDef& g = dados().escolhas.at("golem_cinzas");
    EXPECT_EQ(g.opcoes.size(), 3u);
    EXPECT_EQ(g.opcao("purificar")->requer, "purificou:floresta");
    EXPECT_EQ(dados().inimigos.at("golem_cinzas").escolha, "golem_cinzas");
    EXPECT_TRUE(dados().recursos.contains("cristal"));
    int golens = 0;
    for (const auto& [tipo, t] : w.regioes.at("minas").spawns) golens += tipo == "golem_cinzas";
    EXPECT_EQ(golens, 1);
}

TEST(RegioesVivas, CriaturaVencidaBaixaACorrupcaoESobeASeguranca) {
    World w = vilarejo();
    irFloresta(w);
    const float antes = w.estadoRegioes.at("floresta").corrupcao;
    espantar(w, "sombra");
    const entt::entity s = primeiro(w, "sombra");
    ASSERT_TRUE(s != entt::null);
    w.reg.get<Inimigo>(s).vida = 1;
    golpear(w, s);
    EXPECT_FALSE(w.reg.valid(s));
    EXPECT_FLOAT_EQ(w.estadoRegioes.at("floresta").corrupcao, antes - regra::CORRUPCAO_POR_ABATE);
    EXPECT_FLOAT_EQ(w.estadoRegioes.at("floresta").seguranca, regra::SEGURANCA_POR_ABATE);
}

TEST(RegioesVivas, MenosCorrupcaoMenosCriaturasDeManha) {
    World w = vilarejo();
    const int todas = static_cast<int>(w.regioes.at("floresta").spawns.size());
    irFloresta(w);
    w.renascerInimigos();
    EXPECT_EQ(inimigos(w), todas) << "com 70 de corrupção, a floresta fica cheia como antes";
    w.estadoRegioes.at("floresta").corrupcao = 5.0f;
    w.renascerInimigos();
    const int poucas = inimigos(w);
    EXPECT_LT(poucas, todas * 6 / 10);
    EXPECT_GT(poucas, 0) << "nunca fica vazia";
    EXPECT_FLOAT_EQ(fracaoCriaturas(100.0f), 1.0f);
    EXPECT_NEAR(fracaoCriaturas(0.0f), 0.35f, 0.001f);
}

TEST(RegioesVivas, AbandonadaPioraAteSerTomadaPelasCriaturas) {
    World w = vilarejo();
    irFloresta(w);
    w.entrarRegiao("vilarejo", {61, 19}, Direcao::Esquerda);
    const int todas = static_cast<int>(w.regioes.at("floresta").spawns.size());
    float anterior = w.estadoRegioes.at("floresta").corrupcao;
    int noites = 0;
    while (w.estadoRegioes.at("floresta").corrupcao < regra::INVASAO && noites < 30) {
        w.dormir(false);
        ++noites;
        const float agora = w.estadoRegioes.at("floresta").corrupcao;
        EXPECT_GT(agora, anterior) << "noite " << noites << ": sem ninguém lá, a corrupção só cresce";
        anterior = agora;
    }
    EXPECT_LT(noites, 12) << "abandonada, a floresta é tomada em uns dias";
    EXPECT_TRUE(temNota(w, "as criaturas estão tomando conta"));
    EXPECT_TRUE(w.marcos.contains("tomada:floresta"));
    EXPECT_TRUE(temPagina(w, "tomada:floresta"));
    irFloresta(w);
    EXPECT_EQ(inimigos(w), todas + static_cast<int>(w.regioes.at("floresta").vida.invasao.size())) << "chegam criaturas a mais";
    EXPECT_FALSE(w.estadoRegioes.contains("minas") && w.estadoRegioes.at("minas").corrupcao != 80.0f)
        << "terra nunca visitada fica como estava";
}

TEST(RegioesVivas, ProsperidadeRendeTodaNoite) {
    World w = vilarejo();
    irFloresta(w);
    w.estadoRegioes.at("floresta").prosperidade = 60.0f;
    w.entrarRegiao("vilarejo", {61, 19}, Direcao::Esquerda);
    const int madeira = w.inventario.quantidade("madeira");
    w.dormir(false);
    EXPECT_EQ(w.inventario.quantidade("madeira"), madeira + 5);  // 60 × 0,08
    const auto& v = w.ultimoRelatorio.variacoes;
    EXPECT_TRUE(std::ranges::any_of(v, [](const auto& p) { return p.first == "madeira|Floresta Amaldiçoada" && p.second == 5; }));
}

TEST(RegioesVivas, CoracaoDaFlorestaSoAceitaORitoComPoucaCorrupcao) {
    World w = vilarejo();
    irFloresta(w);
    espantar(w);
    usarTile(w, {27, 10});
    EXPECT_TRUE(avisou(w.tirarEventos(), "Raízes de cinza"));
    EXPECT_FALSE(w.marcos.contains("purificou:floresta"));
    w.estadoRegioes.at("floresta").corrupcao = 20.0f;
    w.renascerInimigos();
    ASSERT_GT(contar(w, "sombra"), 0);
    usarTile(w, {27, 10});
    const auto ev = w.tirarEventos();
    EXPECT_TRUE(w.marcos.contains("purificou:floresta"));
    EXPECT_TRUE(std::ranges::any_of(ev, [](const EventoJogo& e) { return e.tipo == EventoJogo::Tipo::Transformou && e.texto == "Floresta Encantada"; }));
    EXPECT_EQ(w.nomeRegiao("floresta"), "Floresta Encantada");
    EXPECT_EQ(contar(w, "sombra"), 0) << "as sombras somem com o rito";
    EXPECT_EQ(w.eraNoTile({20, 20}), 3) << "a floresta purificada ganha o visual da Alta Fantasia";
    EXPECT_FALSE(w.regiaoHostil());
    EXPECT_LT(w.escuridaoMinAtual(), 0.2f) << "a penumbra da floresta some";
    EXPECT_TRUE(temPagina(w, "purificou:floresta"));
    w.renascerInimigos();
    EXPECT_EQ(contar(w, "sombra") + contar(w, "esqueleto"), 0) << "nem de manhã elas voltam";
    usarTile(w, {27, 10});
    EXPECT_TRUE(avisou(w.tirarEventos(), "pulsa em paz"));
}

TEST(RegioesVivas, FlorestaEncantadaAbandonadaVoltaAEscurecer) {
    World w = vilarejo();
    irFloresta(w);
    w.marcar("purificou:floresta");
    w.entrarRegiao("vilarejo", {61, 19}, Direcao::Esquerda);
    w.estadoRegioes.at("floresta").corrupcao = 59.0f;
    w.estadoRegioes.at("floresta").ultimaVisita = w.relogio.dia - 10;
    w.dormir(false);
    EXPECT_GE(w.estadoRegioes.at("floresta").corrupcao, 60.0f);
    EXPECT_TRUE(temNota(w, "voltou a escurecer"));
    irFloresta(w);
    EXPECT_GT(contar(w, "sombra"), 0) << "as sombras voltaram";
    EXPECT_EQ(w.eraNoTile({20, 20}), 1) << "a cor vai embora com o abandono";
    EXPECT_EQ(w.nomeRegiao("floresta"), "Floresta Encantada") << "o rito não se desfaz: basta voltar e limpar";
}

TEST(RegioesVivas, EntulhoPedeMadeiraEAbreAPassagemDasMinas) {
    World w = vilarejo();
    irFloresta(w);
    espantar(w);
    // Embaixo, a passagem fechada: andar não leva às minas
    w.jogador.pos = {29 * 16 + 8.0f, 33 * 16 + 12.0f};
    w.jogador.dir = Direcao::Baixo;
    passos(w, 60, {{0, 1}});
    EXPECT_EQ(w.regiaoAtual, "floresta") << "o entulho bloqueia";
    w.jogador.pos = {29 * 16 + 8.0f, 33 * 16 + 12.0f};
    w.jogador.dir = Direcao::Baixo;
    w.tirarEventos();
    w.inventario.definir("madeira", 10);
    w.passo(1.0f / 60.0f, {{0, 0}, true});
    EXPECT_TRUE(avisou(w.tirarEventos(), "15 de madeira"));
    w.inventario.definir("madeira", 15);
    w.jogador.acao = 0.0f;
    w.passo(1.0f / 60.0f, {{0, 0}, true});
    EXPECT_TRUE(w.marcos.contains("minas_abertas"));
    EXPECT_EQ(w.inventario.quantidade("madeira"), 0);
    EXPECT_TRUE(temPagina(w, "minas_abertas"));
    passos(w, 90, {{0, 1}});
    EXPECT_EQ(w.regiaoAtual, "minas") << "com a passagem escorada, a trilha desce às minas";
    EXPECT_EQ(w.nomeRegiao("minas"), "Minas Abandonadas");
    EXPECT_TRUE(w.regiaoHostil());
}

TEST(RegioesVivas, VeioDeCristalRendeCristal) {
    World w = vilarejo();
    irMinas(w);
    espantar(w);
    for (int i = 0; i < 5; ++i) usarTile(w, {20, 18});
    EXPECT_EQ(w.inventario.quantidade("cristal"), 1);
    EXPECT_TRUE(w.marcos.contains("primeiro_cristal"));
}

TEST(RegioesVivas, GolemSeRendeEPurificarPedeORitoDaFloresta) {
    World w = vilarejo();
    const entt::entity g = renderGolem(w);
    ASSERT_TRUE(w.reg.valid(g)) << "rendido, ele não morre";
    EXPECT_TRUE(w.reg.get<Inimigo>(g).rendido);
    const auto ev = w.tirarEventos();
    EXPECT_TRUE(std::ranges::any_of(ev, [](const EventoJogo& e) { return e.tipo == EventoJogo::Tipo::AbrirEscolha && e.alvo == "golem_cinzas"; }));
    // De joelhos ele não fere nem anda
    const glm::vec2 onde = w.reg.get<Posicao>(g).v;
    w.jogador.pos = onde + glm::vec2(4, 2);
    w.jogador.vida = 100;
    w.jogador.invulneravel = 0.0f;
    passos(w, 60);
    EXPECT_EQ(w.jogador.vida, 100);
    EXPECT_EQ(w.reg.get<Posicao>(g).v, onde);
    // E perto dele abre a decisão de novo
    w.jogador.pos = onde + glm::vec2(0, 14);
    w.jogador.dir = Direcao::Cima;
    w.jogador.acao = 0.0f;
    w.tirarEventos();
    w.passo(1.0f / 60.0f, {{0, 0}, true});
    EXPECT_EQ(w.alvo().tipo, TipoInteracao::Decidir);
    EXPECT_TRUE(std::ranges::any_of(w.tirarEventos(), [](const EventoJogo& e) { return e.tipo == EventoJogo::Tipo::AbrirEscolha; }));
    EXPECT_FALSE(w.escolher("golem_cinzas", "purificar")) << "sem a floresta purificada, o rito não é conhecido";
    EXPECT_TRUE(avisou(w.tirarEventos(), "Coração da Floresta"));
    EXPECT_TRUE(w.reg.valid(g));
}

TEST(RegioesVivas, DespedacarOGolemReabreAsMinas) {
    World w = vilarejo();
    renderGolem(w);
    const int ouro = w.inventario.quantidade("ouro");
    ASSERT_TRUE(w.escolher("golem_cinzas", "derrotar"));
    EXPECT_EQ(w.inventario.quantidade("ouro"), ouro + 120);
    EXPECT_EQ(contar(w, "golem_cinzas"), 0);
    EXPECT_EQ(w.nomeRegiao("minas"), "Minas de Valdren");
    EXPECT_EQ(w.abates["golem_cinzas"], 1);
    EXPECT_FLOAT_EQ(w.estadoRegioes.at("minas").corrupcao, 30.0f);
    EXPECT_TRUE(temPagina(w, "escolha:golem_cinzas:derrotar"));
    w.renascerInimigos();
    EXPECT_EQ(contar(w, "golem_cinzas"), 0) << "o chefe não volta";
}

TEST(RegioesVivas, PurificarOGolemCriaAsMinasArcanas) {
    World w = vilarejo();
    w.marcar("purificou:floresta");
    renderGolem(w);
    w.renascerInimigos();  // aranhas e esqueletos de volta, para ver que somem na hora da escolha
    ASSERT_GT(contar(w, "aranha"), 0);
    ASSERT_TRUE(w.escolher("golem_cinzas", "purificar"));
    EXPECT_EQ(contar(w, "aranha") + contar(w, "esqueleto") + contar(w, "golem_cinzas"), 0) << "as criaturas somem com o rito, não só de manhã";
    EXPECT_EQ(w.nomeRegiao("minas"), "Minas Arcanas");
    EXPECT_EQ(w.inventario.quantidade("cristal"), 3);
    EXPECT_EQ(w.abates["golem_cinzas"], 0) << "devolvido à luz, não conta como vencido";
    bool guardiao = false;
    for (auto [e, v] : w.reg.view<const EnfeiteRegiao, const Visual>().each()) guardiao = guardiao || v.sprite == "guardiao_cristal";
    EXPECT_TRUE(guardiao) << "o golem vira o guardião das minas";
    EXPECT_FALSE(w.regiaoHostil());
    w.renascerInimigos();
    EXPECT_EQ(contar(w, "aranha") + contar(w, "esqueleto"), 0);
    for (int i = 0; i < 5; ++i) usarTile(w, {20, 18});
    EXPECT_EQ(w.inventario.quantidade("cristal"), 5) << "veio das Minas Arcanas rende 2";
}

TEST(RegioesVivas, PactoDaPoderMasCobraDoReino) {
    World w = vilarejo();
    w.marcar("visitou:floresta");  // a Lua de Sangue só vem depois da floresta
    const float alvoAntes = w.resumo().felicidadeAlvo;
    int luasAntes = 0;
    for (int d = 4; d < 1500; ++d)
        if (const EventoDef* ev = w.eventoDoDia(d); ev && ev->id == "lua_sangue") ++luasAntes;
    renderGolem(w);
    ASSERT_TRUE(w.escolher("golem_cinzas", "pacto"));
    EXPECT_EQ(w.nomeRegiao("minas"), "Minas da Chama");
    EXPECT_EQ(w.equip.arma, "lamina_chama");
    EXPECT_EQ(w.danoArma(), 18);
    EXPECT_FLOAT_EQ(w.resumo().felicidadeAlvo, alvoAntes - 4.0f);
    int luasDepois = 0;
    for (int d = 4; d < 1500; ++d)
        if (const EventoDef* ev = w.eventoDoDia(d); ev && ev->id == "lua_sangue") ++luasDepois;
    EXPECT_GT(luasDepois, luasAntes * 3 / 2) << "a Lua de Sangue vem mais vezes";
    // A corrupção nunca cai abaixo de 50 nas Minas da Chama
    EstadoRegiao& e = w.estadoRegioes.at("minas");
    for (int i = 0; i < 40; ++i) registrarAbate(e, w.transformacao("minas"));
    EXPECT_FLOAT_EQ(e.corrupcao, 50.0f);
}

TEST(RegioesVivas, MissaoDoCoracaoSeCumpreComORito) {
    World w = vilarejo();
    w.missoes["floresta_sussurra"] = {true, 0};
    w.conversar("maelis");  // oferece O Cavaleiro Caído
    w.conversar("maelis");  // e depois O Coração da Floresta
    ASSERT_TRUE(w.missoes.contains("coracao_floresta"));
    irFloresta(w);
    espantar(w);
    w.estadoRegioes.at("floresta").corrupcao = 10.0f;
    usarTile(w, {27, 10});
    EXPECT_TRUE(w.missoes.at("coracao_floresta").concluida);
}

TEST(SaveV6, GuardaAsRegioesEADecisaoDoGolem) {
    const auto pasta = std::filesystem::temp_directory_path() / "koa_teste_save_v6";
    std::filesystem::remove_all(pasta);
    World w = vilarejo();
    renderGolem(w);
    ASSERT_TRUE(w.escolher("golem_cinzas", "pacto"));
    w.estadoRegioes.at("floresta") = {33.0f, 12.0f, 44.0f, 1};
    SaveSystem s(pasta);
    ASSERT_TRUE(s.salvar(w.capturar()));
    EXPECT_TRUE(std::filesystem::exists(pasta / "Save_001/regions.json"));
    std::string erro;
    const auto d = s.carregar("Save_001", &erro);
    ASSERT_TRUE(d) << erro;
    World w2 = vilarejo();
    w2.aplicar(*d);
    EXPECT_FLOAT_EQ(w2.estadoRegioes.at("floresta").corrupcao, 33.0f);
    EXPECT_FLOAT_EQ(w2.estadoRegioes.at("floresta").prosperidade, 44.0f);
    EXPECT_EQ(w2.regiaoAtual, "minas");
    EXPECT_EQ(w2.nomeRegiao("minas"), "Minas da Chama");
    EXPECT_EQ(contar(w2, "golem_cinzas"), 0) << "o golem não volta depois de carregar";
    EXPECT_EQ(w2.equip.arma, "lamina_chama");
    std::filesystem::remove_all(pasta);
}

TEST(SaveV6, SaveV5ComecaAsRegioesComoNoMapa) {
    const auto pasta = std::filesystem::temp_directory_path() / "koa_teste_save_v5_para_v6";
    std::filesystem::remove_all(pasta);
    World w = vilarejo();
    w.relogio.dia = 9;
    w.estadoRegioes.at("floresta").corrupcao = 3.0f;
    SaveSystem s(pasta);
    ASSERT_TRUE(s.salvar(w.capturar()));
    // Vira um save v5: sem regions.json
    std::filesystem::remove(pasta / "Save_001/regions.json");
    auto meta = *ke::fs::lerTexto(pasta / "Save_001/metadata.json");
    const std::string atual = "\"versao_formato\": " + std::to_string(VERSAO_SAVE);
    meta.replace(meta.find(atual), atual.size(), "\"versao_formato\": 5");
    ke::fs::gravarTextoSeguro(pasta / "Save_001/metadata.json", meta);
    std::string erro;
    const auto d = s.carregar("Save_001", &erro);
    ASSERT_TRUE(d) << erro;
    EXPECT_FALSE(d->temRegioes);
    World w2 = vilarejo();
    w2.aplicar(*d);
    EXPECT_FLOAT_EQ(w2.estadoRegioes.at("floresta").corrupcao, 70.0f);
    EXPECT_EQ(w2.estadoRegioes.at("floresta").ultimaVisita, 9) << "não começa contando abandono";
    std::filesystem::remove_all(pasta);
}
