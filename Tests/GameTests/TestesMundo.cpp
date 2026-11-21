// Mundo vivo (1.14): aldeias vizinhas, caravanas, viajantes, a terra tomada que avança, as notícias da manhã e o save v17
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

// Vilarejo com a estrada leste limpa (o mundo vivo começa aí)
World vilarejo(uint32_t semente = 2026) {
    World w;
    std::string erro;
    EXPECT_TRUE(w.carregar(dados(), PASTA_DATA, &erro)) << erro;
    w.semear(semente);
    w.iniciarMundoVivo();
    for (const char* r : {"madeira", "pedra", "ouro", "comida"}) w.inventario.definir(r, 3000);
    w.marcar("missao:limpar_estrada", "", true);
    return w;
}

void noites(World& w, int n) {
    for (int i = 0; i < n; ++i) w.dormir(false);
}

int noticiasNoRelatorio(const World& w) {
    return static_cast<int>(std::ranges::count_if(w.ultimoRelatorio.notas, [](const std::string& n) { return n.starts_with("Notícia: "); }));
}

}

TEST(MundoVivo, DadosEInicio) {
    const DadosMundo& d = dados().mundo;
    EXPECT_EQ(d.aldeias.size(), 3u);
    EXPECT_EQ(d.ajudas.size(), 3u);
    EXPECT_EQ(d.viajantes.size(), 6u);
    EXPECT_EQ(d.noticiasPorManha, 2);
    // Antes da estrada leste, nada acontece longe do herdeiro
    World w;
    std::string erro;
    ASSERT_TRUE(w.carregar(dados(), PASTA_DATA, &erro)) << erro;
    EXPECT_FALSE(w.mundoVivoAtivo());
    const auto antes = w.mundoVivo.aldeias.at("vau_cinzento").prosperidade;
    noites(w, 3);
    EXPECT_FLOAT_EQ(w.mundoVivo.aldeias.at("vau_cinzento").prosperidade, antes);
    EXPECT_TRUE(w.mundoVivo.noticias.empty());
}

// A mesma partida (semente) dá sempre a mesma história das aldeias; a corrupção ao redor manda na prosperidade
TEST(MundoVivo, AldeiasEvoluemPelaSementeEPelaCorrupcao) {
    auto rodar = [](uint32_t semente, float floresta) {
        World w = vilarejo(semente);
        for (int i = 0; i < 25; ++i) {
            w.estadoRegioes.at("floresta").corrupcao = floresta;
            w.dormir(false);
        }
        return w.mundoVivo.aldeias.at("vau_cinzento");
    };
    const EstadoAldeia a = rodar(7, 20.0f), b = rodar(7, 20.0f);
    EXPECT_EQ(a.povo, b.povo);
    EXPECT_FLOAT_EQ(a.prosperidade, b.prosperidade);
    const EstadoAldeia limpa = rodar(7, 20.0f), suja = rodar(7, 84.0f);
    EXPECT_GT(limpa.prosperidade, suja.prosperidade + 15.0f);
    EXPECT_GT(limpa.povo, dados().mundo.aldeia("vau_cinzento")->povo) << "terra em paz: o povo cresce";
    EXPECT_FLOAT_EQ(alvoProsperidade(*dados().mundo.aldeia("vau_cinzento"), 84.0f), 60.0f - 34.0f * 0.6f);
}

TEST(MundoVivo, TerraTomadaAvancaParaAsVizinhas) {
    World calma = vilarejo(), tomada = vilarejo();
    calma.estadoRegioes.at("floresta").corrupcao = 50.0f;
    tomada.estadoRegioes.at("floresta").corrupcao = 95.0f;
    calma.dormir(false);
    tomada.dormir(false);
    EXPECT_NEAR(tomada.estadoRegioes.at("minas").corrupcao - calma.estadoRegioes.at("minas").corrupcao, dados().mundo.avancoPressao, 0.01f);
    EXPECT_TRUE(tomada.marcos.contains("avanco:floresta"));
    EXPECT_FALSE(calma.marcos.contains("avanco:floresta"));
    EXPECT_TRUE(std::ranges::any_of(tomada.mundoVivo.noticias, [](const std::string& n) { return n.find("começou a avançar") != std::string::npos; }));
}

// Muita coisa na mesma noite: o relatório traz só 2 notícias (as mais importantes); o painel das Terras guarda todas
TEST(MundoVivo, NoMaximoDuasNoticiasPorManha) {
    World w = vilarejo();
    w.reino.construcoes.push_back({"mercado_teste", "mercado", "Mercado", 1, {0, 0}});
    w.estadoRegioes.at("floresta").corrupcao = 95.0f;
    for (auto& [id, e] : w.mundoVivo.aldeias) {
        e.prosperidade = 25.0f;  // fome em todas
        e.proximaCaravana = w.relogio.dia + 1;
    }
    w.mundoVivo.aldeias.at("pedra_alta").prosperidade = 40.0f;  // esta manda caravana
    w.mundoVivo.pedidos.push_back({"faminto", "", w.relogio.dia});  // vence esta noite (sem notícia: viajante só vai embora)
    w.dormir(false);
    EXPECT_GT(w.mundoVivo.noticias.size(), 2u);
    EXPECT_EQ(noticiasNoRelatorio(w), 2);
    // A primeira do relatório é a mais importante da noite
    EXPECT_EQ(w.ultimoRelatorio.notas.end()[-2], "Notícia: " + w.mundoVivo.noticias[0]);
    const auto o = ordenarNoticias({{1, "a"}, {9, "b"}, {5, "c"}, {9, "d"}});
    EXPECT_EQ(o, (std::vector<std::string>{"b", "d", "c", "a"}));
}

TEST(MundoVivo, AjudarAldeiaRendeGratidaoEIgnorarCusta) {
    World w = vilarejo();
    auto& vau = w.mundoVivo.aldeias.at("vau_cinzento");
    w.mundoVivo.pedidos.push_back({"fome", "vau_cinzento", w.relogio.dia + 3});
    vau.prosperidade = 30.0f;
    const int comida = w.inventario.quantidade("comida");
    ASSERT_TRUE(w.atenderPedido(0));
    EXPECT_EQ(w.inventario.quantidade("comida"), comida - 30);
    EXPECT_FLOAT_EQ(vau.prosperidade, 50.0f);
    EXPECT_EQ(vau.gratidao, 1);
    EXPECT_TRUE(w.marcos.contains("ajudou_aldeia:vau_cinzento"));
    EXPECT_GT(w.reputacao.generosidade, 0.0f);
    // Segunda ajuda: a aldeia passa a mandar tributo
    w.mundoVivo.pedidos.push_back({"bandidos", "vau_cinzento", w.relogio.dia + 2});
    ASSERT_TRUE(w.atenderPedido(0));
    const int ouro = w.inventario.quantidade("ouro");
    w.dormir(false);
    EXPECT_GE(w.inventario.quantidade("ouro"), ouro + dados().mundo.tributo) << "tributo de quem foi ajudado";
    // Ignorar: o prazo vence, a aldeia perde prosperidade e gente
    World i = vilarejo();
    auto& pa = i.mundoVivo.aldeias.at("pedra_alta");
    pa.prosperidade = 50.0f;
    const int povo = pa.povo;
    i.mundoVivo.pedidos.push_back({"criaturas", "pedra_alta", i.relogio.dia});
    i.dormir(false);
    EXPECT_LT(pa.povo, povo);
    EXPECT_TRUE(std::ranges::none_of(i.mundoVivo.pedidos, [](const PedidoAberto& p) { return p.aldeia == "pedra_alta" && p.def == "criaturas"; }));
}

TEST(MundoVivo, AldeiaAbandonadaMandaRefugiados) {
    World w = vilarejo();
    w.estado.populacaoBase += 20;  // casas sobrando
    const int pop = w.estado.populacao;
    w.mundoVivo.aldeias.at("ermo_velas").povo = 4;
    w.dormir(false);
    EXPECT_TRUE(w.mundoVivo.aldeias.at("ermo_velas").sumiu);
    EXPECT_TRUE(w.marcos.contains("aldeia_sumiu:ermo_velas"));
    EXPECT_GT(w.estado.populacao, pop - 1) << "os sobreviventes vêm para Valdren";
    // Abandonada, não pede mais nada nem manda caravana
    noites(w, 10);
    EXPECT_TRUE(std::ranges::none_of(w.mundoVivo.pedidos, [](const PedidoAberto& p) { return p.aldeia == "ermo_velas"; }));
}

TEST(MundoVivo, CaravanasMexemNoMercado) {
    EXPECT_FLOAT_EQ(riscoCaravana(0.0f, 0), 0.05f);
    EXPECT_FLOAT_EQ(riscoCaravana(100.0f, 0), 0.5f);
    EXPECT_FLOAT_EQ(riscoCaravana(100.0f, 300), 0.05f) << "um reino seguro protege a estrada";
    // A caravana do Vau traz comida: chegando, a comida barateia; atacada, encarece
    int chegaram = 0, atacadas = 0;
    for (uint32_t s = 1; s <= 12; ++s) {
        World w = vilarejo(s);
        w.reino.construcoes.push_back({"mercado_teste", "mercado", "Mercado", 1, {0, 0}});
        w.estadoRegioes.at("floresta").corrupcao = 90.0f;  // estrada perigosa: sai de tudo
        w.mundoVivo.aldeias.at("vau_cinzento").prosperidade = 60.0f;
        w.mundoVivo.aldeias.at("vau_cinzento").proximaCaravana = w.relogio.dia + 1;
        w.mercado = {};
        const int ouro = w.inventario.quantidade("ouro");
        w.dormir(false);
        const float p = w.mercado.pressao.contains("comida") ? w.mercado.pressao.at("comida") : 0.0f;
        const bool chegou = std::ranges::any_of(w.mundoVivo.noticias, [](const std::string& n) { return n.starts_with("Chegou as carroças do Vau"); });
        if (chegou) {
            ++chegaram;
            EXPECT_LT(p, 0.0f);
            EXPECT_GE(w.inventario.quantidade("ouro") - ouro, 5);
        } else {
            ++atacadas;
            EXPECT_GT(p, 0.0f);
        }
        EXPECT_EQ(w.mundoVivo.aldeias.at("vau_cinzento").proximaCaravana, w.relogio.dia + 5);
    }
    EXPECT_GT(chegaram, 0);
    EXPECT_GT(atacadas, 0) << "com a estrada tomada, algumas caem em emboscada";
}

TEST(MundoVivo, ViajantesPedemSoPorUmDia) {
    World w = vilarejo();
    int dia = -1;
    for (int i = 0; i < 40 && dia < 0; ++i) {
        w.dormir(false);
        for (const auto& p : w.mundoVivo.pedidos)
            if (p.aldeia.empty()) dia = w.relogio.dia;
    }
    ASSERT_GT(dia, 0) << "em 40 noites algum viajante aparece";
    const auto it = std::ranges::find_if(w.mundoVivo.pedidos, [](const PedidoAberto& p) { return p.aldeia.empty(); });
    const std::string def = it->def;
    EXPECT_EQ(it->prazo, w.relogio.dia);
    // Atender aplica o que o viajante oferece
    const ViajanteDef& v = *dados().mundo.viajante(def);
    const float feliz = w.estado.felicidade;
    const int guarda = w.guardaTreinada;
    ASSERT_TRUE(w.atenderPedido(static_cast<int>(it - w.mundoVivo.pedidos.begin())));
    EXPECT_NEAR(w.estado.felicidade, std::clamp(feliz + v.felicidade, 0.0f, 100.0f), 0.01f);
    EXPECT_EQ(w.guardaTreinada, guarda + v.soldados);
    // Um viajante que ninguém atende vai embora na noite seguinte
    w.mundoVivo.pedidos.push_back({"bardo", "", w.relogio.dia});
    w.dormir(false);
    EXPECT_TRUE(std::ranges::none_of(w.mundoVivo.pedidos, [&](const PedidoAberto& p) { return p.def == "bardo" && p.prazo < w.relogio.dia; }));
}

TEST(SaveV17, GuardaOMundoVivoEMigraOsAntigos) {
    const auto pasta = std::filesystem::temp_directory_path() / "koa_teste_save_v17";
    std::filesystem::remove_all(pasta);
    World w = vilarejo();
    noites(w, 6);
    w.mundoVivo.aldeias.at("pedra_alta").gratidao = 3;
    w.mundoVivo.pedidos.push_back({"fome", "vau_cinzento", w.relogio.dia + 2});
    SaveSystem s(pasta);
    ASSERT_TRUE(s.salvar(w.capturar()));
    EXPECT_TRUE(std::filesystem::exists(pasta / "Save_001/villages.json"));
    auto d = s.carregar();
    ASSERT_TRUE(d);
    World w2 = vilarejo();
    w2.aplicar(*d);
    for (const auto& [id, e] : w.mundoVivo.aldeias) {
        EXPECT_EQ(w2.mundoVivo.aldeias.at(id).povo, e.povo) << id;
        EXPECT_FLOAT_EQ(w2.mundoVivo.aldeias.at(id).prosperidade, e.prosperidade) << id;
        EXPECT_EQ(w2.mundoVivo.aldeias.at(id).proximaCaravana, e.proximaCaravana) << id;
    }
    EXPECT_EQ(w2.mundoVivo.aldeias.at("pedra_alta").gratidao, 3);
    EXPECT_EQ(w2.mundoVivo.pedidos.size(), w.mundoVivo.pedidos.size());
    EXPECT_EQ(w2.mundoVivo.noticias, w.mundoVivo.noticias);
    // Save v16: sem villages.json, as aldeias começam como nos dados
    std::filesystem::remove(pasta / "Save_001/villages.json");
    auto meta = *ke::fs::lerTexto(pasta / "Save_001/metadata.json");
    const std::string atual = "\"versao_formato\": " + std::to_string(VERSAO_SAVE);
    meta.replace(meta.find(atual), atual.size(), "\"versao_formato\": 16");
    ke::fs::gravarTextoSeguro(pasta / "Save_001/metadata.json", meta);
    d = s.carregar();
    ASSERT_TRUE(d);
    EXPECT_FALSE(d->temMundo);
    World w3 = vilarejo();
    w3.aplicar(*d);
    EXPECT_EQ(w3.mundoVivo.aldeias.at("pedra_alta").povo, dados().mundo.aldeia("pedra_alta")->povo);
    EXPECT_TRUE(w3.mundoVivo.pedidos.empty());
    std::filesystem::remove_all(pasta);
}
