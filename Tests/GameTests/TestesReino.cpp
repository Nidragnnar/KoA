// Fase 5: construir no mapa, economia diária, população, eras e save v3
#include <gtest/gtest.h>

#include <glm/geometric.hpp>

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

void darRecursos(World& w, int n = 500) {
    for (const char* r : {"madeira", "pedra", "ouro"}) w.inventario.definir(r, n);
}

// Primeiro lugar livre perto de um ponto (o mapa tem árvores espalhadas)
glm::ivec2 lugarLivre(const World& w, const std::string& def, glm::ivec2 perto) {
    for (int raio = 0; raio < 30; ++raio)
        for (int dy = -raio; dy <= raio; ++dy)
            for (int dx = -raio; dx <= raio; ++dx)
                if (std::max(std::abs(dx), std::abs(dy)) == raio && w.podeConstruir(def, perto + glm::ivec2(dx, dy))) return perto + glm::ivec2(dx, dy);
    return {-1, -1};
}

}

TEST(Obras, DadosDas8Construcoes) {
    int construiveis = 0, monumentos = 0;
    for (const auto& id : dados().ordemConstrucoes) {
        const BuildingDef& d = dados().construcoes.at(id);
        construiveis += d.construivel && !d.monumento;
        monumentos += d.monumento;
    }
    EXPECT_EQ(construiveis, 8);
    EXPECT_EQ(monumentos, 3);  // memorial, jardim e estátua (fase 9)
    EXPECT_EQ(dados().construcoes.at("taverna").nivel(1)->nome, "Pequena Taverna");
    EXPECT_EQ(dados().construcoes.at("taverna").nivel(3)->nome, "Grande Estalagem");
    EXPECT_FLOAT_EQ(dados().construcoes.at("campo").nivel(1)->efeito("comida_dia"), 5.0f);
}

TEST(Obras, NaoConstroiEmCimaDeArvoreAguaOuCasa) {
    World w = vilarejo();
    std::string motivo;
    const BuildingData& casa = *w.reino.construcao("casa_herdeiro");
    EXPECT_FALSE(w.podeConstruir("moradia", casa.tile, &motivo));
    EXPECT_NE(motivo.find("construções"), std::string::npos) << motivo;
    EXPECT_FALSE(w.podeConstruir("moradia", {5, 37}, &motivo)) << "rio";
    EXPECT_FALSE(w.podeConstruir("casa", {40, 28}, &motivo)) << "a casa do herdeiro não está no menu";
    EXPECT_FALSE(w.podeConstruir("moradia", {0, 0}, &motivo)) << "borda do mapa";
    EXPECT_NE(lugarLivre(w, "moradia", {30, 26}), glm::ivec2(-1, -1)) << "mas há lugar livre no vilarejo";
    const glm::ivec2 praca = w.locais.at("praca");
    EXPECT_FALSE(w.podeConstruir("torre", praca - glm::ivec2(0, 1), &motivo)) << "a praça é dos moradores";
    EXPECT_NE(motivo.find("moradores"), std::string::npos) << motivo;
}

TEST(Obras, CustaRecursosEFicaProntaDeManha) {
    World w = vilarejo();
    darRecursos(w);
    const glm::ivec2 t = lugarLivre(w, "campo", {30, 26});
    ASSERT_NE(t, glm::ivec2(-1, -1));
    const std::string id = w.construir("campo", t);
    ASSERT_FALSE(id.empty());
    const BuildingData* b = w.reino.construcao(id);
    EXPECT_FALSE(b->constructed) << "em obras";
    EXPECT_EQ(w.inventario.quantidade("madeira"), 500 - 8);
    EXPECT_FALSE(w.podeConstruir("campo", t)) << "o terreno ficou ocupado";
    EXPECT_FALSE(w.melhorarConstrucao(id)) << "não melhora enquanto está em obras";
    w.dormir(false);
    EXPECT_TRUE(w.reino.construcao(id)->constructed);
    EXPECT_NE(w.ultimoRelatorio.notas.front().find("Horta Remexida"), std::string::npos);
}

TEST(Obras, RespeitaOLimite) {
    World w = vilarejo();
    darRecursos(w);
    ASSERT_FALSE(w.construir("taverna", lugarLivre(w, "taverna", {30, 26})).empty());
    std::string motivo;
    const glm::ivec2 outro = lugarLivre(w, "moradia", {50, 30});
    EXPECT_FALSE(w.podeConstruir("taverna", outro, &motivo));
    EXPECT_NE(motivo.find("máximo"), std::string::npos);
}

TEST(Obras, SemRecursosNaoConstroi) {
    World w = vilarejo();
    const glm::ivec2 t = lugarLivre(w, "taverna", {30, 26});
    EXPECT_TRUE(w.construir("taverna", t).empty());
    EXPECT_EQ(w.contarConstrucoes("taverna"), 0);
}

TEST(Economia, ImpostosComidaEFelicidade) {
    World w = vilarejo();
    const int ouro0 = w.inventario.quantidade("ouro"), comida0 = w.inventario.quantidade("comida");
    const ResumoReino r = w.resumo();
    EXPECT_EQ(r.populacaoMax, 8 + 2) << "ruínas + cabana";
    EXPECT_EQ(r.impostos, 8 * 2) << "médio = 2 por morador";
    w.dormir(false);
    EXPECT_EQ(w.inventario.quantidade("ouro"), ouro0 + 16);
    EXPECT_EQ(w.inventario.quantidade("comida"), comida0 - 8) << "cada morador come 1";
    // Sem nenhuma melhoria o povo desanima devagar: 40 → 38 (o alvo base é 35)
    EXPECT_NEAR(w.estado.felicidade, 38.0f, 0.01f);
}

TEST(Economia, ImpostoAltoDeixaOPovoInfeliz) {
    World baixo = vilarejo(), alto = vilarejo();
    baixo.estado.imposto = Imposto::Baixo;
    alto.estado.imposto = Imposto::Alto;
    for (int i = 0; i < 4; ++i) baixo.dormir(false), alto.dormir(false);
    EXPECT_GT(baixo.estado.felicidade, alto.estado.felicidade + 15.0f);
    EXPECT_GT(alto.inventario.quantidade("ouro"), baixo.inventario.quantidade("ouro"));
}

TEST(Economia, FomeFazGenteIrEmbora) {
    World w = vilarejo();
    w.inventario.definir("comida", 0);
    w.dormir(false);
    EXPECT_EQ(w.estado.populacao, 7);
    EXPECT_NE(w.ultimoRelatorio.notas.back().find("Faltou comida"), std::string::npos);
}

TEST(Economia, PopulacaoCresceComMoradiaEFelicidade) {
    World w = vilarejo();
    darRecursos(w, 2000);
    w.inventario.definir("comida", 500);
    ASSERT_FALSE(w.construir("taverna", lugarLivre(w, "taverna", {30, 26})).empty());
    ASSERT_FALSE(w.construir("moradia", lugarLivre(w, "moradia", {45, 28})).empty());
    ASSERT_FALSE(w.construir("moradia", lugarLivre(w, "moradia", {12, 22})).empty());
    w.estado.imposto = Imposto::Baixo;
    for (int i = 0; i < 10; ++i) w.dormir(false);
    EXPECT_GT(w.estado.populacao, 12) << "felizes e com casa, novas famílias chegam";
    EXPECT_LE(w.estado.populacao, w.resumo().populacaoMax);
    int aldeoes = 0;
    for (auto [e, a] : w.reg.view<Aldeao>().each()) ++aldeoes;
    EXPECT_EQ(aldeoes, w.estado.populacao - 5) << "a população aparece andando pelo vilarejo";
}

TEST(Economia, EraAvancaComODesenvolvimento) {
    World w = vilarejo();
    EXPECT_EQ(w.reino.eraAlvo(), KingdomEra::DarkAge);
    darRecursos(w, 5000);
    for (const char* d : {"taverna", "mercado", "capela", "forja"}) ASSERT_FALSE(w.construir(d, lugarLivre(w, d, {32, 27})).empty()) << d;
    w.dormir(false);
    for (auto& b : w.reino.construcoes) b.level = 2;  // como se tivesse melhorado tudo
    EXPECT_GE(w.reino.desenvolvimento(), 8);
    EXPECT_EQ(w.reino.eraAlvo(), KingdomEra::Restoration);
}

// Migra uma CÓPIA de um save de verdade (pasta em KOA_SAVE_COPIA); sem a variável, o teste é pulado
TEST(SaveV3, MigraCopiaDeSaveReal) {
    char* pasta = nullptr;
    size_t n = 0;
    if (_dupenv_s(&pasta, &n, "KOA_SAVE_COPIA") != 0 || !pasta) GTEST_SKIP() << "KOA_SAVE_COPIA não definida";
    const std::filesystem::path dir = pasta;
    free(pasta);
    SaveSystem s(dir);
    std::string erro, aviso;
    const auto d = s.carregar("Save_001", &erro, &aviso);
    ASSERT_TRUE(d) << erro;
    EXPECT_TRUE(aviso.empty()) << aviso;
    World w = vilarejo();
    w.aplicar(*d);
    EXPECT_EQ(w.reino.construcao("casa_herdeiro")->level, std::ranges::find(d->construcoes, std::string("casa_herdeiro"), &EstadoConstrucao::id)->nivel);
    EXPECT_EQ(w.relogio.dia, d->dia);
    if (!d->temPovo) EXPECT_EQ(w.estado.populacao, 8) << "save antigo começa com o povo inicial";
    // E grava de volta já no formato novo
    ASSERT_TRUE(s.salvar(w.capturar()));
    const auto d2 = s.carregar();
    ASSERT_TRUE(d2);
    EXPECT_TRUE(d2->temPovo);
    EXPECT_TRUE(d2->temCronicas);
    std::printf("[save real] dia %d, casa nível %d, %zu objetos alterados, %zu moradores, %zu páginas nas Crônicas, título %s\n", d2->dia,
                w.reino.construcao("casa_herdeiro")->level, d2->objetos.size(), d2->moradores.size(), d2->cronicas.size(),
                w.titulo().nome.c_str());
    for (const auto& c : d2->cronicas) std::printf("  [%s] %s\n", c.marco.c_str(), c.texto.c_str());
}

TEST(SaveV3, GuardaObrasEPovo) {
    const auto pasta = std::filesystem::temp_directory_path() / "koa_teste_save_v3";
    std::filesystem::remove_all(pasta);
    World w = vilarejo();
    darRecursos(w);
    const glm::ivec2 t = lugarLivre(w, "moradia", {30, 26});
    const std::string id = w.construir("moradia", t);
    w.estado.imposto = Imposto::Alto;
    w.estado.populacao = 9;
    SaveSystem s(pasta);
    ASSERT_TRUE(s.salvar(w.capturar()));
    World w2 = vilarejo();
    const auto d = s.carregar();
    ASSERT_TRUE(d);
    w2.aplicar(*d);
    const BuildingData* b = w2.reino.construcao(id);
    ASSERT_NE(b, nullptr) << "a obra colocada volta no mesmo lugar";
    EXPECT_EQ(b->tile, t);
    EXPECT_FALSE(b->constructed);
    EXPECT_EQ(w2.estado.imposto, Imposto::Alto);
    EXPECT_EQ(w2.estado.populacao, 9);
    EXPECT_FALSE(w2.podeConstruir("moradia", t)) << "o terreno continua ocupado depois de carregar";
    std::filesystem::remove_all(pasta);
}
