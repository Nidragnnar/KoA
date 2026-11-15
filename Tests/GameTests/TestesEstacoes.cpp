// Fase 10: estações que mudam a economia, acontecimentos raros e o Festival da Colheita
#include <gtest/gtest.h>

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

// Primeiro dia (a partir de `de`) em que cai o acontecimento pedido
int diaCom(const World& w, const std::string& id, int de = 4) {
    for (int d = de; d < 2000; ++d)
        if (const EventoDef* ev = w.eventoDoDia(d); ev && ev->id == id) return d;
    return -1;
}

bool temNota(const World& w, const std::string& trecho) {
    return std::ranges::any_of(w.ultimoRelatorio.notas, [&](const std::string& n) { return n.find(trecho) != std::string::npos; });
}

}

TEST(Estacoes, DadosDasQuatroEstacoesEDoFestival) {
    const DadosEstacoes& e = dados().estacoes;
    EXPECT_EQ(e.de(Estacao::Inverno).nome, "Inverno");
    EXPECT_FLOAT_EQ(e.de(Estacao::Inverno).mod.comida, 0.4f);
    EXPECT_EQ(e.de(Estacao::Verao).mercadorACada, 2);
    EXPECT_EQ(e.festival.estacao, Estacao::Outono);
    EXPECT_EQ(e.festival.diaDaEstacao, 7);
    EXPECT_EQ(e.eventos.size(), 3u);
    EXPECT_EQ(Relogio::DIAS_POR_ESTACAO, 7);
}

TEST(Estacoes, InvernoCortaAsHortasEOutonoColhe) {
    World w = vilarejo();
    w.reino.construcoes.push_back({"campo_teste", "campo", "Horta", 1, {0, 0}});  // 5 de comida por dia
    w.relogio.dia = 2;  // primavera
    EXPECT_EQ(w.resumo().comidaProduzida, 5);
    w.relogio.dia = 15;  // outono: +50%
    EXPECT_EQ(w.resumo().comidaProduzida, 8);
    w.relogio.dia = 22;  // inverno: 40%
    EXPECT_EQ(w.resumo().comidaProduzida, 2);
    EXPECT_EQ(w.estacaoAtual().nome, "Inverno");
}

TEST(Estacoes, VeraoTrazMaisOuroEOMercadorMaisVezes) {
    World w = vilarejo();
    w.reino.construcoes.push_back({"mercado_teste", "mercado", "Mercado", 1, {0, 0}});  // 4 de ouro por dia
    w.relogio.dia = 2;
    const int primavera = w.resumo().ouroProducao;
    w.relogio.dia = 9;
    EXPECT_EQ(w.resumo().ouroProducao, static_cast<int>(std::round(primavera * 1.25f)));
    const NpcDef& corvin = dados().npcs.at("corvin");
    w.marcar("missao:limpar_estrada", "Limpar a Estrada");
    w.relogio.dia = 10;  // verão: a cada 2 dias
    EXPECT_TRUE(w.visitanteHoje(corvin));
    w.relogio.dia = 11;
    EXPECT_FALSE(w.visitanteHoje(corvin));
    w.relogio.dia = 4;  // primavera: a cada 3 dias
    EXPECT_FALSE(w.visitanteHoje(corvin));
    w.relogio.dia = 6;
    EXPECT_TRUE(w.visitanteHoje(corvin));
}

TEST(Estacoes, CriaturasMaisFortesNoInverno) {
    World w = vilarejo();
    w.relogio.dia = 22;
    w.renascerInimigos();
    w.entrarRegiao("floresta", {3, 17}, Direcao::Direita);
    const int vidaBase = dados().inimigos.at("sombra").vida;
    bool achou = false;
    for (auto [e, i] : w.reg.view<Inimigo>().each())
        if (i.tipo == "sombra") {
            EXPECT_EQ(i.vidaMax, static_cast<int>(std::round(vidaBase * 1.3f)));
            achou = true;
        }
    EXPECT_TRUE(achou);
    EXPECT_FLOAT_EQ(w.forcaInimigos(), 1.3f);
}

TEST(Estacoes, AvisaQueOInvernoVemEAnunciaAEstacaoNova) {
    World w = vilarejo();
    w.relogio.dia = 17;  // amanhã é o 4º dia do outono
    w.dormir(false);
    EXPECT_TRUE(temNota(w, "inverno chega em 4 dias")) << w.ultimoRelatorio.notas.size();
    w.relogio.dia = 21;
    w.dormir(false);  // dia 22: primeiro dia do inverno
    EXPECT_TRUE(temNota(w, "Começou o Inverno"));
    EXPECT_TRUE(w.marcos.contains("estacao:inverno"));
}

TEST(Eventos, SorteioFixoPorDiaERequisitos) {
    World w = vilarejo();
    for (int d = 1; d <= dados().estacoes.diasSemEventos; ++d) EXPECT_EQ(w.eventoDoDia(d), nullptr) << "os primeiros dias são calmos";
    EXPECT_EQ(w.eventoDoDia(21), nullptr) << "no dia do festival, nada de acontecimento";
    EXPECT_EQ(diaCom(w, "lua_sangue"), -1) << "sem conhecer a floresta, nada de Lua de Sangue";
    EXPECT_EQ(diaCom(w, "aurora"), -1) << "a aurora pede a era 1";
    const int m = diaCom(w, "meteoros");
    ASSERT_NE(m, -1);
    EXPECT_EQ(w.eventoDoDia(m), w.eventoDoDia(m)) << "o mesmo dia dá sempre o mesmo";
    w.marcar("visitou:floresta", "Floresta Amaldiçoada");
    EXPECT_NE(diaCom(w, "lua_sangue"), -1);
    // Frequência aproximada: uns 13% dos dias com algum acontecimento (sem a aurora)
    int com = 0;
    for (int d = 4; d < 1004; ++d) com += w.eventoDoDia(d) != nullptr;
    EXPECT_GT(com, 80);
    EXPECT_LT(com, 200);
}

TEST(Eventos, LuaDeSangueAvermelhaANoiteEFortaleceAsCriaturas) {
    World w = vilarejo();
    w.marcar("visitou:floresta", "Floresta Amaldiçoada");
    const int d = diaCom(w, "lua_sangue");
    ASSERT_NE(d, -1);
    w.relogio.dia = d;
    w.relogio.minutos = 10 * 60;
    EXPECT_FALSE(w.eventoAgora("lua_sangue")) << "de dia ainda não";
    w.relogio.minutos = 22 * 60;
    EXPECT_TRUE(w.eventoAgora("lua_sangue"));
    const glm::vec3 luz = w.luzAmbiente();
    EXPECT_GT(luz.r, luz.b) << "a noite fica vermelha";
    EXPECT_GE(w.forcaInimigos(), 1.5f - 0.001f);
    w.tirarEventos();
    w.passo(1.0f / 60.0f, {});
    const auto ev = w.tirarEventos();
    EXPECT_TRUE(std::ranges::any_of(ev, [](const EventoJogo& e) { return e.tipo == EventoJogo::Tipo::EventoRaro && e.alvo == "lua_sangue"; }));
    EXPECT_TRUE(w.marcos.contains("evento:lua_sangue"));
}

TEST(Eventos, EstrelaCaiNaManhaSeguinteEAoRecolherNaoVolta) {
    World w = vilarejo();
    const int d = diaCom(w, "meteoros");
    ASSERT_NE(d, -1);
    w.relogio.dia = d;
    w.dormir(false);
    EXPECT_TRUE(temNota(w, "estrela caiu"));
    entt::entity estrela = entt::null;
    for (auto [e, m] : w.reg.view<Meteorito>().each()) estrela = e;
    ASSERT_TRUE(estrela != entt::null);
    // Recolher: herói logo abaixo, olhando para cima, aperta E
    const glm::vec2 p = w.reg.get<Posicao>(estrela).v;
    w.jogador.pos = p + glm::vec2(0, 12);
    w.jogador.dir = Direcao::Cima;
    w.passo(1.0f / 60.0f, {});
    ASSERT_EQ(w.alvo().rotulo, "Recolher a estrela caída");
    const int ouro = w.inventario.quantidade("ouro");
    w.passo(1.0f / 60.0f, {{0, 0}, true, true});
    EXPECT_EQ(w.inventario.quantidade("ouro"), ouro + 40);
    EXPECT_TRUE(w.marcos.contains(std::format("meteorito:{}", d)));
    // Carregando o save no mesmo dia, ela não reaparece
    const DadosSave s = w.capturar();
    World w2 = vilarejo();
    w2.aplicar(s);
    EXPECT_TRUE(w2.reg.view<Meteorito>().empty());
}

TEST(Festival, PovoNaPracaAmizadeEmDobroEAlegriaNoDiaSeguinte) {
    World w = vilarejo();
    w.relogio.dia = 21;  // 7º dia do outono
    ASSERT_TRUE(w.festivalHoje());
    w.relogio.minutos = 16 * 60;
    EXPECT_FALSE(w.festivalAgora());
    w.relogio.minutos = 18 * 60;
    ASSERT_TRUE(w.festivalAgora());
    w.posicionarMoradores();
    const auto& lugares = dados().estacoes.festival.lugares;
    for (auto [e, m, p, a] : w.reg.view<const Morador, const Posicao, const Agenda>().each()) {
        if (dados().npcs.at(m.id).visitante()) continue;
        const glm::ivec2 t{static_cast<int>(p.v.x) / 16, static_cast<int>(p.v.y) / 16};
        EXPECT_TRUE(std::ranges::find(lugares, t) != lugares.end()) << m.id << " não está na festa";
        EXPECT_FALSE(a.dormindo);
    }
    w.tirarEventos();
    w.passo(1.0f / 60.0f, {});
    const auto ev = w.tirarEventos();
    EXPECT_TRUE(std::ranges::any_of(ev, [](const EventoJogo& e) { return e.tipo == EventoJogo::Tipo::Festival; }));
    EXPECT_TRUE(std::ranges::any_of(w.cronicas, [](const EntradaCronica& c) { return c.marco == "festival_era0:1"; }));
    // Conversa no festival: amizade em dobro e fala da festa
    const Conversa c = w.conversar("tilda");
    EXPECT_EQ(w.reg.get<Relacionamento>(w.morador("tilda")).afinidade, 50);
    w.encerrarConversa("tilda");
    w.conversar("tilda");  // a apresentação já foi; agora a fala da festa
    const Conversa c2 = w.conversar("bram");
    (void)c;
    (void)c2;
    w.dormir(false);
    EXPECT_TRUE(temNota(w, "Festival da Colheita ainda alegra"));
}

TEST(Festival, EnfeitesDaFestaCrescemComAEra) {
    World w = vilarejo();
    int festa = 0, porEra[4] = {};
    for (auto [e, en] : w.reg.view<Enfeite>().each())
        if (en.festival) ++festa, ++porEra[std::clamp(en.eraMin, 0, 3)];
    EXPECT_EQ(festa, 6);
    EXPECT_EQ(porEra[0], 2) << "no começo, só duas lanternas e a fogueira";
    EXPECT_GE(porEra[2], 1);
}
