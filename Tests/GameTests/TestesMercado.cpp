// Fase 20: o mercado vivo (preço por oferta, estação e sorte; taxa e limite; o Corvin e as encomendas), as ações das alas
// e o save v15
#include <gtest/gtest.h>

#include <random>

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

World vilarejo() {
    World w;
    std::string erro;
    EXPECT_TRUE(w.carregar(dados(), PASTA_DATA, &erro)) << erro;
    w.inventario.definir("comida", 300);
    return w;
}

void comMercado(World& w, int nivel) { w.reino.construcoes.push_back({"mercado_teste", "mercado", "Mercado", nivel, {0, 0}}); }

void ricos(World& w) {
    for (const char* r : {"madeira", "pedra", "ouro", "comida"}) w.inventario.definir(r, 5000);
    w.inventario.definir("cristal", 200);
}

// O Corvin está na vila hoje (dia de visita, estrada liberada)
void comCorvin(World& w) {
    w.marcar("missao:limpar_estrada", "", true);
    while (!w.corvinNaVila()) ++w.relogio.dia;
}

bool temNota(const World& w, const std::string& trecho) {
    return std::ranges::any_of(w.ultimoRelatorio.notas, [&](const std::string& n) { return n.find(trecho) != std::string::npos; });
}

}

TEST(Mercado, DadosDoMercado) {
    const DadosMercado& d = dados().mercado;
    EXPECT_EQ(d.bens.size(), 19u);
    EXPECT_EQ(d.encomendas.size(), 5u);
    EXPECT_GT(d.taxa(1), d.taxa(3)) << "o Grande Bazar cobra menos";
    EXPECT_GT(d.taxa(0), d.taxa(1)) << "o Corvin cobra mais que o mercado";
    EXPECT_LT(d.limite(1), d.limite(3));
    // A condição que impede lucro comprando e vendendo em seguida vale em todos os níveis
    for (int n = 0; n <= 3; ++n) EXPECT_LT(d.impacto * (1.0f + d.taxa(n)), 2.0f * d.taxa(n)) << "nível " << n;
    EXPECT_FLOAT_EQ(d.reacaoA("horda_saqueada:3").at("comida"), 0.3f);
    EXPECT_TRUE(d.reacaoA("visitou:floresta").empty());
}

TEST(Mercado, PrecoReageAOfertaEAEstacao) {
    World w = vilarejo();
    ricos(w);
    comMercado(w, 1);
    const int compra = w.precoCompraLote("madeira"), venda = w.precoVendaLote("madeira");
    EXPECT_GT(compra, venda) << "a taxa fica com o mercado";
    ASSERT_TRUE(w.comprarNoMercado("madeira"));
    EXPECT_GT(w.precoCompraLote("madeira"), compra) << "comprar sobe o preço";
    ASSERT_TRUE(w.venderNoMercado("madeira"));
    ASSERT_TRUE(w.venderNoMercado("madeira"));
    EXPECT_LT(w.precoVendaLote("madeira"), venda) << "vender baixa o preço";
    // A noite devolve o preço para o normal aos poucos
    const float pressao = w.mercado.pressao["madeira"];
    noiteDoMercado(dados().mercado, w.mercado);
    EXPECT_NEAR(w.mercado.pressao["madeira"], pressao * 0.75f, 1e-4f);
    // Comida: barata no outono, cara no inverno (vale ter guardado)
    w.mercado = {};
    w.relogio.dia = 17;  // outono (cada estação tem 7 dias)
    const float outono = w.precoHoje("comida") / (1.0f + sorteDoDia(dados().mercado, w.semente(), w.relogio.dia, "comida"));
    w.relogio.dia = 24;  // inverno
    const float inverno = w.precoHoje("comida") / (1.0f + sorteDoDia(dados().mercado, w.semente(), w.relogio.dia, "comida"));
    EXPECT_NEAR(inverno / outono, 1.4f / 0.8f, 0.01f);
}

// Teste de propriedade: qualquer sequência de compras e vendas (num dia ou em vários dias da mesma estação) que volta
// ao mesmo estoque nunca termina com mais ouro. A taxa, o impacto de cada lote e a sorte do dia não deixam
TEST(Mercado, NenhumCicloDeCompraEVendaDaLucro) {
    World w = vilarejo();
    comMercado(w, 3);  // a menor taxa é o pior caso
    w.marcar("primeiro_cristal", "", true);
    const DadosMercado& dm = dados().mercado;
    const char* bens[] = {"comida", "madeira", "pedra", "cristal"};
    std::mt19937 rng(20);
    int maiorGanho = -1000000;
    for (int tentativa = 0; tentativa < 300; ++tentativa) {
        w.mercado = {};
        w.relogio.dia = 1 + static_cast<int>(rng() % 3);  // primavera, dias variados (sorte do dia diferente); tudo cabe na estação
        ricos(w);
        std::map<std::string, int> inicio;
        for (const char* b : bens) inicio[b] = w.inventario.quantidade(b);
        const int ouro = w.inventario.quantidade("ouro");
        const int dias = 1 + static_cast<int>(rng() % 2);
        auto proximoDia = [&] {
            noiteDoMercado(dm, w.mercado);
            ++w.relogio.dia;
        };
        for (int d = 0; d < dias; ++d) {
            const int ops = static_cast<int>(rng() % 12);
            for (int k = 0; k < ops; ++k) {
                const std::string b = bens[rng() % 4];
                if (rng() % 2) w.comprarNoMercado(b);
                else w.venderNoMercado(b);
            }
            if (d + 1 < dias) proximoDia();
        }
        // Volta ao estoque do começo (o que faltar compra, o que sobrar vende), dia após dia se o limite acabar
        for (int guarda = 0; guarda < 3; ++guarda) {
            bool igual = true;
            for (const char* b : bens) {
                while (w.inventario.quantidade(b) > inicio[b] && w.venderNoMercado(b)) {}
                while (w.inventario.quantidade(b) < inicio[b] && w.comprarNoMercado(b)) {}
                igual = igual && w.inventario.quantidade(b) == inicio[b];
            }
            if (igual) break;
            proximoDia();
        }
        for (const char* b : bens) ASSERT_EQ(w.inventario.quantidade(b), inicio[b]) << b << " (tentativa " << tentativa << ")";
        const int ganho = w.inventario.quantidade("ouro") - ouro;
        maiorGanho = std::max(maiorGanho, ganho);
        ASSERT_LE(ganho, 0) << "tentativa " << tentativa << ": lucro de " << ganho;
    }
    std::printf("[mercado] 300 ciclos; o melhor deles terminou com %d de ouro\n", maiorGanho);
}

TEST(Mercado, LimiteDoDiaETaxa) {
    World w = vilarejo();
    ricos(w);
    EXPECT_EQ(w.podeNegociar("pedra", true), "Construa o mercado para negociar.");
    comMercado(w, 1);
    int feitos = 0;
    while (w.comprarNoMercado("pedra")) ++feitos;
    EXPECT_EQ(feitos, dados().mercado.limite(1));
    EXPECT_FALSE(w.venderNoMercado("madeira")) << "o limite vale para comprar e vender";
    w.dormir(false);
    EXPECT_EQ(w.mercado.negociadosHoje, 0) << "a noite recomeça o limite";
    EXPECT_TRUE(w.venderNoMercado("madeira"));
    // Itens da vida cotidiana: o mercado compra, mas não vende
    w.inventario.definir("pao", 2);
    EXPECT_FALSE(w.comprarNoMercado("pao"));
    const int ouro = w.inventario.quantidade("ouro");
    ASSERT_TRUE(w.venderNoMercado("pao"));
    EXPECT_GT(w.inventario.quantidade("ouro"), ouro);
    EXPECT_EQ(w.inventario.quantidade("pao"), 1);
    // O que aparece no painel: os recursos e os itens que o herdeiro tem
    auto noPainel = [&](const std::string& id) { return std::ranges::any_of(w.bensNoMercado(false), [&](const BemDef* b) { return b->id == id; }); };
    EXPECT_TRUE(noPainel("pao"));
    EXPECT_FALSE(noPainel("banquete")) << "não tem banquete para vender";
    EXPECT_FALSE(noPainel("cristal")) << "o cristal só depois do primeiro";
}

TEST(Mercado, OCorvinCompraOQueOHerdeiroFazETrazEncomendas) {
    World w = vilarejo();
    ricos(w);
    w.inventario.definir("peixe", 3);
    EXPECT_FALSE(w.venderNoMercado("peixe", true)) << "sem o Corvin na vila";
    comCorvin(w);
    EXPECT_FALSE(w.venderNoMercado("madeira", true)) << "ele só compra colheita, peixe e pratos";
    const int peloCorvin = w.precoVendaLote("peixe", true);
    comMercado(w, 1);
    EXPECT_LT(peloCorvin, w.precoVendaLote("peixe")) << "ele paga menos que o mercado";
    w.reino.construcoes.pop_back();
    const int ouro = w.inventario.quantidade("ouro");
    ASSERT_TRUE(w.venderNoMercado("peixe", true));
    EXPECT_GT(w.inventario.quantidade("ouro"), ouro);
    // Encomenda: paga agora, chega numa visita pelo menos `dias` depois
    const EncomendaDef& e = *dados().mercado.encomenda("cristal");
    const int cristal = w.inventario.quantidade("cristal"), ouro2 = w.inventario.quantidade("ouro");
    ASSERT_TRUE(w.encomendar("cristal"));
    EXPECT_FALSE(w.encomendar("cristal")) << "uma de cada vez";
    EXPECT_EQ(w.inventario.quantidade("ouro"), ouro2 - e.custo.at("ouro"));
    const int pedido = w.relogio.dia;
    int chegou = -1;
    for (int n = 0; n < 15 && chegou < 0; ++n) {
        w.dormir(false);
        if (w.inventario.quantidade("cristal") > cristal) chegou = w.relogio.dia;
    }
    ASSERT_GT(chegou, 0);
    EXPECT_GE(chegou, pedido + e.dias);
    EXPECT_TRUE(w.corvinNaVila()) << "chega no dia de visita";
    EXPECT_EQ(w.inventario.quantidade("cristal"), cristal + 5);
    EXPECT_TRUE(temNota(w, "trouxe a encomenda"));
    EXPECT_TRUE(w.mercado.encomendas.empty());
}

TEST(Mercado, AcontecimentosMexemNosPrecos) {
    World w = vilarejo();
    comMercado(w, 1);
    const int comida = w.precoCompraLote("comida"), pedra = w.precoCompraLote("pedra");
    w.marcar("horda_saqueada:2", "");
    EXPECT_GT(w.precoCompraLote("comida"), comida) << "saque: comida em falta";
    w.marcar("tratado:rota_pedra", "");
    EXPECT_LT(w.precoCompraLote("pedra"), pedra) << "as carroças dos anões barateiam a pedra";
    // O histórico guarda o preço de cada manhã (até 14 dias)
    for (int i = 0; i < 16; ++i) w.dormir(false);
    EXPECT_EQ(w.mercado.historico.at("comida").size(), 14u);
    EXPECT_TRUE(w.mercado.historico.contains("pedra"));
    EXPECT_FALSE(w.mercado.historico.contains("pao")) << "itens que o mercado só compra não têm tendência";
}

TEST(Alas, CadaAlaTemSuaAcaoComRecarga) {
    World w = vilarejo();
    ricos(w);
    w.marcar("era:2", "", true);
    w.marcar("era:3", "", true);
    while (w.nivelCastelo() < 4) ASSERT_TRUE(w.melhorarConstrucao("castelo"));
    for (const char* a : {"salao", "quartel", "tesouro"}) ASSERT_TRUE(w.erguerAla(a)) << a;
    // Festa: alegra e só volta depois da recarga
    w.estado.felicidade = 50.0f;
    EXPECT_FALSE(w.podeAgirAla("biblioteca").empty()) << "a biblioteca não foi erguida";
    ASSERT_TRUE(w.agirAla("salao"));
    EXPECT_FLOAT_EQ(w.estado.felicidade, 56.0f);
    EXPECT_FALSE(w.agirAla("salao"));
    EXPECT_EQ(w.diasParaAgir("salao"), 7);
    // Treino: dois soldados a mais só na próxima horda
    const int defesa = w.defesaDoReino();
    ASSERT_TRUE(w.agirAla("quartel"));
    EXPECT_EQ(w.guardaTreinada, 2);
    EXPECT_EQ(w.defesaDoReino(), defesa + 2 * 6) << "cada soldado vale 6 na conta da batalha";
    // Empréstimo: volta com juros com o povo feliz
    const int ouro = w.inventario.quantidade("ouro");
    ASSERT_TRUE(w.agirAla("tesouro"));
    EXPECT_EQ(w.inventario.quantidade("ouro"), ouro - 60);
    for (int i = 0; i < 6; ++i) {
        w.estado.felicidade = 70.0f;
        w.dormir(false);
    }
    EXPECT_TRUE(temNota(w, "pagaram o empréstimo"));
    EXPECT_EQ(w.emprestimoVolta, 0);
    EXPECT_FALSE(w.podeAgirAla("salao").empty()) << "no 6º dia a festa ainda espera";
    w.dormir(false);
    EXPECT_TRUE(w.podeAgirAla("salao").empty()) << "a recarga passou";
    // A festa conta na fama (generosidade) a cada vez
    const float generosidade = w.reputacao.generosidade;
    ASSERT_TRUE(w.agirAla("salao"));
    EXPECT_GT(w.reputacao.generosidade, generosidade);
}

TEST(Alas, BibliotecaDecifraOsFragmentos) {
    World w = vilarejo();
    ricos(w);
    w.marcar("era:2", "", true);
    w.marcar("era:3", "", true);
    while (w.nivelCastelo() < 2) ASSERT_TRUE(w.melhorarConstrucao("castelo"));
    ASSERT_TRUE(w.erguerAla("biblioteca"));
    const auto& pistas = dados().lore.pistas;
    for (size_t i = 0; i + 1 < pistas.size(); ++i) w.marcar("pista:" + pistas[i].id, "", true);
    ASSERT_TRUE(w.agirAla("biblioteca"));
    EXPECT_TRUE(w.marcos.contains("decifrou:" + pistas.back().id)) << "aponta o fragmento que falta";
    w.ultimaAcaoAla.clear();
    EXPECT_EQ(w.podeAgirAla("biblioteca"), "Não sobrou fragmento para decifrar.");
}

TEST(SaveV15, GuardaOMercadoEAsAlasEMigraOsAntigos) {
    const auto pasta = std::filesystem::temp_directory_path() / "koa_teste_save_v15";
    std::filesystem::remove_all(pasta);
    World w = vilarejo();
    ricos(w);
    comMercado(w, 2);
    ASSERT_TRUE(w.comprarNoMercado("pedra"));
    w.dormir(false);
    w.mercado.encomendas.push_back({"tabuas", 3});
    w.ultimaAcaoAla["salao"] = 4;
    w.guardaTreinada = 2;
    w.emprestimoVolta = 78, w.emprestimoValor = 60, w.emprestimoDia = 9;
    SaveSystem s(pasta);
    ASSERT_TRUE(s.salvar(w.capturar()));
    EXPECT_TRUE(std::filesystem::exists(pasta / "Save_001/market.json"));
    auto d = s.carregar();
    ASSERT_TRUE(d);
    World w2 = vilarejo();
    w2.aplicar(*d);
    EXPECT_FLOAT_EQ(w2.mercado.pressao["pedra"], w.mercado.pressao["pedra"]);
    EXPECT_EQ(w2.mercado.historico.at("pedra"), w.mercado.historico.at("pedra"));
    ASSERT_EQ(w2.mercado.encomendas.size(), 1u);
    EXPECT_EQ(w2.mercado.encomendas[0].id, "tabuas");
    EXPECT_EQ(w2.mercado.comprado, w.mercado.comprado);
    EXPECT_EQ(w2.ultimaAcaoAla["salao"], 4);
    EXPECT_EQ(w2.guardaTreinada, 2);
    EXPECT_EQ(w2.emprestimoVolta, 78);
    EXPECT_EQ(w2.emprestimoDia, 9);
    // Save v14: sem market.json, o mercado começa calmo e o resto carrega igual
    std::filesystem::remove(pasta / "Save_001/market.json");
    auto meta = *ke::fs::lerTexto(pasta / "Save_001/metadata.json");
    const std::string atual = "\"versao_formato\": " + std::to_string(VERSAO_SAVE);
    meta.replace(meta.find(atual), atual.size(), "\"versao_formato\": 14");
    ke::fs::gravarTextoSeguro(pasta / "Save_001/metadata.json", meta);
    d = s.carregar();
    ASSERT_TRUE(d);
    EXPECT_FALSE(d->temMercado);
    World w3 = vilarejo();
    w3.aplicar(*d);
    EXPECT_TRUE(w3.mercado.pressao.empty());
    EXPECT_TRUE(w3.mercado.encomendas.empty());
    EXPECT_EQ(w3.inventario.quantidade("pedra"), w.inventario.quantidade("pedra"));
    std::filesystem::remove_all(pasta);
}
