// Fase 21: a linhagem (casamento, filhos, traços herdados, tutores, sucessão com salto de anos) e o save v16
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

World vilarejo() {
    World w;
    std::string erro;
    EXPECT_TRUE(w.carregar(dados(), PASTA_DATA, &erro)) << erro;
    for (const char* r : {"madeira", "pedra", "ouro", "comida"}) w.inventario.definir(r, 3000);
    return w;
}

void amizade(World& w, const std::string& npc, int valor) {
    for (auto [e, m, rel] : w.reg.view<const Morador, Relacionamento>().each())
        if (m.id == npc) rel.afinidade = valor;
}

void noites(World& w, int n) {
    for (int i = 0; i < n; ++i) w.dormir(false);
}

// Casado com a Edda e com um conselho que pode educar os filhos
World familia() {
    World w = vilarejo();
    amizade(w, "edda", 800);
    EXPECT_TRUE(w.casar("edda"));
    w.marcar("conselho_formado", "", true);
    EXPECT_TRUE(w.nomear("tesoureiro", "bram"));
    w.lealdade["bram"] = 80;
    return w;
}

bool temNota(const World& w, const std::string& trecho) {
    return std::ranges::any_of(w.ultimoRelatorio.notas, [&](const std::string& n) { return n.find(trecho) != std::string::npos; });
}

}

TEST(Linhagem, DadosEFundador) {
    const DadosLinhagem& d = dados().linhagem;
    EXPECT_EQ(d.tracos.size(), 9u);
    EXPECT_EQ(d.conjuges.size(), 8u);
    EXPECT_EQ(d.tutores.size(), 4u);
    World w = vilarejo();
    EXPECT_EQ(w.herdeiro().nome, "Aren");
    EXPECT_EQ(w.linhagem.reinado, 1);
    EXPECT_EQ(w.jogador.vidaMax, 125) << "o fundador é Valente";
    EXPECT_EQ(w.anoDoReino(), 1);
}

// Herança: o primeiro traço vem de um dos genitores; o segundo, do tutor leal (ou da sorte, que às vezes é ruim)
TEST(Linhagem, TracosHerdadosEDoTutor) {
    const DadosLinhagem& d = dados().linhagem;
    int ruins = 0;
    for (uint32_t s = 1; s <= 300; ++s) {
        const auto t = tracosDaMaioridade(d, {"valente"}, {"carismatico"}, "astuto", s);
        ASSERT_EQ(t.size(), 2u);
        EXPECT_TRUE(t[0] == "valente" || t[0] == "carismatico");
        EXPECT_EQ(t[1], "astuto");
        const auto sem = tracosDaMaioridade(d, {"valente"}, {"carismatico"}, "", s);
        ASSERT_EQ(sem.size(), 2u);
        EXPECT_NE(sem[0], sem[1]);
        ruins += d.traco(sem[1])->ruim;
        // Tutor que ensinaria o mesmo traço herdado: o segundo é sorteado, nunca repete
        const auto igual = tracosDaMaioridade(d, {"valente"}, {"valente"}, "valente", s);
        EXPECT_NE(igual[0], igual[1]);
    }
    EXPECT_GT(ruins, 0) << "sem tutor leal, às vezes vem um traço ruim";
    EXPECT_LT(ruins, 150);
}

TEST(Linhagem, CasamentoComMoradorEPolitico) {
    World w = vilarejo();
    EXPECT_FALSE(w.podeCasar("edda").empty()) << "pede amizade";
    amizade(w, "edda", 800);
    const float feliz = w.estado.felicidade;
    const int ouro = w.inventario.quantidade("ouro");
    ASSERT_TRUE(w.casar("edda"));
    EXPECT_EQ(w.inventario.quantidade("ouro"), ouro - 60);
    EXPECT_FLOAT_EQ(w.estado.felicidade, std::min(100.0f, feliz + 5.0f));
    const Membro* c = w.linhagem.membro(w.herdeiro().conjuge);
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->nome, "Edda");
    EXPECT_EQ(c->tracos, std::vector<std::string>{"carismatico"});
    EXPECT_TRUE(w.marcos.contains("casamento:edda"));
    EXPECT_FALSE(w.casar("bram")) << "uma vez casado, não casa de novo";
    // Político: a Ordem aliada manda Arin Vauren com dote; o Culto (rival) não gosta
    World p = vilarejo();
    p.mudarRelacao("ordem", 60.0f - p.relacaoCom("ordem"));
    EXPECT_TRUE(p.podeCasar("anoes").starts_with("Pede relação"));
    const float culto = p.relacaoCom("culto");
    const int cristal = p.inventario.quantidade("cristal");
    ASSERT_TRUE(p.casar("ordem"));
    EXPECT_EQ(p.linhagem.membro(p.herdeiro().conjuge)->nome, "Arin Vauren");
    EXPECT_EQ(p.inventario.quantidade("cristal"), cristal + 8);
    EXPECT_LT(p.relacaoCom("culto"), culto - 15.0f);
}

TEST(Linhagem, FilhosNascemCrescemEAprendemComOTutor) {
    World w = familia();
    noites(w, 6);
    EXPECT_TRUE(w.linhagem.filhosDe(w.herdeiro().id).empty());
    noites(w, 1);
    auto filhos = w.linhagem.filhosDe(w.herdeiro().id);
    ASSERT_EQ(filhos.size(), 1u) << "o primeiro nasce 7 dias depois do casamento";
    const int id = filhos[0]->id;
    EXPECT_FALSE(filhos[0]->adulto);
    EXPECT_TRUE(temNota(w, "Nasceu"));
    EXPECT_TRUE(w.marcos.contains("primeiro_filho"));
    ASSERT_TRUE(w.definirTutor(id, "tesoureiro"));
    EXPECT_FALSE(w.definirTutor(id, "marechal")) << "cargo vago não educa ninguém";
    EXPECT_FALSE(w.podeSuceder(id).empty()) << "criança não herda";
    noites(w, 28);
    const Membro* f = w.linhagem.membro(id);
    ASSERT_TRUE(f->adulto);
    ASSERT_EQ(f->tracos.size(), 2u);
    EXPECT_TRUE(f->tracos[0] == "valente" || f->tracos[0] == "carismatico") << "herdado";
    EXPECT_EQ(f->tracos[1], "astuto") << "o Mestre da Moeda ensinou";
    EXPECT_TRUE(w.podeSuceder(id).empty());
    // Mais dois irmãos e nenhum além do limite
    noites(w, 40);
    EXPECT_EQ(w.linhagem.filhosDe(w.herdeiro().id).size(), 3u);
    EXPECT_EQ(w.linhagem.proximoNascimento, 0);
}

// Uma sucessão completa: muda quem reina e os traços que valem, o reino fica inteiro
TEST(Linhagem, SucessaoTrocaOHerdeiroSemPerderOReino) {
    World w = familia();
    noites(w, 7);
    const int id = w.linhagem.filhosDe(w.herdeiro().id)[0]->id;
    w.definirTutor(id, "tesoureiro");
    noites(w, 28);
    w.reputacao.justica = 40.0f;
    const auto construcoes = w.reino.construcoes.size();
    const int ouro = w.inventario.quantidade("ouro");
    const int lealdade = w.lealdade["bram"];
    const int dia = w.relogio.dia;
    const auto paginas = w.cronicas.size();
    ASSERT_TRUE(w.passarCoroa(id));
    EXPECT_EQ(w.herdeiro().id, id);
    EXPECT_EQ(w.linhagem.reinado, 2);
    EXPECT_EQ(w.anoDoReino(), w.relogio.ano() + 15);
    EXPECT_FALSE(w.linhagem.membro(1)->vivo);
    EXPECT_EQ(w.relogio.dia, dia) << "o salto é de anos de história, não do relógio";
    EXPECT_EQ(w.reino.construcoes.size(), construcoes);
    EXPECT_EQ(w.inventario.quantidade("ouro"), ouro);
    EXPECT_EQ(w.lealdade["bram"], std::min(100, lealdade + 10)) << "o tutor fica orgulhoso";
    EXPECT_NEAR(w.reputacao.justica, 40.0f * 0.7f, 0.01f) << "o povo esquece um pouco";
    // Os traços do novo herdeiro valem (Astuto: taxa do mercado menor)
    const EfeitosTracos e = w.efeitosHerdeiro();
    EXPECT_LT(e.taxaMercado, 0.0f);
    EXPECT_EQ(w.jogador.vidaMax, 100 + e.vida);
    EXPECT_TRUE(w.marcos.contains("sucessao:2"));
    EXPECT_EQ(w.titulo().id, "herdeiro_dinastia");
    ASSERT_GT(w.cronicas.size(), paginas);
    EXPECT_EQ(w.cronicas.back().reinado, 2) << "as páginas novas são do novo reinado";
    EXPECT_FALSE(w.podeCasar("edda").empty()) << "a Edda já é da família";
}

TEST(Linhagem, QuemReinaDemaisMorreEACoroaPassaSozinha) {
    World w = familia();
    noites(w, 7 + 28);
    ASSERT_NE(w.maisVelho(), 0);
    const int velho = w.maisVelho();
    w.linhagem.inicioReinado = w.relogio.dia - dados().linhagem.reinadoMax + 1;
    w.dormir(false);
    EXPECT_EQ(w.herdeiro().id, velho);
    EXPECT_TRUE(temNota(w, "morreu em paz"));
    // Sem filho adulto, quem reina segue no trono (o reino nunca fica sem ninguém)
    World s = vilarejo();
    s.linhagem.inicioReinado = s.relogio.dia - 400;
    s.dormir(false);
    EXPECT_EQ(s.herdeiro().nome, "Aren");
    EXPECT_TRUE(s.marcos.contains("envelheceu:1"));
}

TEST(SaveV16, GuardaAFamiliaESavesAntigosViramAPrimeiraGeracao) {
    const auto pasta = std::filesystem::temp_directory_path() / "koa_teste_save_v16";
    std::filesystem::remove_all(pasta);
    World w = familia();
    noites(w, 7);
    const int id = w.linhagem.filhosDe(w.herdeiro().id)[0]->id;
    w.definirTutor(id, "tesoureiro");
    SaveSystem s(pasta);
    ASSERT_TRUE(s.salvar(w.capturar()));
    EXPECT_TRUE(std::filesystem::exists(pasta / "Save_001/family.json"));
    auto d = s.carregar();
    ASSERT_TRUE(d);
    World w2 = vilarejo();
    w2.aplicar(*d);
    EXPECT_EQ(w2.linhagem.membros.size(), w.linhagem.membros.size());
    EXPECT_EQ(w2.linhagem.membro(id)->tutor, "tesoureiro");
    EXPECT_EQ(w2.linhagem.proximoNascimento, w.linhagem.proximoNascimento);
    EXPECT_EQ(w2.herdeiro().conjuge, w.herdeiro().conjuge);
    EXPECT_EQ(w2.cronicas.back().reinado, 1);
    // Save v15: sem family.json, o herdeiro vira o fundador
    std::filesystem::remove(pasta / "Save_001/family.json");
    auto meta = *ke::fs::lerTexto(pasta / "Save_001/metadata.json");
    const std::string atual = "\"versao_formato\": " + std::to_string(VERSAO_SAVE);
    meta.replace(meta.find(atual), atual.size(), "\"versao_formato\": 15");
    ke::fs::gravarTextoSeguro(pasta / "Save_001/metadata.json", meta);
    d = s.carregar();
    ASSERT_TRUE(d);
    EXPECT_FALSE(d->temLinhagem);
    World w3 = vilarejo();
    w3.aplicar(*d);
    EXPECT_EQ(w3.linhagem.membros.size(), 1u);
    EXPECT_EQ(w3.herdeiro().nome, "Aren");
    EXPECT_EQ(w3.linhagem.reinado, 1);
    EXPECT_EQ(w3.jogador.vidaMax, 125);
    std::filesystem::remove_all(pasta);
}
