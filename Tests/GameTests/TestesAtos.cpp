// Segundo e terceiro atos da Queda (1.17): o Pântano das Cinzas, a Torre do Selo, os fragmentos novos, a Sacerdotisa,
// Varek e os três finais; as regiões novas no save
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

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
    w.inventario.definir("comida", 300);
    w.inventario.definir("cristal", 20);
    return w;
}

void achar(World& w, int ato) {
    for (const PistaDef& p : dados().lore.pistas)
        if (p.ato == ato && p.fonte.empty()) w.marcar("pista:" + p.id, p.titulo);
}

// Pisa na saída da região atual que leva a `destino`
bool atravessar(World& w, const std::string& destino) {
    for (const auto& s : w.infoAtual().saidas)
        if (s.destino == destino) {
            w.jogador.pos = {s.tile.x * 16 + 8.0f, s.tile.y * 16 + 12.0f};
            w.passo(1.0f / 60.0f, {});
            return w.regiaoAtual == destino;
        }
    return false;
}

// Do começo até diante de Varek
World ateATorre(const std::string& sacerdotisa = "interrogar") {
    World w = vilarejo();
    achar(w, 1);
    EXPECT_TRUE(w.marcos.contains("misterio:primeiro_ato"));
    achar(w, 2);
    EXPECT_TRUE(w.recuperarMemoria()) << "os escribas acham a memória";
    EXPECT_FALSE(w.marcos.contains("misterio:segundo_ato")) << "falta a escolha diante da Sacerdotisa";
    EXPECT_TRUE(w.escolher("sacerdotisa", sacerdotisa));
    EXPECT_TRUE(w.marcos.contains("misterio:segundo_ato"));
    return w;
}

}

TEST(Atos, RegioesNovasEPassagensPorAto) {
    World w = vilarejo();
    ASSERT_TRUE(w.regioes.contains("pantano"));
    ASSERT_TRUE(w.regioes.contains("torre"));
    EXPECT_TRUE(w.estadoRegioes.contains("pantano")) << "o pântano tem corrupção (e entra nas Terras)";
    EXPECT_EQ(dados().lore.pistas.size(), 10u);
    EXPECT_EQ(w.totalDoAto(2), 4);
    // Antes do primeiro ato, a floresta não deixa passar para o pântano
    w.entrarRegiao("floresta", {10, 10}, Direcao::Baixo);
    EXPECT_FALSE(atravessar(w, "pantano"));
    achar(w, 1);
    ASSERT_TRUE(atravessar(w, "pantano"));
    // A Torre só com o segundo ato
    EXPECT_FALSE(atravessar(w, "torre"));
    // As criaturas e a Sacerdotisa estão lá
    bool sacerdotisa = false, matilha = false;
    for (auto [e, i] : w.reg.view<const Inimigo>().each()) sacerdotisa |= i.tipo == "sacerdotisa_cinzas", matilha |= i.tipo == "matilha";
    EXPECT_TRUE(sacerdotisa);
    EXPECT_TRUE(matilha);
    EXPECT_TRUE(atravessar(w, "floresta")) << "e volta";
}

TEST(Atos, SequenciaDoMisterio) {
    World w = vilarejo();
    achar(w, 2);  // os do segundo ato não contam antes do primeiro
    EXPECT_FALSE(w.marcos.contains("misterio:segundo_ato"));
    EXPECT_FALSE(w.recuperarMemoria()) << "a memória só aparece depois do primeiro ato";
    World t = ateATorre();
    t.entrarRegiao("pantano", {4, 2}, Direcao::Baixo);
    ASSERT_TRUE(atravessar(t, "torre"));
    bool varek = false;
    for (auto [e, i] : t.reg.view<const Inimigo>().each()) varek |= i.tipo == "varek";
    EXPECT_TRUE(varek);
}

// Os três finais são alcançáveis e cada um deixa a sua marca
TEST(Atos, OsTresFinais) {
    for (const char* final : {"aprisionar", "unir", "destruir"}) {
        SCOPED_TRACE(final);
        World w = ateATorre(final[0] == 'd' ? "entregar" : "libertar");
        const float ordem = w.relacaoCom("ordem"), culto = w.relacaoCom("culto");
        ASSERT_TRUE(w.escolher("chama_final", final));
        EXPECT_TRUE(w.marcos.contains(std::string("final:") + final));
        EXPECT_TRUE(w.marcos.contains("misterio:terceiro_ato"));
        EXPECT_EQ(w.titulo().id, "fim_da_queda");
        EXPECT_TRUE(std::ranges::any_of(w.cronicas, [&](const EntradaCronica& c) { return c.marco == std::string("escolha:chama_final:") + final; }));
        if (std::string(final) == "aprisionar") EXPECT_GT(w.relacaoCom("ordem"), ordem);
        if (std::string(final) == "unir") {
            EXPECT_TRUE(w.equip.possuidos.contains("lamina_chama"));
            EXPECT_GT(w.relacaoCom("culto"), culto);
        }
        if (std::string(final) == "destruir") {
            EXPECT_EQ(w.inventario.quantidade("cristal"), 0) << "o cristal vira pó";
            for (const auto& [id, e] : w.estadoRegioes) EXPECT_LE(e.corrupcao, 15.0f) << id;
        }
    }
}

TEST(Atos, RegioesNovasNoSave) {
    const auto pasta = std::filesystem::temp_directory_path() / "koa_teste_atos";
    std::filesystem::remove_all(pasta);
    World w = ateATorre();
    w.estadoRegioes.at("pantano").corrupcao = 42.0f;
    w.entrarRegiao("pantano", {4, 2}, Direcao::Baixo);
    ASSERT_TRUE(atravessar(w, "torre"));
    SaveSystem s(pasta);
    ASSERT_TRUE(s.salvar(w.capturar()));
    auto d = s.carregar();
    ASSERT_TRUE(d);
    World w2 = vilarejo();
    w2.aplicar(*d);
    EXPECT_EQ(w2.regiaoAtual, "torre");
    EXPECT_FLOAT_EQ(w2.estadoRegioes.at("pantano").corrupcao, 42.0f);
    EXPECT_TRUE(w2.marcos.contains("misterio:segundo_ato"));
    EXPECT_TRUE(atravessar(w2, "pantano")) << "carregou na Torre e sai de volta para o pântano";
    std::filesystem::remove_all(pasta);
}

// 1.20: a arte própria do Pântano, da Torre do Selo e da Coroa Eterna. O renderer troca "<nome>" por "<estilo>_<nome>"
// quando o atlas tem essa versão; aqui, o estilo de cada lugar e o atlas sem arte própria órfã
TEST(Atos, EstiloVisualDasRegioes) {
    World w = vilarejo();
    EXPECT_EQ(w.estiloVisual(), "");
    w.entrarRegiao("pantano", {4, 2}, Direcao::Baixo);
    EXPECT_EQ(w.estiloVisual(), "pantano");
    w.entrarRegiao("torre", {14, 18}, Direcao::Cima);
    EXPECT_EQ(w.estiloVisual(), "selo");
    int tochas = 0;
    for (auto [e, v, l] : w.reg.view<const Visual, const Luz>().each())
        if (v.sprite.starts_with("tocha")) {
            ++tochas;
            EXPECT_GT(l.cor.b, l.cor.r) << "as tochas do selo são violeta";
        }
    EXPECT_GT(tochas, 0);
    w.entrarRegiao("floresta", {10, 10}, Direcao::Baixo);
    EXPECT_EQ(w.estiloVisual(), "") << "a floresta segue com a arte comum";
    w.entrarRegiao("vilarejo", {60, 18}, Direcao::Esquerda);
    w.marcar("era:coroa_eterna", "", true);
    EXPECT_EQ(w.estiloVisual(), "coroa") << "o vilarejo da Coroa Eterna";

    const auto texto = ke::fs::lerTexto(std::filesystem::path(PASTA_DATA).parent_path() / "Assets/Sprites/atlas.json");
    ASSERT_TRUE(texto);
    const auto atlas = nlohmann::json::parse(*texto);
    for (const std::string estilo : {"pantano", "selo", "coroa"}) {
        int tiles = 0;
        for (const auto& [nome, r] : atlas.items()) {
            if (!nome.starts_with(estilo + "_")) continue;
            const std::string comum = nome.substr(estilo.size() + 1);
            EXPECT_TRUE(atlas.contains(comum)) << nome << " não substitui nenhum sprite";
            tiles += comum.starts_with("terreno_") ? 1 : 0;
        }
        EXPECT_GE(tiles, 8) << estilo << ": pelo menos dois terrenos próprios";
    }
}