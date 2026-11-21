// Fase 17: fundações (saves-modelo de cada versão, gravação atômica, semente no save, colisão e regras nos dados)
#include <gtest/gtest.h>

#include <filesystem>

#include "Engine/Platform/FileSystem.h"
#include "Game/Exploration/Components.h"
#include "Game/World/World.h"
#include "Tools/Balanceamento/JogadorAutomatico.h"

using namespace koa;
namespace fs = std::filesystem;

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

glm::ivec2 lugarLivre(const World& w, const std::string& def, glm::ivec2 perto) {
    for (int raio = 0; raio < 30; ++raio)
        for (int dy = -raio; dy <= raio; ++dy)
            for (int dx = -raio; dx <= raio; ++dx)
                if (std::max(std::abs(dx), std::abs(dy)) == raio && w.podeConstruir(def, perto + glm::ivec2(dx, dy))) return perto + glm::ivec2(dx, dy);
    return {-1, -1};
}

// Pasta temporária limpa para cada teste
fs::path temporaria(const std::string& nome) {
    const auto p = fs::temp_directory_path() / ("koa_fase17_" + nome);
    fs::remove_all(p);
    fs::create_directories(p);
    return p;
}

}

// Um save real de cada versão (tirados dos backups do jogo, de v1 a v16; não há save real da v4)
TEST(SavesModelo, TodasAsVersoesAbremEGravamNaAtual) {
    const fs::path modelos = PASTA_FIXTURES "/saves";
    int abertos = 0;
    for (const auto& versao : fs::directory_iterator(modelos)) {
        SCOPED_TRACE(versao.path().filename().string());
        const auto pasta = temporaria("modelo_" + versao.path().filename().string());
        fs::copy(versao.path(), pasta, fs::copy_options::recursive);
        SaveSystem s(pasta);
        std::string erro, aviso;
        const auto d = s.carregar("Save_001", &erro, &aviso);
        ASSERT_TRUE(d) << erro;
        EXPECT_TRUE(aviso.empty()) << aviso;
        World w = vilarejo();
        w.aplicar(*d);
        EXPECT_GE(w.relogio.dia, 1);
        EXPECT_EQ(w.canteiros.size(), dados().cotidiano.canteiros.size());
        ASSERT_NE(w.reino.construcao("castelo"), nullptr) << "o castelo existe mesmo em saves de antes dele";
        EXPECT_EQ(w.relacoes.size(), dados().faccoes.faccoes.size()) << "os vizinhos existem mesmo em saves de antes deles";
        for (const auto& [id, q] : w.inventario.todos()) EXPECT_GE(q, 0) << id;
        // Grava no formato atual e abre de novo
        ASSERT_TRUE(s.salvar(w.capturar()));
        const auto d2 = s.carregar("Save_001", &erro);
        ASSERT_TRUE(d2) << erro;
        EXPECT_EQ(d2->dia, w.relogio.dia);
        EXPECT_EQ(d2->construcoes.size(), w.capturar().construcoes.size());
        const auto meta = ke::fs::lerTexto(pasta / "Save_001/metadata.json");
        ASSERT_TRUE(meta);
        EXPECT_NE(meta->find("\"versao_formato\": " + std::to_string(VERSAO_SAVE)), std::string::npos);
        fs::remove_all(pasta);
        ++abertos;
    }
    EXPECT_EQ(abertos, 15) << "v1, v2, v3, v5 a v16";
}

TEST(SaveAtomico, OSaveAnteriorViraBackupInteiro) {
    const auto pasta = temporaria("atomico");
    SaveSystem s(pasta);
    World w = vilarejo();
    ASSERT_TRUE(s.salvar(w.capturar()));
    EXPECT_FALSE(fs::exists(pasta / "Save_001.bak")) << "o primeiro save não tem anterior";
    w.inventario.definir("ouro", 777);
    ASSERT_TRUE(s.salvar(w.capturar()));
    EXPECT_TRUE(fs::exists(pasta / "Save_001.bak/metadata.json"));
    EXPECT_FALSE(fs::exists(pasta / "Save_001.novo")) << "a pasta de troca não sobra";
    const auto d = s.carregar();
    ASSERT_TRUE(d);
    EXPECT_EQ(d->inventario.at("ouro"), 777);
    fs::remove_all(pasta);
}

TEST(SaveAtomico, QuedaNoMeioNaoMisturaMomentos) {
    const auto pasta = temporaria("queda");
    SaveSystem s(pasta);
    World w = vilarejo();
    w.inventario.definir("ouro", 10);
    ASSERT_TRUE(s.salvar(w.capturar()));
    // Gravação nova interrompida: a pasta ".novo" ficou pela metade (sem metadata)
    fs::create_directories(pasta / "Save_001.novo");
    ke::fs::gravarTextoSeguro(pasta / "Save_001.novo/inventory.json", "{\"ouro\": 999}");
    auto d = s.carregar();
    ASSERT_TRUE(d);
    EXPECT_EQ(d->inventario.at("ouro"), 10) << "o save antigo inteiro continua valendo";
    // Interrompida depois de mover o antigo para .bak: a ".novo" completa vira o save
    w.inventario.definir("ouro", 55);
    ASSERT_TRUE(s.salvar(w.capturar()));
    fs::rename(pasta / "Save_001", pasta / "Save_001.novo_completo");
    fs::remove_all(pasta / "Save_001.novo");
    fs::rename(pasta / "Save_001.novo_completo", pasta / "Save_001.novo");
    d = s.carregar();
    ASSERT_TRUE(d);
    EXPECT_EQ(d->inventario.at("ouro"), 55);
    EXPECT_TRUE(fs::exists(pasta / "Save_001/metadata.json"));
    fs::remove_all(pasta);
}

TEST(Semente, CadaSaveGuardaASuaSorte) {
    World w = vilarejo();
    EXPECT_EQ(w.semente(), 2026u) << "testes e roteiro seguem com a semente fixa";
    w.semear(123456u);
    const DadosSave d = w.capturar();
    EXPECT_NE(d.semente, 0u);
    w.relogio.dia += 1;
    EXPECT_NE(w.capturar().semente, d.semente) << "outro momento, outra sorte";
    World w2 = vilarejo();
    w2.aplicar(d);
    EXPECT_EQ(w2.semente(), d.semente);
    // Save antigo (sem semente): fica com a de sempre
    DadosSave antigo = d;
    antigo.semente = 0;
    World w3 = vilarejo();
    w3.aplicar(antigo);
    EXPECT_EQ(w3.semente(), 2026u);
}

// ---------- Regras nos dados ----------

TEST(RegrasNosDados, FuncoesERuinasVemDoJson) {
    const auto& c = dados().construcoes;
    EXPECT_EQ(c.at("muralha").funcao, "muralha");
    EXPECT_TRUE(c.at("muralha").ruinas);
    EXPECT_FALSE(c.at("muralha").janelas);
    EXPECT_TRUE(c.at("castelo").ruinas);
    EXPECT_FALSE(c.at("casa").contaComoObra);
    EXPECT_EQ(c.at("casa").fumaca.size(), 2u);
    EXPECT_EQ(c.at("forja").acoes, std::vector<std::string>{"forjar"});
    World w = vilarejo();
    EXPECT_TRUE(w.reino.defsEmRuinas.contains("castelo"));
    EXPECT_EQ(w.porFuncao("muralha"), w.reino.construcao("muralha_leste"));
    EXPECT_EQ(w.nivelForja(), 0);
    for (const char* r : {"madeira", "pedra", "ouro"}) w.inventario.definir(r, 2000);
    const std::string forja = w.construir("forja", lugarLivre(w, "forja", {44, 26}));
    ASSERT_FALSE(forja.empty());
    w.dormir(false);
    EXPECT_EQ(w.nivelForja(), 1) << "o nível vem do efeito nivel_forja";
    ASSERT_TRUE(w.melhorarConstrucao(forja));
    EXPECT_EQ(w.nivelForja(), 2);
}

// O código não compara mais o id das construções: as regras vêm dos dados
TEST(RegrasNosDados, NenhumIdDeConstrucaoNoCodigo) {
    int achados = 0;
    for (const auto& arq : fs::recursive_directory_iterator(fs::path(PASTA_DATA).parent_path() / "Game")) {
        if (arq.path().extension() != ".cpp" && arq.path().extension() != ".h") continue;
        const auto texto = ke::fs::lerTexto(arq.path());
        ASSERT_TRUE(texto);
        for (const char* padrao : {"defId == \"", "defId != \""})
            for (size_t p = texto->find(padrao); p != std::string::npos; p = texto->find(padrao, p + 1)) {
                ADD_FAILURE() << arq.path().filename().string() << ": " << texto->substr(p, 40);
                ++achados;
            }
    }
    EXPECT_EQ(achados, 0);
}

// ---------- Colisão em cache ----------

namespace {

bool mesmosObstaculos(const World& w) {
    auto a = w.obstaculos();
    auto b = w.calcularObstaculos();
    auto chave = [](const ke::AABB& c) { return std::tuple(c.min.x, c.min.y, c.max.x, c.max.y); };
    std::ranges::sort(a, {}, chave);
    std::ranges::sort(b, {}, chave);
    return a.size() == b.size() && std::ranges::equal(a, b, [](const ke::AABB& x, const ke::AABB& y) { return x.min == y.min && x.max == y.max; });
}

}

TEST(ColisaoEmCache, IgualAoCalculoCompletoDepoisDeCadaMudanca) {
    World w = vilarejo();
    EXPECT_TRUE(mesmosObstaculos(w));
    // Corta uma árvore até o toco
    entt::entity arvore = entt::null;
    for (auto [e, c] : w.reg.view<const Coletavel>().each())
        if (c.tipo == "arvore") arvore = e;
    ASSERT_TRUE(arvore != entt::null);
    w.reg.get<Coletavel>(arvore).hp = 1;
    w.jogador.pos = w.reg.get<Posicao>(arvore).v + glm::vec2(0, 12);
    w.jogador.dir = Direcao::Cima;
    for (int i = 0; i < 90 && !w.reg.get<Coletavel>(arvore).esgotado; ++i) {
        EntradaMundo in;
        in.interagir = in.interagirSegurando = true;
        w.passo(1.0f / 60.0f, in);
    }
    ASSERT_TRUE(w.reg.get<Coletavel>(arvore).esgotado);
    EXPECT_TRUE(mesmosObstaculos(w)) << "depois de cortar";
    // Uma obra nova e a noite
    for (const char* r : {"madeira", "pedra", "ouro"}) w.inventario.definir(r, 500);
    ASSERT_FALSE(w.construir("moradia", lugarLivre(w, "moradia", {44, 27})).empty());
    EXPECT_TRUE(mesmosObstaculos(w)) << "obra começada";
    w.dormir(false);
    EXPECT_TRUE(mesmosObstaculos(w)) << "obra pronta";
    // Outra região e a volta
    w.entrarRegiao("floresta", {3, 17}, Direcao::Direita);
    EXPECT_TRUE(mesmosObstaculos(w)) << "na floresta";
    w.entrarRegiao("vilarejo", {60, 18}, Direcao::Esquerda);
    EXPECT_TRUE(mesmosObstaculos(w)) << "de volta";
}

TEST(ColisaoEmCache, AnimaisNaoAtravessamParedes) {
    World w = vilarejo();
    for (const char* r : {"madeira", "pedra", "ouro"}) w.inventario.definir(r, 500);
    w.inventario.definir("comida", 500);
    w.reino.construcoes.push_back({"galinheiro_t", "galinheiro", "Galinheiro", 2, {30, 22}, true, true});
    w.reino.construcoes.push_back({"curral_t", "curral", "Curral", 2, {36, 22}, true, true});
    w.sincronizarAnimais();
    for (int i = 0; i < 60 * 60; ++i) {
        w.passo(1.0f / 60.0f, {});
        if (i % 120 != 0) continue;
        for (auto [e, p, a] : w.reg.view<const Posicao, const Animal>().each())
            for (const ke::AABB& o : w.obstaculos())
                ASSERT_FALSE(p.v.x > o.min.x + 1 && p.v.x < o.max.x - 1 && p.v.y > o.min.y + 1 && p.v.y < o.max.y - 1)
                    << a.tipo << " dentro de um obstáculo no quadro " << i;
    }
}
// ---------- Balanceamento ----------

// O mesmo jogador automático da ferramenta, no ritmo médio: as faixas do balanceamento valem nos primeiros 25 dias
// (a ferramenta roda os 120 dias em 4 ritmos; aqui fica curto para o teste não demorar)
TEST(Balanceamento, RitmoMedioDentroDasFaixas) {
    World w = vilarejo();
    w.inventario.definir("comida", 20);
    balanceamento::Ritmo ritmo;
    balanceamento::JogadorAutomatico jogador(w, dados(), ritmo);
    for (int d = 0; d < 25; ++d) jogador.jogarDia();
    const auto al = balanceamento::alarmes(jogador.historico(), ritmo);
    for (const auto& a : al) ADD_FAILURE() << a;
    const auto& fim = jogador.historico().back();
    EXPECT_GE(fim.era, 2);
    EXPECT_LE(fim.diasDeComida, 20.0f);
}

TEST(Balanceamento, CeleiroCheioEstragaEAglomeracaoPesa) {
    World w = vilarejo();
    w.inventario.definir("comida", 5000);
    w.dormir(false);
    bool estragou = false;
    for (const auto& [chave, v] : w.ultimoRelatorio.variacoes) estragou |= chave == "comida|estragou" && v < 0;
    EXPECT_TRUE(estragou);
    EXPECT_LT(w.inventario.quantidade("comida"), 5000);
    w.estado.populacao = 100;
    EXPECT_FLOAT_EQ(w.resumo().aglomeracao, (100 - POVO_CONFORTAVEL) * FELICIDADE_POR_MORADOR);
}