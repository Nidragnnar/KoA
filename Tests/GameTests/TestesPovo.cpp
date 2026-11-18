// Fase 18: o povo com nome (famílias, profissões, desejos, partidas) e o clima de cada dia; save v13
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
    w.inventario.definir("comida", 300);
    return w;
}

glm::ivec2 lugarLivre(const World& w, const std::string& def, glm::ivec2 perto) {
    for (int raio = 0; raio < 30; ++raio)
        for (int dy = -raio; dy <= raio; ++dy)
            for (int dx = -raio; dx <= raio; ++dx)
                if (std::max(std::abs(dx), std::abs(dy)) == raio && w.podeConstruir(def, perto + glm::ivec2(dx, dy))) return perto + glm::ivec2(dx, dy);
    return {-1, -1};
}

bool temNota(const World& w, const std::string& trecho) {
    return std::ranges::any_of(w.ultimoRelatorio.notas, [&](const std::string& n) { return n.find(trecho) != std::string::npos; });
}

// O primeiro dia (a partir de `de`) com o tempo pedido
int diaCom(const World& w, const std::string& tempo, int de = 1) {
    for (int d = de; d < de + 400; ++d)
        if (w.tempoDoDia(d).id == tempo) return d;
    return -1;
}

}

TEST(Povo, DadosDoPovoEDoClima) {
    const DadosPovo& p = dados().povo;
    EXPECT_EQ(p.profissoes.size(), 10u);
    EXPECT_EQ(p.tracos.size(), 6u);
    EXPECT_EQ(p.desejos.size(), 10u);
    EXPECT_GE(p.nomes.size(), 30u);
    EXPECT_EQ(dados().clima.tempos.size(), 5u);
    // Neve só no inverno; tempestade nunca no inverno
    World w = vilarejo();
    for (int d = 1; d <= 28 * 3; ++d) {
        const TempoDef& t = w.tempoDoDia(d);
        if (t.id == "neve") EXPECT_EQ(Relogio{d}.estacao(), Estacao::Inverno) << "dia " << d;
        if (Relogio{d}.estacao() == Estacao::Inverno) EXPECT_NE(t.id, "tempestade");
    }
    EXPECT_EQ(&w.tempoDoDia(10), &w.tempoDoDia(10)) << "o mesmo dia, o mesmo tempo";
}

TEST(Povo, OsSobreviventesSemNomeGanhamNome) {
    World w = vilarejo();
    EXPECT_EQ(w.moradoresComNome(), 5);
    ASSERT_EQ(static_cast<int>(w.povo.size()), w.estado.populacao - 5);
    for (const Habitante& h : w.povo) {
        EXPECT_FALSE(h.nome.empty());
        EXPECT_FALSE(h.familia.empty());
        const ProfissaoDef* p = dados().povo.profissao(h.profissao);
        ASSERT_NE(p, nullptr);
        EXPECT_TRUE(p->requer.empty()) << "sem obras prontas, nada de " << p->id;
        EXPECT_NE(dados().povo.desejo(h.desejo), nullptr);
    }
    int ligados = 0;
    for (auto [e, a] : w.reg.view<const Aldeao>().each()) ligados += w.habitante(a.habitante) != nullptr;
    EXPECT_EQ(ligados, static_cast<int>(w.povo.size())) << "cada aldeão na rua é uma pessoa";
}

TEST(Povo, QuemChegaJuntoEhDaMesmaFamilia) {
    World w = vilarejo();
    const size_t antes = w.povo.size();
    w.estado.populacao += 3;
    std::vector<std::string> notas;
    w.ajustarPovo(false, notas);
    ASSERT_EQ(w.povo.size(), antes + 3);
    EXPECT_EQ(w.povo[antes].familia, w.povo[antes + 1].familia) << "famílias de 2 a 4 pessoas";
    ASSERT_FALSE(notas.empty());
    EXPECT_NE(notas.back().find(w.povo[antes].familia), std::string::npos);
    // Muita gente de uma vez vem em várias famílias pequenas
    w.estado.populacao += 30;
    w.ajustarPovo(false, notas);
    std::map<std::string, int> porFamilia;
    for (size_t i = antes + 3; i < w.povo.size(); ++i) ++porFamilia[w.povo[i].familia];
    EXPECT_GE(porFamilia.size(), 5u);
    // Ninguém chega desejando o que o reino já tem
    for (size_t i = antes; i < w.povo.size(); ++i) EXPECT_FALSE(w.desejoCumprido(*dados().povo.desejo(w.povo[i].desejo))) << w.povo[i].desejo;
    std::set<int> ids;
    for (const Habitante& h : w.povo) ids.insert(h.id);
    EXPECT_EQ(ids.size(), w.povo.size()) << "ids únicos";
}

TEST(Povo, ProfissaoSoComAObraPronta) {
    const DadosPovo& p = dados().povo;
    std::set<std::string> semNada, comForja{"forja"};
    bool ferreiroSem = false, ferreiroCom = false;
    for (uint32_t s = 1; s < 400; ++s) {
        ferreiroSem |= gerarHabitante(p, 1, "X", 1, s, semNada).profissao == "ferreiro";
        ferreiroCom |= gerarHabitante(p, 1, "X", 1, s, comForja).profissao == "ferreiro";
    }
    EXPECT_FALSE(ferreiroSem);
    EXPECT_TRUE(ferreiroCom);
}

TEST(Povo, AFomeLevaGenteComNome) {
    World w = vilarejo();
    w.inventario.definir("comida", 0);
    w.dormir(false);
    EXPECT_EQ(static_cast<int>(w.povo.size()), w.estado.populacao - 5);
    EXPECT_TRUE(temNota(w, "foi embora: faltou comida"));
}

TEST(Povo, DesejoRealizadoViraCronica) {
    World w = vilarejo();
    ASSERT_FALSE(w.povo.empty());
    w.povo[0].desejo = "capela";
    const int id = w.povo[0].id;
    for (const char* r : {"madeira", "pedra", "ouro"}) w.inventario.definir(r, 500);
    ASSERT_FALSE(w.construir("capela", lugarLivre(w, "capela", {44, 26})).empty());
    w.dormir(false);  // a obra fica pronta de noite; o desejo é conferido de manhã
    ASSERT_NE(w.habitante(id), nullptr);
    EXPECT_TRUE(w.habitante(id)->cumprido);
    EXPECT_TRUE(w.marcos.contains("desejo_capela:" + std::to_string(id)));
    EXPECT_TRUE(std::ranges::any_of(w.cronicas, [&](const EntradaCronica& c) { return c.marco == "desejo_capela:" + std::to_string(id); }));
    EXPECT_TRUE(temNota(w, "realizou um desejo"));
    EXPECT_GT(efeitosDoPovo(dados().povo, w.povo).felicidade, 0.0f);
}

TEST(Povo, QuemCansaDeEsperarVaiEmbora) {
    World w = vilarejo();
    ASSERT_FALSE(w.povo.empty());
    Habitante& h = w.povo[0];
    h.desejo = "castelo";  // o castelo ainda está em ruínas
    h.traco = "rabugento";
    h.esperando = 20;
    const int id = h.id;
    const int pop = w.estado.populacao;
    w.estado.felicidade = 30.0f;
    w.dormir(false);
    EXPECT_EQ(w.habitante(id), nullptr);
    EXPECT_TRUE(temNota(w, "cansou de esperar ver o castelo de pé"));
    EXPECT_LE(w.estado.populacao, pop - 1);
    EXPECT_TRUE(w.marcos.contains("partiu:" + std::to_string(id)));
}

TEST(Povo, AsProfissoesRendem) {
    World w = vilarejo();
    const int madeira = w.resumo().madeiraDia;
    for (int i = 0; i < 8; ++i) {
        Habitante h = gerarHabitante(dados().povo, 100 + i, "Teste", 1, static_cast<uint32_t>(i), {});
        h.profissao = "lenhador";
        h.traco = "trabalhador";
        w.povo.push_back(h);
    }
    EXPECT_EQ(w.resumo().madeiraDia, madeira + 3) << "8 lenhadores trabalhadores: 8 × 0,25 × 1,5 = 3";
}

TEST(Clima, ChuvaRegaAHorta) {
    World w = vilarejo();
    const int dia = diaCom(w, "chuva", 2);
    ASSERT_GT(dia, 1);
    w.relogio.dia = dia - 1;
    ASSERT_TRUE(w.usarCanteiro(0));  // planta (sem regar)
    w.dormir(false);
    ASSERT_EQ(w.relogio.dia, dia);
    EXPECT_EQ(w.canteiros[0].regadoDia, dia) << "a chuva regou";
    EXPECT_TRUE(temNota(w, "Chove"));
}

TEST(Clima, NeveDeixaTodoMundoMaisLento) {
    World w = vilarejo();
    auto andar = [&](int dia) {
        w.relogio.dia = dia;
        w.jogador.pos = {40.0f * 16, 19.0f * 16};
        const float x0 = w.jogador.pos.x;
        EntradaMundo in;
        in.mover = {1, 0};
        for (int i = 0; i < 30; ++i) w.passo(1.0f / 60.0f, in);
        return w.jogador.pos.x - x0;
    };
    const int neve = diaCom(w, "neve"), limpo = diaCom(w, "limpo", neve);
    ASSERT_GT(neve, 0);
    ASSERT_GT(limpo, 0);
    const float comNeve = andar(neve), semNeve = andar(limpo);
    EXPECT_NEAR(comNeve / semNeve, 0.8f, 0.05f);
}

TEST(Clima, NeblinaEscondeAHordaEDebaixoDaTerraNaoChove) {
    World w = vilarejo();
    const int neblina = diaCom(w, "neblina"), limpo = diaCom(w, "limpo");
    w.hordaDia = limpo;
    const int forca = w.forcaProximaHorda();
    w.hordaDia = neblina;
    EXPECT_EQ(w.forcaProximaHorda(), static_cast<int>(std::round(forca * 1.2f)));
    w.relogio.dia = diaCom(w, "chuva");
    EXPECT_EQ(w.tempoAqui().id, "chuva");
    w.entrarRegiao("cripta", {3, 10}, Direcao::Baixo);
    EXPECT_EQ(w.tempoAqui().id, "limpo") << "na cripta não chove";
}

TEST(Povo, ConversarAbreAFicha) {
    World w = vilarejo();
    entt::entity e = entt::null;
    for (auto [en, a] : w.reg.view<const Aldeao>().each()) e = en;
    ASSERT_TRUE(e != entt::null);
    w.relogio.minutos = 10 * 60;
    w.reg.get<Posicao>(e).v = {30.0f * 16 + 8, 25.0f * 16};  // longe dos moradores com nome (eles teriam a vez)
    w.jogador.pos = w.reg.get<Posicao>(e).v + glm::vec2(0, 8);
    w.jogador.dir = Direcao::Cima;
    w.reg.get<Movimento>(e).caminho.clear();
    w.reg.get<Aldeao>(e).espera = 99.0f;
    w.passo(1.0f / 60.0f, {});
    EXPECT_EQ(w.alvo().tipo, TipoInteracao::Habitante) << "alvo: " << w.alvo().rotulo;
    w.tirarEventos();
    EntradaMundo in;
    in.interagir = true;
    w.passo(1.0f / 60.0f, in);
    const auto ev = w.tirarEventos();
    const auto it = std::ranges::find_if(ev, [](const EventoJogo& x) { return x.tipo == EventoJogo::Tipo::AbrirFicha; });
    ASSERT_NE(it, ev.end());
    EXPECT_EQ(it->quantidade, w.reg.get<Aldeao>(e).habitante);
}

TEST(SaveV13, GuardaOPovoEMigraOSaveAntigo) {
    const auto pasta = std::filesystem::temp_directory_path() / "koa_teste_save_v13";
    std::filesystem::remove_all(pasta);
    World w = vilarejo();
    w.estado.populacao += 4;
    std::vector<std::string> notas;
    w.ajustarPovo(false, notas);
    w.povo[0].cumprido = true;
    w.povo[1].esperando = 7;
    SaveSystem s(pasta);
    ASSERT_TRUE(s.salvar(w.capturar()));
    EXPECT_TRUE(std::filesystem::exists(pasta / "Save_001/people.json"));
    auto d = s.carregar();
    ASSERT_TRUE(d);
    World w2 = vilarejo();
    w2.aplicar(*d);
    ASSERT_EQ(w2.povo.size(), w.povo.size());
    EXPECT_EQ(w2.povo[0].nomeCompleto(), w.povo[0].nomeCompleto());
    EXPECT_TRUE(w2.povo[0].cumprido);
    EXPECT_EQ(w2.povo[1].esperando, 7);
    EXPECT_EQ(w2.proximoHabitante, w.proximoHabitante);
    // Save v12: o povo ganha nome ao carregar, do tamanho da população
    std::filesystem::remove(pasta / "Save_001/people.json");
    auto meta = *ke::fs::lerTexto(pasta / "Save_001/metadata.json");
    const std::string atual = "\"versao_formato\": " + std::to_string(VERSAO_SAVE);
    meta.replace(meta.find(atual), atual.size(), "\"versao_formato\": 12");
    ke::fs::gravarTextoSeguro(pasta / "Save_001/metadata.json", meta);
    d = s.carregar();
    ASSERT_TRUE(d);
    EXPECT_FALSE(d->temPovoNomeado);
    World w3 = vilarejo();
    w3.aplicar(*d);
    EXPECT_EQ(static_cast<int>(w3.povo.size()), w3.estado.populacao - 5);
    std::filesystem::remove_all(pasta);
}
